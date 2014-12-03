/* -------------------------------------------------------------------- */
/* rts7751r2d-flash.c:                                                     */
/* -------------------------------------------------------------------- */
/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Copyright 2003 (c) Lineo uSolutions,Inc.
*/
/* -------------------------------------------------------------------- */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

//#define FLASH4M_16BIT
#define FLASH16M_16BIT

#if defined(FLASH4M_16BIT)
#define RTS7751R2D_FLASH_SIZE 0x00100000
#else
#define RTS7751R2D_FLASH_SIZE 0x01000000
#endif

#define BUSWIDTH   2 

static __u8 rts7751r2d_read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

static __u16 rts7751r2d_read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

static __u32 rts7751r2d_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void rts7751r2d_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to,(map->map_priv_1 + from),len);
}

static void rts7751r2d_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d,map->map_priv_1 + adr);
	
}

static void rts7751r2d_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d,map->map_priv_1 + adr);
}

static void rts7751r2d_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d,map->map_priv_1 + adr);
}

static void rts7751r2d_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to),from,len);
}

#if defined(CONFIG_XIP_KERNEL)
static struct map_info rts7751r2d_rom_map = {
	name:		"SH-Graphic rom",
	buswidth:	BUSWIDTH,
	size:		(RTS7751R2D_FLASH_SIZE / 2),
	read8:		rts7751r2d_read8,
	read16:		rts7751r2d_read16,
	read32:		rts7751r2d_read32,
	copy_from:	rts7751r2d_copy_from,
	write8:		rts7751r2d_write8,
	write16:	rts7751r2d_write16,
	write32:	rts7751r2d_write32,
	copy_to:	rts7751r2d_copy_to,
};
#endif

static struct map_info rts7751r2d_map = {
	name:		"SH-Graphic flash",
	buswidth:	BUSWIDTH,
#if defined(CONFIG_XIP_KERNEL)
	size:		(RTS7751R2D_FLASH_SIZE / 2),
#else
	size:		RTS7751R2D_FLASH_SIZE,
#endif
	read8:		rts7751r2d_read8,
	read16:		rts7751r2d_read16,
	read32:		rts7751r2d_read32,
	copy_from:	rts7751r2d_copy_from,
	write8:		rts7751r2d_write8,
	write16:	rts7751r2d_write16,
	write32:	rts7751r2d_write32,
	copy_to:	rts7751r2d_copy_to,
};


/*
 * Here are partition information for all known SH-Graphic based devices.
 * See include/linux/mtd/partitions.h for definition of the mtd_partition
 * structure.
 * 
 * The *_max_flash_size is the maximum possible mapped flash size which
 * is not necessarily the actual flash size.  It must correspond to the 
 * value specified in the mapping definition defined by the
 * "struct map_desc *_io_desc" for the corresponding machine.
 */

#if defined(FLASH4M_16BIT)
static struct mtd_partition rts7751r2d_partitions[] = {
	{
		name:		"bootloader",
		size:		0x00080000,
		offset:		0x00000000,
		mask_flags:	MTD_WRITEABLE
	},{
		name:		"SH-Graphic jffs2",
		size:		0x00080000,
		offset:		0x00080000
	}
};
#else

#if defined(CONFIG_XIP_KERNEL)
static struct mtd_partition rts7751r2d_rom_partitions[] = {
        {
                name:		"bootloader",
                offset:		0x00000000,
                size:		0x00020000,
                mask_flags:	MTD_WRITEABLE
        },{
                name:		"kernel",
                offset:		0x00020000,
                size:		0x00300000,
                mask_flags:	MTD_WRITEABLE
        },{
                name:		"cramfs",
                offset:		0x00320000,
                size:		0x004e0000,
                mask_flags:	MTD_WRITEABLE
        }
};
#endif

static struct mtd_partition rts7751r2d_partitions[] = {
#if defined(CONFIG_XIP_KERNEL)
        {
                name: "jffs2",
                offset: 0x00000000,
                size:   0x00800000,
        }
#else
        {
                name: "bootloader",
                offset: 0x00000000,
                size:   0x00020000,
                mask_flags: MTD_WRITEABLE
        },{
                name: "kernel",
                offset: 0x00020000,
                size:   0x00300000,
        },{
                name: "cramfs",
                offset: 0x00320000,
                size:   0x004e0000,
        },{
                name: "jffs2",
                offset: 0x00800000,
                size:   0x00800000,
        }
#endif
};
#endif

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_partition *parsed_parts;
static struct mtd_info *flash_mtd;

#if defined(CONFIG_XIP_KERNEL)
static struct mtd_info* rom_mtd   = (struct mtd_info*)0;
#endif

int __init rts7751r2d_mtd_init(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	char *part_type;

#if defined(CONFIG_XIP_KERNEL)
	rts7751r2d_rom_map.map_priv_1	= P2SEGADDR(0);
	rts7751r2d_map.map_priv_1	= P2SEGADDR(0x00800000);
#else
	rts7751r2d_map.map_priv_1	= P2SEGADDR(0);
#endif

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	printk(KERN_NOTICE "RTS7751R2D flash: probing %d-bit flash bus\n", rts7751r2d_map.buswidth*8);

#if defined(CONFIG_XIP_KERNEL)
	rom_mtd = do_map_probe("map_rom", &rts7751r2d_rom_map);
	if (rom_mtd) {
		rom_mtd->module		= THIS_MODULE;
		rom_mtd->erasesize	= 0x10000;

		parts = rts7751r2d_rom_partitions;
		nb_parts = NB_OF(rts7751r2d_rom_partitions);

		if (nb_parts) {
			add_mtd_partitions(rom_mtd, parts, nb_parts);
		}
		else {
			add_mtd_device(rom_mtd);
		}
	}
#endif

#if defined(FLASH4M_16BIT)
	flash_mtd = do_map_probe("jedec_probe", &rts7751r2d_map);
#else
	flash_mtd = do_map_probe("cfi_probe", &rts7751r2d_map);
#endif
	if (!flash_mtd)
		return -ENXIO;

	flash_mtd->module	= THIS_MODULE;
	flash_mtd->erasesize	= 0x10000;

	/*
	 * Static partition definition selection
	 */
	part_type = "static";
	parts = rts7751r2d_partitions;
	nb_parts = NB_OF(rts7751r2d_partitions);

	if (nb_parts == 0) {
		printk(KERN_NOTICE "RTS7751R2D partition info available, registering whole flash at once\n");
		add_mtd_device(flash_mtd);
	} else {
		printk(KERN_NOTICE "Using %s partition definition\n", part_type);
		add_mtd_partitions(flash_mtd, parts, nb_parts);
	}
	return 0;
}

static void __exit rts7751r2d_mtd_cleanup(void)
{
	if (flash_mtd) {
		del_mtd_partitions(flash_mtd);
		map_destroy(flash_mtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
}

module_init(rts7751r2d_mtd_init);
module_exit(rts7751r2d_mtd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo uSolutions,Inc.");
MODULE_DESCRIPTION("MTD map driver for RTS7751R2D base board");
