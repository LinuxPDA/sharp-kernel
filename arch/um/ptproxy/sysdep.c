/**********************************************************************
sysdep.c

Copyright (C) 1999 Lars Brinkhoff.  See the file COPYING for licensing
terms and conditions.
**********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include <sysdep/ptrace.h>

#include <linux/unistd.h>

int
syscall_get_number (pid_t pid)
{
	return ptrace (PTRACE_PEEKUSER, pid, UM_SYSCALL_NR_OFFSET, 0);
}

void
syscall_cancel (pid_t pid)
{
	int err;

	err = ptrace (PTRACE_POKEUSER, pid, UM_SYSCALL_NR_OFFSET, __NR_getpid);
	if (err == -1)
	{
		fprintf (stderr, "ptproxy: couldn't cancel syscall: "
			 "errno = %d\n", errno);
		exit (1);
	}
}

void
syscall_get_args (pid_t pid, long *arg1, long *arg2,
		  long *arg3, long *arg4, long *arg5)
{
	*arg1 = ptrace (PTRACE_PEEKUSER, pid, UM_SYSCALL_ARG1_OFFSET, 0);
	*arg2 = ptrace (PTRACE_PEEKUSER, pid, UM_SYSCALL_ARG2_OFFSET, 0);
	*arg3 = ptrace (PTRACE_PEEKUSER, pid, UM_SYSCALL_ARG3_OFFSET, 0);
	*arg4 = ptrace (PTRACE_PEEKUSER, pid, UM_SYSCALL_ARG4_OFFSET, 0);
	*arg5 = ptrace (PTRACE_PEEKUSER, pid, UM_SYSCALL_ARG5_OFFSET, 0);
}

void
syscall_set_result (pid_t pid, long result)
{
	ptrace (PTRACE_POKEUSER, pid, UM_SYSCALL_RET_OFFSET, result);
}

void
syscall_continue (pid_t pid)
{
	ptrace (PTRACE_SYSCALL, pid, 0, 0);
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
