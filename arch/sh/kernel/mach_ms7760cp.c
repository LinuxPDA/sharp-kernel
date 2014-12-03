/*
 * arch/sh/kernel/mach_ms7760cp.c
 *
 * based on linux/arch/sh/kernel/mach_7760se.c
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the MS7760CP01.
 *
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>

#include <asm/io_ms7760cp.h>
#include <asm/irq.h>

extern void heartbeat_ms7760cp(void);

void setup_ms7760cp(void);
void init_ms7760cp_IRQ(void);
void ms7760cp_rtc_gettimeofday(struct timeval*);
int  ms7760cp_rtc_settimeofday(const struct timeval*);

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_ms7760cp __initmv = {
#if 1
	mv_name:		"MS7760CP01",
#else
	mv_name:		"7760_SolutionEngine",
#endif

	mv_nr_irqs:		NR_IRQS,

	mv_inb:			ms7760cp_inb,
	mv_inw:			ms7760cp_inw,
	mv_inl:			ms7760cp_inl,
	mv_outb:		ms7760cp_outb,
	mv_outw:		ms7760cp_outw,
	mv_outl:		ms7760cp_outl,

	mv_inb_p:		ms7760cp_inb_p,
	mv_inw_p:		ms7760cp_inw,
	mv_inl_p:		ms7760cp_inl,
	mv_outb_p:		ms7760cp_outb_p,
	mv_outw_p:		ms7760cp_outw,
	mv_outl_p:		ms7760cp_outl,

	mv_insb:		ms7760cp_insb,
	mv_insw:		ms7760cp_insw,
	mv_insl:		ms7760cp_insl,
	mv_outsb:		ms7760cp_outsb,
	mv_outsw:		ms7760cp_outsw,
	mv_outsl:		ms7760cp_outsl,

	mv_readb:		ms7760cp_readb,
	mv_readw:		ms7760cp_readw,
	mv_readl:		ms7760cp_readl,
	mv_writeb:		ms7760cp_writeb,
	mv_writew:		ms7760cp_writew,
	mv_writel:		ms7760cp_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_isa_port2addr:	ms7760cp_isa_port2addr,

	mv_init_arch:		setup_ms7760cp,
	mv_init_irq:		init_ms7760cp_IRQ,

#ifdef CONFIG_HEARTBEAT
	mv_heartbeat:		heartbeat_ms7760cp,
#endif

	mv_rtc_gettimeofday:	ms7760cp_rtc_gettimeofday,
	mv_rtc_settimeofday:	ms7760cp_rtc_settimeofday,

	mv_hw_ms7760cp:		1,
};
ALIAS_MV(ms7760cp)
