/*
 * linux/arch/sh/kernel/sh7720_setup.c
 *
 * Copyright (c) 2004 Lineo Solutions, Inc.
 * Copyright (c) 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi SH7720 processor support.
 */
#include <linux/config.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/ms7720rp.h>
#include <asm/smc37c93x.h>

void dbg_putc(int ch)
{
	while((ctrl_inw(SCSSR0) & 0x0060) ==0) {}
	ctrl_outb(ch, SCFTDR0);
	ctrl_outw(ctrl_inw(SCSSR0) & 0x039F, SCSSR0);
}

void dbg_printk(const char* fmt, ...)
{
	char tmp[256]; int i;
	va_list arg;

	va_start(arg, fmt);
	vsprintf(tmp, fmt, arg);
	va_end(arg);
	for (i = 0; tmp[i]; i++) {
		if (tmp[i] == '\n') {
			dbg_putc('\r');
		}
		dbg_putc(tmp[i]);
	}
}

void dump_a(unsigned long start, unsigned long size)
{
	unsigned long p, n, a, q, ch;
	char tmp[64];

	p = start;
	n = size;
	for (a = 0; a < n ; a++) {
		q = a % 16;
		if (a % 16 == 0) {
			dbg_printk("%08X : ", p + a);
		}
		ch = *((unsigned char*)p + a);
		if (q == 7) {
			dbg_printk("%02X-", ch);
		}
		else {
			dbg_printk("%02X ", ch);
		}
		if ((ch >= 0x20)&&(ch < 0x7F)) {
			tmp[q] = ch; tmp[q+1] = 0;
		}
		else {
			tmp[q] = '.'; tmp[q+1] = 0;
		}
		if (a % 16 == 15) {
			dbg_printk(": %s\n", tmp);
		}
	}
	q = a % 16;
	if (q != 0) {
		q = 16 - q;
		while (q-- > 0) { dbg_printk("   "); }
		dbg_printk(": %s\n", tmp);
	}
}

#if defined(CONFIG_SH7720_DMA)
extern void sh7720_dma_init(void);
#endif

void __init sh7720_setup(void)
{
	printk(KERN_INFO "Hitachi SH7720 processor support.\n");

	/* Port Init */
	ctrl_outw(0x0000,PCCR);		/* LCDC		*/
	ctrl_outw(0x0000,PDCR);		/* LCDC		*/
	ctrl_outw(0x0000,PECR);		/* LCDC & I2C	*/
	ctrl_outw(0x0000,PSCR);		/* SIOF0	*/
	ctrl_outw(0x0000,PUCR);		/* SIOF1	*/

#if defined(CONFIG_SH7720_DMA)
	sh7720_dma_init();
#endif

}
