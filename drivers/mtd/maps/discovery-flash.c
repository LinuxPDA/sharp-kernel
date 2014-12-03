/*
 * discovery-flash.c
 *
 * Copyright (C) 2002  SHARP
 * based on rpxlite.c,v 1.15 2001/10/02 15:05:14 dwmw2 Exp
 *          Handle mapping of the flash on the RPX Lite and CLLF boards
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
 *  ChangeLog:
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>


#define WINDOW_ADDR 0x00000000
#define WINDOW_SIZE 0x01000000
#define BUS_WIDTH 2

static struct mtd_info *mymtd;

__u8 discovery_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 discovery_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 discovery_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void discovery_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, (void *)(map->map_priv_1 + from), len);
}

void discovery_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void discovery_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void discovery_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void discovery_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio((void *)(map->map_priv_1 + to), from, len);
}

struct map_info discovery_map = {
	name: "Discovery-flash",
	size: WINDOW_SIZE,
	buswidth: BUS_WIDTH,
	read8: discovery_read8,
	read16: discovery_read16,
	read32: discovery_read32,
	copy_from: discovery_copy_from,
	write8: discovery_write8,
	write16: discovery_write16,
	write32: discovery_write32,
	copy_to: discovery_copy_to
};

static struct mtd_partition discovery_partitions[1] = {
	{
		name:		"Filesystem",
		size:		0x00e60000,
		offset:		0x00160000
	}
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))


int __init init_discovery(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	char *part_type = "static";

	printk(KERN_NOTICE "Discovery flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);
	discovery_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!discovery_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &discovery_map);
	if (!mymtd) {
		iounmap((void *)discovery_map.map_priv_1);
		return -ENXIO;
	}

	mymtd->module = THIS_MODULE;

	parts = discovery_partitions;
	nb_parts = NB_OF(discovery_partitions);

	printk(KERN_NOTICE "Using %s partision definition\n", part_type);
	add_mtd_partitions(mymtd, parts, nb_parts);

	return 0;
}

static void __exit cleanup_discovery(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (discovery_map.map_priv_1) {
		iounmap((void *)discovery_map.map_priv_1);
		discovery_map.map_priv_1 = 0;
	}
}

module_init(init_discovery);
module_exit(cleanup_discovery);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SHARP (Original: Arnold Christensen <AKC@pel.dk>)");
MODULE_DESCRIPTION("MTD map driver for Discovery");
