/*
 * linux/arch/sh/kernel/mach_rts7751r2d.c
 *
 * Minor tweak of mach_se.c file to reference rts7751r2d-specific items.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the Renesas Technology sales RTS7751R2D
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/irq.h>
#include <asm/machvec_init.h>

#include <asm/io_rts7751r2d.h>

extern void heartbeat_rts7751r2d(void);
extern void setup_rts7751r2d(void);
extern void init_rts7751r2d_IRQ(void);
extern void *rts7751r2d_ioremap(unsigned long, unsigned long);
extern void rts7751r2d_iounmap(void *);

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_rts7751r2d __initmv = {
	mv_name:		"7751R RTS7751R2D",

	mv_nr_irqs:		72,

	mv_inb:			rts7751r2d_inb,
	mv_inw:			rts7751r2d_inw,
	mv_inl:			rts7751r2d_inl,
	mv_outb:		rts7751r2d_outb,
	mv_outw:		rts7751r2d_outw,
	mv_outl:		rts7751r2d_outl,

	mv_inb_p:		rts7751r2d_inb_p,
	mv_inw_p:		rts7751r2d_inw,
	mv_inl_p:		rts7751r2d_inl,
	mv_outb_p:		rts7751r2d_outb_p,
	mv_outw_p:		rts7751r2d_outw,
	mv_outl_p:		rts7751r2d_outl,

	mv_insb:		rts7751r2d_insb,
	mv_insw:		rts7751r2d_insw,
	mv_insl:		rts7751r2d_insl,
	mv_outsb:		rts7751r2d_outsb,
	mv_outsw:		rts7751r2d_outsw,
	mv_outsl:		rts7751r2d_outsl,

	mv_readb:		rts7751r2d_readb,
	mv_readw:		rts7751r2d_readw,
	mv_readl:		rts7751r2d_readl,
	mv_writeb:		rts7751r2d_writeb,
	mv_writew:		rts7751r2d_writew,
	mv_writel:		rts7751r2d_writel,

	mv_ioremap:		rts7751r2d_ioremap,
	mv_iounmap:		rts7751r2d_iounmap,

	mv_isa_port2addr:	rts7751r2d_isa_port2addr,

	mv_init_arch:		setup_rts7751r2d,
	mv_init_irq:		init_rts7751r2d_IRQ,
#ifdef CONFIG_HEARTBEAT
	mv_heartbeat:		heartbeat_rts7751r2d,
#endif
	mv_irq_demux:		rts7751r2d_irq_demux,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_rts7751r2d:		1,
};
ALIAS_MV(rts7751r2d)
