/*
 * linux/arch/arm/mach-cotulla/mm.c
 *
 * Copyright (C) 2001 Lineo, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Change Log
 *	22-Jun-2002 SHARP	Change Memory MAP
 */


#include <linux/mm.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/mach/map.h>

extern void discovery_init(void);

/*
 * Common I/O mapping:
 *
 * Typically, static virtual address mappings are as follow:
 *
 * 0xe8000000-0xefffffff:	flash memory (especially when multiple flash
 * 				banks need to be mapped contigously)
 * 0xf0000000-0xf3ffffff:	miscellaneous stuff (CPLDs, etc.)
 * 0xf4000000-0xf4ffffff:	SA-1111
 * 0xf5000000-0xf5ffffff:	reserved (used by cache flushing area)
 * 0xf6000000-0xffffffff:	reserved (internal COTULLA IO defined above)
 *
 * Below 0xe8000000 is reserved for vm allocation.
 *
 * The machine specific code must provide the extra mapping beside the
 * default mapping provided here.
 */

static struct map_desc standard_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xf0000000, 0x40000000, 0x01400000, DOMAIN_IO, 1, 1, 0, 0 }, /* Peripheral */
  { 0xf4000000, 0x44000000, 0x00200000, DOMAIN_IO, 1, 1, 0, 0 }, /* LCD */		// Richard
  { 0xf6000000, 0x11000000, 0x01000000, DOMAIN_IO, 1, 1, 0, 0 }, /* PCMCIA0 IO */	// CYB
  { 0xf7000000, 0x12000000, 0x01000000, DOMAIN_IO, 1, 1, 0, 0 }, /* Backpack GPIO */	// Richard
  { 0xf8000000, 0x08000000, 0x00100000, DOMAIN_IO, 1, 1, 0, 0 }, /* ASIC3 */		// Richard
  { 0xf9000000, 0xa4000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* Special case */
  { 0xfd000000, 0x48000000, 0x00200000, DOMAIN_IO, 1, 1, 0, 0 }, /* MSCx */
  { 0xff000000, 0x16000000, 0x00100000, DOMAIN_IO, 1, 1, 0, 0 }, /* ppsh */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* 32M main flash (cs0) */
  LAST_DESC
};

void __init cotulla_map_io(void)
{
	iotable_init(standard_io_desc);
#ifdef CONFIG_SABINAL_DISCOVERY
	discovery_init();
#endif
}
