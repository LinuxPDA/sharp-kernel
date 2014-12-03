/*
 * linux/arm/arch-cotulla/asic3.h : This file contains Discovery-specific tweaks.
 *
 * Copyright (C) 2002 Richard Rau (richard.rau@msa.hinet.net)
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
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * ChangeLog:
 *  04-15-2002 Steve Lin, modified for physical address
 *  04-22-2002 Edward Chen translate into ASIC3
 */
/*------------------------------------------------------------------------------*/

/* ASIC-3 defines */
#ifndef __ASIC3_H__
#define __ASIC3_H__

#define	ASIC3_GPIO_BASE	0xf8000000

#define ASIC3_OFFSET_GROUP_A(s,x)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + A_GROUP_BASE +(ASIC3_GPIO_ ## x))))

#define ASIC3_OFFSET_GROUP_B(s,x)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + B_GROUP_BASE +(ASIC3_GPIO_ ## x))))

#define ASIC3_OFFSET_GROUP_C(s,x)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + C_GROUP_BASE +(ASIC3_GPIO_ ## x))))

#define ASIC3_OFFSET_GROUP_D(s,x)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + D_GROUP_BASE +(ASIC3_GPIO_ ## x))))

#define ASIC3_OFFSET_SLEEP_PARAM(s,x)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + SLEEP_PARAM +(ASIC3_ ## x))))

#define ASIC3_OFFSET_PWM_PARAM(s)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + SLEEP_PARAM_PWM )))

#define ASIC3_OFFSET_ALARM_PARAM0(s)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + SLEEP_ALARM_PWM0 )))

#define ASIC3_OFFSET_ALARM_PARAM1(s)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + SLEEP_ALARM_PWM1 )))

#define ASIC3_OFFSET_ALARM_PARAM2(s)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + SLEEP_ALARM_PWM2 )))

#define ASIC3_OFFSET_CH_LED(s,x)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + CH_LED_BASE +(ASIC3_ ## x))))

#define ASIC3_OFFSET_NOTIFY_LED(s,x)   \
     (*((volatile s *) ( ASIC3_GPIO_BASE + NOTIFY_LED_BASE +(ASIC3_ ## x))))

#define ASIC3_OFFSET_CLK(s,x) \
     (*((volatile s *) ( ASIC3_GPIO_BASE + CLOCK_BASE +(_ASIC3_CLOCK_ ## x))))
     
#define ASIC3_OFFSET_INTR(s, x) \
     (*((volatile s *) ( ASIC3_GPIO_BASE + INTR_BASE +(_ASIC3_INTR_ ## x))))

/* for All Group Base address */
#define A_GROUP_BASE			0x00
#define B_GROUP_BASE			0x80
#define C_GROUP_BASE			0x100
#define D_GROUP_BASE			0x180
#define SLEEP_PARAM			0x480
#define SLEEP_PARAM_PWM			0x304
#define SLEEP_ALARM_PWM0		0x282
#define SLEEP_ALARM_PWM1		0x302
#define SLEEP_ALARM_PWM2		0x284

#ifdef CONFIG_SABINAL_DISCOVERY_PreMV
#define CH_LED_BASE			0x380
#define NOTIFY_LED_BASE			0x400
#else
#define CH_LED_BASE			0x400	//0x380
#define NOTIFY_LED_BASE			0x380	//0x400
#endif

#define CLOCK_BASE			0x500
#define INTR_BASE			0x580


/* ALL GROUP Register Definitions */
#define ASIC3_GPIO_GPINTMASK		0x00	/* R/W 0:enable interrupt, 1:mask interrupt */
#define ASIC3_GPIO_GPIODIR		0x02	/* R/W 0:input mode, 1:output mode          */
#define ASIC3_GPIO_GPIOPIOD		0x04	/* Set Output Data 0:clear bit, 1:set bit   */
#define ASIC3_GPIO_GPINTYP		0x06	/* R/W 0:level trigger, 1:edge trigger      */
#define ASIC3_GPIO_GPINETSEL		0x08	/* R/W 0:falling edge, 1:rising edge        */
#define ASIC3_GPIO_GPINTLSEL		0x0a	/* R/W 0:low level, 1:high level detect     */
#define ASIC3_GPIO_GPINTSLP		0x0c	/* R/W 0:enable , 1:disable trigger in sleep mode    */
#define ASIC3_GPIO_GPOSLPOUT		0x0e	/* R/W level 0:low, 1:high in sleep mode             */
#define ASIC3_GPIO_GPOBFOUT		0x10	/* R/W level 0:low, 1:high in batt_fault             */
#define ASIC3_GPIO_GPINTSTAT		0x12	/* R/W 0:no int come/clear bit, 1:interrupt occurred */
#define ASIC3_GPIO_GPIOALT		0x14	/* R/W 0:GPIO function, 1:alternate function*/
#define ASIC3_GPIO_GPIOCONF		0x16	/* Configuration setting bits               */
#define ASIC3_GPIO_GPIOPSTS		0x18	/* Read Data */

/* CH_LED/NOTIFY_LED Register Definitions */
#define	ASIC3_LEDCNTL			0x00	/* LED Control register                      */
#define	ASIC3_LEDPTS			0x02	/* LED Period Time set register, bit11~0     */
#define	ASIC3_LEDDTS			0x04	/* LED Duty time set register, bit12~0       */
#define	ASIC3_LEDASTC			0x06	/* LED Auto Stop Count register, bit 15~0    */
#define	ASIC3_LEDINT			0x08	/* LED Auto Stop Interrupt request           */

#define	ASIC3_PARAM0			0x00	/* Sleep param register 0                     */
#define	ASIC3_PARAM1			0x02	/* Sleep param register 1                     */
#define	ASIC3_PARAM2			0x04	/* Sleep param register 2                     */
#define	ASIC3_PARAM3			0x06	/* Sleep param register 3                     */
#define	ASIC3_PARAM4			0x08	/* Sleep param register 4                     */

#define _ASIC3_CLOCK_Cdexcdcx		0x00
#define _ASIC3_CLOCK_Clksel		0x02

#define _ASIC3_INTR_Intmask		0x00
#define _ASIC3_INTR_Pintstat		0x02
#define _ASIC3_INTR_Intcps		0x04
#define _ASIC3_INTR_Inttbs		0x06


/* Get the GPIO Register Value */
	/* interrupt mask */
#define ASIC3_GPIO_MASK_A		ASIC3_OFFSET_GROUP_A( u16, GPINTMASK )
#define ASIC3_GPIO_MASK_B		ASIC3_OFFSET_GROUP_B( u16, GPINTMASK )
#define ASIC3_GPIO_MASK_C		ASIC3_OFFSET_GROUP_C( u16, GPINTMASK )
#define ASIC3_GPIO_MASK_D		ASIC3_OFFSET_GROUP_D( u16, GPINTMASK )
	/* gpio direction */
#define ASIC3_GPIO_DIR_A		ASIC3_OFFSET_GROUP_A( u16, GPIODIR )
#define ASIC3_GPIO_DIR_B		ASIC3_OFFSET_GROUP_B( u16, GPIODIR )
#define ASIC3_GPIO_DIR_C		ASIC3_OFFSET_GROUP_C( u16, GPIODIR )
#define ASIC3_GPIO_DIR_D		ASIC3_OFFSET_GROUP_D( u16, GPIODIR )
	/* set gpio bit value */
#define ASIC3_GPIO_PIOD_A		ASIC3_OFFSET_GROUP_A( u16, GPIOPIOD )
#define ASIC3_GPIO_PIOD_B		ASIC3_OFFSET_GROUP_B( u16, GPIOPIOD )
#define ASIC3_GPIO_PIOD_C		ASIC3_OFFSET_GROUP_C( u16, GPIOPIOD )
#define ASIC3_GPIO_PIOD_D		ASIC3_OFFSET_GROUP_D( u16, GPIOPIOD )
	/* trigger type selection */
#define ASIC3_GPIO_INTYP_A		ASIC3_OFFSET_GROUP_A( u16, GPINTYP )
#define ASIC3_GPIO_INTYP_B		ASIC3_OFFSET_GROUP_B( u16, GPINTYP )
#define ASIC3_GPIO_INTYP_C		ASIC3_OFFSET_GROUP_C( u16, GPINTYP )
#define ASIC3_GPIO_INTYP_D		ASIC3_OFFSET_GROUP_D( u16, GPINTYP )
	/* edge trigger select for rising/falling */
#define ASIC3_GPIO_ETSEL_A		ASIC3_OFFSET_GROUP_A( u16, GPINETSEL )
#define ASIC3_GPIO_ETSEL_B		ASIC3_OFFSET_GROUP_B( u16, GPINETSEL )
#define ASIC3_GPIO_ETSEL_C		ASIC3_OFFSET_GROUP_C( u16, GPINETSEL )
#define ASIC3_GPIO_ETSEL_D		ASIC3_OFFSET_GROUP_D( u16, GPINETSEL )
	/* level trigger for low/hign */
#define ASIC3_GPIO_LSEL_A		ASIC3_OFFSET_GROUP_A( u16, GPINTLSEL )
#define ASIC3_GPIO_LSEL_B		ASIC3_OFFSET_GROUP_B( u16, GPINTLSEL )
#define ASIC3_GPIO_LSEL_C		ASIC3_OFFSET_GROUP_C( u16, GPINTLSEL )
#define ASIC3_GPIO_LSEL_D		ASIC3_OFFSET_GROUP_D( u16, GPINTLSEL )
	/* interrupt enable/disable in sleep mode */
#define ASIC3_GPIO_INTSLP_A		ASIC3_OFFSET_GROUP_A( u16, GPINTSLP )
#define ASIC3_GPIO_INTSLP_B		ASIC3_OFFSET_GROUP_B( u16, GPINTSLP )
#define ASIC3_GPIO_INTSLP_C		ASIC3_OFFSET_GROUP_C( u16, GPINTSLP )
#define ASIC3_GPIO_INTSLP_D		ASIC3_OFFSET_GROUP_D( u16, GPINTSLP )
	/* gpio output in sleep */
#define ASIC3_GPIO_SLPOUT_A		ASIC3_OFFSET_GROUP_A( u16, GPOSLPOUT )
#define ASIC3_GPIO_SLPOUT_B		ASIC3_OFFSET_GROUP_B( u16, GPOSLPOUT )
#define ASIC3_GPIO_SLPOUT_C		ASIC3_OFFSET_GROUP_C( u16, GPOSLPOUT )
#define ASIC3_GPIO_SLPOUT_D		ASIC3_OFFSET_GROUP_D( u16, GPOSLPOUT )
	/* gpio output in battery fault */
#define ASIC3_GPIO_BFOUT_A		ASIC3_OFFSET_GROUP_A( u16, GPOBFOUT )
#define ASIC3_GPIO_BFOUT_B		ASIC3_OFFSET_GROUP_B( u16, GPOBFOUT )
#define ASIC3_GPIO_BFOUT_C		ASIC3_OFFSET_GROUP_C( u16, GPOBFOUT )
#define ASIC3_GPIO_BFOUT_D		ASIC3_OFFSET_GROUP_D( u16, GPOBFOUT )
	/* interrupt pending status/clear */
#define ASIC3_GPIO_INTSTAT_A		ASIC3_OFFSET_GROUP_A( u16, GPINTSTAT )
#define ASIC3_GPIO_INTSTAT_B		ASIC3_OFFSET_GROUP_B( u16, GPINTSTAT )
#define ASIC3_GPIO_INTSTAT_C		ASIC3_OFFSET_GROUP_C( u16, GPINTSTAT )
#define ASIC3_GPIO_INTSTAT_D		ASIC3_OFFSET_GROUP_D( u16, GPINTSTAT )
	/* alternate functoin set */
#define ASIC3_GPIO_ALT_A		ASIC3_OFFSET_GROUP_A( u16, GPIOALT )
#define ASIC3_GPIO_ALT_B		ASIC3_OFFSET_GROUP_B( u16, GPIOALT )
#define ASIC3_GPIO_ALT_C		ASIC3_OFFSET_GROUP_C( u16, GPIOALT )
#define ASIC3_GPIO_ALT_D		ASIC3_OFFSET_GROUP_D( u16, GPIOALT )
	/* gpio configuration setting */
#define ASIC3_GPIO_CONF_A		ASIC3_OFFSET_GROUP_A( u16, GPIOCONF )
#define ASIC3_GPIO_CONF_B		ASIC3_OFFSET_GROUP_B( u16, GPIOCONF )
#define ASIC3_GPIO_CONF_C		ASIC3_OFFSET_GROUP_C( u16, GPIOCONF )
#define ASIC3_GPIO_CONF_D		ASIC3_OFFSET_GROUP_D( u16, GPIOCONF )
	/* gpio read data */
#define ASIC3_GPIO_PSTS_A		ASIC3_OFFSET_GROUP_A( u16, GPIOPSTS )
#define ASIC3_GPIO_PSTS_B		ASIC3_OFFSET_GROUP_B( u16, GPIOPSTS )
#define ASIC3_GPIO_PSTS_C		ASIC3_OFFSET_GROUP_C( u16, GPIOPSTS )
#define ASIC3_GPIO_PSTS_D		ASIC3_OFFSET_GROUP_D( u16, GPIOPSTS )

/* Get CH_LED REGS  */
#define	ASIC3_LEDCNTL_CH_LED		ASIC3_OFFSET_CH_LED( u16, LEDCNTL )
#define	ASIC3_LEDPTS_CH_LED		ASIC3_OFFSET_CH_LED( u16, LEDPTS )
#define	ASIC3_LEDDTS_CH_LED		ASIC3_OFFSET_CH_LED( u16, LEDDTS )
#define	ASIC3_LEDASTC_CH_LED		ASIC3_OFFSET_CH_LED( u16, LEDASTC )
#define	ASIC3_LEDINT_CH_LED		ASIC3_OFFSET_CH_LED( u16, LEDINT )

/* Get NOTIFY_LED REGS  */
#define	ASIC3_LEDCNTL_NOTIFY_LED	ASIC3_OFFSET_NOTIFY_LED( u16, LEDCNTL )
#define	ASIC3_LEDPTS_NOTIFY_LED		ASIC3_OFFSET_NOTIFY_LED( u16, LEDPTS )
#define	ASIC3_LEDDTS_NOTIFY_LED		ASIC3_OFFSET_NOTIFY_LED( u16, LEDDTS )
#define	ASIC3_LEDASTC_NOTIFY_LED	ASIC3_OFFSET_NOTIFY_LED( u16, LEDASTC )
#define	ASIC3_LEDINT_NOTIFY_LED		ASIC3_OFFSET_NOTIFY_LED( u16, LEDINT )

/* Get SLEEP PARAM REGS  */
#define	ASIC3_SLEEP_PARAM0	ASIC3_OFFSET_SLEEP_PARAM( u16, PARAM0 )
#define	ASIC3_SLEEP_PARAM1	ASIC3_OFFSET_SLEEP_PARAM( u16, PARAM1 )
#define	ASIC3_SLEEP_PARAM2	ASIC3_OFFSET_SLEEP_PARAM( u16, PARAM2 )
#define	ASIC3_SLEEP_PARAM3	ASIC3_OFFSET_SLEEP_PARAM( u16, PARAM3 )
#define	ASIC3_SLEEP_PARAM4	ASIC3_OFFSET_SLEEP_PARAM( u16, PARAM4 )
#define	ASIC3_SLEEP_PARAM5	ASIC3_OFFSET_PWM_PARAM( u16 )
#define	ASIC3_SLEEP_PARAM6	ASIC3_OFFSET_ALARM_PARAM0( u16 )
#define	ASIC3_SLEEP_PARAM7	ASIC3_OFFSET_ALARM_PARAM1( u16 )
#define	ASIC3_SLEEP_PARAM8	ASIC3_OFFSET_ALARM_PARAM2( u16 )

/* Clock Pre-Scale Unit */
#define ASIC3_CLOCK_CDEXCDCX		ASIC3_OFFSET_CLK( u16, Cdexcdcx)
#define ASIC3_CLOCK_CLK_SEL		ASIC3_OFFSET_CLK( u16, Clksel)

/* Interrupt Control Unit */
#define ASIC3_INTR_INTMASK		ASIC3_OFFSET_INTR(u16, Intmask)
#define ASIC3_INTR_PINTSTAT		ASIC3_OFFSET_INTR(u16, Pintstat)
#define ASIC3_INTR_INTCPS		ASIC3_OFFSET_INTR(u16, Intcps)
#define ASIC3_INTR_INTTBS		ASIC3_OFFSET_INTR(u16, Inttbs)

/*------------------------------------------------------------------------------*/
/* GPIO NORMAL PIN SET */
#define	GPIO_P0				(1<<0)
#define	GPIO_P1				(1<<1)
#define	GPIO_P2				(1<<2)
#define	GPIO_P3				(1<<3)
#define	GPIO_P4				(1<<4)
#define	GPIO_P5				(1<<5)
#define	GPIO_P6				(1<<6)
#define	GPIO_P7				(1<<7)
#define	GPIO_P8				(1<<8)
#define	GPIO_P9				(1<<9)
#define	GPIO_P10			(1<<10)
#define	GPIO_P11			(1<<11)
#define	GPIO_P12			(1<<12)
#define	GPIO_P13			(1<<13)
#define	GPIO_P14			(1<<14)
#define	GPIO_P15			(1<<15)

/* GPIO value setting */
#define	GPIO_V0				(1<<0)	/* 0000000000000001 */
#define	GPIO_V1				(1<<1)	/* 0000000000000010 */
#define	GPIO_V2				(1<<2)	/* 0000000000000100 */
#define	GPIO_V3				(1<<3)	/* 0000000000001000 */
#define	GPIO_V4				(1<<4)	/* 0000000000010000 */
#define	GPIO_V5				(1<<5)	/* 0000000000100000 */
#define	GPIO_V6				(1<<6)	/* 0000000001000000 */
#define	GPIO_V7				(1<<7)	/* 0000000010000000 */
#define	GPIO_V8				(1<<8)	/* 0000000100000000 */
#define	GPIO_V9				(1<<9)	/* 0000001000000000 */
#define	GPIO_V10			(1<<10)	/* 0000010000000000 */
#define	GPIO_V11			(1<<11)	/* 0000100000000000 */
#define	GPIO_V12			(1<<12)	/* 0001000000000000 */
#define	GPIO_V13			(1<<13)	/* 0010000000000000 */
#define	GPIO_V14			(1<<14)	/* 0100000000000000 */
#define	GPIO_V15			(1<<15)	/* 1000000000000000 */



/* ASIC3 Group A pin Usage */
#define KEY0				(1<<0)
#define KEY1				(1<<1)
#define KEY2				(1<<2)
#define KEY3				(1<<3)
#define UP_KEY				(1<<4)
#define DOWN_KEY			(1<<5)
#define LEFT_KEY			(1<<6)
#define RIGHT_KEY			(1<<7)
#define ACTION_KEY			(1<<8)
#define PWR_ON_KEY			(1<<9)

/* ASIC3 Group B pin Usage */
#define XDOFF_ON			(1<<0)
#define LCD_AVDD_DVCC_ON		(1<<1)
#define LCD_VCC_ON			(1<<2)
#define VDD_5V_EN			(1<<3)
#define VGL_ON				(1<<4)
#define FL_PWR_ON			(1<<5)
#define VPEN_GPIO			(1<<6)
#define AUD_PWR_ON			(1<<7)
#define HP_MIC				(1<<8)
#define SPK_ON				(1<<9)
#define BUFFER_OE			(1<<10)
#define SD_PWR_ON			(1<<11)
#define SIR_OFF				(1<<12)
#define COM_ON				(1<<13)
#define CODEC_RST			(1<<14)
#define SK3_WP				(1<<15)

/* ASIC3 Group C pin Usage */
#define MUX_EN				(1<<7)
#define A_IN_SEL0			(1<<8)
#define A_IN_SEL1			(1<<9)
#define BACK_PWR_ON			(1<<10)
#define	BACK_I2C_ON			(1<<11)
#define	BACK_RESET			(1<<12)

/* ASIC3 Group D pin Usage */
#define COM_DCD				(1<<0)
#define AC_IN				(1<<1)
#define HEADPHONE_IN			(1<<2)
#define WAKE_UP				(1<<3)
#define SD_DETECT			(1<<4)
#define BACKPACK_DETECT			(1<<5)
#define BACK_INTERRUPT			(1<<6)
#define CF_CHG_EN			(1<<7)
#define CF_DETECT			(1<<8)
#define CF_BATT_FAULT			(1<<9)
#define CF_IOIS				(1<<10)
#define CF_STSCHG			(1<<11)


/* LED bit definition */

/* LEDCNTL : LED Control Reg */
#define LEDTBS				(1<<0)	/* LED time base set */
#define	LEDEN				(1<<4)	/* LED ON/OFF 0:off, 1:on */
#define	LEDSTP				(1<<5)	/* LED ON/OFF auto stop set 0:disable, 1:enable */
#define	LEDMSK				(1<<6)	/* LED Interrupt Mask 0:No mask, 1:mask */

/* LEDINT : LED Auto Stop Interrupt request */
#define ASIRINT				(1<<0)	/* 0:disable, 1:enable */

#define CX2				(1 << 2)
#define EX0				(1 << 13)

#define SDIOHOST			(1 << 0)
#define SDIOBUS				(1 << 1)
#define CX13				(1 << 2)

/* Interrupt Control Pin Def. */
#define INTMASK_GINT			(1 << 0)
#define INTMASK_GINTEL			(1 << 1)
#define INTMASK_MASK0			(1 << 2)
#define INTMASK_MASK1			(1 << 3)
#define INTMASK_MASK2			(1 << 4)
#define INTMASK_MASK3			(1 << 5)
#define INTMASK_MASK4			(1 << 6)
#define INTMASK_MASK5			(1 << 7)

#define INTCPS_SET			(1 << 4)

#endif	//_ASIC3_H__
/*------------------------------------------------------------------------------*/

/* PPSH Decode */
#define ASIC3_PPSH_BASE		0x16000000

#define ASIC3_OFFSET_PPSH(s,x)   \
     (*((volatile s *) ( ASIC3_PPSH_BASE + (ASIC3_ ## x))))

/* for PPSH Offset address */
#define ASIC3_INDEX_REG		0x660
#define	ASIC3_DATA_REG		0x664
#define ASIC3_COM1_PORT		0x7e0
#define ASIC3_COM2_PORT		0x3e0
#define ASIC3_LPT1_PORT		0x6f0
#define ASIC3_LPT2_PORT		0x5e0
#define ASIC3_LPT3_PORT		0x1e0

/* Get the Register/Port data */
#define	ASIC3_PPSH_INDEX		ASIC3_OFFSET_PPSH( u32, INDEX_REG )
#define	ASIC3_PPSH_DATA			ASIC3_OFFSET_PPSH( u32, DATA_REG )
#define	ASIC3_PPSH_COM1			ASIC3_OFFSET_PPSH( u32, COM1_PORT )
#define	ASIC3_PPSH_COM2			ASIC3_OFFSET_PPSH( u32, COM2_PORT )
#define	ASIC3_PPSH_LPT1			ASIC3_OFFSET_PPSH( u32, LPT1_PORT )
#define	ASIC3_PPSH_LPT2			ASIC3_OFFSET_PPSH( u32, LPT2_PORT )
#define	ASIC3_PPSH_LPT3			ASIC3_OFFSET_PPSH( u32, LPT3_PORT )

/*------------------------------------------------------------------------------*/

/* Extended CF Backpack */
#define ASIC3_CF_ATT_CS_BASE		0x10000000
#define ASIC3_CF_MEM_CS_BASE		0x10800000
#define ASIC3_CF_IO_CS_BASE		0x11000000
#define ASIC3_CF_CONFIG_BASE		0x12000000

#define ASIC3_OFFSET_CF_CONFIG(s)   \
     (*((volatile s *) ( ASIC3_CF_CONFIG_BASE )

/* the Configuration Control */
#define ASIC3_CF_CONFIG			ASIC3_OFFSET_CF_CONFIG( u8 )

#define CF_RESET		(1<<0)	/* CF reset */
#define	VPEN			(1<<1)	/* Control write protect function of EEPROM */
#define	CHG_EN			(1<<2)	/* Charge enable signal for Backpack */
#define	CF_3V_ON		(1<<3)	/* Support the power to 3V CF Card   */
#define	CF_5V_ON		(1<<4)	/* Support the power to 5V CF Card   */
#define	SAMPLE			(1<<5)	/* Power Sample signal for Back Part */

/*------------------------------------------------------------------------------*/

/* EEPROM on Backpack */
#define BKPK_EEPROM_BASE		0x01010100

/*------------------------------------------------------------------------------*/

/* Flash Memory -- One Chip */

#define	INTEL_STACKED_16M_BASE		0x00000000

#ifndef FLH_MEM_16MB /* FLH_MEM_24MB */

#define	INTEL_NEXT_8M_BASE		0x04000000

#endif

/*------------------------------------------------------------------------------*/

/* SDRAM Memory -- Two Chip */

#define SDRAM_01_BASE			0x0a000000 /* 32MB */
#define SDRAM_02_BASE			0x0a400000 /* 32MB */

/*------------------------------------------------------------------------------*/

