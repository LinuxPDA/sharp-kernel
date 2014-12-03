/*
 * linux/arch/sh/kernel/setup_rts7751r2d.c
 *
 * Copyright (C) 2000  Kazumoto Kojima
 *
 * Renesas Technology Sales RTS7751R2D Support.
 *
 * Modified for RTS7751R2D by
 * Atom Create Engineering Co., Ltd. 2002.
 * Lineo uSolutions, Inc. 2003.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/renesas_rts7751r2d.h>

#include <linux/mm.h>
#include <linux/vmalloc.h>

#if defined(CONFIG_RTS7751R2D_VER_10)
static int mask_pos[] = {6, 11, 9, 8, 12, 10, 5, 4, 7, 14, 13, 0, 0, 0, 0};
#else
static int mask_pos[] = {11, 9, 8, 12, 10, 6, 5, 4, 7, 14, 13, 0, 0, 0, 0};
#endif

/* defined in mm/ioremap.c */
extern void * p3_ioremap(unsigned long phys_addr, unsigned long size, unsigned long flags);

static unsigned int debug_counter;

#ifdef CONFIG_VOYAGERGX
extern int voyagergx_irq_demux(int irq);
#endif 

static void enable_rts7751r2d_irq(unsigned int irq);
static void disable_rts7751r2d_irq(unsigned int irq);

/* shutdown is same as "disable" */
#define shutdown_rts7751r2d_irq disable_rts7751r2d_irq

static void ack_rts7751r2d_irq(unsigned int irq);
static void end_rts7751r2d_irq(unsigned int irq);

static unsigned int startup_rts7751r2d_irq(unsigned int irq)
{
	enable_rts7751r2d_irq(irq);
	return 0; /* never anything pending */
}

static void disable_rts7751r2d_irq(unsigned int irq)
{
	unsigned long flags;
	unsigned short val;
	unsigned short mask = 0xffff ^ (0x0001 << mask_pos[irq]);

	/* Set the priority in IPR to 0 */
	save_and_cli(flags);
	val = ctrl_inw(IRLCNTR1);
	val &= mask;
	ctrl_outw(val, IRLCNTR1);
	restore_flags(flags);
}

static void enable_rts7751r2d_irq(unsigned int irq)
{
	unsigned long flags;
	unsigned short val;
	unsigned short value = (0x0001 << mask_pos[irq]);

	/* Set priority in IPR back to original value */
	save_and_cli(flags);
	val = ctrl_inw(IRLCNTR1);
	val |= value;
	ctrl_outw(val, IRLCNTR1);
	restore_flags(flags);
}

int rts7751r2d_irq_demux(int irq)
{
#ifdef CONFIG_VOYAGERGX
	int demux_irq;

	demux_irq = voyagergx_irq_demux(irq); 
	return demux_irq;
#else
	return irq;
#endif
}

static void ack_rts7751r2d_irq(unsigned int irq)
{
	disable_rts7751r2d_irq(irq);
}

static void end_rts7751r2d_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_rts7751r2d_irq(irq);
}

static struct hw_interrupt_type rts7751r2d_irq_type = {
	"RTS7751R2D IRQ",
	startup_rts7751r2d_irq,
	shutdown_rts7751r2d_irq,
	enable_rts7751r2d_irq,
	disable_rts7751r2d_irq,
	ack_rts7751r2d_irq,
	end_rts7751r2d_irq,
};

static void make_rts7751r2d_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	irq_desc[irq].handler = &rts7751r2d_irq_type;
	disable_rts7751r2d_irq(irq);
}

/*
 * Initialize IRQ setting
 */
void __init init_rts7751r2d_IRQ(void)
{
	int i;

	/* IRL0=KEY Input
	 * IRL1=Ethernet
	 * IRL2=CF Card
	 * IRL3=CF Card Insert
	 * IRL4=PCMCIA
	 * IRL5=VOYAGER
	 * IRL6=RTC Alarm
	 * IRL7=RTC Timer
	 * IRL8=SD Card
	 * IRL9=PCI Slot #1
	 * IRL10=PCI Slot #2
	 * IRL11=Extention #0
	 * IRL12=Extention #1
	 * IRL13=Extention #2
	 * IRL14=Extention #3
	 */

	for (i=0; i<15; i++)
		make_rts7751r2d_irq(i);

#if defined(CONFIG_VOYAGERGX)
	setup_voyagergx_irq();
#endif
}

/*
 * Initialize the board
 */
void __init setup_rts7751r2d(void)
{
	printk(KERN_INFO "Renesas Technology Sales RTS7751R2D support.\n");
#if defined(CONFIG_PCMCIA_BUSTYPE)
	printk(KERN_INFO "  Area 5/6 PCMCIA Bus type support.\n");
#endif
	ctrl_outw(0x0000, PA_OUTPORT);
	debug_counter = 0;
}

#if defined(CONFIG_PCMCIA_BUSTYPE)
void *area5_io_base;
void *area6_io_base;

int __init cf_init(void)
{
	pgprot_t prot;
	unsigned long paddrbase, psize;

	/* open I/O area window */
	paddrbase = virt_to_phys((void *)(PA_AREA5_IO+0x00000800));
	psize = PAGE_SIZE;
	prot = PAGE_KERNEL_PCC(1, _PAGE_PCC_COM16);
	area5_io_base = p3_ioremap(paddrbase, psize, prot.pgprot);
	if (!area5_io_base) {
		printk("allocate_cf_area : can't open CF I/O window!\n");
		return -ENOMEM;
	}

	/* XXX : do we need attribute and common-memory area also? */

	paddrbase = virt_to_phys((void *)PA_AREA6_IO);
	psize = PAGE_SIZE;
	prot = PAGE_KERNEL_PCC(0, _PAGE_PCC_COM16);
	area6_io_base = p3_ioremap(paddrbase, psize, prot.pgprot);
	if (!area6_io_base) {
		printk("allocate_cf_area : can't open Extention Bus window!\n");
		return -ENOMEM;
	}

	return 0;
}

__initcall (cf_init);

#endif

#ifdef CONFIG_HEARTBEAT

#include <linux/sched.h>

/* Cycle the LED's in the clasic Knightrider/Sun pattern */
void heartbeat_rts7751r2d(void)
{
	static unsigned int cnt = 0, period = 0;
	volatile unsigned short *p = (volatile unsigned short *)PA_OUTPORT;
	static unsigned bit = 0, up = 1;

	cnt += 1;
	if (cnt < period)
		return;

	cnt = 0;

	/* Go through the points (roughly!):
	 * f(0)=10, f(1)=16, f(2)=20, f(5)=35, f(int)->110
	 */
	period = 110 - ((300 << FSHIFT)/((avenrun[0]/5) + (3<<FSHIFT)));

	*p = 1<<bit;
	if (up)
		if (bit == 7) {
			bit--;
			up = 0;
		} else
			bit++;
	else if (bit == 0)
		up = 1;
	else
		bit--;
}
#endif

void rts7751r2d_led(unsigned short value)
{
	ctrl_outw(value, PA_OUTPORT);
}

void debug_led_disp(void)
{
	unsigned short value;

	value = (unsigned short)debug_counter++;
	rts7751r2d_led(value);
	if (value == 0xff)
		debug_counter = 0;
}
