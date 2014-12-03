/*
 * linux/arch/arm/kernel/xscale-time.c
 *
 * Some XScale based sytems don't provide an external timer, so this
 * code allows for use of the built in Performance Monitoring Unit(PMU)
 * clock counter to be used as the timer source.
 * 
 * Author:  Nicolas Pitre
 * Copyright:   (C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * This file implements a timer based on the XScale's internal
 * performance monitoring clock counter.  This code exists for
 * boards without external timers to be developed on or during 
 * early hardware bring up if the external timer does not
 * work.  To utilize it, CONFIG_XSCALE_PMU_TIMER should be
 * set by the configuration scripts.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>
#include <asm/hardware.h>

extern int setup_arm_irq(int, struct irqaction *);

/* IRQs are disabled before entering here from do_gettimeofday() */
static unsigned long xs80200pmu_gettimeoffset (void)
{
	unsigned long elapsed, usec;

	/* We need elapsed timer ticks since last interrupt */
	/* 
	 * Read the CCNT value.  The returned value is 
	 * between -LATCH and 0, 0 corresponding to a full jiffy 
	 */
	asm volatile ("mrc p14, 0, %0, c1, c0, 0" : "=r" (elapsed));
	elapsed += LATCH;

	/* Now convert them to usec */
	usec = (unsigned long)(elapsed*tick)/LATCH;

	return usec;
}

static void xs80200pmu_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	long flags, cnt;
	
	/* Clear and disable CCNT overflow interrupt in the PMNC */
	asm volatile ("mcr p14, 0, %0, c0, c0, 0" : : "r" (0x00000701));

	do {
		save_flags_cli(flags);
		do_timer(regs);
		/* get current count from CCNT */
		asm volatile ("mrc p14, 0, %0, c1, c0, 0" : "=r" (cnt));
		/* 
		 * Bring the count back for one timer tick.
		 * To be precise, we would need to consider the cycles 
		 * involved in this read-modify-write sequence as well.
		 */
		cnt -= LATCH;
		/* write back updated count into CCNT */
		asm volatile ("mcr p14, 0, %0, c1, c0, 0" : : "r" (cnt));
		restore_flags(flags);
		/* clear CCNT overflow in the PMNC */
		asm volatile ("mcr p14, 0, %0, c0, c0, 0" : : "r" (0x00000701));
	} while (cnt >= 0);
	
	/* Enable CCNT overflow interrupt in the PMNC */
	asm volatile ("mcr p14, 0, %0, c0, c0, 0" : : "r" (0x00000041));
}

extern unsigned long (*gettimeoffset)(void);

static struct irqaction timer_irq = {
	name: "timer",
};

void __init setup_timer (void)
{
	gettimeoffset = xs80200pmu_gettimeoffset;
	timer_irq.handler = xs80200pmu_timer_interrupt;
	setup_arm_irq(IRQ_XS80200_PMU, &timer_irq);

	printk("Using XScale PMU as timer source\n");

	/* set the initial CCNT value */
	asm volatile ("mcr p14, 0, %0, c1, c0, 0" : : "r" (-LATCH));
	
	/* enable interrupt to occur on CCNT wraps in the PMNC */
	asm volatile ("mcr p14, 0, %0, c0, c0, 0" : : "r" (0x00000741));
}

