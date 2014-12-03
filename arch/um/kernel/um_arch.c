/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/config.h"
#include "linux/sched.h"
#include "linux/mm.h"
#include "linux/types.h"
#include "linux/tty.h"
#include "linux/init.h"
#include "linux/bootmem.h"
#include "linux/spinlock.h"
#include "linux/utsname.h"
#ifdef CONFIG_BLK_DEV_INITRD
#include "linux/blk.h"
#endif
#include "asm/page.h"
#include "asm/pgtable.h"
#include "asm/ptrace.h"
#include "asm/elf.h"
#include "asm/user.h"
#include "asm/delay.h"
#include "ubd_user.h"
#include "asm/current.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"
#include "mprot.h"
#include "mem_user.h"
#include "umid.h"

unsigned long _stext;

#define DEFAULT_COMMAND_LINE "root=/dev/ubd0"

unsigned long thread_saved_pc(struct thread_struct *thread)
{
	panic("Someone should implement thread_saved_pc");
	return(0);
}

/*
 * get_cpuinfo - Get information on one CPU for use by procfs.
 *
 *	Prints info on the next CPU into buffer.  Beware, doesn't check for
 *	buffer overflow.  Current implementation of procfs assumes that the
 *	resulting data is <= 1K.
 *
 * Args:
 *	buffer	-- you guessed it, the data buffer
 *	cpu_np	-- Input: next cpu to get (start at 0).  Output: Updated.
 *
 *	Returns number of bytes written to buffer.
 */

int get_cpuinfo(char *buffer, unsigned *cpu_np)
{
	char *p = buffer;
	unsigned n;

	/* No SMP at the moment, so just toggle 0/1 */
	n = *cpu_np;
	*cpu_np = 1;
	if (n != 0) {
		return (0);
	}
	p += sprintf(p, "processor\t: user-mode\n");
	p += sprintf(p, "bogomips\t: %lu.%02lu\n",
			loops_per_jiffy/(500000/HZ),
			(loops_per_jiffy/(5000/HZ)) % 100);
	p += sprintf(p, "host\t\t: %s\n", host_info);

	return(strlen(buffer));
}

pte_t * __bad_pagetable(void)
{
	panic("Someone should implement __bad_pagetable");
	return(NULL);
}

extern void start_kernel(void);

extern int debug;
extern int debug_stop;

static int start_kernel_proc(void *unused)
{
	int pid;

	block_signals();
	pid = getpid();
#ifdef CONFIG_SMP
	cpu_tasks[0].pid = pid;
	cpu_tasks[0].task = current;
	smp_num_cpus = 1;
#else
	current_task = &init_task_union.task;
#endif
	if(debug) stop_pid(getpid());
	start_kernel();
	return(0);
}

extern unsigned long high_physmem;

#ifdef CONFIG_HOST_2G_2G
#define START 0x60000000
#else
#define START 0xa0000000
#endif

unsigned long physmem;

unsigned long start_vm;
unsigned long end_vm;

int ncpus = 1;

#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)	((x) << PAGE_SHIFT)

static char *argv1_begin = NULL;
static char *argv1_end = NULL;

void set_cmdline(char *cmd)
{
	if(honeypot) return;
	strcpy(argv1_begin, "[");
	strncat(argv1_begin, cmd, argv1_end - argv1_begin - strlen("[]"));
	strcat(argv1_begin, "]");
	memset(argv1_begin + strlen(argv1_begin), '\0', 
	       argv1_end - argv1_begin - strlen(argv1_begin));  
}

static char *usage_string = 
"User Mode Linux v%s"
" available at http://user-mode-linux.sourceforge.net/\n\n"
"--help\n    Prints this message\n"
"--version\n    Gives the version number of the kernel\n"
"root=<file containing the root fs>\n"
"    This is actually used by the generic kernel in exactly the same\n"
"    way as in any other kernel. If you configure a number of block\n"
"    devices and want to boot off something other than ubd0, you \n"
"    would use something like:\n"
"        root=/dev/ubd5\n\n"
"mem=<Amount of desired ram>\n"
"    This controls how much \"physical\" memory the kernel allocates\n"
"    for the system. The size is specified as a number followed by\n"
"    one of 'k', 'K', 'm', 'M', which have the obvious meanings.\n"
"    This is not related to the amount of memory in the physical\n"
"    machine. It can be more, and the excess, if it's ever used, will\n"
"    just be swapped out.\n        Example: mem=64M\n\n"
#ifdef CONFIG_SMP
"ncpus=<# of desired CPUs>\n"
"    This tells an SMP kernel how many virtual processors to start.\n"
"    Currently, this has no effect because SMP isn't enabled.\n\n" 
#endif
"debugtrace\n"
"    Causes the tracing thread to pause until it is attached by a\n"
"    debugger and continued. This is mostly for debugging crashes\n"
"    early during boot, and should be pretty much obsoleted by\n"
"    the debug switch.\n\n"
"debug\n"
"    Starts up the kernel under the control of gdb. See the \n"
"    kernel debugging tutorial and the debugging session pages\n"
"    at http://user-mode-linux.sourceforge.net/ for more information\n\n"
"umn=<ip-address>\n"
"    This sets the ip address of the host side of the slip device \n"
"    that the umn device configures. This is necessary if you want\n"
"    to set up networking, but your local net isn't 192.168.0.x,\n"
"    or you want to run multiple virtual machines on a network,\n"
"    in which case, you need to assign different ip addresses to the\n"
"    different machines.\n\n"
"ubd<n>=<filename>\n"
"    This is used to associate a device with a file in the underlying\n"
"    filesystem. Usually, there is a filesystem in the file, but \n"
"    that's not required. Swap devices containing swap files can be\n"
"    specified like this. Also, a file which doesn't contain a\n"
"    filesystem can have its contents read in the virtual \n"
"    machine by running dd on the device. n must be in the range\n"
"    0 to 7. Appending an 'r' to the number will cause that device\n"
"    to be mounted read-only. For example ubd1r=./ext_fs\n\n"
"umid=<name>\n"
"    This is used to assign a unique identity to this UML machine\n"
"    Used for naming the pid file and console socket\n";

static void Usage(void)
{
	printf(usage_string, system_utsname.release);
	exit(0);
}

#ifdef CONFIG_BLK_DEV_INITRD
static void read_initrd(char *initrd)
{
	void *area;
	int size;

	size = file_size(initrd);
	if(size < 0) return;
	area = alloc_bootmem(size);
	if(area == NULL) return;
	if(load_initrd(initrd, area, size) == -1) return;
	initrd_start = (unsigned long) area;
	initrd_end = initrd_start + size;
}
#endif

extern unsigned long _stext, _etext, _sdata, _edata, __bss_start, _end;
extern int debug_trace;

int linux_main(int argc, char **argv)
{
	unsigned long start_pfn, end_pfn, bootmap_size;
	unsigned long physmem_size, virtmem_size;
	unsigned int i, have_root, add;
	char *retptr;
	void *sp;
	void *brk_start;
#ifdef CONFIG_BLK_DEV_INITRD
	char *initrd = NULL;
#endif

	remap_data(ROUND_DOWN(&_stext), ROUND_UP(&_etext));
	remap_data(ROUND_DOWN(&_sdata), ROUND_UP(&_edata));
	brk_start = sbrk(0);
	remap_data(ROUND_DOWN(&__bss_start), ROUND_UP(brk_start));

 	/* Start physical memory at least 4M after the current brk */
 	physmem = ROUND_4M(brk_start) + (1 << 22);
 
	/* Create fake command line from argv[]. */
	have_root = 0;
	physmem_size = 32 * 1024 * 1024;
	virtmem_size = physmem_size;
	for (i = 1; i < argc; i++){
		if((i == 1) && (argv[i][0] == ' ')) continue;
		add = 1;
		if(!strncmp(argv[i], "root=", strlen("root="))) have_root = 1;
		else if(!strncmp(argv[i], "mem=", strlen("mem="))) 
			physmem_size = memparse(argv[i] + strlen("mem="), 
						&retptr);
#ifdef CONFIG_SMP
		else if(!strncmp(argv[i], "ncpus=", strlen("ncpus=")))
			ncpus = strtoul(argv[i] + strlen("ncpus="), NULL, 10);
#endif
		else if(!strcmp(argv[i], "debugtrace")) debug_trace = 1;
		else if(!strncmp(argv[i], "debug", strlen("debug"))){
			debug = 1;
			debug_stop = 1;
			if(!strcmp(argv[i], "debug=go")){
				debug_stop = 0;
				add = 0;
			}
		}
#ifdef CONFIG_PT_PROXY
		else if(!strncmp(argv[i], "gdb=", strlen("gdb=")))
			gdb_init = &argv[i][strlen("gdb=")];
		else if(!strncmp(argv[i], "gdb-pid=", strlen("gdb-pid=")))
			gdb_pid = simple_strtoul(&argv[i][strlen("gdb-pid=")],
						 NULL, 0);
#endif
		else if(!strncmp(argv[i], "umid=", strlen("umid="))){
			set_umid(&argv[i][strlen("umid=")]);
		}
		else if(!strncmp(argv[i], "iomem=", strlen("iomem="))){
			parse_iomem(&argv[i][strlen("iomem=")]);
		}
		else if(!strcmp(argv[i], "honeypot")){
			honeypot = 1;
		}
		else if(!strcmp(argv[i], "--version")){
                        printf("%s\n", system_utsname.release);
			exit(0);
		}
		else if(!strcmp(argv[i], "--help")){
                        Usage();
                }
#ifdef CONFIG_BLK_DEV_INITRD
		else if(!strncmp(argv[i], "initrd=", strlen("initrd=")))
			initrd = &argv[i][strlen("initrd=")];
#endif
		if(add) add_arg(saved_command_line, argv[i]);
	}
	if(have_root == 0) add_arg(saved_command_line, DEFAULT_COMMAND_LINE);

	setup_machinename(system_utsname.machine);

	argv1_begin = argv[1];
	argv1_end = &argv[1][strlen(argv[1])];
  
	/* Kernel vm starts after physical memory and is either the size
	 * of physical memory or the remaining space left in the kernel
	 * area of the address space, whichever is smaller.
	 */
	start_vm = physmem + physmem_size + VMALLOC_OFFSET;
	if(start_vm >= get_kmem_end())
		panic("Physical memory too large to allow any kernel "
		      "virtual memory");

	virtmem_size = physmem_size;
	if(physmem_size > get_kmem_end() - start_vm)
		virtmem_size = get_kmem_end() - start_vm;
	end_vm = start_vm + virtmem_size;

	if(virtmem_size < physmem_size)
		printk(KERN_INFO "Kernel virtual memory size shrunk to %ld "
		       "bytes\n", virtmem_size);

	setup_range(-1, NULL, physmem, physmem_size, 
		    physmem_size + VMALLOC_OFFSET + virtmem_size);
	setup_memory();
	high_physmem = physmem + physmem_size;

	start_pfn = PFN_UP(__pa(physmem));
	end_pfn = PFN_DOWN(__pa(high_physmem));
	bootmap_size = init_bootmem(start_pfn, end_pfn - start_pfn);
	free_bootmem(__pa(physmem) + bootmap_size, 
		     high_physmem - physmem - bootmap_size);

#ifdef CONFIG_BLK_DEV_INITRD
	if(initrd != NULL) read_initrd(initrd);
#endif
	init_task.thread.kernel_stack = (unsigned long) &init_task + 
		2 * PAGE_SIZE;
#ifndef CONFIG_SMP
	current = &init_task;
#endif 
 	task_protections((unsigned long) &init_task);
	sp = (void *) init_task.thread.kernel_stack + 2 * PAGE_SIZE - 
		sizeof(unsigned long);
	return(signals(start_kernel_proc, sp));
}

void setup_arch(char **cmdline_p)
{
	paging_init();
 	strcpy(command_line, saved_command_line);
 	*cmdline_p = command_line;
	setup_hostinfo();
}

void check_bugs(void)
{
	return;
}

spinlock_t pid_lock = SPIN_LOCK_UNLOCKED;

void lock_pid(void)
{
	spin_lock(&pid_lock);
}

void unlock_pid(void)
{
	spin_unlock(&pid_lock);
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
