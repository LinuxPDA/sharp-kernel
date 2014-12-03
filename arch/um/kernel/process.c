/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <asm/ptrace.h>
#include <asm/sigcontext.h>
#ifdef PROFILING
#include <sys/gmon.h>
#endif
#include "user_util.h"
#include "kern_util.h"
#include "user.h"
#include "process.h"
#include "signal_kern.h"
#include "signal_user.h"
#include "sysdep/ptrace.h"
#include "sysdep/sigcontext.h"

void stop_pid(int pid)
{
	kill(pid, SIGSTOP);
}

void kill_pid(int pid)
{
	kill(pid, SIGKILL);
}

void usr1_pid(int pid)
{
	kill(pid, SIGUSR1);
}

void cont_pid(int pid)
{
	kill(pid, SIGCONT);
}

void init_new_thread(void *sig_stack, void (*usr1_handler)(int))
{
	int flags = 0;

	if(sig_stack != NULL){
		set_sigstack(sig_stack, 2 * page_size());
		flags = SA_ONSTACK;
	}
	set_handler(SIGSEGV, (__sighandler_t) sig_handler, flags,
		    SIGUSR1, SIGIO, -1);
	set_handler(SIGTRAP, (__sighandler_t) sig_handler, flags, 
		    SIGUSR1, SIGIO, -1);
	set_handler(SIGFPE, (__sighandler_t) sig_handler, flags, 
		    SIGUSR1, SIGIO, -1);
	set_handler(SIGILL, (__sighandler_t) sig_handler, flags, 
		    SIGUSR1, SIGIO, -1);
	set_handler(SIGBUS, (__sighandler_t) sig_handler, flags, 
		    SIGUSR1, SIGIO, -1);
	if(usr1_handler) set_handler(SIGUSR1, usr1_handler, flags, -1);
	signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
	set_timers(1);  /* XXX A bit of a race here */
	init_irq_signals(sig_stack != NULL);
}

/* This sigstop business works around a bug in gcc's -pg support.  
 * Normally a procedure's mcount call comes after esp has been copied to 
 * ebp and the new frame is constructed.  With procedures with no locals,
 * the mcount comes before, as the first thing that the procedure does.
 * When that procedure is main for a thread, ebp comes in as NULL.  So,
 * when mcount dereferences it, it segfaults.  So, UML works around this
 * by adding a non-optimizable local to the various trampolines, fork_tramp
 * and outer_tramp below, and exec_tramp.
 */

static int sigstop = SIGSTOP;

int fork_tramp(void *sig_stack)
{
	int sig = sigstop;

	block_signals();
	init_new_thread(sig_stack, finish_fork_handler);
	kill(getpid(), sig);
	return(0);
}

struct tramp {
	int (*tramp)(void *);
	unsigned long temp_stack;
	unsigned long stack;
	int flags;
	int pid;
};

/* See above for why sigkill is here */

int sigkill = SIGKILL;

int outer_tramp(void *arg)
{
	struct tramp *t;
	int sig = sigkill;

	t = arg;
	t->pid = clone(t->tramp, (void *) t->temp_stack + page_size()/2,
		       t->flags, (void *) t->stack);
	if(t->pid > 0) wait_for_stop(t->pid, SIGSTOP, PTRACE_CONT);
	kill(getpid(), sig);
	exit(0);
}

int start_fork_tramp(unsigned long sig_stack, unsigned long temp_stack,
		     int clone_vm, int (*tramp)(void *))
{
	struct tramp arg;
	unsigned long sp;
	int new_pid, flags, status, err;

	/* The trampoline will run on the temporary stack */
	sp = stack_sp(temp_stack);

	flags = CLONE_FILES | SIGCHLD;
	if(clone_vm) flags |= CLONE_VM;

	arg.tramp = tramp;
	arg.temp_stack = temp_stack;
	arg.stack = sig_stack;
	arg.flags = flags;

	/* Start the process and wait for it to stop itself */
	new_pid = clone(outer_tramp, (void *) sp, flags, &arg);
	if(new_pid < 0) return(-errno);
	while((err = waitpid(new_pid, &status, 0) < 0) && (errno == EINTR)) ;
	if(err < 0) panic("Waiting for outer trampoline failed - errno = %d", 
			  errno);
	if(!WIFSIGNALED(status) || (WTERMSIG(status) != SIGKILL))
		panic("outer trampoline didn't exit with SIGKILL");

	return(arg.pid);
}

void trace_myself(void)
{
	if(ptrace(PTRACE_TRACEME, 0, 0, 0) < 0)
		panic("ptrace failed in trace_myself");
}

void continue_fork(void *task, int pid, struct sys_pt_regs *regs)
{
	if(ptrace(PTRACE_CONT, pid, 0, SIGUSR1) < 0)
		tracer_panic("Couldn't continue forked process");
	wait_for_stop(pid, SIGSTOP, PTRACE_CONT);
	if(ptrace_setregs(pid, regs) < 0)
		tracer_panic("Couldn't restore registers");
}

int get_one_stack(int (*proc)(void *), void *arg, char **stack_out, 
		  struct sys_pt_regs *regs_out)
{
	void *stack;
	unsigned long sp;
	int pid, n;

	if((stack = mmap(NULL, page_size(), 
			 PROT_READ | PROT_WRITE | PROT_EXEC,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED){
		perror("get_one_stack : couldn't mmap stack");
		exit(1);		
	}
	sp = stack_sp((unsigned long) stack);
	pid = clone(proc, (void *) sp, SIGCHLD | CLONE_VM, arg);
	if(pid < 0){
		perror("get_one_stack : couldn't start thread");
		exit(1);
	}
	wait_for_stop(pid, SIGSTOP, PTRACE_CONT);
	if(ptrace(PTRACE_ATTACH, pid, 0, 0) < 0){
		perror("get_one_stack : couldn't attach to child");
		exit(1);
	}
	if(ptrace_getregs(pid, regs_out) < 0){
		perror("get_one_stack : couldn't get registers");
		exit(1);
	}
	kill(pid, SIGKILL);
	kill(pid, SIGCONT);
	waitpid(pid, NULL, 0);
	n = page_size() - (UM_SP(regs_out) & ~page_mask());
	*stack_out = stack;
	
	return(n);
}

void attach_process(int pid)
{
	if((ptrace(PTRACE_ATTACH, pid, 0, 0) < 0) ||
	   (ptrace(PTRACE_CONT, pid, 0, 0) < 0))
		tracer_panic("OP_FORK failed to attach pid");
	wait_for_stop(pid, SIGSTOP, PTRACE_CONT);
}

int signal_frame_size;
static void *local_addr;

static void frame_size(int sig)
{
	int n;

	signal_frame_size = (unsigned long) local_addr - (unsigned long) &n;
}

void calc_sigframe_size(void)
{
	int n;

	signal(SIGUSR1, frame_size);
	local_addr = &n;
	usr1_pid(getpid());
}

void tracer_panic(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vprintf(format, ap);
	while(1) sleep(10);
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
