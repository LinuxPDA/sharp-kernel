#ifndef __ASM_SH_DMA_H
#define __ASM_SH_DMA_H

#include <linux/config.h>
#include <asm/io.h>		/* need byte IO */

#define MAX_DMA_CHANNELS 8
#if defined(CONFIG_CPU_SUBTYPE_SH7760)
#define SH_MAX_DMA_CHANNELS 8
#else	/* ! CONFIG_CPU_SUBTYPE_SH7760 */
#define SH_MAX_DMA_CHANNELS 4
#endif	/* ! CONFIG_CPU_SUBTYPE_SH7760 */

/* The maximum address that we can perform a DMA transfer to on this platform */
/* Don't define MAX_DMA_ADDRESS; it's useless on the SuperH and any
   occurrence should be flagged as an error.  */
/* But... */
/* XXX: This is not applicable to SuperH, just needed for alloc_bootmem */
#define MAX_DMA_ADDRESS      (PAGE_OFFSET+0x10000000)

#if defined(__sh3__)
#define SAR ((unsigned long[]){0xa4000020,0xa4000030,0xa4000040,0xa4000050})
#define DAR ((unsigned long[]){0xa4000024,0xa4000034,0xa4000044,0xa4000054})
#define DMATCR ((unsigned long[]){0xa4000028,0xa4000038,0xa4000048,0xa4000058})
#define CHCR ((unsigned long[]){0xa400002c,0xa400003c,0xa400004c,0xa400005c})
#define DMAOR 0xa4000060UL
#elif defined(__SH4__)
#if defined(CONFIG_CPU_SUBTYPE_SH7760)
#define SAR 	((unsigned long[]){0xffa00000,0xffa00010,0xffa00020,0xffa00030,0xffa00050,0xffa00060,0xffa00070,0xffa00080})
#define DAR 	((unsigned long[]){0xffa00004,0xffa00014,0xffa00024,0xffa00034,0xffa00054,0xffa00064,0xffa00074,0xffa00084})
#define DMATCR 	((unsigned long[]){0xffa00008,0xffa00018,0xffa00028,0xffa00038,0xffa00058,0xffa00068,0xffa00078,0xffa00088})
#define CHCR 	((unsigned long[]){0xffa0000c,0xffa0001c,0xffa0002c,0xffa0003c,0xffa0005c,0xffa0006c,0xfffa0007c,0xffa0008c})
#define DMAOR 0xffa00040UL
/******************************************************************
 * DMA module
 ******************************************************************/
#define DMATCR0		0xffa00008
#define CHCR0		0xffa0000c
/* DMAC - Direct Memory Access Controler */
#define DMARCR		0xfe090008	// DMA Request Control Reg
#define   DMARCR_RPR_ROUND	0x00030000 // priority round robin
#define   DMARCR_RPR_MODE01	0x00010000 // priority mode b'01'
#define   DMARCR_DREQ1_START	0x00000040 // DREQ1 detect start edge
#define DMARSRA		0xfe090000	// Req Resource Select A
#define DMARSRB		0xfe090004
#define   DMARSRA_CH0_WEN	0x80000000 // CH0 write enable
#define   DMARSRA_CH1_WEN	0x00800000 // CH0 write enable
//#define DMAOR		0xffa00040	// Operation Reg
#define   DMAOR_DMS_DMABRG	0x0000c000 // DMA mode DMABRG B'11'
#define   DMAOR_DMA_MODE00	0x00000000 // DMA mode outside req 2ch B'00'
#define   DMAOR_PRI_MODE10	0x00000200 // priority mode B'10'
#define   DMAOR_PRI_ROUND	0x00000300 // priority mode round robin B'11'
#define   DMAOR_NMI_NONE	0x00000000 // NMI NONE
#define   DMAOR_MASTER_EN	0x00000001 // DMA master enable
#define DMABRGCR	0xfe3c0000         // DMABRG control reg
#define   DMABRGCR_A1TXHE	0x20000000	// SSI1 half data send interrupt
#define   DMABRGCR_A1TXEE	0x10000000	// SSI1 all data send interrupt
#define   DMABRGCR_A1TXHF	0x00200000	// SSI1 helf data send flag
#define   DMABRGCR_A1TXEF	0x00100000	// SSI1 all data send flag
#define   DMABRGCR_A0TXEE	0x01000000	// SSI0 all data send interrupt
#define   DMABRGCR_A0TXEF	0x00010000	// SSI0 all data send flag
#define   DMABRGCR_A0RXHE	0x08000000	// SSI0 half data recieve interrupt
#define   DMABRGCR_A0RXEE	0x04000000	// SSI0 all data recieve interrupt
#define   DMABRGCR_A0RXHF	0x00080000	// SSI0 helf data recieve flag
#define   DMABRGCR_A0RXEF	0x00040000	// SSI0 all data recieve flag
#define DMAATXSAR0	0xfe3c0040		// DMA AUDIO 0
#define DMAARXDAR0	0xfe3c0044
#define DMAATXTCR0	0xfe3c0048
#define DMAARXTCR0	0xfe3c004c
#define DMAACR0		0xfe3c0050
#define DMAATXSAR1	0xfe3c0060		// DMA AUDIO 1
#define DMAARXDAR1	0xfe3c0064
#define DMAATXTCR1	0xfe3c0068
#define DMAARXTCR1	0xfe3c006c
#define DMAACR1		0xfe3c0070
#define   DMAACR_RAM_NOALIGN	0x00000000	// no alignment
#define   DMAACR_RAM_BYTEALIGN 	0x01000000	// byte * 4
#define   DMAACR_RAM_WORDALIGN	0x02000000	// word * 2
#define   DMAACR_RAR		0x00040000	// receive DMA auto reload
#define   DMAACR_RDS		0x00020000	// receive DMA teminate
#define   DMAACR_RDE		0x00010000	// receive DMA start
#define   DMAACR_TAM_NOALIGN	0x00000000	// no alignment
#define   DMAACR_TAM_BYTEALIGN	0x00000100	// byte * 4
#define   DMAACR_TAM_WORDALIGN  0x00000200	// word * 2
#define   DMAACR_TAR		0x00000004	// send DMA auto reload
#define   DMAACR_TDS		0x00000002	// send DMA terminate
#define   DMAACR_TDE		0x00000001	// send DMA start

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

/* DMABRG IRQ */
#define DMABRGI0		68	// USB address error interrupt
#define DMABRGI1		69	// all data transfer interrupt
#define DMABRGI2		70	// half data transfer interrupt

#else	/* ! CONFIG_CPU_SUBTYPE_SH7760 */
#define SAR ((unsigned long[]){0xbfa00000,0xbfa00010,0xbfa00020,0xbfa00030})
#define DAR ((unsigned long[]){0xbfa00004,0xbfa00014,0xbfa00024,0xbfa00034})
#define DMATCR ((unsigned long[]){0xbfa00008,0xbfa00018,0xbfa00028,0xbfa00038})
#define CHCR ((unsigned long[]){0xbfa0000c,0xbfa0001c,0xbfa0002c,0xbfa0003c})
#define DMAOR 0xbfa00040UL
#endif	/* ! CONFIG_CPU_SUBTYPE_SH7760 */
#endif	/* __SH4__ */

#if defined(CONFIG_CPU_SUBTYPE_SH7760)
#define DMTE_IRQ ((int[]){DMTE0_IRQ,DMTE1_IRQ,DMTE2_IRQ,DMTE3_IRQ,DMTE4_IRQ,DMTE5_IRQ,DMTE6_IRQ,DMTE7_IRQ})
#else	/* ! CONFIG_CPU_SUBTYPE_SH7760 */
#define DMTE_IRQ ((int[]){DMTE0_IRQ,DMTE1_IRQ,DMTE2_IRQ,DMTE3_IRQ})
#endif	/* ! CONFIG_CPU_SUBTYPE_SH7760 */

#define DMA_MODE_READ	0x00	/* I/O to memory, no autoinit, increment, single mode */
#define DMA_MODE_WRITE	0x01	/* memory to I/O, no autoinit, increment, single mode */
#define DMA_AUTOINIT	0x10

#define REQ_L	0x00000000
#define REQ_E	0x00080000
#define RACK_H	0x00000000
#define RACK_L	0x00040000
#define ACK_R	0x00000000
#define ACK_W	0x00020000
#define ACK_H	0x00000000
#define ACK_L	0x00010000
#define DM_INC	0x00004000
#define DM_DEC	0x00008000
#define SM_INC	0x00001000
#define SM_DEC	0x00002000
#define RS_DUAL	0x00000000
#define RS_IN	0x00000200
#define RS_OUT	0x00000300
#define TM_BURST 0x0000080
#define TS_8	0x00000010
#define TS_16	0x00000020
#define TS_32	0x00000030
#define TS_64	0x00000000
#define TS_BLK	0x00000040
#define CHCR_DE 0x00000001
#define CHCR_TE 0x00000002
#define CHCR_IE 0x00000004

#define DMAOR_COD	0x00000008
#define DMAOR_AE	0x00000004
#define DMAOR_NMIF	0x00000002
#define DMAOR_DME	0x00000001

struct dma_info_t {
	unsigned int chan;
	unsigned int mode_read;
	unsigned int mode_write;
	unsigned long dev_addr;
	unsigned int mode;
	unsigned long mem_addr;
	unsigned int count;
};

static __inline__ void clear_dma_ff(unsigned int dmanr){}

/* These are in arch/sh/kernel/dma.c: */
extern unsigned long claim_dma_lock(void);
extern void release_dma_lock(unsigned long flags);
extern void setup_dma(unsigned int dmanr, struct dma_info_t *info);
extern void enable_dma(unsigned int dmanr);
extern void disable_dma(unsigned int dmanr);
extern void set_dma_mode(unsigned int dmanr, char mode);
extern void set_dma_addr(unsigned int dmanr, unsigned int a);
extern void set_dma_count(unsigned int dmanr, unsigned int count);
extern int get_dma_residue(unsigned int dmanr);

#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy 	(0)
#endif


#endif /* __ASM_SH_DMA_H */
