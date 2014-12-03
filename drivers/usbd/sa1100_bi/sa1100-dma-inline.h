/*
 * linux/drivers/usbd/sa1100_bi/sa1100-dma-inline.h 
 *
 * Copyright (c) 2000, 2001 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * Adapted from: Copyright (C)  Compaq Computer Corporation, 1998, 1999
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *      Bruce Balden <balden@lineo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */



/*
 * Maximum physical DMA buffer size
 */
#define MAX_DMA_SIZE		0x1fff
#define MAX_DMA_ORDER		12


/*
 * DMA control register structure
 */
typedef struct {
	volatile u_long DDAR;
	volatile u_long SetDCSR;
	volatile u_long ClrDCSR;
	volatile u_long RdDCSR;
	volatile dma_addr_t DBSA;
	volatile u_long DBTA;
	volatile dma_addr_t DBSB;
	volatile u_long DBTB;
} dma_regs_t;


/*
 * DMA buffer structure
 */
struct dma_buf_s {
	int size;		/* buffer size */
	dma_addr_t dma_start;	/* starting DMA address */
	dma_addr_t dma_ptr;	/* current DMA pointer position */
	int ref;		/* number of DMA references */
	void *id;		/* to identify buffer from outside */
	struct dma_buf_s *next;	/* next buffer to process */
};

#include "dma.h"

extern sa1100_bi_dma_t sa1100_bi_dma_chan[MAX_DMA_CHANNELS];

#define UDCCR  ((volatile unsigned int *)io_p2v(0x80000000))
#define UDCAR ((volatile unsigned int *)io_p2v(0x80000004))

static __inline__ void _sa1100_bi_dma_run(dmach_t channel)
{
    sa1100_bi_dma_t *dma = &sa1100_bi_dma_chan[channel];
    dma_regs_t *regs = dma->regs;
    if ( dma->active == ACTIVEA) {
        regs->SetDCSR = DCSR_STRTA | DCSR_RUN;
    }
    else {
        regs->SetDCSR = DCSR_STRTB | DCSR_RUN;
    }
}

static __inline__ int _sa1100_bi_dma_queue_buffer_irq(dmach_t channel, dma_addr_t data, int size)
{
    sa1100_bi_dma_t *dma = &sa1100_bi_dma_chan[channel];
    dma_regs_t *regs = dma->regs;
    int status;

    status = regs->RdDCSR;
    if ((status & DCSR_STRTA) && (status & DCSR_STRTB)) {
        return -EBUSY;
    }
    if (((status & DCSR_BIU) && (status & DCSR_STRTB)) || (!(status & DCSR_BIU) && !(status & DCSR_STRTA))) {
        regs->ClrDCSR = DCSR_DONEA | DCSR_STRTA;
        regs->DBSA = data;
        regs->DBTA = size;
        // Once is good, twice is better....
        regs->DBSA = data;
        regs->DBTA = size;
        dma->active = ACTIVEA;
    } else {
        regs->ClrDCSR = DCSR_DONEB | DCSR_STRTB;
        regs->DBSB = data;
        regs->DBTB = size;
        // Once is good, twice is better....
        regs->DBSB = data;
        regs->DBTB = size;
        dma->active = ACTIVEB;
    }
    return 0;
}

static __inline__ int _sa1100_bi_dma_flush_all_irq(dmach_t channel)
{
    sa1100_bi_dma_t *dma = &sa1100_bi_dma_chan[channel];
    dma->regs->ClrDCSR = DCSR_STRTA | DCSR_STRTB | DCSR_RUN | DCSR_IE;
    return 0;
}

static __inline__ int _sa1100_bi_dma_stop_get_current_irq(dmach_t channel, dma_addr_t *addr)
{
    sa1100_bi_dma_t *dma = &sa1100_bi_dma_chan[channel];
    dma_regs_t *regs = dma->regs;

    // addr sometimes can be set incorrectly, the caller must do bounds checking
    switch(dma->active) {
    case ACTIVEA:
        regs->ClrDCSR = DCSR_RUN | DCSR_STRTA | DCSR_RUN | DCSR_IE;
        dma->active = 0;
        *addr = regs->DBSA;     // not reliable
        return 0;
    case ACTIVEB:
        regs->ClrDCSR = DCSR_RUN | DCSR_STRTB | DCSR_RUN | DCSR_IE;
        dma->active = 0;
        *addr = regs->DBSB;     // not reliable
        return 0;
    default:
        dma->active = 0;
        *addr = 0;
        return -ENXIO;
    }
}

