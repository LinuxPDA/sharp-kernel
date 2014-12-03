/*
 * linux/drivers/char/ms77xxxx_rtc.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * RTC driver for ms77xxxx_
 *
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <linux/poll.h>
#include <linux/string.h>
#include <linux/rtc.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
#include <asm/ms7760cp.h>
#else
#error No Platform
#endif

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

#define	DRIVER_VERSION	"0.01"

static char *rtc_devname = "rtc";

static int rtc_irq_data	= 0;

static struct fasync_struct* ms77xxxx_rtc_async_queue;

static DECLARE_WAIT_QUEUE_HEAD(ms77xxxx_rtc_wait);

static ssize_t ms77xxxx_rtc_read(struct file *file, char *buf,
			size_t count, loff_t *ppos);

static int ms77xxxx_rtc_ioctl(struct inode *inode, struct file *file,
                        unsigned int cmd, unsigned long arg);

static unsigned int ms77xxxx_rtc_poll(struct file *file, poll_table *wait);

void ms77xxxx_rtc_get_time(struct rtc_time *rtc_tm);
int ms77xxxx_rtc_set_time(struct rtc_time *rtc_tm);

static spinlock_t ms77xxxx_rtc_lock = SPIN_LOCK_UNLOCKED;

static unsigned long epoch = 2000;      /* year corresponding to 0x00   */


/*
 *	Bits in rtc_status. (7 bits of room for future expansion)
 */
#define PT_RTCS 0x01
#define PT_ARMS 0x02
#define PT_1SEC 0x04
//#define PT_HSEC 0x08

#define CTRL_ARI	0x02
#define CTRL_1SEC	0x04


#define RTC_IS_OPEN			0x01	/* means /dev/rtc is in use	*/
#define RTC_TIMER_ON		0x02	/* missed irq timer active	*/

unsigned char ms77xxxx_rtc_status;		/* bitmapped status byte.	*/
unsigned long ms77xxxx_rtc_freq;		/* Current periodic IRQ rate	*/

unsigned char days_in_mo[] =
{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


static void sh_rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char rtkisr;
	unsigned char ch;

	spin_lock (&ms77xxxx_rtc_lock);

					// SYSTEM STATUS
	h8_in(&rtkisr, 1, SH7760SE_RTKISR);

	if (rtkisr & PT_RTCS){ // RTC STAT
		h8_in(&ch, 1, SH7760SE_RTC_RTCSR);
		if (ch & PT_1SEC){
			ch &= (~PT_1SEC & 0xff);
			h8_out(&ch, 1, SH7760SE_RTC_RTCSR);
			rtc_irq_data = 1;
		}
		if (ch & PT_ARMS){
			ch &= (~PT_ARMS & 0xff);
			h8_out(&ch, 1, SH7760SE_RTC_RTCSR);
			rtc_irq_data = 2;
		}
		rtkisr &= ~PT_RTCS;
		h8_out(&rtkisr, 1, MS7760CP_RTKISR);
	}

	spin_unlock (&ms77xxxx_rtc_lock);
	wake_up_interruptible(&ms77xxxx_rtc_wait);

	kill_fasync (&ms77xxxx_rtc_async_queue, SIGIO, POLL_IN);
}


/*
 *	Now all the various file operations that we export.
 */

static int ms77xxxx_rtc_fasync (int fd, struct file *filp, int on)
{
        return fasync_helper (fd, filp, on, &ms77xxxx_rtc_async_queue);
}

static loff_t ms77xxxx_rtc_llseek(struct file *file, loff_t offset, int origin)
{
        return -ESPIPE;
}

static ssize_t ms77xxxx_rtc_read(struct file *file, char *buf,
                           size_t count, loff_t *ppos)
{
        DECLARE_WAITQUEUE(wait, current);
        unsigned long data;
        ssize_t retval;

	if (count < sizeof(unsigned long))
		return -EINVAL;

	add_wait_queue(&ms77xxxx_rtc_wait, &wait);

	current->state = TASK_INTERRUPTIBLE;

	do {
		spin_lock_irq (&ms77xxxx_rtc_lock);
		data = rtc_irq_data;
		spin_unlock_irq (&ms77xxxx_rtc_lock);

		if (data != 0) {
			rtc_irq_data = 0;
			break;
		}

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
	remove_wait_queue(&ms77xxxx_rtc_wait, &wait);

        return retval;
}

static void control_periodic_irq(int mode)
{
	unsigned char wk;
	unsigned char ch;

	h8_in(&ch, 1, SH7760SE_RTC_RTCCR);

	if (mode == 0){		/* OFF */
		wk	= ch & (~CTRL_1SEC & 0xff);
	}else{			/* ON  */
		wk	= ch | CTRL_1SEC;
	}
	h8_out(&wk, 1, SH7760SE_RTC_RTCCR);
	rtc_irq_data = 0;
}

static void control_alarm_irq(int mode)
{
	unsigned char wk;
	unsigned char ch;

	h8_in(&ch, 1, SH7760SE_RTC_RTCCR);

	if (mode == 0){		/* OFF */
		wk	= ch & (~CTRL_ARI & 0xff);
	}else{			/* ON  */
		wk	= ch | CTRL_ARI;
	}
	h8_out(&wk, 1, SH7760SE_RTC_RTCCR);
	rtc_irq_data = 0;
}

static void get_rtc_data(struct rtc_time *rtc_tm)
{
	unsigned char ch;

	h8_in(&ch, 1, SH7760SE_RTC_SECCNT);
	rtc_tm->tm_sec = ch;
	h8_in(&ch, 1, SH7760SE_RTC_MINCNT);
	rtc_tm->tm_min = ch;
	h8_in(&ch, 1, SH7760SE_RTC_HRCNT);
	rtc_tm->tm_hour = ch;
	h8_in(&ch, 1, SH7760SE_RTC_WKCNT);
	rtc_tm->tm_wday = ch;
	h8_in(&ch, 1, SH7760SE_RTC_DAYCNT);
	rtc_tm->tm_mday = ch;
	h8_in(&ch, 1, SH7760SE_RTC_MONCNT);
	rtc_tm->tm_mon = ch;
	h8_in(&ch, 1, SH7760SE_RTC_YRCNT);
	rtc_tm->tm_year = ch;

	BCD_TO_BIN(rtc_tm->tm_sec);
	BCD_TO_BIN(rtc_tm->tm_min);
	BCD_TO_BIN(rtc_tm->tm_hour);
	BCD_TO_BIN(rtc_tm->tm_wday);
	BCD_TO_BIN(rtc_tm->tm_mday);
	BCD_TO_BIN(rtc_tm->tm_mon);
	BCD_TO_BIN(rtc_tm->tm_year);
}

static void get_rtc_alarm_data(struct rtc_time *rtc_tm)
{
	unsigned char ch;

	h8_in(&ch, 1, SH7760SE_RTC_SECAR);
	rtc_tm->tm_sec = ch;
	h8_in(&ch, 1, SH7760SE_RTC_MINAR);
	rtc_tm->tm_min = ch;
	h8_in(&ch, 1, SH7760SE_RTC_HRAR);
	rtc_tm->tm_hour = ch;
	h8_in(&ch, 1, SH7760SE_RTC_WKAR);
	rtc_tm->tm_wday = ch;
	h8_in(&ch, 1, SH7760SE_RTC_DAYAR);
	rtc_tm->tm_mday = ch;
	h8_in(&ch, 1, SH7760SE_RTC_MONAR);
	rtc_tm->tm_mon = ch;
	h8_in(&ch, 1, SH7760SE_RTC_YRCNT);
	rtc_tm->tm_year = ch;

	BCD_TO_BIN(rtc_tm->tm_sec);
	BCD_TO_BIN(rtc_tm->tm_min);
	BCD_TO_BIN(rtc_tm->tm_hour);
	BCD_TO_BIN(rtc_tm->tm_wday);
	BCD_TO_BIN(rtc_tm->tm_mday);
	BCD_TO_BIN(rtc_tm->tm_mon);
	BCD_TO_BIN(rtc_tm->tm_year);
}

static void set_rtc_data(struct rtc_time *rtc_tm)
{
	unsigned char ch;

	unsigned char mon, day, hrs, min, sec;
	unsigned int yrs;

	yrs = rtc_tm->tm_year + epoch;
	mon = rtc_tm->tm_mon;
	day = rtc_tm->tm_mday;
	hrs = rtc_tm->tm_hour;
	min = rtc_tm->tm_min;
	sec = rtc_tm->tm_sec;

	BIN_TO_BCD(sec);
	BIN_TO_BCD(min);
	BIN_TO_BCD(hrs);
	BIN_TO_BCD(day);
	BIN_TO_BCD(mon);
	BIN_TO_BCD(yrs);

	ch = 0x01;	/* RTC stop */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);

	ch = sec;
	h8_out(&ch, 1, SH7760SE_RTC_SECCNT);
	ch = min;
	h8_out(&ch, 1, SH7760SE_RTC_MINCNT);
	ch = hrs;
	h8_out(&ch, 1, SH7760SE_RTC_HRCNT);
	ch = day;
	h8_out(&ch, 1, SH7760SE_RTC_DAYCNT);
	ch = mon;
	h8_out(&ch, 1, SH7760SE_RTC_MONCNT);
	ch = yrs;
	h8_out(&ch, 1, SH7760SE_RTC_YRCNT);

	ch = 0x21;	/* update */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);
	ch = 0x00;	/* start */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);
}

static void set_rtc_alarm_data(struct rtc_time *rtc_tm)
{
	unsigned char ch;

	unsigned char mon, day, hrs, min, sec;

	mon = rtc_tm->tm_mon;
	day = rtc_tm->tm_mday;
	hrs = rtc_tm->tm_hour;
	min = rtc_tm->tm_min;
	sec = rtc_tm->tm_sec;

	BIN_TO_BCD(sec);
	BIN_TO_BCD(min);
	BIN_TO_BCD(hrs);
	BIN_TO_BCD(day);
	BIN_TO_BCD(mon);

	ch = 0x01;	/* RTC stop */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);

	ch = sec | 0x80;
	h8_out(&ch, 1, SH7760SE_RTC_SECAR);
	ch = min | 0x80;
	h8_out(&ch, 1, SH7760SE_RTC_MINAR);
	ch = hrs | 0x80;
	h8_out(&ch, 1, SH7760SE_RTC_HRAR);
	ch = day | 0x80;
	h8_out(&ch, 1, SH7760SE_RTC_DAYAR);
	ch = mon | 0x80;
	h8_out(&ch, 1, SH7760SE_RTC_MONAR);
	ch = 0;
	h8_out(&ch, 1, SH7760SE_RTC_WKAR);

	ch = 0x21;	/* update */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);
	ch = 0x00;	/* start */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);
}

static int ms77xxxx_rtc_ioctl(struct inode *inode, struct file *file,
                        unsigned int cmd, unsigned long arg)
{
	struct rtc_time	wtime,rtc_tm;
	unsigned char	mon,day,hrs,min,sec,leap_yr;
	unsigned int	yrs;

	wtime.tm_sec	= 0;
	wtime.tm_min	= 0;
	wtime.tm_hour	= 0;
	wtime.tm_mday	= 0;
	wtime.tm_mon	= 0;
	wtime.tm_year	= 0;
	wtime.tm_wday	= 0;
	wtime.tm_yday	= 0;
	wtime.tm_isdst	= 0;

	switch (cmd) {
	case RTC_AIE_OFF:
		spin_lock_irq(&ms77xxxx_rtc_lock);
		control_alarm_irq(0);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		return 0;

	case RTC_AIE_ON:
		spin_lock_irq(&ms77xxxx_rtc_lock);
		control_alarm_irq(1);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		return 0;

	case RTC_PIE_OFF:
		spin_lock_irq(&ms77xxxx_rtc_lock);
		control_periodic_irq(0);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		return 0;

	case RTC_PIE_ON:
		spin_lock_irq(&ms77xxxx_rtc_lock);
		control_periodic_irq(1);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		return 0;

	case RTC_UIE_OFF:
		spin_lock_irq(&ms77xxxx_rtc_lock);
		control_periodic_irq(0);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		return 0;

	case RTC_UIE_ON:
		spin_lock_irq(&ms77xxxx_rtc_lock);
		control_periodic_irq(1);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		return 0;

	case RTC_ALM_READ:
		get_rtc_alarm_data(&wtime);
		break;

	case RTC_ALM_SET:
		if (copy_from_user(&rtc_tm, (struct rtc_time*)arg,
                                   sizeof(struct rtc_time)))
			return -EFAULT;

		yrs = rtc_tm.tm_year + epoch;
		mon = rtc_tm.tm_mon;
		day = rtc_tm.tm_mday;
		hrs = rtc_tm.tm_hour;
		min = rtc_tm.tm_min;
		sec = rtc_tm.tm_sec;

		if (yrs < epoch){
			return -EINVAL;
		}
		leap_yr = ((!(yrs % 4) && (yrs % 100)) || !(yrs % 400));
		if ((mon > 12) || (day == 0)){
			return -EINVAL;
		}
		if (day > (days_in_mo[mon] + ((mon == 2) && leap_yr))){
			return -EINVAL;
		}
		if ((hrs >= 24) || (min >= 60) || (sec >= 60)){
			return -EINVAL;
		}

		/* update the alarm register */
		spin_lock_irq(&ms77xxxx_rtc_lock);
		wtime.tm_sec	= sec;
		wtime.tm_min	= min;
		wtime.tm_hour	= hrs;
		wtime.tm_mday	= day;
		wtime.tm_mon	= mon;
		set_rtc_alarm_data(&wtime);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		return 0;

	case RTC_RD_TIME:
		spin_lock_irq(&ms77xxxx_rtc_lock);
		get_rtc_data(&wtime);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		break;

	case RTC_SET_TIME:
		if (copy_from_user(&rtc_tm, (struct rtc_time*)arg,
                                   sizeof(struct rtc_time)))
			return -EFAULT;

		yrs = rtc_tm.tm_year + epoch;
		mon = rtc_tm.tm_mon;
		day = rtc_tm.tm_mday;
		hrs = rtc_tm.tm_hour;
		min = rtc_tm.tm_min;
		sec = rtc_tm.tm_sec;

		if (yrs < epoch){
			return -EINVAL;
		}
		leap_yr = ((!(yrs % 4) && (yrs % 100)) || !(yrs % 400));
		if ((mon > 12) || (day == 0)){
			return -EINVAL;
		}
		if (day > (days_in_mo[mon] + ((mon == 2) && leap_yr))){
			return -EINVAL;
		}
		if ((hrs >= 24) || (min >= 60) || (sec >= 60)){
			return -EINVAL;
		}
		if ((yrs - epoch) > 255){
			return -EINVAL;
		}

		spin_lock_irq(&ms77xxxx_rtc_lock);
		wtime.tm_sec	= sec;
		wtime.tm_min	= min;
		wtime.tm_hour	= hrs;
		wtime.tm_mday	= day;
		wtime.tm_mon	= mon;
		wtime.tm_year	= yrs - epoch;
		set_rtc_data(&wtime);
		spin_unlock_irq(&ms77xxxx_rtc_lock);
		return 0;

	case RTC_IRQP_READ:
		return put_user(1, (unsigned long *)arg);

	case RTC_IRQP_SET:
		/* Changing the periodic frequency is not supported */
		return -EACCES;

	case RTC_EPOCH_READ:
		return put_user (2000, (unsigned long *)arg);

	case RTC_EPOCH_SET:
		/* Changing the epoch is not supprted */
		return -EACCES;

	default:
		return -EINVAL;
	}

        return copy_to_user((void*)arg, &wtime, sizeof(wtime)) ? -EFAULT : 0;
}

/*
 *	We enforce only one user at a time here with the open/close.
 *	Also clear the previous interrupt data on an open, and clean
 *	up things on a close.
 */

static int ms77xxxx_rtc_open(struct inode *inode, struct file *file)
{
	if (ms77xxxx_rtc_status & RTC_IS_OPEN)
		goto out_busy;

	MOD_INC_USE_COUNT;

	ms77xxxx_rtc_status |= RTC_IS_OPEN;

	return 0;

out_busy:
	return -EBUSY;
}

static int ms77xxxx_rtc_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;

	ms77xxxx_rtc_status &= ~RTC_IS_OPEN;

	return 0;
}

static unsigned int ms77xxxx_rtc_poll(struct file *file, poll_table *wait)
{
        unsigned long l;

	poll_wait(file, &ms77xxxx_rtc_wait, wait);

	spin_lock_irq(&ms77xxxx_rtc_lock);
	l = rtc_irq_data;
	spin_unlock_irq(&ms77xxxx_rtc_lock);

	if (l != 0){
		return POLLIN | POLLRDNORM;
	}

        return 0;
}

#ifdef CONFIG_PROC_FS

static int sh_rtc_proc_output(char *buf)
{
    char		*p;
    struct rtc_time	tm;
	unsigned char ch;

	get_rtc_data(&tm);

	p = buf;
	p += sprintf(p,
                     "rtc_time\t: %02d:%02d:%02d\n"
                     "rtc_date\t: %04d-%02d-%02d\n"
                     "rtc_epoch\t: %04lu\n",
                     (int)tm.tm_hour, (int)tm.tm_min, (int)tm.tm_sec,
                     (int)tm.tm_year + (int)epoch, (int)tm.tm_mon,
		     (int)tm.tm_mday, (unsigned long)epoch);

	get_rtc_alarm_data(&tm);

	p += sprintf(p,
		     "alrm_time\t: %02d:%02d:%02d\n"
                     "alrm_date\t: %04d-%02d-%02d\n",
                     (int)tm.tm_hour, (int)tm.tm_min, (int)tm.tm_sec,
                     (int)tm.tm_year + (int)epoch,
		     (int)tm.tm_mon, (int)tm.tm_mday);

	h8_in(&ch, 1, SH7760SE_RTC_RTCSR);

	p += sprintf(p,"alarm_IRQ\t: %s\n",
		     (ch & PT_ARMS) ? "yes" : "no" );
	p += sprintf(p,"periodic_IRQ\t: %s\n",
		     (ch & PT_1SEC) ? "yes" : "no" );

	p += sprintf(p,"periodic_freq\t: 1\n");
	p += sprintf(p,"batt_status\t: okay\n");

        return (p - buf);
}

static int sh_rtc_read_proc(char*  page,
			       char** start,
			       off_t  off,
			       int    count,
			       int*   eof,
			       void*  data)
{
        int len = sh_rtc_proc_output(page);

	if (len <= off + count) { *eof = 1; }
	*start = page + off;
	len -= off;
	if (len > count) { len = count; }
	if (len < 0)     { len = 0; }

        return len;
}

#endif

/*
 *	The various file operations we support.
 */

static struct file_operations ms77xxxx_rtc_fops = {
	owner:		THIS_MODULE,
	llseek:		ms77xxxx_rtc_llseek,
	read:		ms77xxxx_rtc_read,
	poll:		ms77xxxx_rtc_poll,
	ioctl:		ms77xxxx_rtc_ioctl,
	open:		ms77xxxx_rtc_open,
	release:	ms77xxxx_rtc_release,
	fasync:		ms77xxxx_rtc_fasync,
};

static struct miscdevice ms77xxxx_rtc_dev=
{
	RTC_MINOR,
	"rtc",
	&ms77xxxx_rtc_fops
};

int __init ms77xxxx_rtc_init(void)
{
	int ret;

	misc_register(&ms77xxxx_rtc_dev);

#ifdef CONFIG_PROC_FS
	create_proc_read_entry("driver/rtc", 0, 0, sh_rtc_read_proc, NULL);
#endif

	if ((ret = request_irq(SH7760SE_IRQ_H8IRQ, sh_rtc_interrupt,
			       SA_SHIRQ | SA_INTERRUPT, "rtc", rtc_devname))) {
			printk(KERN_INFO "could not get IRQ\n");
		return ret;
	}

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 RTC driver initialized.\n"); 
#else
#error No Platform
#endif

	return 0;
}

#if 0
static char *days[] = {
	"***", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
#endif

#ifdef MODULE
void __exit ms77xxxx_rtc_cleanup(void)
{
#if 0
	spin_lock_irq(&ms77xxxx_rtc_lock);
	ch = 0x01;	/* RTC stop */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);
	spin_unlock_irq(&ms77xxxx_rtc_lock);
#endif
	free_irq(SH7760SE_IRQ_H8IRQ,rtc_devname);

#ifdef CONFIG_PROC_FS
	remove_proc_entry("driver/rtc", NULL);
#endif

	unregister_chrdev(RTC_MINOR, "rtc");

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 RTC driver removed.\n"); 
#else
#error No Platform
#endif
}
#endif

module_init(ms77xxxx_rtc_init);
#ifdef MODULE
module_exit(ms77xxxx_rtc_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
MODULE_DESCRIPTION("RTC driver for MS7760CP01");
#else
#error No Platform
#endif


