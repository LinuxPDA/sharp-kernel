/*
 * linux/drivers/usbd/sa1100_bi/dma.h
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


typedef struct dma_buf_s dma_buf_t;

#define ACTIVEA 1
#define ACTIVEB 2
#define LASTA   3
#define LASTB   0

typedef struct {
	const char *device_id;  /* Device name */
	int active;             /* 1 if DMA is actually processing data */
	dma_regs_t *regs;       /* points to appropriate DMA registers */
#if defined(CONFIG_SA1100_NEW_DMA_COOPERATION)
	void *old_ops;
#endif

} sa1100_bi_dma_t;

extern sa1100_bi_dma_t sa1100_bi_dma_chan[MAX_DMA_CHANNELS];


void sa1100_bi_dma_done (sa1100_bi_dma_t *dma);
int sa1100_bi_request_dma(dmach_t * channel, const char *device_id, dma_device_t device);
void sa1100_bi_free_dma(dmach_t channel);

int __init sa1100_bi_init_dma(void);


#ifdef CONFIG_SA1111
#define channel_is_sa1111_sac(ch) \
	((ch) >= SA1111_SAC_DMA_BASE && \
	 (ch) <  SA1111_SAC_DMA_BASE + SA1111_SAC_DMA_CHANNELS)
#else
#define channel_is_sa1111_sac(ch) (0)
#endif

