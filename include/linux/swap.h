/*
 * ChangLog:
 *	12-Dec-2002 Lineo Japan, Inc.
 *	16-Apr-2003 Sharp for SL-Zaurus
 */

#ifndef _LINUX_SWAP_H
#define _LINUX_SWAP_H

#include <linux/spinlock.h>
#include <asm/page.h>

#define SWAP_FLAG_PREFER	0x8000	/* set if swap priority specified */
#define SWAP_FLAG_PRIO_MASK	0x7fff
#define SWAP_FLAG_PRIO_SHIFT	0

#define MAX_SWAPFILES 32

/*
 * Magic header for a swap area. The first part of the union is
 * what the swap magic looks like for the old (limited to 128MB)
 * swap area format, the second part of the union adds - in the
 * old reserved area - some extra information. Note that the first
 * kilobyte is reserved for boot loader or disk label stuff...
 *
 * Having the magic at the end of the PAGE_SIZE makes detecting swap
 * areas somewhat tricky on machines that support multiple page sizes.
 * For 2.5 we'll probably want to move the magic to just beyond the
 * bootbits...
 */
union swap_header {
	struct 
	{
		char reserved[PAGE_SIZE - 10];
		char magic[10];			/* SWAP-SPACE or SWAPSPACE2 */
	} magic;
	struct 
	{
		char	     bootbits[1024];	/* Space for disklabel etc. */
		unsigned int version;
		unsigned int last_page;
		unsigned int nr_badpages;
		unsigned int padding[125];
		unsigned int badpages[1];
	} info;
};

#ifdef __KERNEL__

#ifndef NO_MM
/*
 * Max bad pages in the new format..
 */
#define __swapoffset(x) ((unsigned long)&((union swap_header *)0)->x)
#define MAX_SWAP_BADPAGES \
	((__swapoffset(magic.magic) - __swapoffset(info.badpages)) / sizeof(int))
#endif /* NO_MM */

#include <asm/atomic.h>

#define SWP_USED	1
#define SWP_WRITEOK	3

#define SWAP_CLUSTER_MAX 32

#define SWAP_MAP_MAX	0x7fff
#define SWAP_MAP_BAD	0x8000

/*
 * The in-memory structure used to track swap areas.
 */
struct swap_info_struct {
	unsigned int flags;
	kdev_t swap_device;
	spinlock_t sdev_lock;
	struct dentry * swap_file;
	struct vfsmount *swap_vfsmnt;
	unsigned short * swap_map;
	unsigned int lowest_bit;
	unsigned int highest_bit;
	unsigned int cluster_next;
	unsigned int cluster_nr;
	int prio;			/* swap priority */
	int pages;
	unsigned long max;
	int next;			/* next entry on swap list */
};

extern int nr_swap_pages;

/* Swap 50% full? Release swapcache more aggressively.. */
#define vm_swap_full() (nr_swap_pages*2 < total_swap_pages)

extern unsigned int nr_free_pages(void);
extern unsigned int nr_free_buffer_pages(void);
extern int nr_active_pages;
extern int nr_inactive_pages;
extern atomic_t page_cache_size;
extern atomic_t buffermem_pages;

extern spinlock_cacheline_t pagecache_lock_cacheline;
#define pagecache_lock (pagecache_lock_cacheline.lock)

extern void __remove_inode_page(struct page *);

/* Incomplete types for prototype declarations: */
struct task_struct;
struct vm_area_struct;
struct sysinfo;

struct zone_t;

/* linux/mm/swap.c */
extern void FASTCALL(lru_cache_add(struct page *));
extern void FASTCALL(__lru_cache_del(struct page *));
extern void FASTCALL(lru_cache_del(struct page *));

extern void FASTCALL(activate_page(struct page *));

extern void swap_setup(void);

/* linux/mm/vmscan.c */
extern wait_queue_head_t kswapd_wait;
extern int FASTCALL(try_to_free_pages_zone(zone_t *, unsigned int));
extern int FASTCALL(try_to_free_pages(unsigned int));

/* linux/mm/page_io.c */
extern void rw_swap_page(int, struct page *);
extern void rw_swap_page_nolock(int, swp_entry_t, char *);

/* linux/mm/page_alloc.c */

/* linux/mm/swap_state.c */
#define SWAP_CACHE_INFO
#ifdef SWAP_CACHE_INFO
extern void show_swap_cache_info(void);
#endif
extern int add_to_swap_cache(struct page *, swp_entry_t);
extern void __delete_from_swap_cache(struct page *page);
extern void delete_from_swap_cache(struct page *page);
extern void free_page_and_swap_cache(struct page *page);
extern struct page * lookup_swap_cache(swp_entry_t);
extern struct page * read_swap_cache_async(swp_entry_t);

/* linux/mm/oom_kill.c */
extern void out_of_memory(void);
#ifdef CONFIG_FREEPG_SIGNAL
extern void reset_out_of_memory_condition(void);
#endif

/* linux/mm/swapfile.c */
extern int total_swap_pages;
extern unsigned int nr_swapfiles;
extern struct swap_info_struct swap_info[];
extern int is_swap_partition(kdev_t);
extern void si_swapinfo(struct sysinfo *);
extern swp_entry_t get_swap_page(void);
extern void get_swaphandle_info(swp_entry_t, unsigned long *, kdev_t *, 
					struct inode **);
extern int swap_duplicate(swp_entry_t);
extern int swap_count(struct page *);
extern int valid_swaphandles(swp_entry_t, unsigned long *);
extern void swap_free(swp_entry_t);
extern void free_swap_and_cache(swp_entry_t);
struct swap_list_t {
	int head;	/* head of priority-ordered swapfile list */
	int next;	/* swapfile to be used next */
};
extern struct swap_list_t swap_list;
asmlinkage long sys_swapoff(const char *);
asmlinkage long sys_swapon(const char *, int);

extern spinlock_cacheline_t pagemap_lru_lock_cacheline;
#define pagemap_lru_lock pagemap_lru_lock_cacheline.lock

extern void FASTCALL(mark_page_accessed(struct page *));

/*
 * List add/del helper macros. These must be called
 * with the pagemap_lru_lock held!
 */
#define DEBUG_LRU_PAGE(page)			\
do {						\
	if (!PageLRU(page))			\
		BUG();				\
	if (PageActive(page))			\
		BUG();				\
} while (0)

#define add_page_to_active_list(page)		\
do {						\
	DEBUG_LRU_PAGE(page);			\
	SetPageActive(page);			\
	list_add(&(page)->lru, &active_list);	\
	nr_active_pages++;			\
} while (0)

#ifdef CONFIG_ARCH_SHARP_SL
#define add_page_to_inactive_list(page)		\
do {						\
	DEBUG_LRU_PAGE(page);			\
	if (!vm_without_swap || page->mapping) {	\
		list_add(&(page)->lru, &inactive_list);	\
		nr_inactive_pages++;			\
	}	\
	else {	\
		clear_bit(PG_lru, &(page)->flags);	\
	}	\
} while (0)
#else
#define add_page_to_inactive_list(page)		\
do {						\
	DEBUG_LRU_PAGE(page);			\
	list_add(&(page)->lru, &inactive_list);	\
	nr_inactive_pages++;			\
} while (0)
#endif

#define del_page_from_active_list(page)		\
do {						\
	list_del(&(page)->lru);			\
	ClearPageActive(page);			\
	nr_active_pages--;			\
} while (0)

#define del_page_from_inactive_list(page)	\
do {						\
	list_del(&(page)->lru);			\
	nr_inactive_pages--;			\
} while (0)

#ifndef NO_MM

extern spinlock_t swaplock;

#define swap_list_lock()	spin_lock(&swaplock)
#define swap_list_unlock()	spin_unlock(&swaplock)
#define swap_device_lock(p)	spin_lock(&p->sdev_lock)
#define swap_device_unlock(p)	spin_unlock(&p->sdev_lock)

#endif /* NO_MM */

extern void shmem_unuse(swp_entry_t entry, struct page *page);

#endif /* __KERNEL__*/

#endif /* _LINUX_SWAP_H */
