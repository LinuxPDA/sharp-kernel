/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 * $Id: fs.c,v 1.17 2002/08/21 10:42:12 dwmw2 Exp $
 *
 * ChangeLog:
 *     19-Nov-2002 Lineo Japan, Inc.  add function jffs2_get_nr_inodes()
 *     09-Nov-2002 Lineo Japan, Inc.  add code to do avail = 0 when cannot reserve space
 *     01-Nov-2002 Lineo Japan, Inc.  involve nr_bad_blocks in USED calc.
 *
 * ChangeLog:
 *     19-Nov-2002 Lineo Japan, Inc.  add function jffs2_get_nr_inodes()
 *     24-Nov-2002 SHARP  modify storage-avail calculation, and add erasing_dirty_size
 *     09-Nov-2002 Lineo Japan, Inc.  add code to do avail = 0 when cannot reserve space
 *     01-Nov-2002 Lineo Japan, Inc.  involve nr_bad_blocks in USED calc.
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include "nodelist.h"

int jffs2_statfs(struct super_block *sb, struct statfs *buf)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);
	unsigned long avail, avail_dirty;
	uint32_t dirty_now, erasing_dirty_now, nonerasing_dirty_now, next_free;
	int32_t avail_blocks;
	int reserved_blocks = JFFS2_RESERVED_BLOCKS_WRITE + JFFS2_RESERVED_BLOCKS_ROOT;

	buf->f_type = JFFS2_SUPER_MAGIC;
	buf->f_bsize = 1 << PAGE_SHIFT;
	buf->f_blocks = c->flash_size >> PAGE_SHIFT;
	buf->f_files = 0;
	buf->f_ffree = 0;
	buf->f_namelen = JFFS2_MAX_NAME_LEN;

	spin_lock_bh(&c->erase_completion_lock);

	dirty_now = c->dirty_size;
	erasing_dirty_now = c->erasing_dirty_size;
	if (dirty_now < erasing_dirty_now)
		erasing_dirty_now = dirty_now;
	nonerasing_dirty_now = dirty_now - erasing_dirty_now;
	avail_blocks = NR_AVAIL_BLOCKS(c);
	next_free = (c->nextblock) ? (c->nextblock->free_size) : 0;

	if (nonerasing_dirty_now >= JFFS2_RESERVED_BLOCKS_DIRTY * c->sector_size){
		avail_dirty = nonerasing_dirty_now - JFFS2_RESERVED_BLOCKS_DIRTY * c->sector_size;
	}else{
		avail_dirty = 0;
	}

	if (avail_blocks * c->sector_size + next_free + avail_dirty >= reserved_blocks * c->sector_size){
		avail = avail_blocks * c->sector_size + next_free + avail_dirty - reserved_blocks * c->sector_size;
	}else{
		avail = 0;
	}

	if (avail > 0){
		if (avail_blocks < reserved_blocks &&
			(c->cont_gc_count >= JFFS2_MAX_CONT_GC ||
			 avail_blocks <=  1 + JFFS2_RESERVED_BLOCKS_ROOT ||
			 dirty_now < c->sector_size ||
			 nonerasing_dirty_now / c->sector_size < JFFS2_RESERVED_BLOCKS_DIRTY ||
			 avail_blocks + nonerasing_dirty_now / c->sector_size - JFFS2_RESERVED_BLOCKS_DIRTY < reserved_blocks)){
			avail = 0;
		}
	}

	buf->f_bavail = buf->f_bfree = avail >> PAGE_SHIFT;

	D1(jffs2_dump_block_lists(c));

	spin_unlock_bh(&c->erase_completion_lock);

	return 0;
}


#ifdef CONFIG_JFFS2_DYNFRAGTREE

static unsigned long jffs2_nr_inodes; /* the number of inode objects */
static inline void inc_nr_inodes(void) { jffs2_nr_inodes++; }
static inline void dec_nr_inodes(void) { jffs2_nr_inodes--; }
unsigned long jffs2_get_nr_inodes(void) { return jffs2_nr_inodes; }

#else

static inline void inc_nr_inodes(void) { }
static inline void dec_nr_inodes(void) { }
unsigned long jffs2_get_nr_inodes(void) { return 0; }

#endif


void jffs2_clear_inode (struct inode *inode)
{
	/* We can forget about this inode for now - drop all 
	 *  the nodelists associated with it, etc.
	 */
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	
	D1(printk(KERN_DEBUG "jffs2_clear_inode(): ino #%lu mode %o\n", inode->i_ino, inode->i_mode));

	jffs2_do_clear_inode(c, f);

	dec_nr_inodes();
}

void jffs2_read_inode (struct inode *inode)
{
	struct jffs2_inode_info *f;
	struct jffs2_sb_info *c;
	struct jffs2_raw_inode latest_node;
	int ret;

	D1(printk(KERN_DEBUG "jffs2_read_inode(): inode->i_ino == %lu\n", inode->i_ino));

	f = JFFS2_INODE_INFO(inode);
	c = JFFS2_SB_INFO(inode->i_sb);

	jffs2_init_inode_info(f);
	
	ret = jffs2_do_read_inode(c, f, inode->i_ino, &latest_node);

	if (ret) {
		make_bad_inode(inode);
		up(&f->sem);
		return;
	}
	inode->i_mode = je32_to_cpu(latest_node.mode);
	inode->i_uid = je16_to_cpu(latest_node.uid);
	inode->i_gid = je16_to_cpu(latest_node.gid);
	inode->i_size = je32_to_cpu(latest_node.isize);
	inode->i_atime = je32_to_cpu(latest_node.atime);
	inode->i_mtime = je32_to_cpu(latest_node.mtime);
	inode->i_ctime = je32_to_cpu(latest_node.ctime);

	inode->i_nlink = f->inocache->nlink;

	inode->i_blksize = PAGE_SIZE;
	inode->i_blocks = (inode->i_size + 511) >> 9;
	
	switch (inode->i_mode & S_IFMT) {
		unsigned short rdev;

	case S_IFLNK:
		inode->i_op = &jffs2_symlink_inode_operations;
		break;
		
	case S_IFDIR:
	{
		struct jffs2_full_dirent *fd;

		for (fd=f->dents; fd; fd = fd->next) {
			if (fd->type == DT_DIR && fd->ino)
				inode->i_nlink++;
		}
		/* and '..' */
		inode->i_nlink++;
		/* Root dir gets i_nlink 3 for some reason */
		if (inode->i_ino == 1)
			inode->i_nlink++;

		inode->i_op = &jffs2_dir_inode_operations;
		inode->i_fop = &jffs2_dir_operations;
		break;
	}
	case S_IFREG:
		inode->i_op = &jffs2_file_inode_operations;
		inode->i_fop = &jffs2_file_operations;
		inode->i_mapping->a_ops = &jffs2_file_address_operations;
		inode->i_mapping->nrpages = 0;
		break;

	case S_IFBLK:
	case S_IFCHR:
		/* Read the device numbers from the media */
		D1(printk(KERN_DEBUG "Reading device numbers from flash\n"));
		if (jffs2_read_dnode(c, f->metadata, (char *)&rdev, 0, sizeof(rdev)) < 0) {
			/* Eep */
			printk(KERN_NOTICE "Read device numbers for inode %lu failed\n", (unsigned long)inode->i_ino);
			up(&f->sem);
			jffs2_do_clear_inode(c, f);
			make_bad_inode(inode);
			return;
		}			

	case S_IFSOCK:
	case S_IFIFO:
		inode->i_op = &jffs2_file_inode_operations;
		init_special_inode(inode, inode->i_mode, kdev_t_to_nr(mk_kdev(rdev>>8, rdev&0xff)));
		break;

	default:
		printk(KERN_WARNING "jffs2_read_inode(): Bogus imode %o for ino %lu\n", inode->i_mode, (unsigned long)inode->i_ino);
	}

	up(&f->sem);

	inc_nr_inodes();

	D1(printk(KERN_DEBUG "jffs2_read_inode() returning\n"));
}


int jffs2_remount_fs (struct super_block *sb, int *flags, char *data)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);

	if (c->flags & JFFS2_SB_FLAG_RO && !(sb->s_flags & MS_RDONLY))
		return -EROFS;

	/* We stop if it was running, then restart if it needs to.
	   This also catches the case where it was stopped and this
	   is just a remount to restart it */
	if (!(sb->s_flags & MS_RDONLY))
		jffs2_stop_garbage_collect_thread(c);

	if (!(*flags & MS_RDONLY))
		jffs2_start_garbage_collect_thread(c);
	
	sb->s_flags = (sb->s_flags & ~MS_RDONLY)|(*flags & MS_RDONLY);

	return 0;
}

void jffs2_write_super (struct super_block *sb)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);
	sb->s_dirt = 0;

	if (sb->s_flags & MS_RDONLY)
		return;

	D1(printk(KERN_DEBUG "jffs2_write_super()\n"));
	jffs2_garbage_collect_trigger(c);
	jffs2_erase_pending_blocks(c);
	jffs2_mark_erased_blocks(c);
}


/* jffs2_new_inode: allocate a new inode and inocache, add it to the hash,
   fill in the raw_inode while you're at it. */
struct inode *jffs2_new_inode (struct inode *dir_i, int mode, struct jffs2_raw_inode *ri)
{
	struct inode *inode;
	struct super_block *sb = dir_i->i_sb;
	struct jffs2_sb_info *c;
	struct jffs2_inode_info *f;
	int ret;

	D1(printk(KERN_DEBUG "jffs2_new_inode(): dir_i %ld, mode 0x%x\n", dir_i->i_ino, mode));

	c = JFFS2_SB_INFO(sb);
	
	inode = new_inode(sb);
	
	if (!inode)
		return ERR_PTR(-ENOMEM);

	f = JFFS2_INODE_INFO(inode);
	jffs2_init_inode_info(f);

	memset(ri, 0, sizeof(*ri));
	/* Set OS-specific defaults for new inodes */
	ri->uid = cpu_to_je16(current->fsuid);

	if (dir_i->i_mode & S_ISGID) {
		ri->gid = cpu_to_je16(dir_i->i_gid);
		if (S_ISDIR(mode))
			mode |= S_ISGID;
	} else {
		ri->gid = cpu_to_je16(current->fsgid);
	}
	ri->mode =  cpu_to_je32(mode);
	ret = jffs2_do_new_inode (c, f, mode, ri);
	if (ret) {
		make_bad_inode(inode);
		iput(inode);
		return ERR_PTR(ret);
	}
	inode->i_nlink = 1;
	inode->i_ino = je32_to_cpu(ri->ino);
	inode->i_mode = je32_to_cpu(ri->mode);
	inode->i_gid = je16_to_cpu(ri->gid);
	inode->i_uid = je16_to_cpu(ri->uid);
	inode->i_atime = inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	ri->atime = ri->mtime = ri->ctime = cpu_to_je32(inode->i_mtime);

	inode->i_blksize = PAGE_SIZE;
	inode->i_blocks = 0;
	inode->i_size = 0;

	insert_inode_hash(inode);

	inc_nr_inodes();

	return inode;
}


int jffs2_do_fill_super(struct super_block *sb, void *data, int silent)
{
	struct jffs2_sb_info *c;
	struct inode *root_i;
	int ret;

	c = JFFS2_SB_INFO(sb);

	c->sector_size = c->mtd->erasesize;
	c->flash_size = c->mtd->size;

#if 0
	if (c->sector_size < 0x10000) {
		printk(KERN_INFO "jffs2: Erase block size too small (%dKiB). Using 64KiB instead\n",
		       c->sector_size / 1024);
		c->sector_size = 0x10000;
	}
#endif
	if (c->flash_size < 5*c->sector_size) {
		printk(KERN_ERR "jffs2: Too few erase blocks (%d)\n",
		       c->flash_size / c->sector_size);
		return -EINVAL;
	}

	c->cleanmarker_size = sizeof(struct jffs2_unknown_node);
	/* Jvrn -- stick alignment for weird 8-byte-page flash here */

	if (jffs2_cleanmarker_oob(c)) {
		/* Cleanmarker is out-of-band, so inline size zero */
		c->cleanmarker_size = 0;
	}

	if (c->mtd->type == MTD_NANDFLASH) {
		/* Initialise write buffer */
		c->wbuf_pagesize = c->mtd->oobblock;
		c->wbuf_ofs = 0xFFFFFFFF;
		c->wbuf = kmalloc(c->wbuf_pagesize, GFP_KERNEL);
		if (!c->wbuf)
			return -ENOMEM;

		/* Initialize process for timed wbuf flush */
		INIT_TQUEUE(&c->wbuf_task,(void*) jffs2_wbuf_process, (void *)c);
		/* Initialize timer for timed wbuf flush */
		init_timer(&c->wbuf_timer);
		c->wbuf_timer.function = jffs2_wbuf_timeout;
		c->wbuf_timer.data = (unsigned long) c;
	}

	c->inocache_list = kmalloc(INOCACHE_HASHSIZE * sizeof(struct jffs2_inode_cache *), GFP_KERNEL);
	if (!c->inocache_list) {
		ret = -ENOMEM;
		goto out_wbuf;
	}
	memset(c->inocache_list, 0, INOCACHE_HASHSIZE * sizeof(struct jffs2_inode_cache *));

	if ((ret = jffs2_do_mount_fs(c)))
		goto out_inohash;

	ret = -EINVAL;

	D1(printk(KERN_DEBUG "jffs2_do_fill_super(): Getting root inode\n"));
	root_i = iget(sb, 1);
	if (is_bad_inode(root_i)) {
		D1(printk(KERN_WARNING "get root inode failed\n"));
		goto out_nodes;
	}

	D1(printk(KERN_DEBUG "jffs2_do_fill_super(): d_alloc_root()\n"));
	sb->s_root = d_alloc_root(root_i);
	if (!sb->s_root)
		goto out_root_i;

#if LINUX_VERSION_CODE >= 0x20403
	sb->s_maxbytes = 0xFFFFFFFF;
#endif
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = JFFS2_SUPER_MAGIC;
	if (!(sb->s_flags & MS_RDONLY))
		jffs2_start_garbage_collect_thread(c);
	return 0;

 out_root_i:
	iput(root_i);
 out_nodes:
	jffs2_free_ino_caches(c);
	jffs2_free_raw_node_refs(c);
	kfree(c->blocks);
 out_inohash:
	kfree(c->inocache_list);
 out_wbuf:
	if (c->wbuf)
		kfree(c->wbuf);
	return ret;
}
