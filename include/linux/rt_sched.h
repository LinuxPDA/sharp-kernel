/*
 * linux/include/linux/rt_sched.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *	rtai/include/rtai_sched.h
 */
/* 020219 /rtai/include/asm-arm/rtai_sched.h
Copyright (c) 2001 Alex Züpke, SYSGO RTS GmbH (azu@sysgo.de)
COPYRIGHT (C) 2002 Wolfgang Müller (wolfgang.mueller@dsa-ac.de)

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
--------------------------------------------------------------------------
Acknowledgements
- Paolo Mantegazza	(mantegazza@aero.polimi.it)
	creator of RTAI 
*/

#ifndef __LINUX_RT_SCHED_H
#define __LINUX_RT_SCHED_H 1

#include <linux/wait.h>
#include <linux/time.h>
#include <linux/errno.h>

#include <asm/rthal.h>

#define RT_SCHED_VERSION	"1.1.1"

#define RTH_BASE_PRIORITY 0x7fffffff

#define RTH_SCHED_FIFO 0 
#define RTH_SCHED_RR   1 

#define RTH_READY      0x0001
#define RTH_SUSPENDED  0x0002
#define RTH_DELAYED    0x0004
#define RTH_SEMAPHORE  0x0008

#define RTQ_PRIO 0 
#define RTQ_FIFO 4 
#define RTQ_RES  3

#define RSEM_BIN 1 
#define RSEM_CNT 2 
#define RSEM_RES 3

#ifndef CPU_TIME_LOG_SIZE 
#if defined(CONFIG_RT_CPU_TIME_LOG_SIZE)
#define CPU_TIME_LOG_SIZE CONFIG_RT_CPU_TIME_LOG_SIZE
#else
#define CPU_TIME_LOG_SIZE 3
#endif
#endif

typedef unsigned int rt_irq_t;

typedef long long RTIME;
#define RTIME_END 0x7FFFFFFFFFFFFFFFLL

struct rt_calibration {
	int cpu_freq;
	int latency;
	int setup_timer_cpu;
	int setup_timer;
};

struct rt_times {
	unsigned int base_tick;
	unsigned int periodic_tick;
	RTIME        base_time;
	RTIME        tick_time;
	RTIME        intr_time;
};

struct rt_thread;
struct rt_queue;
struct rt_semaphore;

typedef struct rt_thread    RTH;
typedef struct rt_queue     RTQ;
typedef struct rt_semaphore RSEM;

struct rt_queue {
	RTQ* prev;
	RTQ* next;
	RTH* thread;
};

struct rt_semaphore {
	RTQ  queue;
	int  type;
	int  count;
	RTH* owner;
	int  qtype;
};

struct rt_thread {
	int*          stack;
	int*          bstack;
	volatile int  state;
	char*         name;
	volatile int  priority;
	int           base_priority;
	int           policy;
	RTIME         period_time;
	RTIME         resume_time;
	RTIME         yield_time;
	int           rr_quantum;
	int           rr_remaining;
	RTQ           queue;
	RTQ*          blocked;
	RTH*          prio_pass;
	int           suspend;
	int           res;
	RTH*          prev;
	RTH*          next;
	RTH*          tprev;
	RTH*          tnext;
	RTH*          rprev;
	RTH*          rnext;
#if defined(CONFIG_RT_DEBUG)
	RTIME         cpu_start[CPU_TIME_LOG_SIZE];
	RTIME         cpu_end[CPU_TIME_LOG_SIZE];
	RTIME         cpu_time;
	unsigned long cpu_count;
#endif
	void          (*signal)(void);
	void*         priv;
};

#define RSEM_ERR    (0xffff)
#define RSEM_TIMOUT (0xfffe)

RTH*          rth_self(void);
int           rth_init(RTH*, void (*)(int), int, int, int);
int           rth_delete(RTH*);
int           rth_set_signal_handler(RTH*, void (*)(void));
void          rth_set_sched_policy(RTH*, int, int);
int           rth_set_priority(RTH*, int);
int           rth_set_resume_time(RTH*, RTIME);
int           rth_set_period_time(RTH*, RTIME);
int           rth_get_state(RTH*);
int           rth_get_priority(RTH*);
void          rth_yield(void);
int           rth_suspend(RTH*);
int           rth_resume(RTH*);
int           rth_make_periodic(RTH*, RTIME, RTIME);
void          rth_wait_periodic(void);
void          rth_sleep(RTIME);
void          rsem_init(RSEM*, int, int);
int           rsem_delete(RSEM*);
int           rsem_signal(RSEM*);
int           rsem_broadcast(RSEM*);
int           rsem_wait(RSEM*);
int           rsem_wait_timed(RSEM*, RTIME);
RTIME         rt_get_time(void);
RTIME         rt_get_time_ns(void);
RTIME         rt_count2nano(RTIME);
RTIME         rt_nano2count(RTIME);
RTIME         rt_timespec2count(const struct timespec*);
void          rt_count2timespec(RTIME, struct timespec*);
int           rt_printk(const char*, ...);

void          rt_cli(void);
void          rt_sti(void);
unsigned long rt_save_flags_and_cli(void);
void          rt_save_flags(unsigned long*);
void          rt_restore_flags(unsigned long);
unsigned int  rt_startup_irq(rt_irq_t);
void          rt_shutdown_irq(rt_irq_t);
void          rt_enable_irq(rt_irq_t);
void          rt_disable_irq(rt_irq_t);
void          rt_mask_ack_irq(rt_irq_t);
void          rt_unmask_irq(rt_irq_t);
void          rt_pending_irq(rt_irq_t, struct pt_regs* );
int           rt_request_irq(rt_irq_t, void (*)(void));
int           rt_free_irq(rt_irq_t);

void          rthal_init(void);
int           rt_schedule_init(void);
int           rt_pthread_init(void*);

static inline void rt_spin_lock(spinlock_t* lock)
{
	spin_lock(lock);
}

static inline void rt_spin_unlock(spinlock_t* lock)
{
	spin_unlock(lock);
}

static inline void rt_spin_lock_irq(spinlock_t* lock)
{
	hard_cli();
	rt_spin_lock(lock);
}

static inline void rt_spin_unlock_irq(spinlock_t* lock)
{
	rt_spin_unlock(lock);
	hard_sti();
}
static inline unsigned int rt_spin_lock_irqsave(spinlock_t* lock)
{
	unsigned long flags;
	hard_save_flags_and_cli(flags);
	rt_spin_lock(lock);
	return flags;
}

static inline void rt_spin_unlock_irqrestore(unsigned long flags,
					spinlock_t* lock)
{
	rt_spin_unlock(lock);
	hard_restore_flags(flags);
}

#endif
