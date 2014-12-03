/*
 *  linux/drivers/char/sa1100_mcp.c
 *
 *  Copyright (C) 2000 G.Mate, Inc.
 *
 *  Author: Tred Lim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 19 December 2000	: created.
 * 14 November 2001     : modify for SL-5000D Sharp Corporation
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/ucb1200.h>



/* ---------- */

void ucb1200_sa1100_set_mccr0(u32 value)
{
	Ser4MCCR0 = value;
}

u32 ucb1200_sa1100_get_mccr0(void)
{
	return Ser4MCCR0;
}

u32 ucb1200_sa1100_get_mcsr(void)
{
	return Ser4MCSR;
}

void ucb1200_sa1100_set_mcsr(u32 value)
{
	Ser4MCSR = value;
}

u32 ucb1200_sa1100_get_mcdr0(void)
{
	return Ser4MCDR0;
}

void ucb1200_sa1100_set_mcdr0(u32 value)
{
	Ser4MCDR0 = value;
}

EXPORT_SYMBOL(ucb1200_sa1100_set_mccr0);
EXPORT_SYMBOL(ucb1200_sa1100_get_mccr0);
EXPORT_SYMBOL(ucb1200_sa1100_get_mcsr);
EXPORT_SYMBOL(ucb1200_sa1100_set_mcsr);
EXPORT_SYMBOL(ucb1200_sa1100_get_mcdr0);
EXPORT_SYMBOL(ucb1200_sa1100_set_mcdr0);

/* ---------- */

static void sa1100_mcp_enable(void)
{
	Ser4MCSR = 0xffffffff;
	Ser4MCCR0 |= MCCR0_MCE;
}

static void sa1100_mcp_disable(void)
{
#ifndef CONFIG_SA1100_COLLIE
	Ser4MCCR0 &= ~MCCR0_MCE;
#endif
}

static u16 sa1100_mcp_read_codec(u16 addr)
{
	Ser4MCDR2 = ((addr & 0xf) << FShft(MCDR2_ADD)) | MCDR2_Rd;
	while (!(Ser4MCSR & MCSR_CRC));
	return Ser4MCDR2 & 0xffff;
}

static void sa1100_mcp_write_codec(u16 addr, u16 data)
{
	Ser4MCDR2 = ((addr & 0xf) << FShft(MCDR2_ADD))
      		  | MCDR2_Wr | (data & 0xffff);
	/* We'd better wait here so that we can disable sib after writing. */
	while (!(Ser4MCSR & MCSR_CWC));
}

static int sa1100_ucb1200_init(struct ucb1200_arch_info *arch)
{
	int gpio, irq = NO_IRQ;
	unsigned long MCCR0 = 0;

	if (machine_is_assabet()) {
#ifdef CONFIG_SA1100_ASSABET
		BCR_set(BCR_CODEC_RST);
		MCCR0 = MCCR0_IntClk | MCCR0_SmpCnt;
		gpio = GPIO_UCB1300_IRQ;
		irq = IRQ_GPIO_UCB1300_IRQ;
		arch->flags |= UCB1200_ADC_SYNC;
#endif
	}
	else if (machine_is_collie()) {
#ifdef CONFIG_SA1100_COLLIE
		GAFR &= ~GPIO_UCB1200_RESET;
		GPDR |= GPIO_UCB1200_RESET;
		GPSR |= GPIO_UCB1200_RESET;
	  
		MCCR0 = MCCR0_ExtClk | MCCR0_SmpCnt;
		gpio = GPIO_UCB1200_IRQ;
		irq = IRQ_GPIO_UCB1200_IRQ;

		mdelay(500);
#if 1
		arch->flags |= UCB1200_ADC_SYNC;
#endif
#endif
	}
	else if (machine_is_yopy()) {
#ifdef CONFIG_SA1100_YOP
		GAFR &= ~GPIO_UCB1200_RESET;
		GPDR |= GPIO_UCB1200_RESET;
		GPSR |= GPIO_UCB1200_RESET;

		MCCR0 = MCCR0_IntClk | MCCR0_SmpCnt;
		gpio = GPIO_UCB1200_IRQ;
		irq = IRQ_GPIO_UCB1200_IRQ;
		arch->flags |= UCB1200_ADC_SYNC;
#endif
	}
	else if (machine_is_cerf()) {
#ifdef CONFIG_SA1100_CERF
		MCCR0 = MCCR0_IntClk | MCCR0_SmpCnt;
		gpio = GPIO_UCB1200_IRQ;
		irq = IRQ_GPIO_UCB1200_IRQ;
#endif
	}
	else if (machine_is_graphicsclient()) {
#ifdef CONFIG_SA1100_GRAPHICSCLIENT
		MCCR0 = 0;
		gpio = GPIO_GPIO22;
		irq = ADS_EXT_IRQ(8);
		arch->flags |= UCB1200_ADC_SYNC;
#endif
	}
	else if (machine_is_pfs168()) {
#ifdef CONFIG_SA1100_PFS168
		MCCR0 = MCCR0_IntClk | MCCR0_SmpCnt;
		gpio = GPIO_UCB1300_IRQ;
		irq = IRQ_GPIO_UCB1300_IRQ;
#endif
	}
#ifdef CONFIG_SA1100_FREEBIRD
	if (machine_is_freebird()) {
		BCR_set(BCR_FREEBIRD_CODEC_RST);
		MCCR0 = MCCR0_IntClk | MCCR0_SmpCnt;
		gpio = GPIO_FREEBIRD_UCB1300;
		irq = IRQ_GPIO_FREEBIRD_UCB1300_IRQ;
		arch->flags |= UCB1200_ADC_SYNC;
	}
#endif
#ifdef CONFIG_SA1100_FLEXANET
	else if (machine_is_flexanet()) {
		/* By now, all GUI boards have an UCB1300 */
		if (flexanet_GUI_type == FHH_GUI_NONE || flexanet_GUI_type == FHH_GUI_ERROR)
		  return -ENODEV;
		  
		gpio = GPIO_GUI_IRQ;
		irq = IRQ_GPIO_GUI;
		arch->flags |= UCB1200_ADC_SYNC;
		
		/* release the GUI reset */
		BCR_set(BCR_GUI_NRST);
	}
#endif

	if (irq != NO_IRQ) {
		GPDR &= ~gpio;
		set_GPIO_IRQ_edge(gpio, GPIO_BOTH_EDGES);

		/*
		 * Properly configure PPC lines. This seems to prevent
		 * unexpected codec writes when disabling MCP.
		 */
		PPDR &= ~PPC_RXD4;
		PPDR |= PPC_TXD4 | PPC_SCLK | PPC_SFRM;
		PSDR |= PPC_RXD4;
		PSDR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);
		PPSR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);

		/* initialize MCP */
		Ser4MCCR1 = 0;
		Ser4MCSR = 0xffffffff;
#if defined(CONFIG_SA1100_COLLIE)
		Ser4MCCR0 = MCCR0;
#else
		Ser4MCCR0 = 127 | (127 << 8) | MCCR0;
		if (arch->flags & UCB1200_ADC_SYNC) {
			/* Set ADCSYNC to be out with SIB_ZERO set */
			sa1100_mcp_enable();
			sa1100_mcp_write_codec(1, SIB_ZERO | 0x0200);
			sa1100_mcp_write_codec(0, 0x0200);
			sa1100_mcp_disable();
		}
#endif
		arch->irq = irq;
	}

	return 0;
}

static void sa1100_set_adc_sync(int flag)
{
#if !defined(CONFIG_SA1100_COLLIE)
	ucb1200_set_io(9, flag);
#endif
}

static void sa1100_ucb1200_exit(void)
{
	if (machine_is_yopy()) {
#ifdef CONFIG_SA1100_YOPY
	/*	Ser4MCCR0 &= ~MCCR0_MCE;*/
		GPCR |= GPIO_UCB1200_RESET;
#endif
	}
}

struct ucb1200_low_level arch_ucb1200_ops = {
	arch_init:	sa1100_ucb1200_init,
	arch_exit:	sa1100_ucb1200_exit,

	sib_enable:	sa1100_mcp_enable,
	sib_disable:	sa1100_mcp_disable,
	sib_read_codec:	sa1100_mcp_read_codec,
	sib_write_codec:sa1100_mcp_write_codec,

	set_adc_sync:	sa1100_set_adc_sync,
};

EXPORT_SYMBOL(arch_ucb1200_ops);
