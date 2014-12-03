/*
 * linux/drivers/usbd/sa1100_bi/send.h
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

#include <linux/config.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/pci.h>

#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/dma.h>

#include "sa1100-dma-inline.h"

#include "../usbd.h"
#include "../usbd-debug.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "sa1100.h"

/* ep2 dma engine functions ******************************************************************** */


#define MIN(a,b) ((a)<(b))?(a):(b)
#define MAX(a,b) ((a)>(b))?(a):(b)

static int dmachn_tx;                           // tx dma channel
dma_addr_t dma_curpos;

static struct usb_endpoint_instance *ep2_endpoint;

/* *
 * ep2_reset_dma - 
 * @restart:
 *
 * Reset the DMA engine parameters. 
 */
static void ep2_reset_dma(void)
{
    unsigned long flags;

    local_irq_save(flags);

    // stop current dma
    _sa1100_bi_dma_flush_all_irq(dmachn_tx);
    udelay(10);     // XXX

    ep2_endpoint->tx_urb = NULL;
    if (dma_curpos) {
        pci_unmap_single(NULL, dma_curpos, ep2_endpoint->last, PCI_DMA_TODEVICE);
    }
    dma_curpos = (dma_addr_t)NULL;
    ep2_endpoint->tx_urb = NULL;
    ep2_endpoint->sent = 0;

    usbd_flush_tx(ep2_endpoint);

    local_irq_restore(flags);
}


/* ep2 public functions ************************************************************************ */

/**
 * ep2_enable -
 *
 * Initialize the dma variables. Called once.
 *
 */
int ep2_enable(struct usb_device_instance *device, struct usb_endpoint_instance *endpoint, int chn)
{
    dbgENTER(dbgflg_usbdbi_tx,1);
    dmachn_tx = chn;
    ep2_disable();
    //ep2_device = device;
    {
        unsigned long flags;
        local_irq_save(flags);
        ep2_endpoint = endpoint;
        dma_curpos = 0;
        ep2_endpoint->sent = 0;
        ep2_endpoint->tx_urb = NULL;
        local_irq_restore(flags);
    }
    return 0;
}


/*
 * Initialise the DMA system to use the given chn for receiving
 * packets over endpoint 2.
 *
 * chn     : the dma channel to use.
 * returns : 0 if ok, -1 if channel couldn't be used.
 */
void ep2_disable(void)
{
    unsigned long flags;
    dbgENTER(dbgflg_usbdbi_tx,1);
    local_irq_save(flags);
    if (ep2_endpoint) {
        ep2_reset_dma();
        ep2_endpoint = NULL;
    }
    local_irq_restore(flags);
}


/*
 * reset any txpkts, it will be up to the client driver to free up the send
 * queue.
 */
void ep2_reset(void)
{
    dbgENTER(dbgflg_usbdbi_tx,1);
    ep2_reset_dma();
    udc_reset_ep(2);
}


/* Interrupt service routines ****************************************************************** */


/*
 *
 * Interrupt handler - called with interrupts disabled.
 */
void ep2_int_hndlr(unsigned int status, int in_interrupt)
{
    unsigned int udccs2;
    int ok;
    int restart = 0;

    udccs2 = *(UDCCS2);

    // do not do anything if FST is set
    if (udccs2 & UDCCS2_FST) {
        dbg_tx(0,"FST set! (%sint)", in_interrupt ? "" : "not-");
        udc_ep2_fst++;
        return;
    }
    else if (udccs2 & UDCCS2_SST) {
        dbg_tx(0,"SST set, stalling! (%sint)", in_interrupt ? "": "not-");
        udc_stall_ep(2);
        return;
    }

    // if in interrupt we have to stop any current activity
    if (in_interrupt) {

        tx_interrupts++;

        if (!((udccs2 = *(UDCCS2)) & UDCCS2_TPC)) {
            return;
        }

        _sa1100_bi_dma_flush_all_irq(dmachn_tx);

        // if requred reset stall and restart 
        if (udccs2 & (UDCCS2_TPE | UDCCS2_TUR | UDCCS2_SST)) {

            if (udccs2&UDCCS2_SST) {
                SET_AND_TEST(*(UDCCS2) = UDCCS2_SST, _udc(UDCCS2) & UDCCS2_SST, ok);
                if (!ok) {
                    dbg_tx(0,"Waiting too long to set SST %02x", *(UDCCS2));
                }
            }
            udc_ep2_errors++;
            restart = 1;
            if (udccs2 & UDCCS2_TPE) {
                udc_ep2_tpe++;
            }
            //QQQ was: if (udccs2 & UDCCS2_TUR | UDCCS2_SST) 
            if (udccs2 & UDCCS2_TUR) {
                udc_ep2_tur++;
            }
            if (udccs2 & UDCCS2_SST) {
                udc_ep2_sst++;
            }
        
        }

        // if we have an active buffer, umap the buffer and update position if previous send was successful
        if (dma_curpos) {
            pci_unmap_single(NULL, dma_curpos, ep2_endpoint->last, PCI_DMA_TODEVICE);
            dma_curpos = (dma_addr_t)NULL;
        }
    }

    usbd_tx_complete_irq(ep2_endpoint, restart);

    // start dma if we have something to send
    if (ep2_endpoint->tx_urb && ((ep2_endpoint->tx_urb->actual_length - ep2_endpoint->sent) > 0)) {
        int ok1, ok2;
        //int count;
        ep2_endpoint->last = MIN(ep2_endpoint->tx_urb->actual_length - ep2_endpoint->sent, 
                ep2_endpoint->tx_urb->endpoint->tx_packetSize);

        if (!(dma_curpos = pci_map_single(NULL, ep2_endpoint->tx_urb->buffer + ep2_endpoint->sent, 
                        ep2_endpoint->last, PCI_DMA_TODEVICE))) 
        {
            dbg_tx(0,"pci_map_single failed");
        }

        /* The order of the following is critical....
         * Reseting the TPC clears the FIFO, so the DMA engine cannot be started until after
         * TPC is set. But this opens a window where the USB UCD can fail because it is
         * waiting for the DMA engine to fill the FIFO and it receives an IN packet from
         * the host. The best we can do is to minimize the window. [Note that this effect
         * is most noticeable on slow, 133Mhz, processors.]
         *
         * 1. Set IMP and all DMA registers, but do not start DMA yet
         * 2. reset TPC
         * 3. start DMA
         */
        SET_AND_TEST(*(UDCIMP) = ep2_endpoint->last-1, _udc(UDCIMP) != (ep2_endpoint->last - 1), ok1);
        if (ep2_endpoint->last > 16) {
            _sa1100_bi_dma_queue_buffer_irq(dmachn_tx, dma_curpos, ep2_endpoint->last);

            //for (count = 8; count--; *(UDCDR) = '\0');

            SET_AND_TEST(*(UDCCS2) = UDCCS2_TPC, _udc(UDCCS2) & UDCCS2_TPC, ok2);
            _sa1100_bi_dma_run(dmachn_tx);

            if (!ok1 || !ok2) {
                dbg_tx(0,"Waiting too long to set UDCIMP %02x or TPC: %02x", *(UDCIMP),*(UDCCS2));
            }
            dbg_tx(4,"queued %d bytes using DMA", ep2_endpoint->last);
        }
        else {

            // Entire packet is small enough to fit in FIFO, stuff it
            // in directly and forget DMA.
            
            unsigned char *dmabuf = dma_curpos;
            int count = ep2_endpoint->last;

            SET_AND_TEST(*(UDCCS2) = UDCCS2_TPC, _udc(UDCCS2) & UDCCS2_TPC, ok2);
            for (; count--; *(UDCDR) = *dmabuf++);

            dbg_tx(4,"queued %d bytes directly to FIFO", ep2_endpoint->last);
        }
    }
}

