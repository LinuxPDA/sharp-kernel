/*
 * linux/drivers/usbd/sa1100_bi/dma-sa1100.h 
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
 * A minized replacement of arch/arm/kernel/dma-sa1100.c
 *
 * Support functions for the SA11x0 internal DMA channels.
 * (see also Documentation/arm/SA1100/DMA)
 *
 */

#include <linux/config.h>
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
#include <asm/mach/dma.h>

#include "../usbd-debug.h"

extern int dbgflg_usbdbi_dma_flg;
#define dbg_dma(lvl,fmt,args...) dbgPRINT(dbgflg_usbdbi_dma_flg,lvl,fmt,##args)

// Assabet specific - wrap with CONFIG_...
#if !defined(MAX_DMA_CHANNELS)
    #define MAX_DMA_CHANNELS 6
#else
    #if MAX_DMA_CHANNELS <= 0
        #undef MAX_DMA_CHANNELS
        #define MAX_DMA_CHANNELS 6
    #endif
#endif

#include "sa1100-dma-inline.h"

// unfortunately dma-sa1100.c does not export dma_chan so we have to
// provide a minimal replacement for our own use
sa1100_bi_dma_t sa1100_bi_dma_chan[MAX_DMA_CHANNELS];


#if defined(CONFIG_SA1100_NEW_DMA_COOPERATION)
static int noop_dma_request(dmach_t ch, dma_t *dma)
{
    /* Use this for things like allocating an IRQ,
       return non-zero on failure. */
    dbg_dma(5,"called");
    return(0);
}

static void noop_dma_free(dmach_t ch, dma_t *dma)
{
    dbg_dma(5,"called");
}

static void noop_dma_enable(dmach_t ch, dma_t *dma)
{
    dbg_dma(5,"called");
}

static void noop_dma_disable(dmach_t ch, dma_t *dma)
{
    dbg_dma(5,"called");
}

static int noop_dma_residue(dmach_t ch, dma_t *dma)
{
    dbg_dma(5,"called");
    return(0);
}

static int noop_dma_setspeed(dmach_t ch, dma_t *dma, int cycle_ns)
{
    dbg_dma(5,"called");
    return(0);
}

/* no-op ops table for channels we grab from generic dma code */
static struct dma_ops sa1100_bi_dma_ops = {
        request:	noop_dma_request,
        free:		noop_dma_free,
        enable:		noop_dma_enable,   /* Mandatory */
        disable:	noop_dma_disable,  /* Mandatory */
        residue:	noop_dma_residue,
        setspeed:	noop_dma_setspeed,
	type:		"Bus Interface",
};


/*
 * DMA interface functions
 */

int sa1100_bi_request_dma(dmach_t * channel, const char *device_id, dma_device_t device)
{
    sa1100_bi_dma_t *dma = NULL;
    dma_regs_t *regs;
    struct dma_ops *old_ops;
    int ch;
    int flags;

    *channel = -1;		/* to be sure we catch the freeing of a misregistered channel */

    flags = claim_dma_lock();

    // XXX why SA1100_DMA_CHANNELS, and not MAX_DMA_CHANNELS????
   
    for (ch = 0; ch < MAX_DMA_CHANNELS; ch++) {
        dma = &sa1100_bi_dma_chan[ch];
        /* This API is a little awkward, you can't
           request a channel unless you have set the
           ops, and until I changed things, you couldn't
           reset the ops later.  The whole process
           must be done with interrupts off, or we
           might end up setting someone else's ops
           to NULL. */
        if (NULL == dma->device_id && available_dma(ch)) {
            // This _may_ be an available channel
            dma->device_id = device_id;
            old_ops = set_dma_ops(ch,NULL);
            (void) set_dma_ops(ch,&sa1100_bi_dma_ops);
            if (0 != request_dma(ch, device_id)) {
                // Oops, not available.
                (void) set_dma_ops(ch,old_ops);
                dma->device_id = NULL;
            } else {
                // Got one.
                *channel = ch;
		dma->old_ops = old_ops;
                break;
            }
        }
    }
    release_dma_lock(flags);
    if (-1 == *channel) {
        printk(KERN_ERR "%s: no free DMA channel available\n", device_id);
        return -EBUSY;
    }
    regs = dma->regs;
    regs->ClrDCSR = (DCSR_DONEA | DCSR_DONEB | DCSR_STRTA | DCSR_STRTB | DCSR_IE | DCSR_ERROR | DCSR_RUN);
    //regs->DDAR = 0;
    regs->DDAR = device;

    return(0);
}

void sa1100_bi_free_dma(dmach_t channel)
{
    sa1100_bi_dma_t *dma;
    unsigned long flags;

    if ((unsigned) channel >= MAX_DMA_CHANNELS)
        return;

    dbg_dma(5,"-");

    dma = &sa1100_bi_dma_chan[channel];
    sa1100_bi_dma_flush_all(channel);

    /* Tell the other DMA APIs we're done with channel */

    flags = claim_dma_lock();
    free_dma(channel);
    dma->device_id = NULL;
    /* Last but not least, restore the other API ops. */
    (void) set_dma_ops(channel,NULL);
    (void) set_dma_ops(channel,dma->old_ops);
    release_dma_lock(flags);

}

#else

int sa1100_bi_request_dma(dmach_t * channel, const char *device_id, dma_device_t device)
{
    sa1100_bi_dma_t *dma = NULL;
    dma_regs_t *regs;
    int ch, err;

    *channel = -1;		/* to be sure we catch the freeing of a misregistered channel */

    if ((err = sa1100_request_dma(channel,device_id,device))) {
        return -EBUSY;
    }

    printk(KERN_DEBUG"sa1100_bi_request: %s %d\n", device_id, *channel);

    // unfortunately dma-sa1100.c does not export dma_chan so we have to
    // build a minimal replacement for our own use
    dma = &sa1100_bi_dma_chan[(int)*channel];
    dma->device_id = device_id;

    return 0;
}

void sa1100_bi_free_dma(dmach_t channel)
{
    sa1100_bi_dma_t *dma;

    if ((unsigned) channel >= MAX_DMA_CHANNELS)
        return;

    dbg_dma(5,"-");

    dma = &sa1100_bi_dma_chan[channel];
    sa1100_bi_dma_flush_all(channel);

    /* Tell the other DMA APIs we're done with channel */
    sa1100_free_dma(channel);
}

#endif


int sa1100_bi_dma_flush_all(dmach_t channel)
{
    sa1100_bi_dma_t *dma;
    int flags;

    dma = &sa1100_bi_dma_chan[channel];
    local_irq_save(flags);
    dma->regs->ClrDCSR = DCSR_STRTA | DCSR_STRTB | DCSR_RUN | DCSR_IE;
    dma->active = 0;
    local_irq_restore(flags);
    return 0;
}

int __init sa1100_bi_init_dma(void)
{
    int channel;
    for (channel = 0; channel < MAX_DMA_CHANNELS; channel++) {
        sa1100_bi_dma_chan[channel].regs = (dma_regs_t *) io_p2v(_DDAR(channel));
    }
    return 0;
}


