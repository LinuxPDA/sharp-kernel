/*
 *  linux/include/asm-arm/arch-integrator/mmu.h
 *
 *  Copyright (C) 1999 ARM Limited
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
#ifndef __ASM_ARCH_MMU_H
#define __ASM_ARCH_MMU_H

#if 0	//HSU: Temporary use virtual addr of 0x10000000 for easy download
	//     thru Multi-Ice with AxD's load-image command.

/*
 * Task size: 1GB
 */
#define TASK_SIZE	(0x80000000UL)
		//until the temporary 0x80000000 direct 1:1 mapping in mm.c
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

//$$$$$$$$$$$$$$$$ HSU_UNKNOWN $$$$$$$$$$$$$$$$$
// Don't know about this PAGE_OFFSET stuff!
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
/*
 * Page offset: 3GB
 */
//HSU Let's change PAGE_OFFSET to 0x10000000 where our RAM is
//    PAGE_OFFSET is where the virtual address is going to be for our RAM
//#define PAGE_OFFSET	(0xc0000000UL)
#define PAGE_OFFSET	(0x10000000UL)

//HSU PHYS_OFFSET means our physical DRAM area
//#define PHYS_OFFSET	(0x00000000UL)
#define PHYS_OFFSET	(0x10000000UL)

/*
 * On integrator, the dram is contiguous
 * HSU: So is the katana's dram which is contiguous.  Add/Sub PHYS_OFFSET.
 */
#define __virt_to_phys__is_a_macro
#define __virt_to_phys(vpage) ((vpage) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt__is_a_macro
#define __phys_to_virt(ppage) ((ppage) + PAGE_OFFSET - PHYS_OFFSET)

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *              address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *              to an address that the kernel can use.
 */
#define __virt_to_bus__is_a_macro
#define __virt_to_bus(x)	(x - PAGE_OFFSET + PHYS_OFFSET)
#define __bus_to_virt__is_a_macro
#define __bus_to_virt(x)	(x - PHYS_OFFSET + PAGE_OFFSET)

#else	//HSU

/*
 * Task size: 3GB
 */
#define TASK_SIZE	(0xc0000000UL)
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

/*
 * Page offset: 3GB
 */
#define PAGE_OFFSET	(0xc0000000UL)

//HSU PHYS_OFFSET means our physical DRAM area
//#define PHYS_OFFSET	(0x00000000UL)
#define PHYS_OFFSET	(0x10000000UL)

/*
 * On integrator, the dram is contiguous
 * HSU: So is the katana's dram which is contiguous.  Add/Sub PHYS_OFFSET.
 */
#define __virt_to_phys__is_a_macro
#define __virt_to_phys(vpage) ((vpage) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt__is_a_macro
#define __phys_to_virt(ppage) ((ppage) + PAGE_OFFSET - PHYS_OFFSET)

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *              address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *              to an address that the kernel can use.
 */
#define __virt_to_bus__is_a_macro
#define __virt_to_bus(x)	(x - PAGE_OFFSET + PHYS_OFFSET)
#define __bus_to_virt__is_a_macro
#define __bus_to_virt(x)	(x - PHYS_OFFSET + PAGE_OFFSET)

#endif	//1
#endif
