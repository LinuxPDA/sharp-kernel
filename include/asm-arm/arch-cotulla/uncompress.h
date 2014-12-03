/*
 * linux/include/asm/arch-cotulla/uncompress.h
 * 
 * Copyright (c) 2001 Lineo, Inc. 
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *  linux/include/asm-arm/arch-iop80310/uncompress.h
 *
 * Change Log
 *	26-Jun-2002 SHARP	Cut Start MSG
 */

//#ifdef CONFIG_ARCH_IQ80310
//#define UART1_BASE    ((volatile unsigned char *)0xfe800000)
//#define UART2_BASE    ((volatile unsigned char *)0xfe810000)
//#endif
//static __inline__ void putc(char c)
//{
//	while ((UART2_BASE[5] & 0x60) != 0x60);
//	UART2_BASE[0] = c;
//}

#define UART1_BASE    ((volatile unsigned char *)0x40200000)
#define UART2_BASE    ((volatile unsigned char *)0x40700000)

static __inline__ void putc(char c)
{
	while ((UART1_BASE[20] & 0x40) == 0x0);
	UART1_BASE[0] = c;
}

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
#ifdef CONFIG_SABINAL_DISCOVERY
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
