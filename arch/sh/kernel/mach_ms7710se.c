/*
 * linux/arch/sh/kernel/mach_ms7710se.c
 *
 * Copyright (c) 2004 Lineo Solutions, Inc.
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Machine vector for the Hitachi MS7710SE.
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/machvec_init.h>

#include <asm/io_ms7710se.h>
#include <asm/irq.h>
#include <asm/rtc.h>

extern void __init setup_ms7710se(void);
#if 0
extern void ms7710se_rtc_gettimeofday(struct timeval*);
extern int  ms7710se_rtc_settimeofday(const struct timeval*);
#else
extern void sh_rtc_gettimeofday(struct timeval*);
extern int  sh_rtc_settimeofday(const struct timeval*);
#endif
extern void heartbeat_se(void);
extern int  ms7710se_irq_demux(int);

/*
 * The Machine Vector
 */
struct sh_machine_vector mv_ms7710se __initmv = {
	mv_name:		"ms7710se",

	mv_nr_irqs:		104,

	mv_inb:			ms7710se_inb,
	mv_inw:			generic_inw,
	mv_inl:			generic_inl,
	mv_outb:		generic_outb,
	mv_outw:		generic_outw,
	mv_outl:		generic_outl,

	mv_inb_p:		ms7710se_inb_p,
	mv_inw_p:		generic_inw_p,
	mv_inl_p:		generic_inl,
	mv_outb_p:		generic_outb_p,
	mv_outw_p:		generic_outw,
	mv_outl_p:		generic_outl,

	mv_insb:		ms7710se_insb,
	mv_insw:	        generic_insw,
	mv_insl:		generic_insl,
	mv_outsb:		generic_outsb,
	mv_outsw:		generic_outsw,
	mv_outsl:		generic_outsl,

	mv_readb:		ms7710se_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_init_arch:		setup_ms7710se,
	mv_isa_port2addr:	ms7710se_isa_port2addr,
	mv_irq_demux:		ms7710se_irq_demux,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

#ifdef CONFIG_HEARTBEAT
        mv_heartbeat:           heartbeat_se,
#endif
	mv_hw_ms7710se:	1,
};
ALIAS_MV(ms7710se)
