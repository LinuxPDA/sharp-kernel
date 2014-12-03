/*
 * linux/include/asm-arm/arch-dbmx2/serial.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 * include/asm-arm/arch-mx2ads/serial.h
 * (C) 1999 Nicolas Pitre <zhili@motorola.com>
 *
 * All this is intended to be used with a 16550-like UART on the mx1 ads's 
 * PCMCIA(CF) bus.  It has nothing to do with the mx1's internal serial ports.
 * This is included by serial.c
 */


/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD ( 1843200 / 16 )

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE 4


/*
 * Rather empty table...
 * Hardwired serial ports should be defined here.
 * PCMCIA will fill it dynamically.
 */ 
/* UART	CLK     	PORT		IRQ	FLAGS		*/ 
#define STD_SERIAL_PORT_DEFNS	\
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS }

#define EXTRA_SERIAL_PORT_DEFNS

