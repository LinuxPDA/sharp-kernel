/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/include/asm-arm/arch-db1mx1/irq.h
 *
 */
#ifndef __ASM_ARM_ARCH_DBMX1_IRQ_H
#define __ASM_ARM_ARCH_DBMX1_IRQ_H 1

extern void do_IRQ(int, struct pt_regs*);

#define fixup_irq(x)	(x)

#endif
