/*
 * arch/arm/mach-pxa/cotulla_dma.c
 *
 * Support functions for the XScale cotulla internal DMA channels.
 *
 * Copyright (C) 2001 Lineo Japan, Inc.
 *
 * ChangeLog:
 *   12-10-2001 Lineo Japan, Inc.
 *   01-Aug-2002 Lineo Japan, Inc.  for ARCH_PXA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/arch/cotulla_dma.h>

#define MAX_DMA_SIZE	0x1fff
#define MAX_DMA_ORDER	12

#include "cotulla_dma.h"

//#define DEBUG_PRINT(s)		printk(s)
//#define DEBUG_PRINTS(s, d)		printk(s, d)
#define DEBUG_PRINT(s)
#define DEBUG_PRINTS(s, d)
#define DEBUG_PRINTSS(s, d, d2)

static cotulla_dma_t dma_chan[MAX_COTULLA_DMA_CHANNELS];
static cotulla_dma_t dma_restore[1];

typedef struct DMADesctriptorS {
	unsigned long DDADR;
	unsigned long DSADR;
	unsigned long DTADR;
	unsigned long DCMD;
} DMADesctriptorT;

DMADesctriptorT *phys_disc[COTULLA_DMA_CHANNELS];
DMADesctriptorT *virt_disc[COTULLA_DMA_CHANNELS];

#define DMA_DISCRIPTORS		128
#define DCMD_LEN_MASK		0x1FFFU


void dma_register_dump(void)
{
	int i;
	unsigned int val;
	for (i=0; i<16; i++) {
		val = DCSR(i);
		printk("DCSR%d=0x%08x\n", i, val);
		val = DDADR(i);
		printk("DDADR%d=0x%08x\n", i, val);
		val = DSADR(i);
		printk("DSADR%d=0x%08x\n", i, val);
		val = DTADR(i);
		printk("DTADR%d=0x%08x\n", i, val);
		val = DCMD(i);
		printk("DCMD%d=0x%08x\n", i, val);
	}
	for (i=2; i<39; i++) {
		if (i == 15 || i == 16 || i == 23 || i == 24 ||
							i == 29 || i == 34)
			continue;
		val = DRCMR(i);
	}
}

void set_channel_map(dmach_t ch, dma_device_t dev)
{
	DRCMR(dev) = 0x00L;
	DRCMR(dev) |= 0x80L + ch;

	virt_disc[ch] = consistent_alloc(GFP_KERNEL | GFP_DMA,
				sizeof(DMADesctriptorT) * DMA_DISCRIPTORS,
				(dma_addr_t *)&phys_disc[ch]);

}

void clear_channel_map(dmach_t ch)
{
	int i;
	for (i=0; i<39; i++) {
		if ((DRCMR(i) & 0x0f) == ch) {
			DRCMR(i) = 0x0L;
			break;
		}
	}
	if (virt_disc[ch] != NULL) {
		consistent_free(virt_disc[ch],
				sizeof(DMADesctriptorT) * DMA_DISCRIPTORS,
				(dma_addr_t)phys_disc[ch]);
	}
}

int cotulla_stop_dma(dmach_t channel)
{
	unsigned long timeo = jiffies + (HZ*2);
	DCSR(channel) &= ~DCSR_RUN;
	for (;;) {
		if ((DCSR(channel) & DCSR_STOPSTATE)) /* stoped state */
			break;
		if (time_after(jiffies, timeo))
			return -EIO;
	}
	
	return 0;
}

void cotulla_enable_dma(dmach_t channel)
{
	DCSR(channel) |= DCSR_RUN;
}

void cotulla_disable_dma_irq(dmach_t channel)
{
	if (DCSR(channel) & DCSR_ENDINTR) {
		DCSR(channel) |= DCSR_ENDINTR;
	}
}

void cotulla_enable_dma_irq(dmach_t channel)
{
	if (!(DCSR(channel) & DCSR_ENDINTR)) {
		DCSR(channel) |= DCSR_ENDINTR;
	}
}

int cotulla_dma_wait_pending(dmach_t channel)
{
	unsigned long timeo1 = 100000;
	unsigned long timeo2 = 100;
	while (DCSR(channel) & DCSR_REQPEND) {
		if (timeo2 != 0 && timeo1 ==0) {
			timeo1 = 100000;
			timeo2--;
		}
		if (timeo1 == 0) {
			DEBUG_PRINT("dma still has a pennding request.\n");
			return -1;
		}
		timeo1--;
	}
	return 0;
}

unsigned long cotulla_dma_is_busy(dmach_t channel)
{
	return (DCSR(channel) & DCSR_STOPSTATE) ? 0 : 1;
}

unsigned long cotulla_dma_is_interrupt(dmach_t channel)
{
	return ((DCSR(channel) & DCSR_STOPIRQEN) ||
			(DCSR(channel) & DCSR_STARTINTR) ||
			(DCSR(channel) & DCSR_ENDINTR));
}

/*
 * DMA processing...
 */
static int cotulla_start_dma(cotulla_dma_t* dma, dma_addr_t dma_ptr, int size)
{
	dmach_t channel = dma->channel;

	if (cotulla_dma_is_busy( channel )) {
		return -EBUSY;
	}

	if (!dma->recv) {
		DEBUG_PRINT("1\n");
		DSADR(channel) = dma_ptr & ~3L;
		DTADR(channel) = dma->dev_adr & ~3L;
		DCMD(channel) = DCMD_INCSRCADDR | DCMD_FLOWTRG | size;
	} else {
		DEBUG_PRINT("2\n");
		DSADR(channel) = dma->dev_adr & ~3L;
		DTADR(channel) = dma_ptr & ~3L;
		DCMD(channel) = DCMD_INCTRGADDR | DCMD_FLOWSRC | size;
	}
	switch (dma->burst_size) {
		case 8: DCMD(channel) |= DCMD_BURST8; break;
		case 16: DCMD(channel) |= DCMD_BURST16; break;
		case 32: DCMD(channel) |= DCMD_BURST32; break;
		default: return -EINVAL;
	}
	switch (dma->width) {
		case 1: DCMD(channel) |= DCMD_WIDTH1; break;
		case 2: DCMD(channel) |= DCMD_WIDTH2; break;
		case 4: DCMD(channel) |= DCMD_WIDTH4; break;
		default: return -EINVAL;
	}
#if 1
	DCMD(channel) |= DCMD_ENDIRQEN;
#else
	if (dma->active) {
		DCMD(channel) |= DCMD_ENDIRQEN;
		dma->active = 0;
	} else
		DCMD(channel) &= ~DCMD_ENDIRQEN;
#endif

	
	cotulla_enable_dma_irq(channel);
	cotulla_enable_dma(channel);
	return 0;
}

static void cotulla_non_descriptor_dma(cotulla_dma_t* dma)
{
	dma_buf_t* buf;
	int chunksize;

	for (;;) {
		buf = dma->tail;

		if (!buf) {
			if (dma->spin_size && dma->spin_ref >= 0) {
				chunksize = dma->spin_size;
				if (chunksize > MAX_DMA_SIZE) {
					chunksize = (1 << MAX_DMA_ORDER);
				}
				while (cotulla_start_dma(dma, dma->spin_addr,
						       chunksize) == 0) {
					dma->spin_ref++;
				}
				if (dma->curr != NULL) {
					dma->spin_ref = -dma->spin_ref;
				}
			}
			break;
		}

		chunksize = buf->size;
		if (chunksize > MAX_DMA_SIZE) {
			chunksize = (1 << MAX_DMA_ORDER);
		}
		if ((buf->size - chunksize) == 0) {
			dma->active = 1;
		}
		if (cotulla_start_dma(dma, buf->dma_ptr, chunksize) != 0) {
			break;
		}
		if (!dma->curr) {
			dma->curr = buf;
		}
		buf->ref = 1;
		buf->dma_ptr += chunksize;
		buf->size    -= chunksize;
		if (buf->size == 0) {
			dma->tail =  buf->next;
		}
	}
}

static void cotulla_process_dma(cotulla_dma_t* dma)
{
	dma_buf_t* buf;
	int chunksize;
	int i, len;
	int channel = dma->channel;
	DMADesctriptorT *v_disc = 0;
	DMADesctriptorT *p_disc = phys_disc[channel];
	unsigned long phys_src;

	/* Non-descriptor mode */
	if (dma->mode) {
		cotulla_non_descriptor_dma(dma);
		return;
	}

	/* Descriptor mode */
	for (;;) {
		buf = dma->tail;
		if (!buf) {
				if (dma->spin_size && dma->spin_ref >= 0) {
				while (cotulla_dma_is_busy(channel)) {
					dma->spin_ref++;
				}
				DDADR(channel) = (unsigned long)p_disc;
				v_disc = virt_disc[channel];
				chunksize = dma->spin_size;
				if (chunksize > MAX_DMA_SIZE)
					chunksize = (1 << MAX_DMA_ORDER);
				if (!dma->recv) {
					v_disc->DSADR = (unsigned long)buf->dma_ptr & ~3UL;
				} else {
					v_disc->DSADR = dma->dev_adr & ~3UL;
				}
		
				v_disc ->DCMD = (DCMD_LEN_MASK & chunksize) |
						DCMD_FLOWTRG	|
						DCMD_INCSRCADDR;
				if (!dma->recv) {
					v_disc ->DCMD |= DCMD_FLOWTRG;
					v_disc ->DCMD |= DCMD_INCSRCADDR;
				} else {
					v_disc ->DCMD |= DCMD_FLOWSRC;
					v_disc ->DCMD |= DCMD_INCTRGADDR;
				}
				switch (dma->burst_size) {
					case 8 : v_disc ->DCMD |= DCMD_BURST8; break;
					case 16: v_disc ->DCMD |= DCMD_BURST16; break;
					case 32: v_disc ->DCMD |= DCMD_BURST32; break;
					default: return;
				}
				switch (dma->width) {
					case 1: v_disc ->DCMD |= DCMD_WIDTH1; break;
					case 2: v_disc ->DCMD |= DCMD_WIDTH2; break;
					case 4: v_disc ->DCMD |= DCMD_WIDTH4; break;
					default: return;
				}
				v_disc ->DCMD |= DCMD_ENDIRQEN;
				v_disc->DDADR = ((unsigned long)p_disc) | DDADR_STOP;
				
				cotulla_enable_dma_irq(channel);
				cotulla_enable_dma(channel);
				if (dma->curr != NULL) {
					dma->spin_ref = -dma->spin_ref;
				}
			}
			break;
		}

		if (cotulla_dma_is_busy(channel)) {
			DEBUG_PRINT("busy!!\n");
			return;
		}
	
		//phys_src = (unsigned long)__virt_to_phys(buf->dma_ptr) & ~3UL;
		phys_src = (unsigned long)buf->dma_ptr & ~3UL;
		len = 0;
		DDADR(channel) = (unsigned long)p_disc;
		v_disc = virt_disc[channel];
		for (i=0; i<DMA_DISCRIPTORS; i++) {
			chunksize = buf->size;
			if (chunksize > MAX_DMA_SIZE)
				chunksize = (MAX_DMA_SIZE & ~3UL);
				//chunksize = (1 << MAX_DMA_ORDER);
			buf->size -= chunksize;
			v_disc->DSADR = phys_src + (len);
			len += chunksize;
			if (!dma->recv) {
				v_disc->DTADR = dma->dev_adr & ~3UL;
				v_disc->DSADR = (unsigned long)buf->dma_ptr & ~3UL;
			} else {
				v_disc->DTADR = (unsigned long)buf->dma_ptr & ~3UL;
				v_disc->DSADR = dma->dev_adr & ~3UL;
			}
			v_disc ->DCMD = DCMD_LEN_MASK & chunksize;
			if (!dma->recv) {
				v_disc ->DCMD |= DCMD_FLOWTRG;
				v_disc ->DCMD |= DCMD_INCSRCADDR;
			} else {
				v_disc ->DCMD |= DCMD_FLOWSRC;
				v_disc ->DCMD |= DCMD_INCTRGADDR;
			}
			switch (dma->burst_size) {
				case 8 : v_disc ->DCMD |= DCMD_BURST8; break;
				case 16: v_disc ->DCMD |= DCMD_BURST16; break;
				case 32: v_disc ->DCMD |= DCMD_BURST32; break;
				default: return;
			}
			switch (dma->width) {
				case 1: v_disc ->DCMD |= DCMD_WIDTH1; break;
				case 2: v_disc ->DCMD |= DCMD_WIDTH2; break;
				case 4: v_disc ->DCMD |= DCMD_WIDTH4; break;
				default: return;
			}
			if ((i == (DMA_DISCRIPTORS -1)) || (buf->size == 0)) {
				v_disc ->DCMD |= DCMD_ENDIRQEN;
				v_disc->DDADR = ((unsigned long)p_disc) | DDADR_STOP;
				break;
			} else {
				v_disc->DDADR = ((unsigned long)p_disc) +
					(sizeof(DMADesctriptorT) * (i + 1));
			}
				
			v_disc += 1;
		}
		
		
		cotulla_enable_dma_irq(channel);
		cotulla_enable_dma(channel);
		
		if (!dma->curr) {
			dma->curr = buf;
		}
		buf->ref = 1;
		buf->dma_ptr += len;
		if (buf->size == 0) {
			dma->tail =  buf->next;
		}
	}
}

void cotulla_dma_done(cotulla_dma_t* dma)
{
	dma_buf_t* buf = dma->curr;

	if (dma->spin_ref > 0) {
		dma->spin_ref--;
	}
	else if (buf) {
		buf->ref--;
		if (buf->ref == 0 && buf->size == 0) {
			dma->curr = buf->next;
			if (dma->curr == NULL) {
				dma->spin_ref = -dma->spin_ref;
			}
			if (dma->head == buf) {
				dma->head = NULL;
			}
			buf->size = buf->dma_ptr - buf->dma_start;
			if (dma->callback) {
				dma->callback(buf->id, buf->size);
			}
			kfree(buf);
		}
	}

	cotulla_process_dma(dma);
}

static void dma_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int channel;

	for (channel = 0; channel < COTULLA_DMA_CHANNELS; channel++) {
		if (cotulla_dma_is_interrupt( channel )) {
			cotulla_disable_dma_irq(channel);
			if (!cotulla_dma_is_busy( channel )) {
				cotulla_dma_t* dma = &dma_chan[channel];
				cotulla_dma_done(dma);
			}
		}
	}
}

/*
 * DMA interface functions
 */
int cotulla_get_dma_list(char* buf)
{
	int i, len = 0;

	for (i = 0; i < MAX_COTULLA_DMA_CHANNELS; i++) {
		if (dma_chan[i].lock)
			len += sprintf(buf + len, "%2d: %s\n",
				       i, dma_chan[i].device_id);
	}
	return len;
}

int cotulla_request_dma(dmach_t* channel, const char* device_id)
{
	cotulla_dma_t* dma = NULL;
	int ch;

	*channel = -1;
	for (ch = 0; ch < COTULLA_DMA_CHANNELS; ch++) {
		dma = &dma_chan[ch];
		if (xchg(&dma->lock, 1) == 0)
			break;
	}

	if (ch >= COTULLA_DMA_CHANNELS) {
		printk(KERN_ERR "%s: no free DMA channel available\n",
		       device_id);
		return -EBUSY;
	}

	dma = &dma_chan[ch];
	xchg(&dma->lock, 1);
	
	*channel = ch;
	dma->device_id = device_id;
	dma->callback  = NULL;
	dma->spin_size = 0;

	if (cotulla_stop_dma(ch) != 0)
		return -EBUSY;
	cotulla_disable_dma_irq(ch);
	clear_channel_map(ch);

	return 0;
}


int cotulla_dma_set_callback(dmach_t channel, dma_callback_t cb)
{
	cotulla_dma_t* dma;

	if (channel >= COTULLA_DMA_CHANNELS)
		return -EINVAL;

	dma = &dma_chan[channel];

	dma->callback = cb;

	return 0;
}


int cotulla_dma_set_device(dmach_t channel, dma_device_t device,
		dma_addr_t dev_adr, int bsize, int width, int mode, int recv)
{
	cotulla_dma_t* dma;

	if (channel >= COTULLA_DMA_CHANNELS)
		return -EINVAL;

	dma = &dma_chan[channel];
	if (dma->ready)
		return -EINVAL;

	if (cotulla_stop_dma(channel) != 0)
		return -EINVAL;

	cotulla_disable_dma_irq(channel);

	/* DMA Setting */
	set_channel_map(channel, device);
	if (mode) {
		dma->mode = 1; /* no-descriptor fetch mode*/
		DCSR(channel) |= DCSR_NODESC;
	} else
		DCSR(channel) &= ~DCSR_NODESC;

	dma->dev_adr = dev_adr;
	dma->burst_size = bsize;
	dma->width = width;
	if (recv) dma->recv = 1;
	dma->ready = 1;
	return 0;
}


int cotulla_dma_set_spin(dmach_t channel, dma_addr_t addr, int size)
{
	cotulla_dma_t* dma;
	int flags;

	if (channel >= COTULLA_DMA_CHANNELS)
		return -EINVAL;

	dma = &dma_chan[channel];

	local_irq_save(flags);

	dma->spin_addr = addr;
	dma->spin_size = size;
	if (size) {
		cotulla_process_dma(dma);
	}

	local_irq_restore(flags);

	return 0;
}


int cotulla_dma_queue_buffer(dmach_t channel,
			   void* buf_id, dma_addr_t data, int size)
{
	cotulla_dma_t* dma;
	dma_buf_t* buf;
	int flags;

	if (channel >= COTULLA_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];
	if (!dma->ready) {
		return -EINVAL;
	}

	buf = kmalloc(sizeof(*buf), GFP_ATOMIC);
	if (!buf) {
		return -ENOMEM;
	}

	buf->next      = NULL;
	buf->ref       = 0;
	buf->dma_ptr   = data;
	buf->dma_start = data;
	buf->size      = size;
	buf->id        = buf_id;

	local_irq_save(flags);

	if (dma->head) {
		dma->head->next = buf;
	}
	dma->head = buf;
	if (!dma->tail) {
		dma->tail = buf;
	}
	cotulla_process_dma(dma);
	
	local_irq_restore(flags);
	
	return 0;
}

int cotulla_dma_get_current(dmach_t channel, void** buf_id, dma_addr_t* addr)
{
	cotulla_dma_t* dma;
	int flags, ret;

	if (channel >= COTULLA_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];

	local_irq_save(flags);

	if (dma->curr && dma->spin_ref <= 0) {
		dma_buf_t* buf = dma->curr; 

		if (buf_id) {
			*buf_id = buf->id;
		}
		if (dma->mode) {
			if (!dma->recv)
				*addr = DSADR(channel);
			else
				*addr = DTADR(channel);
		} else {
			*addr = 0; /* re-vist for descriptor fetch mode */
		}
		if (*addr < buf->dma_start || *addr > buf->dma_ptr) {
			*addr = buf->dma_ptr;
		}
		ret = 0;
	}
	else {
		if (buf_id) {
			*buf_id = NULL;
		}
		*addr = 0;
		ret = -ENXIO;
	}

	local_irq_restore(flags);

	return ret;
}

int cotulla_dma_stop(dmach_t channel)
{
	cotulla_dma_t* dma;

	if (channel >= COTULLA_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];

	while (cotulla_stop_dma(channel) != 0);
	cotulla_disable_dma_irq(channel);

	return 0;
}

int cotulla_dma_resume(dmach_t channel)
{
	cotulla_dma_t* dma;

	if (channel >= COTULLA_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];

	cotulla_enable_dma(channel);
	cotulla_enable_dma_irq(channel);

	return 0;
}

int cotulla_dma_flush_all(dmach_t channel)
{
	cotulla_dma_t* dma;
	dma_buf_t* buf;
	dma_buf_t* next_buf;
	int flags;

	if (channel >= COTULLA_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];

	local_irq_save(flags);

	cotulla_disable_dma_irq(channel);
	while (cotulla_stop_dma(channel) != 0);

	buf = dma->curr;
	if (!buf) {
		buf = dma->tail;
	}

	dma->head	= NULL;
	dma->tail	= NULL;
	dma->curr	= NULL;
	dma->active	= 0;
	dma->spin_ref	= 0;

	if (dma->spin_size) {
		cotulla_process_dma(dma);
	}

	local_irq_restore(flags);

	while (buf) {
		next_buf = buf->next;
		kfree(buf);
		buf = next_buf;
	}

	return 0;
}

void cotulla_free_dma(dmach_t channel)
{
	cotulla_dma_t* dma;

	if (channel >= COTULLA_DMA_CHANNELS) {
		return;
	}

	dma = &dma_chan[channel];
	if (!dma->lock) {
		printk(KERN_ERR "Trying to free free DMA%d\n", channel);
		return;
	}

	cotulla_dma_set_spin(channel, 0, 0);
	cotulla_dma_flush_all(channel);

	dma->dev_adr = 0;
	dma->burst_size = 0;
	dma->mode = 0;
	dma->width = 0;
	dma->recv = 0;
	dma->ready = 0;
	dma->lock = 0;
}

EXPORT_SYMBOL(cotulla_stop_dma);
EXPORT_SYMBOL(cotulla_enable_dma);
EXPORT_SYMBOL(cotulla_disable_dma_irq);
EXPORT_SYMBOL(cotulla_enable_dma_irq);
EXPORT_SYMBOL(cotulla_dma_is_busy);
EXPORT_SYMBOL(cotulla_dma_is_interrupt);

EXPORT_SYMBOL(cotulla_request_dma);
EXPORT_SYMBOL(cotulla_dma_set_callback);
EXPORT_SYMBOL(cotulla_dma_set_device);
EXPORT_SYMBOL(cotulla_dma_set_spin);
EXPORT_SYMBOL(cotulla_dma_queue_buffer);
EXPORT_SYMBOL(cotulla_dma_get_current);
EXPORT_SYMBOL(cotulla_dma_stop);
EXPORT_SYMBOL(cotulla_dma_resume);
EXPORT_SYMBOL(cotulla_dma_flush_all);
EXPORT_SYMBOL(cotulla_free_dma);

void cotulla_dma_channel_init(void)
{
	int i;
	for (i = 0; i < COTULLA_DMA_CHANNELS; i++) {
		dma_chan[i].channel	= i;
		dma_chan[i].lock	= 0;
		dma_chan[i].device_id	= NULL;
		dma_chan[i].head	= NULL;
		dma_chan[i].tail	= NULL;
		dma_chan[i].curr	= NULL;
		dma_chan[i].dev_adr	= 0;
		dma_chan[i].burst_size	= 0;
		dma_chan[i].mode	= 0;
		dma_chan[i].width	= 0;
		dma_chan[i].recv	= 0;
		dma_chan[i].ready	= 0;
		dma_chan[i].active	= 0;
		dma_chan[i].callback	= NULL;
		dma_chan[i].spin_size	= 0;
		dma_chan[i].spin_addr	= 0;
		dma_chan[i].spin_ref	= 0;
		phys_disc[i]		= NULL;
		virt_disc[i]		= NULL;
	}
}

void cotulla_dma_channel_deinit(void)
{
	int i;
	
	for (i = 0; i < COTULLA_DMA_CHANNELS; i++) {
		if (virt_disc[i] != NULL) {
		consistent_free(virt_disc[i],
				sizeof(DMADesctriptorT) * DMA_DISCRIPTORS,
				(dma_addr_t)phys_disc[i]);
		}
	}
	

}

EXPORT_SYMBOL(cotulla_dma_channel_deinit);
EXPORT_SYMBOL(cotulla_dma_channel_init);

static int __init cotulla_init_dma(void)
//int __init cotulla_init_dma(void)
{
	int  err;

	cotulla_dma_channel_init();
	
	err = request_irq(IRQ_DMA, dma_irq_handler,
			  SA_INTERRUPT, "cotulla-dma", NULL);
	if (err) {
		printk(KERN_ERR "dma: unable to request IRQ_DMA\n");
		return err;
	}
	printk("XScale DMA initilized\n");

	return 0;
}

__initcall(cotulla_init_dma);

