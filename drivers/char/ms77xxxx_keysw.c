/*
 * linux/drivers/char/ms77xxxx_key.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * KEYSW driver for ms77xxxx.
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

#define KEYSW_NAME "keysw"
#define KEYSW_MAJOR 251

#ifndef NO_USE_H8_INT
#define BUFSIZE 128
#define PT_KEYS 0x04
#define STS_KEY_ALLS 0x1e
#define STS_KEY_ONF 0x02
#define STS_KEY_OFFF 0x04
#define STS_KEY_ARKEYF 0x08
#define STS_KEY_PONSWF 0x10

static unsigned char chk_bit[4] = {
	STS_KEY_ONF,
	STS_KEY_OFFF,
	STS_KEY_ARKEYF,
	STS_KEY_PONSWF
};

static unsigned long set_state[4] = {
	0x10,0x20,0x40,0x80
};

static int head = 0, tail = 0;
static unsigned int buf[BUFSIZE];

static unsigned int key_irq_kind;
static unsigned int key_data;

static DECLARE_WAIT_QUEUE_HEAD(ms77xxxx_key_wait);
static struct fasync_struct* ms77xxxx_key_async_queue;

static spinlock_t ms77xxxx_key_lock = SPIN_LOCK_UNLOCKED;
#else
#define GETKEYSW 0x5589
#endif

#ifndef NO_USE_H8_INT
static void sh_key_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char	rtkisr;
	unsigned char	rtkeysr;

	int kind_set;

	kind_set = key_irq_kind = 0;
	spin_lock (&ms77xxxx_key_lock);
					// SYSTEM STATUS
	h8_in(&rtkisr, 1, SH7760SE_RTKISR);

	if (rtkisr & PT_KEYS){
		h8_in(&rtkeysr, 1, SH7760SE_KEY_KEYSR);
		h8_in((char*)&key_data, 2, SH7760SE_KEY_KBITPR);
		while(rtkeysr & STS_KEY_ALLS){
			if (rtkeysr & chk_bit[kind_set]){
				rtkeysr &= (~(chk_bit[kind_set]) & 0xfe);
				h8_out(&rtkeysr, 1, SH7760SE_KEY_KEYSR);
				key_irq_kind |= set_state[kind_set];
			}
			kind_set++;
		}
		rtkisr &= ~PT_KEYS;
		h8_out(&rtkisr, 1, MS7760CP_RTKISR);
	}

	spin_unlock (&ms77xxxx_key_lock);

	buf[head] = key_data | key_irq_kind;

	if (++head >= BUFSIZE) {
		head = 0;
	}
	if ((head == tail) && (++tail >= BUFSIZE)) {
	  	tail = 0;
	}

	if (ms77xxxx_key_async_queue) {
		kill_fasync(&ms77xxxx_key_async_queue, SIGIO, POLL_IN); 
	}
	wake_up_interruptible(&ms77xxxx_key_wait);
}
#else
static unsigned short key_scan(void)
{
	unsigned short nch;
	h8_in((char*)&nch, 2, SH7760SE_KEY_KBITPR);
	return nch;
}

#endif

#ifndef NO_USE_H8_INT
static int ms77xxxx_key_fasync (int fd, struct file *filp, int on)
{
        return fasync_helper (fd, filp, on, &ms77xxxx_key_async_queue);
}

static unsigned int get_data(void)
{
	int last = tail;

	if (++tail == BUFSIZE) {
		tail = 0;
	}
	return buf[last];
}
static ssize_t ms77xxxx_key_read(struct file *file, char *buf,
                           size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned int out;
	int i;

	if (head == tail) {
		add_wait_queue(&ms77xxxx_key_wait, &wait);
		current->state = TASK_INTERRUPTIBLE;
		while ((head == tail) && !signal_pending(current)) {
			schedule();
			current->state = TASK_INTERRUPTIBLE;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&ms77xxxx_key_wait, &wait);
	}

	for (i = count; i >= sizeof(out);
		     i -= sizeof(out), buf += sizeof(out)) {
		if (head == tail)
		  	break;
		out = get_data();

		copy_to_user(buf, &out, sizeof(out));
	}
	return count - i;
}

static unsigned int ms77xxxx_key_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &ms77xxxx_key_wait, wait);
	if (head != tail) {
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

#else
static ssize_t ms77xxxx_key_read(struct file* filp, char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;

	return rc;
}
#endif


static ssize_t ms77xxxx_key_write(struct file* filp, const char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;

	return rc;
}

static int ms77xxxx_key_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
{
#ifdef NO_USE_H8_INT
	unsigned	short keycode;
#endif
	switch (cmd) {
#ifdef NO_USE_H8_INT
		case GETKEYSW:
			keycode = key_scan();
			break;
#endif
		default:
			return -EINVAL;
			break;
	}
#ifdef NO_USE_H8_INT
	return copy_to_user((void *)arg, &keycode, sizeof(unsigned short)) ? -EFAULT : 0;
#endif
}

static int ms77xxxx_key_open(struct inode* inode, struct file* filp)
{
	MOD_INC_USE_COUNT;

	return 0;
}

static int ms77xxxx_key_release(struct inode* inode, struct file* filp)
{
	MOD_DEC_USE_COUNT;

	return 0;
}

static struct file_operations ms77xxxx_key_fops = {
	owner:	 THIS_MODULE,
	read:	 ms77xxxx_key_read,
	write:	 ms77xxxx_key_write,
#ifndef NO_USE_H8_INT
	poll:	 ms77xxxx_key_poll,
#endif
	ioctl:	 ms77xxxx_key_ioctl,
	open:	 ms77xxxx_key_open,
	release: ms77xxxx_key_release,
#ifndef NO_USE_H8_INT
	fasync:	 ms77xxxx_key_fasync,
#endif
};

int __init ms77xxxx_key_init(void)
{
	int rc;
	unsigned char ch;

#ifndef NO_USE_H8_INT
	key_data = 0;
	key_irq_kind = 0;
#endif

	rc = devfs_register_chrdev(KEYSW_MAJOR, KEYSW_NAME, &ms77xxxx_key_fops);
	if (rc < 0)
		return rc;
	devfs_register(NULL, KEYSW_NAME, DEVFS_FL_NONE, KEYSW_MAJOR, 0,
		       0444 | S_IFCHR, &ms77xxxx_key_fops, NULL);

#ifndef NO_USE_H8_INT
	if ((rc = request_irq(SH7760SE_IRQ_H8IRQ, sh_key_interrupt,
			       SA_SHIRQ | SA_INTERRUPT, KEYSW_NAME,sh_key_interrupt))) {
		printk(KERN_ERR "ms77xx_key: failed to register H8IRQ\n");
		return rc;
	}
#endif

#ifndef NO_USE_H8_INT
	ch = 0x2f;	/* KEY input Enable*/
#else
	ch = 0x21;	/* KEY input Enable*/
#endif
	h8_out(&ch, 1, SH7760SE_KEY_KEYCR);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 KEYSW driver initialized.\n"); 
#else
#error No Platform
#endif

	return rc;
}

#ifdef MODULE
void __exit ms77xxxx_key_cleanup(void)
{

#ifndef NO_USE_H8_INT
	free_irq(SH7760SE_IRQ_H8IRQ,sh_key_interrupt);
#endif
	devfs_unregister_chrdev(KEYSW_MAJOR, KEYSW_NAME);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 KEYSW driver removed.\n"); 
#else
#error No Platform
#endif
}
#endif

module_init(ms77xxxx_key_init);
#ifdef MODULE
module_exit(ms77xxxx_key_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
MODULE_DESCRIPTION("LED driver for MS7760CP01");
#else
#error No Platform
#endif

