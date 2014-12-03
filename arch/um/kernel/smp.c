/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifdef CONFIG_SMP

#include "linux/config.h"
#include "linux/sched.h"
#include "linux/threads.h"
#include "linux/smp.h"
#include "asm/processor.h"
#include "asm/spinlock.h"
#include "asm/softirq.h"
#include "asm/hardirq.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"

/* Total count of live CPUs */
int smp_num_cpus = 0;

/* The 'big kernel lock' */
spinlock_t kernel_flag = SPIN_LOCK_UNLOCKED;

/* Per CPU bogomips and other parameters */
struct cpuinfo_um cpu_data[NR_CPUS];

/* which CPU maps to which logical number */
int cpu_number_map[NR_CPUS];

spinlock_t um_bh_lock = SPIN_LOCK_UNLOCKED;

atomic_t global_bh_count;
atomic_t global_bh_lock;

unsigned char global_irq_holder = NO_PROC_ID;
unsigned volatile int global_irq_lock;

/* Set when the idlers are all forked */
int smp_threads_ready = 0;
int num_reschedules_sent = 0;

void smp_send_reschedule(int cpu)
{
	num_reschedules_sent++;
}

static void show(char * str)
{
	int cpu = smp_processor_id();

	printk("\n%s, CPU %d:\n", str, cpu);
}
	
#define MAXCOUNT 100000000

static inline void wait_on_bh(void)
{
	int count = MAXCOUNT;
	do {
		if (!--count) {
			show("wait_on_bh");
			count = ~0;
		}
		/* nothing .. wait for the other bh's to go away */
	} while (atomic_read(&global_bh_count) != 0);
}

/*
 * This is called when we want to synchronize with
 * bottom half handlers. We need to wait until
 * no other CPU is executing any bottom half handler.
 *
 * Don't wait if we're already running in an interrupt
 * context or are inside a bh handler. 
 */
void synchronize_bh(void)
{
	if (atomic_read(&global_bh_count) && !in_interrupt())
		wait_on_bh();
}

void smp_send_stop(void)
{
	printk("Stopping all CPUs\n");
}

void smp_commence(void)
{
}

void smp_boot_cpus(void)
{
	if(ncpus < 1){
		printk("ncpus set to 1\n");
		ncpus = 1;
	}
	else if(ncpus > NR_CPUS){
		printk("ncpus can't be greater than NR_CPUS, set to %d\n", NR_CPUS);
		ncpus = NR_CPUS;
	}
}

int setup_profiling_timer(unsigned int multiplier)
{
	printk("setup_profiling_timer\n");
	return(0);
}

int inited_cpus = 1;

int pid_to_processor_id(int pid)
{
	int i;

	for(i=0;i<inited_cpus;i++){
		if(cpu_tasks[i].pid == pid) return(i);
	}
	panic("hard_smp_processor failed");
	return(-1);
}

int hard_smp_processor_id(void)
{
	return(pid_to_processor_id(getpid()));
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
