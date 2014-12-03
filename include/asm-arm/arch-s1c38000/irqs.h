/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/irqs.h
 *
 */

#ifndef __ASM_ARM_ARCH_S1C38000_IRQS_H
#define __ASM_ARM_ARCH_S1C38000_IRQS_H 1

#include <linux/config.h>

#define NR_IRQS          48

#define IRQ_STWDOG        0   /* Watchdog timer */
#define IRQ_PROG          1   /* Programmable interrupt */
#define IRQ_DEBUG_RX      2   /* Comm Rx debug */
#define IRQ_DEBUG_TX      3   /* Comm Tx debug */
#define IRQ_TIMER0        4   /* Timer 0 */
#define IRQ_TIMER1        5   /* Timer 1 */
#define IRQ_TIMER2        6   /* Timer 2 */
#define IRQ_TIMER3        7   /* Timer 3 */
#define IRQ_TIMER4        8   /* Timer 4 */
#define IRQ_DMA           9   /* DMA */
#define IRQ_GPIOA        10   /* GPIOA or GPIOB */
#define IRQ_GPIOB        11   /* GPIOB */
#define IRQ_SPI1         12   /* Serial Peripheral Interface 1 */
#define IRQ_SPI2         13   /* Serial Peripheral Interface 1 */
#define IRQ_PWM          14   /* PWM */
#define IRQ_UART1        15   /* UART 1 */
#define IRQ_UART2        16   /* UART 2 */
#define IRQ_IRDA         17   /* IRDA */
#define IRQ_USB          18   /* USB */
#define IRQ_ADCONV       19   /* A/D Converter */
#define IRQ_RTC_ALARM    20   /* Real Time Clock alarm */
#define IRQ_GBIF         21   /* General Bus I/F */
#define IRQ_CF           22   /* Compact Flash */

#define	IRQ_GPIO_A(x)	(32 + (x))
#define	IRQ_GPIO_B(x)	(IRQ_GPIO_A(7) + 1 + (x))

#define	IRQ_GPIO_USB		IRQ_GPIO_B(0)
#define	IRQ_GPIO_PCMCIA_DETECT	IRQ_GPIO_B(2)
#define	IRQ_GPIO_PCMCIA_CARD	IRQ_GPIO_B(3)

#endif
