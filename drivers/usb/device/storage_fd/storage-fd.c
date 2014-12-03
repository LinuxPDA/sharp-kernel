/*
 * linux/drivers/usb/device/storage_fd/storage-fd.c - mass storage function driver
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * Written by Shunnosuke kabata
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
 * Based on
 *
 * linux/drivers/usbd/net_fd/net-fd.c - network function driver
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
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

/******************************************************************************
** Include File
******************************************************************************/
#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-export.h"
#include "../usbd-build.h"
#include "../usbd-module.h"

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <net/arp.h>

#include <linux/autoconf.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "../usbd-arch.h"
#include "../hotplug.h"

#include "schedule_task.h"
#include "storageproto.h"

/******************************************************************************
** Macro Define
******************************************************************************/

/**************************************
** Module Information
**************************************/

MODULE_AUTHOR("Shunnosuke kabata");
MODULE_DESCRIPTION("USB Device Mass Storage Function");
USBD_MODULE_INFO("storage_fd 0.1");

/**************************************
** Configration Check
**************************************/

#if !defined (CONFIG_USBD_VENDORID) && !defined(CONFIG_USBD_STORAGE_VENDORID)
#error No Vendor ID
#endif
#if !defined (CONFIG_USBD_PRODUCTID) && !defined(CONFIG_USBD_STORAGE_PRODUCTID)
#error No Product ID
#endif

#if defined(CONFIG_USBD_STORAGE_VENDORID) && (CONFIG_USBD_STORAGE_VENDORID > 0)
#undef CONFIG_USBD_VENDORID
#define CONFIG_USBD_VENDORID    CONFIG_USBD_STORAGE_VENDORID
#endif

#if defined(CONFIG_USBD_STORAGE_PRODUCTID) && (CONFIG_USBD_STORAGE_PRODUCTID > 0)
#undef CONFIG_USBD_PRODUCTID
#define CONFIG_USBD_PRODUCTID   CONFIG_USBD_STORAGE_PRODUCTID
#endif

#ifndef CONFIG_USBD_MAXPOWER
#define CONFIG_USBD_MAXPOWER            0
#endif

#ifndef CONFIG_USBD_MANUFACTURER
#define CONFIG_USBD_MANUFACTURER        "Sharp"
#endif

#define MAXTRANSFER                     (512)

#ifndef CONFIG_USBD_VENDORID
#error "CONFIG_USBD_VENDORID not defined"
#endif

#ifndef CONFIG_USBD_PRODUCTID
#error "CONFIG_USBD_PRODUCTID not defined"
#endif

#ifndef CONFIG_USBD_PRODUCT_NAME
#define CONFIG_USBD_PRODUCT_NAME        "Linux Mass Storage Driver"
#endif

#ifndef CONFIG_USBD_SERIAL_NUMBER_STR
#define CONFIG_USBD_SERIAL_NUMBER_STR   ""
#endif

/*
 * USB 2.0 spec does not mention it, but MaxPower is expected to be at least one 
 * and is tested for in USB configuration tests.
 */

#ifdef CONFIG_USBD_SELFPOWERED
#define BMATTRIBUTE BMATTRIBUTE_RESERVED | BMATTRIBUTE_SELF_POWERED
#define BMAXPOWER   1
#else
#define BMATTRIBUTE BMATTRIBUTE_RESERVED
#define BMAXPOWER   CONFIG_USBD_MAXPOWER
#endif

/*
 * setup some default values for pktsizes and endpoint addresses.
 */

#ifndef CONFIG_USBD_STORAGE_OUT_PKTSIZE
#define CONFIG_USBD_STORAGE_OUT_PKTSIZE     64
#endif

#ifndef CONFIG_USBD_STORAGE_IN_PKTSIZE
#define CONFIG_USBD_STORAGE_IN_PKTSIZE      64
#endif

#ifndef CONFIG_USBD_STORAGE_INT_PKTSIZE
#define CONFIG_USBD_STORAGE_INT_PKTSIZE     16
#endif

#ifndef CONFIG_USBD_STORAGE_OUT_ENDPOINT
#define CONFIG_USBD_STORAGE_OUT_ENDPOINT    1
#endif

#ifndef CONFIG_USBD_STORAGE_IN_ENDPOINT
#define CONFIG_USBD_STORAGE_IN_ENDPOINT     2
#endif

#ifndef CONFIG_USBD_STORAGE_INT_ENDPOINT
#define CONFIG_USBD_STORAGE_INT_ENDPOINT    3
#endif

/*
 * check for architecture specific endpoint configurations
 */

#if defined(ABS_OUT_ADDR)
    //#warning
    //#warning USING ABS ENDPOINT OUT ADDRESS
    #undef CONFIG_USBD_STORAGE_OUT_ENDPOINT

    #if ABS_OUT_ADDR > 0
        #define CONFIG_USBD_STORAGE_OUT_ENDPOINT    ABS_OUT_ADDR
    #endif

#elif defined(MAX_OUT_ADDR) && defined(CONFIG_USBD_STORAGE_OUT_ENDPOINT) && (CONFIG_USBD_STORAGE_OUT_ENDPOINT > MAX_OUT_ADDR)
    //#warning
    //#warning USING DEFAULT ENDPOINT OUT ADDRESS
    #undef CONFIG_USBD_STORAGE_OUT_ENDPOINT
    #define CONFIG_USBD_STORAGE_OUT_ENDPOINT        DFL_OUT_ADDR

#endif  /* elif */

#if defined(ABS_IN_ADDR)
    //#warning
    //#warning USING ABS ENDPOINT IN ADDRESS
    #undef CONFIG_USBD_STORAGE_IN_ENDPOINT

    #if ABS_IN_ADDR > 0
        #define CONFIG_USBD_STORAGE_IN_ENDPOINT     ABS_IN_ADDR
    #endif

#elif defined(MAX_IN_ADDR) && defined(CONFIG_USBD_STORAGE_IN_ENDPOINT) && (CONFIG_USBD_STORAGE_IN_ENDPOINT > MAX_IN_ADDR)
    //#warning
    //#warning USING DEFAULT ENDPOINT IN ADDRESS
    #undef CONFIG_USBD_STORAGE_IN_ENDPOINT
    #define CONFIG_USBD_STORAGE_IN_ENDPOINT         DFL_IN_ADDR

#endif  /* elif */

#if defined(ABS_INT_ADDR)
    //#warning
    //#warning USING ABS ENDPOINT INT ADDRESS
    #undef CONFIG_USBD_STORAGE_INT_ENDPOINT

    #if ABS_INT_ADDR
        #define CONFIG_USBD_STORAGE_INT_ENDPOINT    ABS_INT_ADDR
    #endif

#elif defined(MAX_INT_ADDR) && defined(CONFIG_USBD_STORAGE_INT_ENDPOINT) && (CONFIG_USBD_STORAGE_INT_ENDPOINT > MAX_INT_ADDR)
    //#warning
    //#warning USING DEFAULT ENDPOINT INT ADDRESS
    #undef CONFIG_USBD_STORAGE_INT_ENDPOINT
    #define CONFIG_USBD_STORAGE_INT_ENDPOINT        DFL_INT_ADDR

#endif  /* elif */

#if defined(MAX_OUT_PKTSIZE) && defined(CONFIG_USBD_STORAGE_OUT_PKTSIZE) && CONFIG_USBD_STORAGE_OUT_PKTSIZE > MAX_OUT_PKTSIZE
    //#warning
    //#warning OVERIDING ENDPOINT OUT PKTSIZE 
    #undef CONFIG_USBD_STORAGE_OUT_PKTSIZE
    #define CONFIG_USBD_STORAGE_OUT_PKTSIZE         MAX_OUT_PKTSIZE
#endif

#if defined(MAX_IN_PKTSIZE) && defined(CONFIG_USBD_STORAGE_IN_PKTSIZE) && CONFIG_USBD_STORAGE_IN_PKTSIZE > MAX_IN_PKTSIZE
    //#warning
    //#warning OVERIDING ENDPOINT IN PKTSIZE 
    #undef CONFIG_USBD_STORAGE_IN_PKTSIZE
    #define CONFIG_USBD_STORAGE_IN_PKTSIZE          MAX_IN_PKTSIZE
#endif

#if defined(MAX_INT_PKTSIZE) && defined(CONFIG_USBD_STORAGE_INT_PKTSIZE) && CONFIG_USBD_STORAGE_INT_PKTSIZE > MAX_INT_PKTSIZE
    //#warning
    //#warning OVERIDING ENDPOINT INT PKTSIZE
    #undef CONFIG_USBD_STORAGE_INT_PKTSIZE
    #define CONFIG_USBD_STORAGE_INT_PKTSIZE         MAX_INT_PKTSIZE
#endif

/******************************************************************************
** Variable Declaration
******************************************************************************/

/**************************************
** Module Parameters
**************************************/

static u32  vendor_id;
static u32  product_id;
static int  out_pkt_sz = CONFIG_USBD_STORAGE_OUT_PKTSIZE;
static int  in_pkt_sz  = CONFIG_USBD_STORAGE_IN_PKTSIZE;

MODULE_PARM(vendor_id, "i");
MODULE_PARM(product_id, "i");
MODULE_PARM(out_pkt_sz, "i");
MODULE_PARM(in_pkt_sz, "i");

MODULE_PARM_DESC(vendor_id, "vendor id");
MODULE_PARM_DESC(product_id, "product id");

/**************************************
** Mass Storage Configuration
**************************************/

/*
 * Data Interface Alternate 1 endpoints
 */
static __initdata struct usb_endpoint_description StorageAlt1Endpoints[] = {
    {
        bEndpointAddress:CONFIG_USBD_STORAGE_OUT_ENDPOINT,
        bmAttributes:BULK,
        wMaxPacketSize:CONFIG_USBD_STORAGE_OUT_PKTSIZE,
        bInterval:0,
        direction:OUT,
        transferSize:MAXTRANSFER,
    },

    {
        bEndpointAddress:CONFIG_USBD_STORAGE_IN_ENDPOINT,
        bmAttributes:BULK,
        wMaxPacketSize:CONFIG_USBD_STORAGE_IN_PKTSIZE,
        bInterval:0,
        direction:IN,
        transferSize:MAXTRANSFER,
    },

#if defined(CONFIG_USBD_STORAGE_INT_ENDPOINT) && (CONFIG_USBD_STORAGE_INT_ENDPOINT > 0)
    {
        bEndpointAddress:CONFIG_USBD_STORAGE_INT_ENDPOINT,
        bmAttributes:INTERRUPT,
        wMaxPacketSize:CONFIG_USBD_STORAGE_INT_PKTSIZE,
        bInterval:10,
        direction:IN,
        transferSize:CONFIG_USBD_STORAGE_INT_PKTSIZE,
    },
#endif
};


/*
 * Data Interface Alternate description(s)
 */
static __initdata struct usb_alternate_description StorageAlternateDescriptions[] = {
    {
        #if defined(CONFIG_USBD_STORAGE_NO_STRINGS)
        iInterface:"",
        #else
        iInterface:"Mass Storage Interface",
        #endif
        bAlternateSetting:0,
        classes:0,
        class_list:NULL,
        endpoints:sizeof (StorageAlt1Endpoints) / sizeof (struct usb_endpoint_description),
        endpoint_list:StorageAlt1Endpoints,
    },
};

/*
 * Interface description(s)
 */
static __initdata struct usb_interface_description StorageInterfaces[] = {
    {
        #if defined(CONFIG_USBD_STORAGE_NO_STRINGS)
        iInterface:"",
        #else
        iInterface:"Mass Storage Interface",
        #endif
        bInterfaceClass:MASS_STORAGE_CLASS,
        bInterfaceSubClass:MASS_STORAGE_SUBCLASS_SCSI,
        bInterfaceProtocol:MASS_STORAGE_PROTO_BULK_ONLY,
        alternates:sizeof (StorageAlternateDescriptions) / sizeof (struct usb_alternate_description),
        alternate_list:StorageAlternateDescriptions,
    },
};

/******************************************************************************
** USB Configuration
******************************************************************************/

/*
 * Configuration description(s)
 */
struct __initdata usb_configuration_description StorageDescription[] = {
    {
        #if defined(CONFIG_USBD_STORAGE_NO_STRINGS)
        iConfiguration:"",
        #else
        iConfiguration:"Mass Storage Configuration",
        #endif
        bmAttributes:BMATTRIBUTE,
        bMaxPower:BMAXPOWER,
        interfaces:sizeof (StorageInterfaces) / sizeof (struct usb_interface_description),
        interface_list:StorageInterfaces,
    },
};

/*
 * Device Description
 */
struct __initdata usb_device_description StorageDeviceDescription = {
    bDeviceClass:0,
    bDeviceSubClass:0,  // XXX
    bDeviceProtocol:0,  // XXX
    idVendor:CONFIG_USBD_VENDORID,
    idProduct:CONFIG_USBD_PRODUCTID,
    iManufacturer:CONFIG_USBD_MANUFACTURER,
    iProduct:CONFIG_USBD_PRODUCT_NAME,
    iSerialNumber:CONFIG_USBD_SERIAL_NUMBER_STR,
};

/**************************************
** Other Variable
**************************************/
static int              storage_exit_flag = 0;
static pid_t            storage_pid = 0;
static struct semaphore storage_sem;
struct timer_list       storage_usb_event_tim;


/******************************************************************************
** Global Function
******************************************************************************/

void storage_urb_send(struct usb_device_instance* device, void* buffer,
                      int length)
{
    int         port = 0;
    struct urb* urb;

    if(!(urb = usbd_alloc_urb(device,
                              device->function_instance_array + port,
                              CONFIG_USBD_STORAGE_IN_ENDPOINT,
                              length + 5 + in_pkt_sz))){
        printk(KERN_INFO "storage_fd: usbd_alloc_urb failed. length '%d'.\n", length);
        return;
    }

    if(buffer){
        memcpy(urb->buffer, buffer, length);
    }
    else{
        memset(urb->buffer, 0x00, length);
    }
    urb->actual_length = length;

    if(usbd_send_urb(urb)){
        printk(KERN_INFO "storage_fd: usbd_send_urb failed.\n");
        usbd_dealloc_urb(urb);
        return;
    }

    return;
}

/******************************************************************************
** Local Function
******************************************************************************/

/**************************************
** Called when a USB Device is created or destroyed
**************************************/

static int storage_thread(void *_c)
{
    siginfo_t           info;
    unsigned long       signr;
    char                buff[32];

    /* current status set */
    daemonize();

    /* PID set */
    storage_pid = current->pid;

    /* thread name set */
    sprintf(current->comm, STORAGE_THREAD_NAME);
    (current)->nice = 10;

    /* signal register */
    spin_lock_irq(&current->sigmask_lock);
    siginitsetinv(&current->blocked,
                  sigmask(SIGUSR1) | sigmask(SIGHUP) | sigmask(SIGKILL) |
                  sigmask(SIGSTOP) | sigmask(SIGCONT) | sigmask(SIGTERM) |
                  sigmask(SIGALRM));
    recalc_sigpending(current);
    spin_unlock_irq(&current->sigmask_lock);

    /* media open */
    schedule_task_register(
        (SCHEDULE_TASK_FUNC)storageproto_media_status_check, CONTEXT_STORAGE,
        0, 0, 0, 0);

    /* thread active indicate */
    sprintf(buff, "%d\n", storage_pid);
    hotplug("usbdstorage", buff, "active");

    for(;;){
        set_current_state(TASK_INTERRUPTIBLE);

        if (!signal_pending(current)) {
            schedule();
            continue;
        }

        spin_lock_irq(&current->sigmask_lock);
        signr = dequeue_signal(&current->blocked, &info);
        spin_unlock_irq(&current->sigmask_lock);

        switch(signr) {
        case SIGHUP:
            /* media signal indicate */
            schedule_task_register(
                (SCHEDULE_TASK_FUNC)storageproto_media_status_check, CONTEXT_STORAGE,
                0, 0, 0, 0);

            DBG_STORAGE_FD(KERN_INFO "storage_fd: signal receive 'SIGHUP'.\n");
            break;

        default:
            DBG_STORAGE_FD(KERN_INFO "storage_fd: signal receive '%ld'\n", signr);
            break;
        }

        if(storage_exit_flag) break;
    }
    /* current status set */
    current->state = TASK_RUNNING;

    /* PID clear */
    storage_pid = 0;

    /* thread inactive indicate */
    hotplug("usbdstorage", buff, "inactive");

    up(&storage_sem);

    return 0;
}

static void storage_function_init(struct usb_bus_instance* bus,
                              struct usb_device_instance* device,
                              struct usb_function_driver* function_driver)
{
    /* schedule task init */
    schedule_task_init();

    /* storage protocol initialize */
    storageproto_init();

    /* semaphore init */
    sema_init(&storage_sem, 0);

    /* timer initialize */
    init_timer(&storage_usb_event_tim);

    /* thread create */
    storage_pid = kernel_thread(storage_thread, NULL, 0);

    return;
}

static void storage_function_exit(struct usb_device_instance* device)
{
    /* thread kill */
    storage_exit_flag = 1;
    kill_proc(storage_pid, SIGKILL, 1);
    down(&storage_sem);

    /* delete timer */
    del_timer(&storage_usb_event_tim);

    /* storage protocol exit */
    storageproto_exit();

    /* schedule task delete */
    schedule_task_all_unregister();

    return;
}


/**************************************
** Called to handle USB Events
**************************************/

static void storage_usb_event_delay_timeout(unsigned long param)
{
    /* media signal indicate */
    schedule_task_register(
        (SCHEDULE_TASK_FUNC)storageproto_usb_status_check, (int)param,
        0, 0, 0, 0);

    return;
}

/*
 * storage_event - process a device event
 * @device: usb device 
 * @event: the event that happened
 *
 * Called by the usb device core layer to respond to various USB events.
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 *
 */
void storage_event(struct usb_device_instance* device,
                   usb_device_event_t event, int data)
{
#if 0
    static struct {
        usb_device_event_t  event;
        char*               string;
    } eventAnal[] = {
        {DEVICE_UNKNOWN,            "DEVICE_UNKNOWN"},
        {DEVICE_INIT,               "DEVICE_INIT"},
        {DEVICE_CREATE,             "DEVICE_CREATE"},
        {DEVICE_HUB_CONFIGURED,     "DEVICE_HUB_CONFIGURED"},
        {DEVICE_RESET,              "DEVICE_RESET"},
        {DEVICE_ADDRESS_ASSIGNED,   "DEVICE_ADDRESS_ASSIGNED"},
        {DEVICE_CONFIGURED,         "DEVICE_CONFIGURED"},
        {DEVICE_SET_INTERFACE,      "DEVICE_SET_INTERFACE"},
        {DEVICE_SET_FEATURE,        "DEVICE_SET_FEATURE"},
        {DEVICE_CLEAR_FEATURE,      "DEVICE_CLEAR_FEATURE"},
        {DEVICE_DE_CONFIGURED,      "DEVICE_DE_CONFIGURED"},
        {DEVICE_BUS_INACTIVE,       "DEVICE_BUS_INACTIVE"},
        {DEVICE_BUS_ACTIVITY,       "DEVICE_BUS_ACTIVITY"},
        {DEVICE_POWER_INTERRUPTION, "DEVICE_POWER_INTERRUPTION"},
        {DEVICE_HUB_RESET,          "DEVICE_HUB_RESET"},
        {DEVICE_DESTROY,            "DEVICE_DESTROY"},
        {DEVICE_FUNCTION_PRIVATE,   "DEVICE_FUNCTION_PRIVATE"}
    };
    int i;

    for(i=0; i<(sizeof(eventAnal)/sizeof(eventAnal[0])); i++){
        if(event == eventAnal[i].event){
            DBG_STORAGE_FD(KERN_INFO "storage_fd: event receive '%s'.\n",
                eventAnal[i].string);
            break;
        }
    }
    if(i == (sizeof(eventAnal)/sizeof(eventAnal[0]))){
        DBG_STORAGE_FD(KERN_INFO "storage_fd: unknown event receive.\n");
    }
#endif

    switch(event){
    case DEVICE_ADDRESS_ASSIGNED:
        {
        static int  Is1stCheck = 1;
        if(Is1stCheck){
            Is1stCheck = 0;

            /* delay timer set */
            del_timer(&storage_usb_event_tim);
            storage_usb_event_tim.expires  = jiffies + ((USB_EVENT_DELAY_TIM * HZ) / 1000);
            storage_usb_event_tim.data     = USB_DISCONNECT;
            storage_usb_event_tim.function = storage_usb_event_delay_timeout;
            add_timer(&storage_usb_event_tim);
            break;
        }
        }
    case DEVICE_BUS_ACTIVITY:
        /* delay timer set */
        del_timer(&storage_usb_event_tim);
        storage_usb_event_tim.expires  = jiffies + ((USB_EVENT_DELAY_TIM * HZ) / 1000);
        storage_usb_event_tim.data     = USB_CONNECT;
        storage_usb_event_tim.function = storage_usb_event_delay_timeout;
        add_timer(&storage_usb_event_tim);
        break;

    case DEVICE_BUS_INACTIVE:
        /* delay timer set */
        del_timer(&storage_usb_event_tim);
        storage_usb_event_tim.expires  = jiffies + ((USB_EVENT_DELAY_TIM * HZ) / 1000);
        storage_usb_event_tim.data     = USB_DISCONNECT;
        storage_usb_event_tim.function = storage_usb_event_delay_timeout;
        add_timer(&storage_usb_event_tim);
        break;

    case DEVICE_RESET:
        schedule_task_register(
            (SCHEDULE_TASK_FUNC)storageproto_usb_reset_ind,
            0, 0, 0, 0, 0);
        break;

    default:
        break;
    }

    return;
}

/*
 * storage_recv_setup - called with a control URB 
 * @urb - pointer to struct urb
 *
 * Check if this is a setup packet, process the device request, put results
 * back into the urb and return zero or non-zero to indicate success (DATA)
 * or failure (STALL).
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 *
 */
int storage_recv_setup(struct urb* urb)
{
    return 0;
}

/*
 * storage_recv_urb - called with a received URB 
 * @urb - pointer to struct urb
 *
 * Return non-zero if we failed and urb is still valid (not disposed)
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 *
 */
int storage_recv_urb(struct urb* urb)
{
    int                             port = 0;   // XXX compound device
    struct usb_device_instance*     device;
    struct usb_function_instance*   function;

    if(!urb || !(device = urb->device) ||
       !(function = device->function_instance_array + port)){
        return -EINVAL;
    }

    if(urb->status != RECV_OK){
        return -EINVAL;
    }

    /* URB urb_analysis */
    schedule_task_register(
        (SCHEDULE_TASK_FUNC)storageproto_urb_analysis, (int)urb,
        0, 0, 0, 0);

    return 0;
}

/*
 * storage_urb_sent - called to indicate URB transmit finished
 * @urb: pointer to struct urb
 * @rc: result
 *
 * The usb device core layer will use this to let us know when an URB has
 * been finished with.
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 *
 */
int storage_urb_sent(struct urb* urb, int rc)
{
    int                             port = 0;   // XXX compound device
    struct usb_device_instance*     device;
    struct usb_function_instance*   function;

    if(!urb || !(device = urb->device) ||
       !(function = device->function_instance_array + port)){
        return -EINVAL;
    }

    usbd_dealloc_urb (urb);

    return 0;
}

/**************************************
** Proc file system
**************************************/

static ssize_t storage_proc_read(struct file* file, char* buf, size_t count,
                                 loff_t* pos)
{
    int len = 0, ret;

    if(*pos > 0) return 0;

    if((ret = storageproto_proc_read(file, buf + len, count - len, pos)) < 0){
        return ret;
    }
    len += ret;

    if((ret = schedule_task_proc_read(file, buf + len, count - len, pos)) < 0){
        return ret;
    }
    len += ret;

    return len;
}

static struct file_operations StorageProcOps = {
    read:storage_proc_read,
};

/**************************************
** Module init and exit
**************************************/

struct usb_function_operations StorageFunctionOps = {
    event:storage_event,
    recv_urb:storage_recv_urb,
    recv_setup:storage_recv_setup,
    urb_sent:storage_urb_sent,
    function_init:storage_function_init,
    function_exit:storage_function_exit,
};

struct usb_function_driver StorageFunctionDriver = {
    name:"Mass Storage",
    ops:&StorageFunctionOps,
    device_description:&StorageDeviceDescription,
    configurations:sizeof (StorageDescription) / sizeof (struct usb_configuration_description),
    configuration_description:StorageDescription,
    this_module:THIS_MODULE,
};

/*
 * net_modinit - module init
 *
 */
static int __init net_modinit(void)
{
    struct proc_dir_entry*  proc_entry;

    printk(KERN_INFO "storage_fd: %s (OUT=%d,IN=%d)\n",
        __usbd_module_info, out_pkt_sz, in_pkt_sz);

    printk(KERN_INFO "storage_fd: vendorID: %x productID: %x\n",
        CONFIG_USBD_VENDORID, CONFIG_USBD_PRODUCTID);

    // verify pkt sizes not too small
    if (out_pkt_sz < 3 || in_pkt_sz < 3){
        printk(KERN_INFO "storage_fd: Rx pkt size %d or Tx pkt size %d too small\n",
            out_pkt_sz, in_pkt_sz);
        return (-EINVAL);
    }

    if(vendor_id){
        StorageDeviceDescription.idVendor = vendor_id;
    }
    if(product_id){
        StorageDeviceDescription.idProduct = product_id;
    }

    // register us with the usb device support layer
    if(usbd_register_function(&StorageFunctionDriver)){
        printk(KERN_INFO "storage_fd: usbd_register_function failed.\n");
        return -EINVAL;
    }

    // create proc entry
    if ((proc_entry = create_proc_entry("usb-storage", 0, 0)) == NULL) {
        usbd_deregister_function (&StorageFunctionDriver);
        printk(KERN_INFO "storage_fd: create_proc_entry failed.\n");
        return -ENOMEM;
    }
    proc_entry->proc_fops = &StorageProcOps;

    return 0;
}

/*
 * function_exit - module cleanup
 *
 */
static void __exit net_modexit (void)
{
    // de-register us with the usb device support layer
    usbd_deregister_function (&StorageFunctionDriver);

    // remove proc entry
    remove_proc_entry("usb-storage", NULL);

    return;
}

module_init (net_modinit);
module_exit (net_modexit);
