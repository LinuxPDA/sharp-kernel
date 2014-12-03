/*
 *  linux/include/asm-arm/mmu_context.h
 *
 *  Copyright (C) 1996 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   27-06-1996	RMK	Created
 */
#ifndef __ASM_ARM_MMU_CONTEXT_H
#define __ASM_ARM_MMU_CONTEXT_H

#include <asm/bitops.h>
#include <asm/pgtable.h>
#include <asm/arch/memory.h>
#include <asm/proc-fns.h>
#ifdef CONFIG_ARM_FCSE
#include <asm/pgalloc.h>
#endif

#ifdef CONFIG_ARM_FCSE
extern pgd_t *pgd_shared_fcse_process;

void map_exception_table_mva(pgd_t *pgd);
int get_cpu_pid(struct mm_struct *p);
void release_cpu_pid(struct mm_struct *p);
void copy_page_table_mva(struct mm_struct *mm, int old_pid);
void unmap_shared_page_table(struct mm_struct *mm, int pid);
void cpu_write_pid_register(unsigned long pid);
void cpu_xscale_set_pgd_without_invalidation(unsigned long pgd);

static inline void destroy_context(struct mm_struct *mm)
{
	if (mm->context.cpu_pid) {
		release_cpu_pid(mm);
		cpu_write_pid_register(0);
	}
}

static inline int init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	int old_pid;
	if (mm->context.cpu_pid) {
		flush_cache_all();
		flush_tlb_all();
		old_pid = mm->context.cpu_pid;
		get_cpu_pid(mm);
		copy_page_table_mva(mm, old_pid);
	}
	return 0;
}
#else
#define destroy_context(mm)		do { } while(0)
#define init_new_context(tsk,mm)	0
#endif

/*
 * This is called when "tsk" is about to enter lazy TLB mode.
 *
 * mm:  describes the currently active mm context
 * tsk: task which is entering lazy tlb
 * cpu: cpu number which is entering lazy tlb
 *
 * tsk->mm will be NULL
 */
static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}

/*
 * This is the actual mm switch as far as the scheduler
 * is concerned.  No registers are touched.
 */
static inline void
switch_mm(struct mm_struct *prev, struct mm_struct *next,
	  struct task_struct *tsk, unsigned int cpu)
{
#ifdef CONFIG_ARM_FCSE
	unsigned long npid, ppid;

	npid = next->context.cpu_pid;
	ppid = prev->context.cpu_pid;

	if (prev != next) {
		if (npid != ppid && ppid != 0) {
			cpu_write_pid_register(npid);
		}
		if (ppid == 0 || npid == 0) {
			cpu_switch_mm(next->pgd, tsk);
		}
		else {
			/* clean shared mapping */
			struct vm_area_struct *vma;
			for (vma = prev->mmap; vma != NULL; vma = vma->vm_next) {
				if ((vma->vm_flags & VM_SHARED) &&
					(vma->vm_flags & VM_WRITE)) {
					clean_dcache_range(vma->vm_start, vma->vm_end);
				}
			}
			cpu_xscale_set_pgd_without_invalidation(__virt_to_phys((unsigned long)(next->pgd)));
		}
		if (npid != ppid && ppid == 0) {
			cpu_write_pid_register(npid);
		}
	}
#else
	if (prev != next)
		cpu_switch_mm(next->pgd, tsk);
#endif
}

#define activate_mm(prev, next) \
	switch_mm((prev),(next),NULL,smp_processor_id())

#endif
