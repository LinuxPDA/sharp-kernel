/*
 * linux/arch/arm/mach-pxa/poodle.c
 *
 *  Support for the SHARP Poodle Board.
 *  
 *  Copyright:	Lineo Japan Inc.
 *
 * Based on:
 *  linux/arch/arm/mach-pxa/lubbock.c
 *
 *  Support for the Intel DBPXA250 Development Platform.
 *  
 *  Author:	Nicolas Pitre
 *  Created:	Jun 15, 2001
 *  Copyright:	MontaVista Software Inc.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 * Change Log
 *  12-Dec-2002 Sharp Corporation for Poodle
 */
#include <linux/config.h>
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
#include <asm/delay.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/irq.h>

#include "generic.h"

#include <linux/pm.h>
#ifdef CONFIG_PM
static struct pm_dev *ga_pm_dev;
#endif


extern void sharpsl_charge_start(void);
extern unsigned short chkFatalBatt(void);
extern int pxa_suspend(void);

static void lcm_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int req, i;

	while ( (req = (LCM_ICR & 0x0f00)) ) {
		for (i = 0; i <= 3; i++) {
			if (req & (0x0100<<i)) {
				do_IRQ( LCM_IRQ(i) , regs );
			}
		}
	}
}

static struct irqaction lcm_irq = {
	name:		"Locomo",
	handler:	lcm_IRQ_demux,
	flags:		SA_INTERRUPT
};

static void lcm_mask_and_ack_irq(unsigned int irq)
{
	LCM_ICR &= ~(0x0010 << (irq - LCM_IRQ(0)));
}

static void lcm_mask_irq(unsigned int irq)
{
	LCM_ICR &= ~(0x0010 << (irq - LCM_IRQ(0)));
}

static void lcm_unmask_irq(unsigned int irq)
{
	LCM_ICR |= (0x0010 << (irq - LCM_IRQ(0)));
}

static void lcm_key_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int req, i;

	while ( (req = (LCM_KIC & 0x0001)) ) {
		for (i = 0; i <= 0; i++) {
			if (req & (0x0001<<i)) {
				do_IRQ( LCM_IRQ_KEY_OFFSET(i) , regs );
			}
		}
	}
}

static struct irqaction lcm_key_irq = {
	name:		"lcm-key",
	handler:	lcm_key_IRQ_demux,
	flags:		SA_INTERRUPT
};

static void lcm_key_mask_and_ack_irq(unsigned int irq)
{
	LCM_KIC &= ~(0x0110 << (irq - LCM_IRQ_KEY_OFFSET(0)));
}

static void lcm_key_mask_irq(unsigned int irq)
{
	LCM_KIC &= ~(0x0010 << (irq - LCM_IRQ_KEY_OFFSET(0)));
}

static void lcm_key_unmask_irq(unsigned int irq)
{
	LCM_KIC |= (0x0010 << (irq - LCM_IRQ_KEY_OFFSET(0)));
}

static void lcm_gpio_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int req, i;

	while ( (req = (LCM_GIR & LCM_GPD & 0xffff)) ) {
		for (i = 0; i <= 15; i++) {
			if (req & (0x0001<<i)) {
				do_IRQ( LCM_IRQ_GPIO_OFFSET(i) , regs );
			}
		}
	}
}

static struct irqaction lcm_gpio_irq = {
	name:		"lcm-gpio",
	handler:	lcm_gpio_IRQ_demux,
	flags:		SA_INTERRUPT
};

static void lcm_gpio_mask_and_ack_irq(unsigned int irq)
{
	LCM_GIE &= ~(0x0001 << (irq - LCM_IRQ_GPIO_OFFSET(0)));

	LCM_GWE |=  (0x0001 << (irq - LCM_IRQ_GPIO_OFFSET(0)));
	LCM_GIS &= ~(0x0001 << (irq - LCM_IRQ_GPIO_OFFSET(0)));
	LCM_GWE &= ~(0x0001 << (irq - LCM_IRQ_GPIO_OFFSET(0)));
}

static void lcm_gpio_mask_irq(unsigned int irq)
{
	LCM_GIE &= ~(0x0001 << (irq - LCM_IRQ_GPIO_OFFSET(0)));
}

static void lcm_gpio_unmask_irq(unsigned int irq)
{
	LCM_GIE |= (0x0001 << (irq - LCM_IRQ_GPIO_OFFSET(0)));
}

static void lcm_lt_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int req, i;

	while ( (req = (LCM_LTINT & 0x0001)) ) {
		for (i = 0; i <= 0; i++) {
			if (req & (0x0001<<i)) {
				do_IRQ( LCM_IRQ_LT_OFFSET(i) , regs );
			}
		}
	}
}

static struct irqaction lcm_lt_irq = {
	name:		"lcm-lt",
	handler:	lcm_lt_IRQ_demux,
	flags:		SA_INTERRUPT
};

static void lcm_lt_mask_and_ack_irq(unsigned int irq)
{
	LCM_LTINT &= ~(0x0110 << (irq - LCM_IRQ_LT_OFFSET(0)));
}

static void lcm_lt_mask_irq(unsigned int irq)
{
	LCM_LTINT &= ~(0x0010 << (irq - LCM_IRQ_LT_OFFSET(0)));
}

static void lcm_lt_unmask_irq(unsigned int irq)
{
	LCM_LTINT |= (0x0010 << (irq - LCM_IRQ_LT_OFFSET(0)));
}

static void lcm_spi_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int req, i;

	while ( (req = (LCM_SPIIR & 0x000F)) ) {
		for (i = 0; i <= 3; i++) {
			if (req & (0x0001<<i)) {
				do_IRQ( LCM_IRQ_SPI_OFFSET(i) , regs );
			}
		}
	}
}

static struct irqaction lcm_spi_irq = {
	name:		"lcm-spi",
	handler:	lcm_spi_IRQ_demux,
	flags:		SA_INTERRUPT
};

static void lcm_spi_mask_and_ack_irq(unsigned int irq)
{
	LCM_SPIIE &= ~(0x0001 << (irq - LCM_IRQ_SPI_OFFSET(0)));

	LCM_SPIWE |=  (0x0001 << (irq - LCM_IRQ_SPI_OFFSET(0)));
	LCM_SPIIS &= ~(0x0001 << (irq - LCM_IRQ_SPI_OFFSET(0)));
	LCM_SPIWE &= ~(0x0001 << (irq - LCM_IRQ_SPI_OFFSET(0)));
}

static void lcm_spi_mask_irq(unsigned int irq)
{
	LCM_SPIIE &= ~(0x0001 << (irq - LCM_IRQ_SPI_OFFSET(0)));
}

static void lcm_spi_unmask_irq(unsigned int irq)
{
	LCM_SPIIE |= (0x0001 << (irq - LCM_IRQ_SPI_OFFSET(0)));
}

static void __init scoop_init(void)
{

#define	SCP_INIT_DATA(adr,dat)	(((adr)<<16)|(dat))
#define	SCP_INIT_DATA_END	((unsigned long)-1)
	static const unsigned long scp_init[] =
	{
		SCP_INIT_DATA(SCP_MCR,0x0140),  // 00
		SCP_INIT_DATA(SCP_MCR,0x0100),
		SCP_INIT_DATA(SCP_CDR,0x0000),  // 04
		SCP_INIT_DATA(SCP_CPR,0x0000),  // 0C
		SCP_INIT_DATA(SCP_CCR,0x0000),  // 10
		SCP_INIT_DATA(SCP_IMR,0x0000),  // 18
		SCP_INIT_DATA(SCP_IRM,0x00FF),  // 14
		SCP_INIT_DATA(SCP_ISR,0x0000),  // 1C
		SCP_INIT_DATA(SCP_IRM,0x0000),
		SCP_INIT_DATA(SCP_GPCR,SCP_IO_DIR),  // 20
		SCP_INIT_DATA(SCP_GPWR,SCP_IO_OUT),  // 24
		SCP_INIT_DATA_END
	};
	int	i;
	for(i=0; scp_init[i] != SCP_INIT_DATA_END; i++)
	{
		int	adr = scp_init[i] >> 16;
		SCP_REG(adr) = scp_init[i] & 0xFFFF;
	}
}


#ifdef CONFIG_PM
static int ga_pm_callback(struct pm_dev* pm_dev, pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
		disable_irq( IRQ_GPIO_GA_INT );
		disable_irq( LCM_IRQ_KEY_BASE );
		disable_irq( LCM_IRQ_GPIO_BASE );
		disable_irq( LCM_IRQ_LT_BASE );
		disable_irq( LCM_IRQ_SPI_BASE );

		break;
	case PM_RESUME:
		LCM_ASD  = (6+8+320+30)-10;/* synchronize lccr1 setting : colliefb.c */
		LCM_ASD |= 0x8000;
		LCM_HSD  = (6+8+320+30)-10-128+4;
		LCM_HSD |= 0x8000;
		LCM_HSC  = 128/8;

		enable_irq( IRQ_GPIO_GA_INT );
		enable_irq( LCM_IRQ_KEY_BASE );
		enable_irq( LCM_IRQ_GPIO_BASE );
		enable_irq( LCM_IRQ_LT_BASE );
		enable_irq( LCM_IRQ_SPI_BASE );
		break;
	}
	return 0;
}
#endif

void resume_init(void)
{
//	MSC0 = 0x02da02da; //msc0	
//	MSC1 = 0x7FFC7FFC; //msc1
//	MSC2 = 0x7FF47FFC; //msc2

//	GPDR0=0xDB828000;
//	GPDR1=0xFFB6A887;
//	GPDR2=0x0001FFFF;

//	PGSR0 = 0x01008000; //Sleep State
//	PGSR1 = 0x00160802; //Sleep State 
//	PGSR2 = 0x0001C000; //Sleep State
	
#if 0
	GRER0 = (GRER0 | 1); //raising
	GFER0 = (GFER0 | 1); //failing

	ICLR = 0;
	
	ICMR |= (1 << 10); //bit10, gpio02_80 enable
	ICMR |= (1 << 8); //bit8, gpio00 enable
	
	ICCR = 1; //Only enabled and unmasked will bring the Cotulla out of IDLE mode.
#endif
	
	CKEN |= 0x03; 
	CKEN |= CKEN3_SSP;
	CKEN |= CKEN1_PWM1;
}

static int __init poodle_hw_init(void)
{

  /* cpu initialize */
  /* Pgsr Register */
  	PGSR0 = 0x0146dd80;
  	PGSR1 = 0x03bf0890;
  	PGSR2 = 0x0001c000;

  /* Alternate Register */
  	GAFR0_L = 0x01001000;
  	GAFR0_U = 0x591a8010;
  	GAFR1_L = 0x900a8451;
  	GAFR1_U = 0xaaa5aaaa;
  	GAFR2_L = 0x8aaaaaaa;
  	GAFR2_U = 0x00000002;

  /* Direction Register */
  	GPDR0 = 0xd3f0904c;
  	GPDR1 = 0xfcffb7d3;
  	GPDR2 = 0x0001ffff;

  /* Output Register */
  	GPCR0 = 0x00000000;
  	GPCR1 = 0x00000000;
  	GPCR2 = 0x00000000;

  	GPSR0 = 0x00400000;
  	GPSR1 = 0x00000000;
        GPSR2 = 0x00000000;

  /* i2c initialize */
        i2c_init();

  /* locomo initialize */
	/* KEYBOARD */
	LCM_KIC = 0;
	/* GPIO */
	LCM_GPO = LCM_GPIO_232VCC_ON;
	LCM_GPE = 0;
	LCM_GPD = 0;
	LCM_GIE = 0;
	/* FrontLight */
	LCM_ALS = 0;
	LCM_ALD = 0;
	/* Longtime timer */
	LCM_LTINT = 0;
	/* SPI */
	LCM_SPIIE = 0;

	printk( KERN_INFO "GA Chip: %c%c\n",
		(LCM_VER >> 8), (LCM_VER & 0xff));

#if 0	///////////////////////////// ????
	GAFR |= GPIO_32_768kHz;
	GPDR |= GPIO_32_768kHz;
	TUCR  = TUCR_32_768kHz;

	/* MCP ExtClk */
	GAFR |= GPIO_MCP_CLK;  
	GPDR &= ~GPIO_MCP_CLK;
#endif

	LCM_ASD  = (6+8+320+30)-10;/* synchronize lccr1 setting : colliefb.c */
	LCM_ASD |= 0x8000;
	LCM_HSD  = (6+8+320+30)-10-128+4;
	LCM_HSD |= 0x8000;
	LCM_HSC  = 128/8;

	LCM_TADC = 0x80;   /* XON */
	udelay(1000);
	LCM_TADC |= 0x10;  /* CLK9MEN */
	udelay(100);

	LCM_DAC |= (LCM_DAC_SCLOEB | LCM_DAC_SDAOEB); /* init DAC */


  /* scoop initialize */
	scoop_init();

	/* initialize SSP & CS */
	pxa_ssp_init();

	return 0;
}

static void __init poodle_init_irq(void)
{
	int irq;
	
	pxa_init_irq();

	/* setup extra poodle irqs */
	LCM_ICR = 0;
	for (irq = LCM_IRQ(0); irq <= LCM_IRQ(3); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= lcm_mask_and_ack_irq;
		irq_desc[irq].mask	= lcm_mask_irq;
		irq_desc[irq].unmask	= lcm_unmask_irq;
	}
	GPDR(GPIO_GA_INT) &= ~GPIO_bit(GPIO_GA_INT);
        set_GPIO_IRQ_edge( GPIO_GA_INT, GPIO_FALLING_EDGE );
	setup_arm_irq( IRQ_GPIO_GA_INT, &lcm_irq );

	for (irq = LCM_IRQ_KEY_OFFSET(0);
			irq <= LCM_IRQ_KEY_OFFSET(0); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= lcm_key_mask_and_ack_irq;
		irq_desc[irq].mask	= lcm_key_mask_irq;
		irq_desc[irq].unmask	= lcm_key_unmask_irq;
	}
	setup_arm_irq( LCM_IRQ_KEY_BASE, &lcm_key_irq );

	for (irq = LCM_IRQ_GPIO_OFFSET(0);
			irq <= LCM_IRQ_GPIO_OFFSET(15); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= lcm_gpio_mask_and_ack_irq;
		irq_desc[irq].mask	= lcm_gpio_mask_irq;
		irq_desc[irq].unmask	= lcm_gpio_unmask_irq;
	}
	setup_arm_irq( LCM_IRQ_GPIO_BASE, &lcm_gpio_irq );

	for (irq = LCM_IRQ_LT_OFFSET(0);
			irq <= LCM_IRQ_LT_OFFSET(0); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= lcm_lt_mask_and_ack_irq;
		irq_desc[irq].mask	= lcm_lt_mask_irq;
		irq_desc[irq].unmask	= lcm_lt_unmask_irq;
	}
	setup_arm_irq( LCM_IRQ_LT_BASE, &lcm_lt_irq );

	for (irq = LCM_IRQ_SPI_OFFSET(0);
			irq <= LCM_IRQ_SPI_OFFSET(3); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= lcm_spi_mask_and_ack_irq;
		irq_desc[irq].mask	= lcm_spi_mask_irq;
		irq_desc[irq].unmask	= lcm_spi_unmask_irq;
	}
	setup_arm_irq( LCM_IRQ_SPI_BASE, &lcm_spi_irq );

	poodle_hw_init();
}

static int __init poodle_init(void)
{
  extern u32 sharpsl_emergency_off;

  // enable batt_fault
  PMCR = 0x01;

  /* charge check */
  if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) != 0) {
    sharpsl_charge_start();
  }

	printk("RCSR = %d\n",RCSR);


#ifdef CONFIG_PM
  /* fatal check */
#ifndef CONFIG_POODLE_TR0
	if ( !(GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) ) {
	  printk("poodle.c : main batt low\n");
	  sharpsl_emergency_off = 1;
	  pxa_suspend();
	}
#endif
#if 0 // move to boot
        if ( !chkFatalBatt() ) {
	  printk("poodle.c : chk fatal batt\n");
	  sharpsl_emergency_off = 1;
	  pxa_suspend();
	}
#endif
	if ( RCSR == 0x01 || RCSR == 0x6) {
	  printk("full reset !\n");
	  sharpsl_emergency_off = 1;
	  //pxa_suspend();
	}

	ga_pm_dev = pm_register(PM_SYS_DEV, 0, ga_pm_callback);
#endif

	return 0;
}

__initcall(poodle_init);

static void __init
fixup_poodle(struct machine_desc *desc, struct param_struct *params,
		char **cmdline, struct meminfo *mi)
{
	SET_BANK (0, 0xa0000000, 32*1024*1024);
	mi->nr_banks      = 1;
#if defined(CONFIG_BLK_DEV_INITRD)
	setup_ramdisk (1, 0, 0, 8192);
	setup_initrd (__phys_to_virt(0xa1000000), 4*1024*1024);
	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
#elif defined(CONFIG_MTD)
	ROOT_DEV = MKDEV(31, 0);	/* /dev/mtdblock0 */
#endif

#ifdef CONFIG_SHARPSL_BOOTLDR_PARAMS
	if (params->u1.s.page_size != PAGE_SIZE) {
	    params->u1.s.page_size = PAGE_SIZE;
	    params->u1.s.nr_pages = 32 * 1024 * 1024 / PAGE_SIZE;
	    params->u1.s.ramdisk_size = 0;
	    params->u1.s.flags = FLAG_READONLY | FLAG_RDLOAD | FLAG_RDPROMPT;
	    params->u1.s.rootdev = ROOT_DEV;
	    params->u1.s.initrd_start = 0;
	    params->u1.s.initrd_size = 0;
	    params->u1.s.rd_start = 0;
	    params->u1.s.system_rev = 0;
	    params->u1.s.system_serial_low = 0;
	    params->u1.s.system_serial_high = 0;
	    strcpy(params->commandline, CONFIG_CMDLINE);
	}
#endif

  sharpsl_get_param();
}

static struct map_desc poodle_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xf1000000, 0x10000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* LOCOMO */
  { 0xf2000000, 0x10800000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* SCOOP */
  { 0xf2100000, 0x0C000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* Nand Flash */
  { 0xef000000, 0x00000000, 0x00800000, DOMAIN_IO, 1, 1, 1, 0 }, /* Boot Flash */
  LAST_DESC
};

static void __init poodle_map_io(void)
{
	pxa_map_io();
	iotable_init(poodle_io_desc);

#if 0
	/* This enables the BTUART */
	CKEN |= CKEN7_BTUART;
	set_GPIO_mode(GPIO42_BTRXD_MD);
	set_GPIO_mode(GPIO43_BTTXD_MD);
	set_GPIO_mode(GPIO44_BTCTS_MD);
	set_GPIO_mode(GPIO45_BTRTS_MD);
#endif

	/* setup sleep mode values */
	PWER  = 0x00000002;
	PFER  = 0x00000000;
	PRER  = 0x00000002;
	PGSR0 = 0x00008000;
	PGSR1 = 0x003F0202;
	PGSR2 = 0x0001C000;
	PCFR |= PCFR_OPDE;
}

MACHINE_START(POODLE, "SHARP Poodle")
	MAINTAINER("Lineo Inc.")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
#ifdef CONFIG_SHARPSL_BOOTLDR_PARAMS
	BOOT_PARAMS(0xa0000100)
#endif
	FIXUP(fixup_poodle)
	MAPIO(poodle_map_io)
	INITIRQ(poodle_init_irq)
MACHINE_END
