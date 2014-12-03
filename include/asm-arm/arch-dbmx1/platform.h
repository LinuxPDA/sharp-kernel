/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/include/asm-arm/arch-dbmx1/platform.h
 *
 * Based on:
 *  linux/include/asm-arm/arch-mx1ads/platform.h
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
 *
 */

/**************************************************************************
 * * Copyright © ARM Limited 1998.  All rights reserved.
 * ***********************************************************************/
/* ************************************************************************
 *
 *   MX1 address map
 *
 * ***********************************************************************/


/* ========================================================================
 *  MX1 definitions
 * ========================================================================
 * ------------------------------------------------------------------------
 *  Memory definitions
 * ------------------------------------------------------------------------
 *  MX1 memory map
 */


#ifndef __DBMX1_PLATFORM_H__
#define __DBMX1_PLATFORM_H__

#include <linux/config.h>

#define DBMX1_SRAM_BASE           0x00300000
#define DBMX1_SRAM_SIZE           SZ_128K

#define DBMX1_SFLASH_BASE         0x0C000000
#define DBMX1_SFLASH_SIZE         SZ_16M

#define DBMX1_IO_BASE             0x00200000
#define DBMX1_IO_SIZE             SZ_256K

#define DBMX1_VID_BASE            0x00300000
#define DBMX1_VID_SIZE            0x26000

#define DBMX1_IO2_BASE            0x10000000
#define DBMX1_IO2_SIZE            SZ_128M

#define DBMX1_VID_START           IO_ADDRESS(DBMX1_VID_BASE)


/*
 * XIP kernel text mapping.
 * Note: the exact virtual address is also specified in arch/arm/Makefile.
 */
#ifdef CONFIG_XIP_KERNEL
#define KERNEL_XIP_BASE_PHYS	(CONFIG_XIP_PHYS_ADDR & 0xff000000)
#define KERNEL_XIP_BASE_VIRT	0xf4000000
#endif


/* ------------------------------------------------------------------------
 *  Motorola MX1 system registers
 * ------------------------------------------------------------------------
 *
 */

/*
 *  Register offests.
 *
 */

#define DBMX1_AIPI1_OFFSET             0x00000
#define DBMX1_WDT_OFFSET               0x01000
#define DBMX1_TIM1_OFFSET              0x02000
#define DBMX1_TIM2_OFFSET              0x03000
#define DBMX1_RTC_OFFSET               0x04000
#define DBMX1_LCDC_OFFSET              0x05000
#define DBMX1_UART1_OFFSET             0x06000
#define DBMX1_UART2_OFFSET             0x07000
#define DBMX1_PWM_OFFSET               0x08000
#define DBMX1_DMAC_OFFSET              0x09000
#define DBMX1_AIPI2_OFFSET             0x10000
#define DBMX1_SIM_OFFSET               0x11000
#define DBMX1_USBD_OFFSET              0x12000
#define DBMX1_SPI1_OFFSET              0x13000
#define DBMX1_MMC_OFFSET               0x14000
#define DBMX1_ASP_OFFSET               0x15000
#define DBMX1_BTA_OFFSET               0x16000
#define DBMX1_I2C_OFFSET               0x17000
#define DBMX1_SSI_OFFSET               0x18000
#define DBMX1_SPI2_OFFSET              0x19000
#define DBMX1_MSHC_OFFSET              0x1A000
#define DBMX1_PLL_OFFSET               0x1B000
#define DBMX1_SYSCTRL_OFFSET           0x1B800
#define DBMX1_GPIO_OFFSET              0x1C000
#define DBMX1_EIM_OFFSET               0x20000
#define DBMX1_SDRAMC_OFFSET            0x21000
#define DBMX1_MMA_OFFSET               0x22000
#define DBMX1_AITC_OFFSET              0x23000
#define DBMX1_CSI_OFFSET               0x24000

/*
 *  Register BASEs, based on OFFSETs
 *
 */

#define DBMX1_AIPI1_BASE             (DBMX1_AIPI1_OFFSET + DBMX1_IO_BASE)
#define DBMX1_WDT_BASE               (DBMX1_WDT_OFFSET + DBMX1_IO_BASE)
#define DBMX1_TIM1_BASE              (DBMX1_TIM1_OFFSET + DBMX1_IO_BASE)
#define DBMX1_TIM2_BASE              (DBMX1_TIM2_OFFSET + DBMX1_IO_BASE)
#define DBMX1_RTC_BASE               (DBMX1_RTC_OFFSET + DBMX1_IO_BASE)
#define DBMX1_LCDC_BASE              (DBMX1_LCDC_OFFSET + DBMX1_IO_BASE)
#define DBMX1_UART1_BASE             (DBMX1_UART1_OFFSET + DBMX1_IO_BASE)
#define DBMX1_UART2_BASE             (DBMX1_UART2_OFFSET + DBMX1_IO_BASE)
#define DBMX1_PWM_BASE               (DBMX1_PWM_OFFSET + DBMX1_IO_BASE)
#define DBMX1_DMAC_BASE              (DBMX1_DMAC_OFFSET + DBMX1_IO_BASE)
#define DBMX1_AIPI2_BASE             (DBMX1_AIPI2_OFFSET + DBMX1_IO_BASE)
#define DBMX1_SIM_BASE               (DBMX1_SIM_OFFSET + DBMX1_IO_BASE)
#define DBMX1_USBD_BASE              (DBMX1_USBD_OFFSET + DBMX1_IO_BASE)
#define DBMX1_SPI1_BASE              (DBMX1_SPI1_OFFSET + DBMX1_IO_BASE)
#define DBMX1_MMC_BASE               (DBMX1_MMC_OFFSET + DBMX1_IO_BASE)
#define DBMX1_ASP_BASE               (DBMX1_ASP_OFFSET + DBMX1_IO_BASE)
#define DBMX1_BTA_BASE               (DBMX1_BTA_OFFSET + DBMX1_IO_BASE)
#define DBMX1_I2C_BASE               (DBMX1_I2C_OFFSET + DBMX1_IO_BASE)
#define DBMX1_SSI_BASE               (DBMX1_SSI_OFFSET + DBMX1_IO_BASE)
#define DBMX1_SPI2_BASE              (DBMX1_SPI2_OFFSET + DBMX1_IO_BASE)
#define DBMX1_MSHC_BASE              (DBMX1_MSHC_OFFSET + DBMX1_IO_BASE)
#define DBMX1_PLL_BASE               (DBMX1_PLL_OFFSET + DBMX1_IO_BASE)
#define DBMX1_SYSCTRL_BASE           (DBMX1_SYSCTRL_OFFSET + DBMX1_IO_BASE)
#define DBMX1_GPIO_BASE              (DBMX1_GPIO_OFFSET + DBMX1_IO_BASE)
#define DBMX1_EIM_BASE               (DBMX1_EIM_OFFSET + DBMX1_IO_BASE)
#define DBMX1_SDRAMC_BASE            (DBMX1_SDRAMC_OFFSET + DBMX1_IO_BASE)
#define DBMX1_MMA_BASE               (DBMX1_MMA_OFFSET + DBMX1_IO_BASE)
#define DBMX1_AITC_BASE              (DBMX1_AITC_OFFSET + DBMX1_IO_BASE)
#define DBMX1_CSI_BASE               (DBMX1_CSI_OFFSET + DBMX1_IO_BASE)


#define	VA_SPI2			IO_ADDRESS(DBMX1_SPI2_BASE)
#define	DBMX1_SPI2_RXDATAREG2	(*(volatile unsigned int *)(VA_SPI2 + 0x000))
#define	DBMX1_SPI2_TXDATAREG2	(*(volatile unsigned int *)(VA_SPI2 + 0x004))
#define	DBMX1_SPI2_CONTROLREG2	(*(volatile unsigned int *)(VA_SPI2 + 0x008))
#define	DBMX1_SPI2_INTREG2	(*(volatile unsigned int *)(VA_SPI2 + 0x00c))
#define	DBMX1_SPI2_TESTREG2	(*(volatile unsigned int *)(VA_SPI2 + 0x010))
#define	DBMX1_SPI2_PERIODREG2	(*(volatile unsigned int *)(VA_SPI2 + 0x014))
#define	DBMX1_SPI2_DMAREG2	(*(volatile unsigned int *)(VA_SPI2 + 0x018))
#define	DBMX1_SPI2_RESETREG2	(*(volatile unsigned int *)(VA_SPI2 + 0x01c))

#define	VA_PLL			IO_ADDRESS(DBMX1_PLL_BASE)
#define	DBMX1_PLL_CSCR		(*(volatile unsigned int *)(VA_PLL + 0x000))
#define	DBMX1_PLL_MPCTL0	(*(volatile unsigned int *)(VA_PLL + 0x004))
#define	DBMX1_PLL_MPCTL1	(*(volatile unsigned int *)(VA_PLL + 0x008))
#define	DBMX1_PLL_SPCTL0	(*(volatile unsigned int *)(VA_PLL + 0x00c))
#define	DBMX1_PLL_SPCTL1	(*(volatile unsigned int *)(VA_PLL + 0x010))
#define	DBMX1_PLL_PCDR		(*(volatile unsigned int *)(VA_PLL + 0x020))

#define	VA_SYSCTRL		IO_ADDRESS(DBMX1_SYSCTRL_BASE)
#define	DBMX1_SYSCTRL_RSR	(*(volatile unsigned int *)(VA_SYSCTRL + 0x000))
#define	DBMX1_SYSCTRL_SIDR	(*(volatile unsigned int *)(VA_SYSCTRL + 0x004))
#define	DBMX1_SYSCTRL_FMCR	(*(volatile unsigned int *)(VA_SYSCTRL + 0x008))
#define	DBMX1_SYSCTRL_GPCR	(*(volatile unsigned int *)(VA_SYSCTRL + 0x00c))
#define	DBMX1_SYSCTRL_GCCR	(*(volatile unsigned int *)(VA_SYSCTRL + 0x010))

#define	VA_GPIO			IO_ADDRESS(DBMX1_GPIO_BASE)
#define	DBMX1_GPIO_DDIR_A	(*(volatile unsigned int *)(VA_GPIO + 0x000))
#define	DBMX1_GPIO_OCR1_A	(*(volatile unsigned int *)(VA_GPIO + 0x004))
#define	DBMX1_GPIO_OCR2_A	(*(volatile unsigned int *)(VA_GPIO + 0x008))
#define	DBMX1_GPIO_ICONFA1_A	(*(volatile unsigned int *)(VA_GPIO + 0x00c))
#define	DBMX1_GPIO_ICONFA2_A	(*(volatile unsigned int *)(VA_GPIO + 0x010))
#define	DBMX1_GPIO_ICONFB1_A	(*(volatile unsigned int *)(VA_GPIO + 0x014))
#define	DBMX1_GPIO_ICONFB2_A	(*(volatile unsigned int *)(VA_GPIO + 0x018))
#define	DBMX1_GPIO_DR_A		(*(volatile unsigned int *)(VA_GPIO + 0x01c))
#define	DBMX1_GPIO_GIUS_A	(*(volatile unsigned int *)(VA_GPIO + 0x020))
#define	DBMX1_GPIO_SSR_A	(*(volatile unsigned int *)(VA_GPIO + 0x024))
#define	DBMX1_GPIO_ICR1_A	(*(volatile unsigned int *)(VA_GPIO + 0x028))
#define	DBMX1_GPIO_ICR2_A	(*(volatile unsigned int *)(VA_GPIO + 0x02c))
#define	DBMX1_GPIO_IMR_A	(*(volatile unsigned int *)(VA_GPIO + 0x030))
#define	DBMX1_GPIO_ISR_A	(*(volatile unsigned int *)(VA_GPIO + 0x034))
#define	DBMX1_GPIO_GPR_A	(*(volatile unsigned int *)(VA_GPIO + 0x038))
#define	DBMX1_GPIO_SWR_A	(*(volatile unsigned int *)(VA_GPIO + 0x03c))
#define	DBMX1_GPIO_PUEN_A	(*(volatile unsigned int *)(VA_GPIO + 0x040))
#define	DBMX1_GPIO_DDIR_B	(*(volatile unsigned int *)(VA_GPIO + 0x100))
#define	DBMX1_GPIO_OCR1_B	(*(volatile unsigned int *)(VA_GPIO + 0x104))
#define	DBMX1_GPIO_OCR2_B	(*(volatile unsigned int *)(VA_GPIO + 0x108))
#define	DBMX1_GPIO_ICONFA1_B	(*(volatile unsigned int *)(VA_GPIO + 0x10c))
#define	DBMX1_GPIO_ICONFA2_B	(*(volatile unsigned int *)(VA_GPIO + 0x110))
#define	DBMX1_GPIO_ICONFB1_B	(*(volatile unsigned int *)(VA_GPIO + 0x114))
#define	DBMX1_GPIO_ICONFB2_B	(*(volatile unsigned int *)(VA_GPIO + 0x118))
#define	DBMX1_GPIO_DR_B		(*(volatile unsigned int *)(VA_GPIO + 0x11c))
#define	DBMX1_GPIO_GIUS_B	(*(volatile unsigned int *)(VA_GPIO + 0x120))
#define	DBMX1_GPIO_SSR_B	(*(volatile unsigned int *)(VA_GPIO + 0x124))
#define	DBMX1_GPIO_ICR1_B	(*(volatile unsigned int *)(VA_GPIO + 0x128))
#define	DBMX1_GPIO_ICR2_B	(*(volatile unsigned int *)(VA_GPIO + 0x12c))
#define	DBMX1_GPIO_IMR_B	(*(volatile unsigned int *)(VA_GPIO + 0x130))
#define	DBMX1_GPIO_ISR_B	(*(volatile unsigned int *)(VA_GPIO + 0x134))
#define	DBMX1_GPIO_GPR_B	(*(volatile unsigned int *)(VA_GPIO + 0x138))
#define	DBMX1_GPIO_SWR_B	(*(volatile unsigned int *)(VA_GPIO + 0x13c))
#define	DBMX1_GPIO_PUEN_B	(*(volatile unsigned int *)(VA_GPIO + 0x140))
#define	DBMX1_GPIO_DDIR_C	(*(volatile unsigned int *)(VA_GPIO + 0x200))
#define	DBMX1_GPIO_OCR1_C	(*(volatile unsigned int *)(VA_GPIO + 0x204))
#define	DBMX1_GPIO_OCR2_C	(*(volatile unsigned int *)(VA_GPIO + 0x208))
#define	DBMX1_GPIO_ICONFA1_C	(*(volatile unsigned int *)(VA_GPIO + 0x20c))
#define	DBMX1_GPIO_ICONFA2_C	(*(volatile unsigned int *)(VA_GPIO + 0x210))
#define	DBMX1_GPIO_ICONFB1_C	(*(volatile unsigned int *)(VA_GPIO + 0x214))
#define	DBMX1_GPIO_ICONFB2_C	(*(volatile unsigned int *)(VA_GPIO + 0x218))
#define	DBMX1_GPIO_DR_C		(*(volatile unsigned int *)(VA_GPIO + 0x21c))
#define	DBMX1_GPIO_GIUS_C	(*(volatile unsigned int *)(VA_GPIO + 0x220))
#define	DBMX1_GPIO_SSR_C	(*(volatile unsigned int *)(VA_GPIO + 0x224))
#define	DBMX1_GPIO_ICR1_C	(*(volatile unsigned int *)(VA_GPIO + 0x228))
#define	DBMX1_GPIO_ICR2_C	(*(volatile unsigned int *)(VA_GPIO + 0x22c))
#define	DBMX1_GPIO_IMR_C	(*(volatile unsigned int *)(VA_GPIO + 0x230))
#define	DBMX1_GPIO_ISR_C	(*(volatile unsigned int *)(VA_GPIO + 0x234))
#define	DBMX1_GPIO_GPR_C	(*(volatile unsigned int *)(VA_GPIO + 0x238))
#define	DBMX1_GPIO_SWR_C	(*(volatile unsigned int *)(VA_GPIO + 0x23c))
#define	DBMX1_GPIO_PUEN_C	(*(volatile unsigned int *)(VA_GPIO + 0x240))
#define	DBMX1_GPIO_DDIR_D	(*(volatile unsigned int *)(VA_GPIO + 0x300))
#define	DBMX1_GPIO_OCR1_D	(*(volatile unsigned int *)(VA_GPIO + 0x304))
#define	DBMX1_GPIO_OCR2_D	(*(volatile unsigned int *)(VA_GPIO + 0x308))
#define	DBMX1_GPIO_ICONFA1_D	(*(volatile unsigned int *)(VA_GPIO + 0x30c))
#define	DBMX1_GPIO_ICONFA2_D	(*(volatile unsigned int *)(VA_GPIO + 0x310))
#define	DBMX1_GPIO_ICONFB1_D	(*(volatile unsigned int *)(VA_GPIO + 0x314))
#define	DBMX1_GPIO_ICONFB2_D	(*(volatile unsigned int *)(VA_GPIO + 0x318))
#define	DBMX1_GPIO_DR_D		(*(volatile unsigned int *)(VA_GPIO + 0x31c))
#define	DBMX1_GPIO_GIUS_D	(*(volatile unsigned int *)(VA_GPIO + 0x320))
#define	DBMX1_GPIO_SSR_D	(*(volatile unsigned int *)(VA_GPIO + 0x324))
#define	DBMX1_GPIO_ICR1_D	(*(volatile unsigned int *)(VA_GPIO + 0x328))
#define	DBMX1_GPIO_ICR2_D	(*(volatile unsigned int *)(VA_GPIO + 0x32c))
#define	DBMX1_GPIO_IMR_D	(*(volatile unsigned int *)(VA_GPIO + 0x330))
#define	DBMX1_GPIO_ISR_D	(*(volatile unsigned int *)(VA_GPIO + 0x334))
#define	DBMX1_GPIO_GPR_D	(*(volatile unsigned int *)(VA_GPIO + 0x338))
#define	DBMX1_GPIO_SWR_D	(*(volatile unsigned int *)(VA_GPIO + 0x33c))
#define	DBMX1_GPIO_PUEN_D	(*(volatile unsigned int *)(VA_GPIO + 0x340))

#define	VA_EIM			IO_ADDRESS(DBMX1_EIM_BASE)
#define	DBMX1_EIM_CS0U		(*(volatile unsigned int *)(VA_EIM + 0x000))
#define	DBMX1_EIM_CS0L		(*(volatile unsigned int *)(VA_EIM + 0x004))
#define	DBMX1_EIM_CS1U		(*(volatile unsigned int *)(VA_EIM + 0x008))
#define	DBMX1_EIM_CS1L		(*(volatile unsigned int *)(VA_EIM + 0x00c))
#define	DBMX1_EIM_CS2U		(*(volatile unsigned int *)(VA_EIM + 0x010))
#define	DBMX1_EIM_CS2L		(*(volatile unsigned int *)(VA_EIM + 0x014))
#define	DBMX1_EIM_CS3U		(*(volatile unsigned int *)(VA_EIM + 0x018))
#define	DBMX1_EIM_CS3L		(*(volatile unsigned int *)(VA_EIM + 0x01c))
#define	DBMX1_EIM_CS4U		(*(volatile unsigned int *)(VA_EIM + 0x020))
#define	DBMX1_EIM_CS4L		(*(volatile unsigned int *)(VA_EIM + 0x024))
#define	DBMX1_EIM_CS5U		(*(volatile unsigned int *)(VA_EIM + 0x028))
#define	DBMX1_EIM_CS5L		(*(volatile unsigned int *)(VA_EIM + 0x02c))
#define	DBMX1_EIM_EIM		(*(volatile unsigned int *)(VA_EIM + 0x030))


#define VA_AITC			IO_ADDRESS(DBMX1_AITC_BASE)
#define DBMX1_AITC_INTCNTL	(*(volatile unsigned int *)(VA_AITC + 0x000))
#define DBMX1_AITC_NIMASK	(*(volatile unsigned int *)(VA_AITC + 0x004))
#define DBMX1_AITC_INTENNUM	(*(volatile unsigned int *)(VA_AITC + 0x008))
#define DBMX1_AITC_INTDISNUM	(*(volatile unsigned int *)(VA_AITC + 0x00c))
#define DBMX1_AITC_INTENABLEH	(*(volatile unsigned int *)(VA_AITC + 0x010))
#define DBMX1_AITC_INTENABLEL	(*(volatile unsigned int *)(VA_AITC + 0x014))
#define DBMX1_AITC_INTTYPEH	(*(volatile unsigned int *)(VA_AITC + 0x018))
#define DBMX1_AITC_INTTYPEL	(*(volatile unsigned int *)(VA_AITC + 0x01c))
#define DBMX1_AITC_NIPRIORITY7	(*(volatile unsigned int *)(VA_AITC + 0x020))
#define DBMX1_AITC_NIPRIORITY6	(*(volatile unsigned int *)(VA_AITC + 0x024))
#define DBMX1_AITC_NIPRIORITY5	(*(volatile unsigned int *)(VA_AITC + 0x028))
#define DBMX1_AITC_NIPRIORITY4	(*(volatile unsigned int *)(VA_AITC + 0x02c))
#define DBMX1_AITC_NIPRIORITY3	(*(volatile unsigned int *)(VA_AITC + 0x030))
#define DBMX1_AITC_NIPRIORITY2	(*(volatile unsigned int *)(VA_AITC + 0x034))
#define DBMX1_AITC_NIPRIORITY1	(*(volatile unsigned int *)(VA_AITC + 0x038))
#define DBMX1_AITC_NIPRIORITY0	(*(volatile unsigned int *)(VA_AITC + 0x03c))
#define DBMX1_AITC_NIVECSR	(*(volatile unsigned int *)(VA_AITC + 0x040))
#define DBMX1_AITC_FIVECSR	(*(volatile unsigned int *)(VA_AITC + 0x044))
#define DBMX1_AITC_INTSRCH	(*(volatile unsigned int *)(VA_AITC + 0x048))
#define DBMX1_AITC_INTSRCL	(*(volatile unsigned int *)(VA_AITC + 0x04c))
#define DBMX1_AITC_INTFRCH	(*(volatile unsigned int *)(VA_AITC + 0x050))
#define DBMX1_AITC_INTFRCL	(*(volatile unsigned int *)(VA_AITC + 0x054))
#define DBMX1_AITC_NIPNDH	(*(volatile unsigned int *)(VA_AITC + 0x058))
#define DBMX1_AITC_NIPNDL	(*(volatile unsigned int *)(VA_AITC + 0x05c))
#define DBMX1_AITC_FIPNDH	(*(volatile unsigned int *)(VA_AITC + 0x060))
#define DBMX1_AITC_FIPNDL	(*(volatile unsigned int *)(VA_AITC + 0x064))



/*
 *  MX1 Interrupt numbers
 *
 */
#define IRQ_CSI_INT		 6
#define IRQ_MMA_MAC_INT		 7
#define IRQ_MMA_INT		 8
#define IRQ_COMP_INT		 9
#define IRQ_MSIRQ		10
#define IRQ_GPIO_INT_PORTA	11
#define IRQ_GPIO_INT_PORTB	12
#define IRQ_GPIO_INT_PORTC	13
#define IRQ_LCDC_INT		14
#define IRQ_SIM_IRQ		15
#define IRQ_SIM_DATA		16
#define IRQ_RTC_INT		17
#define IRQ_RTC_SAM_INT		18
#define IRQ_UART2_MINT_PFERR	19
#define IRQ_UART2_MINT_RTS	20
#define IRQ_UART2_MINT_DTR	21
#define IRQ_UART2_MINT_UARTC	22
#define IRQ_UART2_MINT_TX	23
#define IRQ_UART2_MINT_RX	24
#define IRQ_UART1_MINT_PFERR	25
#define IRQ_UART1_MINT_RTS	26
#define IRQ_UART1_MINT_DTR	27
#define IRQ_UART1_MINT_UARTC	28
#define IRQ_UART1_MINT_TX	29
#define IRQ_UART1_MINT_RX	30
#define IRQ_VOICE_DAC_INT	31
#define IRQ_VOICE_ADC_INT	32
#define IRQ_PEN_DATA_INT	33
#define IRQ_PWM_INT		34
#define IRQ_MMC_INT		35
#define IRQ_I2C_INT		39
#define IRQ_SPI2_INT		40
#define IRQ_SPI1_INT		41
#define IRQ_SSI_TX_INT		42
#define IRQ_SSI_TX_ERR_INT	43
#define IRQ_SSI_RX_INT		44
#define IRQ_SSI_RX_ERR_INT	45
#define IRQ_TOUCH_INT		46
#define IRQ_USBD_INT0		47
#define IRQ_USBD_INT1		48
#define IRQ_USBD_INT2		49
#define IRQ_USBD_INT3		50
#define IRQ_USBD_INT4		51
#define IRQ_USBD_INT5		52
#define IRQ_USBD_INT6		53
#define IRQ_BTSYS		55
#define IRQ_BTTIM		56
#define IRQ_BTWUI		57
#define IRQ_TIMER2_INT		58
#define IRQ_TIMER1_INT		59
#define IRQ_DMA_ERR		60
#define IRQ_DMA_INT		61
#define IRQ_GPIO_INT_PORTD	62
#define IRQ_WDT_INT		62

#define	IRQ_GPIO_A(x)	(64 + (x))
#define	IRQ_GPIO_B(x)	(IRQ_GPIO_A(31) + 1 + (x))

#define DBMX1_MAXIRQNUM		IRQ_GPIO_B(31)

#ifdef CONFIG_DBMX1_TPM102
#include <asm/arch/tpm102.h>
#endif

#ifndef MAXIRQNUM
#define MAXIRQNUM		DBMX1_MAXIRQNUM
#endif

#define MAXFIQNUM		MAXIRQNUM
#define MAXSWINUM		MAXIRQNUM

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
