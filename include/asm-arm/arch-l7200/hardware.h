/*
 * linux/include/asm-arm/arch-l7200/hardware.h
 *
 * Copyright (C) 2000 Rob Scott (rscott@mtrob.fdns.net)
 *                    Steve Hill (sjhill@cotw.com)
 *
 * This file contains the hardware definitions for the 
 * LinkUp Systems L7200 SOC development board.
 *
 * Changelog:
 *   02-01-2000	 RS	Created L7200 version, derived from rpc code
 *   03-21-2000	SJH	Cleaned up file
 *   04-21-2000	 RS 	Changed mapping of I/O in virtual space
 *   04-25-2000	SJH	Removed unused symbols and such
 *   05-05-2000	SJH	Complete rewrite
 *   07-31-2000	SJH	Added undocumented debug auxillary port to
 *			get at last two columns for keyboard driver
 *   11-12-2001 Lineo Japan, Inc.			
 */
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#ifndef __ASSEMBLY__
#include <asm/types.h>
#endif

/* Hardware addresses of major areas.
 *  *_START is the physical address
 *  *_SIZE  is the size of the region
 *  *_BASE  is the virtual address
 */
#define RAM_START	0xf0000000	/* RAM - phys */
#define RAM_BASE	0xc0000000	/* virt */
#define RAM_SIZE	0x02000000	/* 32 MB */

#define IO_START	0x80000000      /* I/O - phys */
#define IO_BASE		0xd0000000	/* virt */
#define IO_SIZE		0x00100000	/* 1 MB */

#define IO_START_2	0x90000000      /* I/O - phys */
#define IO_BASE_2	0xd1000000	/* virt */
#define IO_SIZE_2	0x00100000	/* 1 MB */

#define AUX_START	0x1a000000      /* AUX PORT - phys */
#define AUX_BASE	0xd2000000	/* virt */
#define AUX_SIZE 	0x00100000	/* 1 MB */

#define FLASH1_START	0x00000000      /* FLASH BANK 1 - phys */
#define FLASH1_BASE	0xd3000000	/* virt */
#define FLASH1_SIZE 	0x01000000	/* 16 MB */

#define FLASH2_START	0x10000000      /* FLASH BANK 2 - phys */
#define FLASH2_BASE	0xd4000000	/* virt */
#define FLASH2_SIZE 	0x01000000	/* 16 MB */

#define SRAM_START      0x60000000      /* Internal SRAM - phys */
#define SRAM_BASE       0xd7000000      /* virt */
#define SRAM_SIZE       0x00100000      /* 1 MB */

#define IO_START_3	0x88000000      /* I/O - phys */
#define IO_BASE_3	0xd8000000	/* virt */
#define IO_SIZE_3 	0x00010000	/* 1 MB */

#define EIO_START	0x30000000	/* ethernet - phys */
#define EIO_BASE	0xf0000000	/* virt */
#define EIO_SIZE	0x00100000	/* 1 MB */

#define SRAM_START    0x60000000      /* Internal SRAM - phys */
#define SRAM_BASE     0xd7000000      /* virt */
#define SRAM_SIZE     0x00100000      /* 1 MB */

#define SDRAM_START   0xd0000000        /* SDRAM register - phys */
#define SDRAM_BASE    0xda000000        /* virt */
#define SDRAM_SIZE    0x00100000        /* 1 MB */

#define PCIO_BASE	IO_BASE

#if 0
#define __IOB(x) ( (x) < IO_START_3 ?  \
	( (x) < IO_START_2 ?  \
	(*(volatile unsigned char*) (IO_BASE + (x) - IO_START)) : \
	(*(volatile unsigned char*) (IO_BASE_2 + (x) - IO_START_2)) ) : \
	(*(volatile unsigned char*) (IO_BASE_3 + (x) - IO_START_3)) )

#define __IOH(x) ( (x) < IO_START_3 ?  \
	( (x) < IO_START_2 ?  \
	(*(volatile unsigned short*) (IO_BASE + (x) - IO_START)) : \
	(*(volatile unsigned short*) (IO_BASE_2 + (x) - IO_START_2)) ) : \
	(*(volatile unsigned short*) (IO_BASE_3 + (x) - IO_START_3)) )
#define __IOL(x) ( (x) < IO_START_3 ?  \
	( (x) < IO_START_2 ?  \
	(*(volatile unsigned long*) (IO_BASE + (x) - IO_START)) : \
	(*(volatile unsigned long*) (IO_BASE_2 + (x) - IO_START_2)) ) : \
	(*(volatile unsigned long*) (IO_BASE_3 + (x) - IO_START_3)) )
#else
#define __IOB(x) ( (x) < IO_START_2 ?  \
	(*(volatile unsigned char*) (IO_BASE + (x) - IO_START)) : \
	(*(volatile unsigned char*) (IO_BASE_2 + (x) - IO_START_2)) )

#define __IOH(x) ( (x) < IO_START_2 ?  \
	(*(volatile unsigned short*) (IO_BASE + (x) - IO_START)) : \
	(*(volatile unsigned short*) (IO_BASE_2 + (x) - IO_START_2)) )
#define __IOL(x) ( (x) < IO_START_2 ?  \
	(*(volatile unsigned long*) (IO_BASE + (x) - IO_START)) : \
	(*(volatile unsigned long*) (IO_BASE_2 + (x) - IO_START_2)) )
#endif

/*
 * GPIO DMA
 */
#define GPIODMASTAT	0x80010000  /* Status Register */
#define GPIODMACNTL	0x80010004  /* Control Register */
#define GPIODMABPD	0x80010008  /* Byte Packer Data Register */
#define GPIODMAFIFO	0x80010010  /* Data Port */

/*
 * Memory To Memory DMA
 */
#define M2MSTAT		0x80030000   /* status register */
#define M2MCTL		0x80030004   /* control register */
#define M2MCNT		0x80030008   /* transfer count */
#define M2MDATA		0x80030010   /* data port register */

/*
 * Color LCD
 */
#define CLCDCON		0x80049000	/* CLCD Control Register	*/
#define CLCDFLAG	0x80049004	/* CLCD Status Register         */
#define CLCDMASK	0x80049008	/* CLCD Status Mask  Register	*/
#define CLCDINTR	0x8004900c	/* CLCD Interrupt Register	*/
#define CLCDBAR0	0x80049010	/* DMA CH0 base addr reg        */
#define CLCDCAR0	0x80049014	/* DMA CH0 current base addr reg */
#define CLCDBAR1	0x80049018	/* DMA CH1 base addr reg        */
#define CLCDCAR1	0x8004901c	/* DMA CH1 current base addr reg */
#define CLCDTIMING0	0x80049020	/* CLCD Timing Register 0	*/
#define CLCDTIMING1	0x80049024	/* CLCD Timing Register 1	*/
#define CLCDTIMING2	0x80049028	/* CLCD Timing Register 2	*/
#define CLCDTIMING3	0x8004902c	/* CLCD Timing Register 3	*/
#define CLCDTEST	0x80049040	/* CLCD Test Mode  		*/
#define CLCDPALETTE	0x80049400	/* CLCD 256 Palette Entries	*/

#define CLCDEN		0x01		/* Enable CCD Controller	*/
#define CLCDPWR		0x400000	/* CLCD Power Enable		*/
#define CLCDSRAM_EN     0x4000000       /* Frame Buffer Location (SRAM) : L7210 specific */

#define	IO_CLCDCON	__IOL(CLCDCON)
#define	IO_CLCDFLAG	__IOL(CLCDFLAG)
#define	IO_CLCDMASK	__IOL(CLCDMASK)
#define	IO_CLCDINTR	__IOL(CLCDINTR)
#define	IO_CLCDBAR0	__IOL(CLCDBAR0)
#define	IO_CLCDCAR0	__IOL(CLCDCAR0)
#define	IO_CLCDBAR1	__IOL(CLCDBAR1)
#define	IO_CLCDCAR1	__IOL(CLCDCAR1)
#define	IO_CLCDTIMING0	__IOL(CLCDTIMING0)
#define	IO_CLCDTIMING1	__IOL(CLCDTIMING1)
#define	IO_CLCDTIMING2	__IOL(CLCDTIMING2)
#define	IO_CLCDTIMING3	__IOL(CLCDTIMING3)
#define	IO_CLCDTEST	__IOL(CLCDTEST)
#define	IO_CLCDPALETTE	__IOL(CLCDPALETTE)

/* UART1/UART2 */
#define UART1_REG_BASE 	0x80044000 /* UART1 register reg base addr */
#define UART2_REG_BASE 	0x80045000 /* UART2 register reg base addr */

#include <asm/arch/serial_l7200.h>

#define	IO_UARTDR	__IOB(UART1_REG_BASE+UARTDR)
#define	IO_RXSTAT	__IOB(UART1_REG_BASE+RXSTAT)
#define	IO_H_UBRLCR	__IOB(UART1_REG_BASE+H_UBRLCR)
#define	IO_M_UBRLCR	__IOB(UART1_REG_BASE+M_UBRLCR)
#define	IO_L_UBRLCR	__IOB(UART1_REG_BASE+L_UBRLCR)
#define	IO_UARTCON	__IOB(UART1_REG_BASE+UARTCON)
#define	IO_UARTFLG	__IOB(UART1_REG_BASE+UARTFLG)
#define	IO_UARTINTSTAT	__IOB(UART1_REG_BASE+UARTINTSTAT)
#define	IO_UARTINTMASK	__IOB(UART1_REG_BASE+UARTINTMASK)

#define	IO_UARTDR2	__IOB(UART2_REG_BASE+UARTDR)
#define	IO_RXSTAT2	__IOB(UART2_REG_BASE+RXSTAT)
#define	IO_H_UBRLCR2	__IOB(UART2_REG_BASE+H_UBRLCR)
#define	IO_M_UBRLCR2	__IOB(UART2_REG_BASE+M_UBRLCR)
#define	IO_L_UBRLCR2	__IOB(UART2_REG_BASE+L_UBRLCR)
#define	IO_UARTCON2	__IOB(UART2_REG_BASE+UARTCON)
#define	IO_UARTFLG2	__IOB(UART2_REG_BASE+UARTFLG)
#define	IO_UARTINTSTAT2	__IOB(UART2_REG_BASE+UARTINTSTAT)
#define	IO_UARTINTMASK2	__IOB(UART2_REG_BASE+UARTINTMASK)

/* 
 * Serial Interface Bus(SIB)
 */
#define MCCR		0x80040000	/* SIB Control Register */
#define MCDR0		0x80040008	/* SIB Data Register 0  */
#define MCDR1		0x8004000C	/* SIB Data Register 1  */
#define MCDR2		0x80040010	/* SIB Data Register 2  */
#define MCSR		0x80040018	/* SIB Status Register  */

#define IO_MCCR		__IOL(MCCR)
#define IO_MCDR0	__IOL(MCDR0)
#define IO_MCDR1	__IOL(MCDR1)
#define IO_MCDR2	__IOL(MCDR2)
#define IO_MCSR		__IOL(MCSR)

/*
 * System control
 */
#define SYS_CONFIG_CURRENT      0x80050000 /* Current Configuration Register */
#define SYS_CONFIG_NEXT         0x80050004 /* Next Configuration Register */
#define SYS_CONFIG_RUN          0x8005000c /* Run Configuration Register */
#define SYS_CONFIG_COMM         0x80050010 /* Configuration Command Register */
#define SYS_CONFIG_SDRAM        0x80050014 /* SDRAM Config Bypass Register */

#define IO_SYS_CONFIG_CURRENT   __IOL(SYS_CONFIG_CURRENT)
#define IO_SYS_CONFIG_NEXT      __IOL(SYS_CONFIG_NEXT)
#define IO_SYS_CONFIG_RUN       __IOL(SYS_CONFIG_RUN)
#define IO_SYS_CONFIG_COMM      __IOL(SYS_CONFIG_COMM)
#define IO_SYS_CONFIG_SDRAM     __IOL(SYS_CONFIG_SDRAM)

#define SYS_CLOCK_ENABLE	0x80050030 /* Clock Enable Register */
#define SYS_CLOCK_ESYNC		0x80050034 /* Clock Enable Sync Register */
#define SYS_CLOCK_SELECT	0x80050038 /* Clock Mux and Divider Ctrl Reg */

#define IO_SYS_CLOCK_ENABLE	__IOL(SYS_CLOCK_ENABLE)
#define IO_SYS_CLOCK_ESYNC	__IOL(SYS_CLOCK_ESYNC)
#define IO_SYS_CLOCK_SELECT	__IOL(SYS_CLOCK_SELECT)


#define SYS_CONFIG_CURRENT      0x80050000      /* Current Configuration Register */
#define SYS_CONFIG_NEXT         0x80050004      /* Next Configuration Register */
#define SYS_CONFIG_RUN          0x8005000c      /* Run Configuration Register */
#define SYS_CONFIG_COMM         0x80050010      /* Configuration Command Register */
#define SYS_CONFIG_SDRAM        0x80050014      /* SDRAM Configuration Bypass Register */

#define IO_SYS_CONFIG_CURRENT   __IOL(SYS_CONFIG_CURRENT)   /* Current Configuration Register */
#define IO_SYS_CONFIG_NEXT      __IOL(SYS_CONFIG_NEXT)      /* Next Configuration Register */
#define IO_SYS_CONFIG_RUN       __IOL(SYS_CONFIG_RUN)       /* Run Configuration Register */
#define IO_SYS_CONFIG_COMM      __IOL(SYS_CONFIG_COMM)      /* Configuration Command Register */
#define IO_SYS_CONFIG_SDRAM     __IOL(SYS_CONFIG_SDRAM)     /* SDRAM Configuration Bypass Register */

#define SYS_STATUS              0x80050040
#define SYS_STAT_CLR            0x80050044
#define SYS_RESET_REMAP         0x80050048
#define SYS_INT_CTLR            0x8005004c
#define SYS_ID                  0x80050050

#define IO_SYS_STATUS              __IOL(SYS_STATUS)
#define IO_SYS_STAT_CLR            __IOL(SYS_STAT_CLR)
#define IO_SYS_RESET_REMAP         __IOL(SYS_RESET_REMAP)
#define IO_SYS_INT_CTLR            __IOL(SYS_INT_CTLR)
#define IO_SYS_ID                  __IOL(SYS_ID)


/*
 * Synchronous Serial Port
 */
#define SSCR0		0x80041000      /* Control Register 0 */
#define SSCR1		0x80041004      /* Control Register 1 */
#define SSDR		0x80041008      /* Data Register */
#define SSSR		0x8004100c      /* Status Register */

#define IO_SSCR0	__IOL(SSCR0)
#define IO_SSCR1	__IOL(SSCR1)
#define IO_SSDR		__IOL(SSDR)
#define IO_SSSR		__IOL(SSSR)

/*
 * IrDA
 */
#define IRENABLE	0x80046000  	/* Sir/Mir/Fir Selector */
#define IRCON        	0x80046004  	/* Interface Control Register */
#define IRAMV        	0x80046008  	/* Address Match Value Register */
#define IRFLAG       	0x8004600c  	/* Flag Register */
#define IRDATA       	0x80046010  	/* Data FIFOs */
#define IRDATATAIL   	0x80046014  	/* Data Tail Register */
#define IRRIB        	0x80046020  	/* Receive Information Buffer */
#define MISR         	0x80046080  	/* Mir Status Register */
#define MIMR         	0x80046084  	/* Mir Mask Register */
#define MIIR         	0x80046088  	/* Mir Interrupt Register */
#define FISR         	0x80046180  	/* Fir Status Register */
#define FIMR         	0x80046184  	/* Fir Mask Register */
#define FIIR        	0x80046188  	/* Fir Interrupt Register */

#define IO_IRENABLE     __IOL(IRENABLE)
#define IO_IRCON        __IOL(IRCON)
#define IO_IRAMV        __IOL(IRAMV)
#define IO_IRFLAG       __IOL(IRFLAG)
#define IO_IRDATA       __IOL(IRDATA)
#define IO_IRDATATAIL   __IOL(IRDATATAIL)
#define IO_IRRIB        __IOL(IRRIB)
#define IO_MISR         __IOL(MISR)
#define IO_MIMR         __IOL(MIMR)
#define IO_MIIR         __IOL(MIIR)
#define IO_FISR         __IOL(FISR)
#define IO_FIMR         __IOL(FIMR)
#define IO_FIIR         __IOL(FIIR)

/* 
 * DMA Controller Software Interface 
 */
#define DMACTRL_BASE	0x80047000
#define	DMA_COMMON(x)	(DMACTRL_BASE+(x)) 
#define DMA_CHANNEL(n)	(DMACTRL_BASE+0x100+(n)*0x20)
#define DMA_EN		DMA_COMMON(0x000)
#define DMA_TERM	DMA_COMMON(0x004)
#define DMA_BZ		DMA_COMMON(0x008)
#define DMA_UR		DMA_COMMON(0x00c)
#define DMA_ISR		DMA_COMMON(0x010)
#define DMA_ICR		DMA_COMMON(0x014)
#define DMA_IER		DMA_COMMON(0x018)
#define DMA_SBR		DMA_COMMON(0x01c)
#define DMA_RCM1	DMA_COMMON(0x020)
#define DMA_RCM2	DMA_COMMON(0x024)
#define DMA_CxNBA(n)	(DMA_CHANNEL(n)+0x000)
#define DMA_CxNTC(n)	(DMA_CHANNEL(n)+0x004)
#define DMA_CxSBA(n)	(DMA_CHANNEL(n)+0x008)
#define DMA_CxSTC(n)	(DMA_CHANNEL(n)+0x00c)
#define DMA_CxCBA(n)	(DMA_CHANNEL(n)+0x010)
#define DMA_CxCTC(n)	(DMA_CHANNEL(n)+0x014)
#define DMA_CxPA(n)	(DMA_CHANNEL(n)+0x018)
#define DMA_CxCTL(n)	(DMA_CHANNEL(n)+0x01c)

#define IO_DMA_EN       __IOL(DMA_EN)
#define IO_DMA_TERM     __IOL(DMA_TERM)
#define IO_DMA_BZ       __IOL(DMA_BZ)
#define IO_DMA_UR       __IOL(DMA_UR)
#define IO_DMA_ISR      __IOL(DMA_ISR)
#define IO_DMA_ICR      __IOL(DMA_ICR)
#define IO_DMA_IER      __IOL(DMA_IER)
#define IO_DMA_SBR      __IOL(DMA_SBR)
#define IO_DMA_RCM1     __IOL(DMA_RCM1)
#define IO_DMA_RCM2     __IOL(DMA_RCM2)
#define IO_DMA_CxNBA(n) __IOL(DMA_CxNBA(n))
#define IO_DMA_CxNTC(n) __IOL(DMA_CxNTC(n))
#define IO_DMA_CxSBA(n) __IOL(DMA_CxSBA(n))
#define IO_DMA_CxSTC(n) __IOL(DMA_CxSTC(n))
#define IO_DMA_CxCBA(n) __IOL(DMA_CxCBA(n))
#define IO_DMA_CxCTC(n) __IOL(DMA_CxCTC(n))
#define IO_DMA_CxPA(n)  __IOL(DMA_CxPA(n))
#define IO_DMA_CxCTL(n) __IOL(DMA_CxCTL(n))

#define DMA_REQ_OFF				0x00
#define DMA_REQ_UCB1200_AUDIO_RECEIVE		0x01
#define DMA_REQ_UCB1200_AUDIO_TRANSMIT		0x02
#define DMA_REQ_UCB1200_TELECOM_RECEIVE		0x03
#define DMA_REQ_UCB1200_TELECOM_TRANSMIT	0x04
#define DMA_REQ_SSP1_RECEIVE			0x05
#define DMA_REQ_SSP1_TRANSMIT			0x06
#define DMA_REQ_USB_RECEIVE			0x07
#define DMA_REQ_USB_TRANSMIT			0x08
#define DMA_REQ_SPI_SLAVE_RECEIVE		0x09
#define DMA_REQ_SPI_SLAVE_TRANSMIT		0x0A
#define DMA_REQ_UART1_RECEIVE			0x0B
#define DMA_REQ_UART1_TRANSMIT			0x0C
#define DMA_REQ_UART2_RECEIVE			0x0D
#define DMA_REQ_UART2_TRANSMIT			0x0E
#define DMA_REQ_IRDA_RECEIVE			0x0F
#define DMA_REQ_IRDA_TRANSMIT			0x10
#define DMA_REQ_M2M_WRITE			0x11
#define DMA_REQ_M2M_READ			0x12
#define DMA_REQ_GPIO_RECEIVE			0x13
#define DMA_REQ_GPIO_TRANSMIT			0x14
#define DMA_REQ_MMC_RECEIVE			0x15
#define DMA_REQ_MMC_TRANSMIT			0x16
#define DMA_REQ_SIC_AUDIO_RECEIVE		0x17
#define DMA_REQ_SIC_AUDIO_TRANSMIT		0x18
#define DMA_REQ_SIC_MODEM_RECEIVE		0x19
#define DMA_REQ_SIC_MODEM_TRANSMIT		0x1A

#define DMA_CTRL_PW_8	0x01	/* Peripheral Width 8bit */
#define DMA_CTRL_PW_16 	0x02 	/* Peripheral Width 16bit */
#define DMA_CTRL_PW_32	0x03 	/* Peripheral Width 32bit */
#define DMA_CTRL_D	0x04 	/* Direction */
#define DMA_CTRL_BE	0x08 	/* Burst Enable */
#define DMA_CTRL_ARE	0x10 	/* Auto Reload Enable*/
#define DMA_CTRL_SU	0x20 	/* Stop on Underrun */
	
/* 
 *  MultiMediaCard Interface
 */
#define MMCCT   	0x8004a000      /* Control Register */
#define MMCTM   	0x8004a008      /* Timing Register */
#define MMCIS   	0x8004a00c      /* Interrupt and Status Register */
#define MMCIM   	0x8004a010      /* Interrupt Mask Register */
#define MMCIC   	0x8004a014      /* Interrupt Clear Register */
#define MMCCM0  	0x8004a018      /* command argument */
#define MMCCM1  	0x8004a01c      /* command index */
#define MMCRS0  	0x8004a020      /* response token [31:0] */
#define MMCRS1  	0x8004a024      /* response token [63:32] */
#define MMCRS2  	0x8004a028      /* response token [95:64] */
#define MMCRS3  	0x8004a02c      /* response token [127:96] */
#define MMCRS4  	0x8004a030      /* response token [133:96] */
#define MMCDT   	0x8004a034      /* Data Register */
#define MMCCR   	0x8004a038      /* CRC Read Register */
#define MMCDB   	0x8004a03c      /* Data Block Register */
#define MMCIRC  	0x8004a050      /* Internal Register Control Reg. */

#define IO_MMCCT    	__IOL(MMCCT)
#define IO_MMCTM   	__IOL(MMCTM)
#define IO_MMCIS    	__IOL(MMCIS)
#define IO_MMCIM    	__IOL(MMCIM)
#define IO_MMCIC    	__IOL(MMCIC)
#define IO_MMCCM0   	__IOL(MMCCM0)
#define IO_MMCCM1   	__IOL(MMCCM1)
#define IO_MMCRS0   	__IOL(MMCRS0)
#define IO_MMCRS1   	__IOL(MMCRS1)
#define IO_MMCRS2   	__IOL(MMCRS2)
#define IO_MMCRS3   	__IOL(MMCRS3)
#define IO_MMCRS4   	__IOL(MMCRS4)
#define IO_MMCDT    	__IOL(MMCDT)
#define IO_MMCCR    	__IOL(MMCCR)
#define IO_MMCDB    	__IOL(MMCDB)
#define IO_MMCIRC   	__IOL(MMCIRC)

/* 
 *  SD Interface
 */
#define SDCT   		0x8004d000      /* Control Register */
#define SDTM   		0x8004d008      /* Timing Register */
#define SDIS   		0x8004d00c      /* Interrupt and Status Register */
#define SDIM   		0x8004d010      /* Interrupt Mask Register */
#define SDIC   		0x8004d014      /* Interrupt Clear Register */
#define SDCM0  		0x8004d018      /* command argument */
#define SDCM1  		0x8004d01c      /* command index */
#define SDRS0  		0x8004d020      /* response token [31:0] */
#define SDRS1  		0x8004d024      /* response token [63:32] */
#define SDRS2  		0x8004d028      /* response token [95:64] */
#define SDRS3  		0x8004d02c      /* response token [127:96] */
#define SDRS4  		0x8004d030      /* response token [133:96] */
#define SDDT   		0x8004d034      /* Data Register */
#define SDCR   		0x8004d038      /* CRC Read Register */
#define SDDB   		0x8004d03c      /* Data Block Register */
#define SDCR1  		0x8004d040      /* Data Block Register */
#define SDCR2  		0x8004d044      /* Data Block Register */
#define SDIRC  		0x8004d050      /* Internal Register Control Reg. */

#define IO_SDCT    	__IOL(SDCT)
#define IO_SDTM   	__IOL(SDTM)
#define IO_SDIS    	__IOL(SDIS)
#define IO_SDIM    	__IOL(SDIM)
#define IO_SDIC    	__IOL(SDIC)
#define IO_SDCM0   	__IOL(SDCM0)
#define IO_SDCM1   	__IOL(SDCM1)
#define IO_SDRS0   	__IOL(SDRS0)
#define IO_SDRS1   	__IOL(SDRS1)
#define IO_SDRS2   	__IOL(SDRS2)
#define IO_SDRS3   	__IOL(SDRS3)
#define IO_SDRS4   	__IOL(SDRS4)
#define IO_SDDT    	__IOL(SDDT)
#define IO_SDCR    	__IOL(SDCR)
#define IO_SDDB    	__IOL(SDDB)
#define IO_SDCR1    	__IOL(SDCR1)
#define IO_SDCR2    	__IOL(SDCR2)
#define IO_SDIRC   	__IOL(SDIRC)

/*
 * USB Function Controller
 */
#define USBF_REVISION     	0x8004b000
#define USBF_CONTROL      	0x8004b004
#define USBF_STATUS       	0x8004b008
#define USBF_RAWSTATUS    	0x8004b00c
#define USBF_INTENA       	0x8004b010
#define USBF_INTDIS       	0x8004b014
#define USBF_INTCLR       	0x8004b018
#define USBF_CONFIGBUF1   	0x8004b020
#define USBF_ENDPTBUF0    	0x8004b024
#define USBF_ENDPTBUF1    	0x8004b028
#define USBF_ENDPTBUF2    	0x8004b02c
#define USBF_ENDPTBUF3    	0x8004b030
#define USBF_STRINGBUF0   	0x8004b034
#define USBF_STRINGBUF1   	0x8004b038
#define USBF_STRINGBUF2   	0x8004b03c
#define USBF_STRINGBUF3   	0x8004b040
#define USBF_STRINGBUF4   	0x8004b044
#define USBF_F0BCNT       	0x8004b048
#define USBF_F1BCNT       	0x8004b04c
#define USBF_F1TOUT       	0x8004b050
#define USBF_F2BCNT       	0x8004b054
#define USBF_F3BCNT       	0x8004b058
#define USBF_F2TAIL       	0x8004b05c
#define USBF_FIFO0        	0x8004b070
#define USBF_FIFO1        	0x8004b080
#define USBF_FIFO2        	0x8004b0a0
#define USBF_FIFO3        	0x8004b0c0
#define USBF_DESCRIPTORS  	0x8004b100

/*
 * USB Host Controller
 */
#define USBH_HC_REVISION          	0x88000000
#define USBH_HC_CONTROL             	0x88000004
#define USBH_HC_COMMAND_STATUS      	0x88000008
#define USBH_HC_INTERRUPT_STATUS    	0x8800000c
#define USBH_HC_INTERRUPT_ENABLE    	0x88000010
#define USBH_HC_INTERRUPT_DISABLE   	0x88000014
#define USBH_HC_HCCA                	0x88000018
#define USBH_HC_PERIOD_CURRENT_ED   	0x8800001c
#define USBH_HC_CONTROL_HEAD_ED     	0x88000020
#define USBH_HC_CONTROL_CURRENT_ED  	0x88000024
#define USBH_HC_BULK_HEAD_ED        	0x88000028
#define USBH_HC_BULK_CURRENT_ED     	0x8800002c
#define USBH_HC_DONE_HEAD           	0x88000030
#define USBH_HC_FM_INTERVAL         	0x88000034
#define USBH_HC_FM_REMAINING        	0x88000038
#define USBH_HC_FM_NUMBER           	0x8800003c
#define USBH_HC_PERIODIC_START      	0x88000040
#define USBH_HC_LS_THRESHOLD        	0x88000044
#define USBH_HC_RH_DESCRIPTOR_A     	0x88000048
#define USBH_HC_RH_DESCRIPTOR_B     	0x8800004c
#define USBH_HC_RH_STATUS           	0x88000050
#define USBH_HC_RH_PORT_STATUS      	0x88000054
#define USBH_ISR                    	0x88000118
#define USBH_RCR                    	0x8800011c
#define USBH_IER                    	0x88000178
#define USBH_ICR                    	0x8800017c
#define USBH_IRSR                   	0x88000180
#define USBH_LPSR                   	0x88000184

#define IO_USBH_RCR                     __IOL(USBH_RCR)

/* 
 * IRQ
 */

#ifdef CONFIG_ARCH_COMPAT_L7200 /* for only compatibility to L7200. should not use */
#warning "CONFIG_ARCH_COMPAT_L7200 is defined"
#define	IRQSTATUS		0x90001000	/* IRQ status        */
#define	IRQRAWSTATUS		0x90001004	/* IRQ raw status    */
#define	IRQENABLE		0x90001008	/* IRQ enable        */
#define	IRQENABLESET		0x90001008	/* IRQ enable set    */
#define	IRQENABLECLEAR		0x9000100C	/* IRQ enable clear  */

#define IO_IRQSTATUS		__IOL(IRQSTATUS)
#define IO_IRQRAWSTATUS		__IOL(IRQRAWSTATUS)
#define IO_IRQENABLE		__IOL(IRQENABLE)
#define IO_IRQENABLESET		__IOL(IRQENABLESET)
#define IO_IRQENABLECLEAR	__IOL(IRQENABLECLEAR)
#else /* ! CONFIG_ARCH_COMPAT_L7200 */
#define	IRQSTATUS		0x90001080	/* IRQ status        */
#define	IRQHSTATUS		0x90001084	/* IRQ status        */
#define	IRQRAWSTATUS		0x90001090	/* IRQ raw status    */
#define	IRQHRAWSTATUS		0x90001094	/* IRQ raw status    */
#define	IRQENABLE		0x900010a0	/* IRQ enable        */
#define	IRQHENABLE		0x900010a4	/* IRQ enable        */
#define	IRQENABLESET		0x900010a0	/* IRQ enable set    */
#define	IRQHENABLESET		0x900010a4	/* IRQ enable set    */
#define	IRQENABLECLEAR		0x900010b0	/* IRQ enable clear  */
#define	IRQHENABLECLEAR		0x900010b4	/* IRQ enable clear  */

#define IO_IRQSTATUS		__IOL(IRQSTATUS)
#define IO_IRQHSTATUS		__IOL(IRQHSTATUS)
#define IO_IRQRAWSTATUS		__IOL(IRQRAWSTATUS)
#define IO_IRQHRAWSTATUS	__IOL(IRQHRAWSTATUS)
#define IO_IRQENABLE		__IOL(IRQENABLE)
#define IO_IRQHENABLE		__IOL(IRQHENABLE)
#define IO_IRQENABLESET		__IOL(IRQENABLESET)
#define IO_IRQHENABLESET	__IOL(IRQHENABLESET)
#define IO_IRQENABLECLEAR	__IOL(IRQENABLECLEAR)
#define IO_IRQHENABLECLEAR	__IOL(IRQHENABLECLEAR)
#endif /* ! CONFIG_ARCH_COMPAT_L7200 */

#define	FIQSTATUS		0x90001100	/* FIQ status        */
#define	FIQRAWSTATUS		0x90001104	/* FIQ raw status    */
#define	FIQENABLE		0x90001108	/* FIQ enable        */
#define	FIQENABLESET		0x90001108	/* FIQ enable set    */
#define	FIQENABLECLEAR		0x9000110C	/* FIQ enable clear  */

#define IO_FIQSTATUS		__IOL(FIQSTATUS)
#define IO_FIQRAWSTATUS		__IOL(FIQRAWSTATUS)
#define IO_FIQENABLE		__IOL(FIQENABLE)
#define IO_FIQENABLESET		__IOL(FIQENABLESET)
#define IO_FIQENABLECLEAR	__IOL(FIQENABLECLEAR)

/*
 * RTC
 */
#define RTCDR			0x90002000	/* RTC Data Register */
#define RTCMR			0x90002004	/* RTC Match Register */
#define RTCS			0x90002008	/* RTC Status */
#define RTCC			0x90002008	/* RTC Clear */
#define RTCDV			0x9000200c	/* RTC Clock Divider (ro) */
#define RTCCR			0x90002010	/* RTC Control Register */

#define IO_RTCDR		__IOL(RTCDR)
#define IO_RTCMR		__IOL(RTCMR)
#define IO_RTCS			__IOL(RTCS)
#define IO_RTCC			__IOL(RTCC)
#define IO_RTCDV		__IOL(RTCDV)
#define IO_RTCCR		__IOL(RTCCR)

#define RTCCR_RATE_32		0x00	      	/* 32 Hz tick */
#define RTCCR_RATE_64		0x10    	/* 64 Hz tick */
#define RTCCR_RATE_128		0x20      	/* 128 Hz tick */
#define RTCCR_RATE_256		0x30      	/* 256 Hz tick */
#define RTCCR_EN_ALARM		0x01      	/* Enable alarm */
#define RTCCR_EN_TIC		0x04      	/* Enable counter */
#define RTCCR_EN_STWDOG		0x08      	/* Enable watchdog */

/*
 * General Purpose Counter Timer registers
 */
#define	TIMER1LOAD		0x90003000	/* Timer1 load value	*/
#define	TIMER1VALUE		0x90003004	/* Read Timer1 value	*/
#define	TIMER1CONTROL		0x90003008	/* Timer1 control bits	*/
#define	TIMER1CLEAR		0x9000300C	/* Timer1 counter clear	*/
#define	TIMER2LOAD		0x90003020	/* Timer2 load value	*/
#define	TIMER2VALUE		0x90003024	/* Read Timer2 value	*/
#define	TIMER2CONTROL		0x90003028	/* Timer2 control bits	*/
#define	TIMER2CLEAR		0x9000302C	/* Timer2 counter clear	*/
#define STBUZCOUNTER    	0x90003040      /* STBUZ Counter */

#define	IO_TIMER1LOAD		__IOL(TIMER1LOAD)
#define	IO_TIMER1VALUE		__IOL(TIMER1VALUE)
#define	IO_TIMER1CONTROL	__IOL(TIMER1CONTROL)	
#define	IO_TIMER1CLEAR		__IOL(TIMER1CLEAR)
#define	IO_TIMER2LOAD		__IOL(TIMER2LOAD)
#define	IO_TIMER2VALUE		__IOL(TIMER2VALUE)
#define	IO_TIMER2CONTROL	__IOL(TIMER2CONTROL)	
#define	IO_TIMER2CLEAR		__IOL(TIMER2CLEAR)
#define IO_STBUZCOUNTER         __IOL(STBUZCOUNTER)

/* 
 *  Keyboard
 */
#define	KBDR		0x90004000	/* Keyboard Data Register	*/
#define	KBDMR		0x90004004	/* Keyboard Data Mode Register	*/
#define	KBSBSR		0x90004008	/* Keyboard Standby State Reg	*/
#define	KBKSR		0x90004010	/* Keyboard Scan Register	*/

#define IO_KBDR 	__IOL(KBDR)
#define IO_KBDMR  	__IOL(KBDMR)
#define IO_KBSBSR  	__IOL(KBSBSR)
#define IO_KBKSR   	__IOL(KBKSR)

#define KBDSCAN		0x0f	/* Keyboard scan	*/
#define KBSC_HI		0x0     /* All driven high	*/
#define KBSC_LO		0x1     /* All driven low	*/
#define KBSC_X		0x2     /* All pins tristate	*/
#define KBSC_COL0	0x4     /* Col 0 high, others tristate */
#define KBSC_COL1	0x5     /* Col 1 high, others tristate */
#define KBSC_COL2	0x6     /* Col 2 high, others tristate */
#define KBSC_COL3	0x7     /* Col 3 high, others tristate */
#define KBSC_COL4	0x8     /* Col 4 high, others tristate */
#define KBSC_COL5	0x9     /* Col 5 high, others tristate */
#define KBSC_COL6	0xa     /* Col 6 high, others tristate */
#define KBSC_COL7	0xb     /* Col 7 high, others tristate */
#define KBSC_COL8	0xc     /* Col 8 high, others tristate */
#define KBSC_COL9	0xd     /* Col 9 high, others tristate */
#define KBSC_COL10	0xe     /* Col 10 high, others tristate */
#define KBSC_COL11	0xf     /* Col 11 high, others tristate */

/*
 *  GPIO
 */
/* Port A */
#define	PADR		0x90005000	/* Port PA data		*/
#define	PADDR		0x90005004	/* Port PA direction	*/
#define	PASBSR		0x90005008	/* Port PA standby data	*/
#define	PAEENR		0x9000500C	/* edge/level interrupt	*/
#define	PAESNR		0x90005010	/* rising/falling edge	*/
#define	PAESTR		0x90005014	/* Edge interrupt status*/
#define	PAECLR		0x90005014	/* Edge interrupt clear */
#define	PAIMR		0x90005018	/* PA interrupt mask	*/
#define	PAINT		0x9000501C	/* PA interrupt request	*/

#if 1 /* GPIO Port A 29bit in L7210. Harmless to L7205 8bit-Port A */
#define IO_PADR		__IOL(PADR)
#define IO_PADDR	__IOL(PADDR)
#define IO_PASBSR	__IOL(PASBSR)
#define IO_PAEENR  	__IOL(PAEENR)
#define IO_PAESNR  	__IOL(PAESNR)
#define IO_PAESTR	__IOL(PAESTR)
#define IO_PAECLR	__IOL(PAECLR)
#define IO_PAIMR	__IOL(PAIMR)
#define IO_PAINT	__IOL(PAINT)
#else /* GPIO Port A 29bit in L7210 */
#define IO_PADR		__IOB(PADR)
#define IO_PADDR	__IOB(PADDR)
#define IO_PASBSR	__IOB(PASBSR)
#define IO_PAEENR  	__IOB(PAEENR)
#define IO_PAESNR  	__IOB(PAESNR)
#define IO_PAESTR	__IOB(PAESTR)
#define IO_PAECLR	__IOB(PAECLR)
#define IO_PAIMR	__IOB(PAIMR)
#define IO_PAINT	__IOB(PAINT)
#endif

/* Port B */
#define	PBDR		0x90005020	/* Port PB data		*/
#define	PBDDR		0x90005024	/* Port PB direction	*/
#define	PBSBSR		0x90005028	/* Port PB standby data	*/
#define	PBEENR		0x9000502C	/* edge/level interrupt	*/
#define	PBESNR		0x90005030	/* rising/falling edge	*/
#define	PBESTR		0x90005034	/* Edge interrupt status*/
#define	PBECLR		0x90005034	/* Edge interrupt clear */
#define	PBIMR		0x90005038	/* PB interrupt mask	*/
#define	PBINT		0x9000503C	/* PB interrupt request	*/

/* Port C */
#define	PCDR		0x90005040	/* Port PC data		*/
#define	PCDDR		0x90005044	/* Port PC direction	*/
#define	PCSBSR		0x90005048	/* Port PC standby data	*/
#define	PCEENR		0x9000504C	/* edge/level interrupt	*/
#define	PCESNR		0x90005050	/* rising/falling edge	*/
#define	PCESTR		0x90005054	/* Edge interrupt status*/
#define	PCCLR		0x90005054	/* Edge interrupt clear */
#define	PCIMR		0x90005058	/* PC interrupt mask	*/
#define	PCINT		0x9000505C	/* PC interrupt request	*/

/* Port D */
#define	PDDR		0x90005060	/* Port PD data		*/
#define	PDDDR		0x90005064	/* Port PD direction	*/
#define	PDSBSR		0x90005068	/* Port PD standby data	*/
#define	PDEENR		0x9000506C	/* edge/level interrupt	*/
#define	PDESNR		0x90005070	/* rising/falling edge	*/
#define	PDESTR		0x90005074	/* Edge interrupt status*/
#define	PDECLR		0x90005074	/* Edge interrupt clear */
#define	PDIMR		0x90005078	/* PD interrupt mask	*/
#define	PDINT		0x9000507C	/* PD interrupt request	*/

/* Port E */
#define	PEDR		0x90005080	/* Port PE data		*/
#define	PEDDR		0x90005084	/* Port PE direction	*/
#define	PESBSR		0x90005088	/* Port PE standby data	*/
#define	PEEENR		0x9000508C	/* edge/level interrupt	*/
#define	PEESNR		0x90005090	/* rising/falling edge	*/
#define	PEESTR		0x90005094	/* Edge interrupt status*/
#define	PEECLR		0x90005094	/* Edge interrupt clear */
#define	PEIMR		0x90005098	/* PE interrupt mask	*/
#define	PEINT		0x9000509C	/* PE interrupt request	*/

/* Port F */
#define	PFDR		0x900050a0	/* Port PF data		*/
#define	PFDDR		0x900050a4	/* Port PF direction	*/
#define	PFSBSR		0x900050a8	/* Port PF standby data	*/
#define	PFEENR		0x900050ac	/* edge/level interrupt	*/
#define	PFESNR		0x900050b0	/* rising/falling edge	*/
#define	PFESTR		0x900050b4	/* Edge interrupt status*/
#define	PFIMR		0x900050b8	/* PF interrupt mask	*/
#define	PFINT		0x900050bc	/* PF interrupt request	*/

/* Edge Detect */
#define PADE    	0x900050e0      /* PA Edge Detect */
#define PBDE    	0x900050e4      /* PB Edge Detect */
#define PCDE    	0x900050e8      /* PC Edge Detect */
#define PDDE    	0x900050ec      /* PD Edge Detect */
#define PEDE    	0x900050f0      /* PE Edge Detect */
#define PFDE    	0x900050f4      /* PF Edge Detect */

#define PCFG            0x90005180

#if 1 
/* GPIO Port A 29bit in L7210. Harmless to L7205 8bit-Port A */
#define IO_PADR		__IOL(PADR)
#define IO_PADDR	__IOL(PADDR)
#define IO_PASBSR	__IOL(PASBSR)
#define IO_PAEENR  	__IOL(PAEENR)
#define IO_PAESNR  	__IOL(PAESNR)
#define IO_PAESTR	__IOL(PAESTR)
#define IO_PAECLR	__IOL(PAECLR)
#define IO_PAIMR	__IOL(PAIMR)
#define IO_PAINT	__IOL(PAINT)
#else /* GPIO Port A 29bit in L7210 */
#define IO_PADR		__IOB(PADR)
#define IO_PADDR	__IOB(PADDR)
#define IO_PASBSR	__IOB(PASBSR)
#define IO_PAEENR  	__IOB(PAEENR)
#define IO_PAESNR  	__IOB(PAESNR)
#define IO_PAESTR	__IOB(PAESTR)
#define IO_PAECLR	__IOB(PAECLR)
#define IO_PAIMR	__IOB(PAIMR)
#define IO_PAINT	__IOB(PAINT)
#endif

#define IO_PBDR		__IOB(PBDR)
#define IO_PBDDR	__IOB(PBDDR)
#define IO_PBSBSR	__IOB(PBSBSR)
#define IO_PBIMR	__IOB(PBIMR)
#define IO_PBINT	__IOB(PBINT)

#define IO_PCDR		__IOB(PCDR)
#define IO_PCDDR	__IOB(PCDDR)
#define IO_PCSBSR	__IOB(PCSBSR)
#define IO_PCIMR	__IOB(PCIMR)
#define IO_PCINT	__IOB(PCINT)

#define IO_PDDR		__IOB(PDDR)
#define IO_PDDDR	__IOB(PDDDR)
#define IO_PDSBSR	__IOB(PDSBSR)
#define IO_PDEENR  	__IOB(PDEENR)
#define IO_PDESNR  	__IOB(PDESNR)
#define IO_PDESTR	__IOB(PDESTR)
#define IO_PDECLR	__IOB(PDECLR)
#define IO_PDIMR	__IOB(PDIMR)
#define IO_PDINT	__IOB(PDINT)

#define IO_PEDR		__IOB(PEDR)
#define IO_PEDDR	__IOB(PEDDR)
#define IO_PESBSR	__IOB(PESBSR)
#define IO_PEIMR	__IOB(PEIMR)
#define IO_PEINT	__IOB(PEINT)

#define IO_PFDR		__IOB(PFDR)
#define IO_PFDDR	__IOB(PFDDR)
#define IO_PFSBSR	__IOB(PFSBSR)
#define IO_PFEENR  	__IOB(PFEENR)
#define IO_PFESNR  	__IOB(PFESNR)
#define IO_PFESTR	__IOB(PFESTR)
#define IO_PFIMR	__IOB(PFIMR)
#define IO_PFINT	__IOB(PFINT)

#define IO_PADE         __IOB(PADE)
#define IO_PDDE         __IOB(PDDE)
#define IO_PEDE         __IOB(PEDE)
#define IO_PFDE         __IOB(PFDE)

#define IO_PCFG         __IOL(PCFG)

/*
 * Static Memory Interface (SMI)
 */
#define	IOCFG1		0x90007000	/* SMI Configuration Register 1	*/
#define	IOCFG2		0x90007004	/* SMI Configuration Register 2	*/
#define	IOCFG3		0x90007008	/* SMI Configuration Register 3	*/
#define	IOCFG4		0x9000700C	/* SMI Configuration Register 4	*/
#define	IOCFG5		0x90007010	/* SMI Configuration Register 5	*/
#define	IOSR		0x90007014	/* SMI Status Register (IOSR)	*/
#define	IOCFG6		0x90007018	/* SMI Configuration Register 6	*/

#define IO_IOCFG1	__IOL(IOCFG1)
#define IO_IOCFG2	__IOL(IOCFG2)
#define IO_IOCFG3	__IOL(IOCFG3)
#define IO_IOCFG4	__IOL(IOCFG4)
#define IO_IOCFG5	__IOL(IOCFG5)
#define IO_IOSR		__IOL(IOSR)
#define IO_IOCFG6	__IOL(IOCFG6)

/*
 *   SDRAM registers
 */

#define SDRAMCFG_OFFSET    0x000
#define SDRAMRFSH_OFFSET   0x004
#define SDRAMWBFT_OFFSET   0x008

#define __IOL_SDRAM(o)    (*((volatile unsigned long*)((SDRAM_BASE)+(o))))

#define IO_SDRAMCFG        __IOL_SDRAM(SDRAMCFG_OFFSET)
#define IO_SDRAMRFSH       __IOL_SDRAM(SDRAMRFSH_OFFSET)
#define IO_SDRAMWBFT       __IOL_SDRAM(SDRAMWBFT_OFFSET)

#if defined(CONFIG_IRIS)
#include <asm/arch/hardware_iris.h>
#endif

#endif
