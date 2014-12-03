/*
 * linux/include/asm-sh/sh7727.h
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi SH7727 support.
 */
#ifndef __ASM_SH_SH7727_H
#define __ASM_SH_SH7727_H 1

#define BCR1		0xffffff60
#define BCR1_PCM_MASK   (~(3 << 0))
#define BCR1_A5PCM      (1 << 1)
#define BCR1_A6PCM      (1 << 0)

#define BCR2		0xffffff62
#define BCR2_A5_MASK    (~(3 << 10))
#define BCR2_A5_8       (1 << 10)
#define BCR2_A5_16      (2 << 10)
#define BCR2_A5_32      (3 << 10)
#define BCR2_A6_MASK    (~(3 << 12))
#define BCR2_A6_8       (1 << 12)
#define BCR2_A6_16      (2 << 12)
#define BCR2_A6_32      (3 << 12)

#define WCR1		0xffffff64
#define WCR2		0xffffff66
#define WCR2_A6_MASK	(~(3 << 13)
#define PCR		0xffffff6c
#define PCR_A6W3        (1 << 15)

/* SIOF - SIO module with FIFO */
#define SIMDR		0xa40000c0 /* Mode Register */
#define SISCR		0xa40000c2 /* Clock Select Register */
#define SITDAR		0xa40000c4 /* Transmit Data Assign Register */
#define SIRDAR		0xa40000c6 /* Recieve Data Assign Register */
#define SICDAR		0xa40000c8 /* Control Data Assign Register */
#define SICTR		0xa40000cc /* Control Register */
#define SIFCTR		0xa40000d0 /* FIFO control Register */
#define SISTR		0xa40000d4 /* Status Register */
#define SIIER		0xa40000d6 /* Interrupt Enable Register */
#define SITDR		0xa40000e0 /* Transmit Data Register */
#define SIRDR		0xa40000e4 /* Receive Data Register */
#define SITCR		0xa40000e8 /* Transmit Control Register */
#define SIRCR		0xa40000ec /* Receive Control Register */

/* AFEIF - Analog Front End Interface */
#define ACTR1		0xa4000180 /* AFEIF Control Register 1 */
#define ACTR2		0xa4000182 /* AFEIF Control Register 2 */
#define ASTR1		0xa4000184 /* AFEIF Status Register 1 */
#define ASTR2		0xa4000186 /* AFEIF Status Register 2 */
#define MRCR		0xa4000188 /* Make Ratio Count Register */
#define MPCR		0xa400018a /* Minimum Pose Count Register */
#define DPNQ		0xa400018c /* Dail Number Queue */
#define RCNT		0xa400018e /* Ringing Pulse Counter */
#define ACDR		0xa4000190 /* AFE Control Data Register */
#define ASDR		0xa4000192 /* AFE Status Data Register */
#define TDFP		0xa4000194 /* Transmit Data FIFO Port */
#define RDFP		0xa4000198 /* Receive Data FIFO Port */

/* LCDC - LDC Controller */
#define LDICKR		0xa4000c00 /* Input Clock Register */
#define   LDICKR_B_CLK		0x0000
#define   LDICKR_P_CLK		0x1000
#define   LDICKR_E_CLK		0x2000
#define LDMTR		0xa4000c02 /* Module Type Register */
#define   LDMTR_FLMPOL		(1 << 15)
#define   LDMTR_CL1POL		(1 << 14)
#define   LDMTR_DISPPOL		(1 << 13)
#define   LDMTR_DPOL		(1 << 12)
#define   LDMTR_MCNT		(1 << 10)
#define   LDMTR_CL1CNT		(1 << 9)
#define   LDMTR_CL2CNT		(1 << 8)
#define   LDMTR_STN_MONO_4	0x00
#define   LDMTR_STN_MONO_8	0x01
#define   LDMTR_STN_COLOR_4	0x08
#define   LDMTR_STN_COLOR_8	0x09
#define   LDMTR_STN_COLOR_12	0x0A
#define   LDMTR_STN_COLOR_16	0x0B
#define   LDMTR_DSTN_MONO_8	0x11
#define   LDMTR_DSTN_MONO_16	0x13
#define   LDMTR_DSTN_COLOR_8	0x19
#define   LDMTR_DSTN_COLOR_12	0x1A
#define   LDMTR_DSTN_COLOR_16	0x1B
#define   LDMTR_TFT_COLOR_16	0x2B
#define LDDFR		0xa4000c04 /* Data Format Register */
#define   LDDFR_PABD		(1 << 8)
#define   LDDFR_MONO_2		0x00 /* 1bpp */
#define   LDDFR_MONO_4		0x01 /* 2bpp */
#define   LDDFR_MONO_16		0x02 /* 4bpp */
#define   LDDFR_MONO_64		0x04 /* 6bpp */
#define   LDDFR_COLOR_16	0x0A /* 16 */
#define   LDDFR_COLOR_256	0x0C /* 256 */
#define   LDDFR_COLOR_32K	0x1D /* RGB:555 */
#define   LDDFR_COLOR_64K	0x2D /* RGB:565 */
#define LDSMR		0xa4000c06 /* Scan Mode Register */
#define   LDSMR_ROT		(1 << 13)
#define LDSARU		0xa4000c08 /* Start Address Register U */
#define LDSARL		0xa4000c0c /* Start Address Register L */
#define LDLAOR		0xa4000c10 /* Line Address Offset Register */
#define LDPALCR		0xa4000c12 /* Palette Control Register */
#define LDPR		0xa4000800 /* Palette Data Register */
#define LDHCNR		0xa4000c14 /* Horizontal Character Number Register */
#define LDHSYNR		0xa4000c16 /* Horizontal Sync Signal Register */
#define LDVDLNR		0xa4000c18 /* Vertical Display Line Number Register */
#define LDVTLNR		0xa4000c1a /* Vertical Total Line Number Register */
#define LDVSYNR		0xa4000c1c /* Vertical Sync Signal Register */
#define LDACLNR		0xa4000c1e /* AC Line Number Register */
#define LDINTR		0xa4000c20 /* Interrupt Control Register */
#define   LDINTR_VINTSEL (1 << 12)
#define   LDINTR_VINTE   (1 << 8)
#define   LDINTR_VINTS   (1 << 0)
#define LDPMMR		0xa4000c24 /* Power Management Mode Register */
#define   LDPMMR_LPS1 (1 << 1)
#define   LDPMMR_LPS0 (1 << 0)
#define LDPSPR		0xa4000c26 /* Power Sequence Period Register */
#define LDCNTR		0xa4000c28 /* Control Register */
#define   LDCNTR_DON		(1 << 0)
#define   LDCNTR_DON2		(1 << 4)

/* Standby */
#define STBCR           0xffffff82 /* Standby Control Register 1 */
#define STBCR2          0xffffff88 /* Standby Control Register 2 */
#define STBCR3          0xa4000230 /* Standby Control Register 3 */

/* USB PIN Multiplex Controller */
#define EXPFC		0xa4000234 /* Extra Pin Function Controller */

/* EXCPG */
#define EXCPGCR         0xa4000236 /* Extended Clock Generator */

/* Pin Function Controller */
#define PACR		0xa4000100 /* Port A Control Register */
#define PBCR		0xa4000102 /* Port B Control Register */
#define PCCR		0xa4000104 /* Port C Control Register */
#define PDCR		0xa4000106 /* Port D Control Register */
#define PECR		0xa4000108 /* Port E Control Register */
#define PFCR		0xa400010a /* Port F Control Register */
#define PGCR		0xa400010c /* Port G Control Register */
#define PHCR		0xa400010e /* Port H Control Register */
#define PJCR		0xa4000110 /* Port J Control Register */
#define PKCR		0xa4000112 /* Port K Control Register */
#define PLCR		0xa4000114 /* Port L Control Register */
#define SCPCR		0xa4000116 /* SC Port Control Register */
#define PMCR		0xa4000118 /* Port M Control Register */

/* I/O Ports */
#define PADR		0xa4000120 /* Port A Data Register */
#define PBDR		0xa4000122 /* Port B Data Register */
#define PCDR		0xa4000124 /* Port C Data Register */
#define PDDR		0xa4000126 /* Port D Data Register */
#define PEDR		0xa4000128 /* Port E Data Register */
#define PFDR		0xa400012a /* Port F Data Register */
#define PGDR		0xa400012c /* Port G Data Register */
#define PHDR		0xa400012e /* Port H Data Register */
#define PJDR		0xa4000130 /* Port J Data Register */
#define PKDR		0xa4000132 /* Port K Data Register */
#define PLDR		0xa4000134 /* Port L Data Register */
#define SCPDR		0xa4000136 /* SC Port Data Register */
#define PMDR		0xa4000138 /* Port M Data Register */

/* ADC - A/D Converter */
#define ADDRAH		0xa4000080 /* A/D Data Register A (H) */
#define ADDRAL		0xa4000082 /* A/D Data Register A (L) */
#define ADDRBH		0xa4000084 /* A/D Data Register B (H) */
#define ADDRBL		0xa4000086 /* A/D Data Register B (L) */
#define ADDRCH		0xa4000088 /* A/D Data Register C (H) */
#define ADDRCL		0xa400008A /* A/D Data Register C (L) */
#define ADDRDH		0xa400008C /* A/D Data Register D (H) */
#define ADDRDL		0xa400008E /* A/D Data Register D (L) */
#define ADCSR		0xa4000090 /* A/D Cotrol/Status Register */
#define   ADCSR_ADF		(1 << 7)
#define   ADCSR_ADIE		(1 << 6)
#define   ADCSR_ADST		(1 << 5)
#define   ADCSR_MULTI		(1 << 4)
#define   ADCSR_CKS		(1 << 3)
#define   ADCSR_CH2		(1 << 2)
#define   ADCSR_CH1		(1 << 1)
#define   ADCSR_CH0		(1 << 0)
#define ADCR		0xa4000092 /* A/D Control Register */
#define   ADCR_TRGE1		(1 << 7)
#define   ADCR_TRGE0		(1 << 6)
#define   ADCR_SCN		(1 << 5)
#define   ADCR_RESVD1		(1 << 4)
#define   ADCR_RESVD2		(1 << 3)

/* DAC - D/A Converter */
#define DADR0		0xa40000a0 /* D/A Data Register 0 */
#define DADR1		0xa40000a2 /* D/A Data Register 1 */
#define DACR		0xa40000a4 /* D/A Control Register */
#define   DACR_DAOE1		(1 << 7)
#define   DACR_DAOE0		(1 << 6)
#define   DACR_DAE		(1 << 5)

/* PCC - PC Card Controller */
#define PCC0ISR		0xa4000160 /* Interface Status Register */
#define   PCC0ISR_P0RDY		(1 << 7) /* mem */
#define   PCC0ISR_P0IREQ	(1 << 7) /* io */
#define   PCC0ISR_P0MWP		(1 << 6)
#define   PCC0ISR_P0VS2		(1 << 5)
#define   PCC0ISR_P0VS1		(1 << 4)
#define   PCC0ISR_P0CD2		(1 << 3)
#define   PCC0ISR_P0CD1		(1 << 2)
#define   PCC0ISR_P0BVD2	(1 << 1) /* mem */
#define   PCC0ISR_P0BVD1	(1 << 0) /* mem */
#define   PCC0ISR_SPKR		(1 << 1) /* io */
#define   PCC0ISR_STSCHG	(1 << 0) /* io */
#define PCC0GCR		0xa4000162 /* General Control Register */
#define   PCC0GCR_P0DRVE	(1 << 7)
#define   PCC0GCR_P0PCCR	(1 << 6)
#define   PCC0GCR_P0PCCT	(1 << 5)
#define   PCC0GCR_P0USE		(1 << 4)
#define   PCC0GCR_P0MMOD	(1 << 3)
#define   PCC0GCR_P0PA25	(1 << 2)
#define   PCC0GCR_P0PA24	(1 << 1)
#define   PCC0GCR_P0REG		(1 << 0)
#define PCC0CSCR	0xa4000164 /* Card Status Change Register */
#define   PCC0CSCR_P0SCDI	(1 << 7)
#define   PCC0CSCR_P0IREQ	(1 << 5)
#define   PCC0CSCR_P0SC		(1 << 4)
#define   PCC0CSCR_P0CDC	(1 << 3)
#define   PCC0CSCR_P0RC		(1 << 2)
#define   PCC0CSCR_P0BW		(1 << 1)
#define   PCC0CSCR_P0BD		(1 << 0)
#define PCC0CSCIER	0xa4000166 /* Card Status Change Int Enable Register */
#define   PCC0CSCIER_PCRE	(1 << 7)
#define   PCC0CSCIER_IREQE1	(1 << 6)
#define   PCC0CSCIER_IREQE0	(1 << 5)
#define   PCC0CSCIER_P0SCE	(1 << 4)
#define   PCC0CSCIER_P0CDE	(1 << 3)
#define   PCC0CSCIER_P0RE	(1 << 2)
#define   PCC0CSCIER_P0BWE	(1 << 1)
#define   PCC0CSCIER_P0BDE	(1 << 0)

#define SAR0			0xa4000020
#define DAR0			0xa4000024
#define DMATCR0			0xa4000028
#define CHCR0			0xa400002c
#define SAR1			0xa4000030
#define DAR1			0xa4000034
#define DMATCR1			0xa4000038
#define CHCR1			0xa400003c
#define SAR2			0xa4000040
#define DAR2			0xa4000044
#define DMATCR2			0xa4000048
#define CHCR2			0xa400004c
#define SAR3			0xa4000050
#define DAR3			0xa4000054
#define DMATCR3			0xa4000058
#define CHCR3			0xa400005c
#define DMAOR			0xa4000060
#define CHRAR			0xa400022a

#define CHCR_DE			(1 << 0)
#define CHCR_TE			(1 << 1)
#define CHCR_IE			(1 << 2)

/* USB Host */
#define USBH_BASE               0xa4000400

/* USB Function */
#define USBIFR0			0xa4000240
#define USBIFR1			0xa4000241
#define USBEPDR0I		0xa4000242
#define USBEPDR0O		0xa4000243
#define USBTRG			0xa4000244
#define USBFCLR			0xa4000245
#define USBEPSZ0O		0xa4000246
#define USBEPDR0S		0xa4000247
#define USBDASTS		0xa4000248
#define USBEPDR2		0xa4000249
#define USBISR0			0xa400024a
#define USBEPSTL		0xa400024b
#define USBIER0			0xa400024c
#define USBIER1			0xa400024d
#define USBEPDR1		0xa400024e
#define USBEPSZ1		0xa400024f
#define USBISR1			0xa4000250
#define USBDMA			0xa4000251
#define USBEPDR3		0xa4000252

#define WTCNT			0xffffff84
#define WTCSR			0xffffff86

#define SCSCR			0xfffffe84

#define HCCONTROL		0xa4000404
#define MCR			0xffffff68
#define RTCNT			0xffffff70
#define TSTR			0xfffffe92

#define SCSMR2			0xa4000150
#define SCSCR2			0xa4000154
#define SCBRR2			0xa4000152
#define SCFCR2			0xa400015c 

/* SH7727 support function*/

extern void sh7727_setup(void);

typedef struct { 
	unsigned long sar;
	unsigned long dar;
	unsigned long dmatcr;
	unsigned long chcr;
        unsigned long way;        /* 0:area to port , 1:port to area */
} dma_device_t;
typedef int dmach_t;
typedef void (*dma_callback_t)(void* buf_id, int size);

extern int sh7727_request_dma(dmach_t* channel, const char*device_id);
extern int sh7727_dma_set_callback(dmach_t channel, dma_callback_t cb);
extern int sh7727_dma_set_device(dmach_t channel, dma_device_t device);
extern int sh7727_dma_set_spin(dmach_t channel, dma_addr_t addr, int size);
extern int sh7727_dma_queue_buffer(dmach_t channel, void* buf_id, dma_addr_t data, int size);
extern int sh7727_dma_get_current(dmach_t channel, void **buf_id, dma_addr_t* addr);
extern int sh7727_dma_stop(dmach_t channel);
extern int sh7727_dma_resume(dmach_t channel);
extern int sh7727_dma_flush_all( dmach_t channel );
extern void sh7727_free_dma(dmach_t channel);

typedef void (*adc_callback_t)(int, void*, struct pt_regs*);
extern int sh7727_adc_start(unsigned int);
extern void sh7727_adc_stop(void);
extern unsigned int sh7727_adc_read(unsigned int);
extern void sh7727_adc_set_callback(unsigned int, adc_callback_t);

#define MAX_ADC_CHANNELS 7
#define ADC_AN1 0x01
#define ADC_AN2 0x02
#define ADC_AN3 0x03
#define ADC_AN4 0x04
#define ADC_AN5 0x05
#define ADC_AN6 0x06
#define ADC_AN7 0x07

extern int  sh7727fb_get_contrast(void);
extern int  sh7727fb_get_backlight(void);
extern void sh7727fb_set_contrast(int);
extern void sh7727fb_set_backlight(int);

#endif /* __ASM_SH_SH7727_H */
