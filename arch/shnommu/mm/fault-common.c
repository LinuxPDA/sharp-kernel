/*
 *  linux/arch/shnommu/mm/fault-common.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Modifications for ARM processor (c) 1995-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/unaligned.h>

#ifdef CONFIG_CPU_26
#define FAULT_CODE_WRITE	0x02
#define FAULT_CODE_FORCECOW	0x01
#define DO_COW(m)		((m) & (FAULT_CODE_WRITE|FAULT_CODE_FORCECOW))
#define READ_FAULT(m)		(!((m) & FAULT_CODE_WRITE))
#else
/*
 * On 32-bit processors, we define "mode" to be zero when reading,
 * non-zero when writing.  This now ties up nicely with the polarity
 * of the 26-bit machines, and also means that we avoid the horrible
 * gcc code for "int val = !other_val;".
 */
#define DO_COW(m)		(m)
#define READ_FAULT(m)		(!(m))
#endif

NORET_TYPE void die(const char *msg, struct pt_regs *regs, int err) ATTRIB_NORET;

/*
 * This is useful to dump out the page tables associated with
 * 'addr' in mm 'mm'.
 */
void show_pte(struct mm_struct *mm, unsigned long addr)
{
	/* No pte in nommu, I think --gmcnutt */
}

static int
__do_page_fault(struct mm_struct *mm, unsigned long addr, int mode,
		struct task_struct *tsk)
{
	/*
	 * In the normal ARM code we return -2 if we can't find the vma.
	 * Well, with no mmu, we'll never find the vma.
	 * I don't even know if we'll ever call this function...
	 * --gmcnutt
	 */
	return -2;
}

int do_page_fault(unsigned long addr, int mode, struct pt_regs *regs)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	unsigned long fixup;
	int fault;

	tsk = current;
	mm  = tsk->mm;

	/*
	 * We fault-in kernel-space virtual memory on-demand. The
	 * 'reference' page table is init_mm.pgd.
	 *
	 * NOTE! We MUST NOT take any locks for this case. We may
	 * be in an interrupt or a critical region, and should
	 * only copy the information from the master page table,
	 * nothing more.
	 */
	if (addr >= TASK_SIZE)
		goto vmalloc_fault;

	/*
	 * If we're in an interrupt or have no user
	 * context, we must not take the fault..
	 */
	if (in_interrupt() || !mm)
		goto no_context;

	down(&mm->mmap_sem);
	fault = __do_page_fault(mm, addr, mode, tsk);
	up(&mm->mmap_sem);

ret:
	/*
	 * Handle the "normal" case first
	 */
	if (fault > 0)
		return 0;

	/*
	 * We had some memory, but were unable to
	 * successfully fix up this page fault.
	 */
	if (fault == 0)
		goto do_sigbus;

	/*
	 * If we are in kernel mode at this point, we
	 * have no context to handle this fault with.
	 */
	if (!user_mode(regs))
		goto no_context;

	if (fault == -3) {
		/*
		 * We ran out of memory, or some other thing happened to
		 * us that made us unable to handle the page fault gracefully.
		 */
		printk("VM: killing process %s\n", tsk->comm);
		do_exit(SIGKILL);
	} else {
		/*
		 * Something tried to access memory that isn't in our memory map..
		 * User mode accesses just cause a SIGSEGV
		 */
		struct siginfo si;

#ifdef CONFIG_DEBUG_USER
		printk(KERN_DEBUG "%s: unhandled page fault at pc=0x%08lx, "
		       "lr=0x%08lx (bad address=0x%08lx, code %d)\n",
		       tsk->comm, regs->ARM_pc, regs->ARM_lr, addr, mode);
#endif

		tsk->thread.address = addr;
		tsk->thread.error_code = mode;
		tsk->thread.trap_no = 14;
		si.si_signo = SIGSEGV;
		si.si_errno = 0;
		si.si_code = fault == -1 ? SEGV_ACCERR : SEGV_MAPERR;
		si.si_addr = (void *)addr;
		force_sig_info(SIGSEGV, &si, tsk);
	}
	return 0;


/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
 */
do_sigbus:
	/*
	 * Send a sigbus, regardless of whether we were in kernel
	 * or user mode.
	 */
	tsk->thread.address = addr;
	tsk->thread.error_code = mode;
	tsk->thread.trap_no = 14;
	force_sig(SIGBUS, tsk);

	/* Kernel mode? Handle exceptions or die */
	if (user_mode(regs))
		return 0;

no_context:
	/* Are we prepared to handle this kernel fault?  */
	if ((fixup = search_exception_table(instruction_pointer(regs))) != 0) {
#ifdef DEBUG
		printk(KERN_DEBUG "%s: Exception at [<%lx>] addr=%lx (fixup: %lx)\n",
			tsk->comm, regs->ARM_pc, addr, fixup);
#endif
//FIXME		regs->ARM_pc = fixup;
		return 0;
	}

	/*
	 * Oops. The kernel tried to access some bad page. We'll have to
	 * terminate things with extreme prejudice.
	 */
	printk(KERN_ALERT
		"Unable to handle kernel %s at virtual address %08lx\n",
		(addr < PAGE_SIZE) ? "NULL pointer dereference" :
		"paging request", addr);

	show_pte(mm, addr);
	die("Oops", regs, mode);
	do_exit(SIGKILL);

	return 0;

vmalloc_fault:
	/*
	 * I blew away __do_vmalloc_fault because it was mucking with vma stuff
	 * mmnommu doesn't support. I noted that it returns -2 for unsupported
	 * memory areas, and I think any page fault in physical memory falls
	 * into this category, so we'll emulate that here.
	 * --gmcnutt
	 */
	fault = -2;
	goto ret;
}
