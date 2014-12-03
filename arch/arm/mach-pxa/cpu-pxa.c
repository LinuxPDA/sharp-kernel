/*
 *  linux/arch/arm/mach-pxa/cpu-pxa.c
 *
 *  Copyright (C) 2002,2003 Intrinsyc Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * History:
 *   31-Jul-2002 : Initial version [FB]
 *   29-Jan-2003 : added PXA255 support [FB]
 * 
 * Note:
 *
 * Quote from erratum 134: 
 *   "If the operation of these peripherals would be adversely affected,
 *   then these peripherals would have to be disabled during a frequency
 *   change. (MMC,FFUART,STUART,BTUART,IRDA,SSP,UDC,AC97)"
 *
 * This sounds like they are not sure what the bug is...
 * If you run into problems with any of these peripherals, the effected
 * driver should register with cpu freq notification and disable/enable 
 * the peripheral on CPUFREQ_PRECHANGE and CPUFREQ_POSTCHANGE.
 *  
 * So far I've tested this code only under light load. It works for me.
 *
 * TODO: 
 *   - determine min/max freq at runtime
 *   - determine pxbus value at runtime
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpufreq.h>

#include <asm/hardware.h>

#define DEBUGGING	1

#if DEBUGGING
static unsigned int freq_debug = DEBUGGING;
#else
#define freq_debug	0
#endif  

typedef struct
{
	unsigned int khz;
	unsigned int cccr;
	unsigned int pxbus;
} pxa_freqs_t;

#define CCLKCFG_TURBO		0x1
#define CCLKCFG_FCS		0x2

#define PXA250_REV_A1		0x1
#define PXA250_REV_B2		0x4
#define PXA25x_MIN_FREQ		99000

//#define PXA25x_ALLOW_OVERCLOCK

#ifdef PXA25x_ALLOW_OVERCLOCK
#warning *** Overclocking enabled - this may fry your hardware - you have been warned ***
#define OC(x...)	x
#define PXA25x_MAX_FREQ		471000
#else
#define OC(x...)
#define PXA25x_MAX_FREQ		400000
#endif

/* If CONFIG_CPU_FREQ is turned on but we find (at runtime) 
 * we can't support scaling, try to handle requests gracefully.
 */
static int supported;

static pxa_freqs_t pxa250_valid_freqs[] =
{
	{199100, 0x141, 99}, /* mem= 99, run=199, turbo=199, PXbus= 99 */
	{298600, 0x1c1, 99}, /* mem= 99, run=199, turbo=298, PXbus= 99 */
	{398100, 0x241, 99}, /* mem= 99, run=199, turbo=398, PXbus= 99 */
	{0,0}
};

static pxa_freqs_t pxa255_valid_freqs[] =
{
	{ 99000, 0x121, 50}, /* mem= 99, run= 99, turbo= 99, PXbus= 50 */
OC(	{118000, 0x122, 59},)/* mem=118, run=118, turbo=118, PXbus= 59 OC'd mem */
	{199100, 0x141, 99}, /* mem= 99, run=199, turbo=199, PXbus= 99 */
OC(	{236000, 0x142,118},)/* mem=118, run=236, turbo=236, PXbus=118 OC'd mem */
	{298600, 0x1c1, 99}, /* mem= 99, run=199, turbo=298, PXbus= 99 */
OC(	{354000, 0x1c2,118},)/* mem=118, run=236, turbo=354, PXbus=118 OC'd mem */
	{398099, 0x241, 99}, /* mem= 99, run=199, turbo=398, PXbus= 99 */
	{398100, 0x161,196}, /* mem= 99, run=398, turbo=398, PXbus=196 */
OC(	{471000, 0x162,236},)/* mem=118, run=471, turbo=471, PXbus=236 OC'd mem/core/bus */
	{0,0}
};

static pxa_freqs_t *pxa_valid_freqs;

/* This should be called with a valid freq point that was
 * obtained via pxa_validate_speed
 */
static pxa_freqs_t * pxa_get_freq_info( unsigned int khz)
{
	int i=0;
	while( pxa_valid_freqs[i].khz)
	{
		if( pxa_valid_freqs[i].khz == khz) 
			return &pxa_valid_freqs[i]; 
		i++;
	}

	/* shouldn't get here */
	return 0;
}

/* find a valid frequency point */
static unsigned int pxa_validate_speed(unsigned int khz)
{
	int i=0;
	unsigned int vfreq = 0;
	while( pxa_valid_freqs[i].khz && (khz >= pxa_valid_freqs[i].khz))
	{
		vfreq = pxa_valid_freqs[i].khz;
		i++;
	}
	return vfreq;
}

/* This should be called with a valid freq point that was
 * obtained via pxa_validate_speed
 */
static void pxa_setspeed(unsigned int khz)
{
	unsigned long flags;
	unsigned int unused;
	void *ramstart = phys_to_virt(0xa0000000);
	pxa_freqs_t *freq_info;

	if( ! supported) return;

	freq_info = pxa_get_freq_info( khz);

	if( ! freq_info) return;

	CCCR = freq_info->cccr;
	if( freq_debug)
		printk(KERN_INFO "Changing CPU frequency to %d Mhz (PXbus=%dMhz).\n", 
			khz/1000, freq_info->pxbus);

	local_irq_save(flags);
	__asm__ __volatile__("
		ldr	r4, [%1]			@load MDREFR
		b	2f
		.align  5
1:
		mcr	p14, 0, %2, c6, c0, 0		@ set CCLKCFG[FCS]

		@ restart sdcke 0 / 1
		bic	r5,  r4,  #(0x00001000 | 0x00008000)	@ MDREFR_E0PIN | MDREFR_E1PIN
		str	r5,  [%1]			@clear
		str	r4,  [%1]			@restore

		@ Generate refresh cycles for all banks
		ldr	r4, [%3]
		str	r4, [%3]
		str	r4, [%3]
		str	r4, [%3]
		str	r4, [%3]
		str	r4, [%3]
		str	r4, [%3]
		str	r4, [%3]
		str	r4, [%3]

		b	3f
2:		b	1b
3:		nop
		"
		: "=&r" (unused)
		: "r" (&MDREFR), "r" (CCLKCFG_TURBO|CCLKCFG_FCS), "r" (ramstart)
		: "r4", "r5");
	local_irq_restore(flags);
}

static int pxa_init_freqs( void)
{
	int cpu_ver; 
	asm("mrc%? p15, 0, %0, c0, c0" : "=r" (cpu_ver));

	if( (cpu_ver & 0xf) <= PXA250_REV_A1)
	{
		return 0;
	}

	if( (cpu_ver & 0xf) <= PXA250_REV_B2)
	{
		if( freq_debug) printk(KERN_INFO "Using PXA250 frequency points.\n");
		pxa_valid_freqs = pxa250_valid_freqs;
	}	
	else /* C0 and above */
	{
		if( freq_debug) printk(KERN_INFO "Using PXA255 frequency points.\n");
		pxa_valid_freqs = pxa255_valid_freqs;
	}
	
	return 1;
}

static int __init pxa_clk_init(void)
{
	if( pxa_init_freqs())
	{
		if( freq_debug) printk(KERN_INFO "Registering CPU frequency change support.\n");
		supported = 1;

		cpufreq_init( get_clk_frequency_khz(0), PXA25x_MIN_FREQ, PXA25x_MAX_FREQ);
		cpufreq_setfunctions(pxa_validate_speed, pxa_setspeed);
	}
	else
	{
		if( freq_debug) printk(KERN_INFO "Disabling CPU frequency change support.\n");
		/* Note that we have to initialize the generic code in order to 
		 * release a lock (cpufreq_sem). Any registration for freq changes
		 * (e.g. lcd driver) will get blocked otherwise.
		 */
		cpufreq_init( 0, 0, 0);
		cpufreq_setfunctions(pxa_validate_speed, pxa_setspeed);
	}

	return 0;
}

module_init(pxa_clk_init);

MODULE_AUTHOR ("Intrinsyc Software Inc.");
MODULE_LICENSE("GPL");
