/*
 * linux/drivers/char/mx2-led-sw.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define LED_SW_VERSION "0.0.1"

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/miscdevice.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/arch/mx2.h>
#include <asm/arch/platform.h>
#include <asm/arch/led_sw.h>

static int led_sw_major = 242;
static int result = 0;
static devfs_handle_t led_sw_devfs_handle;
static struct fasync_struct *led_sw_fasync;


static int check_device(struct inode *p)
{
	int minor;
	kdev_t dev = p->i_rdev;

	if(MAJOR(dev) != led_sw_major) {
		printk("LED, SW: check_device bad major = %d\n",MAJOR(dev) );
		return -1;
	}

	minor = MINOR(dev);
	if (!minor)
		return minor;
	else {
		printk("LED, SW: check_device bad minor = %d\n",minor );
		return -1;
	}
}

static int mx2_led_sw_fasync(int fd, struct file *filp, int mode)
{
	return(fasync_helper(fd, filp, mode, &led_sw_fasync));
}

static int led_sw_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		     unsigned long arg)
{
	unsigned long val;

	if(check_device(inode) == -1) {
		printk("led_sw_ioctl:bad minor\n");
		return -ENODEV;
	}

#ifdef MX2ADS_MODE
	switch (cmd) {
	case GET_MMAPED_IO:
	{
		val = inw(MX2_MMAPED_IO);
		return copy_to_user((unsigned long *)arg, &val, sizeof val)
				? -EFAULT : 0;
	}
	case SET_MMAPED_IO:
	{
		if(copy_from_user(&val, (unsigned long *)arg, sizeof arg))
				return -EFAULT;
		val &= 0xC000;
		val |= (inw(MX2_MMAPED_IO) & ~0xC000);
		outw(val, MX2_MMAPED_IO);
		
		return 0;
	}
	case GET_GPIOD_DATA:
	{
		val = _reg_GPIO_SSR(GPIOD);
		return copy_to_user((unsigned long *)arg, &val, sizeof val)
				? -EFAULT : 0;
	}
	default:
		return -EINVAL;
	}
#else
	switch (cmd) {
	case GET_SW_DATA:
	{
		//Add data read routine.
		break;
	}
	case GET_LED_DATA:
	{
		//Add data read routine.
		break;
	}
	case SET_LED_DATA:
	{
		if(copy_from_user(&val, (unsigned int *)arg, sizeof val))
			return -EFAULT;
		//Add data write routine.

		return 0;
	}
	case GET_IP_DATA:
	{
		//Add data read routine.
		break;
	}
	default:
		return -EINVAL;
	}
	
	return copy_to_user((unsigned int *)arg, &val, sizeof val)
				? -EFAULT : 0;
#endif /* MX2ADS_MODE */
}

static int led_sw_open(struct inode *inode, struct file *file)
{
	if(check_device(inode) == -1) {
		printk("led_sw_ioctl:bad minor\n");
		return -ENODEV;
	}

	MOD_INC_USE_COUNT;

	return 0;
}

static int led_sw_release(struct inode *inode, struct file *file)
{
	mx2_led_sw_fasync(-1, file, 0);
	MOD_DEC_USE_COUNT;
	return 0;
}

static struct file_operations led_sw_fops = {
	open:		led_sw_open,
	release:	led_sw_release,
	ioctl:		led_sw_ioctl,
	fasync:		mx2_led_sw_fasync
};

int __init mx2_led_sw_init(void)
{
#ifdef MX2ADS_MODE
	/* initialize GPIO-D21(CSPI2_SS0)*/
	_reg_GPIO_GIUS(GPIOD) &= ~0x200000;
	_reg_GPIO_DDIR(GPIOD) &= ~0x200000;
#else
	/* KPP initialize */
	
	/* USB GPIO initialize */
#endif
	result = register_chrdev(led_sw_major, "led_sw", &led_sw_fops);
	if(result < 0) {
		printk("LED, SW driver: Unable to register driver\n");
		return -ENODEV;
	}
	
	led_sw_devfs_handle = devfs_register(NULL, "led_sw",
						DEVFS_FL_DEFAULT,
						led_sw_major, 0,
						S_IFCHR | S_IRUSR | S_IWUSR,
						&led_sw_fops, NULL);

	printk(KERN_INFO "MX2ADS LED, DipSW Driver v" LED_SW_VERSION "\n");
	
	return 0;
}

static void __exit mx2_led_sw_exit (void)
{
	if(result>0) {
		devfs_unregister_chrdev(led_sw_major, "led_sw");
		devfs_unregister(led_sw_devfs_handle);
	}
}

module_init(mx2_led_sw_init);
module_exit(mx2_led_sw_exit);
EXPORT_NO_SYMBOLS;
