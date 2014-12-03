/*
 * linux/drivers/mtd/devices/l7200_mtdrom.c
 * 
 * ROM access on LinkUp 7200 based devices
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

#include <linux/pm.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/delay.h>

#include <asm/hardware.h>

#define DEBUGGING 0

static struct mtd_info* mymtd = (struct mtd_info*)0;
static struct mtd_info* mymtd_iocs1 = (struct mtd_info*)0;

static __u8 l7200_mtdrom_read8(struct map_info* map, unsigned long ofs)
{
	return *(__u8*)(FLASH1_BASE + ofs);
}

static __u16 l7200_mtdrom_read16(struct map_info* map, unsigned long ofs)
{
	return *(__u16*)(FLASH1_BASE + ofs);
}

static __u32 l7200_mtdrom_read32(struct map_info* map, unsigned long ofs)
{
	return *(__u32*)(FLASH1_BASE + ofs);
}

static void l7200_mtdrom_copy_from(struct map_info* map,
			    void* to, unsigned long from, ssize_t len)
{
	memcpy(to, (void*)(FLASH1_BASE + from), len);
}

static struct map_info l7200_mtdrom_map = {
	name:		"mtdrom0",
	read8:		l7200_mtdrom_read8,
	read16:		l7200_mtdrom_read16,
	read32:		l7200_mtdrom_read32,
	copy_from:	l7200_mtdrom_copy_from,
};

#if defined(CONFIG_IRIS)

static __u8 l7200_iocs1_mtdrom_read8(struct map_info* map, unsigned long ofs)
{
	return *(__u8*)(FLASH2_BASE + ofs);
}

static __u16 l7200_iocs1_mtdrom_read16(struct map_info* map, unsigned long ofs)
{
	return *(__u16*)(FLASH2_BASE + ofs);
}

static __u32 l7200_iocs1_mtdrom_read32(struct map_info* map, unsigned long ofs)
{
	return *(__u32*)(FLASH2_BASE + ofs);
}

static void l7200_iocs1_mtdrom_copy_from(struct map_info* map,
			    void* to, unsigned long from, ssize_t len)
{
	memcpy(to, (void*)(FLASH2_BASE + from), len);
}

static struct map_info l7200_iocs1_mtdrom_map = {
	name:		"mtdrom1",
	read8:		l7200_iocs1_mtdrom_read8,
	read16:		l7200_iocs1_mtdrom_read16,
	read32:		l7200_iocs1_mtdrom_read32,
	copy_from:	l7200_iocs1_mtdrom_copy_from,
};


#include <asm/arch/irqs.h>
#include <asm/arch/gpio.h>

#define FLASH_GPIO_WP     bitPB0   /* set HI to enable writing */
#define FLASH_GPIO_RP     bitPB1   /* set HI to enable reading */
#define FLASH_GPIO_VCC    bitPC5   /* set HI to activate VCC to enable writing */

static void irisrom_setup_gpio(void)
{
  /* setup GPIO input/output */
  SET_PB_OUT(FLASH_GPIO_WP|FLASH_GPIO_RP);
  SET_PBSBSR_LO(FLASH_GPIO_WP|FLASH_GPIO_RP);
  SET_PC_OUT(FLASH_GPIO_VCC);
  SET_PCSBSR_LO(FLASH_GPIO_VCC);
}

#define FLASH_VCC_POWERON_WAITLOOP  250000 /* more than 10ms needed */
static void irisrom_enable_write(void)
{
  volatile int i;
  SET_PCDR_HI(FLASH_GPIO_VCC);
  for(i=0;i<FLASH_VCC_POWERON_WAITLOOP;i++){}
  SET_PBDR_HI(FLASH_GPIO_WP);
}

static void irisrom_disable_write(void)
{
  SET_PBDR_LO(FLASH_GPIO_WP);
  SET_PCDR_LO(FLASH_GPIO_VCC);
}

static void irirrom_power_on(void)
{
  SET_PBDR_HI(FLASH_GPIO_RP);
}

static void irirrom_power_off(void)
{
  SET_PBDR_LO(FLASH_GPIO_RP);
}

#define CMD_READ_ID		0x90

#endif /* CONFIG_IRIS */

#if defined(CONFIG_L7205SDB)
/* for LinkUp FlashUtility */
#define BOOT_LOADER_SIZE ((688 + 3) & ~3)	/* size of Boot_SDB.bin */
typedef struct {
	unsigned int size;
	unsigned int rom_address;
	unsigned int sdram_address;
} copy_entry_info;
static unsigned long l7205sdb_max_rom_size = 0x01000000;
static struct mtd_partition l7205sdb_partitions[] = {
{
	name: "mtdrom",
	mask_flags: MTD_WRITEABLE  /* force read-only */
}
};
#elif defined(CONFIG_IRIS)
/* for MMC loader */
#define COPY_ENTRY_INFO_OFFSET 0x40
typedef struct {
	unsigned int rom_address;
	unsigned int sdram_address;
	unsigned int max_size;
	unsigned int dummy;
} copy_entry_info;
static unsigned long iris_max_rom_size = 0x00A00000; /* 12MB */
static struct mtd_partition iris_partitions[] = {
  {
    name: "iocs0rom0",
    mask_flags: MTD_WRITEABLE  /* force read-only */
  },
  {
    name: "iocs0rom1",
    mask_flags: MTD_WRITEABLE  /* force read-only */
  }
};
static struct mtd_partition iris_iocs1_partitions[] = {
  {
    name: "iocs1rom0",
    mask_flags: MTD_WRITEABLE /* force read-only */
  },
  {
    name: "iocs1rom1",
    mask_flags: MTD_WRITEABLE /* force read-only */
  },
  {
    name: "iocs1rom2",
    mask_flags: MTD_WRITEABLE /* force read-only */
  },
  {
    name: "iocs1rom3",
    mask_flags: 0 /* force read-only */
  },
  {
    name: "iocs1rom4",
    mask_flags: MTD_WRITEABLE /* force read-only */
  }
};

void l7200_iocs1_mtdrom_write8(struct map_info *map, __u8 d, unsigned long adr)
{
  //printk("WRITING8 IOCS1 data=%x adr=%x\n",d,adr);
  *(__u8 *)(FLASH2_BASE + adr) = d;
}

void l7200_iocs1_mtdrom_write16(struct map_info *map, __u16 d, unsigned long adr)
{
  //printk("WRITING16 IOCS1 data=%x adr=%x\n",d,adr);
  *(__u16 *)(FLASH2_BASE + adr) = d;
}

void l7200_iocs1_mtdrom_write32(struct map_info *map, __u32 d, unsigned long adr)
{
  //printk("WRITING32 IOCS1 data=%x adr=%x\n",d,adr);
  *(__u32 *)(FLASH2_BASE + adr) = d;
}

void l7200_iocs1_mtdrom_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
  memcpy((void *)(FLASH2_BASE + to), from, len);
}

static struct map_info iris_cfi_map =
{
	name:		"IRIS-IOCS1",
	read8:		l7200_iocs1_mtdrom_read8,
	read16:		l7200_iocs1_mtdrom_read16,
	read32:		l7200_iocs1_mtdrom_read32,
	copy_from:	l7200_iocs1_mtdrom_copy_from,
	write8:		l7200_iocs1_mtdrom_write8,
	write16:	l7200_iocs1_mtdrom_write16,
	write32:	l7200_iocs1_mtdrom_write32,
	copy_to:	l7200_iocs1_mtdrom_copy_to,
};

#ifdef CONFIG_PM

static struct pm_dev *iris_mtdrom_pm_dev; /* registered PM device */

static int iris_mtdrom_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
  static int suspended = 0;
  switch (req) {
  case PM_STANDBY:
  case PM_BLANK:
  case PM_SUSPEND:
    if( suspended ) break;
    irisrom_disable_write();
    irirrom_power_off();
    suspended = 1;
    break;
  case PM_UNBLANK:
  case PM_RESUME:
    if( ! suspended ) break;
    irirrom_power_on();
    irisrom_enable_write();
    udelay(10000); /* wait 10ms */
    suspended = 0;
    break;
  }
  return 0;
}

#endif
#endif


#define NB_OF(x) (sizeof(x)/sizeof(x[0]))

int __init l7200_mtdrom_init(void)
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
		l7205sdb_partitions[0].size   = 0x00400000; /* 4MB */
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
	l7200_mtdrom_map.size = l7205sdb_max_rom_size;
#elif defined(CONFIG_IRIS)

#ifdef CONFIG_PM
	iris_mtdrom_pm_dev = pm_register(PM_SYS_DEV, 0, iris_mtdrom_pm_callback);
#endif

	{
	  	copy_entry_info* copy_entry =
			(copy_entry_info*)(FLASH1_BASE +
					   COPY_ENTRY_INFO_OFFSET);
		iris_partitions[0].offset = 
			copy_entry[1].rom_address - 0x24000000;
		iris_partitions[0].size = copy_entry[1].max_size;

		iris_partitions[1].offset = 
			copy_entry[2].rom_address - 0x24000000;
		iris_partitions[1].size = copy_entry[2].max_size;

#if DEBUGGING
		{
		  	char* p;
			int i,j;
			for (j=0;j<2;j++) {
			  	printk("[%d] %08x ",j,copy_entry[j].max_size);
				printk("%08x ",copy_entry[j].rom_address);
				printk("%08x \n",copy_entry[j].sdram_address);
				p=(char*)(copy_entry[j].rom_address
					  - 0x24000000 + FLASH1_BASE);
				printk("%08x\n", p);
				for (i = 0; i < 16; i++) printk("%02x ",*p++);
				printk("\n");
			}
		}
#endif
	}
	parts = iris_partitions;
	nb_parts = NB_OF(iris_partitions);

	/* delete partition , which size is zero */
	while( iris_partitions[nb_parts-1].size == 0 ) nb_parts--;

	l7200_mtdrom_map.size = iris_max_rom_size;
#endif
	if (!nb_parts) {
		/* printk(KERN_WARNING "l7200_mtdrom: No parts.\n"); */
		return -ENXIO;
	}
	l7200_mtdrom_map.buswidth = buswidth;
/*
	printk(KERN_NOTICE
	       "l7200_mtdrom: probing for %d partitions "
	       "(buswidth = %d)\n", nb_parts, buswidth);
*/
	mymtd = map_rom_probe(&l7200_mtdrom_map);
	if (!mymtd) {
		return -ENXIO;
	}

	mymtd->module = THIS_MODULE;

	add_mtd_partitions(mymtd, parts, nb_parts);

#if defined(CONFIG_IRIS)

	irisrom_setup_gpio();
	irirrom_power_on();

#define DFLT_IOCS1_PART0_START  0
#define DFLT_IOCS1_PART0_SIZE   0x3f0000

	/* set default values */
	{
	  int i;
	  nb_parts = NB_OF(iris_iocs1_partitions);
	  for(i=0;i<nb_parts;i++){
	    iris_iocs1_partitions[i].size = 0;
	    iris_iocs1_partitions[i].offset = 0;
	  }
	  iris_iocs1_partitions[0].size = DFLT_IOCS1_PART0_SIZE;
	  iris_iocs1_partitions[0].offset = DFLT_IOCS1_PART0_START;
	}

	/* read partitions */
#define IRIS1_PARTITION_BLOCK_MAGIC 0x50415254
#define IRIS1_PARTITION_OFFSET      0x10
	if( *((__u32*)(FLASH2_BASE)) == IRIS1_PARTITION_BLOCK_MAGIC ){
	  //printk("found IOCS1 partition block\n");
	  nb_parts = (*((__u32*)(FLASH2_BASE+4)));
	  //printk("partitions = %d\n",nb_parts);
	  {
	    int i;
	    copy_entry_info* copy_entry =
	      (copy_entry_info*)(FLASH2_BASE + IRIS1_PARTITION_OFFSET);
	    for(i=0;i<nb_parts;i++){
#if DEBUGGING
	      printk("[%d] %08x ",i,copy_entry[i].max_size);
	      printk("%08x ",copy_entry[i].rom_address);
	      printk("%08x \n",copy_entry[i].sdram_address);
#endif /* DEBUGGING */
	      iris_iocs1_partitions[i].size = copy_entry[i].max_size;
	      iris_iocs1_partitions[i].offset=  copy_entry[i].rom_address - FLASH2_START;
	    }
	  }
	}else{
	  //printk("NOT found IOCS1 partition block %x\n",*((__u32*)(FLASH2_BASE)));
	}

	parts = iris_iocs1_partitions;
	nb_parts = NB_OF(iris_iocs1_partitions);

	/* delete partition , which size is zero */
	while( iris_iocs1_partitions[nb_parts-1].size == 0 ) nb_parts--;

	l7200_iocs1_mtdrom_map.size = iris_max_rom_size;

	if (!nb_parts) {
		/* printk(KERN_WARNING "l7200_mtdrom: No parts.\n"); */
		return -ENXIO;
	}
	l7200_iocs1_mtdrom_map.buswidth = buswidth;
/*
	printk(KERN_NOTICE
	       "l7200_mtdrom: probing for %d partitions "
	       "(buswidth = %d)\n", nb_parts, buswidth);
*/

#if defined(CONFIG_MTD_SHARP16)

#if 1 /* use FLASH detection */
	irisrom_enable_write();

	mymtd_iocs1 = sharp16_probe(&iris_cfi_map);
	if( ! mymtd_iocs1 ){
	  /* may be RAM... so try again */
	  mymtd_iocs1 = map_rom_probe(&l7200_iocs1_mtdrom_map);
	  if( ! mymtd_iocs1 ){
	    return -ENXIO;
	  }
	}
#else

	mymtd_iocs1 = map_rom_probe(&l7200_iocs1_mtdrom_map);
	if (!mymtd_iocs1) {
		return -ENXIO;
	}
#endif

#else /* ! CONFIG_MTD_SHARP16 */

	mymtd_iocs1 = map_rom_probe(&l7200_iocs1_mtdrom_map);
	if (!mymtd_iocs1) {
		return -ENXIO;
	}

#endif  /* ! CONFIG_MTD_SHARP16 */

	mymtd_iocs1->module = THIS_MODULE;

	add_mtd_partitions(mymtd_iocs1, parts, nb_parts);


#endif  /* CONFIG_IRIS */

	printk(KERN_NOTICE "LinkUp 7200 ROM access initialized.\n");

	return 0;
}

static void __exit l7200_mtdrom_cleanup(void)
{
#if defined(CONFIG_IRIS)
	irirrom_power_off();
	if (mymtd_iocs1) {
	  	del_mtd_partitions(mymtd_iocs1);
		map_destroy(mymtd_iocs1);
	}
#endif  /* CONFIG_IRIS */
	if (mymtd) {
	  	del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
}

module_init(l7200_mtdrom_init);
module_exit(l7200_mtdrom_cleanup);
