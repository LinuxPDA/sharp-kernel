/*
 *  linux/arch/arm/mach-katana/arch.c
 *
 *  Copyright (C) 2003 MediaQ, Incorporated.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
//#include <asm/mach/amba_kmi.h>

extern void katana_map_io(void);
extern void katana_init_irq(void);

#ifdef CONFIG_KMI_KEYB

/* HSU Don't deal with keyboard for now!
static struct kmi_info katana_keyboard __initdata = {
	.base		= IO_ADDRESS(KMI0_BASE),
	.irq		= IRQ_KMIINT0,
	.divisor	= 24 / 8 - 1,
	.type		= KMI_KEYBOARD,
};
*/

/* HSU No mouse support
static struct kmi_info integrator_mouse __initdata = {
	.base		= IO_ADDRESS(KMI1_BASE),
	.irq		= IRQ_KMIINT1,
	.divisor	= 24 / 8 - 1,
	.type		= KMI_MOUSE,
};
*/
#endif

static void __init
katana_fixup(struct machine_desc *desc, struct param_struct *params,
		 char **cmdline, struct meminfo *mi)
{
//	leds_init();
#ifdef CONFIG_KMI_KEYB
	register_kmi(&katana_keyboard);
//	register_kmi(&integrator_mouse);
#endif
#if defined(CONFIG_BLK_DEV_INITRD)
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
	setup_initrd(__phys_to_virt(0x10c00000), 4 * 1024 * 1024);
#elif defined(CONFIG_MTD)
	ROOT_DEV = MKDEV(31, 0);	/* /dev/mtdblock0 */
#endif
	params->u1.s.nr_pages = MEM_SIZE / PAGE_SIZE;
#ifdef CONFIG_XIP_KERNEL
	params->u1.s.page_size = PAGE_SIZE;
	params->u1.s.flags = FLAG_READONLY | FLAG_RDLOAD | FLAG_RDPROMPT;
	params->u1.s.rootdev = ROOT_DEV;
	params->u1.s.ramdisk_size = 0;
#ifdef CONFIG_BLK_DEV_INITRD
	params->u1.s.initrd_start = __phys_to_virt(0x10c00000);
	params->u1.s.initrd_size = 4 * 1024 * 1024;
#else
	params->u1.s.initrd_start = 0;
	params->u1.s.initrd_size = 0;
#endif
	params->u1.s.rd_start = 0;
	params->u1.s.system_rev = 0;
	params->u1.s.system_serial_low = 0;
	params->u1.s.system_serial_high = 0;
	memset(&params->u2, 0, 1024);
	strcpy(params->commandline, CONFIG_CMDLINE);
#endif
}

//HSU: Set Video to non-serialized region with 256K for now!
MACHINE_START(KATANA, "Katana")
	MAINTAINER("MediaQ, Inc.")
	BOOT_MEM(0x10000000, MQ9000_REGS_BASE, MQ9000_REGS_VBASE)
	BOOT_PARAMS(0x10000100)
//        VIDEO(0x40200000, 0x40240000)
	FIXUP(katana_fixup)
	MAPIO(katana_map_io)
	INITIRQ(katana_init_irq)
MACHINE_END
