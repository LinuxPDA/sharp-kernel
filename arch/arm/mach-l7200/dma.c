/*
 * arch/arm/mach-l7200/dma.c
 *
 * Support functions for tthe LinkUp 7200 internal DMA channels.
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * ChangeLog:
 *   03-19-2001 Lineo Japan, Inc.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/malloc.h>
#include <linux/errno.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>

#define MAX_DMA_SIZE	0x1fff
#define MAX_DMA_ORDER	12

#include "dma.h"

static l7200_dma_t dma_chan[MAX_L7200_DMA_CHANNELS];

void l7200_disable_dma(dmach_t channel)
{
  //IO_DMA_TERM |= ( 1 << channel );
  	IO_DMA_TERM = ( 1 << channel );
}

void l7200_enable_dma(dmach_t channel)
{
  //IO_DMA_EN |= ( 1 << channel );
	IO_DMA_EN = ( 1 << channel );
}

void l7200_clear_dma_irq(dmach_t channel)
{
  //IO_DMA_ICR |= ( 1 << channel );
	IO_DMA_ICR = ( 1 << channel );
}

void l7200_disable_dma_irq(dmach_t channel)
{
	IO_DMA_IER &= ~( 1 << channel );
}

void l7200_enable_dma_irq(dmach_t channel)
{
	IO_DMA_IER |= ( 1 << channel );
}

unsigned long l7200_dma_is_busy(dmach_t channel)
{
	return IO_DMA_BZ & ( 1 << channel );
}

unsigned long l7200_dma_is_interrupt(dmach_t channel)
{
	return IO_DMA_ISR & ( 1 << channel );
}

/*
 * DMA processing...
 */
static int l7200_start_dma(l7200_dma_t* dma, dma_addr_t dma_ptr, int size)
{
	dmach_t channel = dma->channel;

	if (l7200_dma_is_busy( channel )) {
		return -EBUSY;
	}

	IO_DMA_CxNBA(channel) = __virt_to_phys(dma_ptr);
	IO_DMA_CxNTC(channel) = size;

	l7200_clear_dma_irq(channel);
	l7200_enable_dma_irq(channel);
	l7200_enable_dma(channel);

	return 0;
}

static void l7200_process_dma(l7200_dma_t* dma)
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
				while (l7200_start_dma(dma, dma->spin_addr,
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
		if (l7200_start_dma(dma, buf->dma_ptr, chunksize) != 0) {
			break;
		}
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

void l7200_dma_done(l7200_dma_t* dma)
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

	l7200_process_dma(dma);
}

static void dma_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int channel;

#if 0
	printk("dma_irq_handler()\n");
#endif
	for (channel = 0; channel < L7200_DMA_CHANNELS; channel++) {
		if (l7200_dma_is_interrupt( channel )) {
			l7200_clear_dma_irq( channel );
			if (!l7200_dma_is_busy( channel )) {
			  	l7200_dma_t* dma = &dma_chan[channel];
				l7200_dma_done(dma);
			}
		}
	}
}

/*
 * DMA interface functions
 */
int l7200_get_dma_list(char* buf)
{
	int i, len = 0;

	for (i = 0; i < MAX_L7200_DMA_CHANNELS; i++) {
		if (dma_chan[i].lock)
			len += sprintf(buf + len, "%2d: %s\n",
				       i, dma_chan[i].device_id);
	}
	return len;
}

int l7200_request_dma(dmach_t* channel, const char* device_id)
{
	l7200_dma_t* dma = NULL;
	int ch;

	*channel = -1;
	for (ch = 0; ch < L7200_DMA_CHANNELS; ch++) {
		dma = &dma_chan[ch];
		if (xchg(&dma->lock, 1) == 0)
			break;
	}

	if (ch >= L7200_DMA_CHANNELS) {
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

	l7200_disable_dma(ch);
	l7200_clear_dma_irq(ch);
	l7200_disable_dma_irq(ch);

	IO_DMA_CxPA(ch) = 0;

	return 0;
}


int l7200_dma_set_callback(dmach_t channel, dma_callback_t cb)
{
	l7200_dma_t* dma;

	if (channel >= L7200_DMA_CHANNELS)
		return -EINVAL;

	dma = &dma_chan[channel];

	dma->callback = cb;

	return 0;
}


int l7200_dma_set_device(dmach_t channel,
			 dma_device_t device, int req, int ctrl)
{
	l7200_dma_t* dma;

	if (channel >= L7200_DMA_CHANNELS)
		return -EINVAL;

	dma = &dma_chan[channel];
	if (dma->ready)
		return -EINVAL;

	l7200_disable_dma(channel);
	l7200_clear_dma_irq(channel);
	l7200_disable_dma_irq(channel);

	/* DMA Setting */
	if (channel <= 3) { 
		IO_DMA_RCM1 &= ~(0x000000ff << (channel * 8));
		IO_DMA_RCM1 |= ( req << (channel * 8)) ;
	}
	else {
		IO_DMA_RCM2 &= ~(0x000000ff << ((channel - 4) * 8));
		IO_DMA_RCM2 |= ( req << ((channel - 4) * 8)) ;
	}
		
	IO_DMA_CxPA(channel)  = device;
	IO_DMA_CxCTL(channel) = ctrl;

	dma->ready = 1;

	return 0;
}


int l7200_dma_set_spin(dmach_t channel, dma_addr_t addr, int size)
{
	l7200_dma_t* dma;
	int flags;

	if (channel >= L7200_DMA_CHANNELS)
		return -EINVAL;

	dma = &dma_chan[channel];

	local_irq_save(flags);

	dma->spin_addr = addr;
	dma->spin_size = size;
	if (size) {
		l7200_process_dma(dma);
	}

	local_irq_restore(flags);

	return 0;
}

int l7200_dma_queue_buffer(dmach_t channel,
			   void* buf_id, dma_addr_t data, int size)
{
	l7200_dma_t* dma;
	dma_buf_t* buf;
	int flags;

	if (channel >= L7200_DMA_CHANNELS) {
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

	l7200_process_dma(dma);

	local_irq_restore(flags);

	return 0;
}

int l7200_dma_get_current(dmach_t channel, void** buf_id, dma_addr_t* addr)
{
	l7200_dma_t* dma;
	int flags, ret;

	if (channel >= L7200_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];

	local_irq_save(flags);

	if (dma->curr && dma->spin_ref <= 0) {
		dma_buf_t* buf = dma->curr; 

		if (buf_id) {
			*buf_id = buf->id;
		}
		*addr = IO_DMA_CxCBA(channel);
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

int l7200_dma_stop(dmach_t channel)
{
	l7200_dma_t* dma;

	if (channel >= L7200_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];

	l7200_disable_dma(channel);
	l7200_disable_dma_irq(channel);

	return 0;
}

int l7200_dma_resume(dmach_t channel)
{
	l7200_dma_t* dma;

	if (channel >= L7200_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];

	l7200_enable_dma(channel);
	l7200_enable_dma_irq(channel);

	return 0;
}

int l7200_dma_flush_all(dmach_t channel)
{
	l7200_dma_t* dma;
	dma_buf_t* buf;
	dma_buf_t* next_buf;
	int flags;

	if (channel >= L7200_DMA_CHANNELS) {
		return -EINVAL;
	}

	dma = &dma_chan[channel];

	local_irq_save(flags);

	l7200_disable_dma(channel);
	l7200_clear_dma_irq(channel);
	l7200_disable_dma_irq(channel);

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
		l7200_process_dma(dma);
	}

	local_irq_restore(flags);

	while (buf) {
		next_buf = buf->next;
		kfree(buf);
		buf = next_buf;
	}

	return 0;
}

void l7200_free_dma(dmach_t channel)
{
	l7200_dma_t* dma;

	if (channel >= L7200_DMA_CHANNELS) {
		return;
	}

	dma = &dma_chan[channel];
	if (!dma->lock) {
		printk(KERN_ERR "Trying to free free DMA%d\n", channel);
		return;
	}

	l7200_dma_set_spin(channel, 0, 0);
	l7200_dma_flush_all(channel);

	dma->ready = 0;
	dma->lock = 0;
}

EXPORT_SYMBOL(l7200_disable_dma);
EXPORT_SYMBOL(l7200_enable_dma);
EXPORT_SYMBOL(l7200_clear_dma_irq);
EXPORT_SYMBOL(l7200_disable_dma_irq);
EXPORT_SYMBOL(l7200_enable_dma_irq);
EXPORT_SYMBOL(l7200_dma_is_busy);
EXPORT_SYMBOL(l7200_dma_is_interrupt);

EXPORT_SYMBOL(l7200_request_dma);
EXPORT_SYMBOL(l7200_dma_set_callback);
EXPORT_SYMBOL(l7200_dma_set_device);
EXPORT_SYMBOL(l7200_dma_set_spin);
EXPORT_SYMBOL(l7200_dma_queue_buffer);
EXPORT_SYMBOL(l7200_dma_get_current);
EXPORT_SYMBOL(l7200_dma_stop);
EXPORT_SYMBOL(l7200_dma_resume);
EXPORT_SYMBOL(l7200_dma_flush_all);
EXPORT_SYMBOL(l7200_free_dma);

static int __init l7200_init_dma(void)
{
	int i, err;

	for (i = 0; i < L7200_DMA_CHANNELS; i++) {
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
	}

	err = request_irq(IRQ_DMA, dma_irq_handler,
			  SA_INTERRUPT, "l7200-dma", NULL);
	if (err) {
		printk(KERN_ERR "dma: unable to request IRQ_DMA\n");
		return err;
	}

	return 0;
}

__initcall(l7200_init_dma);
