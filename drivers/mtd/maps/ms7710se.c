/*
 * linux/drivers/mtd/maps/ms7710se.c
 *
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * Flash memory access on Hitachi MS7710SE.
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
#if 0
__u32 ms7710se_read16(struct map_info* map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

void ms7710se_write16(struct map_info* map, __u32 d, unsigned long ofs)
{
	__raw_writew(d, map->map_priv_1 + ofs);
	mb();
}

void ms7710se_copy_from(struct map_info* map,
			void* to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}
#else
__u32 ms7710se_read32(struct map_info *map, unsigned long ofs)
{
        return __raw_readl(map->map_priv_1 + ofs);
}
                                                                                  
void ms7710se_write32(struct map_info *map, __u32 d, unsigned long adr)
{
        __raw_writel(d, map->map_priv_1 + adr);
        mb();
}
                                                                                  
void ms7710se_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t
len)
{
        memcpy_fromio(to, map->map_priv_1 + from, len);
} 
#endif
static struct mtd_partition* parsed_parts = (struct mtd_partition*)0;
static struct mtd_info* eprom_mtd = (struct mtd_info*)0;
static struct mtd_info* flash_mtd = (struct mtd_info*)0;

#define NB_OF(x) (sizeof(x)/sizeof(x[0]))

struct map_info ms7710se_eprom_map = {
	name:      "MS7710SE01 EPROM",
	size:      0x00400000,
	buswidth:  4,
	copy_from: ms7710se_copy_from,
};

struct map_info ms7710se_flash_map = {
	name:      "MS7710SE01 FLASH",
	size:      0x00800000,
//	buswidth:  1,
	buswidth:  4,
#if 0
	read16:    ms7710se_read16,
	write16:   ms7710se_write16,
#else
	read32:    ms7710se_read32,
	write32:   ms7710se_write32,
#endif
	copy_from: ms7710se_copy_from,
};

static struct mtd_partition ms7710se_eprom_partitions[] = {
	{
		name:       "eprom",
		offset:     0x00000000,
		size:       0x00400000,
                mask_flags: MTD_WRITEABLE
        },
};

static struct mtd_partition ms7710se_flash_partitions[] = {
	{
		name:       "mon",
		offset:     0x00000000,
		size:       0x00080000,
                mask_flags: MTD_WRITEABLE
        },
	{
		name:       "kernel",
		offset:     0x00080000,
		size:       0x00180000,
                mask_flags: MTD_WRITEABLE
        },
	{
		name:       "cramfs",
		offset:     0x00200000,
		size:       0x00600000,
                mask_flags: MTD_WRITEABLE
        }
};

static int __init ms7710se_maps_init(void)
{
	struct mtd_partition* parts;
	int nr_parts = 0;

#if defined(CONFIG_XIP_KERNEL)
	ms7710se_flash_map.map_priv_1 = P2SEGADDR(0);
	ms7710se_eprom_map.map_priv_1 = P2SEGADDR(0x01000000);
	flash_mtd = do_map_probe("map_rom", &ms7710se_flash_map);
	if (!flash_mtd)
		return -ENXIO;
#else
#if 1
	ms7710se_eprom_map.map_priv_1 = P1SEGADDR(0x01000000);
	ms7710se_flash_map.map_priv_1 = P2SEGADDR(0);
	printk(KERN_NOTICE "Probing for flash chips at 0x%08lx:\n",
	       ms7710se_flash_map.map_priv_1);
	flash_mtd = do_map_probe("cfi_probe", &ms7710se_flash_map);
	if (!flash_mtd)
		return -ENXIO;
#else
	ms7710se_eprom_map.map_priv_1 = P1SEGADDR(0);
	ms7710se_flash_map.map_priv_1 = P2SEGADDR(0x01000000);
	printk(KERN_NOTICE "Probing for flash chips at 0x%08lx:\n",
	       ms7710se_flash_map.map_priv_1);
	flash_mtd = do_map_probe("cfi_probe", &ms7710se_flash_map);
	if (!flash_mtd) {
		ms7710se_eprom_map.map_priv_1 = P1SEGADDR(0x01000000);
		ms7710se_flash_map.map_priv_1 = P2SEGADDR(0);
		printk(KERN_NOTICE "Probing for flash chips at 0x%08lx:\n",
		       ms7710se_flash_map.map_priv_1);
		flash_mtd = do_map_probe("cfi_probe", &ms7710se_flash_map);
		if (!flash_mtd) {
			printk(KERN_NOTICE "Flash chips not detected.\n");
			ms7710se_flash_map.map_priv_1 = P2SEGADDR(0x01000000);
			ms7710se_eprom_map.map_priv_1 = P2SEGADDR(0);
			flash_mtd = do_map_probe("map_rom",
						 &ms7710se_flash_map);
			if (!flash_mtd)
				return -ENXIO;
		}
	}
#endif
#endif
	eprom_mtd = do_map_probe("map_rom", &ms7710se_eprom_map);
	if (eprom_mtd) {
		eprom_mtd->module = THIS_MODULE;

		parts = ms7710se_eprom_partitions;
		nr_parts = NB_OF(ms7710se_eprom_partitions);

		if (nr_parts) {
			add_mtd_partitions(eprom_mtd, parts, nr_parts);
		}
		else {
		  	add_mtd_device(eprom_mtd);
		}
	}

	flash_mtd->module = THIS_MODULE;

	parts = ms7710se_flash_partitions;
	nr_parts = NB_OF(ms7710se_flash_partitions);

	if (nr_parts) {
		add_mtd_partitions(flash_mtd, parts, nr_parts);
	}
	else {
		add_mtd_device(flash_mtd);
	}

	return 0;
}

#ifdef MODULE
static void __exit ms7710se_maps_cleanup(void)
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

module_init(ms7710se_maps_init);
#ifdef MODULE
module_exit(ms7710se_maps_cleanup);
#endif
