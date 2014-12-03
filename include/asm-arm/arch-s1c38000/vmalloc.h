/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/vmalloc.h
 */

#ifndef __ASM_ARM_ARCH_S1C38000_VMALLOC_H
#define __ASM_ARM_ARCH_S1C38000_VMALLOC_H 1

#include <linux/config.h>

#define VMALLOC_OFFSET	  (8*1024*1024)
#define VMALLOC_START	  (((unsigned long)high_memory + \
                           VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#define VMALLOC_END       (0xe8000000)

#endif
