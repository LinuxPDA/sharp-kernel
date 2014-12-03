/*
 *  This file contains the hardware definitions of Iris-1 Target
 *
 */

#ifndef __ASM_ARCH_HARDWARE_IRIS_H
#define __ASM_ARCH_HARDWARE_IRIS_H

#define DEBUG_UART_START	0x19000000	/* physical	*/
#define DEBUG_UART_BASE 	0xd9000000	/* virtual	*/
#define DEBUG_UART_SIZE 	0x00100000	/* 1 MB	*/

#define FPGA_START		0x1c000000	/* physical	*/
#define FPGA_BASE		0xdc000000	/* virtual	*/
#define FPGA_SIZE		0x01000000	/* 16MB	*/

#define IO_IOCS0_START          0x24000000       /* physical */
#define IO_IOCS0_BASE           0xCC000000       /* virtual */
#define IO_IOCS0_SIZE           0x04000000       /* size */

/* ========== added for IOCS1 ========== */
#define FLASH2_START	0x10000000      /* FLASH BANK 2 - phys */
#define FLASH2_BASE	0xd4000000	/* virt */
#define FLASH2_SIZE 	0x01000000	/* 16 MB */

/*
 *  Flash ROM Settings
 */
/* flash from 0x0 - 0x3fffff */
#define HAS_IOCS0_FLASH_4MB_0x0000000
/* flash from 0x400000 - 0x6fffff */ 
#undef  HAS_IOCS0_FLASH_4MB_0x0400000
/* flash from 0x1000000 - 0x13fffff */
#undef  HAS_IOCS1_FLASH_4MB_0x0000000
/* concat flashes , 0x400000 - 0x6fffff and 0x1000000 - 0x13fffff */
#define HAS_CONCAT_FLASH_4MB0x04_4MB0x10

#ifdef HAS_CONCAT_FLASH_4MB0x04_4MB0x10
#if defined(HAS_IOCS0_FLASH_4MB_0x0400000) || defined(HAS_IOCS1_FLASH_4MB_0x0000000)
#error "should not use concat flash and dependent flash"
#endif
#endif

#define HAS_IOCS0_FLASH_4MB_0x0000000_BASE  (IO_IOCS0_BASE+0x0)       /* must be virtual */
#define HAS_IOCS0_FLASH_4MB_0x0000000_SIZE  0x3fc000
#define HAS_IOCS0_FLASH_4MB_0x0400000_BASE  (IO_IOCS0_BASE+0x400000)  /* must be virtual */
#define HAS_IOCS0_FLASH_4MB_0x0400000_SIZE  0x3fc000
#define HAS_IOCS1_FLASH_4MB_0x0000000_BASE  (IO_IOCS_BASE+0x0)        /* must be virtual */
#define HAS_IOCS1_FLASH_4MB_0x0000000_SIZE  0x3fc000

#define AUX_REG_BASE AUX_BASE

/*
 *  Debug LED Port ( for Iris-1 Target Only )
 */
/* #define IRIS1_DEBUG_LED   0x1a000000 *//* unnecessary to use. defined 'hardware.h' as 'AUX_REG_' */

/*
 *  FPGA Interfaces (followed to FPGA-SPEC draft No.07 , 2001/01/22)
 */
#define IRIS_FPGA_START 0x1c000000 /* physical address */

/*
 * FPGA registers (16bit mode only)
 */
#define FPGA_OFFSET_CTRL      0x00
#define FPGA_OFFSET_XS        0x02
#define FPGA_OFFSET_YS        0x04
#define FPGA_OFFSET_XW        0x06
#define FPGA_OFFSET_YW        0x08
#define FPGA_OFFSET_EEN       0x0a
#define FPGA_OFFSET_ESEL      0x0c
#define FPGA_OFFSET_DE        0x0e
#define FPGA_OFFSET_LSEL      0x10
#define FPGA_OFFSET_ECL       0x12
#define FPGA_OFFSET_Enable    0x14
#define FPGA_OFFSET_EnSet     0x16
#define FPGA_OFFSET_EnReset   0x18
#define FPGA_OFFSET_RawStat   0x1a
#define FPGA_OFFSET_Status    0x1c
#define FPGA_OFFSET_OutLevel  0x1e
#define FPGA_OFFSET_CLEX      0x20
#define FPGA_OFFSET_ADICLR    0x28
#define FPGA_OFFSET_ADIEN     0x2a
#define FPGA_OFFSET_ADINT     0x2c
#define FPGA_OFFSET_PANEL     0x2e
#define FPGA_OFFSET_GPO       0x30

/* ==================================================
 *           Iris1 Specific Definitions 
 * ================================================== */

/*
 *  Debug LED Port ( for Iris-1 Target Only )
 */
#define IO_IRIS1_DEBUG_LED   (*((volatile unsigned char*)AUX_REG_BASE))

/*
 *  FPGA Port ( for Iris-1 Target Only )
 */
#define __FPGA_IOH(x)	  (*(volatile unsigned short*)(FPGA_BASE + (x)))

#define IO_FPGA_CTRL      __FPGA_IOH(FPGA_OFFSET_CTRL)
#define IO_FPGA_XS        __FPGA_IOH(FPGA_OFFSET_XS)
#define IO_FPGA_YS        __FPGA_IOH(FPGA_OFFSET_YS)
#define IO_FPGA_XW        __FPGA_IOH(FPGA_OFFSET_XW)
#define IO_FPGA_YW        __FPGA_IOH(FPGA_OFFSET_YW)
#define IO_FPGA_EEN       __FPGA_IOH(FPGA_OFFSET_EEN)
#define IO_FPGA_ESEL      __FPGA_IOH(FPGA_OFFSET_ESEL)
#define IO_FPGA_DE        __FPGA_IOH(FPGA_OFFSET_DE)
#define IO_FPGA_LSEL      __FPGA_IOH(FPGA_OFFSET_LSEL)
#define IO_FPGA_ECL       __FPGA_IOH(FPGA_OFFSET_ECL)
#define IO_FPGA_Enable    __FPGA_IOH(FPGA_OFFSET_Enable)
#define IO_FPGA_EnSet     __FPGA_IOH(FPGA_OFFSET_EnSet)
#define IO_FPGA_EnReset   __FPGA_IOH(FPGA_OFFSET_EnReset)
#define IO_FPGA_RawStat   __FPGA_IOH(FPGA_OFFSET_RawStat)
#define IO_FPGA_Status    __FPGA_IOH(FPGA_OFFSET_Status)
#define IO_FPGA_OutLevel  __FPGA_IOH(FPGA_OFFSET_OutLevel)
#define IO_FPGA_CLEX      __FPGA_IOH(FPGA_OFFSET_CLEX)
#define IO_FPGA_ADICLR    __FPGA_IOH(FPGA_OFFSET_ADICLR)
#define IO_FPGA_ADIEN     __FPGA_IOH(FPGA_OFFSET_ADIEN)
#define IO_FPGA_ADINT     __FPGA_IOH(FPGA_OFFSET_ADINT)
#define IO_FPGA_PANEL     __FPGA_IOH(FPGA_OFFSET_PANEL)
#define IO_FPGA_GPO       __FPGA_IOH(FPGA_OFFSET_GPO)

/*
 *  DC/DC
 */
#define PMPCON		0x90006000	/* DC Converter Control Reg	*/

#define IO_PMPCON	__IOL(PMPCON)

#endif
