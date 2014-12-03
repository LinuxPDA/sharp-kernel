/*
 * linux/drivers/char/sh_rtc.c
 *
 * Real Time Clock interface for Linux for SH
 *
 * Copyright (c) 2002 Lineo uSolutions, Inc.
 *
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

#include <asm/irq.h>
#include <asm/rtc.h>
#include <asm/io.h>

#include <linux/rtc.h>

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&0xf) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

#ifndef W_BCD_TO_BIN
#define W_BCD_TO_BIN(val) ((val)=((val)&0xf) + (((val)&0xf0)>>4)*10 + \
				 (((val)&0xf00)>>8)*100 + \
				 (((val)&0xf000)>>12)*1000)
#endif

#ifndef W_BIN_TO_BCD
#define W_BIN_TO_BCD(val) ((val)=(((val)/1000)<<12) + ((((val)%1000)/100)<<8) \
				 + (((((val)%1000)%100)/10)<<4) \
				 + (((val)%1000)%100)%10)
#endif

#define	DRIVER_VERSION	"0.01"

static int rtc_usage	= 0;
volatile static int rtc_irq_data	= 0;

static struct fasync_struct* rtc_async_queue;

static DECLARE_WAIT_QUEUE_HEAD(rtc_wait);

extern spinlock_t rtc_lock;

/* static struct timer_list rtc_irq_timer; */

#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
static unsigned long epoch = 0;      /* year corresponding to 0x0000   */
#elif defined(__sh3__)
static unsigned long epoch = 2000;      /* year corresponding to 0x00   */
#endif

static const unsigned char days_in_mo[] = 
{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


static void sh_rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char	wk;

	spin_lock (&rtc_lock);

	if (irq == RTC_TICK_IRQ){
		wk = ctrl_inb(RCR2);
		if (wk & 0x80){
			wk &= 0x7f;
			ctrl_outb(wk,RCR2);
			rtc_irq_data = 1;
		}
	}

	if (irq == RTC_ALARM_IRQ){
		wk = ctrl_inb(RCR1);
		if (wk & 0x01){
			wk &= 0x98;
			ctrl_outb(wk,RCR1);
			rtc_irq_data = 2;
		}
	}

	spin_unlock (&rtc_lock);
	wake_up_interruptible(&rtc_wait);

	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
}

static int sh_rtc_open(struct inode *minode, struct file *mfile)
{
	if (rtc_usage != 0)
		return -EBUSY;

	MOD_INC_USE_COUNT;
	rtc_usage = 1;

        return 0;
}

static int sh_rtc_release(struct inode *minode, struct file *mfile)
{
	MOD_DEC_USE_COUNT;
	rtc_usage = 0;

        return 0;
}

static int sh_rtc_fasync (int fd, struct file *filp, int on)
{
        return fasync_helper (fd, filp, on, &rtc_async_queue);
}

static loff_t sh_rtc_llseek(struct file *file, loff_t offset, int origin)
{
        return -ESPIPE;
}

static ssize_t sh_rtc_read(struct file* file,
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
	remove_wait_queue(&rtc_wait, &wait);

        return retval;
}

static void control_periodic_irq(int mode)
{
	unsigned char wk;
	unsigned char rcr2;

	if (mode == 0){		/* OFF */
		rcr2	= ctrl_inb(RCR2);
		wk	= rcr2 & 0x0f;
		ctrl_outb(wk,RCR2);
	}else{			/* ON  */
		rcr2	= ctrl_inb(RCR2);
		wk	= rcr2 | 0x60;
		ctrl_outb(wk,RCR2);
	}

	rtc_irq_data = 0;
}

static void control_alarm_irq(int mode)
{
	unsigned char wk;
	unsigned char rcr1;

	if (mode == 0){		/* OFF */
		rcr1	= ctrl_inb(RCR1);
		wk	= rcr1 & 0x91;
		ctrl_outb(wk,RCR1);
	}else{			/* ON  */
		rcr1	= ctrl_inb(RCR1);
		wk	= rcr1 | 0x08;
		ctrl_outb(wk,RCR1);
	}

	rtc_irq_data = 0;
}

static void get_rtc_data(struct rtc_time *tm)
{
	unsigned int wk;

	tm->tm_sec	= 0;
	tm->tm_min	= 0;
	tm->tm_hour	= 0;
	tm->tm_mday	= 0;
	tm->tm_mon	= 0;
	tm->tm_year	= 0;
	tm->tm_wday	= 0;
	tm->tm_yday	= 0;
	tm->tm_isdst	= 0;

	wk = ctrl_inb(RSECCNT);
	BCD_TO_BIN(wk);
	tm->tm_sec	= wk;

	wk = ctrl_inb(RMINCNT);
	BCD_TO_BIN(wk);
	tm->tm_min	= wk;

	wk = ctrl_inb(RHRCNT);
	BCD_TO_BIN(wk);
	tm->tm_hour	= wk;

	wk = ctrl_inb(RWKCNT);
	BCD_TO_BIN(wk);
	tm->tm_wday	= wk;

	wk = ctrl_inb(RDAYCNT);
	BCD_TO_BIN(wk);
	tm->tm_mday	= wk;

	wk = ctrl_inb(RMONCNT);
	BCD_TO_BIN(wk);
	tm->tm_mon	= wk;

#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
	wk = ctrl_inw(RYRCNT);
	W_BCD_TO_BIN(wk);
#elif defined(__sh3__)
	wk = ctrl_inb(RYRCNT);
	BCD_TO_BIN(wk);
#endif
	tm->tm_year	= wk;
}

static void get_rtc_alarm_data(struct rtc_time *tm)
{
	unsigned int wk;

	tm->tm_sec	= 0;
	tm->tm_min	= 0;
	tm->tm_hour	= 0;
	tm->tm_mday	= 0;
	tm->tm_mon	= 0;
	tm->tm_year	= 0;
	tm->tm_wday	= 0;
	tm->tm_yday	= 0;
	tm->tm_isdst	= 0;

	wk = ctrl_inb(RSECAR);
	wk &= 0x7f;
	BCD_TO_BIN(wk);
	tm->tm_sec	= wk;

	wk = ctrl_inb(RMINAR);
	wk &= 0x7f;
	BCD_TO_BIN(wk);
	tm->tm_min	= wk;

	wk = ctrl_inb(RHRAR);
	wk &= 0x7f;
	BCD_TO_BIN(wk);
	tm->tm_hour	= wk;

	wk = ctrl_inb(RDAYAR);
	wk &= 0x7f;
	BCD_TO_BIN(wk);
	tm->tm_mday	= wk;

	wk = ctrl_inb(RMONAR);
	wk &= 0x7f;
	BCD_TO_BIN(wk);
	tm->tm_mon	= wk;

#if defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
	wk = ctrl_inw(RYRAR);
	W_BCD_TO_BIN(wk);
	tm->tm_year	= wk;
#else
#if defined(__SH4__)
	wk = ctrl_inw(RYRCNT);
	W_BCD_TO_BIN(wk);
#elif defined(__sh3__)
	wk = ctrl_inb(RYRCNT);			/* year from RYRCNT reg.     */
	BCD_TO_BIN(wk);
#endif
	tm->tm_year	= wk;
#endif
}

static void set_rtc_data(struct rtc_time *tm)
{
	unsigned int wk;
	unsigned char rcr2;

	rcr2 = ctrl_inb(RCR2);
	wk = rcr2 & 0xfe;
	ctrl_outb(wk,RCR2);

	wk = BIN_TO_BCD(tm->tm_sec);
	ctrl_outb(wk,RSECCNT);

	wk = BIN_TO_BCD(tm->tm_min);
	ctrl_outb(wk,RMINCNT);

	wk = BIN_TO_BCD(tm->tm_hour);
	ctrl_outb(wk,RHRCNT);

	wk = BIN_TO_BCD(tm->tm_mday);
	ctrl_outb(wk,RDAYCNT);

	wk = BIN_TO_BCD(tm->tm_mon);
	ctrl_outb(wk,RMONCNT);

#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
	wk = W_BIN_TO_BCD(tm->tm_year);
	ctrl_outw(wk,RYRCNT);
#elif defined(__sh3__)
	wk = BIN_TO_BCD(tm->tm_year);
	ctrl_outb(wk,RYRCNT);
#endif

	ctrl_outb(rcr2,RCR2);
}

static void set_rtc_alarm_data(struct rtc_time *tm)
{
	unsigned char wk;
	unsigned char rcr1;

	rcr1 = ctrl_inb(RCR1);
	wk = rcr1 & 0x91;
	ctrl_outb(wk,RCR1);

	wk = BIN_TO_BCD(tm->tm_sec);
	ctrl_outb((wk | 0x80),RSECAR);

	wk = BIN_TO_BCD(tm->tm_min);
	ctrl_outb((wk | 0x80),RMINAR);

	wk = BIN_TO_BCD(tm->tm_hour);
	ctrl_outb((wk | 0x80),RHRAR);

	wk = BIN_TO_BCD(tm->tm_mday);
	ctrl_outb((wk | 0x80),RDAYAR);

	wk = BIN_TO_BCD(tm->tm_mon);
	ctrl_outb((wk | 0x80),RMONAR);

#if defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
	{
		unsigned short	yr;
		yr = W_BIN_TO_BCD(tm->tm_year);
		ctrl_outw(yr,RYRAR);
//printk("RYRCNT read %04x\n",ctrl_inw(RYRCNT));
//printk("RYRAR set %04x : read %04x\n",yr, ctrl_inw(RYRAR));
		ctrl_outb(0x80,RCR3);
	}
#endif
	ctrl_outb(rcr1,RCR1);
}

static int sh_rtc_ioctl(struct inode* inode,
			   struct file*  file,
			   unsigned int  cmd,
			   unsigned long arg)
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
		spin_lock_irq(&rtc_lock);
		control_alarm_irq(0);
		spin_unlock_irq(&rtc_lock);
		return 0;

	case RTC_AIE_ON:
		spin_lock_irq(&rtc_lock);
		control_alarm_irq(1);
		spin_unlock_irq(&rtc_lock);
		return 0;

	case RTC_PIE_OFF:
		spin_lock_irq(&rtc_lock);
		control_periodic_irq(0);
		spin_unlock_irq(&rtc_lock);
		return 0;

	case RTC_PIE_ON:
		spin_lock_irq(&rtc_lock);
		control_periodic_irq(1);
		spin_unlock_irq(&rtc_lock);
		return 0;

	case RTC_UIE_OFF:
		spin_lock_irq(&rtc_lock);
		control_periodic_irq(0);
		spin_unlock_irq(&rtc_lock);
		return 0;

	case RTC_UIE_ON:
		spin_lock_irq(&rtc_lock);
		control_periodic_irq(1);
		spin_unlock_irq(&rtc_lock);
		return 0;

	case RTC_ALM_READ:
		get_rtc_alarm_data(&wtime);
		wtime.tm_mon -= 1;
#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
		wtime.tm_year -= 1900;
#elif defined(__sh3__)
		wtime.tm_year += 100;
#endif
		break;

	case RTC_ALM_SET:
		if (copy_from_user(&rtc_tm, (struct rtc_time*)arg,
                                   sizeof(struct rtc_time)))
			return -EFAULT;

#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
		yrs = rtc_tm.tm_year + 1900;
#elif defined(__sh3__)
		yrs = rtc_tm.tm_year + epoch - 100;
#endif
		mon = rtc_tm.tm_mon + 1;
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
#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
		if ((yrs < 1900) || (yrs > 9999)){
#elif defined(__sh3__)
		if ((yrs - epoch) > 255){
#endif
			return -EINVAL;
		}

		/* update the alarm register */
		spin_lock_irq(&rtc_lock);
		wtime.tm_sec	= sec;
		wtime.tm_min	= min;
		wtime.tm_hour	= hrs;
		wtime.tm_mday	= day;
		wtime.tm_mon	= mon;
#if defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
		wtime.tm_year	= yrs;
#endif
		set_rtc_alarm_data(&wtime);
		spin_unlock_irq(&rtc_lock);
		return 0;

	case RTC_RD_TIME:
		spin_lock_irq(&rtc_lock);
		get_rtc_data(&wtime);
		spin_unlock_irq(&rtc_lock);
		wtime.tm_mon -= 1;
#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
		wtime.tm_year -= 1900;
#elif defined(__sh3__)
		wtime.tm_year += 100;
#endif
		break;

	case RTC_SET_TIME:
		if (copy_from_user(&rtc_tm, (struct rtc_time*)arg,
                                   sizeof(struct rtc_time)))
			return -EFAULT;

#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
		yrs = rtc_tm.tm_year + 1900;
#elif defined(__sh3__)
		yrs = rtc_tm.tm_year + epoch - 100;
#endif
		mon = rtc_tm.tm_mon + 1;
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
#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
		if ((yrs < 1900) || (yrs > 9999)){
#elif defined(__sh3__)
		if ((yrs - epoch) > 255){
#endif
			return -EINVAL;
		}

		spin_lock_irq(&rtc_lock);
		wtime.tm_sec	= sec;
		wtime.tm_min	= min;
		wtime.tm_hour	= hrs;
		wtime.tm_mday	= day;
		wtime.tm_mon	= mon;
		wtime.tm_year	= yrs - epoch;
		set_rtc_data(&wtime);
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

static unsigned int sh_rtc_poll(struct file* file, poll_table* wait)
{
        unsigned long l;

	poll_wait(file, &rtc_wait, wait);

	spin_lock_irq(&rtc_lock);
	l = rtc_irq_data;
	spin_unlock_irq(&rtc_lock);

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

	get_rtc_data(&tm);

	p = buf;
	p += sprintf(p,
                     "rtc_time\t: %02d:%02d:%02d\n"
                     "rtc_date\t: %04d-%02d-%02d\n"
                     "rtc_epoch\t: %04lu\n",
                     tm.tm_hour, tm.tm_min, tm.tm_sec,
                     tm.tm_year + epoch, tm.tm_mon, tm.tm_mday,
#if defined(__SH4__) || defined (CONFIG_CPU_SUBTYPE_SH7710) || defined (CONFIG_CPU_SUBTYPE_SH7720)
		     1970);
#elif defined(__sh3__)
		     epoch);
#endif

	get_rtc_alarm_data(&tm);

	p += sprintf(p,
		     "alrm_time\t: %02d:%02d:%02d\n"
                     "alrm_date\t: %04d-%02d-%02d\n",
                     tm.tm_hour, tm.tm_min, tm.tm_sec,
                     tm.tm_year + epoch, tm.tm_mon, tm.tm_mday);

	p += sprintf(p,"alarm_IRQ\t: %s\n",
		     (ctrl_inb(RCR1) & 0x08) ? "yes" : "no" );
	p += sprintf(p,"periodic_IRQ\t: %s\n",
		     (ctrl_inb(RCR2) & 0x70) ? "yes" : "no" );

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

static struct file_operations rtc_fops = {
	owner:		THIS_MODULE,
	llseek:		sh_rtc_llseek,
	read:		sh_rtc_read,
	poll:		sh_rtc_poll,
	ioctl:		sh_rtc_ioctl,
	open:		sh_rtc_open,
	release:	sh_rtc_release,
	fasync:		sh_rtc_fasync,
};


static struct miscdevice shrtc_miscdev = {
	RTC_MINOR,
	"rtc",
	&rtc_fops
};

static void sh_rtc_exit(void)
{
	spin_lock_irq(&rtc_lock);
	ctrl_outb(0x0,RCR1);
	ctrl_outb(0x0,RCR2);
	spin_unlock_irq(&rtc_lock);

	free_irq(RTC_ALARM_IRQ,NULL);
	free_irq(RTC_TICK_IRQ,NULL);

#ifdef CONFIG_PROC_FS
	remove_proc_entry("driver/rtc", NULL);
#endif

	misc_deregister(&shrtc_miscdev);
}

static int __init sh_rtc_init(void)
{
	misc_register(&shrtc_miscdev);

#ifdef CONFIG_PROC_FS
	create_proc_read_entry("driver/rtc", 0, 0, sh_rtc_read_proc, NULL);
#endif

        if (request_irq(RTC_ALARM_IRQ, sh_rtc_interrupt,SA_INTERRUPT,"rtc_alarm",NULL)){
		printk(KERN_ERR "rtc: IRQ %d already in use.\n", RTC_ALARM_IRQ);
		return -EIO;
	}

        if (request_irq(RTC_TICK_IRQ, sh_rtc_interrupt,SA_INTERRUPT,"rtc_tick",NULL)){
		printk(KERN_ERR "rtc: IRQ %d already in use.\n", RTC_TICK_IRQ);
		return -EIO;
	}

	printk(KERN_INFO "SH Real Time Clock Driver v" DRIVER_VERSION "\n");

        return 0;
}

module_init(sh_rtc_init);
module_exit(sh_rtc_exit);

EXPORT_NO_SYMBOLS;

