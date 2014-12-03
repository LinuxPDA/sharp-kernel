/*
 * include/asm-ppc/cache.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef __ARCH_PPC64_CACHE_H
#define __ARCH_PPC64_CACHE_H

#include <linux/config.h>
#include <asm/processor.h>

/* bytes per L1 cache line */
#define L1_CACHE_BYTES	128

#define	L1_CACHE_ALIGN(x)       (((x)+(L1_CACHE_BYTES-1))&~(L1_CACHE_BYTES-1))

#define	SMP_CACHE_BYTES L1_CACHE_BYTES

#ifdef MODULE
#define __cacheline_aligned __attribute__((__aligned__(L1_CACHE_BYTES)))
#else
#define __cacheline_aligned                                     \
  __attribute__((__aligned__(L1_CACHE_BYTES),                   \
                 __section__(".data.cacheline_aligned")))
#endif

#endif
