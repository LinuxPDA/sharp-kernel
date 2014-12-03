/* 
 * arch/sh/kernel/setup_ms7760cp.c
 *
 * Based on linux/arch/sh/kernel/setup_7760se.c
 * Copyright (C) 2003 Takeo Takahashi
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Support functions for SH7760 Solution Engine2.
 *
 * IRQs are as follows on the SH7760 Solution Engine2 board:
 *  [External interrupts (Level encode interrupt)]
 *	IRQ 0: 	IRL0=IRQ3#
 *	IRQ 1: 	IPL1=PCMCIA SIRQ3
 *	IRQ 2: 	IRL2=H8/3048F-ONE
 *	IRQ 3: 	IRL3=UART Ch.B
 *	IRQ 4: 	IRL4=IRQ2# (LAN LANC)
 *	IRQ 5: 	IRL5=PCMCIA SIRQ2
 *	IRQ 6: 	IRL6=UART Ch.A
 *	IRQ 7: 	IRL7=IRQ1# (Extended IDE board INTRQ) or (LAN Ch.B)
 *	IRQ 8: 	IRL8=PCMCIA SIRQ1
 *	IRQ 9: 	IRL9=IRQ0# (LAN Ch.A)
 *	IRQ 10:	IRL10=PCMCIA SIRQ0
 *	IRQ 11: IRL11=none
 *	IRQ 12: IRL12=none
 *	IRQ 13: IRL13=none
 *	IRQ 14: IRL14=none
 *	IRQ 15: -
 *  [On-chip Peripheral interrupts]
 *	IRQ 16: TUNI0
 *	....
 *	IRQ 109: CMT CMTI
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <asm/ms7760cp.h>

#if defined(CONFIG_SH_KGDB) && defined(CONFIG_REMOTE_DEBUG)
#include <asm/kgdb.h>

static int kgdb_uart_setup(void);

struct kgdb_sermap kgdb_uart_sermap =
{ "ttyS", 4, kgdb_uart_setup, NULL };
#endif

int debug_ms7760cp_flag = 0;

void ms7760cp_rtc_init(void); 	/* kernel/rtc-ms7760cp.c */
void print_intc(void);
void print_bsc(void);
void print_sw5(void);
void print_ccr(void);

static void init_h8vol(void);

/*
 * initialize electronic volume to zero.
 */
static void init_h8vol(void)
{
       unsigned char val;

       val = 0;
       h8_out(&val, 1, SH7760SE_VOL_EVRDR);    // right speaker volume
       h8_out(&val, 1, SH7760SE_VOL_EVLDR);    // left speaker volume
#if 0
       printk("H8-VOL: ");
       h8_in(&val, 1, SH7760SE_VOL_EVRDR);
       printk("Right %d, ", val);
       h8_in(&val, 1, SH7760SE_VOL_EVLDR);
       printk("Left %d\n", val);
#endif
}

/*
 * Initialize IRQ setting
 */
void __init init_ms7760cp_IRQ(void)
{
	u16 cr;

	/* Change IRL pin-mode to level encode */
	cr = ctrl_inw(INTC_ICR);
	if (cr & INTC_ICR_IRLM)
		ctrl_outw(cr & ~INTC_ICR_IRLM, INTC_ICR);

	/* disable all interrupts */
	ctrl_outw(0x0000, INTC_IPRA);	/* priority of all interrupts is 0 */
	ctrl_outw(0x0000, INTC_IPRB);	
	ctrl_outw(0x0000, INTC_IPRC);
	ctrl_outw(0x0000, INTC_IPRD);
	ctrl_outl(0x00000000, INTC_INTPRI00);
	ctrl_outl(0x00000000, INTC_INTPRI04);
	ctrl_outl(0x00000000, INTC_INTPRI08);
	ctrl_outl(0x00000000, INTC_INTPRI0C);
	ctrl_outl(0xffffffff, INTC_INTMSK00);		/* mask interrupts */
	ctrl_outl(0x00ffffff, INTC_INTMSK04);

	/*
 	 * enable PCMCIA interrupt
	 *
	 * MRSHPC_ISR: 0110000000000--- (0x6000)
 	 * scratchpad: 0
	 * SIQRn:      1(edge)
	 * CARD IRQ:   100(SIRQ0)
	 * RING IRQ:   000(SIRQn)
	 * MANAGEMENT IRQ: 000(SIRQn)
	 * CPGOOD ENABLE:  0(enable)
	 * DETECT ENABLE:  0(enable)
	 * CRDY_BSY_IREQ:  - (ignored when I/O mode)
	 * BAT WAR ENABLE: - (ignored when I/O mode)
	 * BAT DEAD ENABLE: - (ignored when I/O mode)
	 */
	outw(0x6000, MRSHPC_ICR);

	make_imask_irq(SH7760SE_IRQ_UART_INTA);	/* cpu board ch.A */
	make_imask_irq(SH7760SE_IRQ_UART_INTB);	/* cpu board ch.B */
	make_imask_irq(SH7760SE_IRQ_H8IRQ);	/* H8/3048 */
	make_imask_irq(SH7760SE_IRQ_SIRQ0);	/* pcmcia ireq */
	make_imask_irq(SH7760SE_IRQ_IRQ0);	/* ether boad ch.C */
	make_imask_irq(SH7760SE_IRQ_IRQ1);	/* extended IDE board
						 * or
						 * ether boad ch.D
						 */
	make_imask_irq(SH7760SE_IRQ_IRQ2);	/* ether boad SMC91C111 */

        make_intc2_irq(69,      	/* DMABRGI1(all data transfer int.) */
                       INTC_INTPRI00,
                       5,       	/* 5th group of INTC_INTPRI08 */
                       13,      	/* bit 13 in INTMSKCLR */
                       15);      	/* FIXME: priority */
        make_intc2_irq(70,      	/* DMABRGI2(half data transfer int.) */
                       INTC_INTPRI00,
                       5,       	/* 5th group of INTC_INTPRI08 */
                       12,      	/* bit 12 in INTMSKCLR */
                       15);      	/* FIXME: priority */
}

#if 0
void __init init_hdd(void)
{
	/*
	 * reset extenal hard disk
	 */
	printk("Reset HDD: ");
	ctrl_outw(0x0001, ioremap_nocache(PA_EXT5, 0) + 0x20);
	printk("0x%lx\n", ctrl_inw(ioremap_nocache(PA_EXT5,0)+0x20));
}
#endif

/*
 * Initialize the board
 */
void __init setup_ms7760cp(void)
{	
#if 0
	print_sw5();
	print_bsc();
	print_intc();
	print_ccr();
#endif

	ctrl_outw(0x0001,MS7760CP_SYSCR);	/* H8IREN : ON */

	/* initialize UART Ch.A */
	outw(0x0080, ST16_UART_A_BASE + ST16_LCR);	/* latch:1 */
	outw(0x000c, ST16_UART_A_BASE + ST16_DLL);	/* 38400(LSB) */
	outw(0x0000, ST16_UART_A_BASE + ST16_DLM);	/*      (MSB) */
	outw(0x0000, ST16_UART_A_BASE + ST16_LCR);	/* latch:0 */
	outw(0x0003, ST16_UART_A_BASE + ST16_LCR);	/* 8bit,nonpar,1stop*/
	outw(0x0000, ST16_UART_A_BASE + ST16_IER);	/* disable int. */
	outw(0x0003, ST16_UART_A_BASE + ST16_MCR);	/* RTS:1, DTR:1 */

	init_h8vol();	/* initialize electronic volume */

	/* RTC setting */
	ms7760cp_rtc_init();

#if defined(CONFIG_SH_KGDB) && defined(CONFIG_REMOTE_DEBUG)
	kgdb_serial_setup = kgdb_uart_setup;
#ifdef CONFIG_KGDB_DEFPORT
	kgdb_portnum = CONFIG_KGDB_DEFPORT;
#else
	kgdb_portnum = 0;
#endif
#ifdef CONFIG_KGDB_DEFBAUD
	kgdb_baud = CONFIG_KGDB_DEFBAUD;
#else
	kgdb_baud = 115200;
#endif
	//kgdb_parity = CONFIIG_KGDB_DEFPARITY;
	//kgdb_bits = CONFIG_KGDB_DEFBITS;
	//kgdb_cflag = 0;

	kgdb_register_sermap(&kgdb_uart_sermap);
#endif /* CONFIG_SH_KGDB && CONFIG_REMOTE_DEBUG */
}

void print_intc(void)
{
	printk("ICR=0x%04x\n", (unsigned short)ctrl_inw(INTC_ICR));
	printk("IPRA=0x%04x\n", (unsigned short)ctrl_inw(INTC_IPRA));
	printk("IPRB=0x%04x\n", (unsigned short)ctrl_inw(INTC_IPRB));
	printk("IPRC=0x%04x\n", (unsigned short)ctrl_inw(INTC_IPRC));
	printk("IPRD=0x%04x\n", (unsigned short)ctrl_inw(INTC_IPRD));
	printk("INTPRI00=0x%08x\n", ctrl_inl(INTC_INTPRI00));
	printk("INTPRI04=0x%08x\n", ctrl_inl(INTC_INTPRI04));
	printk("INTPRI08=0x%08x\n", ctrl_inl(INTC_INTPRI08));
	printk("INTPRI0C=0x%08x\n", ctrl_inl(INTC_INTPRI0C));
	printk("INTREQ00=0x%08x\n", ctrl_inl(INTC_INTREQ00));
	printk("INTREQ04=0x%08x\n", ctrl_inl(INTC_INTREQ04));
	printk("INTMSK00=0x%08x\n", ctrl_inl(INTC_INTMSK00));
	printk("INTMSK04=0x%08x\n", ctrl_inl(INTC_INTMSK04));
}

void print_sw5(void)
{
	unsigned char sw5 = ctrl_inb(PA_DIPSW0);
	printk("ID register:\n");
	printk(" SW5-1: %s\n", (sw5 & 0x01)? "OFF":"ON");
	printk(" SW5-2: %s\n", (sw5 & 0x02)? "OFF":"ON");
	printk(" SW5-3: %s\n", (sw5 & 0x04)? "OFF":"ON");
	printk(" SW5-4: %s\n", (sw5 & 0x08)? "OFF":"ON");
	printk(" SW5-5: %s\n", (sw5 & 0x10)? "OFF":"ON");
	printk(" SW5-6: %s\n", (sw5 & 0x20)? "OFF":"ON");
	/* manual is correct ? */
	printk(" SW5-7: %s\n", (sw5 & 0x40)? "ON":"OFF");
	printk(" SW5-8: %s\n", (sw5 & 0x80)? "ON":"OFF");
}

void print_bsc(void)
{
	printk("BCR1=0x%08x\n", ctrl_inl(BCR1));
	printk("BCR2=0x%04x\n", ctrl_inw(BCR2));
	printk("BCR3=0x%04x\n", ctrl_inw(BCR3));
	printk("BCR4=0x%08x\n", ctrl_inl(BCR4));
	printk("WCR1=0x%08x\n", ctrl_inl(WCR1));
	printk("WCR2=0x%08x\n", ctrl_inl(WCR2));
	printk("WCR3=0x%08x\n", ctrl_inl(WCR3));
	printk("WCR4=0x%08x\n", ctrl_inl(WCR4));
#if 0
	printk("FRQCR=0x%04x\n", ctrl_inw(FRQCR));
	printk("STBCR=0x%02x\n", ctrl_inb(STBCR));
	printk("WTCNT=0x%02x\n",  ctrl_inb(WTCNT));
	printk("WTCSR=0x%02x\n",  ctrl_inb(WTCSR));
	printk("STBCR2=0x%02x\n", ctrl_inb(STBCR2));

	printk("CLKSTP00=0x%08x\n", ctrl_inl(CLKSTP00));
	printk("CLKSTPCLR00=0x%08x\n", ctrl_inl(CLKSTPCLR00));
	printk("DCKDR=0x%08x\n", ctrl_inl(DCKDR));
	printk("MCKCR=0x%08x\n", ctrl_inl(MCKCR));

	printk("PACR=%04x\n", ctrl_inw(PACR));
	printk("PBCR=%04x\n", ctrl_inw(PBCR));
	printk("PCCR=%04x\n", ctrl_inw(PCCR));
	printk("PDCR=%04x\n", ctrl_inw(PDCR));
	printk("PECR=%04x\n", ctrl_inw(PECR));
	printk("PFCR=%04x\n", ctrl_inw(PFCR));
	printk("PGCR=%04x\n", ctrl_inw(PGCR));
	printk("PHCR=%04x\n", ctrl_inw(PHCR));
	printk("PJCR=%04x\n", ctrl_inw(PJCR));
	printk("PKCR=%04x\n", ctrl_inw(PKCR));

	printk("INPUPA=%04x\n", ctrl_inw(INPUPA));
	printk("DMAPCR=%04x\n", ctrl_inw(DMAPCR));
	printk("SCIHZR=%04x\n", ctrl_inw(SCIHZR));
	printk("IPSELR=%04x\n", ctrl_inw(IPSELR));

	printk("PADR=%02x\n", ctrl_inb(PADR));
	printk("PBDR=%02x\n", ctrl_inb(PBDR));
	printk("PCDR=%02x\n", ctrl_inb(PCDR));
	printk("PDDR=%02x\n", ctrl_inb(PDDR));
	printk("PEDR=%02x\n", ctrl_inb(PEDR));
	printk("PFDR=%02x\n", ctrl_inb(PFDR));
	printk("PGDR=%02x\n", ctrl_inb(PGDR));
	printk("PHDR=%02x\n", ctrl_inb(PHDR));
	printk("PJDR=%02x\n", ctrl_inb(PJDR));
	printk("PKDR=%02x\n", ctrl_inb(PKDR));

	printk("PAPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PBPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PCPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PDPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PEPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PFPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PGPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PHPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PIPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PJPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("PKPUPR=%02x\n", ctrl_inb(PAPUPR));
	printk("MDPUPR=%02x\n", ctrl_inb(MDPUPR));
	printk("MODSELR=%02x\n", ctrl_inb(MODSELR));

	printk("DMABRGCR=0x%08x\n", ctrl_inl(DMABRGCR));
	printk("DMAOR=0x%08x\n", ctrl_inl(DMAOR));
	printk("DMATCR0=0x%08x\n", ctrl_inl(DMATCR0));
	printk("CHCR0=0x%08x\n", ctrl_inl(CHCR0));
	printk("DMARCR=0x%08x\n", ctrl_inl(DMARCR));
	printk("DMARSRA=0x%08x\n", ctrl_inl(DMARSRA));
	printk("SSICR0=0x%08x\n", ctrl_inl(SSICR0));
	printk("SSISR0=0x%08x\n", ctrl_inl(SSISR0));
	printk("SSICR1=0x%08x\n", ctrl_inl(SSICR1));
	printk("SSISR1=0x%08x\n", ctrl_inl(SSISR1));
	printk("DMAATXSAR1=0x%08x\n", ctrl_inl(DMAATXSAR1));
	printk("DMAATXTCR1=0x%08x\n", ctrl_inl(DMAATXTCR1));
	printk("IPSEL=0x%04x\n", ctrl_inl(0xfe400034));
	printk("DMAACR=0x%08x\n", ctrl_inl(DMAACR1));
	printk("DMAATXTCNT=0x%08x\n", ctrl_inl(0xfe3c0074));// DMAATXTCNT1
	printk("INTC_INTPRI00=0x%08x\n", ctrl_inl(INTC_INTPRI00));
	printk("INTC_INTPRI04=0x%08x\n", ctrl_inl(INTC_INTPRI04));
	printk("INTC_INTPRI08=0x%08x\n", ctrl_inl(INTC_INTPRI08));
	printk("INTC_INTREQ00=0x%08x\n", ctrl_inl(INTC_INTREQ00));
	printk("INTC_INTREQ04=0x%08x\n", ctrl_inl(INTC_INTREQ04));
	printk("INTC_INTMSK00=0x%08x\n", ctrl_inl(INTC_INTMSK00));
	printk("INTC_INTMSK04=0x%08x\n", ctrl_inl(INTC_INTMSK04));
	printk("CLKSTP00=0x%08x\n", ctrl_inl(CLKSTP00));
#endif
}

void print_ccr(void)
{
#define CCR	0xff00001c	/* cache control register */
	printk("CCR=0x%08x\n", ctrl_inl(CCR));
}

static int __init debug_ms7760cp_setup(char *str)
{
	debug_ms7760cp_flag = 1;
	printk("debug_ms7760cp_setup: debug_ms7760cp_flag = %d\n",
			debug_ms7760cp_flag);
	return 1;
}

__setup("debug_ms7760cp", debug_ms7760cp_setup);

#define XST16_THR (ST16_UART_B_BASE + ST16_THR)
#define XST16_LSR (ST16_UART_B_BASE + ST16_LSR)

void dbg_putc(int ch)
{
	while((ctrl_inw(XST16_LSR) & 0x0060) != 0x0060) {}
	ctrl_outw(ch, XST16_THR);
}

void dbg_puts(const char *s)
{
	while (*s) {
		if (*s == '\n') {
			dbg_putc('\r');
		}
		dbg_putc(*s++);
	}
}

int dbg_printk(const char* fmt, ...)
{
	char buf[256];
	va_list args;
	int len = 0;

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	dbg_puts(buf);

	return len;
}

#if defined(CONFIG_SH_KGDB) && defined(CONFIG_REMOTE_DEBUG)
extern int gdb_enter;	/* 1 = enter debugger on boot */
extern int gdb_ttyS;
extern int gdb_baud;
extern int putDebugChar(int);   /* write a single character      */
extern int getDebugChar(void);   /* read and return a single char */

static int kgdb_uart_setup(void)
{
	gdb_enter = kgdb_halt;
	kgdb_halt = 0;

	gdb_ttyS = kgdb_portnum;
	gdb_baud = kgdb_baud;
	//kgdb_parity;
	//kgdb_bits;
	//kgdb_cflag;

	/* Setup complete: initialize function pointers */
	kgdb_getchar = getDebugChar;
	kgdb_putchar = putDebugChar;
	return 0;
}
 
int kgdb_sci_setup(void)
{
	return 0;
}
#endif /* CONFIG_SH_KGDB && CONFIG_REMOTE_DEBUG*/

#ifdef CONFIG_HEARTBEAT

#include <linux/sched.h>

/* Cycle the LED's in the clasic Knightrider/Sun pattern */
void heartbeat_ms7760cp(void)
{
	static unsigned int cnt = 0, period = 0;
	volatile unsigned short *p = (volatile unsigned short *)0xA1400000;
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
