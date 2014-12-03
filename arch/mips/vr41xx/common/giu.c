/*
 * FILE NAME
 *	arch/mips/vr41xx/common/giu.c
 *
 * BRIEF MODULE DESCRIPTION
 *	General-purpose I/O Unit Interrupt routines for NEC VR4100 series.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
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
 *  - New creation, NEC VR4111, VR4121, VR4122 and VR4131 are supported.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include <asm/addrspace.h>
#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/vr41xx/vr41xx.h>

#define VR4111_GIUIOSELL	KSEG1ADDR(0x0b000100)
#define VR4122_GIUIOSELL	KSEG1ADDR(0x0f000140)

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
#include <asm/vr41xx/toadkk-tcs8000.h>
#define GPMODE0		(VR4181A_INTCS_BASE+0x00b300)
#define GPMODE1		(VR4181A_INTCS_BASE+0x00b302)
#define GPMODE2		(VR4181A_INTCS_BASE+0x00b304)
#define GPMODE3		(VR4181A_INTCS_BASE+0x00b306)
#define GPMODE4		(VR4181A_INTCS_BASE+0x00b308)
#define GPMODE5		(VR4181A_INTCS_BASE+0x00b30a)
#define GPMODE6		(VR4181A_INTCS_BASE+0x00b30c)
#define GPMODE7		(VR4181A_INTCS_BASE+0x00b30e)

#define GPDATA0		(VR4181A_INTCS_BASE+0x00b310)
#define GPDATA1		(VR4181A_INTCS_BASE+0x00b312)
#define GPDATA2		(VR4181A_INTCS_BASE+0x00b314)
#define GPDATA3		(VR4181A_INTCS_BASE+0x00b316)

#define GPINEN0		(VR4181A_INTCS_BASE+0x00b318)
#define GPINEN1		(VR4181A_INTCS_BASE+0x00b31a)
#define GPINEN2		(VR4181A_INTCS_BASE+0x00b31c)
#define GPINEN3		(VR4181A_INTCS_BASE+0x00b31e)

#define GPINTTYP0	(VR4181A_INTCS_BASE+0x00b328)
#define GPINTTYP1	(VR4181A_INTCS_BASE+0x00b32a)
#define GPINTTYP2	(VR4181A_INTCS_BASE+0x00b32c)
#define GPINTTYP3	(VR4181A_INTCS_BASE+0x00b32e)
#define GPINTTYP4	(VR4181A_INTCS_BASE+0x00b330)
#define GPINTTYP5	(VR4181A_INTCS_BASE+0x00b332)
#define GPINTTYP6	(VR4181A_INTCS_BASE+0x00b334)
#define GPINTTYP7	(VR4181A_INTCS_BASE+0x00b336)

static u32 gpinttyp_n[] = {
	GPINTTYP0, GPINTTYP1, GPINTTYP2, GPINTTYP3,
	GPINTTYP4, GPINTTYP5, GPINTTYP6, GPINTTYP7,
};

#define GPINTSTAT0	(VR4181A_INTCS_BASE+0x00b338)
#define GPINTSTAT1	(VR4181A_INTCS_BASE+0x00b33a)
#define GPINTSTAT2	(VR4181A_INTCS_BASE+0x00b33c)
#define GPINTSTAT3	(VR4181A_INTCS_BASE+0x00b33e)

u32 vr41xx_giu_base = 0;

#define read_giuint(addr)		readw(addr)
#define write_giuint(val,addr)		writew((val),(addr))

#else
#define GIUIOSELL	0x00
#define GIUIOSELH	0x02
#define GIUINTSTATL	0x08
#define GIUINTSTATH	0x0a
#define GIUINTENL	0x0c
#define GIUINTENH	0x0e
#define GIUINTTYPL	0x10
#define GIUINTTYPH	0x12
#define GIUINTALSELL	0x14
#define GIUINTALSELH	0x16
#define GIUINTHTSELL	0x18
#define GIUINTHTSELH	0x1a

u32 vr41xx_giu_base = 0;

#define read_giuint(offset)		readw(vr41xx_giu_base + (offset))
#define write_giuint(val, offset)	writew((val), vr41xx_giu_base + (offset))
#endif

static inline u16 set_giuint(u32 offset, u16 set)
{
	u16 res;

	res = read_giuint(offset);
	res |= set;
	write_giuint(res, offset);

	return res;
}

static inline u16 clear_giuint(u32 offset, u16 clear)
{
	u16 res;

	res = read_giuint(offset);
	res &= ~clear;
	write_giuint(res, offset);

	return res;
}

void vr41xx_enable_giuint(int pin)
{
#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	if (pin < 16)
		set_giuint(GPINEN0, (u16)1 << (pin - 0));
	else if (pin < 32)
		set_giuint(GPINEN1, (u16)1 << (pin - 16));
	else if (pin < 48)
		set_giuint(GPINEN2, (u16)1 << (pin - 32));
	else
		set_giuint(GPINEN3, (u16)1 << (pin - 48));
#else
	if (pin < 16)
		set_giuint(GIUINTENL, (u16)1 << pin);
	else
		set_giuint(GIUINTENH, (u16)1 << (pin - 16));
#endif
}

void vr41xx_disable_giuint(int pin)
{
#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	if (pin < 16)
		clear_giuint(GPINEN0, (u16)1 << (pin - 0));
	else if (pin < 32)
		clear_giuint(GPINEN1, (u16)1 << (pin - 16));
	else if (pin < 48)
		clear_giuint(GPINEN2, (u16)1 << (pin - 32));
	else
		clear_giuint(GPINEN3, (u16)1 << (pin - 48));

#else
	if (pin < 16)
		clear_giuint(GIUINTENL, (u16)1 << pin);
	else
		clear_giuint(GIUINTENH, (u16)1 << (pin - 16));
#endif
}

void vr41xx_clear_giuint(int pin)
{
#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	if (pin < 16)
		write_giuint((u16)1 << (pin - 0), GPINTSTAT0);
	else if (pin < 32)
		write_giuint((u16)1 << (pin - 16), GPINTSTAT1);
	else if (pin < 48)
		write_giuint((u16)1 << (pin - 32), GPINTSTAT2);
	else
		write_giuint((u16)1 << (pin - 48), GPINTSTAT3);
#else
	if (pin < 16)
		write_giuint(GIUINTSTATL, (u16)1 << pin);
	else
		write_giuint(GIUINTSTATH, (u16)1 << (pin - 16));
#endif
}

void vr41xx_set_irq_trigger(int pin, int trigger, int hold)
{
#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	u16 mask;
	u32 inttyp;

	inttyp = gpinttyp_n[pin >> 2];
	mask = 2 << (2*(pin & 0x03));

	if ( trigger == TRIGGER_EDGE )
		clear_giuint(inttyp, mask);
	else
		set_giuint(inttyp, mask);
#else
	u16 mask;

	if (pin < 16) {
		mask = (u16)1 << pin;
		if (trigger == TRIGGER_EDGE) {
        		set_giuint(GIUINTTYPL, mask);
			if (hold == SIGNAL_HOLD)
				set_giuint(GIUINTHTSELL, mask);
			else
				clear_giuint(GIUINTHTSELL, mask);
		} else {
			clear_giuint(GIUINTTYPL, mask);
			clear_giuint(GIUINTHTSELL, mask);
		}
	} else {
		mask = (u16)1 << (pin - 16);
		if (trigger == TRIGGER_EDGE) {
			set_giuint(GIUINTTYPH, mask);
			if (hold == SIGNAL_HOLD)
				set_giuint(GIUINTHTSELH, mask);
			else
				clear_giuint(GIUINTHTSELH, mask);
		} else {
			clear_giuint(GIUINTTYPH, mask);
			clear_giuint(GIUINTHTSELH, mask);
		}
	}
#endif

	vr41xx_clear_giuint(pin);
}

void vr41xx_set_irq_level(int pin, int level)
{
#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	u16 mask;
	u32 inttyp;

	inttyp = gpinttyp_n[pin >> 2];
	mask = 1 << (2*(pin & 0x03));

	if ( level == LEVEL_HIGH )
		set_giuint(inttyp, mask);
	else
		clear_giuint(inttyp, mask);
#else
	u16 mask;

	if (pin < 16) {
		mask = (u16)1 << pin;
		if (level == LEVEL_HIGH)
			set_giuint(GIUINTALSELL, mask);
		else
			clear_giuint(GIUINTALSELL, mask);
	} else {
		mask = (u16)1 << (pin - 16);
		if (level == LEVEL_HIGH)
			set_giuint(GIUINTALSELH, mask);
		else
			clear_giuint(GIUINTALSELH, mask);
	}
#endif

	vr41xx_clear_giuint(pin);
}

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
#define GIUINT_NR_IRQS		64

#define GIUINT_OFFSET		8
#define GIUINT_CASCADE_IRQ0	(GIUINT_OFFSET+8)
#define GIUINT_CASCADE_IRQ1	(GIUINT_OFFSET+9)
#define GIUINT_CASCADE_IRQ2	(GIUINT_OFFSET+10)
#define GIUINT_CASCADE_IRQ3	(GIUINT_OFFSET+11)
#else
#define GIUINT_CASCADE_IRQ	16
#define GIUINT_NR_IRQS		32
#endif

enum {
	GIUINT_NO_CASCADE,
	GIUINT_CASCADE
};

struct vr41xx_giuint_cascade {
	unsigned int flag;
	int (*get_irq_number)(int irq);
};

static struct vr41xx_giuint_cascade giuint_cascade[GIUINT_NR_IRQS];
static struct irqaction giu_cascade = {no_action, 0, 0, "cascade", NULL, NULL};

static int no_irq_number(int irq)
{
	return -EINVAL;
}

int vr41xx_cascade_irq(unsigned int irq, int (*get_irq_number)(int irq))
{
	unsigned int pin;
	int retval;

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	if (irq < GIU_IRQ(0) || irq > GIU_IRQ(63))
		return -EINVAL;
#else
	if (irq < GIU_IRQ(0) || irq > GIU_IRQ(31))
		return -EINVAL;
#endif

	if(!get_irq_number)
		return -EINVAL;

	pin = irq - GIU_IRQ(0);
	giuint_cascade[pin].flag = GIUINT_CASCADE;
	giuint_cascade[pin].get_irq_number = get_irq_number;

	retval = setup_irq(irq, &giu_cascade);
	if (retval) {
		giuint_cascade[pin].flag = GIUINT_NO_CASCADE;
		giuint_cascade[pin].get_irq_number = no_irq_number;
	}

	return retval;
}

extern unsigned int do_IRQ(int irq, struct pt_regs *regs);

unsigned int giuint_do_IRQ(int pin, struct pt_regs *regs)
{
	struct vr41xx_giuint_cascade *cascade;
	unsigned int retval = 0;
	int giuint_irq, cascade_irq;

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	disable_irq(GIUINT_CASCADE_IRQ0);
	disable_irq(GIUINT_CASCADE_IRQ1);
	disable_irq(GIUINT_CASCADE_IRQ2);
	disable_irq(GIUINT_CASCADE_IRQ3);
#else
	disable_irq(GIUINT_CASCADE_IRQ);
#endif
	cascade = &giuint_cascade[pin];
	giuint_irq = pin + GIU_IRQ(0);
	if (cascade->flag == GIUINT_CASCADE) {
		cascade_irq = cascade->get_irq_number(giuint_irq);
		disable_irq(giuint_irq);
		if (cascade_irq > 0)
			retval = do_IRQ(cascade_irq, regs);
		enable_irq(giuint_irq);
	} else
		retval = do_IRQ(giuint_irq, regs);
#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	enable_irq(GIUINT_CASCADE_IRQ0);
	enable_irq(GIUINT_CASCADE_IRQ1);
	enable_irq(GIUINT_CASCADE_IRQ2);
	enable_irq(GIUINT_CASCADE_IRQ3);
#else
	enable_irq(GIUINT_CASCADE_IRQ);
#endif

	return retval;
}

void (*board_irq_init)(void) = NULL;

void __init vr41xx_giuint_init(void)
{
	int i;

	switch (mips_cpu.cputype) {
	case CPU_VR4111:
	case CPU_VR4121:
		vr41xx_giu_base = VR4111_GIUIOSELL;
		break;
	case CPU_VR4122:
	case CPU_VR4131:
		vr41xx_giu_base = VR4122_GIUIOSELL;
		break;
#if 1 /*@@@@@*/
	case CPU_VR4181A:
		break;
#endif
	default:
		panic("GIU: Unexpected CPU of NEC VR4100 series");
		break;
	}

	for (i = 0; i < GIUINT_NR_IRQS; i++) {
                vr41xx_disable_giuint(i);
		giuint_cascade[i].flag = GIUINT_NO_CASCADE;
		giuint_cascade[i].get_irq_number = no_irq_number;
	}

#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
	if (setup_irq(GIUINT_CASCADE_IRQ0, &giu_cascade))
		printk("GIUINT: Can not cascade IRQ %d.\n", GIUINT_CASCADE_IRQ0);
	if (setup_irq(GIUINT_CASCADE_IRQ1, &giu_cascade))
		printk("GIUINT: Can not cascade IRQ %d.\n", GIUINT_CASCADE_IRQ1);
	if (setup_irq(GIUINT_CASCADE_IRQ2, &giu_cascade))
		printk("GIUINT: Can not cascade IRQ %d.\n", GIUINT_CASCADE_IRQ2);
	if (setup_irq(GIUINT_CASCADE_IRQ3, &giu_cascade))
		printk("GIUINT: Can not cascade IRQ %d.\n", GIUINT_CASCADE_IRQ3);
#else
	if (setup_irq(GIUINT_CASCADE_IRQ, &giu_cascade))
		printk("GIUINT: Can not cascade IRQ %d.\n", GIUINT_CASCADE_IRQ);
#endif

	if (board_irq_init)
		board_irq_init();
}
