/**********************************************************************
ptrace.c

Copyright (C) 1999 Lars Brinkhoff.  See the file COPYING for licensing
terms and conditions.

Jeff Dike (jdike@karaya.com) : Modified for integration into uml
**********************************************************************/

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ptrace.h>

#include "ptproxy.h"

struct {
	int op;
	pid_t pid;
	long addr;
	long data;
	pid_t child;
	struct timeval tv;
} ptrace_record[1024];

int ptrace_index = 0;

void
proxy_ptrace (struct debugger *debugger, int arg1, pid_t arg2,
	      long arg3, long arg4, pid_t child)
{
	int status;

	if(ptrace_index == 1024) ptrace_index = 0;
	ptrace_record[ptrace_index].op = arg1;
	ptrace_record[ptrace_index].pid = arg2;
	ptrace_record[ptrace_index].data = arg3;
	ptrace_record[ptrace_index].addr = arg4;
	ptrace_record[ptrace_index].child = child;
	gettimeofday(&ptrace_record[ptrace_index++].tv, NULL);
	if (debugger->debugee->died)
	{
		debugger->result = -ESRCH;
		debugger->handle_trace = debugger_cancelled_return;
	}

	switch (arg1)
	{
	case PTRACE_ATTACH:
		if (debugger->debugee->traced)
			debugger->result = -EPERM;
		else
		{
			debugger->result = 0;
			debugger->debugee->pid = arg2;
			debugger->debugee->traced = 1;
			if(!debugger->debugee->stopped) kill(child, SIGSTOP);
			else {
				child_proxy(child, 
					    debugger->debugee->wait_status);
			}
		}
		break;

	case PTRACE_CONT:
		debugger->result = ptrace (PTRACE_CONT, child, arg3, arg4);
		break;

	case PTRACE_DETACH:
		if (debugger->debugee->traced)
		{
			debugger->result = 0;
			debugger->debugee->traced = 0;
			kill(child, SIGCONT);
		}
		else
			debugger->result = -EPERM;
		break;

	case PTRACE_GETFPREGS:
	{
		long regs[27];
		int i;

		debugger->result = ptrace (PTRACE_GETFPREGS, child, 0, regs);
		if (debugger->result == -1)
			debugger->result = -errno;
		else
		{
			for (i = 0; i < 27; i++)
				ptrace (PTRACE_POKEDATA, debugger->pid,
					arg4 + 4 * i, regs[i]);
		}
	}
	break;

	case PTRACE_GETREGS:
	{
		long regs[17];
		int i;

		debugger->result = ptrace (PTRACE_GETREGS, child, 0, regs);
		if (debugger->result == -1)
			debugger->result = -errno;
		else
		{
			for (i = 0; i < 17; i++)
				ptrace (PTRACE_POKEDATA, debugger->pid,
					arg4 + 4 * i, regs[i]);
		}
	}
	break;

	case PTRACE_KILL:
		debugger->result = ptrace (PTRACE_KILL, child, arg3, arg4);
		if (debugger->result == -1)
			debugger->result = -errno;
		break;

	case PTRACE_PEEKDATA:
	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKUSER:
		errno = 0;
		debugger->result = ptrace (arg1, child, arg3, 0);
		if (debugger->result == -1 && errno != 0)
			debugger->result = -errno;
		else
		{
			ptrace (PTRACE_POKEDATA, debugger->pid, arg4, 
				debugger->result);
			debugger->result = 0;
		}
		break;
	case PTRACE_POKEDATA:
	case PTRACE_POKETEXT:
	case PTRACE_POKEUSER:
		debugger->result = ptrace (arg1, child, arg3, arg4);
		if (debugger->result == -1)
			debugger->result = -errno;
		break;

	case PTRACE_SETFPREGS:
	{
		long regs[27];
		int i;

		for (i = 0; i < 27; i++)
			regs[i] = ptrace (PTRACE_PEEKDATA, debugger->pid,
					  arg4 + 4 * i, 0);
		debugger->result = ptrace (PTRACE_SETFPREGS, child, 0, regs);
		if (debugger->result == -1)
			debugger->result = -errno;
	}
	break;

	case PTRACE_SETREGS:
	{
		long regs[17];
		int i;

		for (i = 0; i < 17; i++)
			regs[i] = ptrace (PTRACE_PEEKDATA, debugger->pid,
					  arg4 + 4 * i, 0);
		debugger->result = ptrace (PTRACE_SETREGS, child, 0, regs);
		if (debugger->result == -1)
			debugger->result = -errno;
	}
	break;

	case PTRACE_SINGLESTEP:
		debugger->result = ptrace (PTRACE_SINGLESTEP, child, arg3, 
					   arg4);
		if (debugger->result == -1)
			debugger->result = -errno;
		else {
			status = wait_for_stop(child, SIGTRAP, 
					       PTRACE_SINGLESTEP);
			child_proxy(child, status);
		}
		break;

	case PTRACE_SYSCALL:
		debugger->result = ptrace (PTRACE_SYSCALL, child, arg3, arg4);
		if (debugger->result == -1)
			debugger->result = -errno;
		break;

	case PTRACE_TRACEME:
		debugger->result = -EINVAL; 
		break;

	default:
		debugger->result = -EINVAL; 
	}
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
