/*
 * linux/arch/arm/mach-pxa/cotulla_dma.h 
 *
 * Copyright (c) 2001 Lineo Japan, inc.
 *
 * Based on:
 *   arch/arm/mach-sa1100/dma.h
 *   (C) 2000 Nicolas Pitre <nico@cam.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ChangeLog:
 *  12-10-2001 Lineo Japan, Inc.
 */

#ifndef __COTULLA_DMA_H
#define __COTULLA_DMA_H


/* fetch mode */
typedef struct dma_descriptor_group_s {
	unsigned long*	ddadr;	/* descriptor address reg */
	dma_addr_t	dsadr;		/* source address register */
	dma_addr_t	dtadr;		/* target address register */
	dma_addr_t	dcmd;		/* command address register */
}  dma_descriptor_group_t;

typedef struct dma_buf_s {
	int               size;		/* buffer size */
	dma_addr_t        dma_start;	/* starting DMA address */
	dma_addr_t        dma_ptr;	/* current DMA pointer position */
	int               ref;		/* number of DMA references */
	void*             id;		/* to identify buffer from outside */
	struct dma_buf_s* next;		/* next buffer to process */
}  dma_buf_t;

typedef struct {
	dmach_t        channel;   /* channel id */
	unsigned int   lock;      /* Device is allocated */
	const char*    device_id; /* Device name */
	dma_buf_t*     head;      /* where to insert buffers */
	dma_buf_t*     tail;      /* where to remove buffers */
	dma_buf_t*     curr;      /* buffer currently DMA'ed */
	dma_addr_t     dev_adr;	  /* device address */
	int	       burst_size;  /* Burst size */
	int	       mode;	  /* 1 if No-Descriptor fetch mode */
	int	       width;	  /* Width of the on-chip peripheral */
	int	       recv;	  /* 1 if Source addr is device */
	int            ready;     /* 1 if DMA can occur */
	int            active;    /* 1 if DMA is actually processing data */
	dma_callback_t callback;  /* ... to call when buffers are done */
	int            spin_size; /* >0 when DMA should spin and no more buf */
	dma_addr_t     spin_addr; /* DMA address to spin onto */
	int            spin_ref;  /* number of spinning references */
} cotulla_dma_t;

extern cotulla_dma_t dma_chan[MAX_COTULLA_DMA_CHANNELS];

#endif /* __COTULLA_DMA_H */
