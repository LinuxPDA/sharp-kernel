/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/arch/arm/mach-db1mx1/irq.c
 *
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/mach/irq.h>
#include <asm/arch/irq.h>

static void dbmx1_mask_irq(unsigned int irq)
{
    DBMX1_AITC_INTDISNUM = irq;
}

static void dbmx1_unmask_irq(unsigned int irq)
{
    DBMX1_AITC_INTENNUM = irq;
}

 
static void dbmx1_gpioa_mask_irq(unsigned int irq)
{
    DBMX1_GPIO_IMR_A &= ~(1 << (irq - IRQ_GPIO_A(0)));
}

static void dbmx1_gpioa_unmask_irq(unsigned int irq)
{
    DBMX1_GPIO_IMR_A |= 1 << (irq - IRQ_GPIO_A(0));
}

static void dbmx1_gpioa_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    int i, req;

    req = DBMX1_GPIO_ISR_A;
    DBMX1_GPIO_ISR_A = req;
    for (i = 0; i < 32; i++) {
	if (req & (1 << i))
	    asm_do_IRQ(IRQ_GPIO_A(i), regs);
    }
}

static void dbmx1_gpiob_mask_irq(unsigned int irq)
{
    DBMX1_GPIO_IMR_B &= ~(1 << (irq - IRQ_GPIO_B(0)));
}

static void dbmx1_gpiob_unmask_irq(unsigned int irq)
{
    DBMX1_GPIO_IMR_B |= 1 << (irq - IRQ_GPIO_B(0));
}

static void dbmx1_gpiob_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    int i, req;

    req = DBMX1_GPIO_ISR_B;
    DBMX1_GPIO_ISR_B = req;
    for (i = 0; i < 32; i++) {
	if (req & (1 << i))
	    asm_do_IRQ(IRQ_GPIO_B(i), regs);
    }
}

#ifdef CONFIG_DBMX1_TPM102
static void dbmx1_int7_mask_irq(unsigned int irq)
{
    if (irq < IRQ_INT7_START + 8) {
	APLAT_FPGA_IRQ_CTR2 &= ~(1 << (irq - IRQ_INT7_START));
	APLAT_FPGA_IRQ_STR2 &= ~(1 << (irq - IRQ_INT7_START));
    } else if (irq < IRQ_INT7_START + 16) {
	APLAT_FPGA_IRQ_CTR1 &= ~(1 << (irq - IRQ_INT7_START - 8));
	APLAT_FPGA_IRQ_STR1 &= ~(1 << (irq - IRQ_INT7_START - 8));
    } else {
	DBMX1_GPIO_IMR_A &= ~(1 << 7);
    }
}

static void dbmx1_int7_unmask_irq(unsigned int irq)
{
    if (irq < IRQ_INT7_START + 8) {
	APLAT_FPGA_IRQ_STR2 &= ~(1 << (irq - IRQ_INT7_START));
	APLAT_FPGA_IRQ_CTR2 |= 1 << (irq - IRQ_INT7_START);
    } else if (irq < IRQ_INT7_START + 16) {
	APLAT_FPGA_IRQ_STR1 &= ~(1 << (irq - IRQ_INT7_START - 8));
	APLAT_FPGA_IRQ_CTR1 |= 1 << (irq - IRQ_INT7_START - 8);
    } else {
	DBMX1_GPIO_IMR_A |= 1 << 7;
    }
}

static void dbmx1_int7_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    int i;
    unsigned int status;

    status = (APLAT_FPGA_IRQ_STR1 << 8) | APLAT_FPGA_IRQ_STR2;
    if (status) {
	for (i = 0; i < 16; i++) {
	    if ((status & (1 << i)))
		asm_do_IRQ(IRQ_INT7_START + i, regs);
	}
	APLAT_FPGA_IRQ_STR1 = 0x00;
	APLAT_FPGA_IRQ_STR2 = 0x00;
    } else {
	for (i = 16; i <= IRQ_INT7_END - IRQ_INT7_START; i++) {
	    asm_do_IRQ(IRQ_INT7_START + i, regs);
	}
    }
}
#endif

static struct irqaction gpioa_irq = {
    .name	= "gpioa",
    .handler	= dbmx1_gpioa_interrupt,
    .flags	= SA_INTERRUPT
};

static struct irqaction gpiob_irq = {
    .name	= "gpiob",
    .handler	= dbmx1_gpiob_interrupt,
    .flags	= SA_INTERRUPT
};

#ifdef CONFIG_DBMX1_TPM102
static struct irqaction int7_irq = {
    .name	= "int7",
    .handler	= dbmx1_int7_interrupt,
    .flags	= SA_INTERRUPT
};
#endif

void __init dbmx1_init_irq(void)
{
    unsigned int i;
    int irq;

    DBMX1_AITC_INTENABLEH = 0;
    DBMX1_AITC_INTENABLEL = 0;
    DBMX1_AITC_INTTYPEH = 0;
    DBMX1_AITC_INTTYPEL = 0;
    DBMX1_GPIO_ISR_A = 0xffffffff;
    DBMX1_GPIO_ISR_B = 0xffffffff;

    for (i = 0; i < IRQ_GPIO_A(0); i++) {
	irq_desc[i].valid = 1;
	irq_desc[i].probe_ok = 1;
	irq_desc[i].mask_ack = dbmx1_mask_irq;
	irq_desc[i].mask = dbmx1_mask_irq;
	irq_desc[i].unmask = dbmx1_unmask_irq;
    }

    for (irq = IRQ_GPIO_A(0); irq <= IRQ_GPIO_A(31); irq++) {
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= dbmx1_gpioa_mask_irq;
	irq_desc[irq].mask	= dbmx1_gpioa_mask_irq;
	irq_desc[irq].unmask	= dbmx1_gpioa_unmask_irq;
    }
    setup_arm_irq(IRQ_GPIO_INT_PORTA, &gpioa_irq);

    for (irq = IRQ_GPIO_B(0); irq <= IRQ_GPIO_B(31); irq++) {
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= dbmx1_gpiob_mask_irq;
	irq_desc[irq].mask	= dbmx1_gpiob_mask_irq;
	irq_desc[irq].unmask	= dbmx1_gpiob_unmask_irq;
    }
    setup_arm_irq(IRQ_GPIO_INT_PORTB, &gpiob_irq);

#ifdef CONFIG_DBMX1_TPM102
    DBMX1_GPIO_ICR1_A = (DBMX1_GPIO_ICR1_A & 0xffff3fff) | 0x00008000;
    APLAT_FPGA_IRQ_STR1 = 0x00;
    APLAT_FPGA_IRQ_STR2 = 0x00;
    APLAT_FPGA_IRQ_CTR1 = 0x00;
    APLAT_FPGA_IRQ_CTR2 = 0x00;
    APLAT_FPGA_CLK_STR  = 0x0f;

    for (irq = IRQ_INT7_START; irq <= IRQ_INT7_END; irq++) {
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= dbmx1_int7_mask_irq;
	irq_desc[irq].mask	= dbmx1_int7_mask_irq;
	irq_desc[irq].unmask	= dbmx1_int7_unmask_irq;
    }
    setup_arm_irq(IRQ_GPIO_INT7, &int7_irq);
#endif
}
