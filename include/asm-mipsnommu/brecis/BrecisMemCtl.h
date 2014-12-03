/*==========================================================================+
|                                                                           |
|   Title:   MemCtl.h                                                       |
|                                                                           |
|   Author:  Michael Ngo                                                    |
|                                                                           |
|   Date:    09/10/00.                                                      |
|                                                                           |
+===========================================================================+
|   $Header:
+===========================================================================+
|                                                                           |
|   Description:                                                            |
|     This file defines Triad architecture specific constants.              |
|     									    |
|                                                                           |
|   Special Notes:                                                          |
|                                                                           |
|                                                                           |
+==========================================================================*/

#ifndef _BRECIS_MEM_CTL_H
#define _BRECIS_MEM_CTL_H

/*==========================================================================+
|  Memory Controller.
+==========================================================================*/
#define	MEM_CTL_BASE	0xB7F00000	/* Base address to memory contlr   */

#define M8MEG		0x800000
#define M16MEG		0x1000000
#define M32MEG		0x2000000
#define M64MEG		0x4000000
#define M128MEG		0x8000000
#define M256MEG		0x10000000

#ifdef __ASSEMBLER__
#define MEM_CNFG1_REG	(MEM_CTL_BASE + 0x00)
#define MEM_CNFG2_REG	(MEM_CTL_BASE + 0x04)
#define MEM_CNFG3_REG	(MEM_CTL_BASE + 0x08)
#define MEM_CNFG4_REG	(MEM_CTL_BASE + 0x0C)
#define MEM_CNFG5_REG	(MEM_CTL_BASE + 0x10)
#else
#define MEM_CNFG1_REG	(volatile unsigned int *) (MEM_CTL_BASE + 0x00)
#define MEM_CNFG2_REG	(volatile unsigned int *) (MEM_CTL_BASE + 0x04)
#define MEM_CNFG3_REG	(volatile unsigned int *) (MEM_CTL_BASE + 0x08)
#define MEM_CNFG4_REG	(volatile unsigned int *) (MEM_CTL_BASE + 0x0C)
#define MEM_CNFG5_REG	(volatile unsigned int *) (MEM_CTL_BASE + 0x10)
#endif


/*--------------------------------------------------------------------------+
|
| Memory Configuration Register 1:  Bit Definition
| --------------------------------
|
| bit  0           =    Numbanks          0 = 2  banks(only for 16Mb)
|                                         1 = 4  banks(64Mb or higher
|
| bits [2:1]       =    Buswidth         00 = 4  bits wide
|                                        01 = 8  bits wide
|                                        10 = 16 bits wide
|                                        11 = 32 bits wide
|
| bits [5:3]       =    BurstLength     010 = 4
|                                       011 = 8
|                                       111 = page mode
|
| bit  [6]         =    Burst type      0 Only support Seq
|
| bits [9:7]       =    Read latency    010 CAS latency 2
|                                       011 CAS latency 3
|
| bit  11          =    Burst aligned   Burst aligned accesse. If this bit
|                                       set 1, then all the accessed o the
|                                       memory are always burst aligned.
|                                       Auto precharge will be used in this
|                                       Mode
| bit 12           = bus redirection    0 = Bus redirection time = 1 cycle
|                                       1 = Bus redirection time = 2 cycle
| bit [14:13]      = sdram Size        00 = 64Mb or 16Mb Sdram
|                                      01 = 128Mb
|                                      10 = 256Mb
| bit [31:15,10]                       Reserved
|
+--------------------------------------------------------------------------*/

#define MEM_CNTL_2BANKS             0x0
#define MEM_CNTL_4BANKS             0x1

#define MEM_CNTL_BUSW_4B            0x0
#define MEM_CNTL_BUSW_8B            (0x1  << 1)
#define MEM_CNTL_BUSW_16B           (0x2  << 1)
#define MEM_CNTL_BUSW_32B           (0x11 << 1)
 
#define MEM_CNTL_BURST_LEN4         (0x2 << 3)
#define MEM_CNTL_BURST_LEN8         (0x3 << 3)
#define MEM_CNTL_BURST_LEN_PAGE     (0x7 << 3)

#define MEM_CNTL_BURST_TYPE         (0x0 << 6)

#define MEM_CNTL_READ_LATENCY2      (0x2 << 7)
#define MEM_CNTL_READ_LATENCY3      (0x3 << 7)

#define MEM_CNTL_BURST_ALIGNED      (0x1 << 11)
#define MEM_CNTL_BURST_NO_ALIGNED   (0x0 << 11)

#define MEM_CNTL_BUS_DIR_1CYLCE     (0x0 << 12)
#define MEM_CNTL_BUS_DIR_2CYLCE     (0x1 << 12)

#define MEM_CNTL_SDRAM_SIZE_64MB    (0x0 << 13)
#define MEM_CNTL_SDRAM_SIZE_128MB   (0x1 << 13)
#define MEM_CNTL_SDRAM_SIZE_256MB   (0x2 << 13)   

/*--------------------------------------------------------------------------+
| The Following are the sequence to detect COL bits of Memory
+--------------------------------------------------------------------------*/
#define M256Mb64M4_REG1		0x00005199
#define M256Mb64M4_REG2		0x00002000

#define M256Mb32M8_REG1		0x0000519B
#define M256Mb32M8_REG2		0x00002000

#define M256Mb16M16_REG1	0x0000519D
#define M256Mb16M16_REG2	0x00002000

#define M128Mb32M4_REG1		0x00003199
#define M128Mb32M4_REG2		0x00001000

#define M128Mb16M8_REG1		0x0000319B
#define M128Mb16M8_REG2		0x00001000

#define M128Mb8M16_REG1		0x0000319D
#define M128Mb8M16_REG2		0x00001000
#define M64Mb16M4_REG1		0x00001199
#define M64Mb16M4_REG2		0x00001000

#define M64Mb8M8_REG1		0x0000119b
#define M64Mb8M8_REG2		0x00001000

#define M64Mb4M16_REG1		0x0000119d
#define M64Mb4M16_REG2		0x00001000
 
#define M64Mb2M32_REG1		0x0000119E
#define M64Mb2M32_REG2		0x00001000

#define M16Mb4M4_REG1		0x00001198
#define M16Mb4M4_REG2		0x00000800

#define M16Mb2M8_REG1		0x0000119a
#define M16Mb2M8_REG2		0x00000800

#define M16Mb1M16_REG1		0x0000119c
#define M16Mb1M16_REG2		0x00000800

#endif /* !(_BRECIS_MEM_CTL_H) */
