/*
 * linux/drivers/usb/mem-onchip.c
 *
 * memory allocate functions for RAM on chip
 * 
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 * 
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "mem-onchip.h"

#if 0
#define dbg(format, arg...) printk(KERN_DEBUG __FILE__ ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif



//#define MALLOCK_BOUNDART_32BYTE
#define MALLOCK_BOUNDART_16BYTE

#define MAKE_POINTER(p, offset)	((oc_mem_head *)((u32)(p) + offset))

#define INIT_OC_MEM_HEAD(p, s) \
	{\
		INIT_LIST_HEAD(&(p)->free_list);\
		(p)->size = s;\
		(p)->flag = 0;\
	}

#define IS_ALLOCATED_MEMORY(p) ((struct list_head *)(p) == (p)->free_list.next \
			&& (p)->free_list.next == (p)->free_list.prev)

/* memory block header */
typedef struct __oc_mem_head {
        struct list_head free_list;
	size_t size;
	u32    flag;
#ifdef MALLOCK_BOUNDART_32BYTE
	u32    dummy[4];	// for 32byte boundary
#endif
} oc_mem_head;


u32 OC_MEM_TOP = 0;
static u32 OC_MEM_BASE = 0;
static u32 OC_MEM_SIZE = 0;
static u32 OC_MEM_HCCA_BASE = 0;
#define OC_MEM_HCCA_SIZE	0x100

static spinlock_t oc_mem_list_lock = SPIN_LOCK_UNLOCKED;
static int is_oc_hcca_allocated = 0;

static void oc_mem_list_add(oc_mem_head *cp);
static void* oc_malloc(size_t size, int gfp);
static void oc_mfree(void* fp);
static size_t oc_mem_check(int verbose);


/* mem_base is 256byte boundary */
void oc_mem_init(unsigned int mem_base, size_t size)
{
	oc_mem_head *top, *cp;

	if (OC_MEM_BASE) return;

	OC_MEM_TOP = mem_base;
	OC_MEM_HCCA_BASE = mem_base;
	OC_MEM_BASE = mem_base + OC_MEM_HCCA_SIZE;	/* 0x100 for HCCA */
	OC_MEM_SIZE = size - OC_MEM_HCCA_SIZE;

	top = MAKE_POINTER(OC_MEM_BASE, 0);
	INIT_OC_MEM_HEAD(top, 0);

	cp = MAKE_POINTER(top, sizeof(oc_mem_head) + top->size);
	INIT_OC_MEM_HEAD(cp, OC_MEM_SIZE
				- sizeof(oc_mem_head) - top->size
				- sizeof(oc_mem_head));

	list_add(&cp->free_list, &top->free_list);

#if 0
	// for test
	{
		char *a, *b, *c;
		a = oc_malloc(oc_mem_check(1), 0);
			oc_mem_check(1);

		a = oc_malloc(256, 0);
			oc_mem_check(1);
		b = oc_malloc(256, 0);
			oc_mem_check(1);
		c = oc_malloc(256, 0);
			oc_mem_check(1);
		oc_mfree(b);
			oc_mem_check(1);
		b = oc_malloc(256, 0);
			oc_mem_check();
	
		oc_mfree(a);
			oc_mem_check(1);
		oc_mfree(b);
			oc_mem_check(1);
		oc_mfree(c);
			oc_mem_check(1);
		
		a = oc_malloc(256, 0);
			oc_mem_check(1);
		b = oc_malloc(256, 0);
			oc_mem_check(1);
		c = oc_malloc(256, 0);
			oc_mem_check(1);
		oc_mfree(a);
			oc_mem_check(1);
		oc_mfree(c);
			oc_mem_check(1);
		oc_mfree(b);
			oc_mem_check(1);
	}
#endif
}

static void oc_mem_list_add(oc_mem_head *cp)
{
	oc_mem_head *top = (oc_mem_head *)OC_MEM_BASE;
	struct list_head *n;

	/* address order */
	list_for_each(n, &top->free_list) {
		if ((u32)n > (u32)cp) {
			list_add_tail(&cp->free_list, n);
			return;
		}
	}
	list_add_tail(&cp->free_list, &top->free_list);
}

static void* oc_malloc(size_t size, int gfp) 
{
	oc_mem_head *top = (oc_mem_head *)OC_MEM_BASE;
	oc_mem_head *cp = NULL;
	struct list_head *n;

	if (!top)
		panic("oc_malloc: not set OC_MEM_BASE.");

	if (size == 0)
		return (void*)0;

#if defined(MALLOCK_BOUNDART_32BYTE)
	size = ((size + 32 - 1) / 32) * 32;	// for 32byte boundary
#elif defined(MALLOCK_BOUNDART_16BYTE)
	size = ((size + 16 - 1) / 16) * 16;	// for 16byte boundary
#else
	size = ((size + sizeof(u32) - 1) / sizeof(u32)) * sizeof(u32);
#endif

	list_for_each(n, &top->free_list) {
		if (((oc_mem_head *)n)->size >= size) {
			cp = (oc_mem_head *)n;
			break;
		}
	}
	if (!cp) {
		return (void*)0;
	}
		
	if (cp->size > size + sizeof(oc_mem_head)) {
		oc_mem_head *fp;
		size_t alloc = size + sizeof(oc_mem_head);
		size_t new_size = cp->size - alloc;

		list_del(&cp->free_list);
		INIT_OC_MEM_HEAD(cp, size);
		
		fp = MAKE_POINTER(cp, alloc);
		INIT_OC_MEM_HEAD(fp, new_size);
		oc_mem_list_add(fp);
		dbg("separate %08x-%04x %08x-%04x\n", (u32)cp, cp->size, (u32)fp, fp->size);
	} else if (cp->free_list.next == cp->free_list.prev
				&& cp->free_list.next == &top->free_list) {
		dbg("mallock error %08x-%04x\n", (u32)cp, cp->size);
		return (void*)0;
	} else {
		dbg("alloc this %08x-%04x\n", (u32)cp, cp->size);
		list_del_init(&cp->free_list);
	}

	return (void *)(cp + 1);
}

static void oc_mfree(void* fp)
{
	oc_mem_head *top = (oc_mem_head *)OC_MEM_BASE;
	oc_mem_head *cp = (oc_mem_head *)fp - 1;
	struct list_head *n;

	if (fp == (void*)0)
		return;
	if (!IS_ALLOCATED_MEMORY(cp)) {
		dbg("%08x is not allocated memory. n=%08x p=%08x s=%04x\n",
			(u32)fp, (u32)cp->free_list.next, (u32)cp->free_list.prev, cp->size);
		return;
	}
	
	list_for_each(n, &top->free_list) {
		/* cp is just previous of this free block */
		if (MAKE_POINTER(cp, cp->size + sizeof(oc_mem_head))
							== (oc_mem_head *)n) {
			oc_mem_head *pp = (oc_mem_head *)n->prev;
			dbg("P %08x-%04x merged to %08x-%04x\n", (u32)n, ((oc_mem_head *)n)->size, (u32)cp, cp->size);
			list_del(n);
			INIT_OC_MEM_HEAD(cp, cp->size
				+ ((oc_mem_head *)n)->size
					+ sizeof(oc_mem_head));
			/* cp merge to previous free block */
			if (cp == MAKE_POINTER(pp,
				pp->size + sizeof(oc_mem_head)) && pp != top) {
				dbg("P %08x-%04x merged to %08x-%04x\n", (u32)cp, cp->size, (u32)pp, pp->size);
				list_del(&pp->free_list);
				INIT_OC_MEM_HEAD(pp, pp->size
					+ cp->size + sizeof(oc_mem_head));
				cp = pp;
			}
			break;
		}
		/* cp is just next of this free block */
		if (cp == MAKE_POINTER(n, ((oc_mem_head *)n)->size
						+ sizeof(oc_mem_head))) {
			oc_mem_head *np = (oc_mem_head *)n->next;
			dbg("N %08x-%04x merged to %08x-%04x\n", (u32)cp, cp->size, (u32)n, ((oc_mem_head *)n)->size);
			list_del(n);
			INIT_OC_MEM_HEAD((oc_mem_head *)n,
				((oc_mem_head *)n)->size
					+ cp->size + sizeof(oc_mem_head));
			cp = (oc_mem_head *)n;
			if (np == MAKE_POINTER(cp,
					cp->size + sizeof(oc_mem_head))) {
				dbg("N %08x-%04x merged to %08x-%04x\n", (u32)np, np->size, (u32)cp, cp->size);
				list_del(&np->free_list);
				INIT_OC_MEM_HEAD(cp, cp->size
					+ np->size + sizeof(oc_mem_head));
			}
			break;
		}
	}

	oc_mem_list_add(cp);
}


static size_t oc_mem_check(int verbose)
{
	oc_mem_head *top = (oc_mem_head *)OC_MEM_BASE;
	oc_mem_head *nhp = top;
	int bc = 0;
	int alloc_bc = 0;
	int free_bc = 0;
	size_t alloc_size = 0;
	size_t free_size = 0;
	size_t total = 0;

	if (verbose) {
		printk("[No.] A HEAD     PREV     NEXT     SIZE\n");
		printk("-----+-+--------+--------+--------+----\n");
	}
	while ((u32)nhp < OC_MEM_BASE + OC_MEM_SIZE) {
		char *mark;
		if (IS_ALLOCATED_MEMORY(nhp)) {
			mark = "*";
			alloc_bc++;
			alloc_size += nhp->size + sizeof(oc_mem_head);
		} else {
			mark = " ";
			free_bc++;
			free_size += nhp->size + sizeof(oc_mem_head);
		}
		total += nhp->size + sizeof(oc_mem_head);
		if (verbose) {
			printk("[%03d] %s %08x : %08x %08x %04x\n",
				bc++, mark, (u32)nhp,
				(u32)nhp->free_list.prev,
				(u32)nhp->free_list.next, nhp->size);
		}
		nhp = MAKE_POINTER(nhp, nhp->size+sizeof(oc_mem_head));
	}
	if (verbose) {
		printk("-----+-+--------+--------+--------+----\n");
	}
	printk("alloc=%08x(%d) free=%08x(%d) total=%08x\n",
		alloc_size, alloc_bc, free_size, free_bc, total);

	return free_size;
}

#undef kmalloc
#undef kfree

void KFREE(void* p)
{
	dbg("KFREE   %08x\n", p);
	spin_lock(&oc_mem_list_lock);
       	oc_mfree((void*)p);
	spin_unlock(&oc_mem_list_lock);
}
                                                                                
void* KMALLOC(size_t s, int v)
{
       	void *p;
	spin_lock(&oc_mem_list_lock);
	p = oc_malloc(s, v);
	spin_unlock(&oc_mem_list_lock);

//oc_mem_check(0);
	dbg("KMALLOC %08x - %04x\n", p , s);
	if (s != 0 && p == NULL) {
		oc_mem_check(1);
		panic("oc_malloc: no more memory on chip RAM (%dbyts)", s);
	}
	return p;
}

void* oc_hcca_alloc(struct pci_dev *hwdev, size_t size, dma_addr_t *handle)
{
	void *p;

	if (!size)
		return (void *)0;

	if (!OC_MEM_HCCA_BASE)
		panic("oc_malloc: not set OC_MEM_BASE.");
	if (is_oc_hcca_allocated)
		panic("oc_hcca_alloc: HCCA memory was alrady allocated.");
	if (size > OC_MEM_HCCA_SIZE)
		panic("oc_hcca_alloc: no more memory on RAM0 (size=%d)", size);

	p = (void *)OC_MEM_HCCA_BASE;	/* 256byte boundary */
	//*handle = (dma_addr_t)OC_MEM_VAR_TO_OFFSET(p);
	return p;
}

void oc_hcca_free(struct pci_dev *hwdev, size_t size, void *vaddr,
						dma_addr_t dma_handle)
{
	is_oc_hcca_allocated = 0;
}

