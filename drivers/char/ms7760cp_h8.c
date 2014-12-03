/*
 * linux/drivers/char/ms7760cp_h8.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
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

#define H8_NAME "h8"
#define H8_MAJOR 249
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
//#define BASE_ADDR MS7760CP_IRRCR
#define BASE_ADDR 0
#endif


static ssize_t ms7760cp_h8_read(struct file* filp, char* buf, size_t count, loff_t* l)
{
	ssize_t rc = -1;

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	unsigned short st = BASE_ADDR + (short)*l;
	unsigned char wk[256];
	if (count > 256) { count = 256; }
	h8_in(wk, count, st);
	if (copy_to_user(buf, wk, count))
		return -EFAULT;
	rc = count;
#endif
	return rc;
}

static ssize_t ms7760cp_h8_write(struct file* filp,
				 const char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	unsigned short st = BASE_ADDR + (short)*l;
	unsigned char wk[256];
	if (count > 256) { count = 256; }
	if (copy_from_user(wk, buf, count))
		return -EFAULT;
	h8_out(wk, count, st);
	rc = count;
#endif
	return rc;
}

static int ms7760cp_h8_ioctl(struct inode* inode, struct file* filp,
			     unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case 0:
		break;
	}

	return -1;
}

static int ms7760cp_h8_open(struct inode* inode, struct file* filp)
{
	MOD_INC_USE_COUNT;

	return 0;
}

static int ms7760cp_h8_release(struct inode* inode, struct file* filp)
{
	MOD_DEC_USE_COUNT;

	return 0;
}

static struct file_operations ms7760cp_h8_fops = {
	read:	 ms7760cp_h8_read,
	write:	 ms7760cp_h8_write,
	ioctl:	 ms7760cp_h8_ioctl,
	open:	 ms7760cp_h8_open,
	release: ms7760cp_h8_release,
};

int __init ms7760cp_h8_init(void)
{
	int rc;

	rc = devfs_register_chrdev(H8_MAJOR, H8_NAME, &ms7760cp_h8_fops);
	if (rc < 0)
		return rc;
	devfs_register(NULL, H8_NAME, DEVFS_FL_NONE, H8_MAJOR, 0,
		       0666 | S_IFCHR, &ms7760cp_h8_fops, NULL);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 H8 driver initialized.\n"); 
#else
#error No Platform
#endif

	return rc;
}

#ifdef MODULE
void __exit ms7760cp_h8_cleanup(void)
{
	devfs_unregister_chrdev(H8_MAJOR, H8_NAME);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 H8 driver removed.\n"); 
#else
#error No Platform
#endif
}
#endif

module_init(ms7760cp_h8_init);
#ifdef MODULE
module_exit(ms7760cp_h8_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
MODULE_DESCRIPTION("H8IR driver for MS7760CP01");
#else
#error No Platform
#endif

