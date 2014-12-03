/*
 * linux/include/asm-arm/arch-pxa/timex.h
 *
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Change Log
 *	14-Jul-2004 Lineo Solutions, Inc.  for PXA27x
 */

/*
 * PXA250/210 timer
 */
#if defined(CONFIG_CPU_PXA27X)
#define CLOCK_TICK_RATE		3250000
#else
#define CLOCK_TICK_RATE		3686400
#endif
#define CLOCK_TICK_FACTOR	80
