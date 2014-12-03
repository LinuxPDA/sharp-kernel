/*
 * linux/include/asm-arm/arch-dbmx2/platform.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/include/asm-arm/arch-mx2ads/platform.h
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
 *
 * Copyright (C) 2002 Shane Nay (shane@minirl.com)
 * Copyright (C) 2003 Motorola Semiconductor SUZHOU Ltd
 *   - add MX2ADS specific definitions by jimmy xu r58452@motorola.com
 *
 */

/**************************************************************************
 * * Copyright ?ARM Limited 1998.  All rights reserved.
 * ***********************************************************************/
/* ************************************************************************
 *
 *   MX2 address map
 *
 * ***********************************************************************/


/* ========================================================================
 *  MX2 definitions
 * ========================================================================
 * ------------------------------------------------------------------------
 *  Memory definitions
 * ------------------------------------------------------------------------
 *  MX2 memory map
 */


#ifndef __MX2ADS_PLATFORM_H__
#define __MX2ADS_PLATFORM_H__


/* Mx2 ADS SDRAM is from 0xC0000000, 64M, we use the last 16M as RAM disk for usb*/

#define	MX2ADS_SDRAM_DISK_BASE		0xC3000000	// SDRAM disk base (last 16M of SDRAM)
#define MX2ADS_SDRAM_DISK_SIZE  	SZ_16M
#define	MX2ADS_SDRAM_DISK_IOBASE	0xE0000000

#define MX2ADS_BURSTFLASH_BASE         	0xC8000000	//cs0
#define MX2ADS_BURSTFLASH_SIZE         	SZ_32M
#define MX2ADS_BURSTFLASH_IOBASE       	0xE1000000

#define MX2ADS_PER_BASE         	0xCC000000	//cs1	for periperal device IO
#define MX2ADS_PER_SIZE         	SZ_16M
#define MX2ADS_PER_IOBASE       	0xE3000000

#define MX2ADS_IO_BASE             	0x10000000
#define MX2ADS_IO_SIZE             	SZ_1M
#define MX2ADS_IO_IOBASE           	0xE4000000

/* Internel Module reserved space*/

#define MX2ADS_CSI_BASE             	0x80000000
#define MX2ADS_CSI_SIZE             	SZ_1M
#define MX2ADS_CSI_IOBASE           	0xE4100000 	

#define MX2ADS_BMI_BASE             	0xA0000000
#define MX2ADS_BMI_SIZE             	SZ_1M
#define MX2ADS_BMI_IOBASE           	0xE4200000

#define MX2ADS_PCMCIA_BASE             	0xD4000000
#define MX2ADS_PCMCIA_SIZE             	SZ_1M
#define MX2ADS_PCMCIA_IOBASE           	0xE4300000

/* According to MX2 spec, EIM register is mapped to 0xDF001000, not in register area!!
it's so strange, I have to map EIM another space
it is include NFC address space(DF003000 DF003FFF)
Frank Li Change MX2ADS_EMI_BASE from 0xDF001000 to 0xDF000000
*/

#define MX2ADS_EMI_BASE             	0xDF000000
#define MX2ADS_EMI_SIZE             	SZ_1M
#define MX2ADS_EMI_IOBASE           	0xE4400000

/*
 * XIP kernel text mapping.
 * Note: the exact virtual address is also specified in arch/arm/Makefile.
 */
#ifdef CONFIG_XIP_KERNEL
#define KERNEL_XIP_BASE_PHYS	CONFIG_XIP_PHYS_ADDR
#define KERNEL_XIP_BASE_VIRT	(MX2ADS_BURSTFLASH_IOBASE + (CONFIG_XIP_PHYS_ADDR & 0x07ffffff))
#endif

#define IO_ADDRESS(x) 		(((x)-0x10000000)+MX2ADS_IO_IOBASE)		//This Macro only for map register space to 0xe4000000;	
#define NFC_IO_ADDRESS(x) 	(((x)-MX2ADS_EMI_BASE)+MX2ADS_EMI_IOBASE)
#define PCMCIA_IO_ADDRESS       NFC_IO_ADDRESS
/* ------------------------------------------------------------------------
 *  Motorola MX2 system registers
 * ------------------------------------------------------------------------
 *
 */

/*
 *  Register offests.
 *
 */

/* 
We define IO_ADDRESS(x) (x | IO_BASE) in hardware.h, so we just define low bit of module register address here,
IO_ADDRESS(x) = x | 0xE3000000, map register space to 0xE3000000,1M.	jimmy
*/

#define MX2ADS_AIPI1_BASE             0x0
#define MX2ADS_AIPI2_BASE             0x20000

#define MX2ADS_DMA_BASE               0x01000
#define MX2ADS_WDT_BASE               0x02000
#define MX2ADS_GPT1_BASE              0x03000
#define MX2ADS_GPT2_BASE              0x04000
#define MX2ADS_GPT3_BASE              0x05000
#define MX2ADS_PWM_BASE               0x06000
#define MX2ADS_RTC_BASE               0x07000
#define MX2ADS_KPP_BASE               0x08000
#define MX2ADS_OWIRE_BASE             0x09000
#define MX2ADS_UART1_BASE             0x0A000
#define MX2ADS_UART2_BASE             0x0B000
#define MX2ADS_UART3_BASE             0x0C000
#define MX2ADS_UART4_BASE             0x0D000
#define MX2ADS_CSPI1_BASE             0x0E000
#define MX2ADS_CSPI2_BASE             0x0F000
#define MX2ADS_SSI1_BASE              0x10000
#define MX2ADS_SSI2_BASE              0x11000
#define MX2ADS_I2C_BASE               0x12000
#define MX2ADS_SDHC1_BASE             0x13000
#define MX2ADS_SDHC2_BASE             0x14000
#define MX2ADS_GPIO_BASE              0x15000
#define MX2ADS_AUDMUX_BASE            0x16000
#define MX2ADS_LCDC_BASE              0x21000
#define MX2ADS_SLCDC_BASE             0x22000
#define MX2ADS_SAHARA_BASE            0x23000
#define MX2ADS_USBOTG_BASE            0x24000
#define MX2ADS_EMMA_BASE              0x26000
#define MX2ADS_CRM_BASE               0x27000
#define MX2ADS_FIRI_BASE              0x28000
#define MX2ADS_JAM_BASE               0x3E000
#define MX2ADS_MAX_BASE               0x3F000


/*
 *  MX2 Interrupt numbers
 *
 */
/* INT_GPIO*/
#define INT_GPIO                    8
#define INT_FIRI                    9
#define INT_SDHC2                   10
#define INT_SDHC1                   11
#define INT_I2C                     12
#define INT_SSI2                    13
#define INT_SSI1                    14
#define INT_CSPI2                   15
#define INT_CSPI1              	    16
#define INT_UART4                   17
#define INT_UART3                   18
#define INT_UART2                   19
#define INT_UART1                   20
#define INT_KPP_TX                  21
#define INT_RTC_RX                  22
#define INT_PWM                     23
#define INT_GPT3                    24
#define INT_GPT2                    25
#define INT_GPT1                    26
#define INT_WDOG                    27
#define INT_PCMCIA                  28
#define INT_NFC                     29
#define INT_BMI                     30
#define INT_CSI                     31
#define INT_DMACH0                  32
#define INT_DMACH1                  33
#define INT_DMACH2                  34
#define INT_DMACH3                  35
#define INT_DMACH4                  36
#define INT_DMACH5                  37
#define INT_DMACH6                  38
#define INT_DMACH7                  39
#define INT_DMACH8                  40
#define INT_DMACH9                  41
#define INT_DMACH10                 42
#define INT_DMACH11                 43
#define INT_DMACH12                 44
#define INT_DMACH13                 45
#define INT_DMACH14                 46
#define INT_DMACH15                 47
#define INT_EMMAENC                 49
#define INT_EMMADEC                 50
#define INT_EMMAPRP                 51
#define INT_EMMAPP                  52
#define INT_USBWKUP                 53
#define INT_USBMNP                  54
#define INT_USBDMA                  55
#define INT_USBFUNC                 56
#define INT_USBHOST                 57
#define INT_USBCTRL                 58
#define INT_SAHARA                  59
#define INT_SLCDC                   60
#define INT_LCDC                    61





#define MAXIRQNUM                       62
#define MAXFIQNUM                       62
#define MAXSWINUM                       62





#define TICKS_PER_uSEC                  24

/*
 *  These are useconds NOT ticks.
 *
 */
#define mSEC_1                          1000
#define mSEC_5                          (mSEC_1 * 5)
#define mSEC_10                         (mSEC_1 * 10)
#define mSEC_25                         (mSEC_1 * 25)
#define SEC_1                           (mSEC_1 * 1000)




/* 	END */
#endif
