#ifdef __KERNEL__
#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/atomic.h>

extern void disable_irq(unsigned int);
extern void disable_irq_nosync(unsigned int);
extern void enable_irq(unsigned int);

/*
 * this is the # irq's for all ppc arch's (pmac/chrp/prep)
 * so it is the max of them all
 */
#define NR_IRQS			512

#define NUM_8259_INTERRUPTS	16

/*
 * This gets called from serial.c, which is now used on
 * powermacs as well as prep/chrp boxes.
 * Prep and chrp both have cascaded 8259 PICs.
 */
static __inline__ int irq_cannonicalize(int irq)
{
	return irq;
}

#define NR_MASK_WORDS	((NR_IRQS + 63) / 64)
/* pendatic: these are long because they are used with set_bit --RR */
extern unsigned long ppc_cached_irq_mask[NR_MASK_WORDS];
extern unsigned long ppc_lost_interrupts[NR_MASK_WORDS];
extern atomic_t ppc_n_lost_interrupts;

#endif /* _ASM_IRQ_H */
#endif /* __KERNEL__ */
