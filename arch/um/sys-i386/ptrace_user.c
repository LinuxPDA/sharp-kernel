#include <sys/ptrace.h>
#include <errno.h>
#include <asm/ptrace.h>
#include "sysdep/ptrace.h"

int ptrace_getregs(long pid, struct sys_pt_regs *regs_out)
{
	return(ptrace(PTRACE_GETREGS, pid, 0, regs_out));
}

int ptrace_setregs(long pid, struct sys_pt_regs *regs)
{
	return(ptrace(PTRACE_SETREGS, pid, 0, regs));
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
