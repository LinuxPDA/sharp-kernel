/*
 * linux/arch/arm/mach-pxa/discovery_arch.c
 *
 * Copyright (C) 2001 Lineo, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Change Log
 *	24-May-2002 SHARP	Change Bank Settings
 *	31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
 */

#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include "generic.h"

//#include <linux/mm.h>

//#include <asm/pgtable.h>


#define DISCOVERY_EVT2

static void discovery_asic_demux(int irq, void *dev_id,
				   struct pt_regs *regs)
{
	
#if defined(DISCOVERY_EVT)
	int i, spurious, n;

	printk("discovery_ASIC1_demux: irq=%d\n",irq);
	
	//steve TBD: before calling each handler, plz make sure if need to clear status. 
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

//  steve TBD
//	ICMR &= ~(1 << 8);
	GRER0 = (GRER0 & ~mask);
	GFER0 = (GFER0 & ~mask);
	GEDR0 = mask;
}

static void discovery_mask_asic_irq(unsigned int irq)
{
	ICMR &= ~(1 << 8);
}

static void discovery_unmask_asic_irq(unsigned int irq)
{
	int mask = (1);
	
//steve TBD
//	GRER0 = (ASIC_GRER & ~mask) | (ASIC_GPIO_IRQ_rising_edge & mask);
//	GFER0 = (ASIC_GFER & ~mask) | (ASIC_GPIO_IRQ_falling_edge & mask);
	GRER0 = (GRER0 | mask);
	GFER0 = (GFER0 | mask);
	GPDR0 = GPDR0 & 0xfffffffe;

	ICMR |= (1 << (irq + PXA_IRQ_SKIP));
}

void asic_mask_ack_irq(unsigned int irq)
{
/*	DISCOVERY_ASIC1_GPIO_MASK |= (1 << (irq - IRQ_ASIC1_0));
	DISCOVERY_ASIC1_GPIO_LEVEL_STATUS = 0;
	DISCOVERY_ASIC1_GPIO_EDGE_STATUS = 0;*/
	
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
//	DISCOVERY_ASIC1_GPIO_MASK |= (1 << (irq - IRQ_ASIC1_0));
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
//	DISCOVERY_ASIC1_GPIO_MASK &= ~(1 << (irq - IRQ_ASIC1_0));
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

static void __init discovery_init_irq(void)
{
	int irq;

	pxa_init_irq();

	/* setup extra discovery irqs */
	irq = IRQ_GPIO0;
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
	irq_desc[irq].mask_ack	= discovery_mask_and_ack_asic_irq;
	irq_desc[irq].mask	= discovery_mask_asic_irq;
	irq_desc[irq].unmask	= discovery_unmask_asic_irq;

	irq = IRQ_GPIO1;
	irq_desc[irq].valid	= 1;

	for (irq = IRQ_GPIO(81); irq < NR_IRQS; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= asic_mask_ack_irq;
		irq_desc[irq].mask	= asic_mask_irq;
		irq_desc[irq].unmask	= asic_unmask_irq;
	}
//	set_GPIO_IRQ_edge2(GPIO_GPIO(0), GPIO_BOTH_EDGES, 0 ); //steve: no effect. aleardy set in discovery.c
	set_GPIO_IRQ_edge(0, GPIO_BOTH_EDGES); //steve: no effect. aleardy set in discovery.c
	setup_arm_irq( IRQ_GPIO0, &asic_irq );
//	enable_irq(IRQ_GPIO0);

	/*
	 * We generally don't want the LCD IRQ being
	 * enabled as soon as we request it.
	 */
	irq_desc[IRQ_LCD].noautoenable = 1;

//	set_GPIO_IRQ_edge2(GPIO_GPIO(51), GPIO_FALLING_EDGE, 1);
	set_GPIO_IRQ_edge(51, GPIO_FALLING_EDGE);
}

static void __init
fixup_discovery(struct machine_desc *desc, struct param_struct *params,
		char **cmdline, struct meminfo *mi)
{
	printk("fixup_discovery:\n");
	
	system_rev = 0xF;

	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size  = (32*1024*1024);
	mi->bank[0].node  = 0;

	mi->bank[1].start = 0xa4000000;
	mi->bank[1].size  = (4*1024*1024);
	mi->bank[1].node  = 0;
	mi->nr_banks      = 2;


#ifdef CONFIG_ROOT_NFS
	ROOT_DEV = to_kdev_t(0x00ff);   /* /dev/nfs */
#elif defined(CONFIG_BLK_DEV_INITRD)
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	setup_ramdisk( 1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE );
	setup_initrd( 0xC0400000, 24 * 1024 * 1024 );
#elif defined(CONFIG_MTD)
	ROOT_DEV = MKDEV(31, 0);	/* /dev/mtdblock0 */
#endif

}

/*
 * Common I/O mapping:
 *
 * Typically, static virtual address mappings are as follow:
 *
 * 0xe8000000-0xefffffff:	flash memory (especially when multiple flash
 * 				banks need to be mapped contigously)
 * 0xf0000000-0xf3ffffff:	miscellaneous stuff (CPLDs, etc.)
 * 0xf4000000-0xf4ffffff:	SA-1111
 * 0xf5000000-0xf5ffffff:	reserved (used by cache flushing area)
 * 0xf6000000-0xffffffff:	reserved (internal COTULLA IO defined above)
 *
 * Below 0xe8000000 is reserved for vm allocation.
 *
 * The machine specific code must provide the extra mapping beside the
 * default mapping provided here.
 */

#if 0
static struct map_desc standard_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xf0000000, 0x40000000, 0x01400000, DOMAIN_IO, 0, 1, 0, 0 }, /* Peripheral */
  { 0xf4000000, 0x44000000, 0x00200000, DOMAIN_IO, 0, 1, 0, 0 }, /* LCD */		// Richard
  { 0xf6000000, 0x11000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* PCMCIA0 IO */	// CYB
  { 0xf7000000, 0x12000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Backpack GPIO */	// Richard
  { 0xf8000000, 0x08000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* ASIC3 */		// Richard
  { 0xf9000000, 0xa4000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Special case */
  { 0xfd000000, 0x48000000, 0x00200000, DOMAIN_IO, 0, 1, 0, 0 }, /* MSCx */
  { 0xff000000, 0x16000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* ppsh */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* 32M main flash (cs0) */
  LAST_DESC
};
#else
static struct map_desc discovery_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xf6000000, 0x11000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* PCMCIA0 IO */	// CYB
  { 0xf7000000, 0x12000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Backpack GPIO */	// Richard
  { 0xf1000000, 0x08000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* ASIC3 */		// Richard
  { 0xf2000000, 0xa4000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Special case */
  { 0xff000000, 0x16000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* ppsh */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* 32M main flash (cs0) */
  LAST_DESC
};
#endif

extern void discovery_init(void);

static void __init discovery_map_io(void)
{
#if 0
        iotable_init(standard_io_desc);
#else
	pxa_map_io();
	iotable_init(discovery_io_desc);
#endif

	discovery_init();
}

MACHINE_START(DISCOVERY, "Sharp-Discovery")
	MAINTAINER("Lineo, Inc.")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
	FIXUP(fixup_discovery)
	MAPIO(discovery_map_io)
	INITIRQ(discovery_init_irq)
MACHINE_END

