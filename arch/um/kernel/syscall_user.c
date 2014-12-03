/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

/* XXX FIXME : Ensure that SIGIO and SIGVTALRM can't happen immediately
 * after setting up syscall stack
 * block SIGVTALRM in any code that's under wait_for_stop
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <asm/unistd.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include "user_util.h"
#include "kern_util.h"
#include "user.h"
#include "signal_kern.h"
#include "sysdep/ptrace.h"

/* XXX Bogus */
#define ERESTARTSYS	512
#define ERESTARTNOINTR	513
#define ERESTARTNOHAND	514

struct {
	int syscall;
	int pid;
	int result;
	struct timeval start;
	struct timeval end;
} syscall_record[1024];

int syscall_index = 0;

extern int timer_ready;
extern int signal_frame_size;

int syscall_handler(void *unused)
{
	struct sys_pt_regs *regs;
	long result;
	int index, syscall, again;

	kill(getpid(), SIGSTOP);
	unprotect_kernel_mem();
	timer_ready = 1;
	syscall_trace();
	lock_syscall();
	if(syscall_index == 1024) syscall_index = 0;
	index = syscall_index;
	syscall_index++;
	unlock_syscall();
	regs = process_state(NULL, NULL, NULL);
	syscall = UM_SYSCALL_NR(regs);
	syscall_record[index].syscall = UM_SYSCALL_NR(regs);
	syscall_record[index].pid = current_pid();
	syscall_record[index].result = 0xdeadbeef;
	gettimeofday(&syscall_record[index].start, NULL);
	result = execute_syscall(*regs);
	again = 0;
	if((result == -ERESTARTNOHAND) || (result == -ERESTARTSYS) || 
	   (result == -ERESTARTNOINTR))
		do_signal(&result, &again);
	UM_SET_SYSCALL_RETURN(regs, result);
	set_repeat_syscall(again);
	syscall_trace();
	syscall_record[index].result = result;
	gettimeofday(&syscall_record[index].end, NULL);
	ret_from_sys_call(NULL);

	/* XXX 
	 * This is a race, set_user_thread has to be called with signals off
	 */
	timer_ready = 0;
	set_user_thread(NULL, 1, 1, 1);
	return(0);
}

int exit_kernel(int pid, void *task)
{
	void *stack;
	struct sys_pt_regs *regs;
	int tracing, again, n, restore;

	tracing = 1;
	again = get_repeat_syscall(task);
	set_repeat_syscall(0);
	restore = get_restore_regs(task);
	regs = process_state(task, NULL, NULL);
	if(restore){
		if(ptrace_setregs(pid, regs) < 0)
			tracer_panic("Couldn't restore registers");
	}
	if(have_signals(task)){
		if(!restore) ptrace_getregs(pid, regs);
		regs = signal_state(task);
		if(ptrace_setregs(pid, regs) < 0)
			panic("Couldn't set alternate stack state");
	}
	else if(again){
		regs = syscall_state(task, &stack, &n);
		if(ptrace_setregs(pid, regs) < 0)
			panic("Couldn't restart system call");
		memcpy((void *) UM_SP(regs), stack, n);
		tracing = 0;
	}
	return(tracing);
}

extern unsigned long _stext, _etext;

int do_syscall(void *task, int pid)
{
	void *stack;
	struct sys_pt_regs *regs, proc_regs;
	int syscall, n;

	if(ptrace_getregs(pid, &proc_regs) < 0)
		tracer_panic("Couldn't read registers");

	syscall = UM_SYSCALL_NR(&proc_regs);
	if(syscall < 1) return(0);
	
	if((syscall != __NR_sigreturn) &&
	   ((unsigned long *) UM_IP(&proc_regs) >= &_stext) && 
	   ((unsigned long *) UM_IP(&proc_regs) <= &_etext))
		tracer_panic("I'm tracing myself and I can't get out");
	regs = process_state(task, NULL, NULL);
	if(syscall == __NR_sigreturn){
		set_sigreturn_syscall(UM_SYSCALL_NR(regs));
		UM_SYSCALL_NR(regs) = __NR_sigreturn;
	}
	else *regs = proc_regs;
	set_tracing(task, 0);
	regs = syscall_state(task, &stack, &n);
	if(ptrace_setregs(pid, regs) < 0)
		tracer_panic("Couldn't set system call state");
	memcpy((void *) UM_SP(regs), stack, n);

	if((ptrace(PTRACE_POKEUSER, pid, UM_SYSCALL_NR_OFFSET, 
		   __NR_getpid) < 0) ||
	   (ptrace(PTRACE_CONT, pid, 0, 0) < 0)){
		printk("Failed to change syscall number to __NR_getpid\n");
		if(errno == EIO){
			printk("You probably didn't apply the ptrace patch "
			       "to your hosting kernel\n");
		}
		else printk("errno = %d\n", errno);
		tracer_panic("do_syscall : Couldn't force getpid");
	}
	return(1);
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
