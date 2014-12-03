/*
 * linux/drivers/usbd/sa1100_bi/ep0.c
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
 * All comments and references to register values like:
 *
 *      c.f. 11.8.7.1
 *
 * refer to The Intel SA-1110 Developers Manual.
 *
 * See also the application note from Intel:
 *
 *      Universal Serial Bus (USB) Client Device Validataion for 
 *      the StrongARM(tm) SA-1100 Microprocessor 
 *
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>

#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "../usbd.h"
#include "../usbd-debug.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "sa1100.h"

#define MIN(a,b)(((a)<(b))?(a):(b))


#define IOIOIO(reg,val,set,test,rc) do { \
        SET_AND_TEST(set,test,rc); \
        } while(0)

/* 
 * EP0 State - See the Intel Application Note
 */

#define EP0_STATE_IDLE 0
#define EP0_STATE_IN_DATA_PHASE 1
#define EP0_STATE_END_IN 2

char *ep0_state_desc[] = {
    "idle", "in", "end"
};

static int ep0_state = EP0_STATE_IDLE;

//struct urb ep0_urb;
struct urb *ep0_urb;


static unsigned char *buffer;
static unsigned int buffer_cnt;


/*
 * There are two scenarios: 
 *
 *   1. Data to be sent is an exact multiple of the packetsize and less than
 *   the requested size. An empty packet needs to be sent.
 *
 *   2. Everything else. The last packet will be recognized as the last
 *   because it is less than the packetsize OR because we have sent exactly
 *   the required amount of data.
 */

static unsigned int send_zlp;

struct usb_endpoint_instance *ep0_endpoint;

char *send_zlp_description[] = {
    "exact", "forced"
};

int ep0_enable(struct usb_device_instance *device, struct usb_endpoint_instance *endpoint)
{
    ep0_endpoint = endpoint;

    if (!ep0_urb) {
        if (!(ep0_urb = usbd_alloc_urb(device, device->function_instance_array, 0, 512))) {
            dbg_ep0(1, "ep0_enable: usbd_alloc_urb failed\n");
        }
    }
    else {
        dbg_ep0(1, "ep0_enable: ep0_urb already allocated\n");
    }
    return 0;
}

void ep0_disable(void)
{
    if (ep0_urb) {
        usbd_dealloc_urb(ep0_urb);
        ep0_urb = 0;
    }
    else {
        dbg_ep0(1, "ep0_disable: ep0_urb already NULL\n");
    }
}

int ep0_reset(void)
{
    dbgENTER(dbgflg_usbdbi_ep0,1);
    ep0_state = EP0_STATE_IDLE;
    udc_reset_ep(0);
    return 0;
}

/* 
 * ep0_set_opr_and_de
 *
 * Set OPR and optionally DE.
 */
static void ep0_set_opr_and_de(int flag)
{
    int ok;
    IOIOIO(UDCCS0, UDCCS0_S0, *(UDCCS0) = UDCCS0_SO | (flag?UDCCS0_DE:0) , (*(UDCCS0) & UDCCS0_OPR), ok);
    if (!ok) {
        dbg_ep0(0, "failed to reset OPR");
    }
}


/*
 * ep0_read_data
 *
 * Retreive setup data from FIFO.
 */
__inline__ static int ep0_read_data(struct usb_device_request *request, int max)
{
    int fifo_count;
    int bytes_read;
    int i;
    int reads;
    unsigned char *cp = (unsigned char *) request;

    fifo_count = *(UDCWC) & 0xff;

    if (fifo_count != max) {
        dbg_ep0(1, "ep0_read_data: have %d bytes; want max\n", fifo_count);
    }

    reads = 0;
    bytes_read = 0;
    for (reads = 0, bytes_read = 0; fifo_count--; ) {
        i = 0;
        do {
            *cp = (unsigned char) *(UDCD0);
	    //udelay(0);
            reads++;
            i++;
        } while (((*(UDCWC)&0xff) != fifo_count) && (i < 10));
        if (i == 10) {
            dbg_ep0(0, "failed: UDCWC: %x\n", *(UDCWC));
        }
        cp++;
        bytes_read++;
        if ((bytes_read >= max) && (fifo_count > 0)) {
            dbg_ep0(0, "reading too much data\n");
	    break;
        }
    }

    dbg_ep0(5, "ep0_read_data: reads: %d want: %d fifo_count: %d bytes_read: %d CS0: %02x CWC: %02x", 
            reads, max, fifo_count, bytes_read, *(UDCCS0), *(UDCWC));

    return bytes_read;
}


/* *
 * ep0_read_request
 *
 * Called when in IDLE state to get a setup packet.
 */
static void ep0_read_request(unsigned int udccs0, unsigned int udccr)
{
    int i;
    int ok;

    if (!(udccr & UDCCR_UDA)) {
        dbg_ep0(2,"warning, UDC not active");
    }

    // reset SST
    if (udccs0 & UDCCS0_SST) {
        IOIOIO(UDCCS0, UDCCS0_SST, *(UDCCS0) = UDCCS0_SST, *(UDCCS0) & UDCCS0_SST, ok);
        if (!ok) {
            dbg_ep0(0, "failed to reset SST");
        }
    }

    // check UDC Endpoint 0 Control/Status register for OUT Packet Ready (OPR) (c.f. 11.8.7.1)
    if (!(udccs0 & UDCCS0_OPR)) {  
        // wait for another interrupt
        dbg_ep0(3, "wait for another interrupt CS0: %02x", udccs0);
        return;
    }


    // read a device request
    ep0_urb->actual_length = 0;
    if ((i = ep0_read_data(&ep0_urb->device_request, 8)) < 8) {
        ep0_set_opr_and_de(0);
        dbg_ep0(2, "not enough data: %d", i);
        return;
    }

    //ep0_need_data = le16_to_cpu(ep0_urb->device_request.wLength);
    dbg_ep0(1, "bmRequestType:%02x bRequest:%02x wValue:%04x wIndex:%04x wLength:%04x",
            ep0_urb->device_request.bmRequestType, 
            ep0_urb->device_request.bRequest,
            le16_to_cpu(ep0_urb->device_request.wValue),
            le16_to_cpu(ep0_urb->device_request.wIndex),
            le16_to_cpu(ep0_urb->device_request.wLength));

    {
        char * cp = (char *) &ep0_urb->device_request;
        dbg_ep0(2, "%02x %02x %02x %02x %02x %02x %02x %02x",
                cp[0], cp[1], cp[2], cp[3], 
                cp[4], cp[5], cp[6], cp[7]
                );
    }

    // check if we have been initialized
    if (!ep0_urb->device) {
        ep0_set_opr_and_de(1);
        udc_stall_ep(0);
        return;
    }
    
    // submit urb to ep0, stall if non-zero result
    if (usbd_recv_setup(ep0_urb)) {
        dbg_ep0(2, "usb_recv_setup failed, stalling");
        ep0_set_opr_and_de(1);
        udc_stall_ep(0);
        return;
    }

    // check direction of data transfer
    if ((ep0_urb->device_request.bmRequestType&USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) {

        // device request has specified Host-to-Device
        dbg_ep0(5,"HOST2DEVICE wLength: %d length: %d", le16_to_cpu(ep0_urb->device_request.wLength), ep0_urb->actual_length);
        ep0_set_opr_and_de(1);
        return;
    }

    // device reqeust has specified Device-to-Host, so we should be returning data

    // check request length, zero is not legal
    if (!le16_to_cpu(ep0_urb->device_request.wLength)) {
        dbg_ep0(2,"wLength zero, stall");
        ep0_set_opr_and_de(1);
        udc_stall_ep(0);
        return;
    }

    // check that we have some data to send back, zero should not be possible
    if (!ep0_urb->actual_length) {
        dbg_ep0(2,"no data, stall");
        ep0_set_opr_and_de(1);
        udc_stall_ep(0);
        return;
    }

    // everything looks sane, so set buffer pointers and setup for data transfer
    buffer = ep0_urb->buffer;
    buffer_cnt = ep0_urb->actual_length;
    
    /*
     * IFF we are sending a multiple of the packet size AND less than was
     * requested we will need to send an empty packet after all the data packets.
     */
    if (!(buffer_cnt%8) && (buffer_cnt < le16_to_cpu(ep0_urb->device_request.wLength))) {
        send_zlp = 1;
        dbg_ep0(2,"DEVICE2HOST: SEND ZLP %x %x", ep0_urb->actual_length, le16_to_cpu(ep0_urb->device_request.wLength));
    }
    else {
        send_zlp = 0;
        dbg_ep0(2,"DEVICE2HOST: NO ZLP %x %x", ep0_urb->actual_length, le16_to_cpu(ep0_urb->device_request.wLength));
    }

    ep0_state = EP0_STATE_IN_DATA_PHASE;
}

/* 
 * ep0_in
 *
 * Send some data to host.
 */
static void ep0_in(unsigned int udccs0)
{
    int transfer_cnt = 0;
    unsigned int fifo_count;
    volatile int writes=0;
    unsigned long start = jiffies;
    int ok;

    udccs0 = *(UDCCS0);
    dbg_ep0(2,"CS0: %02x", udccs0);

    // check for non empty FIFO
    if (*(UDCWC)) {
        dbg_ep0(0, "FIFO not empty");
        return;
    }

    // process iff ep0 is not stalled, no premature end and IPR is clear
    if (udccs0 & (UDCCS0_SE|UDCCS0_SST)) {
        dbg_ep0(1, "ep0_in: SE or SST set\n");
        return;
    }

    // first check that IPR is not set, if it is then we wait for it to clear.
    if (udccs0 & UDCCS0_IPR) {
        
        for (ok = 0; (*(UDCCS0) & UDCCS0_IPR) && ok < 2000; ok++) {
            udelay(1); // XXX 
        }
        if (ok == 2000) {
            dbg_ep0(0, "IPR clear timed out");
        }
        dbg_ep0(0, "IPR was set count: %d", ok);
    }

    // get up to first 8 bytes of data
    transfer_cnt = MIN(8, buffer_cnt);
    buffer_cnt -= transfer_cnt;
    fifo_count=0;

    do {
        int count = 0;
        do {
            *(UDCD0) = (unsigned long) buffer[fifo_count];
            count++;
            writes++;
        } while ((udc(UDCWC) == fifo_count) && (count < 2000));

        if (udc(UDCWC) <= fifo_count) {
            dbg_ep0(0,"sending STALL, failed writing FIFO transfer: %d fifo_count: %x CS0: %02x CWC: %02x CAR: %02x", 
                    transfer_cnt, fifo_count, *(UDCCS0), *(UDCWC), *(UDCAR));
            udc_stall_ep(0);
            ep0_state = EP0_STATE_IDLE;
            return;
        }

        fifo_count++;

    } while (fifo_count < transfer_cnt);

    dbg_ep0(2, "elapsed: %lx writes: %d fifo_count: %d transfer_cnt: %d buffer_cnt: %d CS0: %02x CWC: %02x", 
            jiffies-start, writes, fifo_count, transfer_cnt, buffer_cnt, *(UDCCS0), *(UDCWC));

    buffer += transfer_cnt;

    // set data end, and start 
    if (buffer_cnt == 0) {
        ep0_state = EP0_STATE_END_IN;
    }

    IOIOIO(UDCCS0, UDCCS0_IPR, 
            *(UDCCS0) = (UDCCS0_IPR | (( ((!buffer_cnt) && (send_zlp != 1))?UDCCS0_DE:0))), 
            !(*(UDCCS0) & UDCCS0_IPR), ok);

    if (!ok) {
        dbg_ep0(0, "failed to set IPR");
    }
}

/*
 * ep0_end_in
 *
 * Handle end of IN, possibly with ZLP.
 */
static void ep0_end_in(unsigned int udccs0)
{
    int ok;

    dbg_ep0(2," status: %02x CS0: %02x", udccs0, *(UDCCS0));

    // reset SST if necessary
    if (udccs0 & UDCCS0_SST) {
        IOIOIO(UDCCS0, UDCCS0_SST, *(UDCCS0) = UDCCS0_SST, *(UDCCS0) & UDCCS0_SST, ok);
        if (!ok) {
            dbg_ep0(0, "failed to reset SST");
        }
    }
    // reset SE if necessary
    if (udccs0 & UDCCS0_SE) {
        IOIOIO(UDCCS0, UDCCS0_SSE, *(UDCCS0) = UDCCS0_SSE, *(UDCCS0) & UDCCS0_SSE, ok);
        if (!ok) {
            dbg_ep0(0, "failed to reset SE");
        }
    }

    // ensure that IPR is not set and send zero length packet if necessary
    if ( !(udc(UDCCS0) & UDCCS0_IPR) && (send_zlp == 1)) {
        IOIOIO(UDCCS0, UDCCS0_DE|UDCCS0_IPR, *(UDCCS0) = (UDCCS0_DE | UDCCS0_IPR), *(UDCCS0) & UDCCS0_IPR, ok);
        if (!ok) {
            dbg_ep0(0, "failed to set DE and IPR");
        }
    }

    // set state to IDLE
    ep0_state = EP0_STATE_IDLE;
    send_zlp = 0;
}



/* 
 * ep0_int_hndlr
 *
 * Handle ep0 interrupts.
 *
 * Generally follows the example in the Intel Application Note.
 *
 */
void ep0_int_hndlr(unsigned int status)
{   
    unsigned int udccs0;
    unsigned int udccr;

    udccs0 = udc(UDCCS0);
    udccr = udc(UDCCR);

    ep0_interrupts++;

    dbg_ep0(3,"------------>[%d:%s:%s CCR: %02x CS0: %02x CWC: %02x CAR: %02x]", 
            ep0_interrupts, ep0_state_desc[ep0_state], send_zlp_description[send_zlp], udccr, udccs0, *(UDCWC), *(UDCAR));

    if (udccs0 == UDCCS0_IPR) {
        dbg_ep0(2,"IPR SET [%d:%02x]", ep0_state, udccs0);
        return;
    }

    if (udccs0 & UDCCS0_SST) {
        dbg_ep0(2,"previously sent stall [%d:%02x]", ep0_state, udccs0);
    }
    
    // SE 
    if (udccs0 & UDCCS0_SE) {
        int ok;
        dbg_ep0(2,"unloading, found UDCCS0_SE: CS0: %02x", udccs0);
        
        // reset SE
        IOIOIO(UDCCS0, UDCCS0_SSE, *(UDCCS0) = UDCCS0_SSE, *(UDCCS0) & UDCCS0_SE, ok);
        if (!ok) {
            dbg_ep0(0, "failed to reset SE");
        }
        
        // clear SE before unloading any setup packets 
        if (udccs0 & UDCCS0_OPR) {
            // reset OPR
            *(UDCCS0) = UDCCS0_SO;
        }
        ep0_state = EP0_STATE_IDLE;
        send_zlp = 0;
    }
    
    if ( (ep0_state != EP0_STATE_IDLE) && (udccs0 & UDCCS0_OPR)) {
        dbg_ep0(2,"Forcing EP0_STATE_IDLE CS0: %02x", udccs0);
        ep0_state = EP0_STATE_IDLE;
    }

    switch (ep0_state) {
    case EP0_STATE_IDLE :
        dbg_ep0(3, "state: EP0_STATE_IDLE CS0: %02x", udccs0);
        ep0_read_request(udccs0, udccr);
        if (ep0_state != EP0_STATE_IN_DATA_PHASE) {
            break;
        }

        // fall through to data phase
        ep0_set_opr_and_de(0);

    case EP0_STATE_IN_DATA_PHASE :
        dbg_ep0(3, "state: EP0_STATE_IN_DATA_PHASE %x", udccs0);
        ep0_in(udccs0);
        break;

    case EP0_STATE_END_IN :
        dbg_ep0(3, "state: EP0_STATE_END_IN %x", udccs0);
        ep0_end_in(udccs0);
        break;
    }
    dbg_ep0(3,"<------------[%s:%s CCR: %02x CS0: %02x CWC: %02x]", 
            ep0_state_desc[ep0_state], send_zlp_description[send_zlp], *(UDCCR), *(UDCCS0), *(UDCWC));
}

