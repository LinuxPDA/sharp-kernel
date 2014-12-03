/*
 * linux/drivers/char/l7200_rtc.c
 *
 * Real Time Clock interface for Linux for LinkUp 7200
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   drivers/char/sa1100-rtc.c
 *   Copyright (c) 2000 Nils Faerber
 *
 * ChangeLog:
 *   03-29-2001 Lineo Japan, Inc. ...
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/poll.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#include <asm/hardware.h>
#include <asm/arch/irqs.h>
#include <linux/rtc.h>

#define	DRIVER_VERSION	"0.01"

static int rtc_usage = 0;
static int rtc_irq_data = 0;

static struct fasync_struct* rtc_async_queue;

static DECLARE_WAIT_QUEUE_HEAD(rtc_wait);

extern spinlock_t rtc_lock;

/* static struct timer_list rtc_irq_timer; */

static unsigned long epoch = 1900;      /* year corresponding to 0x00   */

static const unsigned char days_in_mo[] = 
{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void timemk(unsigned long t, struct rtc_time *tval)
{
        long int days, rem, y;
        const unsigned short int *ip;
        unsigned int wday;
        unsigned int yday;
        const unsigned short int __mon_yday[2][13] =
	{ /* Normal years.  */
                { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
                /* Leap years.  */
                { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
	};

#define __isleap(year) \
 	((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

	days = t / 86400;
	rem = t % 86400;
	while (rem < 0)
	{
		rem += 86400;
		--days;
	}
	while (rem >= 86400)
	{
		rem -= 86400;
		++days;
	}
	tval->tm_hour = rem / 3600;
	rem %= 3600;
	tval->tm_min = rem / 60;
	tval->tm_sec = rem % 60;
	wday = (4 + days) % 7;
	if (wday < 0)   
		wday += 7;
	y = 1970;
#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))
	while (days < 0 || days >= (__isleap (y) ? 366 : 365))
	{
		long int yg = y + days / 365 - (days % 365 < 0);
		days -= ((yg - y) * 365
                         + LEAPS_THRU_END_OF (yg - 1)
                         - LEAPS_THRU_END_OF (y - 1));
		y = yg;
	}
	tval->tm_year = y - 1900;
	yday = days;
	ip = __mon_yday[__isleap(y)];
	for (y = 11; days < (long int) ip[y]; --y)
		continue;
	days -= ip[y];   
	tval->tm_mon = y;
	tval->tm_mday = days + 1;
}

static void l7200_rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        unsigned int rtsr_shadow;
        unsigned int rcnr_shadow;

	spin_lock (&rtc_lock);
	/* Read the clock */
	rtsr_shadow = IO_RTCS;
	IO_RTCC = 0; /* clear TIC event */
	rcnr_shadow  = IO_RTCDR;
	rtc_irq_data = rtsr_shadow & 0x03;
	spin_unlock (&rtc_lock);
	wake_up_interruptible(&rtc_wait);

	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
}

static int l7200_rtc_open(struct inode *minode, struct file *mfile)
{
	if (rtc_usage != 0)
		return -EBUSY;

	MOD_INC_USE_COUNT;
	rtc_usage = 1;

        return 0;
}

static int l7200_rtc_release(struct inode *minode, struct file *mfile)
{
	MOD_DEC_USE_COUNT;
	rtc_usage = 0;

        return 0;
}

static int l7200_rtc_fasync (int fd, struct file *filp, int on)
{
        return fasync_helper (fd, filp, on, &rtc_async_queue);
}

static loff_t l7200_rtc_llseek(struct file *file, loff_t offset, int origin)
{
        return -ESPIPE;
}

static ssize_t l7200_rtc_read(struct file* file,
			      char*        buf,
			      size_t       count,
			      loff_t*      ppos)
{
        DECLARE_WAITQUEUE(wait, current);
        unsigned long data;
        ssize_t retval;

	if (count < sizeof(unsigned long))
		return -EINVAL;

	add_wait_queue(&rtc_wait, &wait);

	current->state = TASK_INTERRUPTIBLE;

	do {
		spin_lock_irq (&rtc_lock);
		data = rtc_irq_data;
		spin_unlock_irq (&rtc_lock);

		if (data != 0) {
			rtc_irq_data=0;
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
	remove_wait_queue(&rtc_wait, &wait);

        return retval;
}

static int l7200_rtc_ioctl(struct inode* inode,
			   struct file*  file,
			   unsigned int  cmd,
			   unsigned long arg)
{
	struct rtc_time wtime, rtc_tm;
	unsigned long tmpctr;
	unsigned char mon, day, hrs, min, sec, leap_yr;
	unsigned int yrs;

	switch (cmd) {
	case RTC_AIE_OFF:
		spin_lock_irq(&rtc_lock);
		IO_RTCCR &= ~RTCCR_EN_ALARM;
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_AIE_ON:
		spin_lock_irq(&rtc_lock);
		IO_RTCCR |= RTCCR_EN_ALARM;
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_PIE_OFF:
		spin_lock_irq(&rtc_lock);
		IO_RTCCR &= ~RTCCR_EN_TIC;
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_PIE_ON:
		spin_lock_irq(&rtc_lock);
		IO_RTCCR |= RTCCR_EN_TIC;
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_UIE_OFF:
		spin_lock_irq(&rtc_lock);
		IO_RTCCR &= ~RTCCR_EN_TIC;
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_UIE_ON:
		spin_lock_irq(&rtc_lock);
		IO_RTCCR |= RTCCR_EN_TIC;
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_ALM_READ:
		tmpctr = IO_RTCMR;
		timemk(tmpctr, &wtime);
		break;
	case RTC_ALM_SET:
		if (copy_from_user(&rtc_tm, (struct rtc_time*)arg,
                                   sizeof(struct rtc_time)))
			return -EFAULT;
		yrs = rtc_tm.tm_year + 1900;
		mon = rtc_tm.tm_mon + 1;   /* tm_mon starts at zero */
		day = rtc_tm.tm_mday;
		hrs = rtc_tm.tm_hour;
		min = rtc_tm.tm_min;
		sec = rtc_tm.tm_sec;

		if (yrs < 1970)
			return -EINVAL;
		leap_yr = ((!(yrs % 4) && (yrs % 100)) || !(yrs % 400));
		if ((mon > 12) || (day == 0))
			return -EINVAL;
		if (day > (days_in_mo[mon] + ((mon == 2) && leap_yr)))
			return -EINVAL;
		if ((hrs >= 24) || (min >= 60) || (sec >= 60))
			return -EINVAL;
		if ((yrs -= epoch) > 255)    /* They are unsigned */
			return -EINVAL;
		tmpctr = mktime(yrs + 1900, mon, day, hrs, min, sec);
		/* update the alarm register */
		spin_lock_irq(&rtc_lock);
		IO_RTCMR = tmpctr;
		spin_unlock_irq(&rtc_lock);

		return 0;
	case RTC_RD_TIME:
		spin_lock_irq(&rtc_lock);
		tmpctr = IO_RTCDR;
		spin_unlock_irq(&rtc_lock);
		timemk(tmpctr, &wtime);
		break;
	case RTC_SET_TIME:
		if (copy_from_user(&rtc_tm, (struct rtc_time*)arg,
                                   sizeof(struct rtc_time)))
			return -EFAULT;
		yrs = rtc_tm.tm_year + 1900;
		mon = rtc_tm.tm_mon + 1;   /* tm_mon starts at zero */
		day = rtc_tm.tm_mday;
		hrs = rtc_tm.tm_hour;
		min = rtc_tm.tm_min;
		sec = rtc_tm.tm_sec;

		if (yrs < 1970)
			return -EINVAL;
		leap_yr = ((!(yrs % 4) && (yrs % 100)) || !(yrs % 400));
		if ((mon > 12) || (day == 0))
			return -EINVAL;
		if (day > (days_in_mo[mon] + ((mon == 2) && leap_yr)))
			return -EINVAL;
		if ((hrs >= 24) || (min >= 60) || (sec >= 60))
			return -EINVAL;
		if ((yrs -= epoch) > 255)    /* They are unsigned */
			return -EINVAL;
		tmpctr = mktime(yrs + 1900, mon, day, hrs, min, sec);
		/* update the clock counter */
		spin_lock_irq(&rtc_lock);
		IO_RTCDR = tmpctr;
		spin_unlock_irq(&rtc_lock);

		return 0;
	case RTC_IRQP_READ:
		return put_user(1, (unsigned long *)arg);
	case RTC_IRQP_SET:
		/* Changing the periodic frequency is not supported */
		return -EACCES;
	case RTC_EPOCH_READ:
		return put_user (1970, (unsigned long *)arg);
	case RTC_EPOCH_SET:
		/* Changing the epoch is not supprted */
		return -EACCES;
	default:
		return -EINVAL;
	}

        return copy_to_user((void*)arg, &wtime, sizeof(wtime)) ? -EFAULT : 0;
}

static unsigned int l7200_rtc_poll(struct file* file, poll_table* wait)
{
        unsigned long l;

	poll_wait(file, &rtc_wait, wait);

	spin_lock_irq(&rtc_lock);
	l = rtc_irq_data;
	spin_unlock_irq(&rtc_lock);

	if (l != 0) {
		return POLLIN | POLLRDNORM;
	}

        return 0;
}

#ifdef CONFIG_PROC_FS

static int l7200_rtc_proc_output(char *buf)
{
        char* p;
        struct rtc_time tm;

	timemk(IO_RTCDR, &tm);
	p = buf;
	p += sprintf(p,
                     "rtc_time\t: %02d:%02d:%02d\n"
                     "rtc_date\t: %04d-%02d-%02d\n"
                     "rtc_epoch\t: %04lu\n",
                     tm.tm_hour, tm.tm_min, tm.tm_sec,
                     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, epoch);
	timemk(IO_RTCMR, &tm);
	p += sprintf(p,
		     "alrm_time\t: %02d:%02d:%02d\n"
                     "alrm_date\t: %04d-%02d-%02d\n",
                     tm.tm_hour, tm.tm_min, tm.tm_sec,
                     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	p += sprintf(p,"trim\t\t: %u\n", (unsigned int)IO_RTCDV);
	p += sprintf(p,"alarm_IRQ\t: %s\n",
		     (IO_RTCS & 0x01) ? "yes" : "no" );
	p += sprintf(p,"periodic_IRQ\t: %s\n",
		     (IO_RTCS & 0x02) ? "yes" : "no" );
	p += sprintf(p,"periodic_freq\t: 1\n");
	p += sprintf(p,"batt_status\t: okay\n");

        return (p - buf);
}

static int l7200_rtc_read_proc(char*  page,
			       char** start,
			       off_t  off,
			       int    count,
			       int*   eof,
			       void*  data)
{
        int len = l7200_rtc_proc_output(page);

	if (len <= off + count) { *eof = 1; }
	*start = page + off;
	len -= off;
	if (len > count) { len = count; }
	if (len < 0)     { len = 0; }

        return len;
}

#endif

static struct file_operations rtc_fops = {
	owner:		THIS_MODULE,
	llseek:		l7200_rtc_llseek,
	read:		l7200_rtc_read,
	poll:		l7200_rtc_poll,
	ioctl:		l7200_rtc_ioctl,
	open:		l7200_rtc_open,
	release:	l7200_rtc_release,
	fasync:		l7200_rtc_fasync,
};


static struct miscdevice l7200rtc_miscdev = {
	RTC_MINOR,
	"rtc",
	&rtc_fops
};

static void l7200_rtc_exit(void)
{
	spin_lock_irq(&rtc_lock);
	IO_RTCCR = 0; /* Disable TICK and ALARM */
	spin_unlock_irq(&rtc_lock);

	free_irq(IRQ_RTC_TICK,NULL);
	free_irq(IRQ_RTC_ALARM,NULL);

#ifdef CONFIG_PROC_FS
	remove_proc_entry("driver/rtc", NULL);
#endif
	misc_deregister(&l7200rtc_miscdev);
}

static int __init l7200_rtc_init(void)
{
	misc_register(&l7200rtc_miscdev);
#ifdef CONFIG_PROC_FS
	create_proc_read_entry("driver/rtc", 0, 0, l7200_rtc_read_proc, NULL);
#endif

	/* Reset the interrupt flags and disable interrupts */
	IO_RTCCR = 0;

        if (request_irq(IRQ_RTC_TICK, l7200_rtc_interrupt,
			SA_INTERRUPT, "rtc_tick", NULL)) {
		printk(KERN_ERR "rtc: IRQ %d already in use.\n", IRQ_RTC_TICK);
		return -EIO;
	}
        if (request_irq(IRQ_RTC_ALARM, l7200_rtc_interrupt,
			SA_INTERRUPT, "rtc_alarm", NULL)) {
		printk(KERN_ERR
		       "rtc: IRQ %d already in use.\n", IRQ_RTC_ALARM);
		return -EIO;
	}

	printk(KERN_INFO "LinkUp 7200 Real Time Clock Driver v"
	       DRIVER_VERSION "\n");

	{
		long time_t = IO_RTCDR;
		sys_stime(&time_t);
	}

        return 0;
}

module_init(l7200_rtc_init);
module_exit(l7200_rtc_exit);

EXPORT_NO_SYMBOLS;

