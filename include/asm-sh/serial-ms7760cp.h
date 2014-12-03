/*
 * include/asm-sh/serial-ms7760cp.h
 * Copyright (c) 2003 Lineo Solutions, Inc.
 * 
 * Based on include/asm-sh/serial-7760se.h
 * SH7760 Solution Engine2 version: Copyright (C) 2003 Takeo Takahashi
 *
 * Configuration details for SH7760 Solution Engine serial port
 */

#ifndef __ASM_SERIAL_MS7760CP_H
#define __ASM_SERIAL_MS7760CP_H

#include <asm/io.h>
#include <asm/ms7760cp.h>

#undef MSR
#undef MCR

#define BASE_BAUD (7372800 / 16)

#define RS_TABLE_SIZE 5

#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF|UART_CLEAR_FIFO|UART_USE_FIFO|ASYNC_SKIP_TEST)

#if 1
/*
  	Do not use UART_D if you want to use the extended IDE board.
  	The extended IDE board uses SH776SE_IRQ_IRQ1 for INTRQ.
*/
#define STD_SERIAL_PORT_DEFNS \
        /* UART CLK   PORT IRQ     FLAGS        */ \
        { 0, BASE_BAUD, MS7760CP_UART_B, MS7760CP_UART_INTB, STD_COM_FLAGS}, \
        { 0, BASE_BAUD, MS7760CP_UART_C, MS7760CP_IRQ0, STD_COM_FLAGS}, \
        { 0, BASE_BAUD, 0, 0, STD_COM_FLAGS}, \
        { 0, BASE_BAUD, 0, 0, STD_COM_FLAGS}, \
        { 0, BASE_BAUD, 0, 0, STD_COM_FLAGS}
#else
#define STD_SERIAL_PORT_DEFNS \
        /* UART CLK   PORT IRQ     FLAGS        */ \
        { 0, BASE_BAUD, MS7760CP_UART_B, MS7760CP_UART_INTB, STD_COM_FLAGS}, \
        { 0, BASE_BAUD, MS7760CP_UART_C, MS7760CP_IRQ0, STD_COM_FLAGS}, \
        { 0, BASE_BAUD, MS7760CP_UART_D, MS7760CP_IRQ1, STD_COM_FLAGS}, \
        { 0, BASE_BAUD, 0, 0, STD_COM_FLAGS}, \
        { 0, BASE_BAUD, 0, 0, STD_COM_FLAGS}
#endif


#define SERIAL_PORT_DFNS STD_SERIAL_PORT_DEFNS
#define irq_cannonicalize(x) (x)

#endif /* __ASM_SERIAL_MS7760CP_H */
