/*
 *  linux/include/asm-sh/sh7760.h
 *
 *  Copyright (c) 2001 Lineo Japan, Inc.
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 *  Hitachi SH7760 support.
 */
#ifndef __ASM_SH_SH7760_FB_H
#define __ASM_SH_SH7760_FB_H 1

/* DMAC - Direct Memory Access Controler */
#define SAR0		0xffa00000	// Source Addr Reg(CH0)
#define SAR1		0xffa00010	// Source Addr Reg(CH1)
#define DAR0		0xffa00004	// Distination Addr Reg(CH0)
#define DAR1		0xffa00014	// Distination Addr Reg(CH1)
#define DMATCR0		0xffa00008	// Transfar count Reg(CH0)
#define DMATCR1		0xffa00018	// Transfar count Reg(CH1)
#define   DMATCR_CNT	0x00000020	// Transfar count
#define CHCR0		0xffa0000c	// Channel Control Reg(CH0)
#define CHCR1		0xffa0001c	// Channel Control Reg(CH1)
#define   CHCR_DSTADDR_SUB	0x00008000 // dst addr subtraction
#define   CHCR_DSTADDR_ADD	0x00004000 // dst addr addition
#define   CHCR_DSTADDR_FIX	0x00000000 // dst addr fixed
#define   CHCR_SRCADDR_SUB	0x00002000 // src addr subtraction
#define   CHCR_SRCADDR_ADD	0x00001000 // src addr addition
#define   CHCR_RESOURCE_OUT	0x00000300 // resource outside req
#define   CHCR_RESOURCE_AUTO	0x00000500 // resource auto req
#define   CHCR_TRANS_BURST	0x00000080 // Burst mode transmit
#define   CHCR_TRANS_SIZE	0x00000020 // word(16byte) transmit
#define   CHCR_CH_SET		0x00000008 // channel set (always 1)
#define   CHCR_IRQ_EN		0x00000004 // interrupt enable
#define   CHCR_DMAC_EN		0x00000001 // DMAC enable
#define   CHCR_DMAC_DIS		0x00000000 // DMAC disable
#define DMARCR		0xfe090008	// DMA Request Control Reg
#define   DMARCR_RPR_ROUND	0x00030000 // priority round robin
#define   DMARCR_RPR_MODE01	0x00010000 // priority mode b'01'
#define   DMARCR_DREQ1_START	0x00000040 // DREQ1 detect start edge
#define DMARSRA		0xfe090000	// Req Resource Select A
#define   DMARSRA_CH0		0x94000000 // CH0 req from DMABRG
#define   DMARSRA_CH1		0x00910000 // CH1 req from DREQ1
#define DMAOR		0xffa00040	// Operation Reg
#define   DMAOR_DMS_DMABRG	0x0000c000 // DMA mode DMABRG B'11'
#define   DMAOR_DMA_MODE00	0x00000000 // DMA mode outside req 2ch B'00'
#define   DMAOR_PRI_MODE10	0x00000200 // priority mode B'10'
#define   DMAOR_PRI_ROUND	0x00000300 // priority mode round robin B'11'
#define   DMAOR_NMI_NONE	0x00000000 // NMI NONE
#define   DMAOR_MASTER_EN	0x00000001 // DMA master enable
#define DMABRGCR	0xfe3c0000         // DMABRG control reg

#define CHCR_STS_TRANS_END	0x00000002 // CHCR transfer end status
#define DMAOR_STS_ADR_ERR	0x00000004 // DMAOR address error status
#define DMAOR_STS_NMI_FLG	0x00000002 // DMAOR NMI status
#define DMARCR_STS_CH7_REQ	0x80000000 // DMARCR CH7 req status
#define DMARCR_STS_CH6_REQ	0x40000000 // DMARCR CH6 req status
#define DMARCR_STS_CH5_REQ	0x20000000 // DMARCR CH5 req status
#define DMARCR_STS_CH4_REQ	0x10000000 // DMARCR CH4 req status
#define DMARCR_STS_CH3_REQ	0x08000000 // DMARCR CH3 req status
#define DMARCR_STS_CH2_REQ	0x04000000 // DMARCR CH2 req status
#define DMARCR_STS_CH1_REQ	0x02000000 // DMARCR CH1 req status
#define DMARCR_STS_CH0_REQ	0x01000000 // DMARCR CH0 req status

/* Low voltage Setting */
#define   CLKSTPCLR00		0xfe0a0010
#define   CLKSTPCLR00_LCDC_ON	0x00100000

/* CPG  - Clock Genelate */
#define DCKDR		0xfe0a0020 /* Clock Divide Register */
#define   DCKDR_PLL3EN		0x00000008 // PLL3 ON
#define   DCKDR_DOKEN		0x00000080 // DCK Enable
#define   DCKDR_DIV1		0x00000001 // n/1
#define   DCKDR_DIV2		0x00000002 // n/2
#define   DCKDR_DIV3		0x00000003 // n/3
#define FRQCR		0xffc00000 /* Clock Divide Register */
#define   FRQCR_CKOEN		0x0800	// CKIO Enable
#define   FRQCR_PLL1EN		0x0400	// PLL1 Enable
#define   FRQCR_PLL2EN		0x0200	// PLL2 Enable
#define   FRQCR_DIV1		0x0000	// n/1
#define   FRQCR_DIV2		0x0040	// n/2
#define   FRQCR_DIV3		0x0080	// n/3
#define   FRQCR_DIV4		0x00c0	// n/4
#define   FRQCR_DIV6		0x0100	// n/6
#define   FRQCR_DIV8		0x0140	// n/8

/* LCDC - LDC Controller */
#define LDICKR		0xfe300c00 /* Input Clock Register */
#define   LDICKR_B_CLK		0x0000	// Bus clock
#define   LDICKR_P_CLK		0x1000	// Peripheral
#define   LDICKR_E_CLK		0x2000	// External
#define   LDICKR_DCDR1		0x0001	// denominator 1 (n/1)
#define   LDICKR_DCDR2		0x0002	// denominator 2 (n/2)
#define LDMTR		0xfe300c02 /* Module Type Register */
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
#define LDDFR		0xfe300c04 /* Data Format Register */
#define   LDDFR_PABD		(1 << 8)
#define   LDDFR_MONO_2		0x00 /* 1bpp */
#define   LDDFR_MONO_4		0x01 /* 2bpp */
#define   LDDFR_MONO_16		0x02 /* 4bpp */
#define   LDDFR_MONO_64		0x04 /* 6bpp */
#define   LDDFR_COLOR_16	0x0A /* 16 */
#define   LDDFR_COLOR_256	0x0C /* 256 */
#define   LDDFR_COLOR_32K	0x1D /* RGB:555 */
#define   LDDFR_COLOR_64K	0x2D /* RGB:565 */
#define LDSMR		0xfe300c06 /* Scan Mode Register */
#define   LDSMR_ROT		(1 << 13)
#define LDSARU		0xfe300c08 /* Start Address Register U */
#define LDSARL		0xfe300c0c /* Start Address Register L */
#define LDLAOR		0xfe300c10 /* Line Address Offset Register */
#define LDPALCR		0xfe300c12 /* Palette Control Register */
#define LDPR		0xfe300800 /* Palette Data Register */
#define LDHCNR		0xfe300c14 /* Horizontal Character Number Register */
#define LDHSYNR		0xfe300c16 /* Horizontal Sync Signal Register */
#define LDVDLNR		0xfe300c18 /* Vertical Display Line Number Register */
#define LDVTLNR		0xfe300c1a /* Vertical Total Line Number Register */
#define LDVSYNR		0xfe300c1c /* Vertical Sync Signal Register */
#define LDACLNR		0xfe300c1e /* AC Line Number Register */
#define LDINTR		0xfe300c20 /* Interrupt Control Register */
#define   LDINTR_VINTE		0x0100 /* vsync interrupt enable */
#define LDPMMR		0xfe300c24 /* Power Management Mode Register */
#define   LDPMMR_LPS1 (1 << 1)
#define   LDPMMR_LPS0 (1 << 0)
#define LDPSPR		0xfe300c26 /* Power Sequence Period Register */
#define LDCNTR		0xfe300c28 /* Control Register */
#define   LDCNTR_DON		(1 << 0)
#define   LDCNTR_DON2		(1 << 4)

/* General I/O Ports */
#define PACR		0xfe400000 /* Port A Control Register */
#define PBCR		0xfe400004 /* Port B Control Register */
#define PCCR		0xfe400008 /* Port C Control Register */
#define PDCR		0xfe40000c /* Port D Control Register */
#define PECR		0xfe400010 /* Port E Control Register */
#define PFCR		0xfe400014 /* Port F Control Register */
#define PGCR		0xfe400018 /* Port G Control Register */
#define PHCR		0xfe40001c /* Port H Control Register */
#define PJCR		0xfe400020 /* Port J Control Register */
#define PKCR		0xfe400024 /* Port K Control Register */

/* General I/O Ports */
#define PADR		0xfe400040 /* Port A Data Register */
#define PBDR		0xfe400044 /* Port B Data Register */
#define PCDR		0xfe400048 /* Port C Data Register */
#define PDDR		0xfe40004c /* Port D Data Register */
#define PEDR		0xfe400050 /* Port E Data Register */
#define PFDR		0xfe400054 /* Port F Data Register */
#define PGDR		0xfe400058 /* Port G Data Register */
#define PHDR		0xfe40005c /* Port H Data Register */
#define PJDR		0xfe400060 /* Port J Data Register */
#define PKDR		0xfe400064 /* Port K Data Register */

/* Pin Function Controller */
#define MDPUPR		0xfe4000a8 /* Mode PinUp control Register */
#define IPSELR		0xfe400034 /* Module Select Register      */
#define MODSELR		0xfe4000ac /* Mode Select Register        */
#define PCPUPR		0xfe400088 /* Port C Pull-Up control      */
#define PDPUPR		0xfe40008c /* Port D Pull-Up control      */
#define PEPUPR		0xfe400090 /* Port E Pull-Up control      */

/* ASIC Control */
#define GRDCLK		0xb0100140 /* GRDCLK CTRL */

/* SH7760 support function*/

extern int  sh7760fb_get_contrast(void);
extern int  sh7760fb_get_backlight(void);
extern void sh7760fb_set_contrast(int);
extern void sh7760fb_set_backlight(int);

#endif /* __ASM_SH_SH7760_FB_H */

