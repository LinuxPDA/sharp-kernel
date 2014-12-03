/*
 * linux/include/asm-arm/arch-dbmx2/led_sw.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __MX2ADS_LED_SW_H__
#define __MX2ADS_LED_SW_H__

#define MX2ADS_MODE

/* ioctl commands */
#define LED_IOCTL_MAGIC	('A')

#ifdef MX2ADS_MODE
/* Memory Mapped IO */
#define MX2_MMAPED_IO	MX2ADS_PER_IOBASE+0x800000	/* 16 bit, Bit15:LED3, Bit14:LED4, Bit0:SW_SEL */
#define GET_MMAPED_IO		_IOR(LED_IOCTL_MAGIC, 0x20, unsigned long)
#define SET_MMAPED_IO		_IOW(LED_IOCTL_MAGIC, 0x21, unsigned long)
#define GET_GPIOD_DATA		_IOR(LED_IOCTL_MAGIC, 0x22, unsigned long)
#else // !MX2ADS_MODE
#define GET_SW_DATA		_IOR(LED_IOCTL_MAGIC, 0x20, unsigned long)
#define GET_LED_DATA		_IOR(LED_IOCTL_MAGIC, 0x21, unsigned long)
#define SET_LED_DATA		_IOW(LED_IOCTL_MAGIC, 0x22, unsigned long)
#define GET_IP_DATA		_IOR(LED_IOCTL_MAGIC, 0x23, unsigned long)
#endif


#endif // __MXADS2_LED_SW_H__
