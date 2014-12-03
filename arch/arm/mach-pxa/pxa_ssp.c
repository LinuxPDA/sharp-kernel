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

#if defined(CONFIG_ARCH_PXA_SPITZ)
#define	GPIO_TG_CS	53
#define	GPIO_MAX1111_CS	20
#else
#define	GPIO_TG_CS	19
#define	GPIO_MAX1111_CS	20
#endif

#if defined(CONFIG_CPU_PXA27X)
#define	GPIO_SSP_SCLK	GPIO19_SCLK
#define	GPIO_SSP_SFRM	GPIO14_SFRM
#define	GPIO_SSP_TXD	GPIO87_STXD
#define	GPIO_SSP_RXD	GPIO86_SRXD
#else
#define	GPIO_SSP_SCLK	GPIO23_SCLK
#define	GPIO_SSP_SFRM	GPIO24_SFRM
#define	GPIO_SSP_TXD	GPIO25_STXD
#define	GPIO_SSP_RXD	GPIO26_SRXD
#endif

#if defined(CONFIG_ARCH_PXA_SPITZ)
#define	SSCR0_MAX1111	0x1b87
#define	SSCR0_ADS7846	0x06ab
#define	SSCR0_LZ9JG18	0x01ab
#else
#define	SSCR0_MAX1111	0x0387
#define	SSCR0_ADS7846	0x00ab
#define	SSCR0_LZ9JG18	0x01ab
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
		SSCR0 = SSCR0_MAX1111;
		GPSR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS); /* TG */
		GPCR(GPIO_MAX1111_CS) = GPIO_bit(GPIO_MAX1111_CS); /* MAX1111 */
		GPSR(GPIO_SSP_SFRM) = GPIO_bit(GPIO_SSP_SFRM); /* ADS7846 */
		break;
#endif
	case CS_ADS7846:
	        SSCR0 = 0;
		SSCR0 = SSCR0_ADS7846;
		GPSR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS); /* TG */
		GPSR(GPIO_MAX1111_CS) = GPIO_bit(GPIO_MAX1111_CS); /* MAX1111 */
		GPCR(GPIO_SSP_SFRM) = GPIO_bit(GPIO_SSP_SFRM); /* ADS7846 */
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
	SSCR0 = SSCR0_LZ9JG18;
#endif
#endif
	spin_unlock_irqrestore(&pxa_ssp_lock, flag);
	return ret;
}

#if defined(CONFIG_ARCH_PXA_CORGI)
unsigned long ssp_put_dac_val(ulong data, int cs)
{
	unsigned long ret;
    int on_value = 0, i;

	spin_lock_irqsave(&pxa_ssp_lock, flag);

	switch (cs) {
	case CS_ADS7846:
	        SSCR0 = 0;
		SSCR0 = SSCR0_ADS7846;
		GPSR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS); /* TG */
		GPSR(GPIO_MAX1111_CS) = GPIO_bit(GPIO_MAX1111_CS); /* MAX1111 */
		GPCR(GPIO_SSP_SFRM) = GPIO_bit(GPIO_SSP_SFRM); /* ADS7846 */
		SSDR = data;
		while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
		break;
	case CS_LZ9JG18:
		GPDR(GPIO_SSP_SCLK) |=  GPIO_bit(GPIO_SSP_SCLK);
		GPDR(GPIO_SSP_TXD) |=  GPIO_bit(GPIO_SSP_TXD);
		GPDR(GPIO_SSP_RXD) &= ~GPIO_bit(GPIO_SSP_RXD);
		GAFR(GPIO_TG_CS) &= ~GAFR_bit(GPIO_TG_CS);
		GAFR(GPIO_MAX1111_CS) &= ~GAFR_bit(GPIO_MAX1111_CS);
		GAFR(GPIO_SSP_SCLK) &= ~GAFR_bit(GPIO_SSP_SCLK);
		GAFR(GPIO_SSP_SFRM) &= ~GAFR_bit(GPIO_SSP_SFRM);
		GAFR(GPIO_SSP_TXD) &= ~GAFR_bit(GPIO_SSP_TXD);
		GPSR(GPIO_MAX1111_CS) = GPIO_bit(GPIO_MAX1111_CS);
		GPSR(GPIO_SSP_SFRM) = GPIO_bit(GPIO_SSP_SFRM);
		GPCR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS);
		GPCR(GPIO_SSP_SCLK) = GPIO_bit(GPIO_SSP_SCLK);
		GPCR(GPIO_SSP_TXD) = GPIO_bit(GPIO_SSP_TXD);
		GPCR(GPIO_SSP_RXD) = GPIO_bit(GPIO_SSP_RXD);
		udelay(10);
		for( i = 0; i < 8; i++){
			if(data&0x80)
				GPSR(GPIO_SSP_TXD) = GPIO_bit(GPIO_SSP_TXD); /* SDO=H */
			else
				GPCR(GPIO_SSP_TXD) = GPIO_bit(GPIO_SSP_TXD); /* SDO=L */
			udelay(10);
			GPSR(GPIO_SSP_SCLK) = GPIO_bit(GPIO_SSP_SCLK); /* SCK=H */
			udelay(10);
			GPCR(GPIO_SSP_SCLK) = GPIO_bit(GPIO_SSP_SCLK); /* SCK=L */
			udelay(10);
			data <<= 1;
		}
		GPCR(GPIO_SSP_TXD) = GPIO_bit(GPIO_SSP_TXD); /* SDO=L */
		GPSR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS); /* CSB=H */
#if defined(CONFIG_CPU_PXA27X)
		GAFR(GPIO_SSP_SCLK) |= GAFR_ALT_FN_1(GPIO_SSP_SCLK);
		GAFR(GPIO_SSP_TXD) |= GAFR_ALT_FN_1(GPIO_SSP_TXD);
#else
		GAFR(GPIO_SSP_SCLK) |= GAFR_ALT_FN_2(GPIO_SSP_SCLK);
		GAFR(GPIO_SSP_TXD) |= GAFR_ALT_FN_2(GPIO_SSP_TXD);
#endif
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

	SSCR0 = SSCR0_LZ9JG18;

	GPSR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS); /* TG */
	GPSR(GPIO_MAX1111_CS) = GPIO_bit(GPIO_MAX1111_CS); /* MAX1111 */
	GPSR(GPIO_SSP_SFRM) = GPIO_bit(GPIO_SSP_SFRM); /* ADS7846 */

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
	SSCR0 = SSCR0_LZ9JG18;
#endif
	SSCR1 = 0;

#if defined(CONFIG_ARCH_PXA_CORGI)
	/* CS control disable all */
	GPDR(GPIO_TG_CS) |= GPIO_bit(GPIO_TG_CS); /* output */
	GPSR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS); /* High */
	GPDR(GPIO_MAX1111_CS) |= GPIO_bit(GPIO_MAX1111_CS); /* output */
	GPSR(GPIO_MAX1111_CS) = GPIO_bit(GPIO_MAX1111_CS); /* High */
	GPDR(GPIO_SSP_SFRM) |= GPIO_bit(GPIO_SSP_SFRM); /* output */
	GPSR(GPIO_SSP_SFRM) = GPIO_bit(GPIO_SSP_SFRM); /* High */
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
    SSCR0 = 0;
    SSCR0 = SSCR0_MAX1111;

    GPSR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS); /* TG */
    GPCR(GPIO_MAX1111_CS) = GPIO_bit(GPIO_MAX1111_CS); /* MAX1111 */
    GPSR(GPIO_SSP_SFRM) = GPIO_bit(GPIO_SSP_SFRM); /* ADS7846 */

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
    GPSR(GPIO_TG_CS) = GPIO_bit(GPIO_TG_CS); /* TG */
    GPSR(GPIO_MAX1111_CS) = GPIO_bit(GPIO_MAX1111_CS); /* MAX1111 */
    GPSR(GPIO_SSP_SFRM) = GPIO_bit(GPIO_SSP_SFRM); /* ADS7846 */
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
