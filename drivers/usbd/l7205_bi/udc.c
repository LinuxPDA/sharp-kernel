/*
 * linux/drivers/usbd/l7205_bi/udc.c -- L7205 USB controller driver. 
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

#include "../usbd-export.h"
#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR("sl@lineo.com, tbr@lineo.com");
MODULE_DESCRIPTION("USB Device L7205/L7210 Bus Interface");

USBD_MODULE_INFO("l7205_bi 0.1-alpha");

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>

#include <linux/netdevice.h>

#include <asm/irq.h>
#include <asm/system.h>

#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>


#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "../usbd-bi.h"

//#include "xxxx.h"
#include "udc.h"
#include "l7205.h"
#include "hardware.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


static struct usb_device_instance *udc_device;          // required for the interrupt handler

/*
 * ep_endpoints - map physical endpoints to logical endpoints
 */
static struct usb_endpoint_instance *ep_endpoints[UDC_MAX_ENDPOINTS];

static struct urb ep0_urb;
static unsigned char usb_address;

unsigned int udc_interrupts;

/* ********************************************************************************************* */

/**
 * xxxxsend_data - send packet via endpoint
 * @ep: logical endpoint number
 * @bp: pointer to data
 * @size: bytes to write
 */
static void __inline__ xxxxsend_data(unsigned char ep, unsigned char *bp, unsigned char size)
{

    if (bp && size) {
        // copy data from buffer to chip
    }

    // arm
}

#if 0
/**
 * udc_start - start transmit
 * @ep:
 */
static void __inline__ udc_start(unsigned int ep, struct usb_endpoint_instance *endpoint, int restart)
{
    if (endpoint->tx_urb) {
        struct urb *urb = endpoint->tx_urb;

        if ((urb->actual_length - endpoint->sent) > 0) {
            endpoint->last = MIN(urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
            xxxxsend_data(ep, urb->buffer + endpoint->sent, endpoint->last);
        }
        else {
            // XXX ZLP
            endpoint->last = 0;
            xxxxsend_data(ep, urb->buffer + endpoint->sent, 0);
        }
    }
}
#endif


#if 0
/**
 * udc_int_hndlr - interrupt handler
 *
 */
static void udc_int_hndlr(int irq, void *dev_id, struct pt_regs *regs)
{
    udc_interrupts++;

}

/**
 * udc_probe - initialize
 *
 **/
static int udc_probe(void)
{
    // reset

    return 0;
}
#endif



/* ********************************************************************************************* */
/*
 * Start of public functions.
 */


/**
 * udc_start_in_irq - start transmit
 * @eendpoint: endpoint instance
 *
 * Called by bus interface driver to see if we need to start a data transmission.
 */
void udc_start_in_irq(struct usb_endpoint_instance *endpoint)
{
    ep2_start_tx(2, endpoint, 0);
}


/**
 * udc_stall_ep - stall endpoint
 * @ep: physical endpoint
 *
 * Stall the endpoint.
 */
void udc_stall_ep(unsigned int ep)
{
    unsigned char status;

    if (ep < UDC_MAX_ENDPOINTS) {

        // stall
    }
}


/**
 * udc_reset_ep - reset endpoint
 * @ep: physical endpoint
 * reset the endpoint.
 *
 * returns : 0 if ok, -1 otherwise
 */
void udc_reset_ep(unsigned int ep)
{
    if (ep < UDC_MAX_ENDPOINTS) {
        // reset

        switch(ep) {
        case 0:
            break;
        case 1:
            ep1_reset();
            break;
        case 0x82:
            ep2_reset();
            break;
        }
    }
}


/**
 * udc_endpoint_halted - is endpoint halted
 * @ep:
 *
 * Return non-zero if endpoint is halted
 */
int udc_endpoint_halted(unsigned int ep)
{
    return 0;
}


/**
 * udc_set_address - set the USB address for this device
 * @address:
 *
 * Called from control endpoint function after it decodes a set address setup packet.
 */
void udc_set_address(unsigned char address)
{
    // address cannot be setup until ack received
    usb_address = address;
}

/**
 * udc_serial_init - set a serial number if available
 */
static int __init udc_serial_init(struct usb_bus_instance *bus)
{
    return -EINVAL;
}

/* ********************************************************************************************* */

/**
 * udc_max_endpoints - max physical endpoints 
 *
 * Return number of physical endpoints.
 */
int udc_max_endpoints(void)
{
    return UDC_MAX_ENDPOINTS;
}


/**
 * udc_check_ep - check logical endpoint 
 * @lep:
 *
 * Return physical endpoint number to use for this logical endpoint or zero if not valid.
 */
int udc_check_ep(int logical_endpoint, int packetsize)
{
    if (packetsize > 32) {
    return 0;
    }

    switch (logical_endpoint) {
    case 1:
        return 1;
    case 0x82:
        return 2;
    case 0x83:
        return 3;
    default:
        return 0;
    }
}


/**
 * udc_set_ep - setup endpoint 
 * @ep:
 * @endpoint:
 *
 * Associate a physical endpoint with endpoint_instance
 */
void udc_setup_ep(struct usb_device_instance * device, unsigned int ep, struct usb_endpoint_instance *endpoint)
{
    printk(KERN_DEBUG"udc_setup_ep: ep: %d\n", ep);
    if (ep < UDC_MAX_ENDPOINTS) {
        ep_endpoints[ep] = endpoint;
        switch(ep) {
        case 0:         // Control
            break;
        case 1:         // OUT
            // XXX XXX
            /*
            {
                unsigned long flags;
                save_flags_cli(flags);
                */
                usbd_fill_rcv(device, endpoint, 5);
                endpoint->rcv_urb = first_urb_detached(&endpoint->rdy);
                /*
                endpoint->rcv_urb = first_urb_detached_irq(&endpoint->rdy);
                restore_flags(flags);
            }
            */
            ep1_enable(device, endpoint);
            break;
        case 2:      // IN
            ep2_enable(device, endpoint);
            break;
        }
    }
}

/**
 * udc_disable_ep - disable endpoint
 * @ep:
 *
 * Disable specified endpoint 
 */
void udc_disable_ep(unsigned int ep)
{
    if (ep < UDC_MAX_ENDPOINTS) {
        struct usb_endpoint_instance *endpoint;

        if ((endpoint = ep_endpoints[ep])) {
            ep_endpoints[ep] = NULL;
            usbd_flush_ep(endpoint);
        }
    }
}


/* ********************************************************************************************* */

/**
 * udc_incradle - is the USB cable connected
 *
 * Return non-zeron if cable is connected.
 */
int udc_connected() {
    return 1;
}


/**
 * udc_connect - enable pullup resistor
 *
 * Turn on the USB connection by enabling the pullup resistor.
 */
void udc_connect(void)
{
}


/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect(void)
{
}

/* ********************************************************************************************* */

/**
 * udc_enable_interrupts - enable interrupts
 *
 * Switch on UDC interrupts.
 *
 */
void udc_all_interrupts(struct usb_device_instance *device)
{
    printk(KERN_DEBUG"udc_enable_interrupts:\n");
    // set interrupt mask
}


/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts(struct usb_device_instance *device)
{
    printk(KERN_DEBUG"udc_enable_interrupts:\n");
    // set interrupt mask
}


/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts(struct usb_device_instance *device)
{
    printk(KERN_DEBUG"udc_disable_interrupts:\n");
    // reset interrupt mask
}

/* ********************************************************************************************* */

/**
 * udc_ep0_packetsize - return ep0 packetsize
 */
int udc_ep0_packetsize(void)
{
    return EP0_PACKETSIZE;
}

#if 0
/**
 * udc_enable - enable the UDC
 *
 * Switch on the UDC
 */
void udc_enable(struct usb_device_instance *device)
{

    printk(KERN_DEBUG"\n");
    printk(KERN_DEBUG"udc_enable: device: %p\n", device);

    // save the device structure pointer
    udc_device = device;

    // ep0 urb
    ep0_urb.device = device;
    usbd_alloc_urb_data(&ep0_urb, 512);

    // enable UDC

}


/**
 * udc_disable - disable the UDC
 *
 * Switch off the UDC
 */
void udc_disable(void)
{
    printk(KERN_DEBUG "************************* udc_disable:\n");

    // disable UDC

    // reset device pointer
    udc_device = NULL;

    // ep0 urb
    kfree(ep0_urb.buffer);
}
#endif

/**
 * udc_startup_events - allow udc code to do any additional startup
 */
void udc_startup_events(struct usb_device_instance *device)
{
    usbd_device_event(device, DEVICE_INIT, 0);
    usbd_device_event(device, DEVICE_CREATE, 0);
    usbd_device_event(device, DEVICE_HUB_CONFIGURED, 0);
    //usbd_device_event(device, DEVICE_RESET, 0);            // XXX should be done from device event
    usbd_device_event(device, DEVICE_ADDRESS_ASSIGNED, 0);
    usbd_device_event(device, DEVICE_CONFIGURED, 0);
    usbd_device_event(device, DEVICE_SET_INTERFACE, 0);
}


/* ********************************************************************************************* */

#if 0
/**
 * udc_init - initialize USB Device Controller
 * 
 * Get ready to use the USB Device Controller.
 *
 * Register an interrupt handler and IO region. Return non-zero for error.
 */
int udc_init(void)
{  
    printk(KERN_DEBUG"udc_init:\n");

    // probe for UDC
    if (udc_probe()) {
        return -EINVAL;
    }
    return 0;
}
#endif

#if 0
/**
 * udc_regs - dump registers
 *
 * Dump registers with printk
 */
void udc_regs(void)
{
    printk("\n");
}
#endif

/**
 * udc_name - return name of USB Device Controller
 */
char * udc_name(void)
{
    return UDC_NAME;
}

#if 0
/**
 * udc_request_udc_irq - request UDC interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_udc_irq()
{
    // request IRQ 
    if (request_irq(IRQ_USBF, udc_int_hndlr, SA_INTERRUPT, "L7205 USBD Bus Interface", NULL) != 0) {
        printk(KERN_DEBUG"usb_ctl: Couldn't request USB irq\n");
        return -EINVAL;
    }
    return 0;
}
#endif

/**
 * udc_request_cable_irq - request Cable interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_cable_irq()
{
    return 0;
}

/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io()
{
}

/**
 * udc_release_udc_irq - release UDC irq
 */
void udc_release_udc_irq()
{
    free_irq(IRQ_USBF, NULL); 
}

/**
 * udc_release_cable_irq - release Cable irq
 */
void udc_release_cable_irq()
{
}

/**
 * udc_release_release_io - release UDC io region
 */
void udc_release_io()
{
}

