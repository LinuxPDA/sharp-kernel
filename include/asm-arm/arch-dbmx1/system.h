/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/include/asm-arm/arch-db1mx1/system.h
 *
 */
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <asm/arch/platform.h>

static void arch_idle(void)
{
    cpu_do_idle();
}

static inline void arch_reset(char mode)
{
    if (mode == 's') {
	cpu_reset(0);
    } else {
    }
}

#endif
