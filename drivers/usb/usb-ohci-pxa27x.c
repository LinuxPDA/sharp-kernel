/*
 * linux/drivers/usb/usb-ohci-pxa27x.c
 * 
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 * 
 * Based on:
 *
 *  linux/drivers/usb/usb-ohci-tc6393.c
 *
 *  The outline of this code was taken from Brad Parkers <brad@heeltoe.com>
 *  original OHCI driver modifications, and reworked into a cleaner form
 *  by Russell King <rmk@arm.linux.org.uk>.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/list.h>

#include <linux/proc_fs.h>
#if defined(CONFIG_ARCH_SHARP_SL) && defined(CONFIG_APM_CPU_IDLE)
#include <asm/sharp_apm.h>
#endif
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "usb-ohci.h"

extern int __devinit hc_add_ohci(struct pci_dev*, int, void*,
			unsigned long, ohci_t**, const char*);
extern void hc_remove_ohci(ohci_t*);

static ohci_t*    pxa27x_ohci = NULL;

#define USB_MEM_SIZE	(0x10000)
#define USB_CONNECT_GPIO		41
#define USBD_VBUS_GPIO_DEVICE	35
#define USBD_VBUS_GPIO_HOST		37

static int __init pxa27x_ohci_init(void)
{
	int ret;
    /* USB Cable Connected ? */
    if(!(GPLR1 & GPIO_bit(USB_CONNECT_GPIO))) {        
        // c.f. 3.6.2 Clock Enable Register
        CKEN &= ~CKEN11_USB;
        CKEN |=  CKEN10_USB;
#if defined(CONFIG_ARCH_SHARP_SL) && defined(CONFIG_APM_CPU_IDLE)
	lock_FCS(LOCK_FCS_USBH, 1);
#endif

        /* Init USB Port 2 Output Control Register */
        UP2OCR = 0x00;

        /* Host Port 2 Transceiver Output Enable */
        UP2OCR |= UP2OCR_HXS;
        UP2OCR |= UP2OCR_HXOE;

        /* D+ Pull Up SW1 ON */
        UP2OCR |= (UP2OCR_DPPDE|UP2OCR_DMPDE);
        /* Set GPIO Pin Hi to Unconnect Host */
        GPSR(USBD_VBUS_GPIO_HOST) = GPIO_bit(USBD_VBUS_GPIO_HOST);

    } else {
	return -1;
    }

    /* Clear PTG Peripheral Control Hold */
    if( PSSR & PSSR_OTGPH ){
        PSSR |= PSSR_OTGPH;
    }

    /* SSE(Sleep Standby Enable)   */
    /*   0 = Enables power to the USB single-ended receivers and the USB port power supply. */
    UHCHR &= ~UHCHR_SSE;
    /* SSEP2(Sleep Standby Enable for Port 2) */
    /*   0 = Enables power to the USB single-ended receivers and the USB port 2 power supply. */
    /*   1 = Disables power to the USB single-ended receivers and the USB port 2 power supply. */
    UHCHR &= ~UHCHR_SSEP2;
    /* FHR(Force Host Controller Reset) */
    /*   0 = Normal operation. */
    UHCHR &= ~UHCHR_FHR;

    udelay(100);

    ret = hc_add_ohci((struct pci_dev*)1,
	   IRQ_USBH1,
	   (void*)0xfe000000,
	   0,
	   &pxa27x_ohci,
	   "usb-ohci");
    if (ret) {
	printk(": pxa27x_ohci_init Error %x\n", ret );
    }

    return ret;
}

#ifdef MODULE
static void __exit pxa27x_ohci_exit(void)
{

    /* disable irq */
    ICMR &= ~(1 << (IRQ_USBH1));
    hc_remove_ohci(pxa27x_ohci);

    /* Set GPIO Pin Low to Unconnect Host */
    GPCR(USBD_VBUS_GPIO_HOST) = GPIO_bit(USBD_VBUS_GPIO_HOST);

    // c.f. 3.6.2 Clock Enable Register
    CKEN &= ~CKEN10_USB;
#if defined(CONFIG_ARCH_SHARP_SL) && defined(CONFIG_APM_CPU_IDLE)
    lock_FCS(LOCK_FCS_USBH, 0);
#endif

}
#endif



module_init(pxa27x_ohci_init);
#ifdef MODULE
module_exit(pxa27x_ohci_exit);
#endif
