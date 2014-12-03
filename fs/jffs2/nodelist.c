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
 *     19-Nov-2002 Lineo Japan, Inc.  add function jffs2_shrink_inode()
 *				      add counter of fragtree elements
 *     18-Nov-2002 Lineo Japan, Inc.  add dynamic construction of fragtree
 *
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include <linux/interrupt.h>
#include <linux/rbtree.h>
#include "nodelist.h"

void jffs2_add_fd_to_list(struct jffs2_sb_info *c, struct jffs2_full_dirent *new, struct jffs2_full_dirent **list)
{
	struct jffs2_full_dirent **prev = list;
	D1(printk(KERN_DEBUG "jffs2_add_fd_to_list( %p, %p (->%p))\n", new, list, *list));

	while ((*prev) && (*prev)->nhash <= new->nhash) {
		if ((*prev)->nhash == new->nhash && !strcmp((*prev)->name, new->name)) {
			/* Duplicate. Free one */
			if (new->version < (*prev)->version) {
				D1(printk(KERN_DEBUG "Eep! Marking new dirent node obsolete\n"));
				D1(printk(KERN_DEBUG "New dirent is \"%s\"->ino #%u. Old is \"%s\"->ino #%u\n", new->name, new->ino, (*prev)->name, (*prev)->ino));
				jffs2_mark_node_obsolete(c, new->raw);
				jffs2_free_full_dirent(new);
			} else {
				D1(printk(KERN_DEBUG "Marking old dirent node (ino #%u) obsolete\n", (*prev)->ino));
				new->next = (*prev)->next;
				jffs2_mark_node_obsolete(c, ((*prev)->raw));
				jffs2_free_full_dirent(*prev);
				*prev = new;
			}
			goto out;
		}
		prev = &((*prev)->next);
	}
	new->next = *prev;
	*prev = new;

 out:
	D2(while(*list) {
		printk(KERN_DEBUG "Dirent \"%s\" (hash 0x%08x, ino #%u\n", (*list)->name, (*list)->nhash, (*list)->ino);
		list = &(*list)->next;
	});
}

/* Put a new tmp_dnode_info into the list, keeping the list in 
   order of increasing version
*/
void jffs2_add_tn_to_list(struct jffs2_tmp_dnode_info *tn, struct jffs2_tmp_dnode_info **list)
{
	struct jffs2_tmp_dnode_info **prev = list;
	
	while ((*prev) && (*prev)->version < tn->version) {
		prev = &((*prev)->next);
	}
	tn->next = (*prev);
        *prev = tn;
}

static void jffs2_free_tmp_dnode_info_list(struct jffs2_tmp_dnode_info *tn)
{
	struct jffs2_tmp_dnode_info *next;

	while (tn) {
		next = tn;
		tn = tn->next;
		jffs2_free_full_dnode(next->fn);
		jffs2_free_tmp_dnode_info(next);
	}
}

static void jffs2_free_full_dirent_list(struct jffs2_full_dirent *fd)
{
	struct jffs2_full_dirent *next;

	while (fd) {
		next = fd->next;
		jffs2_free_full_dirent(fd);
		fd = next;
	}
}


/* Get tmp_dnode_info and full_dirent for all non-obsolete nodes associated
   with this ino, returning the former in order of version */

int jffs2_get_inode_nodes(struct jffs2_sb_info *c, ino_t ino, struct jffs2_inode_info *f,
			  struct jffs2_tmp_dnode_info **tnp, struct jffs2_full_dirent **fdp,
			  uint32_t *highest_version, uint32_t *latest_mctime,
			  uint32_t *mctime_ver)
{
	struct jffs2_raw_node_ref *ref = f->inocache->nodes;
	struct jffs2_tmp_dnode_info *tn, *ret_tn = NULL;
	struct jffs2_full_dirent *fd, *ret_fd = NULL;

	union jffs2_node_union node;
	size_t retlen;
	int err;

	*mctime_ver = 0;

	D1(printk(KERN_DEBUG "jffs2_get_inode_nodes(): ino #%lu\n", ino));
	if (!f->inocache->nodes) {
		printk(KERN_WARNING "Eep. no nodes for ino #%lu\n", ino);
	}

	spin_lock_bh(&c->erase_completion_lock);

	for (ref = f->inocache->nodes; ref && ref->next_in_ino; ref = ref->next_in_ino) {
		/* Work out whether it's a data node or a dirent node */
		if (ref_obsolete(ref)) {
			/* FIXME: On NAND flash we may need to read these */
			D1(printk(KERN_DEBUG "node at 0x%08x is obsoleted. Ignoring.\n", ref_offset(ref)));
			continue;
		}
		/* We can hold a pointer to a non-obsolete node without the spinlock,
		   but _obsolete_ nodes may disappear at any time, if the block
		   they're in gets erased */
		spin_unlock_bh(&c->erase_completion_lock);

		err = jffs2_flash_read(c, (ref_offset(ref)), min(ref->totlen, sizeof(node)), &retlen, (void *)&node);
		if (err) {
			printk(KERN_WARNING "error %d reading node at 0x%08x in get_inode_nodes()\n", err, ref_offset(ref));
			goto free_out;
		}
			

			/* Check we've managed to read at least the common node header */
		if (retlen < min(ref->totlen, sizeof(node.u))) {
			printk(KERN_WARNING "short read in get_inode_nodes()\n");
			err = -EIO;
			goto free_out;
		}
			
		switch (je16_to_cpu(node.u.nodetype)) {
		case JFFS2_NODETYPE_DIRENT:
			D1(printk(KERN_DEBUG "Node at %08x is a dirent node\n", ref_offset(ref)));
			if (retlen < sizeof(node.d)) {
				printk(KERN_WARNING "short read in get_inode_nodes()\n");
				err = -EIO;
				goto free_out;
			}
			if (je32_to_cpu(node.d.version) > *highest_version)
				*highest_version = je32_to_cpu(node.d.version);
			if (ref_obsolete(ref)) {
				/* Obsoleted. This cannot happen, surely? dwmw2 20020308 */
				printk(KERN_ERR "Dirent node at 0x%08x became obsolete while we weren't looking\n",
				       ref_offset(ref));
				BUG();
			}
			fd = jffs2_alloc_full_dirent(node.d.nsize+1);
			if (!fd) {
				err = -ENOMEM;
				goto free_out;
			}
			memset(fd,0,sizeof(struct jffs2_full_dirent) + node.d.nsize+1);
			fd->raw = ref;
			fd->version = je32_to_cpu(node.d.version);
			fd->ino = je32_to_cpu(node.d.ino);
			fd->type = node.d.type;

			/* Pick out the mctime of the latest dirent */
			if(fd->version > *mctime_ver) {
				*mctime_ver = fd->version;
				*latest_mctime = je32_to_cpu(node.d.mctime);
			}

			/* memcpy as much of the name as possible from the raw
			   dirent we've already read from the flash
			*/
			if (retlen > sizeof(struct jffs2_raw_dirent))
				memcpy(&fd->name[0], &node.d.name[0], min((uint32_t)node.d.nsize, (retlen-sizeof(struct jffs2_raw_dirent))));
				
			/* Do we need to copy any more of the name directly
			   from the flash?
			*/
			if (node.d.nsize + sizeof(struct jffs2_raw_dirent) > retlen) {
				int already = retlen - sizeof(struct jffs2_raw_dirent);
					
				err = jffs2_flash_read(c, (ref_offset(ref)) + retlen, 
						   node.d.nsize - already, &retlen, &fd->name[already]);
				if (!err && retlen != node.d.nsize - already)
					err = -EIO;
					
				if (err) {
					printk(KERN_WARNING "Read remainder of name in jffs2_get_inode_nodes(): error %d\n", err);
					jffs2_free_full_dirent(fd);
					goto free_out;
				}
			}
			fd->nhash = full_name_hash(fd->name, node.d.nsize);
			fd->next = NULL;
				/* Wheee. We now have a complete jffs2_full_dirent structure, with
				   the name in it and everything. Link it into the list 
				*/
			D1(printk(KERN_DEBUG "Adding fd \"%s\", ino #%u\n", fd->name, fd->ino));
			jffs2_add_fd_to_list(c, fd, &ret_fd);
			break;

		case JFFS2_NODETYPE_INODE:
			D1(printk(KERN_DEBUG "Node at %08x is a data node\n", ref_offset(ref)));
			if (retlen < sizeof(node.i)) {
				printk(KERN_WARNING "read too short for dnode\n");
				err = -EIO;
				goto free_out;
			}
			if (je32_to_cpu(node.i.version) > *highest_version)
				*highest_version = je32_to_cpu(node.i.version);
			D1(printk(KERN_DEBUG "version %d, highest_version now %d\n", je32_to_cpu(node.i.version), *highest_version));

			if (ref_obsolete(ref)) {
				/* Obsoleted. This cannot happen, surely? dwmw2 20020308 */
				printk(KERN_ERR "Inode node at 0x%08x became obsolete while we weren't looking\n",
				       ref_offset(ref));
				BUG();
			}
			tn = jffs2_alloc_tmp_dnode_info();
			if (!tn) {
				D1(printk(KERN_DEBUG "alloc tn failed\n"));
				err = -ENOMEM;
				goto free_out;
			}

			tn->fn = jffs2_alloc_full_dnode();
			if (!tn->fn) {
				D1(printk(KERN_DEBUG "alloc fn failed\n"));
				err = -ENOMEM;
				jffs2_free_tmp_dnode_info(tn);
				goto free_out;
			}
			tn->version = je32_to_cpu(node.i.version);
			tn->fn->ofs = je32_to_cpu(node.i.offset);
			/* There was a bug where we wrote hole nodes out with
			   csize/dsize swapped. Deal with it */
			if (node.i.compr == JFFS2_COMPR_ZERO && !je32_to_cpu(node.i.dsize) && je32_to_cpu(node.i.csize))
				tn->fn->size = je32_to_cpu(node.i.csize);
			else // normal case...
				tn->fn->size = je32_to_cpu(node.i.dsize);
			tn->fn->raw = ref;
			D1(printk(KERN_DEBUG "dnode @%08x: ver %u, offset %04x, dsize %04x\n",
				  ref_offset(ref), je32_to_cpu(node.i.version),
				  je32_to_cpu(node.i.offset), je32_to_cpu(node.i.dsize)));
			jffs2_add_tn_to_list(tn, &ret_tn);
			break;

		default:
			switch(je16_to_cpu(node.u.nodetype) & JFFS2_COMPAT_MASK) {
			case JFFS2_FEATURE_INCOMPAT:
				printk(KERN_NOTICE "Unknown INCOMPAT nodetype %04X at %08X\n", je16_to_cpu(node.u.nodetype), ref_offset(ref));
				break;
			case JFFS2_FEATURE_ROCOMPAT:
				printk(KERN_NOTICE "Unknown ROCOMPAT nodetype %04X at %08X\n", je16_to_cpu(node.u.nodetype), ref_offset(ref));
				break;
			case JFFS2_FEATURE_RWCOMPAT_COPY:
				printk(KERN_NOTICE "Unknown RWCOMPAT_COPY nodetype %04X at %08X\n", je16_to_cpu(node.u.nodetype), ref_offset(ref));
				break;
			case JFFS2_FEATURE_RWCOMPAT_DELETE:
				printk(KERN_NOTICE "Unknown RWCOMPAT_DELETE nodetype %04X at %08X\n", je16_to_cpu(node.u.nodetype), ref_offset(ref));
				break;
			}
		}
		spin_lock_bh(&c->erase_completion_lock);

	}
	spin_unlock_bh(&c->erase_completion_lock);
	*tnp = ret_tn;
	*fdp = ret_fd;

	return 0;

 free_out:
	jffs2_free_tmp_dnode_info_list(ret_tn);
	jffs2_free_full_dirent_list(ret_fd);
	return err;
}

struct jffs2_inode_cache *jffs2_get_ino_cache(struct jffs2_sb_info *c, int ino)
{
	struct jffs2_inode_cache *ret;

	D2(printk(KERN_DEBUG "jffs2_get_ino_cache(): ino %u\n", ino));
	spin_lock (&c->inocache_lock);

	ret = c->inocache_list[ino % INOCACHE_HASHSIZE];
	while (ret && ret->ino < ino) {
		ret = ret->next;
	}
	
	if (ret && ret->ino != ino)
		ret = NULL;

	spin_unlock(&c->inocache_lock);

	D2(printk(KERN_DEBUG "jffs2_get_ino_cache found %p for ino %u\n", ret, ino));
	return ret;
}

void jffs2_add_ino_cache (struct jffs2_sb_info *c, struct jffs2_inode_cache *new)
{
	struct jffs2_inode_cache **prev;
	D2(printk(KERN_DEBUG "jffs2_add_ino_cache: Add %p (ino #%u)\n", new, new->ino));
	spin_lock(&c->inocache_lock);
	
	prev = &c->inocache_list[new->ino % INOCACHE_HASHSIZE];

	while ((*prev) && (*prev)->ino < new->ino) {
		prev = &(*prev)->next;
	}
	new->next = *prev;
	*prev = new;

	spin_unlock(&c->inocache_lock);
}

void jffs2_del_ino_cache(struct jffs2_sb_info *c, struct jffs2_inode_cache *old)
{
	struct jffs2_inode_cache **prev;
	D2(printk(KERN_DEBUG "jffs2_del_ino_cache: Del %p (ino #%u)\n", old, old->ino));
	spin_lock(&c->inocache_lock);
	
	prev = &c->inocache_list[old->ino % INOCACHE_HASHSIZE];
	
	while ((*prev) && (*prev)->ino < old->ino) {
		prev = &(*prev)->next;
	}
	if ((*prev) == old) {
		*prev = old->next;
	}

	spin_unlock(&c->inocache_lock);
}

void jffs2_free_ino_caches(struct jffs2_sb_info *c)
{
	int i;
	struct jffs2_inode_cache *this, *next;
	
	for (i=0; i<INOCACHE_HASHSIZE; i++) {
		this = c->inocache_list[i];
		while (this) {
			next = this->next;
			D2(printk(KERN_DEBUG "jffs2_free_ino_caches: Freeing ino #%u at %p\n", this->ino, this));
			jffs2_free_inode_cache(this);
			this = next;
		}
		c->inocache_list[i] = NULL;
	}
}

void jffs2_free_raw_node_refs(struct jffs2_sb_info *c)
{
	int i;
	struct jffs2_raw_node_ref *this, *next;

	for (i=0; i<c->nr_blocks; i++) {
		this = c->blocks[i].first_node;
		while(this) {
			next = this->next_phys;
			jffs2_free_raw_node_ref(this);
			this = next;
		}
		c->blocks[i].first_node = c->blocks[i].last_node = NULL;
	}
}
	
struct jffs2_node_frag *jffs2_lookup_node_frag(rb_root_t *fragtree, uint32_t offset)
{
	/* The common case in lookup is that there will be a node 
	   which precisely matches. So we go looking for that first */
	rb_node_t *next;
	struct jffs2_node_frag *prev = NULL;
	struct jffs2_node_frag *frag = NULL;

	D2(printk(KERN_DEBUG "jffs2_lookup_node_frag(%p, %d)\n", fragtree, offset));

	next = fragtree->rb_node;

	while(next) {
		frag = rb_entry(next, struct jffs2_node_frag, rb);

		D2(printk(KERN_DEBUG "Considering frag %d-%d (%p). left %p, right %p\n",
			  frag->ofs, frag->ofs+frag->size, frag, frag->rb.rb_left, frag->rb.rb_right));
		if (frag->ofs + frag->size <= offset) {
			D2(printk(KERN_DEBUG "Going right from frag %d-%d, before the region we care about\n",
				  frag->ofs, frag->ofs+frag->size));
			/* Remember the closest smaller match on the way down */
			if (!prev || frag->ofs > prev->ofs)
				prev = frag;
			next = frag->rb.rb_right;
		} else if (frag->ofs > offset) {
			D2(printk(KERN_DEBUG "Going left from frag %d-%d, after the region we care about\n",
				  frag->ofs, frag->ofs+frag->size));
			next = frag->rb.rb_left;
		} else {
			D2(printk(KERN_DEBUG "Returning frag %d,%d, matched\n",
				  frag->ofs, frag->ofs+frag->size));
			return frag;
		}
	}

	/* Exact match not found. Go back up looking at each parent,
	   and return the closest smaller one */

	if (prev)
		D2(printk(KERN_DEBUG "No match. Returning frag %d,%d, closest previous\n",
			  prev->ofs, prev->ofs+prev->size));
	else 
		D2(printk(KERN_DEBUG "Returning NULL, empty fragtree\n"));
	
	return prev;
}

/* Pass 'c' argument to indicate that nodes should be marked obsolete as
   they're killed. */
void jffs2_kill_fragtree(rb_root_t *root, struct jffs2_sb_info *c)
{
	struct jffs2_node_frag *frag;
	struct jffs2_node_frag *parent;

	if (!root->rb_node)
		return;

	frag = (rb_entry(root->rb_node, struct jffs2_node_frag, rb));

	while(frag) {
		if (frag->rb.rb_left) {
			D2(printk(KERN_DEBUG "Going left from frag (%p) %d-%d\n", 
				  frag, frag->ofs, frag->ofs+frag->size));
			frag = frag_left(frag);
			continue;
		}
		if (frag->rb.rb_right) {
			D2(printk(KERN_DEBUG "Going right from frag (%p) %d-%d\n", 
				  frag, frag->ofs, frag->ofs+frag->size));
			frag = frag_right(frag);
			continue;
		}

		D2(printk(KERN_DEBUG "jffs2_kill_fragtree: frag at 0x%x-0x%x: node %p, frags %d--\n",
			  frag->ofs, frag->ofs+frag->size, frag->node,
			  frag->node?frag->node->frags:0));
			
		if (frag->node && !(--frag->node->frags)) {
			/* Not a hole, and it's the final remaining frag 
			   of this node. Free the node */
			if (c)
				jffs2_mark_node_obsolete(c, frag->node->raw);
			
			jffs2_free_full_dnode(frag->node);
		}
		parent = frag_parent(frag);
		if (parent) {
			if (frag_left(parent) == frag)
				parent->rb.rb_left = NULL;
			else 
				parent->rb.rb_right = NULL;
		}

		jffs2_free_node_frag(frag);
		frag = parent;
	}
}

void jffs2_fragtree_insert(struct jffs2_node_frag *newfrag, struct jffs2_node_frag *base)
{
	rb_node_t *parent = &base->rb;
	rb_node_t **link = &parent;

	D2(printk(KERN_DEBUG "jffs2_fragtree_insert(%p; %d-%d, %p)\n", newfrag, 
		  newfrag->ofs, newfrag->ofs+newfrag->size, base));

	while (*link) {
		parent = *link;
		base = rb_entry(parent, struct jffs2_node_frag, rb);
	
		D2(printk(KERN_DEBUG "fragtree_insert considering frag at 0x%x\n", base->ofs));
		if (newfrag->ofs > base->ofs)
			link = &base->rb.rb_right;
		else if (newfrag->ofs < base->ofs)
			link = &base->rb.rb_left;
		else {
			printk(KERN_CRIT "Duplicate frag at %08x (%p,%p)\n", newfrag->ofs, newfrag, base);
			BUG();
		}
	}

	rb_link_node(&newfrag->rb, &base->rb, link);
}

rb_node_t *rb_next(rb_node_t *node)
{
	/* If we have a right-hand child, go down and then left as far
	   as we can. */
	if (node->rb_right) {
		node = node->rb_right; 
		while (node->rb_left)
			node=node->rb_left;
		return node;
	}

	/* No right-hand children.  Everything down and left is
	   smaller than us, so any 'next' node must be in the general
	   direction of our parent. Go up the tree; any time the
	   ancestor is a right-hand child of its parent, keep going
	   up. First time it's a left-hand child of its parent, said
	   parent is our 'next' node. */
	while (node->rb_parent && node == node->rb_parent->rb_right)
		node = node->rb_parent;

	return node->rb_parent;
}

rb_node_t *rb_prev(rb_node_t *node)
{
	if (node->rb_left) {
		node = node->rb_left; 
		while (node->rb_right)
			node=node->rb_right;
		return node;
	}
	while (node->rb_parent && node == node->rb_parent->rb_left)
		node = node->rb_parent;

	return node->rb_parent;
}

void rb_replace_node(rb_node_t *victim, rb_node_t *new, rb_root_t *root)
{
	rb_node_t *parent = victim->rb_parent;

	/* Set the surrounding nodes to point to the replacement */
	if (parent) {
		if (victim == parent->rb_left)
			parent->rb_left = new;
		else
			parent->rb_right = new;
	} else {
		root->rb_node = new;
	}
	if (victim->rb_left)
		victim->rb_left->rb_parent = new;
	if (victim->rb_right)
		victim->rb_right->rb_parent = new;

	/* Copy the pointers/colour from the victim to the replacement */
	*new = *victim;
}

#ifdef CONFIG_JFFS2_DYNFRAGTREE
int jffs2_construct_fragtree(struct jffs2_sb_info *c, struct jffs2_inode_info *f)
{
	struct jffs2_tmp_dnode_info *tn_list;
	struct jffs2_full_dirent *dummy_fd_list;
	uint32_t dummy_mctime, dummy_ver, highest_version;
	int ret;

	if (! f->inocache) {
		printk(KERN_WARNING "%s: ino %lu: f->inocache is NULL\n",
		       __func__, OFNI_EDONI_2SFFJ(f)->i_ino);
		return -ENOENT;
	}

	if (frag_first(&f->fragtree)) {
		printk(KERN_NOTICE "%s: f->fragtree already exists\n", __func__);
		return 0;
	}

	highest_version = f->highest_version;
	ret = jffs2_get_inode_nodes(c, f->inocache->ino, f, &tn_list, &dummy_fd_list,
				    &highest_version, &dummy_mctime, &dummy_ver);
	if (ret) {
		printk(KERN_CRIT "%s for ino %u returned %d\n",
		       __func__, f->inocache->ino, ret);
		return ret;
	}

	if (f->highest_version != highest_version) {
		printk(KERN_NOTICE "%s: f->highest_version:%u != %u\n",
		       __func__, f->highest_version, highest_version);
		f->highest_version = highest_version;
	}

	if (dummy_fd_list) {
		printk(KERN_NOTICE "%s for directory ino #%u\n",
		       __func__, f->inocache->ino);
		return 0;
	}

	while (tn_list) {
		struct jffs2_tmp_dnode_info *tn;
		struct jffs2_full_dnode *fn;

		tn = tn_list;
		fn = tn->fn;
		if (fn->size)
			jffs2_add_full_dnode_to_inode(c, f, fn);
		tn_list = tn->next;
		jffs2_free_tmp_dnode_info(tn);
	}

	return 0;
}

void jffs2_shrink_inode(struct inode *inode)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);

	if (S_ISREG(inode->i_mode)) {
		jffs2_kill_fragtree(&f->fragtree, NULL);
		CLEAR_NR_FRAGS(f);
		f->fragtree = RB_ROOT;
	}
}

uint32_t jffs2_get_nr_frags(struct inode *inode)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	return f->nr_frags;
}

int jffs2_frags_exists(struct inode *inode)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	return f->fragtree.rb_node != NULL;
}

#endif

/*
 * Local variables:
 *   c-basic-offset: 8
 * End:
 */
