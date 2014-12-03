/*
 *  linux/arch/arm/mach-katana/mm.c
 *
 *  Extra MM routines for the ARM Integrator board
 *
 *  Copyright (C) 1999,2000 Arm Limited
 *  Copyright (c) 2002 by MediaQ, Incorporated.
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
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
 
#include <asm/mach/map.h>


#include <asm/arch/platform.h>


//HSU: Allow read too?  Integrator only has write allowed.
//HSU: MQ9000_DISPLAY_BASE is set to non-serialized area.

static struct map_desc katana_io_desc[] __initdata = {          // r,w,c,b
 //{ MQ9000_RAM_VBASE,	 MQ9000_SDRAM_BASE,	SZ_64M, DOMAIN_IO, 0,1},
#ifndef CONFIG_XIP_KERNEL
 { MQ9000_FLASH_VBASE,   MQ9000_FLASH_BASE,	SZ_16M, DOMAIN_IO, 0,1},
#endif
 { MQ9000_BIUIO_VBASE,   MQ9000_BIUIO_BASE,	SZ_1M,	DOMAIN_IO, 0,1},
 { MQ9000_SSRAM_VBASE,   MQ9000_SSRAM_BASE,	SZ_16M,	DOMAIN_IO, 0,1},
// { MQ9000_DISPLAY_VBASE, MQ9000_DISPLAY_BASE,   SZ_4M,	DOMAIN_IO, 0,1},
 { MQ9000_DISPLAY_VBASE, MQ9000_DISPLAY_BASE,   SZ_4M,	DOMAIN_IO, 0,1,1,1},
// { MQ9000_DMA_VBASE,	 MQ9000_DMA_BASE,	SZ_8M,	DOMAIN_IO, 0,1},
 { MQ9000_EDEV0_VBASE,	 MQ9000_EDEV0_BASE,	SZ_8M,	DOMAIN_IO, 0,1},
 { MQ9000_EDEV1_VBASE,	 MQ9000_EDEV1_BASE,	SZ_8M,	DOMAIN_IO, 0,1},
#if TASK_SIZE <= MQ9000_REGS_BASE
 { MQ9000_REGS_BASE,	  MQ9000_REGS_BASE,	SZ_1M,	DOMAIN_IO, 0,1},
	//add one with direct 1:1 mapping for registers only when TASK_SIZE
	//  is greater than MQ9000_REGS_BASE.
#endif
 { MQ9000_REGS_VBASE,	 MQ9000_REGS_BASE,	SZ_1M,	DOMAIN_IO, 0,1},
 { MQ9000_JAVA_VBASE,    MQ9000_JAVA_BASE,	SZ_4M,	DOMAIN_IO, 0,1},
 LAST_DESC
};

void __init katana_map_io(void)
{
	iotable_init(katana_io_desc);
}
