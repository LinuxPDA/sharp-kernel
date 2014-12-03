/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/serial.h
 *
 */

#define	UART1_BASE	((void *)0xf8000400)
#define	UART2_BASE	((void *)0xf8000420)

#define BASE_BAUD     (24000000/16)
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF|ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE 3
#define STD_SERIAL_PORT_DEFNS \
       /* UART	CLK     	PORT		IRQ	FLAGS		*/ \
	{ 0, BASE_BAUD, 0, IRQ_UART1, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	UART1_BASE, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, IRQ_UART2, STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	UART2_BASE, 2, 0, 0, {}, {}, {}, SERIAL_IO_MEM, NULL}, \
	{ 0, BASE_BAUD, 0, 0,         STD_COM_FLAGS, 0, 0, 0, 0, 0, 0, 0, \
	0, 0, 0, 0, {}, {}, {}, 0, NULL}, \

#define EXTRA_SERIAL_PORT_DEFNS
