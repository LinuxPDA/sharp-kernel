/*
 * linux/arch/arm/mach-xscale/arch.c
 *
 * Copyright (C) 2001 Lineo, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Change Log
 *	24-May-2002 SHARP	Change Bank Settings
 */

#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <asm/types.h>
#include <asm/setup.h>

#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

extern void cotulla_map_io(void);
extern void cotulla_init_irq(void);


static void __init
fixup_cotulla(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
	printk("fixup_cotulla:\n");
	
	system_rev = 0xF;


	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size  = (32*1024*1024);
	mi->bank[0].node  = 0;

	mi->bank[1].start = 0xa4000000;
	mi->bank[1].size  = (4*1024*1024);
	mi->bank[1].node  = 0;
	mi->nr_banks      = 2;


#ifdef CONFIG_ROOT_NFS
	ROOT_DEV = to_kdev_t(0x00ff);   /* /dev/nfs */
#elif defined(CONFIG_BLK_DEV_INITRD)
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	setup_ramdisk( 1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE );
	setup_initrd( 0xC0400000, 24 * 1024 * 1024 );
#elif defined(CONFIG_MTD)
	ROOT_DEV = MKDEV(31, 0);	/* /dev/mtdblock0 */
#endif

}

MACHINE_START(COTULLA, "Cotulla")
	MAINTAINER("Lineo, Inc.")
	BOOT_MEM(0xa0000000, 0x40000000, 0xf0000000)
	FIXUP(fixup_cotulla)
	MAPIO(cotulla_map_io)
	INITIRQ(cotulla_init_irq)
MACHINE_END
