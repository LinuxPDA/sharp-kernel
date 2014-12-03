/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/config.h"
#include "linux/types.h"
#include "linux/mm.h"
#include "linux/fs.h"
#include "linux/init.h"
#include "linux/bootmem.h"
#include "linux/swap.h"
#include "asm/page.h"
#include "asm/pgtable.h"
#include "asm/pgalloc.h"
#include "asm/bitops.h"
#include "asm/uaccess.h"
#include "user_util.h"
#include "kern_util.h"
#include "mem_user.h"
#include "kern.h"

unsigned long high_physmem;

unsigned long low_physmem;

unsigned long vm_start;

unsigned long vm_end;

pgd_t swapper_pg_dir[1024];

unsigned long *empty_zero_page = NULL;

unsigned long *empty_bad_page = NULL;

const char bad_pmd_string[] = "Bad pmd in pte_alloc: %08lx\n";

static unsigned long totalram_pages = 0;

extern char __init_begin, __init_end;

int kmalloc_ok = 0;

void mem_init(void)
{
	max_mapnr = num_physpages = max_low_pfn;

        /* clear the zero-page */
        memset((void *) empty_zero_page, 0, PAGE_SIZE);

	/* this will put all low memory onto the freelists */
	totalram_pages += free_all_bootmem();
	printk("Memory: %luk available\n", 
	       (unsigned long) nr_free_pages() << (PAGE_SHIFT-10));
	kmalloc_ok = 1;
}

void paging_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES];
	int i;

	empty_zero_page = (unsigned long *) alloc_bootmem_low_pages(PAGE_SIZE);
	empty_bad_page = (unsigned long *) alloc_bootmem_low_pages(PAGE_SIZE);
	for(i=0;i<sizeof(zones_size)/sizeof(zones_size[0]);i++) 
		zones_size[i] = 0;
	zones_size[1] = (high_physmem >> PAGE_SHIFT) - 
		(physmem >> PAGE_SHIFT) - zones_size[0];
	free_area_init(zones_size);
}

static int meminfo_22 = 0;

static int meminfo_compat(char *str)
{
	meminfo_22 = 1;
	return(1);
}

__setup("22_meminfo", meminfo_compat);

void si_meminfo(struct sysinfo *val)
{
	val->totalram = totalram_pages;
	val->sharedram = atomic_read(&shmem_nrpages);
	val->freeram = nr_free_pages();
	val->bufferram = atomic_read(&buffermem_pages);
	val->totalhigh = 0;
	val->freehigh = 0;
	val->mem_unit = PAGE_SIZE;
	if(meminfo_22){
		val->freeram <<= PAGE_SHIFT;
		val->bufferram <<= PAGE_SHIFT;
		val->totalram <<= PAGE_SHIFT;
		val->sharedram <<= PAGE_SHIFT;
	}
}

pte_t __bad_page(void)
{
	clear_page(empty_bad_page);
        return pte_mkdirty(mk_pte((struct page *) empty_bad_page, 
				  PAGE_SHARED));
}

/* This can't do anything because nothing in the kernel image can be freed
 * since it's not in kernel physical memory.
 */

void free_initmem(void)
{
}

#ifdef CONFIG_BLK_DEV_INITRD

void free_initrd_mem(unsigned long start, unsigned long end)
{
	if (start < end)
		printk ("Freeing initrd memory: %ldk freed\n", 
			(end - start) >> 10);
	for (; start < end; start += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(start));
		set_page_count(virt_to_page(start), 1);
		free_page(start);
		totalram_pages++;
	}
}

#endif

int do_check_pgt_cache(int low, int high)
{
        int freed = 0;
        if(pgtable_cache_size > high) {
                do {
                        if (pgd_quicklist) {
                                free_pgd_slow(get_pgd_fast());
                                freed++;
                        }
                        if (pmd_quicklist) {
                                pmd_free_slow(pmd_alloc_one_fast(NULL, 0));
                                freed++;
                        }
                        if (pte_quicklist) {
                                pte_free_slow(pte_alloc_one_fast(NULL, 0));
                                freed++;
                        }
                } while(pgtable_cache_size > low);
        }
        return freed;
}

void show_mem(void)
{
        int i, total = 0, reserved = 0;
        int shared = 0, cached = 0;
        int highmem = 0;

        printk("Mem-info:\n");
        show_free_areas();
        printk("Free swap:       %6dkB\n",nr_swap_pages<<(PAGE_SHIFT-10));
        i = max_mapnr;
        while (i-- > 0) {
                total++;
                if (PageHighMem(mem_map+i))
                        highmem++;
                if (PageReserved(mem_map+i))
                        reserved++;
                else if (PageSwapCache(mem_map+i))
                        cached++;
                else if (page_count(mem_map+i))
                        shared += page_count(mem_map+i) - 1;
        }
        printk("%d pages of RAM\n", total);
        printk("%d pages of HIGHMEM\n",highmem);
        printk("%d reserved pages\n",reserved);
        printk("%d pages shared\n",shared);
        printk("%d pages swap cached\n",cached);
        printk("%ld pages in page table cache\n",pgtable_cache_size);
        show_buffers();
}

unsigned long kmem_top = 0;

unsigned long get_kmem_end(void)
{
	if(kmem_top == 0) kmem_top = host_task_size - ABOVE_KMEM;
	return(kmem_top);
}

void set_kmem_end(unsigned long new)
{
	kmem_top = new;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
