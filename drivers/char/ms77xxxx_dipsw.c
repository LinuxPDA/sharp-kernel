/*
 * linux/drivers/char/ms77xxxx_dipsw.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * DIPSW driver for ms77xxxx.
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

#define GETDIPSW 0x5588

#define DSW_NAME "dipsw"
#define DSW_MAJOR 253

static ssize_t ms77xxxx_dipsw_read(struct file* filp, char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;

	char tmp[4];
	sprintf(tmp, "%02X", ctrl_inb(PA_DIPSW0));

	if (count > 0) {
		if (count > 2) { count = 2; }
		copy_to_user(buf, tmp, count);
		rc = count;
	}

	return rc;
}

static int ms77xxxx_dipsw_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
{
	unsigned	char	sw5;

	switch (cmd) {
		case GETDIPSW:
			sw5 = ctrl_inb(PA_DIPSW0);
			break;
	
		default:
			return -EINVAL;
			break;
	}
	return copy_to_user((void *)arg, &sw5, 1) ? -EFAULT : 0;
}

static int ms77xxxx_dipsw_open(struct inode* inode, struct file* filp)
{
	MOD_INC_USE_COUNT;

	return 0;
}

static int ms77xxxx_dipsw_release(struct inode* inode, struct file* filp)
{
	MOD_DEC_USE_COUNT;

	return 0;
}

static struct file_operations ms77xxxx_dipsw_fops = {
	read:	 ms77xxxx_dipsw_read,
	ioctl:	 ms77xxxx_dipsw_ioctl,
	open:	 ms77xxxx_dipsw_open,
	release: ms77xxxx_dipsw_release,
};

int __init ms77xxxx_dipsw_init(void)
{
	int rc;

#if 0
	unsigned char sw5 = ctrl_inb(PA_DIPSW0);
#endif
	rc = devfs_register_chrdev(DSW_MAJOR, DSW_NAME, &ms77xxxx_dipsw_fops);
	if (rc < 0)
		return rc;
	devfs_register(NULL, DSW_NAME, DEVFS_FL_NONE, DSW_MAJOR, 0,
		       0444 | S_IFCHR, &ms77xxxx_dipsw_fops, NULL);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 DIPSW driver initialized.\n"); 

#if 0
	printk("ID register:\n");
	printk(" SW5-1: %s\n", (sw5 & 0x01)? "OFF":"ON");
	printk(" SW5-2: %s\n", (sw5 & 0x02)? "OFF":"ON");
	printk(" SW5-3: %s\n", (sw5 & 0x04)? "OFF":"ON");
	printk(" SW5-4: %s\n", (sw5 & 0x08)? "OFF":"ON");
	printk(" SW5-5: %s\n", (sw5 & 0x10)? "OFF":"ON");
	printk(" SW5-6: %s\n", (sw5 & 0x20)? "OFF":"ON");
	/* manual is correct ? */
	printk(" SW5-7: %s\n", (sw5 & 0x40)? "ON":"OFF");
	printk(" SW5-8: %s\n", (sw5 & 0x80)? "ON":"OFF");
#endif

#else
#error No Platform
#endif

	return rc;
}

#ifdef MODULE
void __exit ms77xxxx_dipsw_cleanup(void)
{
	devfs_unregister_chrdev(DSW_MAJOR, DSW_NAME);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 DIPSW driver removed.\n"); 
#else
#error No Platform
#endif
}
#endif

module_init(ms77xxxx_dipsw_init);
#ifdef MODULE
module_exit(ms77xxxx_dipsw_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
MODULE_DESCRIPTION("DIPSW driver for MS7760CP01");
#else
#error No Platform
#endif
