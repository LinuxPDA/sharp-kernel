/*
 * linux/include/asm-arm/arch-l7200/dma.h
 *
 * Copyright (C) 2000 Steve Hill (sjhill@cotw.com)
 *
 * Changelog:
 *  08-29-2000	SJH	Created
 *  11-12-2001  Lineo Japan, Inc.
 */
#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H 1

#include <linux/config.h>
#include <asm/arch/hardware.h>

#ifndef CONFIG_DMA_L7200
#define CONFIG_DMA_L7200 1
#endif

/*
 * This is the maximum DMA address that can be DMAd to.
 * There should not be more than (0xd0000000 - 0xc0000000)
 * bytes of RAM.
 */
#define MAX_DMA_ADDRESS         0xd0000000
#define MAX_DMA_CHANNELS        0

#define DMA_S0                  0

#if defined(CONFIG_DMA_L7200)

#define L7200_DMA_CHANNELS      8
#define MAX_L7200_DMA_CHANNELS  L7200_DMA_CHANNELS

typedef unsigned int dma_device_t;

typedef void (*dma_callback_t)(void* buf_id, int size);

/* DMA API */
extern void l7200_disable_dma(dmach_t channel);
extern void l7200_enable_dma(dmach_t channel);
extern void l7200_clear_dma_irq(dmach_t channel);
extern void l7200_disable_dma_irq(dmach_t channel);
extern void l7200_enable_dma_irq(dmach_t channel);

extern unsigned long l7200_dma_is_busy(dmach_t channel);
extern unsigned long l7200_dma_is_interrupt(dmach_t channel);

extern int  l7200_get_dma_list(char* buf);
extern int  l7200_request_dma(dmach_t* channel, const char* device_id);
extern int  l7200_dma_set_callback(dmach_t channel, dma_callback_t cb);
extern int  l7200_dma_set_device(dmach_t channel, dma_device_t device,
				 int req, int ctrl);
extern int  l7200_dma_set_spin(dmach_t channel, dma_addr_t addr, int size);
extern int  l7200_dma_queue_buffer(dmach_t channel,
				   void* buf_id, dma_addr_t data, int size);
extern int  l7200_dma_get_current(dmach_t channel,
				  void** buf_id, dma_addr_t* addr);
extern int  l7200_dma_stop(dmach_t channel);
extern int  l7200_dma_resume(dmach_t channel);
extern int  l7200_dma_flush_all(dmach_t channel);
extern void l7200_free_dma(dmach_t channel);

#endif

#endif /* __ASM_ARCH_DMA_H */
