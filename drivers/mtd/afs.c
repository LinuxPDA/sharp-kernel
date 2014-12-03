/*======================================================================

    drivers/mtd/afs.c: ARM Flash Layout/Partitioning
  
    Copyright (C) 2000 ARM Limited
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
  
   This is access code for flashes using ARM's flash partitioning 
   standards.

   $Id: afs.c,v 1.2 2001/06/09 18:57:16 rmk Exp $

======================================================================*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

struct footer_struct {
	u32 image_info_base;	/* Address of first word of ImageFooter  */
	u32 image_start;	/* Start of area reserved by this footer */
	u32 signature;		/* 'Magic' number proves it's a footer   */
	u32 type;		/* Area type: ARM Image, SIB, customer   */
	u32 checksum;		/* Just this structure                   */
};

struct image_info_struct {
	u32 bootFlags;		/* Boot flags, compression etc.          */
	u32 imageNumber;	/* Unique number, selects for boot etc.  */
	u32 loadAddress;	/* Address program should be loaded to   */
	u32 length;		/* Actual size of image                  */
	u32 address;		/* Image is executed from here           */
	char name[16];		/* Null terminated                       */
	u32 headerBase;		/* Flash Address of any stripped header  */
	u32 header_length;	/* Length of header in memory            */
	u32 headerType;		/* AIF, RLF, s-record etc.               */
	u32 checksum;		/* Image checksum (inc. this struct)     */
};

static int
afs_read_footer(struct mtd_info *mtd, u_int *img_start, u_int *iis_start,
		u_int off, u_int mask)
{
	struct footer_struct fs;
	u_int ptr = off + mtd->erasesize - sizeof(fs);
	size_t sz;
	int ret;

	ret = mtd->read(mtd, ptr, sizeof(fs), &sz, (u_char *) &fs);
	if (ret >= 0 && sz != sizeof(fs))
		ret = -EINVAL;

	if (ret < 0) {
		printk(KERN_ERR "AFS: mtd read failed at 0x%x: %d\n",
			ptr, ret);
		return ret;
	}

	/*
	 * Does it contain the magic number?
	 */
	if (fs.signature != 0xa0ffff9f)
		ret = 1;

	*iis_start = fs.image_info_base & mask;
	*img_start = fs.image_start & mask;

	/*
	 * Check the image info base.  This can not
	 * be located after the footer structure.
	 */
	if (*iis_start >= ptr)
		ret = 1;

	/*
	 * Check the start of this image.  The image
	 * data can not be located after this block.
	 */
	if (*img_start > off)
		ret = 1;

	return ret;
}

static int
afs_read_iis(struct mtd_info *mtd, struct image_info_struct *iis, u_int ptr)
{
	size_t sz;
	int ret;

	memset(iis, 0, sizeof(*iis));
	ret = mtd->read(mtd, ptr, sizeof(*iis), &sz, (u_char *) iis);
	if (ret >= 0 && sz != sizeof(*iis))
		ret = -EINVAL;
	if (ret < 0)
		printk(KERN_ERR "AFS: mtd read failed at 0x%x: %d\n",
			ptr, ret);

	return ret;
}

#define MAX_PARTS 16

int parse_afs_partitions(struct mtd_info *mtd, struct mtd_partition **pparts)
{
	struct mtd_partition *parts;
	u_int mask, off, idx;
	int ret = 0;

	parts = kmalloc(sizeof(struct mtd_partition) * MAX_PARTS, GFP_KERNEL);
	if (!parts)
		return -ENOMEM;

	/*
	 * This is the address mask; we use this to mask off out of
	 * range address bits.
	 */
	mask = mtd->size - 1;

	/*
	 * Identify the partitions
	 */
	for (idx = off = 0; off < mtd->size; off += mtd->erasesize) {
		struct image_info_struct iis;
		u_int iis_ptr, img_ptr;
		char *name;

		/* Read the footer. */
		ret = afs_read_footer(mtd, &img_ptr, &iis_ptr, off, mask);
		if (ret < 0)
			break;
		if (ret == 1)
			continue;

		/* Read the image info block */
		ret = afs_read_iis(mtd, &iis, iis_ptr);
		if (ret < 0)
			break;

		name = kmalloc(strlen(iis.name) + 1, GFP_KERNEL);
		ret = -ENOMEM;
		if (!name)
			break;

		strcpy(name, iis.name);

		parts[idx].name		= name;
		parts[idx].size		= mtd->erasesize + off - img_ptr;
		parts[idx].offset	= img_ptr;
		parts[idx].mask_flags	= 0;

		printk("  mtd%d: at 0x%08x, %5dKB, %8u, %s\n",
			idx, img_ptr, parts[idx].size / 1024,
			iis.imageNumber, name);

		idx += 1;
		if (idx >= MAX_PARTS)
			break;
	}

	if (!idx) {
		kfree(parts);
		parts = NULL;
	}

	*pparts = parts;
	return idx ? idx : ret;
}

EXPORT_SYMBOL(parse_afs_partitions);
