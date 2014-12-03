/* $Id: irq_ipr.c,v 1.2 2001/04/11 01:30:54 randy Exp $
 *
 * linux/arch/sh/kernel/irq_ipr.c
 *
 * Copyright (C) 1999  Niibe Yutaka & Takeshi Yaegashi
 * Copyright (C) 2000  Kazumoto Kojima
 *
 * Interrupt handling for IPR-based IRQ.
 *
 * Supported system:
 *	On-chip supporting modules (TMU, RTC, etc.).
 *	On-chip supporting modules for SH7709/SH7709A/SH7729.
 *	Hitachi SolutionEngine external I/O:
 *		MS7709SE01, MS7709ASE01, and MS7750SE01
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/machvec.h>

struct ipr_data {
	unsigned int addr;	/* Address of Interrupt Priority Register */
	int shift;		/* Shifts of the 16-bit data */
	int priority;		/* The priority */
};
static struct ipr_data ipr_data[NR_IRQS];

static void enable_ipr_irq(unsigned int irq);
static void disable_ipr_irq(unsigned int irq);

/* shutdown is same as "disable" */
#define shutdown_ipr_irq disable_ipr_irq

static void mask_and_ack_ipr(unsigned int);
static void end_ipr_irq(unsigned int irq);

static unsigned int startup_ipr_irq(unsigned int irq)
{ 
	enable_ipr_irq(irq);
	return 0; /* never anything pending */
}

static struct hw_interrupt_type ipr_irq_type = {
	"IPR-based-IRQ",
	startup_ipr_irq,
	shutdown_ipr_irq,
	enable_ipr_irq,
	disable_ipr_irq,
	mask_and_ack_ipr,
	end_ipr_irq
};

static void disable_ipr_irq(unsigned int irq)
{
	unsigned long val, flags;
	unsigned int addr = ipr_data[irq].addr;
	unsigned short mask = 0xffff ^ (0x0f << ipr_data[irq].shift);

	/* Set the priority in IPR to 0 */
	save_and_cli(flags);
	val = ctrl_inw(addr);
	val &= mask;
	ctrl_outw(val, addr);
	restore_flags(flags);
}

static void enable_ipr_irq(unsigned int irq)
{
	unsigned long val, flags;
	unsigned int addr = ipr_data[irq].addr;
	int priority = ipr_data[irq].priority;
	unsigned short value = (priority << ipr_data[irq].shift);

	/* Set priority in IPR back to original value */
	save_and_cli(flags);
	val = ctrl_inw(addr);
	val |= value;
	ctrl_outw(val, addr);
	restore_flags(flags);
}

static void mask_and_ack_ipr(unsigned int irq)
{
	disable_ipr_irq(irq);
}

static void end_ipr_irq(unsigned int irq)
{
	enable_ipr_irq(irq);
}

void make_ipr_irq(unsigned int irq, unsigned int addr, int pos, int priority)
{
	disable_irq_nosync(irq);
	ipr_data[irq].addr = addr;
	ipr_data[irq].shift = pos*4; /* POSition (0-3) x 4 means shift */
	ipr_data[irq].priority = priority;

	irq_desc[irq].handler = &ipr_irq_type;
	disable_ipr_irq(irq);
}

void __init init_IRQ(void)
{
	make_ipr_irq(TIMER_IRQ, TIMER_IPR_ADDR, TIMER_IPR_POS, TIMER_PRIORITY);
	make_ipr_irq(RTC_IRQ, RTC_IPR_ADDR, RTC_IPR_POS, RTC_PRIORITY);

	make_ipr_irq(SCI_ERI_IRQ, SCI_IPR_ADDR, SCI_IPR_POS, SCI_PRIORITY);
	make_ipr_irq(SCI_RXI_IRQ, SCI_IPR_ADDR, SCI_IPR_POS, SCI_PRIORITY);
	make_ipr_irq(SCI_TXI_IRQ, SCI_IPR_ADDR, SCI_IPR_POS, SCI_PRIORITY);

#ifdef SCIF_ERI_IRQ
	make_ipr_irq(SCIF_ERI_IRQ, SCIF_IPR_ADDR, SCIF_IPR_POS, SCIF_PRIORITY);
	make_ipr_irq(SCIF_RXI_IRQ, SCIF_IPR_ADDR, SCIF_IPR_POS, SCIF_PRIORITY);
	make_ipr_irq(SCIF_BRI_IRQ, SCIF_IPR_ADDR, SCIF_IPR_POS, SCIF_PRIORITY);
	make_ipr_irq(SCIF_TXI_IRQ, SCIF_IPR_ADDR, SCIF_IPR_POS, SCIF_PRIORITY);
#endif

#ifdef IRDA_ERI_IRQ
	make_ipr_irq(IRDA_ERI_IRQ, IRDA_IPR_ADDR, IRDA_IPR_POS, IRDA_PRIORITY);
	make_ipr_irq(IRDA_RXI_IRQ, IRDA_IPR_ADDR, IRDA_IPR_POS, IRDA_PRIORITY);
	make_ipr_irq(IRDA_BRI_IRQ, IRDA_IPR_ADDR, IRDA_IPR_POS, IRDA_PRIORITY);
	make_ipr_irq(IRDA_TXI_IRQ, IRDA_IPR_ADDR, IRDA_IPR_POS, IRDA_PRIORITY);
#endif

	/* Perform the machine specific initialisation */
	if (sh_mv.mv_init_irq != NULL) {
		sh_mv.mv_init_irq();
	}
}
