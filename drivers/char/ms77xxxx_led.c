/*
 * linux/drivers/char/ms77xxxx_led.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * LED driver for ms77xxxx.
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

#define LED_NAME "led"
#define LED_MAJOR 250

#ifndef CONFIG_LED_MS7760CP_H8
#define CONFIG_LED_MS7760CP_H8 1
#endif
#undef CONFIG_LED_MS7760CP_H8

static ssize_t ms77xxxx_led_read(struct file* filp, char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;

	return rc;
}

static ssize_t ms77xxxx_led_write(struct file* filp, const char* buf, size_t count, loff_t* l)
{
	ssize_t rc = -1;
	char tmp[4];
	int v;
	unsigned char wk;

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	if(copy_from_user(&wk, buf, 1))
	    return -EFAULT;

#if defined(CONFIG_LED_MS7760CP_H8)
	h8_out(&wk,1,MS7760CP_LEDR);
#else
	ctrl_outw(wk, 0xA1400000);
#endif

	rc = 1;
#endif

	return rc;
}

static int ms77xxxx_led_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
{
	unsigned char wk;

	switch (cmd) {
	case 1:
		copy_from_user(&wk, (void *)arg, 1);
		h8_out(&wk, 1, MS7760CP_LEDR);
		break;
	case 3:
		copy_from_user(&wk, (void *)arg, 1);
		ctrl_outw(wk, 0xA1400000);
		break;

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static int ms77xxxx_led_open(struct inode* inode, struct file* filp)
{
	MOD_INC_USE_COUNT;

	return 0;
}

static int ms77xxxx_led_release(struct inode* inode, struct file* filp)
{
	MOD_DEC_USE_COUNT;

	return 0;
}

static struct file_operations ms77xxxx_led_fops = {
	read:	 ms77xxxx_led_read,
	write:	 ms77xxxx_led_write,
	ioctl:	 ms77xxxx_led_ioctl,
	open:	 ms77xxxx_led_open,
	release: ms77xxxx_led_release,
};

int __init ms77xxxx_led_init(void)
{
	int rc;

	rc = devfs_register_chrdev(LED_MAJOR, LED_NAME, &ms77xxxx_led_fops);
	if (rc < 0)
		return rc;
	devfs_register(NULL, LED_NAME, DEVFS_FL_NONE, LED_MAJOR, 0,
		       0666 | S_IFCHR, &ms77xxxx_led_fops, NULL);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 LED driver initialized.\n"); 
#else
#error No Platform
#endif

	return rc;
}

#ifdef MODULE
void __exit ms77xxxx_led_cleanup(void)
{
	devfs_unregister_chrdev(LED_MAJOR, LED_NAME);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 LED driver removed.\n"); 
#else
#error No Platform
#endif
}
#endif

module_init(ms77xxxx_led_init);
#ifdef MODULE
module_exit(ms77xxxx_led_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
MODULE_DESCRIPTION("LED driver for MS7760CP01");
#else
#error No Platform
#endif

