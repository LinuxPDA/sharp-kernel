/*
 * linux/include/asm-arm/arch-l7200/timex.h
 *
 * Copyright (C) 2000 Rob Scott (rscott@mtrob.fdns.net)
 *                    Steve Hill (sjhill@cotw.com)
 *
 * 04-21-2000  RS     Created file
 * 05-03-2000 SJH     Tick rate was wrong
 * 03-28-2001 Lineo Japan, Inc.    Fix
 */
#ifndef __ASM_ARCH_TIMEX_H
#define __ASM_ARCH_TIMEX_H

#if defined(CONFIG_L7205SDB) || defined(CONFIG_IRIS)
#define CLOCK_TICK_RATE		3686400U	/* 3.6MHz */
#else
/*
 * On the ARM720T, clock ticks are set to 128 Hz.
 *
 * NOTE: The actual RTC value is set in 'time.h' which
 *       must be changed when choosing a different tick
 *       rate. The value of HZ in 'param.h' must also
 *       be changed to match below.
 */
#define CLOCK_TICK_RATE		128
#endif

#endif
