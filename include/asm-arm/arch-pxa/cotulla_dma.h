/*
 * linux/include/asm-arm/arch-pxa/cotulla_dma.h
 * 
 * Copyright (c) 2002 Lineo Japan, Inc. 
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 * linux/include/asm-arm/arch-l7200/dma.h
 *
 * Copyright (C) 2000 Steve Hill (sjhill@cotw.com)
 *
 * Changelog:
 *  08-29-2000	SJH	Created
 *  12-11-2001  Lineo Japan, Inc.
 *  01-Aug-2002 Lineo Japan, Inc.  for ARCH_PXA
 */
#ifndef __ASM_ARCH_COTULLA_DMA_H
#define __ASM_ARCH_COTULLA_DMA_H

#include <linux/config.h>
#include <asm/arch/hardware.h>

#ifndef CONFIG_DMA_COTULLA
#define CONFIG_DMA_COTULLA 1
#endif

/*
 * This is the maximum DMA address that can be DMAd to.
 * There should not be more than (0xd0000000 - 0xc0000000)
 * bytes of RAM.
 */
//#define MAX_DMA_ADDRESS         0xd0000000
//#define MAX_DMA_CHANNELS        0

#define DMA_S0                  0

#if defined(CONFIG_DMA_COTULLA)

#define COTULLA_DMA_CHANNELS      16
#define MAX_COTULLA_DMA_CHANNELS  COTULLA_DMA_CHANNELS

typedef unsigned int dma_device_t;

typedef void (*dma_callback_t)(void* buf_id, int size);

/* DMA API */
extern int cotulla_disable_dma(dmach_t channel);
extern void cotulla_enable_dma(dmach_t channel);
extern void cotulla_disable_dma_irq(dmach_t channel);
extern void cotulla_enable_dma_irq(dmach_t channel);

extern unsigned long cotulla_dma_is_busy(dmach_t channel);
extern unsigned long cotulla_dma_is_interrupt(dmach_t channel);

extern int  cotulla_get_dma_list(char* buf);
extern int  cotulla_request_dma(dmach_t* channel, const char* device_id);
extern int  cotulla_dma_set_callback(dmach_t channel, dma_callback_t cb);
extern int  cotulla_dma_set_device(dmach_t channel, dma_device_t device,
		 dma_addr_t dev_adr, int bsize, int width, int mode, int recv);
extern int  cotulla_dma_set_spin(dmach_t channel, dma_addr_t addr, int size);
extern int  cotulla_dma_queue_buffer(dmach_t channel,
				   void* buf_id, dma_addr_t data, int size);
extern int  cotulla_dma_get_current(dmach_t channel,
				  void** buf_id, dma_addr_t* addr);
extern int  cotulla_dma_stop(dmach_t channel);
extern int  cotulla_dma_resume(dmach_t channel);
extern int  cotulla_dma_flush_all(dmach_t channel);
extern void cotulla_free_dma(dmach_t channel);

#endif

#endif /* __ASM_ARCH_COTULLA_DMA_H */
