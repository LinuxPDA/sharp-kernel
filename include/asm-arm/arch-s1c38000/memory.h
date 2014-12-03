/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/memory.h
 *
 */

#ifndef __ASM_ARM_ARCH_S1C38000_MEMORY_H
#define __ASM_ARM_ARCH_S1C38000_MEMORY_H 1

#include <linux/config.h>

#define TASK_SIZE              (0xc0000000UL)
#define TASK_SIZE_26           (0x04000000UL)
#define PAGE_OFFSET            (0xc0000000UL)
#define PHYS_OFFSET            (0x10000000UL)

#define TASK_UNMAPPED_BASE     (TASK_SIZE / 3)

#define __virt_to_phys__is_a_macro
#define __phys_to_virt__is_a_macro
#define __virt_to_bus__is_a_macro
#define __bus_to_virt__is_a_macro
#define __virt_to_phys(x)      ((x) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt(x)      ((x) + PAGE_OFFSET - PHYS_OFFSET)
#define __virt_to_bus(x)       __virt_to_phys(x)
#define __bus_to_virt(x)       __phys_to_virt(x)

#ifdef CONFIG_DISCONTIGMEM
#define NR_NODES               4
#define KVADDR_TO_NID(addr)    (((unsigned long)(addr) - PAGE_OFFSET) >> 27)
#define PFN_TO_NID(pfn)        (((pfn) - PHYS_PFN_OFFSET) >> (27 - PAGE_SHIFT))
#define ADDR_TO_MAPBASE(kaddr) NODE_MEM_MAP(KVADDR_TO_NID(kaddr))
#define PFN_TO_MAPBASE(pfn)    NODE_MEM_MAP(PFN_TO_NID(pfn))
#define LOCAL_MAP_NR(addr)     (((unsigned long)(addr) & 0x07ffffff) >> \
                                 PAGE_SHIFT)
#else
#define PFN_TO_NID(addr)       (0)
#endif

#endif
