/*
 *  linux/include/asm-arm/arch-at91rm9200dk/hardware.h
 *
 *  This file contains the hardware definitions of the At91rm9200dk.
 *
 *  Copyright (C) 2002 ATMEL Rousset
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
 */
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <asm/sizes.h>
#include <asm/arch/platform.h>
#include <asm/arch/AT91RM9200.h>
#include <asm/arch/memory.h>

/* ARM asynchronous clock */
#define AT91C_MASTER_CLK	60000000

#define AT91C_VA_BASE_SYS	(AT91C_BASE_SYS) // (SYS) Virtual Base Address
#define AT91C_VA_BASE_SPI       (AT91C_BASE_SPI)              // (SPI)       Virtual Base Address
#define AT91C_VA_BASE_SSC2      (AT91C_BASE_SSC2)              // (SSC2)     Virtual Base Address
#define AT91C_VA_BASE_SSC1      (AT91C_BASE_SSC1)              // (SSC1)     Virtual Base Address
#define AT91C_VA_BASE_SSC0      (AT91C_BASE_SSC0)             // (SSC0)    Virtual Base Address
#define AT91C_VA_BASE_US3       (AT91C_BASE_US3)              // (US3)     Virtual Base Address
#define AT91C_VA_BASE_US2       (AT91C_BASE_US2)              // (US2)     Virtual Base Address
#define AT91C_VA_BASE_US1       (AT91C_BASE_US1)              // (US1)     Virtual Base Address
#define AT91C_VA_BASE_US0       (AT91C_BASE_US0)              // (US0)     Virtual Base Address
#define AT91C_VA_BASE_EMAC      (AT91C_BASE_EMAC)             // (EMAC) Virtual Base Address
#define AT91C_VA_BASE_TWI       (AT91C_BASE_TWI)              // (TWI)     Virtual Base Address
#define AT91C_VA_BASE_MCI       (AT91C_BASE_MCI)              // (MCI)  Virtual Base Address
#define AT91C_VA_BASE_UDP       (AT91C_BASE_UDP)              // (UDP)  Virtual Base Address
#define AT91C_VA_BASE_TCB0      (AT91C_BASE_TCB0)             // (TCB1) Virtual Base Address
#define AT91C_VA_BASE_TCB1      (AT91C_BASE_TCB1)             // (TCB0) Virtual Base Address
#define AT91C_VA_BASE_DBGU	(AT91C_VA_BASE_SYS + 0x200) // (DBGU) Virtual Base Address
#define AT91C_VA_BASE_AIC	(AT91C_VA_BASE_SYS + 0x000) // (AIC)  Virtual Base Address

#define AT91C_VA_BASE_DMA_EMAC  (PAGE_OFFSET+SZ_512M)
#define AT91C_BASE_DMA_EMAC     (AT91C_BASE_SDRAM+AT91C_SDRAM_SIZE-SZ_1M)
/* macro to get at IO space when running virtually */
#define IO_ADDRESS(x)		(x)

#endif // __ASM_ARCH_HARDWARE_H
