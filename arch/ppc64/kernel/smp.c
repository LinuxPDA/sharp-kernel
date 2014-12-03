/*
 * 
 *
 * SMP support for ppc.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu) borrowing a great
 * deal of code from the sparc and intel versions.
 *
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 *
 * PowerPC-64 Support added by Dave Engebretsen, Peter Bergner, and
 * Mike Corrigan {engebret|bergner|mikec}@us.ibm.com
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/delay.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/init.h>
/* #include <linux/openpic.h> */
#include <linux/spinlock.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>
#include <asm/init.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/smp.h>
#include <asm/Naca.h>
#include <asm/Paca.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/HvCallCfg.h>
#include <asm/time.h>
#include <asm/ppcdebug.h>
#include "open_pic.h"

int smp_threads_ready = 0;
volatile int smp_commenced = 0;
int smp_num_cpus = 1;
int smp_tb_synchronized = 0;
extern atomic_t ipi_recv;
extern atomic_t ipi_sent;
spinlock_t kernel_flag __cacheline_aligned = SPIN_LOCK_UNLOCKED;
cycles_t cacheflush_time;
static int max_cpus __initdata = NR_CPUS;

unsigned long cpu_online_map;

volatile unsigned long cpu_callin_map[NR_CPUS] = {0,};

#define TB_SYNC_PASSES 4
volatile unsigned long __initdata tb_sync_flag = 0;
volatile unsigned long __initdata tb_offset = 0;

extern unsigned char stab_array[];

int start_secondary(void *);
extern int cpu_idle(void *unused);
void smp_call_function_interrupt(void);
void smp_message_pass(int target, int msg, unsigned long data, int wait);
u_int openpic_read(volatile u_int *addr);
static unsigned long iSeries_smp_message[NR_CPUS];
extern struct Naca *naca;
extern struct Paca xPaca[];

/* Forward declarations */
static void smp_iSeries_message_pass(int target, int msg, unsigned long data, int wait);
static int smp_iSeries_probe(void);
static void smp_iSeries_kick_cpu(int nr);
static void smp_iSeries_setup_cpu(int nr);

static void smp_openpic_message_pass(int target, int msg, unsigned long data, int wait);
static int smp_chrp_probe(void);
static void smp_chrp_kick_cpu(int nr);
static void smp_chrp_setup_cpu(int cpu_nr);

static void smp_xics_message_pass(int target, int msg, unsigned long data, int wait);
static int smp_xics_probe(void);
void xics_setup_cpu(void);
void xics_cause_IPI(int cpu);

/*
 * XICS only has a single IPI, so encode the messages per CPU
 */
volatile unsigned long xics_ipi_message[NR_CPUS] = {0};

#define smp_message_pass(t,m,d,w) \
    do { if (smp_ops) \
	     atomic_inc(&ipi_sent); \
	     smp_ops->message_pass((t),(m),(d),(w)); \
       } while(0)

static struct smp_ops_t {
	void  (*message_pass)(int target, int msg, unsigned long data, int wait);
	int   (*probe)(void);
	void  (*kick_cpu)(int nr);
	void  (*setup_cpu)(int nr);

} *smp_ops;

/* iSeries (iSeries) */
static struct smp_ops_t iSeries_smp_ops = {
	smp_iSeries_message_pass,
	smp_iSeries_probe,
	smp_iSeries_kick_cpu,
	smp_iSeries_setup_cpu
};

/* CHRP with openpic */
static struct smp_ops_t chrp_smp_ops = {
	smp_openpic_message_pass,
	smp_chrp_probe,
	smp_chrp_kick_cpu,
	smp_chrp_setup_cpu,
};

/* CHRP with new XICS interrupt controller */
static struct smp_ops_t xics_smp_ops = {
	smp_xics_message_pass,
	smp_xics_probe,
	smp_chrp_kick_cpu,
	smp_chrp_setup_cpu,
};

#ifdef CONFIG_KDB
void smp_kdb_stop(void)
{
}
#endif

static inline void set_tb(unsigned int upper, unsigned int lower)
{
	mtspr(SPRN_TBWL, 0);
	mtspr(SPRN_TBWU, upper);
	mtspr(SPRN_TBWL, lower);
}

void iSeries_smp_message_recv( struct pt_regs * regs )
{
	int cpu = smp_processor_id();
	int msg;

	if ( smp_num_cpus < 2 )
		return;

	for ( msg = 0; msg < 4; ++msg )
		if ( test_and_clear_bit( msg, &iSeries_smp_message[cpu] ) )
			smp_message_recv( msg, regs );

}

static void smp_iSeries_message_pass(int target, int msg, unsigned long data, int wait)
{
	int i;
	for (i = 0; i < smp_num_cpus; ++i) {
		if ( (target == MSG_ALL) || 
                    (target == i) || 
                    ((target == MSG_ALL_BUT_SELF) && (i != smp_processor_id())) ) {
			set_bit( msg, &iSeries_smp_message[i] );
			HvCall_sendIPI(&(xPaca[i]));
		}
	}
}

static int smp_iSeries_probe(void)
{
	unsigned i;
	unsigned np;
	struct ItLpPaca * lpPaca;

	np = 0;
	for (i=0; i < maxPacas; ++i) {
		lpPaca = xPaca[i].xLpPacaPtr;
		if ( lpPaca->xDynProcStatus < 2 ) {
			++np;
			xPaca[i].next_jiffy_update_tb = xPaca[0].next_jiffy_update_tb;
		}
	}
	
	smp_tb_synchronized = 1;
	return np;
}

static void smp_iSeries_kick_cpu(int nr)
{
	struct ItLpPaca * lpPaca;
	/* Verify we have a Paca for processor nr */
	if ( ( nr <= 0 ) ||
	     ( nr >= maxPacas ) )
		return;
	/* Verify that our partition has a processor nr */
	lpPaca = xPaca[nr].xLpPacaPtr;
	if ( lpPaca->xDynProcStatus >= 2 )
		return;
	/* The processor is currently spinning, waiting
	 * for the xProcStart field to become non-zero
	 * After we set xProcStart, the processor will
	 * continue on to secondary_start in iSeries_head.S
	 */
	xPaca[nr].xProcStart = 1;
}

static void smp_iSeries_setup_cpu(int nr)
{
}

static void
smp_openpic_message_pass(int target, int msg, unsigned long data, int wait)
{
	/* make sure we're sending something that translates to an IPI */
	if ( msg > 0x3 ){
		printk("SMP %d: smp_message_pass: unknown msg %d\n",
		       smp_processor_id(), msg);
		return;
	}
	switch ( target )
	{
	case MSG_ALL:
		openpic_cause_IPI(msg, 0xffffffff);
		break;
	case MSG_ALL_BUT_SELF:
		openpic_cause_IPI(msg,
				  0xffffffff & ~(1 << smp_processor_id()));
		break;
	default:
		openpic_cause_IPI(msg, 1<<target);
		break;
	}
}

static int
smp_chrp_probe(void)
{
	if (naca->processorCount > 1)
		openpic_request_IPIs();

	return naca->processorCount;
}

static void
smp_chrp_kick_cpu(int nr)
{
	/* Verify we have a Paca for processor nr */
	if ( ( nr <= 0 ) ||
	     ( nr >= maxPacas ) )
		return;

	/* The processor is currently spinning, waiting
	 * for the xProcStart field to become non-zero
	 * After we set xProcStart, the processor will
	 * continue on to secondary_start in iSeries_head.S
	 */
	xPaca[nr].xProcStart = 1;
}

static void
smp_chrp_setup_cpu(int cpu_nr)
{
	static atomic_t ready = ATOMIC_INIT(1);
	static volatile int frozen = 0;

	if (cpu_nr == 0) {
		/* wait for all the others */
		while (atomic_read(&ready) < smp_num_cpus)
			barrier();
		atomic_set(&ready, 1);
		/* freeze the timebase */
		call_rtas("freeze-time-base", 0, 1, NULL);
		mb();
		frozen = 1;
		set_tb(0, 0);
		xPaca[0].next_jiffy_update_tb = 0;
		while (atomic_read(&ready) < smp_num_cpus)
			barrier();
		/* thaw the timebase again */
		call_rtas("thaw-time-base", 0, 1, NULL);
		mb();
		frozen = 0;
		smp_tb_synchronized = 1;
	} else {
		atomic_inc(&ready);
		while (!frozen)
			barrier();
		set_tb(0, 0);
		xPaca[cpu_nr].next_jiffy_update_tb = 0;
		mb();
		atomic_inc(&ready);
		while (frozen)
			barrier();
	}

	if (OpenPIC_Addr) {
		do_openpic_setup_cpu();
	} else {
	  if (cpu_nr > 0)
	    xics_setup_cpu();
	}
}

static void
smp_xics_message_pass(int target, int msg, unsigned long data, int wait)
{
	int i;

	for (i = 0; i < smp_num_cpus; ++i) {
		if (target == MSG_ALL || target == i
		    || (target == MSG_ALL_BUT_SELF
			&& i != smp_processor_id())) {
			set_bit(msg, &xics_ipi_message[i]);
			mb();
			xics_cause_IPI(i);
			}
	}
}

static int
smp_xics_probe(void)
{
	return naca->processorCount;
}

#if 0
static void
smp_xics_setup_cpu(int cpu_nr)
{
	if (cpu_nr > 0)
		xics_setup_cpu();
}
#endif


void smp_local_timer_interrupt(struct pt_regs * regs)
{
	if (!--(get_paca()->prof_counter)) {
		update_process_times(user_mode(regs));
		(get_paca()->prof_counter)=get_paca()->prof_multiplier;
	}
}

void smp_message_recv(int msg, struct pt_regs *regs)
{
	atomic_inc(&ipi_recv);
	
	switch( msg ) {
	case PPC_MSG_CALL_FUNCTION:
		smp_call_function_interrupt();
		break;
	case PPC_MSG_RESCHEDULE: 
		current->need_resched = 1;
		break;
#ifdef CONFIG_XMON
	case PPC_MSG_XMON_BREAK:
		xmon(regs);
		break;
#endif /* CONFIG_XMON */
#ifdef CONFIG_KDB
	case PPC_MSG_XMON_BREAK:
	        /* This isn't finished yet, obviously -TAI */
		kdb(KDB_REASON_KEYBOARD,0, (kdb_eframe_t) regs);
		break;
#endif
	default:
		printk("SMP %d: smp_message_recv(): unknown msg %d\n",
		       smp_processor_id(), msg);
		break;
	}
}

void smp_send_reschedule(int cpu)
{
	/*
	 * This is only used if `cpu' is running an idle task,
	 * so it will reschedule itself anyway...
	 *
	 * This isn't the case anymore since the other CPU could be
	 * sleeping and won't reschedule until the next interrupt (such
	 * as the timer).
	 *  -- Cort
	 */
	/* This is only used if `cpu' is running an idle task,
	   so it will reschedule itself anyway... */
	smp_message_pass(cpu, PPC_MSG_RESCHEDULE, 0, 0);
}

#ifdef CONFIG_XMON
void smp_send_xmon_break(int cpu)
{
	smp_message_pass(cpu, PPC_MSG_XMON_BREAK, 0, 0);
}
#endif /* CONFIG_XMON */

static void stop_this_cpu(void *dummy)
{
	__cli();
	while (1)
		;
}

void smp_send_stop(void)
{
	smp_call_function(stop_this_cpu, NULL, 1, 0);
	smp_num_cpus = 1;
}

/*
 * Structure and data for smp_call_function(). This is designed to minimise
 * static memory requirements. It also looks cleaner.
 * Stolen from the i386 version.
 */
static spinlock_t call_lock = SPIN_LOCK_UNLOCKED;

static struct call_data_struct {
	void (*func) (void *info);
	void *info;
	atomic_t started;
	atomic_t finished;
	int wait;
} *call_data;

/*
 * This function sends a 'generic call function' IPI to all other CPUs
 * in the system.
 *
 * [SUMMARY] Run a function on all other CPUs.
 * <func> The function to run. This must be fast and non-blocking.
 * <info> An arbitrary pointer to pass to the function.
 * <nonatomic> currently unused.
 * <wait> If true, wait (atomically) until function has completed on other CPUs.
 * [RETURNS] 0 on success, else a negative status code. Does not return until
 * remote CPUs are nearly ready to execute <<func>> or are or have executed.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler, you may call it from a bottom half handler.
 */
int smp_call_function (void (*func) (void *info), void *info, int nonatomic,
			int wait)

{ 
	struct call_data_struct data;
	int ret = -1, cpus = smp_num_cpus-1;
	int timeout;

	if (!cpus)
		return 0;

	data.func = func;
	data.info = info;
	atomic_set(&data.started, 0);
	data.wait = wait;
	if (wait)
		atomic_set(&data.finished, 0);

	spin_lock_bh(&call_lock);
	call_data = &data;
	/* Send a message to all other CPUs and wait for them to respond */
	smp_message_pass(MSG_ALL_BUT_SELF, PPC_MSG_CALL_FUNCTION, 0, 0);

	/* Wait for response */
	timeout = 8000000;
	while (atomic_read(&data.started) != cpus) {
		HMT_low();
		if (--timeout == 0) {
			printk("smp_call_function on cpu %d: other cpus not responding (%d)\n",
			       smp_processor_id(), atomic_read(&data.started));
#ifdef CONFIG_XMON
                        xmon(0);
#endif
			goto out;
		}
		barrier();
		udelay(1);
#ifdef CONFIG_PPC_ISERIES
		HvCallCfg_getLps();
#endif
	}

	if (wait) {
		timeout = 1000000;
		while (atomic_read(&data.finished) != cpus) {
			HMT_low();
			if (--timeout == 0) {
				printk("smp_call_function on cpu %d: other cpus not finishing (%d/%d)\n",
				       smp_processor_id(), atomic_read(&data.finished), atomic_read(&data.started));
				goto out;
			}
			barrier();
			udelay(1);
#ifdef CONFIG_PPC_ISERIES
			HvCallCfg_getLps();
#endif
		}
	}
	ret = 0;

 out:
	HMT_medium();
	spin_unlock_bh(&call_lock);
	return ret;
}

void smp_call_function_interrupt(void)
{
	void (*func) (void *info) = call_data->func;
	void *info = call_data->info;
	int wait = call_data->wait;

	/*
	 * Notify initiating CPU that I've grabbed the data and am
	 * about to execute the function
	 */
	atomic_inc(&call_data->started);
	/*
	 * At this point the info structure may be out of scope unless wait==1
	 */
	(*func)(info);
	if (wait)
		atomic_inc(&call_data->finished);
}

static void smp_space_timers( unsigned nr )
{
	unsigned long offset, i;
	
	offset = tb_ticks_per_jiffy / nr;
	for ( i=1; i<nr; ++i ) {
		xPaca[i].next_jiffy_update_tb = xPaca[i-1].next_jiffy_update_tb + offset;
	}
}

extern unsigned long decr_overclock;

void __init smp_boot_cpus(void)
{
	extern struct current_set_struct current_set[];
	struct Paca *paca;
	extern void __secondary_start_chrp(void);
	int i, cpu_nr;
	struct task_struct *p;
	unsigned long sp;
	unsigned long arpn;

	printk("Entering SMP Mode...\n");
	PPCDBG(PPCDBG_SMP, "smp_boot_cpus: start.  NR_CPUS = 0x%lx\n", NR_CPUS);

	smp_num_cpus = 1;
        smp_store_cpu_info(0);
	/* store this once in the naca since we can't at this time
	 * envision an SMP environment where different processors
	 * would have a different loops_per_jiffy value -- tgall
	 */
	naca->loops_per_jiffy=loops_per_jiffy;

	/*
	 * assume for now that the first cpu booted is
	 * cpu 0, the master -- Cort
	 */
	cpu_callin_map[0] = 1;
	current->processor = 0;

	init_idle();

	for (i = 0; i < NR_CPUS; i++) {
		paca = &(naca->paca[i]);
		paca->prof_counter=1;
		paca->prof_multiplier = 1;
		if(i != 0) {
		        /*
			 * Processor 0's segment table is statically 
			 * initialized to real address 0x5000.  The
			 * Other processor's tables are created and
			 * initialized here.
			 */

			paca->xStab_data.virt = (unsigned long)&stab_array[PAGE_SIZE * (i-1)];
			arpn = physRpn_to_absRpn(___pa(paca->xStab_data.virt) >> 12);
			paca->xStab_data.real = arpn << 12;
			paca->xHwProcNum = cpu_hw_index[i];
			memset((void *)paca->xStab_data.virt, 0, PAGE_SIZE); 
			paca->default_decr = tb_ticks_per_jiffy / decr_overclock;
		}
		PPCDBG(PPCDBG_SMP, 
		       "\tProcessor %d, stab virt = 0x%lx, stab real = 0x%lx\n", 
		       i, paca->xStab_data.virt, paca->xStab_data.real);
	}

	/*
	 * XXX very rough, assumes 20 bus cycles to read a cache line,
	 * timebase increments every 4 bus cycles, 32kB L1 data cache.
	 */
	cacheflush_time = 5 * 1024;

	switch ( _machine ) {
	case _MACH_chrp:
		if (OpenPIC_Addr) {
			smp_ops = &chrp_smp_ops;
		} else {
			smp_ops = &xics_smp_ops;
		}
		break;
	case _MACH_iSeries:
		smp_ops = &iSeries_smp_ops;
		break;
	default:
		printk("SMP not supported on this machine.\n");
		return;
	}
	
	/* Probe arch for CPUs */
	cpu_nr = smp_ops->probe();

	printk("Probe found %d CPUs\n", cpu_nr);

	/*
	 * only check for cpus we know exist.  We keep the callin map
	 * with cpus at the bottom -- Cort
	 */
	if (cpu_nr > max_cpus)
		cpu_nr = max_cpus;

	smp_space_timers( cpu_nr );

	printk("Waiting for %d CPUs\n", cpu_nr-1);

	for ( i = 1 ; i < cpu_nr; i++ ) {
		int c;
		struct pt_regs regs;
		
		/* create a process for the processor */
		/* we don't care about the values in regs since we'll
		   never reschedule the forked task. */
		/* We DO care about one bit in the pt_regs we
		   pass to do_fork.  That is the MSR_FP bit in
		   regs.msr.  If that bit is on, then do_fork
		   (via copy_thread) will call giveup_fpu.
		   giveup_fpu will get a pointer to our (current's)
		   last register savearea via current->thread.regs
		   and using that pointer will turn off the MSR_FP,
		   MSR_FE0 and MSR_FE1 bits.  At this point, this
		   pointer is pointing to some arbitrary point within
		   our stack */

		memset(&regs, 0, sizeof(struct pt_regs));

		if (do_fork(CLONE_VM|CLONE_PID, 0, &regs, 0) < 0)
			panic("failed fork for CPU %d", i);
		p = init_task.prev_task;
		if (!p)
			panic("No idle task for CPU %d", i);

		PPCDBG(PPCDBG_SMP,"\tProcessor %d, task = 0x%lx\n", i, p);

		del_from_runqueue(p);
		unhash_process(p);
		init_tasks[i] = p;

		p->processor = i;
		p->has_cpu = 1;
		current_set[i].task = p;
		sp = ((unsigned long)p) + sizeof(union task_union)
			- STACK_FRAME_OVERHEAD;
		PPCDBG(PPCDBG_SMP, "\tstack pointer virt = 0x%lx\n", sp);
		current_set[i].sp_real = (void *)___pa(sp);
		PPCDBG(PPCDBG_SMP,"\tstack pointer real = 0x%lx\n", 
		       current_set[i].sp_real);

		/* wake up cpus */
		smp_ops->kick_cpu(i);

		/*
		 * wait to see if the cpu made a callin (is actually up).
		 * use this value that I found through experimentation.
		 * -- Cort
		 */
		for ( c = 5000; c && !cpu_callin_map[i] ; c-- ) {
#ifdef CONFIG_PPC_ISERIES
			HvCallCfg_getLps();
#endif	
			udelay(100);
		}
		
		if ( cpu_callin_map[i] )
		{
			printk("Processor %d found.\n", i);
			PPCDBG(PPCDBG_SMP, "\tProcessor %d found.\n", i);
			/* this sync's the decr's -- Cort */
			smp_num_cpus++;
		} else {
			printk("Processor %d is stuck.\n", i);
			PPCDBG(PPCDBG_SMP, "\tProcessor %d is stuck.\n", i);
		}
	}

	/* Setup CPU 0 last (important) */
	smp_ops->setup_cpu(0);
	
	if (smp_num_cpus < 2)
		smp_tb_synchronized = 1;
}

void __init smp_commence(void)
{
	/*
	 *	Lets the callin's below out of their loop.
	 */
	PPCDBG(PPCDBG_SMP, "smp_commence: start\n"); 
	wmb();
	smp_commenced = 1;
}

void __init smp_callin(void)
{
	int cpu = current->processor;
	
        smp_store_cpu_info(cpu);
	set_dec(xPaca[cpu].default_decr);
	cpu_callin_map[cpu] = 1;

	smp_ops->setup_cpu(cpu);

	init_idle();

	/*
	 * This cpu is now "online".  Only set them online
	 * before they enter the loop below since write access
	 * to the below variable is _not_ guaranteed to be
	 * atomic.
	 *   -- Cort <cort@fsmlabs.com>
	 */
	cpu_callin_map[cpu] = 1;
	
	while(!smp_commenced) {
#ifdef CONFIG_PPC_ISERIES
		HvCallCfg_getLps();
#endif
		barrier();
	}
	__sti();
}

/* intel needs this */
void __init initialize_secondary(void)
{
}

/* Activate a secondary processor. */
int start_secondary(void *unused)
{
	int cpu; 

	cpu = current->processor;
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;
	smp_callin();

	/* Go into the idle loop. */
	return cpu_idle(NULL);
}

void __init smp_setup(char *str, int *ints)
{
}

int __init setup_profiling_timer(unsigned int multiplier)
{
	return 0;
}

/* this function is called for each processor
 */
void __init smp_store_cpu_info(int id)
{
        naca->paca[id].pvr = _get_PVR();
}

static int __init maxcpus(char *str)
{
	get_option(&str, &max_cpus);
	return 1;
}

__setup("maxcpus=", maxcpus);
