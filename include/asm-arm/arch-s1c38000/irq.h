/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/irq.h
 *
 */

#ifndef __ASM_ARM_ARCH_S1C38000_IRQ_H
#define __ASM_ARM_ARCH_S1C38000_IRQ_H 1

#include <asm/ptrace.h>

extern void do_IRQ(int, struct pt_regs*);

/*
 * IRQ base register
 */
#define	IRQ_BASE	(IO_BASE + 0x200)

/* 
 * Normal IRQ registers
 */
#define IRQ_STATUS	(*(volatile unsigned long *) (IRQ_BASE + 0x000))
#define IRQ_RAWSTATUS	(*(volatile unsigned long *) (IRQ_BASE + 0x004))
#define IRQ_ENABLE	(*(volatile unsigned long *) (IRQ_BASE + 0x008))
#define IRQ_ENABLECLEAR	(*(volatile unsigned long *) (IRQ_BASE + 0x00c))
#define IRQ_SOFT	(*(volatile unsigned long *) (IRQ_BASE + 0x010))
#define IRQ_LEVEL	(*(volatile unsigned long *) (IRQ_BASE + 0x080))
#define IRQ_EDGE	(*(volatile unsigned long *) (IRQ_BASE + 0x084))
#define IRQ_TRGRESET	(*(volatile unsigned long *) (IRQ_BASE + 0x088))

/* 
 * Fast IRQ registers
 */
#define FIQ_STATUS	(*(volatile unsigned long *) (IRQ_BASE + 0x100))
#define FIQ_RAWSTATUS	(*(volatile unsigned long *) (IRQ_BASE + 0x104))
#define FIQ_ENABLE	(*(volatile unsigned long *) (IRQ_BASE + 0x108))
#define FIQ_ENABLECLEAR	(*(volatile unsigned long *) (IRQ_BASE + 0x10c))
#define FIQ_LEVEL	(*(volatile unsigned long *) (IRQ_BASE + 0x180))
#define FIQ_EDGE	(*(volatile unsigned long *) (IRQ_BASE + 0x184))
#define FIQ_TRGRESET	(*(volatile unsigned long *) (IRQ_BASE + 0x188))

#define fixup_irq(x) (x)

#endif
