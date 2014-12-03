/*
 * linux/arch/sh/kernel/setup_ms7290cp.c
 *
 * Copyright (c) 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi MS7290CP01 support.
 */
#include <linux/config.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/ms7290cp.h>

void ms7290cp_rtc_gettimeofday(struct timeval* tv)
{
	tv->tv_sec  = 0;
	tv->tv_usec = 0;
}

int ms7290cp_rtc_settimeofday(const struct timeval* tv)
{
	return 0;
}

int ms7290cp_irq_demux(int irq)
{
	return irq;

}

int dbg_led_r(int pos)
{
	return ((ctrl_inw(DEBUG_LED) & (1 << (pos + 8))) ? 1 : 0);
}

void dbg_led(int pos, int f)
{
	if (f) {
		ctrl_outw(ctrl_inw(DEBUG_LED) | (1 << (pos + 8)), DEBUG_LED);
	}
	else {
		ctrl_outw(ctrl_inw(DEBUG_LED) & ~(1 << (pos + 8)), DEBUG_LED);
	}
}

int __init setup_ms7290cp(void)
{
#if defined(CONFIG_XIP_KERNEL)

#define BASE_ADDRESS3   0xA4140000
#define BASE_ADDRESS4   0xA4080000
#define BASE_PORT       0xA4050000

#define XICR0 (volatile short *)(BASE_ADDRESS3 + 0xFEE0) /* A414FEEO */
#define XICR1 (volatile short *)(BASE_ADDRESS3 + 0x0010) /* A4140010 */
#define XICR2 (volatile short *)(BASE_ADDRESS3 + 0x0020) /* A4140020 */
#define XIPRA (volatile short *)(BASE_ADDRESS3 + 0xFEE2) /* A414FEE2 */
#define XIPRB (volatile short *)(BASE_ADDRESS3 + 0xFEE4) /* A414FEE4 */
#define XIPRC (volatile short *)(BASE_ADDRESS3 + 0x0016) /* A4140016 */
#define XIPRD (volatile short *)(BASE_ADDRESS3 + 0x0018) /* A4140018 */
#define XIPRE (volatile short *)(BASE_ADDRESS3 + 0x001A) /* A414001A */
#define XIPRF (volatile short *)(BASE_ADDRESS4)          /* A4080000 */
#define XIPRG (volatile short *)(BASE_ADDRESS4 + 0x0002) /* A4080002 */
#define XIPRH (volatile short *)(BASE_ADDRESS4 + 0x0004) /* A4080004 */
#define XIPRI (volatile short *)(BASE_ADDRESS4 + 0x0006) /* A4080006 */
#define XIRR0 (volatile char  *)(BASE_ADDRESS3 + 0x0004) /* A4140004 */

#define XPACR   (volatile short *)(BASE_PORT + 0x0100) /* A4050100 */
#define XPBCR   (volatile short *)(BASE_PORT + 0x0102) /* A4050102 */
#define XPCCR   (volatile short *)(BASE_PORT + 0x0104) /* A4050104 */
#define XPDCR   (volatile short *)(BASE_PORT + 0x0106) /* A4050106 */
#define XPECR   (volatile short *)(BASE_PORT + 0x0108) /* A4050108 */
#define XPFCR   (volatile short *)(BASE_PORT + 0x010A) /* A405010A */
#define XPGCR   (volatile short *)(BASE_PORT + 0x010C) /* A405010C */
#define XPHCR   (volatile short *)(BASE_PORT + 0x010E) /* A405010E */
#define XPJCR   (volatile short *)(BASE_PORT + 0x0110) /* A4050110 */
#define XPKCR   (volatile short *)(BASE_PORT + 0x0112) /* A4050112 */
#define XPLCR   (volatile short *)(BASE_PORT + 0x0114) /* A4050114 */
#define XSCPCR  (volatile short *)(BASE_PORT + 0x0116) /* A4050116 */
#define XPMCR   (volatile short *)(BASE_PORT + 0x0118) /* A4050118 */
#define XPNCR   (volatile short *)(BASE_PORT + 0x011A) /* A405011A */
#define XPQCR   (volatile short *)(BASE_PORT + 0x011C) /* A405011C */
#define XSCPCR2 (volatile short *)(BASE_PORT + 0x011E) /* A405011E */
#define XPSELA  (volatile short *)(BASE_PORT + 0x0140) /* A4050140 */
#define XHIZCRA (volatile short *)(BASE_PORT + 0x0146) /* A4050146 */
#define XHIZCRB (volatile short *)(BASE_PORT + 0x0148) /* A4050148 */

	*XIPRA   = 0x0000;
	*XIPRB   = 0x0000;
	*XIPRC   = 0x0000;
	*XIPRD   = 0x0000;
	*XIPRE   = 0x0000;
	*XIPRF   = 0x0000;
	*XIPRG   = 0xF000;
	*XIPRH   = 0x0000;
	*XIPRI   = 0x0000;
	*XPFCR   = 0x0000;
	*XPACR   = 0x0000;
	*XPCCR   = 0x0000;
	*XPDCR   = 0xFF3F;
	*XPECR   = 0x000F;
	*XPHCR   = 0xD000;
	*XPLCR   = 0xA800;
	*XPMCR   = 0x0000;
	*XPQCR   = 0xF000;
	*XSCPCR  = 0xaf80;
	*XHIZCRA = 0x0000;
	*XHIZCRB = 0x0000;
#endif

	printk(KERN_INFO "Hitachi MS7290CP01 support.\n");

	sh7290_setup();

	ctrl_outw(0xFF00, DEBUG_LED);

#if 0
#define SCSMR0	0xa4430000
#define SCBRR0	0xa4430004
#define SCSCR0	0xa4430008
#define SCTDSR0 0xa443000c
#define SCSSR0	0xa4430014
#define SCFCR0	0xa4430018
#define SCFDR0	0xa443001c
#define SCFTDR0 0xa4430020
#define SCFRDR0 0xa4430024
#define SCFER0	0xa4430010
#define SCPCR	0xa4050116
#define PHCR	0xa405010e

	printk("SCSMR0=%04X\n",  ctrl_inw(SCSMR0));
	printk("SCBRR0=%02X\n",  ctrl_inb(SCBRR0));
	printk("SCSCR0=%04X\n",  ctrl_inw(SCSCR0));
	printk("SCTDSR0=%04X\n", ctrl_inw(SCTDSR0));
	printk("SCSSR0=%04X\n",  ctrl_inw(SCSSR0));
	printk("SCFCR0=%04X\n",  ctrl_inw(SCFCR0));
	printk("SCFDR0=%04X\n",  ctrl_inw(SCFDR0));
	printk("SCFTDR0=%04X\n", ctrl_inw(SCFTDR0));
	printk("SCFRDR0=%04X\n", ctrl_inw(SCFRDR0));
	printk("SCFER0=%04X\n",  ctrl_inw(SCFER0));
	printk("SCPCR=%04X\n",   ctrl_inw(SCPCR));
	printk("PHCR=%04X\n",    ctrl_inw(PHCR));
#endif
	return 0;
}
