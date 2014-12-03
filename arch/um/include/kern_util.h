/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __KERN_UTIL_H__
#define __KERN_UTIL_H__

#include "sysdep/ptrace.h"

#ifndef	_UNISTD_H
extern int write(int, const char *, int);
#endif

extern int ncpus;
extern char *linux_prog;
extern char *gdb_init;
extern int kmalloc_ok;

#define ROUND_DOWN(addr) ((void *)(((unsigned long) addr) & PAGE_MASK))
#define ROUND_UP(addr) ROUND_DOWN(((unsigned long) addr) + PAGE_SIZE - 1)

extern int kernel_fork(unsigned long flags, int (*fn)(void *), void * arg);
extern unsigned long stack_sp(unsigned long page);
extern int kernel_thread_proc(void *data);
extern long execute_syscall(struct sys_pt_regs regs);
extern void syscall_segv(int sig);
extern int current_pid(void);
extern void set_init_pid(int pid);
extern unsigned long alloc_stack(void);
extern int do_signal(unsigned long *error, int *again_out);
extern int is_stack_fault(unsigned long sp);
extern unsigned long segv(unsigned long address, unsigned long ip, 
			  int is_write, int is_user);
extern int set_user_thread(void *task, int on, int restore_regs, 
			   int protect_mem);
extern void syscall_ready(void);
extern void set_tracing(void *t, int tracing);
extern int is_tracing(void *task);
extern int segv_syscall(void);
extern void ret_from_sys_call(void *t);
extern void kern_finish_exec(void *task, int new_pid, unsigned long stack);
extern int page_size(void);
extern int page_mask(void);
extern int need_finish_fork(void);
extern int do_proc_op(void *t, int proc_id);
extern void free_stack(unsigned long stack);
extern void add_input_request(int op, void (*proc)(int), void *arg);
extern int sys_execve(char *file, char **argv, char **env);
extern char *current_cmd(void);
extern void timer_handler(int sig, void *sc, int usermode);
extern int set_signals(int enable);
extern void force_sigbus(void);
extern int pid_to_processor_id(int pid);
extern void block_signals(void);
extern void unblock_signals(void);
extern void deliver_signals(void *t);
extern void lock_syscall(void);
extern void unlock_syscall(void);
extern void lock_trap(void);
extern void unlock_trap(void);
extern void lock_pid(void);
extern void unlock_pid(void);
extern int cpu_idle(void);
extern void finish_fork(void);
extern void *get_current_task(void);
extern void paging_init(void);
extern unsigned long um_virt_to_phys(void *t, unsigned long addr);
extern void init_flush_vm(void);
extern void *process_state(void *t, unsigned long *cr2_out, int *err_out);
extern struct sys_pt_regs *syscall_state(void *t, void **stack_out, 
					 int *size_out);
extern void syscall_trace(void);
extern int hz(void);
extern int switching_modes(void *t);
extern void idle_timer(void);
extern unsigned int do_IRQ(int irq, int user_mode);
extern int external_pid(void *t);
extern void boot_timer_handler(int sig);
extern void interrupt_end(void);
extern void tracing_reboot(void);
extern void tracing_halt(void);
extern void tracing_cb(void (*proc)(void *), void *arg);
extern void debugger_signal(int status, int pid);
extern void child_signal(int pid, int status);
extern int init_ptrace_proxy(int idle_pid, int startup, int stop);
extern void check_stack_overflow(void *ptr);
extern void relay_signal(int sig, void *sc, int usermode);
extern int singlestepping(void *t);
extern void not_implemented(void);
extern void setup_kernel_stack(void);
extern void set_syscall_regs(void *t);
extern void finish_fork_handler(int sig);
extern int user_context(unsigned long sp);
extern void timer_irq(int user_mode);
extern void set_repeat_syscall(int again);
extern int get_repeat_syscall(void *t);
extern void set_sigreturn_syscall(int syscall);
extern void force_flush_all(void);
extern void unprotect_stack(unsigned long stack);
extern void kern_start_exec(int new_pid);
extern void do_exitcalls(void);
extern int get_restore_regs(void *t);
extern int attach_debugger(int idle_pid, int pid, int stop);
extern void *round_up(unsigned long addr);
extern void *round_down(unsigned long addr);
extern void bad_segv(unsigned long address, unsigned long ip, int is_write);
extern int config_gdb(char *str);
extern int remove_gdb(void);
extern char *uml_strdup(char *string);
extern void unprotect_kernel_mem(void);
extern void protect_kernel_mem(void);
extern unsigned long get_kmem_end(void);
extern void set_kmem_end(unsigned long);
extern void *diff_stacks(int size, char *stack1, struct sys_pt_regs *regs1, 
			 char *stack2, struct sys_pt_regs *regs2, 
			 char **mask_out, struct sys_pt_regs *regs_out, 
			 struct sys_pt_regs *regs_out_mask);
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
