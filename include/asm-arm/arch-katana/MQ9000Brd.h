/******************************************************************************
 *
 * Copyright (c) 1999-2001 MediaQ, Inc. or its subsidiaries.
 * All rights reserved.
 *
 * File: MQ9000Brd.h
 *
 * Description:
 *		Hardware description of MQ9000 System. 
 *
 * History:
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *****************************************************************************/
#ifndef _MQ9000BRD_H_
#define _MQ9000BRD_H_

/*******************************************************************************
 * MQ9000 Debug Serial Port
 ******************************************************************************/
#define MQ_UART_PLL_133MHZ    	/* PLL freq. to generate UART Baud Rate */

#ifdef MQ_UART_PLL_133MHZ
/* UART Baud Rate Divisor -						*/
/*    PLL=133.333 MHz, DCLK=32X-16X-8X, MP=4, PS=96-1, DIV=8-2		*/
/* PLL(Freq)=BAUD*DIV*DClk*MP*PS =>					*/
/*    DIV(b31-b16),RDCLK(b15-b12),TDCLK(b10-b8),PS(b7-b4),MP(b3-b0)	*/
#define MQ_PS_1200                   	0x444DD
#define MQ_PS_2400			0x833BD
#define MQ_PS_4800                   	0x9339C
#define MQ_PS_9600			0x9337C
#define MQ_PS_14400                  	0x9336C
#define MQ_PS_19200                  	0x9335C
//#define MQ_PS_38400                  	0x9333C
#define MQ_PS_38400                  	0x0339D // PLL2 133MHz
//#define MQ_PS_57600			0x9332C
#define MQ_PS_57600			0x13393 // PLL2 133MHz
//#define MQ_PS_115200			0x9331C
//#define MQ_PS_115200			0x93313 // 50MHz
#define MQ_PS_115200			0x033B0 // PLL2 133MHz
#define MQ_PS_230400                 	0x9221C
#define MQ_PS_460800                  	0x92215
#endif

#ifdef MQ_UART_PLL_92MHZ
/* UART Baud Rate Divisor -						*/
/*    PLL=92.16 MHz, DCLK=8X-4X-1X, MP=5, PS=96-1, DIV=8-2		*/
/* PLL(Freq)=BAUD*DIV*DClk*MP*PS =>					*/
/*    DIV(b31-b16),RDCLK(b15-b12),TDCLK(b10-b8),PS(b7-b4),MP(b3-b0)	*/
#define MQ_PS_1200                   	0xA44D2
#define MQ_PS_2400			0xA44B2
#define MQ_PS_4800                   	0xA4492
#define MQ_PS_9600			0xA4472
#define MQ_PS_14400                  	0xA4462
#define MQ_PS_19200                  	0xA4452
#define MQ_PS_38400                  	0xA4432
#define MQ_PS_57600                  	0xA4422
#define MQ_PS_115200                  	0xA4412
#define MQ_PS_230400                 	0xA3312
#define MQ_PS_460800                  	0xA2212
#endif

#ifdef MQ_UART_PLL_9MHZ
/* UART Baud Rate Divisor -						*/
/*    PLL=9.216 MHz, DCLK=8X-4X-1X, MP=5, PS=96-1, DIV=8-2		*/
/* PLL(Freq)=BAUD*DIV*DClk*MP*PS =>					*/
/*    DIV(b31-b16),RDCLK(b15-b12),TDCLK(b10-b8),PS(b7-b4),MP(b3-b0)	*/
#define MQ_PS_1200                   	0x811D2
#define MQ_PS_2400			0x811B2
#define MQ_PS_4800                   	0x81192
#define MQ_PS_9600			0x81172
#define MQ_PS_14400                  	0x81162
#define MQ_PS_19200                  	0x81152
#define MQ_PS_38400                  	0x81132
#define MQ_PS_57600                  	0x81122
#define MQ_PS_115200                  	0x81112
#define MQ_PS_230400                 	0x41112
#define MQ_PS_460800                  	0x21112
#endif

#ifdef MQ_UART_PLL_3MHZ
/* UART Baud Rate Divisor -						*/
/*    PLL=3.6864 MHz, DCLK=8X-4X-1X, MP=4, PS=96-1, DIV=8-2		*/
/* PLL(Freq)=BAUD*DIV*DClk*MP*PS =>					*/
/*    DIV(b31-b16),RDCLK(b15-b12),TDCLK(b10-b8),PS(b7-b4),MP(b3-b0)	*/
#define MQ_PS_1200                   	0x800D5
#define MQ_PS_2400			0x800B5
#define MQ_PS_4800                   	0x80095
#define MQ_PS_9600			0x80075
#define MQ_PS_14400                  	0x80065
#define MQ_PS_19200                  	0x80055
#define MQ_PS_38400                  	0x80035
#define MQ_PS_57600                  	0x80025
#define MQ_PS_115200                  	0x80015
#define MQ_PS_230400                 	0x40015
#define MQ_PS_460800                  	0x20015
#endif

#define MQ9000_DBG_COMPORT		2           	// HAL debug COM port
#define MQ9000_DBG_COMBAUD		MQ_PS_115200 	// HAL debug COM baud

/*******************************************************************************
 * MQ9000 OS Timers
 ******************************************************************************/
// Palm 5 : Timer1 = 1 usec (delay), Timer2 = 10 msec (global/periodic)
// WINCE  : Timer1 = 1 msec
// Symbian: ????
#define OS_TIMER1			1
#define OS_TIMER2			2
/* 
 *  These are useconds NOT ticks.  
 * 
 */
#define mSEC_1                          1000
#define mSEC_5                          (mSEC_1 * 5)
#define mSEC_10                         (mSEC_1 * 10)
#define mSEC_25                         (mSEC_1 * 25)
#define SEC_1                           (mSEC_1 * 1000)

/*******************************************************************************
 * MQ9000 I2C LEDs
 ******************************************************************************/
#define NUM_OF_LEDS			4
#define CPU_I2C_ID			0x40		// I2C ID/Addr
#define CPU_I2C_LED			0x03		// I2C output port 1
#define EDEV0_I2C_ID			0x4A		// I2C ID/Addr
#define EDEV0_I2C_HEXLED		0x03		// I2C output port 1

/*******************************************************************************
 * MQ9000 DIP Switches
 ******************************************************************************/
#define CPU_I2C_PORT0			0x00		// I2C input port 0
#define CPU_I2C_PORT1			0x01		// I2C input port 1
#define BOOT_SWITCH1			CPU_I2C_PORT0	// RAM, Std, or Flash boot switch 
#define BOOT_SWITCH2			CPU_I2C_PORT1	// boot switch  
#define RAM_DOWNLOAD			0x01		// ReadBootSwitch RAM image 
#define FLASH_DOWNLOAD			0x02		// ReadBootSwitch Flash image
 
#endif /* _MQ9000BRD_H_ */
