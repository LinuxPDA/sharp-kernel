/*
 *  linux/arch/arm/mach-clps711x/mm.c
 *
 *  Extra MM routines for the ARM Integrator board
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
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
#include <asm/pgtable.h>
#include <asm/page.h>
 
#include <asm/mach/map.h>

#include <asm/hardware/clps7111.h>
#include <asm/arch/syspld.h>

/*
 * Logical      Physical
 */
 
static struct map_desc p720t_io_desc[] __initdata = {
 { SYSPLD_VIRT_BASE, SYSPLD_PHYS_BASE, 1048576, DOMAIN_IO, 0, 1},
 { 0xfe400000, 0x10400000, 1048576, DOMAIN_IO, 0, 1},
 { CLPS7111_VIRT_BASE, CLPS7111_PHYS_BASE, 1048576, DOMAIN_IO, 0, 1 },
 LAST_DESC
};

void __init p720t_map_io(void)
{
	iotable_init(p720t_io_desc);
}
