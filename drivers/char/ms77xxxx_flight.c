/*
 * linux/drivers/char/ms77xxxx_flight.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * FRONT_LIGHT driver for ms77xxxx.
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

#define FRONT_LIGHT_NAME "fl"
#define FRONT_LIGHT_MAJOR 252

#define FRONT_LIGHTIOC_LGT_OFF 0x5577
#define FRONT_LIGHTIOC_LGT_ON  0x5578

#ifdef CONFIG_PM
#include <linux/pm.h>
static struct pm_dev* ms77xxxx_flight_pm_dev;
#endif

static int fl_state;

static void fl_off(void)
{
	unsigned char ch;

	fl_state = 0;

	ch = 0x00;
	h8_out(&ch, 1, SH7760SE_LCD_LCDR);
}

static void fl_on(void)
{
	unsigned char ch;

	fl_state = 1;

	ch = 0x01;
	h8_out(&ch, 1, SH7760SE_LCD_LCDR);
}


static ssize_t ms77xxxx_flight_read(struct file* filp, char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;
	char ch;

	if (count > 0) {
		if (fl_state) { ch = '1'; } else { ch = '0'; }
		copy_to_user(buf, &ch, sizeof(ch));
		rc = 1;
	}

	return rc;
}

static ssize_t ms77xxxx_flight_write(struct file* filp, const char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;
	char ch;
	if (count > 0) {
		copy_from_user(&ch, buf, sizeof(ch));
		switch (ch) {
		case '0': fl_off(); break;
		case '1': fl_on();  break;
		}
		rc = 1;
	}
	return rc;
}

static int ms77xxxx_flight_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case FRONT_LIGHTIOC_LGT_OFF:
		fl_off();
		break;
	case FRONT_LIGHTIOC_LGT_ON:
		fl_on();
		break;

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static int ms77xxxx_flight_open(struct inode* inode, struct file* filp)
{
	MOD_INC_USE_COUNT;

	return 0;
}

static int ms77xxxx_flight_release(struct inode* inode, struct file* filp)
{
	MOD_DEC_USE_COUNT;

	return 0;
}

static struct file_operations ms77xxxx_flight_fops = {
	read:	 ms77xxxx_flight_read,
	write:	 ms77xxxx_flight_write,
	ioctl:	 ms77xxxx_flight_ioctl,
	open:	 ms77xxxx_flight_open,
	release: ms77xxxx_flight_release,
};

#ifdef CONFIG_PM
static int ms77xxxx_flight_pm_callback(struct pm_dev* pm_dev,
				       pm_request_t req, void* data)
{
	static int save_fl_state = 0;

	switch (req) {
	case PM_SUSPEND:
		save_fl_state = fl_state;
		fl_off();
		break;
	case PM_RESUME:
		fl_state = save_fl_state;
		if (fl_state) { fl_on(); }
                break;
	}
	return 0;
}
#endif

int __init ms77xxxx_flight_init(void)
{
	int rc;

	rc = devfs_register_chrdev(FRONT_LIGHT_MAJOR, FRONT_LIGHT_NAME, &ms77xxxx_flight_fops);
	if (rc < 0)
		return rc;
	devfs_register(NULL, FRONT_LIGHT_NAME, DEVFS_FL_NONE,
		       FRONT_LIGHT_MAJOR, 0,
		       0666 | S_IFCHR, &ms77xxxx_flight_fops, NULL);

#ifdef CONFIG_PM
	ms77xxxx_flight_pm_dev = pm_register(PM_SYS_DEV, 0,
					     ms77xxxx_flight_pm_callback);
#endif
	fl_on();

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 FRONT LIGHT driver initialized.\n"); 
#else
#error No Platform
#endif

	return rc;
}

#ifdef MODULE
void __exit ms77xxxx_flight_cleanup(void)
{
#ifdef CONFIG_PM
	pm_unregister_all(ms77xxxx_flight_pm_callback);
#endif
	devfs_unregister_chrdev(FRONT_LIGHT_MAJOR, FRONT_LIGHT_NAME);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 FRONT LIGHT driver removed.\n"); 
#else
#error No Platform
#endif
}
#endif

module_init(ms77xxxx_flight_init);
#ifdef MODULE
module_exit(ms77xxxx_flight_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
MODULE_DESCRIPTION("FRONT LIGHT driver for MS7760CP01");
#else
#error No Platform
#endif

