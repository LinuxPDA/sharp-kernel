/*
 * 
 *
 *  linux/arch/ppc/kernel/process.c
 *
 *  Derived from "arch/i386/kernel/process.c"
 *    Copyright (C) 1995  Linus Torvalds
 *
 *  Updated and modified by Cort Dougan (cort@cs.nmt.edu) and
 *  Paul Mackerras (paulus@cs.anu.edu.au)
 *
 *  PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/config.h>
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
#include <linux/user.h>
#include <linux/elf.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/prom.h>
#include <asm/ppcdebug.h>
#include <asm/iSeries/HvCallHpt.h>

int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpregs);
extern unsigned long _get_SP(void);

struct task_struct *last_task_used_math = NULL;
static struct vm_area_struct init_mmap = INIT_MMAP;
static struct fs_struct init_fs = INIT_FS;
static struct files_struct init_files = INIT_FILES;
static struct signal_struct init_signals = INIT_SIGNALS;
struct mm_struct init_mm = INIT_MM(init_mm);


/* **** THE FOLLOWING IS AN AWFUL HACK ****
 * We define swapper_pg_dir to be ioremap_dir
 * and init_mmap to be ioremap_mmap
 * so that the ioremap_mm will get initialized 
 * correctly by the INIT_MM macro (from linux/sched.h)

 * ioremap_mm is really a dummy mm which is only used
 * to contain the ioremap_dir pointer and the 
 * page_table_lock for the ioremap page table
 * This is similar to the way init_mm is used
 */

#define swapper_pg_dir ioremap_dir
#define init_mmap ioremap_mmap
extern struct vm_area_struct ioremap_mmap;
struct mm_struct ioremap_mm = INIT_MM(ioremap_mm);
struct vm_area_struct ioremap_mmap = IOREMAP_MMAP;
#undef swapper_pg_dir
#undef init_mmap
// **** END OF HACK ****

/* this is 16-byte aligned because it has a stack in it */
union task_union __attribute((aligned(16))) init_task_union = {
	INIT_TASK(init_task_union.task)
};

#ifdef CONFIG_SMP
struct current_set_struct current_set[NR_CPUS] = {{&init_task, 0}, };
#endif

char *sysmap = NULL; 
unsigned long sysmap_size = 0;

extern char __toc_start;

#undef SHOW_TASK_SWITCHES
#undef CHECK_STACK

#if defined(CHECK_STACK)
unsigned long
kernel_stack_top(struct task_struct *tsk)
{
	return ((unsigned long)tsk) + sizeof(union task_union);
}

unsigned long
task_top(struct task_struct *tsk)
{
	return ((unsigned long)tsk) + sizeof(struct task_struct);
}

/* check to make sure the kernel stack is healthy */
int check_stack(struct task_struct *tsk)
{
	unsigned long stack_top = kernel_stack_top(tsk);
	unsigned long tsk_top = task_top(tsk);
	int ret = 0;

#if 0	
	/* check thread magic */
	if ( tsk->thread.magic != THREAD_MAGIC )
	{
		ret |= 1;
		printk("thread.magic bad: %08x\n", tsk->thread.magic);
	}
#endif

	if ( !tsk )
		printk("check_stack(): tsk bad tsk %p\n",tsk);
	
	/* check if stored ksp is bad */
	if ( (tsk->thread.ksp > stack_top) || (tsk->thread.ksp < tsk_top) )
	{
		printk("stack out of bounds: %s/%d\n"
		       " tsk_top %08lx ksp %08lx stack_top %08lx\n",
		       tsk->comm,tsk->pid,
		       tsk_top, tsk->thread.ksp, stack_top);
		ret |= 2;
	}
	
	/* check if stack ptr RIGHT NOW is bad */
	if ( (tsk == current) && ((_get_SP() > stack_top ) || (_get_SP() < tsk_top)) )
	{
		printk("current stack ptr out of bounds: %s/%d\n"
		       " tsk_top %08lx sp %08lx stack_top %08lx\n",
		       current->comm,current->pid,
		       tsk_top, _get_SP(), stack_top);
		ret |= 4;
	}

#if 0	
	/* check amount of free stack */
	for ( i = (unsigned long *)task_top(tsk) ; i < kernel_stack_top(tsk) ; i++ )
	{
		if ( !i )
			printk("check_stack(): i = %p\n", i);
		if ( *i != 0 )
		{
			/* only notify if it's less than 900 bytes */
			if ( (i - (unsigned long *)task_top(tsk))  < 900 )
				printk("%d bytes free on stack\n",
				       i - task_top(tsk));
			break;
		}
	}
#endif

	if (ret)
	{
		panic("bad kernel stack");
	}
	return(ret);
}
#endif /* defined(CHECK_STACK) */

void
enable_kernel_fp(void)
{
#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_FP))
		giveup_fpu(current);
	else
		giveup_fpu(NULL);	/* just enables FP for kernel */
#else
	giveup_fpu(last_task_used_math);
#endif /* CONFIG_SMP */
}

int
dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpregs)
{
	if (regs->msr & MSR_FP)
		giveup_fpu(current);
	memcpy(fpregs, &current->thread.fpr[0], sizeof(*fpregs));
	return 1;
}

void
_switch_to(struct task_struct *prev, struct task_struct *new,
	  struct task_struct **last)
{
	struct thread_struct *new_thread, *old_thread;
	unsigned long s;
	
	__save_flags(s);
	__cli();
#if CHECK_STACK
	check_stack(prev);
	check_stack(new);
#endif

#ifdef SHOW_TASK_SWITCHES
	printk("%s/%d -> %s/%d NIP %08lx cpu %d root %x/%x\n",
	       prev->comm,prev->pid,
	       new->comm,new->pid,new->thread.regs->nip,new->processor,
	       new->fs->root,prev->fs->root);
#endif
#ifdef CONFIG_SMP
	/* avoid complexity of lazy save/restore of fpu
	 * by just saving it every time we switch out if
	 * this task used the fpu during the last quantum.
	 * 
	 * If it tries to use the fpu again, it'll trap and
	 * reload its fp regs.  So we don't have to do a restore
	 * every switch, just a save.
	 *  -- Cort
	 */
	if ( prev->thread.regs && (prev->thread.regs->msr & MSR_FP) )
		giveup_fpu(prev);

	/* prev->last_processor = prev->processor; */
	current_set[smp_processor_id()].task = new;
#endif /* CONFIG_SMP */
	new_thread = &new->thread;
	old_thread = &current->thread;
	*last = _switch(old_thread, new_thread);
	__restore_flags(s);
}

void show_regs(struct pt_regs * regs)
{
	int i;

	printk("NIP: %016lX XER: %016lX LR: %016lX REGS: %p TRAP: %04lx    %s\n",
	       regs->nip, regs->xer, regs->link, regs,regs->trap, print_tainted());
	printk("MSR: %016lx EE: %01x PR: %01x FP: %01x ME: %01x IR/DR: %01x%01x\n",
	       regs->msr, regs->msr&MSR_EE ? 1 : 0, regs->msr&MSR_PR ? 1 : 0,
	       regs->msr & MSR_FP ? 1 : 0,regs->msr&MSR_ME ? 1 : 0,
	       regs->msr&MSR_IR ? 1 : 0,
	       regs->msr&MSR_DR ? 1 : 0);
	printk("TASK = %p[%d] '%s' ",
	       current, current->pid, current->comm);
	printk("Last syscall: %ld ", current->thread.last_syscall);
	printk("\nlast math %p ", last_task_used_math);
	
#ifdef CONFIG_SMP
	/* printk(" CPU: %d last CPU: %d", current->processor,current->last_processor); */
#endif /* CONFIG_SMP */
	
	printk("\n");
	for (i = 0;  i < 32;  i++)
	{
		long r;
		if ((i % 4) == 0)
		{
			printk("GPR%02d: ", i);
		}

		if ( __get_user(r, &(regs->gpr[i])) )
		    goto out;
		printk("%016lX ", r);
		if ((i % 4) == 3)
		{
			printk("\n");
		}
	}
out:
}

void exit_thread(void)
{
	if (last_task_used_math == current)
		last_task_used_math = NULL;
}

void flush_thread(void)
{
	if (last_task_used_math == current)
		last_task_used_math = NULL;
}

void
release_thread(struct task_struct *t)
{
}

/*
 * Copy a thread..
 */
int
copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
	    unsigned long unused,
	    struct task_struct * p, struct pt_regs * regs)
{
	unsigned long msr;
	struct pt_regs * childregs, *kregs;
#ifdef CONFIG_SMP
	extern void ret_from_smpfork(void);
#else
	extern void ret_from_except(void);
#endif
	/* Copy registers */
	childregs = ((struct pt_regs *)
		     ((unsigned long)p + sizeof(union task_union)
		      - STACK_FRAME_OVERHEAD)) - 2;
	*childregs = *regs;
	childregs->gpr[3] = 0;  /* Result from fork() */
	p->thread.regs = childregs;
	p->thread.ksp = (unsigned long) childregs - STACK_FRAME_OVERHEAD;
	p->thread.ksp -= sizeof(struct pt_regs ) + STACK_FRAME_OVERHEAD;
	kregs = (struct pt_regs *)(p->thread.ksp + STACK_FRAME_OVERHEAD);
#ifdef CONFIG_SMP
	kregs->nip = *((unsigned long *)ret_from_smpfork);
#else	
	/* The PPC64 compiler makes use of a TOC to contain function 
	 * pointers.  The function (ret_from_except) is actually a pointer
	 * to the TOC entry.  The first entry is a pointer to the actual
	 * function.
 	 */
	kregs->nip = *((unsigned long *)ret_from_except);
#endif	
	asm volatile("mfmsr %0" : "=r" (msr):);
	kregs->msr = msr;
	kregs->gpr[1] = (unsigned long)childregs - STACK_FRAME_OVERHEAD;
	kregs->gpr[2] = (((unsigned long)&__toc_start) + 0x8000);
	
	if (usp >= (unsigned long) regs) {
		/* Stack is in kernel space - must adjust */
		childregs->gpr[1] = (unsigned long)(childregs + 1);
		*((unsigned long *) childregs->gpr[1]) = 0;
	} else {
		/* Provided stack is in user space */
		childregs->gpr[1] = usp;
	}
	p->thread.last_syscall = -1;
	  
	/*
	 * copy fpu info - assume lazy fpu switch now always
	 *  -- Cort
	 */
	if (regs->msr & MSR_FP)
		giveup_fpu(current);
	memcpy(&p->thread.fpr, &current->thread.fpr, sizeof(p->thread.fpr));
	p->thread.fpscr = current->thread.fpscr;
	childregs->msr &= ~MSR_FP;

#ifdef CONFIG_SMP
	// p->last_processor = NO_PROC_ID;
#endif /* CONFIG_SMP */
	return 0;
}

/*
 * XXX ld.so expects the auxiliary table to start on
 * a 16-byte boundary, so we have to find it and
 * move it up. :-(
 */
static inline void shove_aux_table(unsigned long sp)
{
	int argc;
	char *p;
	unsigned long e;
	unsigned long aux_start, offset;

	sp += sizeof(int);
	if (__get_user(argc, (int *)sp))
		return;
	sp += sizeof(int) + (argc + 1) * sizeof(char *);
	/* skip over the environment pointers */
	do {
		if (__get_user(p, (char **)sp))
			return;
		sp += sizeof(char *);
	} while (p != NULL);
	aux_start = sp;
	/* skip to the end of the auxiliary table */
	do {
		if (__get_user(e, (unsigned long *)sp))
			return;
		sp += 2 * sizeof(unsigned long);
	} while (e != AT_NULL);
	offset = ((aux_start + 15) & ~15) - aux_start;
	if (offset != 0) {
		do {
			sp -= sizeof(unsigned long);
			if (__get_user(e, (unsigned long *)sp)
			    || __put_user(e, (unsigned long *)(sp + offset)))
				return;
		} while (sp > aux_start);
	}
}

/*
 * Set up a thread for executing a new program
 */
void start_thread(struct pt_regs *regs, unsigned long nip, unsigned long sp)
{
	/* NIP is *really* a pointer to the function descriptor for
         * the elf _start routine.  The first entry in the function
         * descriptor is the entry address of _start and the second
         * entry is the TOC value we need to use.
         */
	unsigned long *entry = (unsigned long *)nip;
	unsigned long *toc   = entry + 1;

    PPCDBG(PPCDBG_SYS64, "start_thread - entered - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);
    PPCDBG(PPCDBG_SYS64, "               pt_regs=0x%lx, nip=0x%lx, sp=0x%lx\n", regs, nip, sp);

	set_fs(USER_DS);
	__get_user(regs->nip, entry);
	regs->gpr[1] = sp;
	__get_user(regs->gpr[2], toc);
	regs->msr = MSR_USER64;
	shove_aux_table(sp);
	if (last_task_used_math == current)
		last_task_used_math = 0;
	current->thread.fpscr = 0;
}

asmlinkage int sys_clone(int p1, int p2, int p3, int p4, int p5, int p6,
			 struct pt_regs *regs)
{
	unsigned long clone_flags = p1;
	int res;

	PPCDBG(PPCDBG_SYS64, "sys_clone - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	res = do_fork(clone_flags, regs->gpr[1], regs, 0);
#ifdef CONFIG_SMP
	/* When we clone the idle task we keep the same pid but
	 * the return value of 0 for both causes problems.
	 * -- Cort
	 */
	if ((current->pid == 0) && (current == &init_task))
		res = 1;
#endif /* CONFIG_SMP */

	PPCDBG(PPCDBG_SYS64, "sys_clone - exited - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return res;
}

asmlinkage int sys_fork(int p1, int p2, int p3, int p4, int p5, int p6,
			struct pt_regs *regs)
{
	int res;
	
    PPCDBG(PPCDBG_SYS64, "sys_fork - entered - pid=%ld comm=%s \n", current->pid, current->comm);

  res = do_fork(SIGCHLD, regs->gpr[1], regs, 0);
  
  #ifdef CONFIG_SMP
	/* When we clone the idle task we keep the same pid but
	 * the return value of 0 for both causes problems.
	 * -- Cort
	 */
	if ((current->pid == 0) && (current == &init_task))
		res = 1;
  #endif /* CONFIG_SMP */
	
  PPCDBG(PPCDBG_SYS64, "sys_fork - exited - pid=%ld comm=%s \n", current->pid, current->comm);

	return res;
}

asmlinkage int sys_vfork(int p1, int p2, int p3, int p4, int p5, int p6,
			 struct pt_regs *regs)
{
  	PPCDBG(PPCDBG_SYS64, "sys_vfork - running - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->gpr[1], regs, 0);
}

asmlinkage int sys_execve(unsigned long a0, unsigned long a1, unsigned long a2,
			  unsigned long a3, unsigned long a4, unsigned long a5,
			  struct pt_regs *regs)
{
	int error;
	char * filename;
	
    PPCDBG(PPCDBG_SYS64, "sys_execve - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	filename = getname((char *) a0);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	if (regs->msr & MSR_FP)
		giveup_fpu(current);
  
    PPCDBG(PPCDBG_SYS64, "sys_execve - before do_execve : filename = %s\n", filename);
  
  error = do_execve(filename, (char **) a1, (char **) a2, regs);
  
  if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);

  out:
    PPCDBG(PPCDBG_SYS64, "sys_execve - exited - pid=%ld current=%lx comm=%s error = %lx\n", current->pid, current, current->comm, error);

	return error;
}


unsigned long find_hpte(unsigned long);

static __inline__ void set_pp_bit(unsigned long  pp, HPTE *addr)
{
	unsigned long old;
	unsigned long *p = (unsigned long *)(&(addr->dw1));

	__asm__ __volatile__(
"1:	ldarx	%0,0,%3\n\ 
    rldimi  %0,%2,0,62\n\ 
    stdcx.	%0,0,%3\n\
	bne	1b"
	: "=&r" (old), "=m" (*p)
	: "r" (pp), "r" (p), "m" (*p)
	: "cc");
}


static void updateStackHptePP(  unsigned long newpp, unsigned long ea )
{
    unsigned long vsid,va,vpn;
    long slot;

    
    vsid = get_kernel_vsid( ea );
    va = ( vsid << 28 ) | ( ea & 0x0fffffff );
    vpn = va >> PAGE_SHIFT;

    slot = find_hpte( vpn );

    
	if ( _machine == _MACH_iSeries ) {
		HvCallHpt_setPp( slot, newpp );
	}
	else {
        
		HPTE *  hptep  = htab_data.htab  + slot;
        
        set_pp_bit(newpp , hptep );

		/* Ensure it is out of the tlb too */
		_tlbie( va );

		
		/* Ensure it is visible before validating */
		__asm__ __volatile__ ("eieio" : : : "memory");

	   
		__asm__ __volatile__ ("ptesync" : : : "memory");

    
	}
}


struct task_struct * alloc_task_struct(void) {
    struct task_struct * new_task_ptr;
    
    new_task_ptr = ((struct task_struct *) __get_free_pages(GFP_KERNEL, get_order(THREAD_SIZE)));
    
    /* Set the second page to be write protected - prevent kernel stack overflow     */
    updateStackHptePP(  PP_RXRX , (unsigned long)new_task_ptr + PAGE_SIZE ); 
    
    return new_task_ptr;
}

void free_task_struct(struct task_struct * task_ptr) {
    
    if (atomic_read(&(virt_to_page(task_ptr)->count)) == 1) {
      updateStackHptePP(  PP_RWXX , (unsigned long)task_ptr + PAGE_SIZE );
    } 
    free_pages((unsigned long)(task_ptr), get_order(THREAD_SIZE));
}


void
print_backtrace(unsigned long *sp)
{
	int cnt = 0;
	unsigned long i;

	printk("Call backtrace: ");
	while (sp) {
		if (__get_user( i, &sp[2] ))
			break;
		if (cnt++ % 4 == 0)
			printk("\n");
		printk("%016lX ", i);
		if (cnt > 32) break;
		if (__get_user(sp, (unsigned long **)sp))
			break;
	}
	printk("\n");
}

#if 0
/*
 * Low level print for debugging - Cort
 */
int __init ll_printk(const char *fmt, ...)
{
        va_list args;
	char buf[256];
        int i;

        va_start(args, fmt);
        i=vsprintf(buf,fmt,args);
	ll_puts(buf);
        va_end(args);
        return i;
}

int lines = 24, cols = 80;
int orig_x = 0, orig_y = 0;

void puthex(unsigned long val)
{
	unsigned char buf[10];
	int i;
	for (i = 7;  i >= 0;  i--)
	{
		buf[i] = "0123456789ABCDEF"[val & 0x0F];
		val >>= 4;
	}
	buf[8] = '\0';
	prom_print(buf);
}

void __init ll_puts(const char *s)
{
	int x,y;
	char *vidmem = (char *)/*(_ISA_MEM_BASE + 0xB8000) */0xD00B8000;
	char c;
	extern int mem_init_done;

	if ( mem_init_done ) /* assume this means we can printk */
	{
		printk(s);
		return;
	}

	/*
	 * can't ll_puts on chrp without openfirmware yet.
	 * vidmem just needs to be setup for it.
	 * -- Cort
	 */
	if ( _machine != _MACH_prep )
		return;
	x = orig_x;
	y = orig_y;

	while ( ( c = *s++ ) != '\0' ) {
		if ( c == '\n' ) {
			x = 0;
			if ( ++y >= lines ) {
				/*scroll();*/
				/*y--;*/
				y = 0;
			}
		} else {
			vidmem [ ( x + cols * y ) * 2 ] = c; 
			if ( ++x >= cols ) {
				x = 0;
				if ( ++y >= lines ) {
					/*scroll();*/
					/*y--;*/
					y = 0;
				}
			}
		}
	}

	orig_x = x;
	orig_y = y;
}
#endif

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched    (*(unsigned long *)scheduling_functions_start_here)
#define last_sched     (*(unsigned long *)scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long ip, sp;
	unsigned long stack_page = (unsigned long)p;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;
	sp = p->thread.ksp;
	do {
		sp = *(unsigned long *)sp;
		if (sp < (stack_page + sizeof(struct task_struct)) ||
		    sp >= (stack_page + (4 * PAGE_SIZE)))
			return 0;
		if (count > 0) {
			ip = *(unsigned long *)(sp + 16);
			if (ip < first_sched || ip >= last_sched)
				return (ip & 0xFFFFFFFF);
		}
	} while (count++ < 16);
	return 0;
}

void show_trace_task(struct task_struct *p)
{
	unsigned long ip, sp;
	unsigned long stack_page = (unsigned long)p;
	int count = 0;

	if (!p)
		return;

	printk("Call Trace: ");
	sp = p->thread.ksp;
	do {
		sp = *(unsigned long *)sp;
		if (sp < (stack_page + sizeof(struct task_struct)) ||
		    sp >= (stack_page + (4 * PAGE_SIZE)))
			break;
		if (count > 0) {
			ip = *(unsigned long *)(sp + 16);
			printk("[%016lx] ", ip);
		}
	} while (count++ < 16);
	printk("\n");
}
