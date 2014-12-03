/* 
 * Copyright (C) 2000, 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <fcntl.h>
#include <setjmp.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/page.h>
#include <asm/unistd.h>
#include <asm/ptrace.h>
#include "user_util.h"
#include "kern_util.h"
#include "mem_user.h"
#include "user.h"
#include "process.h"
#include "sysdep/sigcontext.h"

static void signal_segv(int sig)
{
	write(2, "Seg fault in signals\n", strlen("Seg fault in signals\n"));
	for(;;) ;
}

int detach(int pid)
{
	return(ptrace(PTRACE_DETACH, pid, 0, SIGSTOP));
}

int debug = 0;
int debug_stop = 1;

int honeypot = 0;

static int signal_tramp(void *arg)
{
	int (*proc)(void *);

	if(honeypot && munmap((void *) (host_task_size - 0x10000000),
			      0x10000000)) 
		panic("Unmapping stack failed");
	if(ptrace(PTRACE_TRACEME, 0, 0, 0) < 0)
		panic("ptrace PTRACE_TRACEME failed");
	signal(SIGUSR1, SIG_IGN);
	signal(SIGSEGV, (__sighandler_t) sig_handler);
	set_timers(0);
	set_cmdline("(idle thread)");
	set_init_pid(getpid());
	proc = arg;
	return((*proc)(NULL));
}

#ifdef CONFIG_SMP
#error need to make these arrays
#endif

int debugger_pid = -1;
int debugger_fd = -1;
int gdb_pid = -1;

struct {
	unsigned long address;
	int is_write;
	int pid;
	unsigned long sp;
	int is_user;
} segfault_record[1024];

int segfault_index = 0;

struct {
	int pid;
	int signal;
	unsigned long addr;
	struct timeval time;
} signal_record[1024];

int signal_index = 0;
int nsignals = 0;
int debug_trace = 0;
extern int io_nsignals, io_count, intr_count;

extern void signal_usr1(int sig);

int tracing_pid = -1;

int signals(int (*init_proc)(void *), void *sp)
{
	void *task = NULL;
	unsigned long eip = 0;
	int status, pid = 0, sig, cont_type, tracing = 0, op = 0;
	int last_index, proc_id, save_errno = 0;

	setup_kernel_stack();
	setup_signal_stack();
	calc_sigframe_size();
	signal(SIGSEGV, signal_segv);
	signal(SIGUSR1, signal_usr1);
	signal(SIGPIPE, SIG_IGN);
	tracing_pid = getpid();
	printk("tracing thread pid = %d\n", tracing_pid);
	pid = clone(signal_tramp, sp, CLONE_FILES | SIGCHLD, init_proc);
	if(debug){
		if(gdb_pid != -1) 
			debugger_pid = attach_debugger(pid, gdb_pid, 1);
		else debugger_pid = init_ptrace_proxy(pid, 1, debug_stop);
	}
	set_cmdline("(tracing thread)");
	if(debug_trace){
		printk("Tracing thread pausing to be attached\n");
		stop();
	}
	while(1){
		if((pid = waitpid(-1, &status, WUNTRACED)) <= 0){
			if(errno != ECHILD){
				printk("wait failed - errno = %d\n", errno);
			}
			continue;
		}
		save_errno = errno;
		if(pid == debugger_pid){
			if(WIFEXITED(status) || WIFSIGNALED(status))
				debugger_pid = -1;
			debugger_signal(status, external_pid(NULL));
			continue;
		}
		nsignals++;
		if(WIFEXITED(status)){
		}
#ifdef notdef
		{
			printk("Child %d exited with status %d\n", pid, 
			       WEXITSTATUS(status));
		}
#endif
		else if(WIFSIGNALED(status)){
			sig = WTERMSIG(status);
			if(sig != 9){
				printk("Child %d exited with signal %d\n", pid,
				       sig);
			}
		}
		else if(WIFSTOPPED(status)){
			sig = WSTOPSIG(status);
			if(signal_index == 1024){
				signal_index = 0;
				last_index = 1023;
			}
			else last_index = signal_index - 1;
			if(((sig == SIGPROF) || (sig == SIGVTALRM) || 
			    (sig == SIGALRM)) &&
			   (signal_record[last_index].signal == sig) &&
			   (signal_record[last_index].pid == pid))
				signal_index = last_index;
			signal_record[signal_index].pid = pid;
			gettimeofday(&signal_record[signal_index].time, NULL);
			eip = ptrace(PTRACE_PEEKUSER, pid, UM_IP_OFFSET, 0);
			signal_record[signal_index].addr = eip;
			signal_record[signal_index++].signal = sig;
#ifdef CONFIG_SMP
			proc_id = pid_to_processor_id(pid);
			task = cpu_tasks[proc_id].task;
#else
			proc_id = 0;
			task = get_current_task();
#endif
			tracing = is_tracing(task);
			switch(sig){
			case SIGUSR1:
				sig = 0;
				op = do_proc_op(task, proc_id);
				switch(op){
				case OP_EXEC:
				case OP_SWITCH:
					continue;
				case OP_TRACE_ON:
					tracing = exit_kernel(pid, task);
					break;
				case OP_TRACE_OFF:
					tracing = 0;
					break;
				case OP_REBOOT:
				case OP_HALT:
 					kmalloc_ok = 0;
					ptrace(PTRACE_KILL, pid, 0, 0);
					return(op == OP_REBOOT);
				case OP_NONE:
					printk("Detaching pid %d\n", pid);
					detach(pid);
					continue;
				default:
					break;
				}
				break;
			case SIGSTOP:
				if(debugger_pid != -1)
					child_signal(pid, status);
				continue;
			case SIGTRAP:
				sig = 0;
				if(switching_modes(task)) tracing = 0;
				else {
					if(!tracing && (debugger_pid != -1))
						child_signal(pid, status);
					else if(!do_syscall(task, pid)){
						tracing = 0;
						sig = SIGTRAP;
						break;
					}
					continue;
				}
				break;
			case SIGCONT:
				break;
			case SIGSEGV:
				tracing = 0;
				break;
			case SIGIO:
			case SIGALRM:
			case SIGVTALRM:
			case SIGFPE:
			case SIGBUS:
			case SIGILL:
				if(!tracing && (debugger_pid != -1)){
					child_signal(pid, status);
					continue;
				}
				tracing = 0;
				break;
			case SIGPROF:
				if(tracing) sig = 0;
				break;
			case SIGCHLD:
				sig = 0;
				break;
			case SIGHUP:
				if(pid != external_pid(NULL)) continue;
				sig = 0;
				break;				
			default:
				if(debugger_pid != -1){
					child_signal(pid, status);
					continue;
				}
                                tracing = 0;
				break;
			}
			set_tracing(task, tracing);
			if(tracing != 0){
				if(singlestepping(task))
					cont_type = PTRACE_SINGLESTEP;
				else cont_type = PTRACE_SYSCALL;
			}
			else cont_type = PTRACE_CONT;

			/* XXX This doesn't close the hole totally - there
			 * is still a race between the ptrace and the child
			 * continuing.  This thread needs separate data from
			 * the children.
			 */
			errno = save_errno;
			if(ptrace(cont_type, pid, 0, sig) != 0){
				sleep(1);
				if(ptrace(cont_type, pid, 0, sig) != 0){
					tracer_panic("ptrace failed to "
						     "continue process - "
						     "errno = %d\n", 
						     errno);
				}
			}
		}
	}
	return(0);
}

int nsegfaults = 0;

void segv_handler(int sig, void *sc, int usermode)
{
	struct sigcontext_struct *context = sc;
	int index;

	if(usermode && !SEGV_IS_FIXABLE(context)){
		bad_segv(SC_FAULT_ADDR(context), SC_IP(context), 
			 SC_FAULT_WRITE(context));
		return;
	}
	lock_trap();
	index = segfault_index++;
	if(segfault_index == 1024) segfault_index = 0;
	unlock_trap();
	nsegfaults++;
	segfault_record[index].address = SC_FAULT_ADDR(context);
	segfault_record[index].pid = getpid();
	segfault_record[index].is_write = SC_FAULT_WRITE(context);
	segfault_record[index].sp = SC_SP(context);
	segfault_record[index].is_user = usermode;
	segv(SC_FAULT_ADDR(context), SC_IP(context), SC_FAULT_WRITE(context),
	     usermode);
}

extern int timer_ready, timer_on;

static void (*handlers[])(int, void *, int) = {
	[ SIGTRAP ] relay_signal,
	[ SIGFPE ] relay_signal,
	[ SIGILL ] relay_signal,
	[ SIGBUS ] relay_signal,
	[ SIGSEGV] segv_handler,
	[ SIGIO ] sigio_handler,
	[ SIGVTALRM ] timer_handler,
	[ SIGALRM ] timer_handler
};

void irq_handler(int sig, struct sigcontext sc)
{
	int user_mode, save_errno = errno, save_timer = timer_on;

	unprotect_kernel_mem();
	timer_on = 0;
	user_mode = user_context(SC_SP(&sc));
	if(user_mode) timer_ready = 1;
	change_sig(SIGUSR1, 1);
	(*handlers[sig])(sig, &sc, user_mode);
	if(user_mode) interrupt_end();
	block_signals();
	change_sig(SIGUSR1, 0);
	set_user_thread(NULL, user_mode, 0, 0);
	errno = save_errno;
	if(user_mode) timer_ready = 0;
	timer_on = save_timer;
	if(user_mode) protect_kernel_mem();
}

void sig_handler(int sig, struct sigcontext sc)
{
	int user_mode, save_errno = errno, save_timer = timer_on;

	unprotect_kernel_mem();
	timer_on = 0;
	user_mode = user_context(SC_SP(&sc));
	if(user_mode) timer_ready = 1;
	change_sig(SIGUSR1, 1);
	unblock_signals();
	(*handlers[sig])(sig, &sc, user_mode);
	if(user_mode) interrupt_end();
	block_signals();
	change_sig(SIGUSR1, 0);
	set_user_thread(NULL, user_mode, 0, 0);
	errno = save_errno;
	if(user_mode) timer_ready = 0;
	timer_on = save_timer;
	if(user_mode) protect_kernel_mem();
}

extern int timer_irq_inited, missed_ticks;

void alarm_handler(int sig, struct sigcontext sc)
{
	int user_mode;

	if(!timer_irq_inited) return;
	missed_ticks++;
	user_mode = user_context(SC_SP(&sc));
	if(!user_mode && !timer_ready) return;
	if(!timer_on) return;
	irq_handler(sig, sc);
	timer_ready = 1;
}

void do_longjmp(void *p)
{
    jmp_buf *jbuf = (jmp_buf *)p;

    longjmp(*jbuf, 1);
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
