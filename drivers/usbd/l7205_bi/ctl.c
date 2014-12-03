/*
 * linux/drivers/usbd/l7205_bi/clt.c -- L7205 USB controller driver. 
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


// Debug enabling defines
#define TRIGGER       1
#define FLAG_F1ERR    1
// End of debug enabling defines
//

#define USE_L7210     1

#include <linux/config.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>

#include <linux/proc_fs.h>
#include <linux/tqueue.h>

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

#include <linux/delay.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
//#include "ctl.h"

#include "hardware.h"
#include "l7205.h"



/* 
 * Prototypes
 */

int sof_saw_tx_active;
int sof_saw_tx_err;
int sof_saw_rx_err;
int saw_f1err;
int host_reset_ignored;
int host_reset_ignore;
int host_reset_duration = 500;

//unsigned int udc_interrupts;
unsigned int ep0_interrupts;
unsigned int tx_interrupts;
unsigned int rx_interrupts;
unsigned int udc_address_errors;
unsigned int f1rq_errors;
unsigned int f1err_count;
unsigned int udc_rpe_errors;
unsigned int udc_fcs_errors;
unsigned int udc_ep1_errors;
unsigned int udc_ep2_errors;

extern unsigned int udc_interrupts_last;
unsigned int ep0_interrupts_last;
unsigned int rx_interrupts_last;
unsigned int tx_interrupts_last;
unsigned int f1rq_errors_last;

unsigned int udc_rpe_errors;
unsigned int udc_ep1_errors;
unsigned int udc_ep2_errors;



extern int ep2_active;

void udc_int_hndlr(int, void *, struct pt_regs *);
int udc_stall_ep(int ep);


void l7205_toggle(volatile unsigned long reg, unsigned int bits)
{
    reg |= bits;
    udelay(10);
    reg &= ~bits;
}

void l7205_dump_fifos(char *msg, int max) 
{
    int i;

    //printk(KERN_DEBUG "\n");
    //printk(KERN_DEBUG "*********** %s ***********\n", msg);

    for (i = 0; i < max; i++) {
        if ((i%32)==0) {
            printk("\nFF[%02x]: ", i);
        }
        printk("%02x ", __IOB(USBF_FIFOS+i));
    }
    printk("\n");

}

void l7205_regs(char *msg) 
{
    printk(KERN_DEBUG "\n");
    printk(KERN_DEBUG "*********** %s ***********\n", msg);
    printk(KERN_DEBUG "\n");
    printk(KERN_DEBUG"udc: Rev:  %08lx Ctl:  %08lx Sts:  %08lx\n", IO_USBF_REVISION, IO_USBF_CONTROL, IO_USBF_STATUS);
    printk(KERN_DEBUG"udc: IntE: %08lx IntD: %08lx Ep0:  %08lx\n", IO_USBF_INTENA, IO_USBF_INTDIS, IO_USBF_ENDPTBUF0);
    printk(KERN_DEBUG"udc: Ep1:  %08lx Ep2:  %08lx Ep3:  %08lx\n", IO_USBF_ENDPTBUF1, IO_USBF_ENDPTBUF2, IO_USBF_ENDPTBUF3);
    printk(KERN_DEBUG"udc: CFG:  %08lx ST0:  %08lx ST1:  %08lx\n", IO_USBF_CONFIGBUF1, IO_USBF_STRINGBUF0, IO_USBF_STRINGBUF1);
    printk(KERN_DEBUG"udc: ST2:  %08lx ST3:  %08lx ST4:  %08lx\n", IO_USBF_STRINGBUF2, IO_USBF_STRINGBUF3, IO_USBF_STRINGBUF4);

    printk(KERN_DEBUG "\n");

    printk(KERN_DEBUG"udc: CFG:  %02lx:%02lx    ST0:  %02lx:%02lx    ST1:  %02lx:%02lx\n", 
            __IOL(USBF_CONFIGBUF1)>>9, __IOL(USBF_CONFIGBUF1)&0x1ff, 
            __IOL(USBF_STRINGBUF0)>>9, __IOL(USBF_STRINGBUF0)&0x1ff, 
            __IOL(USBF_STRINGBUF1)>>9, __IOL(USBF_STRINGBUF1)&0x1ff);

    printk(KERN_DEBUG"udc: ST2:  %02lx:%02lx    ST3:  %02lx:%02lx    ST4:  %02lx:%02lx\n", 
            __IOL(USBF_STRINGBUF2)>>9, __IOL(USBF_STRINGBUF2)&0x1ff, 
            __IOL(USBF_STRINGBUF3)>>9, __IOL(USBF_STRINGBUF3)&0x1ff, 
            __IOL(USBF_STRINGBUF4)>>9, __IOL(USBF_STRINGBUF4)&0x1ff);

#if 1
    {
        int i;
        for (i = 0; i < 168; i++) {
            if ((i%32)==0) {
                printk("\nDS[%02x]: ", i);
            }
            printk("%02x ", __IOB(USBF_DESCRIPTORS+i));
        }
        printk("\n");
        printk("\n");
    }
#endif

}

void udc_regs(void) 
{
    printk(KERN_DEBUG 
            "%lu Ctl: %08lx Sts: %08lx Raw: %08lx Ena: %08lx EP0: %08lx EP1: %08lx EP2: %08lx "
            "int: %2d ep0: %d rx: %d tx: %d f1rq: %d f1err: %d re: %d e1: %d e2: %d\n", 
            udc_interrupts, 
            IO_USBF_CONTROL, IO_USBF_STATUS, IO_USBF_RAWSTATUS, IO_USBF_INTENA,
            IO_USBF_F0BCNT, IO_USBF_F1BCNT, IO_USBF_F2BCNT,
            udc_interrupts - udc_interrupts_last,
            ep0_interrupts - ep0_interrupts_last, 
            rx_interrupts - rx_interrupts_last, 
            tx_interrupts - tx_interrupts_last,
            f1rq_errors - f1rq_errors_last,
            f1err_count,
            udc_rpe_errors,
            udc_ep1_errors,
            udc_ep2_errors
          );

    udc_interrupts_last = udc_interrupts;
    ep0_interrupts_last = ep0_interrupts;
    rx_interrupts_last = rx_interrupts;
    tx_interrupts_last = tx_interrupts;
    f1rq_errors_last = f1rq_errors;
}


    
static int l7205_copy_descriptor(unsigned char *descriptors, volatile unsigned long reg, int offset, 
        struct usb_descriptor *descriptor, int size) 
{
    // set default 
    if (reg) {
        __IOL(reg) = 0;
    }

    if (!descriptors || !descriptor) {
        return offset;
    }

    if (!size) {
        if (!(size = descriptor->descriptor.generic.bLength)) {
            return offset;
        }
    }

    if (((offset + size) >= (USBF_DESCRIPTORS_MAX-4))) {
        printk(KERN_DEBUG"l7205_copy_descriptor: string to large to copy: offset: %d size: %d\n", offset, size);
        if (reg) {
            __IOL(reg) = (4<<9) | (USBF_DESCRIPTORS_MAX-4);
        }
        descriptors[USBF_DESCRIPTORS_MAX-4] = 4;
        descriptors[USBF_DESCRIPTORS_MAX-3] = 3;
        descriptors[USBF_DESCRIPTORS_MAX-2] = 'A';
        descriptors[USBF_DESCRIPTORS_MAX-1] = 0;
        return offset;
    }

    //printk(KERN_DEBUG"l7205_copy_descriptor: reg: %08lx offset: %02x descriptor: %p size: %02x\n", reg, offset, descriptor, size);

    // set size and offset if we have a register
    if (reg) {
        __IOL(reg) = (size << 9) | offset;
        //printk(KERN_DEBUG"l7205_copy_descriptor: reg: %8lx %8lx size: %2lx off: %2lx\n", 
        //        reg, __IOL(reg), __IOL(reg)>>9, __IOL(reg)&0x1ff);

    }

    memcpy(descriptors +  offset, descriptor, size);

    return (offset + size + 3) & 0x1fc;   // 4
    //return (offset + size + 0xe) & 0x1f0; // 16

}


static int l7205_copy_request(unsigned char *descriptors, struct usb_device_instance *device, 
        volatile unsigned long reg, int offset, int request) 
{
    struct urb *udc_urb;
    usb_device_state_t device_state;

    /*
     * fake a setup request to get descriptors
     */
    if (!(udc_urb = usbd_alloc_urb(device, NULL, 0, 512))) {
        printk(KERN_DEBUG"udc_init: cannot alloc urb\n");
        return -EINVAL;
    }

    udc_urb->device_request.bmRequestType = 0x80;
    udc_urb->device_request.bRequest = USB_REQ_GET_DESCRIPTOR;
    udc_urb->device_request.wValue = cpu_to_le16((request << 8));
    udc_urb->device_request.wLength = cpu_to_le16(200); // max 168?

    device_state = device->device_state;
    device->device_state = STATE_ADDRESSED;

    if (usbd_recv_setup(udc_urb)) {
        printk(KERN_DEBUG"udc_init: cannot process setup request\n");
        usbd_dealloc_urb(udc_urb);
        return offset;
    }

    device->device_state = device_state;
#if 0
    {
        int i;
        for (i = 0; i < udc_urb->actual_length; i++) {
            if ((i%32)==0) {
                printk("\nUR[%02x]:", i);
            }
            printk("%02x ", udc_urb->buffer[i]);
        }
        printk("\n");
        printk("\n");
    }
#endif

    offset = l7205_copy_descriptor(descriptors, reg, offset, (struct usb_descriptor *)udc_urb->buffer, udc_urb->actual_length);

    usbd_dealloc_urb(udc_urb);

    return offset;

}


/**
 * udc_init - initialize L7205 USB Controller
 * 
 * Get ready to use the L7205 USB Controller.
 *
 * Register an interrupt handler and initialize dma.
 */
int udc_init(void)
{  
    printk(KERN_DEBUG"udc_init:\n");

    udc_disable_interrupts(0);
    udc_disable();

    if (IO_USBF_REVISION != USBF_REVISION_11) {
        printk(KERN_DEBUG"udc_ctl: incorrect or not found version: %02lx should be %02x\n", IO_USBF_REVISION, USBF_REVISION_11);
        return -EINVAL;
    }

    return 0;
}

/**
 * udc_exit - Stop using the SA1100 USB Controller
 *
 * Stop using the SA1100 USB Controller.
 *
 * Shutdown and free dma channels, de-register the interrupt handler.
 *
 */
void udc_exit()
{
    printk(KERN_INFO "Unloading L7205 USB Controller\n");

    //ep1_disable();
    //ep2_disable();
    
    udc_disable();
	
}

/*
 * Switch off the SA1100 UDC
 */
void udc_disable(void)
{
    printk(KERN_DEBUG "************************* udc_disable:\n");

    // set USBFUNC_EN
    //IO_SYS_CLOCK_ENABLE &= ~SYS_CLOCK_USBFUNC_EN;
    
    /*
     * reset USB function
     */
    IO_USBF_CONTROL |= USBF_CONTROL_FRST;
    udelay(10);
    IO_USBF_CONTROL &= ~(USBF_CONTROL_ENBL|USBF_CONTROL_INTD);

}

/*
 * Switch on the SA1100 UDC
 */
void udc_enable(struct usb_device_instance *device)
{
    //int i;
    int offset;
    unsigned char *descriptors;

    printk(KERN_DEBUG "************************* udc_enable:\n");

    // allocate tmp buffer
    if (!(descriptors = kmalloc(USBF_DESCRIPTORS_MAX, GFP_ATOMIC))) {
        printk(KERN_INFO "udc_init: cannot malloc descriptors\n");
        return;
    }
    memset(descriptors, 0, USBF_DESCRIPTORS_MAX);
    memset((void *)(__IOA(USBF_DESCRIPTORS)), 0, USBF_DESCRIPTORS_MAX);

    //
    l7205_regs("pre-init");



    /*
     * XXX
     * Disable LED 
     */

                                //        24   20   16 
                                //        14   10    a    8    4    0
                                //      0000 0010 0000 0000 0000 0001
                                //         0    2    0    0    0    1
                                //     00  c    0    0    0    4    7
#if 1
    printk(KERN_DEBUG "1. CLCDCON: %08x SYS_CLOCK_ENABLE: %08x\n", IO_CLCDCON, IO_SYS_CLOCK_ENABLE);

    // disable LCD power (bit 22) and wait 20MS
    IO_CLCDCON &= ~0x20000;
    udelay(100);
    printk(KERN_DEBUG "2. CLCDCON: %08x SYS_CLOCK_ENABLE: %08x\n", IO_CLCDCON, IO_SYS_CLOCK_ENABLE);

    // disable LCD (bit 0)
    IO_CLCDCON &= ~0x1;
    printk(KERN_DEBUG "3. CLCDCON: %08x SYS_CLOCK_ENABLE: %08x\n", IO_CLCDCON, IO_SYS_CLOCK_ENABLE);

    // disable Vee and backligth bits 1, 2
    IO_SYS_CLOCK_ENABLE &= ~0x3;
    printk(KERN_DEBUG "4. CLCDCON: %08x SYS_CLOCK_ENABLE: %08x\n", IO_CLCDCON, IO_SYS_CLOCK_ENABLE);
#endif


    /*
     * Setup FIRCLK to use internal clock - c.f. 13.1.2 Clocks
     */
#if defined(CONFIG_USBD_L7210AB_FIX)
    printk(KERN_DEBUG"udc_init: setting clocks (external clock)\n");
    

    // select external STIRCLK
    IO_SYS_CLOCK_SELECT |= 0;

    // set FIR enable
    IO_SYS_CLOCK_ENABLE |= SYS_CLOCK_FIR_EN;
#else
    printk(KERN_DEBUG"udc_init: setting clocks (internal PLL)\n");
    

    // clear AUXOSCMUX, AUXPLLMUX and AUXPLLMUL bits of CLOCK_AUX
    IO_SYS_CLOCK_AUX = 0;

    // clear AUX0SCMUX and set AUXPLLMUL to 13 for 48Mhz
    IO_SYS_CLOCK_AUX |= SYS_CLOCK_AUXPLLMUL_48;

    // set FIRAUX_SEL
    IO_SYS_CLOCK_SELECT |= SYS_CLOCK_FIRAUX_SEL;

    // set AUXPLL_EN and AUXCLK_EN 
    IO_SYS_CLOCK_ENABLE |= SYS_CLOCK_AUXPLL_EN | SYS_CLOCK_AUXCLK_EN;
#endif

    // wait 8us
    udelay(1); 

    // set USBFUNC_EN
    IO_SYS_CLOCK_ENABLE |= SYS_CLOCK_USBFUNC_EN;

    /*
     * Configuration and Intialization - c.f. 13.1.4 and c.f. 13.1.1
     */
    
    // set UDC control register
    IO_USBF_CONTROL = 0;
    udelay(10);
    IO_USBF_CONTROL = USBF_CONTROL_ENBL;

    // set program modes
#ifndef USE_DMA
    IO_USBF_CONTROL |= USBF_CONTROL_F1MOD_IO | USBF_CONTROL_F2MOD_IO;
#else
    IO_USBF_CONTROL |= USBF_CONTROL_F1MOD_IO | USBF_CONTROL_F2MOD_DMA;
#endif

    // clear all FIFO's
    l7205_toggle(IO_USBF_CONTROL, USBF_CONTROL_F0CLR | USBF_CONTROL_F1CLR | USBF_CONTROL_F2CLR | USBF_CONTROL_F3CLR);

    offset = l7205_copy_request(descriptors, device,               0,      0, USB_DESCRIPTOR_TYPE_DEVICE);
    offset = l7205_copy_request(descriptors, device, USBF_CONFIGBUF1, offset, USB_DESCRIPTOR_TYPE_CONFIGURATION);

    offset = l7205_copy_descriptor(descriptors, USBF_STRINGBUF0, offset, (struct usb_descriptor *)usbd_get_string(1), 0);
    offset = l7205_copy_descriptor(descriptors, USBF_STRINGBUF1, offset, (struct usb_descriptor *)usbd_get_string(2), 0);
    offset = l7205_copy_descriptor(descriptors, USBF_STRINGBUF2, offset, (struct usb_descriptor *)usbd_get_string(3), 0);
    offset = l7205_copy_descriptor(descriptors, USBF_STRINGBUF3, offset, (struct usb_descriptor *)usbd_get_string(4), 0);
    offset = l7205_copy_descriptor(descriptors, USBF_STRINGBUF4, offset, (struct usb_descriptor *)usbd_get_string(5), 0);

    printk(KERN_DEBUG"\n");

    memcpy((unsigned char *)__IOA(USBF_DESCRIPTORS), descriptors, USBF_DESCRIPTORS_MAX);
    kfree(descriptors);

    printk(KERN_DEBUG"udc_enable: setting INTD\n");

    IO_USBF_ENDPTBUF0 = USBF_ENDPTBUF0_EP0MSIZE_8;
    IO_USBF_ENDPTBUF1 = USBF_ENDPTBUF1_EP1TYPE_BULK | 0x20;
    IO_USBF_ENDPTBUF2 = USBF_ENDPTBUF2_EP2TYPE_BULK | 0x20;
    IO_USBF_ENDPTBUF3 = USBF_ENDPTBUF3_EP3TYPE_INT;

    IO_USBF_F1TOUT = 1;
    IO_USBF_F1BCNT = 0x20;
    //IO_USBF_F2BCNT = 0;

    // force USB function core reset
    l7205_toggle(IO_USBF_CONTROL, USBF_CONTROL_FRST);

    // set initialization done bit to indicate descriptors and registers are ready
    IO_USBF_CONTROL |= USBF_CONTROL_INTD;

    // enable interrupts
    IO_USBF_INTENA = 
        USBF_INTENA_ENSUS |
        USBF_INTENA_ENF0ERR |
        USBF_INTENA_ENF1OR |
        USBF_INTENA_ENF1ERR |
        USBF_INTENA_ENF2UR |
        USBF_INTENA_ENF2ERR |
        USBF_INTENA_ENF3ERR |
        USBF_INTENA_ENFRSM |
        USBF_INTENA_ENF0RQ |
        USBF_INTENA_ENF1RQ |
        USBF_INTENA_ENF2RQ |
        USBF_INTENA_ENF3RQ |
        USBF_INTENA_ENSOF |
        USBF_INTENA_ENHRST ; 

    l7205_regs("finished");

    return;
}

#if defined(TRIGGER)

static unsigned int max_triggers = 64;

static void send_usb_trigger(unsigned int status, char *id)
{
    if (max_triggers == 0) {
        printk(KERN_DEBUG "%s & no more triggers\n",id);
        return;
    }
    if (status & USBF_STATUS_F2BSY) {
        printk(KERN_DEBUG "%s & F2BSY\n",id);
    } else {
        static char flag_bytes[48] = {
            0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
            0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
            0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef
        };
        unsigned long *slp;
        unsigned long *txf = IO_USBF_FIFO_TX;
        int i,j,fifo_good;
        slp = (unsigned long *) (void *) (0xfffffffc & (unsigned long) (3 + (void *)flag_bytes));
        j = 0;
        do {
            for (i = 0; i < 8; i++) {
                 IO_USBF_FIFO2 = slp[i];
            }
            fifo_good = 1;
            for (i = 0; i < 8; i++) {
                fifo_good &= (slp[i] == txf[i]);
            }
        } while (++j < 10 && !fifo_good);
        if (fifo_good) {
            IO_USBF_F2BCNT = 32;
            printk(KERN_DEBUG "deadbeef %u %s\n",max_triggers,id);
            max_triggers -= 1;
        } else {
            // too busy receiving?
            printk(KERN_DEBUG "%s & F1BSY\n",id);
        }
    }
}
#endif

#define STATUS_NO_INTR_MASK   ~(USBF_STATUS_F1NE | USBF_STATUS_F2NF | USBF_STATUS_F2NE | USBF_STATUS_F2BSY | USBF_STATUS_VCCMD);

/**
 * udc_int_hndlr - interrupt handler
 *
 */
void udc_int_hndlr(int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned int status;
    unsigned int rawstatus;
    unsigned int orig_status = 0;
    unsigned int orig_rawstatus = 0;
    int loopcount;

    udc_interrupts++;

    /* Yes, this really is meant to be a bitwise "|" and not a shortcut "||", because we
       want to read _both_ status registers, every time. */

    for (loopcount = 0; 0 != (rawstatus = IO_USBF_RAWSTATUS) && loopcount < 10; loopcount++)
    {
        //((0 != ((status = IO_USBF_STATUS) & STATUS_NO_INTR_MASK)) | (0 != (rawstatus = IO_USBF_RAWSTATUS))) && loopcount < 10;
         
        status = IO_USBF_STATUS;

        if (0 == loopcount) {
            //orig_status = status;
            orig_rawstatus = rawstatus;
        }

#if 0
        if (saw_f1err && (rawstatus & ~USBF_RAWSTATUS_RSOF)) {
            printk(KERN_DEBUG"udc_int_hndlr[%u]: saw_F1ERR Ctl: %08lx Sts: %08lx Raw: %08lx Ena: %08lx Dis: %08lx Clr: %08lx\n", 
                    udc_interrupts, IO_USBF_CONTROL, IO_USBF_STATUS, IO_USBF_RAWSTATUS, IO_USBF_INTENA, 
                    IO_USBF_INTDIS, IO_USBF_INTCLR);
        }
#endif


        // check if we have an unusual interrupt
        if (rawstatus & 
                (USBF_RAWSTATUS_RSUS | USBF_RAWSTATUS_RF0ERR | USBF_RAWSTATUS_RF1OR | 
                 USBF_RAWSTATUS_RF1ERR | USBF_RAWSTATUS_RF2ERR | USBF_RAWSTATUS_RF3ERR |
                 USBF_RAWSTATUS_RGRSM | USBF_RAWSTATUS_RF0RQ | USBF_RAWSTATUS_RF3RQ |
                 USBF_RAWSTATUS_RHRST
                 )) 
        {
            if (rawstatus &   USBF_RAWSTATUS_RSUS) {
                IO_USBF_INTCLR = USBF_INTCLR_CSUS;
                printk(KERN_DEBUG"udc_int_hndlr[%u]: SUS Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RSUS;
                //status &= ~USBF_STATUS_SUS;
            }
            if (rawstatus &   USBF_RAWSTATUS_RF0ERR) {
                IO_USBF_INTCLR = USBF_INTCLR_CF0ERR;
                printk(KERN_DEBUG"udc_int_hndlr[%u]: F0ERR Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RF0ERR;
                //status &= ~USBF_STATUS_F0ERR;
            }
            if (rawstatus &   USBF_RAWSTATUS_RF1OR) {
                printk(KERN_DEBUG"udc_int_hndlr[%u]: F1OR Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                sof_saw_rx_err = 0;
                ep1_int_hndlr(status, rawstatus, 1, USBF_INTCLR_CF1OR);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RF1OR;
                //status &= ~USBF_STATUS_F1OR;
            }

            if (rawstatus &   USBF_RAWSTATUS_RF1ERR) {
                printk(KERN_DEBUG"udc_int_hndlr[%u]: F1ERR Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                f1err_count += 1;
                sof_saw_rx_err = 0;
                saw_f1err = 1;
#if defined(FLAG_F1ERR) && defined(TRIGGER)
                send_usb_trigger(status,"F1ERR");
#endif
                ep1_int_hndlr(status, rawstatus, 1, USBF_INTCLR_CF1ERR);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RF1ERR;
                //status &= ~USBF_STATUS_F1ERR;
            }

            if (rawstatus &   USBF_RAWSTATUS_RF2UR) {
                printk(KERN_DEBUG"udc_int_hndlr[%u]: F2UR Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                sof_saw_tx_err = 0;
                sof_saw_tx_active = 0;
                ep2_int_hndlr(status, rawstatus, 1, 0, USBF_INTCLR_CF2UR, 0);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RF2UR;
                //status &= ~USBF_STATUS_F2UR;
            }

            if (rawstatus &   USBF_RAWSTATUS_RF2ERR) {
                printk(KERN_DEBUG"udc_int_hndlr[%u]: F2ERR Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                sof_saw_tx_err = 0;
                sof_saw_tx_active = 0;
                ep2_int_hndlr(status, rawstatus, 1, 1, USBF_INTCLR_CF2ERR, 0);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RF2ERR;
                //status &= ~USBF_STATUS_F2ERR;
            }

            if (rawstatus &   USBF_RAWSTATUS_RF3ERR) {
                IO_USBF_INTCLR = USBF_INTCLR_CF3ERR;
                printk(KERN_DEBUG"udc_int_hndlr[%u]: F3ERR Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RF3ERR;
                //status &= ~USBF_STATUS_F3ERR;
            }

            if (rawstatus &   USBF_RAWSTATUS_RGRSM) {
                IO_USBF_INTCLR = USBF_INTCLR_CGRSM;
                printk(KERN_DEBUG"udc_int_hndlr[%u]: GRSM Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RGRSM;
                //status &= ~USBF_STATUS_GRSM;
            }

            if (rawstatus &   USBF_RAWSTATUS_RF0RQ) {
                IO_USBF_INTCLR = USBF_INTCLR_CF0RQ;
                printk(KERN_DEBUG"udc_int_hndlr[%u]: F0RQ Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RF0RQ;
                //status &= ~USBF_STATUS_F0RQ;
            }

            if (rawstatus &   USBF_RAWSTATUS_RF3RQ) {
                IO_USBF_INTCLR = USBF_INTCLR_CF3RQ;
                printk(KERN_DEBUG"udc_int_hndlr[%u]: F3RQ Sts: %08lx Raw: %08lx\n", 
                        udc_interrupts, IO_USBF_STATUS, IO_USBF_RAWSTATUS);
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RF3RQ;
                //status &= ~USBF_STATUS_F3RQ;
            }

            if (rawstatus &   USBF_RAWSTATUS_RHRST) {
                // L7210 AB version would give multiple HRST when the first is cleared,
                // ignore them for duration ms (use SOF to count down the flag).
                if (host_reset_ignore <= 0) {
                    IO_USBF_INTCLR = USBF_INTCLR_CHRST;
                    printk(KERN_DEBUG"udc_int_hndlr[%u]: HRST\n", udc_interrupts);
                    l7205_toggle(IO_USBF_CONTROL, USBF_CONTROL_FRST);
                    IO_USBF_CONTROL = USBF_CONTROL_ENBL | USBF_CONTROL_F1MOD_IO | USBF_CONTROL_F2MOD_IO | USBF_CONTROL_INTD;
                    l7205_regs("HRST");
                    if (!host_reset_ignored) {
                        host_reset_ignored = 1;
                        host_reset_ignore = host_reset_duration;
                        printk(KERN_DEBUG"udc_int_hndlr[%u]: HRST ignored for the next %dms\n", 
                                udc_interrupts,host_reset_duration);
                    }
                } else {
                    printk(KERN_DEBUG"udc_int_hndlr[%u]: HRST ignored\n", udc_interrupts);
                }
                //continue;
                rawstatus &= ~USBF_RAWSTATUS_RHRST;
                //status &= ~USBF_STATUS_HRST;
            }

        }

        if (rawstatus &   USBF_RAWSTATUS_RF1RQ) {
            //ep1_int_hndlr(status, rawstatus, 0, USBF_INTCLR_CF1RQ);
            sof_saw_rx_err = 0;
            if (0 != loopcount) {
                f1rq_errors += 1;
                // print out all the f1rq events for every 256th interrupt, to show pattern of rawstatus behaviour
                // (caching, bouncing?)  (e.g. 2,3,5 or 1,2,6)
#if 0
                if (!(udc_interrupts&0xff)) {
                    printk(KERN_DEBUG"udc_int_hndlr[%u/%d]: F1RQ would have been missed! f1rq: %u O_Raw#%08x->%08x\n",
                           udc_interrupts,loopcount,f1rq_errors,orig_rawstatus,rawstatus);
                }
#endif
            }
            ep1_int_hndlr(status, rawstatus, 0, USBF_INTCLR_CF1RQ);
            //continue;
            rawstatus &= ~USBF_RAWSTATUS_RF1RQ;
            //status &= ~USBF_STATUS_F1RQ;
        }

        if (rawstatus &   USBF_RAWSTATUS_RF2RQ) {
            //printk(KERN_DEBUG"udc_int_hndlr[%u]: F2RQ\n", udc_interrupts);
            sof_saw_tx_err = 0;
            sof_saw_tx_active = 0;
            ep2_int_hndlr(status, rawstatus, 1, 0, USBF_INTCLR_CF2RQ, 0);
            //continue;
            rawstatus &= ~USBF_RAWSTATUS_RF2RQ;
            //status &= ~USBF_STATUS_F2RQ;
        }

        if (rawstatus &   USBF_RAWSTATUS_RSOF) {
            unsigned int status = IO_USBF_STATUS;
            IO_USBF_INTCLR = USBF_INTCLR_CSOF;

            // this seems to be required in case we miss a rcv interrupt
            if (sof_saw_rx_err++ > 10) {
                ep1_tick();
                sof_saw_rx_err = 0;
            }

            // this seems to be required in case we miss a tx interrupt
            if ((sof_saw_tx_active++ > 10) && ep2_active) {
                //printk(KERN_DEBUG"udc_int_hndlr[%d]: ep2_active\n", udc_interrupts);
                ep2_int_hndlr(status, rawstatus, 1, 0, 0, 0);
                sof_saw_tx_active = 0;
            }

            // l7210 AB version would give multiple HRST when the first is cleared,
            // ignore them for 15 ms (use SOF to count down the flag).
            if (host_reset_ignore > 0) {
                host_reset_ignore -= 1;
                if (host_reset_ignore <= 10) {
                    printk(KERN_DEBUG"udc_int_hndlr[%u]: SOF HRST ignore -> %d\n", udc_interrupts, host_reset_ignore);
                }
            }

            if ((status & USBF_STATUS_F2BSY) && (status & USBF_STATUS_F2NE) && (status & USBF_STATUS_F2NF)) {

                if (sof_saw_tx_err++ == 20) {
                    printk(KERN_DEBUG"udc_int_hndlr[%u]: SOF_SAW_TX_ERR %08lx\n", udc_interrupts, IO_USBF_STATUS);
                    //l7205_toggle(IO_USBF_CONTROL, USBF_CONTROL_F2CLR);
                }

                if (sof_saw_tx_err>=10000) {
                    //IO_USBF_F2BCNT = 0;
                    sof_saw_tx_err = 0;
                }
                //continue;
            }

            rawstatus &= ~USBF_RAWSTATUS_RSOF;
            //status &= ~USBF_STATUS_SOF;
        }

        // TBR clear all the non-interrupt status 
        //status &= STATUS_NO_INTR_MASK;
        if (/* 0 != status ||*/ 0 != rawstatus) {
            printk(KERN_DEBUG"udc_int_hndlr[%u]: missed servicing interrupt bits Sts#%08x Raw#%08x\n",
                    udc_interrupts,status,rawstatus);
        }

    } // end of loopcount loop

#if 0
    if (loopcount >= 10 && 0 != rawstatus) {
        printk(KERN_DEBUG"udc_int_hndlr[%u] missed interrupt bits Sts#%08x Raw#%08x\n",udc_interrupts,status,rawstatus);
    }
#endif
}

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

