/*
 * linux/include/asm-arm/arch-dbmx2/time.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/include/asm-arm/arch-mx2ads/time.h
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
 * Copyright (C) 2002 Motorola Semiconductors HK Ltd
 *
 */

#ifndef U32
typedef unsigned long	U32;
#endif

#include <asm/system.h>
#include <asm/leds.h>
#include <asm/arch/platform.h>
#include <asm/arch/mx2.h>


#if 0		//we have defined those in mx2.h
#define MX2_TMR1_VA_VASE	(IO_ADDRESS(MX2ADS_TIM1_BASE))
#define _reg_TMR_TCTL1     (*((volatile U32 *)(MX2_TMR1_VA_VASE+0x00)))
#define _reg_TMR_TPRER1    (*((volatile U32 *)(MX2_TMR1_VA_VASE+0x04)))
#define _reg_TMR_TCMP1     (*((volatile U32 *)(MX2_TMR1_VA_VASE+0x08)))
#define _reg_TMR_TSTAT1    (*((volatile U32 *)(MX2_TMR1_VA_VASE+0x14)))

#define MX2_IC_VA_BASE		(IO_ADDRESS(MX2ADS_IC_BASE))
#define _reg_AITC_INTENNUM (*((volatile U32 *)(MX2_IC_VA_BASE+0x08)))
#define _reg_AITC_INTTYPEH (*((volatile U32 *)(MX2_IC_VA_BASE+0x18)))
#define _reg_AITC_INTENH   (*((volatile U32 *)(MX2_IC_VA_BASE+0x10)))
#define _reg_AITC_INTENL   (*((volatile U32 *)(MX2_IC_VA_BASE+0x14)))

#define MX2_RTC_VA_BASE		(IO_ADDRESS(MX2ADS_RTC_BASE))
#define _reg_RTC_RCCTL     (*((volatile U32 *)(MX2_RTC_VA_BASE+0x10)))
#define _reg_RTC_RTCISR    (*((volatile U32 *)(MX2_RTC_VA_BASE+0x14)))
#define _reg_RTC_RTCIENR   (*((volatile U32 *)(MX2_RTC_VA_BASE+0x18)))
#endif

#define TIMER_RELOAD	LATCH

#define TICKS2USECS(x)	(((x) * 1000 + CLOCK_TICK_RATE / 2000) \
			 / (CLOCK_TICK_RATE / 1000))


static unsigned long mx2ads_gettimeoffset(void)
{
	unsigned long ticks;

	ticks = _reg_GPT_TCN(GPT1);
	if (_reg_GPT_TSTAT(GPT1) & 1)
		ticks += TIMER_RELOAD;
	return TICKS2USECS(ticks);
}

/*
 * IRQ handler for the timer
 */
static void mx2ads_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	// clear the interrupt
   if (_reg_GPT_TSTAT(GPT1))
   	_reg_GPT_TSTAT(GPT1) = 0;    // clear interrupt

	do_timer(regs);
	do_profile(regs);
}

#ifdef CONFIG_RTHAL
static void rt_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
}

static struct irqaction irq_gpt2  = { rt_timer_interrupt, SA_INTERRUPT, 0, "rt_timer", NULL, NULL};
#endif

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
extern __inline__ void setup_timer(void)
{
	_reg_CRM_PCCR1 |= 1<<25;
#if 0
	_reg_GPT_TPRER(GPT1) = 0;
	_reg_GPT_TCMP(GPT1) = 328;
	_reg_GPT_TCTL(GPT1) = 0x1f;
#else
	_reg_GPT_TPRER(GPT1) = 3;
	_reg_GPT_TCMP(GPT1) = LATCH;
	_reg_GPT_TCTL(GPT1) = 0x13;
#endif

#if 0
	_reg_GPT_TPRER(GPT1) = 7;
	_reg_GPT_TCMP(GPT1) = 2000;
	_reg_GPT_TCTL(GPT1) = 0x13;

#endif // precise_timer

#ifdef CONFIG_RTHAL
	_reg_CRM_PCCR1 |= 1<<26;
	_reg_GPT_TPRER(GPT2) = 0;
	_reg_GPT_TCTL(GPT2) = 0;
#endif

	timer_irq.handler = mx2ads_timer_interrupt;

	// Make irqs happen for the system timer
	setup_arm_irq(INT_GPT1, &timer_irq);
	gettimeoffset = mx2ads_gettimeoffset;
#ifdef CONFIG_RTHAL
	setup_arm_irq(INT_GPT2, &irq_gpt2);
#endif
}
