/*
 *  linux/arch/arm/mach-clps711x/p720t.c
 *
 *  Copyright (C) 2000-2001 Deep Blue Solutions Ltd
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
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/arch/syspld.h>

extern void clps711x_init_irq(void);
extern void p720t_map_io(void);

static void __init
fixup_p720t(struct machine_desc *desc, struct param_struct *params,
	    char **cmdline, struct meminfo *mi)
{
	struct tag *tag = (struct tag *)params;

	tag->hdr.tag = ATAG_CORE;
	tag->hdr.size = tag_size(tag_core);
	tag->u.core.flags = 0;
	tag->u.core.pagesize = PAGE_SIZE;
	tag->u.core.rootdev = 0x0100;

	tag = tag_next(tag);
	tag->hdr.tag = ATAG_MEM;
	tag->hdr.size = tag_size(tag_mem32);
	tag->u.mem.size = 4096;
	tag->u.mem.start = PHYS_OFFSET;

	tag = tag_next(tag);
	tag->hdr.tag = 0;
	tag->hdr.size = 0;
}

MACHINE_START(P720T, "ARM-Prospector720T")
	MAINTAINER("ARM Ltd/Deep Blue Solutions Ltd")
	BOOT_MEM(0xc0000000, 0x80000000, 0xff000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_p720t)
	MAPIO(p720t_map_io)
	INITIRQ(clps711x_init_irq)
MACHINE_END

static int p720t_hw_init(void)
{
	/*
	 * Power down as much as possible in case we don't
	 * have the drivers loaded.
	 */
	PLD_LCDEN = 0;
	PLD_PWR  &= ~(PLD_S4_ON|PLD_S3_ON|PLD_S2_ON|PLD_S1_ON);

	PLD_KBD   = 0;
	PLD_IO    = 0;
	PLD_IRDA  = 0;
	PLD_CODEC = 0;
	PLD_TCH   = 0;
	PLD_SPI   = 0;
#ifndef CONFIG_DEBUG_LL
	PLD_COM2  = 0;
	PLD_COM1  = 0;
#endif

	return 0;
}

__initcall(p720t_hw_init);

