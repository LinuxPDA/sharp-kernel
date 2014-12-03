/*
 * linux/include/asm/arch-cotulla/hardware.h
 * 
 * Copyright (c) 2001 Lineo, Inc. 
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 * linux/include/asm-arm/arch-iop80310/hardware.h
 *
 * Change Log
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <linux/config.h>

// added by CYB start
/*
 * Those are statically mapped PCMCIA IO space for designs using it as a
 * generic IO bus, typically with ISA parts, hardwired IDE interfaces, etc.
 * The actual PCMCIA code is mapping required IO region at run time.
 */
#define PCMCIA_IO_0_BASE	0xf6000000
#define PCMCIA_IO_1_BASE	0xf7000000


/*
 * We requires absolute addresses i.e. (PCMCIA_IO_0_BASE + 0x3f8) for 
 * in*()/out*() macros to be usable for all cases.
 */
#define PCIO_BASE		0
// added by CYB end

// Generic chipset bits
#include "cotulla.h"

#define GPIO_FALLING_EDGE       1
#define GPIO_RISING_EDGE        2
#define GPIO_BOTH_EDGES         3

#endif  /* _ASM_ARCH_HARDWARE_H */
