/*
 * linux/drivers/char/ms77xxxx_ts.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * Touch screen driver for ms77xxxx.
 *
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
#include <asm/ms7760cp.h>
#else
#error No Platform
#endif

#undef TS_DEBUG
#ifdef TS_DEBUG
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#undef NO_USE_H8_INT
#ifdef NO_USE_H8_INT
static struct timer_list pen_event_check_timer;
#define PEN_EVENT_CHECK_INTERVAL (HZ / 10)	/* 1sec / n */
static void ms77xxxx_ts_event_check(unsigned long data);
#endif

#define TS_NAME "ts"
#define TS_MAJOR 254

#define BUFSIZE 128

#ifdef CONFIG_PM
#include <linux/pm.h>
static struct pm_dev* ms77xxxx_ts_pm_dev;
#endif

#define swap_hlb(x)	((((unsigned short)x & 0xff) << 8) | (((unsigned short)x >> 8) & 0xff))

/* From Compaq's Touch Screen Specification version 0.2 (draft) */
typedef struct {
	short pressure;
	short x;
	short y;
  	short millisecs;
} TS_EVENT;

static DECLARE_WAIT_QUEUE_HEAD(queue);
static struct fasync_struct* fasync;
static int raw_max_x, raw_max_y;
static int res_x, res_y;
static int raw_min_x, raw_min_y;
static int cal_ok, x_rev, y_rev, xyswap;
static int head = 0, tail = 0;
static TS_EVENT cur_data, buf[BUFSIZE];
static int ts_state = 0;
static int ts_open_count;


static struct fasync_struct* fasync;

static void print_par(void)
{
	printk(KERN_INFO " Kernel ==> cal_ok = %d\n",cal_ok);
	printk(KERN_INFO " Kernel ==> raw_max_x = %d\n",raw_max_x);
	printk(KERN_INFO " Kernel ==> raw_max_y = %d\n",raw_max_y);
	printk(KERN_INFO " Kernel ==> res_x = %d\n",res_x);
	printk(KERN_INFO " Kernel ==> res_y = %d\n",res_y);
	printk(KERN_INFO " Kernel ==> raw_min_x = %d\n",raw_min_x);
	printk(KERN_INFO " Kernel ==> raw_min_y = %d\n",raw_min_y);
	printk(KERN_INFO " Kernel ==> xyswap = %d\n",xyswap);
	printk(KERN_INFO " Kernel ==> x_rev = %d\n",x_rev);
	printk(KERN_INFO " Kernel ==> y_rev = %d\n",y_rev);
}

static void ts_clear(void)
{
	int i;

	head = tail = 0;
	for (i = 0; i < BUFSIZE; i++) {
		buf[i].pressure = 0;
		buf[i].x = 0;
		buf[i].y = 0;
		buf[i].millisecs = 0;
	}
}

#ifdef NO_USE_H8_INT
#define AVERAGE 1
#else
#define AVERAGE 3
#endif
static int avg = 0;
static int avx = 0;
static int avy = 0;
static int b_state = 0;

static void new_data(void)
{
	cur_data.x = 4000 - cur_data.x;
	cur_data.y = 4000 - cur_data.y;

	if (cur_data.pressure) {
		b_state = 1;
		avx += cur_data.x;
		avy += cur_data.y;
		avg++;
		if (avg == AVERAGE) {
			cur_data.x = avx / AVERAGE;
			cur_data.y = avy / AVERAGE;
			avx = 0;
			avy = 0;
			avg = 0;
		}
		else
			return;
	}
	else {
		if (b_state == 0)
			return;
		
		b_state = 0;
		avx = 0;
		avy = 0;
		avg = 0;
	}

	cur_data.millisecs = (short)jiffies;
	buf[head] = cur_data;

	if (++head >= BUFSIZE) {
		head = 0;
	}
	if ((head == tail) && (++tail >= BUFSIZE)) {
	  	tail = 0;
	}

	if (fasync) {
		kill_fasync(&fasync, SIGIO, POLL_IN); 
	}
	wake_up_interruptible(&queue);
}

static TS_EVENT get_data(void)
{
	int last = tail;

	if (++tail == BUFSIZE) {
		tail = 0;
	}
	return buf[last];
}

static void wait_for_action(void)
{
	unsigned char val = 0;
	h8_out(&val, 1, MS7760CP_TPLCR); /* TP stop */

#ifdef NO_USE_H8_INT
        init_timer(&pen_event_check_timer);
        pen_event_check_timer.function = ms77xxxx_ts_event_check;
        pen_event_check_timer.data = 0;
#endif

	ts_open_count = 0;
}

static void start_wait_for_action(void)
{
	unsigned char val;

	val = TPLSCR_100MS;
	//val = TPLSCR_20MS;
	h8_out(&val, 1, MS7760CP_TPLSCR); /* Sampling 100ms */
	val = TPLCR_TP_STR | TPLCR_PEN_ONI
			   | TPLCR_PEN_OFFI | TPLCR_PEN_ONRE;
	h8_out(&val, 1, MS7760CP_TPLCR); /* TP start, down IE, up IE */

#ifdef NO_USE_H8_INT
        // Start timer function
        pen_event_check_timer.expires = jiffies + PEN_EVENT_CHECK_INTERVAL;
        add_timer(&pen_event_check_timer);
#endif
}

static void stop_wait_for_action(void)
{
	unsigned char val = 0;
	h8_out(&val, 1, MS7760CP_TPLCR); /* TP stop */

#ifdef NO_USE_H8_INT
	del_timer_sync(&pen_event_check_timer);
#endif
}

static ssize_t ms77xxxx_ts_read(struct file* filp, char* buf, size_t count, loff_t* l)
{
	TS_EVENT t, out;
	DECLARE_WAITQUEUE(wait, current);
	int i;

	if (head == tail) {
		if (filp->f_flags & O_NONBLOCK)
		  	return -EAGAIN;
		add_wait_queue(&queue, &wait);
		current->state = TASK_INTERRUPTIBLE;
		while ((head == tail) && !signal_pending(current)) {
			schedule();
			current->state = TASK_INTERRUPTIBLE;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&queue, &wait);
	}

	for (i = count; i >= sizeof(out);
		     i -= sizeof(out), buf += sizeof(out)) {
		if (head == tail)
		  	break;
		t = get_data();
		out.pressure = t.pressure;
		if (cal_ok) {
		  out.x = (x_rev) ?
		    ((raw_max_x - t.x) * res_x) / (raw_max_x - raw_min_x) :
		    ((t.x - raw_min_x) * res_x) / (raw_max_x - raw_min_x);
		  out.y = (y_rev) ?
		    ((raw_max_y - t.y) * res_y) / (raw_max_y - raw_min_y) :
		    ((t.y - raw_min_y) * res_y) / (raw_max_y - raw_min_y);
		}
		else {
			out.x = t.x;
			out.y = t.y;
		}
		out.millisecs = t.millisecs;

		copy_to_user(buf, &out, sizeof(out));
	}

	return count - i;
}

static unsigned int ms77xxxx_ts_poll(struct file* filp, poll_table* wait)
{
	poll_wait(filp, &queue, wait);
	if (head != tail) {
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static int ms77xxxx_ts_fasync(int fd, struct file* filp, int on)
{
	int ret = fasync_helper(fd, filp, on, &fasync);
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static ssize_t ms77xxxx_ts_write(struct file* filp, const char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;

	return rc;
}
#endif

static int ms77xxxx_ts_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case 3:
	  	raw_max_x = arg;
		break;
	case 4:
	  	raw_max_y = arg;
		break;
	case 5:
	  	res_x = arg;
		break;
	case 6:
	  	res_y = arg;
		break;
	case 10:
	  	raw_min_x = arg;
		break;
	case 11:
	  	raw_min_y = arg;
		break;
	case 12:
		xyswap = arg;
	case 13:
	  	/* 0 = Enable calibration ; 1 = Calibration OK */
	  	cal_ok = arg;
	case 14:
		ts_clear();
		break;
	case 15:
		x_rev = arg;
		break;
	case 16:
	  	y_rev = arg;
		break;
	case 17:
		print_par();
		break;
	}
	return 0;
}

static int ms77xxxx_ts_open(struct inode* inode, struct file* filp)
{
	if (!ts_open_count++)
		start_wait_for_action();

	ts_clear();

	MOD_INC_USE_COUNT;

	return 0;
}

static int ms77xxxx_ts_release(struct inode* inode, struct file* filp)
{
	if (!--ts_open_count)
		stop_wait_for_action();

	ts_clear();

	ms77xxxx_ts_fasync(-1, filp, 0);

	MOD_DEC_USE_COUNT;

	return 0;
}

#ifndef NO_USE_H8_INT
static void ms77xxxx_ts_interrupt(int irq, void *ptr, struct pt_regs *regs)
{
	unsigned char rtkisr;
	unsigned char tplsr;

DPRINTK("ms77xxxx_ts_interrupt: ");
	h8_in(&rtkisr, 1, MS7760CP_RTKISR);
#ifdef TS_DEBUG
printk("0x%08x\n",rtkisr);
#endif
	if (!(rtkisr & RTKISR_TPIF))
		return;
	rtkisr &= ~RTKISR_TPIF;

	h8_in(&tplsr, 1, MS7760CP_TPLSR);
	if (tplsr & TPLSR_PEN_ONIF) { /* PEN_ON */
		tplsr &= ~TPLSR_PEN_ONIF;
		cur_data.pressure = 1000;
		ts_state = 1;
		DPRINTK("Pen ON : ");
	}
	else if (tplsr & TPLSR_PNE_OFFIF) { /* PEN_OFF */
		tplsr &= ~TPLSR_PNE_OFFIF;
		cur_data.pressure = 0;
		ts_state = 0;
		DPRINTK("Pen OFF: ");
	}
	else {
		//DPRINTK("Pen ???: %02x\n", tplsr);
		tplsr &= ~(TPLSR_PEN_ONIF | TPLSR_PNE_OFFIF);
		h8_out(&tplsr, 1, MS7760CP_TPLSR);
		h8_out(&rtkisr, 1, MS7760CP_RTKISR);
		return;
	}

	h8_in((unsigned char *)&cur_data.x, 2, MS7760CP_XPAR);
	h8_in((unsigned char *)&cur_data.y, 2, MS7760CP_YPAR);
	cur_data.x = swap_hlb(cur_data.x);
	cur_data.y = swap_hlb(cur_data.y);
#ifdef TS_DEBUG
	printk("(0x%04x,0x%04x)\n",
		(unsigned short)cur_data.x, (unsigned short)cur_data.y);
#endif

	cur_data.millisecs = (short)jiffies;

	new_data();

	h8_out(&tplsr, 1, MS7760CP_TPLSR);
	h8_out(&rtkisr, 1, MS7760CP_RTKISR);
}

#else
static void ms77xxxx_ts_event_check(unsigned long data)
{
	unsigned char rtkisr = ~RTKISR_TPIF;
	unsigned char tplsr;

	h8_in(&tplsr, 1, MS7760CP_TPLSR);
	if (ts_state == 0 && (tplsr & TPLSR_PEN_ONIF)) { /* PEN_ON */
//		tplsr = ~TPLSR_PEN_ONIF;
		tplsr = ~(TPLSR_PEN_ONIF | TPLSR_PNE_OFFIF);
		cur_data.pressure = 1000;
		ts_state = 1;
		DPRINTK("Pen ON : ");
	}
	else if (ts_state == 1 && !(tplsr & TPLSR_PEN_ONIF)
				&& (tplsr & TPLSR_PNE_OFFIF)) { /* PEN_OFF */
		//tplsr = ~TPLSR_PNE_OFFIF;
		tplsr = ~(TPLSR_PEN_ONIF | TPLSR_PNE_OFFIF);
		cur_data.pressure = 0;
		ts_state = 0;
		DPRINTK("Pen OFF: ");
	}
	else if (ts_state == 1) {
//	else if (ts_state == 1 && (tplsr & TPLSR_PEN_ONIF)) {
//		tplsr = ~(TPLSR_PEN_ONIF | TPLSR_PNE_OFFIF);
		tplsr = ~(TPLSR_PEN_ONIF);
		cur_data.pressure = 1000;
		DPRINTK("Pen ON-: ");
	}
	else {
		//tplsr = ~(TPLSR_PEN_ONIF | TPLSR_PNE_OFFIF);
		tplsr = ~(TPLSR_PEN_ONIF);
		goto END;
	}

	h8_in((unsigned char *)&cur_data.x, 2, MS7760CP_XPAR);
	h8_in((unsigned char *)&cur_data.y, 2, MS7760CP_YPAR);
	cur_data.x = swap_hlb(cur_data.x);
	cur_data.y = swap_hlb(cur_data.y);
#ifdef TS_DEBUG
	printk("(0x%04x,0x%04x)\n",
		(unsigned short)cur_data.x, (unsigned short)cur_data.y);
#endif

	cur_data.millisecs = (short)jiffies;

	new_data();

END:;
	h8_out(&tplsr, 1, MS7760CP_TPLSR);
	//h8_out(&rtkisr, 1, MS7760CP_RTKISR);

        pen_event_check_timer.expires = jiffies + PEN_EVENT_CHECK_INTERVAL;
	add_timer(&pen_event_check_timer);
}
#endif

static struct file_operations ms77xxxx_ts_fops = {
	read:	 ms77xxxx_ts_read,
	poll:	 ms77xxxx_ts_poll,
	ioctl:	 ms77xxxx_ts_ioctl,
	fasync:	 ms77xxxx_ts_fasync,
	open:	 ms77xxxx_ts_open,
	release: ms77xxxx_ts_release,
};

#ifdef CONFIG_PM
static int ms77xxxx_ts_pm_callback(struct pm_dev* pm_dev,
				 pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
		break;
	case PM_RESUME:
		head = tail = 0;
                break;
	}
	return 0;
}
#endif

int __init ms77xxxx_ts_init(void)
{
	int rc;

	rc = devfs_register_chrdev(TS_MAJOR, TS_NAME, &ms77xxxx_ts_fops);
	if (rc < 0) {
		printk(KERN_ERR "ms7727_ts: failed to register char dev\n");
		return rc;
	}
	devfs_register(NULL, TS_NAME, DEVFS_FL_NONE, TS_MAJOR, 0,
		       0444 | S_IFCHR, &ms77xxxx_ts_fops, NULL);

#ifndef NO_USE_H8_INT
	if ((rc = request_irq(MS7760CP_H8IRQ, ms77xxxx_ts_interrupt,
			   SA_SHIRQ | SA_INTERRUPT, TS_NAME, ms77xxxx_ts_interrupt))) {
		printk(KERN_ERR "ms77xx_ts: failed to register H8IRQ\n");
		return rc;
	}
#endif

	raw_max_x = 4000;
	raw_max_y = 4000;
	raw_min_x = 200;
	raw_min_y = 200;

	res_x     = 240;
	res_y     = 320;

	head = 0;
	tail = 0;
	xyswap = 0;

	cal_ok = 0;
	x_rev = 0;
	y_rev = 0;

	ts_clear();
	
	init_waitqueue_head(&queue);
	
#ifdef CONFIG_PM
	ms77xxxx_ts_pm_dev = pm_register(PM_SYS_DEV, 0,
					 ms77xxxx_ts_pm_callback);
#endif
	wait_for_action();

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 tocuh screen driver initialized.\n"); 
#else
#error No Platform
#endif

	return 0;
}

#ifdef MODULE
void __exit ms77xxxx_ts_cleanup(void)
{
#ifdef CONFIG_PM
	pm_unregister_all(ms77xxxx_ts_pm_callback);
#endif
#ifndef NO_USE_H8_INT
	free_irq(MS7760CP_H8IRQ, ms77xxxx_ts_interrupt);
#endif
	//stop_wait_for_action();
	devfs_unregister_chrdev(TS_MAJOR, TS_NAME);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 touch screen driver removed.\n"); 
#else
#error No Platform
#endif
}
#endif

module_init(ms77xxxx_ts_init);
#ifdef MODULE
module_exit(ms77xxxx_ts_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
MODULE_DESCRIPTION("Touch screen driver for MS7760CP01");
#else
#error No Platform
#endif

