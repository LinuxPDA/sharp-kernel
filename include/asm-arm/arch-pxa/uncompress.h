/*
 * linux/include/asm-arm/arch-pxa/uncompress.h
 *  
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Change Log
 *	31-Jul-2002 Lineo Japan, Inc.
 *	12-Dec-2002 Sharp Corporation for Poodle and Corgi
 */

#define FFUART		((volatile unsigned long *)0x40100000)
#define BTUART		((volatile unsigned long *)0x40200000)
#define STUART		((volatile unsigned long *)0x40700000)

#if defined(CONFIG_SABINAL_DISCOVERY)
#define UART		BTUART
#else
#define UART		FFUART
#endif


static __inline__ void putc(char c)
{
	while (!(UART[5] & 0x20));
	UART[0] = c;
}

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
  return;
#endif

	while (*s) {
		putc(*s);
		if (*s == '\n')
			putc('\r');
		s++;
	}
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
