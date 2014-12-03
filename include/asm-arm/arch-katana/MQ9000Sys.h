/*
 *  MQ9000Sys.H : System Registers header file for MQ9000 Controller.
 *
 *  Copyright (c) 2002 by MediaQ, Incorporated.
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
 */

#ifndef _MQ9000SYS_H_
#define _MQ9000SYS_H_

#include <asm/arch/sizes.h>

/*******************************************************************************
 * MQ9000 System Memory 
 ******************************************************************************/
#ifdef MQDBG_BOARD_A
#define MQ_SDRAM_SIZE			SZ_64M			// External SDRAM
#define	MQ_FLASH_SIZE			SZ_16M			// External FLASH
#define MQ_EDEV0_SIZE			SZ_64M			// External device 0 
#define MQ_EDEV0_SIZE			SZ_64M			// External device 1 
#define MQ_SSRAM_SIZE			SZ_2M			// External SRAM
#define MQ_BIUIO_SIZE			8				// External IO 
#define MQ_ESRAM_SIZE			SZ_320K			// Embedded SRAM
#endif

/*******************************************************************************
 * MQ9000 System Registers Bit Definitions
 ******************************************************************************/
// ---------------------------------------------
// PMU (Active, Idle) Registers
// ---------------------------------------------
#define MQ_PLL1_ENABLE			0x00000001
#define MQ_PLL2_ENABLE			0x00000002
#define MQ_ROSC_ENABLE			0x00000008		// Relaxation Oscillator
#define MQ_GE_ENABLE			0x00000010		// Graphics Engine
#define MQ_JAVA_ENABLE			0x00000020		
#define MQ_SBLT_ENABLE			0x00000030		// Stretch Blt
#define MQ_EBIU_ENABLE			0x00000040		// External Bus
#define MQ_USBD_ENABLE			0x00000080		// USB Device	
#define MQ_FOSC_ENABLE			0x00000080		// Fast Oscillator Enable		
#define MQ_FOSC_CNTRL			0x00000300	 	// Fast Oscillator Control
#define MQ_FOSC_10pF			0x00000000		// 10 pF loading capacitance
#define MQ_FOSC_18pF			0x00000100		// 18 pF loading capacitance
#define MQ_FOSC_3MHz			0x00000200		// 18 pF Cload w/ 3.86 MHz crystal
#define MQ_FOSC_5MHz			0x00000300		// 18 pF Cload w/ 5.76 MHz crystal
#define MQ_FOSC_2048			0x00000000		// FOsc. startup time = 61.2 ms
#define MQ_FOSC_4096			0x00010000		// FOsc. startup time = 122.4 ms
#define MQ_FOSC_8192			0x00020000		// FOsc. startup time = 244.8 ms
#define MQ_FOSC_16384			0x00030000		// FOsc. startup time = 489.6 ms
#define MQ_PLL_2048				0x00000000		// PLL locked in time = 61.2 ms
#define MQ_PLL_4096				0x00010000		// PLL locked in time = 122.4 ms
#define MQ_PLL_8192				0x00020000		// PLL locked in time = 244.8 ms
#define MQ_PLL_16384			0x00030000		// PLL locked in time = 489.6 ms

// ---------------------------------------------
// PLL Registers
// ---------------------------------------------
#define MQ_PLL_FOSC_REF			0x00000000		// PLL reference clock = FOSCLK
#define MQ_PLL_L2CLK_REF		0x00000001		// PLL reference clock = L2CLK
#define MQ_PLL_BYPASS			0x00000002		// PLL bypass mode
#define MQ_PLL_P_DIV1			0x00000000		// P divisor = 1
#define MQ_PLL_P_DIV2			0x00000010		// P divisor = 2
#define MQ_PLL_P_DIV4			0x00000020		// P divisor = 4
#define MQ_PLL_P_DIV8			0x00000030		// P divisor = 8
#define MQ_PLL_P_DIV16			0x00000040		// P divisor = 16
#define MQ_PLL_N_PARAM			0x00000700		// N parameter
#define MQ_PLL_M_PARAM			0x000FF000		// M parameter
#define MQ_PLL_VSS_DBG			0x00000000		// VSS debug 
#define MQ_PLL_REF_DBG			0x00200000		// Internal ref clock debug
#define MQ_PLL_N_DBG			0x00400000		// N debug
#define MQ_PLL_M_DBG			0x00600000		// M debug
#define MQ_PLL_P_DBG			0x00800000		// P debug
#define MQ_PLL_VCO_DBG			0x00A00000		// VCO debug
#define MQ_PLL_I_25				0x03000000		// PLL current (I) charge: 25%
#define MQ_PLL_I_50				0x02000000		// PLL current (I) charge: 50%
#define MQ_PLL_I_75				0x01000000		// PLL current (I) charge: 75%
#define MQ_PLL_I_100			0x00000000		// PLL current (I) charge: 100%

// ---------------------------------------------
// PMU Clock Select Register
// ---------------------------------------------
#define MQ_ROSC_7MHz			0x00000000		// ROSC freq = 7MHz	
#define MQ_ROSC_8MHz			0x00000001		// ROSC freq = 8MHz	
#define MQ_ROSC_9MHz			0x00000002		// ROSC freq = 9MHz	
#define MQ_ROSC_10MHz			0x00000003		// ROSC freq = 10MHz	
#define MQ_ROSC_11MHz			0x00000004		// ROSC freq = 11MHz	
#define MQ_ROSC_12MHz			0x00000005		// ROSC freq = 12MHz	
#define MQ_ROSC_13MHz			0x00000006		// ROSC freq = 13MHz	
#define MQ_ROSC_14MHz			0x00000007		// ROSC freq = 14MHz	
#define MQ_FCLK_SRC_FOSC		0x00000000		// FCLK source = FOsc.
#define MQ_FCLK_SRC_ROSC		0x00000010		// FCLK source = ROsc.
#define MQ_FCLK_SRC_PLL1		0x00000020		// FCLK source = PLL1
#define MQ_FCLK_SRC_PLL1_2		0x00000030		// FCLK source = PLL1 / 2
#define MQ_FCLK_SRC_PLL1_4		0x00000040		// FCLK source = PLL1 / 4
#define MQ_FCLK_SRC_PLL1_8		0x00000050		// FCLK source = PLL1 / 8
#define MQ_FCLK_SRC_PLL1_16     0x00000060		// FCLK source = PLL1 / 16
#define MQ_FCLK_SRC_PLL1_32		0x00000070		// FCLK source = PLL1 / 32
#define MQ_FCLK_SRC_PLL2		0x00000080		// FCLK source = PLL2
#define MQ_FCLK_SRC_PLL2_2		0x00000090		// FCLK source = PLL2 / 2
#define MQ_FCLK_SRC_PLL2_4		0x000000A0		// FCLK source = PLL2 / 4
#define MQ_FCLK_SRC_PLL2_8		0x000000B0		// FCLK source = PLL2 / 8
#define MQ_FCLK_SRC_PLL2_16     0x000000C0		// FCLK source = PLL2 / 16
#define MQ_FCLK_SRC_PLL2_32		0x000000D0		// FCLK source = PLL2 / 32

// ---------------------------------------------
// PMU GE Control Registers
// ---------------------------------------------
#define MQ_GE_PM_ENABLE			0x00000001		// GE power-managed bit
#define MQ_GE_FORCE_BUSY		0x00000002		// GE (forced) Busy
#define MQ_GECLK_ENABLE			0x00000004		// GE clock	
#define MQ_GECLK_DIV1			0x00000000		// GE clock divide by 1
#define MQ_GECLK_DIV15			0x00000008		// GE clock divide by 1.5
#define MQ_GECLK_DIV2			0x00000010		// GE clock divide by 2
#define MQ_GECLK_DIV25			0x00000018		// GE clock divide by 2.5
#define MQ_GECLK_DIV3			0x00000020		// GE clock divide by 3
#define MQ_GECLK_DIV4			0x00000028		// GE clock divide by 3
#define MQ_GECLK_DIV5			0x00000030		// GE clock divide by 3
#define MQ_GECLK_DIV6			0x00000038		// GE clock divide by 3

// ---------------------------------------------
// PMU Status Registers
// ---------------------------------------------
#define MQ_PMU_FOSC				0x00000001		// FOsc. status
#define MQ_PMU_PLL1				0x00000002	 	// PLL1 status
#define MQ_PMU_PLL2				0x00000004	 	// PLL2 status
#define MQ_PMU_MIU				0x00000020	 	// MIU status
#define MQ_PMU_GE				0x00000040	 	// GE status
#define MQ_PMU_GC				0x00000080	 	// GC status

// ---------------------------------------------
// EBIU Control Registers
// ---------------------------------------------
#define MQ_SDRAM_ENABLE			0x00000001		// External SDRAM
#define MQ_SSRAM_ENABLE			0x00000002		// External SRAM
#define MQ_EDEV0_ENABLE			0x00000004		// External Peripheral 0
#define MQ_EDEV1_ENABLE			0x00000008		// External Peripheral 1
#define MQ_NANDF_ENABLE			0x00000010		// External NAND Flash
#define MQ_ESRAM_ENABLE			0x00000020		// Embedded SRAM

// ---------------------------------------------
// EBIU CPU Access Registers
// ---------------------------------------------
#define MQ_CPUFIFO1_ENABLE		0x00000000		// CPU Fifo 1 seq access 
#define MQ_CPUFIFO1_READ		0x00000000		// CPU Fifo 1 read access
#define MQ_CPUFIFO1_WRITE		0x00000002		// CPU Fifo 1 write access
#define MQ_CPUFIFO2_ENABLE		0x00000004		// CPU Fifo 2 seq access 
#define MQ_CPUFIFO2_READ		0x00000000		// CPU Fifo 2 read access
#define MQ_CPUFIFO2_WRITE		0x00000008		// CPU Fifo 2 write access
#define MQ_CPUFIFO1_BUSY		0x00000010		// CPU Fifo 1 busy
#define MQ_CPUFIFO2_BUSY		0x00000020		// CPU Fifo 2 busy
#define MQ_CPUFIFO1_SDRAM		0x00000000		// CPU Fifo 1 access SDRAM
#define MQ_CPUFIFO1_SSRAM		0x00000100		// CPU Fifo 1 access ext. SRAM
#define MQ_CPUFIFO1_ESRAM		0x00000200		// CPU Fifo 1 access emb. SRAM
#define MQ_CPUFIFO2_SDRAM		0x00000000		// CPU Fifo 2 access SDRAM
#define MQ_CPUFIFO2_SSRAM		0x00000400		// CPU Fifo 2 access ext. SRAM
#define MQ_CPUFIFO2_ESRAM		0x00000800		// CPU Fifo 2 access emb. SRAM
// EBIU CPU Access Tuned Parameters (TBD at runtime)
#define MQ_CPUFIFO1_TRHLD		0x00000000		// CPU Fifo 1 threshold value
#define MQ_CPUFIFO2_TRHLD		0x00000000		// CPU Fifo 2 threshold value
#define MQ_CPUFIFO1_FILLED		0x00000000		// CPU Fifo 1 filled status check
#define MQ_CPUFIFO2_FILLED		0x00000000		// CPU Fifo 2 filled status check

// ---------------------------------------------
// EBIU Flash Interface Register
// ---------------------------------------------
#define MQ_FLASH_CLK			0x00000000		// Flash driven by EBIU clock
#define MQ_FLASH_CLK_DIV2		0x00000001		// Flash driven by EBIU clock / 2
#define MQ_FLASH_SYNC_MODE		0x00000002		// Flash synch. mode
#define MQ_FLASH_ASYNC_MODE		0x00000004		// Flash asynch. page mode
#define MQ_FLASH_AMD_BURST		0x00000008      // AMD Flash synch. burst mode 
#define MQ_FLASH_BURST_1		0x00000000		// Burst count = 1
#define MQ_FLASH_BURST_2		0x00000010		// Burst count = 2
#define MQ_FLASH_BURST_4		0x00000020		// Burst count = 4
#define MQ_FLASH_BURST_8		0x00000030		// Burst count = 8
#define MQ_FLASH_CLK_PHASE		0x01000000		// Burst clock phase = EBIU Clock
#define MQ_FLASH_INV_CLK_PHASE  0x00000000		// Burst clock phase = ~EBIU Clock
// EBIU Flash Tuned Parameters (TBD at runtime)
#if   defined (DEV_AMD_FLASH) 
#define MQ_FLASH_WR_CYCLE		0x00000100		// Write cycle
#define MQ_FLASH_INIT_RD_TIME	0x00000000		// Init. read access time
#define MQ_FLASH_PAGE_RD_TIME	0x00010000		// Page read access time
#define MQ_FLASH_CS_RECOV_TIME  0x00000000      // CS recovery time
#define MQ_FLASH_BURST_DELAY	0x00000000		// Flash burst delay b25-b29 (nsec)
#define MQ_FLASH_MODE	(MQ_FLASH_CLK | MQ_FLASH_SYNC_MODE | MQ_FLASH_BURST_1 |   \
						 MQ_FLASH_WR_CYCLE | MQ_FLASH_INV_CLK_PHASE | 			  \
						 MQ_FLASH_INIT_RD_TIME | MQ_FLASH_PAGE_RD_TIME |		  \
						 MQ_FLASH_CS_RECOV_TIME | MQ_FLASH_BURST_DELAY)      
#elif defined (DEV_INTEL_FLASH)
#define MQ_FLASH_WR_CYCLE		0x00000100		// Write cycle
#define MQ_FLASH_INIT_RD_TIME	0x00000000		// Init. read access time
#define MQ_FLASH_PAGE_RD_TIME	0x00010000		// Page read access time
#define MQ_FLASH_CS_RECOV_TIME  0x00000000      // CS recovery time
#define MQ_FLASH_BURST_DELAY	0x00000000		// Flash burst delay b25-b29 (nsec)
#define MQ_FLASH_MODE	(MQ_FLASH_CLK | MQ_FLASH_SYNC_MODE | MQ_FLASH_BURST_1 |   \
						 MQ_FLASH_WR_CYCLE | MQ_FLASH_INV_CLK_PHASE | 			  \
						 MQ_FLASH_INIT_RD_TIME | MQ_FLASH_PAGE_RD_TIME |		  \
						 MQ_FLASH_CS_RECOV_TIME | MQ_FLASH_BURST_DELAY)
#else  // Default settings
#define MQ_FLASH_WR_CYCLE		0x00000100		// Write cycle
#define MQ_FLASH_INIT_RD_TIME	0x00000000		// Init. read access time
#define MQ_FLASH_PAGE_RD_TIME	0x00010000		// Page read access time
#define MQ_FLASH_CS_RECOV_TIME  0x00000000      // CS recovery time
#define MQ_FLASH_BURST_DELAY	0x00000000		// Flash burst delay b25-b29 (nsec)
#define MQ_FLASH_MODE	(MQ_FLASH_CLK | MQ_FLASH_SYNC_MODE | MQ_FLASH_BURST_1 |   \
						 MQ_FLASH_WR_CYCLE | MQ_FLASH_INV_CLK_PHASE | 			  \
						 MQ_FLASH_INIT_RD_TIME | MQ_FLASH_PAGE_RD_TIME |		  \
						 MQ_FLASH_CS_RECOV_TIME | MQ_FLASH_BURST_DELAY) 	 
#endif

// ---------------------------------------------
// EBIU SSRAM Interface Register
// ---------------------------------------------
#define MQ_SSRAM_8BIT_BUS 		0x00000000		// SRAM data bus size = 8
#define MQ_SSRAM_16BIT_BUS		0x00000001		// SRAM data bus size = 16
#define MQ_SSRAM_BURST_1		0x00000000		// SRAM burst count = 1
#define MQ_SSRAM_BURST_2		0x00000002		// SRAM burst count = 2
#define MQ_SSRAM_BURST_4		0x00000004		// SRAM burst count = 4
#define MQ_SSRAM_BURST_8		0x00000006		// SRAM burst count = 8
#define MQ_SSRAM_CS2WE_N1		0x00000000		// OE/WE 1 clk < CS
#define MQ_SSRAM_CS2WE_0		0x00000010		// OE/WE = CS
#define MQ_SSRAM_CS2WE_P1		0x00000020		// OE/WE 1 clk > CS
#define MQ_SSRAM_CS2WE_H_N1		0x00000000		// CS hold time < OE/WE
#define MQ_SSRAM_CS2WE_H_0		0x00000040		// CS hold time = OE/WE
#define MQ_SSRAM_CS2WE_H_P1		0x00000080		// CS hold time > 0E/WE
// EBIU SSRAM Tuned Parameters (TBD at runtime)
#if defined (DEV_SSRAM_16BIT) /*0x00804263*/
#define MQ_SSRAM_WR_CYCLE		0x00000200		// Write cycle
#define MQ_SSRAM_RD_TIME		0x00004000		// Read cycle time
#define MQ_SSRAM_CS_RECOV_TIME	0x00000000		// CS recovery time
#define MQ_SSRAM_Z_TIME			0x00800000		// Data bus floating time
#define MQ_SSRAM_MODE	(MQ_SSRAM_16BIT_BUS | MQ_SSRAM_BURST_2 | 		\
						 MQ_SSRAM_CS2WE_P1 | MQ_SSRAM_CS2WE_H_0 |		\
						 MQ_SSRAM_WR_CYCLE | MQ_SSRAM_RD_TIME | 		\
						 MQ_SSRAM_CS_RECOV_TIME | MQ_SSRAM_Z_TIME)
#else  						  /*0x018CE755 = Default settings */
#define MQ_SSRAM_WR_CYCLE		0x00000700		// Write cycle
#define MQ_SSRAM_RD_TIME		0x0000E000		// Read cycle time
#define MQ_SSRAM_CS_RECOV_TIME	0x000C0000		// CS recovery time
#define MQ_SSRAM_Z_TIME			0x01800000		// Data bus floating time
#define MQ_SSRAM_MODE	(MQ_SSRAM_16BIT_BUS | MQ_SSRAM_BURST_4 | 		\
						 MQ_SSRAM_CS2WE_0 | MQ_SSRAM_CS2WE_H_0 |		\
						 MQ_SSRAM_WR_CYCLE | MQ_SSRAM_RD_TIME | 		\
						 MQ_SSRAM_CS_RECOV_TIME | MQ_SSRAM_Z_TIME)
#endif

// ---------------------------------------------
// EBIU External Peripheral Interface Register
// ---------------------------------------------
#define MQ_EDEV_8BIT_BUS		0x00000000		// EDev bus size = 8
#define MQ_EDEV_16BIT_BUS		0x00000001		// EDev bus size = 16
#define MQ_EDEV_BURST_1			0x00000000		// EDEV burst count = 1 
#define MQ_EDEV_BURST_2			0x00000002		// EDEV burst count = 2 
#define MQ_EDEV_BURST_4			0x00000004		// EDEV burst count = 4 
#define MQ_EDEV_BURST_8			0x00000006		// EDEV burst count = 8
#define MQ_EDEV_RDY_DISABLE		0x00000008		// EDEV ready detection
#define MQ_EDEV_RDY_HIGH		0x00000000		// EDEV ready polarity high
#define MQ_EDEV_RDY_LOW			0x00000010		// EDEV ready polarity low
#define MQ_EDEV_BURST_DISABLE	0x00000020		// EDEV burst disable
#define MQ_EDEV_CS2WE_N1		0x00000000		// OE/WE 1 clk < CS
#define MQ_EDEV_CS2WE_0			0x00000100		// OE/WE = CS
#define MQ_EDEV_CS2WE_P1		0x00000200		// OE/WE 1 clk > CS
#define MQ_EDEV_CS2WE_H_N1		0x00000000		// CS hold time < OE/WE
#define MQ_EDEV_CS2WE_H_0		0x00000400		// CS hold time = OE/WE
#define MQ_EDEV_CS2WE_H_P1		0x00000800		// CS hold time > OE/WE
// EBIU EDEV Tuned Parameters (TBD at runtime)
#if   defined (MQDBG_BOARD_A) 
#define MQ_EDEV_WE_RECOV_TIME	0x00000000		// WE recovery time
#define MQ_EDEV_CS_RECOV_TIME	0x00000000		// CS recovery time
#define MQ_EDEV_Z_TIME			0x00000000		// Data bus floating time
#define MQ_EDEV0_MODE	(MQ_EDEV_16BIT_BUS | MQ_EDEV_BURST_1 | 			 \
						 MQ_EDEV_RDY_DISABLE | MQ_EDEV_RDY_LOW |		 \
						 MQ_EDEV_CS2WE_0 | MQ_EDEV_CS2WE_H_0 |			 \
						 MQ_EDEV_WE_RECOV_TIME | MQ_EDEV_CS_RECOV_TIME | \
						 MQ_EDEV_Z_TIME)
#define MQ_EDEV1_MODE	MQ_EDEV0_MODE
#else  // Default settings
#define MQ_EDEV_WE_RECOV_TIME	0x00000000		// WE recovery time
#define MQ_EDEV_CS_RECOV_TIME	0x00000000		// CS recovery time
#define MQ_EDEV_Z_TIME			0x00000000		// Data bus floating time
#define MQ_EDEV0_MODE	(MQ_EDEV_16BIT_BUS | MQ_EDEV_BURST_1 | 			 \
						 MQ_EDEV_RDY_DISABLE | MQ_EDEV_RDY_LOW |		 \
						 MQ_EDEV_CS2WE_0 | MQ_EDEV_CS2WE_H_0 |			 \
						 MQ_EDEV_WE_RECOV_TIME | MQ_EDEV_CS_RECOV_TIME | \
						 MQ_EDEV_Z_TIME)
#define MQ_EDEV1_MODE	MQ_EDEV0_MODE
#endif

// ---------------------------------------------
// EBIU SDRAM Control Register 1
// ---------------------------------------------
#define MQ_SDRAM_16BIT_BUS		0x00000000		// SDRAM bus size = 16
#define MQ_SDRAM_32BIT_BUS		0x00000001		// SDRAM bus size = 32
#define MQ_SDRAM_HIGH_WORD		0x00000000		// SDRAM connected via upper 16-bit
#define MQ_SDRAM_LOW_WORD		0x00000002		// SDRAM connected via lower 16-bit
#define MQ_SDRAM_BURST_1		0x00000000		// SDRAM burst count = 1
#define MQ_SDRAM_BURST_2		0x00000004		// SDRAM burst count = 2
#define MQ_SDRAM_BURST_4		0x00000008		// SDRAM burst count = 4
#define MQ_SDRAM_BURST_8		0x0000000C		// SDRAM burst count = 8
#define MQ_SDRAM_1_MODULE		0x00000000		// SDRAM module = 1
#define MQ_SDRAM_2_MODULE		0x00000010		// SDRAM module = 2
#define MQ_SDRAM_SZ_8MB			0x00000000		// SDRAM total size = 8MB
#define MQ_SDRAM_SZ_16MB		0x00000020		// SDRAM total size = 8MB					
#define MQ_SDRAM_SZ_32MB		0x00000040		// SDRAM total size = 8MB
#define MQ_SDRAM_SZ_64MB		0x00000060		// SDRAM total size = 8MB
#define MQ_SDRAM_PM_ACTIVE		0x00000080		// SDRAM (Active) power mgmt enable
#define MQ_SDRAM_PM_SLEEP		0x00000100		// SDRAM (Sleep) power mgmt enable
#define MQ_SDRAM_OPEN_PAGE		0x00000200		// SDRAM open page control
// EBIU SDRAM Tuned Parameters (TBD at runtime)
#if defined (DEV_SDRAM_32BIT) /*0x0025EC7D*/
#define MQ_SDRAM_A_ECLK			0x00000C00		// SDRAM activation EBIU clk latency
#define MQ_SDRAM_C_ECLK			0x0000E000		// SDRAM command EBIU clk latency
#define MQ_SDRAM_P_ECLK			0x00010000		// SDRAM precharge EBIU clk latency
#define MQ_SDRAM_AC_ECLK  		0x00040000		// SDRAM activation to command ECLKs
#define MQ_SDRAM_AP_ECLK		0x00200000		// SDRAM activation to precharge ECLKs
#define MQ_SDRAM_CP_ECLK		0x00000000		// SDRAM command to precharge ECLKs
#define MQ_SDRAM_CAS_LATENCY	0x00000000		// SDRAM CAS latency 
#define MQ_SDRAM_MODE1	(MQ_SDRAM_32BIT_BUS | MQ_SDRAM_HIGH_WORD | 					\
						 MQ_SDRAM_BURST_8 | MQ_SDRAM_2_MODULE |						\
						 MQ_SDRAM_SZ_64MB | 										\
						 MQ_SDRAM_A_ECLK | MQ_SDRAM_C_ECLK | MQ_SDRAM_P_ECLK |		\
						 MQ_SDRAM_AC_ECLK | MQ_SDRAM_AP_ECLK | MQ_SDRAM_CP_ECLK |	\
						 MQ_SDRAM_CAS_LATENCY)
#else  						  /*0x0025EC40 = default 16Bit settings */
#define MQ_SDRAM_A_ECLK			0x00000C00		// SDRAM activation EBIU clk latency
#define MQ_SDRAM_C_ECLK			0x0000E000		// SDRAM command EBIU clk latency
#define MQ_SDRAM_P_ECLK			0x00010000		// SDRAM precharge EBIU clk latency
#define MQ_SDRAM_AC_ECLK  		0x00040000		// SDRAM activation to command ECLKs
#define MQ_SDRAM_AP_ECLK		0x00200000		// SDRAM activation to precharge ECLKs
#define MQ_SDRAM_CP_ECLK		0x00000000		// SDRAM command to precharge ECLKs
#define MQ_SDRAM_CAS_LATENCY	0x00000000		// SDRAM CAS latency 
#define MQ_SDRAM_MODE1	(MQ_SDRAM_16BIT_BUS | MQ_SDRAM_HIGH_WORD | 					\
						 MQ_SDRAM_BURST_1 | MQ_SDRAM_1_MODULE |						\
						 MQ_SDRAM_SZ_8MB | MQ_SDRAM_OPEN_PAGE |						\
						 MQ_SDRAM_A_ECLK | MQ_SDRAM_C_ECLK | MQ_SDRAM_P_ECLK |		\
						 MQ_SDRAM_AC_ECLK | MQ_SDRAM_AP_ECLK | MQ_SDRAM_CP_ECLK |	\
						 MQ_SDRAM_CAS_LATENCY)
#endif

// ---------------------------------------------
// EBIU SDRAM Control Register 2
// ---------------------------------------------
#define MQ_SDRAM_EMODE_ENABLE 	0x00000001		// SDRAM extended mode enable
#define MQ_SDRAM_CLK_PHASE_INV	0x00000000		// SDRAM clock phase = ~EBIU clock
// EBIU SDRAM Tuned Parameters (TBD at runtime)
#if defined (DEV_SDRAM_32BIT) /*0x00000000*/
#define MQ_SDRAM_EMODE			0x00000000		// SDRAM extended mode b1-b13
#define MQ_SDRAM_CLK_DELAY		0x000E0000		// SDRAM clock delay (nsec) b17-b21
#define MQ_SDRAM_MODE2	(MQ_SDRAM_CLK_DELAY)
#else 						  /*0x00000000 = Default 16Bit settings*/
#define MQ_SDRAM_EMODE			0x00000000		// SDRAM extended mode b1-b13
#define MQ_SDRAM_CLK_DELAY		0x00020000		// SDRAM clock delay (nsec) b17-b21
#define MQ_SDRAM_MODE2	(MQ_SDRAM_CLK_PHASE_INV)
#endif

// ---------------------------------------------
// EBIU SDRAM Control Register 3
// ---------------------------------------------
#define MQ_SDRAM_AR_BURST_1		0x00000000		// SDRAM auto-refresh burst cnt=1
#define MQ_SDRAM_AR_BURST_2		0x00000001		// SDRAM auto-refresh burst cnt=2
#define MQ_SDRAM_AR_BURST_3		0x00000002		// SDRAM auto-refresh burst cnt=3
#define MQ_SDRAM_AR_BURST_4		0x00000003		// SDRAM auto-refresh burst cnt=4
// EBIU SDRAM Tuned Parameters (TBD at runtime)
#if defined (DEV_SDRAM_32BIT) /*0x00000640*/
#define MQ_SDRAM_AR_TIMING		0x00000640		// SDRAM auto-refresh timing b2-b13
#define MQ_SDRAM_MODE3	(MQ_SDRAM_AR_BURST_1 | MQ_SDRAM_AR_TIMING)
#else 						  /*0x00000000 = Default 16Bit settings */
#define MQ_SDRAM_AR_TIMING		0x00000640		// SDRAM auto-refresh timing b2-b13
#define MQ_SDRAM_MODE3	(MQ_SDRAM_AR_BURST_1 | MQ_SDRAM_AR_TIMING)
#endif

// ---------------------------------------------
// EBIU SDRAM Control Register 4
// ---------------------------------------------
#define MQ_SDRAM_C_PULSE_DISABLE 0x00000000		// SDRAM cmd pulse disable
#define MQ_SDRAM_C_PULSE_ENABLE  0x00000001 	// SDRAM cmd pulse enable
// EBIU SDRAM Tuned Parameters (TBD at runtime)
#if defined (DEV_SDRAM_32BIT) /*0x00000000*/
#define MQ_SDRAM_MODE4	MQ_SDRAM_C_PULSE_DISABLE
#else 						  /*0x00000000 = Default 16Bit settings */
#define MQ_SDRAM_MODE4	MQ_SDRAM_C_PULSE_DISABLE
#endif

// ---------------------------------------------
// EBIU ESRAM Control Register 
// ---------------------------------------------
#define MQ_ESRAM_BURST_1		0x00000000		// ESRAM burst count = 1
#define MQ_ESRAM_BURST_2		0x00000001		// ESRAM burst count = 2
#define MQ_ESRAM_BURST_4		0x00000002		// ESRAM burst count = 4
#define MQ_ESRAM_BURST_8		0x00000003		// ESRAM burst count = 8
#define MQ_ESRAM_BIG_ENDIAN		0x00000004		// ESRAM fetches are in BE mode
// EBIU ESRAM Tuned Parameters (TBD at runtime)
#if   defined (MQDBG_BOARD_A)
#define MQ_ESRAM_MODE	(MQ_ESRAM_BURST_1)
#else
#define MQ_ESRAM_MODE	(MQ_ESRAM_BURST_1)
#endif

// ---------------------------------------------
// EBIU DMA Control Register 1 
// ---------------------------------------------
#define MQ_EDMAC_SDRAM			0x00000000		// SDRAM is target memory
#define MQ_EDMAC_SSRAM			0x00000001		// SSRAM is target memory
#define MQ_EDMAC_ESRAM			0x00000002		// ESRAM is target memory
#define MQ_EDMAC_GE_SDRAM		0x00000000		// SDRAM is GE DMA target memory
#define MQ_EDMAC_GE_SSRAM		0x00000004		// SSRAM is GE DMA target memory
#define MQ_EDMAC_USB_BE			0x00000008		// USB device DMA is big endian
#define MQ_EDMAC_SD_BE			0x00000010		// SD DMA is big endian
#define MQ_EDMAC_SSC_BE			0x00000020		// SSC DMA is big endian
#define MQ_EDMAC_U2_BE			0x00000040		// UART2 DMA is big endian
#define MQ_EDMAC_U3_BE			0x00000080		// UART3 DMA is big endian
#define MQ_EDMAC_BE				0x00000100		// EDMAC is big endian
#define MQ_EDMAC_NAND_BE		0x00000200		// NAND Flash DMA is big endian
// EBIU EDMAC Tuned Parameters (TBD at runtime)
#if   defined (MQDBG_BOARD_A)
#define MQ_EDMAC_MODE1	(MQ_EDMAC_SDRAM | MQ_EDMAC_GE_SSRAM)
#else
#define MQ_EDMAC_MODE1	(MQ_EDMAC_SDRAM | MQ_EDMAC_GE_SSRAM)
#endif

// ---------------------------------------------
// EBIU DMA Control Register 2
// ---------------------------------------------
#define MQ_EDMAC_USB_TX_TRHLD	0x00000001		// USB Transmit DMA treshold (4bits)
#define MQ_EDMAC_USB_RX_TRHLD	0x00000010		// USB Receive DMA treshold (4bits)
#define MQ_EDMAC_SD_TX_TRHLD	0x00000100		// SD Transmit DMA treshold (4bits)
#define MQ_EDMAC_SD_RX_TRHLD	0x00001000		// SD Receive DMA treshold (4bits)
#define MQ_EDMAC_SSC_TX_TRHLD	0x00010000		// SSC Transmit DMA treshold (4bits)
#define MQ_EDMAC_SSC_RX_TRHLD	0x00100000		// SSC Receive DAM treshold (4bits)
// EBIU EDMAC Tuned Parameters (TBD at runtime)
#if   defined (MQDBG_BOARD_A)
#define MQ_EDMAC_MODE2	(MQ_EDMAC_USB_TX_TRHLD | MQ_EDMAC_USB_RX_TRHLD |	\
						 MQ_EDMAC_SD_TX_TRHLD | MQ_EDMAC_SD_RX_TRHLD |		\
						 MQ_EDMAC_SSC_TX_TRHLD | MQ_EDMAC_SSC_RX_TRHLD)
#else
#define MQ_EDMAC_MODE2	(MQ_EDMAC_USB_TX_TRHLD | MQ_EDMAC_USB_RX_TRHLD |	\
						 MQ_EDMAC_SD_TX_TRHLD | MQ_EDMAC_SD_RX_TRHLD |		\
						 MQ_EDMAC_SSC_TX_TRHLD | MQ_EDMAC_SSC_RX_TRHLD)
#endif

// ---------------------------------------------
// EBIU DMA Control Register 3 
// ---------------------------------------------
#define MQ_EDMAC_U2_TX_TRHLD	0x00000001		// UART2 Transmit DMA treshold (5bits)
#define MQ_EDMAC_U2_RX_TRHLD	0x00000020		// UART2 Receive DMA treshold (5bits)
#define MQ_EDMAC_U3_TX_TRHLD	0x00000040		// UART3 Transmit DMA treshold (5bits)
#define MQ_EDMAC_U3_RX_TRHLD	0x00000800		// UART3 Receive DMA treshold (5bits)
// EBIU EDMAC Tuned Parameters (TBD at runtime)
#if   defined (MQDBG_BOARD_A)
#define MQ_EDMAC_MODE3	(MQ_EDMAC_U2_TX_TRHLD | MQ_EDMAC_U2_RX_TRHLD |		\
						 MQ_EDMAC_U3_TX_TRHLD | MQ_EDMAC_U3_RX_TRHLD )
#else
#define MQ_EDMAC_MODE3	(MQ_EDMAC_U2_TX_TRHLD | MQ_EDMAC_U2_RX_TRHLD |		\
						 MQ_EDMAC_U3_TX_TRHLD | MQ_EDMAC_U3_RX_TRHLD )
#endif

// ---------------------------------------------
// I2C Control Register 
// ---------------------------------------------
#define MQ_I2C_INIT_STATE		0x11000011		// Bring-up state of I2C Master
#define MQ_I2C_READ_DONE		0x00000800		// Check I2C Master read shift done
#define MQ_I2C_WRITE_DONE		0x00000020		// Check I2C Master write shift done

// ---------------------------------------------
// RTC Timer Registers 
// ---------------------------------------------
#define MQ_RTC_PTM_SOSC			0x00000000		// Periodic Timer clock = slow osc.(32KHz)	
#define MQ_RTC_PTM_FOSC			0x00000002		// Periodic Timer clock = fast osc.
#define MQ_RTC_PTM_ENABLE		0x00000004		// Periodic Timer enabled
#define MQ_RTC_RTM_ENABLE		0x00000008		// Real Time Clock enabled
#define MQ_RTC_WTM_ENABLE		0x00000010		// Watchdog Timer enabled
#define MQ_RTC_WTM_SOSC			0x00000000		// Watchdog Timer clock = slow osc.(32KHz)
#define MQ_RTC_WTM_FOSC			0x00000020		// Watchdog Timer clock = fast osc.
#define MQ_RTC_HEX_FORMAT		0x01000000		// Real Time Clock in Hex format
#define MQ_RTC_GMT_TIME			0x02000000		// Military(24:00) vs GMT (12:00)time
#define MQ_RTC_DAYOFWEEK_CMP	0x04000000		// Alarm compare in day of week format
#define MQ_RTC_DAYOFMONTH_CMP   0x00000000		// Alarm compare in day of month format
#define MQ_RTC_DLS_ENABLE		0x08000000		// Day light saving time enabled
#define MQ_RTC_EU_DLS			0x10000000		// European Day Light Saving Time
#define MQ_RTC_US_DLS		(~MQ_RTC_EU_DLS)	// US Day Light Saving Time
#define MQ_RTC_PTM_INT	   		0x00000001		// Periodic Timer interrupt enable
#define MQ_RTC_RTM_INT			0x00000002		// Real Time Clock interrupt enable
#define MQ_RTC_WTM_INT			0x00000004		// Watchdog timer generate interrupt
#define MQ_RTC_WTM_RESET 	(~MQ_RTC_WTM_INT) 	// Watchdog timer generate system reset
#define MQ_RTC_TIME_INT			0x00000008		// Time alarm enabled
#define MQ_RTC_DAY_INT			0x00000010		// Day alarm enabled
#define MQ_RTC_DATE_INT			0x00000020		// Date alarm enabled
#define MQ_RTC_DATETIME_INT		0x00000040		// Date & Time alarm enabled
#define MQ_RTC_ALARM_INT	(MQ_RTC_TIME_INT|MQ_RTC_DAY_INT|MQ_RTC_DATE_INT|MQ_RTC_DATETIME_INT)
#define MQ_RTC_L1_RATIO			0x0000FF00		// L1 clock divisor (ratio)
#define MQ_RTC_YEAR_SEC			0x01E13380		// Num of secs in a year
#define MQ_RTC_LEAPYEAR_SEC		0x01E28500		// Num of secs in a leap year
#define MQ_RTC_MONTH_SEC		0x002819A0		// Avg. num of secs in a month (12 ms/year)
#define MQ_RTC_DAY_SEC			0x00015180		// Num of secs in a day
#define MQ_RTC_HOUR_SEC			0x00000E10		// Num of secs in an hour
#define MQ_RTC_MINUTE_SEC		0x0000003C		// Num of secs in a minute

// ---------------------------------------------
// TIMER/GPIOs Registers 
// ---------------------------------------------
#define MQ_TIMER_PLL1_CLK		0x00000000	// clock feeds PLL1
#define MQ_TIMER_PLL2_CLK		0x00000001	// clock feeds PLL2
#define MQ_TIMER_OSC_CLK		0x00000002	// clock feeds osc
#define MQ_TIMER0_MODE_MASK		0x00000700	// GPIO61 mode mask
#define MQ_TIMER0_SELF_MODE		0x00000300	// GPIO61 mode
#define MQ_TIMER1_MODE_MASK		0x0001c000	// GPIO62 mode mask
#define MQ_TIMER1_SELF_MODE		0x0000c000	// GPIO62 mode
//#define MQ_TIMER_DIVIDER1		0x00000078	// clock devider (1/16)
//#define MQ_TIMER_DIVIDER1		0x00000008	// clock devider (1/2)
#define MQ_TIMER_DIVIDER1		0x00000038	// clock devider (1/8)
#define MQ_TIMER_DIVIDER2		0x00000300	// clock devider (1/4)


/*******************************************************************************
 * MQ9000 Interrupt Bits Definitions
 ******************************************************************************/
#define IRQ_GLOBAL	   		   BIT0
#define FIQ_GLOBAL			   BIT1
// IRQ enable, mask, and status bits 
#define IRQ_GC1VS_RISE         BIT0			// GC1 VSync Rising Edge
#define IRQ_GC1VS_FALL         BIT1			// GC1 VSync Falling Edge
#define IRQ_GC1VD_RISE         BIT2			// GC1 VDisplay Rising Edge
#define IRQ_GC1VD_FALL         BIT3			// GC1 VDisplay Falling Edge
#define IRQ_CMDFIFO_HALF       BIT4			// CMD FIFO Half
#define IRQ_CMDFIFO_EMTY       BIT5			// CMD FIFO Empty
#define IRQ_SRCFIFO_HALF       BIT6			// SRC FIFO Half
#define IRQ_SRCFIFO_EMTY       BIT7			// SRC FIFO Empty
#define IRQ_GE_IDLE            BIT8			// GE Idle
#define IRQ_SBLT               BIT9			// GE Stretch Blt
#define IRQ_VI                 BIT10		// Video Input
#define IRQ_SD                 BIT11		// Secure Digital
#define IRQ_I2C	               BIT12		// I2C / SSPBus
#define IRQ_TIMER              BIT13		// Timer
#define IRQ_SSC                BIT14		// Audio Codec
#define IRQ_DMAC               BIT15		// DMA Controller
#define IRQ_SPI                BIT16		// Serial Peripheral Interface
#define IRQ_RTC                BIT17		// Real Time Clock	
#define IRQ_KB                 BIT18		// Matrix Keyboard
#define IRQ_UART1              BIT19		// UART 1 (IRDA)
#define IRQ_UART2              BIT20		// UART 2 (Baseband)
#define IRQ_UART3              BIT21		// UART 3 (Bluetooth)
#define IRQ_EDEV0              BIT22		// External Device 0
#define IRQ_EDEV1              BIT23		// External Device 1
#define IRQ_USBD               BIT24		// USB Device
#define IRQ_USBD_WAKE          BIT25		// USB Device Wake-up
#define IRQ_PWM                BIT26		// Pulse Width Modulation
#define IRQ_NAND               BIT27		// NAND Flash
#define IRQ_ALL                0xFFFFFFF
// FIQ enable, mask, and status bits (same as IRQ bits)
#define FIQ_GC1VS_RISE         BIT0			// GC1 VSync Rising Edge
#define FIQ_GC1VS_FALL         BIT1			// GC1 VSync Falling Edge
#define FIQ_GC1VD_RISE         BIT2			// GC1 VDisplay Rising Edge
#define FIQ_GC1VD_FALL         BIT3			// GC1 VDisplay Falling Edge
#define FIQ_CMDFIFO_HALF       BIT4			// CMD FIFO Half
#define FIQ_CMDFIFO_EMTY       BIT5			// CMD FIFO Empty
#define FIQ_SRCFIFO_HALF       BIT6			// SRC FIFO Half
#define FIQ_SRCFIFO_EMTY       BIT7			// SRC FIFO Empty
#define FIQ_GE_IDLE            BIT8			// GE Idle
#define FIQ_SBLT               BIT9			// GE Stretch Blt
#define FIQ_VI                 BIT10		// Video Input
#define FIQ_SD                 BIT11		// Secure Digital
#define FIQ_I2C	               BIT12		// I2C / SSPBus
#define FIQ_TIMER              BIT13		// Timer
#define FIQ_SSC                BIT14		// Audio Codec
#define FIQ_DMAC               BIT15		// DMA Controller
#define FIQ_SPI                BIT16		// Serial Peripheral Interface
#define FIQ_RTC                BIT17		// Real Time Clock	
#define FIQ_KB                 BIT18		// Matrix Keyboard
#define FIQ_UART1              BIT19		// UART 1 (IRDA)
#define FIQ_UART2              BIT20		// UART 2 (Baseband)
#define FIQ_UART3              BIT21		// UART 3 (Bluetooth)
#define FIQ_EDEV0              BIT22		// External Device 0
#define FIQ_EDEV1              BIT23		// External Device 1
#define FIQ_USBD               BIT24		// USB Device
#define FIQ_USBD_WAKE          BIT25		// USB Device Wake-up
#define FIQ_PWM                BIT26		// Pulse Width Modulation
#define FIQ_NAND               BIT27		// NAND Flash
#define FIQ_ALL                0xFFFFFFF

#endif	//_MQ9000SYS_H_
