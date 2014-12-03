/*
 *  linux/drivers/char/pfs168_dtmf.c
 *
 *  Copyright (C) 2001, MontaVista Software, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  15 February 2001 - created.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/malloc.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/switches.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>


#define PFS168_DTMF_NAME "pfs168_dtmf"

#define DTMF_BUF_SIZE		64

struct dtmf_queue {
	unsigned long head;
	unsigned long tail;
	wait_queue_head_t proc_list;
	unsigned char buf[DTMF_BUF_SIZE];
};

static struct dtmf_queue *queue;        /* DTMF data buffer. */
static int dtmf_count = 0;

static spinlock_t dtmf_lock = SPIN_LOCK_UNLOCKED;


static void pfs168_dtmf_handler(int, void *, struct pt_regs *);

static unsigned char get_from_queue(void)
{
       unsigned char result;
       unsigned long flags;

       spin_lock_irqsave(&dtmf_lock, flags);
       result = queue->buf[queue->tail];
       queue->tail = (queue->tail + 1) & (DTMF_BUF_SIZE-1);
       spin_unlock_irqrestore(&dtmf_lock, flags);

       return result;
}


static inline int queue_empty(void)
{
       return queue->head == queue->tail;
}


static ssize_t dtmf_read(struct file *file, char *buffer, size_t count, loff_t *pos) {
	DECLARE_WAITQUEUE(wait, current);
	ssize_t i = count;
	unsigned char c;

	if (queue_empty()) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		add_wait_queue(&queue->proc_list, &wait);

repeat:
		set_current_state(TASK_INTERRUPTIBLE);

		if (queue_empty() && !signal_pending(current)) {
			schedule();
			goto repeat;
		}

		current->state = TASK_RUNNING;
		remove_wait_queue(&queue->proc_list, &wait);
	}

	while (i > 0 && !queue_empty()) {
		c = get_from_queue();
		put_user(c, buffer++);
		i--;
	}

	if (count-i) {
		file->f_dentry->d_inode->i_atime = CURRENT_TIME;
		return count-i;
	}

	if (signal_pending(current))
		return -ERESTARTSYS;

	return 0;
}

static ssize_t dtmf_write(struct file *file, const char *buffer, size_t count, loff_t *ppos) {
	return -EINVAL;
}

static unsigned int dtmf_poll(struct file *file, poll_table *wait) {
       poll_wait(file, &queue->proc_list, wait);

       if (!queue_empty())
               return POLLIN | POLLRDNORM;

       return 0;
}

static int dtmf_open(struct inode *inode, struct file *file) {
	if(dtmf_count > 0)
		return -EBUSY;

	queue->head = queue->tail = 0;          /* Flush input queue */

	MOD_INC_USE_COUNT;
	++dtmf_count;

	return 0;
}

static int dtmf_release(struct inode *inode, struct file *file) {
	--dtmf_count;
	MOD_DEC_USE_COUNT;

	return 0;
}

static struct file_operations dtmf_ops = {
	read:		dtmf_read,
	write:		dtmf_write,
	poll:		dtmf_poll,
	open:		dtmf_open,
	release:	dtmf_release
};

static struct miscdevice dtmf_misc = {
	MISC_DYNAMIC_MINOR, PFS168_DTMF_NAME, &dtmf_ops
};


static int dtmf_init(void) {
	queue = (struct dtmf_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	memset(queue, 0, sizeof(*queue));
	queue->head = queue->tail = 0;
	init_waitqueue_head(&queue->proc_list);

	set_GPIO_IRQ_edge(GPIO_GPIO16, GPIO_RISING_EDGE);

	if (request_irq(IRQ_GPIO16, pfs168_dtmf_handler, SA_INTERRUPT, PFS168_DTMF_NAME, NULL) < 0) {
		printk(KERN_ERR "%s: unable to register IRQ for GPIO 16\n", PFS168_DTMF_NAME);
		return -EIO;
	}

	return 0;
}

static int dtmf_shutdown(void) {
	free_irq(IRQ_GPIO16, NULL);
	kfree(queue);
	return 0;
}

static void pfs168_dtmf_handler(int irq, void *dev_id, struct pt_regs *regs) {
	if (dtmf_count) {
		int head = queue->head;

		queue->buf[head]  = "d1234567890*#abc"[PFS168_SYSDTMF & 0x0f];
		head = (head + 1) & (DTMF_BUF_SIZE-1);
		if (head != queue->tail) {
			queue->head = head;
			wake_up_interruptible(&queue->proc_list);
		}	/* Else, Queue is full, so throw it away... */
	}
}

static int __init pfs168_dtmf_init(void) {
	if (dtmf_init() < 0) {
		printk(KERN_ERR "%s: unable to initialize PFS-168 DTMF\n", PFS168_DTMF_NAME);
		return -EIO;
	}

	if (misc_register(&dtmf_misc)<0) {
		printk(KERN_ERR "%s: unable to register misc device\n", PFS168_DTMF_NAME);
		return -EIO;
	}

	printk("PFS-168 DTMF initialized\n");

	return 0;
}

static void __exit pfs168_dtmf_exit(void) {
	dtmf_shutdown();

	if (misc_deregister(&dtmf_misc) < 0)
		printk(KERN_ERR "%s: unable to deregister misc device\n", PFS168_DTMF_NAME);
}

module_init(pfs168_dtmf_init);
module_exit(pfs168_dtmf_exit);
