/*
 * linux/arch/arm/mach-sa1100/sa1111.c
 *
 * SA1111 support
 *
 * Original code by John Dorsey
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains all generic SA1111 support, except for DMA which is
 * provided separately in dma-sa1111.c.
 *
 * All initialization functions provided here are intended to be called
 * from machine specific code with proper arguments when required.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/errno.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/irq.h>

#include "sa1111.h"


/*
 * SA1111 initialization
 */

int __init sa1111_init(void)
{
	unsigned long id = SKID;

	if((id & SKID_ID_MASK) == SKID_SA1111_ID)
		printk( KERN_INFO "SA-1111 Microprocessor Companion Chip: "
			"silicon revision %lx, metal revision %lx\n",
			(id & SKID_SIREV_MASK)>>4, (id & SKID_MTREV_MASK));
	else {
		printk(KERN_ERR "Could not detect SA-1111!\n");
		return -EINVAL;
	}

	/*
	 * First, set up the 3.6864MHz clock on GPIO 27 for the SA-1111:
	 * (SA-1110 Developer's Manual, section 9.1.2.1)
	 */
	GAFR |= GPIO_32_768kHz;
	GPDR |= GPIO_32_768kHz;
	TUCR = TUCR_3_6864MHz;

	/* Now, set up the PLL and RCLK in the SA-1111: */
	SKCR = SKCR_PLL_BYPASS | SKCR_RDYEN | SKCR_OE_EN;
	udelay(100);
	SKCR = SKCR_PLL_BYPASS | SKCR_RCLKEN | SKCR_RDYEN | SKCR_OE_EN;

	/*
	 * SA-1111 Register Access Bus should now be available. Clocks for
	 * any other SA-1111 functional blocks must be enabled separately
	 * using the SKPCR.
	 */

	/*
	 * If the system is going to use the SA-1111 DMA engines, set up
	 * the memory bus request/grant pins. Also configure the shared
	 * memory controller on the SA-1111 (SA-1111 Developer's Manual,
	 * section 3.2.3) and power up the DMA bus clock:
	 */
	GAFR |= (GPIO_MBGNT | GPIO_MBREQ);
	GPDR |= GPIO_MBGNT;
	GPDR &= ~GPIO_MBREQ;
	TUCR |= TUCR_MR;

	return 0;
}


/*
 * SA1111  Interrupt support
 */

void sa1111_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int i;
	unsigned long stat0, stat1;

	for(;;) {
		stat0 = INTSTATCLR0, stat1 = INTSTATCLR1;
		if( !stat0 && !stat1 ) break;
		if( stat0 )
			for( i = 0; i < 32; i++ )
				if( stat0 & (1<<i) )
					do_IRQ( SA1111_IRQ(i), regs );

		if( stat1 )
			for( i = 32; i < 55; i++ )
				if( stat1 & (1<<(i-32)) )
					do_IRQ( SA1111_IRQ(i), regs );
	}
}

static struct irqaction sa1111_irq = {
	name:		"SA1111",
	handler:	sa1111_IRQ_demux,
	flags:		SA_INTERRUPT
};

static void sa1111_mask_and_ack_lowirq(unsigned int irq)
{
	unsigned int mask = 1 << (irq - SA1111_IRQ(0));

	// broken hardware: interrupt events are lost if they occur
	// while the interrupts are disabled.
	//INTEN0 &= ~mask;
	INTSTATCLR0 = mask;
}

static void sa1111_mask_and_ack_highirq(unsigned int irq)
{
	unsigned int mask = 1 << (irq - SA1111_IRQ(32));

	//INTEN1 &= ~mask;
	INTSTATCLR1 = mask;
}

static void sa1111_mask_lowirq(unsigned int irq)
{
	//INTEN0 &= ~(1 << (irq - SA1111_IRQ(0)));
}

static void sa1111_mask_highirq(unsigned int irq)
{
	//INTEN1 &= ~(1 << (irq - SA1111_IRQ(32)));
}

static void sa1111_unmask_lowirq(unsigned int irq)
{
	INTEN0 |= 1 << (irq - SA1111_IRQ(0));
}

static void sa1111_unmask_highirq(unsigned int irq)
{
	INTEN1 |= 1 << ((irq - SA1111_IRQ(32)));
}

void __init sa1111_init_irq(int gpio_nr)
{
	int irq;

	/* disable all IRQs */
	INTEN0 = 0;
	INTEN1 = 0;

	/* detect on rising edge */
	INTPOL0 = 0;
	INTPOL1 = 0;

	/* clear all IRQs */
	INTSTATCLR0 = -1;
	INTSTATCLR1 = -1;

	for (irq = SA1111_IRQ(0); irq <= SA1111_IRQ(26); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= sa1111_mask_and_ack_lowirq;
		irq_desc[irq].mask	= sa1111_mask_lowirq;
		irq_desc[irq].unmask	= sa1111_unmask_lowirq;
	}
	for (irq = SA1111_IRQ(32); irq <= SA1111_IRQ(54); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= sa1111_mask_and_ack_highirq;
		irq_desc[irq].mask	= sa1111_mask_highirq;
		irq_desc[irq].unmask	= sa1111_unmask_highirq;
	}

	/* Not every machines has the SA1111 interrupt routed to a GPIO */
	if (gpio_nr >= 0) {
		set_GPIO_IRQ_edge (GPIO_GPIO(gpio_nr), GPIO_RISING_EDGE);
		setup_arm_irq (SA1100_GPIO_TO_IRQ(gpio_nr), &sa1111_irq);
	}
}
