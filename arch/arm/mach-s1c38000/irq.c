/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * arch/arm/mach-s1c38000/irq.c
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/ptrace.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/irq.h>

extern unsigned long soft_irq_mask;

static void s1c38000_mask_irq(unsigned int irq)
{
    IRQ_ENABLECLEAR = 1 << irq;
    soft_irq_mask &= ~(1 << irq);
}

static void s1c38000_unmask_irq(unsigned int irq)
{
    soft_irq_mask |= (1 << irq);
    IRQ_ENABLE = 1 << irq;
}
 
static void s1c38000_gpioa_mask_irq(unsigned int irq)
{
    GPIOA_MASK &= ~(1 << (irq - IRQ_GPIO_A(0)));
}

static void s1c38000_gpioa_unmask_irq(unsigned int irq)
{
    GPIOA_MASK |= 1 << (irq - IRQ_GPIO_A(0));
}

static void s1c38000_gpioa_interrupt(int irq, void *dev_id,
				     struct pt_regs *regs)
{
    int i, req;

    req = GPIOA_INT & 0xff;
    for (i = 0; i < 8; i++) {
	if (req & (1 << i))
	    asm_do_IRQ(IRQ_GPIO_A(i), regs);
    }
}

static void s1c38000_gpiob_mask_irq(unsigned int irq)
{
    GPIOB_MASK &= ~(1 << (irq - IRQ_GPIO_B(0)));
}

static void s1c38000_gpiob_unmask_irq(unsigned int irq)
{
    GPIOB_MASK |= 1 << (irq - IRQ_GPIO_B(0));
}

static void s1c38000_gpiob_interrupt(int irq, void *dev_id,
				     struct pt_regs *regs)
{
    int i, req;

    req = GPIOB_INT & 0xff;
    for (i = 0; i < 8; i++) {
	if (req & (1 << i))
	    do_IRQ(IRQ_GPIO_B(i), regs);
    }
}

static struct irqaction gpioa_irq = {
    .name	= "gpioa",
    .handler	= s1c38000_gpioa_interrupt,
    .flags	= SA_INTERRUPT
};

static struct irqaction gpiob_irq = {
    .name	= "gpiob",
    .handler	= s1c38000_gpiob_interrupt,
    .flags	= SA_INTERRUPT
};

void __init s1c38000_init_irq(void)
{
    int irq;

    IRQ_ENABLECLEAR = 0xffffffff;	/* clear all interrupt enables */
    FIQ_ENABLECLEAR = 0xffffffff;	/* clear all fast interrupt enables */

    for (irq = 0; irq < IRQ_GPIO_A(0); irq++) {
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= s1c38000_mask_irq;
	irq_desc[irq].mask	= s1c38000_mask_irq;
	irq_desc[irq].unmask	= s1c38000_unmask_irq;
    }

    for (irq = IRQ_GPIO_A(0); irq <= IRQ_GPIO_A(7); irq++) {
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= s1c38000_gpioa_mask_irq;
	irq_desc[irq].mask	= s1c38000_gpioa_mask_irq;
	irq_desc[irq].unmask	= s1c38000_gpioa_unmask_irq;
    }
    setup_arm_irq(IRQ_GPIOA, &gpioa_irq);

    for (irq = IRQ_GPIO_B(0); irq <= IRQ_GPIO_B(7); irq++) {
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= s1c38000_gpiob_mask_irq;
	irq_desc[irq].mask	= s1c38000_gpiob_mask_irq;
	irq_desc[irq].unmask	= s1c38000_gpiob_unmask_irq;
    }
    setup_arm_irq(IRQ_GPIOB, &gpiob_irq);
}
