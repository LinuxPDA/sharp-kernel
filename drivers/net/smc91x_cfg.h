/*------------------------------------------------------------------------
 . smc91111_cfg.h - local configurations for the LAN91C111 Ethernet Driver
 .
 . Copyright (C) 2002 Accelent Systems, Inc.
 .
 . This program is free software; you can redistribute it and/or modify
 . it under the terms of the GNU General Public License as published by
 . the Free Software Foundation; either version 2 of the License, or
 . (at your option) any later version.
 .
 . This program is distributed in the hope that it will be useful,
 . but WITHOUT ANY WARRANTY; without even the implied warranty of
 . MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 . GNU General Public License for more details.
 .
 . You should have received a copy of the GNU General Public License
 . along with this program; if not, write to the Free Software
 . Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 .
 . Author: Jeff Sutherland <jeffs@accelent.com>
 .
 . Purpose: This file contains machine/architecture specific configuration
 .	    twiddles, hacks, and kludges for the smc91111 driver so that all
 .	    that crud can be removed from the driver in an effort to make it
 .	    as generic (and clean) as possible.
 .
 . Changelog:
 .	06-03-2003: Lineo uSolutions, Inc.
 .
 ---------------------------------------------------------------------------*/

#ifndef _SMC91111_CFG_H_
#define _SMC91111_CFG_H_

#if CONFIG_ARCH_KATANA

/* Platform specific defines: */

#define NO_AUTOPROBE 1

/* More than one chip per system may be supported by this driver: */

#define NETWORK_INTERFACE_COUNT 1

#define NETWORK_INTERFACE0_REGISTER_BASE_ADDRESS	0xD7000300
// and so on and so forth...

#define NETWORK_INTERFACE0_INTERRUPT			23

/* Because of bank switching, the LAN91xxx uses only 16 I/O ports (on an
 * isa based system, that is.) */

#ifdef SMC_IO_EXTENT
#undef SMC_IO_EXTENT
#endif

#define SMC_IO_EXTENT	(16 << 2)

// Use power-down feature of the chip
#define SMC_POWER_DOWN	1

#undef USE_32_BIT
#undef USE_SMC_DMA

static unsigned long smc_portlist[] __initdata = {
	NETWORK_INTERFACE0_REGISTER_BASE_ADDRESS,
	0
};

static unsigned int smc_irqlist[] __initdata = {
	NETWORK_INTERFACE0_INTERRUPT,
	0
};

/*
 * Wait time for TX packet memory to be free.  This probably shouldn't be
 * tuned that much, as waiting for this means nothing else happens
 * in the system. Originally this was set to 16 but I'm not sure you want to
 * sit and spin here waiting to tx packets.  Let interrupts do their job.
 */

#define MEMORY_WAIT_TIME 4

#endif /* CONFIG_ARCH_KATANA */


/* Support for the Accelent PXA IDP and friends:
 * Architecture: ARM
 * Processor:	 Intel PXA
 * Machine:	 Accelent PXA IDP
 */

#ifdef CONFIG_ARCH_PXA_IDP

/* Local i/o macros for the PXA IDP (32 bit bus i/f)
*  Hack alert: It is not possible to do anything other than a dword read op
*  on the PXA processor.  The arch specific inb/inw/inl do the right things
*  by byte shuffling, but the only thing that works for reading a fifo is an
*  op that supports the native width of the fifo.  In this case it's 32 bits,
*  or the inl op.  Don't worry, DMA does the right thing via the DQMx lines,
*  and writes also do the right thing.  Believe it or not, this is a *feature*,
*  not a bug, according to Intel. 
*/

/* Platform specific defines: */

#define NETWORK_INTERFACE0_PHYS_DATA_ADDR (IDP_ETH_PHYS + 0x300 + DATA_REG)
#define NETWORK_INTERFACE1_PHYS_DATA_ADDR (IDP_ETH_PHYS + 0x200 + DATA_REG)

#define NO_AUTOPROBE 1

/* More than one chip per system may be supported by this driver: */

#define NETWORK_INTERFACE_COUNT 1

#define NETWORK_INTERFACE0_REGISTER_BASE_ADDRESS	(ETH_BASE + 0x300)
#define NETWORK_INTERFACE1_REGISTER_BASE_ADDRESS	0
// and so on and so forth...

#define NETWORK_INTERFACE0_INTERRUPT			ETHERNET_IRQ
#define NETWORK_INTERFACE1_INTERRUPT			0

/* Because of bank switching, the LAN91xxx uses only 16 I/O ports (on an
 * isa based system, that is.) */

#ifdef SMC_IO_EXTENT
#undef SMC_IO_EXTENT
#endif

#define SMC_IO_EXTENT	(16 << 2)

// Use power-down feature of the chip
#define SMC_POWER_DOWN	1

#define USE_32_BIT 1
#define USE_SMC_DMA 1

#ifdef USE_SMC_DMA

static unsigned long smc_devaddrlist[] __initdata = {
	NETWORK_INTERFACE0_PHYS_DATA_ADDR,
#if NETWORK_INTERFACE_COUNT > 1
	NETWORK_INTERFACE1_PHYS_DATA_ADDR,
#endif
	0
};
#endif

static unsigned long smc_portlist[] __initdata = {
	NETWORK_INTERFACE0_REGISTER_BASE_ADDRESS,
#if NETWORK_INTERFACE_COUNT > 1
	NETWORK_INTERFACE1_REGISTER_BASE_ADDRESS,
#endif
	0
};

/* no support for pcmcia card drivers just yet...*/
/*
#if NETWORK_DRIVER_PCMCIA_MODE == TRUE
static unsigned long smc_attmem_list[] = {
	NETWORK_INTERFACE0_ATTRIBUTE_BASE_ADDRESS,
	
#if NETWORK_INTERFACE_COUNT > 1
	NETWORK_INTERFACE1_ATTRIBUTE_BASE_ADDRESS,
#endif
	0
};
#endif
*/

static unsigned int smc_irqlist[] __initdata = {
	NETWORK_INTERFACE0_INTERRUPT,
#if NETWORK_INTERFACE_COUNT > 1
	NETWORK_INTERFACE1_INTERRUPT,
#endif
	0
};

/*
 * Wait time for TX packet memory to be free.  This probably shouldn't be
 * tuned that much, as waiting for this means nothing else happens
 * in the system. Originally this was set to 16 but I'm not sure you want to
 * sit and spin here waiting to tx packets.  Let interrupts do their job.
 */

#define MEMORY_WAIT_TIME 4

#endif /* config_arch_pxa_idp */


#if CONFIG_ARCH_INNOKOM

/* Local i/o macros for the PXA IDP (32 bit bus i/f)
*  Hack alert: It is not possible to do anything other than a dword read op
*  on the PXA processor.  The arch specific inb/inw/inl do the right things
*  by byte shuffling, but the only thing that works for reading a fifo is an
*  op that supports the native width of the fifo.  In this case it's 32 bits,
*  or the inl op.  Don't worry, DMA does the right thing via the DQMx lines,
*  and writes also do the right thing.  Believe it or not, this is a *feature*,
*  not a bug, according to Intel.
*/

/* Platform specific defines: */

#define NETWORK_INTERFACE0_PHYS_DATA_ADDR (INNOKOM_ETH_PHYS + DATA_REG)

#define NO_AUTOPROBE 1

/* More than one chip per system may be supported by this driver: */

#define NETWORK_INTERFACE_COUNT 1

#define NETWORK_INTERFACE0_REGISTER_BASE_ADDRESS	(ETH_BASE)
// and so on and so forth...

#define NETWORK_INTERFACE0_INTERRUPT			ETHERNET_IRQ

/* Because of bank switching, the LAN91xxx uses only 16 I/O ports (on an
 * isa based system, that is.) */

#ifdef SMC_IO_EXTENT
#undef SMC_IO_EXTENT
#endif

#define SMC_IO_EXTENT	(16 << 2)

// Use power-down feature of the chip
#define SMC_POWER_DOWN	1

#undef USE_32_BIT
#undef USE_SMC_DMA

#ifdef USE_SMC_DMA

static unsigned long smc_devaddrlist[] __initdata = {
	NETWORK_INTERFACE0_PHYS_DATA_ADDR,
	0
};
#endif

static unsigned long smc_portlist[] __initdata = {
	NETWORK_INTERFACE0_REGISTER_BASE_ADDRESS,
	0
};

/* no support for pcmcia card drivers just yet...*/
/*
#if NETWORK_DRIVER_PCMCIA_MODE == TRUE
static unsigned long smc_attmem_list[] = {
	NETWORK_INTERFACE0_ATTRIBUTE_BASE_ADDRESS,

#if NETWORK_INTERFACE_COUNT > 1
	NETWORK_INTERFACE1_ATTRIBUTE_BASE_ADDRESS,
#endif
	0
};
#endif
*/

static unsigned int smc_irqlist[] __initdata = {
	NETWORK_INTERFACE0_INTERRUPT,
	0
};

/*
 * Wait time for TX packet memory to be free.  This probably shouldn't be
 * tuned that much, as waiting for this means nothing else happens
 * in the system. Originally this was set to 16 but I'm not sure you want to
 * sit and spin here waiting to tx packets.  Let interrupts do their job.
 */

#define MEMORY_WAIT_TIME 4


#endif /* CONFIG_ARCH_PXA_INNOKOM */


/* If you want to use autoprobing for your hardware, you may make use of
 * this table. The LAN91C111 can be at any of the following port addresses.
 * To change for a slightly different card, you can add it to the array.
 * Keep in mind that the array must end in zero.
 */

#if 0
static unsigned int smc_portlist[] __initdata =
    { 0x200, 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0,
	0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0, 0x3C0, 0x3E0, 0
};
#endif

#endif /*  _SMC91111_CFG_H_ */
