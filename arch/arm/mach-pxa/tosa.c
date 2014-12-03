/*
 * arch/arm/mach-pxa/tosa.c
 *
 *  Support for the SHARP Tosa Board.
 *  
 *  (C) Copyright 2004 Lineo Solutions, Inc.
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
 *  26-Dec-2003 Sharp Corporation for Tosa
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/irq.h>

#include "generic.h"
#include <linux/ac97_codec.h>
#include <linux/pm.h>
#ifdef CONFIG_PM
static struct pm_dev *ga_pm_dev;
extern int pxa_suspend(void);
#endif

extern void sharpsl_get_param(void);
extern void sharpsl_charge_start(void);
//extern unsigned short chkFatalBatt(void);
extern void pxa_nssp_init(void);
extern void tosa_ac97_init(void);
extern void i2c_init(int reset);

unsigned short reset_scoop_gpio(unsigned short);

static void tc6393_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
  int req, i;
  
  while ( (req = (TC6393_SYS_REG(TC6393_SYS_ISR)
			& ~(TC6393_SYS_REG(TC6393_SYS_IMR)))) ) {
    for (i = 0; i <= 7; i++) {
      if ((req & (0x0001<<i))) {
        switch (TC6393_IRQ(i)) {
	  case TC6393_IRQ_USBINT:
#if 0
	    if (!(TC6393_USB_REG(TC6393_USB_SVPMCS) & TC6393_USB_PM_PMES))
	      continue;
#endif
	    break;
	}
	do_IRQ( TC6393_IRQ(i) , regs );
      }
    }
    break;
  }
}
						
static struct irqaction tc6393_irq = {
  name:           "TC6393",
  handler:        tc6393_IRQ_demux,
  flags:          SA_INTERRUPT
};

static void tc6393_mask_and_ack_irq(unsigned int irq)
{
  TC6393_SYS_REG(TC6393_SYS_IMR) |= (0x0001 << (irq - TC6393_IRQ(0)));
  switch (irq) {
    case TC6393_IRQ_USBINT:
      TC6393_USB_REG(TC6393_USB_SVPMCS) |= TC6393_USB_PM_PMES;
      break;
  }
}
		
static void tc6393_mask_irq(unsigned int irq)
{
  TC6393_SYS_REG(TC6393_SYS_IMR) |= (0x0001 << (irq - TC6393_IRQ(0)));
}
		
static void tc6393_unmask_irq(unsigned int irq)
{
  TC6393_SYS_REG(TC6393_SYS_IMR) &= ~(0x0001 << (irq - TC6393_IRQ(0)));
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

static spinlock_t scoop_lock = SPIN_LOCK_UNLOCKED;

unsigned short set_scoop_gpio(unsigned short bit)
{
	unsigned short gpio_bit;
	unsigned long flag;

	spin_lock_irqsave(&scoop_lock, flag);
	gpio_bit = SCP_REG_GPWR | bit;
	SCP_REG_GPWR = gpio_bit;
	spin_unlock_irqrestore(&scoop_lock, flag);

	return gpio_bit;
}
EXPORT_SYMBOL(set_scoop_gpio);

unsigned short reset_scoop_gpio(unsigned short bit)
{
	unsigned short gpio_bit;
	unsigned long flag;

	spin_lock_irqsave(&scoop_lock, flag);
	gpio_bit = SCP_REG_GPWR & ~bit;
	SCP_REG_GPWR = gpio_bit;
	spin_unlock_irqrestore(&scoop_lock, flag);

	return gpio_bit;
}
EXPORT_SYMBOL(reset_scoop_gpio);


static void __init scoop2_init(void)
{
  static const unsigned long scp2_init[] =
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
    SCP_INIT_DATA(SCP_GPCR,SCP2_IO_DIR),  // 20
    SCP_INIT_DATA(SCP_GPWR,SCP2_IO_OUT),  // 24
    SCP_INIT_DATA_END
  };
  int i;
  for(i=0; scp2_init[i] != SCP_INIT_DATA_END; i++) {
    int adr = scp2_init[i] >> 16;
    SCP2_REG(adr) = scp2_init[i] & 0xFFFF;
  }

  reset_scoop_gpio(SCP_IR_POWERDWN);
}

static spinlock_t scoop2_lock = SPIN_LOCK_UNLOCKED;

unsigned short set_scoop2_gpio(unsigned short bit)
{
  unsigned short gpio_bit;
  unsigned long flag;
		
  spin_lock_irqsave(&scoop2_lock, flag);
  gpio_bit = SCP2_REG_GPWR | bit;
  SCP2_REG_GPWR = gpio_bit;
  spin_unlock_irqrestore(&scoop2_lock, flag);

  return gpio_bit;
}
EXPORT_SYMBOL(set_scoop2_gpio);

unsigned short reset_scoop2_gpio(unsigned short bit)
{
  unsigned short gpio_bit;
  unsigned long flag;  		

  spin_lock_irqsave(&scoop2_lock, flag);
  gpio_bit = SCP2_REG_GPWR & ~bit;
  SCP2_REG_GPWR = gpio_bit;
  spin_unlock_irqrestore(&scoop2_lock, flag);

  return gpio_bit;
}
EXPORT_SYMBOL(reset_scoop2_gpio);
		
static void tc6393_init(void)
{
  reset_scoop2_gpio(SCP2_TC3693_L3V_ON);
  reset_scoop2_gpio(SCP2_TC6393_SUSPEND);
  reset_scoop_gpio(SCP_TC6393_REST_IN);
  set_GPIO_mode(GPIO11_3_6MHz_MD);
  set_GPIO_mode(GPIO18_RDY_MD);
  mdelay(1);
  set_scoop2_gpio(SCP2_TC6393_SUSPEND);
  mdelay(10);
  set_scoop_gpio(SCP_TC6393_REST_IN);
  set_scoop2_gpio(SCP2_TC3693_L3V_ON);

  printk("init TC6369 Revision %d\n", TC6393_SYS_REG(TC6393_SYS_RIDR));
  TC6393_SYS_REG(TC6393_SYS_FER) = 0;

  /* clock setting */
  TC6393_SYS_REG(TC6393_SYS_PLL2CR) = 0x0cc1;
  //TC6393_SYS_REG(TC6393_SYS_ConfigCR) = 0x1;
  //TC6393_SYS_REG(TC6393_SYS_PLL1CR1) = 0xdf00;
  //TC6393_SYS_REG(TC6393_SYS_PLL1CR2) = 0x002c;
  //TC6393_SYS_REG(TC6393_SYS_ConfigCR) = 0x0;
  TC6393_SYS_REG(TC6393_SYS_CCR) = 0x1310;

  TC6393_SYS_REG(TC6393_SYS_MCR) = 0x80AA;

  				/* GPIO */
  	TC6393_SYS_REG(TC6393_SYS_GPER) = 0x3300;       /* 15-0 GPO */
//	TC6393_SYS_REG(TC6393_SYS_GPOOECR1) = TC6393_GPO_OE;
//  	TC6393_SYS_REG(TC6393_SYS_GPODSR1) = TC6393_CARD_VCC_ON;/* 15-0 GPO set */
  	TC6393_SYS_REG(TC6393_SYS_GPODSR1) = TC6393_CARD_VCC_ON | TC6393_CHARGE_OFF_JC;/* 15-0 GPO set */
	TC6393_SYS_REG(TC6393_SYS_GPOOECR1) = TC6393_GPO_OE;

}

void tc6393_resume(void)
{
  tc6393_init();
}
EXPORT_SYMBOL(tc6393_resume);

void tc6393_suspend(void)
{
  reset_scoop2_gpio(SCP2_TC3693_L3V_ON);
  reset_scoop_gpio(SCP_TC6393_REST_IN);
  reset_scoop2_gpio(SCP2_TC6393_SUSPEND);
  set_GPIO_mode(GPIO11_3_6MHz|GPIO_OUT);
  GPSR0 = GPIO_bit(GPIO11_3_6MHz);
}
EXPORT_SYMBOL(tc6393_suspend);

#ifdef CONFIG_PM
static int ga_pm_callback(struct pm_dev* pm_dev, pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
	    break;
	case PM_RESUME:
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

static int __init tosa_hw_init(void)
{

  	/* scoop initialize */
	scoop_init();
	scoop2_init();

	/* initialize I2C */
	i2c_init(1);

	/* TC6393 initialize */
	tc6393_init();

	/* initialize SSP & CS */
	pxa_nssp_init();
	
	/* initialize AC97 */
	tosa_ac97_init();

	return 0;
}

static void __init tosa_init_irq(void)
{
	int irq;

	pxa_init_irq();

	/* setup extra tosa irqs */
	TC6393_SYS_REG(TC6393_SYS_IRR) = 0;
 	TC6393_SYS_REG(TC6393_SYS_IMR) = 0xbf;
	for (irq = TC6393_IRQ(0); irq <= TC6393_IRQ(7); irq++) {
	 	irq_desc[irq].valid     = 1;
		irq_desc[irq].probe_ok  = 1;
		irq_desc[irq].mask_ack  = tc6393_mask_and_ack_irq;
		irq_desc[irq].mask      = tc6393_mask_irq;
		irq_desc[irq].unmask    = tc6393_unmask_irq;
	}
	GPDR(GPIO_TC6393_INT) &= ~GPIO_bit(GPIO_TC6393_INT);
	set_GPIO_IRQ_edge( GPIO_TC6393_INT, GPIO_FALLING_EDGE );
	setup_arm_irq( IRQ_GPIO_TC6393_INT, &tc6393_irq );

	tosa_hw_init();
}

static int __init tosa_init(void)
{
#ifdef CONFIG_PM
  extern u32 sharpsl_emergency_off;
#endif

  // enable batt_fault
  PMCR = 0x01;

  /* charge check */
  if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) == 0) {
    sharpsl_charge_start();
  }

  //sharpsl_kick_jacket_check_queue();

  printk("RCSR = %d\n",RCSR);

#ifdef CONFIG_PM
  /* fatal check */
#ifndef CONFIG_ARCH_PXA_TOSA_SKIP
	if ( !(GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) ) {
		printk("corgi.c : main batt low\n");
		sharpsl_emergency_off = 1;
		pxa_suspend();
	}
#endif	/*  CONFIG_ARCH_PXA_TOSA_SKIP  */

	if ( RCSR == 0x01 || RCSR == 0x6) {
		printk("full reset !\n");
		sharpsl_emergency_off = 1;
	}

	ga_pm_dev = pm_register(PM_SYS_DEV, 0, ga_pm_callback);
#endif

	return 0;
}

__initcall(tosa_init);

static void __init fixup_tosa(struct machine_desc *desc,
		struct param_struct *params,
		char **cmdline, struct meminfo *mi)
{
	SET_BANK (0, 0xa0000000, 64*1024*1024);
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

static struct map_desc tosa_io_desc[] __initdata = {
  /* virtual     physical    length      domain     r  w  c  b */
  /* TC6393 (LCDC, USBC, NANDC) */
  { 0xf1000000, TOSA_LCDC_PHYS, 0x00400000, DOMAIN_IO, 1, 1, 0, 0 },
  /* SCOOP2 for internel CF */
  { 0xf2000000, TOSA_CF_PHYS, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 },
  /* SCOOP2 for Jacket CF */
  { 0xf2200000, TOSA_SCOOP_PHYS, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 },
  /* Nor Flash */
  { 0xef000000, 0x00000000, 0x00800000, DOMAIN_IO, 1, 1, 1, 0 },
  LAST_DESC
};

static void __init tosa_map_io(void)
{
	pxa_map_io();
	iotable_init(tosa_io_desc);

	set_GPIO_mode(GPIO_ON_RESET | GPIO_IN);

	/* setup sleep mode values */
	PWER  = 0x00000002;
	PFER  = 0x00000000;
	PRER  = 0x00000002;
	PGSR0 = 0x00000000;
	PGSR1 = 0x00FF0002;
	PGSR2 = 0x00014000;
	PCFR |= PCFR_OPDE;
}

MACHINE_START(TOSA, "SHARP Tosa")
	MAINTAINER("Lineo uSolutions, Inc.")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
#ifdef CONFIG_SHARPSL_BOOTLDR_PARAMS
	BOOT_PARAMS(0xa0000100)
#endif
	FIXUP(fixup_tosa)
	MAPIO(tosa_map_io)
	INITIRQ(tosa_init_irq)
MACHINE_END

