/*
 * linux/drivers/mtd/devices/sa1100_mtdrom.c
 * 
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on drivers/mtd/mtdram.c
 *   Author: Alexander Larsson <alex@cendio.se>
 *  Copyright(c) 1999 Alexander Larsson <alex@cendio.se>
 * 
 * Change Log
 *	12-Mar-2002 Lineo Japan, Inc.
 */
#include <linux/module.h>
#include <linux/config.h>
#include <linux/malloc.h>
#include <linux/ioport.h>
#include <linux/mtd/compatmac.h>
#include <linux/mtd/mtd.h>
#include <asm/io.h>

#ifndef CONFIG_MTDROM_ABS_POS
#define CONFIG_MTDROM_ABS_POS 0
#endif

#ifdef MODULE
static unsigned long total_size = CONFIG_MTDROM_TOTAL_SIZE;
static unsigned long erase_size = CONFIG_MTDROM_ERASE_SIZE;
MODULE_PARM(total_size,"l");
MODULE_PARM(erase_size,"l");
#define MTDROM_TOTAL_SIZE (total_size * 1024)
#define MTDROM_ERASE_SIZE (erase_size * 1024)
#else
#define MTDROM_TOTAL_SIZE (CONFIG_MTDROM_TOTAL_SIZE * 1024)
#define MTDROM_ERASE_SIZE (CONFIG_MTDROM_ERASE_SIZE * 1024)
#endif

static struct mtd_info* mtd_info;

static int sa1100_ram_erase(struct mtd_info* mtd, struct erase_info* instr)
{
	if (instr->addr + instr->len > mtd->size) {
		return -EINVAL;
	}
	
	memset((char*)mtd->priv + instr->addr, 0xff, instr->len);

	instr->state = MTD_ERASE_DONE;

	if (instr->callback) {
	  	(*(instr->callback))(instr);
	}
	return 0;
}

static int sa1100_ram_point(struct mtd_info* mtd, loff_t from,
			   size_t len, size_t* retlen, u_char** mtdbuf)
{
	if (from + len > mtd->size) {
	  	return -EINVAL;
	}

	*mtdbuf = mtd->priv + from;
	*retlen = len;
	return 0;
}

static void sa1100_ram_unpoint (struct mtd_info* mtd, u_char* addr)
{
}

static int sa1100_ram_read(struct mtd_info* mtd, loff_t from,
			  size_t len, size_t* retlen, u_char* buf)
{
	if (from + len > mtd->size) {
	  	return -EINVAL;
	}

	memcpy(buf, mtd->priv + from, len);

	*retlen = len;
	return 0;
}

static int sa1100_ram_write(struct mtd_info* mtd, loff_t to,
			   size_t len, size_t* retlen, const u_char* buf)
{
	if (to + len > mtd->size) {
	  	return -EINVAL;
	}

	memcpy ((char* )mtd->priv + to, buf, len);

	*retlen = len;
	return 0;
}

mod_exit_t sa1100_mtdrom_cleanup(void)
{
	if (mtd_info) {
	  	del_mtd_device(mtd_info);
		if (mtd_info->priv) {
#if CONFIG_MTDROM_ABS_POS > 0
			iounmap(mtd_info->priv);
#else
			vfree(mtd_info->priv);
#endif
		}
		kfree(mtd_info);
	}
}

extern struct module __this_module;

mod_init_t sa1100_mtdrom_init(void)
{
	/* Allocate some memory*/
	mtd_info = (struct mtd_info*)
	  	kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (mtd_info == 0) {
	  	return 0;
	}

	memset(mtd_info, 0, sizeof(*mtd_info));

	/* Setup the MTD structure */
	mtd_info->name = "mtdrom";
	mtd_info->type = MTD_ROM;
	mtd_info->flags = MTD_CAP_ROM;
	mtd_info->size = MTDROM_TOTAL_SIZE;
	mtd_info->erasesize = MTDROM_ERASE_SIZE;
#if CONFIG_MTDROM_ABS_POS > 0
	mtd_info->priv = ioremap(CONFIG_MTDROM_ABS_POS, MTDROM_TOTAL_SIZE);
#else
	mtd_info->priv = vmalloc(MTDROM_TOTAL_SIZE);
	memset(mtd_info->priv, 0xff, MTDROM_TOTAL_SIZE);
#endif

	mtd_info->module = THIS_MODULE;			

	if (!mtd_info->priv) {
	  	kfree(mtd_info);
		mtd_info = NULL;
		return -ENOMEM;
	}
	mtd_info->erase = sa1100_ram_erase;
	mtd_info->point = sa1100_ram_point;
	mtd_info->unpoint = sa1100_ram_unpoint;
	mtd_info->read = sa1100_ram_read;
	mtd_info->write = sa1100_ram_write;

	if (add_mtd_device(mtd_info)) {
#if CONFIG_MTDROM_ABS_POS > 0
	  	iounmap(mtd_info->priv);
#else
		vfree(mtd_info->priv);
#endif	
		kfree(mtd_info);
		mtd_info = NULL;
		return -EIO;
	}

	return 0;
}

module_init(sa1100_mtdrom_init);
module_exit(sa1100_mtdrom_cleanup);
