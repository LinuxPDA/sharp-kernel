/*
 * linux/drivers/char/ms77xxxx_serial_eeprom.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * Serial EEPROM driver for ms77xxxx.
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

#define SEEPROM_NAME "serial_eeprom"
#define SEEPROM_MAJOR 248

#define DR_SIZE  MS7760CP_EEPDR_SIZE

static ssize_t ms77xxxx_serial_eeprom_read(struct file* filp, char* buf, size_t count, loff_t* l)
{
    ssize_t rc = 0;
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
    unsigned char dr[DR_SIZE];
    unsigned short stad;

    stad =  MS7760CP_EEPDR + (unsigned short)(*l);

    if (count + *l > DR_SIZE) {
      count = DR_SIZE - *l;
    }

    h8_in(dr, count, stad);

    if (copy_to_user(buf, dr, count))
	return -EFAULT;

    rc = count;
#endif
	
    return rc;
}

static ssize_t ms77xxxx_serial_eeprom_write(struct file* filp, const char* buf, size_t count, loff_t* l)
{
	ssize_t rc = 0;
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	unsigned char sr,dr[DR_SIZE],vf[DR_SIZE];
	unsigned short stad =  MS7760CP_EEPDR + (unsigned short)(*l);

	if ( count + *l > DR_SIZE)
	  return -EINVAL;

	if (copy_from_user(dr,buf,count))
	  return -EFAULT;

	h8_out(dr, count, stad);

						// verify
    h8_in(vf, count, stad);

	if (memcmp(dr,vf,count) != 0)
		return -EFAULT;

	rc = count;
#endif
	return rc;
}

static int ms77xxxx_serial_eeprom_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case 0:
		break;
	}

	return 0;
}

static int ms77xxxx_serial_eeprom_open(struct inode* inode, struct file* filp)
{
	MOD_INC_USE_COUNT;

	return 0;
}

static int ms77xxxx_serial_eeprom_release(struct inode* inode, struct file* filp)
{
	MOD_DEC_USE_COUNT;

	return 0;
}

static struct file_operations ms77xxxx_serial_eeprom_fops = {
	read:	 ms77xxxx_serial_eeprom_read,
	write:	 ms77xxxx_serial_eeprom_write,
	ioctl:	 ms77xxxx_serial_eeprom_ioctl,
	open:	 ms77xxxx_serial_eeprom_open,
	release: ms77xxxx_serial_eeprom_release,
};

int __init ms77xxxx_serial_eeprom_init(void)
{
	int rc;

	rc = devfs_register_chrdev(SEEPROM_MAJOR, SEEPROM_NAME, &ms77xxxx_serial_eeprom_fops);
	if (rc < 0)
		return rc;
	devfs_register(NULL, SEEPROM_NAME, DEVFS_FL_NONE, SEEPROM_MAJOR, 0,
		       0666 | S_IFCHR, &ms77xxxx_serial_eeprom_fops, NULL);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 Serial EEPROM driver initialized.\n"); 
#else
#error No Platform
#endif

	return rc;
}

#ifdef MODULE
void __exit ms77xxxx_serial_eeprom_cleanup(void)
{
	devfs_unregister_chrdev(SEEPROM_MAJOR, SEEPROM_NAME);

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	printk(KERN_INFO "MS7760CP01 Serial EEPROM driver removed.\n"); 
#else
#error No Platform
#endif
}
#endif

module_init(ms77xxxx_serial_eeprom_init);
#ifdef MODULE
module_exit(ms77xxxx_serial_eeprom_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
MODULE_DESCRIPTION("Serial EEPROM driver for MS7760CP01");
#else
#error No Platform
#endif

