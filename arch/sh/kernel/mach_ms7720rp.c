/*
 * linux/arch/sh/kernel/mach_ms7720rp.c
 *
 * Copyright (c) 2004 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Machine vector for the Hitachi MS7720RP01
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/machvec_init.h>

#include <asm/io_ms7720rp.h>
#include <asm/irq.h>
#include <asm/rtc.h>

extern void __init setup_ms7720rp(void);
extern void ms7720rp_rtc_gettimeofday(struct timeval*);
extern int  ms7720rp_rtc_settimeofday(const struct timeval*);
extern int  ms7720rp_irq_demux(int);

/*
 * The Machine Vector
 */
struct sh_machine_vector mv_ms7720rp __initmv = {
	mv_name:		"ms7720rp",

	mv_nr_irqs:		112 + 16,

	mv_inb:			ms7720rp_inb,
	mv_inw:			generic_inw,
	mv_inl:			generic_inl,
	mv_outb:		generic_outb,
	mv_outw:		generic_outw,
	mv_outl:		generic_outl,

	mv_inb_p:		ms7720rp_inb_p,
	mv_inw_p:		generic_inw_p,
	mv_inl_p:		generic_inl,
	mv_outb_p:		generic_outb_p,
	mv_outw_p:		generic_outw,
	mv_outl_p:		generic_outl,

	mv_insb:		ms7720rp_insb,
	mv_insw:	        generic_insw,
	mv_insl:		generic_insl,
	mv_outsb:		generic_outsb,
	mv_outsw:		generic_outsw,
	mv_outsl:		generic_outsl,

	mv_readb:		ms7720rp_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_init_arch:		setup_ms7720rp,
	mv_isa_port2addr:	ms7720rp_isa_port2addr,
	mv_irq_demux:		ms7720rp_irq_demux,

	mv_rtc_gettimeofday:	ms7720rp_rtc_gettimeofday,
	mv_rtc_settimeofday:	ms7720rp_rtc_settimeofday,

	mv_hw_ms7720rp:	1,
};
ALIAS_MV(ms7720rp)
