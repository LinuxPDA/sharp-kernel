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


/* 
 * EP0 State - See the Intel Application Note
 */

#define EP0_STATE_IDLE 0
#define EP0_STATE_IN_DATA_PHASE 1
#define EP0_STATE_END_IN 2

extern unsigned char usb_address;

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


int ep0_enable (struct usb_device_instance *device, struct usb_endpoint_instance *endpoint)
{
    ep0_endpoint = endpoint;

    dbg_ep0(0, "ep0_urb: %p", ep0_urb);
    if (!ep0_urb) {
        if (!(ep0_urb = usbd_alloc_urb (device, device->function_instance_array, 0, 512))) {
            dbg_ep0 (1, "ep0_enable: usbd_alloc_urb failed\n");
        }
    } else {
        dbg_ep0 (1, "ep0_enable: ep0_urb already allocated\n");
    }
    return 0;
}

void ep0_disable (void)
{
    dbg_ep0(0, "ep0_urb: %p", ep0_urb);
    if (ep0_urb) {
        usbd_dealloc_urb (ep0_urb);
        ep0_urb = 0;
    } else {
        dbg_ep0 (1, "ep0_disable: ep0_urb already NULL\n");
    }
}

int ep0_reset (void)
{
    dbgENTER (dbgflg_usbdbi_ep0, 1);
    ep0_state = EP0_STATE_IDLE;
    udc_reset_ep (0);
    return 0;
}

#if 0
static __inline__ void ep0_inactive(void)
{
    int ok = 0;

    while ((*(UDCCR) & UDCCR_UDA) && (ok++ < 6)) {
	udelay(1);
    }
    if (ok) {
        dbg_ep0 (1, "delayed till not active\n");
    }
}

static __inline__ void ep0_active(void)
{
    int ok = 0;

    while (!(*(UDCCR) & UDCCR_UDA) && (ok++ < 6)) {
	udelay(1);
    }
    if (ok) {
        dbg_ep0 (1, "delayed till active\n");
    }
}
#else

/*
 * There times when the UDC signals that it is Active and there are
 * some actions, such as reseting OPR (setting SO) or setting DE
 * that can cause problems IFF they are done while active.
 *
 * Typically the IN/NAK poll consumes about 7.5uS. Using OSCR which
 * has a cycle time of .2712 uS we typically see about 20 ticks. The
 * active portion is 5 ticks, the inactive portion is about 15 ticks.
 *
 * Unfortunately simply testing for not active is no sufficent as there
 * is a small but significant chance that the UDC is about to become
 * active.
 *
 * The only reliable way to fix this is to wait for the device to
 * become active, then inactive, then to the required change.
 *
 * An overall limit of about 10uS is required for when the the host
 * is not polling.
 *
 */

static __inline__ void ep0_inactive(void)
{
    int ok = 0;
    u32 oscr = OSCR;

    while ((*(UDCCR) & UDCCR_UDA) && ((OSCR - oscr) < 30)) {
	//udelay(1);
	ok++;
    }
    if (ok) {
        dbg_ep0 (1, "delayed till not active\n");
    }
}

static __inline__ void ep0_active(void)
{
    int ok = 0;
    u32 oscr = OSCR;

    while (!(*(UDCCR) & UDCCR_UDA) && ((OSCR - oscr) < 30)) {
	//udelay(1);
	ok++;
    }
    if (ok) {
        dbg_ep0 (1, "delayed till active\n");
    }
}
#endif

/* 
 * ep0_set_opr
 *
 * Set OPR and optionally DE.
 */
static void ep0_set_opr (void)
{
    int ok = 0;

    ep0_active();
    ep0_inactive();
    //ep0_active();
    //ep0_inactive();

    SET_AND_TEST(*(UDCCS0) = UDCCS0_SO , (*(UDCCS0) & UDCCS0_OPR), ok);
    if (!ok) {
	dbg_ep0(0, "Failed to reset OPR");
    }
    else {
	dbg_ep0(1, "reset OPR");
    }
}

/* 
 * ep0_set_opr_and_de
 *
 * Set OPR and optionally DE.
 */
static void ep0_set_opr_and_de (void)
{
    int ok = 0;

    ep0_active();
    ep0_inactive();
    //ep0_active();
    //ep0_inactive();

    //SET_AND_TEST2(*(UDCCS0) = (UDCCS0_SO | UDCCS0_DE) , (*(UDCCS0) & UDCCS0_OPR), ok);
    SET_AND_TEST(*(UDCCS0) = (UDCCS0_SO | UDCCS0_DE) , (*(UDCCS0) != UDCCS0_DE), ok);

    if (!ok) {
	dbg_ep0(0, "Failed to reset OPR and DE");
    }
    else {
	dbg_ep0(1, "reset OPR and DE");
    }
}


/*
 * ep0_read_data
 *
 * Retreive setup data from FIFO.
 */
__inline__ static int ep0_read_data (struct usb_device_request *request, int max)
{
    int fifo_count;
    int bytes_read;
    int i;
    int reads;
    unsigned char *cp = (unsigned char *) request;
    int udcwc;

    fifo_count = *(UDCWC) & 0xff;

    if (fifo_count != max) {
        dbg_ep0 (1, "ep0_read_data: have %d bytes; want max\n", fifo_count);
    }

    reads = 0;
    bytes_read = 0;
    for (reads = 0, bytes_read = 0; fifo_count--;) {
        i = 0;
        do {
            *cp = (unsigned char) *(UDCD0);
            // XXX without this reads may be corrupt, typically the wLength field
            // will contain bad data, which in turn will cause get descriptor config
            // to return the full set of descriptors in response to limited first
            // get descriptor 9, which will BSOD windows
            udelay(10);
            reads++;
            i++;
        } while ((((udcwc = *(UDCWC) & 0xff)) > fifo_count) && (i < 10));
        if (i == 10) {
            dbg_ep0 (0, "failed: UDCWC: %2x %2x bytes: %d fifo: %d reads: %d no change\n",
                     udcwc, *(UDCWC), bytes_read, fifo_count, reads);
        }
        if (udcwc < (fifo_count - 1)) {
            dbg_ep0 (0, "failed: UDCWC: %2x %2x bytes: %d fifo: %d reads: %d too low\n",
                     udcwc, *(UDCWC), bytes_read, fifo_count, reads);
        }
        cp++;
        bytes_read++;
        if ((bytes_read >= max) && (fifo_count > 0)) {
            dbg_ep0 (0, "reading too much data\n");
            break;
        }
    }

    dbg_ep0 (2, "ep0_read_data: reads: %d want: %d fifo_count: %d bytes_read: %d CS0: %02x CWC: %02x",
            reads, max, fifo_count, bytes_read, *(UDCCS0), *(UDCWC));

    return bytes_read;
}


/* *
 * ep0_read_request
 *
 * Called when in IDLE state to get a setup packet.
 */
static __inline__ void ep0_read_request (unsigned int udccs0, unsigned int udccr)
{
    int i;
    int ok;

    //if (!(udccr & UDCCR_UDA)) {
    //    dbg_ep0 (3, "warning, UDC not active");
    //}

    // unusual conditions?
    if (udccs0 & (UDCCS0_SST | UDCCS0_OPR)) {
        if (udccs0 & UDCCS0_SST) {
	    // reset SST
	    SET_AND_TEST (*(UDCCS0) = UDCCS0_SST, *(UDCCS0) & UDCCS0_SST, ok);
            if (!ok) {
                dbg_ep0 (0, "failed to reset SST CS0: %02x ok: %d", *(UDCCS0), ok);
            }
        }
        // check UDC Endpoint 0 Control/Status register for OUT Packet Ready (OPR) (c.f. 11.8.7.1)
        if (!(udccs0 & UDCCS0_OPR)) {
            // wait for another interrupt
            dbg_ep0 (3, "wait for another interrupt CS0: %02x", udccs0);
            return;
        }
    }

    // read a device request
    ep0_urb->actual_length = 0;
    if ((i = ep0_read_data (&ep0_urb->device_request, 8)) < 8) {
        ep0_set_opr ();
        dbg_ep0 (2, "not enough data: %d", i);
        return;
    }
    //ep0_need_data = le16_to_cpu(ep0_urb->device_request.wLength);
    dbg_ep0 (3, "bmRequestType:%02x bRequest:%02x wValue:%04x wIndex:%04x wLength:%04x",
            ep0_urb->device_request.bmRequestType,
            ep0_urb->device_request.bRequest,
            le16_to_cpu (ep0_urb->device_request.wValue),
            le16_to_cpu (ep0_urb->device_request.wIndex),
            le16_to_cpu (ep0_urb->device_request.wLength));

    {
        char *cp = (char *) &ep0_urb->device_request;
        dbg_ep0 (1, "%02x %02x %02x %02x %02x %02x %02x %02x",
                cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]
                );
    }

    // check if we have been initialized
    if (!ep0_urb->device) {
        ep0_set_opr_and_de ();
        udc_stall_ep (0);
        return;
    }

    // check direction of data transfer
    if ((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) {

        // if this is a set address save it
        if ((ep0_urb->device_request.bmRequestType == 0) && (ep0_urb->device_request.bRequest == USB_REQ_SET_ADDRESS)) {

            usb_address = le16_to_cpu(ep0_urb->device_request.wValue) & 0xff;

            /*
             * very occasionally register write will fail, delay and redo twice
             * to ensure that it actually got through, cannot check as UDC
             * will not propagate until SO and DE are set. We also need a delay
             * after setting SO and DE, exiting too soon can also result in
             * problems if address has not propagated.
             */


            *(UDCAR) = usb_address;
            udelay(2);
            *(UDCAR) = usb_address;
            udelay(2);
            *(UDCAR) = usb_address;
            ep0_set_opr_and_de();   
            udelay(40);            

            if (usbd_recv_setup(ep0_urb)) {
                dbg_ep0(5, "usb_recv_setup failed, not stalling");
            }
        }
        else {
            //ep0_set_opr_and_de();
            // submit urb to ep0, stall if non-zero result
            if (usbd_recv_setup(ep0_urb)) {
                dbg_ep0(1, "usb_recv_setup failed, stalling");
                udc_stall_ep(0);
            }
            ep0_set_opr_and_de(); // tbr getting better results with this.
        }
    }
    else {
        // submit urb to ep0, stall if non-zero result
        if (usbd_recv_setup (ep0_urb)) {
            dbg_ep0 (2, "usb_recv_setup failed, stalling");
            ep0_set_opr_and_de ();
            udc_stall_ep (0);
            return;
        }

        // device reqeust has specified Device-to-Host, so we should be returning data

        // check request length, zero is not legal
        if (!le16_to_cpu (ep0_urb->device_request.wLength)) {
            dbg_ep0 (0, "wLength zero, stall");
            ep0_set_opr_and_de ();
            udc_stall_ep (0);
            return;
        }
        // check that we have some data to send back, zero should not be possible
        if (!ep0_urb->actual_length) {
            dbg_ep0 (0, "no data, stall");
            ep0_set_opr_and_de ();
            udc_stall_ep (0);
            return;
        }
        // everything looks sane, so set buffer pointers and setup for data transfer
        buffer = ep0_urb->buffer;
        buffer_cnt = ep0_urb->actual_length;

        /*
         * IFF we are sending a multiple of the packet size AND less than was
         * requested we will need to send an empty packet after all the data packets.
         */
        if (!(buffer_cnt % 8) && (buffer_cnt < le16_to_cpu (ep0_urb->device_request.wLength))) {
            send_zlp = 1;
            dbg_ep0 (2, "DEVICE2HOST: SEND ZLP %x %x", ep0_urb->actual_length,
                    le16_to_cpu (ep0_urb->device_request.wLength));
        } else {
            send_zlp = 0;
            dbg_ep0 (2, "DEVICE2HOST: NO ZLP %x %x", ep0_urb->actual_length,
                    le16_to_cpu (ep0_urb->device_request.wLength));
        }

        ep0_state = EP0_STATE_IN_DATA_PHASE;
    }
}

/* 
 * ep0_in
 *
 * Send some data to host.
 */
static __inline__ void ep0_in (unsigned int udccs0)
{
    int transfer_cnt = 0;
    unsigned int fifo_count;
    volatile int writes = 0;
    unsigned long start = jiffies;
    int ok;

    udccs0 = *(UDCCS0);
    dbg_ep0 (2, "CS0: %02x", udccs0);

    // check for non empty FIFO
    if (*(UDCWC)) {
        dbg_ep0 (0, "FIFO not empty");
        return;
    }
    // process iff ep0 is not stalled, no premature end and IPR is clear
    if (udccs0 & (UDCCS0_SE | UDCCS0_SST)) {
        dbg_ep0 (1, "ep0_in: SE or SST set\n");
        return;
    }
    // first check that IPR is not set, if it is then we wait for it to clear.
    if (udccs0 & UDCCS0_IPR) {

        for (ok = 0; (*(UDCCS0) & UDCCS0_IPR) && ok < 2000; ok++) {
            udelay (25);	// XXX 
        }
        if (ok == 2000) {
            dbg_ep0 (0, "IPR clear timed out");
        }
        dbg_ep0 (0, "IPR was set count: %d", ok);
    }
    // get up to first 8 bytes of data
    transfer_cnt = MIN (8, buffer_cnt);
    buffer_cnt -= transfer_cnt;
    fifo_count = 0;

    do {
        int count = 0;
        int udcwc;
        do {
            *(UDCD0) = (unsigned long) buffer[fifo_count];
            count++;
            writes++;
        } while (((udcwc = udc (UDCWC)) == fifo_count) && (count < 2000));

        if (udcwc <= fifo_count) {
            dbg_ep0 (0,
                    "sending STALL, failed writing FIFO transfer: %d fifo_count: %x CS0: %02x CWC: %02x",
                    transfer_cnt, fifo_count, *(UDCCS0), udcwc);
            udc_stall_ep (0);
            ep0_state = EP0_STATE_IDLE;
            return;
        }

        fifo_count++;

    } while (fifo_count < transfer_cnt);

    dbg_ep0 (3, "elapsed: %lx writes: %d fifo_count: %d transfer_cnt: %d buffer_cnt: %d CS0: %02x CWC: %02x",
            jiffies - start, writes, fifo_count, transfer_cnt, buffer_cnt, *(UDCCS0), *(UDCWC));

    buffer += transfer_cnt;

    // set data end, and start 
    if (buffer_cnt == 0) {
        ep0_state = EP0_STATE_END_IN;
    }

    SET_AND_TEST ( *(UDCCS0) = (UDCCS0_IPR | ((((!buffer_cnt) && (send_zlp != 1)) ? UDCCS0_DE : 0))),
                !(*(UDCCS0) & UDCCS0_IPR), ok);

    if (!ok) {
        dbg_ep0 (0, "failed to set IPR CS0: %02x ok: %d", *(UDCCS0), ok);
    }
}

/*
 * ep0_end_in
 *
 * Handle end of IN, possibly with ZLP.
 */
static  __inline__ void ep0_end_in (unsigned int udccs0)
{
    int ok;

    dbg_ep0 (2, " status: %02x CS0: %02x", udccs0, *(UDCCS0));

    // reset SST if necessary
    if (udccs0 & UDCCS0_SST) {
	SET_AND_TEST (*(UDCCS0) = UDCCS0_SST, *(UDCCS0) & UDCCS0_SST, ok);
        if (!ok) {
            dbg_ep0 (0, "failed to reset SST");
        }
    }
    // reset SE if necessary
    if (udccs0 & UDCCS0_SE) {
	SET_AND_TEST (*(UDCCS0) = UDCCS0_SSE, *(UDCCS0) & UDCCS0_SSE, ok);
        if (!ok) {
            dbg_ep0 (0, "failed to reset SE");
        }
    }
    // ensure that IPR is not set and send zero length packet if necessary
    if (!(udc (UDCCS0) & UDCCS0_IPR) && (send_zlp == 1)) {
	SET_AND_TEST (*(UDCCS0) = (UDCCS0_DE | UDCCS0_IPR),
                (*(UDCCS0) & (UDCCS0_DE | UDCCS0_IPR)) != (UDCCS0_DE | UDCCS0_IPR), ok);
        if (!ok) {
            dbg_ep0 (0, "failed to set DE and IPR CS0: %02x", *(UDCCS0));
        }
        else {
            dbg_ep0 (1, "set DE and IPR, ZLP sent CS0: %02x", *(UDCCS0));
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
void ep0_int_hndlr (unsigned int status)
{
    unsigned int udccs0;
    unsigned int udccr;

    udccs0 = udc (UDCCS0);
    udccr = udc (UDCCR);

    ep0_interrupts++;

    dbg_ep0 (4, "------------>[%d:%s:%s CCR: %02x CS0: %02x CWC: %02x CAR: %02x]",
            ep0_interrupts, ep0_state_desc[ep0_state], send_zlp_description[send_zlp], udccr,
            udccs0, *(UDCWC), *(UDCAR));

    // any unusual conditions?
    if (udccs0 & (UDCCS0_IPR | UDCCS0_SST | UDCCS0_SE)) {

        if (udccs0 == UDCCS0_IPR) {
            dbg_ep0 (3, "IPR SET [%d:%02x]", ep0_state, udccs0);
            return;
        }

        if (udccs0 & UDCCS0_SST) {
	    int ok;
            dbg_ep0 (3, "previously sent stall [%d:%02x]", ep0_state, udccs0);
	    if (udccs0 & UDCCS0_SST) {
                SET_AND_TEST (*(UDCCS0) = UDCCS0_SST, *(UDCCS0) & UDCCS0_SST, ok);
                if (!ok) {
                    dbg_ep0 (0, "failed to reset SST CS0: %02x ok: %d", *(UDCCS0), ok);
                }
            }
        }

        if (udccs0 & UDCCS0_SE) {
            int ok;
            dbg_ep0 (3, "unloading, found UDCCS0_SE: CS0: %02x", udccs0);

            // reset SE
	    SET_AND_TEST (*(UDCCS0) = UDCCS0_SSE, *(UDCCS0) & UDCCS0_SE, ok);
            if (!ok) {
                dbg_ep0 (0, "failed to reset SE CS0: %02x ok: %d", *(UDCCS0), ok);
            }
            // clear SE before unloading any setup packets 
            if (udccs0 & UDCCS0_OPR) {
                // reset OPR
                *(UDCCS0) = UDCCS0_SO;
            }
            ep0_state = EP0_STATE_IDLE;
            if (send_zlp) {
                dbg_ep0 (0, "not sending ZLP CS0: %02x ok: %d", *(UDCCS0));
            }
            send_zlp = 0;
        }
    }

    if ((ep0_state != EP0_STATE_IDLE) && (udccs0 & UDCCS0_OPR)) {
        dbg_ep0 (3, "Forcing EP0_STATE_IDLE CS0: %02x", udccs0);
        ep0_state = EP0_STATE_IDLE;
    }

    switch (ep0_state) {
    case EP0_STATE_IDLE:
        dbg_ep0 (4, "state: EP0_STATE_IDLE CS0: %02x", udccs0);
        ep0_read_request (udccs0, udccr);
        if (ep0_state != EP0_STATE_IN_DATA_PHASE) {
            break;
        }
        // fall through to data phase
        ep0_set_opr ();

    case EP0_STATE_IN_DATA_PHASE:
        dbg_ep0 (4, "state: EP0_STATE_IN_DATA_PHASE %x", udccs0);
        ep0_in (udccs0);
        break;

    case EP0_STATE_END_IN:
        dbg_ep0 (4, "state: EP0_STATE_END_IN %x", udccs0);
        ep0_end_in (udccs0);
        break;
    }
    dbg_ep0 (4, "<------------[%s:%s CCR: %02x CS0: %02x CWC: %02x]",
            ep0_state_desc[ep0_state], send_zlp_description[send_zlp], *(UDCCR), *(UDCCS0),
            *(UDCWC));
}

