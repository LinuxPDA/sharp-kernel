/* 
 * Copyright (C) 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __SIGNAL_USER_H__
#define __SIGNAL_USER_H__

extern int signal_stack_size;

extern void change_sig(int signal, int on);
extern void signal_handler(void *task, unsigned long handler, int sig);
extern void set_sigstack(void *stack, int size);
extern void set_handler(int sig, void (*handler)(int), int flags, ...);
extern void setup_stack(unsigned long stack_top, struct sys_pt_regs *regs_out);
extern void setup_signal_stack(void);

#endif

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
