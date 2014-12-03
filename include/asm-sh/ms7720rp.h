/*
 * linux/include/asm-sh/ms7720rp.h
 *
 * Copyright (c) 2004 Lineo Solutions, Inc.
 * Copyright (c) 2002 Lineo Japan, Inc.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi MS7720RP01 support
 */
#ifndef __ASM_SH_MS7720RP_H
#define __ASM_SH_MS7720RP_H 1

#include <asm/sh7720.h>

#define MRSHPC_IRQ		7

#define PA_BCR          	0xb7000000      /* FPGA */

#define MRSHPC_CTRL_BASE	0xb83fffe0
#define MRSHPC_MEM_BASE		0xb8400000
#define MRSHPC_ATTR_BASE	0xb8500000
#define MRSHPC_IO_BASE		0xb8600000

#define PA_MRSHPC		MRSHPC_CTRL_BASE
#define PA_MRSHPC_MW1		MRSHPC_MEM_BASE
#define PA_MRSHPC_MW2		MRSHPC_ATTR_BASE
#define PA_MRSHPC_IO		MRSHPC_IO_BASE
#define MRSHPC_OPTION   (PA_MRSHPC + 6)
#define MRSHPC_CSR      (PA_MRSHPC + 8)
#define MRSHPC_ISR      (PA_MRSHPC + 10)
#define MRSHPC_ICR      (PA_MRSHPC + 12)
#define MRSHPC_CPWCR    (PA_MRSHPC + 14)
#define MRSHPC_MW0CR1   (PA_MRSHPC + 16)
#define MRSHPC_MW1CR1   (PA_MRSHPC + 18)
#define MRSHPC_IOWCR1   (PA_MRSHPC + 20)
#define MRSHPC_MW0CR2   (PA_MRSHPC + 22)
#define MRSHPC_MW1CR2   (PA_MRSHPC + 24)
#define MRSHPC_IOWCR2   (PA_MRSHPC + 26)
#define MRSHPC_CDCR     (PA_MRSHPC + 28)
#define MRSHPC_PCIC_INFO (PA_MRSHPC + 30)
                                                                                
#define BCR_ILCR1       (PA_BCR + 2)
#define BCR_ILCR2       (PA_BCR + 3)
#define BCR_ILCR3       (PA_BCR + 4)
#define BCR_ILCR4       (PA_BCR + 5)
#define BCR_ILCR5       (PA_BCR + 6)
#define BCR_ILCR6       (PA_BCR + 7)
#define BCR_ILCR7       (PA_BCR + 8)
#define BCR_ILCR8       (PA_BCR + 9)
                                                                                
void dbg_putc(int);
void dbg_printk(const char*, ...);
void dump_a(unsigned long, unsigned long);

#endif /* __ASM_SH_MS7720RP_H */
