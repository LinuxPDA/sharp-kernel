/*
 * linux/arch/arm/mach-pxa/corgi.c
 *
 *  Support for the SHARP Corgi Board.
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
 *  12-Dec-2002 Sharp Corporation for Corgi
 *  01-Apr-2003 Sharp for Shepherd
 *  29-Sep-2004 Lineo Solutions, Inc.  for Spitz
 *  28-Feb-2005 Sharp Corporation for Akita
 *  05-Apr-2005 Sharp Corporation for Borzoi
 *  16-Jan-2006 Sharp Corporation for Terrier
 *
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

#include <asm/arch/irq.h>

#include "generic.h"


#include <linux/pm.h>
#ifdef CONFIG_PM
static struct pm_dev *ga_pm_dev;
#endif

extern void sharpsl_charge_start(void);
extern unsigned short chkFatalBatt(void);
extern int pxa_suspend(void);
extern void pxa_ssp_init(void);
extern void sharpsl_get_param(void);

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

#if defined(CONFIG_ARCH_PXA_AKITA)
#include <asm/arch/poodle_i2sc.h>

static unsigned char ioexp_output_value = 0;
static unsigned char ioexp_config_value = IOEXP_ALL;

/*
 I/O-0
 I/O-1
 I/O-2 PA19 MICON
 I/O-3 PA18 BL_ON
 I/O-4 PA17 BLCONT
 I/O-5 PA12 AKENB
 I/O-6 PA11 IR_ON_B
 I/O-7
*/
#define I2C_OPEN_WAIT 10
static int i2c_open_retry(unsigned char address)
{
  int err = 0,cnt = 0;
  int start_time;

  if ( in_interrupt() ) {
    err = i2c_open( address );
    printk("%s: call from interrupt!(%d)\n",__func__,err);
    return err;
  }

  while(1) {
    err = i2c_open( address );
    if ( !err ) break;
    start_time = jiffies;
    printk("%s: schedule!\n",__func__);
    while (1) {
      schedule();
      if ( start_time - jiffies > 2 ) break;
    }
    cnt++;
    if ( cnt > I2C_OPEN_WAIT ) {
      printk("%s: schedule timeout(%d)!\n",__func__,err);
      return err;
    }
  }
  //printk("%s: success(%d)!\n",__func__,err);
  return err;
}

static void __init ioexp_init(void)
{
  ioexp_output_value = IOEXP_IO_OUT;
  ioexp_config_value = IOEXP_IO_DIR;

  i2c_open( IOEXP_DEVICE_ADR );
  i2c_byte_write( IOEXP_DEVICE_ADR, IOEXP_POLINV_REG_ADR, 0 );
  i2c_byte_write( IOEXP_DEVICE_ADR, IOEXP_OUTPUT_REG_ADR, ioexp_output_value );
  i2c_byte_write( IOEXP_DEVICE_ADR, IOEXP_CONFIG_REG_ADR, ioexp_config_value );
  i2c_close( IOEXP_DEVICE_ADR );
}

unsigned char set_port_ioexp(unsigned char bit)
{
  if(i2c_open_retry( IOEXP_DEVICE_ADR )){
    printk("%s: i2c open error!\n",__func__);
    return ioexp_output_value;
  }

  ioexp_output_value |= bit;
  if(i2c_byte_write( IOEXP_DEVICE_ADR, 
		     IOEXP_OUTPUT_REG_ADR, ioexp_output_value))
    printk("%s: i2c write error!\n",__func__);

  i2c_close( IOEXP_DEVICE_ADR );
  return ioexp_output_value;
}

unsigned char reset_port_ioexp(unsigned char bit)
{
  if(i2c_open_retry( IOEXP_DEVICE_ADR )){
    printk("%s: i2c open error!\n",__func__);
    return ioexp_output_value;
  }

  ioexp_output_value &= ~bit;
  if(i2c_byte_write( IOEXP_DEVICE_ADR, 
		     IOEXP_OUTPUT_REG_ADR, ioexp_output_value))
    printk("%s: i2c write error!\n",__func__);

  i2c_close( IOEXP_DEVICE_ADR );
  return ioexp_output_value;
}

unsigned char get_port_ioexp(void)
{
  return ioexp_output_value;
}

unsigned char set_input_ioexp(unsigned char bit)
{
  if(i2c_open_retry( IOEXP_DEVICE_ADR )){
    printk("%s: i2c open error!\n",__func__);
    return ioexp_config_value;
  }

  ioexp_config_value |= bit;
  if(i2c_byte_write( IOEXP_DEVICE_ADR, 
		     IOEXP_CONFIG_REG_ADR, ioexp_config_value))
    printk("%s: i2c write error!\n",__func__);

  i2c_close( IOEXP_DEVICE_ADR );
  return ioexp_config_value;
}

unsigned char set_output_ioexp(unsigned char bit)
{
  if(i2c_open_retry( IOEXP_DEVICE_ADR )){
    printk("%s: i2c open error!\n",__func__);
    return ioexp_config_value;
  }

  ioexp_config_value &= ~bit;
  if(i2c_byte_write( IOEXP_DEVICE_ADR, 
		     IOEXP_CONFIG_REG_ADR, ioexp_config_value))
    printk("%s: i2c write error!\n",__func__);

  i2c_close( IOEXP_DEVICE_ADR );
  return ioexp_config_value;
}

#elif defined(CONFIG_ARCH_PXA_SPITZ)
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

//  reset_scoop_gpio(SCP_IR_POWERDWN);
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
		
#endif

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

#if defined(CONFIG_ARCH_PXA_TERRIER)
	MSC1 |= (0x8 << 16); 
#endif
	
	CKEN |= 0x03; 
	CKEN |= CKEN3_SSP;
	CKEN |= CKEN1_PWM1;


}

#if defined(CONFIG_CPU_PXA27X)

void usbh_interrupt_init(void)
{
	unsigned long cken;

	cken = CKEN;
	CKEN |= CKEN10_USB;
	UHCHIE  = 0x00000000;
	UHCINTD = 0xffffffff;
	UHCINTS = 0xffffffff;
	UHCSTAT = 0xffffffff;
	CKEN = cken;
}

#endif

static int __init corgi_hw_init(void)
{
#if 0	/////////////////////////////
  /* cpu initialize */
        // Alternate Register
	GAFR = ( GPIO_LDD8 | GPIO_LDD9 | GPIO_LDD10 | GPIO_LDD11 | GPIO_LDD12 | \
		 GPIO_LDD13 | GPIO_LDD14 | GPIO_LDD15 | GPIO_SSP_TXD | \
		 GPIO_SSP_SCLK | GPIO_SSP_SFRM | GPIO_SSP_CLK | GPIO_TIC_ACK | \
		 GPIO_32_768kHz );

	// Direction Register
	GPDR = ( GPIO_LDD8 | GPIO_LDD9 | GPIO_LDD10 | GPIO_LDD11 | GPIO_LDD12 | \
		 GPIO_LDD13 | GPIO_LDD14 | GPIO_LDD15 | GPIO_SSP_TXD | \
		 GPIO_SSP_SCLK | GPIO_SSP_SFRM | GPIO_SDLC_SCLK | \
		 GPIO_SDLC_AAF | GPIO_UART_SCLK1 | GPIO_32_768kHz );
	// Output Register
	GPLR = GPIO_GPIO18;

	// PPC pin setting
	PPDR = ( PPC_LDD0 | PPC_LDD1 | PPC_LDD2 | PPC_LDD3 | PPC_LDD4 | PPC_LDD5 | \
		 PPC_LDD6 | PPC_LDD7 | PPC_L_PCLK | PPC_L_LCLK | PPC_L_FCLK | PPC_L_BIAS | \
	 	 PPC_TXD1 | PPC_TXD2 | PPC_RXD2 | PPC_TXD3 | PPC_TXD4 | PPC_SCLK | PPC_SFRM );

	PPSR = ( PPC_RXD2 | PPC_RXD3 );

	PSDR = ( PPC_RXD1 | PPC_RXD2 | PPC_RXD3 | PPC_RXD4 );

#endif

#if 0 // for debug
	printk("GAFR0_L=%x\n",GAFR0_L);
	printk("GAFR0_U=%x\n",GAFR0_U);
	printk("GAFR1_L=%x\n",GAFR1_L);
	printk("GAFR1_U=%x\n",GAFR1_U);
	printk("GAFR2_L=%x\n",GAFR2_L);
	printk("GAFR2_U=%x\n",GAFR2_U);
	printk("GAFR3_L=%x\n",GAFR3_L);
	printk("GAFR3_U=%x\n",GAFR3_U);

	printk("GPDR0=%x\n",GPDR0);
	printk("GPDR1=%x\n",GPDR1);
	printk("GPDR2=%x\n",GPDR2);
	printk("GPDR3=%x\n",GPDR3);
#endif

#if defined(CONFIG_ARCH_PXA_TERRIER)
	MSC1 |= (0x8 << 16); 
#endif

  /* i2c initialize */
        i2c_init();

  /* scoop initialize */
	scoop_init();
#if defined(CONFIG_ARCH_PXA_AKITA)
	ioexp_init();
#elif defined(CONFIG_ARCH_PXA_SPITZ)
	scoop2_init();
#endif

	/* initialize SSP & CS */
	pxa_ssp_init();

#if defined(CONFIG_CPU_PXA27X)
	usbh_interrupt_init();
#endif

	return 0;
}

static void __init corgi_init_irq(void)
{
	//int irq;
	
	pxa_init_irq();

	/* setup extra corgi irqs */

	corgi_hw_init();
}

static int __init corgi_init(void)
{
  extern u32 sharpsl_emergency_off;


  // enable batt_fault
#if defined(CONFIG_CPU_PXA27X)
  PMCR = 0x00;
#else
  PMCR = 0x01;
#endif

  /* charge check */
  if (AC_IN_STATUS) {
    sharpsl_charge_start();
  }

	printk("RCSR = %d\n",RCSR);


#ifdef CONFIG_PM
  /* fatal check */
#if !defined(CONFIG_CORGI_TR0) && !defined(CONFIG_SPITZ_TR0)
	if ( !(GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) ) {
		printk("corgi.c : main batt low\n");
		sharpsl_emergency_off = 1;
		pxa_suspend();
	}
#if 0 // move to boot
        if ( !chkFatalBatt() ) {
		printk("corgi.c : chk fatal batt\n");
		sharpsl_emergency_off = 1;
		pxa_suspend();
	}
#endif
#endif

	if ( RCSR == 0x01 || RCSR == 0x6) {
		printk("full reset !\n");
		sharpsl_emergency_off = 1;
//		pxa_suspend();
	}

	ga_pm_dev = pm_register(PM_SYS_DEV, 0, ga_pm_callback);
#endif

	return 0;
}

__initcall(corgi_init);

static void __init
fixup_corgi(struct machine_desc *desc, struct param_struct *params,
		char **cmdline, struct meminfo *mi)
{
#if defined(CONFIG_ARCH_PXA_SHEPHERD)
	SET_BANK (0, 0xa0000000, 64*1024*1024);
#else
	SET_BANK (0, 0xa0000000, 32*1024*1024);
#endif
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

static struct map_desc corgi_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xf1000000, 0x08000000, 0x01000000, DOMAIN_IO, 1, 1, 0, 0 }, /* LCDC (readable for Qt driver) */
  { 0xf2000000, 0x10800000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* SCOOP */
  { 0xf2100000, 0x0C000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* Nand Flash */
#if defined(CONFIG_ARCH_PXA_SPITZ)
  { 0xf2200000, 0x08800000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* SCOOP2 */
#endif
  { 0xef000000, 0x00000000, 0x00800000, DOMAIN_IO, 1, 1, 1, 0 }, /* Boot Flash */
  LAST_DESC
};

static void __init corgi_map_io(void)
{
	pxa_map_io();
	iotable_init(corgi_io_desc);

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
	PGSR0 = 0x0158C000;
	PGSR1 = 0x00FF0080;
	PGSR2 = 0x0001C004;
	PCFR |= PCFR_OPDE;
}

#if defined(CONFIG_ARCH_PXA_TERRIER)
MACHINE_START(CORGI, "SHARP Terrier")
#elif defined(CONFIG_ARCH_PXA_BORZOI)
MACHINE_START(CORGI, "SHARP Borzoi")
#elif defined(CONFIG_ARCH_PXA_AKITA)
MACHINE_START(CORGI, "SHARP Akita")
#elif defined(CONFIG_ARCH_PXA_SPITZ)
MACHINE_START(CORGI, "SHARP Spitz")
#elif defined(CONFIG_ARCH_PXA_BOXER)
MACHINE_START(CORGI, "SHARP Boxer")
#elif defined(CONFIG_ARCH_PXA_HUSKY)
MACHINE_START(CORGI, "SHARP Husky")
#elif defined(CONFIG_ARCH_PXA_SHEPHERD)
MACHINE_START(CORGI, "SHARP Shepherd")
#else
MACHINE_START(CORGI, "SHARP Corgi")
#endif
	MAINTAINER("Lineo Inc.")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
#ifdef CONFIG_SHARPSL_BOOTLDR_PARAMS
	BOOT_PARAMS(0xa0000100)
#endif
	FIXUP(fixup_corgi)
	MAPIO(corgi_map_io)
	INITIRQ(corgi_init_irq)
MACHINE_END
