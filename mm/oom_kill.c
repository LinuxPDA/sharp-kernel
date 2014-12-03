/*
 *  linux/mm/oom_kill.c
 * 
 *  Copyright (C)  1998,2000  Rik van Riel
 *	Thanks go out to Claus Fischer for some serious inspiration and
 *	for goading me into coding this file...
 *
 *  The routines in this file are used to kill a process when
 *  we're seriously out of memory. This gets called from kswapd()
 *  in linux/mm/vmscan.c when we really run out of memory.
 *
 *  Since we won't call these routines often (on a well-configured
 *  machine) this file will double as a 'coding guide' and a signpost
 *  for newbie kernel hackers. It features several pointers to major
 *  kernel subsystems and hints as to where to find out what things do.
 *
 * Change Log
 *     12-Nov-2001 Lineo Japan, Inc.
 *     13-Nov-2002 SHARP
 *     16-Jan-2003 SHARP add VM switch
 *     24-Feb-2003 SHARP modify check out of memory function
 *     05-Aug-2003 SHARP for Tosa
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/timex.h>
#ifdef CONFIG_FREEPG_SIGNAL
#include <linux/freepg_signal.h>
#endif

/* #define DEBUG */

/**
 * int_sqrt - oom_kill.c internal function, rough approximation to sqrt
 * @x: integer of which to calculate the sqrt
 * 
 * A very rough approximation to the sqrt() function.
 */
static unsigned int int_sqrt(unsigned int x)
{
	unsigned int out = x;
	while (x & ~(unsigned int)1) x >>=2, out >>=1;
	if (x) out -= out >> 2;
	return (out ? out : 1);
}	

/**
 * oom_badness - calculate a numeric value for how bad this task has been
 * @p: task struct of which task we should calculate
 *
 * The formula used is relatively simple and documented inline in the
 * function. The main rationale is that we want to select a good task
 * to kill when we run out of memory.
 *
 * Good in this context means that:
 * 1) we lose the minimum amount of work done
 * 2) we recover a large amount of memory
 * 3) we don't kill anything innocent of eating tons of memory
 * 4) we want to kill the minimum amount of processes (one)
 * 5) we try to kill the process the user expects us to kill, this
 *    algorithm has been meticulously tuned to meet the priniciple
 *    of least surprise ... (be careful when you change it)
 */

static int badness(struct task_struct *p)
{
	int points, cpu_time, run_time;

	if (!p->mm)
		return 0;
	/*
	 * The memory size of the process is the basis for the badness.
	 */
	points = p->mm->total_vm;

	/*
	 * CPU time is in seconds and run time is in minutes. There is no
	 * particular reason for this other than that it turned out to work
	 * very well in practice. This is not safe against jiffie wraps
	 * but we don't care _that_ much...
	 */
	cpu_time = (p->times.tms_utime + p->times.tms_stime) >> (SHIFT_HZ + 3);
	run_time = (jiffies - p->start_time) >> (SHIFT_HZ + 10);

	points /= int_sqrt(cpu_time);
	points /= int_sqrt(int_sqrt(run_time));

	/*
	 * Niced processes are most likely less important, so double
	 * their badness points.
	 */
	if (p->nice > 0)
		points *= 2;

	/*
	 * Superuser processes are usually more important, so we make it
	 * less likely that we kill those.
	 */
	if (cap_t(p->cap_effective) & CAP_TO_MASK(CAP_SYS_ADMIN) ||
				p->uid == 0 || p->euid == 0)
		points /= 4;

	/*
	 * We don't want to kill a process with direct hardware access.
	 * Not only could that mess up the hardware, but usually users
	 * tend to only have this flag set on applications they think
	 * of as important.
	 */
	if (cap_t(p->cap_effective) & CAP_TO_MASK(CAP_SYS_RAWIO))
		points /= 4;
#ifdef DEBUG
	printk(KERN_DEBUG "OOMkill: task %d (%s) got %d points\n",
	p->pid, p->comm, points);
#endif
	return points;
}

/*
 * Simple selection loop. We chose the process with the highest
 * number of 'points'. We need the locks to make sure that the
 * list of task structs doesn't change while we look the other way.
 *
 * (not docbooked, we don't want this one cluttering up the manual)
 */
static struct task_struct * select_bad_process(void)
{
	int maxpoints = 0;
	short level = 0;
	struct task_struct *p = NULL;
	struct task_struct *chosen = NULL;

	read_lock(&tasklist_lock);
	for_each_task(p) {
		if (p->pid) {
			int points = badness(p);
			if (is_oom_kill_survival_process(p)) {
				if (oom_kill_survival_get(p) > level)
					level = oom_kill_survival_get(p);
			}
			else if (points > maxpoints) {
				chosen = p;
				maxpoints = points;
			}
		}
	}
	if (level > 0 && chosen == NULL) {
		for_each_task(p) {
			if (oom_kill_survival_get(p) == level) {
				int points = badness(p);
				if (points > maxpoints) {
					chosen = p;
					maxpoints = points;
				}
			}
		}
	}
	read_unlock(&tasklist_lock);
	return chosen;
}

/**
 * We must be careful though to never send SIGKILL a process with
 * CAP_SYS_RAW_IO set, send SIGTERM instead (but it's unlikely that
 * we select a process with CAP_SYS_RAW_IO set).
 */
void oom_kill_task(struct task_struct *p)
{
	printk(KERN_ERR "Out of Memory: Killed process %d (%s).\n", p->pid, p->comm);

	/*
	 * We give our sacrificial lamb high priority and access to
	 * all the memory it needs. That way it should be able to
	 * exit() and clear out its resources quickly...
	 */
	p->counter = 5 * HZ;
	p->flags |= PF_MEMALLOC | PF_MEMDIE;

	/* This process has hardware access, be more careful. */
	if (cap_t(p->cap_effective) & CAP_TO_MASK(CAP_SYS_RAWIO)) {
		force_sig(SIGTERM, p);
	} else {
		force_sig(SIGKILL, p);
	}
}

/**
 * oom_kill - kill the "best" process when we run out of memory
 *
 * If we run out of memory, we have the choice between either
 * killing a random task (bad), letting the system crash (worse)
 * OR try to be smart about which process to kill. Note that we
 * don't have to be perfect here, we just have to be good.
 */
static void oom_kill(void)
{
	struct task_struct *p = select_bad_process(), *q;

	/* Found nothing?!?! Either we hang forever, or we panic. */
	if (p == NULL)
		panic("Out of memory and no killable processes...\n");

	/* kill all processes that share the ->mm (i.e. all threads) */
	read_lock(&tasklist_lock);
	for_each_task(q) {
		if(q->mm == p->mm) oom_kill_task(q);
	}
	read_unlock(&tasklist_lock);

	/*
	 * Make kswapd go out of the way, so "p" has a good chance of
	 * killing itself before someone else gets the chance to ask
	 * for more memory.
	 */
	current->policy |= SCHED_YIELD;
	schedule();
	return;
}

#ifndef CONFIG_FREEPG_SIGNAL
#define DO_OOM_KILL_COUNT		10
#else
//#define FREEPG_DEBUG
#if 1
#define DO_RESET_CONDITION_COUNT	2
#define DO_OOM_KILL_COUNT		30
#define HIGH_THRESHOLD			14
#define LOW_THRESHOLD			4
#else
#define DO_RESET_CONDITION_COUNT	2
#define DO_OOM_KILL_COUNT		10
#define HIGH_THRESHOLD			5
#define LOW_THRESHOLD			1
#endif

#if defined(CONFIG_ARCH_SHARP_SL)
#define MIN_KILL_INTERVAL (60*HZ)
#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined (CONFIG_ARCH_PXA_TOSA)
#define OOM_KILL_PG_CACHE_SIZE	(1280)		/* 5MB */
#define MIN_SIGNAL_PG_CACHE_SIZE	(1536)	/* 6MB */
#define LOW_SIGNAL_PG_CACHE_SIZE	(2048)	/* 8MB */
#else
#define OOM_KILL_PG_CACHE_SIZE	(384)		/* 1.5MB */
#define MIN_SIGNAL_PG_CACHE_SIZE	(512)	/* 2MB */
#define LOW_SIGNAL_PG_CACHE_SIZE	(768)	/* 3MB */
#endif
#endif

static unsigned long first, last, count, good_count, retry_count;

void reset_out_of_memory_condition(void)
{
	if (++good_count >= DO_RESET_CONDITION_COUNT) {
		unsigned long now = jiffies;
		first = now;
		last = now;
		count = 0;
		retry_count = 0;
	}
}

#define K(x) ((x) << (PAGE_SHIFT-10))

static inline int is_memory_low(void)
{
	int buffer_cache;
	struct sysinfo meminfo;

	if (retry_count > 10)
		return 1;

	si_meminfo(&meminfo);
	buffer_cache = atomic_read(&page_cache_size) - swapper_space.nrpages;

#if 0
	/* 32M : 512K */
	if (meminfo.freeram > meminfo.totalram / 64)
		goto retry_shrink_cache;
#endif

	/* 32M : 6.4M(20%) */
	if (buffer_cache > meminfo.totalram / 5)
		goto retry_shrink_cache;

	return 1;	// low

retry_shrink_cache:
#ifdef FREEPG_DEBUG
	printk("Good(%d) : Total %luKB  Free %luKB  Buffers %luKB  Cached %luKB\n",
		retry_count, 
		K(meminfo.totalram),K(meminfo.freeram),K(meminfo.bufferram),
		K(atomic_read(&page_cache_size) 
				- meminfo.bufferram - swapper_space.nrpages) );
#endif
	retry_count++;
	return 0;
}
#endif

#if defined(CONFIG_ARCH_SHARP_SL)
#if defined(CONFIG_ARCH_PXA_CORGI)
//03.02.24  for signal control (see video/w100fb.c)
extern int disable_signal_to_mm;
#endif
void check_out_of_memory(void)
{
	int pgsize = atomic_read(&page_cache_size);
	static unsigned long prev_level, prev_action;
	unsigned long now;
	now = jiffies;
	if (pgsize > LOW_SIGNAL_PG_CACHE_SIZE) {
		if ((prev_level != 1) || (now - prev_action > MIN_KILL_INTERVAL)) {
			prev_level = 0;
//			printk("oom_reset()! %d\n", pgsize);
			freepg_signal_reset();
			return;
		}
	}
	if (now - prev_action > MIN_KILL_INTERVAL) {
		if (pgsize < OOM_KILL_PG_CACHE_SIZE) {
//			printk("oom_kill()! %d\n", pgsize);
			prev_level = 3;
			prev_action = now;
			oom_kill();
			freepg_signal_reset();
			return;
		}
		else {
//			printk("oom_kill_timeout()! %d\n", pgsize);
			prev_level = 0;
		}
	}
	if ((prev_level < 2) && (pgsize < MIN_SIGNAL_PG_CACHE_SIZE)) {
//		printk("oom_min()! %d\n", pgsize);

#if defined(CONFIG_ARCH_PXA_CORGI) // 03.02.24 disable the signal
	  if (disable_signal_to_mm){
	    //printk("W100FB: oom_min()! %d\n", pgsize);
	    return;
	  }
#endif 
		prev_level = 2;
		prev_action = now;
		freepg_signal_min();
		return;
	}
	else if ((prev_level < 1) && (pgsize < LOW_SIGNAL_PG_CACHE_SIZE)) {
//		printk("oom_low()! %d\n", pgsize);

#if defined(CONFIG_ARCH_PXA_CORGI) // 03.02.24 disable the signal
	  if (disable_signal_to_mm){
	    //printk("W100FB: oom_low()! %d\n", pgsize);
	    return;
	  }
#endif 
		prev_level = 1;
		prev_action = now;
		freepg_signal_low();
		return;
	}
}
#endif

/**
 * out_of_memory - is the system out of memory?
 */
void out_of_memory(void)
{
#ifndef CONFIG_FREEPG_SIGNAL
	static unsigned long first, last, count;
#endif
	unsigned long now, since;

	/*
	 * Enough swap space left?  Not OOM.
	 */
	if (nr_swap_pages > 0)
		return;

#ifdef CONFIG_FREEPG_SIGNAL
        good_count = 0;
#endif
	now = jiffies;
	since = now - last;
	last = now;

	/*
	 * If it's been a long time since last failure,
	 * we're not oom.
	 */
	last = now;
	if (since > 5*HZ)
		goto reset;

	/*
	 * If we haven't tried for at least one second,
	 * we're not really oom.
	 */
	since = now - first;
	if (since < HZ)
		return;

#ifdef CONFIG_FREEPG_SIGNAL
	if (!is_memory_low())
		return;
#ifdef FREEPG_DEBUG
	printk("XX oom_kill (%d)\n", count+1);
#endif
	if (count >= DO_OOM_KILL_COUNT) {
		return;
	} else if (count >= HIGH_THRESHOLD) {
		freepg_signal_min();
#ifdef FREEPG_DEBUG
		if (count == HIGH_THRESHOLD) print_meminfo();
#endif
	} else if (count >= LOW_THRESHOLD) {
		freepg_signal_low();
#ifdef FREEPG_DEBUG
		if (count == LOW_THRESHOLD) print_meminfo();
#endif
	}
#endif

	/*
	 * If we have gotten only a few failures,
	 * we're not really oom. 
	 */
	if (++count < DO_OOM_KILL_COUNT)
		return;

	/*
	 * Ok, really out of memory. Kill something.
	 */
#ifdef FREEPG_DEBUG
	print_meminfo();
#endif
	oom_kill();
#ifdef CONFIG_FREEPG_SIGNAL
	freepg_signal_reset();
#endif

reset:
	first = now;
	count = 0;
#ifdef CONFIG_FREEPG_SIGNAL
	retry_count = 0;
#endif
}


#ifdef FREEPG_DEBUG
#define memlist_next(x) ((x)->next)

static void print_meminfo()
{
	struct sysinfo meminfo;

#if 0
 	unsigned int order;
	unsigned type;

	for (type = 0; type < MAX_NR_ZONES; type++) {
		struct list_head *head, *curr;
		zone_t *zone = pgdat_list->node_zones + type;
 		unsigned long nr, total, flags;

		total = 0;
		if (zone->size) {
			spin_lock_irqsave(&zone->lock, flags);
		 	for (order = 0; order < MAX_ORDER; order++) {
				head = &(zone->free_area + order)->free_list;
				curr = head;
				nr = 0;
				for (;;) {
					curr = memlist_next(curr);
					if (curr == head)
						break;
					nr++;
				}
				total += nr * (1 << order);
				printk("%lu*%lukB ", nr, K(1UL) << order);
			}
			spin_unlock_irqrestore(&zone->lock, flags);
			printk("= %lukB)\n", K(total));

		}
	}
#endif

	si_meminfo(&meminfo);
	//si_swapinfo(&meminfo);

	printk("Total %luKB  Free %luKB  Buffers %luKB  Cached %luKB\n",
		K(meminfo.totalram),K(meminfo.freeram),K(meminfo.bufferram),
		K(atomic_read(&page_cache_size) - meminfo.bufferram - swapper_space.nrpages));
	printk("( Active: %d, inactive: %d, free: %d )\n",
	       nr_active_pages,
	       nr_inactive_pages,
	       nr_free_pages());

#if 0
	printk("active %d% / inactive %d% / free %d% "
		" :: active %d / inactive %d\n",
		nr_active_pages*100/meminfo.totalram,
		nr_inactive_pages*100/meminfo.totalram,
		meminfo.freeram*100/meminfo.totalram,
		nr_active_pages*100/(nr_active_pages+nr_inactive_pages),
		nr_inactive_pages*100/(nr_active_pages+nr_inactive_pages));

#endif
}
#endif
