/*
 * linux/arch/sh/kernel/setup_ms7710se.c
 *
 * Copyright (c) 2004 Lineo Solutions, Inc.
 * Copyright (c) 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi MS7710SE support.
 */
#include <linux/config.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/ms7710se.h>
#include <asm/smc37c93x.h>

#if 0
void ms7710se_rtc_gettimeofday(struct timeval* tv)
{
	tv->tv_sec  = 0;
	tv->tv_usec = 0;
}

int ms7710se_rtc_settimeofday(const struct timeval* tv)
{
	return 0;
}
#endif

int ms7710se_irq_demux(int irq)
{
	return irq;

}
#if 0
int dbg_led_r(int pos)
{
	return ((ctrl_inw(DEBUG_LED) & (1 << (pos + 8))) ? 1 : 0);
}

void dbg_led(int pos, int f)
{
	if (f) {
		ctrl_outw(ctrl_inw(DEBUG_LED) | (1 << (pos + 8)), DEBUG_LED);
	}
	else {
		ctrl_outw(ctrl_inw(DEBUG_LED) & ~(1 << (pos + 8)), DEBUG_LED);
	}
}
#endif
/*
 * Configure the Super I/O chip
 */
static void __init smsc_config(int index, int data)
{
	outb_p(index, INDEX_PORT);
	outb_p(data, DATA_PORT);
}

static void __init init_smsc(void)
{
	outb_p(CONFIG_ENTER, CONFIG_PORT);
	outb_p(CONFIG_ENTER, CONFIG_PORT);
#if 0
	/* FDC */
	smsc_config(CURRENT_LDN_INDEX, LDN_FDC);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 6); /* IRQ6 */

	/* IDE1 */
	smsc_config(CURRENT_LDN_INDEX, LDN_IDE1);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 14); /* IRQ14 */

	/* AUXIO (GPIO): to use IDE1 */
	smsc_config(CURRENT_LDN_INDEX, LDN_AUXIO);
	smsc_config(GPIO46_INDEX, 0x00); /* nIOROP */
	smsc_config(GPIO47_INDEX, 0x00); /* nIOWOP */
#endif
        /* RTC */
        smsc_config(CURRENT_LDN_INDEX, LDN_RTC);
        smsc_config(ACTIVATE_INDEX, 0x01);
        smsc_config(IRQ_SELECT_INDEX, 8); /* IRQ8 */

        /* KBD */
        smsc_config(CURRENT_LDN_INDEX, LDN_KBC);
        smsc_config(ACTIVATE_INDEX, 0x01);
        smsc_config(IRQ_SELECT_INDEX, 1); /* IRQ1 */
        smsc_config(IRQ_SELECT_INDEX2, 12); /* IRQ12 */

        /* COM1 */
        smsc_config(CURRENT_LDN_INDEX, LDN_COM1);
        smsc_config(ACTIVATE_INDEX, 0x01);
        smsc_config(IO_BASE_HI_INDEX, 0x03);
        smsc_config(IO_BASE_LO_INDEX, 0xf8);
        smsc_config(IRQ_SELECT_INDEX, 4); /* IRQ4 */

        /* COM2 */
        smsc_config(CURRENT_LDN_INDEX, LDN_COM2);
        smsc_config(ACTIVATE_INDEX, 0x01);
        smsc_config(IO_BASE_HI_INDEX, 0x02);
        smsc_config(IO_BASE_LO_INDEX, 0xf8);
        smsc_config(IRQ_SELECT_INDEX, 3); /* IRQ3 */

        /* PARPORT */
        smsc_config(CURRENT_LDN_INDEX, LDN_PARPORT);
        smsc_config(ACTIVATE_INDEX, 0x01);
        smsc_config(IO_BASE_HI_INDEX, 0x03);
        smsc_config(IO_BASE_LO_INDEX, 0x78);
        smsc_config(IRQ_SELECT_INDEX, 5); /* IRQ5 */

	outb_p(CONFIG_EXIT, CONFIG_PORT);
}
int __init setup_ms7710se(void)
{

	printk(KERN_INFO "Hitachi MS7710SE support.\n");
#if 0
	{ short i;
	  for (i = 0; i <=0xc;i+=2)
		printk(" ADDR %08x:%04x\n",BCR_ILCRA+i,(short)(*((short *)(BCR_ILCRA+i))));
	}
#endif
	//sh7710se_setup();
        make_ipr_irq( 14, BCR_ILCRA, 2, 0);
        make_ipr_irq( 13, BCR_ILCRG, 0, 0);
        make_ipr_irq( 12, BCR_ILCRG, 1, 0);
        make_ipr_irq( 11, BCR_ILCRB, 0, 0);
        make_ipr_irq( 10, BCR_ILCRG, 2, 0);
        make_ipr_irq(  9, BCR_ILCRG, 3, 0);
        make_ipr_irq(  8, BCR_ILCRC, 1, 0);
        make_ipr_irq(  6, BCR_ILCRA, 1, 0);
        make_ipr_irq(  5, BCR_ILCRF, 1, 0);
        make_ipr_irq(  4, BCR_ILCRC, 0, 0);
        make_ipr_irq(  2, BCR_ILCRD, 3, 0);
        make_ipr_irq(  1, BCR_ILCRF, 3, 0);
#if 0
        make_ipr_irq( 3, BCR_ILCRE, 2, 0x0f - 3); /* PCIRQ2 */
        make_ipr_irq( 7, BCR_ILCRE, 1, 0x0f - 7); /* PCIRQ1 */
        make_ipr_irq(15, BCR_ILCRE, 0, 0x0f -15); /* PCIRQ0 */
#endif
        make_ipr_irq( 3, BCR_ILCRE, 2, 0); /* PCIRQ2 */
        make_ipr_irq( 7, BCR_ILCRE, 1, 0x0f - 7); /* PCIRQ1 */
        make_ipr_irq(15, BCR_ILCRE, 0, 0); /* PCIRQ0 */
        /*
         * Super I/O (Just mimic PC):
         *  2: keyboard
         *  4: serial 1
         *  6: mouse
         *  8: serial 0
         * 11: printer
         * 14: ide0
         */ 

	init_smsc();


	ctrl_outw(0xFF00, DEBUG_LED);

	return 0;
}
