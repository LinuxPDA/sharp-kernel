/*
 * Flash memory access on Mx21 based devices
 * 
 * (C) 2000 Nicolas Pitre <nico@cam.org>
 * 
 * $Id: mx21-flash.c,v 1.26 2002/03/13 16:30:44 rmk Exp $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/io.h>


#ifndef CONFIG_ARCH_MX2ADS
#error This is for MX2ADS architecture only
#endif


#define WINDOW_ADDR 0xC8000000

static __u8 mx21_read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

static __u16 mx21_read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

static __u32 mx21_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void mx21_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void mx21_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void mx21_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void mx21_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void mx21_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info mx21_map = {
	name:		"Motorola Mx2 Flash",
	read8:		mx21_read8,
	read16:		mx21_read16,
	read32:		mx21_read32,
	copy_from:	mx21_copy_from,
	write8:		mx21_write8,
	write16:	mx21_write16,
	write32:	mx21_write32,
	copy_to:	mx21_copy_to,

	map_priv_1:	WINDOW_ADDR,
	map_priv_2:	-1,
};


/*
 * Here are partition information for all known SA1100-based devices.
 * See include/linux/mtd/partitions.h for definition of the mtd_partition
 * structure.
 *
 * The *_max_flash_size is the maximum possible mapped flash size which
 * is not necessarily the actual flash size.  It must be no more than
 * the value specified in the "struct map_desc *_io_desc" mapping
 * definition for the corresponding machine.
 *
 * Please keep these in alphabetical order, and formatted as per existing
 * entries.  Thanks.
 */

#define MX2ADS_FLASH_SIZE		0x02000000
static struct mtd_partition mx2ads_partitions[] =
{
#if 0
	{
		name:		"bootloader",
		size:		0x00100000,
		offset:		0x00000000
	//	mask_flags:	MTD_WRITEABLE
	}, 
	{
		name:		"kernel",
		size:		0x00200000,
		offset:		MTDPART_OFS_APPEND
	//	mask_flags:	MTD_WRITEABLE
	}, 
/*	{	name:           "file system1",
		size:           0x01000000,
		offset:		MTDPART_OFS_APPEND
	},
*/	{
		name:		"file system",
		size:		MTDPART_SIZ_FULL,
	//	mask_flags:	MTD_WRITEABLE,
		offset:		MTDPART_OFS_APPEND
	}
#else
	{
		name:		"bootldr",
		size:		0x00020000,
		offset:		0x00000000
	}, 
	{
		name:		"bootflag",
		size:		0x00020000,
		offset:		MTDPART_OFS_APPEND
	}, 
	{
		name:		"kernel",
		size:		0x001e0000,
		offset:		MTDPART_OFS_APPEND
	}, 
	{
		name:		"kernel2",
		size:		0x001e0000,
		offset:		MTDPART_OFS_APPEND
	},
	{
		name:		"file system",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND
	},
#endif
};


extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);
extern int parse_bootldr_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static struct mtd_partition *parsed_parts;
static struct mtd_info *mymtd;

int __init mx21_mtd_init(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0, ret;
	int parsed_nr_parts = 0;
	const char *part_type;
	unsigned long base = -1UL;

	/* Default flash buswidth */
	mx21_map.buswidth = 4;

	base=WINDOW_ADDR;
	/*
	 * Static partition definition selection
	 */
	part_type = "static";

	parts = mx2ads_partitions;
	nb_parts = ARRAY_SIZE(mx2ads_partitions);
	mx21_map.size = MX2ADS_FLASH_SIZE;
	
	/*
	 * For simple flash devices, use ioremap to map the flash.
	 */
	if (base != (unsigned long)-1) 
	{
		if (!request_mem_region(base, mx21_map.size, "flash"))
			return -EBUSY;
		mx21_map.map_priv_2 = base;
		mx21_map.map_priv_1 = (unsigned long)
				ioremap(base, mx21_map.size);
		ret = -ENOMEM;
		if (!mx21_map.map_priv_1)
			goto out_err;
	}

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	printk(KERN_NOTICE "MX2Ads flash: probing %d-bit flash bus\n", mx21_map.buswidth*8);
#ifdef CONFIG_XIP_KERNEL
	mymtd = do_map_probe("map_rom", &mx21_map);
#else
	mymtd = do_map_probe("cfi_probe", &mx21_map);
#endif
	ret = -ENXIO;
	if (!mymtd)
		goto out_err;
	mymtd->module = THIS_MODULE;

	/*
	 * Dynamic partition selection stuff (might override the static ones)
	 */
#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(mymtd, &parsed_parts);

		if (ret > 0) {
			part_type = "RedBoot";
			parsed_nr_parts = ret;
		}
	}
#endif
#ifdef CONFIG_MTD_BOOTLDR_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_bootldr_partitions(mymtd, &parsed_parts);
		if (ret > 0) {
			part_type = "Compaq bootldr";
			parsed_nr_parts = ret;
		}
	}
#endif

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	}

	if (nb_parts == 0) {
		printk(KERN_NOTICE "MX21 flash: no partition info available, registering whole flash at once\n");
		add_mtd_device(mymtd);
	} else 
	{
		printk(KERN_NOTICE "Using %s partition definition\n", part_type);
		add_mtd_partitions(mymtd, parts, nb_parts);
	}
	return 0;

 out_err:
	if (mx21_map.map_priv_2 != -1) 
	{
		iounmap((void *)mx21_map.map_priv_1);
		release_mem_region(mx21_map.map_priv_2, mx21_map.size);
	}
	return ret;
}

static void __exit mx21_mtd_cleanup(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (mx21_map.map_priv_2 != -1) {
		iounmap((void *)mx21_map.map_priv_1);
		release_mem_region(mx21_map.map_priv_2, mx21_map.size);
	}
}

module_init(mx21_mtd_init);
module_exit(mx21_mtd_cleanup);

MODULE_AUTHOR("Frank Li zhili@motorola.com");
MODULE_DESCRIPTION("MX2ADS CFI map driver");
MODULE_LICENSE("GPL");
