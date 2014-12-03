/*
 * linux/arch/sh/kernel/sh7727_dma.c
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * Based on arch/arm/kernel/dma-sa1100.c
 * Copyright (C) 2000 Nicolas Pitre
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Hitachi SH7727 dma support.
 *
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/pci.h>

#include <asm/sh7727.h>
#include <asm/io.h>
#include <asm/irq.h>

//#define DEBUG
#undef DEBUG

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
	int            ready;     /* 1 if DMA can occur */
	int            active;    /* 1 if DMA is actually processing data */
	dma_callback_t callback;  /* ... to call when buffers are done */
	int            spin_size; /* >0 when DMA should spin and no more buf */
	dma_addr_t     spin_addr; /* DMA address to spin onto */
	int            spin_ref;  /* number of spinning references */
    unsigned long  iobase;
    unsigned long  way;              /*  */
} sh7727_dma_t;

#define MAX_SH7727_DMA_CHANNELS 4
static sh7727_dma_t dma_chan[MAX_SH7727_DMA_CHANNELS];

#define SAR(x)    ((unsigned long)(x) + 0x00)
#define DAR(x)    ((unsigned long)(x) + 0x04)
#define DMATCR(x) ((unsigned long)(x) + 0x08)
#define CHCR(x)   ((unsigned long)(x) + 0x0c)

void sh7727_disable_dma(dmach_t channel)
{
	dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) & ~CHCR_DE), CHCR(dev));
}

void sh7727_enable_dma(dmach_t channel)
{
	dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) | CHCR_DE), CHCR(dev));
}

void sh7727_clear_dma_irq(dmach_t channel)
{
	dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) & ~(CHCR_TE|CHCR_DE)), CHCR(dev));
}

void sh7727_disable_dma_irq(dmach_t channel)
{
	dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) & ~CHCR_IE), CHCR(dev));
}

void sh7727_enable_dma_irq(dmach_t channel)
{
	dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) | CHCR_IE), CHCR(dev));
}

int sh7727_dma_is_busy(dmach_t channel)
{
	dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;
	unsigned long value = ctrl_inl(CHCR(dev));
	if (value & CHCR_DE) {
		if ((value & CHCR_TE) == 0)
			return 1;
	}
	return 0;
}

static int sh7727_start_dma(sh7727_dma_t* dma, dma_addr_t dma_ptr, int size)
{
	dmach_t channel = dma->channel;
	dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;
	if (sh7727_dma_is_busy(channel))
		return -EBUSY;

	if (dma_chan[channel].way)
	    ctrl_outl(dma_ptr,    DAR(dev));
	else
	    ctrl_outl(dma_ptr,    SAR(dev));
	ctrl_outl(size,       DMATCR(dev));
	ctrl_outl((ctrl_inl(CHCR(dev)) & 0xFFFFFFF8) | CHCR_IE | CHCR_DE,
		  CHCR(dev));

#ifdef DEBUG
	printk("START DMA channel= %d\n",channel);
	printk("  SAR    = %lx\n",ctrl_inl(SAR(dev)));
	printk("  DAR    = %lx\n",ctrl_inl(DAR(dev)));
	printk("  DMATCR = %lx\n",ctrl_inl(DMATCR(dev)));
	printk("  CHCR   = %lx\n",ctrl_inl(CHCR(dev)));
#endif
	return 0;
}

static void sh7727_process_dma(sh7727_dma_t* dma)
{
	dma_buf_t* buf;
	int chunksize;

	for (;;) {
		buf = dma->tail;

		if (!buf) {
			if (dma->spin_size && dma->spin_ref >= 0) {
				chunksize = dma->spin_size;
				while (sh7727_start_dma(dma, dma->spin_addr,
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
		if (sh7727_start_dma(dma, buf->dma_ptr, chunksize) != 0)
			break;
		dma->active = 1;
		if (!dma->curr) {
			dma->curr = buf;
		}
		buf->ref++;
		buf->dma_ptr += chunksize;
		buf->size    -= chunksize;
		if (buf->size == 0) {
			dma->tail = buf->next;
		}
	}
}

void sh7727_dma_done(sh7727_dma_t* dma)
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

	sh7727_process_dma(dma);
}

static void sh7727_dma_interrupt(int irq, void* dev_id, struct pt_regs* regs)
{
	int channel = -1;

	switch (irq) {
	case DMAC_DEI0_IRQ: channel = 0; break;
	case DMAC_DEI1_IRQ: channel = 1; break;
	case DMAC_DEI2_IRQ: channel = 2; break;
	case DMAC_DEI3_IRQ: channel = 3; break;
	}

	if (channel >= 0) {
		dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;

		if (sh7727_dma_is_busy(channel) == 0) {
			sh7727_dma_t* dma = &dma_chan[channel];
			ctrl_outl((ctrl_inl(CHCR(dev)) & 0xFFFFFFF8),
				  CHCR(dev));
			sh7727_dma_done(dma);
		}
		else {
		  printk("dma_interrupt: ch=%d\n", channel);
		}
	}
	else {
	  printk("dma_interrupt: irq=%d\n", irq);
	}
}

int sh7727_get_dma_list(char* buf)
{
	int i, len = 0;

	for (i = 0; i < MAX_SH7727_DMA_CHANNELS; i++) {
		if (dma_chan[i].lock)
			len += sprintf(buf + len, "%2d: %s\n",
				       i, dma_chan[i].device_id);
	}
	return len;
}

int sh7727_request_dma(dmach_t* channel, const char* device_id)
{
	sh7727_dma_t* dma = NULL;
	dma_device_t* dev = NULL;
	int ch, err;
	static int initialized = 0;

	if (!initialized) {
		err = request_irq(DMAC_DEI0_IRQ, sh7727_dma_interrupt,
				  SA_INTERRUPT, "sh7727-dma", NULL);
		if (err) {
#ifdef DEBUG
			printk("dma: unable to request DMAC_DEI0_IRQ\n");
#endif
			return err;
		}
		err = request_irq(DMAC_DEI1_IRQ, sh7727_dma_interrupt,
				  SA_INTERRUPT, "sh7727-dma", NULL);
		if (err) {
#ifdef DEBUG
			printk("dma: unable to request DMAC_DEI1_IRQ\n");
#endif
			return err;
		}
		err = request_irq(DMAC_DEI2_IRQ, sh7727_dma_interrupt,
				  SA_INTERRUPT, "sh7727-dma", NULL);
		if (err) {
#ifdef DEBUG
			printk("dma: unable to request DMAC_DEI2_IRQ\n");
#endif
			return err;
		}
		err = request_irq(DMAC_DEI3_IRQ, sh7727_dma_interrupt,
				  SA_INTERRUPT, "sh7727-dma", NULL);
		if (err) {
#ifdef DEBUG
			printk("dma: unable to request DMAC_DEI3_IRQ\n");
#endif
			return err;
		}
		ctrl_outw(0x0001, DMAOR);
		initialized = 1;
	}

	*channel = -1;
	for (ch = 0; ch < MAX_SH7727_DMA_CHANNELS; ch++) {
		dma = &dma_chan[ch];
		if (xchg(&dma->lock, 1) == 0)
			break;
	}

	if (ch >= MAX_SH7727_DMA_CHANNELS) {
#ifdef DEBUG
		printk("%s: no free DMA channel available\n",
		       device_id);
#endif
		return -EBUSY;
	}

	dma = &dma_chan[ch];
	xchg(&dma->lock, 1);
	
	*channel = ch;
	dma->device_id = device_id;
	dma->callback  = NULL;
	dma->spin_size = 0;

	dev = (dma_device_t*)dma_chan[ch].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) & 0xFFFFFFF8), CHCR(dev));

	return 0;
}


int sh7727_dma_set_callback(dmach_t channel, dma_callback_t cb)
{
	sh7727_dma_t* dma;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return -EINVAL;
	if (channel < 0)
		return -EINVAL;

	dma = &dma_chan[channel];

	dma->callback = cb;

	return 0;
}

int sh7727_dma_set_device(dmach_t channel, dma_device_t device)
{
	sh7727_dma_t* dma;
	dma_device_t* dev;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return -EINVAL;
	if (channel < 0)
		return -EINVAL;

	dma = &dma_chan[channel];
	if (dma->ready)
		return -EINVAL;

	dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl(device.sar,    SAR(dev));
	ctrl_outl(device.dar,    DAR(dev));
	ctrl_outl(device.dmatcr, DMATCR(dev));
	ctrl_outl(device.chcr,   CHCR(dev));
	dma->way = device.way;

	if (device.dar == SITDR) {
		unsigned short val = 0;
		switch (channel) {
		case 0: val = ((ctrl_inw(CHRAR)&0xFFF0) | 0x000a); break;
		case 1: val = ((ctrl_inw(CHRAR)&0xFF0F) | 0x00a0); break;
		case 2: val = ((ctrl_inw(CHRAR)&0xF0FF) | 0x0a00); break;
		case 3: val = ((ctrl_inw(CHRAR)&0x0FFF) | 0xa000); break;
		}
		ctrl_outw(val, CHRAR);
	}
	if (device.sar == SIRDR) {
	    unsigned short val = 0;
	    switch(channel) {
	    case 0: val = ((ctrl_inw(CHRAR)&0xFFF0) | 0x0009);break;
	    case 1: val = ((ctrl_inw(CHRAR)&0xFF0F) | 0x0090);break;
	    case 2: val = ((ctrl_inw(CHRAR)&0xF0FF) | 0x0900);break;
	    case 3: val = ((ctrl_inw(CHRAR)&0x0FFF) | 0x9000);break;
	    }
	    ctrl_outw(val, CHRAR);
	}

	dma->ready = 1;

	return 0;
}

int sh7727_dma_set_spin(dmach_t channel, dma_addr_t addr, int size)
{
	sh7727_dma_t* dma;
	int flags;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return -EINVAL;

	dma = &dma_chan[channel];

	local_irq_save(flags);

	dma->spin_addr = addr;
	dma->spin_size = size;
	if (size) {
		sh7727_process_dma(dma);
	}

	local_irq_restore(flags);

	return 0;
}

int sh7727_dma_queue_buffer(dmach_t channel,
			   void* buf_id, dma_addr_t data, int size)
{
	sh7727_dma_t* dma;
	dma_buf_t* buf;
	int flags;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return -EINVAL;
	if (channel < 0)
		return -EINVAL;

	dma = &dma_chan[channel];

	if (!dma->ready)
		return -EINVAL;

	buf = kmalloc(sizeof(*buf), GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

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

	sh7727_process_dma(dma);

	local_irq_restore(flags);

	return 0;
}

int sh7727_dma_get_current(dmach_t channel, void** buf_id, dma_addr_t* addr)
{
	sh7727_dma_t* dma;	
	int flags, ret;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return -EINVAL;
	if (channel < 0)
		return -EINVAL;

	dma = &dma_chan[channel];

	local_irq_save(flags);

	if (dma->curr && dma->spin_ref <= 0) {
		dma_buf_t* buf = dma->curr; 
		dma_device_t* dev = (dma_device_t*)dma_chan[channel].iobase;

		if (buf_id) {
			*buf_id = buf->id;
		}
		*addr = ctrl_inl(SAR(dev));
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

int sh7727_dma_stop(dmach_t channel)
{
	dma_device_t* dev;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return -EINVAL;
	if (channel < 0)
		return -EINVAL;

	dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) & 0xFFFFFFF8), CHCR(dev));

	return 0;
}

int sh7727_dma_resume(dmach_t channel)
{
	dma_device_t* dev;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return -EINVAL;
	if (channel < 0)
		return -EINVAL;

	dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) & 0xFFFFFFF8) | CHCR_IE | CHCR_DE,
		  CHCR(dev));

	return 0;
}

int sh7727_dma_flush_all(dmach_t channel)
{
	sh7727_dma_t* dma;
	dma_device_t* dev;
	dma_buf_t* buf;
	dma_buf_t* next_buf;
	int flags;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return -EINVAL;
	if (channel < 0)
		return -EINVAL;

	dma = &dma_chan[channel];

	local_irq_save(flags);

	dev = (dma_device_t*)dma_chan[channel].iobase;
	ctrl_outl((ctrl_inl(CHCR(dev)) & 0xFFFFFFF8), CHCR(dev));

	buf = dma->curr;
	if (!buf) {
		buf = dma->tail;
	}

	dma->head     = NULL;
	dma->tail     = NULL;
	dma->curr     = NULL;
	dma->active   = 0;
	dma->spin_ref = 0;

	if (dma->spin_size) {
		sh7727_process_dma(dma);
	}

	local_irq_restore(flags);

	while (buf) {
		next_buf = buf->next;
		kfree(buf);
		buf = next_buf;
	}

	return 0;
}

void sh7727_free_dma(dmach_t channel)
{
	sh7727_dma_t* dma;

	if (channel >= MAX_SH7727_DMA_CHANNELS)
		return;
	if (channel < 0)
		return;
	dma = &dma_chan[channel];
	if (!dma->lock) {
#ifdef DEBUG
		printk(KERN_ERR "Trying to free free DMA%d\n", channel);
#endif
		return;
	}

	sh7727_dma_set_spin(channel, 0, 0);
	sh7727_dma_flush_all(channel);

	dma->ready = 0;
	dma->lock = 0;
}

int __init sh7727_dma_init(void)
{
	dma_device_t* dev;
	int i;

	for (i = 0; i < MAX_SH7727_DMA_CHANNELS; i++) {
		dma_chan[i].channel   = i;
		dma_chan[i].lock      = 0;
		dma_chan[i].device_id = NULL;
		dma_chan[i].head      = NULL;
		dma_chan[i].tail      = NULL;
		dma_chan[i].curr      = NULL;
		dma_chan[i].ready     = 0;
		dma_chan[i].active    = 0;
		dma_chan[i].callback  = NULL;
		dma_chan[i].spin_size = 0;
		dma_chan[i].spin_addr = 0;
		dma_chan[i].spin_ref  = 0;
		dma_chan[i].way       = 0;   /* K.Kino */
		switch (i) {
		case 0: dma_chan[i].iobase = 0xa4000020; break;
		case 1: dma_chan[i].iobase = 0xa4000030; break;
		case 2: dma_chan[i].iobase = 0xa4000040; break;
		case 3: dma_chan[i].iobase = 0xa4000050; break;
		}
		dev = (dma_device_t*)dma_chan[i].iobase;
		ctrl_outl(0, CHCR(dev));
	}

	printk(KERN_INFO "SH7727 DMAC initialized.\n");

	return 0;
}
