/*
 *  linux/drivers/mmc/mmc_pxa.h 
 *
 *  Author: Vladimir Shebordaev, Igor Oblakov   
 *  Copyright:  MontaVista Software Inc.
 *
 *  $Id: mmc_pxa.h,v 0.3.1.6 2002/09/25 19:25:48 ted Exp ted $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef __MMC_PXA_P_H__
#define __MMC_PXA_P_H__

#include <linux/completion.h>

/* PXA-250 MMC controller registers */

/* MMC_STRPCL */
#define MMC_STRPCL_STOP_CLK     (0x0001UL)
#define MMC_STRPCL_START_CLK        (0x0002UL)

/* MMC_STAT */
#define MMC_STAT_END_CMD_RES        (0x0001UL << 13)
#define MMC_STAT_PRG_DONE       (0x0001UL << 12)
#define MMC_STAT_DATA_TRAN_DONE     (0x0001UL << 11)
#define MMC_STAT_CLK_EN         (0x0001UL << 8)
#define MMC_STAT_RECV_FIFO_FULL     (0x0001UL << 7)
#define MMC_STAT_XMIT_FIFO_EMPTY    (0x0001UL << 6)
#define MMC_STAT_RES_CRC_ERROR      (0x0001UL << 5)
#define MMC_STAT_SPI_READ_ERROR_TOKEN   (0x0001UL << 4)
#define MMC_STAT_CRC_READ_ERROR     (0x0001UL << 3)
#define MMC_STAT_CRC_WRITE_ERROR    (0x0001UL << 2)
#define MMC_STAT_TIME_OUT_RESPONSE  (0x0001UL << 1)
#define MMC_STAT_READ_TIME_OUT      (0x0001UL)

#define MMC_STAT_ERRORS (MMC_STAT_RES_CRC_ERROR|MMC_STAT_SPI_READ_ERROR_TOKEN\
        |MMC_STAT_CRC_READ_ERROR|MMC_STAT_TIME_OUT_RESPONSE\
        |MMC_STAT_READ_TIME_OUT)

/* MMC_CLKRT */
#define MMC_CLKRT_20MHZ         (0x0000UL)
#define MMC_CLKRT_10MHZ         (0x0001UL)
#define MMC_CLKRT_5MHZ          (0x0002UL)
#define MMC_CLKRT_2_5MHZ        (0x0003UL)
#define MMC_CLKRT_1_25MHZ       (0x0004UL)
#define MMC_CLKRT_0_625MHZ      (0x0005UL)
#define MMC_CLKRT_0_3125MHZ     (0x0006UL)

/* MMC_SPI */
#define MMC_SPI_DISABLE         (0x00UL)
#define MMC_SPI_EN          (0x01UL)
#define MMC_SPI_CS_EN           (0x01UL << 2)
#define MMC_SPI_CS_ADDRESS      (0x01UL << 3)
#define MMC_SPI_CRC_ON          (0x01UL << 1)

/* MMC_CMDAT */
#define MMC_CMDAT_MMC_DMA_EN        (0x0001UL << 7)
#define MMC_CMDAT_INIT          (0x0001UL << 6)
#define MMC_CMDAT_BUSY          (0x0001UL << 5)
#define MMC_CMDAT_STREAM        (0x0001UL << 4)
#define MMC_CMDAT_BLOCK         (0x0000UL << 4)
#define MMC_CMDAT_WRITE         (0x0001UL << 3)
#define MMC_CMDAT_READ          (0x0000UL << 3)
#define MMC_CMDAT_DATA_EN       (0x0001UL << 2)
#define MMC_CMDAT_R1            (0x0001UL)
#define MMC_CMDAT_R2            (0x0002UL)
#define MMC_CMDAT_R3            (0x0003UL)

/* MMC_RESTO */
#define MMC_RES_TO_MAX          (0x007fUL) /* [6:0] */

/* MMC_RDTO */
#define MMC_READ_TO_MAX         (0x0ffffUL) /* [15:0] */

/* MMC_BLKLEN */
#define MMC_BLK_LEN_MAX         (0x03ffUL) /* [9:0] */

/* MMC_PRTBUF */
#define MMC_PRTBUF_BUF_PART_FULL       (0x01UL) 
#define MMC_PRTBUF_BUF_FULL		(0x00UL    )

/* MMC_I_MASK */
#define MMC_I_MASK_TXFIFO_WR_REQ        (0x01UL << 6)
#define MMC_I_MASK_RXFIFO_RD_REQ        (0x01UL << 5)
#define MMC_I_MASK_CLK_IS_OFF           (0x01UL << 4)
#define MMC_I_MASK_STOP_CMD         (0x01UL << 3)
#define MMC_I_MASK_END_CMD_RES          (0x01UL << 2)
#define MMC_I_MASK_PRG_DONE         (0x01UL << 1)
#define MMC_I_MASK_DATA_TRAN_DONE       (0x01UL)
#define MMC_I_MASK_ALL              (0x07fUL)


/* MMC_I_REG */
#define MMC_I_REG_TXFIFO_WR_REQ     (0x01UL << 6)
#define MMC_I_REG_RXFIFO_RD_REQ     (0x01UL << 5)
#define MMC_I_REG_CLK_IS_OFF        (0x01UL << 4)
#define MMC_I_REG_STOP_CMD      (0x01UL << 3)
#define MMC_I_REG_END_CMD_RES       (0x01UL << 2)
#define MMC_I_REG_PRG_DONE      (0x01UL << 1)
#define MMC_I_REG_DATA_TRAN_DONE    (0x01UL)
#define MMC_I_REG_ALL           (0x007fUL)

/* MMC_CMD */
#define MMC_CMD_INDEX_MAX       (0x006fUL)  /* [5:0] */
#define CMD(x)  (x)

/* MMC_ARGH */
/* MMC_ARGL */
/* MMC_RES */
/* MMC_RXFIFO */
#define MMC_RXFIFO_PHYS_ADDR 0x41100040 //MMC_RXFIFO physical address
/* MMC_TXFIFO */ 
#define MMC_TXFIFO_PHYS_ADDR 0x41100044 //MMC_TXFIFO physical address

/* implementation specific declarations */
#define PXA_MMC_BLKSZ_MAX (1<<9) /* actually 1023 */
#define PXA_MMC_NOB_MAX ((1<<16)-2)
#define PXA_MMC_BLOCKS_PER_BUFFER (2)

#define PXA_MMC_IODATA_SIZE (PXA_MMC_BLOCKS_PER_BUFFER*PXA_MMC_BLKSZ_MAX) /* 1K */

typedef enum _pxa_mmc_fsm {         /* command processing FSM */
    PXA_MMC_FSM_IDLE = 1,
    PXA_MMC_FSM_CLK_OFF,
    PXA_MMC_FSM_END_CMD,
    PXA_MMC_FSM_BUFFER_IN_TRANSIT,
    PXA_MMC_FSM_END_BUFFER, 
    PXA_MMC_FSM_END_IO, 
    PXA_MMC_FSM_END_PRG,
    PXA_MMC_FSM_ERROR
} pxa_mmc_state_t;

#define PXA_MMC_STATE_LABEL( state ) (\
    (state == PXA_MMC_FSM_IDLE) ? "IDLE" :\
    (state == PXA_MMC_FSM_CLK_OFF) ? "CLK_OFF" :\
    (state == PXA_MMC_FSM_END_CMD) ? "END_CMD" :\
    (state == PXA_MMC_FSM_BUFFER_IN_TRANSIT) ? "IN_TRANSIT" :\
    (state == PXA_MMC_FSM_END_BUFFER) ? "END_BUFFER" :\
    (state == PXA_MMC_FSM_END_IO) ? "END_IO" :\
    (state == PXA_MMC_FSM_END_PRG) ? "END_PRG" : "UNKNOWN" )

typedef enum _pxa_mmc_result {
    PXA_MMC_NORMAL = 0,
    PXA_MMC_INVALID_STATE = -1,
    PXA_MMC_TIMEOUT = -2,
    PXA_MMC_ERROR = -3
} pxa_mmc_result_t;

typedef u32 pxa_mmc_clkrt_t;

typedef char *pxa_mmc_iodata_t;
#ifdef PIO
typedef struct _pxa_mmc_piobuf_rec {
    char *pos; /* current buffer position */
    int   cnt; /* byte counter */
} pxa_mmc_piobuf_rec_t, *pxa_mmc_piobuf_t;
#else /* i.e. DMA */
typedef struct _pxa_mmc_dmabuf_rec { /* TODO: buffer ring, DMA irq completion */
	int chan; /* dma channel no */
	dma_addr_t phys_addr; /* iodata physical address */
	pxa_dma_desc *read_desc; /* input descriptor array virtual address */
	pxa_dma_desc *write_desc; /* output descriptor array virtual address */
	dma_addr_t read_desc_phys_addr; /* descriptor array physical address */
	dma_addr_t write_desc_phys_addr; /* descriptor array physical address */
	pxa_dma_desc *last_read_desc; /* last input descriptor 
				       * used by the previous transfer 
				       */
	pxa_dma_desc *last_write_desc; /* last output descriptor
					* used by the previous transfer 
					*/
} pxa_mmc_dmabuf_rec_t, *pxa_mmc_dmabuf_t;
#endif

typedef struct _pxa_mmc_iobuf_rec {
    ssize_t blksz; /* current block size in bytes */
    ssize_t bufsz; /* buffer size for each transfer */
    ssize_t nob; /* number of blocks pers buffer */ 
#ifndef PIO 
    pxa_mmc_dmabuf_rec_t buf; /* i.e. DMA buffer ring on the iodata */
#else /* i.e. DMA */
    pxa_mmc_piobuf_rec_t buf; /* PIO buffer accounting */
#endif
    pxa_mmc_iodata_t iodata; /* I/O data buffer */
} pxa_mmc_iobuf_rec_t, *pxa_mmc_iobuf_t;

typedef struct _pxa_mmc_hostdata_rec {
    pxa_mmc_state_t state; /* FSM */
#ifdef CONFIG_PM
    int suspended;
#endif
    pxa_mmc_iobuf_rec_t iobuf; /* data transfer state */
    
    int busy;   /* atomic busy flag */
    struct completion completion; /* completion */
#if CONFIG_MMC_DEBUG_IRQ
    int irqcnt;
    int timeo;
#endif
    
/* cached controller state */
    u32 mmc_i_reg;  /* interrupt last requested */
    u32 mmc_i_mask; /* mask to be set by intr handler */
    u32 mmc_stat;   /* status register at the last intr */
    u32 mmc_cmdat;  /* MMC_CMDAT at the last inr */
    u8 mmc_res[16]; /* response to the last command in host order */ 
    u32 saved_mmc_clkrt;
    u32 saved_mmc_resto;
    u32 saved_mmc_spi;
    u32 saved_drcmrrxmmc;
    u32 saved_drcmrtxmmc;

/* controller options */
    pxa_mmc_clkrt_t clkrt; /* current bus clock rate */
} pxa_mmc_hostdata_rec_t, *pxa_mmc_hostdata_t;

#define PXA_MMC_STATUS( ctrlr ) (((pxa_mmc_hostdata_t)ctrlr->host_data)->mmc_stat)
#define PXA_MMC_RESPONSE( ctrlr, idx ) ((((pxa_mmc_hostdata_t)ctrlr->host_data)->mmc_res)[idx])
#define PXA_MMC_CLKRT( ctrlr ) (((pxa_mmc_hostdata_t)ctrlr->host_data)->clkrt)  

#define SAVED_MMC_CLKRT (hostdata->saved_mmc_clkrt)
#define SAVED_MMC_RESTO (hostdata->saved_mmc_resto)
#define SAVED_MMC_SPI (hostdata->saved_mmc_spi)
#define SAVED_DRCMRRXMMC (hostdata->saved_drcmrrxmmc )
#define SAVED_DRCMRTXMMC (hostdata->saved_drcmrtxmmc )

static inline int __pxa_mmc_clkrt( int speed )
{
    return MMC_CLKRT_20MHZ; /* TODO */
}

/* PXA MMC controller specific card data */
typedef struct _pxa_mmc_card_data_rec { 
    pxa_mmc_clkrt_t clkrt;   /* clock rate to be set for the card */
} pxa_mmc_card_data_rec_t, *pxa_mmc_card_data_t;

#ifdef CONFIG_MMC_DEBUG
#undef MMC_DUMP_R1
#undef MMC_DUMP_R2
#undef MMC_DUMP_R3
#define MMC_DUMP_R2( ctrlr ) MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R2 response: 0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 15 ), \
PXA_MMC_RESPONSE( ctrlr, 14 ), \
PXA_MMC_RESPONSE( ctrlr, 13 ), \
PXA_MMC_RESPONSE( ctrlr, 12 ), \
PXA_MMC_RESPONSE( ctrlr, 11 ), \
PXA_MMC_RESPONSE( ctrlr, 10 ), \
PXA_MMC_RESPONSE( ctrlr, 9 ), \
PXA_MMC_RESPONSE( ctrlr, 8 ), \
PXA_MMC_RESPONSE( ctrlr, 7 ), \
PXA_MMC_RESPONSE( ctrlr, 6 ), \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ), \
PXA_MMC_RESPONSE( ctrlr, 0 ) );
#define MMC_DUMP_R1( ctrlr ) MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R1(b) response: 0x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ) );
#define MMC_DUMP_R3( ctrlr )	MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R3 response: 0x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ) );

#endif
#endif /* __MMC_PXA_P_H__ */
