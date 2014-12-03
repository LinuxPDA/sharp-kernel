/*
 *  linux/include/asm-arm/arch-integrator/irqs.h
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Change Log
 *	7-Jan-2003 Lineo Japan, Inc.
 *
 */

/* Use the integrator definitions */
#include <asm/arch/platform.h>

//**** Must define NR_IRQS so that interrupt structure array (??) can be
//**** allocated.
#define	NR_IRQS			66		// IRQs number

#define IRQNO_GC1VS_RISE         0		// GC1 VSync Rising Edge
#define IRQNO_GC1VS_FALL         1		// GC1 VSync Falling Edge
#define IRQNO_GC1VD_RISE         2		// GC1 VDisplay Rising Edge
#define IRQNO_GC1VD_FALL         3		// GC1 VDisplay Falling Edge
#define IRQNO_CMDFIFO_HALF       4		// CMD FIFO Half
#define IRQNO_CMDFIFO_EMTY       5		// CMD FIFO Empty
#define IRQNO_SRCFIFO_HALF       6		// SRC FIFO Half
#define IRQNO_SRCFIFO_EMTY       7		// SRC FIFO Empty
#define IRQNO_GE_IDLE            8		// GE Idle
#define IRQNO_SBLT               9 		// GE Stretch Blt
#define IRQNO_VI                 10		// Video Input
#define IRQNO_SD                 11		// Secure Digital
#define IRQNO_I2C	         12		// I2C / SSPBus
#define IRQNO_TIMER              13		// Timer
#define IRQNO_SSC                14		// Audio Codec
#define IRQNO_DMAC               15		// DMA Controller
#define IRQNO_SPI                16		// Serial Peripheral Interface
#define IRQNO_RTC                17		// Real Time Clock	
#define IRQNO_KB                 18		// Matrix Keyboard
#define IRQNO_UART1              19		// UART 1 (IRDA)
#define IRQNO_UART2              20		// UART 2 (Baseband)
#define IRQNO_UART3              21		// UART 3 (Bluetooth)
#define IRQNO_EDEV0              22		// External Device 0
#define IRQNO_EDEV1              23		// External Device 1
#define IRQNO_USBD               24		// USB Device
#define IRQNO_USBD_WAKE          25		// USB Device Wake-up
#define IRQNO_PWM                26		// Pulse Width Modulation
#define IRQNO_NAND               27		// NAND-Flash

// extended IRQ (LINEO Japan, Inc.)
#define IRQ_GPIO(n)		(28 + n)
#define IRQ_TO_Timer_GPIO(n)	(n - 28)

#define IRQNO_GPIO0		IRQ_GPIO(0)	// GPIO0 input transition
#define IRQNO_GPIO1		IRQ_GPIO(1)	// GPIO1 input transition
#define IRQNO_GPIO2		IRQ_GPIO(2)	// GPIO2 input transition
#define IRQNO_GPIO3		IRQ_GPIO(3)	// GPIO3 input transition
#define IRQNO_GPIO4		IRQ_GPIO(4)	// GPIO4 input transition
#define IRQNO_GPIO5		IRQ_GPIO(5)	// GPIO5 input transition
#define IRQNO_GPIO6		IRQ_GPIO(6)	// GPIO6 input transition
#define IRQNO_GPIO7		IRQ_GPIO(7)	// GPIO7 input transition
#define IRQNO_GPIO8		IRQ_GPIO(8)	// GPIO8 input transition
#define IRQNO_GPIO9		IRQ_GPIO(9)	// GPIO9 input transition
#define IRQNO_GPIO10		IRQ_GPIO(10)	// GPIO10 input transition
#define IRQNO_GPIO11		IRQ_GPIO(11)	// GPIO11 input transition
#define IRQNO_GPIO111		IRQ_GPIO(12)	// GPIO111 input transition
#define IRQNO_GPIO112		IRQ_GPIO(13)	// GPIO112 input transition
#define IRQNO_GPIO113		IRQ_GPIO(14)	// GPIO113 input transition
#define IRQNO_GPIO114		IRQ_GPIO(15)	// GPIO114 input transition
#define IRQNO_GPIO115		IRQ_GPIO(16)	// GPIO115 input transition
#define IRQNO_GPIO61		IRQ_GPIO(17)	// GPIO61 input transition
#define IRQNO_GPIO62		IRQ_GPIO(18)	// GPIO62 input transition
#define IRQNO_GPIO63		IRQ_GPIO(19)	// GPIO63 input transition
#define IRQNO_LED		IRQ_GPIO(20)	// LED cycle done
#define IRQNO_TIMER0		IRQ_GPIO(21)	// Timer module 0 timer mode
#define IRQNO_TIMER1		IRQ_GPIO(22)	// Timer module 1 timer mode
#define IRQNO_TIMER2		IRQ_GPIO(23)	// Timer module 2 timer mode
#define IRQNO_GPIO41		IRQ_GPIO(24)	// GPIO41 input transition
#define IRQNO_GPIO42		IRQ_GPIO(25)	// GPIO42 input transition
#define IRQNO_GPIO43		IRQ_GPIO(26)	// GPIO43 input transition
#define IRQNO_GPIO44		IRQ_GPIO(27)	// GPIO44 input transition
#define IRQNO_GPIO46		IRQ_GPIO(28)	// GPIO46 input transition
#define IRQNO_GPIO47		IRQ_GPIO(29)	// GPIO47 input transition
#define IRQNO_GPIO48		IRQ_GPIO(30)	// GPIO48 input transition
#define IRQNO_GPIO49		IRQ_GPIO(31)	// GPIO49 input transition

#define IRQNO_GPIO116		IRQ_GPIO(32)	// GPIO116 input transition
#define IRQNO_GPIO117		IRQ_GPIO(33)	// GPIO117 input transition
#define IRQNO_GPIO118		IRQ_GPIO(34)	// GPIO118 input transition
#define IRQNO_GPIO119		IRQ_GPIO(35)	// GPIO119 input transition

#define IRQ_TO_RTC(n)		(IRQ_GPIO(35) + 1 + n)
#define RTC_TO_IRQ(n)		(n - IRQ_GPIO(35) - 1)

#define IRQNO_RTCPRD		IRQ_TO_RTC(0)	// RTC periodic timer
#define IRQNO_RTCALM		IRQ_TO_RTC(1)	// RTC alarm

#define IRQ_KATANATS		IRQNO_GPIO119	// Touch Panel
#define IRQ_GPIO_PWDN		IRQNO_GPIO0	// Power down button
