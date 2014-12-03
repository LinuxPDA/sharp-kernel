/* -------------------------------------------------------------------- */
/* usb-ohci-voyagergx.c:                                                */
/* -------------------------------------------------------------------- */
/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Copyright 2003 (c) Lineo uSolutions,Inc.
*/
/* -------------------------------------------------------------------- */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/pci.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/voyagergx_reg.h>

#include "usb-ohci.h"
#include "mem-onchip.h"

extern hc_found_ohci (struct pci_dev *, int ,
				void *, const struct pci_device_id *);
extern void hc_release_ohci (ohci_t *);

static ohci_t* voyagergx_ohci;

//#define VOYAGER_USB_MEM	(0xb03e0000)
#define VOYAGER_USB_MEM		(0xb0000000)
#define VOYAGER_USB_MEM_SIZE	(0x10000)

static void __init voyagergx_ohci_configure(void)
{
	unsigned long           val;

        // Power Mode 0 Gate
        val = inl(POWER_MODE0_GATE);
        outl((val | POWER_MODE0_GATE_UH), POWER_MODE0_GATE);

	val = inl(POWER_MODE1_GATE);
        outl((val | POWER_MODE1_GATE_UH), POWER_MODE1_GATE);

        //Miscellaneous USB Clock Selsct
        val = inl(MISC_CTRL);
	val &= ~MISC_CTRL_USBCLK_48;
        outl(val, MISC_CTRL);

        // Interrupt Mask
        val = inl(VOYAGER_INT_MASK);
        val |= 0x00000040;
        outl(val, VOYAGER_INT_MASK);

	oc_mem_init(VOYAGER_USB_MEM, VOYAGER_USB_MEM_SIZE);
}

static int __init voyagergx_ohci_init(void)
{
	int ret;

	voyagergx_ohci_configure();
	return hc_add_ohci((struct pci_dev*)1,
			   VOYAGER_USBH_IRQ,
			   (void*)VOYAGER_USBH_BASE,
			   0,
			   &voyagergx_ohci,
			   "usb-ohci");
}

static void __exit voyagergx_ohci_exit(void)
{
	hc_remove_ohci(voyagergx_ohci);
}

module_init(voyagergx_ohci_init);
module_exit(voyagergx_ohci_exit);
