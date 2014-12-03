/*
 * linux/drivers/mtd/maps/ms7720rp.c
 *
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * Flash memory access on Hitachi MS7720RP01
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

__u32 ms7720rp_read16(struct map_info* map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

void ms7720rp_write16(struct map_info* map, __u32 d, unsigned long ofs)
{
	__raw_writew(d, map->map_priv_1 + ofs);
	mb();
}

void ms7720rp_copy_from(struct map_info* map,
			void* to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static struct mtd_partition* parsed_parts = (struct mtd_partition*)0;

static struct mtd_info* flash_mtd = (struct mtd_info*)0;

#define NB_OF(x) (sizeof(x)/sizeof(x[0]))

struct map_info ms7720rp_flash_map = {
	name:      "MS7720RP01 FLASH",
	size:      0x01000000,
	buswidth:  2,
	read16:    ms7720rp_read16,
	write16:   ms7720rp_write16,
	copy_from: ms7720rp_copy_from,
};

#if 1
static struct mtd_partition ms7720rp_flash_partitions[] = {
	{
		name:       "sh-ipl",
		offset:     0x00000000,
		size:       0x00010000,
                mask_flags: MTD_WRITEABLE
        },
	{
		name:       "cramfs",
		offset:     0x00010000,
		size:       0x00FE0000,
                mask_flags: MTD_WRITEABLE
        }
};
#else
static struct mtd_partition ms7720rp_flash_partitions[] = {
	{
		name:       "mon",
		offset:     0x00000000,
		size:       0x00080000,
                mask_flags: MTD_WRITEABLE
        },
	{
		name:       "kernel",
		offset:     0x00080000,
		size:       0x00780000,
                mask_flags: MTD_WRITEABLE
        },
	{
		name:       "cramfs",
		offset:     0x00800000,
		size:       0x00800000,
                mask_flags: MTD_WRITEABLE
        }
};
#endif

static int __init ms7720rp_maps_init(void)
{
	struct mtd_partition* parts;
	int nr_parts = 0;

#if defined(CONFIG_XIP_KERNEL)
	ms7720rp_flash_map.map_priv_1 = P2SEGADDR(0);
	flash_mtd = do_map_probe("map_rom", &ms7720rp_flash_map);
	if (!flash_mtd)
		return -ENXIO;
#else
	ms7720rp_flash_map.map_priv_1 = P2SEGADDR(0);
	printk(KERN_NOTICE "Probing for flash chips at 0x%08lx:\n",
	       ms7720rp_flash_map.map_priv_1);
	flash_mtd = do_map_probe("map_rom", &ms7720rp_flash_map);
	if (!flash_mtd)
		return -ENXIO;
#endif

	flash_mtd->module = THIS_MODULE;

	parts = ms7720rp_flash_partitions;
	nr_parts = NB_OF(ms7720rp_flash_partitions);

	if (nr_parts) {
		add_mtd_partitions(flash_mtd, parts, nr_parts);
	}
	else {
		add_mtd_device(flash_mtd);
	}

	return 0;
}

#ifdef MODULE
static void __exit ms7720rp_maps_cleanup(void)
{
	if (parsed_parts) {
		del_mtd_partitions(flash_mtd);
	}
	else {
		del_mtd_device(flash_mtd);
	}
	map_destroy(flash_mtd);
}
#endif

module_init(ms7720rp_maps_init);
#ifdef MODULE
module_exit(ms7720rp_maps_cleanup);
#endif
