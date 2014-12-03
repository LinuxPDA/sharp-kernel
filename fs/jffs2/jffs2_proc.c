/*
 * fs/jffs2/jffs2_proc.c
 *
 * Copyright (C) 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * $Id: jffs2_proc.c,v 1.13 2002/12/19 02:42:45 yamade Exp $
 *
 * Derived from fs/jffs/jffs_proc.c
 *
 * JFFS -- Journaling Flash File System, Linux implementation.
 *
 * Copyright (C) 2000  Axis Communications AB.
 *
 * Created by Simon Kagstrom <simonk@axis.com>.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *  Overview:
 *   This file defines JFFS partition entries in the proc file system.
 *
 *  TODO:
 *   Create some more proc files for different kinds of info, i.e. statistics
 *   about written and read bytes, number of calls to different routines,
 *   reports about failures.
 *
 * ChangLog:
 *     05-Dec-2002 SHARP  add proc-files
 */

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/jffs.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include "jffs2_proc.h"
#include "nodelist.h"
#include "nodemerge.h"

/*
 * Structure for a JFFS2 partition in the system
 */
struct jffs2_partition_dir {
    struct jffs2_sb_info *c;
    struct proc_dir_entry *part_root;
    struct proc_dir_entry *part_partition_size;
    struct proc_dir_entry *part_sector_size;
    struct proc_dir_entry *part_used_size;
    struct proc_dir_entry *part_dirty_size;
    struct proc_dir_entry *part_free_size;
    struct proc_dir_entry *part_wasted_size;
    struct proc_dir_entry *part_erasing_size;
    struct proc_dir_entry *part_free_blocks;
    struct proc_dir_entry *part_erasing_blocks;
    struct proc_dir_entry *part_gcing_blocks;
    struct proc_dir_entry *part_bad_blocks;
    struct proc_dir_entry *part_cont_gc_count;
    struct proc_dir_entry *part_reserved_blocks_write;
    struct proc_dir_entry *part_reserved_blocks_root;
    struct proc_dir_entry *part_reserved_blocks_bad;
    struct proc_dir_entry *part_reliable_free_size;
    struct proc_dir_entry *part_reliable_free_blocks;
    struct proc_dir_entry *part_nonerasing_dirty_size;
    struct proc_dir_entry *part_effective_gc_count;
    struct proc_dir_entry *part_dump_block_lists;
    struct proc_dir_entry *part_nodemerge;
    struct jffs2_partition_dir *next;
};

/*
 * Structure for top-level entry in '/proc/fs' directory
 */
struct proc_dir_entry *jffs2_proc_root;

/*
 * Linked list of 'jffs2_partition_dirs' to help us track
 * the mounted JFFS2 partitions in the system
 */
static struct jffs2_partition_dir *jffs2_part_dirs = 0;

/*
 * Read functions for entries
 */
static int jffs2_proc_partition_size_read(char *page, char **start, off_t off,
					  int count, int *eof, void *data);
static int jffs2_proc_sector_size_read(char *page, char **start, off_t off,
				       int count, int *eof, void *data);
static int jffs2_proc_used_size_read(char *page, char **start, off_t off,
				     int count, int *eof, void *data);
static int jffs2_proc_dirty_size_read(char *page, char **start, off_t off,
				      int count, int *eof, void *data);
static int jffs2_proc_free_size_read(char *page, char **start, off_t off,
				     int count, int *eof, void *data);
static int jffs2_proc_wasted_size_read(char *page, char **start, off_t off,
				       int count, int *eof, void *data);
static int jffs2_proc_erasing_size_read(char *page, char **start, off_t off,
					int count, int *eof, void *data);
static int jffs2_proc_free_blocks_read(char *page, char **start, off_t off,
				       int count, int *eof, void *data);
static int jffs2_proc_erasing_blocks_read(char *page, char **start, off_t off,
					  int count, int *eof, void *data);
static int jffs2_proc_gcing_blocks_read(char *page, char **start, off_t off,
					  int count, int *eof, void *data);
static int jffs2_proc_bad_blocks_read(char *page, char **start, off_t off,
				      int count, int *eof, void *data);
static int jffs2_proc_cont_gc_count_read(char *page, char **start, off_t off,
					 int count, int *eof, void *data);
static int jffs2_proc_reserved_blocks_write_read(char *page, char **start, off_t off,
						 int count, int *eof, void *data);
static int jffs2_proc_reserved_blocks_root_read(char *page, char **start, off_t off,
						int count, int *eof, void *data);
static int jffs2_proc_reserved_blocks_bad_read(char *page, char **start, off_t off,
					       int count, int *eof, void *data);
static int jffs2_proc_reliable_free_size_read(char *page, char **start, off_t off,
					      int count, int *eof, void *data);
static int jffs2_proc_reliable_free_blocks_read(char *page, char **start, off_t off,
					      int count, int *eof, void *data);
static int jffs2_proc_nonerasing_dirty_size_read(char *page, char **start, off_t off,
					      int count, int *eof, void *data);
static int jffs2_proc_effective_gc_count_read(char *page, char **start, off_t off,
					      int count, int *eof, void *data);
static int jffs2_proc_dump_block_lists_read(char *page, char **start, off_t off,
					      int count, int *eof, void *data);
static int jffs2_proc_nodemerge_write(struct file *file, const char *buffer,
				      unsigned long count, void *data);


/*
 * Register a JFFS2 partition directory (called upon mount)
 */
int
jffs2_register_jffs2_proc_dir(kdev_t dev, struct jffs2_sb_info *c)
{
    struct jffs2_partition_dir *part_dir;
    struct proc_dir_entry *part_root = 0;
    struct proc_dir_entry *part_partition_size = 0;
    struct proc_dir_entry *part_sector_size = 0;
    struct proc_dir_entry *part_used_size = 0;
    struct proc_dir_entry *part_dirty_size = 0;
    struct proc_dir_entry *part_free_size = 0;
    struct proc_dir_entry *part_wasted_size = 0;
    struct proc_dir_entry *part_erasing_size = 0;
    struct proc_dir_entry *part_free_blocks = 0;
    struct proc_dir_entry *part_erasing_blocks = 0;
    struct proc_dir_entry *part_gcing_blocks = 0;
    struct proc_dir_entry *part_bad_blocks = 0;
    struct proc_dir_entry *part_cont_gc_count = 0;
    struct proc_dir_entry *part_reserved_blocks_write = 0;
    struct proc_dir_entry *part_reserved_blocks_root = 0;
    struct proc_dir_entry *part_reserved_blocks_bad = 0;
    struct proc_dir_entry *part_reliable_free_size = 0;
    struct proc_dir_entry *part_reliable_free_blocks = 0;
    struct proc_dir_entry *part_nonerasing_dirty_size = 0;
    struct proc_dir_entry *part_effective_gc_count = 0;
    struct proc_dir_entry *part_dump_block_lists = 0;
    struct proc_dir_entry *part_nodemerge = 0;

    /* Allocate structure for local JFFS2 partition table */
    if (!(part_dir = (struct jffs2_partition_dir *)
	  kmalloc (sizeof (struct jffs2_partition_dir), GFP_KERNEL))) {
	return -ENOMEM;
    }

    /* Create entry for this partition */
    part_root = proc_mkdir(kdevname(dev), jffs2_proc_root);
    if (! part_root) {
	kfree(part_dir);
	return -ENOMEM;
    }

    /* Create entry for 'info' file */
    part_partition_size = create_proc_read_entry("partition_size", 0, part_root,
						 jffs2_proc_partition_size_read, c);
    part_sector_size = create_proc_read_entry("sector_size", 0, part_root, 
					      jffs2_proc_sector_size_read, c);
    part_used_size = create_proc_read_entry("used_size", 0, part_root,
					    jffs2_proc_used_size_read, c);
    part_dirty_size = create_proc_read_entry("dirty_size", 0, part_root,
					     jffs2_proc_dirty_size_read, c);
    part_free_size = create_proc_read_entry("free_size", 0, part_root,
					    jffs2_proc_free_size_read, c);
    part_wasted_size = create_proc_read_entry("wasted_size", 0, part_root,
					      jffs2_proc_wasted_size_read, c);
    part_erasing_size = create_proc_read_entry("erasing_size", 0, part_root,
					       jffs2_proc_erasing_size_read, c);
    part_free_blocks = create_proc_read_entry("free_blocks", 0, part_root,
					      jffs2_proc_free_blocks_read, c);
    part_erasing_blocks = create_proc_read_entry("erasing_blocks", 0, part_root,
						 jffs2_proc_erasing_blocks_read, c);
    part_gcing_blocks = create_proc_read_entry("gcing_blocks", 0, part_root,
						 jffs2_proc_gcing_blocks_read, c);
    part_bad_blocks = create_proc_read_entry("bad_blocks", 0, part_root,
					     jffs2_proc_bad_blocks_read, c);
    part_cont_gc_count = create_proc_read_entry("cont_gc_count", 0, part_root,
						jffs2_proc_cont_gc_count_read, c);
    part_reserved_blocks_write = create_proc_read_entry("reserved_blocks_write", 0, part_root,
							jffs2_proc_reserved_blocks_write_read, c);
    part_reserved_blocks_root = create_proc_read_entry("reserved_blocks_root", 0, part_root,
						jffs2_proc_reserved_blocks_root_read, c);
    part_reserved_blocks_bad = create_proc_read_entry("reserved_blocks_bad", 0, part_root,
						      jffs2_proc_reserved_blocks_bad_read, c);
    part_reliable_free_size = create_proc_read_entry("reliable_free_size", 0, part_root,
						      jffs2_proc_reliable_free_size_read, c);
    part_reliable_free_blocks = create_proc_read_entry("reliable_free_blocks", 0, part_root,
						      jffs2_proc_reliable_free_blocks_read, c);
    part_nonerasing_dirty_size = create_proc_read_entry("nonerasing_dirty_size", 0, part_root,
						      jffs2_proc_nonerasing_dirty_size_read, c);
    part_effective_gc_count = create_proc_read_entry("effective_gc_count", 0, part_root,
						     jffs2_proc_effective_gc_count_read, c);
    part_dump_block_lists = create_proc_read_entry(".dump_block_lists", 0, part_root,
						     jffs2_proc_dump_block_lists_read, c);
    part_nodemerge = create_proc_entry(".nodemerge", 0222, part_root);
    if (! part_partition_size || ! part_sector_size || ! part_used_size ||
	! part_dirty_size || ! part_free_size || ! part_wasted_size ||
	! part_erasing_size || ! part_free_blocks ||
	! part_erasing_blocks || ! part_gcing_blocks ||
	! part_bad_blocks || ! part_cont_gc_count || ! part_reserved_blocks_write ||
	! part_reserved_blocks_root || ! part_reserved_blocks_bad ||
	! part_reliable_free_size || ! part_reliable_free_blocks ||
	! part_nonerasing_dirty_size || ! part_effective_gc_count ||
	! part_dump_block_lists || ! part_nodemerge) {
	if (part_partition_size)
	    remove_proc_entry(part_partition_size->name, part_root);
	if (part_sector_size)
	    remove_proc_entry(part_sector_size->name, part_root);
	if (part_used_size)
	    remove_proc_entry(part_used_size->name, part_root);
	if (part_dirty_size)
	    remove_proc_entry(part_dirty_size->name, part_root);
	if (part_free_size)
	    remove_proc_entry(part_free_size->name, part_root);
	if (part_wasted_size)
	    remove_proc_entry(part_wasted_size->name, part_root);
	if (part_erasing_size)
	    remove_proc_entry(part_erasing_size->name, part_root);
	if (part_free_blocks)
	    remove_proc_entry(part_free_blocks->name, part_root);
	if (part_erasing_blocks)
	    remove_proc_entry(part_erasing_blocks->name, part_root);
	if (part_gcing_blocks)
	    remove_proc_entry(part_gcing_blocks->name, part_root);
	if (part_bad_blocks)
	    remove_proc_entry(part_bad_blocks->name, part_root);
	if (part_cont_gc_count)
	    remove_proc_entry(part_cont_gc_count->name, part_root);
	if (part_reserved_blocks_write)
	    remove_proc_entry(part_reserved_blocks_write->name, part_root);
	if (part_reserved_blocks_root)
	    remove_proc_entry(part_reserved_blocks_root->name, part_root);
	if (part_reserved_blocks_bad)
	    remove_proc_entry(part_reserved_blocks_bad->name, part_root);
	if (part_reliable_free_size)
	    remove_proc_entry(part_reliable_free_size->name, part_root);
	if (part_reliable_free_blocks)
	    remove_proc_entry(part_reliable_free_blocks->name, part_root);
	if (part_nonerasing_dirty_size)
	    remove_proc_entry(part_nonerasing_dirty_size->name, part_root);
	if (part_effective_gc_count)
	    remove_proc_entry(part_effective_gc_count->name, part_root);
	if (part_dump_block_lists)
	    remove_proc_entry(part_dump_block_lists->name, part_root);
	if (part_nodemerge)
	    remove_proc_entry(part_nodemerge->name, part_root);
	kfree(part_dir);
	return -ENOMEM;
    }
    part_nodemerge->write_proc = jffs2_proc_nodemerge_write;
    part_nodemerge->data = c;

    /* Fill in structure for table and insert in the list */
    part_dir->c = c;
    part_dir->part_root = part_root;
    part_dir->part_partition_size = part_partition_size;
    part_dir->part_sector_size = part_sector_size;
    part_dir->part_used_size = part_used_size;
    part_dir->part_dirty_size = part_dirty_size;
    part_dir->part_free_size = part_free_size;
    part_dir->part_wasted_size = part_wasted_size;
    part_dir->part_erasing_size = part_erasing_size;
    part_dir->part_free_blocks = part_free_blocks;
    part_dir->part_erasing_blocks = part_erasing_blocks;
    part_dir->part_gcing_blocks = part_gcing_blocks;
    part_dir->part_bad_blocks = part_bad_blocks;
    part_dir->part_cont_gc_count = part_cont_gc_count;
    part_dir->part_reserved_blocks_write = part_reserved_blocks_write;
    part_dir->part_reserved_blocks_root = part_reserved_blocks_root;
    part_dir->part_reserved_blocks_bad = part_reserved_blocks_bad;
    part_dir->part_reliable_free_size = part_reliable_free_size;
    part_dir->part_reliable_free_blocks = part_reliable_free_blocks;
    part_dir->part_nonerasing_dirty_size = part_nonerasing_dirty_size;
    part_dir->part_effective_gc_count = part_effective_gc_count;
    part_dir->part_dump_block_lists = part_dump_block_lists;
    part_dir->part_nodemerge = part_nodemerge;
    part_dir->next = jffs2_part_dirs;
    jffs2_part_dirs = part_dir;

    /* Return happy */
    return 0;
}


/*
 * Unregister a JFFS2 partition directory (called at umount)
 */
int
jffs2_unregister_jffs2_proc_dir(struct jffs2_sb_info *c)
{
    struct jffs2_partition_dir *part_dir = jffs2_part_dirs;
    struct jffs2_partition_dir *prev_part_dir = 0;

    while (part_dir) {
	if (part_dir->c == c) {
	    struct proc_dir_entry *part_root = part_dir->part_root;

	    /* Remove entries for partition */
	    remove_proc_entry(part_dir->part_partition_size->name, part_root);
	    remove_proc_entry(part_dir->part_sector_size->name, part_root);
	    remove_proc_entry(part_dir->part_used_size->name, part_root);
	    remove_proc_entry(part_dir->part_dirty_size->name, part_root);
	    remove_proc_entry(part_dir->part_free_size->name, part_root);
	    remove_proc_entry(part_dir->part_wasted_size->name, part_root);
	    remove_proc_entry(part_dir->part_erasing_size->name, part_root);
	    remove_proc_entry(part_dir->part_free_blocks->name, part_root);
	    remove_proc_entry(part_dir->part_erasing_blocks->name, part_root);
	    remove_proc_entry(part_dir->part_gcing_blocks->name, part_root);
	    remove_proc_entry(part_dir->part_bad_blocks->name, part_root);
	    remove_proc_entry(part_dir->part_cont_gc_count->name, part_root);
	    remove_proc_entry(part_dir->part_reserved_blocks_write->name, part_root);
	    remove_proc_entry(part_dir->part_reserved_blocks_root->name, part_root);
	    remove_proc_entry(part_dir->part_reserved_blocks_bad->name, part_root);
	    remove_proc_entry(part_dir->part_reliable_free_size->name, part_root);
	    remove_proc_entry(part_dir->part_reliable_free_blocks->name, part_root);
	    remove_proc_entry(part_dir->part_nonerasing_dirty_size->name, part_root);
	    remove_proc_entry(part_dir->part_effective_gc_count->name, part_root);
	    remove_proc_entry(part_dir->part_dump_block_lists->name, part_root);
	    remove_proc_entry(part_dir->part_nodemerge->name, part_root);
	    remove_proc_entry(part_root->name, jffs2_proc_root);

	    /* Remove entry from list */
	    if (prev_part_dir)
		prev_part_dir->next = part_dir->next;
	    else
		jffs2_part_dirs = part_dir->next;

	    /* Free memory for entry */
	    kfree(part_dir);

	    /* Return happy */
	    return 0;
	}

	/* Move to next entry */
	prev_part_dir = part_dir;
	part_dir = part_dir->next;
    }

    /* Return unhappy */
    return -1;
}


static int
jffs2_proc_partition_size_read(char *page, char **start, off_t off,
			       int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->flash_size);
    *eof = 1;
    return len;
}


static int
jffs2_proc_sector_size_read(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->sector_size);
    *eof = 1;
    return len;
}


static int
jffs2_proc_used_size_read(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->used_size);
    *eof = 1;
    return len;
}


static int
jffs2_proc_dirty_size_read(char *page, char **start, off_t off,
			   int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->dirty_size);
    *eof = 1;
    return len;
}


static int
jffs2_proc_free_size_read(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->free_size);
    *eof = 1;
    return len;
}


static int
jffs2_proc_wasted_size_read(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->wasted_size);
    *eof = 1;
    return len;
}


static int
jffs2_proc_erasing_size_read(char *page, char **start, off_t off,
			     int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->erasing_size);
    *eof = 1;
    return len;
}


static int
jffs2_proc_free_blocks_read(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->nr_free_blocks);
    *eof = 1;
    return len;
}


static int
jffs2_proc_erasing_blocks_read(char *page, char **start, off_t off,
			       int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->nr_erasing_blocks);
    *eof = 1;
    return len;
}


static int
jffs2_proc_gcing_blocks_read(char *page, char **start, off_t off,
			       int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len;
    int gcing_blocks, very_dirty_blocks, nodemerge_dirty_blocks;

    if (down_interruptible(&c->alloc_sem)){
	len = sprintf(page, "%u\n", 0);
	*eof = 1;
	return len;
    }
    spin_lock_bh(&c->erase_completion_lock);

    if (list_empty(&c->very_dirty_list)) {
	very_dirty_blocks = 0;
    } else {
	struct list_head *this;
	int	numblocks = 0;

	list_for_each(this, &c->very_dirty_list) {
	    struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
	    numblocks ++;
	}
	very_dirty_blocks = numblocks;
    }

    if (list_empty(&c->nodemerge_list)) {
	nodemerge_dirty_blocks = 0;
    } else {
	struct list_head *this;
	int	numblocks = 0;

	list_for_each(this, &c->nodemerge_list) {
	    struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
	    if (ISDIRTY(jeb->dirty_size)) {
		numblocks ++;
	    }
	}
	nodemerge_dirty_blocks = numblocks;
    }

    spin_unlock_bh(&c->erase_completion_lock);
    up(&c->alloc_sem);

    gcing_blocks = very_dirty_blocks + nodemerge_dirty_blocks;

    len = sprintf(page, "%u\n", gcing_blocks);
    *eof = 1;
    return len;
}


static int
jffs2_proc_bad_blocks_read(char *page, char **start, off_t off,
			   int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->nr_bad_blocks);
    *eof = 1;
    return len;
}


static int
jffs2_proc_cont_gc_count_read(char *page, char **start, off_t off,
			      int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->cont_gc_count);
    *eof = 1;
    return len;
}


static int
jffs2_proc_reserved_blocks_write_read(char *page, char **start, off_t off,
				      int count, int *eof, void *data)
{
    int len = sprintf(page, "%d\n", JFFS2_RESERVED_BLOCKS_WRITE);
    *eof = 1;
    return len;
}


static int
jffs2_proc_reserved_blocks_root_read(char *page, char **start, off_t off,
				     int count, int *eof, void *data)
{
    int len = sprintf(page, "%d\n", JFFS2_RESERVED_BLOCKS_ROOT);
    *eof = 1;
    return len;
}


static int
jffs2_proc_reserved_blocks_bad_read(char *page, char **start, off_t off,
				    int count, int *eof, void *data)
{
    int len = sprintf(page, "%d\n", JFFS2_RESERVED_BLOCKS_BAD);
    *eof = 1;
    return len;
}


static int
jffs2_proc_reliable_free_size_read(char *page, char **start, off_t off,
				    int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len;
    unsigned long avail;
    uint32_t next_free;
    int32_t avail_blocks;
    int reserved_blocks = JFFS2_RESERVED_BLOCKS_WRITE + JFFS2_RESERVED_BLOCKS_ROOT;

    spin_lock_bh(&c->erase_completion_lock);

    avail_blocks = NR_AVAIL_BLOCKS(c);
    next_free = (c->nextblock) ? (c->nextblock->free_size) : 0;

    if (avail_blocks >= reserved_blocks){
	avail = avail_blocks * c->sector_size + next_free - reserved_blocks * c->sector_size;
    }else{
	avail = 0;
    }

    spin_unlock_bh(&c->erase_completion_lock);

    len = sprintf(page, "%lu\n", avail);
    *eof = 1;
    return len;
}


static int
jffs2_proc_reliable_free_blocks_read(char *page, char **start, off_t off,
				    int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len;
    unsigned long avail;
    int32_t avail_blocks;
    int reserved_blocks = JFFS2_RESERVED_BLOCKS_WRITE + JFFS2_RESERVED_BLOCKS_ROOT;

    avail_blocks = NR_AVAIL_BLOCKS(c);
    if (avail_blocks >= reserved_blocks){
	avail = avail_blocks - reserved_blocks;
    }else{
	avail = 0;
    }

    len = sprintf(page, "%lu\n", avail);
    *eof = 1;
    return len;
}


static int
jffs2_proc_nonerasing_dirty_size_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len;
    uint32_t dirty_now, erasing_dirty_now, nonerasing_dirty_now;

    spin_lock_bh(&c->erase_completion_lock);

    dirty_now = c->dirty_size;
    erasing_dirty_now = c->erasing_dirty_size;
    if (dirty_now < erasing_dirty_now)
	erasing_dirty_now = dirty_now;
    nonerasing_dirty_now = dirty_now - erasing_dirty_now;

    spin_unlock_bh(&c->erase_completion_lock);

    len = sprintf(page, "%lu\n", nonerasing_dirty_now);
    *eof = 1;
    return len;
}


static int
jffs2_proc_effective_gc_count_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = sprintf(page, "%u\n", c->effective_gc_count);
    *eof = 1;
    return len;
}


static int
jffs2_proc_dump_block_lists_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
    struct jffs2_sb_info *c = (struct jffs2_sb_info *) data;
    int len = 0, tlen;

    if (down_interruptible(&c->alloc_sem)){
	len = sprintf(page, "down(alloc_sem) failed.\n");
	*eof = 1;
	return len;
    }
    spin_lock_bh(&c->erase_completion_lock);

    if (list_empty(&c->clean_list)) {
	tlen = sprintf(page + len, "clean_list: blocks=(0)\n");
	len += tlen;
    } else {
	struct list_head *this;
	int	numblocks = 0;
	unsigned long dirty = 0;

	list_for_each(this, &c->clean_list) {
	    struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
	    numblocks ++;
	    dirty += jeb->dirty_size;
	}
	tlen = sprintf(page + len, "clean_list: blocks=(%d) dirty=(%lu,%lu)\n", numblocks, dirty, dirty / numblocks);
	len += tlen;
    }

    if (list_empty(&c->very_dirty_list)) {
	tlen = sprintf(page + len, "very_dirty_list: blocks=(0)\n");
	len += tlen;
    } else {
	struct list_head *this;
	int	numblocks = 0;
	unsigned long dirty = 0;

	list_for_each(this, &c->very_dirty_list) {
	    struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
	    numblocks ++;
	    dirty += jeb->dirty_size;
	}
	tlen = sprintf(page + len, "very_dirty_list: blocks=(%d) dirty=(%lu,%lu)\n", numblocks, dirty, dirty / numblocks);
	len += tlen;
    }

    if (list_empty(&c->dirty_list)) {
	tlen = sprintf(page + len, "dirty_list: blocks=(0)\n");
	len += tlen;
    } else {
	struct list_head *this;
	int	numblocks0 = 0, numblocks1 = 0, numblocks2 = 0, numblocks3 = 0;
	unsigned long dirty0 = 0, dirty1 = 0, dirty2 = 0, dirty3 = 0;

	list_for_each(this, &c->dirty_list) {
	    struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
	    if(jeb->dirty_size >= GC_EFFECTIVE_BLOCK_DIRTY_SIZE_LL){
		numblocks3 ++;
		dirty3 += jeb->dirty_size;
	    }else if(jeb->dirty_size >= GC_EFFECTIVE_BLOCK_DIRTY_SIZE_L){
		numblocks2 ++;
		dirty2 += jeb->dirty_size;
	    }else if(jeb->dirty_size >= GC_EFFECTIVE_BLOCK_DIRTY_SIZE){
		numblocks1 ++;
		dirty1 += jeb->dirty_size;
	    }else{
		numblocks0 ++;
		dirty0 += jeb->dirty_size;
	    }
	}
	tlen = sprintf(page + len, "dirty_list: blocks=(%d) dirty=(%lu,%lu)\n",
		       numblocks0 + numblocks1 + numblocks2 + numblocks3,
		       dirty0 + dirty1 + dirty2 + dirty3,
		       (dirty0 + dirty1 + dirty2 + dirty3) / (numblocks0 + numblocks1 + numblocks2 + numblocks3));
	len += tlen;
	tlen = sprintf(page + len, "dirty_list[3]: blocks=(%d) dirty=(%lu,%lu)\n", numblocks3, dirty3,
		       (numblocks3 == 0) ? 0 : (dirty3 / numblocks3));
	len += tlen;
	tlen = sprintf(page + len, "dirty_list[2]: blocks=(%d) dirty=(%lu,%lu)\n", numblocks2, dirty2,
		       (numblocks2 == 0) ? 0 : (dirty2 / numblocks2));
	len += tlen;
	tlen = sprintf(page + len, "dirty_list[1]: blocks=(%d) dirty=(%lu,%lu)\n", numblocks1, dirty1,
		       (numblocks1 == 0) ? 0 : (dirty1 / numblocks1));
	len += tlen;
	tlen = sprintf(page + len, "dirty_list[0]: blocks=(%d) dirty=(%lu,%lu)\n", numblocks0, dirty0,
		       (numblocks0 == 0) ? 0 : (dirty0 / numblocks0));
	len += tlen;
    }

    if (list_empty(&c->nodemerge_list)) {
	tlen = sprintf(page + len, "nodemerge_list: blocks=(0)\n");
	len += tlen;
    } else {
	struct list_head *this;
	int	numblocks = 0;
	unsigned long dirty = 0;

	list_for_each(this, &c->nodemerge_list) {
	    struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);
	    numblocks ++;
	    dirty += jeb->dirty_size;
	}
	tlen = sprintf(page + len, "nodemerge_list: blocks=(%d) dirty=(%lu,%lu)\n", numblocks, dirty, dirty / numblocks);
	len += tlen;
    }

    spin_unlock_bh(&c->erase_completion_lock);
    up(&c->alloc_sem);

    *eof = 1;
    return len;
}


static unsigned long
atoul(const char* p)
{
    unsigned long n = 0;

    while (p && ! isdigit(*p))
	p++;
    while (p && isdigit(*p)) {
	n = n * 10 + *p - '0';
	p++;
    }
    return n;
}


static int
jffs2_proc_nodemerge_write(struct file *file, const char *buffer, unsigned long count,
			   void *data)
{
    extern void jffs2_shrink_inode(struct inode *);

    struct jffs2_sb_info *c = (struct jffs2_sb_info *)data;
    char str[16];
    unsigned long ino = 0;
    unsigned long len = (sizeof str < count) ? sizeof str : count;
    struct inode *inode;
    struct jffs2_inode_info *f;

    memset(str, '\0', sizeof str);
    if (copy_from_user(str, buffer, len))
	return -EFAULT;

    ino = atoul(str);
    printk(KERN_DEBUG "nodemerge: start ino=%d\n", ino);
    if (ino > c->highest_ino)
	return -EINVAL;

    inode = iget(OFNI_BS_2SFFJ(c), ino);
    if (is_bad_inode(inode)) {
	printk(KERN_NOTICE "iget() failed\n");
	iput(inode);
	return -EIO;
    }

    f = JFFS2_INODE_INFO(inode);
    if (down_interruptible(&f->sem)) {
	printk(KERN_NOTICE "nodemerge: down_interruptible failed\n");
	iput(inode);
	return -ERESTARTSYS;
    }
    jffs2_construct_fragtree_nolock_if_missing(c, f);
    jffs2_merge_nodes(c, f, inode->i_size, JFFS2_FRAGS_NR_NODES_THRESHOLD_FORCE);
    up(&f->sem);

    iput(inode);

    printk(KERN_DEBUG "nodemerge: end ino=%d\n", ino);
    return count;
}
