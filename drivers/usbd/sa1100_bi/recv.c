/*
 * linux/drivers/usbd/sa1100_bi/recv.c
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
/***************************************************************************/

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

/* ep1 dma engine functions ******************************************************************** */

static int dmachn_rx;           // rx dma channel
static int dma_enabled;         // dma channel has been setup
static int dma_active;          // dma engine is running

static dma_addr_t dma_curpos;
struct usb_device_instance *ep1_device; 
struct usb_endpoint_instance *ep1_endpoint;

/* *
 * ep1_stop_dma - stop the DMA engine
 *
 * Stop the DMA engine. 
 */
static void ep1_stop_dma(struct urb *urb)
{
    if (urb && dma_enabled) {
        unsigned long flags;
        save_flags_cli(flags);
        if (!dma_active) {
            restore_flags(flags);
            dbg_rx(1, "dma OFF: %x %x %x", dmachn_rx, dma_enabled, dma_active);
            return;
        }

        dma_active = 0;
        restore_flags(flags);

        if (ep1_endpoint->rcv_packetSize > 20) {
            sa1100_bi_dma_flush_all(dmachn_rx);
        }
    }
}


/* *
 * ep1_start_dma - start the DMA engine running
 *
 * Start the DMA engine. This should allow incoming data to be stored
 * in the DMA receive buffer.
 *
 */
static void ep1_start_dma(struct urb *urb)
{
    if (urb && dma_enabled) {
        unsigned long flags;
        int ok;
        save_flags_cli(flags);
        if (dma_active) {
            restore_flags(flags);
            dbg_rx(1, "dma ON: %x %x %x", dmachn_rx, dma_enabled, dma_active);
            return;
        }

        dma_active = 1;

        dbg_rx(1, "urb: %p urb->buffer: %p actual_length: %d rcv_packetSize: %d",
                urb, urb->buffer, urb->actual_length, urb->endpoint->rcv_packetSize);

        if (!(dma_curpos = pci_map_single(NULL, (void *)urb->buffer + urb->actual_length, 
                        urb->endpoint->rcv_packetSize, PCI_DMA_FROMDEVICE))) 
        {
            restore_flags(flags);
            dbg_rx(0,"pci_map_single failed");
            return;
        }

        SET_AND_TEST( *(UDCCS1) = UDCCS1_RPC, _udc(UDCCS1) & UDCCS1_RPC, ok);


        // enable DMA only if packet to big for FIFO (twenty bytes)
        if (ep1_endpoint->rcv_packetSize > 20) {
            _sa1100_bi_dma_queue_buffer_irq(dmachn_rx, dma_curpos, urb->endpoint->rcv_packetSize);
            _sa1100_bi_dma_run(dmachn_rx);
        }

        restore_flags(flags);
    }
}


/* ep1 public functions ************************************************************************ */

/**
 * ep1_init - initialize the endpoint
 *
 */
void ep1_enable(struct usb_device_instance *device, struct usb_endpoint_instance *endpoint, int chn)
{
    if (endpoint) {
        int ok;
        ep1_device = device;
        ep1_endpoint = endpoint;
        dmachn_rx = chn;

        //usbd_fill_rcv(device, endpoint, 5);

        SET_AND_TEST(*(UDCOMP) = ep1_endpoint->rcv_packetSize-1, udc(UDCOMP) != ep1_endpoint->rcv_packetSize-1, ok);
        if (!ok) {
            dbg_rx(0,"FAILED setting pktsize %d", *(UDCOMP));
        }

        // setup the dma engine operating parameters
        /*
        {
            unsigned long flags;
            save_flags_cli(flags);
            */
            dma_active = 0;
            dma_enabled = 1;
            ep1_endpoint->rcv_urb = first_urb_detached(&ep1_endpoint->rdy);
            /*
            ep1_endpoint->rcv_urb = first_urb_detached_irq(&ep1_endpoint->rdy);
            restore_flags(flags);
        }
        */
        ep1_start_dma(ep1_endpoint->rcv_urb);
    }
}


/**
 * ep1_reset - reset the endpoint
 *
 * Return non-zero if error.
 */
void ep1_reset(void)
{
    int ok;

    dbg_rx(1, "reset");

    ep1_stop_dma(ep1_endpoint->rcv_urb);

    if (ep1_endpoint->rcv_urb) {
        ep1_endpoint->rcv_urb->actual_length = 0;
    }
    ep1_start_dma(ep1_endpoint->rcv_urb);
    
    udc_reset_ep(1);
    SET_AND_TEST(*(UDCOMP) = ep1_endpoint->rcv_packetSize-1, udc(UDCOMP) != ep1_endpoint->rcv_packetSize-1, ok);
    if (!ok) {
        dbg_rx(0,"FAILED setting pktsize %d", *(UDCOMP));
    }
}


/**
 * ep1_disable - disable endpoint for use
 *
 */
void ep1_disable()
{
    dbg_rx(1, "disable");

    if (dma_active) {
        ep1_stop_dma(ep1_endpoint->rcv_urb);
    }

    if (!dma_enabled) {
        dbg_rx(1,"!dma_enabled: %x %x %x", dmachn_rx, dma_enabled, dma_active);
        return;
    }
    {
        unsigned long flags;
        save_flags_cli(flags);
        if (dma_active) {
            restore_flags(flags);
            dbg_rx(1, "dma_active: %x %x %x", dmachn_rx, dma_enabled, dma_active);
            ep1_stop_dma(ep1_endpoint->rcv_urb);
            usbd_dealloc_urb(ep1_endpoint->rcv_urb);
            ep1_endpoint->rcv_urb = NULL;
        }
        dma_enabled = 0;
        restore_flags(flags);
    }

    usbd_flush_rcv(ep1_endpoint);
    ep1_endpoint = NULL;
}


/*
 * Endpoint 1 interrupts will be directed here.
 *
 * status : the contents of the UDC Status/Interrupt Register.
 */
void ep1_int_hndlr(unsigned int status)
{
    dma_addr_t dma_address;
    unsigned int udccs1;
    int ok;
    unsigned int len = 0;
    unsigned int urb_bad = 0;
    
    rx_interrupts++;

    if (!((udccs1 = *(UDCCS1)) & UDCCS1_RPC)) {
        SET_AND_TEST( *(UDCCS1) = UDCCS1_SST, _udc(UDCCS1) & UDCCS1_SST, ok);
        SET_AND_TEST( *(UDCCS1) = UDCCS1_RPC, _udc(UDCCS1) & UDCCS1_RPC, ok);
        return;
    }

    //dbg_rx(4, "udccs1: %02x", udccs1);

    dma_active = 0;

    // DMA or non-DMA
    // XXX - we should be able to collapse these together
    if (ep1_endpoint->rcv_packetSize <= 20) {

        // non-DMA version
        if (!(udccs1 & (UDCCS1_RPE))) {

            // get residual data from fifo
            if (ep1_endpoint->rcv_urb) {
                unsigned char *dmabuf;

                dmabuf = ep1_endpoint->rcv_urb->buffer + ep1_endpoint->rcv_urb->actual_length;

                // XXX Note that it is apparantly impossible to do this with 100%
                // reliability. Only copy data in if/while length is less than packet size

                for (; (len < (unsigned) ep1_endpoint->rcv_packetSize) && *(UDCCS1) & UDCCS1_RNE; len++) {
                    *dmabuf++ = *(UDCDR);
                    //udelay(1);
                }
                // record an error if the FIFO is not empty
                udc_ep1_errors += ((urb_bad = *(UDCCS1) & UDCCS1_RNE))?1:0;
            }
            else {
                dbg_rx(0, "NO RCV URB");
            }
        }
        else {
            udc_rpe_errors++;
        }
    }
    // retrieve dma_address, note that this is not reliable
    else if (!_sa1100_bi_dma_stop_get_current_irq(dmachn_rx, &dma_address)) {

        // DMA version
        // stop dma engine, if we can get the current address, unmap, get residual data, and give to bottom half
        pci_unmap_single(NULL, dma_curpos, ep1_endpoint->rcv_packetSize, PCI_DMA_FROMDEVICE);

        if (!(udccs1 & (UDCCS1_RPE))) {

            // get residual data from fifo
            if (ep1_endpoint->rcv_urb) {
                unsigned char *dmabuf;

                len = dma_address - dma_curpos;
                dmabuf = ep1_endpoint->rcv_urb->buffer + ep1_endpoint->rcv_urb->actual_length + len;

                /* WARNING - it is impossible empty the FIFO with 100% accuracy. 
                 *
                 * We only copy data in if/while length is less than packet size
                 *
                 * There are two problems. First fetching the dma address can fail. In this case len will
                 * be too large and we do not attempt to read any data.
                 *
                 * Second, reads sometimes fail to update the FIFO read address resulting in too much
                 * data being retrieved. Slightly delaying between each read helps to reduce this problem
                 * at the expense of increasing latency.
                 *
                 * This (and the similiar problem with the TX FIFO) are the reason that it is NOT SAFE
                 * to operate the SA1100 without a CRC across all bulk transfers. The normal USB CRC which
                 * the hardware uses to protect each packet being sent is not sufficent. The data is being
                 * damaged getting it into and out of the FIFO(s).
                 */
                
                for (; (len < (unsigned) ep1_endpoint->rcv_packetSize) && *(UDCCS1) & UDCCS1_RNE; len++) {
                    *dmabuf++ = *(UDCDR);
                    udelay(0);
                }

                // record an error if the FIFO is not empty
                udc_ep1_errors += ((urb_bad = *(UDCCS1) & UDCCS1_RNE))?1:0;
            }
            else {
                dbg_rx(0, "NO RCV URB");
            }
        }
        else {
            udc_rpe_errors++;
        }
    }
    else {
        //dbg_rx(0, "nothing to unmap");
    }

    // let the upper levels handle the urb
    usbd_rcv_complete_irq(ep1_endpoint, len, urb_bad);

    // clear SST if necessary
    if (udccs1 & UDCCS1_SST) {
        SET_AND_TEST( *(UDCCS1) = UDCCS1_SST, _udc(UDCCS1) & UDCCS1_SST, ok);
        if (!ok) {
            dbg_rx(0, "could not clear SST: %02x", *(UDCCS1));
        }
    }

    // start DMA and reset RPC 
    SET_AND_TEST( *(UDCCS1) = UDCCS1_RPC, _udc(UDCCS1) & UDCCS1_RPC, ok);
    if (!ok) {
        dbg_rx(0, "could not clear RPC: %02x", *(UDCCS1));
    }

    if (dma_enabled && ep1_endpoint->rcv_urb) {
        dma_active = 1;

        // start DMA if packetsize to big for FIFO (twenty bytes)
        if ((dma_curpos = pci_map_single(NULL, (void *)ep1_endpoint->rcv_urb->buffer+ ep1_endpoint->rcv_urb->actual_length, 
                        ep1_endpoint->rcv_packetSize, PCI_DMA_FROMDEVICE)) && (ep1_endpoint->rcv_packetSize > 20)) 
        {
            _sa1100_bi_dma_queue_buffer_irq(dmachn_rx, dma_curpos, ep1_endpoint->rcv_packetSize);
            _sa1100_bi_dma_run(dmachn_rx);
        }
    }
}

