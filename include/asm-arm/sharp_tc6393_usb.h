/*
 * linux/include/asm-arm/sharp_tc6393_usb.h
 *
 * Sharp PDA Driver Header File
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#ifndef _SHARP_TC6393_USB_H
#define _SHARP_TC6393_USB_H

#define TC6393_OHCI_IOC_MAGIC   'U'

#define TC6393_OHCI_IOC_PDOWN   _IO(TC6393_OHCI_IOC_MAGIC, 10)
#define TC6393_OHCI_IOC_PON     _IO(TC6393_OHCI_IOC_MAGIC, 11)
#define TC6393_OHCI_IOC_PCHECK  _IO(TC6393_OHCI_IOC_MAGIC, 12)

#endif /* _SHARP_TC6393_USB_H */
