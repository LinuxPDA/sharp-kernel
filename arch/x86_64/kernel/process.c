/*
 *  linux/arch/x86-64/kernel/process.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  Pentium III FXSR, SSE support
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * 
 *  X86-64 port
 *	Andi Kleen
 * 
 *  $Id: process.c,v 1.34 2001/08/15 07:52:55 ak Exp $
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/interrupt.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/ldt.h>
#include <asm/processor.h>
#include <asm/i387.h>
#include <asm/desc.h>
#include <asm/mmu_context.h>
#include <asm/pda.h>
#include <asm/prctl.h>

#include <linux/irq.h>

asmlinkage extern void ret_from_fork(void);

int hlt_counter;

/*
 * Powermanagement idle function, if any..
 */
void (*pm_idle)(void);

/*
 * Power off function, if any
 */
void (*pm_power_off)(void);

void disable_hlt(void)
{
	hlt_counter++;
}

void enable_hlt(void)
{
	hlt_counter--;
}

/*
 * We use this if we don't have any better
 * idle routine..
 */
static void default_idle(void)
{
	if (!hlt_counter) {
		__cli();
		if (!current->need_resched)
			safe_halt();
		else
			__sti();
	}
}

/*
 * On SMP it's slightly faster (but much more power-consuming!)
 * to poll the ->need_resched flag instead of waiting for the
 * cross-CPU IPI to arrive. Use this option with caution.
 */
static void poll_idle (void)
{
	int oldval;

	__sti();

	/*
	 * Deal with another CPU just having chosen a thread to
	 * run here:
	 */
	oldval = xchg(&current->need_resched, -1);

	if (!oldval)
		asm volatile(
			"2:"
			"cmpl $-1, %0;"
			"rep; nop;"
			"je 2b;"
				: :"m" (current->need_resched));
}

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle (void)
{
	/* endless idle loop with no priority at all */
	init_idle();
	current->nice = 20;
	current->counter = -100;

	while (1) {
		void (*idle)(void) = pm_idle;
		if (!idle)
			idle = default_idle;
		while (!current->need_resched)
			idle();
		schedule();
		check_pgt_cache();
	}
}

static int __init idle_setup (char *str)
{
	if (!strncmp(str, "poll", 4)) {
		printk("using polling idle threads.\n");
		pm_idle = poll_idle;
	}

	return 1;
}

__setup("idle=", idle_setup);

void machine_real_restart(unsigned char *code, int length)
{
	cli();
	panic( "restart is currently not implemented.\n" );
	while(1);
}

/* Dummies currently */
int reboot_smp; 
int reboot_thru_bios;

void machine_restart(char * __unused)
{
#if CONFIG_SMP
	/*
	 * Stop all CPUs and turn off local APICs and the IO-APIC, so
	 * other OSs see a clean IRQ state.
	 */
	smp_send_stop();
	disable_IO_APIC();
#endif

	machine_real_restart(0,0);
}

void machine_halt(void)
{
}

void machine_power_off(void)
{
	if (pm_power_off)
		pm_power_off();
}

/* Prints also some state that isn't saved in the pt_regs */ 
void show_regs(struct pt_regs * regs)
{
	unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L, fs, gs;
	unsigned int fsindex,gsindex;
	unsigned int ds,cs; 

	printk("\n");
	printk("RIP: %04x:[<%016lx>]", (unsigned)regs->cs & 0xffff, regs->rip);
	printk(" RSP: %016lx", regs->rsp);
	printk(" EFLAGS: %08lx    %s\n",regs->eflags, print_tainted());
	printk("RAX: %016lx RBX: %016lx RCX: %016lx RDX: %016lx\n",
		regs->rax,regs->rbx,regs->rcx,regs->rdx);
	printk("RSI: %016lx RDI: %016lx RBP: %016lx\n",
		regs->rsi, regs->rdi, regs->rbp);
	printk("R08: %016lx R09: %016lx R10: %016lx R11: %016lx\n",
	        regs->r8, regs->r9, regs->r10, regs->r11);
	printk("R12: %016lx R13: %016lx R14: %016lx R15: %016lx\n",
	        regs->r12, regs->r13, regs->r14, regs->r15);

	asm("movl %%ds,%0" : "=r" (ds)); 
	asm("movl %%cs,%0" : "=r" (cs)); 
	asm("movl %%fs,%0" : "=r" (fsindex));
	asm("movl %%gs,%0" : "=r" (gsindex));

	rdmsrl(0xc0000100, fs);
	rdmsrl(0xc0000101, gs); 
	printk("FS: %016lx(%04x) GS: %016lx(%04x) DS: %04x CS: %04x\n", 
	       fs,fsindex,gs,gsindex,ds,cs); 

	asm("movq %%cr0, %0": "=r" (cr0));
	asm("movq %%cr2, %0": "=r" (cr2));
	asm("movq %%cr3, %0": "=r" (cr3));
	asm("movq %%cr4, %0": "=r" (cr4));

	printk("CR0: %016lx CR2: %016lx CR3: %016lx CR4: %016lx\n", 
	       cr0, cr2, cr3, cr4);
}

/*
 * No need to lock the MM as we are the last user
 */
void release_segments(struct mm_struct *mm)
{
	void * ldt = mm->context.segments;

	/*
	 * free the LDT
	 */
	if (ldt) {
		mm->context.segments = NULL;
		clear_LDT();
		vfree(ldt);
	}
}

#define __STR(x) #x
#define __STR2(x) __STR(x)

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
	/* nothing to do ... */
}

void flush_thread(void)
{
	struct task_struct *tsk = current;

	memset(tsk->thread.debugreg, 0, sizeof(unsigned long)*8);
	/*
	 * Forget coprocessor state..
	 */
	clear_fpu(tsk);
	tsk->used_math = 0;
}

void release_thread(struct task_struct *dead_task)
{
	if (dead_task->mm) {
		void * ldt = dead_task->mm->context.segments;

		// temporary debugging check
		if (ldt) {
			printk("WARNING: dead process %8s still has LDT? <%p>\n",
					dead_task->comm, ldt);
			BUG();
		}
	}
}

/*
 * we do not have to muck with descriptors here, that is
 * done in switch_mm() as needed.
 */
void copy_segments(struct task_struct *p, struct mm_struct *new_mm)
{
	struct mm_struct * old_mm;
	void *old_ldt, *ldt;
 
	ldt = NULL;
	old_mm = current->mm;
	if (old_mm && (old_ldt = old_mm->context.segments) != NULL) {
		/*
		 * Completely new LDT, we initialize it from the parent:
		 */
		ldt = vmalloc(LDT_ENTRIES*LDT_ENTRY_SIZE);
		if (!ldt)
			printk(KERN_WARNING "ldt allocation failed\n");
		else
			memcpy(ldt, old_ldt, LDT_ENTRIES*LDT_ENTRY_SIZE);
	}
	new_mm->context.segments = ldt;
	new_mm->context.cpuvalid = 0UL;
	return;
}

int copy_thread(int nr, unsigned long clone_flags, unsigned long rsp, 
		unsigned long unused,
	struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;
	struct task_struct *me = current;

	childregs = ((struct pt_regs *) (THREAD_SIZE + (unsigned long) p)) - 1;

	*childregs = *regs;

	childregs->rax = 0;
	childregs->rsp = rsp;
	if (rsp == ~0) {
		childregs->rsp = (__u64)childregs;
	}

	p->thread.rsp = (unsigned long) childregs;
	p->thread.rsp0 = (unsigned long) (childregs+1);
	p->thread.userrsp = current->thread.userrsp; 

	p->thread.rip = (unsigned long) ret_from_fork;

	p->thread.fs = me->thread.fs;
	p->thread.gs = me->thread.gs;

	asm("movl %%gs,%0" : "=m" (p->thread.gsindex));
	asm("movl %%fs,%0" : "=m" (p->thread.fsindex));
	asm("movl %%es,%0" : "=m" (p->thread.es));
	asm("movl %%ds,%0" : "=m" (p->thread.ds));

	unlazy_fpu(current);	
	p->thread.i387 = current->thread.i387;

	return 0;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
	int i;

/* changed the size calculations - should hopefully work better. lbt */
	dump->magic = CMAGIC;
	dump->start_code = 0;
	dump->start_stack = regs->rsp & ~(PAGE_SIZE - 1);
	dump->u_tsize = ((unsigned long) current->mm->end_code) >> PAGE_SHIFT;
	dump->u_dsize = ((unsigned long) (current->mm->brk + (PAGE_SIZE-1))) >> PAGE_SHIFT;
	dump->u_dsize -= dump->u_tsize;
	dump->u_ssize = 0;
	for (i = 0; i < 8; i++)
		dump->u_debugreg[i] = current->thread.debugreg[i];  

	if (dump->start_stack < TASK_SIZE)
		dump->u_ssize = ((unsigned long) (TASK_SIZE - dump->start_stack)) >> PAGE_SHIFT;

#define SAVE(reg) dump->regs.reg = regs->reg
	SAVE(rax);
	SAVE(rbx);
	SAVE(rcx);
	SAVE(rdx);
	SAVE(rsi);
	SAVE(rdi);
	SAVE(rbp);
	SAVE(r8);
	SAVE(r9);
	SAVE(r10);
	SAVE(r11);
	SAVE(r12);
	SAVE(r13);
	SAVE(r14);
	SAVE(r15);
	SAVE(orig_rax); 
	SAVE(rip); 
#undef SAVE

	/* FIXME: Should use symbolic names for msr-s! */
	rdmsrl(0xc0000100, dump->regs.fs_base);
	rdmsrl(0xc0000101, dump->regs.kernel_gs_base); 

	dump->u_fpvalid = dump_fpu (regs, &dump->i387);
}

/*
 * This special macro can be used to load a debugging register
 */
#define loaddebug(thread,register) \
		set_debug(thread->debugreg[register], register)

/*
 *	switch_to(x,y) should switch tasks from x to y.
 *
 * We fsave/fwait so that an exception goes off at the right time
 * (as a call from the fsave or fwait in effect) rather than to
 * the wrong process. 
 * 
 * This could still be optimized: 
 * - fold all the options into a flag word and test it with a single test.
 * - could test fs/gs bitsliced
 */
void __switch_to(struct task_struct *prev_p, struct task_struct *next_p)
{
	struct thread_struct *prev = &prev_p->thread,
				 *next = &next_p->thread;
	struct tss_struct *tss = init_tss + smp_processor_id();


	unlazy_fpu(prev_p);

	/*
	 * Reload esp0, LDT and the page table pointer:
	 */
	tss->rsp0 = next->rsp0;

	/* 
	 * Switch DS and ES.
	 */
	asm volatile("movl %%es,%0" : "=m" (prev->es)); 
	if (unlikely(next->es != prev->es))
		loadsegment(es, next->es); 
	
	asm volatile ("movl %%ds,%0" : "=m" (prev->ds)); 
	if (unlikely(next->ds != prev->ds))
		loadsegment(ds, next->ds);

	/* 
	 * Switch FS and GS.
	 */
	{ 
		unsigned int fsindex;

		asm volatile("movl %%fs,%0" : "=g" (fsindex)); 
		if (unlikely(fsindex != next->fsindex)) /* or likely? */
			loadsegment(fs, next->fsindex);
		if (unlikely(fsindex != prev->fsindex))
			prev->fs = 0; 
		if (fsindex != prev->fsindex || prev->fs != next->fs) 
			wrmsrl(MSR_FS_BASE, next->fs); 
		prev->fsindex = fsindex;
	}

	{
		unsigned int gsindex;

		asm volatile("movl %%gs,%0" : "=g" (gsindex)); 
		if (unlikely(gsindex != next->gsindex)) { 
			/* should load gs in syscall exit after swapgs instead */ 
			int nr = smp_processor_id(); 
			__cli(); 
			loadsegment(gs, next->gsindex); 
			wrmsrl(MSR_GS_BASE, cpu_pda+nr); 
			__sti(); 
		}
		if (unlikely(gsindex != prev->gsindex)) 
			prev->gs = 0;				
		if (gsindex != prev->gsindex || prev->gs != next->gs)
			wrmsrl(MSR_KERNEL_GS_BASE, next->gs); 
		prev->gsindex = gsindex;
	}

	/* 
	 * Switch the PDA context.
	 */
	prev->userrsp = read_pda(oldrsp); 
	write_pda(oldrsp, next->userrsp); 
	write_pda(pcurrent, next_p); 
	write_pda(kernelstack, (unsigned long)next_p + THREAD_SIZE - PDA_STACKOFFSET);

	/*
	 * Now maybe reload the debug registers
	 */
	if (unlikely(next->debugreg[7])) {
		loaddebug(next, 0);
		loaddebug(next, 1);
		loaddebug(next, 2);
		loaddebug(next, 3);
		/* no 4 and 5 */
		loaddebug(next, 6);
		loaddebug(next, 7);
	}


	/* 
	 * Handle the IO bitmap 
	 */ 
	if (unlikely(prev->ioperm || next->ioperm)) {
		if (next->ioperm) {
			/*
			 * 4 cachelines copy ... not good, but not that
			 * bad either. Anyone got something better?
			 * This only affects processes which use ioperm().
			 * [Putting the TSSs into 4k-tlb mapped regions
			 * and playing VM tricks to switch the IO bitmap
			 * is not really acceptable.]
			 * On x86-64 we could put multiple bitmaps into 
			 * the GDT and just switch offsets
			 * This would require ugly special cases on overflow
			 * though -AK 
			 */
			memcpy(tss->io_bitmap, next->io_bitmap,
				 IO_BITMAP_SIZE*sizeof(u32));
			tss->io_map_base = IO_BITMAP_OFFSET;
		} else {
			/*
			 * a bitmap offset pointing outside of the TSS limit
			 * causes a nicely controllable SIGSEGV if a process
			 * tries to use a port IO instruction. The first
			 * sys_ioperm() call sets up the bitmap properly.
			 */
			tss->io_map_base = INVALID_IO_BITMAP_OFFSET;
		}
	}
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage 
long sys_execve(char *name, char **argv,char **envp, struct pt_regs regs)
{
	long error;
	char * filename;

	filename = getname(name);
	error = PTR_ERR(filename);
	if (IS_ERR(filename)) 
		return error;
	error = do_execve(filename, argv, envp, &regs); 
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);
	return error;
}

void set_personality_64bit(void)
{
	/* inherit personality from parent */

	/* Make sure to be in 64bit mode */
	current->thread.flags = 0;
}

asmlinkage long sys_fork(struct pt_regs regs)
{
	return do_fork(SIGCHLD, regs.rsp, &regs, 0);
}

asmlinkage long sys_clone(unsigned long clone_flags, unsigned long newsp, struct pt_regs regs)
{
	if (!newsp)
		newsp = regs.rsp;
	return do_fork(clone_flags, newsp, &regs, 0);
}

/*
 * This is trivial, and on the face of it looks like it
 * could equally well be done in user mode.
 *
 * Not so, for quite unobvious reasons - register pressure.
 * In user mode vfork() cannot have a stack frame, and if
 * done by calling the "clone()" system call directly, you
 * do not have enough call-clobbered registers to hold all
 * the information you need.
 */
asmlinkage long sys_vfork(struct pt_regs regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs.rsp, &regs, 0);
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched	((unsigned long) scheduling_functions_start_here)
#define last_sched	((unsigned long) scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	return -1;  /* FIXME */ 
}
#undef last_sched
#undef first_sched

asmlinkage int sys_arch_prctl(int code, unsigned long addr)
{ 
	int ret = 0; 
	unsigned long tmp; 
	switch (code) { 
	case ARCH_SET_GS:
		asm volatile("movw %%gs,%0" : "=g" (current->thread.gsindex)); 
		current->thread.gs = addr;
		ret = checking_wrmsrl(MSR_KERNEL_GS_BASE, addr); 
		break;
	case ARCH_SET_FS:
		asm volatile("movw %%fs,%0" : "=g" (current->thread.fsindex)); 
		current->thread.fs = addr;
		ret = checking_wrmsrl(MSR_FS_BASE, addr); 
		break;

		/* Returned value may not be correct when the user changed fs/gs */ 
	case ARCH_GET_FS:
		rdmsrl(MSR_FS_BASE, tmp);
		ret = put_user(tmp, (unsigned long *)addr); 
		break; 

	case ARCH_GET_GS: 
		rdmsrl(MSR_KERNEL_GS_BASE, tmp); 
		ret = put_user(tmp, (unsigned long *)addr); 
		break;

	default:
		ret = -EINVAL;
		break;
	} 
	return ret;	
} 

