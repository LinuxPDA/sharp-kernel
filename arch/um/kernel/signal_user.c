/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <sys/mman.h>
#include "user_util.h"
#include "kern_util.h"
#include "user.h"
#include "signal_user.h"
#include "signal_kern.h"
#include "sysdep/sigcontext.h"

extern int timer_on;

char *signal_stack = NULL;
char *signal_stack_mask = NULL;
int signal_stack_size = 0;
struct sys_pt_regs signal_regs;
struct sys_pt_regs signal_regs_mask;

static int setup_signal_tramp(void *stack)
{
	set_sigstack(stack, page_size());
	set_handler(SIGUSR1, signal_deliverer, SA_ONSTACK, -1);
	usr1_pid(getpid());
}

void setup_signal_stack(void)
{
	struct sys_pt_regs regs1, regs2;
	char *sigstack1, *sigstack2, *stack1, *stack2;
	int size;

	sigstack1 = mmap(NULL, page_size(), PROT_READ | PROT_WRITE | PROT_EXEC,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	sigstack2 = mmap(NULL, page_size(), PROT_READ | PROT_WRITE | PROT_EXEC,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if((sigstack1 == MAP_FAILED) || (sigstack2 == MAP_FAILED)){
		perror("setup_signal_stack : couldn't mmap stacks");
		exit(1);		
	}
	size = get_one_stack(setup_signal_tramp, sigstack1, &stack1, &regs1);
	if(get_one_stack(setup_signal_tramp, sigstack2, &stack2, 
			 &regs2) != size){
		printf("setup_signal_stack : differing stack sizes\n");
		exit(1);
	}

	signal_stack = diff_stacks(size, sigstack1, &regs1, sigstack2, &regs2, 
				   &signal_stack_mask, &signal_regs, 
				   &signal_regs_mask);
	signal_stack_size = size;

	if((munmap(sigstack1, page_size()) < 0) || 
	   (munmap(sigstack2, page_size()) < 0) || 
	   (munmap(stack1, page_size()) < 0) || 
	   (munmap(stack2, page_size()) < 0)){
		perror("setup_signal_stack : unmapping temp stacks");
		exit(1);
	}	
}

void setup_stack(unsigned long stack_top, struct sys_pt_regs *regs_out)
{
	char *frame;
	unsigned long val;
	int i, n;

	n = sizeof(signal_regs.regs)/sizeof(signal_regs.regs[0]);
	for(i = 0; i < n; i++){
		regs_out->regs[i] = signal_regs.regs[i];
		if(signal_regs_mask.regs[i]) 
			regs_out->regs[i] += stack_top;
	}

	frame = (char *) (stack_top - signal_stack_size);
	for(i = 0; i < signal_stack_size;){
		if(signal_stack_mask[i] == 0){
			frame[i] = signal_stack[i];
			i++;
		}
		else {
			val = *((unsigned long *) &signal_stack[i]);
			val += stack_top;
			*((unsigned long *) &frame[i]) = val;
			i += sizeof(unsigned long);
		}
	}
}

void set_sigstack(void *sig_stack, int size)
{
	stack_t stack;

	stack.ss_sp = (__ptr_t) sig_stack;
	stack.ss_flags = 0;
	stack.ss_size = size - sizeof(void *);
	if(sigaltstack(&stack, NULL) != 0)
		panic("sigaltstack failed");
}

void set_handler(int sig, void (*handler)(int), int flags, ...)
{
	struct sigaction action;
	va_list ap;
	int mask;

	va_start(ap, flags);
	action.sa_handler = handler;
	sigemptyset(&action.sa_mask);
	while((mask = va_arg(ap, int)) != -1){
		sigaddset(&action.sa_mask, mask);
	}
	action.sa_flags = flags;
	action.sa_restorer = NULL;
	if(sigaction(sig, &action, NULL) < 0)
		panic("sigaction failed");
}

void change_sig(int signal, int on)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, signal);
	sigprocmask(on ? SIG_UNBLOCK : SIG_BLOCK, &sigset, NULL);
}

void signal_handler(void *task, unsigned long h, int sig)
{
	void (*handler)(int, struct sigcontext);
	void *regs;
	unsigned long cr2;
	int err;
	UM_ALLOCATE_SC(sc);

	regs = process_state(task, &cr2, &err);
	fill_in_sigcontext(&sc, regs, cr2, err);
	handler = (void (*)(int, struct sigcontext)) h;
	(*handler)(sig, sc);
}

static void change_signals(int type)
{
	sigset_t mask;

	sigemptyset(&mask);
	if(type == SIG_BLOCK)
		timer_on = 0;
	else {
		timer_on = 1;
		sigaddset(&mask, SIGVTALRM);
		sigaddset(&mask, SIGALRM);
	}
	sigaddset(&mask, SIGIO);
	if(sigprocmask(type, &mask, NULL) < 0)
		panic("Failed to change signal mask - errno = %d", errno);
}

void block_signals(void)
{
	change_signals(SIG_BLOCK);
}

void unblock_signals(void)
{
	change_signals(SIG_UNBLOCK);
}

#define SIGIO_BIT 0
#define SIGVTALRM_BIT 1

static int enable_mask(sigset_t *mask)
{
	int sigs;

	sigs = sigismember(mask, SIGIO) ? 0 : 1 << SIGIO_BIT;
	sigs |= sigismember(mask, SIGVTALRM) ? 0 : 1 << SIGVTALRM_BIT;
	sigs |= sigismember(mask, SIGALRM) ? 0 : 1 << SIGVTALRM_BIT;
	if(timer_on) sigs |= 1 << SIGVTALRM_BIT;
	return(sigs);
}

int set_signals(int enable)
{
	sigset_t mask;
	int ret;

	sigemptyset(&mask);
	if(enable & (1 << SIGIO_BIT)) sigaddset(&mask, SIGIO);
	if(enable & (1 << SIGVTALRM_BIT)){
		timer_on = 1;
		sigaddset(&mask, SIGVTALRM);
		sigaddset(&mask, SIGALRM);
	}
	if(sigprocmask(SIG_UNBLOCK, &mask, &mask) < 0)
		panic("Failed to enable signals");
	ret = enable_mask(&mask);
	sigemptyset(&mask);
	if((enable & (1 << SIGIO_BIT)) == 0) sigaddset(&mask, SIGIO);
	if((enable & (1 << SIGVTALRM_BIT)) == 0){
		timer_on = 0;
	}
	if(sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
		panic("Failed to block signals");
	return(ret);
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
