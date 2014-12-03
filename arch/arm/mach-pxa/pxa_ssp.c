/*
 * linux/arch/arm/mach-pxa/pxa_ssp.c
 *
 * SSP read routines for discovery/poodle/corgi (SHARP)
 *
 * Copyright (C) 2002  LINEO Japan, Inc.
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
 * Change Log
 *  12-Dec-2002 Sharp Corporation for Poodle and Corgi
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/hardware.h>
#if defined(CONFIG_ARCH_PXA_CORGI)
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/corgi.h>
#endif

static spinlock_t pxa_ssp_lock = SPIN_LOCK_UNLOCKED;
static unsigned long flag;

unsigned long ssp_get_dac_val(ulong data, int cs)
{
	unsigned long ret;

	spin_lock_irqsave(&pxa_ssp_lock, flag);

#if defined(CONFIG_ARCH_PXA_CORGI)
	switch (cs) {
#if 0
	case CS_MAX1111:
		disable_irq(IRQ_GPIO_TP_INT);
		SSCR0 = 0x387;
		GPSR0 = GPIO_bit(GPIO19_DREQ1); /* TG */
		GPCR0 = GPIO_bit(GPIO20_DREQ0); /* MAX1111 */
		GPSR0 = GPIO_bit(GPIO24_SFRM); /* ADS7846 */
		break;
#endif
	case CS_ADS7846:
		SSCR0 = 0xab;
		GPSR0 = GPIO_bit(GPIO19_DREQ1); /* TG */
		GPSR0 = GPIO_bit(GPIO20_DREQ0); /* MAX1111 */
		GPCR0 = GPIO_bit(GPIO24_SFRM); /* ADS7846 */
		break;
	default:
		spin_unlock_irqrestore(&pxa_ssp_lock, flag);
		return 0;
	}
#endif

	SSDR = data;
	while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
	udelay(1);
	while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
	ret = (SSDR);

#if defined(CONFIG_ARCH_PXA_CORGI)
#if 0
	if (cs == CS_MAX1111) {
		GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);
		enable_irq(IRQ_GPIO_TP_INT);
	}
	SSCR0 = 0x1ab;
#endif
#endif
	spin_unlock_irqrestore(&pxa_ssp_lock, flag);
	return ret;
}

#define TG_GPIO_GAFR0_MASK         0x000cc0c0

#if defined(CONFIG_ARCH_PXA_CORGI)
unsigned long ssp_put_dac_val(ulong data, int cs)
{
	unsigned long ret;
    int on_value = 0, i;

	spin_lock_irqsave(&pxa_ssp_lock, flag);

	switch (cs) {
	case CS_ADS7846:
		SSCR0 = 0xab;
		GPSR0 = GPIO_bit(GPIO19_DREQ1); /* TG */
		GPSR0 = GPIO_bit(GPIO20_DREQ0); /* MAX1111 */
		GPCR0 = GPIO_bit(GPIO24_SFRM); /* ADS7846 */
		SSDR = data;
		while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
		break;
	case CS_LZ9JG18:
		GPDR0 = (GPDR0 & ~GPIO_bit(GPIO26_SRXD))|
			GPIO_bit(GPIO23_SCLK)|GPIO_bit(GPIO25_STXD);
		GAFR0_U &= ~(TG_GPIO_GAFR0_MASK | 0x30300);
		GPSR0 = GPIO_bit(20)|GPIO_bit(24);
		GPCR0 = GPIO_bit(19)|GPIO_bit(23)|GPIO_bit(25)|GPIO_bit(26);
		udelay(10);
		for( i = 0; i < 8; i++){
			if(data&0x80)
				GPSR0 = GPIO_bit(25); /* SDO=H */
			else
				GPCR0 = GPIO_bit(25); /* SDO=L */
			udelay(10);
			GPSR0 = GPIO_bit(23); /* SCK=H */
			udelay(10);
			GPCR0 = GPIO_bit(23); /* SCK=L */
			udelay(10);
			data <<= 1;
		}
		GPCR0 = GPIO_bit(25); /* SDO=L */
		GPSR0 = GPIO_bit(19); /* CSB=H */
		GAFR0_U = (GAFR0_U & ~TG_GPIO_GAFR0_MASK)|0x88000;
		spin_unlock_irqrestore(&pxa_ssp_lock, flag);
		break;
	default:
		spin_unlock_irqrestore(&pxa_ssp_lock, flag);
		return 0;
	}
	return ret;
}

unsigned long ssp_only_get_dac_val(int cs)
{
	unsigned long ret;

	switch (cs) {
	case CS_ADS7846:
		break;
	default:
		return 0;
	}
	while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
	ret = (SSDR);

	SSCR0 = 0x1ab;

	spin_unlock_irqrestore(&pxa_ssp_lock, flag);
	return ret;
}
#endif

void pxa_ssp_init(void)
{
	/* initialize SSP */
	CKEN |= CKEN3_SSP;
#if defined(CONFIG_ARCH_PXA_POODLE)
	SSCR0 = 0x11ab;
#else
	SSCR0 = 0x1ab;
#endif
	SSCR1 = 0;

#if defined(CONFIG_ARCH_PXA_CORGI)
	/* CS control disable all */
	GPDR0 |= GPIO_bit(GPIO19_DREQ1); /* output */
	GPSR0 = GPIO_bit(GPIO19_DREQ1); /* High */
	GPDR0 |= GPIO_bit(GPIO20_DREQ0); /* output */
	GPSR0 = GPIO_bit(GPIO20_DREQ0); /* High */
	GPDR0 |= GPIO_bit(GPIO24_SFRM); /* output */
	GPSR0 = GPIO_bit(GPIO24_SFRM); /* High */
#endif
}

#if defined(CONFIG_ARCH_PXA_CORGI)
int ssp_get_max1111_val(ulong data)
{
    unsigned long ret;
    volatile unsigned int dummy;
    int voltage,voltage1,voltage2;

    spin_lock_irqsave(&pxa_ssp_lock, flag);

//
    SSCR0 = 0x387;
    GPSR0 = GPIO_bit(GPIO19_DREQ1); /* TG */
    GPCR0 = GPIO_bit(GPIO20_DREQ0); /* MAX1111 */
    GPSR0 = GPIO_bit(GPIO24_SFRM); /* ADS7846 */

    udelay(1);

// TB1/RB1
    SSDR = data;
    while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
    udelay(1);
    while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
    dummy = SSDR;

// TB12/RB2
    SSDR = 0;
    while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
    udelay(1);
    while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
    voltage1 = SSDR;

// TB13/RB3
    SSDR = 0;
    while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
    udelay(1);
    while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
    voltage2 = SSDR;

//
    GPSR0 = GPIO_bit(GPIO19_DREQ1); /* TG */
    GPSR0 = GPIO_bit(GPIO20_DREQ0); /* MAX1111 */
    GPSR0 = GPIO_bit(GPIO24_SFRM); /* ADS7846 */
    SSCR0 = 0;

//
    if (voltage1 & 0xc0 || voltage2 & 0x3f){
	voltage = -1;
    }else{
  	voltage = ((voltage1 << 2) & 0xfc) | ((voltage2 >> 6) & 0x03);
    }

//
    spin_unlock_irqrestore(&pxa_ssp_lock, flag);
    return voltage;
}
#endif

#if defined(CONFIG_ARCH_PXA_CORGI)
EXPORT_SYMBOL(ssp_get_max1111_val);
#endif
EXPORT_SYMBOL(ssp_get_dac_val);
EXPORT_SYMBOL(pxa_ssp_init);
