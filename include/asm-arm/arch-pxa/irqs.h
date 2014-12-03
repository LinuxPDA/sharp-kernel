/*
 *  linux/include/asm-arm/arch-pxa/irqs.h
 *  
 *  Author:	Nicolas Pitre
 *  Created:	Jun 15, 2001
 *  Copyright:	MontaVista Software Inc.
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ChangLog:
 *	12-Dec-2002 Lineo Japan, Inc.
 */

#define PXA_IRQ_SKIP	8	/* The first 8 IRQs are reserved */
#define PXA_IRQ(x)		((x) - PXA_IRQ_SKIP)

#define	IRQ_GPIO0	PXA_IRQ(8)	/* GPIO0 Edge Detect */
#define	IRQ_GPIO1	PXA_IRQ(9)	/* GPIO1 Edge Detect */
#define	IRQ_GPIO_2_80	PXA_IRQ(10)	/* GPIO[2-80] Edge Detect */
#define	IRQ_USB		PXA_IRQ(11)	/* USB Service */
#define	IRQ_PMU		PXA_IRQ(12)	/* Performance Monitoring Unit */
#define	IRQ_I2S		PXA_IRQ(13)	/* I2S Interrupt */
#define	IRQ_AC97	PXA_IRQ(14)	/* AC97 Interrupt */
#define	IRQ_LCD		PXA_IRQ(17)	/* LCD Controller Service Request */
#define	IRQ_I2C		PXA_IRQ(18)	/* I2C Service Request */
#define	IRQ_ICP		PXA_IRQ(19)	/* ICP Transmit/Receive/Error */
#define	IRQ_STUART	PXA_IRQ(20)	/* STUART Transmit/Receive/Error */
#define	IRQ_BTUART	PXA_IRQ(21)	/* BTUART Transmit/Receive/Error */
#define	IRQ_FFUART	PXA_IRQ(22)	/* FFUART Transmit/Receive/Error*/
#define	IRQ_MMC		PXA_IRQ(23)	/* MMC Status/Error Detection */
#define	IRQ_SSP		PXA_IRQ(24)	/* SSP Service Request */
#define	IRQ_DMA 	PXA_IRQ(25)	/* DMA Channel Service Request */
#define	IRQ_OST0 	PXA_IRQ(26)	/* OS Timer match 0 */
#define	IRQ_OST1 	PXA_IRQ(27)	/* OS Timer match 1 */
#define	IRQ_OST2 	PXA_IRQ(28)	/* OS Timer match 2 */
#define	IRQ_OST3 	PXA_IRQ(29)	/* OS Timer match 3 */
#define	IRQ_RTC1Hz	PXA_IRQ(30)	/* RTC HZ Clock Tick */
#define	IRQ_RTCAlrm	PXA_IRQ(31)	/* RTC Alarm */

#define GPIO_2_80_TO_IRQ(x)	\
			PXA_IRQ((x) - 2 + 32)
#define IRQ_GPIO(x)	(((x) < 2) ? (IRQ_GPIO0 + (x)) : GPIO_2_80_TO_IRQ(x))

#define IRQ_TO_GPIO_2_80(i)	\
			((i) - PXA_IRQ(32) + 2)
#define IRQ_TO_GPIO(i)	((i) - (((i) > IRQ_GPIO1) ? IRQ_GPIO(2) : IRQ_GPIO(0)))

#define	NR_IRQS		(IRQ_GPIO(80) + 1)

#if defined(CONFIG_SA1111)

#define IRQ_SA1111_START	(IRQ_GPIO(80) + 1)
#define SA1111_IRQ(x)		(IRQ_SA1111_START + (x))

#define IRQ_GPAIN0		SA1111_IRQ(0)
#define IRQ_GPAIN1		SA1111_IRQ(1)
#define IRQ_GPAIN2		SA1111_IRQ(2)
#define IRQ_GPAIN3		SA1111_IRQ(3)
#define IRQ_GPBIN0		SA1111_IRQ(4)
#define IRQ_GPBIN1		SA1111_IRQ(5)
#define IRQ_GPBIN2		SA1111_IRQ(6)
#define IRQ_GPBIN3		SA1111_IRQ(7)
#define IRQ_GPBIN4		SA1111_IRQ(8)
#define IRQ_GPBIN5		SA1111_IRQ(9)
#define IRQ_GPCIN0		SA1111_IRQ(10)
#define IRQ_GPCIN1		SA1111_IRQ(11)
#define IRQ_GPCIN2		SA1111_IRQ(12)
#define IRQ_GPCIN3		SA1111_IRQ(13)
#define IRQ_GPCIN4		SA1111_IRQ(14)
#define IRQ_GPCIN5		SA1111_IRQ(15)
#define IRQ_GPCIN6		SA1111_IRQ(16)
#define IRQ_GPCIN7		SA1111_IRQ(17)
#define IRQ_MSTXINT		SA1111_IRQ(18)
#define IRQ_MSRXINT		SA1111_IRQ(19)
#define IRQ_MSSTOPERRINT	SA1111_IRQ(20)
#define IRQ_TPTXINT		SA1111_IRQ(21)
#define IRQ_TPRXINT		SA1111_IRQ(22)
#define IRQ_TPSTOPERRINT	SA1111_IRQ(23)
#define SSPXMTINT	SA1111_IRQ(24)
#define SSPRCVINT	SA1111_IRQ(25)
#define SSPROR		SA1111_IRQ(26)
#define AUDXMTDMADONEA	SA1111_IRQ(32)
#define AUDRCVDMADONEA	SA1111_IRQ(33)
#define AUDXMTDMADONEB	SA1111_IRQ(34)
#define AUDRCVDMADONEB	SA1111_IRQ(35)
#define AUDTFSR		SA1111_IRQ(36)
#define AUDRFSR		SA1111_IRQ(37)
#define AUDTUR		SA1111_IRQ(38)
#define AUDROR		SA1111_IRQ(39)
#define AUDDTS		SA1111_IRQ(40)
#define AUDRDD		SA1111_IRQ(41)
#define AUDSTO		SA1111_IRQ(42)
#define USBPWR		SA1111_IRQ(43)
#define NIRQHCIM	SA1111_IRQ(44)
#define HCIBUFFACC	SA1111_IRQ(45)
#define HCIRMTWKP	SA1111_IRQ(46)
#define NHCIMFCIR	SA1111_IRQ(47)
#define PORT_RESUME	SA1111_IRQ(48)
#define S0_READY_NINT	SA1111_IRQ(49)
#define S1_READY_NINT	SA1111_IRQ(50)
#define S0_CD_VALID	SA1111_IRQ(51)
#define S1_CD_VALID	SA1111_IRQ(52)
#define S0_BVD1_STSCHG	SA1111_IRQ(53)
#define S1_BVD1_STSCHG	SA1111_IRQ(54)

#define SA1111_IRQ_MAX	SA1111_IRQ(54)

#undef NR_IRQS
#define NR_IRQS		(SA1111_IRQ_MAX + 1)

#endif	// defined(CONFIG_SA1111)

#if defined(CONFIG_ARCH_LUBBOCK) || defined(CONFIG_ARCH_PXA_IDP) 
#if CONFIG_SA1111
#define LUBBOCK_IRQ(x)	(SA1111_IRQ_MAX + 1 + (x))
#else
#define LUBBOCK_IRQ(x)	(IRQ_GPIO(80) + 1 + (x))
#endif

#define LUBBOCK_SD_IRQ		LUBBOCK_IRQ(0)
#define LUBBOCK_SA1111_IRQ	LUBBOCK_IRQ(1)
#define LUBBOCK_USB_IRQ		LUBBOCK_IRQ(2)
#define LUBBOCK_ETH_IRQ		LUBBOCK_IRQ(3)
#define LUBBOCK_UCB1400_IRQ	LUBBOCK_IRQ(4)
#define LUBBOCK_BB_IRQ		LUBBOCK_IRQ(5)

#undef NR_IRQS
#define NR_IRQS		(LUBBOCK_IRQ(5) + 1)

#endif	// CONFIG_ARCH_LUBBOCK


//steve TBD
#ifdef CONFIG_SABINAL_DISCOVERY

// Richard 0418
#define IRQ_DISCOVERY_CF_RDYBSY 	IRQ_GPIO(16)
#define IRQ_DISCOVERY_CF_IRQ 		IRQ_DISCOVERY_CF_RDYBSY
#define IRQ_DISCOVERY_RDY		IRQ_GPIO(18)
#define IRQ_DISCOVERY_SPIRXD		IRQ_GPIO(26)
#define IRQ_DISCOVERY_I2S_DATAIN	IRQ_GPIO(29)
#define IRQ_DISCOVERY_COM_RXD		IRQ_GPIO(42)
#define IRQ_DISCOVERY_COM_CTS		IRQ_GPIO(44)
#define IRQ_DISCOVERY_IR_RXD		IRQ_GPIO(46)
#define IRQ_DISCOVERY_SD_WP_HOST	IRQ_GPIO(48)
#define IRQ_DISCOVERY_USB_GPIO		IRQ_GPIO(50)
#define IRQ_DISCOVERY_PEN_IRQ		IRQ_GPIO(51)
#define IRQ_DISCOVERY_ACTION		IRQ_GPIO(52)
#define IRQ_DISCOVERY_SD_INT		IRQ_GPIO(54)
// End Richard 0418

#if 0 // Richard 0425 modify for ASIC3
#define IRQ_ASIC1_0              (IRQ_GPIO80+1)
#define IRQ_ASIC1_1              (IRQ_GPIO80+2)
#define IRQ_ASIC1_2              (IRQ_GPIO80+3)
#define IRQ_ASIC1_3              (IRQ_GPIO80+4)
#define IRQ_ASIC1_4              (IRQ_GPIO80+5)
#define IRQ_ASIC1_5              (IRQ_GPIO80+6)
#define IRQ_ASIC1_6              (IRQ_GPIO80+7)
#define IRQ_ASIC1_7              (IRQ_GPIO80+8)
#define IRQ_ASIC1_8              (IRQ_GPIO80+9)
#define IRQ_ASIC1_9              (IRQ_GPIO80+10)
#define IRQ_ASIC1_10             (IRQ_GPIO80+11)
#define IRQ_ASIC1_11             (IRQ_GPIO80+12)
#define IRQ_ASIC1_12             (IRQ_GPIO80+13)
#define IRQ_ASIC1_13             (IRQ_GPIO80+14)
#define IRQ_ASIC1_14             (IRQ_GPIO80+15)
#define IRQ_ASIC1_15		(IRQ_GPIO80+16)
#endif

#define IRQ_ASIC3_A		IRQ_GPIO(80)

#define IRQ_ASIC3_A0 		(IRQ_ASIC3_A + 1)
#define IRQ_ASIC3_A1		(IRQ_ASIC3_A + 2)
#define IRQ_ASIC3_A2		(IRQ_ASIC3_A + 3)
#define IRQ_ASIC3_A3		(IRQ_ASIC3_A + 4)
#define IRQ_ASIC3_A4		(IRQ_ASIC3_A + 5)
#define IRQ_ASIC3_A5		(IRQ_ASIC3_A + 6)
#define IRQ_ASIC3_A6		(IRQ_ASIC3_A + 7)
#define IRQ_ASIC3_A7		(IRQ_ASIC3_A + 8)
#define IRQ_ASIC3_A8		(IRQ_ASIC3_A + 9)
#define IRQ_ASIC3_A9		(IRQ_ASIC3_A + 10)
#define IRQ_ASIC3_A10		(IRQ_ASIC3_A + 11)
#define IRQ_ASIC3_A11		(IRQ_ASIC3_A + 12)
#define IRQ_ASIC3_A12		(IRQ_ASIC3_A + 13)
#define IRQ_ASIC3_A13		(IRQ_ASIC3_A + 14)
#define IRQ_ASIC3_A14		(IRQ_ASIC3_A + 15)
#define IRQ_ASIC3_A15		(IRQ_ASIC3_A + 16)

#define IRQ_ASIC3_B		IRQ_ASIC3_A15

#define IRQ_ASIC3_B0		(IRQ_ASIC3_B + 1)
#define IRQ_ASIC3_B1		(IRQ_ASIC3_B + 2)
#define IRQ_ASIC3_B2		(IRQ_ASIC3_B + 3)
#define IRQ_ASIC3_B3		(IRQ_ASIC3_B + 4)
#define IRQ_ASIC3_B4		(IRQ_ASIC3_B + 5)
#define IRQ_ASIC3_B5		(IRQ_ASIC3_B + 6)
#define IRQ_ASIC3_B6		(IRQ_ASIC3_B + 7)
#define IRQ_ASIC3_B7		(IRQ_ASIC3_B + 8)
#define IRQ_ASIC3_B8		(IRQ_ASIC3_B + 9)
#define IRQ_ASIC3_B9		(IRQ_ASIC3_B + 10)
#define IRQ_ASIC3_B10		(IRQ_ASIC3_B + 11)
#define IRQ_ASIC3_B11		(IRQ_ASIC3_B + 12)
#define IRQ_ASIC3_B12		(IRQ_ASIC3_B + 13)
#define IRQ_ASIC3_B13		(IRQ_ASIC3_B + 14)
#define IRQ_ASIC3_B14		(IRQ_ASIC3_B + 15)
#define IRQ_ASIC3_B15		(IRQ_ASIC3_B + 16)

#define IRQ_ASIC3_C		IRQ_ASIC3_B15

#define IRQ_ASIC3_C0		(IRQ_ASIC3_C + 1)
#define IRQ_ASIC3_C1		(IRQ_ASIC3_C + 2)
#define IRQ_ASIC3_C2		(IRQ_ASIC3_C + 3)
#define IRQ_ASIC3_C3		(IRQ_ASIC3_C + 4)
#define IRQ_ASIC3_C4		(IRQ_ASIC3_C + 5)
#define IRQ_ASIC3_C5		(IRQ_ASIC3_C + 6)
#define IRQ_ASIC3_C6		(IRQ_ASIC3_C + 7)
#define IRQ_ASIC3_C7		(IRQ_ASIC3_C + 8)
#define IRQ_ASIC3_C8		(IRQ_ASIC3_C + 9)
#define IRQ_ASIC3_C9		(IRQ_ASIC3_C + 10)
#define IRQ_ASIC3_C10		(IRQ_ASIC3_C + 11)
#define IRQ_ASIC3_C11		(IRQ_ASIC3_C + 12)
#define IRQ_ASIC3_C12		(IRQ_ASIC3_C + 13)
#define IRQ_ASIC3_C13		(IRQ_ASIC3_C + 14)
#define IRQ_ASIC3_C14		(IRQ_ASIC3_C + 15)
#define IRQ_ASIC3_C15		(IRQ_ASIC3_C + 16)

#define IRQ_ASIC3_D		IRQ_ASIC3_C15

#define IRQ_ASIC3_D0		(IRQ_ASIC3_D + 1)
#define IRQ_ASIC3_D1		(IRQ_ASIC3_D + 2)
#define IRQ_ASIC3_D2		(IRQ_ASIC3_D + 3)
#define IRQ_ASIC3_D3		(IRQ_ASIC3_D + 4)
#define IRQ_ASIC3_D4		(IRQ_ASIC3_D + 5)
#define IRQ_ASIC3_D5		(IRQ_ASIC3_D + 6)
#define IRQ_ASIC3_D6		(IRQ_ASIC3_D + 7)
#define IRQ_ASIC3_D7		(IRQ_ASIC3_D + 8)
#define IRQ_ASIC3_D8		(IRQ_ASIC3_D + 9)
#define IRQ_ASIC3_D9		(IRQ_ASIC3_D + 10)
#define IRQ_ASIC3_D10		(IRQ_ASIC3_D + 11)
#define IRQ_ASIC3_D11		(IRQ_ASIC3_D + 12)
#define IRQ_ASIC3_D12		(IRQ_ASIC3_D + 13)
#define IRQ_ASIC3_D13		(IRQ_ASIC3_D + 14)
#define IRQ_ASIC3_D14		(IRQ_ASIC3_D + 15)
#define IRQ_ASIC3_D15		(IRQ_ASIC3_D + 16)

#if 0
#define ASIC1_WAKE_UP          (0)
#define ASIC1_CF_IOIS16        (1)
#define ASIC1_SD_DETECT        (2)
#define ASIC1_CF_DETECT	       (3)
#define ASIC1_CF_CHG_EN        (4)
#define ASIC1_KEY_IN           (5)
#define ASIC1_KEY_INTERRUPT    (6)
#define ASIC1_HEADPHONE        (7)
#define ASIC1_AC_IN            (8)
#define ASIC1_PWR_ON           (9)
#define ASIC1_COM_DCD          (10)
#define ASIC1_CONN60_4         (11)
#define ASIC1_CONN60_5         (12)
#define ASIC1_BACKPART_DETECT  (13)
#define ASIC1_CF_BATT_FAULT    (14)

#define IRQ_ASIC1_WAKE_UP			IRQ_ASIC1_0
#define IRQ_ASIC1_CF_IOIS16			IRQ_ASIC1_1
#define IRQ_ASIC1_SD_DETECT			IRQ_ASIC1_2
#define IRQ_ASIC1_CF_DETECT			IRQ_ASIC1_3
#define IRQ_ASIC1_CF_CHG_EN			IRQ_ASIC1_4
#define IRQ_ASIC1_KEY_IN			IRQ_ASIC1_5
#define IRQ_ASIC1_KEY_INTERRUPT			IRQ_ASIC1_6
#define IRQ_ASIC1_HEADPHONE			IRQ_ASIC1_7
#define IRQ_ASIC1_AC_IN				IRQ_ASIC1_8
#define IRQ_ASIC1_PWR_ON			IRQ_ASIC1_9
#define IRQ_ASIC1_COM_DCD			IRQ_ASIC1_10
#define IRQ_ASIC1_CONN60_4			IRQ_ASIC1_11
#define IRQ_ASIC1_CONN60_5			IRQ_ASIC1_12
#define IRQ_ASIC1_BACKPART_DETECT		IRQ_ASIC1_13
#define IRQ_ASIC1_CF_BATT_FAULT			IRQ_ASIC1_14

#define ASIC1_WAKE_UP_BIT          (1 << 0)
#define ASIC1_CF_IOIS16_BIT        (1 << 1)
#define ASIC1_SD_DETECT_BIT        (1 << 2)
#define ASIC1_CF_DETECT_BIT	   (1 << 3)
#define ASIC1_CF_CHG_EN_BIT        (1 << 4)
#define ASIC1_KEY_IN_BIT           (1 << 5)
#define ASIC1_KEY_INTERRUPT_BIT    (1 << 6)
#define ASIC1_HEADPHONE_BIT        (1 << 7)
#define ASIC1_AC_IN_BIT            (1 << 8)
#define ASIC1_PWR_ON_BIT           (1 << 9)
#define ASIC1_COM_DCD_BIT          (1 << 10)
#define ASIC1_CONN60_4_BIT         (1 << 11)
#define ASIC1_CONN60_5_BIT         (1 << 12)
#define ASIC1_BACKPART_DETECT_BIT  (1 << 13)
#define ASIC1_CF_BATT_FAULT_BIT    (1 << 14)
#endif

#define IRQ_ASIC3_KEY0			IRQ_ASIC3_A0
#define IRQ_ASIC3_KEY1			IRQ_ASIC3_A1
#define IRQ_ASIC3_KEY2			IRQ_ASIC3_A2
#define IRQ_ASIC3_KEY3			IRQ_ASIC3_A3
#define IRQ_ASIC3_UP			IRQ_ASIC3_A4
#define IRQ_ASIC3_DOWN			IRQ_ASIC3_A5
#define IRQ_ASIC3_LEFT			IRQ_ASIC3_A6
#define IRQ_ASIC3_RIGHT			IRQ_ASIC3_A7
#define IRQ_ASIC3_ACTION		IRQ_ASIC3_A8
#define IRQ_ASIC3_PWR_ON		IRQ_ASIC3_A9

#define IRQ_ASIC3_COM_DCD		IRQ_ASIC3_D0
#define IRQ_ASIC3_AC_IN			IRQ_ASIC3_D1
#define IRQ_ASIC3_HEADPHONE_IN		IRQ_ASIC3_D2
#define IRQ_ASIC3_WAKEUP		IRQ_ASIC3_D3
#define IRQ_ASIC3_SD_DETECT		IRQ_ASIC3_D4
#define IRQ_ASIC3_BACKPART_DET		IRQ_ASIC3_D5
#define IRQ_ASIC3_BACK_INTERRUPT	IRQ_ASIC3_D6
#define IRQ_ASIC3_CF_CHG_EN		IRQ_ASIC3_D7
#define IRQ_ASIC3_CF_DETECT		IRQ_ASIC3_D8
#define IRQ_ASIC3_CF_BATT_FAULT		IRQ_ASIC3_D9
#define IRQ_ASIC3_CF_IOIS16		IRQ_ASIC3_D10
#define IRQ_ASIC3_CF_STSCHG		IRQ_ASIC3_D11

#define IRQ_ASIC1_WAKE_UP			IRQ_ASIC3_WAKEUP
#define IRQ_ASIC1_CF_IOIS16			IRQ_ASIC3_CF_IOIS16
#define IRQ_ASIC1_SD_DETECT			IRQ_ASIC3_SD_DETECT
#define IRQ_ASIC1_CF_DETECT			IRQ_ASIC3_CF_DETECT
#define IRQ_ASIC1_CF_CHG_EN			IRQ_ASIC3_CF_CHG_EN
//#define IRQ_ASIC1_KEY_IN			IRQ_ASIC1_5
//#define IRQ_ASIC1_KEY_INTERRUPT		IRQ_ASIC1_6
#define IRQ_ASIC1_HEADPHONE			IRQ_ASIC3_HEADPHONE_IN
#define IRQ_ASIC1_AC_IN				IRQ_ASIC3_AC_IN
#define IRQ_ASIC1_PWR_ON			IRQ_ASIC3_PWR_ON
#define IRQ_ASIC1_COM_DCD			IRQ_ASIC3_COM_DCD
//#define IRQ_ASIC1_CONN60_4			IRQ_ASIC1_11
//#define IRQ_ASIC1_CONN60_5			IRQ_ASIC1_12
#define IRQ_ASIC1_BACKPART_DETECT		IRQ_ASIC3_BACKPART_DET
#define IRQ_ASIC1_CF_BATT_FAULT			IRQ_ASIC3_CF_BATT_FAULT

#undef NR_IRQS
#define	NR_IRQS			(IRQ_ASIC3_D15 + 1)
#define IRQ_ASIC1_GPIO_NUM(n)	(IRQ_ASIC3_D15 + n)
#define ASIC1_GPIO_NUM_IRQ(i)	(i - IRQ_ASIC3_D15)

#define GPIO_ASIC1 (0)

#endif

#if defined(CONFIG_ARCH_PXA_POODLE)
#if CONFIG_SA1111
#error POODLE configuration error
#else
#define LCM_IRQ(x)	(IRQ_GPIO(80) + 1 + (x))
#endif

#define LCM_IRQ_KEY_BASE	LCM_IRQ(0)
#define LCM_IRQ_GPIO_BASE	LCM_IRQ(1)
#define LCM_IRQ_LT_BASE		LCM_IRQ(2)
#define LCM_IRQ_SPI_BASE	LCM_IRQ(3)

#define	LCM_IRQ_KEY_OFFSET(x)	(LCM_IRQ(3) + 1 + (x))

#define LCM_IRQ_KEY             LCM_IRQ_KEY_OFFSET(0)

#define	LCM_IRQ_GPIO_OFFSET(x)	(LCM_IRQ_KEY_OFFSET(0) + 1 + (x))

#define LCM_IRQ_GPIO0		LCM_IRQ_GPIO_OFFSET(0)
#define LCM_IRQ_GPIO1		LCM_IRQ_GPIO_OFFSET(1)
#define LCM_IRQ_GPIO2		LCM_IRQ_GPIO_OFFSET(2)
#define LCM_IRQ_GPIO3		LCM_IRQ_GPIO_OFFSET(3)
#define LCM_IRQ_GPIO4		LCM_IRQ_GPIO_OFFSET(4)
#define LCM_IRQ_GPIO5		LCM_IRQ_GPIO_OFFSET(5)
#define LCM_IRQ_GPIO6		LCM_IRQ_GPIO_OFFSET(6)
#define LCM_IRQ_GPIO7		LCM_IRQ_GPIO_OFFSET(7)
#define LCM_IRQ_GPIO8		LCM_IRQ_GPIO_OFFSET(8)
#define LCM_IRQ_GPIO9		LCM_IRQ_GPIO_OFFSET(9)
#define LCM_IRQ_GPIO10		LCM_IRQ_GPIO_OFFSET(10)
#define LCM_IRQ_GPIO11		LCM_IRQ_GPIO_OFFSET(11)
#define LCM_IRQ_GPIO12		LCM_IRQ_GPIO_OFFSET(12)
#define LCM_IRQ_GPIO13		LCM_IRQ_GPIO_OFFSET(13)
#define LCM_IRQ_GPIO14		LCM_IRQ_GPIO_OFFSET(14)
#define LCM_IRQ_GPIO15		LCM_IRQ_GPIO_OFFSET(15)

#define	LCM_IRQ_LT_OFFSET(x)	(LCM_IRQ_GPIO_OFFSET(15) + 1 + (x))

#define LCM_IRQ_LT              LCM_IRQ_LT_OFFSET(0)

#define	LCM_IRQ_SPI_OFFSET(x)	(LCM_IRQ_LT_OFFSET(0) + 1 + (x))

#define LCM_IRQ_SPI_RFR		LCM_IRQ_SPI_OFFSET(0)
#define LCM_IRQ_SPI_RFW		LCM_IRQ_SPI_OFFSET(1)
#define LCM_IRQ_SPI_OVRN	LCM_IRQ_SPI_OFFSET(2)
#define LCM_IRQ_SPI_TEND	LCM_IRQ_SPI_OFFSET(3)

#define POODLE_IRQ_MAX		LCM_IRQ_SPI_OFFSET(3)

#undef  NR_IRQS
#define NR_IRQS			(POODLE_IRQ_MAX + 1)

#endif	// CONFIG_ARCH_PXA_POODLE
