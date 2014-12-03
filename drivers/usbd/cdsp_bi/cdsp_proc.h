/*
 * linux/drivers/usbd/cdsp_bi/cdsp_proc.h - USB device CDSP bus interface
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

#ifndef USB_PROC_H
#define USB_PROC_H 1

extern int usb_proc_init(void);
extern void usb_proc_term(void);

/* List support functions ******************************************************************** */

static inline struct urb *
list_entry_urb(const struct list_head *le)
{
    return list_entry(le, struct urb, urbs);
}

#endif
