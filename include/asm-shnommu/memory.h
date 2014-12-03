/*
 *  linux/include/asm-arm/memory.h
 *
 *  Copyright (C) 2000 Russell King
 *  Copyright (C) 2001 RidgeRun, Inc (http://www.ridgerun.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Note: this file should not be included by non-asm/.h files
 *
 *  Modifications:
 *  02/20/01  Gordon McNutt  Leveraged for uClinux-2.4.x
 */
#ifndef __ASM_SH_MEMORY_H
#define __ASM_SH_MEMORY_H

extern int _end_kernel;
#define PHYS_OFFSET  ((unsigned long) &_end_kernel)
#define PAGE_OFFSET  PHYS_OFFSET

/*
 * Virtual, bus and physical addresses are all the same when there's no MMU.
 * --gmcnutt
 */
#define virt_to_bus(x) x
#define bus_to_virt(x) x
#define virt_to_phys(x) x
#define phys_to_virt(x) x

/*
 * For some reason other asm/.h files refer to these instead of the more 
 * public macros above.
 * --gmcnutt
 */
#define __virt_to_bus(x) x
#define __bus_to_virt(x) x
#define __virt_to_phys(x) x
#define __phys_to_virt(x) x


/*
 * arch/armnommu/kernel/armksyms.c needs these.
 * --gmcnutt
 */
#define __virt_to_phys__is_a_macro
#define __phys_to_virt__is_a_macro
#define __virt_to_bus__is_a_macro
#define __bus_to_virt__is_a_macro

#endif
