/*
 * katana-xipfl.c
 *
 *  Copyright:	Lineo uSolutions, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 * 
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/arch/MQ9000Hwr.h>


#define FLASH_PARTITION0_ADDR	0x00000000
#define FLASH_PARTITION0_SIZE	0x00100000

#define FLASH_PARTITION1_ADDR	0x00100000
#define FLASH_PARTITION1_SIZE	0x00700000


// x8 mode
static __u8 katana_x8_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + (ofs << 1));
}

static __u16 katana_x8_read16(struct map_info *map, unsigned long ofs)
{
	__u16 val = __raw_readb(map->map_priv_1 + (ofs << 1)) & 0xff;
	val |= (__raw_readb(map->map_priv_1 + (ofs << 1) + 2) << 8) & 0xff00;
	return val;
}

static __u32 katana_x8_read32(struct map_info *map, unsigned long ofs)
{
	__u32 val = __raw_readb(map->map_priv_1 + (ofs << 1)) & 0xff;
	val |= (__raw_readb(map->map_priv_1 + (ofs << 1) + 2) << 8) & 0xff00;
	val |= (__raw_readb(map->map_priv_1 + (ofs << 1) + 4) << 16) & 0xff0000;
	val |= (__raw_readb(map->map_priv_1 + (ofs << 1) + 6) << 24) & 0xff000000;
	return val;
}

static void katana_x8_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	int i;
	__u8 *pt = (__u8 *)to;
	__u32 src = from << 1;
	for (i=0; i<len; i++) {
		*pt = __raw_readb(map->map_priv_1 + src);
		pt++;
		src += 2;
	}
}

static void katana_x8_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + (adr << 1));
	mb();
}

static void katana_x8_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writeb(d & 0xff, map->map_priv_1 + (adr << 1));
	__raw_writeb((d >> 8) & 0xff, map->map_priv_1 + (adr << 1) + 2);
	mb();
}

static void katana_x8_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writeb(d & 0xff, map->map_priv_1 + (adr << 1));
	__raw_writeb((d >> 8) & 0xff, map->map_priv_1 + (adr << 1) + 2);
	__raw_writeb((d >> 16) & 0xff, map->map_priv_1 + (adr << 1) + 4);
	__raw_writeb((d >> 24) & 0xff, map->map_priv_1 + (adr << 1) + 6);
	mb();
}

static void katana_x8_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	int i;
	__u32 dist = to << 1;
	__u8 *pt = (__u8 *)from;
	for (i=0; i<len; i++) {
		__raw_writeb(*pt, map->map_priv_1 + dist);
		pt++;
		dist += 2;
	}
}

// x16 mode
static __u8 katana_x16_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 katana_x16_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static __u32 katana_x16_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

static void katana_x16_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void katana_x16_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

static void katana_x16_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

static void katana_x16_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

static void katana_x16_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

static struct map_info katana_map = {
	name: "AM29LV640ML Flash (x8)",
	size: 0x00800000,
	buswidth: 1,
	read8: katana_x8_read8,
	read16: katana_x8_read16,
	read32: katana_x8_read32,
	copy_from: katana_x8_copy_from,
	write8: katana_x8_write8,
	write16: katana_x8_write16,
	write32: katana_x8_write32,
	copy_to: katana_x8_copy_to,
	map_priv_1: MQ9000_SSRAM_VBASE,
};

static struct mtd_info *katana_mtd = NULL;

struct mtd_partition katana_parts[] = {
	{
		name	: "Bootloader",
		offset	: FLASH_PARTITION0_ADDR,
		size	: FLASH_PARTITION0_SIZE
	},
	{	
		name	: "Persistant storage",
		offset	: FLASH_PARTITION1_ADDR,
		size	: FLASH_PARTITION1_SIZE
	}
};

#define PARTITION_COUNT (sizeof(katana_parts)/sizeof(struct mtd_partition))

int __init init_katana(void)
{
	static char *rom_probe_types[] = { "amd_flash", "jedec_probe", 0 };
	char **chip_type;
	int i;
        volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	pMQRegs->sb.sb07.SRAMCtrl = 0x00010865; // SRAM -> Flash CS change
	chip_type = rom_probe_types;
	for(; !katana_mtd && *chip_type; chip_type++) {
		katana_mtd = do_map_probe(*chip_type, &katana_map);
	}
	if (!katana_mtd) {
		return -ENXIO;
	}

	katana_mtd->module = THIS_MODULE;
	i = add_mtd_partitions(katana_mtd, katana_parts, PARTITION_COUNT);
	if (i != 0)
		return -ENXIO;
	printk(KERN_NOTICE "KATANA board flash device initialized\n");

	return 0;
}

static void __exit cleanup_katana(void)
{
	del_mtd_device(katana_mtd);
	map_destroy(katana_mtd);
}

module_init(init_katana);
module_exit(cleanup_katana);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTD map driver for MediaQ Katana board");

