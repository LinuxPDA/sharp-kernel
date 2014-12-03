/*
 * linux/drivers/char/ms7727rp_ts.c
 *
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * Touch Driver for MS7727RP01/02
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
#include <linux/poll.h>
#include <linux/string.h>

#include <asm/ms7727rp.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#define TS_NAME "ts"
#define TS_MAJOR 254

#define BUFSIZE 128

#ifdef CONFIG_PM
#include <linux/pm.h>
static struct pm_dev* ms7727rp_ts_pm_dev;
#endif

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

#define AVERAGE 3
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
#if 1
	h8_write(TPSCR, 0x0010); /* Sampling 100ms */
	h8_write(TPCR,  0x000F); /* TP start, dow IE, up IE */
#else
	h8_write(TPSCR, 0x0001); /* Sampling 100ms */
	h8_write(TPCR,  0x0007); /* TP start, dow IE, up IE */
#endif
}

static int ms7727rp_ts_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
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

static ssize_t ms7727rp_ts_read(struct file* filp, char* buf, size_t count, loff_t* l)
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

static unsigned int ms7727rp_ts_poll(struct file* filp, poll_table* wait)
{
	poll_wait(filp, &queue, wait);
	if (head != tail) {
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static int ms7727rp_ts_fasync(int fd, struct file* filp, int on)
{
	int ret = fasync_helper(fd, filp, on, &fasync);
	if (ret < 0)
		return ret;

	return 0;
}

static int ms7727rp_ts_open(struct inode* inode, struct file* filp)
{
	ts_clear();

	MOD_INC_USE_COUNT;

	return 0;
}

static int ms7727rp_ts_release(struct inode* inode, struct file* filp)
{
	ts_clear();

	ms7727rp_ts_fasync(-1, filp, 0);

	MOD_DEC_USE_COUNT;

	return 0;
}

static void ms7727rp_ts_interrupt(int irq, void *ptr, struct pt_regs *regs)
{
	unsigned short tpsr;

	tpsr = h8_read(TPSR) & 0x0006;
	if (tpsr & 0x0002) { /* PEN_ON */
		tpsr &= 0x0004;
		cur_data.pressure = 1000;
		ts_state = 1;
	}
	else if (tpsr & 0x0004) { /* PEN_OFF */
		tpsr &= 0x0002;
		cur_data.pressure = 0;
		ts_state = 0;
	}
	else {
		tpsr &= 0x0006;
		h8_write(TPSR, tpsr);
		h8_write(RTKISR, 0x0000);
		return;
	}

	cur_data.x  = h8_read(XPAR_H) << 8;
	cur_data.x |= h8_read(XPAR_L) & 0xff;
	cur_data.y  = h8_read(YPAR_H) << 8;
	cur_data.y |= h8_read(YPAR_L) & 0xff;

	cur_data.millisecs = (short)jiffies;

	new_data();

	h8_write(TPSR, tpsr);
	h8_write(RTKISR, 0x0000);
}

static struct file_operations ms7727rp_ts_fops = {
	read:	 ms7727rp_ts_read,
	poll:	 ms7727rp_ts_poll,
	ioctl:	 ms7727rp_ts_ioctl,
	fasync:	 ms7727rp_ts_fasync,
	open:	 ms7727rp_ts_open,
	release: ms7727rp_ts_release,
};

#ifdef CONFIG_PM
static int ms7727rp_ts_pm_callback(struct pm_dev* pm_dev,
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

int __init ms7727rp_ts_init(void)
{
	int ret;

	register_chrdev(TS_MAJOR, TS_NAME, &ms7727rp_ts_fops);

	if ((ret = request_irq(PINT_IRQ_BASE + 11, ms7727rp_ts_interrupt,
			       SA_INTERRUPT, TS_NAME, NULL)))	{
		printk(KERN_ERR "ms7727_ts: failed to register PINT11\n");
		return ret;
	}

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
	ms7727rp_ts_pm_dev = pm_register(PM_SYS_DEV, 0,
					 ms7727rp_ts_pm_callback);
#endif
	wait_for_action();

	printk(KERN_INFO "MS7727RP02 Touch Screen driver initialized. \n"); 

	return 0;
}

#ifdef MODULE
void __exit ms7727rp_ts_cleanup(void)
{
	unregister_chrdev(TS_MAJOR, TS_NAME);

	printk(KERN_INFO "MS7727RP02 Touch Screen driver removed.\n");
}
#endif

module_init(ms7727rp_ts_init);
#ifdef MODULE
module_exit(ms7727rp_ts_cleanup);
#endif
