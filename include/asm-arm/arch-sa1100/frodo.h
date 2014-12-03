#ifndef _INCLUDE_FRODO_H_
#define _INCLUDE_FRODO_H_

/*
 * linux/include/asm-arm/arch-sa1100/frodo.h
 *
 * Author: Abraham van der Merwe <abraham@2d3d.co.za>
 *
 * This file contains the hardware specific definitions for 2d3D, Inc.
 * SA-1110 Development Board.
 *
 * Only include this file from SA1100-specific files.
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * History:
 *
 *   2002/05/20   Added I2C definitions
 *                Updated USB port definitions
 *                Removed scratchpad register
 *                Added definitions for second UART
 *
 *   2002/04/19   Added USB definitions
 *
 *   2002/04/17   Added flow control definitions for UART1
 *
 *   2002/03/14   Added ethernet reset definitions
 *
 *   2002/02/28   Ethernet (cs89x0) support
 *
 *   2002/02/27   IDE support
 *
 *   2002/02/22   Added some CPLD registers to control backlight and
 *                general purpose LEDs
 *
 *   2002/01/31   Initial version
 */

/* CPLD registers */
#define FRODO_CPLD_PCMCIA_COMMAND	*((u16 *) 0xf0000000)
#define FRODO_CPLD_PCMCIA_STATUS	*((u16 *) 0xf0004000)
#define FRODO_CPLD_IDE				*((u16 *) 0xf0008000)
#define FRODO_CPLD_UART1			*((u16 *) 0xf000c000)
#define FRODO_CPLD_USB				*((u16 *) 0xf0010000)
#define FRODO_CPLD_ETHERNET			*((u16 *) 0xf0014000)
#define FRODO_CPLD_UART2			*((u16 *) 0xf0018000)
#define FRODO_CPLD_GENERAL			*((u16 *) 0xf0004004)
#define FRODO_CPLD_I2C				*((u16 *) 0xf0008004)

/* general command/status register */
#define FRODO_LCD_BACKLIGHT			0x0400		/* R/W */
#define FRODO_LED1					0x0100		/* R/W */
#define FRODO_LED2					0x0200		/* R/W */
#define FRODO_PUSHBUTTON			0x8000		/* R/O */

/* ethernet register */
#define FRODO_ETH_RESET				0x8000		/* R/W */

/* IDE related definitions */
#define FRODO_IDE_GPIO				GPIO_GPIO23
#define FRODO_IDE_IRQ				IRQ_GPIO23
#define FRODO_IDE_DATA				0xf0020000
#define FRODO_IDE_CTRL				0xf0038004

/* Ethernet related definitions */
#define FRODO_ETH_GPIO				GPIO_GPIO20
#define FRODO_ETH_IRQ				IRQ_GPIO20
#define FRODO_ETH_MEMORY			0xf0060000
#define FRODO_ETH_IO				0xf0070000

/* USB device controller */
#define FRODO_USB_DC_CTRL			0xf0040006
#define FRODO_USB_DC_DATA			0xf0040004
#define FRODO_USB_DC_GPIO			GPIO_GPIO19
#define FRODO_USB_DC_IRQ			IRQ_GPIO19

/* USB host controller */
#define FRODO_USB_HC_CTRL			0xf0040002
#define FRODO_USB_HC_DATA			0xf0040000
#define FRODO_USB_HC_GPIO			GPIO_GPIO18
#define FRODO_USB_HC_IRQ			IRQ_GPIO18

/* This UART supports all the funky things */
#define FRODO_UART1_RI				0x0100		/* R/O */
#define FRODO_UART1_DCD				0x0200		/* R/O */
#define FRODO_UART1_CTS				0x0400		/* R/O */
#define FRODO_UART1_DSR				0x0800		/* R/O */
#define FRODO_UART1_DTR				0x2000		/* R/W */
#define FRODO_UART1_RTS				0x4000		/* R/W */

/* Console port. Only supports a subset of the control lines */
#define FRODO_UART2_CTS				0x1000		/* R/O */
#define FRODO_UART2_RTS				0x8000		/* R/W */

/* USB command register */
#define FRODO_USB_HWAKEUP			0x2000		/* R/W */
#define FRODO_USB_DWAKEUP			0x4000		/* R/W */
#define FRODO_USB_NDPSEL			0x8000		/* R/W */

/* I2C adapter information */
#define FRODO_I2C_SCL_OUT			0x2000		/* R/W */
#define FRODO_I2C_SCL_IN			0x1000		/* R/O */
#define FRODO_I2C_SDA_OUT			0x8000		/* R/W */
#define FRODO_I2C_SDA_IN			0x4000		/* R/O */

#endif	/* _INCLUDE_FRODO_H_ */
