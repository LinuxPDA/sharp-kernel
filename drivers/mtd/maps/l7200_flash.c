/*
 * linux/drivers/mtd/l7200_flash.c
 * 
 * flash access on LinkUp 7200 based devices
 * 
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on drivers/mtd/sa1100-flash.c
 * 
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>

#define DEBUGGING 0

static struct mtd_info* mymtd = (struct mtd_info*)0;

static __u8 l7200_read8(struct map_info* map, unsigned long ofs)
{
	return *(__u8*)(FLASH1_BASE + ofs);
}

static __u16 l7200_read16(struct map_info* map, unsigned long ofs)
{
	return *(__u16*)(FLASH1_BASE + ofs);
}

static __u32 l7200_read32(struct map_info* map, unsigned long ofs)
{
	return *(__u32*)(FLASH1_BASE + ofs);
}

static void l7200_copy_from(struct map_info* map,
			    void* to, unsigned long from, ssize_t len)
{
	memcpy(to, (void*)(FLASH1_BASE + from), len);
}

static void l7200_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(FLASH1_BASE + adr) = d;
}

static void l7200_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(FLASH1_BASE + adr) = d;
}

static void l7200_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(FLASH1_BASE + adr) = d;
}

static void l7200_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(FLASH1_BASE + to), from, len);
}

#if defined(CONFIG_L7205SDB)
static void l7200sdb_set_vpp(int vpp)
{
	IO_PEDDR |= 0x80;
	if (vpp)
		IO_PEDR &= ~0x80;
	else
		IO_PEDR |= 0x80;
}
#endif

static struct map_info l7200_mtdflash_map = {
	name:		"mtdflash0",
	read8:		l7200_read8,
	read16:		l7200_read16,
	read32:		l7200_read32,
	copy_from:	l7200_copy_from,
	write8:         l7200_write8,
	write16:        l7200_write16,
	write32:        l7200_write32,
	copy_to:        l7200_copy_to,
#if defined(CONFIG_L7205SDB)
	set_vpp:	l7200sdb_set_vpp,
#endif
};

#if defined(CONFIG_L7205SDB)
/* for LinkUp FlashUtility */
#define BOOT_LOADER_SIZE ((688 + 3) & ~3)	/* size of Boot_SDB.bin */
typedef struct {
	unsigned int size;
	unsigned int rom_address;
	unsigned int sdram_address;
} copy_entry_info;
static unsigned long l7205sdb_max_flash_size = 0x01000000;
static struct mtd_partition l7205sdb_partitions[] = {
	{
		name: "root cramfs",
		mask_flags: MTD_WRITEABLE  /* force read-only */
	},
	{
		name:	"user cramfs",
		offset:	0x800000,
		size:	0x800000,
	}
};
#endif

#define NB_OF(x) (sizeof(x)/sizeof(x[0]))

int __init l7200_flash_init(void)
{
	struct mtd_partition* parts;
	int nb_parts = 0;
	int buswidth = 4;
	
#if defined(CONFIG_L7205SDB)
	{
	  	copy_entry_info* copy_entry =
			(copy_entry_info*)(FLASH1_BASE + BOOT_LOADER_SIZE);
		l7205sdb_partitions[0].offset =
		  	(unsigned long)copy_entry[1].rom_address;
		l7205sdb_partitions[0].size   = 
			0x00800000 - (unsigned long)copy_entry[1].rom_address;
#if DEBUGGING
		{
		  	char* p;
			int i,j;
			for (j=0;j<3;j++) {
			  	printk("[%d] %08x ",j,copy_entry[j].size);
				printk("%08x ",copy_entry[j].rom_address);
				printk("%08x \n",copy_entry[j].sdram_address);
				p = (char*)(copy_entry[j].rom_address +
					    FLASH1_BASE);
				printk("%08x\n", p);
				for (i = 0; i < 16; i++) printk("%02x ",*p++);
					printk("\n");
			}
		}
#endif
	}
	parts = l7205sdb_partitions;
	nb_parts = NB_OF(l7205sdb_partitions);
	l7200_mtdflash_map.size = l7205sdb_max_flash_size;
#endif
	if (!nb_parts) {
		/* printk(KERN_WARNING "l7200_mtdrom: No parts.\n"); */
		return -ENXIO;
	}
	l7200_mtdflash_map.buswidth = buswidth;
/*
	printk(KERN_NOTICE
	       "l7200_mtdflash: probing for %d partitions "
	       "(buswidth = %d)\n", nb_parts, buswidth);
*/
	mymtd = cfi_probe(&l7200_mtdflash_map);
	if (!mymtd) {
		return -ENXIO;
	}

	mymtd->module = THIS_MODULE;

	add_mtd_partitions(mymtd, parts, nb_parts);

	printk(KERN_NOTICE "LinkUp 7200 flash access initialized.\n");

	return 0;
}

static void __exit l7200_flash_cleanup(void)
{
	if (mymtd) {
	  	del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
}

module_init(l7200_flash_init);
module_exit(l7200_flash_cleanup);
