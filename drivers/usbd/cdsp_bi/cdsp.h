/*
 * linux/drivers/usbd/cdsp_bi/cdsp.h - USB device CDSP bus interface
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

#ifndef CDSP_H
#define CDSP_H 1

//extern struct usb_bus_instance *bus;
extern struct usb_device_instance *device_array[];
extern struct list_head urbs;

extern void bi_stall(struct usb_bus_instance *bus, struct urb *urb);
extern void bi_ack(struct usb_bus_instance *bus, struct urb *urb);
extern void bi_result(struct usb_bus_instance *bus, struct urb *urb);

#endif
