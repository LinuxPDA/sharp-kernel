/******************************************************************************
 *
 * Copyright (c) 1999-2001 MediaQ, Inc. or its subsidiaries.
 *
 * File: MQ9000HWR.h
 *
 * Description:
 *		Hardware description of MQ9000 System-on-a-chip
 *
 * History:
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *****************************************************************************/

#ifndef	__MQ9000HWR_H__
#define	__MQ9000HWR_H__


// *** Header Files ***
#include <asm/arch/types.h>
#include <asm/arch/MQ9000Regs.h>

/*******************************************************************************
 * MQ9000 Platform Specific Definitions
 ******************************************************************************/
// Fixed Physical Base Addresses
#define MQ9000_BOOT_BASE	0x00000000	// Boot (low Vect) address
#define MQ9000_FLASH_BASE	0xF0000000	// External FLASH 
#define MQ9000_EDEV0_BASE	0x20000000	// External device 0 
#define MQ9000_EDEV1_BASE	0x28000000	// External device 1 
#define MQ9000_SSRAM_BASE	0x30000000	// External SRAM 
#define MQ9000_BIUIO_BASE	0x38000000	// External IO 
#define MQ9000_ESRAM_BASE	0x40000000	// Embedded SRAM 
 
// Physical (maximum) Space
#define MQ9000_SDRAM_MAXSIZE	SZ_256M		// External SDRAM 
#define MQ9000_FLASH_MAXSIZE	SZ_256M		// External FLASH
#define MQ9000_EDEV0_MAXSIZE	SZ_128M		// External device 0 
#define MQ9000_EDEV1_MAXSIZE	SZ_128M		// External device 1 
#define MQ9000_SSRAM_MAXSIZE	SZ_64M		// External SRAM 
#define MQ9000_BIUIO_MAXSIZE	0x00000008	// External IO 
#define MQ9000_ESRAM_MAXSIZE	SZ_2M		// Embedded SRAM 

// Physical Base Addresses
//#define MQ9000_SDRAM_BASE	0x00000000	// External SDRAM => MemMap=1
#define MQ9000_SDRAM_BASE	0x10000000	// External SDRAM => MemMap=0
#define MQ9000_DMA_BASE		(MQ9000_SDRAM_BASE + SZ_48M + SZ_2M) // DMA base
#define MQ9000_DMA_SIZE		SZ_2M		// DMA size
#define MQ9000_FLASH_END	(MQ9000_FLASH_BASE + MQ9000_FLASH_SIZE)
#define MQ9000_DISPLAY_BASE	(MQ9000_ESRAM_BASE + SZ_512K) // Frame buffer base

// Virtual (Cached) Base Addresses
#define MQ9000_RAM_VBASE	0xC0000000	// RAM virtual base
#define MQ9000_FLASH_VBASE	0xD0000000	// Flash virtual base
#define MQ9000_CODE_VBASE	(MQ9000_RAM_VCBASE + SZ_32M) // Code virt. base
#define MQ9000_STACK_VBASE	(MQ9000_RAM_VCBASE + SZ_48M) // Stack virt. base
#define MQ9000_BIUIO_VBASE	0xD2000000	// External IO
#define MQ9000_SSRAM_VBASE	0xD3000000	// External SRAM base
#define MQ9000_DISPLAY_VBASE	0xD4000000	// Frame buffer base
#define MQ9000_DMA_VBASE	0xD5000000	// DMA region base
#define MQ9000_EDEV0_VBASE	0xD6000000	// External devices base
#define MQ9000_EDEV1_VBASE	0xD7000000	// External devices base
#define MQ9000_REGS_VBASE	0xD8000000	// Internal modules base
#define MQ9000_JAVA_VBASE	0xD9000000	// Java module base

// Module Base Physical Addresses
#define MQ9000_REGS_BASE	0x80000000	// Generic register base
#define MQ9000_USBD_BASE	0x80000800	// USB device
#define MQ9000_VI_BASE		0x80001000	// Video input
#define MQ9000_SSC_BASE		0x80001800	// Audio codec
#define MQ9000_SPI_BASE		0x80002000	// Serial peripheral
#define MQ9000_LED_BASE		MQ9000_SPI_BASE	// LED
#define MQ9000_SD_BASE		0x80002800	// Secure digital
#define MQ9000_KB_BASE		0x80003000	// Matrix keyboard
#define MQ9000_TIMER_BASE	0x80003800	// Timer
#define MQ9000_GPIO_BASE	MQ9000_TIMER_BASE // GPIO
#define MQ9000_UART1_BASE	0x80004000	// UART1
#define MQ9000_IRDA_BASE	MQ9000_UART1_BASE // IRDA
#define MQ9000_UART2_BASE	0x80004800	// UART2
#define MQ9000_UART3_BASE	0x80005000	// UART3
#define MQ9000_PMU_BASE		0x80005800	// Power management
#define MQ9000_I2C_FIFO_BASE	0x80006000	// I2C / SSP FIFO
#define MQ9000_I2C_BASE		0x80006800	// I2C / SSP
#define MQ9000_INTR_BASE	0x80007000	// Interrupt
#define MQ9000_RTC_BASE		0x80007800	// Real time clock
#define MQ9000_FPI_BASE		0x80008000	// Flat panel interface
#define MQ9000_GC_BASE		0x80008800	// Graphic controller
#define MQ9000_GE1_BASE		0x80009000	// Graphic engine 1
#define MQ9000_GE2_BASE		0x80009800	// Graphic engine 2 (stretch)
#define MQ9000_MIU_BASE		0x8000A000	// Memory interface
#define MQ9000_EBIU_BASE	0x8000A800	// External bus interface
#define MQ9000_DMAC_BASE	0x8000B000	// DMA controller
#define MQ9000_CC_BASE		0x8000B800	// CPU interface
#define MQ9000_PWM_BASE		0x8000C800	// Pulse width modulation
#define MQ9000_SRCFIFO_BASE	0x8000D000	// GE source fifo
#define MQ9000_CPLUT_BASE	0x8000E000	// Color palette table
#define MQ9000_YFIFO_BASE	0x8000E800	// Y-Fifo
#define MQ9000_UFIFO_BASE	0x8000F000	// U-Fifo
#define MQ9000_VFIFO_BASE	0x8000F800	// V-Fifo
#define MQ9000_JAVA_BASE	0x90000000	// Java 

//------------------------------------------------
// Platform Configuration Macros
//------------------------------------------------
#define MQDBG_BOARD_A

#ifdef MQDBG_BOARD_A
// Select Flash Device (ONE TYPE ONLY per image built)
//#define DEV_AMD_FLASH		/* 2 MB Flash  */
#define DEV_STRATA_FLASH	/* 32 MB Flash (asynchronous/page) */
//#define DEV_INTEL_FLASH	/* 4 MB Flash  (synchronous) */
#define DEV_FLASH_BURST		/* Enable flash burst (see Reset_A.s) */
#define DEV_SDRAM_32BIT
#define DEV_SSRAM_16BIT
#endif

#include <asm/arch/MQ9000Sys.h>
#include <asm/arch/MQ9000Brd.h>

// Physical Block Sizes (board specific)
#define MQ9000_SDRAM_SIZE		MQ_SDRAM_SIZE	// External SDRAM 
#define MQ9000_FLASH_SIZE		MQ_FLASH_SIZE	// External FLASH
#define MQ9000_EDEV0_SIZE		MQ_EDEV0_SIZE	// External device 0 
#define MQ9000_EDEV1_SIZE		MQ_EDEV0_SIZE	// External device 1 
#define MQ9000_SSRAM_SIZE		MQ_SSRAM_SIZE	// External SRAM 
#define MQ9000_BIUIO_SIZE		MQ_BIUIO_SIZE	// External IO 
#define MQ9000_ESRAM_SIZE		MQ_ESRAM_SIZE	// Embedded SRAM

// External Peripheral Device Modes (EBIU)
#define MQ9000_FLASH_MODE		MQ_FLASH_MODE	// See MQ9000Sys.h
#define MQ9000_SSRAM_MODE		MQ_SSRAM_MODE	// See MQ9000Sys.h
#define MQ9000_ESRAM_MODE		MQ_ESRAM_MODE	// See MQ9000Sys.h
#define MQ9000_EDEV0_MODE		MQ_EDEV0_MODE	// See MQ9000Sys.h
#define MQ9000_EDEV1_MODE		MQ_EDEV1_MODE	// See MQ9000Sys.h
#define MQ9000_SDRAM_MODE1		MQ_SDRAM_MODE1	// See MQ9000Sys.h
#define MQ9000_SDRAM_MODE2		MQ_SDRAM_MODE2	// See MQ9000Sys.h
#define MQ9000_SDRAM_MODE3		MQ_SDRAM_MODE3	// See MQ9000Sys.h
#define MQ9000_SDRAM_MODE4		MQ_SDRAM_MODE4	// See MQ9000Sys.h
#define MQ9000_EDMAC_MODE1		MQ_EDMAC_MODE1	// See MQ9000Sys.h
#define MQ9000_EDMAC_MODE2		MQ_EDMAC_MODE2	// See MQ9000Sys.h
#define MQ9000_EDMAC_MODE3		MQ_EDMAC_MODE3	// See MQ9000Sys.h
 
#ifndef __ASSEMBLY__	//linux gcc does not like typedef

/*******************************************************************************
 * MQ9000 MMU (TLB) Page Structure
 ******************************************************************************/
typedef union {
  struct {
    UInt32	tag:2;
    UInt32	ignored:30;
  } faultEntry;
  struct {
    UInt32	tag:2;
    UInt32	implementationDefined:3;
    UInt32	domain:4;
    UInt32	shouldBeZero:1;
    UInt32	base:22;
  } coarseEntry;
  struct {
    UInt32	tag:2;
    UInt32	buffered:1;
    UInt32	cacheable:1;
    UInt32	implementationDefined:1;
    UInt32	domain:4;
    UInt32	shouldBeZero2:1;
    UInt32	accessPermissions:2;
    UInt32	shouldBeZero1:8;
    UInt32	base:12;
  } sectionEntry;
  struct {
    UInt32	tag:2;
    UInt32	implementationDefined:3;
    UInt32	domain:4;
    UInt32	shouldBeZero:3;
    UInt32	base:20;
  } fineEntry;
} FirstLevelTableEntry, *FirstLevelTableEntryPtr;

typedef union {
  struct {
    UInt32	tag:2;
    UInt32	ignored:30;
  } faultEntry;
  struct {
    UInt32	tag:2;
    UInt32	buffered:1;
    UInt32	cacheable:1;
    UInt32	accessPermissions0:2;
    UInt32	accessPermissions1:2;
    UInt32	accessPermissions2:2;
    UInt32	accessPermissions3:2;
    UInt32	shouldBeZero:4;
    UInt32	base:16;
  } largePageEntry;
  struct {
    UInt32	tag:2;
    UInt32	buffered:1;
    UInt32	cacheable:1;
    UInt32	accessPermissions0:2;
    UInt32	accessPermissions1:2;
    UInt32	accessPermissions2:2;
    UInt32	accessPermissions3:2;
    UInt32	base:20;
  } smallPageEntry;
  struct {
    UInt32	tag:2;
    UInt32	buffered:1;
    UInt32	cacheable:1;
    UInt32	accessPermissions:2;
    UInt32	shouldBeZero:4;
    UInt32	base:22;
  } tinyPageEntry;
} SecondLevelTableEntry, *SecondLevelTableEntryPtr;

typedef enum {
  FaultEntryType = 0x0,
  CoarseEntryType = 0x1,
  SectionEntryType = 0x2,
  FineEntryType = 0x3,
  LargePageType = 0x1,
  SmallPageType = 0x2,
  TinyPageType = 0x3
} PageTableEntryType;

#endif	//__ASSEMBLY__

#define CoarseEntryRequiredBits	0x4
#define BitsPerMegabyte	20
#define BitsPerPage	12

#endif // __EP73XXHWR_H__
