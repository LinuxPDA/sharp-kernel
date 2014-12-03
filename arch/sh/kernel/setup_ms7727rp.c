/*
 * linux/arch/sh/kernel/setup_ms7727rp.c
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi MS7727RP02 support.
 *
 */
#include <linux/config.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/ms7727rp.h>

#define DEBUG_LED 0xbb000000

void dbg_led(unsigned int pos, unsigned int f)
{
	if (f) {
		ctrl_outw(ctrl_inw(DEBUG_LED) | (1 << (pos + 8)), DEBUG_LED);
	}
	else {
		ctrl_outw(ctrl_inw(DEBUG_LED) & ~(1 << (pos + 8)), DEBUG_LED);
	}
}

int __init setup_ms7727rp(void)
{
	printk(KERN_INFO "Hitachi MS7727RP02 support.\n");

	sh7727_setup();

	ctrl_outw(0xFF00, DEBUG_LED);

	ctrl_outb(0x00, STBCR);
	ctrl_outb(0x00, STBCR2);
	ctrl_outb(0x00, STBCR3);

	ctrl_outw(ctrl_inw(BCR1) & BCR1_PCM_MASK, BCR1);
	ctrl_outw((ctrl_inw(BCR2) & BCR2_A6_MASK) | BCR2_A6_16, BCR2);

	ctrl_outw(0x0000, PACR);
	ctrl_outw(0x0000, PBCR);
	ctrl_outw(0x0000, PCCR);
	ctrl_outw(0x2000, PDCR);
	ctrl_outw(0x0504, PECR);
	ctrl_outw(0x00aa, PFCR);
	ctrl_outw(0xa200, PGCR);
	ctrl_outw(0x0800, PHCR);
	ctrl_outw(0x0540, PJCR);
	ctrl_outw(0x0000, PKCR);
	ctrl_outw(0xaaaa, PLCR);
	ctrl_outw(0xaa00, PMCR);

	/* LCD */
	ctrl_outw(ctrl_inw(PDCR) & 0x0300, PDCR);
	ctrl_outw(ctrl_inw(PECR) & 0xCF3F, PECR);
	ctrl_outw(ctrl_inw(PHCR) & 0x3FFF, PHCR);
	ctrl_outw(ctrl_inw(PMCR) & 0xFF00, PMCR);

	h8_init();

	return 0;
}
