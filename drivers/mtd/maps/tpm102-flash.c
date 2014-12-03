/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * drivers/mtd/maps/tpm102-flash.c
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>


#define WINDOW_ADDR 	APLAT_FLASH_BASE
#define WINDOW_SIZE 	16*1024*1024
#define BUSWIDTH 	4

static __u8 tpm102_read8(struct map_info *map, unsigned long ofs)
{
    return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 tpm102_read16(struct map_info *map, unsigned long ofs)
{
    return __raw_readw(map->map_priv_1 + ofs);
}

static __u32 tpm102_read32(struct map_info *map, unsigned long ofs)
{
    return __raw_readl(map->map_priv_1 + ofs);
}

static void tpm102_copy_from(struct map_info *map, void *to,
			     unsigned long from, ssize_t len)
{
    memcpy_fromio(to, (void *)(map->map_priv_1 + from), len);
}

static void tpm102_write8(struct map_info *map, __u8 d, unsigned long adr)
{
    __raw_writeb(d, map->map_priv_1 + adr);
}

static void tpm102_write16(struct map_info *map, __u16 d, unsigned long adr)
{
    __raw_writew(d, map->map_priv_1 + adr);
}

static void tpm102_write32(struct map_info *map, __u32 d, unsigned long adr)
{
    __raw_writel(d, map->map_priv_1 + adr);
}

static void tpm102_copy_to(struct map_info *map, unsigned long to,
			   const void *from, ssize_t len)
{
    memcpy_toio((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info tpm102_map = {
	name:		"TPM102 flash",
	size:		WINDOW_SIZE,
	buswidth:	BUSWIDTH,
	read8:		tpm102_read8,
	read16:		tpm102_read16,
	read32:		tpm102_read32,
	copy_from:	tpm102_copy_from,
	write8:		tpm102_write8,
	write16:	tpm102_write16,
	write32:	tpm102_write32,
	copy_to:	tpm102_copy_to
};

static struct mtd_partition tpm102_partitions[] = {
	{
		name:		"Kernel",
		size:		0x00200000,
		offset:		0x00000000,
	},{
		name:		"Filesystem",
		size:		0x00e00000,
		offset:		0x00200000
	}
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *mymtd;
static struct mtd_partition *parsed_parts;

extern int parse_redboot_partitions(struct mtd_info *master,
				    struct mtd_partition **pparts);

static int __init init_tpm102(void)
{
    struct mtd_partition *parts;
    int nb_parts = 0;
    int parsed_nr_parts = 0;
    char *part_type = "static";

    printk("Probing TPM102 flash at physical address 0x%08x\n", WINDOW_ADDR);
    tpm102_map.map_priv_1 = (unsigned long)__ioremap(WINDOW_ADDR,
						     WINDOW_SIZE, 0);
    if (!tpm102_map.map_priv_1) {
	printk("Failed to ioremap\n");
	return -EIO;
    }
#ifdef CONFIG_XIP_KERNEL
    mymtd = do_map_probe("map_rom", &tpm102_map);
#else
    mymtd = do_map_probe("cfi_probe", &tpm102_map);
#endif
    if (!mymtd) {
	iounmap((void *)tpm102_map.map_priv_1);
	return -ENXIO;
    }
    mymtd->module = THIS_MODULE;

    if (parsed_nr_parts > 0) {
	parts = parsed_parts;
	nb_parts = parsed_nr_parts;
    } else {
	parts = tpm102_partitions;
	nb_parts = NB_OF(tpm102_partitions);
    }
    if (nb_parts) {
	printk(KERN_NOTICE "Using %s partition definition\n", part_type);
	add_mtd_partitions(mymtd, parts, nb_parts);
    } else {
	add_mtd_device(mymtd);
    }
    return 0;
}

static void __exit cleanup_tpm102(void)
{
    if (mymtd) {
	del_mtd_partitions(mymtd);
	map_destroy(mymtd);
	if (parsed_parts)
	    kfree(parsed_parts);
    }
    if (tpm102_map.map_priv_1)
	iounmap((void *)tpm102_map.map_priv_1);
    return 0;
}

module_init(init_tpm102);
module_exit(cleanup_tpm102);

