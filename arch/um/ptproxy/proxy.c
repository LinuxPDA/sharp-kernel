/**********************************************************************
proxy.c

Copyright (C) 1999 Lars Brinkhoff.  See the file COPYING for licensing
terms and conditions.

Jeff Dike (jdike@karaya.com) : Modified for integration into uml
**********************************************************************/

/* XXX This file shouldn't refer to CONFIG_* */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>

#include "ptproxy.h"
#include "sysdep.h"
#include "wait.h"

#include "user_util.h"
#include "chan.h"
#include "user.h"

static void debugger_normal_return (debugger_state *, pid_t);

/*
 * Handle debugger trap, i.e. syscall.
 */

void
debugger_syscall (debugger_state *debugger, pid_t child)
{
	int cancel = 0;
	long arg1, arg2, arg3, arg4, arg5;
	int syscall;

	syscall = syscall_get_number (debugger->pid);
	syscall_get_args (debugger->pid, &arg1, &arg2, &arg3, &arg4, &arg5);

	switch (syscall)
	{
	case __NR_execve:
		debugger->handle_trace = debugger_syscall; /* execve never returns */
		break;
	case __NR_ptrace:
		if(debugger->debugee->pid != 0) arg2 = debugger->debugee->pid;
		proxy_ptrace (debugger, arg1, arg2, arg3, arg4, child);
		cancel = 1;
		debugger->handle_trace = debugger_cancelled_return;
		break;
	case __NR_waitpid:
	case __NR_wait4:
		proxy_wait (debugger,
			    syscall == __NR_waitpid ? WAIT_WAITPID :
			    syscall == __NR_wait4   ? WAIT_WAIT4 : -1,
			    debugger->debugee->pid, (int *)arg2, arg3, 
			    (void *)arg4);
		cancel = 1;
		debugger->handle_trace = proxy_wait_return;
		break;
	case __NR_kill:
		if(arg1 == debugger->debugee->pid){
			debugger->result = kill(child, arg2);
			cancel = 1;
			debugger->handle_trace = debugger_cancelled_return;
		}
		else {
			debugger->handle_trace = debugger_normal_return;
		}
		break;
	default:
		debugger->handle_trace = debugger_normal_return;
	}

	if (cancel)
		syscall_cancel (debugger->pid);

	syscall_continue (debugger->pid);
}

void
debugger_normal_return (debugger_state *debugger, pid_t unused)
{
	debugger->handle_trace = debugger_syscall;
	syscall_continue (debugger->pid);
}

void
debugger_cancelled_return (debugger_state *debugger, pid_t unused)
{
	syscall_set_result (debugger->pid, debugger->result);
	debugger->handle_trace = debugger_syscall;
	syscall_continue (debugger->pid);
}

#ifdef CONFIG_SMP
#error need to make these arrays
#endif

static debugger_state debugger;
static debugee_state debugee;

void
init_proxy (pid_t debugger_pid, int stopped, int status)
{
	debugger.pid = debugger_pid;
	debugger.handle_trace = debugger_syscall;
	debugger.debugee = &debugee;
	debugger.stopped = 0;

	debugee.pid = 0;
	debugee.traced = 0;
	debugee.stopped = stopped;
	debugee.event = 0;
	debugee.zombie = 0;
	debugee.died = 0;
	debugee.debugger = &debugger;
	debugee.wait_status = status;
}

void debugger_proxy(int status, int pid)
{
	if (WIFSTOPPED (status))
	{
				/* debugger got a signal */

		if (WSTOPSIG (status) == SIGTRAP)
		{
			debugger.handle_trace (&debugger, pid);
		}
		else
		{
				/* go on as usual */
			ptrace (PTRACE_SYSCALL, debugger.pid, 
				0, WSTOPSIG (status));
		}
	}
	else if (WIFEXITED (status))
	{
				/* debugger exited with status 
				   WEXITSTATUS (status) */
	}
	else if (WIFSIGNALED (status))
	{
				/* debugger died from signal 
				   WTERMSIG (status) */
	}
	else
	{
				/* unknown event */
		panic("proxy got unknown status (0x%x) on debugger "
		      "(pid %d)", status, debugger.pid);
		exit (1);
	}
}

void child_proxy(pid_t pid, int status)
{
	debugee.stopped = 1;
	debugee.event = 1;
	debugee.wait_status = status;

	if (WIFSTOPPED (status))
	{
				/* child got a signal */
	}
	else if (WIFEXITED (status))
	{
				/* child exited with status WEXITSTATUS 
				   (status) */
		debugee.zombie = 1;
	}
	else if (WIFSIGNALED (status))
	{
				/* child died from signal WTERMSIG 
				   (status) */
		debugee.zombie = 1;
	}
	else
	{
		/* unknown event */
		panic("proxy got unknown status (0x%x) on child "
		      "(pid %d)", status, pid);
	}

	if (debugee.stopped)
	{
		int r;

		r = kill (debugger.pid, SIGCHLD);
		if (debugger.stopped)
			proxy_wait_return (&debugger, -1);
	}
}

extern struct io_chan gdb_chan;

int start_debugger(char *prog, int startup, int stop, int *fd_out)
{
	int err, slave, child;

	err = open_chan_pair(&gdb_chan, NULL, NULL);
	if(err) return(err);
	slave = CHAN_OUT_FD(gdb_chan);
	if((child = fork()) == 0){
		char tempname[sizeof("/tmp/gdb_init-XXXXXX")];
		char arg[sizeof("--command=/tmp/gdb_init-XXXXXX")];
		int fd;

	        if(setsid() < 0) perror("setsid");
		if((dup2(slave, 0) < 0) || (dup2(slave, 1) < 0) || 
		   (dup2(slave, 2) < 0)){
			printk("start_debugger : dup2 failed, errno = %d\n",
			       errno);
			exit(1);
		}
		if(ioctl(0, TIOCSCTTY, 0) < 0){
			printk("start_debugger : TIOCSCTTY failed, "
			       "errno = %d\n", errno);
			exit(1);
		}
		if(tcsetpgrp (1, getpid()) < 0){
			printk("start_debugger : tcsetpgrp failed, "
			       "errno = %d\n", errno);
#ifdef notdef
			exit(1);
#endif
		}
		strcpy(tempname, "/tmp/gdb_init-XXXXXX");
		if((fd = mkstemp(tempname)) < 0){
			printk("start_debugger : mkstmp failed, errno = %d\n",
			       errno);
			exit(1);
		}
		sprintf(arg, "--command=%s", tempname);
		write(fd, "att 1\n", strlen("att 1\n"));
		write(fd, "b panic\n", strlen("b panic\n"));
		write(fd, "b stop\n", strlen("b stop\n"));
		write(fd, "handle SIGWINCH nostop noprint\n", 
		      strlen("handle SIGWINCH nostop noprint\n"));
		if(startup){
			if(stop){
				write(fd, "b start_kernel\n",
				      strlen("b start_kernel\n"));
			}
			write(fd, "c\n", strlen("c\n"));
		}
		if(ptrace(PTRACE_TRACEME, 0, 0, 0) < 0){
			printk("start_debugger :  PTRACE_TRACEME failed, "
			       "errno = %d\n", errno);
			exit(1);
		}
		execlp("gdb", "gdb", arg, prog, NULL);
		printk("start_debugger : exec of gdb failed, errno = %d\n", 
		       errno);
	}
	if(child < 0){
		printk("start_debugger : fork for gdb failed, errno = %d\n", 
		       errno);
		return(-1);
	}
	*fd_out = slave;
	return(child);
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
