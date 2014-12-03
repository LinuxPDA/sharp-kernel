/****************************************************************************/
/*
 *      linux/include/asm-arm/arch-l7200/gpio.h
 *
 *      Registers and  helper functions for the L7200 Link-Up Systems
 *      GPIO.
 *
 *      (C) Copyright 2000, S A McConnell  (samcconn@cotw.com)
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 *  
 *  Change Log:
 *    11-12-2001  Lineo Japan, Inc.
 *  
 */

#ifndef __ASM_ARCH_GPIO_H
#define __ASM_ARCH_GPIO_H 1

#define GPIO_OFF   0x00005000  /* Offset from IO_START to the GPIO reg's. */

/* IO_START and IO_BASE are defined in hardware.h */

#define GPIO_START (IO_START_2 + GPIO_OFF) /* Physical addr of the GPIO reg. */
#define GPIO_BASE  (IO_BASE_2  + GPIO_OFF) /* Virtual addr of the GPIO reg. */

/* Offsets from the start of the GPIO for all the registers. */
#define PADR_OFF     0x000
#define PADDR_OFF    0x004
#define PASBSR_OFF   0x008
#define PAEENR_OFF   0x00c
#define PAESNR_OFF   0x010
#define PAESTR_OFF   0x014
#define PAIMR_OFF    0x018
#define PAINT_OFF    0x01c

#define PBDR_OFF     0x020
#define PBDDR_OFF    0x024
#define PBSBSR_OFF   0x028
#define PBIMR_OFF    0x038
#define PBINT_OFF    0x03c

#define PCDR_OFF     0x040
#define PCDDR_OFF    0x044
#define PCSBSR_OFF   0x048
#define PCIMR_OFF    0x058
#define PCINT_OFF    0x05c

#define PDDR_OFF     0x060
#define PDDDR_OFF    0x064
#define PDSBSR_OFF   0x068
#define PDEENR_OFF   0x06c
#define PDESNR_OFF   0x070
#define PDESTR_OFF   0x074
#define PDIMR_OFF    0x078
#define PDINT_OFF    0x07c

#define PEDR_OFF     0x080
#define PEDDR_OFF    0x084
#define PESBSR_OFF   0x088
#define PEEENR_OFF   0x08c
#define PEESNR_OFF   0x090
#define PEESTR_OFF   0x094
#define PEIMR_OFF    0x098
#define PEINT_OFF    0x09c

#define VEE_EN         0x02
#define BACKLIGHT_EN   0x04

#if defined(CONFIG_IRIS)
#include <asm/arch/gpio_iris.h>
#endif

#endif
