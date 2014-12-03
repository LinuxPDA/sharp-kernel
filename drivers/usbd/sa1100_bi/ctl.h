/*
 * linux/drivers/usbd/sa1100_bi/ctl.h
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
 *
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*****************************************************************************/

#ifndef __USB_CTL_H__
#define __USB_CTL_H__


//#include <linux/if_ether.h>

//#include "asm/proc/cache.h"

/*----------------------------------------------------------------------
 * Device controller interface specification
 *---------------------------------------------------------------------*/
#if 0

typedef void (*usb_connect_callback_t)(int);
typedef void (*usb_suspend_callback_t)(void);
typedef void (*usb_error_callback_t)(int);


/*
 * Function Prototypes
 */

int udc_init(void);
void udc_cleanup(void);
void udc_connect(void);
void udc_disconnect(void);
void udc_enable(void);
void udc_disable(void);
void udc_enable_interrupts(int);
void udc_disable_interrupts(int);
void udc_configure(void *);
void udc_write_reg(volatile unsigned int *, unsigned int);
void usbctl_clear_descriptors(void);


void ep0_int_hndlr(unsigned int status);
void ep0_idle(unsigned int ep0_status);
void ep0_reset(void);

int ep1_recv(void); 
int ep1_init(int chn);
void ep1_int_hndlr(unsigned int status);
int ep1_reset(void);

int ep2_start(void);
void ep2_stop(void);
void ep2_reset(void);
int ep2_init(int chn);
void ep2_int_hndlr(unsigned int status);
#endif

/*
 * Called to request access to the USB. Passes descriptor 
 * information for device enumeration, and callback entry points. 
 * At this point the UCD is enabled, and possibly as a result the 
 * power sub-system will be notified of a change in the available power.
 *
 * usb_device_t : function filled device structure detailing how
 *                the bus should be enumerated.
 * callbacks    : callback entry points, all callbacks are compulsory.
 * returns      : a USB status code.
 */
int
usbctl_start(void);

/*
 * called if the client function is no longer needed. This will 
 * disable the UCD, and the power system will be alerted of the 
 * change in available power.
 */
int 
usbctl_release(void);

/* 
 * Power Interface
 */
typedef int (*usb_power_callback_t)(int);

/*
 * register a power driver with the client USB controller. The power 
 * driver now waits for the provided callback to tell it that it may 
 * start drawing power.
 */
int 
register_usb_power(int, usb_power_callback_t);

/*
 * send a block across the bus.
 */
int
usb_transmit_skb(struct sk_buff *);

/* Who uses this? */
/*#define GPIO_USB_ENABLE  GPIO_GPIO(24)*/

/*---------------------------------------------------------------------- 
 * USB Protocol Information
 *---------------------------------------------------------------------*/

/*
 * EndPoint Information
 */

/* The SA1100 endpoints */
#define SA1100_UDC_RX_EP 1
#define SA1100_UDC_TX_EP 2

/* The USB_EP_ADDRESS macro should be used with the macros USB_OUT
 * and USB_IN when creating an endp structure. The macro creates 
 * and address suitable for transmit on the USB.
 */
#define USB_OUT 0
#define USB_IN 1
#define USB_EP_ADDRESS(a,d) (((a)&0xf) | ((d) << 7))
/* Attributes : Note that only control and bulk are 
 * supported by the SA1100.
 */
#define USB_EP_CONTROL  0x00
#define USB_EP_ISOC     0x01
#define USB_EP_BULK     0x02
#define USB_EP_INT      0x03

/*
 * Configuration Information
 */
#define USB_CONFIG_BUSPOWERED     0x80
#define USB_CONFIG_SELFPOWERED    0x40
#define USB_CONFIG_REMOTEWAKE     0x20
#define USB_POWER(x)   (x)/2            /* converts mA to USB units of A */

/*
 * Device Information
 */

#define USB_V100_COMPLIANT 0x100
#define USB_VENDOR_COMPAQ  1183
#define COMPAQ_ITSY_ID     0x505A

/*
 * USB Protocol Specific Definitions
 */

/*      Request Codes      */
#define GET_STATUS         0
#define CLEAR_FEATURE      1
#define SET_FEATURE        3
#define SET_ADDRESS        5
#define GET_DESCRIPTOR     6
#define SET_DESCRIPTOR     7
#define GET_CONFIGURATION  8
#define SET_CONFIGURATION  9
#define GET_INTERFACE      10
#define SET_INTERFACE      11

/*      Descriptor Types    */
#define USB_DESC_DEVICE     1
#define USB_DESC_CONFIG     2
#define USB_DESC_STRING     3
#define USB_DESC_INTERFACE  4
#define USB_DESC_ENDPOINT   5

/* USB Device Requests */
typedef struct 
{
    __u8 bmRequestType;
    __u8 bRequest;
    __u16 wValue;
    __u16 wIndex;
    __u16 wLength;
} usb_dev_request_t  __attribute__ ((packed));

#define USB_REQUEST_RECIPIENT(x) ((x)&0x0f)
#define USB_RECIPIENT_DEVICE 0
#define USB_RECIPIENT_INTERFACE 1
#define USB_RECIPIENT_ENDPOINT 2

/* Device Descriptor Definitions */
#define USB_DEVDESC_LENGTH          0x12

#define USB_DEVDESC_BLENGTH         0
#define USB_DEVDESC_BDESCTYPE       1
#define USB_DEVDESC_BCDUSB_LO       2
#define USB_DEVDESC_BCDUSB_HI       3
#define USB_DEVDESC_BDEVCLASS       4
#define USB_DEVDESC_BDEVSUBCLASS    5
#define USB_DEVDESC_BDEVPROT        6
#define USB_DEVDESC_BMAXSIZE        7
#define USB_DEVDESC_IDVEND_LO       8
#define USB_DEVDESC_IDVEND_HI       9
#define USB_DEVDESC_IDPROD_LO       10
#define USB_DEVDESC_IDPROD_HI       11
#define USB_DEVDESC_BCDDEV_LO       12
#define USB_DEVDESC_BCDDEV_HI       13
#define USB_DEVDESC_IMANU           14
#define USB_DEVDESC_IPRODUCT        15
#define USB_DEVDESC_ISERNUM         16
#define USB_DEVDESC_BNUMCONF        17

/* Config Descriptor Definitions */
#define USB_CONFDESC_LENGTH            9

#define USB_CONFDESC_BLENGTH           0
#define USB_CONFDESC_BDESCTYPE         1
#define USB_CONFDESC_WTOTLENGTH_LO     2
#define USB_CONFDESC_WTOTLENGTH_HI     3
#define USB_CONFDESC_BNUMIFACE         4
#define USB_CONFDESC_BCONFVAL          5
#define USB_CONFDESC_ICONF             6
#define USB_CONFDESC_BMATTRIB          7
#define USB_CONFDESC_MAXPOWER          8

/* Interface Descriptor Definitions */
#define USB_IFACEDESC_LENGTH  0x09

#define USB_IFACEDESC_BLENGTH          0
#define USB_IFACEDESC_BDESCTYPE        1
#define USB_IFACEDESC_BIFACENUM        2
#define USB_IFACEDESC_BALTSETTING      3
#define USB_IFACEDESC_BNUMENDP         4
#define USB_IFACEDESC_BIFACECLASS      5
#define USB_IFACEDESC_BIFACESUBCLASS   6
#define USB_IFACEDESC_BIFACEPROT       7
#define USB_IFACEDESC_IIFACE           8

/* Endpoint Descriptor Definition */
#define USB_ENDPDESC_LENGTH   0x7

#define USB_ENDPDESC_BLENGTH           0
#define USB_ENDPDESC_BDESCTYPE         1
#define USB_ENDPDESC_BADDRESS          2
#define USB_ENDPDESC_BMATTR            3
#define USB_ENDPDESC_WMAXPKTSZE_LO     4
#define USB_ENDPDESC_WMAXPKTSZE_HI     5
#define USB_ENDPDESC_BINTERVAL         6

/*
 * SA1100 Register Definitions and Masks
 */
// Ser0UDCCR
#define UDCCR  ((volatile unsigned int *)io_p2v(0x80000000))
#define UDCCR_INTS (UDCCR_EIM | UDCCR_RIM | UDCCR_TIM | UDCCR_SRM | UDCCR_REM)
#define UDCCR_ERR29 0x80        // enable ERRATA 29 fix for B5 stepping

#define UDCAR ((volatile unsigned int *)io_p2v(0x80000004))
#define UDCOMP ((volatile unsigned int *)io_p2v(0x80000008))
#define UDCIMP ((volatile unsigned int *)io_p2v(0x8000000C))

// Ser0UDCCS0
#define UDCCS0 ((volatile unsigned int *)io_p2v(0x80000010))

#define UDCD0  ((volatile unsigned int *)io_p2v(0x8000001c))
#define UDCDR  ((volatile unsigned int *)io_p2v(0x80000028))
#define UDCWC  ((volatile unsigned int *)io_p2v(0x80000020))
#define UDCSR  ((volatile unsigned int *)io_p2v(0x80000030))

// Ser0UDCCS1
#define UDCCS1  ((volatile unsigned int *)io_p2v(0x80000014))

// Ser0UDCCS2
#define UDCCS2  ((volatile unsigned int *)io_p2v(0x80000018))

#define VENDOR_SPECIFIC_CLASS       0xff
#define VENDOR_SPECIFIC_SUBCLASS    0xff
#define VENDOR_SPECIFIC_PROTOCOL    0xff

#define DDAR  ((volatile unsigned int *)io_p2v(0xb0000000))
#define DDCS  ((volatile unsigned int *)io_p2v(0xb000000c))
#define DDBA  ((volatile unsigned int *)io_p2v(0xb0000010))
#define DDTA  ((volatile unsigned int *)io_p2v(0xb0000014))
#define DDBB  ((volatile unsigned int *)io_p2v(0xb0000018))
#define DDTB  ((volatile unsigned int *)io_p2v(0xb000001c))

#define DMA_UDC_TRANSMIT                0x80000A04
#define DMA_UDC_RECEIVE                 0x80000A15

#endif /* __USB_CTL_H__ */
