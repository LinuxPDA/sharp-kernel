#ifndef _PPC64_DELAY_H
#define _PPC64_DELAY_H

/*
 * Copyright 1996, Paul Mackerras.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * PPC64 Support added by Dave Engebretsen, Todd Inglett, Mike Corrigan,
 * Anton Blanchard.
 */

#include <asm/param.h>
#include <asm/processor.h>
#include <asm/time.h>

extern unsigned long tb_ticks_per_usec;

static inline void __delay(unsigned long loops)
{
	unsigned long start = get_tb();

	while((get_tb()-start) < loops)
		HMT_low();
}

static __inline__ void udelay(unsigned long usecs)
{
	unsigned long loops = tb_ticks_per_usec * usecs;
	__delay(loops);
	HMT_medium();
}

#endif /* _PPC64_DELAY_H */
