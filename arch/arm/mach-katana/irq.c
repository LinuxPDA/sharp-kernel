/*
 *  linux/arch/arm/mach-katana/irq.c
 *
 *  Copyright (C) 1999 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Change Log
 *	7-Jan-2003 Lineo Japan, Inc.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/mach/irq.h>
#include <asm/arch/irq.h>
#include <asm/arch/MQK_spi.h>

#include <asm/arch/platform.h>	//Katana's stuff

static int GPIO_IRQ_rising_edge[2];
static int GPIO_IRQ_falling_edge[2];
static int GPIO_IRQ_double_clear[2];

void set_GPIO_IRQ_edge(int gpio_nr, int edge)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	long flags;

	local_irq_save(flags);
	if (edge & GPIO_FALLING_EDGE)
		set_bit (gpio_nr, GPIO_IRQ_falling_edge);
	else
		clear_bit (gpio_nr, GPIO_IRQ_falling_edge);
	if (edge & GPIO_RISING_EDGE)
		set_bit (gpio_nr, GPIO_IRQ_rising_edge);
	else
		clear_bit (gpio_nr, GPIO_IRQ_rising_edge);
	irq_desc[IRQ_GPIO(gpio_nr)].valid = 1;

	if (gpio_nr >= 0 && gpio_nr <= 16)
		pMQRegs->gp.gp1A.GpioModeEnable |= (1 << gpio_nr);
	if (gpio_nr >= 17 && gpio_nr <= 19)
		pMQRegs->gp.gp1A.GpioModeEnable |= (1 << (gpio_nr + 12));
	if (gpio_nr >= 24 && gpio_nr <= 31)
		pMQRegs->gp.gp1A.GpioModeEnable |= (1 << (gpio_nr - 3));
	if (gpio_nr >= 32 && gpio_nr <= 35)
		pMQRegs->gp.gp1A.GpioModeEnable |= (1 << (gpio_nr - 15));

	local_irq_restore(flags);
}

EXPORT_SYMBOL(set_GPIO_IRQ_edge);

void set_GPIO_IRQ_polarity(int gpio_nr)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	int nr, shift, mask;

	nr = (gpio_nr < 32) ? 0 : 1;
	shift = nr ? (gpio_nr - 32) : gpio_nr;
	mask = (GPIO_IRQ_rising_edge[nr] & (1 << shift)) ? 1 : 0;
	mask = (GPIO_IRQ_falling_edge[nr] & (1 << shift)) ? (mask ? 2 : 0) : mask;

	if (gpio_nr >= 0 && gpio_nr <= 11)
		pMQRegs->gp.gp0E.TimerLEDInt2 =
		  (pMQRegs->gp.gp0E.TimerLEDInt2 & ~(3 << (gpio_nr * 2))) |
		  (mask << (gpio_nr * 2));
	if (gpio_nr == 12)
		pMQRegs->gp.gp0D.TimerLEDInt1 =
		  (pMQRegs->gp.gp0D.TimerLEDInt1 & ~(3 << 24)) | (mask << 24);
	if (gpio_nr == 13)
		pMQRegs->gp.gp0E.TimerLEDInt2 =
		  (pMQRegs->gp.gp0E.TimerLEDInt2 & ~(3 << 24)) | (mask << 24);
	if (gpio_nr >= 14 && gpio_nr <= 16)
		pMQRegs->gp.gp0D.TimerLEDInt1 =
		  (pMQRegs->gp.gp0D.TimerLEDInt1 & ~(3 << ((gpio_nr * 2) - 2))) |
		  (mask << ((gpio_nr * 2) - 2));
	if (gpio_nr >= 17 && gpio_nr <= 19)
		pMQRegs->gp.gp0E.TimerLEDInt2 =
		  (pMQRegs->gp.gp0E.TimerLEDInt2 & ~(3 << ((gpio_nr * 2) - 8))) |
		  (mask << ((gpio_nr * 2) - 8));
	if (gpio_nr >= 24 && gpio_nr <= 31)
		pMQRegs->gp.gp16.GpioPinInt =
		  (pMQRegs->gp.gp16.GpioPinInt & (3 << ((gpio_nr * 2) - 40))) |
		  (mask << ((gpio_nr * 2) - 40));
	if (gpio_nr >= 32 && gpio_nr <= 35)
		pMQRegs->gp.gp18.Gpio116_119Int =
		  (pMQRegs->gp.gp18.Gpio116_119Int & ~(3 << ((gpio_nr * 2) - 60))) |
		  (mask << ((gpio_nr * 2) - 60));
}

void enable_GPIO_irq(int gpio_nr)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	int index = gpio_nr >> 5;
	int mask = 1 << (index ? (gpio_nr - 32) : gpio_nr);

	/* clear the status register */
	if (GPIO_IRQ_double_clear[index] & mask) {
		if (index == 0)
			pMQRegs->gp.gp0F.TimerLEDIntStat &= mask;
		else
			pMQRegs->gp.gp19.Gpio116_119IntStat &= mask;
	}

	if (gpio_nr >= 0 && gpio_nr <= 11)
		pMQRegs->gp.gp0D.TimerLEDInt1 |= (1 << gpio_nr);
	if (gpio_nr == 12)
		pMQRegs->gp.gp0D.TimerLEDInt1 |= (1 << 20);
	if (gpio_nr == 13)
		pMQRegs->gp.gp0D.TimerLEDInt1 |= (1 << 12);
	if (gpio_nr >= 14 && gpio_nr <= 16)
		pMQRegs->gp.gp0D.TimerLEDInt1 |= (1 << (gpio_nr + 7));
	if (gpio_nr >= 17 && gpio_nr <= 23)
		pMQRegs->gp.gp0D.TimerLEDInt1 |= (1 << (gpio_nr - 4));
	if (gpio_nr >= 24 && gpio_nr <= 31)
		pMQRegs->gp.gp16.GpioPinInt |= (1 << (gpio_nr - 24));
	if (gpio_nr >= 32 && gpio_nr <= 35)
		pMQRegs->gp.gp18.Gpio116_119Int |= (1 << (gpio_nr - 32));
}

void disable_GPIO_irq(int gpio_nr)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	if (gpio_nr >= 0 && gpio_nr <= 11)
		pMQRegs->gp.gp0D.TimerLEDInt1 &= ~(1 << gpio_nr);
	if (gpio_nr == 12)
		pMQRegs->gp.gp0D.TimerLEDInt1 &= ~(1 << 20);
	if (gpio_nr == 13)
		pMQRegs->gp.gp0D.TimerLEDInt1 &= ~(1 << 12);
	if (gpio_nr >= 14 && gpio_nr <= 16)
		pMQRegs->gp.gp0D.TimerLEDInt1 &= ~(1 << (gpio_nr + 7));
	if (gpio_nr >= 17 && gpio_nr <= 23)
		pMQRegs->gp.gp0D.TimerLEDInt1 &= ~(1 << (gpio_nr - 4));
	if (gpio_nr >= 24 && gpio_nr <= 31)
		pMQRegs->gp.gp16.GpioPinInt &= ~(1 << (gpio_nr - 24));
	if (gpio_nr >= 32 && gpio_nr <= 35)
		pMQRegs->gp.gp18.Gpio116_119Int &= ~(1 << (gpio_nr - 32));
}

void clear_GPIO_irq(int gpio_st, int reg)
{
	int i;

	if (gpio_st == 0) return;

	for (i = 0; i < 32; ++i) {
		if (gpio_st & (1 << i)) {
			disable_GPIO_irq(reg ? (i + 32) : i);
			if (reg && i > 3) break;
		}
	}
}

int get_GPIO_level(int index)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	int status;

	if (index == 0) {
		status = pMQRegs->gp.gp0B.GpioData & 0xfff;
		status |= (pMQRegs->gp.gp0B.GpioData & (1 << 16)) >> 4;
		status |= (pMQRegs->gp.gp0B.GpioData & (1 << 12)) << 1;
		status |= (pMQRegs->gp.gp0B.GpioData & (7 << 17)) >> 3;
		status |= (pMQRegs->gp.gp0B.GpioData & (7 << 13)) << 4;
		status |= pMQRegs->gp.gp14.GpioPinDir & (0xff << 24);
	} else
		status = (pMQRegs->gp.gp17.Gpio116_119 & (0xf << 12)) >> 12;

	return status;
}

/*
 * Demux handler for Timer/GPIOs edge detect interrupts
 */

static int Timer_GPIO_enabled[2];	/* enabled i.e. unmasked GPIO IRQs */
static int Timer_GPIO_spurious[2];	/* GPIOs that triggered when masked */

static void mq_Timer_GPIO_demux(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	int i, gedr, spurious;

	while ((gedr = pMQRegs->gp.gp0F.TimerLEDIntStat)) {
		/*
		 * We don't want to disable the irq when the corresponding
		 * IRQ is masked because we could miss a level transition
		 * i.e. an IRQ which need servicing as soon as it is
		 * unmasked.  However, such situation should happen only
		 * during the loop below.  Thus all IRQs which aren't
		 * enabled at this point are considered spurious.  Those
		 * are cleared but only de-activated if they happen twice.
		 */
		spurious = gedr & ~Timer_GPIO_enabled[0];
		if (spurious) {
			pMQRegs->gp.gp0F.TimerLEDIntStat = spurious;
			clear_GPIO_irq(spurious & Timer_GPIO_spurious[0], 0);
			Timer_GPIO_spurious[0] |= spurious;
			gedr ^= spurious;
			if (!gedr) continue;
		}

		for (i = 0; i < 32; ++i) {
			if (gedr & (1 << i)) {
				do_IRQ(IRQ_GPIO(i), regs);
			}
		}
	}

	while ((gedr = pMQRegs->gp.gp19.Gpio116_119IntStat)) {
		spurious = gedr & ~Timer_GPIO_enabled[1];
		if (spurious) {
			pMQRegs->gp.gp19.Gpio116_119IntStat = spurious;
			clear_GPIO_irq(spurious & Timer_GPIO_spurious[1], 1);
			Timer_GPIO_spurious[1] |= spurious;
			gedr ^= spurious;
			if (!gedr) continue;
		}

		for (i = 0; i < 4; ++i) {
			if (gedr & (1 << i)) {
				do_IRQ(IRQ_GPIO(32) + i, regs);
			}
		}
	}
}

static struct irqaction Timer_GPIO_irqaction = {
	name:		"Timer GPIO",
	handler:	mq_Timer_GPIO_demux,
	flags:		SA_INTERRUPT
};

static void mq_RTC_demux(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	int i, gedr;

	while ((gedr = pMQRegs->rt.rt09.IntStatus) & 0x00000003) {
		for (i = 0; i < 2; ++i) {
			if (gedr & (1 << i)) {
				do_IRQ(IRQ_TO_RTC(i), regs);
			}
		}
	}
}

static struct irqaction RTC_irqaction = {
	name:		"RTC",
	handler:	mq_RTC_demux,
	flags:		SA_INTERRUPT
};

static void mq_mask_and_ack_Timer_GPIO_irq(unsigned int irq)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	int gpio_nr = IRQ_TO_Timer_GPIO(irq);
	int index = gpio_nr >> 5;
	int mask = 1 << (index ? (gpio_nr - 32) : gpio_nr);
	Timer_GPIO_spurious[index] &= ~mask;
	Timer_GPIO_enabled[index] &= ~mask;
	if (index == 0)
		pMQRegs->gp.gp0F.TimerLEDIntStat = mask;
	else
		pMQRegs->gp.gp19.Gpio116_119IntStat = mask;
}

static void mq_mask_Timer_GPIO_irq(unsigned int irq)
{
	int gpio_nr = IRQ_TO_Timer_GPIO(irq);
	int index = gpio_nr >> 5;
	int mask = 1 << (index ? (gpio_nr - 32) : gpio_nr);
	Timer_GPIO_spurious[index] &= ~mask;
	Timer_GPIO_enabled[index] &= ~mask;
}

static void mq_unmask_Timer_GPIO_irq(unsigned int irq)
{
	int gpio_nr = IRQ_TO_Timer_GPIO(irq);
	int index = gpio_nr >> 5;
	int mask = 1 << (index ? (gpio_nr - 32) : gpio_nr);
	if (Timer_GPIO_spurious[index] & mask) {
		/*
		 * We don't want to miss an interrupt that would have occurred
		 * while it was masked.  Simulate it if it is the case.
		 */
		int state = get_GPIO_level(index);
		if (((state & GPIO_IRQ_rising_edge[index]) |
		     (~state & GPIO_IRQ_falling_edge[index])) & mask)
		{
			/* just in case it gets referenced: */
			struct pt_regs dummy;

			memzero(&dummy, sizeof(dummy));
			do_IRQ(irq, &dummy);

			/* we are being called recursively from do_IRQ() */
			return;
		}
	}
	Timer_GPIO_enabled[index] |= mask;
	set_GPIO_IRQ_polarity(gpio_nr);
	enable_GPIO_irq(gpio_nr);
}

static void mq_mask_and_ack_RTC_irq(unsigned int irq)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	int rt_nr = RTC_TO_IRQ(irq);
	pMQRegs->rt.rt08.IntEnable &= ~(1 << rt_nr);
}

static void mq_mask_RTC_irq(unsigned int irq)
{
	mq_mask_and_ack_RTC_irq(irq);
}

static void mq_unmask_RTC_irq(unsigned int irq)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	int rt_nr = RTC_TO_IRQ(irq);
	unsigned long status;

	do {
		pMQRegs->rt.rt09.IntStatus = (1 << rt_nr);
		status = pMQRegs->rt.rt09.IntStatus;
	} while (status & (1 << rt_nr));
	pMQRegs->rt.rt08.IntEnable |= (1 << rt_nr);
}

static void katana_mask_irq(unsigned int irq)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	pMQRegs->in.in01.IRQMask &= ~(0x1L << irq);	//disable IRQ
}

static void katana_mask_ack_irq(unsigned int irq)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	pMQRegs->in.in01.IRQMask &= ~(0x1L << irq);	//disable IRQ
	pMQRegs->in.in04.RawStatus = (0x1L << irq);	//ack Raw Status bit
							//Is this needed???
}

static void katana_unmask_irq(unsigned int irq)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	pMQRegs->in.in01.IRQMask |= (0x1L << irq);	//enable IRQ
}
 
void __init katana_init_irq(void)
{
//****** For now let's just hardcode specific interrupts for *******
//****** PTM Timer, UART1 and UART2. Ideally, we should have *******
//****** handled all our interrupt bits.  That's later.      *******

	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	unsigned int i;

	for (i = 0; i <= IRQNO_NAND; i++) {
	        if (((0x1L << i) && IRQ_ALL) != 0) {
		        irq_desc[i].valid	= 1;
			irq_desc[i].probe_ok	= 1;
			irq_desc[i].mask_ack	= katana_mask_ack_irq;
			irq_desc[i].mask	= katana_mask_irq;
			irq_desc[i].unmask	= katana_unmask_irq;
		}
	}

	/* Disable all interrupts initially. */
	/* Do the core module ones */
	pMQRegs->in.in00.Control &= ~IRQ_GLOBAL;	//clr IRQ

	/* do the header card stuff next */
	pMQRegs->in.in01.IRQMask &= IRQ_ALL;		//clear all mask
	pMQRegs->in.in04.RawStatus |= IRQ_ALL;		//clear all
	pMQRegs->in.in00.Control |= IRQ_GLOBAL;		//enable IRQ

	/* for Timer/GPIOs interupit */
	for (i = IRQNO_GPIO0; i <= IRQNO_GPIO119; i++) {
		irq_desc[i].valid	= 0;
		irq_desc[i].probe_ok	= 1;
		irq_desc[i].mask_ack	= mq_mask_and_ack_Timer_GPIO_irq;
		irq_desc[i].mask	= mq_mask_Timer_GPIO_irq;
		irq_desc[i].unmask	= mq_unmask_Timer_GPIO_irq;
	}
	pMQRegs->gp.gp00.TimerCtrl |= TIMER_ENABLE; 	//Enable Timer Module
	//pMQRegs->gp.gp0C.TimerClock = MQ_TIMER_OSC_CLK;
	//pMQRegs->gp.gp0C.TimerClock = MQ_TIMER_PLL1_CLK | MQ_TIMER_DIVIDER1
	//						| MQ_TIMER_DIVIDER2;
	//pMQRegs->gp.gp0C.TimerClock = MQ_TIMER_PLL2_CLK | MQ_TIMER_DIVIDER;
	pMQRegs->gp.gp0C.TimerClock = MQ_TIMER_PLL2_CLK | MQ_TIMER_DIVIDER1
							| MQ_TIMER_DIVIDER2;
	setup_arm_irq(IRQNO_TIMER, &Timer_GPIO_irqaction);

	/* for RTC interupit */
	for (i = IRQNO_RTCPRD; i <= IRQNO_RTCALM; i++) {
		irq_desc[i].valid	= 1;
		irq_desc[i].probe_ok	= 1;
		irq_desc[i].mask_ack	= mq_mask_and_ack_RTC_irq;
		irq_desc[i].mask	= mq_mask_RTC_irq;
		irq_desc[i].unmask	= mq_unmask_RTC_irq;
	}
	setup_arm_irq(IRQNO_RTC, &RTC_irqaction);

	// Listed here which is necessary to clear Interrupt Status Register
	// after the transaction of a handler due to the interrupt is occured
	// by using a controller in a handler. (LINEO Japan, Inc.)
	set_bit(IRQ_TO_Timer_GPIO(IRQ_KATANATS), GPIO_IRQ_double_clear);
	set_bit(IRQ_TO_Timer_GPIO(IRQNO_TIMER1), GPIO_IRQ_double_clear);
}
