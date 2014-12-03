/*
 * linux/drivers/usbd/sa1100_bi/sa1100-init.c
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


#ifdef CONFIG_SA1100_BITSY
#include <linux/h3600_ts.h>
#endif

#include "../usbd.h"
#include "../usbd-debug.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"

#include "sa1100.h"

#if defined(CONFIG_SA1100_COLLIE)
#define STUB_OUT_TICK 1
#endif

extern unsigned char usb_address;
extern unsigned int udc_interrupts;
extern unsigned int ep0_interrupts;
extern unsigned int tx_interrupts;
extern unsigned int rx_interrupts;
extern unsigned int sus_interrupts;
extern unsigned int res_interrupts;
extern unsigned int udc_address_errors;
extern unsigned int udc_ticks;
extern unsigned int udc_fixed;

/* Clock Tick Debug support ****************************************************************** */


#define RETRYTIME 10
#if defined(CONFIG_USBD_STALL_TIMEOUT)
  // If an URB waits more than...
#define USBD_STALL_TIMEOUT_SECONDS        CONFIG_USBD_STALL_TIMEOUT
#else
#define USBD_STALL_TIMEOUT_SECONDS        0  // Stall watchdog hobbled
#endif

#if defined(CONFIG_USBD_STALL_DISCONNECT_DURATION)
#define USBD_STALL_DISCONNECT_DURATION    CONFIG_USBD_STALL_DISCONNECT_DURATION
#else
#define USBD_STALL_DISCONNECT_DURATION    2  // seconds
#endif

extern unsigned int udc_interrupts_last;
unsigned int ep0_interrupts_last;
unsigned int rx_interrupts_last;
unsigned int tx_interrupts_last;

unsigned int udc_rpe_errors;
unsigned int udc_ep1_errors;
unsigned int udc_ep2_errors;
unsigned int udc_ep2_tpe;
unsigned int udc_ep2_tur;
unsigned int udc_ep2_sst;
unsigned int udc_ep2_fst;


#if !defined(STUB_OUT_TICK)
static unsigned long tx_queue_head_timestamp(unsigned long now)
{
    /* Return the timestamp from the urb at the head of the TX
       queue if there is one, otherwise return "now".
       Do it with interrupts off so the urb doesn't get snatched
       from our fingers while we are trying to see if it's there. */
    struct usb_device_instance *device;
    struct usb_endpoint_instance *endpoint;
    struct urb_link *tx_head = NULL;
    unsigned long flags;
    local_irq_save(flags);
    if (NULL != (device = device_array[0]) && NULL != device->bus &&
        NULL != device->bus->endpoint_array &&
        NULL != (endpoint = device->bus->endpoint_array + EP2_TX_PHYSICAL) &&
        NULL != (tx_head = endpoint->tx.next) &&
        tx_head != &endpoint->tx)
    {   // There is an urb in the queue
        struct urb *urb = p2surround(struct urb,link,tx_head);
        now = urb->jiffies;
    }
    local_irq_restore(flags);
    return(now);
}


char hexdigit(int hex)
{
    hex &= 0xf;
    return (hex < 0xa)? ('0' + hex):('a'+hex-0xa);
}

int hex2buf(char *buf, char *str, int num)
{
    char *cp = buf;
    while (*str) {
        *cp++ = *str++;
    }
    *cp++ = hexdigit(num>>4);
    *cp++ = hexdigit(num&0xf);
    *cp++ = ' ';
    return (cp - buf);
}

extern int send_length;
extern int send_todo;
extern int sent_last;

static void show_info(void)
{
    char buf[100];
    char *cp;
    volatile short int gpdr;
/*
    if (udc_interrupts_last == udc_interrupts) {
        udc_irq_enable();
    }
*/
    if (udc_interrupts != udc_interrupts_last) {
        printk(KERN_DEBUG "--------------\n");
    }

    // do some work
    memset(buf, 0, sizeof(buf));
    cp = buf;
    cp += hex2buf(cp, "CCR:", *(UDCCR));
    cp += hex2buf(cp, "CSR:", *(UDCSR));
    cp += hex2buf(cp, "CAR:", *(UDCAR));
    cp += hex2buf(cp, "CS0:", *(UDCCS0));
    cp += hex2buf(cp, "CS1:", *(UDCCS1));
    cp += hex2buf(cp, "CS2:", *(UDCCS2));
    cp += hex2buf(cp, "IMP:", *(UDCIMP));
    cp += hex2buf(cp, "OMP:", *(UDCOMP));
    cp += hex2buf(cp, "WC:", *(UDCWC));
    cp += hex2buf(cp, "SR:", *(UDCSR));

    gpdr = GPDR;
    printk(KERN_DEBUG 
            "[%u] %s int:%2d ep0:%d rx:%d tx:%d sus:%d res:%d er:%d re:%d e1:%d e2:%d tpe:%d tur:%d sst:%d fst:%d tck:%d fix:%d\n", 
            udc_interrupts, buf, udc_interrupts - udc_interrupts_last,
            ep0_interrupts - ep0_interrupts_last, 
            rx_interrupts - rx_interrupts_last, 
            tx_interrupts - tx_interrupts_last,
            sus_interrupts, res_interrupts,
            udc_address_errors,
            udc_rpe_errors,
            udc_ep1_errors,
            udc_ep2_errors,
            udc_ep2_tpe,
            udc_ep2_tur,
            udc_ep2_sst,
            udc_ep2_fst,
            udc_ticks,
            udc_fixed
          );
    udc_interrupts_last = udc_interrupts;
    ep0_interrupts_last = ep0_interrupts;
    rx_interrupts_last = rx_interrupts;
    tx_interrupts_last = tx_interrupts;
}
#endif

int udc_regs(void *data) 
{
#if !defined(STUB_OUT_TICK)
    if ( _udc(UDCAR) != usb_address) {
        printk(KERN_DEBUG"ADDRESS ERROR DETECTED\n");
        udc_address_errors++;
    }
    *(UDCAR) = usb_address;

    show_info();
#endif
    return 0;
}

