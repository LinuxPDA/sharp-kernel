/*
 * linux/arch/arm/mach-sa1100/collie.c
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * This file contains all Collie-specific tweaks.
 *
 * Based on:
 * linux/arch/arm/mach-sa1100/assabet.c
 *
 * Author: Nicolas Pitre
 *
 * This file contains all Assabet-specific tweaks.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * ChangeLog:
 *  04-16-2001 Lineo Japan,Inc. ...
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/irq.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "generic.h"

#include <linux/pm.h>
#ifdef CONFIG_PM
static struct pm_dev *ga_pm_dev;
#endif

#include <asm/ucb1200.h>
#include <asm/arch/tc35143.h>


extern int ucb1200_init(void);
extern int ucb1200_adc_exc_init(void);
extern int collie_get_on_mode;


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

static void __init lcm_init_irq(void)
{
	int irq;

	for (irq = LCM_IRQ(0); irq <= LCM_IRQ(3); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= lcm_mask_and_ack_irq;
		irq_desc[irq].mask	= lcm_mask_irq;
		irq_desc[irq].unmask	= lcm_unmask_irq;
	}
	GPDR &= ~GPIO_GA_INT;
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
}


static void __init scoop_init()
{

#ifdef CONFIG_COLLIE_TS
	/* CF_BUS_OFF */
	cf_buf_ctrl = CF_BUS_POWER_OFF;
	CF_BUF_CTRL = cf_buf_ctrl;

#else	/* ! CONFIG_COLLIE_TS */
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
#endif

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
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
		LCM_ASD  = (6+8+320+30)-10;/* synchronize lccr1 setting : colliefb.c */
		LCM_ASD |= 0x8000;
		LCM_HSD  = (6+8+320+30)-10-128+4;
		LCM_HSD |= 0x8000;
		LCM_HSC  = 128/8;
#else	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
		LCM_ASD  = (6+8+320+30)-10;/* synchronize lccr1 setting : colliefb.c */
		LCM_ASD |= 0x8000;
		LCM_HSD  = 0;
		LCM_HSD |= 0x8000;
		LCM_HSC  = 0;
#endif	/* end CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */

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

static int __init collie_init(void)
{
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


  /* locomo initialize */
	LCM_ICR = 0;
	/* KEYBOARD */
	LCM_KIC = 0;
	/* GPIO */
	LCM_GPO = 0;
	LCM_GPE = ( LCM_GPIO(2) | LCM_GPIO(3) | LCM_GPIO(13) | LCM_GPIO(14) );
	LCM_GPD = ( LCM_GPIO(2) | LCM_GPIO(3) | LCM_GPIO(13) | LCM_GPIO(14) );
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

	GAFR |= GPIO_32_768kHz;
	GPDR |= GPIO_32_768kHz;
	TUCR  = TUCR_32_768kHz;

	/* MCP ExtClk */
	GAFR |= GPIO_MCP_CLK;  
	GPDR &= ~GPIO_MCP_CLK;

#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
	LCM_ASD  = (6+8+320+30)-10;/* synchronize lccr1 setting : colliefb.c */
	LCM_ASD |= 0x8000;
	LCM_HSD  = (6+8+320+30)-10-128+4;
	LCM_HSD |= 0x8000;
	LCM_HSC  = 128/8;
#else	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
	LCM_ASD  = (6+8+320+30)-10;/* synchronize lccr1 setting : colliefb.c */
	LCM_ASD |= 0x8000;
	LCM_HSD  = 0;
	LCM_HSD |= 0x8000;
	LCM_HSC  = 0;
#endif	/* end CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */

	LCM_TADC = 0x80;   /* XON */
	udelay(1000);
	LCM_TADC |= 0x10;  /* CLK9MEN */
	udelay(100);

	LCM_DAC |= (LCM_DAC_SCLOEB | LCM_DAC_SDAOEB); /* init DAC */


	lcm_init_irq();

  /* tc35143f initialize */
	ucb1200_init();
	ucb1200_adc_exc_init();

	ucb1200_set_io_direction(TC35143_GPIO_VERSION0,TC35143_IODIR_INPUT );
	ucb1200_set_io_direction(TC35143_GPIO_TBL_CHK, TC35143_IODIR_OUTPUT);

#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0) || \
    defined(CONFIG_COLLIE_TR1) || defined(CONFIG_COLLIE_DEV)
	ucb1200_set_io_direction(TC35143_GPIO_VPEN_ON, TC35143_IODIR_OUTPUT);
#else
	ucb1200_set_io_direction(TC35143_GPIO_VPEN_ON, TC35143_IODIR_INPUT);
#endif
	ucb1200_set_io_direction(TC35143_GPIO_IR_ON,   TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_AMP_ON,  TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_VERSION1,TC35143_IODIR_INPUT );
	ucb1200_set_io_direction(TC35143_GPIO_CHRG_ON, TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_MBAT_ON, TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_BBAT_ON, TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_TMP_ON,  TC35143_IODIR_OUTPUT);

  /* scoop initialize */
	scoop_init();

  /* charge check */
	if ((GPLR & GPIO_AC_IN) != 0) {
	  suspend_collie_charge_on();
	}


	printk("RCSR = %d\n",RCSR);
	collie_get_on_mode = RCSR;


#ifndef CONFIG_COLLIE_TR0
  /* fatal check */
	if ( !(GPLR & GPIO_MAIN_BAT_LOW) ) {
	  printk("collie.c : main batt low\n");
	  sa1110_suspend();
	}
        if ( !chkFatalBatt() ) {
	  printk("collie.c : chk fatal batt\n");
	  sa1110_suspend();
	}

	if ( RCSR == 0x01 ) {
	  printk("full reset !\n");
	  sa1110_suspend();
	}
#endif

#ifdef CONFIG_PM
	ga_pm_dev = pm_register(PM_SYS_DEV, 0, ga_pm_callback);
#endif

	return 0;
}

__initcall(collie_init);

static void __init fixup_collie(struct machine_desc* desc,
				struct param_struct* params,
				char** cmdline,
				struct meminfo* mi)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0) || \
    defined(CONFIG_COLLIE_TR1) || defined(CONFIG_COLLIE_DEV)
	SET_BANK( 0, 0xc0000000, 32*1024*1024 );
#else
	SET_BANK( 0, 0xc0000000, 64*1024*1024 );
#endif
	mi->nr_banks = 1;

	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	setup_ramdisk( 1, 0, 0, 8192 );
	setup_initrd( 0xc0800000, 3*1024*1024 );
}

static struct map_desc collie_io_desc[] __initdata = {
#ifdef CONFIG_COLLIE_TS
#ifdef CONFIG_XIP_ROM
{ 0xe8000000, 0x08000000, 0x02000000, DOMAIN_IO, 1, 1, 1, 0 }, /* 32M main flash (cs1) */
#else
{ 0xe8000000, 0x08000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* 32M main flash (cs1) */
#endif
{ 0xea000000, 0x00000000, 0x02000000, DOMAIN_IO, 1, 1, 1, 0 }, /* 32M boot flash (cs0) */
{ 0xec000000, 0x10000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* 32M telios flash (cs2) */
{ 0xf2000000, 0x18000000, 0x00001000, DOMAIN_IO, 1, 1, 0, 0 }, /* 4kb debug LED (cs3) */
{ 0xf3000000, 0x48000000, 0x00001000, DOMAIN_IO, 1, 1, 0, 0 }, /* 4kb CF buf ctrl (cs5) */
#else
{ 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* 32M main flash (cs0) */
{ 0xea000000, 0x08000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* 32M boot flash (cs1) */
{ 0xf2000000, 0xC2000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* 32M Special Case     */
#endif
{ 0xf0000000, 0x40000000, 0x01000000, DOMAIN_IO, 1, 1, 0, 0 }, /* 16M LOCOMO  & SCOOP (cs4) */
  LAST_DESC
};

static void __init collie_map_io(void)
{
	sa1100_map_io();
	iotable_init(collie_io_desc);
}

MACHINE_START(COLLIE, "Sharp-Collie")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	FIXUP(fixup_collie)
	MAPIO(collie_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
