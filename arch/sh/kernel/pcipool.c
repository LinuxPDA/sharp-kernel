/*
 * linux/arch/sh/kernel/pcipool.c
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 *  Based on linux/arch/arm/mach-sa1100/pcipool.c
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>

#include <asm/page.h>

struct pci_pool*
   pci_pool_create(const char*, struct pci_dev*, size_t, size_t, size_t, int);
void  pci_pool_destroy (struct pci_pool*);
void* pci_pool_alloc (struct pci_pool*, int, dma_addr_t*);
void  pci_pool_free (struct pci_pool*, void*, dma_addr_t);

struct pci_pool {
	struct list_head	page_list;
	spinlock_t		lock;
	size_t			blocks_per_page;
	size_t			size;
	int			flags;
	struct pci_dev*		dev;
	size_t			allocation;
	char			name[32];
	wait_queue_head_t	waitq;
};

struct pci_page {
	struct list_head	page_list;
	void*			vaddr;
	dma_addr_t		dma;
	unsigned long		bitmap[0];
};

#define	POOL_TIMEOUT_JIFFIES	((100 /* msec */ * HZ) / 1000)
#define	POOL_POISON_BYTE	0xa7

struct pci_pool* pci_pool_create(const char* name,
				 struct pci_dev* pdev,
				 size_t size,
				 size_t align,
				 size_t allocation,
				 int flags)
{
	struct pci_pool* retval;

	if (align == 0)
		align = 1;
	if (size == 0)
		return 0;
	else if (size < align)
		size = align;
	else if ((size % align) != 0) {
		size += align + 1;
		size &= ~(align - 1);
	}

	if (allocation == 0) {
		if (PAGE_SIZE < size) {
			allocation = size;
		}
		else {
			allocation = PAGE_SIZE;
		}
	}
	else if (allocation < size) 
		return 0;

	if (!(retval = kmalloc(sizeof(*retval), flags)))
		return retval;

	strncpy(retval->name, name, sizeof retval->name);
	retval->name[sizeof retval->name - 1] = 0;

	retval->dev = pdev;
	INIT_LIST_HEAD(&retval->page_list);
	spin_lock_init(&retval->lock);
	retval->size            = size;
	retval->flags           = flags;
	retval->allocation      = allocation;
	retval->blocks_per_page = allocation / size;
	init_waitqueue_head(&retval->waitq);

	return retval;
}

static struct pci_page* pool_alloc_page (struct pci_pool* pool, int mem_flags)
{
	struct pci_page* page;
	size_t mapsize;

	mapsize  = pool->blocks_per_page;
	mapsize  = (mapsize + BITS_PER_LONG - 1) / BITS_PER_LONG;
	mapsize *= sizeof(long);

	page = (struct pci_page*)kmalloc(mapsize + sizeof(*page), mem_flags);
	if (!page)
		return 0;

	page->vaddr = pci_alloc_consistent(pool->dev,
					   pool->allocation, &page->dma);
	if (page->vaddr) {
		memset(page->bitmap, 0xff, mapsize);
		if (pool->flags & SLAB_POISON) {
			memset(page->vaddr, POOL_POISON_BYTE,
			       pool->allocation);
		}
		list_add(&page->page_list, &pool->page_list);
	}
	else {
		kfree(page);
		page = 0;
	}

	return page;
}


static inline int is_page_busy(int blocks, unsigned long *bitmap)
{
	while (blocks > 0) {
		if (*bitmap++ != ~0UL)
			return 1;
		blocks -= BITS_PER_LONG;
	}
	return 0;
}

static void pool_free_page (struct pci_pool *pool, struct pci_page *page)
{
	dma_addr_t dma = page->dma;

	if (pool->flags & SLAB_POISON) {
		memset(page->vaddr, POOL_POISON_BYTE, pool->allocation);
	}
	pci_free_consistent (pool->dev, pool->allocation, page->vaddr, dma);
	list_del (&page->page_list);
	kfree(page);
}

void pci_pool_destroy (struct pci_pool *pool)
{
	unsigned long flags;

	spin_lock_irqsave (&pool->lock, flags);
	while (!list_empty(&pool->page_list)) {
		struct pci_page* page =
			list_entry(pool->page_list.next, struct pci_page, page_list);
		if (is_page_busy(pool->blocks_per_page, page->bitmap)) {
			printk(KERN_ERR "pci_pool_destroy %s/%s, %p busy\n",
			       pool->dev ? pool->dev->slot_name : NULL,
			       pool->name,
			       page->vaddr);

			list_del(&page->page_list);
			kfree(page);
		}
		else {
			pool_free_page(pool, page);
		}
	}
	spin_unlock_irqrestore(&pool->lock, flags);
	kfree (pool);
}

void* pci_pool_alloc (struct pci_pool* pool, int mem_flags, dma_addr_t* handle)
{
	struct list_head* entry;
	struct pci_page* page;
	unsigned long flags;
	size_t offset;
	int map, block, i;
	void* retval;

restart:
	spin_lock_irqsave(&pool->lock, flags);
	list_for_each(entry, &pool->page_list) {
		page = list_entry(entry, struct pci_page, page_list);
		for (map = 0, i = 0; i < pool->blocks_per_page;
		     i += BITS_PER_LONG, map++) {
			if (page->bitmap[map] == 0)
				continue;
			block = ffz(~ page->bitmap [map]);
			if ((i + block) < pool->blocks_per_page) {
				clear_bit(block, &page->bitmap [map]);
				offset = (BITS_PER_LONG * map) + block;
				offset *= pool->size;
				goto ready;
			}
		}
	}
	page = pool_alloc_page(pool, mem_flags);
	if (page == (struct pci_page*)0) {
		if (mem_flags == SLAB_KERNEL) {
			DECLARE_WAITQUEUE(wait, current);

			current->state = TASK_INTERRUPTIBLE;
			add_wait_queue(&pool->waitq, &wait);
			spin_unlock_irqrestore(&pool->lock, flags);

			schedule_timeout(POOL_TIMEOUT_JIFFIES);

			current->state = TASK_RUNNING;
			remove_wait_queue(&pool->waitq, &wait);
			goto restart;
		}
		retval = 0;
		goto done;
	}

	clear_bit (0, &page->bitmap [0]);
	offset = 0;
ready:
	retval = offset + page->vaddr;
	*handle = offset + page->dma;
done:
	spin_unlock_irqrestore(&pool->lock, flags);
	return retval;
}

static struct pci_page* pool_find_page (struct pci_pool* pool, dma_addr_t dma)
{
	struct list_head* entry;
	struct pci_page* page;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	list_for_each(entry, &pool->page_list) {
		page = list_entry(entry, struct pci_page, page_list);
		if (dma < page->dma)
			continue;
		if (dma < (page->dma + pool->allocation))
			goto done;
	}
	page = 0;
done:
	spin_unlock_irqrestore(&pool->lock, flags);
	return page;
}

void pci_pool_free(struct pci_pool* pool, void* vaddr, dma_addr_t dma)
{
	struct pci_page* page;
	unsigned long flags;
	int map, block;

	if ((page = pool_find_page(pool, dma)) == 0) {
		printk(KERN_ERR "pci_pool_free %s/%s, %p/%x (bad dma)\n",
		       pool->dev ? pool->dev->slot_name : NULL,
		       pool->name, vaddr, dma);
		return;
	}

	block  = dma - page->dma;
	block /= pool->size;
	map = block / BITS_PER_LONG;
	block %= BITS_PER_LONG;

	if (pool->flags & SLAB_POISON) {
		memset(vaddr, POOL_POISON_BYTE, pool->size);
	}

	spin_lock_irqsave(&pool->lock, flags);
	set_bit(block, &page->bitmap [map]);
	if (waitqueue_active(&pool->waitq)) {
		wake_up(&pool->waitq);
	}
	spin_unlock_irqrestore(&pool->lock, flags);
}

