/*
 * linux/include/asm-sh/ms7290cp.h
 *
 * Copyright (c) 2002 Lineo Japan, Inc.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi MS7290CP01 support.
 */
#ifndef __ASM_SH_MS7290CP_H
#define __ASM_SH_MS7290CP_H 1

#include <asm/sh7290.h>

#define HD66770_INDEX		0xb8000000
#define HD66770_DATA		0xb8000004

#define MRSHPC_IRQ		IRQ4_IRQ

#define MRSHPC_CTRL_BASE	0xac3fffe0
#define MRSHPC_MEM_BASE		0xac400000
#define MRSHPC_ATTR_BASE	0xac500000
#define MRSHPC_IO_BASE		0xac600000

#define PA_MRSHPC		MRSHPC_CTRL_BASE
#define PA_MRSHPC_MW1		MRSHPC_MEM_BASE
#define PA_MRSHPC_MW2		MRSHPC_ATTR_BASE
#define PA_MRSHPC_IO		MRSHPC_IO_BASE

#define DEBUG_LED		0xb0000000
#define DIPSW			0xb1000000

void dbg_led(int, int);
int  dbg_led_r(int);

void dbg_putc(int);
void dbg_printk(const char*, ...);
void dump_a(unsigned long, unsigned long);

#endif /* __ASM_SH_MS7290CP_H */
