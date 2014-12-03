/*
 *  linux/arch/arm/mach-dbmx2/irq.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/arch/arm/mach-mx2ads/irq.c
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) 2002 Shane Nay (shane@minirl.com)
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
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/mach/irq.h>
#include <asm/arch/mx2.h>

/*
 *
 * We simply use the ENABLE DISABLE registers inside of the mx2
 * to turn on/off specific interrupts.  FIXME- We should
 * also add support for the accelerated interrupt controller
 * by putting offets to irq jump code in the appropriate
 * places.
 *
 */

#if 0			//we have defined it in mx2.h
#define INTENNUM_OFF              0x8
#define INTDISNUM_OFF             0xC
#define VA_AITC_BASE              IO_ADDRESS(MX2ADS_AITC_BASE)
#define MX2ADS_AITC_INTDISNUM     (VA_AITC_BASE + INTDISNUM_OFF)
#define MX2ADS_AITC_INTENNUM      (VA_AITC_BASE + INTENNUM_OFF)
#endif

#ifndef U32
typedef unsigned long U32;
#endif

#if 0				//we have defined it in mx2.h
#define _reg_AITC_INTENNUM 	(*((volatile U32 *)(VA_AITC_BASE+0x08)))
#define _reg_AITC_INTDISNUM	(*((volatile U32 *)(VA_AITC_BASE+0x0C)))
#define _reg_AITC_INTTYPEH	(*((volatile U32 *)(VA_AITC_BASE+0x18)))
#define _reg_AITC_INTTYPEL	(*((volatile U32 *)(VA_AITC_BASE+0x1C)))
#define _reg_AITC_FIVECSR	(*((volatile U32 *)(VA_AITC_BASE+0x44)))
#endif

#define TYPE_BIT_MASK(n) 		(1 << n)



static void
dbmx2_mask_irq(unsigned int irq)
{
	//__raw_writel(irq, MX2ADS_AITC_INTDISNUM);
	_reg_AITC_INTDISNUM = irq;
}

static void
dbmx2_unmask_irq(unsigned int irq)
{
	//__raw_writel(irq, MX2ADS_AITC_INTENNUM);
	_reg_AITC_INTENNUM = irq;
}

// Thomas Wong add this on 2002/3/15 for FIQ support
void dbmx2_mask_fiq(unsigned int irq)
{
	_reg_AITC_INTDISNUM = (unsigned long) irq;
}

// Thomas Wong add this on 2002/3/15 for FIQ support
void dbmx2_unmask_fiq(unsigned int irq)
{
	_reg_AITC_INTENNUM = (unsigned long)irq;
}

// Thomas Wong add this on 2002/3/14 for FIQ support
void dbmx2_set_type_to_fiq(unsigned int irq)
{
	if (irq >= 32)
	{
		_reg_AITC_INTTYPEH |= TYPE_BIT_MASK(irq-32);
	}
	else
	{
		_reg_AITC_INTTYPEL |= TYPE_BIT_MASK(irq);
	}
}

// Thomas Wong add this on 2002/3/14 for FIQ support
void dbmx2_set_type_to_irq(unsigned int irq)
{
	if (irq >= 32)
	{
		_reg_AITC_INTTYPEH &= ~TYPE_BIT_MASK(irq-32);
	}
	else
	{
		_reg_AITC_INTTYPEL &= ~TYPE_BIT_MASK(irq);
	}
}

// Thomas Wong add this on 2002/3/15 for FIQ support
unsigned int dbmx2_get_fiq_num()
{
//	printk("Now in sc_get_fiq_num ...\n");
	return (_reg_AITC_FIVECSR);
}

void __init
dbmx2_init_irq(void)
{
	unsigned int i;

	for (i = 0; i < NR_IRQS; i++) {
		irq_desc[i].valid = 1;
		irq_desc[i].probe_ok = 1;
		irq_desc[i].mask_ack = dbmx2_mask_irq;
		irq_desc[i].mask = dbmx2_mask_irq;
		irq_desc[i].unmask = dbmx2_unmask_irq;
	}

	/* Disable all interrupts initially. */
	/* In mx2 this is done in the bootloader. */
}
