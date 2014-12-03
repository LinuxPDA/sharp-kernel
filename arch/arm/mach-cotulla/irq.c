/*
 * linux/arch/arm/mach-cotulla/irq.c
 *
 * Copyright (C) 2001 Lineo, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * This code is based on arch/arm/mach-sa1100/irq.c
 *
 * ChangLog:
 *	29-APR-2002 Richard Rau
*/

#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/irq.h>
#include <asm/arch/cotulla.h>

#ifdef CONFIG_SABINAL_DISCOVERY
#include <asm/arch/discovery_asic3.h>
#endif

/*
 * Cotulla GPIO edge detection for IRQs:
 * IRQs are generated on Falling-Edge, Rising-Edge, or both.
 * This must be called *before* the appropriate IRQ is registered.
 * Use this instead of directly setting GRER/GFER.
 */

static int GPIO_IRQ_rising_edge[3];
static int GPIO_IRQ_falling_edge[3];

void set_GPIO_IRQ_edge(int gpio_mask, int edge, int gpio_num)
{
	if (edge & GPIO_FALLING_EDGE)
		GPIO_IRQ_falling_edge[gpio_num] |= gpio_mask;
	else
		GPIO_IRQ_falling_edge[gpio_num] &= ~gpio_mask;
	if (edge & GPIO_RISING_EDGE)
		GPIO_IRQ_rising_edge[gpio_num] |= gpio_mask;
	else
		GPIO_IRQ_rising_edge[gpio_num] &= ~gpio_mask;
}

EXPORT_SYMBOL(set_GPIO_IRQ_edge);



/*
 * We don't need to ACK IRQs on the SA1100 unless they're GPIOs
 * this is for internal IRQs i.e. from 11 to 31.
 */

static void cotulla_mask_irq(unsigned int irq)
{
	ICMR &= ~(1 << irq);
}

static void cotulla_unmask_irq(unsigned int irq)
{
	ICMR |= (1 << irq);
}

/*
 * GPIO IRQs must be acknoledged.  This is for IRQs from 0 to 10.
 */

static void cotulla_mask_and_ack_GPIO8_9_irq(unsigned int irq)
{
	ICMR &= ~(1 << irq);
	GEDR(0) = (1 << (irq - 8));
}

static void cotulla_mask_GPIO0_9_irq(unsigned int irq)
{
	ICMR &= ~(1 << irq);
}

static void cotulla_unmask_GPIO8_9_irq(unsigned int irq)
{
	GRER(0) = (GRER(0) & ~(1 << (irq - 8))) | (GPIO_IRQ_rising_edge[0] & (1 << (irq - 8)));
	GFER(0) = (GFER(0) & ~(1 << (irq - 8))) | (GPIO_IRQ_falling_edge[0] & (1 << (irq - 8)));
	ICMR |= (1 << irq);
}

/*
 * Install handler for GPIO 2-80 edge detect interrupts
 */


static int GPIO_2_80_enabled[3];	/* enabled i.e. unmasked GPIO IRQs */
static int GPIO_2_80_spurious[3];	/* GPIOs that triggered when masked */

static void cotulla_GPIO2_80_demux(int irq, void *dev_id,
				   struct pt_regs *regs)
{
	int i, spurious, n;

	for (n=0; n<3; n++) {
		while ((irq = (GEDR(n) & (!n ? 0xfffffffc : 0xffffffff)))) {
			/*
			 * We don't want to clear GRER/GFER when the
			 * corresponding IRQ is masked because we could miss
			 * a level transition i.e. an IRQ which need servicing
			 * as soon as it is unmasked.  However, such situation
			 * should happen only during the loop below.  Thus all
			 * IRQs which aren't enabled at this point are
			 * considered spurious.  Those are cleared but only
			 * de-activated if they happen twice.
			 */
			spurious = irq & ~GPIO_2_80_enabled[n];
			if (spurious) {
				GEDR(n) = spurious;
				GRER(n) &= ~(spurious & GPIO_2_80_spurious[n]);
				GFER(n) &= ~(spurious & GPIO_2_80_spurious[n]);
				GPIO_2_80_spurious[n] |= spurious;
				irq ^= spurious;
				if (!irq) continue;
			}

			for (i = ((n == 0) ? 2 : 0); i <= 31; ++i) {
				if (irq & (1<<i)) {
					do_IRQ (IRQ_GPIO_NUM(n, i), regs);
				}
			}
		}
	}
}

static struct irqaction GPIO2_80_irq = {
	name:		"GPIO 2-80",
	handler:	cotulla_GPIO2_80_demux,
	flags:		SA_INTERRUPT
};

#ifdef CONFIG_SABINAL_DISCOVERY
#define DISCOVERY_EVT2

static void discovery_asic_demux(int irq, void *dev_id,
				   struct pt_regs *regs)
{
	
#if defined(DISCOVERY_EVT)
	int i, spurious, n;

	printk("discovery_ASIC1_demux: irq=%d\n",irq);
	
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_WAKE_UP_BIT) {
		do_IRQ (IRQ_ASIC1_WAKE_UP, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_CF_IOIS16_BIT) {
		do_IRQ (IRQ_ASIC1_CF_IOIS16, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_EDGE_STATUS &  ASIC1_SD_DETECT_BIT) {
		do_IRQ (IRQ_ASIC1_SD_DETECT, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_EDGE_STATUS &  ASIC1_CF_DETECT_BIT) {
		do_IRQ (IRQ_ASIC1_CF_DETECT, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_CF_CHG_EN_BIT) {
		do_IRQ (IRQ_ASIC1_CF_CHG_EN, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_KEY_IN_BIT) {
		do_IRQ (IRQ_ASIC1_KEY_IN, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_EDGE_STATUS &  ASIC1_KEY_INTERRUPT_BIT) {
		do_IRQ (IRQ_ASIC1_KEY_INTERRUPT, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_HEADPHONE_BIT) {
		do_IRQ (IRQ_ASIC1_HEADPHONE, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_AC_IN_BIT) {
		do_IRQ (IRQ_ASIC1_AC_IN, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_PWR_ON_BIT) {
		do_IRQ (IRQ_ASIC1_PWR_ON, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_EDGE_STATUS &  ASIC1_COM_DCD_BIT) {
		do_IRQ (IRQ_ASIC1_COM_DCD, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_CONN60_4_BIT) {
		do_IRQ (IRQ_ASIC1_CONN60_4, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_CONN60_5_BIT) {
		do_IRQ (IRQ_ASIC1_CONN60_5, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_BACKPART_DETECT_BIT) {
		do_IRQ (IRQ_ASIC1_BACKPART_DETECT, regs);
		return;
	}
	if (DISCOVERY_ASIC1_GPIO_LEVEL_STATUS &  ASIC1_CF_BATT_FAULT_BIT) {
		do_IRQ (IRQ_ASIC1_CF_BATT_FAULT, regs);
		return;
	}
#elif defined(DISCOVERY_EVT2) // Richard 0429 modified for ASIC3
	int i;
	
//	printk("discovery_ASIC1_demux: irq=%d\n",irq);
	
	for (i = IRQ_ASIC3_A0;i <= IRQ_ASIC3_A15;i++) {
		if (ASIC3_GPIO_INTSTAT_A & 1 << (i - IRQ_ASIC3_A0)) {
			do_IRQ(i, regs);
			return;
		}
	}
/*
	for (i = IRQ_ASIC3_B0;i <= IRQ_ASIC3_B15;i++) {
		if (ASIC3_GPIO_INTSTAT_B & 1 << (i - IRQ_ASIC3_B0)) {
			do_IRQ(i, regs);
			return;
		}
	}  
	for (i = IRQ_ASIC3_C0;i <= IRQ_ASIC3_C15;i++) {
		if (ASIC3_GPIO_INTSTAT_C & 1 << (i - IRQ_ASIC3_C0)) {
			do_IRQ(i, regs);
			return;
		}
	}
*/	
	for (i = IRQ_ASIC3_D0;i <= IRQ_ASIC3_D15;i++) {
		if (ASIC3_GPIO_INTSTAT_D & 1 << (i - IRQ_ASIC3_D0)) {
			do_IRQ(i, regs);
			return;
		}
	}
#endif
}

static struct irqaction asic_irq = {
	name:		"ASIC IRQ",
	handler:	discovery_asic_demux,
	flags:		SA_INTERRUPT
};

static void discovery_mask_and_ack_asic_irq(unsigned int irq)
{
	int mask = (1);

//	ICMR &= ~(1 << 8);
	GRER(0) = (GRER(0) & ~mask);
	GFER(0) = (GFER(0) & ~mask);
	GEDR(0) = mask;
}

static void discovery_mask_asic_irq(unsigned int irq)
{
	ICMR &= ~(1 << 8);
}

static void discovery_unmask_asic_irq(unsigned int irq)
{
	int mask = (1);
	
//	GRER(0) = (ASIC_GRER & ~mask) | (ASIC_GPIO_IRQ_rising_edge & mask);
//	GFER(0) = (ASIC_GFER & ~mask) | (ASIC_GPIO_IRQ_falling_edge & mask);
	GRER(0) = (GRER(0) | mask);
	GFER(0) = (GFER(0) | mask);
	GPDR(0) = GPDR(0) & 0xfffffffe;

	ICMR |= (1 << irq);
}

#endif 

static void cotulla_mask_and_ack_GPIO2_80_irq(unsigned int irq)
{
	int n = (irq < 62) ? 0 : ((irq < 94) ? 1 : 2);
	int mask = (1 << GPIO_NUM_IRQ(n, irq));
	GPIO_2_80_spurious[n] &= ~mask;
	GPIO_2_80_enabled[n] &= ~mask;
	GEDR(n) = mask;
}

static void cotulla_mask_GPIO2_80_irq(unsigned int irq)
{
	int n = (irq < 62) ? 0 : ((irq < 94) ? 1 : 2);
	int mask = (1 << GPIO_NUM_IRQ(n, irq));
	GPIO_2_80_spurious[n] &= ~mask;
	GPIO_2_80_enabled[n] &= ~mask;
}

static void cotulla_unmask_GPIO2_80_irq(unsigned int irq)
{
	int n = (irq < 62) ? 0 : ((irq < 94) ? 1 : 2);
	int mask = (1 << GPIO_NUM_IRQ(n, irq));
	if (GPIO_2_80_spurious[n] & mask) {
		/*
		 * We don't want to miss an interrupt that would have occurred
		 * while it was masked.  Simulate it if it is the case.
		 */
		int state = GPLR(n);
		if (((state & GPIO_IRQ_rising_edge[n]) |
		     (~state & GPIO_IRQ_falling_edge[n])) & mask)
		  {
			/* just in case it gets referenced: */
			struct pt_regs dummy;

			memzero(&dummy, sizeof(dummy));
			do_IRQ(irq, &dummy);

			/* we are being called recursively from do_IRQ() */
			return;
		  }
	}
	GPIO_2_80_enabled[n] |= mask;
	GRER(n) = (GRER(n) & ~mask) | (GPIO_IRQ_rising_edge[n] & mask);
	GFER(n) = (GFER(n) & ~mask) | (GPIO_IRQ_falling_edge[n] & mask);
}


void asic_mask_ack_irq(unsigned int irq)
{
	int remainder = ((irq - IRQ_ASIC3_A0) % 16);
	u16 shift = (1 << remainder);

//	printk("%s: %d, %d \n", __FUNCTION__, irq, remainder);
	switch ((int)((irq - IRQ_ASIC3_A0) / 16)) {
	case 0:
		ASIC3_GPIO_MASK_A |= shift;
		ASIC3_GPIO_INTSTAT_A &= ~shift;
		break;
/*	case 1:
		ASIC3_GPIO_MASK_B |= shift;
		ASIC3_GPIO_INTSTAT_B &= ~shift;
		break;
	case 2:
		ASIC3_GPIO_MASK_C |= shift;
		ASIC3_GPIO_INTSTAT_C &= ~shift;
		break;
*/	case 3:
		ASIC3_GPIO_MASK_D |= shift;
		ASIC3_GPIO_INTSTAT_D &= ~shift;
		break;
	}	
}

void asic_mask_irq(unsigned int irq)
{
	int remainder = ((irq - IRQ_ASIC3_A0) % 16);
	u16 shift = (1 << remainder);
	
//	printk("%s: %d, %d \n", __FUNCTION__, irq, remainder);
	switch ((int)((irq - IRQ_ASIC3_A0) / 16)) {
	case 0:
		ASIC3_GPIO_MASK_A |= shift;
		break;
/*	case 1:
		ASIC3_GPIO_MASK_B |= shift;
		break;
	case 2:
		ASIC3_GPIO_MASK_C |= shift;
		break;
*/	case 3:
		ASIC3_GPIO_MASK_D |= shift;
		break;
	}
}

void asic_unmask_irq(unsigned int irq)
{
	int remainder = ((irq - IRQ_ASIC3_A0) % 16);
	u16 shift = (1 << remainder);
	
	ASIC3_INTR_INTMASK |= INTMASK_GINT;
	
//	printk("%s: %d, %d \n", __FUNCTION__, irq, remainder);
	switch ((int)((irq - IRQ_ASIC3_A0) / 16)) {
	case 0:
		ASIC3_GPIO_MASK_A &= ~shift;
		break;
/*	case 1:
		ASIC3_GPIO_MASK_B &= ~shift;
		break;
	case 2:
		ASIC3_GPIO_MASK_C &= ~shift;
		break;
*/	case 3:
		ASIC3_GPIO_MASK_D &= ~shift;
		break;
	}
	ASIC3_GPIO_INTSTAT_A = 0;
	ASIC3_GPIO_INTSTAT_D = 0;
}

void __init cotulla_init_irq(void)
{
	int irq, n;

	/* disable all IRQs */
	ICMR = 0;

	/* all IRQs are IRQ, not FIQ */
	ICLR = 0;

	/* clear all GPIO edge detects */
	for (n=0; n<3; n++) {
		GFER(n) = 0;
		GRER(n) = 0;
		GEDR(n) = -1;
	}

	/*
	 * Whatever the doc says, this has to be set for the wait-on-irq
	 * instruction to work... on a Cotulla rev 9 at least.
	 */
	ICCR = 1;

	
#ifdef CONFIG_SABINAL_DISCOVERY

	irq = 8;
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= discovery_mask_and_ack_asic_irq;
	irq_desc[irq].mask	= discovery_mask_asic_irq;
	irq_desc[irq].unmask	= discovery_unmask_asic_irq;

	irq = 9;
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= cotulla_mask_and_ack_GPIO8_9_irq;
	irq_desc[irq].mask	= cotulla_mask_GPIO0_9_irq;
	irq_desc[irq].unmask	= cotulla_unmask_GPIO8_9_irq;

#else
	for (irq = 8; irq <= 9; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= cotulla_mask_and_ack_GPIO8_9_irq;
		irq_desc[irq].mask	= cotulla_mask_GPIO0_9_irq;
		irq_desc[irq].unmask	= cotulla_unmask_GPIO8_9_irq;
	}
#endif

	for (irq = 10; irq <= 31; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 0;
		irq_desc[irq].mask_ack	= cotulla_mask_irq;
		irq_desc[irq].mask	= cotulla_mask_irq;
		irq_desc[irq].unmask	= cotulla_unmask_irq;
	}

	for (irq = 32; irq <= 110; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= cotulla_mask_and_ack_GPIO2_80_irq;
		irq_desc[irq].mask	= cotulla_mask_GPIO2_80_irq;
		irq_desc[irq].unmask	= cotulla_unmask_GPIO2_80_irq;
	}

#ifdef CONFIG_SABINAL_DISCOVERY
	for (irq = 111; irq < NR_IRQS; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= asic_mask_ack_irq;
		irq_desc[irq].mask	= asic_mask_irq;
		irq_desc[irq].unmask	= asic_unmask_irq;
	}
	set_GPIO_IRQ_edge(GPIO_GPIO0, GPIO_BOTH_EDGES, 0 ); //steve: no effect. aleardy set in discovery.c
	setup_arm_irq( IRQ_GPIO0, &asic_irq );
//	enable_irq(IRQ_GPIO0);

#endif

	setup_arm_irq( IRQ_GPIO2_80, &GPIO2_80_irq );

	/*
	 * We generally don't want the LCD IRQ being
	 * enabled as soon as we request it.
	 */
	irq_desc[IRQ_LCD].noautoenable = 1;

	set_GPIO_IRQ_edge(GPIO_GPIO51, GPIO_FALLING_EDGE, 1);

}
