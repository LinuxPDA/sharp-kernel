/*
 * linux/drivers/usbd/l7205_bi/send.c -- L7205 USB controller driver. 
 *
 * Copyright (c) 2000, 2001 Lineo
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

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/cache.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/system.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/arch/gpio.h>
//#include <asm/arch/dma.h>
//#include <asm/arch/arch-l7200/dma.h>


#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

#include "../usbd.h"
#include "../usbd-debug.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "hardware.h"
#include "l7205.h"
//#include "ctl.h"

/* ep2 dma engine functions ******************************************************************** */

//static int dmachn_tx;                           // tx dma channel

struct usb_device_instance *ep2_device;
struct usb_endpoint_instance *ep2_endpoint;

#ifdef USE_DMA
static int ep2_usb_irq;
#endif

int ep2_active;
extern int saw_f1err;

#define MIN(a,b) ((a)<(b))?(a):(b)
#define MAX(a,b) ((a)>(b))?(a):(b)

struct dma_buf {
    unsigned char      *buffer;         // where to send from 
    int                 last;           // size of mapped address
    int                 todo;           // amount left to do
    struct urb         *urb;
};

#define MAX_DMA_BUFS 1
struct dma_buf dma_bufs[MAX_DMA_BUFS];


/* ep2 public functions ************************************************************************ */


static void ep2_dma_callback(int irq, void *dummpy, struct pt_regs *fp)
{
    //printk(KERN_DEBUG"ep2_dma_callback:\n");
}

/**
 * ep2_enable -
 * @chn:
 *
 * Initialize the dma variables. Called once.
 *
 */
int ep2_enable(struct usb_device_instance *device, struct usb_endpoint_instance *endpoint)
{
    printk(KERN_DEBUG"ep2_enable:\n");
    ep2_device = device;
    ep2_endpoint = endpoint;
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
}

/*
 * reset any txpkts, it will be up to the client driver to free up the send
 * queue.
 */
void ep2_reset(void)
{
}

/* Interrupt service routines ****************************************************************** */

int ep2_fill(unsigned char *cp, int len) 
{
    int j;
    int flag = 0;
    static int ep2_last = 0;
    unsigned long *buf = (unsigned long *) cp;

    //printk(KERN_DEBUG"ep2_fill: len: %d ep2_last: %d\n", len, ep2_last);

    // reset fifo read pointer
    l7205_toggle(IO_USBF_CONTROL, USBF_CONTROL_F2CLR);
#if 0
    printk(KERN_DEBUG"fifo:");
    for (j = 0; (j < 8); j++) {
        unsigned long *p2 = (unsigned long *) IO_USBF_FIFO_TX;
        printk("%08x ", p2[j]);
    }
    printk("\n");

    // reset fifo write pointer to beginning of fifo
    for (;ep2_last < 8; ep2_last++) {
        IO_USBF_FIFO2 = 0x12345678;
    }

    printk(KERN_DEBUG"fifo:");
    for (j = 0; (j < 8); j++) {
        unsigned long *p2 = (unsigned long *) IO_USBF_FIFO_TX;
        printk("%08x ", p2[j]);
    }
    printk("\n");
#endif

    ep2_last = (len + 3) / 4;

    // copy data into fifo
    for (j = 0; j < ep2_last; j++) {
        IO_USBF_FIFO2 = buf[j];
    }

    //if (!ignore_f1ne && (IO_USBF_STATUS & USBF_STATUS_F1NE)) 

    // check fifo contents
    for (j = 0; !flag && (j < ep2_last); j++) {
        unsigned long *p2 = (unsigned long *) IO_USBF_FIFO_TX;
        flag |= (buf[j] != p2[j]);
    }
#if 0
    if (flag) {
        /printk(KERN_DEBUG"buf: ");
        for (j = 0; (j < 8); j++) {
            printk("%08x ", buf[j]);
        }
        printk("\n");
        printk(KERN_DEBUG"fifo:");
        for (j = 0; (j < 8); j++) {
            unsigned long *p2 = (unsigned long *) IO_USBF_FIFO_TX;
            printk("%08x ", p2[j]);
        }
        printk("\n");
    }
#endif
    //printk(KERN_DEBUG"ep2_fill: len: %d ep2_last: %d flag: %d\n", len, ep2_last, flag);
    return flag;
}


/*
 *
 * Interrupt handler - called with interrupts disabled.
 */
int ep2_start(unsigned int status, unsigned int rawstatus, int in_interrupt, int error, int mask, int ignore_f1ne)
{
    int len = 0;
    int i;
    int j;
    int flag = 0;

    ep2_active = 0;

    // if we don't have an urb try and get one
    if (ep2_endpoint->tx_urb) {

        if (!(len = MIN((ep2_endpoint->tx_urb->actual_length - ep2_endpoint->sent), 
                        ep2_endpoint->tx_urb->endpoint->tx_packetSize)))
        {
            //printk(KERN_DEBUG"ep2_start: nothing to send\n");
            return 0;
        }

        {
            char *cp = ep2_endpoint->tx_urb->buffer + ep2_endpoint->sent;
            if (ep2_fill(cp, len) && ep2_fill(cp, len) && ep2_fill(cp, len)) {
                printk(KERN_DEBUG"ep2_start: FIFO copy failed sending truncated packet: %d\n", len);
                ep2_endpoint->last = 0;
                ep2_endpoint->sent = 0;
                len--;
            }
        }


        //printk(KERN_DEBUG"ep2_start: sending: %d\n", len);
        ep2_endpoint->last = len;
        IO_USBF_F2BCNT = len;
        ep2_active = 1;
        }
    return 0;
}

/*
 *
 * Interrupt handler - called with interrupts disabled.
 */
void ep2_int_hndlr(unsigned int status, unsigned int rawstatus, int in_interrupt, int error, int mask, int ignore_f1ne)
{
    int restart = 0;

    tx_interrupts++; 

    //reset stall and restart if an error
    if ((status & (USBF_STATUS_F2ERR|USBF_STATUS_F2BSY)) || error) {

        if (status & USBF_STATUS_F2BSY) {
            return;
        }

        printk(KERN_DEBUG"ep2_int_hndlr[%d:%d]: F2ERR %08x\n", udc_interrupts, tx_interrupts-1, IO_USBF_STATUS);

        l7205_toggle(IO_USBF_CONTROL, USBF_CONTROL_F2CLR);
        restart = 1;
        ep2_reset();
        IO_USBF_INTCLR = mask;
        return;
    }

    // if we have active buffer, umap the buffer and update position if previous send was successful
    if (ep2_endpoint->tx_urb) {

        if (!restart) {
            ep2_endpoint->sent += ep2_endpoint->last;
            ep2_endpoint->last = 0;
        }

        // if current buffer is finished call urb sent and advance to next urb 
        if ((ep2_endpoint->tx_urb->actual_length - ep2_endpoint->sent) <= 0) {
            usbd_urb_sent_irq(ep2_endpoint->tx_urb, SEND_FINISHED_OK);
            ep2_endpoint->tx_urb = NULL;
        }
    }

    IO_USBF_INTCLR = mask;
    ep2_start(status, rawstatus, in_interrupt, error, mask, ignore_f1ne);
}


/**
 * udc_start - start transmit
 * @ep:
 */
void ep2_start_tx(unsigned int ep, struct usb_endpoint_instance *endpoint, int restart)
{
    if (!(IO_USBF_STATUS & USBF_STATUS_F2BSY)) {
        ep2_start(0, 0, 0, 0, 0, 0);
    }
}

