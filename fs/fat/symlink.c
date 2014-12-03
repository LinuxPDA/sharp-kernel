/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2002, 2003 Lineo Japan, Inc.
 *
 * fs/fat/symlink.c
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>

#if defined(CONFIG_VFAT_SHORTCUT_SYMLINK) || defined(CONFIG_VFAT_SHORTCUT_SYMLINK_MODULE)

static char* fat_getlink(struct dentry* de)
{
	struct inode* inode = de->d_inode;
	struct super_block* sb = inode->i_sb;
	struct buffer_head* bh;
	unsigned char *buf, *data;
	unsigned long phys;
	short size;

	phys = fat_bmap(inode, 0);
	bh   = fat_bread(sb, phys);
	buf  = bh->b_data + 0x4c;
	data = buf + 2;
	size = (unsigned short)buf[0] + (unsigned short)buf[1] * 256;

	buf = kmalloc(size + 1, GFP_KERNEL);
	memcpy(buf, data, size);
	*(buf + size) = 0;

	fat_brelse(sb, bh);

	return buf;
}

int fat_readlink(struct dentry* de, char* buffer, int buflen)
{
	unsigned char* kbuf;
	int ret;

	kbuf = fat_getlink(de);

	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	ret = vfs_readlink(de, buffer, buflen, kbuf);
	kfree(kbuf); 

	return ret;
}

int fat_follow_link(struct dentry* de, struct nameidata* nd)
{
	unsigned char* kbuf;
	int ret;

	kbuf = fat_getlink(de);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);
	
	ret = vfs_follow_link(nd, kbuf);
	kfree(kbuf);
	
	return ret;
} 

struct inode_operations fat_symlink_inode_operations = {
	readlink:	fat_readlink,
	follow_link:	fat_follow_link,
};

#endif
