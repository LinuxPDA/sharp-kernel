/*
 * linux/include/asm-sh/ms7727rp.h
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi MS7727RP02 support.
 */
#ifndef __ASM_SH_MS7727RP_H
#define __ASM_SH_MS7727RP_H 1

#include <asm/sh7727.h>

#define MRSHPC_IRQ		IRQ4_IRQ

#define MRSHPC_CTRL_BASE	0xb83fffe0
#define MRSHPC_MEM_BASE		0xb8400000
#define MRSHPC_ATTR_BASE	0xb8500000
#define MRSHPC_IO_BASE		0xb8600000

#define PA_MRSHPC		MRSHPC_CTRL_BASE
#define PA_MRSHPC_MW1		MRSHPC_MEM_BASE
#define PA_MRSHPC_MW2		MRSHPC_ATTR_BASE
#define PA_MRSHPC_IO		MRSHPC_IO_BASE

#define H8_CTRL_BASE		0xba000000

#define XPAR_H			(H8_CTRL_BASE + 0x0020)
#define XPAR_L			(H8_CTRL_BASE + 0x0021)
#define YPAR_H			(H8_CTRL_BASE + 0x0022)
#define YPAR_L			(H8_CTRL_BASE + 0x0023)
#define TPSR			(H8_CTRL_BASE + 0x0030)
#define TPCR			(H8_CTRL_BASE + 0x0031)
#define TPSCR			(H8_CTRL_BASE + 0x0032)

#define KEYCR			(H8_CTRL_BASE + 0x0040)
#define KCODER			(H8_CTRL_BASE + 0x0041)
#define KATIMER			(H8_CTRL_BASE + 0x0042)
#define KEYSR			(H8_CTRL_BASE + 0x0043)
#define RTKISR          	(H8_CTRL_BASE + 0x0090)

extern void h8_init(void);
extern unsigned short h8_read(unsigned int addr);
extern void h8_write(unsigned int addr, unsigned short value);

#endif /* __ASM_SH_MS7727RP_H */
