/*
 * linux/drivers/usbd/l7205_bi/recv.c -- L7205 USB controller driver. 
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
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/mach/dma.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/skbuff.h>

#include "../usbd.h"
#include "../usbd-debug.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "hardware.h"
#include "l7205.h"
//#include "ctl.h"

struct usb_device_instance *ep1_device; 
struct usb_endpoint_instance *ep1_endpoint;

unsigned ep1_transfersize;

extern int saw_f1err;

/* ep1 public functions ************************************************************************ */

void __inline__ ep1_clear(int mask)
{
    volatile unsigned int junk;
    int j;
    if (mask) {
        IO_USBF_INTCLR = mask;
    }
    for (j = 0; j < 8; j++) {
        junk = IO_USBF_FIFO1 ;
    }
    //l7205_toggle(IO_USBF_CONTROL, USBF_CONTROL_F2CLR);
    IO_USBF_F1BCNT = 0;
}

void ep1_tick(void)
{
    unsigned long flags;
    save_flags_cli(flags);
    if (IO_USBF_STATUS & USBF_STATUS_F1NE) {
        //printk(KERN_DEBUG"ep1_tick[%d]: F1NE\n", udc_interrupts);
        ep1_clear(0);
    }
    restore_flags(flags);
}

/**
 * ep1_enable - enable endpoint for use
 *
 */
int ep1_enable(struct usb_device_instance *device, struct usb_endpoint_instance *endpoint)
{
    ep1_device = device;
    ep1_endpoint = endpoint;
    return 0;
}

/**
 * ep1_reset - reset the endpoint
 *
 * Return non-zero if error.
 */
void ep1_reset(void)
{
    return;
}


/**
 * ep1_disable - disable endpoint for use
 *
 */
void ep1_disable()
{
    struct urb *urb;
    ep1_endpoint = NULL;
}

static unsigned char last_packet[10*32];

/*
 * Endpoint 1 interrupts will be directed here.
 *
 * status : the contents of the UDC Status/Interrupt Register.
 */
void ep1_int_hndlr(unsigned int status, unsigned int rawstatus, int error, int mask)
{
    dma_addr_t dma_address;
    unsigned int udccs1;
    int ok;
    unsigned char *bufp;
    unsigned long *lp;
    int len;
    int j;
    
    rx_interrupts++;

    if (!ep1_endpoint) {
        return 0;
    }

    if (!error && !ep1_endpoint->rcv_urb) {
        printk(KERN_DEBUG"ep1_int_hndlr[%d:%d]: no ep1_endpoint->rcv_urb!\n", udc_interrupts, rx_interrupts);
        ep1_endpoint->rcv_urb = first_urb_detached(&ep1_endpoint->rdy);

        if (!ep1_endpoint->rcv_urb) {
            printk(KERN_DEBUG"ep1_int_hndlr[%d:%d]: still no ep1_endpoint->rcv_urb!\n", udc_interrupts, rx_interrupts);
            error = 1;
        }
    }

    if (!(status & USBF_STATUS_F1NE) || error) {
        if (mask) {
            IO_USBF_INTCLR = mask;
        }
        ep1_clear(mask);
        return;
    }

    // get actual number of bytes available in FIFO
    len = IO_USBF_F1BCNT;

    // prepare to copy
    bufp = ep1_endpoint->rcv_urb->buffer + ep1_endpoint->rcv_urb->actual_length;
    lp = (unsigned long*) bufp;

    // checked later to preserve timing, for production, check here
#if 0
    // Paranioa - make sure we do not overflow the buffer
    if ((ep1_endpoint->rcv_urb->actual_length + 4*((3+len)/4)) > ep1_endpoint->rcv_urb->buffer_length) {
        printk(KERN_DEBUG "ep1_int_handler: buffer overflow al:%u+len:%u > bl:%u\n",ep1_endpoint->rcv_urb->actual_length,len,ep1_endpoint->rcv_urb->buffer_length);
        len = ep1_endpoint->rcv_urb->buffer_length - ep1_endpoint->rcv_urb->actual_length;
    }
#endif

    // copy 4 bytes at a time, fetching more than available seems ok
    for (j = 0; j < 8; j++) {
        lp[j] = IO_USBF_FIFO1 ;
    }

#if 0  // TBR
    if (saw_f1err || !(rx_interrupts&0xfff))
    {
        int i;
        printk(KERN_DEBUG"ep1_int_hndlr[%d:%d]: ", udc_interrupts, rx_interrupts);
        for (i = 0; i < 32; i++) {
            printk("%02x ", bufp[i]);
        }
        //printk(".. ");
        //for (i = 26; i < len; i++) {
        //    printk("%02x ", bufp[i]);
        //}
        printk("\n");
    }
#endif

#if 0
    if (IO_USBF_STATUS & USBF_STATUS_F1NE) {
        volatile unsigned int junk;
        printk(KERN_DEBUG"ep1_int_hndlr[%d:%d]: F1NE still set len: %d j: %d F1BCNT: %d Status: %08x\n", 
                udc_interrupts, rx_interrupts, len, j, IO_USBF_F1BCNT, IO_USBF_STATUS);

        ep1_clear(mask);

        if (IO_USBF_STATUS & USBF_STATUS_F1NE) {
            printk(KERN_DEBUG"ep1_int_hndlr[%d:%d]: F1NE really still set j: %d F1BCNT: %d Status: %08x\n", 
                    udc_interrupts, rx_interrupts, j, IO_USBF_F1BCNT, IO_USBF_STATUS);
        }
    }
    //ep1_clear(mask);
#endif

    if (mask) {
        IO_USBF_INTCLR = mask;
    }
    IO_USBF_F1BCNT = 0;

#if 0
    // Paranioa - make sure we have not overflowed the buffer
    if ((ep1_endpoint->rcv_urb->actual_length + 4*((3+len)/4)) > ep1_endpoint->rcv_urb->buffer_length) {
        printk(KERN_DEBUG "ep1_int_handler: buffer overflow al:%u+len:%u > bl:%u\n",ep1_endpoint->rcv_urb->actual_length,len,ep1_endpoint->rcv_urb->buffer_length);
        len = ep1_endpoint->rcv_urb->buffer_length - ep1_endpoint->rcv_urb->actual_length;
    }
#endif

    // add to urb length
    ep1_endpoint->rcv_urb->actual_length += len;

#if 0
    // if we have a short packet or have filled the urb buffer then pass it up 
    if ((len < ep1_endpoint->rcv_packetSize) || 
            ((ep1_endpoint->rcv_urb->buffer_length - ep1_endpoint->rcv_urb->actual_length) < ep1_endpoint->rcv_packetSize))
    {
        usbd_recv_urb_irq(ep1_endpoint->rcv_urb);
        ep1_endpoint->rcv_urb = first_urb_detached(&ep1_endpoint->rdy);
    }
#endif
    if (ep1_endpoint->endpoint_address) {
        //printk(KERN_DEBUG"sl11_out: receive len: %d\n", len);
        usbd_rcv_complete_irq(ep1_endpoint, len, 0);
    }
    else {
        printk(KERN_DEBUG"sl11_out: cannot receive no endpoint address len: %d\n", len);

    }

}

