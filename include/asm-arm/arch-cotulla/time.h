/*
 * linux/include/asm-arm/arch-cotulla/time.h
 *
 * Copyright (C) 2001 LINEO Japan, inc.
 * 
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Refered : linux/include/asm-arm/arch-sa1100/time.h
 *
 * Change Log
 * 2002/06/07  SHARP Corporation    modify for SL-A300
 */


#define RTC_DEF_DIVIDER		32768 - 1
//#define RTC_DEF_TRIM            0
#define RTC_DEF_TRIM            0x11b3

static unsigned long __init cotulla_get_rtc_time(void)
{
	/*
	 * According to the manual we should be able to let RTTR be zero
	 * and then a default diviser for a 32.768KHz clock is used.
	 * Apparently this doesn't work, at least for my SA1110 rev 5.
	 * If the clock divider is uninitialized then reset it to the
	 * default value to get the 1Hz clock.
	 */
	if (RTTR == 0) {
		RTTR = RTC_DEF_DIVIDER + (RTC_DEF_TRIM << 16);
		printk(KERN_WARNING "Warning: uninitialized Real Time Clock\n");
		/* The current RTC value probably doesn't make sense either */
		RCNR = 0;
		return 0;
	}	
	return RCNR;
}

static int cotulla_set_rtc(void)
{
	unsigned long current_time = xtime.tv_sec;

	if (RTSR & RTSR_ALE) {
		/* make sure not to forward the clock over an alarm */
		unsigned long alarm = RTAR;
		if (current_time >= alarm && alarm >= RCNR)
			return -ERESTARTSYS;
	}
	RCNR = current_time;
	return 0;
}

/* IRQs are disabled before entering here from do_gettimeofday() */
static unsigned long cotulla_gettimeoffset (void)
{
	unsigned long ticks_to_match, elapsed, usec;

	/* Get ticks before next timer match */
	ticks_to_match = OSMR0 - OSCR;

	/* We need elapsed ticks since last match */
	elapsed = LATCH - ticks_to_match;

	/* Now convert them to usec */
	usec = (unsigned long)(elapsed*tick)/LATCH;

	return usec;
}

static void cotulla_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	long flags;
	int next_match;

	/* Loop until we get ahead of the free running timer.
	 * This ensures an exact clock tick count and time acuracy.
	 * IRQs are disabled inside the loop to ensure coherence between
	 * lost_ticks (updated in do_timer()) and the match reg value, so we
	 * can use do_gettimeofday() from interrupt handlers.
	 */
	do {
		do_set_rtc();
		save_flags_cli( flags );
		do_timer(regs);
		OSSR = OSSR_M0;  /* Clear match on timer 0 */
		next_match = (OSMR0 += LATCH);
		restore_flags( flags );
	} while( (signed long)(next_match - OSCR) <= 0 );
}

static inline void setup_timer(void)
{
	gettimeoffset = cotulla_gettimeoffset;
	set_rtc = cotulla_set_rtc;
#ifdef CONFIG_SABINAL_DISCOVERY
	if ( RCSR == 0x1 ) {
	  RTTR = RTC_DEF_DIVIDER + (RTC_DEF_TRIM << 16);
	  RCNR = ( mktime(2003,1,1,0,0,0) - ( 9*60*60 ) );
	  xtime.tv_sec = ( mktime(2003,1,1,0,0,0) - ( 9*60*60 ) );
	} else {
	  xtime.tv_sec = cotulla_get_rtc_time();
	}
#else
	xtime.tv_sec = cotulla_get_rtc_time();
#endif
	timer_irq.handler = cotulla_timer_interrupt;
	OSMR0 = 0;		/* set initial match at 0 */
	OSSR = 0xf;		/* clear status on all timers */
	setup_arm_irq(IRQ_OST0, &timer_irq);
	OIER |= OIER_E0;	/* enable match on timer 0 to cause interrupts */
	OSCR = 0;		/* initialize free-running timer, force first match */
}
