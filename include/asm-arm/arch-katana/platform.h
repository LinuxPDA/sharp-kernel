/*
 * Copyright -A© ARM Limited 1998.$)B
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
 */

/* ************************************************************************
 * 
 *   Integrator address map
 * 
 * 	NOTE: This is a multi-hosted header file for use with uHAL and
 * 	      supported debuggers.
 * 
 * ***********************************************************************/

#ifndef __address_h
#define __address_h                     1

#include <asm/arch/MQ9000Hwr.h>

#define	PBCS0_BASE	0x20000000
#define	PBCS1_BASE	0x28000000
#define	SRCS_BASE	0x30000000
#define	SBI_FIFO_BASE	0x38000000
#define	ESRAM_BASE	0x40000000
#define	ESRAM_SER_BASE	0x40200000
#define	NAND_FLASH_BASE	0x50000000
#define	UD_BASE		0x80000000
#define	VI_BASE		0x80001000
#define	I2S_BASE	0x80001800	//I2S Audio Codec
#define	SPI_BASE	0x80002000
#define	SD_BASE		0x80002800
#define	KB_BASE		0x80003000
#define	GP_BASE		0x80003800	//GPIO/Timer
#define	U1_BASE		0x80004000	//UART 1
#define	U2_BASE		0x80004800	//UART 2
#define	U3_BASE		0x80005000	//UART 3
#define	PM_BASE		0x80005800	//Power Management
#define	SSP_FIFO_BASE	0x80006000	//SSP Transmit FIFO
#define	SSP_BASE	0x80006800	//SSP Bus
#define	IN_BASE		0x80007000	//INTR Ctrl
#define	RTC_BASE	0x80007800
#define	FP_BASE		0x80008000
#define	GC_BASE		0x80008800
#define	GE_BASE		0x80009000
#define	GE2_BASE	0x80009800
#define	MM_BASE		0x8000A000	//Embedded MIU
#define	SBI_BASE	0x8000A800	//External SBI

//... and more...

#define	MQ_REG_PHYS_OFFSET(b)	(b - 0x80000000)	

//The followings are used in arch/arm/kernel/entry-armv.S where our
//  individual interrupt (status AND mask) bits are interpreted and
//  an IRQNO_ is generated.
#define	IN_VBASE	(MQ9000_REGS_VBASE + (MQ_REG_PHYS_OFFSET(IN_BASE)))
#define	IN_IRQ_MASK_VREG	(IN_VBASE + 0x04)	//in01
#define	IN_RAW_STATUS_VREG	(IN_VBASE + 0x10)	//in04

#endif // __address_h
