/* 
 * linux/arch/sh/kernel/io_ms7290cp.c
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * I/O routine for Hitachi MS7290CP01.
 *
 */

#include <linux/config.h>

#include <asm/machvec.h>
#include <asm/io.h>
#include <asm/delay.h>

#include <asm/ms7290cp.h>

#define PORT2ADDR(x) (sh_mv.mv_isa_port2addr(x))

static void delay(void)
{
	ctrl_inw(0xa0000000);
}

unsigned long ms7290cp_isa_port2addr(unsigned long offset)
{
	return offset;
}

unsigned char ms7290cp_inb(unsigned long port)
{
	if ((port >= MRSHPC_MEM_BASE)&&(port < MRSHPC_MEM_BASE + 0x00400000))
		return *(volatile unsigned char*)(port + 0x00040000); 

	return *(volatile unsigned char*)PORT2ADDR(port);
}

unsigned char ms7290cp_inb_p(unsigned long port)
{
	unsigned char v;

	if ((port >= MRSHPC_MEM_BASE)&&(port < MRSHPC_MEM_BASE + 0x00400000))
		v = *(volatile unsigned char*)(port + 0x00040000); 
	else
		v = *(volatile unsigned char*)PORT2ADDR(port);

	return v;
}

void ms7290cp_insb(unsigned long port, void* buffer, unsigned long count)
{
	unsigned char* p = (unsigned char*)buffer;

	while(count--) {
		*p++ = inb(port);
	}
}

unsigned char ms7290cp_readb(unsigned long port)
{
	if ((port >= MRSHPC_MEM_BASE)&&(port < MRSHPC_MEM_BASE + 0x00400000))
		return *(volatile unsigned char*)(port + 0x00040000); 

	return *(volatile unsigned char*)PORT2ADDR(port);
}
