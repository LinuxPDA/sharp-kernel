/*
 *  linux/drivers/misc/ucb1x00-ts.c
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/delay.h>

#include <asm/dma.h>

#include "ucb1x00.h"

/*
 * This structure is nonsense - millisecs is not very useful
 * since the field size is too small.  Also, we SHOULD NOT
 * be exposing jiffies to user space directly.
 */
struct ts_event {
	u16		pressure;
	u16		x;
	u16		y;
	u16		pad;
	struct timeval	stamp;
};

#define NR_EVENTS	16

struct ucb1x00_ts {
	struct ucb1x00		*ucb;
	struct fasync_struct	*fasync;
	wait_queue_head_t	read_wait;

	wait_queue_head_t	irq_wait;
	struct completion	complete;
	struct task_struct	*rtask;
	u16			x_res;
	u16			y_res;

	u8			evt_head;
	u8			evt_tail;
	struct ts_event		events[NR_EVENTS];
};

#define ucb1x00_ts_evt_pending(ts)	((volatile u8)(ts)->evt_head != (ts)->evt_tail)
#define ucb1x00_ts_evt_get(ts)		((ts)->events + (ts)->evt_tail)
#define ucb1x00_ts_evt_pull(ts)		((ts)->evt_tail = ((ts)->evt_tail + 1) & (NR_EVENTS - 1))

static struct ucb1x00_ts ucbts;

static inline void ucb1x00_ts_evt_add(struct ucb1x00_ts *ts, u16 pressure, u16 x, u16 y)
{
	int next_head;

	next_head = (ts->evt_head + 1) & (NR_EVENTS - 1);
	if (next_head != ts->evt_tail) {
		ts->events[ts->evt_head].pressure = pressure;
		ts->events[ts->evt_head].x = x;
		ts->events[ts->evt_head].y = y;
		get_fast_time(&ts->events[ts->evt_head].stamp);
		ts->evt_head = next_head;

		if (ts->fasync)
			kill_fasync(&ts->fasync, SIGIO, POLL_IN);
		wake_up_interruptible(&ts->read_wait);
	}
}

/*
 * Switch to interrupt mode.
 */
static inline void ucb1x00_ts_mode_int(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_INT);
}

/*
 * Switch to X position reading
 */
static inline void ucb1x00_ts_mode_xpos(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);
}

/*
 * Switch to X position reading
 */
static inline void ucb1x00_ts_mode_ypos(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);
}

/*
 * Switch to X plate resistance mode
 */
static inline void ucb1x00_ts_mode_xres(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
}

/*
 * Switch to Y plate resistance mode
 */
static inline void ucb1x00_ts_mode_yres(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
}

static void ucb1x00_ts_event(struct ucb1x00_ts *ts, int touch)
{
	int x, y, p;

	ucb1x00_adc_enable(ts->ucb);

	/*
	 * Read the pressure.
	 */
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	p = ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPY, UCB_NOSYNC);

	/*
	 * Read X position.
	 */
	ucb1x00_ts_mode_xpos(ts);
	x = ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPY, UCB_NOSYNC);

	/*
	 * Read Y position.
	 */
	ucb1x00_ts_mode_ypos(ts);
	y = ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPX, UCB_NOSYNC);
	ucb1x00_adc_disable(ts->ucb);

	/*
	 * Filtering is policy.  Policy belongs in user space.  We
	 * therefore leave it to user space to do any filtering
	 * they please.
	 */
	ucb1x00_ts_evt_add(ts, p, x, y);
}

/*
 * This is a RT kernel thread that handles the ADC accesses
 * (mainly so we can use semaphores in the UCB1200 core code
 * to serialise accesses to the ADC).
 */
static int ucb1x00_thread(void *_ts)
{
	struct ucb1x00_ts *ts = _ts;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	ts->rtask = tsk;

	daemonize();
	tsk->tty = NULL;
//	tsk->policy = SCHED_FIFO;
//	tsk->rt_priority = 1;
	strcpy(tsk->comm, "ktsd");

	/* only want to receive SIGKILL */
	spin_lock_irq(&tsk->sigmask_lock);
	siginitsetinv(&tsk->blocked, sigmask(SIGKILL));
	recalc_sigpending(tsk);
	spin_unlock_irq(&tsk->sigmask_lock);

	add_wait_queue(&ts->irq_wait, &wait);

	ucb1x00_adc_enable(ts->ucb);
	ucb1x00_ts_mode_xres(ts);
	ts->x_res = ucb1x00_adc_read(ts->ucb, 0, UCB_NOSYNC);
	ucb1x00_ts_mode_yres(ts);
	ts->y_res = ucb1x00_adc_read(ts->ucb, 0, UCB_NOSYNC);
	ucb1x00_adc_disable(ts->ucb);

	complete(&ts->complete);

	for (;;) {
		signed long interval;
		int val;

		/*
		 * Set to interrupt mode, and wait a settling time.
		 */
		set_task_state(tsk, TASK_INTERRUPTIBLE);
		ucb1x00_enable(ts->ucb);
		ucb1x00_ts_mode_int(ts);
		udelay(500);

		set_task_state(tsk, TASK_INTERRUPTIBLE);
		val = ucb1x00_reg_read(ts->ucb, UCB_TS_CR);
		if (val & (UCB_TS_CR_TSPX_LOW | UCB_TS_CR_TSMX_LOW)) {
			ucb1x00_enable_irq(ts->ucb, UCB_IRQ_TSPX, UCB_FALLING);
			interval = MAX_SCHEDULE_TIMEOUT;
		} else {
			interval = HZ/50;
		}

		ucb1x00_disable(ts->ucb);
		if (signal_pending(tsk))
			break;

		schedule_timeout(interval);

		/*
		 * We got an IRQ, which work us up.  Process the touchscreen.
		 */
		ucb1x00_ts_event(ts, interval == MAX_SCHEDULE_TIMEOUT);
	}

	remove_wait_queue(&ts->irq_wait, &wait);
	ts->rtask = NULL;
	return 0;
}

/*
 * We only detect touch screen _touches_ with this interrupt
 * handler, and even then we just schedule our task.
 */
static void ucb1x00_ts_irq(int idx, void *id)
{
	struct ucb1x00_ts *ts = id;
	ucb1x00_disable_irq(ts->ucb, UCB_IRQ_TSPX, UCB_FALLING);
	wake_up(&ts->irq_wait);
}

/*
 * User space driver interface.
 */
static ssize_t
ucb1x00_ts_read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	struct ucb1x00_ts *ts = filp->private_data;
	char *ptr = buffer;
	int err = 0;

	add_wait_queue(&ts->read_wait, &wait);
	while (count >= sizeof(struct ts_event)) {
		err = -ERESTARTSYS;
		if (signal_pending(current))
			break;

		if (ucb1x00_ts_evt_pending(ts)) {
			struct ts_event *evt = ucb1x00_ts_evt_get(ts);

			err = copy_to_user(ptr, evt, sizeof(struct ts_event));
			ucb1x00_ts_evt_pull(ts);

			if (err)
				break;

			ptr += sizeof(struct ts_event);
			count -= sizeof(struct ts_event);
			continue;
		}

		set_current_state(TASK_INTERRUPTIBLE);
		err = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			break;
		schedule();
	}
	remove_wait_queue(&ts->read_wait, &wait);
 
	return ptr == buffer ? err : ptr - buffer;
}

static unsigned int ucb1x00_ts_poll(struct file *filp, poll_table *wait)
{
	struct ucb1x00_ts *ts = filp->private_data;
	int ret = 0;

	poll_wait(filp, &ts->read_wait, wait);
	if (ucb1x00_ts_evt_pending(ts))
		ret = POLLIN | POLLRDNORM;

	return ret;
}

static int ucb1x00_ts_fasync(int fd, struct file *filp, int on)
{
	struct ucb1x00_ts *ts = filp->private_data;

	return fasync_helper(fd, filp, on, &ts->fasync);
}

static int ucb1x00_ts_open(struct inode *inode, struct file *filp)
{
	struct ucb1x00_ts *ts = &ucbts;
	int ret;

	/*
	 * Start the RT thread
	 */
	lock_kernel();
	if (ts->rtask == NULL) {
		init_completion(&ts->complete);
		init_waitqueue_head(&ts->irq_wait);
		ret = kernel_thread(ucb1x00_thread, ts, CLONE_FS | CLONE_FILES);
		if (ret < 0)
			goto out;
		wait_for_completion(&ts->complete);
	}

	ret = ucb1x00_hook_irq(ts->ucb, UCB_IRQ_TSPX, ucb1x00_ts_irq, ts);
	if (ret == 0)
		filp->private_data = ts;

	unlock_kernel();

out:
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static int ucb1x00_ts_release(struct inode *inode, struct file *filp)
{
	struct ucb1x00_ts *ts = filp->private_data;

	lock_kernel();
	ucb1x00_ts_fasync(-1, filp, 0);

	ucb1x00_enable(ts->ucb);
	ucb1x00_free_irq(ts->ucb, UCB_IRQ_TSPX, ts);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR, 0);
	ucb1x00_disable(ts->ucb);

	if (ts->rtask)
		send_sig(SIGKILL, ts->rtask, 1);
	unlock_kernel();

	return 0;
}

static struct file_operations ucb1x00_fops = {
	owner:		THIS_MODULE,
	read:		ucb1x00_ts_read,
	poll:		ucb1x00_ts_poll,
	open:		ucb1x00_ts_open,
	release:	ucb1x00_ts_release,
	fasync:		ucb1x00_ts_fasync,
};

/*
 * Initialisation.
 *
 * The official UCB1x00 touchscreen is a miscdevice:
 *   10 char        Non-serial mice, misc features
 *                   14 = /dev/touchscreen/ucb1x00  UCB 1x00 touchscreen
 */
static struct miscdevice ucb1x00_ts_dev = {
	minor:	14,
	name:	"touchscreen/ucb1x00",
	fops:	&ucb1x00_fops,
};

static int __init ucb1x00_ts_init(void)
{
	int ret = -ENODEV;

	ucbts.ucb = ucb1x00_get();

	if (ucbts.ucb) {
		init_waitqueue_head(&ucbts.read_wait);

		ret = misc_register(&ucb1x00_ts_dev);
	}
	return ret;
}

static void __exit ucb1x00_ts_exit(void)
{
	misc_deregister(&ucb1x00_ts_dev);
}

module_init(ucb1x00_ts_init);
module_exit(ucb1x00_ts_exit);
