/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/sched.h"
#include "asm/ptrace.h"

/* determines which flags the user has access to. */
/* 1 = access 0 = no access */
#define FLAG_MASK 0x00044dd5

int putreg(struct task_struct *child, unsigned long regno, 
	   unsigned long value)
{
	switch (regno >> 2) {
	case FS:
		if (value && (value & 3) != 3)
			return -EIO;
		child->thread.process_regs.regs[FS] = value;
		return 0;
	case GS:
		if (value && (value & 3) != 3)
			return -EIO;
		child->thread.process_regs.regs[GS] = value;
		return 0;
	case DS:
	case ES:
		if (value && (value & 3) != 3)
			return -EIO;
		value &= 0xffff;
		break;
	case SS:
	case CS:
		if ((value & 3) != 3)
			return -EIO;
		value &= 0xffff;
		break;
	case EFL:
		value &= FLAG_MASK;
		value |= child->thread.process_regs.regs[EFL];
		break;
	}
	child->thread.process_regs.regs[regno >> 2] = value;
	return 0;
}

unsigned long getreg(struct task_struct *child, unsigned long regno)
{
	unsigned long retval = ~0UL;

	switch (regno >> 2) {
	case FS:
	case GS:
	case DS:
	case ES:
	case SS:
	case CS:
		retval = 0xffff;
		/* fall through */
	default:
		retval &= child->thread.process_regs.regs[regno >> 2];
	}
	return retval;
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
