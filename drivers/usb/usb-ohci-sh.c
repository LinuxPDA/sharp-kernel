/*
 * linux/drivers/usb/usb-ohci-sh.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * Based on usb-ohci.c and usb-ohci-sa1111.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * USB OHCI compatible driver for SH7xxx
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include <asm/irq.h>
#include <asm/io.h>
#if defined(CONFIG_CPU_SUBTYPE_SH7760)
#include <asm/sh7760.h>
#elif defined(CONFIG_CPU_SUBTYPE_SH7727)
#include <asm/sh7727.h>
#elif defined(CONFIG_CPU_SUBTYPE_SH)
#include <asm/sh.h>
#endif

#include "usb-ohci.h"

#undef kmalloc
#undef kfree

#if defined(CONFIG_CPU_SUBTYPE_SH7760)
#define SH7760_USB_MEM		(0xFE341000)
#define SH7760_USB_MEM_SIZE	(0x2000)
#endif

#if !defined(CONFIG_CPU_SUBTYPE_SH7760)

void KFREE(void* p)
{
	u32 a = (u32)p;
	if ((a & 0xF0000000) >= 0xA0000000) {
		a = (a & 0x1FFFFFFF) + 0x80000000;
	}
	kfree((void*)a);
}

void* KMALLOC(size_t s, int v)
{
	void* p = kmalloc(s, v);
	u32 a = (u32)p;
	a = (a & 0x1FFFFFFF) + 0xA0000000;
	return (void*)a;
}
#endif

extern int  hc_add_ohci(struct pci_dev*, int, void*,
			unsigned long, ohci_t**, const char*);
extern void hc_remove_ohci(ohci_t*);

static ohci_t* sh_ohci;

static int __init sh_ohci_init(void)
{
	int ret;

#if defined(CONFIG_SH_PFMDS11)
	ctrl_outw(0x0001, EXPFC); /* P1:USBF, P2:USBH */
	ctrl_outw(ctrl_inw(PECR) & 0xFFF3, PECR);

	ctrl_outw(ctrl_inw(PDCR) & 0xCFFF, PDCR);
	ctrl_outb(ctrl_inb(STBCR3)  | 0x08, STBCR3);  /* USBH stop */
	ctrl_outb(ctrl_inb(EXCPGCR) | 0x30, EXCPGCR); /* ExtClk 48MHz */
	ctrl_outb(ctrl_inb(STBCR3)  & 0xF7, STBCR3);  /* USBH start */
#endif
#if defined(CONFIG_SH_MS7727RP)
	ctrl_outw(0x0001, EXPFC); /* P1:USBF, P2:USBH */
	ctrl_outw(ctrl_inw(PECR) & 0xFFF3, PECR);

	ctrl_outw(ctrl_inw(PDCR) & 0xCFFF, PDCR);
	ctrl_outb(ctrl_inb(STBCR3)  | 0x08, STBCR3);  /* USBH stop */
	ctrl_outb(ctrl_inb(EXCPGCR) | 0x30, EXCPGCR); /* ExtClk 48MHz */
	ctrl_outb(ctrl_inb(STBCR3)  & 0xF7, STBCR3);  /* USBH start */
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7760)

	ctrl_outl(0x00020000,0xFE3C008C); /* DMAUCR */
	ctrl_outl(0x00020000,0xFE080060); /* INTMSKCLR00 */
	oc_mem_init(SH7760_USB_MEM, SH7760_USB_MEM_SIZE);

	ctrl_outw(ctrl_inw(PHCR) & 0xFFC0, PHCR);
#endif

	return hc_add_ohci((struct pci_dev*)1,
			   USBH_IRQ,
			   (void*)USBH_BASE,
			   0,
			   &sh_ohci,
			   "usb-ohci");
}

#ifdef MODULE
static void __exit sh_ohci_exit(void)
{
	hc_remove_ohci(sh_ohci);
}
#endif

module_init(sh_ohci_init);
#ifdef MODULE
module_exit(sh_ohci_exit);
#endif
