/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/slab.h"
#include "linux/smp_lock.h"
#include "asm/ptrace.h"
#include "asm/pgtable.h"
#include "asm/pgalloc.h"
#include "asm/uaccess.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"

static void check_vma(unsigned long addr)
{
	struct vm_area_struct *vma;

	vma = find_vma(current->mm, addr);
	if((vma == NULL) || (vma->vm_start > addr)) force_sigbus();
}

/* See comment above fork_tramp for why sigstop is defined and used like
 * this
 */

static int sigstop = SIGSTOP;

static int exec_tramp(void *sig_stack)
{
	int sig = sigstop;

	block_signals();
	init_new_thread(sig_stack, NULL);
	kill(getpid(), sig);
	return(0);
}

void flush_thread(void)
{
	unsigned long stack;
	int new_pid;

	stack = alloc_stack();
	new_pid = start_fork_tramp(current->thread.kernel_stack,
				   stack, 0, exec_tramp);
	if(new_pid < 0){
		printk(KERN_ERR 
		       "flush_thread : new thread failed, errno = %d\n",
		       errno);
		do_exit(SIGKILL);
	}
	forward_interrupts(new_pid);
	current->thread.request.op = OP_EXEC;
	current->thread.request.u.exec.pid = new_pid;
	unprotect_stack((unsigned long) current);
	usr1_pid(getpid());

        current->thread.extern_pid = new_pid;
	free_page(stack);
	protect(physmem, high_physmem - physmem, 1, 1, 0);
	task_protections((unsigned long) current);
	force_flush_all();
	unblock_signals();
}

void start_thread(struct pt_regs * regs, unsigned long eip, unsigned long esp)
{
	check_vma(current->mm->brk - 1);
	check_vma(eip);
	set_fs(USER_DS);
	flush_tlb_mm(current->mm);
	if(UM_SP(&current->thread.process_regs) == 0)
		current->thread.process_regs = current->thread.syscall_regs;
	UM_IP(&current->thread.process_regs) = eip;
	UM_SP(&current->thread.process_regs) = esp;
	UM_ELF_ZERO(&current->thread.process_regs) = 0;
	UM_FIX_EXEC_STACK(esp);
}

static int execve1(char *file, char **argv, char **env)
{
        int error;

        error = do_execve(file, argv, env, NULL);
        if (error == 0){
                current->ptrace &= ~PT_DTRACE;
                set_cmdline(current_cmd());
        }
        return(error);
}

int um_execve(char *file, char **argv, char **env)
{
	if(execve1(file, argv, env) == 0) set_user_thread(current, 1, 1, 1);
	return(-1);
}

int sys_execve(char *file, char **argv, char **env)
{
	int error;
	char *filename;

	lock_kernel();
	filename = getname((char *) file);
	error = PTR_ERR(filename);
	if (IS_ERR(filename)) goto out;
	error = execve1(filename, argv, env);
	putname(filename);
 out:
	unlock_kernel();
	return(error);
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
