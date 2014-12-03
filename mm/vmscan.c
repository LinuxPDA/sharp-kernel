/*
 *  linux/mm/vmscan.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Swap reorganised 29.12.95, Stephen Tweedie.
 *  kswapd added: 7.1.96  sct
 *  Removed kswapd_ctl limits, and swap out as many pages as needed
 *  to bring the system back to freepages.high: 2.4.97, Rik van Riel.
 *  Zone aware kswapd started 02/00, Kanoj Sarcar (kanoj@sgi.com).
 *  Multiqueue VM started 5.8.00, Rik van Riel.
 *
 * Change Log
 *	12-Nov-2001 Lineo Japan, Inc.
 */

#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/smp_lock.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/file.h>

#include <asm/pgalloc.h>

extern void shrink_pgtcache_memory(int gfp_mask);

int vm_static_inactive_target;

static inline void age_page_up(struct page *page)
{
	page->age = min((int) (page->age + PAGE_AGE_ADV), PAGE_AGE_MAX); 
}

static inline void age_page_down(struct page *page)
{
	page->age -= min(PAGE_AGE_DECL, (int)page->age);
}

/*
 * Estimate whether a zone has enough inactive or free pages..
 */
static unsigned int zone_inactive_plenty(zone_t *zone)
{
	unsigned int inactive;

	if (!zone->size)
		return 0;
		
	inactive = zone->inactive_dirty_pages;
	inactive += zone->inactive_clean_pages;
	inactive += zone->free_pages;

	return (inactive > (zone->size * 2 / 5));
}

#define FREE_PLENTY_FACTOR 2
static unsigned int zone_free_plenty(zone_t *zone)
{
	unsigned int free;

	free = zone->free_pages;
	free += zone->inactive_clean_pages;

	return free > zone->pages_high * FREE_PLENTY_FACTOR;
}

static unsigned int free_plenty(void)
{
	unsigned int free;

	free = nr_free_pages();
	free += nr_inactive_clean_pages();

	return free > freepages.high * FREE_PLENTY_FACTOR;
}

/*
 * We only do page aging if the object in question is in use or
 * if the cache is getting small. The "small cache" thing happens
 * when the working set of processes is getting very large and we
 * need to be careful which pages we evict...
 */
static inline int cache_is_small(void)
{
	int bufferpages = atomic_read(&buffermem_pages);
	int pagecache = atomic_read(&page_cache_size) - swapper_space.nrpages;

	int limit = num_physpages * page_cache.borrow_percent / 100;

	return bufferpages + pagecache < limit;
}

static inline int page_mapping_notused(struct page * page)
{
	struct address_space * mapping = page->mapping;

	/* 
	 * If a swap cache page is in the RSS of a process, we age it.
	 * Otherwise, we don't.
	 */
	if (PageSwapCache(page)) {
	       	if (page_count(page) > (1 + !!page->buffers))
			return 0;

		return 1;
	}

	/* If the cache is small, always use page aging. */
	if (cache_is_small())
		return 0;

	if (!mapping)
		return 1;

	/* This mapping is really large and would monopolise the pagecache. */
	if (mapping->nrpages > atomic_read(&page_cache_size) / 20);
		return 1;

	/* File is mmaped by somebody. */
	if (mapping->i_mmap || mapping->i_mmap_shared)
		return 0;

	return 1;
}

/*
 * The swap-out function returns 1 if it successfully
 * scanned all the pages it was asked to (`count').
 * It returns zero if it couldn't do anything,
 *
 * rss may decrease because pages are shared, but this
 * doesn't count as having freed a page.
 */

/* mm->page_table_lock is held. mmap_sem is not held */
static void try_to_swap_out(struct mm_struct * mm, struct vm_area_struct* vma, unsigned long address, pte_t * page_table, struct page *page)
{
	pte_t pte;
	swp_entry_t entry;

	/* Don't look at this pte if it's been accessed recently. */
	if (ptep_test_and_clear_young(page_table)) {
		age_page_up(page);
		return;
	}

	/*
	 * We age down really anonymous pages here, pages which
	 * already have a mapping are aged down on the active
	 * list instead.
	 * This is done so heavily shared pages (think libc.so)
	 * are only aged down once and won't be swapped out when
	 * still in active use.
	 */
	if (!page->mapping)
		age_page_down(page);

	/* 
	 * If we have plenty inactive pages on this 
	 * zone, skip it.
	 */
	if (zone_inactive_plenty(page->zone))
		return;

	/*
	 * Don't swap out a page which is still in use.
	 */
	if (page->age > 0)
		return;

	if (TryLockPage(page))
		return;

	/* From this point on, the odds are that we're going to
	 * nuke this pte, so read and clear the pte.  This hook
	 * is needed on CPUs which update the accessed and dirty
	 * bits in hardware.
	 */
	flush_cache_page(vma, address);
	pte = ptep_get_and_clear(page_table);
	flush_tlb_page(vma, address);

	/*
	 * Is the page already in the swap cache? If so, then
	 * we can just drop our reference to it without doing
	 * any IO - it's already up-to-date on disk.
	 */
	if (PageSwapCache(page)) {
		entry.val = page->index;
		if (pte_dirty(pte))
			set_page_dirty(page);
		swap_duplicate(entry);
set_swap_pte:
		set_pte(page_table, swp_entry_to_pte(entry));
drop_pte:
		mm->rss--;
		if (!page->age)
			deactivate_page(page);
		UnlockPage(page);
		memc_clear(vma->vm_mm, page);
		page_cache_release(page);
		return;
	}

	/*
	 * Is it a clean page? Then it must be recoverable
	 * by just paging it in again, and we can just drop
	 * it..  or if it's dirty but has backing store,
	 * just mark the page dirty and drop it.
	 *
	 * However, this won't actually free any real
	 * memory, as the page will just be in the page cache
	 * somewhere, and as such we should just continue
	 * our scan.
	 *
	 * Basically, this just makes it possible for us to do
	 * some real work in the future in "refill_inactive()".
	 */
	if (page->mapping) {
		if (pte_dirty(pte))
			set_page_dirty(page);
		goto drop_pte;
	}
	/*
	 * Check PageDirty as well as pte_dirty: page may
	 * have been brought back from swap by swapoff.
	 */
	if (!pte_dirty(pte) && !PageDirty(page))
		goto drop_pte;

	/*
	 * This is a dirty, swappable page.  First of all,
	 * get a suitable swap entry for it, and make sure
	 * we have the swap cache set up to associate the
	 * page with that swap entry.
	 */
	swap_list_lock();
	entry = get_swap_page();
	if (entry.val) {
		/* Add it to the swap cache and mark it dirty */
		add_to_swap_cache(page, entry);
		swap_list_unlock();
		set_page_dirty(page);
		goto set_swap_pte;
	}

	/* No swap space left */
	swap_list_unlock();
	set_pte(page_table, pte);
	UnlockPage(page);
	return;
}

/* mm->page_table_lock is held. mmap_sem is not held */
static int swap_out_pmd(struct mm_struct * mm, struct vm_area_struct * vma, pmd_t *dir, unsigned long address, unsigned long end, int count)
{
	pte_t * pte;
	unsigned long pmd_end;

	if (pmd_none(*dir))
		return count;
	if (pmd_bad(*dir)) {
		pmd_ERROR(*dir);
		pmd_clear(dir);
		return count;
	}
	
	pte = pte_offset(dir, address);
	
	pmd_end = (address + PMD_SIZE) & PMD_MASK;
	if (end > pmd_end)
		end = pmd_end;

	do {
		if (pte_present(*pte)) {
			struct page *page = pte_page(*pte);

			if (VALID_PAGE(page) && !PageReserved(page)) {
				try_to_swap_out(mm, vma, address, pte, page);
				if (!--count)
					break;
			}
		}
		address += PAGE_SIZE;
		pte++;
	} while (address && (address < end));
	mm->swap_address = address + PAGE_SIZE;
	return count;
}

/* mm->page_table_lock is held. mmap_sem is not held */
static inline int swap_out_pgd( struct mm_struct * mm, struct vm_area_struct * vma, pgd_t *dir, unsigned long address, unsigned long end, int count)
{
	pmd_t * pmd;
	unsigned long pgd_end;

	if (pgd_none(*dir))
		return count;
	if (pgd_bad(*dir)) {
		pgd_ERROR(*dir);
		pgd_clear(dir);
		return count;
	}

	pmd = pmd_offset(dir, address);

	pgd_end = (address + PGDIR_SIZE) & PGDIR_MASK;	
	if (pgd_end && (end > pgd_end))
		end = pgd_end;
	
	do {
		count = swap_out_pmd(mm, vma, pmd, address, end, count);
		if (!count)
			break;
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
	return count;
}

/* mm->page_table_lock is held. mmap_sem is not held */
static int swap_out_vma(struct mm_struct * mm, struct vm_area_struct * vma, unsigned long address, int count)
{
	pgd_t *pgdir;
	unsigned long end;

	/* Don't swap out areas which are locked down */
	if (vma->vm_flags & (VM_LOCKED|VM_RESERVED))
		return count;

	pgdir = pgd_offset(mm, address);

	end = vma->vm_end;
	if (address >= end)
		BUG();
	do {
		count = swap_out_pgd(mm, vma, pgdir, address, end, count);
		if (!count)
			break;
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		pgdir++;
	} while (address && (address < end));
	return count;
}

/*
 * Returns non-zero if we scanned all `count' pages
 */
static int swap_out_mm(struct mm_struct * mm, int count)
{
	unsigned long address;
	struct vm_area_struct* vma;

	if (!count)
		return 1;
	/*
	 * Go through process' page directory.
	 */

	/*
	 * Find the proper vm-area after freezing the vma chain 
	 * and ptes.
	 */
	spin_lock(&mm->page_table_lock);
	address = mm->swap_address;
	vma = find_vma(mm, address);
	if (vma) {
		if (address < vma->vm_start)
			address = vma->vm_start;

		for (;;) {
			count = swap_out_vma(mm, vma, address, count);
			if (!count)
				goto out_unlock;
			vma = vma->vm_next;
			if (!vma)
				break;
			address = vma->vm_start;
		}
	}
	/* Reset to 0 when we reach the end of address space */
	mm->swap_address = 0;

out_unlock:
	spin_unlock(&mm->page_table_lock);
	return !count;
}

#define SWAP_SHIFT	5
#define SWAP_MIN	8

static inline int swap_amount(struct mm_struct *mm)
{
	int nr = mm->rss >> SWAP_SHIFT;
	if (nr < SWAP_MIN) {
		nr = SWAP_MIN;
		if (nr > mm->rss)
			nr = mm->rss;
	}
	return nr;
}

/* Placeholder for swap_out(): may be updated by fork.c:mmput() */
struct mm_struct *swap_mm = &init_mm;

static void swap_out(unsigned int priority, int gfp_mask)
{
	int counter;
	int retval = 0;
	struct mm_struct *mm = current->mm;

	/* Scan part of the process virtual memory. */
	counter = (mmlist_nr << SWAP_SHIFT) >> priority;
	do {
		spin_lock(&mmlist_lock);
		mm = swap_mm;
		if (mm == &init_mm) {
			mm = list_entry(mm->mmlist.next, struct mm_struct, mmlist);
			if (mm == &init_mm)
				goto empty;
		}
		/* Set pointer for next call to next in the list */
		swap_mm = list_entry(mm->mmlist.next, struct mm_struct, mmlist);

		/* Make sure the mm doesn't disappear when we drop the lock.. */
		atomic_inc(&mm->mm_users);
		spin_unlock(&mmlist_lock);

		/* Walk about 6% of the address space each time */
		retval |= swap_out_mm(mm, swap_amount(mm));
		mmput(mm);
	} while (--counter >= 0);
	return;

empty:
	spin_unlock(&mmlist_lock);
}


/**
 * reclaim_page -	reclaims one page from the inactive_clean list
 * @zone: reclaim a page from this zone
 *
 * The pages on the inactive_clean can be instantly reclaimed.
 * The tests look impressive, but most of the time we'll grab
 * the first page of the list and exit successfully.
 */
struct page * reclaim_page(zone_t * zone)
{
	struct page * page = NULL;
	struct list_head * page_lru;
	swp_entry_t entry = {0};
	int maxscan;

	/*
	 * We only need the pagemap_lru_lock if we don't reclaim the page,
	 * but we have to grab the pagecache_lock before the pagemap_lru_lock
	 * to avoid deadlocks and most of the time we'll succeed anyway.
	 */
	spin_lock(&pagecache_lock);
	spin_lock(&pagemap_lru_lock);
	maxscan = zone->inactive_clean_pages;
	while ((page_lru = zone->inactive_clean_list.prev) !=
			&zone->inactive_clean_list && maxscan--) {
		page = list_entry(page_lru, struct page, lru);

		/* Wrong page on list?! (list corruption, should not happen) */
		if (!PageInactiveClean(page)) {
			printk("VM: reclaim_page, wrong page on list.\n");
			list_del(page_lru);
			page->zone->inactive_clean_pages--;
			continue;
		}

		/* Page is or was in use?  Move it to the active list. */
		if (PageReferenced(page) || page->age > 0 ||
				page_count(page) > (1 + !!page->buffers)) {
			del_page_from_inactive_clean_list(page);
			add_page_to_active_list(page);
			page->age = max((int)page->age, PAGE_AGE_START);
			continue;
		}

		/* The page is dirty, or locked, move to inactive_dirty list. */
		if (page->buffers || PageDirty(page) || TryLockPage(page)) {
			del_page_from_inactive_clean_list(page);
			add_page_to_inactive_dirty_list(page);
			continue;
		}

		/* OK, remove the page from the caches. */
                if (PageSwapCache(page)) {
			entry.val = page->index;
			__delete_from_swap_cache(page);
			goto found_page;
		}

		if (page->mapping) {
			__remove_inode_page(page);
			goto found_page;
		}

		/* We should never ever get here. */
		printk(KERN_ERR "VM: reclaim_page, found unknown page\n");
		list_del(page_lru);
		zone->inactive_clean_pages--;
		UnlockPage(page);
	}
	spin_unlock(&pagemap_lru_lock);
	spin_unlock(&pagecache_lock);
	return NULL;

found_page:
	del_page_from_inactive_clean_list(page);
	spin_unlock(&pagemap_lru_lock);
	spin_unlock(&pagecache_lock);
	if (entry.val)
		swap_free(entry);
	UnlockPage(page);
	page->age = PAGE_AGE_START;
	if (page_count(page) != 1)
		printk("VM: reclaim_page, found page with count %d!\n",
				page_count(page));
	return page;
}

static inline int page_dirty(struct page *page)
{
	struct buffer_head *tmp, *bh;

	if (PageDirty(page))
		return 1;

	if (page->mapping && !page->buffers)
		return 0;

	tmp = bh = page->buffers;

	do {
		if (tmp->b_state & ((1<<BH_Dirty) | (1<<BH_Lock)))
			return 1;
		tmp = tmp->b_this_page;
	} while (tmp != bh);

	return 0;
}

/**
 * page_launder - clean dirty inactive pages, move to inactive_clean list
 * @gfp_mask: what operations we are allowed to do
 * @sync: are we allowed to do synchronous IO in emergencies ?
 *
 * This function is called when we are low on free / inactive_clean
 * pages, its purpose is to refill the free/clean list as efficiently
 * as possible.
 *
 * This means we do writes asynchronously as long as possible and will
 * only sleep on IO when we don't have another option. Since writeouts
 * cause disk seeks and make read IO slower, we skip writes alltogether
 * when the amount of dirty pages is small.
 *
 * This code is heavily inspired by the FreeBSD source code. Thanks
 * go out to Matthew Dillon.
 */
#define	CAN_DO_FS	((gfp_mask & __GFP_FS) && should_write)
#define	WRITE_LOW_WATER		5
#define	WRITE_HIGH_WATER	10
int page_launder(int gfp_mask, int sync)
{
	int maxscan, cleaned_pages, failed_pages, total;
	struct list_head * page_lru;
	struct page * page;

	/*
	 * This flag determines if we should do writeouts of dirty data
	 * or not.  When more than WRITE_HIGH_WATER percentage of the
	 * pages we process would need to be written out we set this flag
	 * and will do writeout, the flag is cleared once we go below
	 * WRITE_LOW_WATER.  Note that only pages we actually process
	 * get counted, ie. pages where we make it beyond the TryLock.
	 *
	 * XXX: These flags still need tuning.
	 */
	static int should_write = 0;

	cleaned_pages = 0;
	failed_pages = 0;
	
	/*
	 * The gfp_mask tells try_to_free_buffers() below if it should
	 * wait do IO or may be allowed to wait on IO synchronously.
	 *
	 * Note that syncronous IO only happens when a page has not been
	 * written out yet when we see it for a second time, this is done
	 * through magic in try_to_free_buffers().
	 */
	if (!should_write)
		gfp_mask &= ~(__GFP_WAIT | __GFP_IO);
	else if (!sync)
		gfp_mask &= ~__GFP_WAIT;

	/* The main launder loop. */
	spin_lock(&pagemap_lru_lock);
	maxscan = nr_inactive_dirty_pages;
	while ((page_lru = inactive_dirty_list.prev) != &inactive_dirty_list &&
				maxscan-- > 0) {
		page = list_entry(page_lru, struct page, lru);

		/* Wrong page on list?! (list corruption, should not happen) */
		if (!PageInactiveDirty(page)) {
			printk("VM: page_launder, wrong page on list.\n");
			list_del(page_lru);
			nr_inactive_dirty_pages--;
			page->zone->inactive_dirty_pages--;
			continue;
		}

		/*
		 * The page is in active use or really unfreeable. Move to
		 * the active list and adjust the page age if needed.
		 */
		if (PageReferenced(page) || page->age || page_ramdisk(page)) {
			del_page_from_inactive_dirty_list(page);
			add_page_to_active_list(page);
			page->age = max((int)page->age, PAGE_AGE_START);
			continue;
		}

		/*
		 * The page is still in the page tables of some process,
		 * move it to the active list but leave page age at 0;
		 * either swap_out() will make it freeable soon or it is
		 * mlock()ed...
		 *
		 * The !PageLocked() test is to protect us from ourselves,
		 * see the code around the writepage() call.
		 */
		if ((page_count(page) > (1 + !!page->buffers)) &&
						!PageLocked(page)) {
			del_page_from_inactive_dirty_list(page);
			add_page_to_active_list(page);
			continue;
		}

		/*
		 * If this zone has plenty of pages free, don't spend time
		 * on cleaning it but only move clean pages out of the way
		 * so we won't have to scan those again.
		 */
		if (zone_free_plenty(page->zone)) {
			if (!page->mapping || page_dirty(page)) {
				list_del(page_lru);
				list_add(page_lru, &inactive_dirty_list);
				continue;
			}
		}

		/*
		 * The page is locked. IO in progress?
		 * Move it to the back of the list.
		 */
		if (TryLockPage(page)) {
			list_del(page_lru);
			list_add(page_lru, &inactive_dirty_list);
			continue;
		}

		/*
		 * Dirty swap-cache page? Write it out if
		 * last copy..
		 */
		if (PageDirty(page)) {
			int (*writepage)(struct page *);

			/* Can a page get here without page->mapping? */
			if (!page->mapping)
				goto page_active;
			writepage = page->mapping->a_ops->writepage;
			if (!writepage)
				goto page_active;

			/* Can't do it? Move it to the back of the list. */
			if (!CAN_DO_FS) {
				list_del(page_lru);
				list_add(page_lru, &inactive_dirty_list);
				UnlockPage(page);
				failed_pages++;
				continue;
			}

			/* OK, do a physical asynchronous write to swap.  */
			ClearPageDirty(page);
			page_cache_get(page);
			spin_unlock(&pagemap_lru_lock);

			writepage(page);
			page_cache_release(page);

			/* And re-start the thing.. */
			spin_lock(&pagemap_lru_lock);
			continue;
		}

		/*
		 * If the page has buffers, try to free the buffer mappings
		 * associated with this page. If we succeed we either free
		 * the page (in case it was a buffercache only page) or we
		 * move the page to the inactive_clean list.
		 *
		 * On the first round, we should free all previously cleaned
		 * buffer pages
		 */
		if (page->buffers) {
			int clearedbuf;
			/*
			 * Since we might be doing disk IO, we have to
			 * drop the spinlock and take an extra reference
			 * on the page so it doesn't go away from under us.
			 */
			del_page_from_inactive_dirty_list(page);
			page_cache_get(page);
			spin_unlock(&pagemap_lru_lock);

			/* Try to free the page buffers. */
			clearedbuf = try_to_release_page(page, gfp_mask);

			/*
			 * Re-take the spinlock. Note that we cannot
			 * unlock the page yet since we're still
			 * accessing the page_struct here...
			 */
			spin_lock(&pagemap_lru_lock);

			/* The buffers were not freed. */
			if (!clearedbuf) {
				add_page_to_inactive_dirty_list(page);
				failed_pages++;

			/* The page was only in the buffer cache. */
			} else if (!page->mapping) {
				atomic_dec(&buffermem_pages);
				cleaned_pages++;

			/* The page has more users besides the cache and us. */
			} else if (page_count(page) > 2) {
				add_page_to_active_list(page);

			/* OK, we "created" a freeable page. */
			} else /* page->mapping && page_count(page) == 2 */ {
				add_page_to_inactive_clean_list(page);
				cleaned_pages++;
			}

			/*
			 * Unlock the page and drop the extra reference.
			 * We can only do it here because we are accessing
			 * the page struct above.
			 */
			UnlockPage(page);
			page_cache_release(page);

			continue;
		} else if (page->mapping && !PageDirty(page)) {
			/*
			 * If a page had an extra reference in
			 * deactivate_page(), we will find it here.
			 * Now the page is really freeable, so we
			 * move it to the inactive_clean list.
			 */
			del_page_from_inactive_dirty_list(page);
			add_page_to_inactive_clean_list(page);
			UnlockPage(page);
			cleaned_pages++;
		} else {
page_active:
			/*
			 * OK, we don't know what to do with the page.
			 * It's no use keeping it here, so we move it to
			 * the active list.
			 */
			del_page_from_inactive_dirty_list(page);
			add_page_to_active_list(page);
			page->age = max((int)page->age, PAGE_AGE_START);
			UnlockPage(page);
		}
	}
	spin_unlock(&pagemap_lru_lock);

	/*
	 * Set the should_write flag, for the next callers of page_launder.
	 * If we go below the low watermark we stop the writeout of dirty
	 * pages, writeout is started when we get above the high watermark.
	 */
	total = failed_pages + cleaned_pages;
	if (should_write && failed_pages * 100 < WRITE_LOW_WATER * total)
		should_write = 0;
	else if (!should_write && failed_pages * 100 > WRITE_HIGH_WATER * total)
		should_write = 1;

	/* Return the number of pages moved to the inactive_clean list. */
	return cleaned_pages;
}

/**
 * refill_inactive_scan - scan the active list and find pages to deactivate
 * @priority: the priority at which to scan
 * @target: number of pages to deactivate, zero for background aging
 *
 * This function will scan a portion of the active list to find
 * unused pages, those pages will then be moved to the inactive list.
 */
int refill_inactive_scan(unsigned int priority)
{
	struct list_head * page_lru;
	struct page * page;
	int maxscan = nr_active_pages >> priority;
	int nr_deactivated = 0;

	/* Take the lock while messing with the list... */
	spin_lock(&pagemap_lru_lock);
	while (maxscan-- > 0 && (page_lru = active_list.prev) != &active_list) {
		page = list_entry(page_lru, struct page, lru);

		/* Wrong page on list?! (list corruption, should not happen) */
		if (!PageActive(page)) {
			printk("VM: refill_inactive, wrong page on list.\n");
			list_del(page_lru);
			nr_active_pages--;
			continue;
		}

		/*
		 * Do aging on the pages.  Every time a page is referenced,
		 * page->age gets incremented.  If it wasn't referenced, we
		 * decrement page->age.  The page gets moved to the inactive
		 * list when one of the following is true:
		 * - the page age reaches 0
		 * - the object the page belongs to isn't in active use
		 * - the object the page belongs to is hogging the cache
		 */
		if (PageTestandClearReferenced(page)) {
			age_page_up(page);
		} else {
			age_page_down(page);
		}

		/*
		 * Don't deactivate pages from zones which have
		 * plenty inactive pages.
		 */
		if (zone_inactive_plenty(page->zone)) {
			goto skip_page;
		}

		/* Deactivate a page once page->age reaches 0. */
		if (!page->age)
			deactivate_page_nolock(page);

		/*
		 * Deactivate pages from files which aren't in use, busy
		 * pages will be referenced while on the inactive list.
		 */
		if (page_mapping_notused(page))
			deactivate_page_nolock(page);

		/*
		 * If the page is still on the active list, move it
		 * to the other end of the list. Otherwise we exit if
		 * we have done enough work.
		 */
skip_page:
		if (PageActive(page)) {
			list_del(page_lru);
			list_add(page_lru, &active_list);
		} else {
			nr_deactivated++;
		}
	}
	spin_unlock(&pagemap_lru_lock);

	return nr_deactivated;
}

long count_ramdisk_pages(void)
{
	struct list_head *page_lru;
	struct page *page;
	long nr_ramdisk = 0;

	spin_lock(&pagemap_lru_lock);
	for (page_lru = active_list.next; page_lru != &active_list;
	     page_lru = page_lru->next) {
		page = list_entry(page_lru, struct page, lru);
		if (page_ramdisk(page))
			nr_ramdisk ++;
	}
	spin_unlock(&pagemap_lru_lock);

	return nr_ramdisk;
}

/*
 * Check if there are zones with a severe shortage of free pages,
 * or if all zones have a minor shortage.
 */
int free_shortage(void)
{
	pg_data_t *pgdat;
	unsigned int global_free = 0;
	unsigned int global_target = freepages.high;

	/* Are we low on free pages anywhere? */
	pgdat = pgdat_list;
	do {
		int i;
		for(i = 0; i < MAX_NR_ZONES; i++) {
			zone_t *zone = pgdat->node_zones+ i;
			unsigned int free;

			if (!zone->size)
				continue;

			free = zone->free_pages;
			free += zone->inactive_clean_pages;

			/* Local shortage? */
			if (free < zone->pages_low)
				return 1;

			global_free += free;
		}
		pgdat = pgdat->node_next;
	} while (pgdat);

	/* Global shortage? */
	return global_free < global_target;
}

/*
 * Are we low on inactive pages globally or in any zone?
 */
int inactive_shortage(void)
{
	pg_data_t *pgdat;
	unsigned int global_target = freepages.high + inactive_target();
	unsigned int global_inactive = 0;

	pgdat = pgdat_list;
	do {
		int i;
		for(i = 0; i < MAX_NR_ZONES; i++) {
			zone_t *zone = pgdat->node_zones + i;
			unsigned int inactive;

			if (!zone->size)
				continue;

			inactive  = zone->inactive_dirty_pages;
			inactive += zone->inactive_clean_pages;
			inactive += zone->free_pages;

			/* Local shortage? */
			if (inactive < zone->pages_high)
				return 1;

			global_inactive += inactive;
		}
		pgdat = pgdat->node_next;
	} while (pgdat);

	/* Global shortage? */
	return global_inactive < global_target;
}

#define DEF_PRIORITY (6)

/*
 * Refill_inactive is the function used to scan and age the pages on
 * the active list and in the working set of processes, moving the
 * little-used pages to the inactive list.
 *
 * When called by kswapd, we try to deactivate as many pages as needed
 * to recover from the inactive page shortage. This makes it possible
 * for kswapd to keep up with memory demand so user processes can get
 * low latency on memory allocations.
 *
 * However, when the system starts to get overloaded we can get called
 * by user processes. For user processes we want to both reduce the
 * latency and make sure that multiple user processes together don't
 * deactivate too many pages. To achieve this we simply do less work
 * when called from a user process.
 */
static int refill_inactive(unsigned int gfp_mask)
{
	int progress = 0, maxtry;

	maxtry = 1 << DEF_PRIORITY;

	do {
		if (current->need_resched) {
			 __set_current_state(TASK_RUNNING);
			schedule();
			if (!inactive_shortage())
				return 1;
		}

		/* Walk the VM space for a bit.. */
		swap_out(DEF_PRIORITY, gfp_mask);

		/* ..and refill the inactive list */
		progress += refill_inactive_scan(DEF_PRIORITY);

		if (--maxtry <= 0)
			break;
	} while (inactive_shortage());

	return progress;
}

/*
 * Worker function for kswapd and try_to_free_pages, we get
 * called whenever there is a shortage of free/inactive_clean
 * pages.
 *
 * This function will also move pages to the inactive list,
 * if needed.
 */
static int do_try_to_free_pages(unsigned int gfp_mask, int user)
{
	int ret = 0;

	/*
	 * Eat memory from filesystem page cache, buffer cache,
	 * dentry, inode and filesystem quota caches.
	 */
	ret += page_launder(gfp_mask, user);
	shrink_dcache_memory(0, gfp_mask);
	shrink_icache_memory(0, gfp_mask);
	shrink_dqcache_memory(DEF_PRIORITY, gfp_mask);
	shrink_pgtcache_memory(gfp_mask);

	/*
	 * If needed, we move pages from the active list
	 * to the inactive list.
	 */
	if (inactive_shortage())
		ret += refill_inactive(gfp_mask);

	/* 	
	 * Reclaim unused slab cache memory.
	 */
	kmem_cache_reap(gfp_mask);

	return ret;
}


#ifdef CONFIG_FREEPG_SIGNAL
static struct {
	int cur_level;		/* 0: normal, 1: warning, 2: critical */
	int min_avail;		/* minimum available pages.
				 * Signal is never sent unless available
				 * memory is less than min_avail. */
	unsigned long lasttick;	/* last-sent time */
} freepg_sig;


freepg_signal_watermark_t freepg_sig_watermark = {
	comm: "memalerter",
	low: 0,
	mid: 0,
	high: 0,
	cur: 0,
};


static void
freepg_sig_send(int level)
{
	int signo = (level == 2) ? SIGHUP : SIGUSR1;
	struct task_struct* p = NULL;
	struct task_struct* recipient_task = NULL;
	struct siginfo si;

	freepg_sig.cur_level = level;

	if (level == 0)
		return;

	read_lock(&tasklist_lock);
	for_each_task(p) {
		if (strncmp(p->comm, freepg_sig_watermark.comm, sizeof p->comm) == 0) {
			recipient_task = p;
			break;
		}
	}

	if (recipient_task == NULL)
		goto finish;

	si.si_signo = signo;
	si.si_errno = 0;
	si.si_code = SI_KERNEL;
	si.si_pid = current->pid;
	si.si_uid = current->uid;
	freepg_sig.lasttick = jiffies;
	send_sig_info(signo, &si, recipient_task);

  finish:
	read_unlock(&tasklist_lock);
}


#if 0
/*
 * do not count buffer pages added by bdflush.
 */
static int
nr_launderable_pages(void)
{
	int nr_pages;
	struct list_head* page_lru;

	nr_pages = 0;
	spin_lock(&pagemap_lru_lock);
	for (page_lru = inactive_dirty_list.prev;
	     page_lru != &inactive_dirty_list; page_lru = page_lru->prev) {
		struct page* page;

		page = list_entry(page_lru, struct page, lru);

		if (! PageInactiveDirty(page) ||
		    PageReferenced(page) ||
		    (! page->buffers && page_count(page) > 1) ||
		    page_ramdisk(page) ||
		    PageLocked(page))
			continue;

		if (PageDirty(page)) {
			if (! page->mapping->a_ops->writepage)
				continue;
			nr_pages++;
			continue;
		}

		if (page->buffers)
			continue;
		
		if (page->mapping)
			nr_pages++;
	}
	spin_unlock(&pagemap_lru_lock);

	return nr_pages;
}
#endif


static void
freepg_sig_send_if_under(void)
{
	int nr_fps;
	int cur_avail;

	if (freepg_sig.cur_level == 2 &&
	    time_before(jiffies, freepg_sig.lasttick + 10 * HZ))
		return;

	cur_avail = nr_fps = nr_free_pages();
	cur_avail += atomic_read(&buffermem_pages);
	cur_avail += atomic_read(&page_cache_size);
	freepg_sig_watermark.cur = nr_fps + nr_inactive_clean_pages();
	if (cur_avail >= freepg_sig.min_avail)
		return;

	if (freepg_sig_watermark.cur < freepg_sig_watermark.low)
		freepg_sig_send(2);
	else if ((freepg_sig.cur_level < 1 ||
		  time_after_eq(jiffies, freepg_sig.lasttick + 10 * HZ)) &&
		 freepg_sig_watermark.cur < freepg_sig_watermark.mid)
		freepg_sig_send(1);
}



static void
freepg_sig_send_if_over(void)
{
	if (freepg_sig.cur_level == 0)
		return;

	freepg_sig_watermark.cur = nr_free_pages() +
		nr_inactive_clean_pages();
	if (freepg_sig_watermark.cur >= freepg_sig_watermark.high)
		freepg_sig_send(0);
}
#endif


DECLARE_WAIT_QUEUE_HEAD(kswapd_wait);
DECLARE_WAIT_QUEUE_HEAD(kswapd_done);

/*
 * The background pageout daemon, started as a kernel thread
 * from the init process. 
 *
 * This basically trickles out pages so that we have _some_
 * free memory available even if there is no other activity
 * that frees anything up. This is needed for things like routing
 * etc, where we otherwise might have all activity going on in
 * asynchronous contexts that cannot page things out.
 *
 * If there are applications that are active memory-allocators
 * (most normal use), this basically shouldn't matter.
 */
int kswapd(void *unused)
{
	struct task_struct *tsk = current;

	daemonize();
	strcpy(tsk->comm, "kswapd");
	sigfillset(&tsk->blocked);
	
	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	tsk->flags |= PF_MEMALLOC;

	/*
	 * Kswapd main loop.
	 */
	for (;;) {
		static long recalc = 0;

		/*
		 * We try to rebalance the VM either when we are short
		 * on free pages or when we have a shortage of inactive
		 * pages and are getting low on free pages.
		 */
		if (free_shortage() || (inactive_shortage() && !free_plenty()))
			do_try_to_free_pages(GFP_KSWAPD, 0);

		/* Once a second ... */
		if (time_after(jiffies, recalc + HZ)) {
			recalc = jiffies;

			/* Do background page aging. */
			refill_inactive_scan(DEF_PRIORITY);
		}

		/* 
		 * We go to sleep if either the free page shortage
		 * or the inactive page shortage is gone. We do this
		 * because:
		 * 1) we need no more free pages   or
		 * 2) the inactive pages need to be flushed to disk,
		 *    it wouldn't help to eat CPU time now ...
		 *
		 * We go to sleep for one second, but if it's needed
		 * we'll be woken up earlier...
		 */
#ifdef CONFIG_FREEPG_SIGNAL
		freepg_sig_send_if_under();
#endif
		if (!free_shortage() || !inactive_shortage()) {
#ifdef CONFIG_FREEPG_SIGNAL
			freepg_sig_send_if_over();
#endif
			interruptible_sleep_on_timeout(&kswapd_wait, HZ);
		/*
		 * If we couldn't free enough memory, we see if it was
		 * due to the system just not having enough memory.
		 * If that is the case, the only solution is to kill
		 * a process (the alternative is enternal deadlock).
		 *
		 * If there still is enough memory around, we just loop
		 * and try free some more memory...
		 */
		} else if (out_of_memory()) {
			oom_kill();
		}
	}
}

void wakeup_kswapd(void)
{
	if (waitqueue_active(&kswapd_wait))
		wake_up_interruptible(&kswapd_wait);
}

/*
 * Called by non-kswapd processes when they want more
 * memory but are unable to sleep on kswapd because
 * they might be holding some IO locks ...
 */
int try_to_free_pages(unsigned int gfp_mask)
{
	int ret = 1;

	if (gfp_mask & __GFP_WAIT) {
		current->flags |= PF_MEMALLOC;
		ret = do_try_to_free_pages(gfp_mask, 1);
		current->flags &= ~PF_MEMALLOC;
	}

	return ret;
}

DECLARE_WAIT_QUEUE_HEAD(kreclaimd_wait);
/*
 * Kreclaimd will move pages from the inactive_clean list to the
 * free list, in order to keep atomic allocations possible under
 * all circumstances.
 */
int kreclaimd(void *unused)
{
	struct task_struct *tsk = current;
	pg_data_t *pgdat;

	daemonize();
	strcpy(tsk->comm, "kreclaimd");
	sigfillset(&tsk->blocked);
	current->flags |= PF_MEMALLOC;

	while (1) {

		/*
		 * We sleep until someone wakes us up from
		 * page_alloc.c::__alloc_pages().
		 */
		interruptible_sleep_on(&kreclaimd_wait);

		/*
		 * Move some pages from the inactive_clean lists to
		 * the free lists, if it is needed.
		 */
		pgdat = pgdat_list;
		do {
			int i;
			for(i = 0; i < MAX_NR_ZONES; i++) {
				zone_t *zone = pgdat->node_zones + i;
				if (!zone->size)
					continue;

				while (zone->free_pages < zone->pages_low) {
					struct page * page;
					page = reclaim_page(zone);
					if (!page)
						break;
					__free_page(page);
				}
			}
			pgdat = pgdat->node_next;
		} while (pgdat);
	}
}


static int __init kswapd_init(void)
{
	printk("Starting kswapd v1.8\n");
	swap_setup();
#ifdef CONFIG_FREEPG_SIGNAL
	{
		const int min = freepages.min;
		struct sysinfo sysinfo;

		freepg_sig_watermark.low = min * 5;
		freepg_sig_watermark.mid = min * 8;
		freepages.high = min * 6;
		freepg_sig_watermark.high = min * 10;
		si_meminfo(&sysinfo);
		freepg_sig.min_avail = sysinfo.totalram * 3 / 10;
	}
#endif
	kernel_thread(kswapd, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
	kernel_thread(kreclaimd, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
	return 0;
}

module_init(kswapd_init)

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
