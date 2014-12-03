/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include <signal.h>
#include "user_util.h"
#include "kern_util.h"
#include "user.h"

void do_exec(int old_pid, int new_pid)
{
	struct sys_pt_regs regs;

	if((ptrace(PTRACE_ATTACH, new_pid, 0, 0) < 0) ||
	   (ptrace(PTRACE_CONT, new_pid, 0, 0) < 0) ||
	   (waitpid(new_pid, 0, WUNTRACED) < 0))
		tracer_panic("do_exec failed to attach proc");

	if(ptrace_getregs(old_pid, &regs) < 0)
		tracer_panic("do_exec failed to get registers");

	kill(old_pid, SIGKILL);

	if((ptrace_setregs(new_pid, &regs) < 0) ||
	   (ptrace(PTRACE_CONT, new_pid, 0, 0) < 0))
		tracer_panic("do_exec failed to start new proc");
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
