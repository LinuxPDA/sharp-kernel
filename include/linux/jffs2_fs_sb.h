/* $Id: jffs2_fs_sb.h,v 1.34 2002/09/09 16:29:07 dwmw2 Exp $ */

/*
 * ChangeLog:
 *     27-Nov-2002 Lineo Japan, Inc.  add effective-gc mode
 *     24-Nov-2002 SHARP  add erasing_dirty_size
 *     15-Nov-2002 Lineo Japan, Inc.  add nodemerge facility
 *     05-Nov-2002 Lineo Japan, Inc.  modify nr_bad_blocks type
 *     29-Oct-2002 Lineo Japan, Inc.  add member nr_bad_blocks and cont_gc_count
 *
 */

#ifndef _JFFS2_FS_SB
#define _JFFS2_FS_SB

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/tqueue.h>
#include <linux/completion.h>
#include <asm/semaphore.h>
#include <linux/list.h>

#define JFFS2_SB_FLAG_RO 1
#define JFFS2_SB_FLAG_MOUNTING 2
#ifdef CONFIG_JFFS2_NODEMERGE
#define JFFS2_SB_FLAG_GCING_A_BLOCK 4
#else
#define JFFS2_SB_FLAG_GCING_A_BLOCK 0
#endif

/* A struct for the overall file system control.  Pointers to
   jffs2_sb_info structs are named `c' in the source code.  
   Nee jffs_control
*/
struct jffs2_sb_info {
	struct mtd_info *mtd;

	uint32_t highest_ino;
	uint32_t checked_ino;

	unsigned int flags;

	struct task_struct *gc_task;	/* GC task struct */
	struct semaphore gc_thread_start; /* GC thread start mutex */
	struct completion gc_thread_exit; /* GC thread exit completion port */

	struct semaphore alloc_sem;	/* Used to protect all the following 
					   fields, and also to protect against
					   out-of-order writing of nodes.
					   And GC.
					*/
	uint32_t cleanmarker_size;	/* Size of an _inline_ CLEANMARKER
					 (i.e. zero for OOB CLEANMARKER */

	uint32_t flash_size;
	uint32_t used_size;
	uint32_t dirty_size;
	uint32_t wasted_size;
	uint32_t free_size;
	uint32_t erasing_size;
	uint32_t bad_size;
	uint32_t sector_size;
	uint32_t unchecked_size;
	uint32_t erasing_dirty_size;

	uint32_t nr_free_blocks;
	uint32_t nr_erasing_blocks;
	int32_t nr_bad_blocks;

	uint32_t cont_gc_count;

	uint32_t nr_blocks;
	struct jffs2_eraseblock *blocks;	/* The whole array of blocks. Used for getting blocks 
						 * from the offset (blocks[ofs / sector_size]) */
	struct jffs2_eraseblock *nextblock;	/* The block we're currently filling */

	struct jffs2_eraseblock *gcblock;	/* The block we're currently garbage-collecting */

	struct list_head clean_list;		/* Blocks 100% full of clean data */
	struct list_head very_dirty_list;	/* Blocks with lots of dirty space */
	struct list_head dirty_list;		/* Blocks with some dirty space */
	struct list_head erasable_list;		/* Blocks which are completely dirty, and need erasing */
	struct list_head erasable_pending_wbuf_list;	/* Blocks which need erasing but only after the current wbuf is flushed */
	struct list_head erasing_list;		/* Blocks which are currently erasing */
	struct list_head erase_pending_list;	/* Blocks which need erasing now */
	struct list_head erase_complete_list;	/* Blocks which are erased and need the clean marker written to them */
	struct list_head free_list;		/* Blocks which are free and ready to be used */
	struct list_head bad_list;		/* Bad blocks. */
	struct list_head bad_used_list;		/* Bad blocks with valid data in. */
#ifdef CONFIG_JFFS2_NODEMERGE
	struct list_head nodemerge_list;	/* Blocks with node-merge */
#endif

	spinlock_t erase_completion_lock;	/* Protect free_list and erasing_list 
						   against erase completion handler */
	wait_queue_head_t erase_wait;		/* For waiting for erases to complete */

	struct jffs2_inode_cache **inocache_list;
	spinlock_t inocache_lock;
	
	/* Sem to allow jffs2_garbage_collect_deletion_dirent to
	   drop the erase_completion_lock while it's holding a pointer 
	   to an obsoleted node. I don't like this. Alternatives welcomed. */
	struct semaphore erase_free_sem;

	/* Write-behind buffer for NAND flash */
	unsigned char *wbuf;
	uint32_t wbuf_ofs;
	uint32_t wbuf_len;
	uint32_t wbuf_pagesize;
	struct tq_struct wbuf_task;		/* task for timed wbuf flush */
	struct timer_list wbuf_timer;		/* timer for flushing wbuf */

	uint32_t effective_gc_count;

	/* OS-private pointer for getting back to master superblock info */
	void *os_priv;
};

#endif /* _JFFS2_FB_SB */
