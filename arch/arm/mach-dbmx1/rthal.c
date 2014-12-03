/*
 * linux/arch/arm/mach-dbmx1/rthal.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * I/O routine and setup routines for Motorola DragonBall MX1.
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

#define TIMER_VA_BASE	IO_ADDRESS(DBMX1_TIM1_BASE)
#define TIMER_TCTL1	(TIMER_VA_BASE + 0x00L)
#define TIMER_TCMP1	(TIMER_VA_BASE + 0x08L)
#define TIMER_TCN1	(TIMER_VA_BASE + 0x10L)
#define TIMER_TSTAT1	(TIMER_VA_BASE + 0x14L)
#define TIM_PERCLK1	0x02
#define TIM_INTEN	0x10
#define TIM_ENAB	0x01
#define TIM_FRR		0x100

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
	volatile unsigned long *tcn1 = (unsigned long *)TIMER_TCN1;
	unsigned long flags, count;

	hard_save_flags_and_cli(flags);

	count = *tcn1;
	if (count < rt_tsc.hltsc[0]) {
		rt_tsc.hltsc[1]++;
	}
	rt_tsc.hltsc[0] = count;

	hard_restore_flags(flags);

	return rt_tsc.tsc;
}

void rthal_set_timer(int delay)
{
	volatile unsigned long *tstat1 = (unsigned long *)TIMER_TSTAT1;
	volatile unsigned long *tcmp1 = (unsigned long *)TIMER_TCMP1;
	volatile unsigned long *tcn1 = (unsigned long *)TIMER_TCN1;
	unsigned long flags;

	if (delay == 0) {
		if (*tstat1) *tstat1 = 0x0;
		return;
	}

	if (delay < timer_minimum_value) {
		delay = timer_minimum_value;
	}

	hard_save_flags_and_cli(flags);

	delay_count_value = delay;
	*tcmp1 = *tcn1 + delay;

	hard_restore_flags(flags);

}

static void rt_timer_handler(void)
{
	volatile unsigned long *tstat1 = (unsigned long *)TIMER_TSTAT1;
	volatile unsigned long *tcmp1 = (unsigned long *)TIMER_TCMP1;
	volatile unsigned long *tcn1 = (unsigned long *)TIMER_TCN1;
	unsigned long flags;
	unsigned long next_match;

	rt_get_time();

	hard_save_flags_and_cli(flags);

	if (*tstat1) *tstat1 = 0x0;
	next_match = (*tcmp1 += delay_count_value);

	hard_restore_flags(flags);

	rthal_timer_handler();
}

void rthal_request_rt_timer(void (*handler)(void))
{
	volatile unsigned long *tctl1 = (unsigned long *)TIMER_TCTL1;
	volatile unsigned long *tcmp1 = (unsigned long *)TIMER_TCMP1;
	volatile unsigned long *tstat1 = (unsigned long *)TIMER_TSTAT1;
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

	*tctl1 = 0x0;
	*tstat1 = 0x0;
	*tcmp1 = rt_times.periodic_tick;
	*tctl1 = (TIM_FRR | TIM_PERCLK1 | TIM_INTEN | TIM_ENAB);

	hard_unlock_all(flags);
}

void rt_free_timer(void)
{
	volatile unsigned long *tctl1 = (unsigned long *)TIMER_TCTL1;
	volatile unsigned long *tcmp1 = (unsigned long *)TIMER_TCMP1;
	volatile unsigned long *tstat1 = (unsigned long *)TIMER_TSTAT1;
	unsigned long flags;

	flags = hard_lock_all();

	*tstat1 = 0x0;
	*tcmp1 = 0x0;
	*tctl1 = 0x0;

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
	RTIME count;
	unsigned long flags;
	int i;
	volatile unsigned long *tctl1 = (unsigned long *)TIMER_TCTL1;
	volatile unsigned long *tstat1 = (unsigned long *)TIMER_TSTAT1;
	volatile unsigned long *tcmp1 = (unsigned long *)TIMER_TCMP1;
	volatile unsigned long *tcn1 = (unsigned long *)TIMER_TCN1;

	/* calibrate SETUP TIMER LATENCY */
	*tctl1 = 0x0;
	*tstat1 = 0x0;
	*tcmp1 = LATCH;
	*tctl1 = (TIM_FRR | TIM_PERCLK1 | TIM_ENAB);

	count = rt_get_time();
	for (i = 0; i < 4; i++) {
		rthal_set_timer(LATCH);
	}
	setup_time_match_reg = (unsigned long)((rt_get_time() - count) / 4);
	setup_time_match_reg = (unsigned long)rt_count2nano(setup_time_match_reg);

	/* calibrate TIMER INTRRUPT LATENCY */
	// stop timer
	*tctl1 = 0x0;
	*tstat1 = 0x0;

	rt_request_irq(RT_TIMER_IRQ, trap);

	latency_match_reg = 0;
	test_period = rt_nano2count(PERIOD);
    	next = test_period;
	rt_tsc.tsc = 0;

	*tctl1 = 0x0;
	*tstat1 = 0x0;
	*tcmp1 = test_period;
	*tctl1 = (TIM_FRR | TIM_PERCLK1 | TIM_INTEN | TIM_ENAB);
    
	while (!cal_end) {
		rt_get_time();
	}
	latency_match_reg = (unsigned long)rt_count2nano(latency_match_reg) +
	    setup_time_match_reg;

	rt_free_irq(RT_TIMER_IRQ);
}

static void trap(void)
{
	static int count = 0;
	unsigned long flags;
	volatile unsigned long *tstat1 = (unsigned long *)TIMER_TSTAT1;
	volatile unsigned long *tcmp1 = (unsigned long *)TIMER_TCMP1;
	volatile unsigned long *tcn1 = (unsigned long *)TIMER_TCN1;

	trap_total += rt_get_time() - next;	

	if (*tstat1)
	    *tstat1 = 0x0;
	if (++count >= 4) {
		latency_match_reg = (unsigned long)(trap_total / 4);
		cal_end = 1;
		*tcmp1 = *tcn1 + LATCH;
	}
	else {
		hard_save_flags_and_cli(flags);
	    	next = rt_get_time() + test_period;
		*tcmp1 = next;
		hard_restore_flags(flags);
	}
}
#endif
