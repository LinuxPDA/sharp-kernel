/*
 *  linux/arch/arm/mach-katana/leds.c
 *
 *  Integrator LED control routines
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (c) 2003 by MediaQ, Incorporated.
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
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/leds.h>
#include <asm/system.h>
#include <asm/mach-types.h>

#include <asm/arch/platform.h>	//HSU: Our register stuff

static int saved_leds;

static void katana_leds_event(led_event_t ledevt)
{
#define	MQ_TIMER_LED	0x1
#define	MQ_RED_LED	0x2
#define	MQ_GREEN_LED	0x4
#define	MQ_BLUE_LED	0x8
#define	MQ_AMBER_LED	0x10
#define	MQ_START_LED	0x80	//vs. STOP
#define	MQ_CLAIM_LED	0x40	//vs. RELEASE
#define	MQ_IDLE_LED	0x20	//IDLE START vs. END

	//LED is contained in SSP/I2C block (IC)
	unsigned long flags;
	volatile MQ9000REGS	*pMQReg = (MQ9000REGS *)MQ9000_REGS_VBASE;
        volatile _IC		*pIC = (_IC *)&(pMQReg->ic);

	unsigned int update_alpha_leds;
	
	// yup, change the LEDs
	local_irq_save(flags);
	update_alpha_leds = 0;

	switch(ledevt) {
	case led_start:
		saved_leds |= MQ_START_LED;
		update_alpha_leds = 1;
		break;

	case led_stop:
		saved_leds &= ~MQ_START_LED;
		update_alpha_leds = 1;
		break;

	case led_claim:
		saved_leds |= MQ_CLAIM_LED;
		update_alpha_leds = 1;
		break;

	case led_release:
		saved_leds &= MQ_CLAIM_LED;
		update_alpha_leds = 1;
		break;

	case led_idle_start:
		saved_leds |= MQ_IDLE_LED;
		update_alpha_leds = 1;
		break;

	case led_idle_end:
		saved_leds &= ~MQ_IDLE_LED;
		update_alpha_leds = 1;
		break;

	case led_timer:
		saved_leds ^= MQ_TIMER_LED;
		update_alpha_leds = 1;
		break;

	case led_red_on:
		saved_leds |= MQ_RED_LED;
		update_alpha_leds = 1;
		break;

	case led_red_off:
		saved_leds &= ~MQ_RED_LED;
		update_alpha_leds = 1;
		break;

	case led_green_on:
		saved_leds |= MQ_GREEN_LED;
		update_alpha_leds = 1;
		break;

	case led_green_off:
		saved_leds &= ~MQ_GREEN_LED;
		update_alpha_leds = 1;
		break;

	case led_blue_on:
		saved_leds |= MQ_BLUE_LED;
		update_alpha_leds = 1;
		break;

	case led_blue_off:
		saved_leds &= ~MQ_BLUE_LED;
		update_alpha_leds = 1;
		break;

	case led_amber_on:
		saved_leds |= MQ_AMBER_LED;
		update_alpha_leds = 1;
		break;

	case led_amber_off:
		saved_leds &= ~MQ_AMBER_LED;
		update_alpha_leds = 1;
		break;

	default:
		break;
	}

	if (update_alpha_leds) {
		//Reset MMU - temp code
		pIC->ic00.Control = 0x0;
		pIC->ic00.Control = MQ_I2C_INIT_STATE;
		// Setup I2C index and data
	#ifdef SET_ALPHA_LED
		pMQReg->it[0] = EDEV0_I2C_HEXLED | (saved_leds << 8);
		pIC->ic03.Transfer = 0x00010000 | EDEV0_I2C_ID;
	#else
		pMQReg->it[0] = CPU_I2C_LED | (saved_leds << 8);
		pIC->ic03.Transfer = 0x00010000 | CPU_I2C_ID;
	#endif
		while( !(pIC->ic05.Status & (BIT5 | BIT1)))
			barrier();
		//HSU: should we really check for status here???
	}
	local_irq_restore(flags);
}

//HSU static int __init leds_init(void)
int __init leds_init(void)
{
	if (machine_is_katana())
	{
#if 0	//Don't do any leds.  It's not working!!!
		leds_event = katana_leds_event;
#endif
	}

	return 0;
}

__initcall(leds_init);
