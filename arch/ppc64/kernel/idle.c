/*
 * 
 *
 * Idle daemon for PowerPC.  Idle daemon will handle any action
 * that needs to be taken when the system becomes idle.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/cache.h>

#include <asm/time.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/HvCall.h>
#include <asm/iSeries/ItLpQueue.h>

unsigned long maxYieldTime = 0;
unsigned long minYieldTime = 0xffffffffffffffffUL;

static void yield_shared_processor(void)
{
	struct Paca *paca;
	unsigned long tb;
	unsigned long yieldTime;

	/* Turn off the run light */
	unsigned long CTRL;
	CTRL = mfspr(CTRLF);
	CTRL &= ~RUNLATCH;
	mtspr(CTRLT, CTRL);

	HMT_low();

	paca = (struct Paca *)mfspr(SPRG3);
	HvCall_setEnabledInterrupts( HvCall_MaskIPI |
				     HvCall_MaskLpEvent |
				     HvCall_MaskLpProd |
				     HvCall_MaskTimeout );

	__cli();
	__sti();
	tb = get_tb();
	/* Compute future tb value when yield should expire */
	HvCall_yieldProcessor( HvCall_YieldTimed, tb+tb_ticks_per_jiffy );

	yieldTime = get_tb() - tb;
	if ( yieldTime > maxYieldTime )
		maxYieldTime = yieldTime;

	if ( yieldTime < minYieldTime )
		minYieldTime = yieldTime;

	/*
	 * disable/enable will force any pending interrupts
	 * to be seen.
	 */
	__cli();
	/* 
	 * The decrementer stops during the yield.  Just force 
	 * a fake decrementer now and the timer interrupt code
	 * will straighten it all out.  We have to do this
	 * while disabled so we don't do it between where it is
	 * checked and where it is reset.
	 */
		
	paca->xLpPaca.xIntDword.xFields.xDecrInt = 1;
	__sti();
}

int idled(void)
{
	struct Paca *paca;
	long oldval;

	/* endless loop with no priority at all */
	current->nice = 20;
	current->counter = -100;
	init_idle();	

	paca = (struct Paca *)mfspr(SPRG3);

	for (;;) {
	
		if ( paca->xLpPaca.xSharedProc ) {
			if ( !current->need_resched )
				yield_shared_processor();
		}
		else {
			/* Avoid an IPI by setting need_resched */
			oldval = xchg(&current->need_resched, -1);
			if (!oldval) {
				while(current->need_resched == -1) {
#ifdef CONFIG_PPC_ISERIES
					HMT_medium();
					if ( ItLpQueue_isLpIntPending( paca->lpQueuePtr ) )
						process_iSeries_events();
					HMT_low();
#endif
				}
			}
		}
		if (current->need_resched) {
			HMT_medium();
			schedule();
			check_pgt_cache();
		}
	}
	return 0;
}

/*
 * SMP entry into the idle task - calls the same thing as the
 * non-smp versions. -- Cort
 */
int cpu_idle(void)
{
	idled();
	return 0; 
}


