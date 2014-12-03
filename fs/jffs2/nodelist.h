/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 * $Id: nodelist.h,v 1.83 2002/09/06 16:46:29 dwmw2 Exp $
 *
 * ChangeLog:
 *     05-Dec-2002 SHARP  adjust REVERVED_BLOCKS values for storage-full
 *     27-Nov-2002 Lineo Japan, Inc.  add effective-gc mode
 *     23-Nov-2002 Lineo Japan, Inc.  add JFFS2_RESERVED_BLOCKS_DIRTY
 *				      add JFFS2_RESERVED_BLOCKS_CLEAN
 *     19-Nov-2002 Lineo Japan, Inc.  add counter of fragtree elements
 *     18-Nov-2002 Lineo Japan, Inc.  add dynamic construction of fragtree
 *     11-Nov-2002 Lineo Japan, Inc.  add JFFS2_RESERVED_BLOCKS_ROOT
 *     29-Oct-2002 Lineo Japan, Inc.  add JFFS2_RESERVED_BLOCKS_BAD and JFFS2_MAX_CONT_GC
 *
 */

#ifndef __JFFS2_NODELIST_H__
#define __JFFS2_NODELIST_H__

#include <linux/config.h>
#include <linux/fs.h>

#include <linux/mtd/compatmac.h> /* For min/max in older kernels */
#include <linux/jffs2.h>
#include <linux/jffs2_fs_sb.h>
#include <linux/jffs2_fs_i.h>
#include "os-linux.h"

#ifndef CONFIG_JFFS2_FS_DEBUG
#define CONFIG_JFFS2_FS_DEBUG 2
#endif

#if CONFIG_JFFS2_FS_DEBUG > 0
#define D1(x) x
#else
#define D1(x)
#endif

#if CONFIG_JFFS2_FS_DEBUG > 1
#define D2(x) x
#else
#define D2(x)
#endif

/*
  This is all we need to keep in-core for each raw node during normal
  operation. As and when we do read_inode on a particular inode, we can
  scan the nodes which are listed for it and build up a proper map of 
  which nodes are currently valid. JFFSv1 always used to keep that whole
  map in core for each inode.
*/
struct jffs2_raw_node_ref
{
	struct jffs2_raw_node_ref *next_in_ino; /* Points to the next raw_node_ref
		for this inode. If this is the last, it points to the inode_cache
		for this inode instead. The inode_cache will have NULL in the first
		word so you know when you've got there :) */
	struct jffs2_raw_node_ref *next_phys;
	uint32_t flash_offset;
	uint32_t totlen;
	
        /* flash_offset & 3 always has to be zero, because nodes are
	   always aligned at 4 bytes. So we have a couple of extra bits
	   to play with. So we set the least significant bit to 1 to
	   signify that the node is obsoleted by later nodes.
	*/
#define REF_UNCHECKED	0	/* We haven't yet checked the CRC or built its inode */
#define REF_OBSOLETE	1	/* Obsolete, can be completely ignored */
#define REF_PRISTINE	2	/* Completely clean. GC without looking */
#define REF_NORMAL	3	/* Possibly overlapped. Read the page and write again on GC */
#define ref_flags(ref)		((ref)->flash_offset & 3)
#define ref_offset(ref)		((ref)->flash_offset & ~3)
#define ref_obsolete(ref)	(((ref)->flash_offset & 3) == REF_OBSOLETE)
#define mark_ref_normal(ref)    do { (ref)->flash_offset = ref_offset(ref) | REF_NORMAL; } while(0)
};

/* 
   Used for keeping track of deletion nodes &c, which can only be marked
   as obsolete when the node which they mark as deleted has actually been 
   removed from the flash.
*/
struct jffs2_raw_node_ref_list {
	struct jffs2_raw_node_ref *rew;
	struct jffs2_raw_node_ref_list *next;
};

/* For each inode in the filesystem, we need to keep a record of
   nlink, because it would be a PITA to scan the whole directory tree
   at read_inode() time to calculate it, and to keep sufficient information
   in the raw_node_ref (basically both parent and child inode number for 
   dirent nodes) would take more space than this does. We also keep
   a pointer to the first physical node which is part of this inode, too.
*/
struct jffs2_inode_cache {
	struct jffs2_scan_info *scan; /* Used during scan to hold
		temporary lists of nodes, and later must be set to
		NULL to mark the end of the raw_node_ref->next_in_ino
		chain. */
	struct jffs2_inode_cache *next;
	struct jffs2_raw_node_ref *nodes;
	uint32_t ino;
	int nlink;
};

#define INOCACHE_HASHSIZE 128

struct jffs2_scan_info {
	struct jffs2_full_dirent *dents;
	struct jffs2_tmp_dnode_info *tmpnodes;
	/* Latest i_size info */
	uint32_t version;
	uint32_t isize;
};
/*
  Larger representation of a raw node, kept in-core only when the 
  struct inode for this particular ino is instantiated.
*/

struct jffs2_full_dnode
{
	struct jffs2_raw_node_ref *raw;
	uint32_t ofs; /* Don't really need this, but optimisation */
	uint32_t size;
	uint32_t frags; /* Number of fragments which currently refer
			to this node. When this reaches zero, 
			the node is obsolete.
		     */
};

/* 
   Even larger representation of a raw node, kept in-core only while
   we're actually building up the original map of which nodes go where,
   in read_inode()
*/
struct jffs2_tmp_dnode_info
{
	struct jffs2_tmp_dnode_info *next;
	struct jffs2_full_dnode *fn;
	uint32_t version;
};       

struct jffs2_full_dirent
{
	struct jffs2_raw_node_ref *raw;
	struct jffs2_full_dirent *next;
	uint32_t version;
	uint32_t ino; /* == zero for unlink */
	unsigned int nhash;
	unsigned char type;
	unsigned char name[0];
};
/*
  Fragments - used to build a map of which raw node to obtain 
  data from for each part of the ino
*/
struct jffs2_node_frag
{
	rb_node_t rb;
	struct jffs2_full_dnode *node; /* NULL for holes */
	uint32_t size;
	uint32_t ofs; /* Don't really need this, but optimisation */
};

struct jffs2_eraseblock
{
	struct list_head list;
	int bad_count;
	uint32_t offset;		/* of this block in the MTD */

	uint32_t used_size;
	uint32_t dirty_size;
	uint32_t wasted_size;
	uint32_t free_size;	/* Note that sector_size - free_size
				   is the address of the first free space */
	struct jffs2_raw_node_ref *first_node;
	struct jffs2_raw_node_ref *last_node;

	struct jffs2_raw_node_ref *gc_node;	/* Next node to be garbage collected */

	/* For deletia. When a dirent node in this eraseblock is
	   deleted by a node elsewhere, that other node can only 
	   be marked as obsolete when this block is actually erased.
	   So we keep a list of the nodes to mark as obsolete when
	   the erase is completed.
	*/
	// MAYBE	struct jffs2_raw_node_ref_list *deletia;
};

#define ACCT_SANITY_CHECK(c, jeb) do { \
	if (jeb->used_size + jeb->dirty_size + jeb->free_size +jeb->wasted_size != c->sector_size) { \
		printk(KERN_NOTICE "Eeep. Space accounting for block at 0x%08x is screwed\n", jeb->offset); \
		printk(KERN_NOTICE "free 0x%08x + dirty 0x%08x + used %08x + wasted %08x != total %08x\n", \
		jeb->free_size, jeb->dirty_size, jeb->used_size, jeb->wasted_size, c->sector_size); \
		BUG(); \
	} \
	if (c->used_size + c->dirty_size + c->free_size + c->erasing_size + c->bad_size + c->wasted_size != c->flash_size) { \
		printk(KERN_NOTICE "Eeep. Space accounting superblock info is screwed\n"); \
		printk(KERN_NOTICE "free 0x%08x + dirty 0x%08x + used %08x + erasing %08x + bad %08x + wasted %08x != total %08x\n", \
		c->free_size, c->dirty_size, c->used_size, c->erasing_size, c->bad_size, c->wasted_size, c->flash_size); \
		BUG(); \
	} \
} while(0)

#define ACCT_PARANOIA_CHECK(jeb) do { \
		uint32_t my_used_size = 0; \
		struct jffs2_raw_node_ref *ref2 = jeb->first_node; \
		while (ref2) { \
			if (!ref_obsolete(ref2)) \
				my_used_size += ref2->totlen; \
			ref2 = ref2->next_phys; \
		} \
		if (my_used_size != jeb->used_size) { \
			printk(KERN_NOTICE "Calculated used size %08x != stored used size %08x\n", my_used_size, jeb->used_size); \
			BUG(); \
		} \
	} while(0)

#define ALLOC_NORMAL	0	/* Normal allocation */
#define ALLOC_DELETION	1	/* Deletion node. Best to allow it */
#define ALLOC_GC	2	/* Space requested for GC. Give it or die */

#define JFFS2_RESERVED_BLOCKS_BASE 3						/* Number of free blocks there must be before we... */
#define JFFS2_RESERVED_BLOCKS_WRITE (JFFS2_RESERVED_BLOCKS_BASE + 2)		/* ... allow a normal filesystem write */
#define JFFS2_RESERVED_BLOCKS_DELETION (JFFS2_RESERVED_BLOCKS_BASE + 1)		/* ... allow a normal filesystem deletion */
#define JFFS2_RESERVED_BLOCKS_GCTRIGGER 39					/* ... wake up the GC thread */
#define JFFS2_RESERVED_BLOCKS_GCBAD (JFFS2_RESERVED_BLOCKS_BASE + 1)		/* ... pick a block from the bad_list to GC */
#define JFFS2_RESERVED_BLOCKS_GCMERGE (JFFS2_RESERVED_BLOCKS_BASE)		/* ... merge pages when garbage collecting */
//#define JFFS2_RESERVED_BLOCKS_BAD 24
#define JFFS2_RESERVED_BLOCKS_BAD CONFIG_JFFS2_RESERVED_BLOCKS_BAD
#define JFFS2_RESERVED_BLOCKS_ROOT 5
#define JFFS2_RESERVED_BLOCKS_DIRTY 24
#define JFFS2_RESERVED_BLOCKS_CLEAN 12
#if JFFS2_RESERVED_BLOCKS_CLEAN < JFFS2_RESERVED_BLOCKS_WRITE + JFFS2_RESERVED_BLOCKS_ROOT
#error assure that JFFS2_RESERVED_BLOCKS_CLEAN >= JFFS2_RESERVED_BLOCKS_WRITE + JFFS2_RESERVED_BLOCKS_ROOT
#endif
#if JFFS2_RESERVED_BLOCKS_GCTRIGGER <= JFFS2_RESERVED_BLOCKS_WRITE + JFFS2_RESERVED_BLOCKS_ROOT + JFFS2_RESERVED_BLOCKS_DIRTY
#error assure that JFFS2_RESERVED_BLOCKS_GCTRIGGER > JFFS2_RESERVED_BLOCKS_WRITE + JFFS2_RESERVED_BLOCKS_ROOT + JFFS2_RESERVED_BLOCKS_DIRTY
#endif
#define JFFS2_MAX_CONT_GC 3000

#define NR_AVAIL_BLOCKS(c) ((c)->nr_free_blocks + (c)->nr_erasing_blocks - max(0, JFFS2_RESERVED_BLOCKS_BAD - (c)->nr_bad_blocks))

/* How much dirty space before it goes on the very_dirty_list */
#define VERYDIRTY(c, size) ((size) >= ((c)->sector_size / 2))

/* check if dirty space is more than 255 Byte */
#define ISDIRTY(size) ((size) >  sizeof (struct jffs2_raw_inode) + JFFS2_MIN_DATA_LEN) 

/* node merge threshold */
#define JFFS2_FRAGS_NR_NODES_THRESHOLD_NORMAL	 4
#define JFFS2_FRAGS_NR_NODES_THRESHOLD_FORCE	 1

#define PAD(x) (((x)+3)&~3)

typedef enum { GCMODE_NORMAL, GCMODE_EFFECTIVE, } jffs2_gcmode_t;

#define GC_EFFECTIVE_BLOCK_DIRTY_SIZE		(1*1024)
#define GC_EFFECTIVE_BLOCK_DIRTY_SIZE_L		(2*1024)
#define GC_EFFECTIVE_BLOCK_DIRTY_SIZE_LL	(4*1024)
#define GC_EFFECTIVE_TOTAL_DIRTY_SIZE(c)	(JFFS2_RESERVED_BLOCKS_DIRTY * (c)->sector_size)
#define GC_EFFECTIVE_TOTAL_DIRTY_SIZE_L(c)	(JFFS2_RESERVED_BLOCKS_DIRTY * (c)->sector_size * 2)
#define GC_EFFECTIVE_TOTAL_DIRTY_SIZE_LL(c)	(JFFS2_RESERVED_BLOCKS_DIRTY * (c)->sector_size * 4)

static inline int jffs2_raw_ref_to_inum(struct jffs2_raw_node_ref *raw)
{
	while(raw->next_in_ino) {
		raw = raw->next_in_ino;
	}

	return ((struct jffs2_inode_cache *)raw)->ino;
}

static inline struct jffs2_node_frag *frag_first(rb_root_t *root)
{
	rb_node_t *node = root->rb_node;

	if (!node)
		return NULL;
	while(node->rb_left)
		node = node->rb_left;
	return rb_entry(node, struct jffs2_node_frag, rb);
}
#define rb_parent(rb) ((rb)->rb_parent)
#define frag_next(frag) rb_entry(rb_next(&(frag)->rb), struct jffs2_node_frag, rb)
#define frag_prev(frag) rb_entry(rb_prev(&(frag)->rb), struct jffs2_node_frag, rb)
#define frag_parent(frag) rb_entry(rb_parent(&(frag)->rb), struct jffs2_node_frag, rb)
#define frag_left(frag) rb_entry((frag)->rb.rb_left, struct jffs2_node_frag, rb)
#define frag_right(frag) rb_entry((frag)->rb.rb_right, struct jffs2_node_frag, rb)
#define frag_erase(frag, list) rb_erase(&frag->rb, list);

/* nodelist.c */
D1(void jffs2_print_frag_list(struct jffs2_inode_info *f));
void jffs2_add_fd_to_list(struct jffs2_sb_info *c, struct jffs2_full_dirent *new, struct jffs2_full_dirent **list);
void jffs2_add_tn_to_list(struct jffs2_tmp_dnode_info *tn, struct jffs2_tmp_dnode_info **list);
int jffs2_get_inode_nodes(struct jffs2_sb_info *c, ino_t ino, struct jffs2_inode_info *f,
			  struct jffs2_tmp_dnode_info **tnp, struct jffs2_full_dirent **fdp,
			  uint32_t *highest_version, uint32_t *latest_mctime,
			  uint32_t *mctime_ver);
struct jffs2_inode_cache *jffs2_get_ino_cache(struct jffs2_sb_info *c, int ino);
void jffs2_add_ino_cache (struct jffs2_sb_info *c, struct jffs2_inode_cache *new);
void jffs2_del_ino_cache(struct jffs2_sb_info *c, struct jffs2_inode_cache *old);
void jffs2_free_ino_caches(struct jffs2_sb_info *c);
void jffs2_free_raw_node_refs(struct jffs2_sb_info *c);
struct jffs2_node_frag *jffs2_lookup_node_frag(rb_root_t *fragtree, uint32_t offset);
void jffs2_kill_fragtree(rb_root_t *root, struct jffs2_sb_info *c_delete);
void jffs2_fragtree_insert(struct jffs2_node_frag *newfrag, struct jffs2_node_frag *base);
rb_node_t *rb_next(rb_node_t *);
rb_node_t *rb_prev(rb_node_t *);
void rb_replace_node(rb_node_t *victim, rb_node_t *new, rb_root_t *root);

/* nodemgmt.c */
int jffs2_reserve_space(struct jffs2_sb_info *c, uint32_t minsize, uint32_t *ofs, uint32_t *len, int prio);
int jffs2_reserve_space_gc(struct jffs2_sb_info *c, uint32_t minsize, uint32_t *ofs, uint32_t *len);
int jffs2_add_physical_node_ref(struct jffs2_sb_info *c, struct jffs2_raw_node_ref *new);
void jffs2_complete_reservation(struct jffs2_sb_info *c);
void jffs2_mark_node_obsolete(struct jffs2_sb_info *c, struct jffs2_raw_node_ref *raw);
void jffs2_dump_block_lists(struct jffs2_sb_info *c);

/* write.c */
int jffs2_do_new_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f, uint32_t mode, struct jffs2_raw_inode *ri);
struct jffs2_full_dnode *jffs2_write_dnode(struct jffs2_sb_info *c, struct jffs2_inode_info *f, struct jffs2_raw_inode *ri, const unsigned char *data, uint32_t datalen, uint32_t flash_ofs,  uint32_t *writelen);
struct jffs2_full_dirent *jffs2_write_dirent(struct jffs2_sb_info *c, struct jffs2_inode_info *f, struct jffs2_raw_dirent *rd, const unsigned char *name, uint32_t namelen, uint32_t flash_ofs,  uint32_t *writelen);
int jffs2_write_inode_range(struct jffs2_sb_info *c, struct jffs2_inode_info *f,
			    struct jffs2_raw_inode *ri, unsigned char *buf, 
			    uint32_t offset, uint32_t writelen, uint32_t *retlen);
int jffs2_do_create(struct jffs2_sb_info *c, struct jffs2_inode_info *dir_f, struct jffs2_inode_info *f, struct jffs2_raw_inode *ri, const char *name, int namelen);
int jffs2_do_unlink(struct jffs2_sb_info *c, struct jffs2_inode_info *dir_f, const char *name, int namelen, struct jffs2_inode_info *dead_f);
int jffs2_do_link (struct jffs2_sb_info *c, struct jffs2_inode_info *dir_f, uint32_t ino, uint8_t type, const char *name, int namelen);


/* readinode.c */
int jffs2_truncate_fraglist_1 (struct jffs2_sb_info *c, rb_root_t *list, uint32_t size);
#ifdef CONFIG_JFFS2_DYNFRAGTREE
static inline void jffs2_truncate_fraglist (struct jffs2_sb_info *c, struct jffs2_inode_info *f, uint32_t size)
{
	f->nr_frags -= jffs2_truncate_fraglist_1(c, &f->fragtree, size);
}
int jffs2_add_full_dnode_to_fraglist_1(struct jffs2_sb_info *c, rb_root_t *list, struct jffs2_full_dnode *fn, int* added_frags);
static inline int jffs2_add_full_dnode_to_fraglist(struct jffs2_sb_info *c, rb_root_t *list, struct jffs2_full_dnode *fn)
{
	int dummy = 0;
	return jffs2_add_full_dnode_to_fraglist_1(c, list, fn, &dummy);
}

#define ADDED_FRAGS_INC() ((*added_frags)++)
#define ADDED_FRAGS_DEC() ((*added_frags)--)
#define CLEAR_NR_FRAGS(f) ((f)->nr_frags = 0)

static inline void jffs2_check_nr_frags(struct jffs2_inode_info *f)
{
    struct jffs2_node_frag *frag;
    uint32_t count = 0;
    for (frag = frag_first(&f->fragtree); frag; frag = frag_next(frag))
	count++;
    if (count != f->nr_frags) {
	printk("ino #%lu: nr_frags:%u != %u\n", OFNI_EDONI_2SFFJ(f)->i_ino, f->nr_frags, count);
    }
    else
	printk("ino #%lu: nr_frags:%u\n", OFNI_EDONI_2SFFJ(f)->i_ino, f->nr_frags);
}
#else
static inline void jffs2_truncate_fraglist (struct jffs2_sb_info *c, struct jffs2_inode_info *f, uint32_t size)
{
	jffs2_truncate_fraglist_1(c, &f->fragtree, size);
}
int jffs2_add_full_dnode_to_fraglist(struct jffs2_sb_info *c, rb_root_t *list, struct jffs2_full_dnode *fn);

#define ADDED_FRAGS_INC()
#define ADDED_FRAGS_DEC()
#define CLEAR_NR_FRAGS(f)
#endif
int jffs2_add_full_dnode_to_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f, struct jffs2_full_dnode *fn);
int jffs2_do_read_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f, 
			uint32_t ino, struct jffs2_raw_inode *latest_node);
void jffs2_do_clear_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f);

/* malloc.c */
int jffs2_create_slab_caches(void);
void jffs2_destroy_slab_caches(void);

struct jffs2_full_dirent *jffs2_alloc_full_dirent(int namesize);
void jffs2_free_full_dirent(struct jffs2_full_dirent *);
struct jffs2_full_dnode *jffs2_alloc_full_dnode(void);
void jffs2_free_full_dnode(struct jffs2_full_dnode *);
struct jffs2_raw_dirent *jffs2_alloc_raw_dirent(void);
void jffs2_free_raw_dirent(struct jffs2_raw_dirent *);
struct jffs2_raw_inode *jffs2_alloc_raw_inode(void);
void jffs2_free_raw_inode(struct jffs2_raw_inode *);
struct jffs2_tmp_dnode_info *jffs2_alloc_tmp_dnode_info(void);
void jffs2_free_tmp_dnode_info(struct jffs2_tmp_dnode_info *);
struct jffs2_raw_node_ref *jffs2_alloc_raw_node_ref(void);
void jffs2_free_raw_node_ref(struct jffs2_raw_node_ref *);
struct jffs2_node_frag *jffs2_alloc_node_frag(void);
void jffs2_free_node_frag(struct jffs2_node_frag *);
struct jffs2_inode_cache *jffs2_alloc_inode_cache(void);
void jffs2_free_inode_cache(struct jffs2_inode_cache *);

/* gc.c */
int jffs2_garbage_collect_pass(struct jffs2_sb_info *c, jffs2_gcmode_t);

/* read.c */
int jffs2_read_dnode(struct jffs2_sb_info *c, struct jffs2_full_dnode *fd, unsigned char *buf, int ofs, int len);
int jffs2_read_inode_range(struct jffs2_sb_info *c, struct jffs2_inode_info *f,
			   unsigned char *buf, uint32_t offset, uint32_t len);
char *jffs2_getlink(struct jffs2_sb_info *c, struct jffs2_inode_info *f);


/* compr.c */
unsigned char jffs2_compress(unsigned char *data_in, unsigned char *cpage_out, 
			     uint32_t *datalen, uint32_t *cdatalen);
int jffs2_decompress(unsigned char comprtype, unsigned char *cdata_in, 
		     unsigned char *data_out, uint32_t cdatalen, uint32_t datalen);

/* scan.c */
int jffs2_scan_medium(struct jffs2_sb_info *c);
void jffs2_rotate_lists(struct jffs2_sb_info *c);

/* build.c */
int jffs2_do_mount_fs(struct jffs2_sb_info *c);

/* erase.c */
void jffs2_erase_block(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);
void jffs2_erase_pending_blocks(struct jffs2_sb_info *c);
void jffs2_mark_erased_blocks(struct jffs2_sb_info *c);
void jffs2_erase_pending_trigger(struct jffs2_sb_info *c);

#ifdef CONFIG_JFFS2_FS_NAND
/* wbuf.c */
int jffs2_flush_wbuf(struct jffs2_sb_info *c, int pad);
int jffs2_check_nand_cleanmarker(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);
int jffs2_write_nand_cleanmarker(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);
int jffs2_nand_read_failcnt(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);
#endif

/* compr_zlib.c */
int jffs2_zlib_init(void);
void jffs2_zlib_exit(void);

/* dynamic construction of fragtree */
#ifdef CONFIG_JFFS2_DYNFRAGTREE
int jffs2_construct_fragtree(struct jffs2_sb_info*, struct jffs2_inode_info*);
static inline int jffs2_construct_fragtree_nolock_if_missing(struct jffs2_sb_info* c,
							     struct jffs2_inode_info* f)
{
	if (S_ISREG(OFNI_EDONI_2SFFJ(f)->i_mode) && ! frag_first(&f->fragtree))
		return jffs2_construct_fragtree(c, f);
	else
		return 0;
}

static inline int jffs2_construct_fragtree_if_missing(struct jffs2_sb_info* c,
						      struct jffs2_inode_info* f)
{
	if (S_ISREG(OFNI_EDONI_2SFFJ(f)->i_mode) && ! frag_first(&f->fragtree)) {
		int ret;

		down(&f->sem);
		ret = jffs2_construct_fragtree(c, f);
		up(&f->sem);

		return ret;
	}
	else
		return 0;
}
#else
static inline int jffs2_construct_fragtree_nolock_if_missing(struct jffs2_sb_info* c,
							     struct jffs2_inode_info* f)
{
	return 0;
}

static inline int jffs2_construct_fragtree_if_missing(struct jffs2_sb_info* c,
						      struct jffs2_inode_info* f)
{
	return 0;
}
#endif

#endif /* __JFFS2_NODELIST_H__ */
