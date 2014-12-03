/*
 *  Real Time Clock interface for Linux on MQ9000 (ARM922T core)
 *
 *  Copyright (c) 2003 LINEO Japan, Inc.
 *
 *  Based on rtc.c & sa1100-rtc.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <asm/bitops.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <linux/rtc.h>
#include <asm/arch/MQ9000Regs.h>
#include <asm/arch/mq9000_rtc.h>

#define	DRIVER_VERSION		"1.00"

#define TIMER_FREQ		(4358144 / 2) // TIMER0 133MHz/32 PLL2 ??
//#define TIMER_FREQ		4358144 // TIMER1 133MHz/32 PLL2
//#define TIMER_FREQ		(3686400 / 2) // TIMER 0 Oscillator ??
//#define TIMER_FREQ		3686400 // TIMER1 Oscillator
#define TIMER_MISSED		(TIMER_FREQ / 21)

/* Those are the bits from a classic RTC we want to mimic */
#define RTC_IRQF		0x80	/* any of the following 3 is active */
#define RTC_PF			0x40
#define RTC_AF			0x20
#define RTC_UF			0x10

static unsigned long rtc_status;
static unsigned long rtc_irq_data;
static unsigned long rtc_freq = 1024;

static struct timer_list rtc_irq_timer;
static struct fasync_struct *rtc_async_queue;
static DECLARE_WAIT_QUEUE_HEAD(rtc_wait);

extern spinlock_t rtc_lock;


static int rtdayofweek(struct rtc_time *tm)
{
	int d = (tm->tm_year - 70) * 365
			+ LEAPS_THRU_END_OF(tm->tm_year - 1 + 1900)
			- LEAPS_THRU_END_OF(1970 - 1);
	d += dayofyear(tm->tm_year, tm->tm_mon, tm->tm_mday) - 1;
	return ((4 + d) % 7);
}

static unsigned long rttimeofday(struct rtc_time *tm)
{
	int rt;
	rt = (tm->tm_hour << 16) & 0xff0000;
	rt |= (tm->tm_min << 8) & 0xff00;
	rt |= tm->tm_sec & 0xff;
	return rt;
}

static unsigned long rtdate(struct rtc_time *tm)
{
	int rt;
	rt = ((tm->tm_year - 70) << 24) & 0xff000000;
	rt |= ((tm->tm_mon + 1) << 16) & 0xff0000;
	rt |= (tm->tm_mday << 8) & 0xff00;
	rt |= (rtdayofweek(tm) + 1) & 0xff;
	return rt;
}

static void rtc_periodic_irq(unsigned long data)
{
	rtc_irq_data |= (RTC_UF|RTC_IRQF);
	rtc_irq_data += 0x100;
	rtc_irq_timer.expires = jiffies + HZ;
	add_timer(&rtc_irq_timer);

	/* wake up waiting process */
	wake_up_interruptible(&rtc_wait);
	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
}

static void rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	unsigned int rtsr = pMQRegs->rt.rt09.IntStatus;

	if (rtsr & MQ_RTC_RTM_INT)
		rtc_irq_data |= (RTC_AF|RTC_IRQF);
	rtc_irq_data += 0x100;

	/* wake up waiting process */
	wake_up_interruptible(&rtc_wait);
	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
}

static void timer0_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	pMQRegs->gp.gp05.Timer0Cmp += (TIMER_FREQ - TIMER_MISSED) / rtc_freq;
	rtc_irq_data += (0x100|RTC_PF|RTC_IRQF);

	wake_up_interruptible(&rtc_wait);
	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
}

static int rtc_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit (1, &rtc_status))
		return -EBUSY;
	rtc_irq_data = 0;
	return 0;
}

static int rtc_release(struct inode *inode, struct file *file)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	spin_lock_irq (&rtc_lock);
	pMQRegs->rt.rt08.IntEnable &= ~MQ_RTC_RTM_INT;
	pMQRegs->rt.rt09.IntStatus = MQ_RTC_RTM_INT;
	disable_irq(IRQNO_TIMER0);
	spin_unlock_irq (&rtc_lock);
	rtc_status = 0;
	return 0;
}

static int rtc_fasync (int fd, struct file *filp, int on)
{
	return fasync_helper (fd, filp, on, &rtc_async_queue);
}

static unsigned int rtc_poll(struct file *file, poll_table *wait)
{
	poll_wait (file, &rtc_wait, wait);
	return (rtc_irq_data) ? 0 : POLLIN | POLLRDNORM;
}

static loff_t rtc_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

ssize_t rtc_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long data;
	ssize_t retval;

	if (count < sizeof(unsigned long))
		return -EINVAL;

	add_wait_queue(&rtc_wait, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	for (;;) {
		spin_lock_irq (&rtc_lock);
		data = rtc_irq_data;
		if (data != 0) {
			rtc_irq_data = 0;
			break;
		}
		spin_unlock_irq (&rtc_lock);

		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto out;
		}

		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			goto out;
		}

		schedule();
	}

	retval = put_user(data, (unsigned long *)buf);
	if (!retval)
		retval = sizeof(unsigned long);

out:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&rtc_wait, &wait);
	return retval;
}

static int rtc_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	struct rtc_time tm, tm2;

	switch (cmd) {
	case RTC_AIE_OFF:
		spin_lock_irq(&rtc_lock);
		pMQRegs->rt.rt04.Control &= ~MQ_RTC_RTM_ENABLE;
		disable_irq(IRQNO_RTCALM);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_AIE_ON:
		spin_lock_irq(&rtc_lock);
		pMQRegs->rt.rt08.IntEnable |= MQ_RTC_DATETIME_INT;
		pMQRegs->rt.rt04.Control |= MQ_RTC_RTM_ENABLE;
		pMQRegs->rt.rt09.IntStatus &= ~1L;
		enable_irq(IRQNO_RTCALM);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_UIE_OFF:
		spin_lock_irq(&rtc_lock);
		del_timer(&rtc_irq_timer);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_UIE_ON:
		spin_lock_irq(&rtc_lock);
		rtc_irq_timer.expires = jiffies + HZ;
		add_timer(&rtc_irq_timer);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_PIE_OFF:
		spin_lock_irq(&rtc_lock);
		disable_irq(IRQNO_TIMER0);
		pMQRegs->gp.gp00.TimerCtrl &= ~MQ_TIMER0_MODE_MASK;
		pMQRegs->gp.gp05.Timer0Cmp = 0;
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_PIE_ON:
		if ((rtc_freq > 64) && !capable(CAP_SYS_RESOURCE))
			return -EACCES;
		spin_lock_irq(&rtc_lock);
		pMQRegs->gp.gp00.TimerCtrl = (pMQRegs->gp.gp00.TimerCtrl &
				~MQ_TIMER0_MODE_MASK) | MQ_TIMER0_SELF_MODE;
		pMQRegs->gp.gp05.Timer0Cmp =
				(TIMER_FREQ - TIMER_MISSED) / rtc_freq;
		rtc_irq_data = 0;
		enable_irq(IRQNO_TIMER0);
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_ALM_READ:
		decodetime(pMQRegs->rt.rt02.AlarmTime, pMQRegs->rt.rt03.AlarmDate, &tm);
		break;
	case RTC_ALM_SET:
		if (copy_from_user (&tm2, (struct rtc_time*)arg, sizeof (tm2)))
			return -EFAULT;
		decodetime(pMQRegs->rt.rt00.TimeOfDay, pMQRegs->rt.rt01.Date, &tm);
		if ((unsigned)tm2.tm_hour < 24)
			tm.tm_hour = tm2.tm_hour;
		if ((unsigned)tm2.tm_min < 60)
			tm.tm_min = tm2.tm_min;
		if ((unsigned)tm2.tm_sec < 60)
			tm.tm_sec = tm2.tm_sec;
		pMQRegs->rt.rt02.AlarmTime = rttimeofday(&tm);
		pMQRegs->rt.rt03.AlarmDate = rtdate(&tm);
		return 0;
	case RTC_RD_TIME:
		decodetime(pMQRegs->rt.rt00.TimeOfDay, pMQRegs->rt.rt01.Date, &tm);
		break;
	case RTC_SET_TIME:
		if (!capable(CAP_SYS_TIME))
			return -EACCES;
		if (copy_from_user (&tm, (struct rtc_time*)arg, sizeof (tm)))
			return -EFAULT;
		tm.tm_year += 1900;
		if (tm.tm_year < 1970 || (unsigned)tm.tm_mon >= 12 ||
		    tm.tm_mday < 1 || tm.tm_mday > (days_in_mo[tm.tm_mon] +
				(tm.tm_mon == 1 && is_leap(tm.tm_year))) ||
		    (unsigned)tm.tm_hour >= 24 ||
		    (unsigned)tm.tm_min >= 60 ||
		    (unsigned)tm.tm_sec >= 60)
			return -EINVAL;
		pMQRegs->rt.rt00.TimeOfDay = rttimeofday(&tm);
		pMQRegs->rt.rt01.Date = rtdate(&tm);
		return 0;
	case RTC_IRQP_READ:
		return put_user(rtc_freq, (unsigned long *)arg);
	case RTC_IRQP_SET:
		if (arg < 1 || arg > TIMER_FREQ)
			        return -EINVAL;
		if ((arg > 64) && (!capable(CAP_SYS_RESOURCE)))
			        return -EACCES;
		rtc_freq = arg;
		return 0;
	case RTC_EPOCH_READ:
		return put_user (1970, (unsigned long *)arg);
	default:
		return -EINVAL;
	}
	return copy_to_user ((void *)arg, &tm, sizeof (tm)) ? -EFAULT : 0;
}

static struct file_operations rtc_fops = {
	owner:		THIS_MODULE,
	llseek:		rtc_llseek,
	read:		rtc_read,
	poll:		rtc_poll,
	ioctl:		rtc_ioctl,
	open:		rtc_open,
	release:	rtc_release,
	fasync:		rtc_fasync,
};

static struct miscdevice mq9000rtc_miscdev = {
	RTC_MINOR,
	"rtc",
	&rtc_fops
};

static int rtc_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	char *p = page;
	int len;
	struct rtc_time tm;

	decodetime(pMQRegs->rt.rt00.TimeOfDay, pMQRegs->rt.rt01.Date, &tm);
	p += sprintf(p, "rtc_time\t: %02d:%02d:%02d\n"
			"rtc_date\t: %04d-%02d-%02d\n"
			"rtc_epoch\t: %04d\n",
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 1970);
	decodetime(pMQRegs->rt.rt02.AlarmTime, pMQRegs->rt.rt03.AlarmDate, &tm);
	p += sprintf(p, "alrm_time\t: %02d:%02d:%02d\n"
			"alrm_date\t: %04d-%02d-%02d\n",
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	p += sprintf(p, "alarm_IRQ\t: %s\n",
		(pMQRegs->rt.rt08.IntEnable & MQ_RTC_RTM_INT) ? "yes" : "no" );
	//p += sprintf(p, "update_IRQ\t: %s\n",
		//(pMQRegs->rt.rt08.IntEnable & MQ_RTC_PTM_INT) ? "yes" : "no");
	p += sprintf(p, "periodic_IRQ\t: %s\n",
		((pMQRegs->gp.gp00.TimerCtrl & MQ_TIMER0_MODE_MASK) == MQ_TIMER0_SELF_MODE) ? "yes" : "no");
	p += sprintf(p, "periodic_freq\t: %ld\n", rtc_freq);

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int __init rtc_init(void)
{
	int ret;

	misc_register (&mq9000rtc_miscdev);
	create_proc_read_entry ("driver/rtc", 0, 0, rtc_read_proc, NULL);

	init_timer(&rtc_irq_timer);
	rtc_irq_timer.function = rtc_periodic_irq;

	ret = request_irq(IRQNO_RTCALM, rtc_interrupt, SA_INTERRUPT, "rtc alarm", NULL);
	if (ret) {
		printk(KERN_ERR "rtc: IRQ %d already in use.\n", IRQNO_RTCALM);
		goto IRQ_RTCALM_failed;
	}

	set_GPIO_IRQ_edge(IRQ_TO_Timer_GPIO(IRQNO_TIMER0), GPIO_FALLING_EDGE);
	ret = request_irq(IRQNO_TIMER0, timer0_interrupt, SA_INTERRUPT, "rtc timer", NULL);
	if (ret) {
		printk(KERN_ERR "rtc: IRQ %d already in use.\n", IRQNO_TIMER0);
		goto IRQ_TIMER0_failed;
	}

	printk (KERN_INFO "MQ9000 Real Time Clock driver v" DRIVER_VERSION "\n");

	return 0;

IRQ_TIMER0_failed:
	free_irq(IRQNO_RTCALM, NULL);
IRQ_RTCALM_failed:
	remove_proc_entry ("driver/rtc", NULL);
	misc_deregister (&mq9000rtc_miscdev);
	return ret;
}

static void __exit rtc_exit(void)
{
	free_irq(IRQNO_RTCALM, NULL);
	free_irq(IRQNO_TIMER0, NULL);
	remove_proc_entry ("driver/rtc", NULL);
	misc_deregister (&mq9000rtc_miscdev);
}

module_init(rtc_init);
module_exit(rtc_exit);
