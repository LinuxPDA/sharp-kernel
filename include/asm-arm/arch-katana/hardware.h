/*
 *  linux/include/asm-arm/arch-integrator/hardware.h
 *
 *  This file contains the hardware definitions of the Integrator.
 *
 *  Copyright (C) 1999 ARM Limited.
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
 *
 * Change Log
 *	8-Jan-2003 Lineo Japan, Inc.
 *
 */
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <asm/sizes.h>
#include <asm/arch/platform.h>

/*
 * XIP kernel text mapping.
 * Note: the exact virtual address is also specified in arch/arm/Makefile.
 */
#ifdef CONFIG_XIP_KERNEL
#define KERNEL_XIP_BASE_PHYS	((CONFIG_XIP_PHYS_ADDR & 0xffe00000) + 0xf0000000)
#define KERNEL_XIP_BASE_VIRT	0xd0000000
#endif

//HSU
#define	MEM_SIZE	(64*1024*1024L)	//use 64MB
//#define	MEM_SIZE	(32*1024*1024L)	//use 32MB

#if 0	//HSU every constants and regs should be in platform.h-->MQ9000Hwr.h
/*
 * Where in virtual memory the IO devices (timers, system controllers
 * and so on)
 */
#define IO_BASE			0xF0000000                 // VA of IO 
#define IO_SIZE			0x0B000000                 // How much?
#define IO_START		INTEGRATOR_HDR_BASE        // PA of IO

/*
 * Similar to above, but for PCI addresses (memory, IO, Config and the
 * V3 chip itself).  WARNING: this has to mirror definitions in platform.h
 */
//#define PCI_MEMORY_VADDR        0xe8000000
//#define PCI_CONFIG_VADDR        0xec000000
//#define PCI_V3_VADDR            0xed000000
//#define PCI_IO_VADDR            0xee000000

//#define PCIO_BASE		PCI_IO_VADDR
//#define PCIMEM_BASE		PCI_MEMORY_VADDR

/* macro to get at IO space when running virtually */
#define IO_ADDRESS(x) (((x) >> 4) + IO_BASE) 

//#define pcibios_assign_all_busses()	1

//#define PCIBIOS_MIN_IO		0x6000
//#define PCIBIOS_MIN_MEM 	0x00100000

#endif	//HSU

#ifndef __ASSEMBLY__

/*
 * GPIO edge detection for IRQs:
 * IRQs are generated on Falling-Edge, Rising-Edge, or both.
 * This must be called *before* the corresponding IRQ is registered.
 */
#define GPIO_FALLING_EDGE	1
#define GPIO_RISING_EDGE	2
#define GPIO_BOTH_EDGES		3
extern void set_GPIO_IRQ_edge(int gpio_nr, int edge_mask);

#endif

#endif

