/*
 * include/asm-arm/arch-cotulla/serial.h
 *
 * Change Log
 *  22-Jul-2002 SHARP Corporation: 
 *              Change file name from 'serial.h' to 'serial_pxa200.h'
 */

#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <asm/arch/cotulla.h>
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

#ifdef CONFIG_ARCH_COTULLA

#define RS_TABLE_SIZE 3

#if defined(CONFIG_UART0_DFLT_CONSOLE)

#define STD_SERIAL_PORT_DEFNS			\
       /* UART CLK      PORT        IRQ        FLAGS        */		\
	{ 0, BASE_BAUD, 0, IRQ_BTUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	COTULLA_UART2_BASE, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, /* ttyS1 */ \
	{ 0, BASE_BAUD, 0, IRQ_STUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	COTULLA_UART3_BASE, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, /* ttyS2 */ \
	{ 0, BASE_BAUD, 0, 0, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	0, 2, 0, 0, {}, {}, {}, 0, 0}

#elif defined(CONFIG_UART1_DFLT_CONSOLE)

#define STD_SERIAL_PORT_DEFNS			\
	{ 0, BASE_BAUD, 0, IRQ_BTUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	COTULLA_UART2_BASE, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_STUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	COTULLA_UART3_BASE, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, 0, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	0, 2, 0, 0, {}, {}, {}, 0, 0}

#else

#define STD_SERIAL_PORT_DEFNS			\
	{ 0, BASE_BAUD, 0, IRQ_STUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	COTULLA_UART3_BASE, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_BTUART, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	COTULLA_UART2_BASE, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, 0, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	0, 2, 0, 0, {}, {}, {}, 0, 0}

#endif

#endif // CONFIG_ARCH_COTULLA


#define EXTRA_SERIAL_PORT_DEFNS

#endif
