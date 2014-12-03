/*
 * include/asm-sh/io_ms7727rp.h
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * I/O functions for Hitachi MS7727RP02.
 */
#ifndef __ASM_SH_IO_MS7727RP_H
#define __ASM_SH_IO_MS7727RP_H

#include <asm/io_generic.h>

extern unsigned long  ms7727rp_isa_port2addr(unsigned long);
extern unsigned char  ms7727rp_inb(unsigned long);
extern unsigned char  ms7727rp_inb_p(unsigned long);
extern void           ms7727rp_insb(unsigned long, void*, unsigned long);
extern unsigned char  ms7727rp_readb(unsigned long);

#ifdef __WANT_IO_DEF

#define __inb		ms7727rp_inb
#define __inw		generic_inw
#define __inl		generic_inl
#define __outb		generic_outb
#define __outw		generic_outw
#define __outl		generic_outl

#define __inb_p		ms7727rp_inb_p
#define __inw_p		generic_inw_p
#define __inl_p		generic_inl
#define __outb_p	generic_outb_p
#define __outw_p	generic_outw
#define __outl_p	generic_outl

#define __insb		ms7727rp_insb
#define __insw		generic_insw
#define __insl		generic_insl
#define __outsb		generic_outsb
#define __outsw		generic_outsw
#define __outsl		generic_outsl

#define __readb		ms7727rp_readb
#define __readw		generic_readw
#define __readl		generic_readl
#define __writeb	generic_writeb
#define __writew	generic_writew
#define __writel	generic_writel

#define __isa_port2addr	ms7727rp_isa_port2addr
#define __ioremap	generic_ioremap
#define __iounmap	generic_iounmap

#endif

#endif /* _ASM_SH_IO_MS7727RP_H */
