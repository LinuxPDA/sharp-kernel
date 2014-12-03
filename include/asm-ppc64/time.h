/*
 * 
 * Common time prototypes and such for all ppc machines.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu) to merge
 * Paul Mackerras' version and mine for PReP and Pmac.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __PPC64_TIME_H
#define __PPC64_TIME_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/config.h>
#include <linux/mc146818rtc.h>

#include <asm/processor.h>
#include <asm/Paca.h>
#include <asm/iSeries/HvCall.h>

/* time.c */
extern unsigned long tb_ticks_per_jiffy;
extern unsigned long tb_ticks_per_usec;
extern unsigned long tb_last_stamp;

extern void to_tm(int tim, struct rtc_time * tm);
extern time_t last_rtc_update;

int via_calibrate_decr(void);

static __inline__ unsigned long get_tb(void)
{
	return mftb();
}

/* Accessor functions for the decrementer register. */
static __inline__ unsigned int get_dec(void)
{
	return (mfspr(SPRN_DEC));
}

static __inline__ void set_dec(int val)
{
    struct Paca * paca;
    int cur_dec;

    paca = (struct Paca *)mfspr(SPRG3);
    if ( paca->xLpPaca.xSharedProc ) {
	paca->xLpPaca.xVirtualDecr = val;
	cur_dec = get_dec();
	if ( cur_dec > val )
	    HvCall_setVirtualDecr();
    }
    else
	mtspr(SPRN_DEC, val);
}

extern __inline__ unsigned long tb_ticks_since(unsigned long tstamp) {
    return get_tb() - tstamp;
}
#endif /* __KERNEL__ */
#endif /* __PPC64_TIME_H */
