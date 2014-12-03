/*
 * linuc/drivers/char/mx2-rtc.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
********************************************************************************
	Copyright (C) 2002 Motorola GSG-China
 
	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
	USA.
*******************************************************************************/

#define RTC_VERSION	"0.3.0"

#define HW_2HZ
// define this for 32.768kHz; undef for 32.000kHz
#define KHz32768

#define RTC_IO_EXTENT	0x10	/* Only really two ports, but...	*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/arch/mx2.h>

#define RTC_AIE			0x0004
#define RTC_PIE			0xff80
#define RTC_UIE			0x0010
#define RTC_SIE			0x0001

#define RTC_IRQ			INT_RTC_RX

//#define _DEBUG_RTC
#ifdef _DEBUG_RTC
#	define RTC_DB(fmt, args...) 	printk("\n[%s]:[line%d]----"fmt, __FILE__, __LINE__, ## args)
#	define RTC_LOC			printk(KERN_ERR"\nCurrent Location [%s]:[%d]\n", __FILE__, __LINE__)
#	define FUNC_START		printk(KERN_ERR"\n[%s]:start!\n", __FUNCTION__)
#	define FUNC_END			printk(KERN_ERR"\n[%s]:end!\n", __FUNCTION__)
#	define CLUE(arg)		printk(" %s\n",arg)

#else
#	define RTC_DB(fmt, args...)
#	define RTC_LOC
#	define FUNC_START
#	define FUNC_END
#endif

#define is_leap(year) \
        ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

#define TRY_SLEEPON

static struct fasync_struct *rtc_async_queue;

#ifdef TRY_SLEEPON
wait_queue_head_t rtc_wait;
#else
static DECLARE_WAIT_QUEUE_HEAD(rtc_wait);
#endif

extern spinlock_t rtc_lock;

static struct timer_list rtc_irq_timer;

static loff_t rtc_llseek(struct file *file, loff_t offset, int origin);

static ssize_t rtc_read(struct file *file, char *buf,
			size_t count, loff_t *ppos);

static int rtc_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg);

#if RTC_IRQ
static unsigned int rtc_poll(struct file *file, poll_table *wait);
#endif

static void get_rtc_time (struct rtc_time *rtc_tm);
static void get_rtc_alm_time (struct rtc_time *alm_tm);
#if RTC_IRQ
static void rtc_dropped_irq(unsigned long data);

static void set_rtc_irq_bit(unsigned int bit);
static void mask_rtc_irq_bit(unsigned int bit);
#endif

static int rtc_read_proc(char *page, char **start, off_t off,
                         int count, int *eof, void *data);

/*
 *	Bits in rtc_status. (6 bits of room for future expansion)
 */

#define RTC_IS_OPEN		0x01	/* means /dev/rtc is in use	*/
#define RTC_TIMER_ON		0x02	/* missed irq timer active	*/

/*
 * rtc_status is never changed by rtc_interrupt, and ioctl/open/close is
 * protected by the big kernel lock. However, ioctl can still disable the timer
 * in rtc_status and then with del_timer after the interrupt has read
 * rtc_status but before mod_timer is called, which would then reenable the
 * timer (but you would need to have an awful timing before you'd trip on it)
 */
static unsigned long rtc_status = 0;	/* bitmapped status byte.	*/
static unsigned long rtc_freq = 0;	/* Current periodic IRQ rate	*/
static unsigned long rtc_irq_data = 0;	/* our output to the world	*/
static unsigned long before_wk = 0;     /* the seconds value before set the stopwatch */

/*
 *	If this driver ever becomes modularised, it will be really nice
 *	to make the epoch retain its value across module reload...
 */

static unsigned long epoch = 1970;	/* year corresponding to 0x00	*/

static const unsigned char days_in_mo[] = 
{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#if RTC_IRQ
/*
 *	A very tiny interrupt handler. It runs with SA_INTERRUPT set,
 *	but there is possibility of conflicting with the set_rtc_mmss()
 *	call (the rtc irq and the timer irq can easily run at the same
 *	time in two different CPUs). So we need to serializes
 *	accesses to the chip with the rtc_lock spinlock that each
 *	architecture should implement in the timer code.
 *	(See ./arch/XXXX/kernel/time.c for the set_rtc_mmss() function.)
 */

static void rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/*
	 *	Can be an alarm interrupt, update complete interrupt,
	 *	or a periodic interrupt. We store the status in the
	 *	low byte and the number of interrupts received since
	 *	the last read in the remainder of rtc_irq_data.
	 */
	static int freq_2HZ = 0;
	unsigned long isr;

#ifndef HW_2HZ
	if ((rtc_freq == 2 ) && (freq_2HZ == 0))
	{	
		freq_2HZ = 1;
		/* clear RTCISR */
		isr = _reg_RTC_RTCISR;
		_reg_RTC_RTCISR = isr; 
		
		return;
	}
#endif

	freq_2HZ = 0;
	spin_lock (&rtc_lock);
	rtc_irq_data += 0x10000; /* 0x100 */
	rtc_irq_data &= ~0xffff;
	rtc_irq_data |= (_reg_RTC_RTCISR & 0xffff);
//	RTC_DB(" rtc_irq_data 0x%lx",rtc_irq_data);
	//acknowledge the interrupt 	
	isr = _reg_RTC_RTCISR;
//	RTC_DB("before setting RTCISR is 0x%lx",isr);

	_reg_RTC_RTCISR = isr; 

	isr = _reg_RTC_RTCISR;
//	RTC_DB("aftre setting RTCISR is 0x%lx",isr);

	if (rtc_status & RTC_TIMER_ON)
		mod_timer(&rtc_irq_timer, jiffies + HZ/rtc_freq + 2*HZ/100);

	spin_unlock (&rtc_lock);

	/* Now do the rest of the actions */
	wake_up_interruptible(&rtc_wait);	

	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
	
}
#endif

/*
 *	Now all the various file operations that we export.
 */

static loff_t rtc_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

#ifdef  ZHIMING_VERSION
static ssize_t rtc_read(struct file *file, char *buf,
			size_t count, loff_t *ppos)
{
	//	FUNC_START; //??? why error here?

#if !RTC_IRQ
	return -EIO;
#else
	DECLARE_WAITQUEUE(wait, current);
	unsigned long data,j;
	ssize_t retval;
	
	if (count < sizeof(unsigned long))
		return -EINVAL;
#if 0
	add_wait_queue(&rtc_wait, &wait);

	current->state = TASK_INTERRUPTIBLE;
#endif	
	
	do {
		/* First make it right. The make it fast. Putting this whole
		 * block within the parentheses of a while would be too
		 * confusing. And no, xchg() is not the answer. */
		spin_lock_irq (&rtc_lock);
		data = rtc_irq_data;
		rtc_irq_data = 0;
		spin_unlock_irq (&rtc_lock);

		if (data != 0)
			break;

		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto out;
		}
#if 0
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			goto out;
		}
		schedule();
#endif
	} while (1);
	/* wait some seconds to get the accurate watchstop */
#if 1
	j = jiffies+before_wk*HZ;
	RTC_DB("befor_wk = 0x%lx",before_wk);
	before_wk = 0;
	while(jiffies < j)
		schedule();
#endif
#if 0
	current ->timeout = j;
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	current ->timeout = 0;
#endif
	retval = put_user(data, (unsigned long *)buf); 
	if (!retval)
		retval = sizeof(unsigned long); 
 out:
#if 0
	current->state = TASK_RUNNING;
	remove_wait_queue(&rtc_wait, &wait);
#endif
	
	return retval;
#endif
}
#else //!ZHIMING_VERSION
static ssize_t rtc_read(struct file *file, char *buf,
			size_t count, loff_t *ppos)
{
	//	FUNC_START; //??? why error here?

#if !RTC_IRQ
	return -EIO;
#else
#ifdef TRY_SLEEPON
	unsigned long data;
	unsigned int j;
	ssize_t retval;

	if(count < sizeof(unsigned long))
		return -EINVAL;

	/* First make it right. The make it fast. Putting this whole
	 * block within the parentheses of a while would be too
	 * confusing. And no, xchg() is not the answer. */
	spin_lock_irq (&rtc_lock);
	data = rtc_irq_data;
	rtc_irq_data = 0;
	spin_unlock_irq (&rtc_lock);

	if(file->f_flags & O_NONBLOCK){
		if(!data)
			return -EAGAIN;
		retval = put_user(data, (unsigned long *)buf); 
		return retval; 
	}else{
		while(1){ //ensure copy to user or block
			spin_lock_irq (&rtc_lock);
			data = rtc_irq_data;
			rtc_irq_data = 0;
			spin_unlock_irq (&rtc_lock);
			if(data){
#if 1
	j = jiffies+before_wk*HZ;
	RTC_DB("befor_wk = 0x%lx",before_wk);
	before_wk = 0;
	while(jiffies < j)
		schedule();
#endif
				return put_user(data, (unsigned long *)buf);
			}else{
				interruptible_sleep_on(&rtc_wait);
				// TODO
				if(signal_pending(current))
					return -ERESTARTSYS;
			}
		}
	}
	
#else //!TRY_SLEEPON
	DECLARE_WAITQUEUE(wait, current);
	unsigned long data;
	ssize_t retval;
	
	if (count < sizeof(unsigned long))
		return -EINVAL;
	add_wait_queue(&rtc_wait, &wait);

	current->state = TASK_INTERRUPTIBLE;
	
	do {
		/* First make it right. The make it fast. Putting this whole
		 * block within the parentheses of a while would be too
		 * confusing. And no, xchg() is not the answer. */
		spin_lock_irq (&rtc_lock);
		data = rtc_irq_data;
		rtc_irq_data = 0;
		spin_unlock_irq (&rtc_lock);

		if (data != 0)
			break;

		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto out;
		}
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			goto out;
		}
		schedule();
	} while (1);
	
	retval = put_user(data, (unsigned long *)buf); 
	if (!retval)
		retval = sizeof(unsigned long); 
out: 
	current->state = TASK_RUNNING;
	remove_wait_queue(&rtc_wait, &wait);
	
	return retval;
#endif // TRY_SLEEPON
#endif // !RTC_IRQ
}
#endif // ZhimingVersion


static int rtc_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		     unsigned long arg)
{
	struct rtc_time wtime;
//	unsigned int j;
	
	;
	switch (cmd) {
#if RTC_IRQ
	case RTC_WIE_OFF:	/* mask stop watch int. enab. bit */
	{
		mask_rtc_irq_bit(RTC_SIE);
		return 0;
	}
	case RTC_WIE_ON:	/* Allow watch interrupts */
	{
		set_rtc_irq_bit(RTC_SIE);
		return 0;
	}
	case RTC_AIE_OFF:	/* Mask alarm int. enab. bit	*/
	{
		mask_rtc_irq_bit(RTC_AIE);
		return 0;
	}
	case RTC_AIE_ON:	/* Allow alarm interrupts.	*/
	{
		set_rtc_irq_bit(RTC_AIE);
		return 0;
	}
	case RTC_PIE_OFF:	/* Mask periodic int. enab. bit	*/
	{
		mask_rtc_irq_bit(rtc_freq ==1 ? rtc_freq <<4 : rtc_freq << 6);
		if (rtc_status & RTC_TIMER_ON) {
			spin_lock_irq (&rtc_lock);
			rtc_status &= ~RTC_TIMER_ON;
			del_timer(&rtc_irq_timer);
			spin_unlock_irq (&rtc_lock);
		}
		return 0;
	}
	case RTC_PIE_ON:	/* Allow periodic int in RTCISR [bit 7~bit 15]*/
	{

		/*
		 * We don't really want Joe User enabling more
		 * than 64Hz of interrupts on a multi-user machine.
		 * you can see linux\include\linux\capablities.h 
		 */
		if ((rtc_freq > 64) && (!capable(CAP_SYS_RESOURCE)))
			return -EACCES;
		
		if (!(rtc_status & RTC_TIMER_ON)) {
			spin_lock_irq (&rtc_lock);
			rtc_irq_timer.expires = jiffies + HZ/rtc_freq + 2*HZ/100;
			add_timer(&rtc_irq_timer);
			rtc_status |= RTC_TIMER_ON;
			spin_unlock_irq (&rtc_lock);
		}	
		/* rtc_freq = 1HZ RTCISR[bit :4]
		 * rtc_freq = 2HZ RTCISR[bit :7]
		 * rtc_freq = 4HZ RTCISR[bit :8]
		 * ...
		 * rtc_freq = 512HZ RTCISR[bit :15]
		 */
		//set_rtc_irq_bit(rtc_freq << 6);		
		set_rtc_irq_bit((rtc_freq ==1) ? (rtc_freq<<4): (rtc_freq<<6));
		return 0;
	}
	case RTC_UIE_OFF:	/* Mask ints from RTC updates.	*/
	{
		mask_rtc_irq_bit(RTC_UIE);//???
		return 0;
	}
	case RTC_UIE_ON:	/* Allow ints for RTC updates.	*/
	{
		set_rtc_irq_bit(RTC_UIE);
		return 0;
	}
#endif
	case RTC_WKALM_SET:
	{
		if (arg > 0x3f)
			return -EFAULT;
		spin_lock_irq(&rtc_lock);
		before_wk = _reg_RTC_SECOND & 0x3f;
		_reg_RTC_STPWCH = arg;
		spin_unlock_irq(&rtc_lock);
		return 0;
	}
	case RTC_WKALM_RD:
	{
		unsigned int val;

		val = _reg_RTC_STPWCH;

		return put_user(val, (unsigned long *)arg);
	}
	case RTC_ALM_READ:	/* Read the present alarm time */
	{
		/*
		 * This returns a struct rtc_time. Reading >= 0xc0
		 * means "don't care" or "match all". Only the tm_hour,
		 * tm_min, and tm_sec values are filled in.
		 */

		get_rtc_alm_time(&wtime);
		break; 
	}
	case RTC_ALM_SET:	/* Store a time into the alarm */
	{
		struct rtc_time tm;
		unsigned long day;

		if (copy_from_user(&tm, (struct rtc_time*)arg,
				   sizeof(struct rtc_time)))
			return -EFAULT;
		tm.tm_year += 1900;
		if (tm.tm_year < 1970 || (unsigned)tm.tm_mon >= 12 ||
			tm.tm_mday < 1 || tm.tm_mday > (days_in_mo[tm.tm_mon] +
			(tm.tm_mon == 1 && is_leap(tm.tm_year))) ||
			(unsigned)tm.tm_hour >= 24 ||
			(unsigned)tm.tm_min >= 60 ||
			(unsigned)tm.tm_sec >= 60) return -EINVAL;
		
		day = mktime(tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
		day = day - (unsigned)((tm.tm_hour*60 + tm.tm_min)*60 + tm.tm_sec);
		day = day/24/60/60;

		if(day > 65535)  return -EINVAL;

		RTC_DB("\nday=%ld hrs=%ld min=%ld sec=%ld\n", day,
			(unsigned)tm.tm_hour, (unsigned)tm.tm_min,
			(unsigned)tm.tm_sec);

		spin_lock_irq(&rtc_lock);
		_reg_RTC_DAYALARM = day;
		_reg_RTC_ALRM_HM = (unsigned)((tm.tm_hour << 8) | tm.tm_min);
		_reg_RTC_ALRM_SEC = (unsigned)tm.tm_sec;
		spin_unlock_irq(&rtc_lock);

		RTC_DB("DAY=%0d HOUR=%ld HIN=%ld SEC=%ld\n", _reg_RTC_DAYALARM,
				_reg_RTC_ALRM_HM>>8, _reg_RTC_ALRM_HM&0xff,
				_reg_RTC_ALRM_SEC);

		return 0;
	}
	case RTC_RD_TIME:	/* Read the time/date from RTC	*/
	{
		get_rtc_time(&wtime);
		break;
	}
	case RTC_SET_TIME:	/* Set the RTC */
	{
		struct rtc_time tm;
		unsigned long day;

		if (copy_from_user(&tm, (struct rtc_time*)arg,
				   sizeof(struct rtc_time)))
			return -EFAULT;
		tm.tm_year += 1900;
		if (tm.tm_year < 1970 || (unsigned)tm.tm_mon >= 12 ||
			tm.tm_mday < 1 || tm.tm_mday > (days_in_mo[tm.tm_mon] +
			(tm.tm_mon == 1 && is_leap(tm.tm_year))) ||
			(unsigned)tm.tm_hour >= 24 ||
			(unsigned)tm.tm_min >= 60 ||
			(unsigned)tm.tm_sec >= 60) return -EINVAL;
		
		day = mktime(tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
		day = day - (unsigned)((tm.tm_hour*60 + tm.tm_min)*60 + tm.tm_sec);
		day = day/24/60/60;

		if(day > 65535)  return -EINVAL;

		RTC_DB("\nday=%ld hrs=%ld min=%ld sec=%ld\n", day,
				(unsigned)tm.tm_hour, (unsigned)tm.tm_min,
				(unsigned)tm.tm_sec);

		spin_lock_irq(&rtc_lock);
		_reg_RTC_DAYR = day;
		_reg_RTC_HOURMIN = (unsigned)((tm.tm_hour << 8) | tm.tm_min);
		_reg_RTC_SECOND = (unsigned)tm.tm_sec;
		spin_unlock_irq(&rtc_lock);

		RTC_DB("DAY=%0d HOUR=%ld HIN=%ld SEC=%ld\n", _reg_RTC_DAYR,
				_reg_RTC_HOURMIN>>8, _reg_RTC_HOURMIN&0xff,
				_reg_RTC_SECOND);
		
		return 0;
	}
#if RTC_IRQ
	case RTC_IRQP_READ:	/* Read the periodic IRQ rate.	*/
	{
		return put_user(rtc_freq, (unsigned long *)arg);
	}
	case RTC_IRQP_SET:	/* Set periodic IRQ rate to rtc_freq */
	{
		int tmp = 0;

		/* 
		 * The range we can do is 1,2,4,8.. 512Hz.
		 */
		if (((arg < 2) || (arg > 512))&& arg != 1)
			return -EINVAL;

		while (arg > (1<<tmp))/* get the power of arg */ 
			tmp++;

		/*
		 * Check that the input was really a power of 2.
		 */
		if (arg != (1<<tmp))
			return -EINVAL;

		spin_lock_irq(&rtc_lock);
		rtc_freq = arg;
		spin_unlock_irq(&rtc_lock);
		return 0;
	}
#elif !defined(CONFIG_DECSTATION)
	case RTC_EPOCH_READ:	/* Read the epoch.	*/
	{
		return put_user (epoch, (unsigned long *)arg);
	}
#endif
	default:
		return -EINVAL;
	}
	
	
	return copy_to_user((void *)arg, &wtime, sizeof wtime) ? -EFAULT : 0;
}

/*
 *	We enforce only one user at a time here with the open/close.
 *	Also clear the previous interrupt data on an open, and clean
 *	up things on a close.
 */

/* We use rtc_lock to protect against concurrent opens. So the BKL is not
 * needed here. Or anywhere else in this driver. */
static int rtc_open(struct inode *inode, struct file *file)
{
	
	spin_lock_irq (&rtc_lock);

	if(rtc_status & RTC_IS_OPEN)
		goto out_busy;

	rtc_status |= RTC_IS_OPEN;

	rtc_irq_data = 0;
	spin_unlock_irq (&rtc_lock);
	
	return 0;

out_busy:
	spin_unlock_irq (&rtc_lock);
	return -EBUSY;

}

static int rtc_fasync (int fd, struct file *filp, int on)

{
	return fasync_helper (fd, filp, on, &rtc_async_queue);
}

static int rtc_release(struct inode *inode, struct file *file)
{
	
#if RTC_IRQ
	/*
	 * Turn off all interrupts once the device is no longer
	 * in use, and clear the data.
	 */

	spin_lock_irq(&rtc_lock);
	if (rtc_status & RTC_TIMER_ON) {
		rtc_status &= ~RTC_TIMER_ON;
		del_timer(&rtc_irq_timer);
	}
	spin_unlock_irq(&rtc_lock);

	if (file->f_flags & FASYNC) {
		rtc_fasync (-1, file, 0);
	}
#endif

	spin_lock_irq (&rtc_lock);
	rtc_irq_data = 0;
	spin_unlock_irq (&rtc_lock);

	/* No need for locking -- nobody else can do anything until this rmw is
	 * committed, and no timer is running. */
	rtc_status &= ~RTC_IS_OPEN;
	
	return 0;
}

#if RTC_IRQ
/* Called without the kernel lock - fine */
static unsigned int rtc_poll(struct file *file, poll_table *wait)
{
	unsigned long l;

	poll_wait(file, &rtc_wait, wait);

	spin_lock_irq (&rtc_lock);
	l = rtc_irq_data;
	spin_unlock_irq (&rtc_lock);

	if (l != 0)
		return POLLIN | POLLRDNORM;
	return 0;
}
#endif

/*
 *	The various file operations we support.
 */

static struct file_operations rtc_fops = {
	owner:		THIS_MODULE,
	llseek:		rtc_llseek,
	read:		rtc_read,
#if RTC_IRQ
	poll:		rtc_poll,
#endif
	ioctl:		rtc_ioctl,
	open:		rtc_open,
	release:	rtc_release,
	fasync:		rtc_fasync,
};

static struct miscdevice rtc_dev=
{
	RTC_MINOR,
	"rtc",
	&rtc_fops
};

static int __init rtc_init(void)
{
	_reg_CRM_PCCR1 |= 0x20000000; //enable rtc module
#ifdef KHz32768	
	_reg_RTC_RTCCTL = 0x01;
	_reg_RTC_RTCCTL = 0x80;
#else
	_reg_RTC_RTCCTL = 0x01;
	_reg_RTC_RTCCTL = 0xa0;
#endif

	_reg_RTC_DAYR = mktime(1970, 1, 1, 0, 0, 0)/24/60/60;
	_reg_RTC_HOURMIN = 0;
	_reg_RTC_SECOND = 0;
	RTC_DB("DAY=%0d HOUR=%ld MIN=%ld SEC=%ld\n", _reg_RTC_DAYR,
			_reg_RTC_HOURMIN>>8, _reg_RTC_HOURMIN&0xff,
			_reg_RTC_SECOND);

#if RTC_IRQ
	if(request_irq(RTC_IRQ, 
			rtc_interrupt, 
			SA_SHIRQ | SA_INTERRUPT, 
			"rtc", 
			"rtc"))
	{
		/* Yeah right, seeing as irq 22 doesn't even hit the bus. */
		printk(KERN_ERR "rtc: IRQ %d is not free.\n", RTC_IRQ);
		return -EIO;
	}
#endif

	misc_register(&rtc_dev);
	create_proc_read_entry ("driver/rtc", 0, 0, rtc_read_proc, NULL);

#if RTC_IRQ
	init_timer(&rtc_irq_timer);
	rtc_irq_timer.function = rtc_dropped_irq;
	spin_lock_irq(&rtc_lock);
	spin_unlock_irq(&rtc_lock);
	rtc_freq = 512;
#endif
#ifdef TRY_SLEEPON
	init_waitqueue_head(&rtc_wait);
#endif 

	printk(KERN_INFO "Real Time Clock Driver v" RTC_VERSION "\n");
	
	return 0;
}

static void __exit rtc_exit (void)
{
	
	remove_proc_entry ("driver/rtc", NULL);
	misc_deregister(&rtc_dev);

#if RTC_IRQ
	free_irq (RTC_IRQ, "rtc");
#endif

}

module_init(rtc_init);
module_exit(rtc_exit);
EXPORT_NO_SYMBOLS;

#if RTC_IRQ
static void rtc_dropped_irq(unsigned long data)
{
	unsigned long freq;
	
	
	spin_lock_irq (&rtc_lock);

	/* Just in case someone disabled the timer from behind our back... */
	if (rtc_status & RTC_TIMER_ON)
		mod_timer(&rtc_irq_timer, jiffies + HZ/rtc_freq + 2*HZ/100);

	rtc_irq_data += ((rtc_freq/HZ)<<16);//((rtc_freq/HZ)<<8);
	rtc_irq_data &= ~0xffff;
	
	rtc_irq_data |= (_reg_RTC_RTCISR & 0xffff);

	freq = rtc_freq;

	spin_unlock_irq(&rtc_lock);

//	printk(KERN_WARNING "rtc: lost some interrupts at %ldHz.\n", freq);

	/* Now we have new data */
	wake_up_interruptible(&rtc_wait);

	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
	
}
#endif

/*
 *	Info exported via "/proc/driver/rtc".
 */

static int rtc_proc_output (char *buf)
{
	char *p;
	struct rtc_time tm;
	unsigned long freq;

	spin_lock_irq(&rtc_lock);
	freq = rtc_freq;
	spin_unlock_irq(&rtc_lock);

	p = buf;

	get_rtc_time(&tm);

	/*
	 * There is no way to tell if the luser has the RTC set for local
	 * time or for Universal Standard Time (GMT). Probably local though.
	 */
	p += sprintf(p,
		     "rtc_time\t: %02d:%02d:%02d\n"
		     "rtc_date\t: %04d-%02d-%02d\n"
	 	     "rtc_epoch\t: %04lu\n",
		     tm.tm_hour, tm.tm_min, tm.tm_sec,
		     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, epoch);

	get_rtc_alm_time(&tm);

	/*
	 * We implicitly assume 24hr mode here. Alarm values >= 0xc0 will
	 * match any value for that particular field. Values that are
	 * greater than a valid time, but less than 0xc0 shouldn't appear.
	 */
	p += sprintf(p, "alarm\t\t: ");
	if (tm.tm_hour <= 24)
		p += sprintf(p, "%02d:", tm.tm_hour);
	else
		p += sprintf(p, "**:");

	if (tm.tm_min <= 59)
		p += sprintf(p, "%02d:", tm.tm_min);
	else
		p += sprintf(p, "**:");

	if (tm.tm_sec <= 59)
		p += sprintf(p, "%02d\n", tm.tm_sec);
	else
		p += sprintf(p, "**\n");
	return  p - buf;
}

static int rtc_read_proc(char *page, char **start, off_t off,
                         int count, int *eof, void *data)
{
        int len = rtc_proc_output (page);
        if (len <= off+count) *eof = 1;
        *start = page + off;
        len -= off;
        if (len>count) len = count;
        if (len<0) len = 0;
        return len;
}

static void decodetime (unsigned long t, struct rtc_time *tval)
{
	long days, month, year, rem;

	days = t / 86400;
	rem = t % 86400;
	tval->tm_hour = rem / 3600;
	rem %= 3600;
	tval->tm_min = rem / 60;
	tval->tm_sec = rem % 60;
	tval->tm_wday = (4 + days) % 7;

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

	year = 1970 + days / 365;
	days -= ((year - 1970) * 365
			+ LEAPS_THRU_END_OF (year - 1)
			- LEAPS_THRU_END_OF (1970 - 1));
	if (days < 0) {
		year -= 1;
		days += 365 + is_leap(year);
	}
	tval->tm_year = year - 1900;
	tval->tm_yday = days + 1;

	month = 0;
	if (days >= 31) {
		days -= 31;
		month++;
		if (days >= (28 + is_leap(year))) {
			days -= (28 + is_leap(year));
			month++;
			while (days >= days_in_mo[month]) {
				days -= days_in_mo[month];
				month++;
			}
		}
	}
	tval->tm_mon = month;
	tval->tm_mday = days + 1;
}

static void get_rtc_time(struct rtc_time *rtc_tm)
{
	unsigned long dayr, hour, min, sec;

	spin_lock_irq(&rtc_lock);
	sec = _reg_RTC_SECOND;
	min = _reg_RTC_HOURMIN & 0x3f;
	hour = (_reg_RTC_HOURMIN >> 8) & 0x1f;
	dayr = _reg_RTC_DAYR;
	spin_unlock_irq(&rtc_lock);

	decodetime((((dayr*24 + hour)*60 + min)*60 + sec), rtc_tm);
}

static void get_rtc_alm_time(struct rtc_time *alm_tm)
{
	unsigned long dayr, hour, min, sec;

	spin_lock_irq(&rtc_lock);
	sec = _reg_RTC_ALRM_SEC;
	min = _reg_RTC_ALRM_HM & 0x3f;
	hour = (_reg_RTC_ALRM_HM >> 8) & 0x1f;
	dayr = _reg_RTC_DAYALARM;
	spin_unlock_irq(&rtc_lock);

	decodetime((((dayr*24 + hour)*60 + min)*60 + sec), alm_tm);
}

#if RTC_IRQ
/*
 * Used to disable/enable interrupts for any one of UIE, AIE, PIE.
 * Rumour has it that if you frob the interrupt enable/disable
 * bits in RTC_CONTROL, you should read RTC_INTR_FLAGS, to
 * ensure you actually start getting interrupts. Probably for
 * compatibility with older/broken chipset RTC implementations.
 * We also clear out any old irq data after an ioctl() that
 * meddles with the interrupt enable/disable bits.
 */

static void mask_rtc_irq_bit(unsigned int bit)
{
	spin_lock_irq(&rtc_lock);
	/* for 2hz cant work well we use 4hz to simulate it */
	if (bit == 0x80) //???
		bit = 0x100;

//	WRITEREG(RTCIENR, READREG(RTCIENR) & ~bit);
	
	//_reg_RTC_RTCIENR = 0; //???
	_reg_RTC_RTCIENR &= ~bit;
	rtc_irq_data = 0;
	spin_unlock_irq(&rtc_lock);
}

static void set_rtc_irq_bit(unsigned int bit)
{

	spin_lock_irq(&rtc_lock);
	/* use 4 hz to simulate 2hz */

#ifndef HW_2HZ
	if (bit == 0x80){
		bit = 0x100;
	}
#endif

	_reg_RTC_RTCIENR |= bit;
	
	rtc_irq_data = 0;
	spin_unlock_irq(&rtc_lock);
}
#endif
