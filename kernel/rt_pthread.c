/*
 * linux/kernel/rt_pthread.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *	rtai/upscheduler/rtai_sched.c.ml
 */
/*
COPYRIGHT (C) 2000  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
ACKNOWLEDGMENTS: 
- Steve Papacharalambous (stevep@zentropix.com) has contributed a very 
  informative proc filesystem procedure.
- Stuart Hughes (sehughes@zentropix.com) has helped in porting this 
  module to 2.4.xx.
- Stefano Picerno (stefanopp@libero.it) for suggesting a simple fix to 
  distinguish a timeout from an abnormal retrun in timed sem waits.
- Geoffrey Martin (gmartin@altersys.com) for a fix to functions with timeouts.
*/

#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/timex.h>
#include <linux/sched.h>

#include <asm/param.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <linux/rt_sched.h>
#include <linux/pthread.h>
#include <asm/rthal.h>

static void  rt_mem_init(void);
static void* rt_malloc(size_t);
static void  rt_free(void*);

static int rfire;

RTH*  rth_current;
RTH   rth_base;
RTIME rth_btime;

extern void rthal_request_rt_timer(void (*)(void));
extern int  rthal_request_irq(rt_irq_t, void (*)(int, void*, struct pt_regs*),
			      char*, void*);
extern void rthal_set_timer(int);
extern void rthal_switch_cpu_flag(int);

static void rt_schedule(void);

static void enq_blocked(RTH* th, RTQ* queue, int qtype)
{
	RTQ* q;
	th->blocked = (q = queue);
	if (!qtype) {
		while ((q = q->next) != queue &&
		       (q->thread)->priority <= th->priority);
	}
	q->prev = (th->queue.prev = q->prev)->next  = &(th->queue);
	th->queue.next = q;
}

static void deq_blocked(RTH* th)
{
	th->prio_pass = (RTH*)0;
	(th->queue.prev)->next = th->queue.next;
	(th->queue.next)->prev = th->queue.prev;
	th->blocked = (RTQ*)0;
}

static void enq_ready(RTH* ready_th)
{
	RTH* th;
	th = rth_base.rnext;
	while (ready_th->priority >= th->priority) {
		th = th->rnext;
	}
	th->rprev = (ready_th->rprev = th->rprev)->rnext = ready_th;
	ready_th->rnext = th;
}

static int renq_ready(RTH* th, int priority)
{
	int rc;

	if ((rc = th->priority != priority)) {
		th->priority = priority;
		(th->rprev)->rnext = th->rnext;
		(th->rnext)->rprev = th->rprev;
		enq_ready(th);
	}

	return rc;
}

static void rem_ready(RTH* th)
{
	if (th->state == RTH_READY) {
		(th->rprev)->rnext = th->rnext;
		(th->rnext)->rprev = th->rprev;
	}
}

static void rem_ready_current(void)
{
	(rth_current->rprev)->rnext = rth_current->rnext;
	(rth_current->rnext)->rprev = rth_current->rprev;
}

static void enq_timed(RTH* timed_th)
{
	RTH* th;
	th = rth_base.tnext;
	while (timed_th->resume_time > th->resume_time) {
		th = th->tnext;
	}
	th->tprev = (timed_th->tprev = th->tprev)->tnext = timed_th;
	timed_th->tnext = th;
}

static void rem_timed(RTH* th)
{
	if ((th->state & RTH_DELAYED)) {
		(th->tprev)->tnext = th->tnext;
		(th->tnext)->tprev = th->tprev;
	}
}

static void pass_prio(RTH* to, RTH* from)
{
	RTQ* q;

	from->prio_pass = to;
	while (to && to->priority > from->priority) {
		to->priority = from->priority;
		if (to->state == RTH_READY) {
			(to->rprev)->rnext = to->rnext;
			(to->rnext)->rprev = to->rprev;
			enq_ready(to);
		}
		else if ((q = to->blocked) &&
			 !((to->state & RTH_SEMAPHORE) &&
			   ((RSEM*)q)->qtype)) {
			(to->queue.prev)->next = to->queue.next;
			(to->queue.next)->prev = to->queue.prev;
			while ((q = q->next) != to->blocked && 
			       (q->thread)->priority <= to->priority);
			q->prev = (to->queue.prev = q->prev)->next =
			  &(to->queue);
			to->queue.next = q;
		}
		to = to->prio_pass;
	}
}

RTH* rth_self(void)
{
	return rth_current;
}

static void rth_startup(void(*thread)(int), int data)
{
	hard_sti();
	thread(data);
	rth_delete(rth_current);
}

int rth_init(RTH* th,
	     void (*rt_thread)(int),
	     int data,
	     int stack_size,
	     int priority)
{
	unsigned long flags;
	int* st;
#if defined(CONFIG_RT_DEBUG)
	int i;
#endif

	if (!(st = (int*)rt_malloc(stack_size))) {
		return -ENOMEM;
	}
	
	memset(th, 0, sizeof(RTH));

	th->stack  = (int*)(((unsigned long)st + stack_size - 0x10) & ~0xF);
	th->bstack = st;
        th->stack[0] = 0;
	th->suspend = 1;
	th->state = (RTH_SUSPENDED|RTH_READY);
	th->priority = th->base_priority = priority;
	th->prio_pass = 0;
	th->period_time = 0;
	th->resume_time = RTIME_END;
	th->queue.prev = &(th->queue);      
	th->queue.next = &(th->queue);      
	th->queue.thread = th;
	th->tprev = th->tnext = th->rprev = th->rnext = th;
	th->blocked = (RTQ*)0;
#if defined(CONFIG_RT_DEBUG)
	for (i = 1; i < CPU_TIME_LOG_SIZE; i++) {
		th->cpu_start[i] = th->cpu_end[i] = rt_get_time();
	}
	th->cpu_start[0] = th->cpu_end[0] = rt_get_time();
#endif

	rthal_init_thread_stack();

	hard_save_flags_and_cli(flags);
	rth_base.prev->next = th;
	th->prev = rth_base.prev;
	rth_base.prev = th;
	hard_restore_flags(flags);

	return 0;
}

int rth_delete(RTH* th)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);
	
	if (!(th->res & 0x0000ffff) || (th == rth_current) ||
	    (rth_current->priority == RTH_BASE_PRIORITY)) {
		rem_timed(th);
		if (th->blocked) {
			(th->queue.prev)->next = th->queue.next;
			(th->queue.next)->prev = th->queue.prev;
			if (th->state & RTH_SEMAPHORE) {
				if (!((RSEM*)(th->blocked))->type) {
					((RSEM*)(th->blocked))->count++;
				}
				else {
					((RSEM*)(th->blocked))->count = 1;
				}
			}
		}
		if (!((th->prev)->next = th->next)) {
			rth_base.prev = th->prev;
		}
		else {
			(th->next)->prev = th->prev;
		}
		rem_ready(th);
		th->state = 0;
		if (th->bstack) {
			rt_free(th->bstack);
		}
		th->bstack = 0;
		if (th == rth_current) {
			for (;;) {
				rt_schedule();
			}
		}
	}
	else {
		th->suspend = -0x7FFFFFFF;
	}
	hard_restore_flags(flags);

	return 0;
}

int rth_set_signal_handler(RTH* th, void (*handler)(void))
{
	th->signal = handler;
	return 0;
}

void rth_set_sched_policy(RTH* th, int policy, int rr_quantum_ns)
{
	th->policy = policy;
	if (th->policy) {
		th->rr_quantum = rt_nano2count(rr_quantum_ns); 
		if (!th->rr_quantum) {
			th->rr_quantum = rt_times.base_tick; 
		}
		th->rr_remaining = th->rr_quantum;
		th->yield_time = 0; 
	}
}

int rth_set_priority(RTH* th, int priority)
{
	unsigned long flags;
	int prio, sched;
	RTQ* q;

	hard_save_flags_and_cli(flags);

	sched = 0;
	prio = th->base_priority;

	if ((th->base_priority = priority) < th->priority) {
		do {
			th->priority = priority;
			if (th->state == RTH_READY) {
				(th->rprev)->rnext = th->rnext;
				(th->rnext)->rprev = th->rprev;
				enq_ready(th);
				sched = 1;
			}
			else if ((q = th->blocked) &&
				 !((th->state & RTH_SEMAPHORE) &&
				   ((RSEM*)q)->qtype)) {
				(th->queue.prev)->next = th->queue.next;
				(th->queue.next)->prev = th->queue.prev;
				while ((q = q->next) != th->blocked &&
				       (q->thread)->priority <= priority);
				q->prev = (th->queue.prev =
					   q->prev)->next = &(th->queue);
				th->queue.next = q;
				sched = 1;
			}
		} while ((th = th->prio_pass) &&
			 th->priority > priority);
		if (sched) {
			rt_schedule();
		}
	}

	hard_restore_flags(flags);

	return prio;
}

int rth_set_resume_time(RTH* th, RTIME new_resume_time)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	if (th->state & RTH_DELAYED) {
		if (((th->resume_time = new_resume_time) -
		     (th->tnext)->resume_time) > 0) {
			rem_timed(th);
			enq_timed(th);
			
			hard_restore_flags(flags);

			return 0;
        	}
        }

	hard_restore_flags(flags);

	return -ETIME;
}

int rth_set_period_time(RTH* th, RTIME new_period_time)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	th->period_time = new_period_time;

	hard_restore_flags(flags);

	return 0;
}

int rth_get_state(RTH* th)
{
	return th->state;
}

int rth_get_priority(RTH* th)
{
	return th->priority;
}

void rth_yield(void)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	{
		RTH* th = rth_current->rnext;
		while (rth_current->priority == th->priority) {
			th = th->rnext;
		}
		if (th != rth_current->rnext) {
			(rth_current->rprev)->rnext = rth_current->rnext;
			(rth_current->rnext)->rprev = rth_current->rprev;
			th->rprev = (rth_current->rprev = th->rprev)->rnext =
			  rth_current;
			rth_current->rnext = th;
			rt_schedule();
		}
	}

	hard_restore_flags(flags);
}

int rth_suspend(RTH* th)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);

        if (!th->suspend++ && !th->res) {
		rem_ready(th);
		th->state |= RTH_SUSPENDED;
		if (th == rth_current) {
			rt_schedule();
		}
	}

	hard_restore_flags(flags);

	return 0;
}

int rth_resume(RTH* th)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	if (!(--th->suspend)) {
		rem_timed(th);
		if (((th->state &= ~RTH_SUSPENDED) & ~RTH_DELAYED) ==
		    RTH_READY) {
			enq_ready(th);
			rt_schedule();
		}
	}

	hard_restore_flags(flags);

	return 0;
}

int rth_make_periodic(RTH* th, RTIME delay, RTIME period_time)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	th->resume_time = rt_get_time() + delay;
	th->period_time = period_time;
        th->suspend = 0;
	if (!(th->state & RTH_DELAYED)) {
		rem_ready(th);
		th->state = (th->state & ~RTH_SUSPENDED)|RTH_DELAYED;
		enq_timed(th);
	}

	rt_schedule();

	hard_restore_flags(flags);

	return 0;
}

void rth_wait_periodic(void)
{
	unsigned long flags;
	RTIME	resume_time;
	

	hard_save_flags_and_cli(flags);

	resume_time = rth_current->resume_time + rth_current->period_time;
	if (resume_time > rth_btime) {
		rth_current->resume_time = resume_time;
		rth_current->state |= RTH_DELAYED;
		rem_ready_current();
		enq_timed(rth_current);
		rt_schedule();
	}
	else {
		rth_current->resume_time = 
			rt_get_time() + rth_current->period_time;
	}

	hard_restore_flags(flags);
}

void rth_sleep(RTIME delay)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	if ((rth_current->resume_time = (rt_get_time()) + delay) > rth_btime) {
		rth_current->state |= RTH_DELAYED;
		rem_ready_current();
		enq_timed(rth_current);
		rt_schedule();
	}

	hard_restore_flags(flags);
}

RTIME rt_get_time_ns(void)
{
	return llimd(rt_get_time(), 1000000000, rt_calibration.cpu_freq);
}

RTIME rt_count2nano(RTIME counts)
{
	int sign;

	if (counts > 0) {
		sign = 1;
	}
	else {
		sign = 0;
		counts = - counts;
	}
	counts = llimd(counts, 1000000000, rt_calibration.cpu_freq);
	return (sign) ? counts : - counts;
}

RTIME rt_nano2count(RTIME ns)
{
	int sign;

	if (ns > 0) {
		sign = 1;
	}
	else {
		sign = 0;
		ns = - ns;
	}
	ns = llimd(ns, rt_calibration.cpu_freq, 1000000000);

	return (sign) ? ns : - ns;
}

RTIME rt_timespec2count(const struct timespec* ts)
{
	return rt_nano2count(ts->tv_sec * 1000000000LL + ts->tv_nsec);
}

void rt_count2timespec(RTIME rt, struct timespec* ts)
{
	ts->tv_sec = ulldiv(rt_count2nano(rt), 1000000000, 
			    (unsigned long*)&ts->tv_nsec);
}

static void rt_schedule(void)
{
	RTH* th;
	RTH* nth;
	RTIME intr_time;
	RTIME now;
	int prio, delay, pr;
	static int prev_delay;
#if defined(CONFIG_RT_DEBUG)
	int i;
#endif
	
	prio = RTH_BASE_PRIORITY;
	th = nth = &rth_base;

	if (rth_current->policy > 0) {
		rth_current->rr_remaining =
		  rth_current->yield_time - rt_times.tick_time;
		if (rth_current->rr_remaining <= 0) {
			rth_current->rr_remaining = rth_current->rr_quantum;
			if (rth_current->state == RTH_READY) {
				th = rth_current->rnext;
				while (rth_current->priority == th->priority) {
					th = th->rnext;
				}
				if (th != rth_current->rnext) {
					(rth_current->rprev)->rnext =
					  rth_current->rnext;
					(rth_current->rnext)->rprev =
					  rth_current->rprev;
					th->rprev = 
					  (rth_current->rprev =
					   th->rprev)->rnext = rth_current;
					rth_current->rnext = th;
				}
			}
		}
	}

	rth_btime = rt_get_time() + (RTIME)rt_calibration.latency;

	th = rth_base.tnext;
	while (th->resume_time <= rth_btime) {
		if ((th->state &= ~(RTH_DELAYED|RTH_SEMAPHORE))
		    == RTH_READY) {
			enq_ready(th);
		}
		th = th->tnext;
	}
	rth_base.tnext = th;
	th->tprev = &rth_base;

	do { prio = (nth = rth_base.rnext)->priority; } while(0);

	if (nth->policy > 0) {
		nth->yield_time = rth_btime + nth->rr_remaining;
	}

	intr_time = rfire ? rt_times.intr_time :
	  rt_times.intr_time + (RTIME)rt_times.base_tick;

	if (nth->policy > 0) {
		pr = 1;
		if (nth->yield_time < intr_time) {
			intr_time = nth->yield_time;
		}
	}
	else {
		pr = 0;
	}

	th = &rth_base;
	while ((th = th->tnext) != &rth_base) {
		if (th->priority <= prio && th->resume_time < intr_time) {
			intr_time = th->resume_time;
			pr = 1;
			break;
		}
	}

	if ((pr) || (!rfire && prio == RTH_BASE_PRIORITY)) {
		rfire = 1;
		if (pr) {
			rt_times.intr_time = intr_time;
		}

		now = rt_get_time();
		delay = (int)(rt_times.intr_time - now) -
		  rt_calibration.latency;

		if (delay < rt_calibration.latency) {
			delay = rt_calibration.latency;
			if (prev_delay)
				delay += prev_delay;
				
			rt_times.intr_time = now + delay;

			prev_delay = delay;
		}
		else {
			prev_delay = 0;
		}

		rthal_set_timer(delay);
	}

	if (nth != rth_current) {
		if (rth_current == &rth_base) {
			rthal_switch_cpu_flag(1);
		}
		if (nth == &rth_base) {
			rthal_switch_cpu_flag(0);
		}
#if defined(CONFIG_RT_DEBUG)
		nth->cpu_count++;
		for (i = CPU_TIME_LOG_SIZE-1; i > 0; i--) {
			rth_current->cpu_end[i-1] = rth_current->cpu_end[i];
			nth->cpu_end[i-1] = nth->cpu_end[i];
		}
		rth_current->cpu_end[0] = nth->cpu_start[0] = rt_get_time();
		rth_current->cpu_time += (rth_current->cpu_end[0] -
					  rth_current->cpu_start[0]);
#endif
		rthal_switch_thread(nth);

		if (rth_current->signal) {
			(*rth_current->signal)();
		}
	}
}

static void rt_timer_handler(void)
{

	RTH* th;
	RTH* nth;
	RTIME now;
	int prio, delay, pr;
	static int prev_delay;
#if defined(CONFIG_RT_DEBUG)
	int i;
#endif

	prio = RTH_BASE_PRIORITY;
	th = nth = &rth_base;
	rt_times.tick_time = rt_get_time();
	rth_btime = rt_times.tick_time + (RTIME)rt_calibration.latency;

	th = rth_base.tnext;
	while (th->resume_time <= rth_btime) {
		if ((th->state &= ~(RTH_DELAYED|RTH_SEMAPHORE))
		    == RTH_READY) {
			enq_ready(th);
		}
		th = th->tnext;
	}
	rth_base.tnext = th;
	th->tprev = &rth_base;

	if (rth_current->policy > 0) {
		rth_current->rr_remaining =
		  rth_current->yield_time - rt_times.tick_time;
		if (rth_current->rr_remaining <= 0) {
			rth_current->rr_remaining = rth_current->rr_quantum;
			if (rth_current->state == RTH_READY) {
				th = rth_current->rnext;
				while (rth_current->priority == th->priority) {
					th = th->rnext;
				}
				if (th != rth_current->rnext) {
					(rth_current->rprev)->rnext =
					  rth_current->rnext;
					(rth_current->rnext)->rprev =
					  rth_current->rprev;
					th->rprev =
					  (rth_current->rprev =
					   th->rprev)->rnext = rth_current;
					rth_current->rnext = th;
				}
			}
		}
	}

	do { prio = (nth = rth_base.rnext)->priority; } while(0);

	if (nth->policy > 0) {
		nth->yield_time = rth_btime + nth->rr_remaining;
	}
	rt_times.intr_time = rt_times.tick_time + (RTIME)rt_times.base_tick;
	if (nth->policy > 0) {
		pr = 1;
		if (nth->yield_time < rt_times.intr_time) {
			rt_times.intr_time = nth->yield_time;
		}
	}
	else {
		pr = 1;
	}

	th = &rth_base;
	while ((th = th->tnext) != &rth_base) {
		if (th->priority <= prio &&
		    th->resume_time < rt_times.intr_time) {
			rt_times.intr_time = th->resume_time;
			pr = 1;
			break;
		}
	}

	if ((rfire = pr)) {
		now = rt_get_time();
		delay = (int)(rt_times.intr_time - now) -
		  rt_calibration.latency;

		if (delay < rt_calibration.latency) {
			delay = rt_calibration.latency;
			if (prev_delay)
				delay += prev_delay;
				
			rt_times.intr_time = now + delay;

			prev_delay = delay;
		}
		else {
			prev_delay = 0;
		}

		rthal_set_timer(delay);
	}
	
	if (nth != rth_current) {
		if (rth_current == &rth_base) {
			rthal_switch_cpu_flag(1);
		}
		if (nth == &rth_base) {
			rthal_switch_cpu_flag(0);
		}

#if defined(CONFIG_RT_DEBUG)
		nth->cpu_count++;
		for (i = CPU_TIME_LOG_SIZE-1; i > 0; i--) {
			rth_current->cpu_end[i-1] = rth_current->cpu_end[i];
			nth->cpu_end[i-1] = nth->cpu_end[i];
		}
		rth_current->cpu_end[0] = nth->cpu_start[0] = rt_get_time();
		rth_current->cpu_time += (rth_current->cpu_end[0] -
					  rth_current->cpu_start[0]);
#endif
		rthal_switch_thread(nth);

		if (rth_current->signal) {
			(*rth_current->signal)();
		}
	}
}

void rsem_init(RSEM* sem, int value, int type)
{
	sem->count = value;
	sem->qtype = (type & RTQ_FIFO) ? 1 : 0;

	type = (type & 3) - 2;

	if ((sem->type = type) < 0 && value > 1) {
		sem->count = 1;
	}
	else if (type > 0) {
		sem->type = sem->count = 1;
	}
	sem->queue.prev = &(sem->queue);
	sem->queue.next = &(sem->queue);
	sem->queue.thread = sem->owner = 0;
}

int rsem_delete(RSEM* sem)
{
	unsigned long flags;
	RTH* th;
	int sched;
	RTQ* q;

	hard_save_flags_and_cli(flags);

	sched = 0;
	q = &(sem->queue);

	while ((q = q->next) != &(sem->queue) && (th = q->thread)) {
		rem_timed(th);
		if (th->state != RTH_READY &&
		    (th->state &= ~(RTH_SEMAPHORE|RTH_DELAYED))
		    == RTH_READY) {
			enq_ready(th);
			sched = 1;
		}
	}

	if ((th = sem->owner) && sem->type > 0) {
		if (th->res & 0x000ffff) {
			--th->res;
		}
		if (!th->res) {
			sched = renq_ready(th, th->base_priority);
		}
		else if (!(rth_current->res & 0x000ffff)) {
			sched = renq_ready(rth_current,
					   rth_current->base_priority);
		}

		if (th->suspend) {
			if (th->suspend > 0) {
				th->state |= RTH_SUSPENDED;
				rem_ready(th);
				sched = 1;
			}
			else {
				rth_delete(th);
			}
		}
	}

	if (sched) {
		rt_schedule();
	}

	hard_restore_flags(flags);

	return 0;
}

int rsem_signal(RSEM* sem)
{
	RTH* th;
	int sched;
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	if (sem->type > 0) {
		if (sem->type > 1) {
			sem->type--;
			hard_restore_flags(flags);
			return 0;
		} 
		if (++sem->count > 1) {
			sem->count = 1;
		}
	}
	else {
		sem->count++;
	}

	if ((th = (sem->queue.next)->thread)) {
		deq_blocked(th);
		rem_timed(th);
		if ((th->state != RTH_READY) &&
		    (th->state &= ~(RTH_SEMAPHORE|RTH_DELAYED))
		    == RTH_READY) {
			enq_ready(th);
			if (sem->type <= 0) {
				rt_schedule();
				hard_restore_flags(flags);
				return 0;
			}
		}
	}

	if (sem->type > 0) {
		sem->owner = 0;

		if (rth_current->res & 0x000ffff) {
			--rth_current->res;
		}

		if (!rth_current->res) {
			sched = renq_ready(rth_current,
					   rth_current->base_priority);
		}
		else if (!(rth_current->res & 0x000ffff)) {
			sched = renq_ready(rth_current,
					   rth_current->base_priority);
		}
		else {
			sched = 0;
		}

		if (rth_current->suspend) {
			if (rth_current->suspend > 0) {
				rth_current->state |= RTH_SUSPENDED;
				rem_ready_current();
				sched = 1;
			}
			else {
				rth_delete(rth_current);
			}	
		}	
		if (sched) {
			rt_schedule();
		}	
	}

	hard_restore_flags(flags);

	return 0;
}

int rsem_broadcast(RSEM* sem)
{
	unsigned long flags;
	int sched;
	RTH* th;
	RTQ* q;

	sched = 0;
	q = &(sem->queue);

	hard_save_flags_and_cli(flags);

	while ((q = q->next) != &(sem->queue)) {
		deq_blocked(th = q->thread);
		rem_timed(th);
		if (th->state != RTH_READY &&
		    (th->state &= ~(RTH_SEMAPHORE|RTH_DELAYED))
		    == RTH_READY) {
			enq_ready(th);
			sched = 1;
		}
	}

	sem->count = 0;
	sem->queue.prev = sem->queue.next = &(sem->queue);

	if (sched) {
		rt_schedule();
	}

	hard_restore_flags(flags);

	return 0;
}

int rsem_wait(RSEM* sem)
{
	unsigned long flags;
	int count;

	hard_save_flags_and_cli(flags);

	if ((count = sem->count) > 0) {
		if (sem->type > 0) {
			if (sem->owner == rth_current) {
				sem->type++;
				hard_restore_flags(flags);
				return 0;
			}
			(sem->owner = rth_current)->res++;
		}
		sem->count--;
	}
	else {
		if (sem->type > 0) {
			if (sem->owner == rth_current) {
				sem->type++;
				hard_restore_flags(flags);
				return count;
			}
			pass_prio(sem->owner, rth_current);
		}
		sem->count--;
		rth_current->state |= RTH_SEMAPHORE;
		rem_ready_current();
		enq_blocked(rth_current, &sem->queue, sem->qtype);
		rt_schedule();
	
		if (rth_current->blocked) {
			rth_current->prio_pass = (RTH*)0;
			hard_restore_flags(flags);
			return RSEM_ERR;
		}
		else { 
			count = sem->count;
		}

		if (sem->type > 0) {
			(sem->owner = rth_current)->res++;
		}
	}

	hard_restore_flags(flags);

	return count;
}

int rsem_wait_if(RSEM* sem)
{
	int count;
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	if ((count = sem->count) > 0) {
		if (sem->type > 0) {
			if (sem->owner == rth_current) {
				sem->type++;
				hard_restore_flags(flags);
				return 0;
			}
			(sem->owner = rth_current)->res++;
		}
		sem->count--;
	}

	hard_restore_flags(flags);
	return count;
}

static int rsem_wait_until(RSEM* sem, RTIME time)
{
	int count;
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	if ((count = sem->count) <= 0) {
		rth_current->blocked = &sem->queue;
		if ((rth_current->resume_time = time) > rth_btime) {
			if (sem->type > 0) {
				if (sem->owner == rth_current) {
					sem->type++;
					hard_restore_flags(flags);
					return 0;
				}
				pass_prio(sem->owner, rth_current);
			}
			sem->count--;
			rth_current->state |= (RTH_SEMAPHORE|RTH_DELAYED);
			rem_ready_current();
			enq_blocked(rth_current, &sem->queue, sem->qtype);
			enq_timed(rth_current);	
			rt_schedule();
		}
		else {
			sem->count--;
			rth_current->queue.prev = rth_current->queue.next =
			  &rth_current->queue;
		}
		if (rth_current->blocked) {
			deq_blocked(rth_current);
			if(++sem->count > 1 && sem->type) {
				sem->count = 1;
			}
			hard_restore_flags(flags);
			return RSEM_TIMOUT;
		}
		else {
			count = sem->count;
		}
	}
	else {
		sem->count--;
	}

	if (sem->type > 0) {
		(sem->owner = rth_current)->res++;
	}

	hard_restore_flags(flags);

	return count;
}

int rsem_wait_timed(RSEM* sem, RTIME delay)
{
	return rsem_wait_until(sem, rt_get_time() + delay);
}

#include <linux/console.h>

int rt_printk(const char* fmt, ...)
{
	static spinlock_t display_lock = SPIN_LOCK_UNLOCKED;
	char buf[1024];
	unsigned long flags;
	va_list args;
	int len = 0;

	flags = rt_spin_lock_irqsave(&display_lock);
	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	printk("%s", buf);
	rt_spin_unlock_irqrestore(flags, &display_lock);

	return len;
}

#if defined(CONFIG_RT_DEBUG)
static void rt_debug_func(int data)
{
}
#endif

int rt_schedule_init(void)
{
#if defined(CONFIG_RT_DEBUG)
	RTH th;
	int i;
#endif
	rt_mem_init();

	rth_base.name        = "rth_base";
	rth_base.policy      = 0;
	rth_base.state       = RTH_READY;
	rth_base.priority    = rth_base.base_priority = RTH_BASE_PRIORITY;
	rth_base.signal      = 0;
	rth_base.prev        = &rth_base;
	rth_base.next        = 0;
	rth_base.resume_time = RTIME_END;

	rth_base.tprev = rth_base.tnext =
	  rth_base.rprev = rth_base.rnext = &rth_base;

#if defined(CONFIG_RT_DEBUG)
	for (i = 1; i < CPU_TIME_LOG_SIZE; i++) {
		rth_base.cpu_start[i] = rth_base.cpu_end[i] = 0;
	}
	rth_base.cpu_start[0] = rth_base.cpu_end[0] = rt_get_time();
	rth_base.cpu_time = 0;
#endif
	rth_current = &rth_base;

	rt_calibration.latency =
	  imuldiv(RT_TIMER_LATENCY, rt_calibration.cpu_freq, 1000000000);

	rthal_request_rt_timer(rt_timer_handler);
	rth_btime = rt_times.tick_time + rt_calibration.latency;

	rfire = 1;

#if defined(CONFIG_RT_DEBUG)
	rth_init(&th, rt_debug_func, 0, 4096, 1);
	rth_resume(&th);
#endif

	printk("RealTime scheduler version %s initialized.\n", 
							RT_SCHED_VERSION);

	return 0;
}

static int get_max_priority(int policy)
{
	switch (policy) {
	case SCHED_FIFO:
	case SCHED_RR:
		return 99;
	case SCHED_OTHER:
		return 0;
	}
	return -EINVAL;
}

static int get_min_priority(int policy)
{
	switch (policy) {
	case SCHED_FIFO:
	case SCHED_RR:
		return 1;
	case SCHED_OTHER:
		return 0;
	}
	return -EINVAL;
}

static rt_pthread_data_t rt_pthreads[CONFIG_RTH_PTHREAD_MAX];

typedef void (*RTH_THREAD)(int);

#define RTH_PTHREAD_DEFAULT_PRIORITY   20
#define RTH_PTHREAD_DEFAULT_STACK_SIZE 4000

#define PTHREAD_PRIORITY_FACTOR 99
#define ENOTSUP EOPNOTSUPP

pthread_t pthread_self(void)
{
	return ((rt_pthread_t)(rth_self()->priv))->p_tid;
}

int pthread_create(pthread_t* thread, pthread_attr_t* attr,
		   void* (*start_routine)(void*), void* arg)
{
	int j, rc;
	unsigned long flags;

	hard_save_flags_and_cli(flags);

	for (j = 0; j < CONFIG_RTH_PTHREAD_MAX; j++) {
		if (rt_pthreads[j].p_tid == 0) {
		  rt_pthreads[j].p_nextwaiting = NULL;
		  rt_pthreads[j].p_tid = j + 1;
		  rt_pthreads[j].p_priority = RTH_PTHREAD_DEFAULT_PRIORITY; 
		  rt_pthreads[j].p_policy = SCHED_OTHER;
		  rt_pthreads[j].p_spinlock = NULL;
		  rt_pthreads[j].p_signal = 0;
		  rt_pthreads[j].p_signal_jmp = NULL;
		  rt_pthreads[j].p_cancel_jmp = NULL;
		  rt_pthreads[j].p_terminated = 0;
		  rt_pthreads[j].p_detached = attr == NULL ? 0 : attr->detachstate;
		  rt_pthreads[j].p_exited = 0;
		  rt_pthreads[j].p_retval = NULL;
		  rt_pthreads[j].p_joining = NULL;
		  rt_pthreads[j].p_cancelstate = PTHREAD_CANCEL_ENABLE;
		  rt_pthreads[j].p_canceltype = PTHREAD_CANCEL_DEFERRED;
		  rt_pthreads[j].p_canceled = 0;
		  rt_pthreads[j].p_errno = 0;
		  rt_pthreads[j].p_start_args.schedpolicy = SCHED_OTHER;
		  
            if(attr != NULL && attr->schedpolicy != SCHED_OTHER) {
                switch(attr->inheritsched) {
                case PTHREAD_EXPLICIT_SCHED:
                    rt_pthreads[j].p_start_args.schedpolicy = attr->schedpolicy;
                    rt_pthreads[j].p_start_args.schedparam = attr->schedparam;
                    break;
                case PTHREAD_INHERIT_SCHED:
                  break;
                }
                rt_pthreads[j].p_priority =
                rt_pthreads[j].p_start_args.schedparam.sched_priority;
                rt_pthreads[j].p_policy =
                rt_pthreads[j].p_start_args.schedpolicy;
            }
            rt_pthreads[j].p_start_args.start_routine = start_routine;
            rt_pthreads[j].p_start_args.arg = arg;
            if(rt_pthreads[j].p_priority > PTHREAD_PRIORITY_FACTOR) {
                rt_pthreads[j].p_priority = PTHREAD_PRIORITY_FACTOR;
            }
            rt_pthreads[j].thread.priority =
                   PTHREAD_PRIORITY_FACTOR - rt_pthreads[j].p_priority;

            rc = rth_init(&rt_pthreads[j].thread,
			   (RTH_THREAD)start_routine,
			   (int)arg, 
			   RTH_PTHREAD_DEFAULT_STACK_SIZE,
			   RTH_PTHREAD_DEFAULT_PRIORITY);

            if( rc == 0 ) {
		*thread = rt_pthreads[j].p_tid = j + 1;

		rt_pthreads[j].thread.priv = (void*)&rt_pthreads[j];
#if 1
		rth_set_sched_policy(&rt_pthreads[j].thread, RTH_SCHED_RR,
				     1000000);
#endif
                if( rth_resume(&rt_pthreads[j].thread) < 0 ) {
		    hard_save_flags_and_cli(flags);
                    rth_delete(&rt_pthreads[j].thread);
                    rt_pthreads[j].p_tid = 0;

		    hard_restore_flags(flags);
                    return EAGAIN;
                }

		hard_restore_flags(flags);
                return 0;
            }
	    else {
                hard_restore_flags(flags);
                return EAGAIN;
            }

        }

    }

    hard_restore_flags(flags);

    return EAGAIN;

}

void pthread_exit(void* retval)
{
	((rt_pthread_t)(rth_self()->priv))->p_tid = 0;
	rth_delete(rth_self());
}

int pthread_attr_init(pthread_attr_t* attr)
{
	attr->detachstate = PTHREAD_CREATE_JOINABLE;
	attr->schedpolicy = SCHED_OTHER;
	attr->schedparam.sched_priority = 0;
	attr->inheritsched = PTHREAD_EXPLICIT_SCHED;
	attr->scope = PTHREAD_SCOPE_SYSTEM;
	return 0;
}

int pthread_attr_destroy(pthread_attr_t* attr)
{
	return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate)
{
	if ((detachstate < PTHREAD_CREATE_JOINABLE) ||
	    (detachstate > PTHREAD_CREATE_DETACHED))
		return EINVAL;

	attr->detachstate = detachstate;
	return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate)
{
	*detachstate = attr->detachstate;
	return 0;
}

int pthread_attr_setschedparam(pthread_attr_t* attr,
			       const struct sched_param* param)
{
	if (param->sched_priority < get_min_priority(attr->schedpolicy) ||
	    param->sched_priority > get_max_priority(attr->schedpolicy))
		return EINVAL;

	attr->schedparam = *param;
	return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t* attr,
			       struct sched_param* param)
{
	*param = attr->schedparam;
	return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy)
{
	if ((policy != SCHED_OTHER) &&
	    (policy != SCHED_FIFO) &&
	    (policy != SCHED_RR))
		return EINVAL;

	attr->schedpolicy = policy;
	return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t* attr, int* policy)
{
	*policy = attr->schedpolicy;
	return 0;
}

int pthread_attr_setinheritsched(pthread_attr_t* attr, int inherit)
{
	if ((inherit != PTHREAD_INHERIT_SCHED) &&
	    (inherit != PTHREAD_EXPLICIT_SCHED))
		return EINVAL;

	attr->inheritsched = inherit;
	return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t* attr, int* inherit)
{
	*inherit = attr->inheritsched;
	return 0;
}

int pthread_attr_setscope(pthread_attr_t* attr, int scope)
{
	switch (scope) {
	case PTHREAD_SCOPE_SYSTEM:
		attr->scope = scope;
		return 0;
	case PTHREAD_SCOPE_PROCESS:
		return ENOTSUP;
	}

	return EINVAL;
}

int pthread_attr_getscope(const pthread_attr_t* attr, int* scope)
{
	*scope = attr->scope;
	return 0;
}

int pthread_setschedparam(pthread_t tid, int policy,
                          const struct sched_param* param)
{
	int max_prio = get_max_priority(policy);
	int min_prio = get_min_priority(policy);
	rt_pthread_t pth;
	unsigned long flags;

	pth = &rt_pthreads[tid-1];

	if (pth->p_tid == 0)
		return ESRCH;

	if ((min_prio < 0) || (max_prio < 0))
		return EINVAL;

	if ((param->sched_priority < min_prio) ||
	    (param->sched_priority > max_prio))
		return EINVAL;

	hard_save_flags_and_cli(flags);

	pth->p_priority = param->sched_priority;
	pth->p_policy   = policy;
	pth->thread.priority      = PTHREAD_PRIORITY_FACTOR - pth->p_priority;
	pth->thread.base_priority = PTHREAD_PRIORITY_FACTOR - pth->p_priority;

	hard_restore_flags(flags);

	return 0;
}

int pthread_getschedparam(pthread_t tid, int* policy,
			  struct sched_param* param)
{
	rt_pthread_t pth;

	pth = &rt_pthreads[tid-1];

	if (pth->p_tid == 0)
		return ESRCH;

	*policy = pth->p_policy;
	param->sched_priority = pth->p_priority;

	return 0;
}

int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* mutex_attr)
{
	mutex->m_owner = 0;
	mutex->m_kind = (!mutex_attr) ?
	  PTHREAD_MUTEX_FAST_NP : mutex_attr->mutexkind;

	rsem_init(&mutex->m_semaphore, 1, RSEM_RES);

	return 0;
} 

int pthread_mutex_destroy(pthread_mutex_t* mutex)
{
	if (!mutex->m_semaphore.queue.prev) {
		rsem_init(&mutex->m_semaphore, 1, RSEM_RES);
	}
	
	if (rsem_wait_if(&mutex->m_semaphore) > 0) {
		rsem_signal(&mutex->m_semaphore);
		return rsem_delete(&mutex->m_semaphore);
	}

	return EBUSY;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex)
{
	int rc;

	if (!mutex->m_semaphore.queue.prev) {
		rsem_init(&mutex->m_semaphore, 1, RSEM_RES);
	}
	
	rc = (rsem_wait_if(&mutex->m_semaphore) > 0) ? 0 : EBUSY;

	mutex->m_owner = mutex->m_semaphore.owner;

	return rc;
}

int pthread_mutex_lock(pthread_mutex_t* mutex)
{
	int rc;

	if (!mutex->m_semaphore.queue.prev) {
		rsem_init(&mutex->m_semaphore, 1, RSEM_RES);
	}
	
	rc = (rsem_wait(&mutex->m_semaphore) < RSEM_TIMOUT) ? 0 : EINVAL;

	mutex->m_owner = mutex->m_semaphore.owner;

	return rc;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
	int rc;

	if (!mutex->m_semaphore.queue.prev) {
		rsem_init(&mutex->m_semaphore, 1, RSEM_RES);
	}

	rc = (rsem_signal(&mutex->m_semaphore) < RSEM_TIMOUT) ? 0 : EINVAL;

	mutex->m_owner = mutex->m_semaphore.owner;

	return rc;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
	attr->mutexkind = PTHREAD_MUTEX_FAST_NP;
	return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
	return 0;
}

int pthread_mutexattr_setkind_np(pthread_mutexattr_t* attr, int kind)
{
	if ((kind != PTHREAD_MUTEX_FAST_NP) &&
	    (kind != PTHREAD_MUTEX_RECURSIVE_NP) &&
	    (kind != PTHREAD_MUTEX_ERRORCHECK_NP))
		return EINVAL;

	attr->mutexkind = kind;
	return 0;
}

int pthread_mutexattr_getkind_np(const pthread_mutexattr_t* attr, int* kind)
{
	*kind = attr->mutexkind;
	return 0;
}

int pthread_cond_init(pthread_cond_t* cond,
                      const pthread_condattr_t* cond_attr)
{
	rsem_init(&cond->c_waiting, 0, RSEM_BIN);
	return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond)
{
	if (!cond->c_waiting.queue.prev) {
		rsem_init(&cond->c_waiting, 0, RSEM_BIN);
	}

	rsem_delete(&cond->c_waiting);
	return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
	if (!cond->c_waiting.queue.prev) {
		rsem_init(&cond->c_waiting, 0, RSEM_BIN);
	}

	rsem_signal(&mutex->m_semaphore);
	rsem_wait(&cond->c_waiting);
	rsem_wait(&mutex->m_semaphore);
	return 0;
}

int pthread_cond_timedwait(pthread_cond_t* cond,
			   pthread_mutex_t* mutex,
                           const struct timespec* abstime) 
{
	RTIME time = rt_timespec2count(abstime);
	int rc = 0;

	if (!cond->c_waiting.queue.prev) {
		rsem_init(&cond->c_waiting, 0, RSEM_BIN);
	}

	rsem_signal(&mutex->m_semaphore);
	rc = rsem_wait_until(&cond->c_waiting, time);
	rsem_wait(&mutex->m_semaphore);
	return (rc >= RSEM_TIMOUT) ? -1 : 0;
}

int pthread_cond_signal(pthread_cond_t* cond)
{
	if (!cond->c_waiting.queue.prev) {
		rsem_init(&cond->c_waiting, 0, RSEM_BIN);
	}

	return rsem_signal(&cond->c_waiting);
}

int pthread_cond_broadcast(pthread_cond_t* cond)
{
	if (!cond->c_waiting.queue.prev) {
		rsem_init(&cond->c_waiting, 0, RSEM_BIN);
	}

	return rsem_broadcast(&cond->c_waiting);
}

int pthread_condattr_init(pthread_condattr_t* attr)
{
	return 0;
}

int pthread_condattr_destroy(pthread_condattr_t* attr)
{
	return 0;
}

int pthread_suspend_np(pthread_t thread)
{
	RTH* th = &rt_pthreads[thread-1].thread;
	if (th) {
		return rth_suspend(th);
	}
	return EINVAL;
} 

int pthread_resume_np(pthread_t thread, int flag)
{
	RTH* th = &rt_pthreads[thread-1].thread;
	if (th) {
		switch (flag) {
		case PTHREAD_COUNT_RESUME_NP:
			return rth_suspend(th);
		case PTHREAD_FORCE_RESUME_NP:
		        th->suspend = 1;
			return rth_suspend(th);
		}
	}
	return EINVAL;
} 

int nanosleep(const struct timespec* ts1, struct timespec* ts2)
{
	RTIME expire;

	if ((ts1->tv_nsec >= 1000000000L) ||
	    (ts1->tv_nsec < 0) ||(ts1->tv_sec < 0))
		return -EINVAL;

	expire = rt_get_time() + rt_timespec2count(ts1);
	rth_sleep(rt_timespec2count(ts1));

	expire -= rt_get_time();
	if (expire > 0) {
		if (ts2) {
			rt_count2timespec(expire, ts2);
		}
		return -EINTR;
	}

	return 0;
}

int sched_yield(void)
{
	rth_yield();
	return 0;
}

int rt_pthread_init(void* p)
{
	return 0;
}



typedef struct __M {
	struct __M* next;
	struct __M* prev;
	size_t size;
	u32    flag;
} M;


static __inline__ void __rt_listdel(M* prev, M* next)
{
	next->prev = prev;
	prev->next = next;
}

static __inline__ void rt_listdel(M* p)
{
	__rt_listdel(p->prev, p->next);
}

static __inline__ void __rt_listadd(M* p, M* prev, M* next)
{
	p->next    = next;
	p->prev    = prev;
	next->prev = p;
	prev->next = p;
}

static __inline__ void rt_listadd(M* p, M* head)
{
	__rt_listadd(p, head, head->next);
	p->flag = 0;
}

static __inline__ void rt_listadd_tail(M* p, M* head)
{
	__rt_listadd(p, head->prev, head);
	p->flag = 0;
}

/************************************************
 *						*
 *	Memory control module for rt_schedule   *
 *						*
 ************************************************/

static u32 KMEM_BASE = 0;
static u32 KMEM_SIZE = 0;

unsigned char rt_mem_buffer[CONFIG_RT_MEM_SIZE];

u32 rt_mem_base;
u32 rt_mem_size;

u32 rt_mem_total_size;
u32 rt_mem_free_size;


static void rt_mem_init(void)
{
	M* root; M* cp; M* ep;

	if (KMEM_BASE) return;

	rt_mem_base = (u32)rt_mem_buffer;
	rt_mem_size = CONFIG_RT_MEM_SIZE;

	KMEM_BASE = rt_mem_base;
	KMEM_SIZE = rt_mem_size;

	rt_mem_total_size = rt_mem_size;
	rt_mem_free_size  = rt_mem_size;

	root = (M*)KMEM_BASE;
	root->next = (M*)(KMEM_BASE + sizeof(M));
	root->flag = 0;

	ep = root->next;
	ep->size = 0;
	ep->next = (M*)((u32)ep + sizeof(M));
	ep->prev = (M*)root;
	ep->flag = 0;
	cp = ep->next;
		
	cp->size = KMEM_SIZE - sizeof(M) - sizeof(M) * 2;
	cp->prev = ep;
	cp->next = (M*)root;
	cp->flag = 0;

	root->prev = cp;
	root->size = cp->size;

}

void* rt_malloc(size_t size) 
{
	M* root;
	M* cp;
	M* np;
	M* pt;
	u32	flags;

	if (size == 0)
		return (void*)0;

	if (!KMEM_BASE) { rt_mem_init(); }
		
	hard_save_flags_and_cli(flags);

	root = (M*)KMEM_BASE;
	cp = root->next;
	pt = root->next;

	size = ((size + sizeof(u32) - 1) / sizeof(u32)) * sizeof(u32);

	while (cp != root) {
		if(cp->flag == 0) {
			if (cp->size >= size + sizeof(M)) {
				break;
			}
			cp->flag = 0;
		}
		cp = cp->next;
	}
	
	if (cp == root) {
		hard_restore_flags(flags);
		return (void*)0;
	}

	if (cp->size > (size + sizeof(M))) {
		size_t all = size + sizeof(M);
		size_t new_size = cp->size - all;

		cp->size = size;

		rt_listdel(cp);

		np = (M*)((u32)cp + all);
		np->size = new_size;

		while (pt != root) {
			if (pt->size >= new_size || pt->next == root) {
				if (pt->next == root) {
					rt_listadd(np, pt);
				}
				else {
					rt_listadd_tail(np, pt);
				}
				break;
			}
			pt = pt->next;
		}
	}
 	else if ((cp->prev == root->next) && (cp->next == root)) {
		hard_restore_flags(flags);
		return (void*)0;
	}
	else {
		rt_listdel(cp);
	}

	rt_mem_free_size -= (sizeof(M) + cp->size);
	hard_restore_flags(flags);

	return (M*)((u32)cp + sizeof(M));
}

void rt_free(void* fp)
{
	M* root = (M*)KMEM_BASE;
	M* cp   = fp - sizeof(M);
	M* pt;
	M* bt;
	M* nt;
	u32	flags;

	if (fp == (void*)0)
		return;

	hard_save_flags_and_cli(flags);

	pt = root->next;
	bt = root->next->next;
	nt = root->next->next;
	while(nt != root) {
		if(nt == cp) {
			hard_restore_flags(flags);
			return;
		}
		nt = nt->next;
	}

	rt_mem_free_size += (sizeof(M) + cp->size);

	nt = root->next->next;

	while (nt != root) {
		if (nt == (M*)((u32)cp + cp->size + sizeof(M))) {
			rt_listdel(nt);
			cp->size += nt->size + sizeof(M);
			break;
		}
		nt = nt->next;
	}
	while (bt != root) {
		if (cp == (M*)((u32)bt + bt->size + sizeof(M))) {
			rt_listdel(bt);
			bt->size += cp->size + sizeof(M);
			cp = bt;
			break;
		}
		bt = bt->next;
	}

	for (; pt != root; pt = pt->next) {

		if (pt->size >= cp->size || pt->next == root) {

			if (pt->next == root) {
				if (pt->size < cp->size) {
					rt_listadd(cp, pt);
				}
				else {
					rt_listadd_tail(cp, pt);
				}
			}
			else {
				rt_listadd_tail(cp, pt);
			}
			break;
		}
	}
	hard_restore_flags(flags);
}

EXPORT_SYMBOL(rth_current);
EXPORT_SYMBOL(rth_base);
EXPORT_SYMBOL(rth_btime);

