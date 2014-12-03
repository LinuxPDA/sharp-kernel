/*
 * L7205(L7210) DMA based 16bit DAC sound driver for IRIS (SHARP)
 *
 * DAC : LC78817M (SANYO)
 * I/F : Serial interface to codec (SIC) AIL mode
 *
 * (c) 2001 by K.Yamade (SHARP)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License Version2. See the file COPYING in the main directory of this archive
 * for more details.
 *
 * ChangeLog:
 *  04-03-2001  Lineo Japan, Inc.
 */
#ifndef __ASM_ARCH_DMASOUND_IRIS_H
#define __ASM_ARCH_DMASOUND_IRIS_H

#if 0
/* K.Yamade memo */
#define	IRIS_DMA_CHANNEL0	0x00
#define	IRIS_SOUND_DMA_CHANNEL	IRIS_DMA_CHANNEL0
#endif

#define SYS_CLOCK_AUX		0x8005003C	/* Aux PLL Conf Register */
#define	IO_SYS_CLOCK_AUX	__IOL(SYS_CLOCK_AUX)

/*
 * Serial interface to codec
 */
#define	SIC_BASE	0x8004C000

#define	SICR0		(SIC_BASE+0x00)	/* SIC Common Control Reg */
#define	SICR1		(SIC_BASE+0x04)	/* SIC AIL Control Reg */
#define	SICR2		(SIC_BASE+0x08)	/* SIC AC-link Control Reg */
#define	SISR		(SIC_BASE+0x0C)	/* SIC Status Reg */
#define	SIRSR		(SIC_BASE+0x10)	/* SIC RAW Status Reg */
#define	SIIER		(SIC_BASE+0x14)	/* SIC Interupt Mask Enable Reg */
#define	SIIDR		(SIC_BASE+0x18)	/* SIC Interupt Mask Disable Reg */
#define	SIICR		(SIC_BASE+0x1C)	/* SIC Interrupt Clear Reg */
#define	L3CAR		(SIC_BASE+0x20)	/* L3 Control Bus Address Reg */
#define	L3CDR		(SIC_BASE+0x24)	/* L3 Control Bus Data Reg */
#define	ACCAR		(SIC_BASE+0x28)	/* AC-link Command Address Reg */
#define	ACCDR		(SIC_BASE+0x2C)	/* AC-link Command Data Reg */
#define	ACSAR		(SIC_BASE+0x30)	/* AC-link Status Address Reg */
#define	ACSDR		(SIC_BASE+0x34)	/* AC-link Status Data Reg */
#define	ACGDR		(SIC_BASE+0x38)	/* AC-link GPIO Data Reg */
#define	ACGSR		(SIC_BASE+0x3C)	/* AC-link GPIO Status Reg */
#define	SIADR		(SIC_BASE+0x80)	/* SIC Data Regs */
#define	SIMDR		(SIC_BASE+0xC0)	/* SIC Modem Data Reg */

#define	IO_SICR0	__IOL(SICR0)
#define	IO_SICR1	__IOL(SICR1)
#define	IO_SICR2	__IOL(SICR2)
#define	IO_SISR		__IOL(SISR)
#define	IO_SIRSR	__IOL(SIRSR)
#define	IO_SIIER	__IOL(SIIER)
#define	IO_SIIDR	__IOL(SIIDR)
#define	IO_SIICR	__IOL(SIICR)
#define	IO_L3CAR	__IOL(L3CAR)
#define	IO_L3CDR	__IOL(L3CDR)
#define	IO_ACCAR	__IOL(ACCAR)
#define	IO_ACCDR	__IOL(ACCDR)
#define	IO_ACSAR	__IOL(ACSAR)
#define	IO_ACSDR	__IOL(ACSDR)
#define	IO_ACGDR	__IOL(ACGDR)
#define	IO_ACGSR	__IOL(ACGSR)
#define	IO_SIADR	__IOL(SIADR)
#define	IO_SIMDR	__IOL(SIMDR)

/* SICR0 - SIC Common Control Register */
#define	SICR0_ENB	0x00000001	/* Enable SAC function */
#define	SICR0_SAIL	0x00000002	/* ACLINK and AIL selection */
#define	SICR0_BCKD	0x00000004	/* BIT_CLK pin direction */
#define	SICR0_SYNCD	0x00000008	/* Sync PAD direction */
#define	SICR0_RST	0x00000010	/* Reset SIC Control and FIFOs */
#define	SICR0_SSPI	0x00000020	/* SPI PADS Usage */
#define	SICR0_L3KB	0x00000040	/* Enable L3 on Keyboard Pads */

/* SICR1 - SIC Audio Interface Logic Control Register */
#define	SICR1_EREC	0x00000001	/* Enable recording function - AIL */
#define	SICR1_ERPL	0x00000002	/* Enable replay function - AIL */
#define	SICR1_MODE	0x0000000C	/* Interface Logic Format */
#define	SICR1_MODEMSB	0	 	/* Interface Logic Format */
#define	SICR1_MODELSB	(1<<2) 		/* Interface Logic Format */
#define	SICR1_MODERES	(2<<2) 		/* Interface Logic Format */
#define	SICR1_MODE4	(3<<2) 		/* Interface Logic Format */
#define	SICR1_BPHA	0x00000010	/* Audio I/F Logic I/O Clock Phase */
#define	SICR1_CNTL	0x00000020	/* EN CODEC ctrl data in audio frame */
#define	SICR1_RDST	0x00000040
#define	SICR1_L3EN	0x00010000
#define	SICR1_L3MB	0x00020000
#define	SICR1_ENLBF	0x80000000

/* SICR2 - Serial Audio AC-link Control Register */
#define	SICR2_EREC	0x00000001	/* Enable recording function */
#define	SICR2_ERPL	0x00000002	/* Enable playback function */
#define	SICR2_EINC	0x00000004	/* Enable Modem incoming data */
#define	SICR2_EOUT	0x00000008	/* Enable Modem output data */
#define	SICR2_EGPIO	0x00000010	/* Enable receiving GPIO data */
#define	SICR2_WKUP	0x00000100	/* Wakeup AC97 CODEC */
#define	SICR2_DRSTO	0x00000080	/* Disable Read Status Timeout */
#define	SICR2_ENLBF	0x00000040	/* Enable Loopback Function */

/* SISR - SIC Status Register */
#define	SISR_DTD	0x00000001	/* Data Transfer Done */
#define	SISR_RDD	0x00000002	/* Data Received */
#define	SISR_GTD	0x00000004	/* GPIO Data Transfer Done */
#define	SISR_STAD	0x00000008	/* Control Status Write are Done */
#define	SISR_BSY	0x00000010	/* SIC busy */
#define	SISR_LBSY	0x00000020	/* L3 busy */
#define	SISR_ATNF	0x00000040	/* Audio Transmit FIFO Not Full */
#define	SISR_ARNE	0x00000080	/* Audio Receive FIFO Not Empty */
#define	SISR_ATFS	0x00000100	/* Audio Transmit FIFO Service Req */
#define	SISR_ARFS	0x00000200	/* Audio Receive FIFO Service Req */
#define	SISR_ATUR	0x00000400	/* Audio Transmit FIFO Underrun */
#define	SISR_AROR	0x00000800	/* Audio Receive FIFO Overrun */
#define	SISR_MTNF	0x00001000	/* Modem Transmit FIFO Not Full */
#define	SISR_MRNE	0x00002000	/* Modem Receive FIFO Not Empty */
#define	SISR_MTFS	0x00004000	/* Modem Transmit FIFO Service Req */
#define	SISR_MRFS	0x00008000	/* Modem Receive FIFO Service Req */
#define	SISR_MTUR	0x00010000	/* Modem Transmit FIFO Underrun */
#define	SISR_MROR	0x00020000	/* Modem Receive FIFO Overrun */
#define	SISR_RSTO	0x00040000	/* Read status Timeout */
#define	SISR_CLPM	0x00080000	/* Low Power Mode */
#define	SISR_CRDY	0x00100000	/* Codec ready */
#define	SISR_RESU	0x00200000	/* CODEC Resume Intrpt */
#define	SISR_GINT	0x00400000	/* GPIO Status Change Intrpt */
#define	SISR_RS3V	0x00800000	/* Receive  Slot 3 Valid */
#define	SISR_RS4V	0x01000000	/* Receive Slot 4 Valid */
#define	SISR_RS5V	0x02000000	/* Receive Slot 5 Valid */
#define	SISR_RS12V	0x04000000	/* Receive Slot 12 Valid */

/* SIICR - SIC Interrupt Clear Register */
#define	SIICR_DTD	0x00000001	/* Data Transfer Done */
#define	SIICR_RDD	0x00000002	/* Data Received */
#define	SIICR_GTD	0x00000004	/* GPIO Data Transfer Done */
#define	SIICR_STAD	0x00000008	/* Control Status Write are Done */
#define	SIICR_ATUR	0x00000400	/* Audio Transmit FIFO Underrun */
#define	SIICR_AROR	0x00000800	/* Audio Receive FIFO Overrun */
#define	SIICR_MTUR	0x00010000	/* Modem Transmit FIFO Underrun */
#define	SIICR_MROR	0x00020000	/* Modem Receive FIFO Overrun */
#define	SIICR_RSTO	0x00040000	/* Read status Timeout */
#define	SIICR_RESU	0x00200000	/* CODEC Resume Intrpt */
#define	SIICR_GINT	0x00400000	/* GPIO Status Change Intrpt */

/* SIIER - SIC Interrupt Mask Enable Register */
#define	SIIER_DTD	0x00000001	/* Data Transfer Done */
#define	SIIER_RDD	0x00000002	/* Data Received */
#define	SIIER_GTD	0x00000004	/* GPIO Data Transfer Done */
#define	SIIER_STAD	0x00000008	/* Control Status Write are Done */
#define	SIIER_ATFS	0x00000100	/* Audio Transmit FIFO Service Req */
#define	SIIER_ARFS	0x00000200	/* Audio Receive FIFO Service Req */
#define	SIIER_ATUR	0x00000400	/* Audio Transmit FIFO Underrun */
#define	SIIER_AROR	0x00000800	/* Audio Receive FIFO Overrun */
#define	SIIER_MTFS	0x00004000	/* Modem Transmit FIFO Service Req */
#define	SIIER_MRFS	0x00008000	/* Modem Receive FIFO Service Req */
#define	SIIER_MTUR	0x00010000	/* Modem Transmit FIFO Underrun */
#define	SIIER_MROR	0x00020000	/* Modem Receive FIFO Overrun */
#define	SIIER_RSTO	0x00040000	/* Read status Timeout */
#define	SIIER_RESU	0x00200000	/* CODEC Resume Intrpt */
#define	SIIER_GINT	0x00400000	/* GPIO Status Change Intrpt */


/* SIIDR - SIC Interrupt Mask Disable Register */
#define	SIIDR_DTD	0x00000001	/* Data Transfer Done */
#define	SIIDR_RDD	0x00000002	/* Data Received */
#define	SIIDR_GTD	0x00000004	/* GPIO Data Transfer Done */
#define	SIIDR_STAD	0x00000008	/* Control Status Write are Done */
#define	SIIDR_ATFS	0x00000100	/* Audio Transmit FIFO Service Req */
#define	SIIDR_ARFS	0x00000200	/* Audio Receive FIFO Service Req */
#define	SIIDR_ATUR	0x00000400	/* Audio Transmit FIFO Underrun */
#define	SIIDR_AROR	0x00000800	/* Audio Receive FIFO Overrun */
#define	SIIDR_MTFS	0x00004000	/* Modem Transmit FIFO Service Req */
#define	SIIDR_MRFS	0x00008000	/* Modem Receive FIFO Service Req */
#define	SIIDR_MTUR	0x00010000	/* Modem Transmit FIFO Underrun */
#define	SIIDR_MROR	0x00020000	/* Modem Receive FIFO Overrun */
#define	SIIDR_RSTO	0x00040000	/* Read status Timeout */
#define	SIIDR_RESU	0x00200000	/* CODEC Resume Intrpt */
#define	SIIDR_GINT	0x00400000	/* GPIO Status Change Intrpt */

/* SIADR - Serial Audio Data Registers (8 - 32bit registers) */
#define	SIADR_ADTL	0x0000FFFF	/* 16-bit Left Channel Data */
#define	SIADR_ADTH	0xFFFF0000	/* 16-bit Right Channel Data */

/*** Some low level helpers *****/
#define	SICSYN_SEL00	0x00000000
#define	SICSYN_SEL01	0x00200000
#define	SICSYN_SEL10	0x00400000

#define	SICDIV_SEL000	0x00000000
#define	SICDIV_SEL001	0x00020000
#define	SICDIV_SEL010	0x00040000
#define	SICDIV_SEL011	0x00060000
#define	SICDIV_SEL101	0x000A0000

#endif /* __ASM_ARCH_DMASOUND_IRIS_H */
