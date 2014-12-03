/*
 *  linux/arch/arm/mm/mm-lusl7200.c
 *
 *  Copyright (C) 2000 Steve Hill (sjhill@cotw.com)
 *
 *  Extra MM routines for L7200 architecture
 *
 *  Changelog:
 *    03-04-2001 Lineo Japan, Inc.	Changed I/O map for L7205SDB,IRIS
 *
 */
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/page.h>
#include <asm/proc/domain.h>

#include <asm/mach/map.h>

static struct map_desc l7200_io_desc[] __initdata = {
	{ IO_BASE,	 IO_START,	 IO_SIZE,	DOMAIN_IO, 0, 1 ,0 ,0},
	{ IO_BASE_2,	 IO_START_2,	 IO_SIZE_2,	DOMAIN_IO, 0, 1 ,0 ,0},
#if 1
	{ IO_BASE_3,	 IO_START_3,	 IO_SIZE_3,	DOMAIN_IO, 0, 1 ,0 ,0},
#endif
	{ AUX_BASE,      AUX_START,      AUX_SIZE,      DOMAIN_IO, 0, 1 ,0 ,0},
	{ FLASH1_BASE,   FLASH1_START,   FLASH1_SIZE,   DOMAIN_IO, 0, 1 ,0 ,0},
	{ FLASH2_BASE,   FLASH2_START,   FLASH2_SIZE,   DOMAIN_IO, 0, 1 ,0 ,0},
	{ SRAM_BASE,     SRAM_START,     SRAM_SIZE,     DOMAIN_IO, 0, 1, 0, 0},
#if defined(CONFIG_L7205SDB)
	{ EIO_BASE,      EIO_START,      EIO_SIZE,      DOMAIN_IO, 0, 1 ,0 ,0},
#endif
#if defined(CONFIG_IRIS)
	{ DEBUG_UART_BASE, DEBUG_UART_START, DEBUG_UART_SIZE,
	  DOMAIN_IO, 0, 1 ,0 ,0 },
	{ FPGA_BASE,     FPGA_START,     FPGA_SIZE,     DOMAIN_IO, 0, 1 ,0 ,0},
#ifdef CONFIG_XIP_ROM
        { IO_IOCS0_BASE, IO_IOCS0_START, IO_IOCS0_SIZE,	DOMAIN_KERNEL, 0, 1 ,1, 0},
#else
        { IO_IOCS0_BASE, IO_IOCS0_START, IO_IOCS0_SIZE,	DOMAIN_IO, 0, 1 ,0, 0},
#endif
	{ EIO_BASE,      EIO_START,      EIO_SIZE,      DOMAIN_IO, 0, 1 ,0 ,0},
/* ========== added for IOCS1 ========== */
	{ FLASH2_BASE,   FLASH2_START,   FLASH2_SIZE,      DOMAIN_IO, 0, 1 ,0 ,0},
/* =========== added for SDRAM registers ============ */
	{ SDRAM_BASE,    SDRAM_START,    SDRAM_SIZE, DOMAIN_IO, 0, 1, 0, 0},
#endif
	LAST_DESC
};

void __init l7200_map_io(void)
{
	iotable_init(l7200_io_desc);
}
