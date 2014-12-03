/*
 * include/asm-arm/arch-pxa/serial_pxa200.h
 *      
 * Change Log
 *	12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *	26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 */

#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <linux/config.h>

/*
 * This assumes you have a 14.7456 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD ( 14745600 / 16 )

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE 3

#if defined(CONFIG_SABINAL_DISCOVERY)
#define CONFIG_UART1_DFLT_CONSOLE
#elif defined(CONFIG_ARCH_PXA_POODLE)
#define CONFIG_UART2_DFLT_CONSOLE
#elif defined(CONFIG_ARCH_PXA_CORGI)
#define CONFIG_UART2_DFLT_CONSOLE
#elif defined(CONFIG_ARCH_PXA_TOSA)
#define CONFIG_UART2_DFLT_CONSOLE
#endif
#if defined(CONFIG_UART0_DFLT_CONSOLE)

#define STD_SERIAL_PORT_DEFNS			\
       /* UART CLK      PORT        IRQ        FLAGS        */		\
	{ 0, BASE_BAUD, 0, IRQ_FFUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&FFUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, /* ttyS0 */ \
	{ 0, BASE_BAUD, 0, IRQ_BTUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&BTUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, /* ttyS1 */ \
	{ 0, BASE_BAUD, 0, IRQ_STUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&STUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, /* ttyS2 */ \
	{ 0, BASE_BAUD, 0, 0, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	0, 2, 0, 0, {}, {}, {}, 0, NULL} /* ttyS3 */

#elif defined(CONFIG_UART1_DFLT_CONSOLE)

#define STD_SERIAL_PORT_DEFNS			\
	{ 0, BASE_BAUD, 0, IRQ_BTUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&BTUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_STUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&STUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_FFUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&FFUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, 0, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	0, 2, 0, 0, {}, {}, {}, 0, NULL}

#elif defined(CONFIG_UART2_DFLT_CONSOLE)

#define STD_SERIAL_PORT_DEFNS			\
	{ 0, BASE_BAUD, 0, IRQ_FFUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&FFUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_STUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&STUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_BTUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&BTUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, 0, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	0, 2, 0, 0, {}, {}, {}, 0, NULL}

#else

#define STD_SERIAL_PORT_DEFNS			\
	{ 0, BASE_BAUD, 0, IRQ_STUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&STUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_FFUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&FFUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_BTUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	(void *)&BTUART, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, 0, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	0, 2, 0, 0, {}, {}, {}, 0, NULL}

#endif

#define EXTRA_SERIAL_PORT_DEFNS

#endif
