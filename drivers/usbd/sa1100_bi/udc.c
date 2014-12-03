/*
 * linux/drivers/usbd/sa1100_bi/udc.c
 *
 * Copyright (c) 2000, 2001 Lineo
 * Copyright (c) 2001 Hewlett Packard
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
MODULE_DESCRIPTION("USB Device SA-1100 Bus Interface");

USBD_MODULE_INFO("sa1100_bi 0.2");

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>

#include <linux/proc_fs.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,6)
#define USE_ADD_DEL_TIMER_FOR_USBADDR_CHECK 1
#include <linux/timer.h>
#else
#undef USE_ADD_DEL_TIMER_FOR_USBADDR_CHECK
#include <linux/tqueue.h>
#endif

#include <linux/netdevice.h>
#include <linux/version.h>
#include <linux/pci.h>
#include <linux/cache.h>


#include <asm/dma.h>
#include <asm/mach/dma.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/hardware.h>

#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>


#include "../usbd-debug.h"
#include "sa1100-dma-inline.h"

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "../usbd-bi.h"

#include "sa1100.h"
#include "udc.h"

#if defined(CONFIG_SA1100_COLLIE)       // XXX change to 5500
#include <asm-arm/arch-sa1100/collie.h>
#include <asm/ucb1200.h>
#include <asm/arch/tc35143.h>
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


static struct usb_device_instance *udc_device;          // required for the interrupt handler

/*
 * ep_endpoints - map physical endpoints to logical endpoints
 */
static struct usb_endpoint_instance *ep_endpoints[UDC_MAX_ENDPOINTS];

//static struct urb ep0_urb;
unsigned char usb_address;

unsigned int udc_interrupts;
unsigned int ep0_interrupts;
unsigned int tx_interrupts;
unsigned int rx_interrupts;
unsigned int sus_interrupts;
unsigned int res_interrupts;
unsigned int udc_address_errors;
unsigned int udc_ticks;
unsigned int udc_fixed;

int usbd_rcv_dma;
int usbd_tx_dma;

static int udc_saw_sof;

#ifdef USE_ADD_DEL_TIMER_FOR_USBADDR_CHECK
static struct timer_list sa1100_usb_dev_addr_check;
static int usb_addr_check_initialized = 0;
#define CHECK_INTERVAL   1
#else
static struct tq_struct sa1100_tq;
#endif



/* ********************************************************************************************* */


static u32 getCPUID(char **stepping)
{
    u32 cpuID;

    __asm__ __volatile__(     " mrc   p15, 0, %0, c0, c0"
            : "=r" (cpuID)
            :);

    if (NULL != stepping) {
        switch( cpuID & 0xf) {
        case 0:  *stepping = "A0"; break;
        case 4:  *stepping = "B0"; break;
        case 5:  *stepping = "B1"; break;
        case 6:  *stepping = "B2"; break;
        case 8:  *stepping = "B4"; break;
        case 9:  *stepping = "B5"; break;
        default: *stepping = "??";
            dbg_udc(0, "stepping unknown, ID#%x",(cpuID&0xf));
        }
    }

    return(cpuID);
}



void udc_fix_errata_29(char *msg)
{
    int ok;
    u32 cpuID;  // from init.c

    cpuID = getCPUID(NULL);

    // Set errata 29 fix bit, if possible
    if (cpuID == 0 || (cpuID & 0xF) >= 9) {
        // Unknown CPU, or B5 stepping and above
        // set errata 29 fix enable (for B5 and above, B4 will ignore)
        int udc_cr;
        for (ok = 10; ok > 0; ok--) {
            *(UDCCR) |= /*UDCCR_ERR29*/ 0x80;
            // Do some dummy reads....
            udc_cr = *(UDCCR);
            udc_cr = *(UDCCR);
            udc_cr = *(UDCCR);
            if (udc(UDCCR) & /*UDCCR_ERR29*/ 0x80) {
                dbg_udc(0,"%s: set errata 29 fix bit worked, UDCCR#%02x ok=%d cpuID#%08x",msg,udc(UDCCR),ok,cpuID);
                ok = -2;
                break;
            }
        }
        if (ok != -2) {
            dbg_udc(0,"%s: set errata 29 fix bit failed, UDCCR#%02x ok=%d cpuID#%08x",msg,udc(UDCCR),ok,cpuID);
        }
    } else {
        dbg_udc(0,"%s: errata 29 fix bit not available, cpuID#%08x",msg,cpuID);
    }
}

/* ********************************************************************************************* */

/**
 * sa1100_tick - clock timer task 
 * @data:
 *  
 * Run from global clock tick to check if we are suspended.
 */ 
#ifdef USE_ADD_DEL_TIMER_FOR_USBADDR_CHECK
static void sa1100_tick(unsigned long data)
#else
static void sa1100_tick(void *data)
#endif
{       
    udc_ticks++;

    // is driver active
    if (data) {

        if ( _udc(UDCAR) != usb_address) {
            printk(KERN_DEBUG"sa1100_tick: ADDRESS ERROR DETECTED\n");
            udc_address_errors++;
            udc_fixed++;
        }
        *(UDCAR) = usb_address;

        // re-queue task
#ifdef USE_ADD_DEL_TIMER_FOR_USBADDR_CHECK
        sa1100_usb_dev_addr_check.expires = jiffies + CHECK_INTERVAL;
        add_timer(&sa1100_usb_dev_addr_check);
#else
        queue_task(&sa1100_tq, &tq_timer);
#endif

    }
}


/* ********************************************************************************************* */
/* IO
 */

// See sa1100.h

volatile int udc(volatile unsigned int *regaddr)
{
    volatile unsigned int value;
    int ok;
    for(ok = 1000, value = *(regaddr); value != *(regaddr) && ok--; value = *(regaddr));
    if (!ok) {
        dbg_udc(0,"NOT OK: %p %x", regaddr, value);
    }
    return value;
}


/* ********************************************************************************************* */
/* Control (endpoint zero)
 */

// See ep0.c


/* ********************************************************************************************* */
/* Bulk OUT (recv)
 */

// See recv.c



/* ********************************************************************************************* */
/* Bulk IN (tx)
 */

// See send.c



/* ********************************************************************************************* */
/* Interrupt Handler
 */

/**
 * int_hndlr_cable - interrupt handler for cable
 */
static void int_hndlr_cable(int irq, void *dev_id, struct pt_regs *regs)
{
#ifdef CONFIG_SA1100_USBCABLE_GPIO
    dbg_udc(1, "udc_cradle_interrupt:");
    udc_cable_event();
#endif
}


/**
 * int_hndlr_device - interrupt handler
 *
 */
static void int_hndlr_device(int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned int status;

    status = *(UDCSR);
    *(UDCSR) = status;

    udc_interrupts++;

    //dbg_udc(0, "");
    //dbg_udc(0, "int_hndlr_device[%d]: CSR: %02x CCR: %02x", udc_interrupts, status, *(UDCCR));

    // Handle common interrupts first, IN (tx) and OUT (recv)

    if (status & UDCSR_RIR) {
        ep1_int_hndlr(status);
    }

    if (status & UDCSR_TIR) {
        ep2_int_hndlr(status, 1);
    }

    // handle less common interrupts
    if (status & (UDCSR_EIR | UDCSR_RSTIR | UDCSR_SUSIR | UDCSR_RESIR)) {
        if (status & UDCSR_EIR) {
            ep0_int_hndlr(status);
        }

        if (status & UDCSR_RSTIR) {
            dbg_intr(1,"DEVICE_RESET: CSR: %02x CS0: %02x CAR: %02x", *(UDCSR), *(UDCCS0), *(UDCAR));
            usbd_device_event(udc_device, DEVICE_RESET, 0);
        }

        if (status & UDCSR_SUSIR) {
            dbg_intr(1,"SUSPEND address: %02x irq: %02x status: %02x", *(UDCAR), irq, status);
            sus_interrupts++;
            usbd_device_event(udc_device, DEVICE_BUS_INACTIVE, 0);
            udc_suspended_interrupts(udc_device);
            udc_ticker_poke();
        }

        if (status & UDCSR_RESIR) {
            dbg_intr(1,"RESUME address: %02x irq: %02x status: %02x", *(UDCAR), irq, status);
            res_interrupts++;
            usbd_device_event(udc_device, DEVICE_BUS_ACTIVITY, 0);
            udc_all_interrupts(udc_device);
            udc_ticker_poke();
        }
    }

    // Check that the UDC has not forgotton it's address, force it back to correct value
    if ( _udc(UDCAR) != usb_address) {
        udc_address_errors++;
    }
    *(UDCAR) = usb_address;

}


/* ********************************************************************************************* */


/* ********************************************************************************************* */
/*
 * Start of public functions.
 */

/**
 * udc_init - initialize
 *
 * Return non-zero if we cannot see device.
 **/
int udc_init(void)
{
    // reset

    return 0;
}

/**
 * udc_start_in_irq - start transmit
 * @endpoint:
 *
 * Called with interrupts disabled.
 */
void udc_start_in_irq(struct usb_endpoint_instance *endpoint)
{
    ep2_int_hndlr(0,0);
}


void udc_stall_ep0(void)
{
    int ok;
    // QQQ eh?
    dbg_udc(0,"stalling ep0 (UDCCS0_FST,UDCCS0_FST,UDCCS0_SST)");

    // write 1 to set FST
    SET_AND_TEST( (*(UDCCS0) &= UDCCS0_FST), !(udc(UDCCS0) & UDCCS0_FST), ok);
    if (!ok) {
        dbg_udc(0,"cannot stall !(UDCCS0&UDCCS0_FST) UDCCS0: %02x", *(UDCCS0));
    }

    // write 0 to reset FST
    SET_AND_TEST( (*(UDCCS0) &= ~UDCCS0_FST), (udc(UDCCS0) & UDCCS0_FST), ok);
    if (!ok) {
        dbg_udc(0,"cannot stall (UDCCS0&UDCCS0_FST) UDCCS0: %02x", *(UDCCS0));
    }

    // write 1 to reset SST
    SET_AND_TEST( (*(UDCCS0) = UDCCS0_SST), (udc(UDCCS0) & UDCCS0_SST), ok);
    if (!ok) {
        dbg_udc(0,"cannot stall (UDCCS0&UDCCS0_SST) UDCCS0: %02x", *(UDCCS0));
    }
}

static void stall_ep_n(int ep, volatile unsigned int *regaddr, int fst)
{
    int ok;
    dbg_udc(0,"stalling ep %d (FST)",ep);

    // write 1 to set FST
    SET_AND_TEST( (*(regaddr) = fst), !(udc(regaddr) & fst), ok);
    if (!ok) {
        dbg_udc(0,"cannot stall !(reg&fst) UDCCS%d: %02x", ep, *(regaddr));
    }
}

static void reset_ep_n(int ep, volatile unsigned int *regaddr, int fst, int sst)
{
    int ok;
    dbg_udc(1,"reset ep %d (FST)",ep);

    // write 0 to reset FST
    SET_AND_TEST( (*(regaddr) &= ~fst), (udc(regaddr) & fst), ok);
    if (!ok) {
        dbg_udc(0,"cannot stall !(reg&fst) UDCCS%d: %02x", ep, *(regaddr));
    }
    // write 1 to reset SST
    SET_AND_TEST( (*(regaddr) = sst), (udc(regaddr) & sst), ok);
    if (!ok) {
        dbg_udc(0,"cannot stall (reg&sst) UDCCS%d: %02x", ep, *(UDCCS0));
    }
}

/**
 * udc_stall_ep - stall endpoint
 * @ep: physical endpoint
 *
 * Stall the endpoint.
 */
void udc_stall_ep(unsigned int ep)
{
    dbg_udc(0,"STALLING %d (FST)",ep);
    switch (ep) {
    case 1:
        stall_ep_n(1, UDCCS1, UDCCS1_FST);
        break;
    case 2:
        stall_ep_n(2, UDCCS2, UDCCS2_FST);
        break;
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
    dbg_udc(1,"RESETING %d (FST)",ep);
    switch (ep) {
    case 0:
        reset_ep_n(1, UDCCS0, UDCCS0_FST, UDCCS0_SST);
        break;
    case 1:
        reset_ep_n(1, UDCCS1, UDCCS1_FST, UDCCS1_SST);
        break;
    case 2:
        reset_ep_n(2, UDCCS2, UDCCS2_FST, UDCCS2_SST);
        break;
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
    switch (ep) {
    case 0:
        return (udc(UDCCS0) & UDCCS0_FST)!=0;
    case 1:
        return (udc(UDCCS1) & UDCCS1_FST)!=0;
    case 2:
        return (udc(UDCCS2) & UDCCS2_FST)!=0;
    }
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
    // address can be setup, udc will wait until ack received
    //dbg_udc(0, "udc_set_address: %d", address);
    usb_address = address;
    *(UDCAR) = address;
}

#ifdef CONFIG_SA1100_BITSY
static int crc32( char * s, int length ) {
    /* indices */
    int pByte;
    int pBit;

    const unsigned long poly = 0xedb88320;
    unsigned long crc_value = 0xffffffff;

    for ( pByte = 0; pByte < length; pByte ++ ) {
        unsigned char   c = *(s++);
        for ( pBit = 0; pBit < 8; pBit++ ) {
            crc_value = (crc_value>>1)^ (((crc_value^c)&0x01)?poly:0);
            c >>= 1;
        }
    }
    return  crc_value;
}       
#endif

/**
 * udc_serial_init - set a serial number if available
 */
int __init udc_serial_init(struct usb_bus_instance *bus)
{
    char *stepping;
    u32 id = getCPUID(&stepping);

    dbg_init(2,"serial number");

#ifdef CONFIG_SA1100_BITSY
    if (machine_is_bitsy()) {
        char serial_number[22];
        int i;
        int j;
        memset(&serial_number, 0, sizeof(serial_number));
        for (i=0,j=0; i<20; i++,j++) {
            char buf[4];        
            h3600_eeprom_read(5 + i, buf, 2);
            serial_number[j] = buf[1];
        }
        serial_number[j] = '\0';
        bus->serial_number = crc32(serial_number, 22);
        if (bus->serial_number_str = kmalloc(strlen(serial_number) + 1, GFP_KERNEL)) {
            strcpy(bus->serial_number_str, serial_number);
        }
        //dbg_udc(0,  "serial: %s %08x", bus->serial_number_str, bus->serial_number);
        return 0;
    }
#endif

#ifdef CONFIG_SA1100_CALYPSO
    if (machine_is_calypso()) {
        __u32 eerom_serial;
        int i;
        if ((i = CalypsoIicGet( IIC_ADDRESS_SERIAL0, (unsigned char*)&eerom_serial, sizeof(eerom_serial)))) {
            bus->serial_number = eerom_serial;  
        }
        if (bus->serial_number_str = kmalloc(9, GFP_KERNEL)) {
            sprintf(bus->serial_number_str, "%08X", eerom_serial & 0xffffffff);
        }
        //dbg_udc(0,  "serial: %s %08x", bus->serial_number_str, bus->serial_number);
        return 0;
    }
#endif
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
 * @logical_endpoint:
 *
 * Return physical endpoint number to use for this logical endpoint or zero if not valid.
 */
int udc_check_ep(int logical_endpoint, int packetsize)
{
#ifndef CONFIG_SA1100_NO_DMA
    if (packetsize > 64) {
        return 0;
    }
#else
    switch (logical_endpoint) {
    case 1:
        if (packetsize > 20) {
            return 0;
        }
    case 0x82:
        if (packetsize > 16) {
            return 0;
        }
    default:
        return 0;
    }
#endif
    switch (logical_endpoint) {
    case 1:
        return 1;
    case 0x82:
        return 2;
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
    dbg_udc(1, "udc_setup_ep[%d]:", ep);

    if (ep < UDC_MAX_ENDPOINTS) {

        ep_endpoints[ep] = endpoint;

        if (endpoint) {
            switch(ep) {
            case 0:         // Control
                ep0_enable(device, endpoint);
                break;

            case 1:         // OUT
                // XXX XXX 
                usbd_fill_rcv(device, endpoint, 5);
                endpoint->rcv_urb = first_urb_detached(&endpoint->rdy);
                ep1_enable(device, endpoint, usbd_rcv_dma);
                break;

            case 2:      // IN
                ep2_enable(device, endpoint, usbd_tx_dma);
                break;
            }
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
        switch (ep) {
        case 0:
            ep0_disable();
            break;
        case 1:
            ep1_disable();
            break;
        case 2:
            ep2_disable();
            break;
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
    int rc = 1;
#ifdef CONFIG_SA1100_USBCABLE_GPIO

#ifdef CONFIG_SA1100_USBCABLE_ACTIVE_HIGH 
    rc = (GPLR & GPIO_GPIO(23)) != 0;
    dbg_udc(1, "udc_connected: ACTIVE_HIGH: %d", rc);
#else
    rc = (GPLR & GPIO_GPIO(23)) == 0;
    dbg_udc(1, "udc_connected: ACTIVE_LOW: %d", rc);
#endif
#else
#warning UDC_CONNECTED not implemented
#endif
    return rc;
}


/**
 * udc_connect - enable pullup resistor
 *
 * Turn on the USB connection by enabling the pullup resistor.
 */
void udc_connect(void)
{
#if defined(CONFIG_SA1100_COLLIE)       // XXX change to 5500

    #warning COLLIE CONNECT

    if (ucb1200_test_io(TC35143_GPIO_VERSION0) != 0 ||
	ucb1200_test_io(TC35143_GPIO_VERSION1) != 0) {
        SCP_REG_GPCR |= SCP_GPCR_PA19;    /* direction : out mode */
        SCP_REG_GPWR |= SCP_GPCR_PA19;    /* set Hi */
        dbg_udc(1, "udc_connect: %d GPCR: %x GPWR: %x GPRR: %x", 
                SCP_GPCR_PA19, SCP_REG_GPCR, SCP_REG_GPWR, SCP_REG_GPRR);
    }

#elif defined(CONFIG_SA1100_CONNECT_GPIO)

    GAFR |= GPIO_GPIO(CONFIG_SA1100_CONNECT_GPIO);
    GPDR |= GPIO_GPIO(CONFIG_SA1100_CONNECT_GPIO);
    
#ifdef CONFIG_SA1100_CONNECT_ACTIVE_HIGH
    GPSR = GPIO_GPIO(CONFIG_SA1100_CONNECT_GPIO);
    dbg_udc(1, "udc_connect: %d ACTIVE_HIGH %x %x %x", CONFIG_SA1100_CONNECT_GPIO, 
            GPIO_GPIO(CONFIG_SA1100_CONNECT_GPIO), GPDR, GPLR);
#else
    GPCR = GPIO_GPIO(CONFIG_SA1100_CONNECT_GPIO);
    dbg_udc(1, "udc_connect: %d ACTIVE_LOW", CONFIG_SA1100_CONNECT_GPIO);
#endif

#else /* defined(CONFIG_SA1100_CONNECT_GPIO) */
#warning UDC_CONNECT not implemented
#endif
}


/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect(void)
{

#if defined(CONFIG_SA1100_COLLIE)       // XXX change to 5500

    if (ucb1200_test_io(TC35143_GPIO_VERSION0) != 0 ||
	ucb1200_test_io(TC35143_GPIO_VERSION1) != 0) {
        SCP_REG_GPWR &= ~SCP_GPCR_PA19;    /* set Hi */
        SCP_REG_GPCR &= ~SCP_GPCR_PA19;    /* direction : out mode */

        dbg_udc(1, "udc_disconnect: %d GPCR: %x GPWR: %x GPRR: %x", 
                SCP_GPCR_PA19, SCP_REG_GPCR, SCP_REG_GPWR, SCP_REG_GPRR);
    }
 

#elif defined(CONFIG_SA1100_CONNECT_GPIO)

#ifdef CONFIG_SA1100_CONNECT_ACTIVE_HIGH
    GPCR = GPIO_GPIO(CONFIG_SA1100_CONNECT_GPIO);
    dbg_udc(1, "udc_disconnect: %d ACTIVE_HIGH", CONFIG_SA1100_CONNECT_GPIO);
#else
    GPSR = GPIO_GPIO(CONFIG_SA1100_CONNECT_GPIO);
    dbg_udc(1, "udc_disconnect: %d ACTIVE_LOW", CONFIG_SA1100_CONNECT_GPIO);
#endif

    GPDR &= ~GPIO_GPIO(CONFIG_SA1100_CONNECT_GPIO);

#endif
}


/* ********************************************************************************************* */

static __inline__ void set_interrupts(int flags)
{
    int ok;
    //dbg_udc(1, "udc_set_interrupts: %02x", flags);

    // set interrupt mask
    SET_AND_TEST( (*(UDCCR) = flags), (udc(UDCCR) != flags), ok);
    if (!ok) {
        dbg_intr(0,"failed to set UDCCR %02x to %02x", *(UDCCR), flags);
    }
}

/**
 * udc_enable_interrupts - enable interrupts
 *
 * Switch on UDC interrupts.
 *
 */
void udc_all_interrupts(struct usb_device_instance *device)
{
    set_interrupts(0);
    dbg_udc(1, "%02x", *(UDCCR));

    // setup tick
#ifdef USE_ADD_DEL_TIMER_FOR_USBADDR_CHECK
    if (!usb_addr_check_initialized) {
        init_timer(&sa1100_usb_dev_addr_check);
        usb_addr_check_initialized = 1;
        sa1100_usb_dev_addr_check.function = sa1100_tick;
        sa1100_usb_dev_addr_check.data = 1;
        sa1100_usb_dev_addr_check.expires = jiffies + CHECK_INTERVAL;
        add_timer(&sa1100_usb_dev_addr_check);
    }
#else
    // XXX sa1100_tq.sync = 0;
    sa1100_tq.routine = sa1100_tick;
    sa1100_tq.data = &udc_saw_sof;
    queue_task(&sa1100_tq, &tq_timer);
#endif

}


/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts(struct usb_device_instance *device)
{
    set_interrupts(UDCCR_SRM);
    dbg_udc(1, "%02x", *(UDCCR));
}


/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts(struct usb_device_instance *device)
{
    set_interrupts(UDCCR_SRM | UDCCR_EIM | UDCCR_RIM | UDCCR_TIM | UDCCR_SRM | UDCCR_REM);
    dbg_udc(1, "%02x", *(UDCCR));
}

/* ********************************************************************************************* */

/**
 * udc_ep0_packetsize - return ep0 packetsize
 */
int udc_ep0_packetsize(void)
{
    return EP0_PACKETSIZE;
}

/**
 * udc_enable - enable the UDC
 *
 * Switch on the UDC
 */
void udc_enable(struct usb_device_instance *device)
{
    int ok;
    dbg_udc(1,  "************************* udc_enable:");

    //dbg_udc(0, "");
    //dbg_udc(0, "udc_enable: device: %p", device);

    // save the device structure pointer
    udc_device = device;

    // ep0 urb
    //ep0_urb.device = device;
    //usbd_alloc_urb_data(&ep0_urb, 512);

    // enable UDC
    for (ok=0; (*(UDCCR) & UDCCR_UDA) && ok<100000; ok++ );
    if (ok>10000) {
        dbg_udc(0,"Waiting too long to go inactive: UDCCR: %02x", *(UDCCR));
    }

    // set UDD
    SET_AND_TEST( (*(UDCCR) |= UDCCR_UDD), (!udc(UDCCR) & UDCCR_UDD), ok);
    if (!ok) {
        dbg_udc(0,"Waiting too long to disable: UDCCR: %02x", *(UDCCR));
    }

    // reset UDD
    SET_AND_TEST( (*(UDCCR) &= ~UDCCR_UDD), (udc(UDCCR) & UDCCR_UDD), ok);
    if (!ok) {
        dbg_udc(0,"can't enable udc, FAILED: %02x", *(UDCCR));
    }
}


/**
 * udc_disable - disable the UDC
 *
 * Switch off the UDC
 */
void udc_disable(void)
{
    int ok;
    dbg_udc(1,  "************************* udc_disable:");

    // tell tick task to stop
#ifdef USE_ADD_DEL_TIMER_FOR_USBADDR_CHECK
    del_timer(&sa1100_usb_dev_addr_check);
    sa1100_usb_dev_addr_check.data = 0;
    usb_addr_check_initialized = 0;
#else
    sa1100_tq.data = NULL;
    while (sa1100_tq.sync) {
        //printk(KERN_DEBUG"waiting for sa1100_tq to stop\n");
        schedule_timeout(10*HZ);
    }
#endif

    // disable UDC
    
    for (ok=0; (*(UDCCR) & UDCCR_UDA) && ok<100000; ok++ );
    if (ok>10000) {
        dbg_udc(0,"Waiting too long to go inactive: UDCCR: %02x", *(UDCCR));
    }

    // set UDD
    SET_AND_TEST( (*(UDCCR) |= UDCCR_UDD), (!udc(UDCCR) & UDCCR_UDD), ok);
    if (!ok) {
        dbg_udc(0,"Waiting too long to disable: UDCCR: %02x", *(UDCCR));
    }

    dbg_udc(7,"CCR: %02x", *(UDCCR));


    // reset device pointer
    udc_device = NULL;

    // ep0 urb
    //kfree(ep0_urb.buffer);

}


/**
 * udc_startup - allow udc code to do any additional startup
 */
void udc_startup_events(struct usb_device_instance *device)
{
    usbd_device_event(device, DEVICE_INIT, 0);
    usbd_device_event(device, DEVICE_CREATE, 0);
    usbd_device_event(device, DEVICE_HUB_CONFIGURED, 0);
    usbd_device_event(device, DEVICE_RESET, 0);            // XXX should be done from device event
}


/* ********************************************************************************************* */

/**
 * udc_name - return name of USB Device Controller
 */
char * udc_name(void)
{
    return UDC_NAME;
}

/**
 * udc_request_udc_irq - request UDC interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_udc_irq()
{
    // request IRQ and two dma channels
    // XXX GPDR &= ~GPIO_GPIO(23);
    // XXX set_GPIO_IRQ_edge(GPIO_GPIO(CONFIG_SA1100_USBCABLE_GPIO), GPIO_BOTH_EDGES);

    dbg_init(2,"requesting udc irq: %d %d", 13, IRQ_Ser0UDC);

    return request_irq(IRQ_Ser0UDC, int_hndlr_device, SA_INTERRUPT, "SA1100 USBD Bus Interface", NULL);
}

/**
 * udc_request_cable_irq - request Cable interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_cable_irq()
{
#ifdef CONFIG_SA1100_USBCABLE_GPIO
    dbg_init(2,"requesting cable irq: %d %d", 
            CONFIG_SA1100_USBCABLE_GPIO,
            SA1100_GPIO_TO_IRQ(CONFIG_SA1100_USBCABLE_GPIO));
    GPDR &= ~GPIO_GPIO(23);
    set_GPIO_IRQ_edge(GPIO_GPIO(CONFIG_SA1100_USBCABLE_GPIO), GPIO_BOTH_EDGES);
    return request_irq(SA1100_GPIO_TO_IRQ(CONFIG_SA1100_USBCABLE_GPIO), int_hndlr_cable, 
            SA_INTERRUPT, "SA1100 Monitor", NULL);
#else
    return 0;
#endif
}

/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io()
{
    sa1100_bi_init_dma();

    if (sa1100_bi_request_dma(&usbd_rcv_dma, "SA1100 USBD OUT - rcv dma", DMA_Ser0UDCRd)) {
        dbg_init(2,"channel %d taken", usbd_rcv_dma);
        return -EINVAL;
    }


    if (sa1100_bi_request_dma(&usbd_tx_dma, "SA1100 USBD IN - tx dma", DMA_Ser0UDCWr)) {
        dbg_init(2,"channel %d taken", usbd_tx_dma);
        sa1100_bi_free_dma(usbd_rcv_dma);
        return -EINVAL;
    }

    return 0;
}

/**
 * udc_release_udc_irq - release UDC irq
 */
void udc_release_udc_irq()
{
    free_irq(IRQ_Ser0UDC, NULL); 
}

/**
 * udc_release_cable_irq - release Cable irq
 */
void udc_release_cable_irq()
{
#ifdef CONFIG_SA1100_USBCABLE_GPIO
    dbg_init(2,"freeing cable irq: %d", CONFIG_SA1100_USBCABLE_GPIO);
    free_irq(SA1100_GPIO_TO_IRQ(CONFIG_SA1100_USBCABLE_GPIO), NULL); 
#endif
}

/**
 * udc_release_io - release UDC io region
 */
void udc_release_io()
{
    sa1100_bi_free_dma(usbd_rcv_dma);
    sa1100_bi_free_dma(usbd_tx_dma);
}

