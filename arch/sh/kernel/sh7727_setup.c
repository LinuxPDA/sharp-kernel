/*
 * linux/arch/sh/kernel/sh7727_setup.c
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi SH7727 processor support.
 *
 */
#include <linux/config.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/sh7727.h>

extern void sh7727_dma_init(void);
extern void sh7727_adc_init(void);
	
void __init sh7727_setup(void)
{
	printk(KERN_INFO "Hitachi SH7727 processor support.\n");

#if defined(CONFIG_SH7727_DMA)
	sh7727_dma_init();
#endif
#if defined(CONFIG_SH7727_ADC)
	sh7727_adc_init();
#endif
}
