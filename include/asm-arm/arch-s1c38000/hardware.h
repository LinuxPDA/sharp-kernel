/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/hardware.h
 *
 */

#ifndef __ASM_ARM_ARCH_S1C38000_HARDWARE_H
#define __ASM_ARM_ARCH_S1C38000_HARDWARE_H 1

#include <linux/config.h>
#include <asm/mach-types.h>

#define IO_START		0xf8000000      /* I/O */
#define IO_SIZE			0x00100000
#define IO_BASE			0xf8000000

#define IO_START_2		0x07000000      /* I/O */
#define IO_SIZE_2		0x01000000
#define IO_BASE_2		0xfb000000

#define	FLASH_START		0x00000000	/* LCD */
#define	FLASH_SIZE		0x00800000
#define	FLASH_BASE		0xfc000000

#define	LCD_IO_START		0xf0000000	/* LCD */
#define	LCD_IO_SIZE		0x08000000
#define	LCD_IO_BASE		0xf0000000

#define PCIO_BASE 0

/*
 * GPIO registers
 */
#define	GPIO_BASE	(IO_BASE + 0x480)

#define GPIOA_DIRECTION	(*(volatile unsigned long *) (GPIO_BASE + 0x000))
#define GPIOA_DATA	(*(volatile unsigned long *) (GPIO_BASE + 0x004))
#define GPIOA_LEVEL	(*(volatile unsigned long *) (GPIO_BASE + 0x008))
#define GPIOA_TYPE	(*(volatile unsigned long *) (GPIO_BASE + 0x00c))
#define GPIOA_MASK	(*(volatile unsigned long *) (GPIO_BASE + 0x010))
#define GPIOA_INT	(*(volatile unsigned long *) (GPIO_BASE + 0x014))
#define GPIOB_DIRECTION	(*(volatile unsigned long *) (GPIO_BASE + 0x018))
#define GPIOB_DATA	(*(volatile unsigned long *) (GPIO_BASE + 0x01c))
#define GPIOB_LEVEL	(*(volatile unsigned long *) (GPIO_BASE + 0x020))
#define GPIOB_TYPE	(*(volatile unsigned long *) (GPIO_BASE + 0x024))
#define GPIOB_MASK	(*(volatile unsigned long *) (GPIO_BASE + 0x028))
#define GPIOB_INT	(*(volatile unsigned long *) (GPIO_BASE + 0x02c))
#define GPIO_INTREQ	(*(volatile unsigned long *) (GPIO_BASE + 0x030))

#endif
