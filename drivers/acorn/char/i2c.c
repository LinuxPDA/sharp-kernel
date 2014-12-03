/*
 *  linux/drivers/acorn/char/i2c.c
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  ARM IOC/IOMD i2c driver.
 *
 *  On Acorn machines, the following i2c devices are on the bus:
 *	- PCF8583 real time clock & static RAM
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/rtc.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/hardware/ioc.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#include "pcf8583.h"

extern int (*set_rtc)(void);

static struct i2c_client *rtc_client;

static inline int rtc_command(int cmd, void *data)
{
	int ret = -EIO;

	if (rtc_client)
		ret = rtc_client->driver->command(rtc_client, cmd, data);

	return ret;
}

/*
 * Read the current RTC time and date, and update xtime.
 */
static void get_rtc_time(struct rtc_tm *rtctm, unsigned char *year)
{
	unsigned char ctrl;
	struct mem rtcmem = { 0xc0, 1, year };

	/*
	 * Ensure that the RTC is running.
	 */
	rtc_command(RTC_GETCTRL, &ctrl);
	if (ctrl & 0xc0) {
		unsigned char new_ctrl;

		new_ctrl = ctrl & ~0xc0;

		printk("RTC: resetting control %02X -> %02X\n",
		       ctrl, new_ctrl);

		rtc_command(RTC_SETCTRL, &new_ctrl);
	}

	/*
	 * Acorn machines store the year in
	 * the static RAM at location 192.
	 */
	if (rtc_command(MEM_READ, &rtcmem))
		return;

	if (rtc_command(RTC_GETDATETIME, rtctm))
		return;

	if (*year < 70)
		*year += 100;
}

/*
 * Set the RTC time only.  Note that
 * we do not touch the date.
 */
static int set_rtc_time(void)
{
	struct rtc_tm new_rtctm, old_rtctm;
	unsigned long nowtime = xtime.tv_sec;

	if (rtc_command(RTC_GETDATETIME, &old_rtctm))
		return 0;

	new_rtctm.cs    = xtime.tv_usec / 10000;
	new_rtctm.secs  = nowtime % 60;	nowtime /= 60;
	new_rtctm.mins  = nowtime % 60;	nowtime /= 60;
	new_rtctm.hours = nowtime % 24;

	/*
	 * avoid writing when we're going to change the day
	 * of the month.  We will retry in the next minute.
	 * This basically means that if the RTC must not drift
	 * by more than 1 minute in 11 minutes.
	 *
	 * [ rtc: 1/1/2000 23:58:00, real 2/1/2000 00:01:00,
	 *   rtc gets set to 1/1/2000 00:01:00 ]
	 */
	if ((old_rtctm.hours == 23 && old_rtctm.mins  == 59) ||
	    (new_rtctm.hours == 23 && new_rtctm.mins  == 59))
		return 1;

	return rtc_command(RTC_SETTIME, &new_rtctm);
}

static int rtc_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	unsigned char year;
	struct rtc_time rtctm;
	struct rtc_tm rtc_raw;

	switch (cmd) {
	case RTC_ALM_READ:
	case RTC_ALM_SET:
		break;

	case RTC_RD_TIME:
		get_rtc_time(&rtc_raw, &year);
		rtctm.tm_sec  = rtc_raw.secs;
		rtctm.tm_min  = rtc_raw.mins;
		rtctm.tm_hour = rtc_raw.hours;
		rtctm.tm_mday = rtc_raw.mday;
		rtctm.tm_mon  = rtc_raw.mon;
		rtctm.tm_year = year;
		return copy_to_user((void *)arg, &rtctm, sizeof(rtctm))
				 ? -EFAULT : 0;

	case RTC_SET_TIME:
		break;
	}
	return -EINVAL;
}

static struct file_operations rtc_fops = {
	ioctl:	rtc_ioctl,
};

static struct miscdevice rtc_dev = {
	minor:	RTC_MINOR,
	name:	"rtc",
	fops:	&rtc_fops,
};

/* IOC / IOMD i2c driver */

#define FORCE_ONES	0xdc
#define SCL		0x02
#define SDA		0x01

/*
 * We must preserve all non-i2c output bits in IOC_CONTROL.
 * Note also that we need to preserve the value of SCL and
 * SDA outputs as well (which may be different from the
 * values read back from IOC_CONTROL).
 */
static u_int force_ones;

static void ioc_setscl(void *data, int state)
{
	u_int ioc_control = ioc_readb(IOC_CONTROL) & ~(SCL | SDA);
	u_int ones = force_ones;

	if (state)
		ones |= SCL;
	else
		ones &= ~SCL;

	force_ones = ones;

 	ioc_writeb(ioc_control | ones, IOC_CONTROL);
}

static void ioc_setsda(void *data, int state)
{
	u_int ioc_control = ioc_readb(IOC_CONTROL) & ~(SCL | SDA);
	u_int ones = force_ones;

	if (state)
		ones |= SDA;
	else
		ones &= ~SDA;

	force_ones = ones;

 	ioc_writeb(ioc_control | ones, IOC_CONTROL);
}

static int ioc_getscl(void *data)
{
	return (ioc_readb(IOC_CONTROL) & SCL) != 0;
}

static int ioc_getsda(void *data)
{
	return (ioc_readb(IOC_CONTROL) & SDA) != 0;
}

static struct i2c_algo_bit_data ioc_data = {
	setsda:		ioc_setsda,
	setscl:		ioc_setscl,
	getsda:		ioc_getsda,
	getscl:		ioc_getscl,
	udelay:		 80,
	mdelay:		 80,
	timeout:	100
};

static int ioc_client_reg(struct i2c_client *client)
{
	if (client->id == I2C_DRIVERID_PCF8583 &&
	    client->addr == 0x50) {
		struct rtc_tm rtctm;
		unsigned char year;

		rtc_client = client;
		get_rtc_time(&rtctm, &year);

		xtime.tv_usec = rtctm.cs * 10000;
		xtime.tv_sec  = mktime(1900 + year, rtctm.mon, rtctm.mday,
				       rtctm.hours, rtctm.mins, rtctm.secs);
		set_rtc = set_rtc_time;
	}

	return 0;
}

static int ioc_client_unreg(struct i2c_client *client)
{
	if (client == rtc_client) {
		set_rtc = NULL;
		rtc_client = NULL;
	}

	return 0;
}

static struct i2c_adapter ioc_ops = {
	name:			"IOC/IOMD",
	id:			I2C_HW_B_IOC,
	algo_data:		&ioc_data,
	client_register:	ioc_client_reg,
	client_unregister:	ioc_client_unreg
};

static int __init i2c_ioc_init(void)
{
	int ret;

	force_ones = FORCE_ONES | SCL | SDA;

	ret = i2c_bit_add_bus(&ioc_ops);

	if (ret >= 0)
		misc_register(&rtc_dev);

	return ret;
}

__initcall(i2c_ioc_init);
