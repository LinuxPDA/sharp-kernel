/* $Id: cache.h,v 1.2 2001/04/11 01:28:21 randy Exp $
 *
 * include/asm-sh/cache.h
 *
 * Copyright 1999 (C) Niibe Yutaka
 */
#ifndef __ASM_SH_CACHE_H
#define __ASM_SH_CACHE_H

#define flush_cache_all()			do { } while (0)
#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_range(mm,start,end)		do { } while (0)
#define flush_cache_page(vma,vmaddr)		do { } while (0)
#define flush_page_to_ram(page)			do { } while (0)

#define invalidate_dcache_range(start,end)	do { } while (0)
#define clean_dcache_range(start,end)		do { } while (0)
#define flush_dcache_range(start,end)		do { } while (0)
#define flush_dcache_page(page)			do { } while (0)
#define clean_dcache_entry(_s)                  do { } while (0)
#define clean_cache_entry(_start)		do { } while (0)

#define flush_icache_range(start,end)		do { } while (0)
#define flush_icache_page(vma,page)		do { } while (0)
#define clean_cache_area(_start,_size)          do { } while (0)

/*
 * TLB flushing:
 *
 *  - flush_tlb_all() flushes all processes TLBs
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB's
 *  - flush_tlb_page(vma, vmaddr) flushes one page
 *  - flush_tlb_range(mm, start, end) flushes a range of pages
 */
#define flush_tlb_all()				memc_update_all()
#define flush_tlb_mm(mm)			do { } while (0)
#define flush_tlb_range(mm, start, end)		do { (void)(start); (void)(end); } while (0)
#define flush_tlb_page(vma, vmaddr)		do { } while (0)

/* bytes per L1 cache line */
#if defined(__sh2__)
#define        L1_CACHE_BYTES  16
#elif defined(__sh3__)
#define        L1_CACHE_BYTES  16
#elif defined(__SH4__)
#define        L1_CACHE_BYTES  32
#endif

#endif /* __ASM_SH_CACHE_H */
