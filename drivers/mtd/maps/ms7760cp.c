/*
 * linux/drivers/mtd/maps/ms7760cp.c
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * Flash memory access on MS7760CP01
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

static __u16 ms7760cp_read16(struct map_info* map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static void ms7760cp_write16(struct map_info* map, __u16 d, unsigned long ofs)
{
	__raw_writew(d, map->map_priv_1 + ofs);
	mb();
}

static void ms7760cp_copy_from(struct map_info* map,
			       void* to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

#ifdef MODULE
static struct mtd_partition* parsed_parts = (struct mtd_partition*)0;
#endif
static struct mtd_info* eprom_mtd = (struct mtd_info*)0;
static struct mtd_info* flash_mtd = (struct mtd_info*)0;

#define NB_OF(x) (sizeof(x)/sizeof(x[0]))

struct map_info ms7760cp_eprom_map = {
	name:      "MS7760CP01 EPROM",
	size:      0x00400000,
	buswidth:  4,
	copy_from: ms7760cp_copy_from,
};

struct map_info ms7760cp_flash_map = {
	name:      "MS7760CP01 FLASH",
	size:      0x00800000,
	buswidth:  2,
	read16:    ms7760cp_read16,
	write16:   ms7760cp_write16,
	copy_from: ms7760cp_copy_from,
};

static struct mtd_partition ms7760cp_eprom_partitions[] = {
	{
		name:       "eprom",
		offset:     0x00000000,
		size:       0x00400000,
                mask_flags: MTD_WRITEABLE
        },
};

static struct mtd_partition ms7760cp_flash_partitions[] = {
	{
		name:       "bootloader",
		offset:     0x00000000,
		size:       0x00040000,
                mask_flags: MTD_WRITEABLE
        },
	{
		name:       "kernel",
		offset:     0x00040000,
		size:       0x001c0000,
                mask_flags: MTD_WRITEABLE
        },
	{
		name:       "fs",
		offset:     0x00200000,
		size:       0x00600000,
                mask_flags: 0 
              /*  mask_flags: MTD_WRITEABLE */
        }
};

static int __init ms7760cp_maps_init(void)
{
	struct mtd_partition* parts;
	int nr_parts = 0;

#if defined(CONFIG_XIP_KERNEL)
	ms7760cp_flash_map.map_priv_1 = P2SEGADDR(0);
	ms7760cp_eprom_map.map_priv_1 = P2SEGADDR(0x01000000);
	flash_mtd = do_map_probe("map_rom", &ms7760cp_flash_map);
	if (!flash_mtd)
		return -ENXIO;
#else
	ms7760cp_eprom_map.map_priv_1 = P1SEGADDR(0);
	ms7760cp_flash_map.map_priv_1 = P2SEGADDR(0x01000000);
	printk(KERN_NOTICE "Probing for flash chips at 0x%08lx:\n",
	       ms7760cp_flash_map.map_priv_1);
	flash_mtd = do_map_probe("cfi_probe", &ms7760cp_flash_map);
	if (!flash_mtd) {
		ms7760cp_eprom_map.map_priv_1 = P1SEGADDR(0x01000000);
		ms7760cp_flash_map.map_priv_1 = P2SEGADDR(0);
		printk(KERN_NOTICE "Probing for flash chips at 0x%08lx:\n",
		       ms7760cp_flash_map.map_priv_1);
		flash_mtd = do_map_probe("cfi_probe", &ms7760cp_flash_map);
		if (!flash_mtd) {
			printk(KERN_NOTICE "Flash chips not detected.\n");
			return -ENXIO;
		}
	}
#endif
	flash_mtd->module = THIS_MODULE;

	parts = ms7760cp_flash_partitions;
	nr_parts = NB_OF(ms7760cp_flash_partitions);

	if (nr_parts) {
		add_mtd_partitions(flash_mtd, parts, nr_parts);
	}
	else {
		add_mtd_device(flash_mtd);
	}

	eprom_mtd = do_map_probe("map_rom", &ms7760cp_eprom_map);
	if (eprom_mtd) {
		eprom_mtd->module = THIS_MODULE;

		parts = ms7760cp_eprom_partitions;
		nr_parts = NB_OF(ms7760cp_eprom_partitions);

		if (nr_parts) {
			add_mtd_partitions(eprom_mtd, parts, nr_parts);
		}
		else {
		  	add_mtd_device(eprom_mtd);
		}
	}

	return 0;
}

#ifdef MODULE
static void __exit ms7760cp_maps_cleanup(void)
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

module_init(ms7760cp_maps_init);
#ifdef MODULE
module_exit(ms7760cp_maps_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
MODULE_DESCRIPTION("MTD map driver for MS7760CP01");
