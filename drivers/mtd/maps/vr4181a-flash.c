/*
 * Flash memory access on VR4181A based devices
 * 
 * (C) 2000 Nicolas Pitre <nico@cam.org>
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

static __u8 vr4181a_read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

static __u16 vr4181a_read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

static __u32 vr4181a_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void vr4181a_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void vr4181a_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void vr4181a_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void vr4181a_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void vr4181a_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info vr4181a_map = {
	name:		"VR4181A flash",
	read8:		vr4181a_read8,
	read16:		vr4181a_read16,
	read32:		vr4181a_read32,
	copy_from:	vr4181a_copy_from,
	write8:		vr4181a_write8,
	write16:	vr4181a_write16,
	write32:	vr4181a_write32,
	copy_to:	vr4181a_copy_to,
};


/*
 * Here are partition information for all known VR4181A-based devices.
 * See include/linux/mtd/partitions.h for definition of the mtd_partition
 * structure.
 * 
 * The *_max_flash_size is the maximum possible mapped flash size which
 * is not necessarily the actual flash size.  It must correspond to the 
 * value specified in the mapping definition defined by the
 * "struct map_desc *_io_desc" for the corresponding machine.
 */


#ifdef CONFIG_TOADKK_TCS8000 /*@@@@@*/
#define WINDOW_ADDR	0xbf000000
static unsigned long tcs8000_max_flash_size = 0x01000000;
static struct mtd_partition tcs8000_partitions[] = {
	{ 
		name:		"root file system",
		offset:		0x00000000,
		size:		0x00c00000,
	},{
		name:		"monitor",
		offset:		0x00c00000,
		size:		0x00040000,
                mask_flags:	MTD_WRITEABLE,
	},{
		name:		"monitor II",
		offset:		0x00c40000,
		size:		0x00040000,
                mask_flags:	MTD_WRITEABLE,
	},{
		name:		"configration save area",
		offset:		0x00c80000,
		size:		0x00080000,
	},{
		name:		"kernel",
		offset:		0x00d00000,
		size:		0x00300000,
	},
};
#endif /* CONFIG_TOADKK_TCS8000 */

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))


extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);
extern int parse_bootldr_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static struct mtd_partition *parsed_parts;
static struct mtd_info *mymtd;

int __init vr4181a_mtd_init(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	char *part_type;
	
	/* Default flash buswidth */
	int gpio;
	
	vr4181a_map.buswidth = 2;
	vr4181a_map.map_priv_1 = WINDOW_ADDR;

	/*
	 * Static partition definition selection
	 */
	part_type = "static";


#ifdef CONFIG_TOADKK_TCS8000
	parts = tcs8000_partitions;
	nb_parts = NB_OF(tcs8000_partitions);
	vr4181a_map.size = tcs8000_max_flash_size;
#endif

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	printk(KERN_NOTICE "VR4181A flash: probing %d-bit flash bus\n", vr4181a_map.buswidth*8);
#if 1
	mymtd = do_map_probe("cfi_probe", &vr4181a_map);
#else
	mymtd = do_map_probe("map_rom", &vr4181a_map);
#endif
	if (!mymtd)
		return -ENXIO;
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
		printk(KERN_NOTICE "VR4181A flash: no partition info available, registering whole flash at once\n");
		add_mtd_device(mymtd);
	} else {
		printk(KERN_NOTICE "Using %s partition definition\n", part_type);
		add_mtd_partitions(mymtd, parts, nb_parts);
	}
	return 0;
}

static void __exit vr4181a_mtd_cleanup(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	*(unsigned long *)0xbfc00000 = 0x00ff00ff;	/* force array mode */
}

module_init(vr4181a_mtd_init);
module_exit(vr4181a_mtd_cleanup);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("VR4181A CFI map driver");
MODULE_LICENSE("GPL");
