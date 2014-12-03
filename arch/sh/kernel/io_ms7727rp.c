/* 
 * linux/arch/sh/kernel/io_ms7727rp.c
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * I/O routine for Hitachi MS7727RP02.
 *
 */

#include <linux/config.h>

#include <asm/machvec.h>
#include <asm/io.h>
#include <asm/delay.h>

#include <asm/ms7727rp.h>

#define PORT2ADDR(x) (sh_mv.mv_isa_port2addr(x))

static void delay(void)
{
	ctrl_inw(0xa0000000);
}

unsigned long ms7727rp_isa_port2addr(unsigned long offset)
{
	return offset;
}

unsigned char ms7727rp_inb(unsigned long port)
{
	if ((port >= 0xb8400000)&&(port < 0xb8400000 + 0x00400000))
		return *(volatile unsigned char*)(port + 0x00040000); 

	return *(volatile unsigned char*)PORT2ADDR(port);
}

unsigned char ms7727rp_inb_p(unsigned long port)
{
	unsigned char v;

	if ((port >= 0xb8400000)&&(port < 0xb8400000 + 0x00400000))
		v = *(volatile unsigned char*)(port + 0x00040000); 
	else
		v = *(volatile unsigned char*)PORT2ADDR(port);

	delay();
	return v;
}

void ms7727rp_insb(unsigned long port, void* buffer, unsigned long count)
{
	unsigned char* p = (unsigned char*)buffer;

	while(count--) {
		*p++ = inb(port);
	}
}

unsigned char ms7727rp_readb(unsigned long port)
{
	if ((port >= 0xb8400000)&&(port < 0xb8400000 + 0x00400000))
		return *(volatile unsigned char*)(port + 0x00040000); 

	return *(volatile unsigned char*)PORT2ADDR(port);
}

unsigned short h8_read(unsigned int addr)
{
	delay(); udelay(100);

	/* address write */
	ctrl_outb((ctrl_inb(PJDR) & 0xC7), PJDR);
	ctrl_outw(addr, H8_CTRL_BASE);

	delay(); udelay(100);

	/* data read */
	ctrl_outb((ctrl_inb(PJDR) & 0xC7) | 0x10, PJDR);
	udelay(100);

	return ctrl_inw(H8_CTRL_BASE);
}

void h8_write(unsigned int addr, unsigned short value)
{
	delay(); udelay(100);

	/* address write */
	ctrl_outb((ctrl_inb(PJDR) & 0xC7), PJDR);
	ctrl_outw(addr, H8_CTRL_BASE);

	delay(); udelay(100);

	/* data write */
	ctrl_outb((ctrl_inb(PJDR) & 0xC7) | 0x08, PJDR);
	udelay(100);

	ctrl_outw(value, H8_CTRL_BASE);
}

void h8_init(void)
{
	ctrl_outw(0x0540, PJCR);

	h8_write(KEYCR,  0x0000); /* Key Controler */
	h8_write(TPCR,   0x0000); /* Touch panel controler */
	h8_write(KEYSR,  0x0000); /* Key Status */
	h8_write(TPSR,   0x0000); /* Touch panel Status */
	h8_write(RTKISR, 0x0000); /* Status */
}
