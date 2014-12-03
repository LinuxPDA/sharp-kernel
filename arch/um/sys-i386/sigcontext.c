/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <asm/ptrace.h>
#include <asm/sigcontext.h>
#include "sysdep/ptrace.h"
#include "user_util.h"

void fill_in_sigcontext(void *sc_ptr, struct sys_pt_regs *regs,
			unsigned long cr2, int err)
{
	struct sigcontext *sc;

	sc = sc_ptr;
	sc->ebx = regs->regs[EBX];
	sc->ecx = regs->regs[ECX];
	sc->edx = regs->regs[EDX];
	sc->esi = regs->regs[ESI];
	sc->edi = regs->regs[EDI];
	sc->ebp = regs->regs[EBP];
	sc->eax = regs->regs[EAX];
	sc->ds = regs->regs[DS];
	sc->es = regs->regs[ES];
	sc->fs = regs->regs[FS];
	sc->gs = regs->regs[GS];
	sc->eip = regs->regs[EIP];
	sc->cs = regs->regs[CS];
	sc->eflags = regs->regs[EFL];
	sc->esp_at_signal = regs->regs[UESP];
	sc->ss = regs->regs[SS];
	sc->err = err;
	sc->cr2 = cr2;
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
