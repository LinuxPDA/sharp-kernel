/*
 * linux/include/asm-arm/arch-cotulla/irq.h
 * 
 * Copyright (C) 2001 LINEO Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 * linux/include/asm-arm/arch-sa1100/irq.h
 * 
 * Author: Nicolas Pitre
 */

#define fixup_irq(irq)  (irq)


/*
 * This prototype is required for cascading of multiplexed interrupts.
 * Since it doesn't exist elsewhere, we'll put it here for now.
 */
extern void do_IRQ(int irq, struct pt_regs *regs);
