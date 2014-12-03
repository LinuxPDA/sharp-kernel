/*
 * fs/jffs2/nodemerge.c
 *
 * Copyright (C) 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * $Id: nodemerge.c,v 1.1.1.1 2002/12/18 10:29:25 yamade Exp $
 *
 * ChangLog:
 *     05-Dec-2002 SHARP  nodemerge-thershold is changable
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/rbtree.h>
#include <linux/pagemap.h>
#include <linux/jffs2_fs_i.h>
#include "nodelist.h"
#include "nodemerge.h"

/*
 * return number of jffs2_node_frags for specified page.
 */
static int
jffs2_count_node_frags(rb_root_t* fragtree,
		       unsigned long page_index)
{
    struct jffs2_node_frag* frag;
    uint32_t offset = page_index << PAGE_CACHE_SHIFT;
    uint32_t next_offset = (page_index + 1) << PAGE_CACHE_SHIFT;
    int count = 0;

    for (frag = jffs2_lookup_node_frag(fragtree, offset);
	 frag && frag->ofs < offset; frag = frag_next(frag))
	;
    while (frag && frag->ofs < next_offset) {
	count++;
	frag = frag_next(frag);
    }

    return count;
}


static uint32_t
frag_totlen(const struct jffs2_node_frag* frag)
{
    return frag->node ? frag->node->raw->totlen : 0;
}


/*
 * return most dirty block in the page.
 */
static struct jffs2_eraseblock*
jffs2_most_dirty_block_in_page(const struct jffs2_sb_info* c,
			       rb_root_t* fragtree,
			       unsigned long page_index)
{
    struct jffs2_node_frag* frag;
    uint32_t offset = page_index << PAGE_CACHE_SHIFT;
    uint32_t next_offset = (page_index + 1) << PAGE_CACHE_SHIFT;
    struct jffs2_eraseblock* ret_jeb = 0;
    uint32_t dirty_size = 0;
    
    for (frag = jffs2_lookup_node_frag(fragtree, offset);
	 frag && frag->ofs < next_offset; frag = frag_next(frag)) {
	struct jffs2_eraseblock* jeb;

	if (! frag->node)
	    continue;

	jeb = &c->blocks[frag->node->raw->flash_offset / c->sector_size];
	if (jeb == c->gcblock)	/* this page is been gcing */
	    return NULL;
	else if (jeb != c->nextblock && 
		 jeb->wasted_size + jeb->dirty_size + frag_totlen(frag) > dirty_size) {
	    ret_jeb = jeb;
	    dirty_size = jeb->wasted_size + jeb->dirty_size;
	}
    }
    return ret_jeb;
}


/*
 * Must be called with the f->sem held.
 */
void
jffs2_merge_nodes(struct jffs2_sb_info* c,
		  struct jffs2_inode_info* f,
		  uint32_t size, int frags_threshold)
{
    unsigned long i;
    unsigned long nr_pages = (size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;

    for (i = 0; i < nr_pages; i++) {
	struct jffs2_eraseblock* jeb;

	if (jffs2_count_node_frags(&f->fragtree, i) <= frags_threshold)
	    continue;

	jeb = jffs2_most_dirty_block_in_page(c, &f->fragtree, i);
	if (! jeb)
	    continue;

	/* must do erase_completion_lock, because jeb may be linked to
	 * the free_list */
	spin_lock_bh(&c->erase_completion_lock);
	list_del(&jeb->list);
	list_add_tail(&jeb->list, &c->nodemerge_list);
	spin_unlock_bh(&c->erase_completion_lock);
    }

    jffs2_garbage_collect_trigger(c);
}
