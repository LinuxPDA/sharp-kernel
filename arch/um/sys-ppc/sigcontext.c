#include "asm/ptrace.h"
#include "asm/sigcontext.h"
#include "sysdep/ptrace.h"
#include "user_util.h"

void fill_in_sigcontext(void *scontext, struct sys_pt_regs *regs,
                        unsigned long cr2, int err)
{
        struct sigcontext_struct *sc = scontext;
#if 0
        int i;
        // general purpose regs
        for (i=0; i<32; ++i) {
                sc->regs->gpr[i] = regs->regs[PT_R0 + i];
        }
        sc->regs->nip = regs->regs[PT_NIP];
        sc->regs->msr = regs->regs[PT_MSR];
        sc->regs->orig_gpr3 = regs->regs[PT_ORIG_R3];
        sc->regs->ctr = regs->regs[PT_CTR];
        sc->regs->link = regs->regs[PT_LNK];
        sc->regs->xer = regs->regs[PT_XER];
        sc->regs->ccr = regs->regs[PT_CCR];
        sc->regs->mq = regs->regs[PT_MQ];
        sc->regs->trap = err;
        sc->regs->dar = cr2;
#endif
	/* This is a bit of a hack; there's some confusion with the
	 * various definitions of [sys_]pt_regs, and everything isn't
	 * quite coming together quite right. */
	memcpy(sc->regs, regs, sizeof(struct sys_pt_regs));
        /*(sc->regs) = *regs; */
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
