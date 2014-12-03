/*
 * linux/drivers/usbd/l7205_bi/ep0.c -- L7205 USB controller driver. 
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


/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/pkt_sched.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/cache.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>

#include <asm/system.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/atomic.h>
#include <asm/types.h>

#include <asm/uaccess.h>
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


/* 
 * Prototypes
 */
//static void inline ep0_in_data(unsigned int);
////static void inline ep0_end_xfer(unsigned int ep0_status);
////static int ep0_read_data(struct usb_device_request *request, int max);
////static void ep0_idle(unsigned int);
////static void ep0_serviced_opr_data_end(int );

//void ep0_serviced_opr_data_end(int flag);

/* 
 * EP0 State Information 
 * This is used to determine how to respond to host requests,
 * e.g. in the case where a data packet is being sent over
 * multiple USB packets.
 */
#define EP0_IDLE 0
#define EP0_IN_DATA_PHASE 1
#define EP0_OUT_DATA_PHASE 2
#define EP0_END_XFER 3


//static int ep0_state = EP0_IDLE;


int ep0_need_data;


static unsigned char *buffer;
static unsigned int buffer_cnt;


int ep0_init(struct usb_device_instance * device)
{

    printk(KERN_DEBUG"ep0_init:\n");

    return 0;

}


/* unfortunatly there are three scenarios of IN packets,
 * which require different behaviour from the driver.
 * 1. setup requests > 8 bytes, and exact packet size
 *    -> normal case, set DE in END_XFER interrupt
 * 2. setup request <= 8 bytes.
 *    -> this requires setting DE with IPR.
 * 3. setup requests > size of response
 *    -> this requires setting IPR with DE in end_xfer interrupt
 *       so that a 0 length packet is returned instead of a stall.
 */
#define END_XFER_EXACT     0
#define END_XFER_IMMEDIATE 1
#define END_XFER_FORCED    2
static unsigned int end_xfer;

int ep0_enable(struct usb_device_instance *device)
{
    return 0;
}

void ep0_disable(void)
{

}


int ep0_reset(void)
{
#if 0
    dbgENTER(dbg_ep0,1);
    ep0_state = EP0_IDLE;
    ep0_need_data = 0;
    ep0_reset_ep(0);
    dbgLEAVE(dbg_ep0,1);
#endif
    return 0;
}

/* 
 * Interrupt Handler for enpoint 0.
 * 
 * This routine is called iff the EIR bit in the UDC 
 * status/interrupt register is set. It implements the 
 * flowchart detailed in the "USB Client Device Validation 
 * for the StrongARM SA-1100 Microprocessor" document.
 *
 */
void ep0_int_hndlr(unsigned int status)
{   
    //unsigned int ep0_status;

#if 0
    ep0_interrupts++;
    dbgep0(3,"-------------[%lx:%d:%d:%02x:%02x]", jiffies, ep0_state, ep0_need_data, *(UDCCR), ep0_status);
#endif
}

