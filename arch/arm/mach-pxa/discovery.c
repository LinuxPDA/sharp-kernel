/*
 * linux/arch/arm/mach-pxa/discovery.c
 *
 * Copyright (C) 2002 Richard Rau (richard.rau@msa.hinet.net)
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
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Change Log
 *	31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/arch/hardware.h>
#include <asm/arch/discovery_asic.h>
#include <asm/arch/discovery_asic3.h>
#include <asm/arch/discovery_gpio.h>
#include <asm/arch/irqs.h>

static int discovery_egpio1 = 0x205e;
static int discovery_egpio2 = 0x9b;
static int discovery_bpgpio = 0x00;		

static void asic_init(void)
{

#if 1
/* begin : initialization for PWM */ 
	PGSR0 = 0x01008000; //Sleep State
//	PGSR1 = 0x01020802; //Sleep State 

#ifdef CONFIG_SABINAL_DISCOVERY_PreMV
	PGSR1 = 0x00160802; //Sleep State 
#else
	PGSR1 = 0x00020802; //Sleep State 
#endif

	PGSR2 = 0x0001C000; //Sleep State
	PRER = 0x00000001;
	PFER = 0x80000002;
	PEDR = 0x80000003;
	PWER = 0x80000003;
	PCFR = 0x00000001;
	GRER0 = (GRER0 | 1); //raising
	GFER0 = (GFER0 | 1); //failing
/* end : initialization for PWM */
#endif

/*	
	ASIC3_GPIO_MASK_A = 0xfc00;
	ASIC3_GPIO_MASK_B = 0xffff;
	ASIC3_GPIO_MASK_C = 0xffff;
	ASIC3_GPIO_MASK_D = 0xf000;
*/
	ASIC3_GPIO_MASK_A = 0xffff;
	ASIC3_GPIO_MASK_B = 0xffff;
	ASIC3_GPIO_MASK_C = 0xffff;
	ASIC3_GPIO_MASK_D = 0xffff;
	
	// 0: Input 1: Output
	ASIC3_GPIO_DIR_A = 0xfc00;
	ASIC3_GPIO_DIR_B = 0xffff;	
	ASIC3_GPIO_DIR_C = 0xffff;	
	ASIC3_GPIO_DIR_D = 0xf200;	
	
	ASIC3_GPIO_PIOD_A = 0x0000;
	ASIC3_GPIO_PIOD_B = 0x3000;
	ASIC3_GPIO_PIOD_C = 0x0480;
	ASIC3_GPIO_PIOD_D = 0x0000;
	
	ASIC3_GPIO_INTYP_A = 0xffff;
//	ASIC3_GPIO_INTYP_B = ;
//	ASIC3_GPIO_INTYP_C = ;
	ASIC3_GPIO_INTYP_D = 0xffff;
	
	ASIC3_GPIO_ETSEL_A = 0xffff;
//	ASIC3_GPIO_ETSEL_B = ;
//	ASIC3_GPIO_ETSEL_C = ;
	ASIC3_GPIO_ETSEL_D = 0x008c;
		
//	ASIC3_GPIO_LSEL_A = ; 
//	ASIC3_GPIO_LSEL_B = ;
//	ASIC3_GPIO_LSEL_C = ;
//	ASIC3_GPIO_LSEL_D = ;

	ASIC3_GPIO_INTSLP_A = 0xfdff;
//	ASIC3_GPIO_INTSLP_B = ;
//	ASIC3_GPIO_INTSLP_C = ;
	ASIC3_GPIO_INTSLP_D = 0xfff4;

//	ASIC3_GPIO_SLPOUT_A = ;
	ASIC3_GPIO_SLPOUT_B = 0x1000;
	ASIC3_GPIO_SLPOUT_C = 0x0080;
//	ASIC3_GPIO_SLPOUT_D = ;

//	ASIC3_GPIO_BFOUT_A = ;
	ASIC3_GPIO_BFOUT_B = 0x1000;
	ASIC3_GPIO_BFOUT_C = 0x0080;
//	ASIC3_GPIO_BFOUT_D = ;

	ASIC3_GPIO_ALT_A = 0xf800;
	ASIC3_GPIO_ALT_B = 0x0000;
	ASIC3_GPIO_ALT_C = 0x007f;
	ASIC3_GPIO_ALT_D = 0x0000;


	// Initial Interrupt
	ASIC3_INTR_INTCPS |= 0x0e ;
	ASIC3_CLOCK_CDEXCDCX |= EX0;
	ASIC3_CLOCK_CLK_SEL |= CX13;
	ASIC3_INTR_INTTBS = 0x0400;
	ASIC3_INTR_INTMASK |= INTMASK_GINT;
	ASIC3_INTR_INTMASK |= INTMASK_GINTEL;
	ASIC3_INTR_INTCPS |= INTCPS_SET;

}

void discovery_init(void)
{
	printk("discovery_init\n");

	// cpu initialize
	printk("initial MSCx...\n");
	MSC0 = 0x02da02da; //msc0	
	MSC1 = 0x7FFC7FFC; //msc1
	MSC2 = 0x7FF47FFC; //msc2

	// ASIC3 initialize
	ASIC3_GPIO_CONF_A = 0x01;
	ASIC3_GPIO_CONF_B = 0x01;
	ASIC3_GPIO_CONF_C = 0x01;
	ASIC3_GPIO_CONF_D = 0x01;
	printk("Initial ASIC3... \n");
	asic_init();
	
}

void clr_discovery_egpio1(u16 x)
{
//	discovery_egpio1 &= ~x;
//	DISCOVERY_EGPIO1 = discovery_egpio1;
}

void clr_discovery_egpio2(u8 x)
{
//	discovery_egpio2 &= ~x;
//	DISCOVERY_EGPIO2 = discovery_egpio2;
}

void clr_discovery_bpgpio(u8 x)
{
	discovery_bpgpio &= ~x;
	DISCOVERY_BPEGPIO = discovery_bpgpio;
}

void set_discovery_egpio1(u16 x)
{
//	discovery_egpio1 |= x;
//	DISCOVERY_EGPIO1 = discovery_egpio1;
}

void set_discovery_egpio2(u8 x)
{
//	discovery_egpio2 |= x;
//	DISCOVERY_EGPIO2 = discovery_egpio2;
}

void set_discovery_bpgpio(u8 x)
{
	discovery_bpgpio |= x;
	DISCOVERY_BPEGPIO = discovery_bpgpio;
}

u16 discovery_read_egpio1(void)
{
//	return discovery_egpio1;
}

u8 discovery_read_egpio2(void)
{
//	return discovery_egpio2;
}

u8 discovery_read_bpgpio(void)
{
	return discovery_bpgpio;
}

void set_ASIC1_GPIO_IRQ_edge(int gpio_mask, int edge)
{

}

void set_ASIC1_GPIO_IRQ_level(int gpio_mask, int level)
{

}

void resume_init(void)
{
	MSC0 = 0x02da02da; //msc0	
	MSC1 = 0x7FFC7FFC; //msc1
	MSC2 = 0x7FF47FFC; //msc2

//	GPDR0=0xDB828000;
//	GPDR1=0xFFB6A887;
//	GPDR2=0x0001FFFF;

	PGSR0 = 0x01008000; //Sleep State

#ifdef CONFIG_SABINAL_DISCOVERY_PreMV
	PGSR1 = 0x00160802; //Sleep State 
#else
	PGSR1 = 0x00020802; //Sleep State //Johny
#endif

	PGSR2 = 0x0001C000; //Sleep State
	
	GRER0 = (GRER0 | 1); //raising
	GFER0 = (GFER0 | 1); //failing

	ICLR = 0;
	
	ICMR |= (1 << 10); //bit10, gpio02_80 enable
	ICMR |= (1 << 8); //bit8, gpio00 enable
	
	ICCR = 1; //Only enabled and unmasked will bring the Cotulla out of IDLE mode.
	
	CKEN |= 0x03; 
	CKEN |= CKEN3_SSP;
	CKEN |= CKEN1_PWM1;

	
	// 0: Input 1: Output
	ASIC3_GPIO_DIR_A = 0xfc00;
	ASIC3_GPIO_DIR_B = 0xffff;	
	ASIC3_GPIO_DIR_C = 0xffff;	
	ASIC3_GPIO_DIR_D = 0xf200;	
	
	ASIC3_GPIO_INTYP_A = 0xffff;
	ASIC3_GPIO_INTYP_D = 0xffff;
//	ASIC3_GPIO_ETSEL_A = 0xffff;
//	ASIC3_GPIO_ETSEL_D = 0x008c;

		
//	ASIC3_GPIO_SLPOUT_A = ;
	ASIC3_GPIO_SLPOUT_B = 0x1000;
	ASIC3_GPIO_SLPOUT_C = 0x0080;
//	ASIC3_GPIO_SLPOUT_D = ;
//	ASIC3_GPIO_BFOUT_A = ;
	ASIC3_GPIO_BFOUT_B = 0x1000;
	ASIC3_GPIO_BFOUT_C = 0x0080;
//	ASIC3_GPIO_BFOUT_D = ;

	ASIC3_GPIO_ALT_A = 0xf800;
	ASIC3_GPIO_ALT_B = 0x0000;
	ASIC3_GPIO_ALT_C = 0x007f;
	ASIC3_GPIO_ALT_D = 0x0000;


	// Initial Interrupt
	ASIC3_INTR_INTCPS |= 0x0e ;
	ASIC3_CLOCK_CDEXCDCX |= EX0;
	ASIC3_CLOCK_CLK_SEL |= CX13;
	ASIC3_INTR_INTTBS =0x0400;
	ASIC3_INTR_INTMASK |= INTMASK_GINT;
	ASIC3_INTR_INTMASK |= INTMASK_GINTEL;
	ASIC3_INTR_INTCPS |= INTCPS_SET;


}

EXPORT_SYMBOL(discovery_init);
EXPORT_SYMBOL(clr_discovery_egpio1);
EXPORT_SYMBOL(clr_discovery_egpio2);
EXPORT_SYMBOL(clr_discovery_bpgpio);
EXPORT_SYMBOL(set_discovery_egpio1);
EXPORT_SYMBOL(set_discovery_egpio2);
EXPORT_SYMBOL(set_discovery_bpgpio);
EXPORT_SYMBOL(discovery_read_egpio1);
EXPORT_SYMBOL(discovery_read_egpio2);
EXPORT_SYMBOL(discovery_read_bpgpio);
EXPORT_SYMBOL(set_ASIC1_GPIO_IRQ_edge);
EXPORT_SYMBOL(set_ASIC1_GPIO_IRQ_level);

EXPORT_SYMBOL(resume_init);
