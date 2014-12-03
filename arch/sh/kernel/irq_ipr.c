/* $Id: irq_ipr.c,v 1.20 2001/07/15 23:26:56 gniibe Exp $
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
 *	On-chip supporting modules for SH7709/SH7709A/SH7729/SH7727 and SH7290.
 *	Hitachi SolutionEngine external I/O:
 *		MS7709SE01, MS7709ASE01, and MS7750SE01
 *
 * ChangeLog
 *	20040220 Add support Hitachi SolutionEngine MS7710SE
 *	20040220 Add support Hitachi SolutionEngine MS7720RP
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/machvec.h>

#if defined(CONFIG_SH_MS7720RP)
#include <asm/ms7720rp.h>
#endif

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
	"IPR-IRQ",
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
#if defined(CONFIG_CPU_SUBTYPE_SH7760)
	unsigned long mask;

	if (addr < INTC_IPRA) {
		mask = 0xffffffff ^ (0x0f << ipr_data[irq].shift);
	}
	else{
		mask = 0xffff ^ (0x0f << ipr_data[irq].shift);
	}

	if (addr < INTC_IPRA) {
#if defined(CONFIG_RTHAL)
		hard_save_flags_and_cli(flags);
#else
		save_and_cli(flags);
#endif
		val = ctrl_inl(addr);
		val &= mask;
		ctrl_outl(val, addr);
#if defined(CONFIG_RTHAL)
		hard_restore_flags(flags);
#else
		restore_flags(flags);
#endif
	}
	else {
#if defined(CONFIG_RTHAL)
		hard_save_flags_and_cli(flags);
#else
		save_and_cli(flags);
#endif
		val = ctrl_inw(addr);
		val &= mask;
		ctrl_outw(val, addr);
#if defined(CONFIG_RTHAL)
		hard_restore_flags(flags);
#else
		restore_flags(flags);
#endif
	}
#else
	unsigned short mask = 0xffff ^ (0x0f << ipr_data[irq].shift);

	/* Set the priority in IPR to 0 */
#if defined(CONFIG_RTHAL)
	hard_save_flags_and_cli(flags);
#else
	save_and_cli(flags);
#endif
#if defined(CONFIG_SH_MS7720RP)
		if( ( addr >= BCR_ILCR1 ) && ( addr <= BCR_ILCR8 )){
			val = ctrl_inb(addr);
			val &= mask;
			ctrl_outb(val, addr);
		} else {
			val = ctrl_inw(addr);
			val &= mask;
			ctrl_outw(val, addr);
		}
#else
	val = ctrl_inw(addr);
	val &= mask;
	ctrl_outw(val, addr);
#endif
#if defined(CONFIG_RTHAL)
	hard_restore_flags(flags);
#else
	restore_flags(flags);
#endif
#endif
}

static void enable_ipr_irq(unsigned int irq)
{
	unsigned long val, flags;
	unsigned int addr = ipr_data[irq].addr;
#if defined(CONFIG_CPU_SUBTYPE_SH7760)
	unsigned long priority = ipr_data[irq].priority;
	unsigned long value = (priority << ipr_data[irq].shift);

	if (addr < INTC_IPRA) {
#if defined(CONFIG_RTHAL)
		hard_save_flags_and_cli(flags);
#else
		save_and_cli(flags);
#endif
		val = ctrl_inl(addr);
		val |= value;
		ctrl_outl(val, addr);
#if defined(CONFIG_RTHAL)
		hard_restore_flags(flags);
#else
		restore_flags(flags);
#endif
	}
	else {
#if defined(CONFIG_RTHAL)
		hard_save_flags_and_cli(flags);
#else
		save_and_cli(flags);
#endif
		val = ctrl_inw(addr);
		val |= value;
		ctrl_outw(val, addr);
#if defined(CONFIG_RTHAL)
		hard_restore_flags(flags);
#else
		restore_flags(flags);
#endif
	}
#else
	int priority = ipr_data[irq].priority;
	unsigned short value = (priority << ipr_data[irq].shift);

	/* Set priority in IPR back to original value */
#if defined(CONFIG_RTHAL)
	hard_save_flags_and_cli(flags);
#else
	save_and_cli(flags);
#endif
#if defined(CONFIG_SH_MS7720RP)
	if( ( addr >= BCR_ILCR1 ) && ( addr <= BCR_ILCR8 )){
		val = ctrl_inb(addr);
		val |= value;
		ctrl_outb(val, addr);
	} else {
		val = ctrl_inw(addr);
		val |= value;
		ctrl_outw(val, addr);
	}
#else
	val = ctrl_inw(addr);
	val |= value;
	ctrl_outw(val, addr);
#endif
#if defined(CONFIG_RTHAL)
	hard_restore_flags(flags);
#else
	restore_flags(flags);
#endif
#endif
}

static void mask_and_ack_ipr(unsigned int irq)
{
	disable_ipr_irq(irq);

#if defined(CONFIG_CPU_SUBTYPE_SH7707) || \
    defined(CONFIG_CPU_SUBTYPE_SH7709) || \
    defined(CONFIG_CPU_SUBTYPE_SH7710) || \
    defined(CONFIG_CPU_SUBTYPE_SH7720) || \
    defined(CONFIG_CPU_SUBTYPE_SH7727) || \
    defined(CONFIG_CPU_SUBTYPE_SH7290)
	/* This is needed when we use edge triggered setting */
	/* XXX: Is it really needed? */
#if defined(CONFIG_CPU_SUBTYPE_SH7290)
	if (IRQ0_IRQ <= irq && irq <= IRQ7_IRQ) {
#elif defined(CONFIG_CPU_SUBTYPE_SH7710) || defined(CONFIG_CPU_SUBTYPE_SH7720)
	if (IRQ4_IRQ <= irq && irq <= IRQ5_IRQ) {
#else
	if (IRQ0_IRQ <= irq && irq <= IRQ5_IRQ) {
#endif
		/* Clear external interrupt request */
		int a = ctrl_inb(INTC_IRR0);
		a &= ~(1 << (irq - IRQ0_IRQ));
		ctrl_outb(a, INTC_IRR0);
	}
#endif
}

static void end_ipr_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
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

#if defined(CONFIG_CPU_SUBTYPE_SH7707) || \
    defined(CONFIG_CPU_SUBTYPE_SH7709) || \
    defined(CONFIG_CPU_SUBTYPE_SH7727)
static unsigned char pint_map[256];
static unsigned long portcr_mask = 0;

static void enable_pint_irq(unsigned int irq);
static void disable_pint_irq(unsigned int irq);

/* shutdown is same as "disable" */
#define shutdown_pint_irq disable_pint_irq

static void mask_and_ack_pint(unsigned int);
static void end_pint_irq(unsigned int irq);

static unsigned int startup_pint_irq(unsigned int irq)
{ 
	enable_pint_irq(irq);
	return 0; /* never anything pending */
}

static struct hw_interrupt_type pint_irq_type = {
	"PINT-IRQ",
	startup_pint_irq,
	shutdown_pint_irq,
	enable_pint_irq,
	disable_pint_irq,
	mask_and_ack_pint,
	end_pint_irq
};

static void disable_pint_irq(unsigned int irq)
{
	unsigned long val, flags;

#if defined(CONFIG_RTHAL)
	hard_save_flags_and_cli(flags);
#else
	save_and_cli(flags);
#endif
	val = ctrl_inw(INTC_INTER);
	val &= ~(1 << (irq - PINT_IRQ_BASE));
	ctrl_outw(val, INTC_INTER);	/* disable PINTn */
	portcr_mask &= ~(3 << (irq - PINT_IRQ_BASE)*2);
#if defined(CONFIG_RTHAL)
	hard_restore_flags(flags);
#else
	restore_flags(flags);
#endif
}

static void enable_pint_irq(unsigned int irq)
{
	unsigned long val, flags;

#if defined(CONFIG_RTHAL)
	hard_save_flags_and_cli(flags);
#else
	save_and_cli(flags);
#endif
	val = ctrl_inw(INTC_INTER);
	val |= 1 << (irq - PINT_IRQ_BASE);
	ctrl_outw(val, INTC_INTER);	/* enable PINTn */
	portcr_mask |= 3 << (irq - PINT_IRQ_BASE)*2;
#if defined(CONFIG_RTHAL)
	hard_restore_flags(flags);
#else
	restore_flags(flags);
#endif
}

static void mask_and_ack_pint(unsigned int irq)
{
	disable_pint_irq(irq);
}

static void end_pint_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_pint_irq(irq);
}

void make_pint_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	irq_desc[irq].handler = &pint_irq_type;
	disable_pint_irq(irq);
}
#endif

void __init init_IRQ(void)
{

#if defined(CONFIG_CPU_SUBTYPE_SH7707) || \
    defined(CONFIG_CPU_SUBTYPE_SH7709) || \
    defined(CONFIG_CPU_SUBTYPE_SH7727)
	int i;
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7760)
        ctrl_outw(0x0000, INTC_IPRA);
        ctrl_outw(0x0000, INTC_IPRB);
        ctrl_outw(0x0000, INTC_IPRC);
        ctrl_outw(0x0000, INTC_IPRD);
        ctrl_outl(0x00000000, INTC_INTPRI00);
        ctrl_outl(0x00000000, INTC_INTPRI04);
        ctrl_outl(0x00000000, INTC_INTPRI08);
        ctrl_outl(0x00000000, INTC_INTPRI0C);
        ctrl_outl(0xffffffff, INTC_INTMSK00);
        ctrl_outl(0x00ffffff, INTC_INTMSK04);
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7727)
	ctrl_outb(0, INTC_IRR0);
	ctrl_outb(0, INTC_IRR1);
	ctrl_outb(0, INTC_IRR2);
	ctrl_outb(0, INTC_IRR3);
	ctrl_outb(0, INTC_IRR4);
	ctrl_outw(0, INTC_ICR0);
	ctrl_outw(0, INTC_ICR1);
	ctrl_outw(0, INTC_ICR2);
	ctrl_outw(0xFF7F, INTC_ICR3);
	ctrl_outw(0, INTC_INTER);
	ctrl_outw(0, INTC_IPRA);
	ctrl_outw(0, INTC_IPRB);
	ctrl_outw(0, INTC_IPRC);
	ctrl_outw(0, INTC_IPRD);
	ctrl_outw(0, INTC_IPRE);
	ctrl_outw(0, INTC_IPRF);
	ctrl_outw(0, INTC_IPRG);
#endif

	make_ipr_irq(TIMER_IRQ, TIMER_IPR_ADDR, TIMER_IPR_POS, TIMER_PRIORITY);
#if defined(CONFIG_RTSCHED) && defined(CONFIG_CPU_SUBTYPE_SH7710)
	make_ipr_irq(TMU1_IRQ, TMU1_IPR_ADDR, TMU1_IPR_POS, TMU1_PRIORITY_RT);
#else
	make_ipr_irq(TMU1_IRQ, TMU1_IPR_ADDR, TMU1_IPR_POS, TMU1_PRIORITY);
#endif

#ifdef RTC_IRQ
	make_ipr_irq(RTC_IRQ, RTC_IPR_ADDR, RTC_IPR_POS, RTC_PRIORITY);
#endif

	make_ipr_irq(RTC_ALARM_IRQ, RTC_IPR_ADDR, RTC_IPR_POS, RTC_PRIORITY);
	make_ipr_irq(RTC_TICK_IRQ,  RTC_IPR_ADDR, RTC_IPR_POS, RTC_PRIORITY);

#ifdef SCI_ERI_IRQ
	make_ipr_irq(SCI_ERI_IRQ, SCI_IPR_ADDR, SCI_IPR_POS, SCI_PRIORITY);
	make_ipr_irq(SCI_RXI_IRQ, SCI_IPR_ADDR, SCI_IPR_POS, SCI_PRIORITY);
#if defined(CONFIG_CPU_SUBTYPE_SH7760)
	make_ipr_irq(SCI_BRI_IRQ, SCI_IPR_ADDR, SCI_IPR_POS, SCI_PRIORITY);
#endif
	make_ipr_irq(SCI_TXI_IRQ, SCI_IPR_ADDR, SCI_IPR_POS, SCI_PRIORITY);
#endif

#ifdef SCIF0_IRQ
	make_ipr_irq(SCIF0_IRQ, SCIF0_IPR_ADDR, SCIF0_IPR_POS, SCIF0_PRIORITY);
#endif
#ifdef SCIF1_IRQ
	make_ipr_irq(SCIF1_IRQ, SCIF1_IPR_ADDR, SCIF1_IPR_POS, SCIF1_PRIORITY);
#endif

#ifdef SCIF1_ERI_IRQ
	make_ipr_irq(SCIF1_ERI_IRQ, SCIF1_IPR_ADDR, SCIF1_IPR_POS, SCIF1_PRIORITY);
	make_ipr_irq(SCIF1_RXI_IRQ, SCIF1_IPR_ADDR, SCIF1_IPR_POS, SCIF1_PRIORITY);
	make_ipr_irq(SCIF1_BRI_IRQ, SCIF1_IPR_ADDR, SCIF1_IPR_POS, SCIF1_PRIORITY);
	make_ipr_irq(SCIF1_TXI_IRQ, SCIF1_IPR_ADDR, SCIF1_IPR_POS, SCIF1_PRIORITY);
#endif

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

#if defined(CONFIG_CPU_SUBTYPE_SH7290)
	make_ipr_irq(IRQ0_IRQ, IRQ0_IPR_ADDR, IRQ0_IPR_POS, IRQ0_PRIORITY);
	make_ipr_irq(IRQ1_IRQ, IRQ1_IPR_ADDR, IRQ1_IPR_POS, IRQ1_PRIORITY);
	make_ipr_irq(IRQ2_IRQ, IRQ2_IPR_ADDR, IRQ2_IPR_POS, IRQ2_PRIORITY);
	make_ipr_irq(IRQ3_IRQ, IRQ3_IPR_ADDR, IRQ3_IPR_POS, IRQ3_PRIORITY);
	make_ipr_irq(IRQ4_IRQ, IRQ4_IPR_ADDR, IRQ4_IPR_POS, IRQ4_PRIORITY);
	make_ipr_irq(IRQ5_IRQ, IRQ5_IPR_ADDR, IRQ5_IPR_POS, IRQ5_PRIORITY);
	make_ipr_irq(IRQ6_IRQ, IRQ6_IPR_ADDR, IRQ6_IPR_POS, IRQ6_PRIORITY);
	make_ipr_irq(IRQ7_IRQ, IRQ7_IPR_ADDR, IRQ7_IPR_POS, IRQ7_PRIORITY);
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7710)
	make_ipr_irq(EDMAC_EINT0_IRQ, EDMAC_IPR_ADDR, EDMAC_EINT0_IPR_POS, EDMAC_PRIORITY);
	make_ipr_irq(EDMAC_EINT1_IRQ, EDMAC_IPR_ADDR, EDMAC_EINT1_IPR_POS, EDMAC_PRIORITY);
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7707) || \
    defined(CONFIG_CPU_SUBTYPE_SH7709) || \
    defined(CONFIG_CPU_SUBTYPE_SH7727)
	/*
	 * Initialize the Interrupt Controller (INTC)
	 * registers to their power on values
	 */ 

	/*
	 * Enable external irq (INTC IRQ mode).
	 * You should set corresponding bits of PFC to "00"
	 * to enable these interrupts.
	 */
	make_ipr_irq(IRQ0_IRQ, IRQ0_IPR_ADDR, IRQ0_IPR_POS, IRQ0_PRIORITY);
	make_ipr_irq(IRQ1_IRQ, IRQ1_IPR_ADDR, IRQ1_IPR_POS, IRQ1_PRIORITY);
	make_ipr_irq(IRQ2_IRQ, IRQ2_IPR_ADDR, IRQ2_IPR_POS, IRQ2_PRIORITY);
	make_ipr_irq(IRQ3_IRQ, IRQ3_IPR_ADDR, IRQ3_IPR_POS, IRQ3_PRIORITY);
	make_ipr_irq(IRQ4_IRQ, IRQ4_IPR_ADDR, IRQ4_IPR_POS, IRQ4_PRIORITY);
	make_ipr_irq(IRQ5_IRQ, IRQ5_IPR_ADDR, IRQ5_IPR_POS, IRQ5_PRIORITY);
	make_ipr_irq(PINT0_IRQ, PINT0_IPR_ADDR, PINT0_IPR_POS, PINT0_PRIORITY);
	make_ipr_irq(PINT8_IRQ, PINT8_IPR_ADDR, PINT8_IPR_POS, PINT8_PRIORITY);

	enable_ipr_irq(PINT0_IRQ);
	enable_ipr_irq(PINT8_IRQ);

	for(i = 0; i < 16; i++) {
		make_pint_irq(PINT_IRQ_BASE + i);
	}
	for(i = 0; i < 256; i++) {
		if(i & 1) pint_map[i] = 0;
		else if(i & 2) pint_map[i] = 1;
		else if(i & 4) pint_map[i] = 2;
		else if(i & 8) pint_map[i] = 3;
		else if(i & 0x10) pint_map[i] = 4;
		else if(i & 0x20) pint_map[i] = 5;
		else if(i & 0x40) pint_map[i] = 6;
		else if(i & 0x80) pint_map[i] = 7;
	}

#endif /* CONFIG_CPU_SUBTYPE_SH7707 || CONFIG_CPU_SUBTYPE_SH7709 */

#if defined(CONFIG_CPU_SUBTYPE_SH7760)
        make_ipr_irq(GPIO_IRQ,GPIO_IPR_ADDR,GPIO_IPR_POS,GPIO_PRIORITY);
        make_ipr_irq(IRQ4_IRQ,IRQ4_IPR_ADDR,IRQ4_IPR_POS,IRQ4_PRIORITY);
        make_ipr_irq(IRQ5_IRQ,IRQ5_IPR_ADDR,IRQ5_IPR_POS,IRQ5_PRIORITY);
        make_ipr_irq(IRQ6_IRQ,IRQ6_IPR_ADDR,IRQ6_IPR_POS,IRQ6_PRIORITY);
        make_ipr_irq(IRQ7_IRQ,IRQ7_IPR_ADDR,IRQ7_IPR_POS,IRQ7_PRIORITY);
        make_ipr_irq(CAN0_IRQ,CAN0_IPR_ADDR,CAN0_IPR_POS,CAN0_PRIORITY);
        make_ipr_irq(CAN1_IRQ,CAN1_IPR_ADDR,CAN1_IPR_POS,CAN1_PRIORITY);
        make_ipr_irq(SSI0_IRQ,SSI0_IPR_ADDR,SSI0_IPR_POS,SSI0_PRIORITY);
        make_ipr_irq(SSI1_IRQ,SSI1_IPR_ADDR,SSI1_IPR_POS,SSI1_PRIORITY);
        make_ipr_irq(AC970_IRQ,AC970_IPR_ADDR,AC970_IPR_POS,AC970_PRIORITY);
        make_ipr_irq(AC971_IRQ,AC971_IPR_ADDR,AC971_IPR_POS,AC971_PRIORITY);
        make_ipr_irq(IIC0_IRQ,IIC0_IPR_ADDR,IIC0_IPR_POS,IIC0_PRIORITY);
        make_ipr_irq(IIC1_IRQ,IIC1_IPR_ADDR,IIC1_IPR_POS,IIC1_PRIORITY);
        make_ipr_irq(USBH_IRQ,USBH_IPR_ADDR,USBH_IPR_POS,USBH_PRIORITY);
        make_ipr_irq(LCDC_IRQ,LCDC_IPR_ADDR,LCDC_IPR_POS,LCDC_PRIORITY);
        make_ipr_irq(DMABRG0_IRQ,DMABRG_IPR_ADDR,DMABRG_IPR_POS,
                     DMABRG_PRIORITY);
        make_ipr_irq(DMABRG1_IRQ,DMABRG_IPR_ADDR,DMABRG_IPR_POS,
                     DMABRG_PRIORITY);
        make_ipr_irq(DMABRG2_IRQ,DMABRG_IPR_ADDR,DMABRG_IPR_POS,
                     DMABRG_PRIORITY);
        make_ipr_irq(SIM_ERI_IRQ,SIM_IPR_ADDR,SIM_IPR_POS,SIM_PRIORITY);
        make_ipr_irq(SIM_RXI_IRQ,SIM_IPR_ADDR,SIM_IPR_POS,SIM_PRIORITY);
        make_ipr_irq(SIM_TXI_IRQ,SIM_IPR_ADDR,SIM_IPR_POS,SIM_PRIORITY);
        make_ipr_irq(SIM_TEI_IRQ,SIM_IPR_ADDR,SIM_IPR_POS,SIM_PRIORITY);
        make_ipr_irq(SPI_IRQ,SPI_IPR_ADDR,SPI_IPR_POS,SPI_PRIORITY);
        make_ipr_irq(MMC0_IRQ,MMC_IPR_ADDR,MMC_IPR_POS,MMC_PRIORITY);
        make_ipr_irq(MMC1_IRQ,MMC_IPR_ADDR,MMC_IPR_POS,MMC_PRIORITY);
        make_ipr_irq(MMC2_IRQ,MMC_IPR_ADDR,MMC_IPR_POS,MMC_PRIORITY);
        make_ipr_irq(MMC3_IRQ,MMC_IPR_ADDR,MMC_IPR_POS,MMC_PRIORITY);
        make_ipr_irq(MFI_IRQ,MFI_IPR_ADDR,MFI_IPR_POS,MFI_PRIORITY);
        make_ipr_irq(FLSTE_IRQ,FLCTL_IPR_ADDR,FLCTL_IPR_POS,FLCTL_PRIORITY);
        make_ipr_irq(FLTEND_IRQ,FLCTL_IPR_ADDR,FLCTL_IPR_POS,FLCTL_PRIORITY);
        make_ipr_irq(FLTRQ0_IRQ,FLCTL_IPR_ADDR,FLCTL_IPR_POS,FLCTL_PRIORITY);
        make_ipr_irq(FLTRQ1_IRQ,FLCTL_IPR_ADDR,FLCTL_IPR_POS,FLCTL_PRIORITY);
        make_ipr_irq(ADC_IRQ,ADC_IPR_ADDR,ADC_IPR_POS,ADC_PRIORITY);
        make_ipr_irq(CMT_IRQ,CMT_IPR_ADDR,CMT_IPR_POS,CMT_PRIORITY);
        ctrl_outl(0x00000ff0,INTC_INTMSKCLR00);
#endif

#if 0
#if defined(CONFIG_CPU_SUBTYPE_SH7760)
	ctrl_outl(0x00000ff0,INTC_INTMSKCLR00);
	ctrl_outw(0x3fff,INTX_CPLDIRQMSK);
#endif
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7727)
	make_ipr_irq(DMAC_DEI0_IRQ,DMAC_IPR_ADDR,DMAC_IPR_POS,DMAC_PRIORITY);
	make_ipr_irq(DMAC_DEI1_IRQ,DMAC_IPR_ADDR,DMAC_IPR_POS,DMAC_PRIORITY);
	make_ipr_irq(DMAC_DEI2_IRQ,DMAC_IPR_ADDR,DMAC_IPR_POS,DMAC_PRIORITY);
	make_ipr_irq(DMAC_DEI3_IRQ,DMAC_IPR_ADDR,DMAC_IPR_POS,DMAC_PRIORITY);
	make_ipr_irq(ADC_IRQ,   ADC_IPR_ADDR,   ADC_IPR_POS,   ADC_PRIORITY);
	make_ipr_irq(LCDC_IRQ,  LCDC_IPR_ADDR,  LCDC_IPR_POS,  LCDC_PRIORITY);
	make_ipr_irq(PCC_IRQ,   PCC_IPR_ADDR,   PCC_IPR_POS,   PCC_PRIORITY);
	make_ipr_irq(USBH_IRQ,  USBH_IPR_ADDR,  USBH_IPR_POS,  USBH_PRIORITY);
	make_ipr_irq(USBF0_IRQ, USBF0_IPR_ADDR, USBF0_IPR_POS, USBF0_PRIORITY);
	make_ipr_irq(USBF1_IRQ, USBF1_IPR_ADDR, USBF1_IPR_POS, USBF1_PRIORITY);
	make_ipr_irq(AFEIF_IRQ, AFEIF_IPR_ADDR, AFEIF_IPR_POS, AFEIF_PRIORITY);
	make_ipr_irq(SIOF_ERI_IRQ, SIOF_IPR_ADDR, SIOF_IPR_POS, SIOF_PRIORITY);
	make_ipr_irq(SIOF_TXI_IRQ, SIOF_IPR_ADDR, SIOF_IPR_POS, SIOF_PRIORITY);
	make_ipr_irq(SIOF_RXI_IRQ, SIOF_IPR_ADDR, SIOF_IPR_POS, SIOF_PRIORITY);
	make_ipr_irq(SIOF_CCI_IRQ, SIOF_IPR_ADDR, SIOF_IPR_POS, SIOF_PRIORITY);
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7720)
	make_ipr_irq(DMTE0_IRQ,DMA_IPR_ADDR,DMA_IPR_POS,DMA_PRIORITY);
	make_ipr_irq(DMTE1_IRQ,DMA_IPR_ADDR,DMA_IPR_POS,DMA_PRIORITY);
	make_ipr_irq(DMTE2_IRQ,DMA_IPR_ADDR,DMA_IPR_POS,DMA_PRIORITY);
	make_ipr_irq(DMTE3_IRQ,DMA_IPR_ADDR,DMA_IPR_POS,DMA_PRIORITY);
	make_ipr_irq(DMTE4_IRQ,DMAC2_IPR_ADDR,DMAC2_IPR_POS,DMAC2_PRIORITY);
	make_ipr_irq(DMTE5_IRQ,DMAC2_IPR_ADDR,DMAC2_IPR_POS,DMAC2_PRIORITY);
#endif

	/* Perform the machine specific initialisation */
	if (sh_mv.mv_init_irq != NULL) {
		sh_mv.mv_init_irq();
	}
}

#if defined(CONFIG_CPU_SUBTYPE_SH7707) || \
    defined(CONFIG_CPU_SUBTYPE_SH7709) || \
    defined(CONFIG_CPU_SUBTYPE_SH7727)
int ipr_irq_demux(int irq)
{
#if defined(CONFIG_SH_MS7727RP)
	if (irq == PINT0_IRQ)
	{
		irq = PINT_IRQ_BASE;
	}
	else if (irq == PINT8_IRQ)
	{
		irq = PINT_IRQ_BASE + 11;
	}
#else
	unsigned long creg, dreg, d, sav;

	if(irq == PINT0_IRQ)
	{
#if defined(CONFIG_CPU_SUBTYPE_SH7707)
		creg = PORT_PACR;
		dreg = PORT_PADR;
#else
		creg = PORT_PCCR;
		dreg = PORT_PCDR;
#endif
		sav = ctrl_inw(creg);
		ctrl_outw(sav | portcr_mask, creg);
		d = (~ctrl_inb(dreg) ^ ctrl_inw(INTC_ICR2)) & ctrl_inw(INTC_INTER) & 0xff;
		ctrl_outw(sav, creg);
		if (d == 0) return irq;
		return PINT_IRQ_BASE + pint_map[d];
	}
	else if(irq == PINT8_IRQ)
	{
#if defined(CONFIG_CPU_SUBTYPE_SH7707)
		creg = PORT_PBCR;
		dreg = PORT_PBDR;
#else
		creg = PORT_PFCR;
		dreg = PORT_PFDR;
#endif
		sav = ctrl_inw(creg);
		ctrl_outw(sav | (portcr_mask >> 16), creg);
		d = (~ctrl_inb(dreg) ^ (ctrl_inw(INTC_ICR2) >> 8)) & (ctrl_inw(INTC_INTER) >> 8) & 0xff;
		ctrl_outw(sav, creg);
		if(d == 0) return irq;
		return PINT_IRQ_BASE + 8 + pint_map[d];
	}
#endif
	return irq;
}
#endif
