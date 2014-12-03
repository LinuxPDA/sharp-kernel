/*
 * linux/drivers/mtd/maps/ms7727rp.c
 *
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * Flash memory access on Hitachi MS7727RP02.
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

__u32 ms7727rp_read16(struct map_info* map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

void ms7727rp_write16(struct map_info* map, __u32 d, unsigned long ofs)
{
	__raw_writew(d, map->map_priv_1 + ofs);
	mb();
}

void ms7727rp_copy_from(struct map_info* map,
			void* to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static struct mtd_partition* parsed_parts = (struct mtd_partition*)0;
static struct mtd_info* eprom_mtd = (struct mtd_info*)0;
static struct mtd_info* flash_mtd = (struct mtd_info*)0;

#define NB_OF(x) (sizeof(x)/sizeof(x[0]))

struct map_info ms7727rp_eprom_map = {
	name:      "MS7727RP01 EPROM",
	size:      0x00400000,
	buswidth:  4,
	copy_from: ms7727rp_copy_from,
};

struct map_info ms7727rp_flash_map = {
	name:      "MS7727RP01 FLASH",
	size:      0x01000000,
	buswidth:  2,
	read16:    ms7727rp_read16,
	write16:   ms7727rp_write16,
	copy_from: ms7727rp_copy_from,
};

static struct mtd_partition ms7727rp_eprom_partitions[] = {
	{
		name:       "eprom",
		offset:     0x00000000,
		size:       0x00100000,
                mask_flags: MTD_WRITEABLE
        },
};

static struct mtd_partition ms7727rp_flash_partitions[] = {
	{
		name:       "kernel",
		offset:     0x00000000,
		size:       0x00200000,
                mask_flags: MTD_WRITEABLE
        },
	{
		name:       "cramfs",
		offset:     0x00200000,
		size:       0x00e00000,
                mask_flags: MTD_WRITEABLE
        }
};

static int __init ms7727rp_maps_init(void)
{
	struct mtd_partition* parts;
	int nr_parts = 0;

#if defined(CONFIG_XIP_KERNEL)
	ms7727rp_flash_map.map_priv_1 = P2SEGADDR(0);
	ms7727rp_eprom_map.map_priv_1 = P2SEGADDR(0x01000000);
	flash_mtd = do_map_probe("map_rom", &ms7727rp_flash_map);
	if (!flash_mtd)
		return -ENXIO;
#else
	ms7727rp_eprom_map.map_priv_1 = P1SEGADDR(0);
	ms7727rp_flash_map.map_priv_1 = P2SEGADDR(0x01000000);
	printk(KERN_NOTICE "Probing for flash chips at 0x%08lx:\n",
	       ms7727rp_flash_map.map_priv_1);
	flash_mtd = do_map_probe("cfi_probe", &ms7727rp_flash_map);
	if (!flash_mtd) {
		ms7727rp_eprom_map.map_priv_1 = P1SEGADDR(0x01000000);
		ms7727rp_flash_map.map_priv_1 = P2SEGADDR(0);
		printk(KERN_NOTICE "Probing for flash chips at 0x%08lx:\n",
		       ms7727rp_flash_map.map_priv_1);
		flash_mtd = do_map_probe("cfi_probe", &ms7727rp_flash_map);
		if (!flash_mtd) {
			printk(KERN_NOTICE "Flash chips not detected.\n");
			ms7727rp_flash_map.map_priv_1 = P2SEGADDR(0x01000000);
			ms7727rp_eprom_map.map_priv_1 = P2SEGADDR(0);
			flash_mtd = do_map_probe("map_rom",
						 &ms7727rp_flash_map);
			if (!flash_mtd)
				return -ENXIO;
		}
	}
#endif
	eprom_mtd = do_map_probe("map_rom", &ms7727rp_eprom_map);
	if (eprom_mtd) {
		eprom_mtd->module = THIS_MODULE;

		parts = ms7727rp_eprom_partitions;
		nr_parts = NB_OF(ms7727rp_eprom_partitions);

		if (nr_parts) {
			add_mtd_partitions(eprom_mtd, parts, nr_parts);
		}
		else {
		  	add_mtd_device(eprom_mtd);
		}
	}

	flash_mtd->module = THIS_MODULE;

	parts = ms7727rp_flash_partitions;
	nr_parts = NB_OF(ms7727rp_flash_partitions);

	if (nr_parts) {
		add_mtd_partitions(flash_mtd, parts, nr_parts);
	}
	else {
		add_mtd_device(flash_mtd);
	}

	return 0;
}

#ifdef MODULE
static void __exit ms7727rp_maps_cleanup(void)
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

module_init(ms7727rp_maps_init);
#ifdef MODULE
module_exit(ms7727rp_maps_cleanup);
#endif
