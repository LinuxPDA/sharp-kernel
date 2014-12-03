/*
 * linux/arm/arch-cotulla/discovery_gpio.h :This file contains Discovery-specific tweaks.
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
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 * 
 * ChangeLog:
 *  04-04-2002 Richard Rau
 */
#ifndef _INCLUDE_DISCOVERY_GPIO_H_ 
#define _INCLUDE_DISCOVERY_GPIO_H_


/*
 * Internel GPIO
 */
#ifndef GPIO_GPIO
#define GPIO_GPIO(Nb)   		(1 << (Nb % 32))
#endif

#define GPIO_DISCOVERY_GPIO_INT		GPIO_GPIO(0)
#define GPIO_DISCOVERY_GP_RST		GPIO_GPIO(1)

#define GPIO_DISCOVERY_CS1		GPIO_GPIO(15)
#define GPIO_DISCOVERY_CF_RDYBSY	GPIO_GPIO(16)
#define GPIO_DISCOVERY_CF_IRQ		GPIO_DISCOVERY_CF_RDYBSY
#define GPIO_DISCOVERY_PWM_BRI		GPIO_GPIO(17)
#define GPIO_DISCOVERY_RDY		GPIO_GPIO(18)

#define GPIO_DISCOVERY_SPI_CLK		GPIO_GPIO(23)
#define GPIO_DISCOVERY_SPI_CS		GPIO_GPIO(24)
#define GPIO_DISCOVERY_SPI_TXD		GPIO_GPIO(25)
#define GPIO_DISCOVERY_SPI_RXD		GPIO_GPIO(26)
//#define GPIO_DISCOVERY_CF_VDD_ON	GPIO_GPIO(27)
#define GPIO_DISCOVERY_CF_RESET		GPIO_GPIO(27)
#define GPIO_DISCOVERY_I2S_BIT_CLK	GPIO_GPIO(28)
#define GPIO_DISCOVERY_I2S_DATA_IN	GPIO_GPIO(29)
#define GPIO_DISCOVERY_I2S_DATA_OUT	GPIO_GPIO(30)
#define GPIO_DISCOVERY_I2S_SYNC		GPIO_GPIO(31)
#define GPIO_DISCOVERY_I2S_SYS_CLK	GPIO_GPIO(32)
#define GPIO_DISCOVERY_CS5		GPIO_GPIO(33)
#define GPIO_DISCOVERY_CONN60_1		GPIO_GPIO(34)

#define GPIO_DISCOVERY_CONN60_2		GPIO_GPIO(39)

#define GPIO_DISCOVERY_COM_RXD		GPIO_GPIO(42)
#define GPIO_DISCOVERY_COM_TXD		GPIO_GPIO(43)
#define GPIO_DISCOVERY_COM_CTS		GPIO_GPIO(44)
#define GPIO_DISCOVERY_COM_RTS		GPIO_GPIO(45)
#define GPIO_DISCOVERY_IR_RXD		GPIO_GPIO(46)
#define GPIO_DISCOVERY_IR_TXD		GPIO_GPIO(47)
#define GPIO_DISCOVERY_SD_WP_HOST	GPIO_GPIO(48)
#define GPIO_DISCOVERY_PWE		GPIO_GPIO(49)
#define GPIO_DISCOVERY_USB_GPIO		GPIO_GPIO(50)
#define GPIO_DISCOVERY_PEN_IRQ		GPIO_GPIO(51)
#define GPIO_DISCOVERY_ACTION		GPIO_GPIO(52)
#define GPIO_DISCOVERY_SD_CLK_O		GPIO_GPIO(53)
#define GPIO_DISCOVERY_SD_INT		GPIO_GPIO(54)
#define GPIO_DISCOVERY_CONN60_5		GPIO_GPIO(55)
#define GPIO_DISCOVERY_CHG_EN		GPIO_GPIO(56)
#define GPIO_DISCOVERY_BATT_SAM		GPIO_GPIO(57)
#define GPIO_DISCOVERY_LED_D0		GPIO_GPIO(58)
#define GPIO_DISCOVERY_LED_D1		GPIO_GPIO(59)
#define GPIO_DISCOVERY_LED_D2		GPIO_GPIO(60)
#define GPIO_DISCOVERY_LED_D3		GPIO_GPIO(61)
#define GPIO_DISCOVERY_LED_D4		GPIO_GPIO(62)
#define GPIO_DISCOVERY_LED_D5		GPIO_GPIO(63)
#define GPIO_DISCOVERY_LED_D6		GPIO_GPIO(64)
#define GPIO_DISCOVERY_LED_D7		GPIO_GPIO(65)
#define GPIO_DISCOVERY_LED_D8		GPIO_GPIO(66)
#define GPIO_DISCOVERY_LED_D9		GPIO_GPIO(67)
#define GPIO_DISCOVERY_LED_D10		GPIO_GPIO(68)
#define GPIO_DISCOVERY_LED_D11		GPIO_GPIO(69)
#define GPIO_DISCOVERY_LED_D12		GPIO_GPIO(70)
#define GPIO_DISCOVERY_LED_D13		GPIO_GPIO(71)
#define GPIO_DISCOVERY_LED_D14		GPIO_GPIO(72)
#define GPIO_DISCOVERY_LED_D15		GPIO_GPIO(73)
#define GPIO_DISCOVERY_L_FCLK		GPIO_GPIO(74)
#define GPIO_DISCOVERY_L_LCLK		GPIO_GPIO(75)
#define GPIO_DISCOVERY_L_PCLK		GPIO_GPIO(76)
#define GPIO_DISCOVERY_L_BIAS		GPIO_GPIO(77)
#define GPIO_DISCOVERY_CS2		GPIO_GPIO(78)
#define GPIO_DISCOVERY_CS3		GPIO_GPIO(79)
#define GPIO_DISCOVERY_CS4		GPIO_GPIO(80)


/*
 * Externel GPIO
 */

#define EGPIO1_DISCOVERY_XDOFF_ON	(1<<0)
#define EGPIO1_DISCOVERY_LCD_AVDD	(1<<1)
#define EGPIO1_DISCOVERY_LCD_VC_ON	(1<<2)
#define EGPIO1_DISCOVERY_VDD_5VEN	(1<<3)
#define EGPIO1_DISCOVERY_VGL_ON		(1<<4)
#define EGPIO1_DISCOVERY_REV1		(1<<5)
#define EGPIO1_DISCOVERY_FL_PWR_ON	(1<<6)
#define EGPIO1_DISCOVERY_VPEN		(1<<7)
#define EGPIO1_DISCOVERY_AUD_PWR_ON	(1<<8)
#define EGPIO1_DISCOVERY_HP_MIC		(1<<9)
#define EGPIO1_DISCOVERY_KEY_SERCLK	(1<<10)
#define EGPIO1_DISCOVERY_KEY_SERSL	(1<<11)
#define EGPIO1_DISCOVERY_SPK_ON		(1<<12)
#define EGPIO1_DISCOVERY_BUFFER_OE	(1<<13)
#define EGPIO1_DISCOVERY_SD_PWR_ON	(1<<14)
#define EGPIO1_DISCOVERY_CF_RESET	(1<<15)

#define EGPIO2_DISCOVERY_SIR_OFF	(1<<0)
#define EGPIO2_DISCOVERY_COM_ON		(1<<1)
#define EGPIO2_DISCOVERY_A_IN_SEL0	(1<<2)
#define EGPIO2_DISCOVERY_A_IN_SEL1	(1<<3)
#define EGPIO2_DISCOVERY_NOTIFY_LED	(1<<4)
#define EGPIO2_DISCOVERY_CH_LED		(1<<5)
#define EGPIO2_DISCOVERY_CODEC_RST	(1<<6)
#define EGPIO2_DISCOVERY_MUX_EN		(1<<7)

#define DISCOVERY_EGPIO1		*((volatile u16 *)0xfa000000)
#define DISCOVERY_EGPIO2		*((volatile u8  *)0xf8000000)
#define DISCOVERY_BPEGPIO		*((volatile u8  *)0xf7000000)

enum discovery_egpio_type {
	DIS_EGPIO2_NOTIFY_LED,
	DSI_EGPIO2_CH_LED
};

#endif /* _INCLUDE_DISCOVERY_GPIO_H_ */
