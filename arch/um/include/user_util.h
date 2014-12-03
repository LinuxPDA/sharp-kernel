/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __USER_UTIL_H__
#define __USER_UTIL_H__

#include "asm/types.h"
#include "sysdep/ptrace.h"

extern int grantpt(int __fd);
extern int unlockpt(int __fd);
extern char *ptsname(int __fd);

#ifndef	_UNISTD_H
extern int write(int, const char *, int);
#endif

enum { OP_NONE, OP_EXEC, OP_SWITCH, OP_THREAD, OP_FORK, OP_FORK_FINISH,
       OP_TRACE_ON, OP_TRACE_OFF, OP_REBOOT, OP_HALT, OP_CB };

struct cpu_task {
	int pid;
	void *task;
};

extern struct cpu_task cpu_tasks[];

extern unsigned long low_physmem;
extern unsigned long high_physmem;
extern unsigned long physmem;
extern unsigned long end_vm;
extern unsigned long start_vm;

extern int tracing_pid;
extern int honeypot;

extern char host_info[];

extern char saved_command_line[];
extern char command_line[];

extern int gdb_pid;

extern void *open_maps(void);
extern int read_map(void *fd, unsigned long *start_out, 
		    unsigned long *end_out, char *r_out, char *w_out, 
		    char *x_out, char *p_out);
extern void close_maps(void *fd);
extern unsigned long get_brk(void);
extern __u64 file_size(char *file);
extern void stop(void);
extern int proc_start_thread(unsigned long ip, unsigned long sp);
extern void stack_protections(unsigned long address);
extern void task_protections(unsigned long address);
extern void abandon_proc_space(int (*proc)(void *), unsigned long sp);
extern int signals(int (*init_proc)(void *), void *sp);
extern void map(unsigned long virt, void *p, unsigned long len, 
		int r, int w, int x);
extern int unmap(unsigned long address, unsigned long len);
extern void protect(unsigned long addr, unsigned long len, int r, int w, 
		    int x);
extern void stop_pid(int pid);
extern void kill_pid(int pid);
extern void usr1_pid(int pid);
extern void cont_pid(int pid);
extern int __personality(int);
extern int syscall_handler(void *unused);
extern int do_syscall(void *task, int pid);
extern int wait_for_stop(int pid, int sig, int cont_type);
extern void *add_signal_handler(int sig, void (*handler)(int));
extern void signal_init(void);
extern void finish_exec(int old_pid, int new_pid, struct sys_pt_regs *regs);
extern int start_fork_tramp(unsigned long sig_stack, unsigned long temp_stack,
			    int clone_vm, int (*tramp)(void *));
extern void trace_myself(void);
extern void timer(void);
extern void get_profile_timer(void);
extern void disable_profile_timer(void);
extern void set_timers(int set_signal);
extern int clone_and_wait(int (*fn)(void *), void *arg, void *sp, int flags);
extern int input_loop(void);
extern void continue_execing_proc(int pid);
extern int linux_main(int argc, char **argv);
extern void remap_data(void *segment_start, void *segment_end);
extern void set_cmdline(char *cmd);
extern void continue_fork(void *task, int pid, struct sys_pt_regs *regs);
extern void input_cb(void (*proc)(void *), void *arg, int arg_len);
extern void setup_input(void);
extern int exit_kernel(int pid, void *task);
extern int get_pty(void);
extern void save_signal_state(int *sig_ptr);
extern void fill_in_sigcontext(void *sc, struct sys_pt_regs *regs,
			       unsigned long cr2, int err);
extern int activate_fd(int irq, int fd, void *dev_id);
extern void reactivate_fd(int fd);
extern void free_irq_fd(void *dev_id);
extern void forward_interrupts(int pid);
extern void init_irq_signals(int on_sigstack);
extern void *um_kmalloc(int size);
extern int raw(int fd, int complain);
extern void cooked(int fd);
extern int switcheroo(int fd, int prot, void *from, void *to, int size);
extern void idle_sleep(int secs);
extern int get_one_stack(int (*proc)(void *), void *arg, char **stack_out, 
			 struct sys_pt_regs *regs_out);
extern void setup_machinename(char *machine_out);
extern void setup_hostinfo(void);
extern void add_arg(char *cmd_line, char *arg);
extern void sigio_handler(int sig, void *sc, int usermode);
extern void init_new_thread(void *sig_stack, void (*usr1_handler)(int));
extern void start_exec(int old_pid, int new_pid, int *error, 
		       struct sys_pt_regs *regs);
extern void attach_process(int pid);
extern void calc_sigframe_size(void);
extern int fork_tramp(void *sig_stack);
extern void do_exec(int old_pid, int new_pid);
extern void tracer_panic(char *msg, ...);
extern void close_fd(int);
extern char *get_umid(void);
extern void ptrace_cont_pid(int pid);
extern void create_pid_file(char *name);
extern int load_initrd(char *filename, void *buf, int size);
extern int ptrace_getregs(long pid, struct sys_pt_regs *regs_out);
extern int ptrace_setregs(long pid, struct sys_pt_regs *regs_in);
extern void do_longjmp(void *p);
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
