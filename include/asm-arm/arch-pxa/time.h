/*
 * linux/include/asm-arm/arch-pxa/time.h
 *
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ChangLog:
 *	12-Dec-2002 Lineo Japan, Inc.
 *	12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *	26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 */

#ifdef CONFIG_SABINAL_DISCOVERY 
#define RTC_DEF_DIVIDER		32768 - 1
//#define RTC_DEF_TRIM		0
#define RTC_DEF_TRIM		0x11b3
#endif

#ifdef CONFIG_ARCH_PXA_POODLE
#define RTC_DEF_DIVIDER		32768 - 1
#define RTC_DEF_TRIM		0
#elif CONFIG_ARCH_PXA_CORGI
#define RTC_DEF_DIVIDER		32768 - 1
#define RTC_DEF_TRIM		0
#elif CONFIG_ARCH_PXA_TOSA
#define RTC_DEF_DIVIDER		32768 - 1
#define RTC_DEF_TRIM		0
#endif

#ifdef CONFIG_ARCH_SHARP_SL
#define SHARP_SL_DEF_YEAR	2003
#endif

static inline unsigned long pxa_get_rtc_time(void)
{
#ifdef CONFIG_ARCH_SHARP_SL
	if (RTTR == 0) {
		RTTR = RTC_DEF_DIVIDER + (RTC_DEF_TRIM << 16);
		printk(KERN_WARNING "Warning: uninitialized Real Time Clock\n");
		/* The current RTC value probably doesn't make sense either */
		RCNR = 0;
		return 0;
        }
#endif
	return RCNR;
}

static int pxa_set_rtc(void)
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
static unsigned long pxa_gettimeoffset (void)
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

static void pxa_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
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
		do_leds();
		do_set_rtc();
		save_flags_cli( flags );
		do_timer(regs);
		OSSR = OSSR_M0;  /* Clear match on timer 0 */
		next_match = (OSMR0 += LATCH);
		restore_flags( flags );
	} while( (signed long)(next_match - OSCR) <= 0 );
}

extern inline void setup_timer (void)
{
	gettimeoffset = pxa_gettimeoffset;
	set_rtc = pxa_set_rtc;
	xtime.tv_sec = pxa_get_rtc_time();
	timer_irq.handler = pxa_timer_interrupt;
	OSMR0 = 0;		/* set initial match at 0 */
	OSSR = 0xf;		/* clear status on all timers */
	setup_arm_irq(IRQ_OST0, &timer_irq);
	OIER |= OIER_E0;	/* enable match on timer 0 to cause interrupts */
	OSCR = 0;		/* initialize free-running timer, force first match */

}

