/*
 *  linux/arch/arm/mach-pxa/generic.c
 *
 *  Author:	Nicolas Pitre
 *  Created:	Jun 15, 2001
 *  Copyright:	MontaVista Software Inc.
 * 
 * Code common to all PXA machines.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Since this file should be linked before any other machine specific file,
 * the __initcall() here will be executed first.  This serves as default
 * initialization stuff for PXA machines which can be overriden later if
 * need be.
 *
 * ChangLog:
 *	12-Dec-2002 Lineo Japan, Inc.
 *	05-Aug-2004 Lineo Solutions, Inc.  for PXA27x
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>

#include <asm/hardware.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/mach/map.h>

#include "generic.h"

/*
 * Various clock factors driven by the CCCR register.
 */

#if defined(CONFIG_CPU_PXA27X)

/* Run Mode Frequency to Turbo Mode Frequency Multiplier (N) */
/* Note: we store the value N * 2 here. */
static unsigned char N2_clk_mult[16] = { 2, 2, 2, 3, 4, 5, 6, 0 };

/* Crystal clock */
#define BASE_CLK	13000000

#else

/* Crystal Frequency to Memory Frequency Multiplier (L) */
static unsigned char L_clk_mult[32] = { 0, 27, 32, 36, 40, 45, 0, };

/* Memory Frequency to Run Mode Frequency Multiplier (M) */
static unsigned char M_clk_mult[4] = { 0, 1, 2, 4 };

/* Run Mode Frequency to Turbo Mode Frequency Multiplier (N) */
/* Note: we store the value N * 2 here. */
static unsigned char N2_clk_mult[8] = { 0, 0, 2, 3, 4, 0, 6, 0 };

/* Crystal clock */
#define BASE_CLK	3686400

#endif

/*
 * Get the clock frequency as reflected by CCCR and the turbo flag.
 * We assume these values have been applied via a fcs.
 * If info is not 0 we also display the current settings.
 */
unsigned int get_clk_frequency_khz( int info)
{
#if defined(CONFIG_CPU_PXA27X)
	unsigned long cccr, clkcfg;
	unsigned int l, L, n2, N, b, B, m, M, k, K;

	cccr = CCCR;
	asm( "mrc\tp14, 0, %0, c6, c0, 0" : "=r" (clkcfg) );

	l  =  (cccr >> 0) & 0x1f;
	n2 = N2_clk_mult[(cccr >> 7) & 0x0f];
	b  = (clkcfg & 8) ? 1 : 2;
	k = (l <=  7) ? 1 : (l <= 16) ? 2 : 4;
	if ((cccr & 0x02000000) == 0)
		m = (l <= 10) ? 1 : (l <= 20) ? 2 : 4;
	else
		m  = (clkcfg & 8) ? 1 : 2;

	L = l * BASE_CLK;
	N = n2 * L / 2;
	B = L / b;
	M = L / m;
	K = L / k;

	if( info)
	{
		N += 5000;
		printk( KERN_INFO "Turbo Mode clock : %d.%02dMHz (*%d.%d, %sactive)\n",
			N / 1000000, (N % 1000000) / 10000,
			n2 / 2, (n2 % 2) * 5, (clkcfg & 1) ? "" : "in" );
		L += 5000;
		printk( KERN_INFO "Run Mode clock   : %d.%02dMHz\n",
			L / 1000000, (L % 1000000) / 10000 );
		B += 5000;
		printk( KERN_INFO "System bus clock : %d.%02dMHz (1/%d)\n",
			B / 1000000, (B % 1000000) / 10000, b );
		M += 5000;
		printk( KERN_INFO "Memory clock     : %d.%02dMHz (1/%d)\n",
			M / 1000000, (M % 1000000) / 10000, m );
		K += 5000;
		printk( KERN_INFO "LCD clock        : %d.%02dMHz (1/%d)\n",
			K / 1000000, (K % 1000000) / 10000, k );
		N -= 5000;
		L -= 5000;
	}

	return (clkcfg & 1) ? (N/1000) : (L/1000);
#else
	unsigned long cccr, turbo;
	unsigned int l, L, m, M, n2, N;

	cccr = CCCR;
	asm( "mrc\tp14, 0, %0, c6, c0, 0" : "=r" (turbo) );

	l  =  L_clk_mult[(cccr >> 0) & 0x1f];
	m  =  M_clk_mult[(cccr >> 5) & 0x03];
	n2 = N2_clk_mult[(cccr >> 7) & 0x07];

	L = l * BASE_CLK;
	M = m * L;
	N = n2 * M / 2;

	if( info)
	{
		L += 5000;
		printk( KERN_INFO "Memory clock: %d.%02dMHz (*%d)\n",
			L / 1000000, (L % 1000000) / 10000, l );
		M += 5000;
		printk( KERN_INFO "Run Mode clock: %d.%02dMHz (*%d)\n",
			M / 1000000, (M % 1000000) / 10000, m );
		N += 5000;
		printk( KERN_INFO "Turbo Mode clock: %d.%02dMHz (*%d.%d, %sactive)\n",
			N / 1000000, (N % 1000000) / 10000, n2 / 2, (n2 % 2) * 5,
			(turbo & 1) ? "" : "in" );
	}

	return (turbo & 1) ? (N/1000) : (M/1000);
#endif
}

EXPORT_SYMBOL(get_clk_frequency_khz);

/*
 * Return the current lclk requency in units of 10kHz
 */
unsigned int get_lclk_frequency_10khz(void)
{
#if defined(CONFIG_CPU_PXA27X)
	return ((CCCR >> 0) & 0x1f) * BASE_CLK / 10000;
#else
	return L_clk_mult[(CCCR >> 0) & 0x1f] * BASE_CLK / 10000;
#endif
}

EXPORT_SYMBOL(get_lclk_frequency_10khz);

/*
 * Handy function to set GPIO alternate functions
 */

void set_GPIO_mode(int gpio_mode)
{
	long flags;
	int gpio = gpio_mode & GPIO_MD_MASK_NR;
	int fn = (gpio_mode & GPIO_MD_MASK_FN) >> 8;
	int gafr;

	local_irq_save(flags);
	if (gpio_mode & GPIO_MD_MASK_DIR)
		GPDR(gpio) |= GPIO_bit(gpio);
	else
		GPDR(gpio) &= ~GPIO_bit(gpio);
	gafr = GAFR(gpio) & ~(0x3 << (((gpio) & 0xf)*2));
	GAFR(gpio) = gafr |  (fn  << (((gpio) & 0xf)*2));
	local_irq_restore(flags);
}

EXPORT_SYMBOL(set_GPIO_mode);

/* 
 * Note that 0xfffe0000-0xffffffff is reserved for the vector table and
 * cache flush area.
 */
static struct map_desc standard_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
#ifndef CONFIG_ARCH_SABINAL
  { 0xf6000000, 0x20000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* PCMCIA0 IO */
  { 0xf7000000, 0x30000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* PCMCIA1 IO */
#endif
  { 0xf8000000, 0x40000000, 0x01800000, DOMAIN_IO, 0, 1, 0, 0 }, /* Devs */
  { 0xfa000000, 0x44000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* LCD */
  { 0xfc000000, 0x48000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* Mem Ctl */
#if defined(CONFIG_CPU_PXA27X)
  { 0xfe000000, 0x4c000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* USB Host */
  { 0xfe100000, 0x5c000000, 0x00040000, DOMAIN_IO, 0, 1, 0, 0 }, /* SRAM */
#endif
  { 0xff000000, 0x00000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* UNCACHED_PHYS_0 */
  LAST_DESC
};

void __init pxa_map_io(void)
{
	iotable_init(standard_io_desc);
	get_clk_frequency_khz( 1);
}
