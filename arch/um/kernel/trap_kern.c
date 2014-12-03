/* 
 * Copyright (C) 2000, 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/kernel.h"
#include "linux/sched.h"
#include "linux/mm.h"
#include "linux/spinlock.h"
#include "linux/config.h"
#include "linux/init.h"
#include "asm/semaphore.h"
#include "asm/pgtable.h"
#include "asm/pgalloc.h"
#include "asm/a.out.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"
#include "chan.h"
#include "debug.h"
#include "mconsole_kern.h"

extern int nsyscalls;

unsigned long segv(unsigned long address, unsigned long ip, int is_write, 
		   int is_user)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	struct siginfo si;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long page;
	int ok;

	if((address >= start_vm) && (address < end_vm)){
		flush_tlb_kernel_vm();
		return(0);
	}
	if(mm == NULL) panic("Segfault with no mm");
	down_read(&mm->mmap_sem);
	vma = find_vma(mm, address);
	ok = 1;
	if(!vma) ok = 0;
	else if(is_write && !(vma->vm_flags & VM_WRITE)) ok = 0;
	else if(vma->vm_start > address){
		if((vma->vm_flags & VM_STACK_FLAGS) != VM_STACK_FLAGS) ok = 0;
		else if(expand_stack(vma, address)) ok = 0;
	}
	if(!ok){
		if (current->thread.fault_catcher != NULL) {
			current->thread.fault_addr = (void *) address;
			up_read(&mm->mmap_sem);
			do_longjmp(current->thread.fault_catcher);
		}
		else if(current->thread.fault_addr != NULL){
			panic("fault_addr set but no fault catcher");
		}
		if(!is_user) 
			panic("Kernel mode fault at addr 0x%lx, ip 0x%lx", 
			      address, ip);
		si.si_signo = SIGSEGV;
		si.si_code = SEGV_ACCERR;
		si.si_addr = (void *) address;
		current->thread.cr2 = address;
		current->thread.err = is_write;
		force_sig_info(SIGSEGV, &si, current);
		up_read(&mm->mmap_sem);
		return(0);
	}
	page = address & PAGE_MASK;
	if(page == (unsigned long) current + PAGE_SIZE)
		panic("Kernel stack overflow");
	pgd = pgd_offset(mm, page);
	pmd = pmd_offset(pgd, page);
	do {
 survive:
		switch (handle_mm_fault(mm, vma, address, is_write)) {
		case 1:
			current->min_flt++;
			break;
		case 2:
			current->maj_flt++;
			break;
		default:
			if (current->pid == 1) {
				current->policy |= SCHED_YIELD;
				schedule();
				goto survive;
			}
			/* Fall through to bad area case */
		case 0:
			up_read(&mm->mmap_sem);
			force_sig(SIGBUS, current);
			return(0);
		}
		pte = pte_offset(pmd, page);
		pte_mkyoung(*pte);
		if(is_write) pte_mkdirty(*pte);
	} while(is_write && !pte_write(*pte));
	if(is_write && !pte_write(*pte)) panic("page not writeable");
	if(!is_write && !pte_present(*pte)){
		printk("Page disappeared while handling fault");
		force_sigbus();
	}
	flush_tlb_page(vma, page);
	up_read(&mm->mmap_sem);
	return(0);
}

void bad_segv(unsigned long address, unsigned long ip, int is_write)
{
	struct siginfo si;

	printk(KERN_ERR "Unfixable SEGV in '%s' (pid %d) at 0x%lx "
	       "(ip 0x%lx)\n", current->comm, current->pid, address, ip);
	si.si_signo = SIGSEGV;
	si.si_code = SEGV_ACCERR;
	si.si_addr = (void *) address;
	current->thread.cr2 = address;
	current->thread.err = is_write;
	force_sig_info(SIGSEGV, &si, current);
}

void relay_signal(int sig, void *sc, int usermode)
{
	force_sig(sig, current);
}

void trap_init(void)
{
}

spinlock_t trap_lock = SPIN_LOCK_UNLOCKED;

void lock_trap(void)
{
	spin_lock(&trap_lock);
}

void unlock_trap(void)
{
	spin_unlock(&trap_lock);
}

extern int debugger_pid;
extern int debugger_fd;

#ifdef CONFIG_PT_PROXY

void debugger_signal(int status, pid_t pid)
{
	debugger_proxy(status, pid);
}

void child_signal(pid_t pid, int status)
{
	child_proxy(pid, status);
}

struct io_chan gdb_chan = XTERM_IO_CHAN_INIT(0, "", 1, INIT_STATIC);
char *gdb_init = "xterm";

static void exit_debugger_cb(void *unused)
{
	if(debugger_pid != -1){
		if(gdb_pid != -1){
			if(ptrace(PTRACE_DETACH, debugger_pid, 0, 0) < 0)
				printk("Detaching debugger failed - "
				       "errno = %d\n", errno);
			gdb_pid = -1;
		}
		else {
			kill(debugger_pid, SIGKILL);
			kill(debugger_pid, SIGCONT);
			while(waitpid(debugger_pid, NULL, WNOHANG) > 0) ;
		}
		debugger_pid = -1;
	}
	close_chan_pair(&gdb_chan);
}

static void exit_debugger(void)
{
	tracing_cb(exit_debugger_cb, NULL);
}

__exitcall(exit_debugger);

static void gdb_announce(char *dev_name, int dev)
{
	printf("gdb assigned device '%s'\n", dev_name);
}

static struct chan_opts opts = {
	announce: 	gdb_announce,
	dev:		-1,
	xterm_title:	"UML kernel debugger",
	raw_pty:	0
};

struct gdb_data {
	char *str;
	int err;
};

static void config_gdb_cb(void *arg)
{
	struct gdb_data *data = arg;
	int err, pid;

	data->err = -1;
	if(debugger_pid != -1) exit_debugger_cb(NULL);
	if(!strncmp(data->str, "pid,", strlen("pid,"))){
		data->str += strlen("pid,");
		pid = simple_strtoul(data->str, NULL, 0);
		debugger_pid = attach_debugger(current_task->thread.extern_pid,
					       pid, 0);
		if(debugger_pid != -1){
			data->err = 0;
			gdb_pid = pid;
		}
		return;
	}
	gdb_chan = ((struct io_chan) XTERM_IO_CHAN_INIT(0, "", 1, 
							INIT_STATIC));
	err = parse_chan_pair(data->str, 0, &gdb_chan, INIT_ONE, &opts);
	if(err) return;
	data->err = 0;
	debugger_pid = start_debugger(linux_prog, 0, 0, &debugger_fd);
	init_proxy(debugger_pid, 0, 0);
}

int gdb_config(char *str)
{
	struct gdb_data data;

	if(*str++ != '=') return(-1);
	data.str = str;
	tracing_cb(config_gdb_cb, &data);
	return(data.err);
}

void remove_gdb_cb(void *unused)
{
	exit_debugger_cb(NULL);
}

int gdb_remove(char *unused)
{
	tracing_cb(remove_gdb_cb, NULL);
	return(0);
}

static struct mc_device gdb_mc = {
	name:		"gdb",
	config:		gdb_config,
	remove:		gdb_remove,
};

int gdb_mc_init(void)
{
	mconsole_register_dev(&gdb_mc);
	return(0);
}

__initcall(gdb_mc_init);

void signal_usr1(int sig)
{
	if(debugger_pid != -1){
		printk("The debugger is already running\n");
		return;
	}
	if(parse_chan_pair(gdb_init, 0, &gdb_chan, INIT_ONE, &opts)) return;
	debugger_pid = start_debugger(linux_prog, 0, 0, &debugger_fd);
	init_proxy(debugger_pid, 0, 0);
}

int init_ptrace_proxy(int idle_pid, int startup, int stop)
{
	int pid, status;

	if(parse_chan_pair(gdb_init, 0, &gdb_chan, INIT_ONE, &opts)){
		ptrace(PTRACE_CONT, idle_pid, 0, 0);
		return(-1);
	}
	pid = start_debugger(linux_prog, startup, stop, &debugger_fd);
	status = wait_for_stop(idle_pid, SIGSTOP, PTRACE_CONT);
 	if(pid < 0){
		ptrace(PTRACE_CONT, idle_pid, 0, 0);
		return(-1);
	}
	init_proxy(pid, 1, status);
	return(pid);
}

int attach_debugger(int idle_pid, int pid, int stop)
{
	int status = 0;

	if(ptrace(PTRACE_ATTACH, pid, 0, 0) < 0){
		printf("Failed to attach pid %d, errno = %d\n", pid, errno);
		return(-1);
	}
	if(stop) status = wait_for_stop(idle_pid, SIGSTOP, PTRACE_CONT);
	init_proxy(pid, 1, status);
	return(pid);
}

#else

void debugger_signal(int status, pid_t pid){ }
void child_signal(pid_t pid, int status){ }
int init_ptrace_proxy(int idle_pid, int startup, int stop)
{
	printk("debug requested when CONFIG_PT_PROXY is off\n");
	wait_for_stop(idle_pid, SIGSTOP, PTRACE_CONT);
	ptrace(PTRACE_CONT, idle_pid, 0, 0);
	return(-1);
}

void signal_usr1(int sig)
{
	printk("debug requested when CONFIG_PT_PROXY is off\n");
}

int attach_debugger(int idle_pid, int pid, int stop)
{
	printk(KERN_ERR "attach_debugger called when CONFIG_PT_PROXY "
	       "is off\n");
	return(-1);
}

int config_gdb(char *str)
{
	return(-1);
}
 
int remove_gdb(void)
{
	return(-1);
}

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
