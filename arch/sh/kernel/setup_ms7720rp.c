/*
 * linux/arch/sh/kernel/setup_ms7720rp.c
 *
 * Copyright (c) 2004 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * setup routine for Hitachi MS7720RP01
 */
#include <linux/config.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/ms7720rp.h>
#include <asm/smc37c93x.h>

void ms7720rp_rtc_gettimeofday(struct timeval* tv)
{
	tv->tv_sec  = 0;
	tv->tv_usec = 0;
}

int ms7720rp_rtc_settimeofday(const struct timeval* tv)
{
	return 0;
}

int ms7720rp_irq_demux(int irq)
{
	return irq;

}

int __init setup_ms7720rp(void)
{

	printk(KERN_INFO "Hitachi MS7720RP01 support.\n");

	sh7720_setup();

        /*
         *  2: PCIRQ2
         *  6: PCIRQ1
         * 14: PCIRQ0
         */

	ctrl_outw(0x4000,ICR1);

        make_ipr_irq( 0, BCR_ILCR5, 1, 0x0f- 0); /* PCIRQ3 */
        make_ipr_irq(11, BCR_ILCR5, 0, 0x0f-11); /* PCIRQ2 */
        make_ipr_irq( 9, BCR_ILCR6, 1, 0x0f- 9); /* PCIRQ1 */
        make_ipr_irq( 7, BCR_ILCR6, 0, 0x0f- 7); /* PCIRQ0 */

	return 0;
}
