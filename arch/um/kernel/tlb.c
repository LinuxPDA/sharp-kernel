/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/sched.h"
#include "linux/slab.h"
#include "linux/bootmem.h"
#include "asm/pgtable.h"
#include "asm/pgalloc.h"
#include "asm/a.out.h"
#include "asm/processor.h"
#include "asm/mmu_context.h"
#include "asm/uaccess.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"

static void fix_range(struct mm_struct *mm, unsigned long start_addr, 
		      unsigned long end_addr, int force)
{
	pgd_t *npgd;
	pmd_t *npmd;
	pte_t *npte;
	unsigned long addr;
	int r, w, x;

	if((current->thread.extern_pid != -1) && 
	   (current->thread.extern_pid != getpid()))
		panic("fix_range fixing wrong address space");
	if(mm == NULL) return;
	for(addr=start_addr;addr<end_addr;){
		if(addr == TASK_SIZE){
			/* Skip over kernel text, kernel data, and physical
			 * memory, which don't have ptes, plus kernel virtual
			 * memory, which is flushed separately, and remap
			 * the process stack.  The only way to get here is
			 * if (end_addr == STACK_TOP) > TASK_SIZE, which is
			 * only true in the honeypot case.
			 */
			addr = STACK_TOP - ABOVE_KMEM;
			continue;
		}
		npgd = pgd_offset(mm, addr);
		npmd = pmd_offset(npgd, addr);
		if(pmd_present(*npmd)){
			npte = pte_offset(npmd, addr);
			r = pte_read(*npte);
			w = pte_write(*npte);
			x = pte_exec(*npte);
			if(!pte_dirty(*npte)) w = 0;
			if(!pte_young(*npte)){
				r = 0;
				w = 0;
			}
			if(force || !pte_present(*npte) || pte_newpage(*npte)){
				if(munmap((void *) addr, PAGE_SIZE) < 0)
					panic("munmap failed, errno = %d\n",
					      errno);
				if(pte_present(*npte))
					map(addr, pte_address(*npte),
					    PAGE_SIZE, r, w, x);
			}
			else if(pte_newprot(*npte))
				protect(addr, PAGE_SIZE, r, w, x);
			*npte = pte_mkuptodate(*npte);
			addr += PAGE_SIZE;
		}
		else {
			if(force || pmd_newpage(*npmd)){
				if(munmap((void *) addr, PMD_SIZE) < 0)
					panic("munmap failed, errno = %d\n",
					      errno);
			}
			addr += PMD_SIZE;
		}
	}
}

void flush_tlb_kernel_vm(void)
{
	struct mm_struct *mm;
	pgd_t *npgd;
	pmd_t *npmd;
	pte_t *npte;
	unsigned long addr;
	int r, w, x;

	mm = &init_mm;
	for(addr = start_vm; addr < end_vm;){
		npgd = pgd_offset(mm, addr);
		npmd = pmd_offset(npgd, addr);
		if(pmd_present(*npmd)){
			unsigned long mask = PAGE_MASK | _PAGE_NEWPAGE | 
				_PAGE_NEWPROT;
			npte = pte_offset(npmd, addr);

			if((pte_val(*npte) & ~mask) == 
			   pgprot_val(PAGE_KERNEL)){
				r = 1;
				w = 1;
				x = 1;
			}
			else {
				r = 1;
				w = 0;
				x = 1;
			}
			if(!pte_present(*npte) || pte_newpage(*npte)){
				if(munmap((void *) addr, PAGE_SIZE) < 0)
					panic("munmap failed, errno = %d\n",
					      errno);
				if(pte_present(*npte))
					map(addr, pte_address(*npte),
					    PAGE_SIZE, r, w, x);
			}
			else if(pte_newprot(*npte))
				protect(addr, PAGE_SIZE, r, w, x);
			addr += PAGE_SIZE;

		}
		else {
			if(pmd_newpage(*npmd)){
				if(munmap((void *) addr, PMD_SIZE) < 0)
					panic("munmap failed, errno = %d\n",
					      errno);
			}
			addr += PMD_SIZE;
		}
	}
}

void flush_tlb_range(struct mm_struct *mm, unsigned long start, 
		     unsigned long end)
{
	if(mm != current->mm) return;

	/* Assumes that the range start ... end is entirely within
	 * either process memory or kernel vm
	 */
	if((start >= start_vm) && (start < end_vm)) flush_tlb_kernel_vm();
	else fix_range(mm, start, end, 0);
}

void flush_tlb_mm(struct mm_struct *mm)
{
	if(mm != current->mm) return;

	fix_range(mm, 0, STACK_TOP, 0);
	flush_tlb_kernel_vm();
}

void flush_tlb_page(struct vm_area_struct *vma, unsigned long address)
{
	address &= PAGE_MASK;
	flush_tlb_range(vma->vm_mm, address, address + PAGE_SIZE);
}

void flush_tlb_all(void)
{
	flush_tlb_mm(current->mm);
}

void force_flush_all(void)
{
	fix_range(current->mm, 0, STACK_TOP, 1);
	flush_tlb_kernel_vm();
}

pgd_t *pgd_offset_proc(struct mm_struct *mm, unsigned long address)
{
	return(pgd_offset(mm, address));
}

pmd_t *pmd_offset_proc(pgd_t *pgd, unsigned long address)
{
	return(pmd_offset(pgd, address));
}

pte_t *pte_offset_proc(pmd_t *pmd, unsigned long address)
{
	return(pte_offset(pmd, address));
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
