/*
 * linux/arch/arm/mach-pxa/rthal.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * I/O routine and setup routines for XScale.
 *
 * Based on:
 *	rtai/arch/arm/mach-sa1100/rtai_timer.c
 */
/* 020222 rtai/arch/arm/mach-sa1100/rtai_timer.c
COPYRIGHT (C) 2002 Guennadi Liakhovetski, DSA GmbH (gl@dsa-ac.de)
COPYRIGHT (C) 2002 Wolfgang Müller (wolfgang.mueller@dsa-ac.de)
Copyright (c) 2001 Alex Züpke, SYSGO RTS GmbH (azu@sysgo.de)

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
--------------------------------------------------------------------------
Acknowledgements
- Paolo Mantegazza	(mantegazza@aero.polimi.it)
	creator of RTAI 
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/rt_sched.h>

#include <asm/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/atomic.h>

#define	RTHAL_MACH_VERSION	"-1"

static union rt_tsc {
	unsigned long long tsc;
	unsigned long hltsc[2];
} rt_tsc;

struct rt_calibration rt_calibration;
struct rt_times       rt_times;

volatile int delay_count_value;

static void  (*rthal_timer_handler)(void);
static void* save_rt_timer_handler = (void*)0;

static unsigned long timer_minimum_value;

extern void __init rthal_init_common(char *);

static void rthal_null_handler(int irq, void* dev_id, struct pt_regs *regs)
{
}

RTIME rt_get_time(void)
{
	unsigned long flags, count;

	hard_save_flags_and_cli(flags);

	count = OSCR;
	if (count < rt_tsc.hltsc[0]) {
		rt_tsc.hltsc[1]++;
	}
	rt_tsc.hltsc[0] = count;

	hard_restore_flags(flags);

	return rt_tsc.tsc;
}

void rthal_set_timer(int delay)
{
	unsigned long flags;

	if (delay == 0) {
		OSSR = OSSR_M1;
		return;
	}

	if (delay < timer_minimum_value) {
		delay = timer_minimum_value;
	}

	hard_save_flags_and_cli(flags);

	OIER &= ~OIER_E1;
	OSSR = OSSR_M1;
	delay_count_value = delay;
	OSMR1 = delay + OSCR;
	OIER |= OIER_E1;

	hard_restore_flags(flags);

}

static void rt_timer_handler(void)
{
	unsigned long flags;
	int next_match;

	rt_get_time();

	hard_save_flags_and_cli(flags);

	do {
		OSSR = OSSR_M1;
		next_match = (OSMR1 += delay_count_value);
	} while ((signed long)(next_match - OSCR) <= 0);

	hard_restore_flags(flags);

	rthal_timer_handler();
}

void rthal_request_rt_timer(void (*handler)(void))
{
	unsigned long flags;
	RTIME t;

	flags = hard_lock_all();

	save_rt_timer_handler = (void*)IRQ_DESC[RT_TIMER_IRQ].action->handler;
	IRQ_DESC[RT_TIMER_IRQ].action->handler = rthal_null_handler;

	rt_free_irq(RT_TIMER_IRQ);
	rthal_timer_handler = handler;
	rt_request_irq(RT_TIMER_IRQ, rt_timer_handler);

	rt_tsc.tsc = 0;
	delay_count_value = LATCH;

	rt_times.base_tick = LATCH;
	rt_times.periodic_tick = rt_times.base_tick;
	rt_times.tick_time = t = rt_get_time();
	rt_times.intr_time = t + (RTIME)rt_times.periodic_tick;
	rt_times.base_time = t + (RTIME)rt_times.base_tick;

	rthal_set_timer(rt_times.periodic_tick);

	hard_unlock_all(flags);
}

void rt_free_timer(void)
{
	unsigned long flags;

	flags = hard_lock_all();

	OIER &= ~OIER_E1;
	OSSR = OSSR_M1;
	OSMR1 = 0;

	rt_free_irq(RT_TIMER_IRQ);

	IRQ_DESC[RT_TIMER_IRQ].action->handler = save_rt_timer_handler;

	hard_unlock_all(flags);
}

#if defined(CONFIG_RTHAL_CALIBRATION)

unsigned long latency_match_reg, setup_time_match_reg;

#define PERIOD 50000
static RTIME next, trap_total, test_period;
static void trap(void); 
static void calibrate_latency(void);
static int cal_end = 0;
#endif

void __init rthal_init(void)
{
	rt_tsc.hltsc[0] = 0;
	rt_tsc.hltsc[1] = 0;

	memset(&rt_calibration, 0, sizeof(rt_calibration));
	memset(&rt_times, 0, sizeof(rt_times));

	rt_calibration.cpu_freq = CLOCK_TICK_RATE;

	rthal_init_common(RTHAL_MACH_VERSION);

#if defined(CONFIG_RTSCHED)

#if defined(CONFIG_RTHAL_CALIBRATION)
	calibrate_latency();
#endif
#if 0
	printk("TIMER SETUP TIME        = %5d nsec\n", SETUP_TIME_MATCH_REG);
#endif
	printk("TIMER INTERRUPT LATENCY = %5d nsec\n", LATENCY_MATCH_REG);

	timer_minimum_value = (unsigned long)rt_nano2count(LATENCY_MATCH_REG);

#endif
}


#if defined(CONFIG_RTHAL_CALIBRATION)
static void calibrate_latency(void)
{
	latency_match_reg = 3000;
	setup_time_match_reg = 7000;
	latency_match_reg += setup_time_match_reg;
}

static void trap(void)
{
}
#endif
