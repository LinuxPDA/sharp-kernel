/*
 * linux/arch/sh/kernel/sh7290_setup.c
 *
 * Copyright (c) 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi SH7290 processor support.
 */
#include <linux/config.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/sh7290.h>

#if 0
#define SCSSR0		0xA4430014
#define SCFTDR0		0xA4430020
#endif
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

void __init sh7290_setup(void)
{
	printk(KERN_INFO "Hitachi SH7290 processor support.\n");

	ctrl_outb(0x00,   STBCR);
	ctrl_outb(0x00,   STBCR2);
	ctrl_outb(0x00,   STBCR3);
	ctrl_outb(0x00,   STBCR4);
	ctrl_outb(0x00,   STBCR5);
}
