/*
 * linux/include/asm-arm/arch-cotulla/irqs.h
 *
 * Copyright (C) 2001 LINEO Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * ChangeLog:
 *  04-04-2002 Richard Rau : add IRQ def. of Discovery
 */

#ifndef __IRQS_H_
#define __IRQS_H_

#include <linux/config.h>

#define COTULLA_IRQ(x)		(0 + (x))

#define	IRQ_GPIO0		COTULLA_IRQ(8)  /* GPIO #0 Interrupt */
#define	IRQ_GPIO1		COTULLA_IRQ(9)  /* GPIO #1 Interrupt */
#define	IRQ_GPIO2_80		COTULLA_IRQ(10) /* GPIO[80..2]Interrupt */

#define IRQ_USB			COTULLA_IRQ(11)  /* USB Service Interrupt */
#define IRQ_PMU			COTULLA_IRQ(12) /* PMU unit Interrupt */
#define IRQ_I2S			COTULLA_IRQ(13) /* I2S Interrupt */
#define IRQ_AC9			COTULLA_IRQ(14) /* AC97 Interrupt */
#define IRQ_LCD			COTULLA_IRQ(17) /* LCD request Interrupt */
#define IRQ_I2C			COTULLA_IRQ(18) /* I2C Interrupt */
#define IRQ_ICP			COTULLA_IRQ(19) /* ICP Interrupt */
#define IRQ_STUART		COTULLA_IRQ(20) /* STUART Interrupt */
#define IRQ_BTUART		COTULLA_IRQ(21) /* BTUART Interrupt */
#define IRQ_FFUART		COTULLA_IRQ(22) /* FFUART Interrupt */
#define IRQ_MMC			COTULLA_IRQ(23) /* MMC Detection Interrupt */
#define IRQ_SSP			COTULLA_IRQ(24) /* SSP Request Interrupt */
#define IRQ_DMA			COTULLA_IRQ(25) /* DMA Request Interrupt */
#define IRQ_OST0		COTULLA_IRQ(26) /* OS Timer #0 Interrupt */
#define IRQ_OST1		COTULLA_IRQ(27) /* OS Timer #1 Interrupt */
#define IRQ_OST2		COTULLA_IRQ(28) /* OS Timer #2 Interrupt */
#define IRQ_OST3		COTULLA_IRQ(29) /* OS Timer #3 Inerrupt */
#define IRQ_RTC1Hz		COTULLA_IRQ(30) /* RTC Tick Interrupt */
#define IRQ_RTCAlrm		COTULLA_IRQ(31) /* RTC Alarm Interrupt */

#define	IRQ_GPIO_2_31(x)	(32 + (x) - 2)
#define	IRQ_GPIO_32_63(x)	(64 + (x) - 2)
#define	IRQ_GPIO_64_80(x)	(96 + (x) - 2)
#define IRQ_GPIO_NUM(n, x)	((n) == 0 ? IRQ_GPIO_2_31((x)) : ((n) == 1 ? IRQ_GPIO_32_63(x) : IRQ_GPIO_64_80(x)))
#define IRQ_GPIO_2_80(i)       (((i) < 2 ) ? COTULLA_IRQ(i) : (32 + (i) - 2))

#define IRQ_GPIO2               IRQ_GPIO_2_80(2)
#define IRQ_GPIO3               IRQ_GPIO_2_80(3)
#define IRQ_GPIO4               IRQ_GPIO_2_80(4)
#define IRQ_GPIO5               IRQ_GPIO_2_80(5)
#define IRQ_GPIO6               IRQ_GPIO_2_80(6)
#define IRQ_GPIO7               IRQ_GPIO_2_80(7)
#define IRQ_GPIO8               IRQ_GPIO_2_80(8)
#define IRQ_GPIO9               IRQ_GPIO_2_80(9)
#define IRQ_GPIO10		IRQ_GPIO_2_80(10)
#define	IRQ_GPIO11		IRQ_GPIO_2_80(11)
#define	IRQ_GPIO12		IRQ_GPIO_2_80(12)
#define	IRQ_GPIO13		IRQ_GPIO_2_80(13)
#define	IRQ_GPIO14		IRQ_GPIO_2_80(14)
#define	IRQ_GPIO15		IRQ_GPIO_2_80(15)
#define	IRQ_GPIO16		IRQ_GPIO_2_80(16)
#define	IRQ_GPIO17		IRQ_GPIO_2_80(17)
#define	IRQ_GPIO18		IRQ_GPIO_2_80(18)
#define	IRQ_GPIO19		IRQ_GPIO_2_80(19)
#define	IRQ_GPIO20		IRQ_GPIO_2_80(20)
#define	IRQ_GPIO21		IRQ_GPIO_2_80(21)
#define	IRQ_GPIO22		IRQ_GPIO_2_80(22)
#define	IRQ_GPIO23		IRQ_GPIO_2_80(23)
#define	IRQ_GPIO24		IRQ_GPIO_2_80(24)
#define	IRQ_GPIO25		IRQ_GPIO_2_80(25)
#define	IRQ_GPIO26		IRQ_GPIO_2_80(26)
#define	IRQ_GPIO27		IRQ_GPIO_2_80(27)
#define	IRQ_GPIO28		IRQ_GPIO_2_80(28)
#define	IRQ_GPIO29		IRQ_GPIO_2_80(29)
#define IRQ_GPIO30              IRQ_GPIO_2_80(30)
#define IRQ_GPIO31              IRQ_GPIO_2_80(31)
#define IRQ_GPIO32              IRQ_GPIO_2_80(32)
#define IRQ_GPIO33              IRQ_GPIO_2_80(33)
#define IRQ_GPIO34              IRQ_GPIO_2_80(34)
#define IRQ_GPIO35              IRQ_GPIO_2_80(35)
#define IRQ_GPIO36              IRQ_GPIO_2_80(36)
#define IRQ_GPIO37              IRQ_GPIO_2_80(37)
#define IRQ_GPIO38              IRQ_GPIO_2_80(38)
#define IRQ_GPIO39              IRQ_GPIO_2_80(39)
#define IRQ_GPIO40              IRQ_GPIO_2_80(40)
#define IRQ_GPIO41              IRQ_GPIO_2_80(41)
#define IRQ_GPIO42              IRQ_GPIO_2_80(42)
#define IRQ_GPIO43              IRQ_GPIO_2_80(43)
#define IRQ_GPIO44              IRQ_GPIO_2_80(44)
#define IRQ_GPIO45              IRQ_GPIO_2_80(45)
#define IRQ_GPIO46              IRQ_GPIO_2_80(46)
#define IRQ_GPIO47              IRQ_GPIO_2_80(47)
#define IRQ_GPIO48              IRQ_GPIO_2_80(48)
#define IRQ_GPIO49              IRQ_GPIO_2_80(49)
#define IRQ_GPIO50              IRQ_GPIO_2_80(50)
#define IRQ_GPIO51              IRQ_GPIO_2_80(51)
#define IRQ_GPIO52              IRQ_GPIO_2_80(52)
#define IRQ_GPIO53              IRQ_GPIO_2_80(53)
#define IRQ_GPIO54              IRQ_GPIO_2_80(54)
#define IRQ_GPIO55              IRQ_GPIO_2_80(55)
#define IRQ_GPIO56              IRQ_GPIO_2_80(56)
#define IRQ_GPIO57              IRQ_GPIO_2_80(57)
#define IRQ_GPIO58              IRQ_GPIO_2_80(58)
#define IRQ_GPIO59              IRQ_GPIO_2_80(59)
#define IRQ_GPIO60              IRQ_GPIO_2_80(60)
#define IRQ_GPIO61              IRQ_GPIO_2_80(61)
#define IRQ_GPIO62              IRQ_GPIO_2_80(62)
#define IRQ_GPIO63              IRQ_GPIO_2_80(63)
#define IRQ_GPIO64              IRQ_GPIO_2_80(64)
#define IRQ_GPIO65              IRQ_GPIO_2_80(65)
#define IRQ_GPIO66              IRQ_GPIO_2_80(66)
#define IRQ_GPIO67              IRQ_GPIO_2_80(67)
#define IRQ_GPIO68              IRQ_GPIO_2_80(68)
#define IRQ_GPIO69              IRQ_GPIO_2_80(69)
#define IRQ_GPIO70              IRQ_GPIO_2_80(70)
#define IRQ_GPIO71              IRQ_GPIO_2_80(71)
#define IRQ_GPIO72              IRQ_GPIO_2_80(72)
#define IRQ_GPIO73              IRQ_GPIO_2_80(73)
#define IRQ_GPIO74              IRQ_GPIO_2_80(74)
#define IRQ_GPIO75              IRQ_GPIO_2_80(75)
#define IRQ_GPIO76              IRQ_GPIO_2_80(76)
#define IRQ_GPIO77              IRQ_GPIO_2_80(77)
#define IRQ_GPIO78              IRQ_GPIO_2_80(78)
#define IRQ_GPIO79              IRQ_GPIO_2_80(79)
#define IRQ_GPIO80              IRQ_GPIO_2_80(80)

#define IRQ_COTULLA_TIMER       IRQ_OST0        /* Timer Interrupt */

#define COTULLA_GPIO_TO_IRQ(i)	(((i) < 2 ) ? COTULLA_IRQ(i) : (((i) < 30) ? IRQ_GPIO_NUM(0, i) : (((i) < 62) ? IRQ_GPIO_NUM(1, i) : IRQ_GPIO_NUM(2, i))))

/* To get the GPIO number from an IRQ number */
#define GPIO_2_31_IRQ(i)	(2 + (i) - 32)
#define GPIO_32_63_IRQ(i)	((i) - 62)
#define GPIO_64_80_IRQ(i)	((i) - 94)
#define GPIO_NUM_IRQ(n, i)	((n) == 0 ? GPIO_2_31_IRQ(i) : ((n) == 1 ? GPIO_32_63_IRQ(i) : GPIO_64_80_IRQ(i)))
#define COTULLA_IRQ_TO_GPIO(i) 	(((i) < 11 ) ? (i) : (((i) < 30) ? GPIO_NUM_IRQ(0, i) : (((i) < 62) ? GPIO_NUM_IRQ(1, i) : GPIO_NUM_IRQ(2, i))))

//steve TBD
#ifdef CONFIG_SABINAL_DISCOVERY

// Richard 0418
#define IRQ_DISCOVERY_CF_RDYBSY 	IRQ_GPIO16
#define IRQ_DISCOVERY_CF_IRQ 		IRQ_DISCOVERY_CF_RDYBSY
#define IRQ_DISCOVERY_RDY		IRQ_GPIO18
#define IRQ_DISCOVERY_SPIRXD		IRQ_GPIO26
#define IRQ_DISCOVERY_I2S_DATAIN	IRQ_GPIO29
#define IRQ_DISCOVERY_COM_RXD		IRQ_GPIO42
#define IRQ_DISCOVERY_COM_CTS		IRQ_GPIO44
#define IRQ_DISCOVERY_IR_RXD		IRQ_GPIO46
#define IRQ_DISCOVERY_SD_WP_HOST	IRQ_GPIO48
#define IRQ_DISCOVERY_USB_GPIO		IRQ_GPIO50
#define IRQ_DISCOVERY_PEN_IRQ		IRQ_GPIO51
#define IRQ_DISCOVERY_ACTION		IRQ_GPIO52
#define IRQ_DISCOVERY_SD_INT		IRQ_GPIO54
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

#define IRQ_ASIC3_A		IRQ_GPIO80

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

#define	NR_IRQS			(IRQ_ASIC3_D15 + 1)
#define IRQ_ASIC1_GPIO_NUM(n)	(IRQ_ASIC3_D15 + n)
#define ASIC1_GPIO_NUM_IRQ(i)	(i - IRQ_ASIC3_D15)

#define GPIO_ASIC1 (0)

#else

#define	NR_IRQS		(IRQ_GPIO80 + 1)

#endif

/*
 * XS80200 specific IRQs
 */
#define IRQ_XS80200_BCU		0	/* Bus Control Unit */
#define IRQ_XS80200_PMU		1	/* Performance Monitoring Unit */
#define IRQ_XS80200_EXTIRQ	2	/* external IRQ signal */
#define IRQ_XS80200_EXTFIQ	3	/* external IRQ signal */

#endif	// __IRQS_H_
