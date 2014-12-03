/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/config.h"
#include "linux/sched.h"
#include "linux/interrupt.h"
#include "linux/mm.h"
#include "linux/slab.h"
#include "linux/utsname.h"
#include "linux/fs.h"
#include "linux/utime.h"
#include "linux/smp_lock.h"
#include "linux/module.h"
#include "linux/init.h"
#include "asm/unistd.h"
#include "asm/mman.h"
#include "asm/segment.h"
#include "asm/stat.h"
#include "asm/pgtable.h"
#include "asm/processor.h"
#include "asm/pgalloc.h"
#include "asm/spinlock.h"
#include "asm/uaccess.h"
#include "asm/user.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"
#include "signal_kern.h"
#include "signal_user.h"

struct cpu_task cpu_tasks[NR_CPUS] = { [0 ... NR_CPUS - 1] = { -1, NULL } };

static struct task_struct *get_task(int pid, int require)
{
	struct task_struct *task, *ret;

	ret = NULL;
	read_lock(&tasklist_lock);
	for_each_task(task){
		if(task->pid == pid){
			ret = task;
			break;
		}
	}
	read_unlock(&tasklist_lock);
	if(require && (ret == NULL)) panic("get_task couldn't find a task\n");
	return(ret);
}

int external_pid(void *t)
{
	struct task_struct *task = t;
	if(task == NULL) task = current;
	return(task->thread.extern_pid);
}

void free_stack(unsigned long stack)
{
	free_page(stack);
}

void set_init_pid(int pid)
{
	init_task.thread.extern_pid = pid;
}

int set_user_thread(void *t, int on, int restore_regs, int protect_mem)
{
	struct task_struct *task;
	int ret;

	if(t == NULL) task = current;
	else task = t;
	if(on == task->thread.tracing) return(on);
	ret = task->thread.tracing;
	task->thread.request.op = on ? OP_TRACE_ON : OP_TRACE_OFF;
	task->thread.request.u.tracing.restore_regs = restore_regs;
	if(on && protect_mem) protect_kernel_mem();
	usr1_pid(getpid());
	return(ret);
}

void set_tracing(void *task, int tracing)
{
	((struct task_struct *) task)->thread.tracing = tracing;
}

int is_tracing(void *t)
{
	struct task_struct *task;

	if(t == NULL) task = current;
	else task = t;
	return(task->thread.tracing);
}

extern void schedule_tail(struct task_struct *prev);

static int new_thread_proc(void *t)
{
	struct task_struct *task;
	int (*fn)(void *), pid;
	void *arg;

	task = t;
	trace_myself();
	init_new_thread(NULL, NULL);
	pid = getpid();
	fn = task->thread.request.u.thread.proc;
	arg = task->thread.request.u.thread.arg;
	task->thread.extern_pid = pid;
	stop_pid(pid);
	set_cmdline("(kernel thread)");
	flush_tlb_all();
	if(current->thread.request.u.cswitch.from != NULL)
		schedule_tail(current->thread.request.u.cswitch.from);
	(*fn)(arg);
	do_exit(0);
	return(0);
}

unsigned long alloc_stack(void)
{
	unsigned long page;

	if((page = __get_free_page(GFP_KERNEL)) == 0)
		panic("Couldn't allocate new stack");
	stack_protections(page);
	return(page);
}

extern int inited_cpus;

static int start_kernel_thread(struct task_struct *task, int (*fn)(void *),
			       void *arg, int cpu)
{
	int extern_pid;
	unsigned long sp;

	sp = ((unsigned long) task) + 4 * PAGE_SIZE - sizeof(void *);
	task->thread.request.u.thread.proc = fn;
	task->thread.request.u.thread.arg = arg;
	task->thread.extern_pid = -1;
	extern_pid = clone_and_wait(new_thread_proc, task, (void *) sp, 
				    CLONE_FILES | SIGCHLD);
	if(task->thread.extern_pid == -1) 
		tracer_panic("task didn't set its pid");
	task->mm = NULL;
	task->active_mm = NULL;
#ifdef CONFIG_SMP
	if(cpu != -1){
		cpu_tasks[cpu].pid = extern_pid;
		cpu_tasks[cpu].task = task;
		inited_cpus++;
		init_tasks[cpu] = task;
		cpu_number_map[cpu] = cpu;
		task->processor = cpu;
		cont_pid(extern_pid);
	}
#endif
	return(extern_pid);
}

static int kernel_thread1(int (*fn)(void *), void * arg, unsigned long flags, 
			  int cpu, int *extern_pid_out)
{
	struct task_struct *new_task;
	int pid, extern_pid;

	pid = do_fork(CLONE_VM | flags, 0, NULL, 0);
	if(pid < 0) panic("do_fork failed in kernel_thread");
	new_task = get_task(pid, 1);
	current->thread.request.op = OP_THREAD;
	current->thread.request.u.thread.proc = fn;
	current->thread.request.u.thread.arg = arg;
	current->thread.request.u.thread.flags = flags;
	current->thread.request.u.thread.new_task = new_task;
	current->thread.request.u.thread.cpu = cpu;
	usr1_pid(getpid());
	extern_pid = current->thread.request.u.thread.new_pid;
	if(extern_pid < 0){
		printk("Kernel thread failed : errno = %d", -extern_pid);
		return(extern_pid);
	}
	if(extern_pid_out != NULL) *extern_pid_out = extern_pid;
	current->thread.request.u.cswitch.from = NULL;
	return(pid);
}

int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	return(kernel_thread1(fn, arg, flags, -1, NULL));
}

void switch_mm(struct mm_struct *prev, struct mm_struct *next, 
	       struct task_struct *tsk, unsigned cpu)
{
	if (prev != next) 
		clear_bit(cpu, &prev->cpu_vm_mask);
	set_bit(cpu, &next->cpu_vm_mask);
}

void *_switch_to(void *prev, void *next)
{
	struct task_struct *from, *to;

	from = prev;
	to = next;
	current->thread.request.op = OP_SWITCH;
	current->thread.request.u.cswitch.to = next;
	forward_interrupts(to->thread.extern_pid);
	block_signals();
	usr1_pid(getpid());
	flush_tlb_all();
	unblock_signals();
	return(current->thread.request.u.cswitch.from);
}

void ret_from_sys_call(void *t)
{
	struct task_struct *task;

	task = t;
	if(task == NULL) task = current;
	if(task->need_resched) schedule();
	if(task->sigpending != 0) do_signal(NULL, NULL);
}

void release_thread(struct task_struct *task)
{
}

void exit_thread(void)
{
	unprotect_stack((unsigned long) current);
}

char *generic_stack = NULL;
char *generic_stack_mask = NULL;
int generic_stack_size = 0;
struct sys_pt_regs generic_regs;
struct sys_pt_regs generic_regs_mask;

void set_syscall_regs(void *t)
{
	struct sys_pt_regs *regs;
	int i;
	struct task_struct *task = t;

	regs = &task->thread.syscall_regs;
	for(i = 0; i < sizeof(regs->regs)/sizeof(regs->regs[0]); i++){
		task->thread.syscall_regs.regs[i] = generic_regs.regs[i];
		if(generic_regs_mask.regs[i]) 
			task->thread.syscall_regs.regs[i] += 
				task->thread.kernel_stack + 2 * PAGE_SIZE;
	}
}

int copy_thread(int nr, unsigned long clone_flags, unsigned long sp,
		unsigned long stack_top, struct task_struct * p, 
		struct pt_regs * regs)
{
	int new_pid, clone_vm;
	unsigned long stack;

	p->thread = (struct thread_struct) INIT_THREAD;
	p->thread.kernel_stack = (unsigned long) p + 2 * PAGE_SIZE;
	p->thread.tracing = current->thread.forking;
	set_syscall_regs(p);
	if(current->thread.forking){
		p->thread.tracing = 0;
		stack = alloc_stack();
		clone_vm = (p->mm == current->mm);
		new_pid = start_fork_tramp(p->thread.kernel_stack, stack,
					   clone_vm, fork_tramp);
		if(new_pid < 0){
			printk("copy_thread : clone failed with errno = %d",
			       -new_pid);
			return(new_pid);
		}
		current->thread.request.op = OP_FORK;
		current->thread.request.u.fork.pid = new_pid;
		usr1_pid(getpid());
		p->thread.process_regs = current->thread.process_regs;
		UM_SET_SYSCALL_RETURN(&p->thread.process_regs, 0);
		if(sp != 0) UM_SP(&p->thread.process_regs) = sp;
		p->thread.extern_pid = new_pid;
		p->thread.request.op = OP_FORK_FINISH;
		p->thread.request.u.fork_finish.stack = stack;
	}
	current->need_resched = 1;
	return(0);
}

void tracing_reboot(void)
{
	current->thread.request.op = OP_REBOOT;
	usr1_pid(getpid());
}

void tracing_halt(void)
{
	current->thread.request.op = OP_HALT;
	usr1_pid(getpid());
}

void tracing_cb(void (*proc)(void *), void *arg)
{
	if(getpid() == tracing_pid){
		(*proc)(arg);
	}
	else {
		current->thread.request.op = OP_CB;
		current->thread.request.u.cb.proc = proc;
		current->thread.request.u.cb.arg = arg;
		usr1_pid(getpid());
	}
}

struct {
	struct task_struct *from;
	struct task_struct *to;
	int processor;
} switch_record[1024];

int switch_index = 0;

int do_proc_op(void *t, int proc_id)
{
	struct task_struct *task, *to;
	struct thread_struct *thread;
	int op, pid;

	task = t;
	thread = &task->thread;
	op = thread->request.op;
	switch(op){
	case OP_NONE:
	case OP_TRACE_ON:
	case OP_TRACE_OFF:
		break;
	case OP_EXEC:
		do_exec(thread->extern_pid, thread->request.u.exec.pid);
		break;
	case OP_SWITCH:
		if((task->state == TASK_ZOMBIE) && 
		   (thread->extern_pid != -1))
			kill_pid(thread->extern_pid);
		to = thread->request.u.cswitch.to;
		switch_record[switch_index].from = task;
		switch_record[switch_index].to = to;
		switch_record[switch_index++].processor = to->processor;
		if(switch_index == 1024) switch_index = 0;
#ifdef CONFIG_SMP
		cpu_tasks[proc_id].task = to;
		cpu_tasks[proc_id].pid = to->thread.extern_pid;
		if(cpu_tasks[0].pid == cpu_tasks[1].pid)
			tracer_panic("Scheduled a process on two processors");
#else
		current = to;
#endif
		if(to->thread.request.op == OP_FORK_FINISH){
			to->thread.request.u.fork_finish.from = task;
			continue_fork(to, to->thread.extern_pid, 
				      &to->thread.process_regs);
			to->thread.request.op = OP_NONE;
			set_tracing(to, 1);
		}
		else to->thread.request.u.cswitch.from = task;
		cont_pid(to->thread.extern_pid);
		break;
	case OP_THREAD:
		pid = start_kernel_thread(thread->request.u.thread.new_task,
					  thread->request.u.thread.proc,
					  thread->request.u.thread.arg,
					  thread->request.u.thread.cpu);
		thread->request.u.thread.new_pid = pid;
		break;
	case OP_FORK:
		attach_process(thread->request.u.fork.pid);
		break;
	case OP_CB:
		(*thread->request.u.cb.proc)(thread->request.u.cb.arg);
		break;
	case OP_REBOOT:
	case OP_HALT:
		break;
	default:
		tracer_panic("Bad op in do_proc_op");
		break;
	}
	thread->request.op = OP_NONE;
	return(op);
}

unsigned long stack_sp(unsigned long page)
{
	return(page + PAGE_SIZE - sizeof(void *));
}

int current_pid(void)
{
	return(current->pid);
}

static void do_idle(void)
{
	idle_timer();
	
	while(1){
		/* endless idle loop with no priority at all */
		current->nice = 20;
		current->counter = -100;

		/*
		 * although we are an idle CPU, we do not want to
		 * get into the scheduler unnecessarily.
		 */
		if (current->need_resched) {
			schedule();
			check_pgt_cache();
		}
		idle_sleep(10);
	}
}

static int idle_proc(void *unused)
{
	del_from_runqueue(current);
	init_idle();
#ifdef CONFIG_SMP
	smp_num_cpus++;
#endif
	do_idle();
	return(0);
}

int cpu_idle(void)
{
	int i, pid;

	if(ncpus > 1){
		printk("Starting up other processors:\n");
		for(i=1;i<ncpus;i++){
			kernel_thread1(idle_proc, NULL, 0, i, &pid);
			printk("\t#%d - idle thread pid = %d\n", i, pid);
		}
	}
	do_idle();
	return(0);
}

int page_size(void)
{
	return(PAGE_SIZE);
}

int page_mask(void)
{
	return(PAGE_MASK);
}

unsigned long um_virt_to_phys(void *t, unsigned long addr)
{
	struct task_struct *task;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	task = t;
	if(task->mm == NULL) return(0xffffffff);
	pgd = pgd_offset(task->mm, addr);
	pmd = pmd_offset(pgd, addr);
	if(!pmd_present(*pmd)) return(0xffffffff);
	pte = pte_offset(pmd, addr);
	if(!pte_present(*pte)) return(0xffffffff);
	return((pte_val(*pte) & PAGE_MASK) + (addr & ~PAGE_MASK));
}

char *current_cmd(void)
{
#ifdef CONFIG_SMP
	return("(Unknown)");
#else
	unsigned long addr;

	if((addr = um_virt_to_phys(current, 
				   current->mm->arg_start)) == 0xffffffff) 
		return("(Unknown)");
	else return((char *) addr);
#endif
}

void force_sigbus(void)
{
	printk("Killing pid %d because of a lack of memory\n", current->pid);
	lock_kernel();
	sigaddset(&current->pending.signal, SIGBUS);
	recalc_sigpending(current);
	current->flags |= PF_SIGNALED;
	do_exit(SIGBUS | 0x80);
}

void finish_fork_handler(int sig)
{
	force_flush_all();
	if(current->mm != current->p_pptr->mm)
		protect(physmem, high_physmem - physmem, 1, 1, 0);
	task_protections((unsigned long) current);
	if(current->thread.request.u.fork_finish.from)
		schedule_tail(current->thread.request.u.fork_finish.from);
	free_page(current->thread.request.u.fork_finish.stack);
	change_sig(SIGUSR1, 1);
	unblock_signals();
	kill(getpid(), SIGSTOP);
}

void *get_current_task(void)
{
	return(current);
}

void *process_state(void *t, unsigned long *cr2_out, int *err_out)
{
	struct task_struct *task;

	if(t == NULL) task = current;
	else task = t;
	if(cr2_out) *cr2_out = task->thread.cr2;
	if(err_out) *err_out = task->thread.err;
	return(&task->thread.process_regs);
}

struct sys_pt_regs *syscall_state(void *t, void **stack_out, int *size_out)
{
	struct task_struct *task;

	task = t;
	*stack_out = generic_stack;
	*size_out = generic_stack_size;
	return(&task->thread.syscall_regs);
}

int get_repeat_syscall(void *t)
{
	struct task_struct *task;

	if(t == NULL) task = current;
	else task = t;
	return(task->thread.repeat_syscall);
}

void set_repeat_syscall(int again)
{
	current->thread.repeat_syscall = again;
}

void dump_thread(struct pt_regs *regs, struct user *u)
{
}

int switching_modes(void *t)
{
	struct task_struct *task;

	task = t;
	return(task->thread.request.op == OP_TRACE_OFF);
}

void enable_hlt(void)
{
	panic("enable_hlt");
}

void disable_hlt(void)
{
	panic("disable_hlt");
}

extern int signal_frame_size;

void interrupt_end(void)
{
	if(current->need_resched) schedule();
	do_signal(NULL, NULL);
}

void *um_kmalloc(int size)
{
	return(kmalloc(size, GFP_KERNEL));
}

unsigned long get_fault_addr(void)
{
	return((unsigned long) current->thread.fault_addr);
}

EXPORT_SYMBOL(get_fault_addr);

int singlestepping(void *t)
{
	struct task_struct *task;
	int ret;

	task = (struct task_struct *) t;
	ret = (task->ptrace & PT_DTRACE);
	task->ptrace &= ~PT_DTRACE;
	return(ret);
}

void not_implemented(void)
{
	printk("Something isn't implemented in here\n");
}

EXPORT_SYMBOL(not_implemented);

void *diff_stacks(int size, char *stack1, struct sys_pt_regs *regs1, 
		  char *stack2, struct sys_pt_regs *regs2, char **mask_out,
		  struct sys_pt_regs *regs_out, 
		  struct sys_pt_regs *regs_mask_out)
{
	char *mask;
	void *new;
	unsigned long s1, s2, n1, n2, off1, off2, i;

	s1 = (unsigned long) stack1;
	s2 = (unsigned long) stack2;
	new = malloc(size);
	mask = malloc(size);
	if((new == NULL) || (mask == NULL)){
		perror("diff_stacks : allocating new stack and mask");
		exit(1);
	}
	memset(mask, 0, size);
	for(i = PAGE_SIZE - size; i < PAGE_SIZE - sizeof(void *); i++){
		/* This is horribly word-length- and byte-order-dependent */
		n1 = stack1[i] | (stack1[i + 1] << 8) |
			(stack1[i + 2] << 16) | stack1[i + 3] << 24;
		n2 = stack2[i] | (stack2[i + 1] << 8) |
			(stack2[i + 2] << 16) | stack2[i + 3] << 24;

		/* Internal pointers have to be different on different
		 * stacks, and they have to point to the stack page.  If not,
		 * then keep looking.
		 */
		if((n1 == n2) || ((n1 & PAGE_MASK) != s1) || 
		   ((n2 & PAGE_MASK) != s2)) continue;

		/* The offsets have to be the same and they have to point
		 * within the used area of the stack.
		 */
		off1 = n1 & ~PAGE_MASK;
		off2 = n2 & ~PAGE_MASK;
		if((off1 == off2) && (off1 > PAGE_SIZE - size)){
			*((unsigned long *) &stack1[i]) -= s1 + PAGE_SIZE;
			mask[i - (PAGE_SIZE - size)] = 1;
			i += sizeof(unsigned long);
		}
	}
	memcpy((char *) new, &stack1[PAGE_SIZE - size], size);
	for(i = 0; i < sizeof(regs1->regs)/sizeof(regs1->regs[0]); i++){
		regs_out->regs[i] = regs1->regs[i];
		regs_mask_out->regs[i] = 0;
		if((regs1->regs[i] == regs2->regs[i]) || 
		   ((regs1->regs[i] & PAGE_MASK) != s1) || 
		   ((regs2->regs[i] & PAGE_MASK) != s2)) continue;
		off1 = regs1->regs[i] & ~PAGE_MASK;
		off2 = regs2->regs[i] & ~PAGE_MASK;
		if((off1 == off2) && (off1 >= PAGE_SIZE - size)){
			regs_out->regs[i] = regs1->regs[i] - s1 - PAGE_SIZE;
			regs_mask_out->regs[i] = 1;
		}
	}
	*mask_out = mask;
	return(new);
}

void setup_kernel_stack(void)
{
	struct sys_pt_regs regs1, regs2;
	char *stack1, *stack2;
	int size;

	size = get_one_stack(syscall_handler, NULL, &stack1, &regs1);
	if(get_one_stack(syscall_handler, NULL, &stack2, &regs2) != size){
		printf("setup_kernel_stack : differing stack sizes\n");
		exit(1);
	}

	generic_stack = diff_stacks(size, stack1, &regs1, stack2, &regs2, 
				    &generic_stack_mask, &generic_regs, 
				    &generic_regs_mask);
	generic_stack_size = size;

	if((munmap(stack1, PAGE_SIZE) < 0) || 
	   (munmap(stack2, PAGE_SIZE) < 0)){
		perror("setup_kernel_stack : unmapping temp stacks");
		exit(1);
	}
}

int user_context(unsigned long sp)
{
	return((sp & (PAGE_MASK << 1)) != current->thread.kernel_stack);
}

int get_restore_regs(void *t)
{
	struct task_struct *task = t;

	return(task->thread.request.u.tracing.restore_regs);
}

extern void remove_umid_dir(void);
__exitcall(remove_umid_dir);

extern exitcall_t __exitcall_begin, __exitcall_end;

void do_exitcalls(void)
{
	exitcall_t *call;

	call = &__exitcall_end;
	while (--call >= &__exitcall_begin)
		(*call)();
}

void *round_up(unsigned long addr)
{
	return(ROUND_UP(addr));
}

void *round_down(unsigned long addr)
{
	return(ROUND_DOWN(addr));
}

char *uml_strdup(char *string)
{
	char *new;

	new = kmalloc(strlen(string) + 1, GFP_KERNEL);
	if(new == NULL) return(NULL);
	strcpy(new, string);
	return(new);
}

void unprotect_kernel_mem(void)
{
	unsigned long start_stack, end_stack;

	if(current_task == &init_task) return;
	start_stack = (unsigned long) current + PAGE_SIZE;
	end_stack = (unsigned long) current + PAGE_SIZE * 4;
	protect(physmem, start_stack - physmem, 1, 1, 1);
	protect(end_stack, high_physmem - end_stack, 1, 1, 1);
}

void protect_kernel_mem(void)
{
	unsigned long start_stack, end_stack;

	if(current_task == &init_task) return;
	start_stack = (unsigned long) current + PAGE_SIZE;
	end_stack = (unsigned long) current + PAGE_SIZE * 4;
	protect(physmem, start_stack - physmem, 1, 0, 1);
	protect(end_stack, high_physmem - end_stack, 1, 0, 1);
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
