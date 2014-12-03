/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 *
 * ChangeLog:
 *     15-Nov-2002 Lineo Japan, Inc.  add nodemerge facility
 *     20-Sep-2002 Lineo Japan, Inc.  add jffs2_orphaned_inodes
 *					but it is useless right now
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include "nodelist.h"

static enum { ORPHANED_DELETE, ORPHANED_ERROR } orphaned_inodes_mode;

int jffs2_build_inode_pass1(struct jffs2_sb_info *, struct jffs2_inode_cache *);
int jffs2_build_remove_unlinked_inode(struct jffs2_sb_info *, struct jffs2_inode_cache *);


#define for_each_inode(i, c, ic) for (i=0; i<INOCACHE_HASHSIZE; i++) for (ic=c->inocache_list[i]; ic; ic=ic->next) 

/* Scan plan:
 - Scan physical nodes. Build map of inodes/dirents. Allocate inocaches as we go
 - Scan directory tree from top down, setting nlink in inocaches
 - Scan inocaches for inodes with nlink==0
*/
static int jffs2_build_filesystem(struct jffs2_sb_info *c)
{
	int ret;
	int i;
	struct jffs2_inode_cache *ic;

	/* First, scan the medium and build all the inode caches with
	   lists of physical nodes */

	c->flags |= JFFS2_SB_FLAG_MOUNTING;
	ret = jffs2_scan_medium(c);
	c->flags &= ~JFFS2_SB_FLAG_MOUNTING;

	if (ret)
		return ret;

	D1(printk(KERN_DEBUG "Scanned flash completely\n"));
	D1(jffs2_dump_block_lists(c));
	/* Now build the data map for each inode, marking obsoleted nodes
	   as such, and also increase nlink of any children. */
	for_each_inode(i, c, ic) {
		D1(printk(KERN_DEBUG "Pass 1: ino #%u\n", ic->ino));
		ret = jffs2_build_inode_pass1(c, ic);
		if (ret) {
			D1(printk(KERN_WARNING "Eep. jffs2_build_inode_pass1 for ino %d returned %d\n", ic->ino, ret));
			return ret;
		}
		cond_resched();
	}
	D1(printk(KERN_DEBUG "Pass 1 complete\n"));
	D1(jffs2_dump_block_lists(c));
	
	/* Next, scan for inodes with nlink == 0 and remove them. If
	   they were directories, then decrement the nlink of their
	   children too, and repeat the scan. As that's going to be
	   a fairly uncommon occurrence, it's not so evil to do it this
	   way. Recursion bad. */
	do { 
		D1(printk(KERN_DEBUG "Pass 2 (re)starting\n"));
		ret = 0;
		for_each_inode(i, c, ic) {
			D1(printk(KERN_DEBUG "Pass 2: ino #%u, nlink %d, ic %p, nodes %p\n", ic->ino, ic->nlink, ic, ic->nodes));
			if (ic->nlink)
				continue;
			
			/* XXX: Can get high latency here. Move the cond_resched() from the end of the loop? */

			switch (orphaned_inodes_mode) {
			case ORPHANED_ERROR:
				printk(KERN_ERR "%s: detect orphaned inode\n", __func__);
				return -EIO;

			default:
				break;
			}
			ret = jffs2_build_remove_unlinked_inode(c, ic);
			if (ret)
				break;
		/* -EAGAIN means the inode's nlink was zero, so we deleted it,
		   and furthermore that it had children and their nlink has now
		   gone to zero too. So we have to restart the scan. */
		} 
		D1(jffs2_dump_block_lists(c));

		cond_resched();
	
	} while(ret == -EAGAIN);

	D1(printk(KERN_DEBUG "Pass 2 complete\n"));
	
	/* Finally, we can scan again and free the dirent nodes and scan_info structs */
	for_each_inode(i, c, ic) {
		struct jffs2_scan_info *scan = ic->scan;
		struct jffs2_full_dirent *fd;
		D1(printk(KERN_DEBUG "Pass 3: ino #%u, ic %p, nodes %p\n", ic->ino, ic, ic->nodes));
		if (!scan) {
			if (ic->nlink) {
				D1(printk(KERN_WARNING "Why no scan struct for ino #%u which has nlink %d?\n", ic->ino, ic->nlink));
			}
			continue;
		}
		ic->scan = NULL;
		while(scan->dents) {
			fd = scan->dents;
			scan->dents = fd->next;
			jffs2_free_full_dirent(fd);
		}
		kfree(scan);
		cond_resched();
	}
	D1(printk(KERN_DEBUG "Pass 3 complete\n"));
	D1(jffs2_dump_block_lists(c));

	/* Rotate the lists by some number to ensure wear levelling */
	jffs2_rotate_lists(c);

	return ret;
}
	
int jffs2_build_inode_pass1(struct jffs2_sb_info *c, struct jffs2_inode_cache *ic)
{
	struct jffs2_tmp_dnode_info *tn;
	struct jffs2_full_dirent *fd;
	rb_root_t fragtree = RB_ROOT;
	struct jffs2_tmp_dnode_info *metadata = NULL;

	D1(printk(KERN_DEBUG "jffs2_build_inode building inode #%u\n", ic->ino));
	if (ic->ino > c->highest_ino)
		c->highest_ino = ic->ino;

	if (!ic->scan->tmpnodes && ic->ino != 1) {
		D1(printk(KERN_DEBUG "jffs2_build_inode: ino #%u has no data nodes!\n", ic->ino));
	}
	/* Build the list to make sure any obsolete nodes are marked as such */
	while(ic->scan->tmpnodes) {
		tn = ic->scan->tmpnodes;
		ic->scan->tmpnodes = tn->next;
		
		if (metadata && tn->version > metadata->version) {
			D1(printk(KERN_DEBUG "jffs2_build_inode_pass1 ignoring old metadata at 0x%08x\n",
				  ref_offset(metadata->fn->raw)));
			
			jffs2_mark_node_obsolete(c, metadata->fn->raw);
			jffs2_free_full_dnode(metadata->fn);
			jffs2_free_tmp_dnode_info(metadata);
			metadata = NULL;
		}
			
		if (tn->fn->size) {
			jffs2_add_full_dnode_to_fraglist (c, &fragtree, tn->fn);
			jffs2_free_tmp_dnode_info(tn);
		} else {
			if (!metadata) {
				metadata = tn;
			} else {
				/* This will only happen if it has the _same_ version
				   number as the existing metadata node. */
				D1(printk(KERN_DEBUG "jffs2_build_inode_pass1 ignoring new metadata at 0x%08x\n",
					  ref_offset(tn->fn->raw)));
				
				jffs2_mark_node_obsolete(c, tn->fn->raw);
				jffs2_free_full_dnode(tn->fn);
				jffs2_free_tmp_dnode_info(tn);
			}
		}
	}

	if (ic->scan->version) {
		/* It's a regular file, so truncate it to the last known
		   i_size, if necessary */
		D1(printk(KERN_DEBUG "jffs2_build_inode_pass1 truncating fraglist to 0x%08x\n", ic->scan->isize));
		jffs2_truncate_fraglist_1(c, &fragtree, ic->scan->isize);
	}
	
	/* OK. Now clear up */
	if (metadata) {
		jffs2_free_full_dnode(metadata->fn);
		jffs2_free_tmp_dnode_info(metadata);
	}
	metadata = NULL;

	jffs2_kill_fragtree(&fragtree, NULL);

	/* Now for each child, increase nlink */
	for(fd=ic->scan->dents; fd; fd = fd->next) {
		struct jffs2_inode_cache *child_ic;
		if (!fd->ino)
			continue;
		
		/* XXX: Can get high latency here with huge directories */

		child_ic = jffs2_get_ino_cache(c, fd->ino);
		if (!child_ic) {
			printk(KERN_NOTICE "Eep. Child \"%s\" (ino #%u) of dir ino #%u doesn't exist!\n",
				  fd->name, fd->ino, ic->ino);
			continue;
		}

		if (child_ic->nlink++ && fd->type == DT_DIR) {
			printk(KERN_NOTICE "Child dir \"%s\" (ino #%u) of dir ino #%u appears to be a hard link\n", fd->name, fd->ino, ic->ino);
			if (fd->ino == 1 && ic->ino == 1) {
				printk(KERN_NOTICE "This is mostly harmless, and probably caused by creating a JFFS2 image\n");
				printk(KERN_NOTICE "using a buggy version of mkfs.jffs2. Use at least v1.17.\n");
			}
			/* What do we do about it? */
		}
		D1(printk(KERN_DEBUG "Increased nlink for child \"%s\" (ino #%u)\n", fd->name, fd->ino));
		/* Can't free them. We might need them in pass 2 */
	}
	return 0;
}

int jffs2_build_remove_unlinked_inode(struct jffs2_sb_info *c, struct jffs2_inode_cache *ic)
{
	struct jffs2_raw_node_ref *raw;
	struct jffs2_full_dirent *fd;
	int ret = 0;

	if(!ic->scan) {
		D1(printk(KERN_DEBUG "ino #%u was already removed\n", ic->ino));
		return 0;
	}

	D1(printk(KERN_DEBUG "JFFS2: Removing ino #%u with nlink == zero.\n", ic->ino));
	
	for (raw = ic->nodes; raw != (void *)ic; raw = raw->next_in_ino) {
		D1(printk(KERN_DEBUG "obsoleting node at 0x%08x\n", ref_offset(raw)));
		jffs2_mark_node_obsolete(c, raw);
	}

	if (ic->scan->dents) {
		int whinged = 0;
		D1(printk(KERN_DEBUG "Inode #%u was a directory which may have children...\n", ic->ino));

		while(ic->scan->dents) {
			struct jffs2_inode_cache *child_ic;

			fd = ic->scan->dents;
			ic->scan->dents = fd->next;

			if (!fd->ino) {
				/* It's a deletion dirent. Ignore it */
				D1(printk(KERN_DEBUG "Child \"%s\" is a deletion dirent, skipping...\n", fd->name));
				jffs2_free_full_dirent(fd);
				continue;
			}
			if (!whinged) {
				whinged = 1;
				printk(KERN_NOTICE "Inode #%u was a directory with children - removing those too...\n", ic->ino);
			}

			D1(printk(KERN_DEBUG "Removing child \"%s\", ino #%u\n",
				  fd->name, fd->ino));
			
			child_ic = jffs2_get_ino_cache(c, fd->ino);
			if (!child_ic) {
				printk(KERN_NOTICE "Cannot remove child \"%s\", ino #%u, because it doesn't exist\n", fd->name, fd->ino);
				jffs2_free_full_dirent(fd);
				continue;
			}
			jffs2_free_full_dirent(fd);
			child_ic->nlink--;
		}
		ret = -EAGAIN;
	}
	kfree(ic->scan);
	ic->scan = NULL;

	/*
	   We don't delete the inocache from the hash list and free it yet. 
	   The erase code will do that, when all the nodes are completely gone.
	*/

	return ret;
}

int jffs2_do_mount_fs(struct jffs2_sb_info *c)
{
	int i;

	c->free_size = c->flash_size;
	c->nr_blocks = c->flash_size / c->sector_size;
#ifdef CONFIG_JFFS2_LARGE_PARTITION
	c->blocks = consistent_alloc(GFP_KERNEL,
				     sizeof(struct jffs2_eraseblock) * c->nr_blocks,
				     &c->blocks_phys);
#else
	c->blocks = kmalloc(sizeof(struct jffs2_eraseblock) * c->nr_blocks, GFP_KERNEL);
#endif
	if (!c->blocks)
		return -ENOMEM;
	for (i=0; i<c->nr_blocks; i++) {
		INIT_LIST_HEAD(&c->blocks[i].list);
		c->blocks[i].offset = i * c->sector_size;
		c->blocks[i].free_size = c->sector_size;
		c->blocks[i].dirty_size = 0;
		c->blocks[i].wasted_size = 0;
		c->blocks[i].used_size = 0;
		c->blocks[i].first_node = NULL;
		c->blocks[i].last_node = NULL;
	}

	init_MUTEX(&c->alloc_sem);
	init_MUTEX(&c->erase_free_sem);
	init_waitqueue_head(&c->erase_wait);
	spin_lock_init(&c->erase_completion_lock);
	spin_lock_init(&c->inocache_lock);

	INIT_LIST_HEAD(&c->clean_list);
	INIT_LIST_HEAD(&c->very_dirty_list);
	INIT_LIST_HEAD(&c->dirty_list);
	INIT_LIST_HEAD(&c->erasable_list);
	INIT_LIST_HEAD(&c->erasing_list);
	INIT_LIST_HEAD(&c->erase_pending_list);
	INIT_LIST_HEAD(&c->erasable_pending_wbuf_list);
	INIT_LIST_HEAD(&c->erase_complete_list);
	INIT_LIST_HEAD(&c->free_list);
	INIT_LIST_HEAD(&c->bad_list);
	INIT_LIST_HEAD(&c->bad_used_list);
#ifdef CONFIG_JFFS2_NODEMERGE
	INIT_LIST_HEAD(&c->nodemerge_list);
#endif
	c->highest_ino = 1;

	if (jffs2_build_filesystem(c)) {
		D1(printk(KERN_DEBUG "build_fs failed\n"));
		jffs2_free_ino_caches(c);
		jffs2_free_raw_node_refs(c);
#ifdef CONFIG_JFFS2_LARGE_PARTITION
		consistent_free( c->blocks,
				 sizeof(struct jffs2_eraseblock) * c->nr_blocks,
				 c->blocks_phys );
#else
		kfree(c->blocks);
#endif
		return -EIO;
	}
	return 0;
}


static int __init jffs2_setup(char* s)
{
	if (strcmp(s, "error") == 0)
		orphaned_inodes_mode = ORPHANED_ERROR;
	else
		orphaned_inodes_mode = ORPHANED_DELETE;
	return 1;
}


__setup("jffs2_orphaned_inodes=", jffs2_setup);

/*
 * Local variables:
 *   c-basic-offset: 8
 * End:
 */
