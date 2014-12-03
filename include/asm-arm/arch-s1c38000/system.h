/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/system.h
 *
 */

#ifndef __ASM_ARM_ARCH_S1C38000_SYSTEM_H
#define __ASM_ARM_ARCH_S1C38000_SYSTEM_H 1

#include <linux/config.h>

static inline void arch_idle(void)
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
