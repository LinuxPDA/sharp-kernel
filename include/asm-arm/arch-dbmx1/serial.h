/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-dbmx1/serial.h
 *
 */

#define BASE_BAUD     (8192000/16)
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF|ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE 2
#define STD_SERIAL_PORT_DEFNS \
/* UART	CLK     	PORT		IRQ		FLAGS	*/ \
{ 0,	BASE_BAUD,	0,		0,	STD_COM_FLAGS }, \
{ 0,	BASE_BAUD,	0,		0,	STD_COM_FLAGS }, \

#define EXTRA_SERIAL_PORT_DEFNS
