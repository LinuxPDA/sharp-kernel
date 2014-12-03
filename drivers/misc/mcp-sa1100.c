/*
 *  linux/drivers/misc/mcp-sa1100.c
 *
 *  Copyright (C) 2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 *  SA1100 MCP (Multimedia Communications Port) driver.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>

#include <asm/dma.h>
#include <asm/hardware.h>
#include <asm/system.h>

#include "mcp.h"

static void
mcp_sa1100_set_telecom_divisor(struct mcp *mcp, unsigned int divisor)
{
	unsigned int mccr0;

	divisor /= 32;

	mccr0 = Ser4MCCR0 & ~0x00007f00;
	mccr0 |= divisor << 8;
	Ser4MCCR0 = mccr0;
}

static void
mcp_sa1100_set_audio_divisor(struct mcp *mcp, unsigned int divisor)
{
	unsigned int mccr0;

	divisor /= 32;

	mccr0 = Ser4MCCR0 & ~0x0000007f;
	mccr0 |= divisor;
	Ser4MCCR0 = mccr0;
}

static void
mcp_sa1100_write(struct mcp *mcp, unsigned int reg, unsigned int val)
{
	Ser4MCDR2 = reg << 17 | MCDR2_Wr | (val & 0xffff);
	while (!(Ser4MCSR & MCSR_CWC))
		barrier();
}

static unsigned int
mcp_sa1100_read(struct mcp *mcp, unsigned int reg)
{
	Ser4MCDR2 = reg << 17 | MCDR2_Rd;
	while (!(Ser4MCSR & MCSR_CRC))
		barrier();
	return Ser4MCDR2 & 0xffff;
}

static void mcp_sa1100_enable(struct mcp *mcp)
{
	Ser4MCSR = -1;
	Ser4MCCR0 |= MCCR0_MCE;
}

static void mcp_sa1100_disable(struct mcp *mcp)
{
	Ser4MCCR0 &= ~MCCR0_MCE;
}

struct mcp mcp_sa1100 = {
	lock:			SPIN_LOCK_UNLOCKED,
	div_rate:		11981000/32,
	dma_audio_rd:		DMA_Ser4MCP0Rd,
	dma_audio_wr:		DMA_Ser4MCP0Wr,
	dma_telco_rd:		DMA_Ser4MCP1Rd,
	dma_telco_wr:		DMA_Ser4MCP1Wr,
	set_telecom_divisor:	mcp_sa1100_set_telecom_divisor,
	set_audio_divisor:	mcp_sa1100_set_audio_divisor,
	reg_write:		mcp_sa1100_write,
	reg_read:		mcp_sa1100_read,
	enable:			mcp_sa1100_enable,
	disable:		mcp_sa1100_disable,
};

EXPORT_SYMBOL(mcp_sa1100);

static int mcp_sa1100_init(void)
{
	if (machine_is_adsbitsy()       || machine_is_assabet()        ||
	    machine_is_cerf()           || machine_is_flexanet()       ||
	    machine_is_freebird()       || machine_is_graphicsclient() ||
	    machine_is_graphicsmaster() || machine_is_lart()           ||
	    machine_is_pfs168()         || machine_is_simpad()         ||
	    machine_is_yopy()) {
		/*
		 * Setup the PPC unit correctly.
		 */
		PPDR &= ~PPC_RXD4;
		PPDR |= PPC_TXD4 | PPC_SCLK | PPC_SFRM;
		PSDR |= PPC_RXD4;
		PSDR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);
		PPSR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);

		Ser4MCSR = -1;
		Ser4MCCR1 = 0;
		Ser4MCCR0 = 0x00007f7f;

		mcp_register(&mcp_sa1100);
	}

	return 0;
}

module_init(mcp_sa1100_init);
