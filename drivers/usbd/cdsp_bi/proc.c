/*
 * linux/drivers/usbd/cdsp_bi/proc.c - USB device CDSP bus interface
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

#ifdef MODULE
#include <linux/module.h>
#else
#error MODULE not defined
#endif
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "cdsp.h"
#include "cdsp_proc.h"

/* Proc Filesystem *************************************************************************** */


/* dohex
 *
 */
void dohexdigit(char *cp, unsigned char val) 
{
    if (val < 0xa) {
        *cp = val + '0';
    }
    else if ((val >= 0x0a) && (val <= 0x0f)) {
        *cp = val - 0x0a + 'a';
    }
}

/* dohex
 *
 */
void dohexval(char *cp, unsigned char val) 
{
    dohexdigit(cp+0, val>>4);
    dohexdigit(cp+1, val&0xf);
}

/**
 * usb_bus_proc_read - implement proc file system read.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Standard proc file system read function.
 *
 * We let upper layers iterate for us, *pos will indicate which device to return
 * statistics for.
 */
static          ssize_t
usb_bus_proc_read(struct file *file, char *buf, size_t count, loff_t * pos)
{
    unsigned long   page;
    int len = 0;
    int index;


    // get a page, max 4095 bytes of data...
    if (!(page = get_free_page(GFP_KERNEL))) {
        return -ENOMEM;
    }

    len = 0;
    index = (*pos)++;

    if (index-- == 0) {

        struct list_head *lhd;
        struct urb *urb = NULL;

        // pretend to iterate, but we'll actually just look at the first entry
        list_for_each(lhd, &urbs) {

            char *cp;
            int count;

            // delete from list, get urb address, dump it and deallocate it
            urb = list_entry_urb(lhd);

            len += sprintf((char *)page+len, "%04lx", urb->serial);
            len += sprintf((char *)page+len, " [");

            cp = urb->buffer;
            count = urb->actual_length;
            while (count--) {
                //printk(KERN_DEBUG"submit: %x %p %p %2x\n", urb->serial, urb, cp, *cp);
                //len += sprintf((char *)page+len, "%02x ", *cp++);
                dohexval((char *)page+len, *cp++);
                len += 2;
                *(((char *)page)+len++) = ' ';
            }

            if (*(((char *)page)+len) == ' ') {
                len--;
            }
            len += sprintf((char *)page+len, "]\n");

            list_del(&urb->urbs);
            usb_dealloc_urb(urb);
            break;

        }
    }

    //read_unlock(&usb_device_rwlock);

    if (len > count) {
        len = -EINVAL;
    }

    else if (len > 0 && copy_to_user(buf, (char *) page, len)) {
        len = -EFAULT;
    }

    free_page(page);
    return len;
}

/**
 * usb_gethex - get a hex value from data buffer
 * @s - pointer to a string pointer
 */
static int __init usb_gethex(char **s, int *max)
{   
    char        c; 
    int         i = 0;
    int         val = 0;

    for(i = 0; max && *max && (i < 2) && s && *s; ) {
        
        c = *(*s)++;
        (*max)--;

        if (c == ' ') {
            continue;
        }

        if (isdigit(c)) {
            val = val*0x10 + c - '0';
        }
        else if ((c >= 'a') && (c <= 'f')) {
            val = val*0x10 + c - 'a' + 10;
        }
        else if ((c >= 'A') && (c <= 'F')) {
            val = val*0x10 + c - 'A' + 10;
        }
        else {
            return val;
        }
        i++;
    }
    return val;
}   


/**
 * usb_bus_proc_write - implement proc file system write.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Standard proc file system write function.
 *
 * We let upper layers iterate for us, *pos will indicate which device to return
 * statistics for.
 */
static          ssize_t
usb_bus_proc_write(struct file *file, const char *buf, size_t count, loff_t * pos)
{
    int len = 0;
    char *cp;
    char *dp;
    void *buffer;
    void *packet;
    unsigned int num;
    unsigned int val;
    unsigned int endpoint;
    struct urb *urb;
    struct usb_bus_instance *bus;
    struct usb_device_instance *device;

    //printk(KERN_DEBUG "usb_bus_proc_write: enter count: %d\n", count);
    if (NULL == (device = device_array[0])) {
        return(0);
    }
    bus = device->bus;

    // sanity checks
    if ((count < 0) || (count > 4000) || ((buffer = kmalloc(count,GFP_KERNEL))==NULL)) {
        return -EINVAL;
    }

    // copy data in from user space
    if (copy_from_user(buffer, buf, count)) {
        kfree(buffer);
        return -EINVAL;
    }

    //for (val = count, dp = buffer; count && *dp; val--, dp++) {
    //    printk(KERN_DEBUG "source: %2x\n", *dp);
    //}

    cp = buffer;
    len = count;

    endpoint = usb_gethex(&cp, &len);

    val = usb_gethex(&cp, &len);

    printk(KERN_DEBUG "-------------> endpoint: %d length: %x\n", endpoint, val);

    if ((val > 4000) || ((packet = kmalloc(val+1,GFP_KERNEL))==NULL)) {
        kfree(buffer);
        return -EINVAL;
    }
    memset(packet, 0, val+1);
    p

    dp = packet;
    num = val;
    for(len=count; num && len; num--) {
        *dp++ = usb_gethex(&cp, &len);
    }

    {
        // allocate an urb
        if ((urb = usb_alloc_urb(bus->device, NULL, endpoint, 0)) == NULL) {
            bi_stall(bus, NULL);
        }

        if (endpoint == 0) {
            urb->pid = USB_PID_SETUP;
            memcpy(&urb->device_request, (char *)packet, sizeof(struct usb_device_request));
            kfree(packet);
        }
        else {
            urb->pid = USB_PID_DATA0;
            urb->endpoint = endpoint;
            urb->buffer = packet;
            urb->buffer_length = val+1;
            urb->actual_length = val;
        }

        // pass urb to upper layers
        if (usb_recv_urb(urb)) {
            bi_stall(bus, urb);         // they didn't like it
        }
        else {
            if (endpoint == 0) {
                bi_result(bus, urb);    // return device request resutls
            }
            else {
                bi_ack(bus, urb);       // return ack for data packet
            }
        }
    }

    //kfree(packet);
    kfree(buffer);
    return count;
}

static struct file_operations usb_device_proc_operations = {
    read:usb_bus_proc_read,
    write:usb_bus_proc_write
};

int usb_proc_init(void)
{
    struct proc_dir_entry *p;

    // create proc filesystem entry
    if ((p = create_proc_entry("usb-bus-test", 0, 0)) == NULL)
        return -ENOMEM;
    p->proc_fops = &usb_device_proc_operations;
    return 0;
}

void usb_proc_term(void)
{
    // remove proc filesystem entry
    remove_proc_entry("usb-bus-test", NULL);
    
}
