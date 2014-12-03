/*
 * linux/drivers/usbd/usbd-func.c - USB Function support
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

#include "usbd.h"
#include "usbd-debug.h"
#include "usbd-func.h"
#include "usbd-bus.h"
#include "usbd-inline.h"


#define LANGID_ENGLISH          "\011"
#define LANGID_US_ENGLISH       "\004"

#define LANGIDs  LANGID_ENGLISH 

//#define LANGIDs  LANGID_ENGLISH LANGID_US_ENGLISH LANGID_US_ENGLISH

extern int maxstrings;
extern struct usb_string_descriptor **usb_strings;
extern int usb_devices;
extern struct usb_function_driver ep0_driver;

extern struct list_head function_drivers;
extern struct list_head devices;        

static __inline__ struct usb_function_driver *
list_entry_func(const struct list_head *le)
{
    return list_entry(le, struct usb_function_driver, drivers);
}

static __inline__ struct usb_device_instance *
list_entry_device(const struct list_head *le)
{
    return list_entry(le, struct usb_device_instance, devices);
}

void lkfree (void *p)
{
    if (p) {
        kfree(p);
    }
}

extern int      dbgflg_usbdfd_init;
extern int      dbgflg_usbdfd_usbe;

extern int registered_functions;
extern int registered_devices;


#define dbg_init(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_init,lvl,fmt,##args)
#define dbg_usbe(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_usbe,lvl,fmt,##args)



/* *
 * usbd_alloc_string - allocate a string descriptor and return index number
 * @str: pointer to string
 *
 * Find an empty slot in index string array, create a corresponding descriptor
 * and return the slot number.
 */
__init __u8 usbd_alloc_string(char *str)
{
    int i;
    struct usb_string_descriptor *string;
    __u8 bLength;
    __u16 *wData;

    //return 0;

    if (!str || !strlen(str)) {
        return 0;
    }
    dbg_init(8,"%s", str);

    // find an empty string descriptor slot
    for (i=0;i<maxstrings;i++) {

        if (usb_strings[i] == NULL) {

            dbg_init(1,"%s -> %d", str, i);

            bLength = sizeof(struct usb_string_descriptor) + 2*strlen(str);

            if (!(string = ckmalloc(bLength, GFP_KERNEL))) {
                return 0;
            }

            string->bLength = bLength;
            string->bDescriptorType = USB_DT_STRING;

            for (wData = string->wData;*str;) {
                *wData++ = (__u16)(*str++);
            }

            // store in string index array
            usb_strings[i] = string;

            return i;
        }
    }
    return 0;
}


/* *
 * usbd_dealloc_string - deallocate a string descriptor
 * @index: index into strings array to deallocte
 *
 * Find and remove an allocated string.
 */
void __exit usbd_dealloc_string(__u8 index)
{
    struct usb_string_descriptor *string;

    dbg_init(1,"%d", index);

    if ((index > 0) && (index < maxstrings) && (string = usb_strings[index])) {
        usb_strings[index] = NULL;
        dbg_init(1,"%p", string);
        lkfree(string);
    }
}

__u8 device_descriptor_sizes[] = {
    0,
    sizeof(struct usb_device_descriptor),
    sizeof(struct usb_configuration_descriptor),
    sizeof(struct usb_string_descriptor),
    sizeof(struct usb_interface_descriptor),
    sizeof(struct usb_endpoint_descriptor)
};

__u8 class_descriptor_sizes[] = {
    sizeof(struct usb_class_header_function_descriptor),        // 0x0
    sizeof(struct usb_class_call_management_descriptor),        // 0x1
    sizeof(struct usb_class_abstract_control_descriptor),       // 0x2
    sizeof(struct usb_class_direct_line_descriptor),            // 0x3
    sizeof(struct usb_class_telephone_ringer_descriptor),       // 0x4
    sizeof(struct usb_class_telephone_operational_descriptor),  // 0x5
    sizeof(struct usb_class_union_function_descriptor),         // 0x6
    sizeof(struct usb_class_country_selection_descriptor),      // 0x7
    sizeof(struct usb_class_telephone_operational_descriptor),  // 0x8
    sizeof(struct usb_class_usb_terminal_descriptor),           // 0x9
    sizeof(struct usb_class_network_channel_descriptor),        // 0xa
    sizeof(struct usb_class_protocol_unit_function_descriptor), // 0xb
    sizeof(struct usb_class_extension_unit_descriptor),         // 0xc
    sizeof(struct usb_class_multi_channel_descriptor),          // 0xd
    sizeof(struct usb_class_capi_control_descriptor),           // 0xe
    sizeof(struct usb_class_ethernet_networking_descriptor),    // 0xf
    sizeof(struct usb_class_atm_networking_descriptor)          // 0x10
};


/* *
 * usbd_alloc_descriptor - allocate a usb descriptor
 * bDescriptorType 
 *
 * Allocate and initialize a generic usb descriptor.
 */
__init static void * usbd_alloc_descriptor(__u8 bDescriptorType, __u8 bDescriptorSubtype, __u8 elements)
{
    struct usb_descriptor *descriptor; 
    int length;


    dbg_init(1,"type: %d subtype: %d elements: %d", bDescriptorType, bDescriptorSubtype, elements);

    switch (bDescriptorType) {
    case USB_DT_DEVICE:
    case USB_DT_CONFIG:
    case USB_DT_INTERFACE:
    case USB_DT_ENDPOINT:
        length = device_descriptor_sizes[bDescriptorType];
        break;

    case CS_INTERFACE:
        switch (bDescriptorSubtype) {

        case USB_ST_HEADER:
        case USB_ST_CMF:
        case USB_ST_ACMF:
        case USB_ST_DLMF:
        case USB_ST_TRF:
        case USB_ST_TCLF:

        case USB_ST_NCT:
        case USB_ST_MCMF:
        case USB_ST_CCMF:
        case USB_ST_ENF:
        case USB_ST_ATMNF:
            length = class_descriptor_sizes[bDescriptorSubtype];
            break;

        case USB_ST_UF:
        case USB_ST_CSF:
        case USB_ST_TOMF:
        case USB_ST_USBTF:
        case USB_ST_PUF:
        case USB_ST_EUF:
            length = class_descriptor_sizes[bDescriptorSubtype] + elements; // XXX check me
            break;

        default:
            return NULL;

        }
        break;

    case USB_DT_STRING:
    default:
        return NULL;
    }

    if (!(descriptor = ckmalloc(length, GFP_KERNEL))) {
        return NULL;
    }

    switch (bDescriptorType) {
    case CS_INTERFACE:
        descriptor->descriptor.generic.bDescriptorSubtype = bDescriptorSubtype;

    case USB_DT_DEVICE:
    case USB_DT_CONFIG:
    case USB_DT_INTERFACE:
    case USB_DT_ENDPOINT:
        descriptor->descriptor.generic.bLength = length;
        descriptor->descriptor.generic.bDescriptorType = bDescriptorType;
        break;


    case USB_DT_STRING:
        break;
    }

    dbg_init(2,"descriptor: %p", descriptor);

    return descriptor;
}


/* *
 * usbd_dealloc_descriptor - deallocate a usb descriptor
 * @descriptor: pointer to usb descriptor to deallocate
 *
 * Deallocate a descriptor, first deallocate the index string descriptors.
 */
static __exit void usbd_dealloc_descriptor(struct usb_descriptor *descriptor)
{
    dbg_init(2,"descriptor: %p type: %d", descriptor, descriptor->descriptor.generic.bDescriptorType);
    if (descriptor) {
        switch (descriptor->descriptor.generic.bDescriptorType) {
        case USB_DT_DEVICE:
            dbg_init(2,"USB_DT_DEVICE");
            usbd_dealloc_string(descriptor->descriptor.device.iManufacturer);
            usbd_dealloc_string(descriptor->descriptor.device.iProduct);
            usbd_dealloc_string(descriptor->descriptor.device.iManufacturer);
            break;
        case USB_DT_CONFIG:
            dbg_init(2,"USB_DT_CONFIG");
            usbd_dealloc_string(descriptor->descriptor.configuration.iConfiguration);
            break;
        case USB_DT_INTERFACE:
            dbg_init(2,"USB_DT_INTERFACE");
            usbd_dealloc_string(descriptor->descriptor.interface.iInterface);
            break;
        case CS_INTERFACE:
            dbg_init(2,"class descriptor: %p type: %d subtype: %d", 
                    descriptor, descriptor->descriptor.generic.bDescriptorType,
                    descriptor->descriptor.generic.bDescriptorSubtype
            );
            dbg_init(2,"USB_CS_INTERFACE");
            {
                struct usb_class_descriptor *class_descriptor = (struct usb_class_descriptor *)descriptor;
                switch(descriptor->descriptor.generic.bDescriptorSubtype) {

                case USB_ST_HEADER:
                case USB_ST_CMF:
                case USB_ST_ACMF:
                case USB_ST_DLMF:
                case USB_ST_TRF:
                case USB_ST_TCLF:
                case USB_ST_UF:
                case USB_ST_CSF:
                case USB_ST_TOMF:
                case USB_ST_USBTF:
                case USB_ST_NCT:
                case USB_ST_PUF:
                case USB_ST_EUF:
                case USB_ST_MCMF:
                case USB_ST_CCMF:
                    break;
                case USB_ST_ENF:
                    dbg_init(2,"USB_ST_ENF");
                    usbd_dealloc_string(class_descriptor->descriptor.ethernet_networking.iMACAddress);
                    break;
                case USB_ST_ATMNF:
                    break;
                }
            }
        }
        lkfree(descriptor);
    }
}

/* usb-device USB FUNCTION generic functions ************************************************* */


/* *
* alloc_function_classes - allocate class descriptor array
* @classes: number of classes
* @class_description_array: pointer to an array of class descriptions
*
 * Return a pointer to an array of pointers to class descriptors. The descriptors
 * will be filled in with information from the class description array.
 * 
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_class_descriptor **
alloc_function_classes(int classes, struct usb_class_description *class_description_array)
{
    int class;
    struct usb_class_descriptor **classes_descriptor_array;

    dbg_init(1,"classes: %d", classes);

    if (!classes || !class_description_array) {
        return NULL;
    }

    // allocate the class descriptor array
    if (!(classes_descriptor_array = ckmalloc(sizeof(struct usb_class_descriptor *)*classes, GFP_KERNEL))) {
        return NULL;
    }
    for (class = 0; class < classes; class++) {
        struct usb_class_description *class_description = class_description_array + class;
        struct usb_class_descriptor *class_descriptor;
        //int elements;

        dbg_init(2,"class: %d Subtype: %d elements: %d", 
                class, class_description->bDescriptorSubtype, class_description->elements);

        // allocate an class descriptor to use
        if (!(class_descriptor = usbd_alloc_descriptor(CS_INTERFACE, 
                        class_description->bDescriptorSubtype, class_description->elements))) 
        {
            dbg_init(0, "usbd_alloc_descriptor failed");
            return NULL;
        }
        switch(class_description->bDescriptorSubtype) {

        case USB_ST_HEADER:
            class_descriptor->descriptor.header_function.bcdCDC = cpu_to_le16(CLASS_BCD_VERSION);
            break;
        case USB_ST_CMF:
        case USB_ST_ACMF:
            class_descriptor->descriptor.abstract_control.bmCapabilities = 
                class_description->description.abstract_control.bmCapabilities;
            break;
        case USB_ST_DLMF:
        case USB_ST_TRF:
        case USB_ST_TCLF:
            // XXX generalize USB_ST_UF here
            dbg_init(0, "NOT IMPLEMENTED");
            break;

        case USB_ST_UF:

            dbg_init(1, "USB_ST_UF: bMasterInterface: %02x", class_description->description.union_function.bMasterInterface);
            dbg_init(1, "USB_ST_UF: bSlaveInterface[0]: %02x", class_description->description.union_function.bSlaveInterface[0]);

            class_descriptor->descriptor.union_function.bMasterInterface = 
                class_description->description.union_function.bMasterInterface;

            class_descriptor->descriptor.union_function.bSlaveInterface0[0] = 
                class_description->description.union_function.bSlaveInterface[0];
#if 0
            for (elements = 0; elements < class_description->elements; elements++) {

                dbg_init(1, "USB_ST_UF: copying bSlaveInterface[%d] %02x", elements, 
                    class_description->description.union_function.bSlaveInterface[elements]);

                class_descriptor->descriptor.union_function.bSlaveInterface0[elements] = 
                    class_description->description.union_function.bSlaveInterface[elements];
            }
#endif
            break;
        case USB_ST_CSF:
        case USB_ST_TOMF:
        case USB_ST_USBTF:
        case USB_ST_NCT:
        case USB_ST_PUF:
        case USB_ST_EUF:
        case USB_ST_MCMF:
        case USB_ST_CCMF:
        case USB_ST_ENF:
            class_descriptor->descriptor.ethernet_networking.bmEthernetStatistics = 
                class_description->description.ethernet_networking.bmEthernetStatistics;

            class_descriptor->descriptor.ethernet_networking.iMACAddress = 
                usbd_alloc_string(class_description->description.ethernet_networking.iMACAddress);

            class_descriptor->descriptor.ethernet_networking.wMaxSegmentSize = 
                cpu_to_le16(class_description->description.ethernet_networking.wMaxSegmentSize);

            class_descriptor->descriptor.ethernet_networking.wNumberMCFilters = 
                cpu_to_le16(class_description->description.ethernet_networking.wNumberMCFilters);

            class_descriptor->descriptor.ethernet_networking.bNumberPowerFilters = 
                class_description->description.ethernet_networking.bNumberPowerFilters;

            break;
        case USB_ST_ATMNF:
            break;
        }

        // save in class descriptor array
        classes_descriptor_array[class] = class_descriptor;
    }
    return classes_descriptor_array;
}


/* *
 * alloc_function_endpoints - allocate endpoint descriptor array
 * @endpoints: number of endpoints
 * @endpoint_description_array: pointer to an array of endpoint descriptions
 *
 * Return a pointer to an array of pointers to endpoint descriptors. The descriptors
 * will be filled in with information from the endpoint description array.
 * 
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_endpoint_descriptor **
alloc_function_endpoints(int endpoints, struct usb_endpoint_description *endpoint_description_array)
{
    int i;
    struct usb_endpoint_descriptor **endpoint_descriptor_array;


    if (!endpoints || !endpoint_description_array) {
        return NULL;
    }

    // allocate the endpoint descriptor array
    if (!(endpoint_descriptor_array = ckmalloc(sizeof(struct usb_endpoint_descriptor *)*endpoints, GFP_KERNEL))) {
        return NULL;
    }
    for (i=0;i<endpoints;i++) {
        struct usb_endpoint_description *endpoint_description = endpoint_description_array+i;
        struct usb_endpoint_descriptor *endpoint_descriptor;

        dbg_init(1,"endpoint: %d:%d", i, endpoints);

        // allocate an endpoint descriptor to use
        if (!(endpoint_descriptor = usbd_alloc_descriptor(USB_DT_ENDPOINT, 0, 0))) {
            return NULL;
        }
        endpoint_descriptor->bEndpointAddress = endpoint_description->bEndpointAddress | endpoint_description->direction;
        endpoint_descriptor->bmAttributes = endpoint_description->bmAttributes;
        // XXX cpu_to_le16()?
        endpoint_descriptor->wMaxPacketSize = endpoint_description->wMaxPacketSize;
        endpoint_descriptor->bInterval = endpoint_description->bInterval;

        // save in endpoint descriptor array
        endpoint_descriptor_array[i] = endpoint_descriptor;
    }
    return endpoint_descriptor_array;
}


/* *
 * alloc_endpoint_transfer - allocate endpoint transfer array
 * @endpoints: number of endpoints
 * @endpoint_description_array: pointer to an array of endpoint descriptions
 *
 * Return a pointer to an array of transfers sizes for each endpointi.
 * 
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static int *
alloc_endpoint_transfer(int endpoints, struct usb_endpoint_description *endpoint_description_array)
{
    int i;
    int *endpoint_transfersize;

    if (!endpoints || !endpoint_description_array) {
        return NULL;
    }

    // allocate the endpoint descriptor array
    if (!(endpoint_transfersize = ckmalloc(sizeof(int)*endpoints, GFP_KERNEL))) {
        return NULL;
    }
    for (i=0;i<endpoints;i++) {
        struct usb_endpoint_description *endpoint_description = endpoint_description_array+i;
        dbg_init(1,"endpoint: %d:%d", i, endpoints);
        endpoint_transfersize[i] = endpoint_description->transferSize;
    }
    return endpoint_transfersize;
}

#if 0
/* *
 * alloc_function_interface - allocate alternate instance array
 *
 * Return a pointer to an array of alternate instances. Each instance contains a pointer
 * to a filled in alternate descriptor and a pointer to the endpoint descriptor array.
 *
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_alternate_instance *
alloc_function_interface (
        int bInterfaceNumber,
        struct usb_interface_description *interface_description)
{
    struct usb_alternate_instance *alternate_instance;
    struct usb_interface_descriptor *interface_descriptor;

    dbg_init(1,"interface: %d", bInterfaceNumber);

    // allocate array of alternate instances
    if (!(alternate_instance = ckmalloc(sizeof(struct usb_alternate_instance), GFP_KERNEL))) {
        dbg_init(0,"ckmalloc failed");
        return NULL;
    }

    // allocate a descriptor for this interface
    if (!(interface_descriptor = usbd_alloc_descriptor(USB_DT_INTERFACE, 0, 0))) {
        dbg_init(0,"usbd_alloc_descriptor failed");
        return NULL;
    }

    interface_descriptor->bInterfaceNumber = bInterfaceNumber + 1; // XXX
    interface_descriptor->bInterfaceClass = interface_description->bInterfaceClass;

    interface_descriptor->bInterfaceSubClass = interface_description->bInterfaceSubClass;
    interface_descriptor->bInterfaceProtocol = interface_description->bInterfaceProtocol;

    interface_descriptor->bAlternateSetting = 0;
    interface_descriptor->iInterface = usbd_alloc_string(interface_description->iInterface);
    
    // XXX it should be possible to collapse the next two functions into one.
    alternate_instance->endpoints_descriptor_array = NULL;
    alternate_instance->endpoint_transfersize_array = NULL;

    // save number of alternates, classes and endpoints for this alternate
    alternate_instance->endpoints = 0;
    alternate_instance->interface_descriptor = interface_descriptor;
    return alternate_instance;
}
#endif

/* *
 * alloc_function_alternates - allocate alternate instance array
 * @alternates: number of alternates
 * @alternate_description_array: pointer to an array of alternate descriptions
 *
 * Return a pointer to an array of alternate instances. Each instance contains a pointer
 * to a filled in alternate descriptor and a pointer to the endpoint descriptor array.
 *
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_alternate_instance *
alloc_function_alternates (
        int bInterfaceNumber,
        struct usb_interface_description *interface_description,
        int alternates, 
        struct usb_alternate_description *alternate_description_array)
{
    int i;
    struct usb_alternate_instance *alternate_instance_array;

    dbg_init(1,"bInterfaceNumber: %d, alternates: %d", bInterfaceNumber, alternates);

    // allocate array of alternate instances
    if (!(alternate_instance_array = ckmalloc(sizeof(struct usb_alternate_instance) * alternates, GFP_KERNEL))) {
        return NULL;
    }
    
    // iterate across the alternate descriptions
    for (i=0;i<alternates;i++) {
        
        struct usb_alternate_description *alternate_description = alternate_description_array+i;
        struct usb_alternate_instance *alternate_instance = alternate_instance_array + i;
        struct usb_interface_descriptor *interface_descriptor;

        dbg_init(1,"alternate: %d:%d", i, alternates);

        // allocate a descriptor for this interface
        if (!(interface_descriptor = usbd_alloc_descriptor(USB_DT_INTERFACE, 0, 0))) {
            return NULL;
        }

        interface_descriptor->bInterfaceNumber = bInterfaceNumber; // XXX
        interface_descriptor->bInterfaceClass = interface_description->bInterfaceClass;

        interface_descriptor->bInterfaceSubClass = interface_description->bInterfaceSubClass;
        interface_descriptor->bInterfaceProtocol = interface_description->bInterfaceProtocol;

        interface_descriptor->bAlternateSetting = alternate_description->bAlternateSetting;
        interface_descriptor->bNumEndpoints = alternate_description->endpoints;
        interface_descriptor->iInterface = usbd_alloc_string(alternate_description->iInterface? 
                alternate_description->iInterface:interface_description->iInterface);
        
        // XXX Classes
        
       alternate_instance->classes_descriptor_array = 
                    alloc_function_classes( 
                            alternate_description->classes, 
                            alternate_description->class_list);

        alternate_instance->classes = alternate_description->classes;

        // XXX it should be possible to collapse the next two functions into one.
        alternate_instance->endpoints_descriptor_array = 
                    alloc_function_endpoints( alternate_description->endpoints, alternate_description->endpoint_list);
        
        alternate_instance->endpoint_transfersize_array = 
                    alloc_endpoint_transfer( alternate_description->endpoints, alternate_description->endpoint_list);


        // save number of alternates, classes and endpoints for this alternate
        alternate_instance->endpoints = alternate_description->endpoints;
        alternate_instance->interface_descriptor = interface_descriptor;
    }
    return alternate_instance_array;
}

/* *
 * alloc_function_interfaces - allocate interface instance array
 * @interfaces: number of interfaces
 * @interface_description_array: pointer to an array of interface descriptions
 *
 * Return a pointer to an array of interface instances. Each instance contains a pointer
 * to a filled in interface descriptor and a pointer to the endpoint descriptor array.
 *
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_interface_instance *
alloc_function_interfaces (
        int interfaces, 
        struct usb_interface_description *interface_description_array)
{
    int interface;
    struct usb_interface_instance *interface_instance_array;

    // allocate array of interface instances
    if (!(interface_instance_array = ckmalloc(sizeof(struct usb_interface_instance) * interfaces, GFP_KERNEL))) {
        return NULL;
    }
    
    // iterate across the interface descriptions
    for (interface = 0; interface < interfaces; interface++) {

        struct usb_interface_description *interface_description = interface_description_array + interface;
        struct usb_interface_instance *interface_instance = interface_instance_array + interface;

        dbg_init(1,"_");
        dbg_init(1,"interface: %d:%d   alternates: %d", interface, interfaces, interface_description->alternates);
        
        // save the interface description in the interface instance array
        //interface_instance_array[interface].interface_descriptor = interface_descriptor;

        interface_instance->alternates_instance_array = 
            alloc_function_alternates(interface, 
                    interface_description,
                    interface_description->alternates, 
                    interface_description->alternate_list );
        interface_instance->alternates = interface_description->alternates;

    }
    return interface_instance_array;
}


/* *
 * alloc_function_configurations - allocate configuration instance array
 * @configurations: number of configurations
 * @configuration_description_array: pointer to an array of configuration descriptions
 *
 * Return a pointer to an array of configuration instances. Each instance contains a pointer
 * to a filled in configuration descriptor and a pointer to the interface instances array.
 *
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_configuration_instance * 
alloc_function_configurations ( 
        int configurations, 
        struct usb_configuration_description *configuration_description_array)
{
    int i;
    struct usb_configuration_instance *configuration_instance_array;


    // allocate array
    if (!(configuration_instance_array = ckmalloc(sizeof(struct usb_configuration_instance) * configurations, GFP_KERNEL)) )
    {
        return NULL;
    }

    // fill in array
    for (i = 0; i < configurations; i++) {
        int j;
        int length;

        struct usb_configuration_description *configuration_description = configuration_description_array+i;
        struct usb_configuration_descriptor *configuration_descriptor;

        dbg_init(1,"configurations: %d:%d", i, configurations);

        // allocate a descriptor
        if (!(configuration_descriptor = usbd_alloc_descriptor(USB_DT_CONFIG, 0, 0))) {
            return NULL;
        }

        // setup fields in configuration descriptor
        // XXX c.f. 9.4.7 zero is default, so config MUST BE 1 to n, not 0 to n-1
        // XXX N.B. the configuration itself is fetched 0 to n-1.
        //
        configuration_descriptor->bConfigurationValue = i + 1; 
        configuration_descriptor->wTotalLength = 0;
        configuration_descriptor->bNumInterfaces = configuration_description->interfaces;
        configuration_descriptor->iConfiguration = usbd_alloc_string(configuration_description->iConfiguration);
        configuration_descriptor->bmAttributes = configuration_description->bmAttributes;
        configuration_descriptor->bMaxPower = configuration_description->bMaxPower;

        // save the configuration descriptor in the configuration instance array
        configuration_instance_array[i].configuration_descriptor = configuration_descriptor;

        if (!( configuration_instance_array[i].interface_instance_array = 
                    alloc_function_interfaces(configuration_description->interfaces, configuration_description->interface_list))
                ) 
        {
            // XXX
            return NULL;
        }

        for (length = 0, j = 0; j < configuration_descriptor->bNumInterfaces;j++) {
            int alternate;
            struct usb_interface_instance *interface_instance = configuration_instance_array[i].interface_instance_array + j;


            for (alternate = 0; alternate < interface_instance->alternates; alternate++ ) {
                int class;
                struct usb_alternate_instance *alternate_instance = interface_instance->alternates_instance_array + alternate;

                for (class = 0; class < alternate_instance->classes; class++) {
                    struct usb_class_descriptor *class_descriptor = 
                        alternate_instance->classes_descriptor_array[class];

                    length += class_descriptor->descriptor.generic.bFunctionLength;
                }

                length += 
                    sizeof(struct usb_interface_descriptor) +
                    alternate_instance->endpoints * 
                    sizeof(struct usb_endpoint_descriptor);
            }
        }
        configuration_descriptor->wTotalLength = cpu_to_le16(sizeof(struct usb_configuration_descriptor) + length); 

        dbg_init(1, "-------> [%d] %d %d %d length: %d", i, 
                configuration_descriptor->bNumInterfaces,
                configuration_descriptor->bConfigurationValue,
                configuration_descriptor->iConfiguration,
                length
              );
        configuration_instance_array[i].interfaces = configuration_description->interfaces;

    }
    return configuration_instance_array;
}


/* *
 * usbd_dealloc_function - deallocate a function driver
 * @function_driver: pointer to a function driver structure
 *
 * Find and free all descriptor structures associated with a function structure.
 */
static void __exit usbd_dealloc_function(struct usb_function_driver *function_driver)
{
    int configuration;
    struct usb_configuration_instance *configuration_instance_array =
        function_driver->configuration_instance_array;

    dbg_init(1,"%s", function_driver->name);

    // iterate across the descriptors list and de-allocate all structures
    if (function_driver->configuration_instance_array ) {
        for (configuration=0; configuration < function_driver->configurations;configuration++) {
            int interface;
            struct usb_configuration_instance *configuration_instance = configuration_instance_array + configuration;

            dbg_init(1,"%s configuration: %d", function_driver->name, configuration);

            for (interface=0;interface< configuration_instance->interfaces;interface++) {

                int alternate;
                struct usb_interface_instance *interface_instance = 
                    configuration_instance->interface_instance_array + interface;

                dbg_init(1,"%s configuration: %d interface: %d", function_driver->name, configuration, interface);

                for (alternate = 0; alternate < interface_instance->alternates; alternate++) {
                    int class;
                    int endpoint;
                    struct usb_alternate_instance *alternate_instance =
                        interface_instance->alternates_instance_array + alternate;

                    dbg_init(1,"%s configuration: %d interface: %d alternate: %d", 
                            function_driver->name, configuration, interface, alternate);

                    for (class = 0; class < alternate_instance->classes; class++) {

                        dbg_init(1,"%s configuration: %d interface: %d alternate: %d class: %d", 
                                function_driver->name, configuration, interface, alternate, class);

                        usbd_dealloc_descriptor((struct usb_descriptor *)
                                alternate_instance->classes_descriptor_array[class]);
                    }
                    for (endpoint=0;endpoint< alternate_instance->endpoints;endpoint++) {

                        dbg_init(1,"%s configuration: %d interface: %d alternate: %d endpoint: %d", 
                                function_driver->name, configuration, interface, alternate, endpoint);

                        usbd_dealloc_descriptor((struct usb_descriptor *)
                                alternate_instance->endpoints_descriptor_array[endpoint]);
                    }
                    lkfree(alternate_instance->endpoints_descriptor_array);
                    lkfree(alternate_instance->endpoint_transfersize_array);
                    lkfree(alternate_instance->classes_descriptor_array);
                    usbd_dealloc_descriptor((struct usb_descriptor *)alternate_instance->interface_descriptor);
                }

                lkfree(interface_instance->alternates_instance_array);
            }

            lkfree(configuration_instance->interface_instance_array);
            usbd_dealloc_descriptor((struct usb_descriptor *)
                    configuration_instance_array[configuration].configuration_descriptor);
        }
    }
    usbd_dealloc_descriptor((struct usb_descriptor *)function_driver->device_descriptor);
    lkfree(configuration_instance_array);
}
    
__init static int usbd_strings_init(void)
{
    if (maxstrings > 254) {
        dbg_init(1, "setting maxstrings to maximum value of 254");
        maxstrings = 254;
    }
    if (!(usb_strings = ckmalloc(sizeof(struct usb_string_descriptor *)*maxstrings, GFP_KERNEL))) {
        return -1;
    }
    if (usbd_alloc_string(LANGIDs)!=0) {
        lkfree(usb_strings);
        return -1;
    }
    return 0;
}



/* *
 * usbd_register_function - register a usb function driver
 * @function_driver: pointer to function driver structure
 *
 * Used by a USB Function driver to register itself with the usb device layer.
 *
 * It will create a usb_function_instance structure.
 *
 * The user friendly configuration/interface/endpoint descriptions are compiled into
 * the equivalent ready to use descriptor records.
 *
 * All function drivers must register before any bus interface drivers.
 *
 */
__init int usbd_register_function(struct usb_function_driver *function_driver)
{
    //struct usb_device_descriptor *device_descriptor;

    /* Create a function driver structure, copy the configuration,
     * interface and endpoint descriptions into descriptors, add to the
     * function drivers list.
     */

    dbg_init(1, "--");

    if (registered_devices) {
        dbg_init(1, "cannot register after device created: %s", function_driver->name);
        return -EINVAL;
    }

    // initialize the strings pool
    if (usbd_strings_init()) {
        dbg_init(0, "usbd_strings_init failed");
        return -EINVAL;
    }


    dbg_init(9,"USE_COUNT %d",GET_USE_COUNT(THIS_MODULE));
    list_add_tail(&function_driver->drivers, &function_drivers);
    registered_functions++;
    dbg_init(1,"registered_functions: %d", registered_functions);
    dbg_init(1, "--------------");

    return 0;
}


/* *
 * usbd_deregister_function - called by a USB FUNCTION driver to deregister itself
 * @function_driver: pointer to struct usb_function_driver
 *
 * Called by a USB Function driver De-register a usb function driver.
 */
void __exit usbd_deregister_function(struct usb_function_driver *function_driver)
{
    dbg_init(1, "%s", function_driver->name);

    list_del(&function_driver->drivers);
    registered_functions--;

    //usbd_dealloc_function(function_driver);

    dbg_init(9,"USE_COUNT %d",GET_USE_COUNT(THIS_MODULE));
}




void usbd_function_init(
        struct usb_bus_instance *bus, 
        struct usb_device_instance *device, 
        struct usb_function_driver *function_driver)
{
    struct usb_device_descriptor *device_descriptor;

    dbg_init(1, "-");

    if (function_driver->ops->function_init) {
        function_driver->ops->function_init(bus, device, function_driver);
    }

    if (!(device_descriptor = usbd_alloc_descriptor(USB_DT_DEVICE, 0, 0))) {
        return;
    }

    device_descriptor->bcdUSB = USB_BCD_VERSION;
    device_descriptor->bDeviceClass = function_driver->device_description->bDeviceClass;
    device_descriptor->bDeviceSubClass = function_driver->device_description->bDeviceSubClass;
    device_descriptor->bDeviceProtocol = function_driver->device_description->bDeviceProtocol;

    device_descriptor->bMaxPacketSize0 = 0;             // this will be set in ep0

    device_descriptor->idVendor = function_driver->device_description->idVendor;
    device_descriptor->idProduct = function_driver->device_description->idProduct;

    device_descriptor->bcdDevice = 0;                   // XXX

    device_descriptor->iManufacturer = usbd_alloc_string(function_driver->device_description->iManufacturer);
    device_descriptor->iProduct = usbd_alloc_string(function_driver->device_description->iProduct);

#ifdef CONFIG_USBD_USER_SERIAL_NUMBER
    if (bus->serial_number_str && strlen(bus->serial_number_str)) {
        device_descriptor->iSerialNumber = usbd_alloc_string(bus->serial_number_str);
    }
    else {
        device_descriptor->iSerialNumber = usbd_alloc_string(function_driver->device_description->iSerialNumber);
    }
#else
    // XXX should not need to do anything here
#endif
    device_descriptor->bNumConfigurations = function_driver->configurations;

    dbg_init(3,"configurations: %d", device_descriptor->bNumConfigurations);


    // allocate the configuration descriptor array
    if (!(function_driver->configuration_instance_array = 
                alloc_function_configurations(
                    function_driver->configurations, 
                    function_driver->configuration_description)))
    {
        dbg_init(1,"failed during function configuration %s", function_driver->name);
        usbd_dealloc_descriptor((struct usb_descriptor *)device_descriptor);
        return;
    }
    function_driver->device_descriptor = device_descriptor;
    dbg_init(1, "- finished");
}

void usbd_function_close(struct usb_device_instance *device)
{
    struct usb_function_instance *function;

    if ((function = usbd_device_function_instance(device, 0))) {
        if (function->function_driver->ops->function_exit) {
            function->function_driver->ops->function_exit(device);
        }
    }
    usbd_dealloc_function(function->function_driver);
}





