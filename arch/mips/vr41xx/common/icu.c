/*
 * FILE NAME
 *	arch/mips/vr41xx/vr4122/common/icu.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Interrupt Control Unit routines for the NEC VR4122 and VR4131.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - Added support for NEC VR4111 and VR4121.
 *
 *  Paul Mundt <lethal@chaoticdreams.org>
 *  - kgdb support.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC VR4122 and VR4131 are supported.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>

#include <asm/addrspace.h>
#include <asm/cpu.h>
#include <asm/gdb-stub.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/vr41xx/vr41xx.h>

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
#define MIPS_CPU_IRQ_BASE	0
#define SYSINT0_IRQ_BASE	8
#define SYSINT0_IRQ_LAST	23
#define SYSINT1_IRQ_BASE	24
#define SYSINT1_IRQ_LAST	39
#define SYSINT2_IRQ_BASE	40
#define SYSINT2_IRQ_LAST	55
#define SYSINT3_IRQ_BASE	56
#define SYSINT3_IRQ_LAST	71
#define GIUINT_IRQ_BASE		GIU_IRQ(0)
#define GIUINT_IRQ_LAST		GIU_IRQ(63)
#else
#define MIPS_CPU_IRQ_BASE	0
#define SYSINT1_IRQ_BASE	8
#define SYSINT1_IRQ_LAST	23
#define SYSINT2_IRQ_BASE	24
#define SYSINT2_IRQ_LAST	39
#define GIUINT_IRQ_BASE		GIU_IRQ(0)
#define GIUINT_IRQ_LAST		GIU_IRQ(31)
#endif

#define ICU_CASCADE_IRQ		(MIPS_CPU_IRQ_BASE + 2)

extern asmlinkage void vr41xx_handle_interrupt(void);

extern void __init init_generic_irq(void);
extern void mips_cpu_irq_init(u32 irq_base);
extern unsigned int do_IRQ(int irq, struct pt_regs *regs);

extern void vr41xx_giuint_init(void);
extern unsigned int giuint_do_IRQ(int pin, struct pt_regs *regs);

static u32 vr41xx_icu0_base = 0;
static u32 vr41xx_icu1_base = 0;
static u32 vr41xx_icu2_base = 0;
static u32 vr41xx_icu3_base = 0;

#define VR4111_SYSINT1REG	KSEG1ADDR(0x0b000080)
#define VR4111_SYSINT2REG	KSEG1ADDR(0x0b000200)

#define VR4122_SYSINT1REG	KSEG1ADDR(0x0f000080)
#define VR4122_SYSINT2REG	KSEG1ADDR(0x0f0000a0)

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
#include <asm/vr41xx/toadkk-tcs8000.h>
#define VR4181A_SYSINT0REG	(VR4181A_INTCS_BASE+0x00b088)
#define VR4181A_SYSINT1REG	(VR4181A_INTCS_BASE+0x00b08a)
#define VR4181A_SYSINT2REG	(VR4181A_INTCS_BASE+0x00b08c)
#define VR4181A_SYSINT3REG	(VR4181A_INTCS_BASE+0x00b08e)

#define SYSINT0REG	VR4181A_SYSINT0REG
#define SYSINT1REG	VR4181A_SYSINT1REG
#define SYSINT2REG	VR4181A_SYSINT2REG
#define SYSINT3REG	VR4181A_SYSINT3REG
#define MSYSINT0REG	(VR4181A_INTCS_BASE+0x00b090)
#define MSYSINT1REG	(VR4181A_INTCS_BASE+0x00b092)
#define MSYSINT2REG	(VR4181A_INTCS_BASE+0x00b094)
#define MSYSINT3REG	(VR4181A_INTCS_BASE+0x00b096)

#define GPINTMSK0	(VR4181A_INTCS_BASE+0x00b320)
#define GPINTMSK1	(VR4181A_INTCS_BASE+0x00b322)
#define GPINTMSK2	(VR4181A_INTCS_BASE+0x00b324)
#define GPINTMSK3	(VR4181A_INTCS_BASE+0x00b326)
#define GPINTSTAT0	(VR4181A_INTCS_BASE+0x00b338)
#define GPINTSTAT1	(VR4181A_INTCS_BASE+0x00b33a)
#define GPINTSTAT2	(VR4181A_INTCS_BASE+0x00b33c)
#define GPINTSTAT3	(VR4181A_INTCS_BASE+0x00b33e)

#define NMIREG		(VR4181A_INTCS_BASE+0x00b09c)
#define SOFTINTREG	(VR4181A_INTCS_BASE+0x00b09e)

#define read_icu(addr)	readw(addr)
#define write_icu(val,addr)	writew((val),(addr))

#define read_icu0(addr)	read_icu(addr)
#define write_icu0(val,addr)	write_icu((val),(addr))
#define read_icu1(addr)	read_icu(addr)
#define write_icu1(val,addr)	write_icu((val),(addr))
#define read_icu2(addr)	read_icu(addr)
#define write_icu2(val,addr)	write_icu((val),(addr))
#define read_icu3(addr)	read_icu(addr)
#define write_icu3(val,addr)	write_icu((val),(addr))

#else

#define SYSINT1REG	0x00
#define GIUINTLREG	0x08
#define MSYSINT1REG	0x0c
#define MGIUINTLREG	0x14
#define NMIREG		0x18
#define SOFTREG		0x1a

#define SYSINT2REG	0x00
#define GIUINTHREG	0x02
#define MSYSINT2REG	0x06
#define MGIUINTHREG	0x08

#define read_icu1(offset)	readw(vr41xx_icu1_base + (offset))
#define write_icu1(val, offset)	writew((val), vr41xx_icu1_base + (offset))

#define read_icu2(offset)	readw(vr41xx_icu2_base + (offset))
#define write_icu2(val, offset)	writew((val), vr41xx_icu2_base + (offset))
#endif

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
static inline u16 set_icu(u32 addr, u16 set)
{
	u16 res;

	res = read_icu(addr);
	res |= set;
	write_icu(res, addr);

	return res;
}

static inline u16 clear_icu(u32 addr, u16 clear)
{
	u16 res;

	res = read_icu(addr);
	res &= ~clear;
	write_icu(res, addr);

	return res;
}
#define set_icu0(addr,set)	set_icu(addr,set)
#define set_icu1(addr,set)	set_icu(addr,set)
#define set_icu2(addr,set)	set_icu(addr,set)
#define set_icu3(addr,set)	set_icu(addr,set)
#define clear_icu0(addr,clear)	clear_icu(addr,clear)
#define clear_icu1(addr,clear)	clear_icu(addr,clear)
#define clear_icu2(addr,clear)	clear_icu(addr,clear)
#define clear_icu3(addr,clear)	clear_icu(addr,clear)
#else

static inline u16 set_icu1(u16 offset, u16 set)
{
	u16 res;

	res = read_icu1(offset);
	res |= set;
	write_icu1(res, offset);

	return res;
}

static inline u16 clear_icu1(u16 offset, u16 clear)
{
	u16 res;

	res = read_icu1(offset);
	res &= ~clear;
	write_icu1(res, offset);

	return res;
}

static inline u16 set_icu2(u16 offset, u16 set)
{
	u16 res;

	res = read_icu2(offset);
	res |= set;
	write_icu2(res, offset);

	return res;
}

static inline u16 clear_icu2(u16 offset, u16 clear)
{
	u16 res;

	res = read_icu2(offset);
	res &= ~clear;
	write_icu2(res, offset);

	return res;
}
#endif

/*=======================================================================*/

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
static void enable_sysint0_irq(unsigned int irq)
{
	set_icu0(MSYSINT0REG, (u16)1 << (irq - SYSINT0_IRQ_BASE));
}

static void disable_sysint0_irq(unsigned int irq)
{
	clear_icu0(MSYSINT0REG, (u16)1 << (irq - SYSINT0_IRQ_BASE));
}

static unsigned int startup_sysint0_irq(unsigned int irq)
{ 
	set_icu0(MSYSINT0REG, (u16)1 << (irq - SYSINT0_IRQ_BASE));

	return 0; /* never anything pending */
}

#define shutdown_sysint0_irq	disable_sysint0_irq
#define ack_sysint0_irq		disable_sysint0_irq

static void end_sysint0_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		set_icu0(MSYSINT0REG, (u16)1 << (irq - SYSINT0_IRQ_BASE));
}

static struct hw_interrupt_type sysint0_irq_type = {
	"SYSINT0",
	startup_sysint0_irq,
	shutdown_sysint0_irq,
	enable_sysint0_irq,
	disable_sysint0_irq,
	ack_sysint0_irq,
	end_sysint0_irq,
	NULL
};
/*===========================================================================*/
static void enable_sysint3_irq(unsigned int irq)
{
	set_icu3(MSYSINT3REG, (u16)1 << (irq - SYSINT3_IRQ_BASE));
}

static void disable_sysint3_irq(unsigned int irq)
{
	clear_icu3(MSYSINT3REG, (u16)1 << (irq - SYSINT3_IRQ_BASE));
}

static unsigned int startup_sysint3_irq(unsigned int irq)
{ 
	set_icu3(MSYSINT3REG, (u16)1 << (irq - SYSINT3_IRQ_BASE));

	return 0; /* never anything pending */
}

#define shutdown_sysint3_irq	disable_sysint3_irq
#define ack_sysint3_irq		disable_sysint3_irq

static void end_sysint3_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		set_icu3(MSYSINT3REG, (u16)1 << (irq - SYSINT3_IRQ_BASE));
}

static struct hw_interrupt_type sysint3_irq_type = {
	"SYSINT3",
	startup_sysint3_irq,
	shutdown_sysint3_irq,
	enable_sysint3_irq,
	disable_sysint3_irq,
	ack_sysint3_irq,
	end_sysint3_irq,
	NULL
};
/*===========================================================================*/
#endif

#ifdef CONFIG_TOADKK_TCS8000
static inline void inline_enable_sysint1_irq ( unsigned int irq )
{
	switch ( irq ) {
	case VR4181A_ECU0_INT:
		writeb ( 0x01, VR4181A_ECUINTMSK0 );
		break;
	case VR4181A_ECU1_INT:
		writeb ( 0x01, VR4181A_ECUINTMSK1 );
		break;
	default:
		break;
	}

	set_icu1(MSYSINT1REG, (u16)1 << (irq - SYSINT1_IRQ_BASE));
}
#endif

static void enable_sysint1_irq(unsigned int irq)
{
#ifdef CONFIG_TOADKK_TCS8000
	inline_enable_sysint1_irq ( irq );
#else
	set_icu1(MSYSINT1REG, (u16)1 << (irq - SYSINT1_IRQ_BASE));
#endif
}

static void disable_sysint1_irq(unsigned int irq)
{
#ifdef CONFIG_TOADKK_TCS8000
	switch ( irq ) {
	case VR4181A_ECU0_INT:
		writeb ( 0x01, VR4181A_ECUINT0 );
		writeb ( 0x00, VR4181A_ECUINTMSK0 );
		break;
	case VR4181A_ECU1_INT:
		writeb ( 0x01, VR4181A_ECUINT1 );
		writeb ( 0x00, VR4181A_ECUINTMSK1 );
		break;
	default:
		break;
	}
#endif
	clear_icu1(MSYSINT1REG, (u16)1 << (irq - SYSINT1_IRQ_BASE));
}

static unsigned int startup_sysint1_irq(unsigned int irq)
{
#ifdef CONFIG_TOADKK_TCS8000
	inline_enable_sysint1_irq ( irq );
#else
	set_icu1(MSYSINT1REG, (u16)1 << (irq - SYSINT1_IRQ_BASE));
#endif

	return 0; /* never anything pending */
}

#define shutdown_sysint1_irq	disable_sysint1_irq
#define ack_sysint1_irq		disable_sysint1_irq

static void end_sysint1_irq(unsigned int irq)
{
#ifdef CONFIG_TOADKK_TCS8000
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS))) {
		inline_enable_sysint1_irq ( irq );
	}
#else
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		set_icu1(MSYSINT1REG, (u16)1 << (irq - SYSINT1_IRQ_BASE));
#endif
}

static struct hw_interrupt_type sysint1_irq_type = {
	"SYSINT1",
	startup_sysint1_irq,
	shutdown_sysint1_irq,
	enable_sysint1_irq,
	disable_sysint1_irq,
	ack_sysint1_irq,
	end_sysint1_irq,
	NULL
};

/*=======================================================================*/

static void enable_sysint2_irq(unsigned int irq)
{
	set_icu2(MSYSINT2REG, (u16)1 << (irq - SYSINT2_IRQ_BASE));
}

static void disable_sysint2_irq(unsigned int irq)
{
	clear_icu2(MSYSINT2REG, (u16)1 << (irq - SYSINT2_IRQ_BASE));
}

static unsigned int startup_sysint2_irq(unsigned int irq)
{
	set_icu2(MSYSINT2REG, (u16)1 << (irq - SYSINT2_IRQ_BASE));

	return 0; /* never anything pending */
}

#define shutdown_sysint2_irq	disable_sysint2_irq
#define ack_sysint2_irq		disable_sysint2_irq

static void end_sysint2_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		set_icu2(MSYSINT2REG, (u16)1 << (irq - SYSINT2_IRQ_BASE));
}

static struct hw_interrupt_type sysint2_irq_type = {
	"SYSINT2",
	startup_sysint2_irq,
	shutdown_sysint2_irq,
	enable_sysint2_irq,
	disable_sysint2_irq,
	ack_sysint2_irq,
	end_sysint2_irq,
	NULL
};

/*=======================================================================*/

static void enable_giuint_irq(unsigned int irq)
{
	int pin;

	pin = irq - GIUINT_IRQ_BASE;

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	if (pin < 16) {
		set_icu(GPINTMSK0, (u16)1 << (pin - 0));
	} else if (pin < 32) {
		set_icu(GPINTMSK1, (u16)1 << (pin - 16));
	} else if (pin < 48) {
		set_icu(GPINTMSK2, (u16)1 << (pin - 32));
	} else {
		set_icu(GPINTMSK3, (u16)1 << (pin - 48));
	}
#else
	if (pin < 16)
		set_icu1(MGIUINTLREG, (u16)1 << pin);
	else
		set_icu2(MGIUINTHREG, (u16)1 << (pin - 16));
#endif

	vr41xx_enable_giuint(pin);
}

static void disable_giuint_irq(unsigned int irq)
{
	int pin;

	pin = irq - GIUINT_IRQ_BASE;
	vr41xx_disable_giuint(pin);

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	if (pin < 16) {
		clear_icu(GPINTMSK0, (u16)1 << (pin - 0));
	} else if (pin < 32) {
		clear_icu(GPINTMSK1, (u16)1 << (pin - 16));
	} else if (pin < 48) {
		clear_icu(GPINTMSK2, (u16)1 << (pin - 32));
	} else {
		clear_icu(GPINTMSK3, (u16)1 << (pin - 48));
	}
#else
	if (pin < 16)
		clear_icu1(MGIUINTLREG, (u16)1 << pin);
	else
		clear_icu2(MGIUINTHREG, (u16)1 << (pin - 16));
#endif
}

static unsigned int startup_giuint_irq(unsigned int irq)
{
	vr41xx_clear_giuint(irq - GIUINT_IRQ_BASE);

	enable_giuint_irq(irq);

	return 0; /* never anything pending */
}

#define shutdown_giuint_irq	disable_giuint_irq

static void ack_giuint_irq(unsigned int irq)
{
	disable_giuint_irq(irq);

	vr41xx_clear_giuint(irq - GIUINT_IRQ_BASE);
}

static void end_giuint_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_giuint_irq(irq);
}

static struct hw_interrupt_type giuint_irq_type = {
	"GIUINT",
	startup_giuint_irq,
	shutdown_giuint_irq,
	enable_giuint_irq,
	disable_giuint_irq,
	ack_giuint_irq,
	end_giuint_irq,
	NULL
};

/*=======================================================================*/

static struct irqaction icu_cascade = {no_action, 0, 0, "cascade", NULL, NULL};

static void __init vr41xx_icu_init(void)
{
	int i;

	switch (mips_cpu.cputype) {
	case CPU_VR4111:
	case CPU_VR4121:
		vr41xx_icu1_base = VR4111_SYSINT1REG;
		vr41xx_icu2_base = VR4111_SYSINT2REG;
		break;
	case CPU_VR4122:
	case CPU_VR4131:
		vr41xx_icu1_base = VR4122_SYSINT1REG;
		vr41xx_icu2_base = VR4122_SYSINT2REG;
		break;
#if 1 /*@@@@@*/
	case CPU_VR4181A:
		vr41xx_icu0_base = VR4181A_SYSINT0REG;
		vr41xx_icu1_base = VR4181A_SYSINT1REG;
		vr41xx_icu2_base = VR4181A_SYSINT2REG;
		vr41xx_icu3_base = VR4181A_SYSINT3REG;
		break;
#endif
	default:
		panic("Unexpected CPU of NEC VR4100 series");
		break;
	}

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	write_icu(0, MSYSINT0REG);
	write_icu(0, MSYSINT1REG);
	write_icu(0, MSYSINT2REG);
	write_icu(0, MSYSINT3REG);

	write_icu(0, GPINTMSK0);
	write_icu(0, GPINTMSK1);
	write_icu(0, GPINTMSK2);
	write_icu(0, GPINTMSK3);

	for (i = SYSINT0_IRQ_BASE; i <= GIUINT_IRQ_LAST; i++) {
		if ( i >= SYSINT0_IRQ_BASE && i <= SYSINT0_IRQ_LAST)
			irq_desc[i].handler = &sysint0_irq_type;
		else if (i >= SYSINT1_IRQ_BASE && i <= SYSINT1_IRQ_LAST)
			irq_desc[i].handler = &sysint1_irq_type;
		else if (i >= SYSINT2_IRQ_BASE && i <= SYSINT2_IRQ_LAST)
			irq_desc[i].handler = &sysint2_irq_type;
		else if (i >= SYSINT3_IRQ_BASE && i <= SYSINT3_IRQ_LAST)
			irq_desc[i].handler = &sysint3_irq_type;
		else if (i >= GIUINT_IRQ_BASE && i <= GIUINT_IRQ_LAST)
			irq_desc[i].handler = &giuint_irq_type;
	}
#else
	write_icu1(0, MSYSINT1REG);
	write_icu1(0, MGIUINTLREG);

	write_icu2(0, MSYSINT2REG);
	write_icu2(0, MGIUINTHREG);

	for (i = SYSINT1_IRQ_BASE; i <= GIUINT_IRQ_LAST; i++) {
		if (i >= SYSINT1_IRQ_BASE && i <= SYSINT1_IRQ_LAST)
			irq_desc[i].handler = &sysint1_irq_type;
		else if (i >= SYSINT2_IRQ_BASE && i <= SYSINT2_IRQ_LAST)
			irq_desc[i].handler = &sysint2_irq_type;
		else if (i >= GIUINT_IRQ_BASE && i <= GIUINT_IRQ_LAST)
			irq_desc[i].handler = &giuint_irq_type;
	}
#endif

	setup_irq(ICU_CASCADE_IRQ, &icu_cascade);
}

void __init init_IRQ(void)
{
	memset(irq_desc, 0, sizeof(irq_desc));

	init_generic_irq();
	mips_cpu_irq_init(MIPS_CPU_IRQ_BASE);
	vr41xx_icu_init();

	vr41xx_giuint_init();

	set_except_vector(0, vr41xx_handle_interrupt);

#ifdef CONFIG_REMOTE_DEBUG
	printk("Setting debug traps - please connect the remote debugger.\n");
	set_debug_traps();
	breakpoint();
#endif
}

/*=======================================================================*/

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
static inline void giuint_irqdispatch(u16 gpend0, u16 gpend1, u16 gpend2, u16 gpend3, struct pt_regs *regs)
{
	int i;

	if (gpend0) {
		for (i = 0; i < 16; i++) {
			if (gpend0 & (0x0001 << i)) {
				giuint_do_IRQ(i, regs);
				return;
			}
		}
	} else if (gpend1) {
		for (i = 0; i < 16; i++) {
			if (gpend1 & (0x0001 << i)) {
				giuint_do_IRQ(i+16, regs);
				return;
			}
		}
	} else if (gpend2) {
		for (i = 0; i < 16; i++) {
			if (gpend2 & (0x0001 << i)) {
				giuint_do_IRQ(i+32, regs);
				return;
			}
		}
	} else if (gpend3) {
		for (i = 0; i < 16; i++) {
			if (gpend3 & (0x0001 << i)) {
				giuint_do_IRQ(i+48, regs);
				return;
			}
		}
	}
}
#else
static inline void giuint_irqdispatch(u16 pendl, u16 pendh, struct pt_regs *regs)
{
	int i;

	if (pendl) {
		for (i = 0; i < 16; i++) {
			if (pendl & (0x0001 << i)) {
				giuint_do_IRQ(i, regs);
				return;
			}
		}
	}
	else if (pendh) {
		for (i = 0; i < 16; i++) {
			if (pendh & (0x0001 << i)) {
				giuint_do_IRQ(i + 16, regs);
				return;
			}
		}
	}
}
#endif

asmlinkage void icu_irqdispatch(struct pt_regs *regs)
{
#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	u16 pend0, pend1, pend2, pend3, gpend0, gpend1, gpend2, gpend3;
	u16 mask0, mask1, mask2, mask3, gmask0, gmask1, gmask2, gmask3;
	int i;
	
	pend0 = read_icu(SYSINT0REG);
	pend1 = read_icu(SYSINT1REG);
	pend2 = read_icu(SYSINT2REG);
	pend3 = read_icu(SYSINT3REG);

	mask0 = read_icu(MSYSINT0REG);
	mask1 = read_icu(MSYSINT1REG);
	mask2 = read_icu(MSYSINT2REG);
	mask3 = read_icu(MSYSINT3REG);
	
	gpend0 = read_icu(GPINTSTAT0);
	gpend1 = read_icu(GPINTSTAT1);
	gpend2 = read_icu(GPINTSTAT2);
	gpend3 = read_icu(GPINTSTAT3);

	gmask0 = read_icu(GPINTMSK0);
	gmask1 = read_icu(GPINTMSK1);
	gmask2 = read_icu(GPINTMSK2);
	gmask3 = read_icu(GPINTMSK3);

	pend0 &= mask0;
	pend1 &= mask1;
	pend2 &= mask2;
	pend3 &= mask3;

	gpend0 &= gmask0;
	gpend1 &= gmask1;
	gpend2 &= gmask2;
	gpend3 &= gmask3;

	if (pend0) {
		if ( pend0 & ((1<<8)|(1<<9)|(1<<10)|(1<<11))) {
			giuint_irqdispatch( gpend0, gpend1, gpend2, gpend3, regs );
		} else {
			for ( i = 0; i < 16; ++i ) {
				if ( pend0 & (1<<i) ) {
					do_IRQ(SYSINT0_IRQ_BASE + i, regs);
					break;
				}
			}
		}
		return;
	} else if (pend1) {
		for ( i = 0; i < 16; ++i ) {
			if ( pend1 & (1<<i) ) {
				do_IRQ(SYSINT1_IRQ_BASE + i, regs);
				break;
			}
		}
		return;
	} else if (pend2) {
		for ( i = 0; i < 16; ++i ) {
			if ( pend1 & (1<<i) ) {
				do_IRQ(SYSINT2_IRQ_BASE + i, regs);
				break;
			}
		}
		return;
	} else if (pend3) {
		for ( i = 0; i < 16; ++i ) {
			if ( pend3 & (1<<i) ) {
				do_IRQ(SYSINT3_IRQ_BASE + i, regs);
				break;
			}
		}
		return;
	}
#else
	u16 pend1, pend2, pendl, pendh;
	u16 mask1, mask2, maskl, maskh;
	int i;

	pend1 = read_icu1(SYSINT1REG);
	mask1 = read_icu1(MSYSINT1REG);

	pend2 = read_icu2(SYSINT2REG);
	mask2 = read_icu2(MSYSINT2REG);

	pendl = read_icu1(GIUINTLREG);
	maskl = read_icu1(MGIUINTLREG);

	pendh = read_icu2(GIUINTHREG);
	maskh = read_icu2(MGIUINTHREG);

	pend1 &= mask1;
	pend2 &= mask2;
	pendl &= maskl;
	pendh &= maskh;

	if (pend1) {
		if ((pend1 & 0x01ff) == 0x0100) {
			giuint_irqdispatch(pendl, pendh, regs);
		}
		else {
			for (i = 0; i < 16; i++) {
				if (pend1 & (0x0001 << i)) {
					do_IRQ(SYSINT1_IRQ_BASE + i, regs);
					break;
				}
			}
		}
		return;
	}
	else if (pend2) {
		for (i = 0; i < 16; i++) {
			if (pend2 & (0x0001 << i)) {
				do_IRQ(SYSINT2_IRQ_BASE + i, regs);
				break;
			}
		}
	}
#endif
}
