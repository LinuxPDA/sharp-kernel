/*
 *  linux/mm/swap.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 */

/*
 * This file contains the default values for the opereation of the
 * Linux VM subsystem. Fine-tuning documentation can be found in
 * linux/Documentation/sysctl/vm.txt.
 * Started 18.12.91
 * Swap aging added 23.2.95, Stephen Tweedie.
 * Buffermem limits added 12.3.98, Rik van Riel.
 */

#include <linux/mm.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/pagemap.h>
#include <linux/init.h>

#include <asm/dma.h>
#include <asm/uaccess.h> /* for copy_to/from_user */
#include <asm/pgtable.h>

/*
 * We identify three levels of free memory.  We never let free mem
 * fall below the freepages.min except for atomic allocations.  We
 * start background swapping if we fall below freepages.high free
 * pages, and we begin intensive swapping below freepages.low.
 *
 * Actual initialization is done in mm/page_alloc.c
 */
freepages_t freepages = {
	0,	/* freepages.min */
	0,	/* freepages.low */
	0	/* freepages.high */
};

/* How many pages do we try to swap or page in/out together? */
int page_cluster;

buffer_mem_t buffer_mem = {
	2,	/* minimum percent buffer */
	10,	/* borrow percent buffer */
	60	/* maximum percent buffer */
};

buffer_mem_t page_cache = {
	2,	/* minimum percent page cache */
	50,	/* borrow percent page cache */
	75	/* maximum */
};

pager_daemon_t pager_daemon = {
	512,	/* base number for calculating the number of tries */
	SWAP_CLUSTER_MAX,	/* minimum number of tries */
	8,	/* do swap I/O in clusters of this size */
};

/**
 * (de)activate_page - move pages from/to active and inactive lists
 * @page: the page we want to move
 * @nolock - are we already holding the pagemap_lru_lock?
 *
 * Deactivate_page will move an active page to the right
 * inactive list, while activate_page will move a page back
 * from one of the inactive lists to the active list. If
 * called on a page which is not on any of the lists, the
 * page is left alone.
 */
void deactivate_page_nolock(struct page * page)
{
	/*
	 * Don't touch it if it's not on the active list.
	 * (some pages aren't on any list at all)
	 */
	if (PageActive(page) && !page_ramdisk(page)) {
		page->age = 0;
		ClearPageReferenced(page);
		del_page_from_active_list(page);
		add_page_to_inactive_dirty_list(page);
	}
}	

void deactivate_page(struct page * page)
{
	spin_lock(&pagemap_lru_lock);
	deactivate_page_nolock(page);
	spin_unlock(&pagemap_lru_lock);
}

/*
 * Move an inactive page to the active list.
 */
void activate_page_nolock(struct page * page)
{
	if (PageInactiveDirty(page)) {
		del_page_from_inactive_dirty_list(page);
		add_page_to_active_list(page);
	} else if (PageInactiveClean(page)) {
		del_page_from_inactive_clean_list(page);
		add_page_to_active_list(page);
	} else {
		/*
		 * The page was not on any list, so we take care
		 * not to do anything.
		 */
	}

	/* Make sure the page gets a fair chance at staying active. */
	if (page->age < PAGE_AGE_START)
		page->age = PAGE_AGE_START;
}

void activate_page(struct page * page)
{
	spin_lock(&pagemap_lru_lock);
	activate_page_nolock(page);
	spin_unlock(&pagemap_lru_lock);
}

/**
 * lru_cache_add: add a page to the page lists
 * @page: the page to add
 */
void lru_cache_add(struct page * page)
{
	spin_lock(&pagemap_lru_lock);
	if (!PageLocked(page))
		BUG();
	DEBUG_ADD_PAGE
	add_page_to_active_list(page);
	/* This should be relatively rare */
	if (!page->age)
		deactivate_page_nolock(page);
	spin_unlock(&pagemap_lru_lock);
}

/**
 * __lru_cache_del: remove a page from the page lists
 * @page: the page to add
 *
 * This function is for when the caller already holds
 * the pagemap_lru_lock.
 */
void __lru_cache_del(struct page * page)
{
	if (PageActive(page)) {
		del_page_from_active_list(page);
	} else if (PageInactiveDirty(page)) {
		del_page_from_inactive_dirty_list(page);
	} else if (PageInactiveClean(page)) {
		del_page_from_inactive_clean_list(page);
	} else {
		printk("VM: __lru_cache_del, found unknown page ?!\n");
	}
	DEBUG_ADD_PAGE
}

/**
 * lru_cache_del: remove a page from the page lists
 * @page: the page to remove
 */
void lru_cache_del(struct page * page)
{
	if (!PageLocked(page))
		BUG();
	spin_lock(&pagemap_lru_lock);
	__lru_cache_del(page);
	spin_unlock(&pagemap_lru_lock);
}

/*
 * Perform any setup for the swap system
 */
void __init swap_setup(void)
{
	/* Use a smaller cluster for memory <16MB or <32MB */
	if (num_physpages < ((16 * 1024 * 1024) >> PAGE_SHIFT))
		page_cluster = 2;
	else if (num_physpages < ((32 * 1024 * 1024) >> PAGE_SHIFT))
		page_cluster = 3;
	else
		page_cluster = 4;
}
