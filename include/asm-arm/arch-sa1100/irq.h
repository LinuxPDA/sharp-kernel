/*
 * linux/include/asm-arm/arch-sa1100/irq.h
 * 
 * Author: Nicolas Pitre
 */

#ifndef _ASM_ARCH_IRQ_H
#define _ASM_ARCH_IRQ_H

#define fixup_irq(x)	(x)

/*
 * This prototype is required for cascading of multiplexed interrupts.
 * Since it doesn't exist elsewhere, we'll put it here for now.
 */
extern void do_IRQ(int irq, struct pt_regs *regs);
extern void sa1100_GPIO11_27_demux(int irq, void *dev_id, struct pt_regs *regs);

#endif /* _ASM_ARCH_IRQ_H */
