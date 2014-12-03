/*
 * linux/arch/sh/kernel/rthal.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * I/O routine and setup routines for SH-Solution Engine board.
 *
 * Based on:
 *	rtai/arch/arm/rtai.c
 */
/* 020222 rtai/arch/arm/rtai.c
COPYRIGHT (C) 2002 Guennadi Liakhovetski, DSA GmbH (gl@dsa-ac.de)
COPYRIGHT (C) 2002 Wolfgang Müller (wolfgang.mueller@dsa-ac.de)
COPYRIGHT (C) 2002 Thomas Gleixner (gleixner@autronix.de)
Copyright (c) 2001 Alex Züpke, SYSGO RTS GmbH (azu@sysgo.de)

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
/*
--------------------------------------------------------------------------
Changelog

03-10-2002	TG	added support for trap handling
			added function rt_is_linux
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>

#include <asm/system.h>
#include <asm/smp.h>
#include <asm/io.h>
#include <asm/atomic.h>

#include <linux/rt_sched.h>
#include <asm/rthal.h>

#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
static struct proc_dir_entry* proc_rtinfo;
#endif

static void linux_cli(void)
{
	hard_cli();
}

static void linux_sti(void)
{
	hard_sti();
}

static void linux_save_flags(unsigned long* fp)
{ 
	unsigned long flags;
	hard_save_flags(flags);
	*fp = flags;
}

static void linux_restore_flags(unsigned long flags)
{
	hard_restore_flags(flags); 
}

static void linux_save_flags_and_cli(unsigned long* fp)
{ 
	unsigned long flags;
	hard_save_flags_and_cli(flags);
	*fp = flags;
}

static unsigned long linux_save_and_cli(void)
{ 
	return hard_save_and_cli();
}

extern unsigned int do_IRQx(int, struct pt_regs*);

struct rt_hal rthal = {
	do_IRQx,
	linux_cli,
	linux_sti,
	linux_save_flags,
	linux_restore_flags,
	linux_save_flags_and_cli,
	linux_save_and_cli,
};

#if defined(CONFIG_RTSCHED)

extern RTH*  rth_current;
extern RTH   rth_base;
extern RTIME rth_btime;

#define MAX_PENDING_IRQS  256
#define INC_WRAP(x,y) do { x = ++(x) & ((y)-1); } while (0)

static struct rt_hal rthal0;

struct rt_ctrl {
	volatile int            in;
	volatile int            out;
	volatile int            losts;
	volatile int            pending_irq[MAX_PENDING_IRQS];
	volatile struct pt_regs pending_regs[MAX_PENDING_IRQS];
#if defined(CONFIG_RT_DEBUG)
	volatile RTIME          pending_time[MAX_PENDING_IRQS];
#endif
	volatile unsigned int   used;
	volatile unsigned int   sti;
	spinlock_t              data_lock;
	spinlock_t              ctrl_lock;
} ctrl0;

struct rt_cpu {
	volatile unsigned long intr_flag;
	volatile unsigned long base_intr_flag;
} cpu0;

#ifndef IRQ_TIME_LOG_SIZE 
#if defined(CONFIG_RT_IRQ_TIME_LOG_SIZE)
#define IRQ_TIME_LOG_SIZE CONFIG_RT_IRQ_TIME_LOG_SIZE
#else
#define IRQ_TIME_LOG_SIZE 3
#endif
#endif

static struct rt_handler {
	void          (*handler)(void);
	char*         name;
#if defined(CONFIG_RT_DEBUG)
	unsigned long interrupt_count;
	unsigned long pending_count;
	RTIME         interrupt_time;
	RTIME         pending_time[IRQ_TIME_LOG_SIZE];
	RTIME         handler_start[IRQ_TIME_LOG_SIZE];
	RTIME         handler_end[IRQ_TIME_LOG_SIZE];
	RTIME         handler_time_min;
	RTIME         handler_time_max;
#endif
} irq_handler0[NR_IRQS];

struct rt_times       rt_times;
struct rt_calibration rt_calibration;

static struct hw_interrupt_type* irq_desc0[NR_IRQS];

static unsigned long irq_action0[NR_IRQS];
static int irq_chain0[NR_IRQS];

static void (*irq_ack0[NR_IRQS])(rt_irq_t);
static void (*irq_unmask0[NR_IRQS])(rt_irq_t);
static void (*irq_end0[NR_IRQS])(rt_irq_t);

static void  (*timer_handler)(void);
static void* save_rt_timer_handler = (void*)0;

union rt_tsc {
	unsigned long long tsc;
	unsigned long hltsc[2];
} rt_tsc;

static void rthal_cli(void);
static void rthal_sti_flag(void);

void (*_rthal_sti_flag)(void) = (void *)rthal_sti_flag;
void (*_rthal_cli)(void) = (void *)rthal_cli;
unsigned long rthal_interrupt_time0;

#define TSC_MAX_COUNT (unsigned long)0xFFFFFFFF
#define TSC_LONG_COUNT (unsigned long)0x7FFFFFFF
#define TMU1_TCR_INIT  0x0000
#define TMU2_TCR_INIT  0x0000
#define TMU0_START     0x01
#define TMU1_START     0x02
#define TMU2_START     0x04

#define TMU1_IRQ	17
#define TMU1_IPR_ADDR	INTC_IPRA
#define TMU1_IPR_POS	 2
#define TMU1_PRIORITY	 2

#if defined(__sh3__)
#define TMU_TOCR	0xfffffe90
#define TMU_TSTR	0xfffffe92
#define TMU0_TCOR	0xfffffe94
#define TMU0_TCNT	0xfffffe98
#define TMU0_TCR	0xfffffe9c
#define TMU1_TCOR	0xfffffea0
#define TMU1_TCNT	0xfffffea4
#define TMU1_TCR	0xfffffea8
#define TMU2_TCOR	0xfffffeac
#define TMU2_TCNT	0xfffffeb0
#define TMU2_TCR	0xfffffeb4
#elif defined(__SH4__)
#define TMU_TOCR	0xffd80000
#define TMU_TSTR	0xffd80004
#define TMU0_TCOR	0xffd80008
#define TMU0_TCNT	0xffd8000c
#define TMU0_TCR	0xffd80010
#define TMU1_TCOR	0xffd80014
#define TMU1_TCNT	0xffd80018
#define TMU1_TCR	0xffd8001c
#define TMU2_TCOR	0xffd80020
#define TMU2_TCNT	0xffd80024
#define TMU2_TCR	0xffd80028
#endif

static unsigned int rthal_sti(void);

void rt_cli(void)
{
	hard_cli();
}

void rt_sti(void)
{
	hard_sti();
}

unsigned long rt_save_flags_and_cli(void)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);
	return flags;
}

void rt_save_flags(unsigned long* fp)
{
	unsigned long flags;

	hard_save_flags(flags);
	*fp = flags;
}

void rt_restore_flags(unsigned long flags)
{
	if (flags & IBIT) {
		hard_cli();
	}
	else {
		hard_sti();
	}
}

static void rthal_null_handler(int irq, void* dev_id, struct pt_regs* regs)
{
}

static void rthal_null_irq(rt_irq_t irq)
{
}

static inline int rthal_have_pending_irq(void)
{
	return (ctrl0.pending_irq[ctrl0.out] != -1);
}

static inline rt_irq_t rthal_pending_irq(void)
{
	return ctrl0.pending_irq[ctrl0.out];
}

static inline struct pt_regs* rthal_pending_regs(void)
{
	return (struct pt_regs*)(&ctrl0.pending_regs[ctrl0.out]);
}
#if defined(CONFIG_RT_DEBUG)
static inline RTIME rthal_pending_time(void)
{
	return ctrl0.pending_time[ctrl0.out];
}
#endif
void rt_pending_irq(rt_irq_t irq, struct pt_regs* regs)
{
	rt_spin_lock_irq(&ctrl0.data_lock);
	if (ctrl0.pending_irq[ctrl0.in] != -1) {
		ctrl0.losts++;
	}
	ctrl0.pending_irq[ctrl0.in] = irq;
	ctrl0.pending_regs[ctrl0.in] = *regs;
#if defined(CONFIG_RT_DEBUG)
	ctrl0.pending_time[ctrl0.in] = rt_get_time();
#endif
	INC_WRAP(ctrl0.in, MAX_PENDING_IRQS);
	rt_spin_unlock_irq(&ctrl0.data_lock);
}

static inline void rt_clear_pending_irq(rt_irq_t irq)
{
	rt_spin_lock_irq(&ctrl0.data_lock);
	ctrl0.pending_irq[ctrl0.out] = -1;
	INC_WRAP(ctrl0.out, MAX_PENDING_IRQS);
	rt_spin_unlock_irq(&ctrl0.data_lock);
}

static inline void rthal_init_pending_irq(void)
{
	int i;

	ctrl0.in = ctrl0.out = ctrl0.losts = 0;
	for (i = 0; i < MAX_PENDING_IRQS; i++) {
		ctrl0.pending_irq[i] = -1;
	}
}

unsigned int rt_startup_irq(rt_irq_t irq)
{
	struct hw_interrupt_type* irq_desc = irq_desc0[irq];
	unsigned long flags;
	unsigned int rc = 0;

	if ((irq_desc) && (irq_desc->startup)) {
		flags = rt_spin_lock_irqsave(&ctrl0.ctrl_lock);
		rc = irq_desc->startup(irq);
		rt_spin_unlock_irqrestore(flags, &ctrl0.ctrl_lock);
	}

	return rc;
}

void rt_shutdown_irq(rt_irq_t irq)
{
	struct hw_interrupt_type* irq_desc = irq_desc0[irq];
	unsigned long flags;

	if ((irq_desc) && (irq_desc->shutdown)) {
		flags = rt_spin_lock_irqsave(&ctrl0.ctrl_lock);
		irq_desc->shutdown(irq);
		rt_spin_unlock_irqrestore(flags, &ctrl0.ctrl_lock);
	}
}

void rt_enable_irq(rt_irq_t irq)
{
	struct hw_interrupt_type* irq_desc = irq_desc0[irq];
	unsigned long flags;

	if ((irq_desc) && (irq_desc->enable)) {
		flags = rt_spin_lock_irqsave(&ctrl0.ctrl_lock);
		irq_desc->enable(irq);
		rt_spin_unlock_irqrestore(flags, &ctrl0.ctrl_lock);
	}
}

void rt_disable_irq(rt_irq_t irq)
{
	struct hw_interrupt_type* irq_desc = irq_desc0[irq];
	unsigned long flags;

	if ((irq_desc) && (irq_desc->disable)) {
		flags = rt_spin_lock_irqsave(&ctrl0.ctrl_lock);
		irq_desc->disable(irq);
		rt_spin_unlock_irqrestore(flags, &ctrl0.ctrl_lock);
	}
}

void rt_mask_ack_irq(rt_irq_t irq)
{
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&ctrl0.ctrl_lock);

	if (irq_ack0[irq]) {
		irq_ack0[irq](irq);
	}

	rt_spin_unlock_irqrestore(flags, &ctrl0.ctrl_lock);
}

void rt_unmask_irq(rt_irq_t irq)
{
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&ctrl0.ctrl_lock);

	if (irq_unmask0[irq]) {
		irq_unmask0[irq](irq);
	}

	rt_spin_unlock_irqrestore(flags, &ctrl0.ctrl_lock);
}

static asmlinkage unsigned int rthal_do_irq(int irq, struct pt_regs* regs)
{
#if defined(CONFIG_RT_DEBUG)
	RTIME t;
	unsigned long count;
	int i;

	irq_handler0[irq].interrupt_count++;
	irq_handler0[irq].interrupt_time = rt_get_time();
	count = 0xffffffff - rthal_interrupt_time0;
	irq_handler0[irq].interrupt_time &= 0xFFFFFFFF00000000;
	irq_handler0[irq].interrupt_time |= count;
	if (count > rt_tsc.hltsc[0]) {
		irq_handler0[irq].interrupt_time -= 0x0000000100000000;
	}
#endif
	if (irq == RT_TIMER_IRQ) {
#if defined(CONFIG_RT_DEBUG)
		for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
			irq_handler0[irq].pending_time[i] =
			  irq_handler0[irq].pending_time[i-1];
			irq_handler0[irq].handler_start[i] =
			  irq_handler0[irq].handler_start[i-1];
		}
		irq_handler0[irq].pending_time[0] = 0;
		irq_handler0[irq].handler_start[0] = rt_get_time();
#endif
		if (irq_handler0[irq].handler) {
			((void (*)(int))irq_handler0[irq].handler)(irq);
		}
#if defined(CONFIG_RT_DEBUG)
		for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
			irq_handler0[irq].handler_end[i] =
			  irq_handler0[irq].handler_end[i-1];
		}
		irq_handler0[irq].handler_end[0] = rt_get_time();
		t = irq_handler0[irq].handler_end[0] -
		  irq_handler0[irq].handler_start[0];
		if ((irq_handler0[irq].handler_time_min == 0) ||
		    (t < irq_handler0[irq].handler_time_min)) {
			irq_handler0[irq].handler_time_min = t;
		}
		if (t > irq_handler0[irq].handler_time_max) {
			irq_handler0[irq].handler_time_max = t;
		}
#endif
		if ((ctrl0.used) && (cpu0.intr_flag)) {
			rthal_sti();
			hard_cli();
			return 1;
		}
		hard_cli();
		return 0;
	}
	else if (irq == TIMER_IRQ) {
#if !defined(CONFIG_RT_DEBUG)
		rt_get_time(); /* for update */
#endif
		if (!in_interrupt() &&
		    (!ctrl0.used || (ctrl0.used && cpu0.intr_flag))) {

			struct hw_interrupt_type* irq_desc = irq_desc0[irq];
			unsigned long save_intr_flag = cpu0.intr_flag;
			
			irq_desc->disable(irq);
			cpu0.intr_flag = 0;

			hard_sti();
#if defined(CONFIG_RT_DEBUG)
			for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
				irq_handler0[irq].pending_time[i] =
				  irq_handler0[irq].pending_time[i-1];
				irq_handler0[irq].handler_start[i] =
				  irq_handler0[irq].handler_start[i-1];
			}
			irq_handler0[irq].pending_time[0] = 0;
			irq_handler0[irq].handler_start[0] = rt_get_time();
#endif
			rthal0.do_IRQ(irq, regs);
#if defined(CONFIG_RT_DEBUG)
			for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
				irq_handler0[irq].handler_end[i] =
				  irq_handler0[irq].handler_end[i-1];
			}
			irq_handler0[irq].handler_end[0] = rt_get_time();
			t = irq_handler0[irq].handler_end[0] -
			  irq_handler0[irq].handler_start[0];
			if ((irq_handler0[irq].handler_time_min == 0) ||
			    (t < irq_handler0[irq].handler_time_min)) {
				irq_handler0[irq].handler_time_min = t;
			}
			if (t > irq_handler0[irq].handler_time_max) {
				irq_handler0[irq].handler_time_max = t;
			}
#endif
			hard_cli();
			
			cpu0.intr_flag = save_intr_flag;
			irq_desc->enable(irq);

			return 1;
		}
	}

	if ((irq >= 0) && (irq < NR_IRQS)) {
		int is_spin_lock = 0;

		irq_ack0[irq](irq);

		if (irq_handler0[irq].handler) {
#if defined(CONFIG_RT_DEBUG)
			for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
				irq_handler0[irq].pending_time[i] =
				  irq_handler0[irq].pending_time[i-1];
				irq_handler0[irq].handler_start[i] =
				  irq_handler0[irq].handler_start[i-1];
			}
			irq_handler0[irq].pending_time[0] = 0;
			irq_handler0[irq].handler_start[0] = rt_get_time();
#endif
			((void (*)(int))irq_handler0[irq].handler)(irq);
#if defined(CONFIG_RT_DEBUG)
			for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
				irq_handler0[irq].handler_end[i] =
				  irq_handler0[irq].handler_end[i-1];
			}
			irq_handler0[irq].handler_end[0] = rt_get_time();
			t = irq_handler0[irq].handler_end[0] -
			  irq_handler0[irq].handler_start[0];
			if ((irq_handler0[irq].handler_time_min == 0) ||
			    (t < irq_handler0[irq].handler_time_min)) {
				irq_handler0[irq].handler_time_min = t;
			}
			if (t > irq_handler0[irq].handler_time_max) {
				irq_handler0[irq].handler_time_max = t;
			}
#endif
			irq_unmask0[irq](irq);
			rt_spin_lock_irq(&ctrl0.data_lock);
			is_spin_lock = 1;
		}
		else if (ctrl0.used && cpu0.intr_flag) {
			struct hw_interrupt_type* irq_desc = irq_desc0[irq];
			unsigned long save_intr_flag = cpu0.intr_flag;
			
			irq_desc->disable(irq);
			cpu0.intr_flag = 0;

			hard_sti();
#if defined(CONFIG_RT_DEBUG)
			for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
				irq_handler0[irq].pending_time[i] =
				  irq_handler0[irq].pending_time[i-1];
				irq_handler0[irq].handler_start[i] =
				  irq_handler0[irq].handler_start[i-1];
			}
			irq_handler0[irq].pending_time[0] = 0;
			irq_handler0[irq].handler_start[0] = rt_get_time();
#endif
			rthal0.do_IRQ(irq, regs);
#if defined(CONFIG_RT_DEBUG)
			{
				for (i = IRQ_TIME_LOG_SIZE - 1; i > 0; i--) {
				  irq_handler0[irq].handler_end[i] =
				    irq_handler0[irq].handler_end[i-1];
				}
			}
			irq_handler0[irq].handler_end[0] = rt_get_time();
			t = irq_handler0[irq].handler_end[0] -
			  irq_handler0[irq].handler_start[0];
			if ((irq_handler0[irq].handler_time_min == 0) ||
			    (t < irq_handler0[irq].handler_time_min)) {
				irq_handler0[irq].handler_time_min = t;
			}
			if (t > irq_handler0[irq].handler_time_max) {
				irq_handler0[irq].handler_time_max = t;
			}
#endif
			hard_cli();
			
			cpu0.intr_flag = save_intr_flag;
			irq_desc->enable(irq);

			return 1;
		}
		else {
#if defined(CONFIG_RT_DEBUG)
			irq_handler0[irq].pending_count++;
#endif
			rt_pending_irq(irq, regs);
		}

		if ((ctrl0.used) && (cpu0.intr_flag)) {
			if (is_spin_lock) {
				rt_spin_unlock_irq(&(ctrl0.data_lock));
			}

			rthal_sti();
			hard_cli();
			return  1;
		}
		if (is_spin_lock) {
			rt_spin_unlock_irq(&(ctrl0.data_lock));
		}
	}
	
	hard_cli();

	return 0;
}

static void rthal_cli(void)
{
	cpu0.intr_flag = 0;
}

static inline void rthal_clear_sti(volatile int* addr)
{
	*addr = 0;
}

static void rthal_sti_flag(void)
{
	cpu0.intr_flag = IBIT;
}

static unsigned int rthal_sti(void)
{
      	struct rt_cpu* cpu = &cpu0;
 	rt_irq_t irq;
	struct pt_regs rt_regs;
#if defined(CONFIG_RT_DEBUG)
	RTIME t;
	int i;
#endif

	rt_spin_lock_irq(&(ctrl0.data_lock));
	cpu->intr_flag = 0;
       	while ((irq = rthal_pending_irq()) != -1) {
		rt_regs = *rthal_pending_regs();
#if defined(CONFIG_RT_DEBUG)
		t = rthal_pending_time();
#endif
		rt_clear_pending_irq(irq);

		rt_spin_unlock_irq(&(ctrl0.data_lock));
#if defined(CONFIG_RT_DEBUG)
		for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
			irq_handler0[irq].pending_time[i] =
			  irq_handler0[irq].pending_time[i-1];
			irq_handler0[irq].handler_start[i] =
			  irq_handler0[irq].handler_start[i-1];
		}
		irq_handler0[irq].pending_time[0] = t;
		irq_handler0[irq].handler_start[0] = rt_get_time();
#endif
		rthal0.do_IRQ(irq, &rt_regs);
#if defined(CONFIG_RT_DEBUG)
		for (i = IRQ_TIME_LOG_SIZE-1; i > 0; i--) {
			irq_handler0[irq].handler_end[i] =
			  irq_handler0[irq].handler_end[i-1];
		}
		irq_handler0[irq].handler_end[0] = rt_get_time();
		t = irq_handler0[irq].handler_end[0] -
		  irq_handler0[irq].handler_start[0];
		if ((irq_handler0[irq].handler_time_min == 0) ||
		    (t < irq_handler0[irq].handler_time_min)) {
			irq_handler0[irq].handler_time_min = t;
		}
		if (t > irq_handler0[irq].handler_time_max) {
			irq_handler0[irq].handler_time_max = t;
		}
#endif
		rt_spin_lock_irq(&(ctrl0.data_lock));
	}
	cpu->intr_flag = IBIT;
	rt_spin_unlock_irq(&(ctrl0.data_lock));

	return 1;
}

static void rthal_save_flags(unsigned long* fp)
{
	unsigned long flags;

	hard_save_flags(flags);
	*fp = (flags & ~IBIT) |	(cpu0.intr_flag  ? 0 : IBIT);
}

static void rthal_restore_flags(unsigned long flags)
{
	if (flags & IBIT) {
		cpu0.intr_flag = 0;
	}
	else {
		rthal_sti();
	}
}

static void rthal_save_flags_and_cli(unsigned long* fp)
{
	unsigned long flags;

	rthal_save_flags(&flags);
        cpu0.intr_flag = 0;
	*fp = flags;
}

unsigned long rthal_save_and_cli(void)
{
	unsigned long flags;

	rthal_save_flags(&flags);
        cpu0.intr_flag = 0;
	return flags;
}

static unsigned int rthal_startup_irq(rt_irq_t irq)
{
	struct hw_interrupt_type* irq_desc = irq_desc0[irq];
	unsigned int rc = 0;

	if ((irq_desc) && (irq_desc->startup)) {
		rt_spin_lock_irq(&ctrl0.ctrl_lock);
		rc = irq_desc->startup(irq);
		rt_spin_unlock_irq(&ctrl0.ctrl_lock);
	}

	return rc;
}

static void rthal_shutdown_irq(rt_irq_t irq)
{
	struct hw_interrupt_type* irq_desc = irq_desc0[irq];

	if ((irq_desc) && (irq_desc->shutdown)) {
		rt_spin_lock_irq(&ctrl0.ctrl_lock);
		irq_desc->shutdown(irq);
		rt_spin_unlock_irq(&ctrl0.ctrl_lock);
	}
}

static void rthal_enable_irq(rt_irq_t irq)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_desc0[irq]->enable) {
		irq_desc0[irq]->enable(irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}

static void rthal_disable_irq(rt_irq_t irq)
{
#if 1
	rt_disable_irq(irq);
#else
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_end0[irq]) {
		irq_end0[irq](irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
#endif
}

#if 0
static void rthal_ack_irq(unsigned int irq)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_desc0[irq]->ack) {
		irq_desc0[irq]->ack(irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}
#endif

static void rthal_end_irq(unsigned int irq)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_desc0[irq]->end) {
		irq_desc0[irq]->end(irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}

static void rthal_set_affinity(unsigned int irq, unsigned long mask)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_desc0[irq]->set_affinity) {
		irq_desc0[irq]->set_affinity(irq, mask);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}


static struct hw_interrupt_type rthal_irq_type = { 
	typename:	"RTHAL",
	startup:	rthal_startup_irq,
	shutdown:	rthal_shutdown_irq,
	enable:		rthal_enable_irq,
	disable:	rthal_disable_irq,
	ack:		rthal_null_irq,
	end:		rthal_end_irq,
	set_affinity:	rthal_set_affinity,
};

static struct hw_interrupt_type rt_irq_type = { 
	typename:	"RT",
	startup:	(unsigned int (*)(unsigned int))rthal_null_irq,
	shutdown:	rthal_null_irq,
	enable:		rthal_null_irq,
	disable:	rthal_null_irq,
	ack:		rthal_null_irq,
	end:		rthal_null_irq,
	set_affinity:	NULL,
};

int rt_request_irq(rt_irq_t irq, void (*handler)(void))
{
	unsigned long flags;

	if ((irq >= NR_IRQS) || !(handler))
		return -EINVAL;

	if (irq_handler0[irq].handler)
		return -EBUSY;

	hard_save_flags_and_cli(flags);

	IRQ_DESC[irq].handler = &rt_irq_type;
	
	irq_handler0[irq].handler = handler;

	irq_end0[irq] = rthal_null_irq;

	hard_restore_flags(flags);

	return 0;
}

int rt_free_irq(rt_irq_t irq)
{
	unsigned long flags;

	if ((irq >= NR_IRQS) || !(irq_handler0[irq].handler))
		return -EINVAL;

	hard_save_flags_and_cli(flags);

	IRQ_DESC[irq].handler = &rthal_irq_type;

	irq_handler0[irq].handler = 0;

	irq_end0[irq] = irq_unmask0[irq];

	hard_restore_flags(flags);

	return 0;
}

int rthal_request_irq(rt_irq_t irq,
	void (*handler)(int irq, void* dev_id, struct pt_regs* regs),
	char* handler_id, void* dev_id)
{
	unsigned long flags;

	if (irq >= NR_IRQS || !handler)
		return -EINVAL;

	save_flags_and_cli(flags);

	if (!irq_chain0[irq]++) {
		if (IRQ_DESC[irq].action) {
			irq_action0[irq] = IRQ_DESC[irq].action->flags;
			IRQ_DESC[irq].action->flags |= SA_SHIRQ;
		}
	}

	restore_flags(flags);
	request_irq(irq, handler, SA_SHIRQ, handler_id, dev_id);

	return 0;
}

int rthal_free_irq(rt_irq_t irq, void* dev_id)
{
	unsigned long flags;

	if ((irq >= NR_IRQS) || !(irq_chain0[irq]))
		return -EINVAL;

	free_irq(irq, dev_id);
	save_flags_and_cli(flags);

	if (!(--irq_chain0[irq]) && (IRQ_DESC[irq].action)) {
		IRQ_DESC[irq].action->flags = irq_action0[irq];
	}

	restore_flags(flags);

	return 0;
}

void rthal_switch_cpu_flag(int mode)
{
	if (mode) {
		if (ctrl0.used) {
			cpu0.base_intr_flag = cpu0.intr_flag;
		}
		cpu0.intr_flag = 0;
		ctrl0.used = 0;
	}
	else {
		ctrl0.used = 1;
		cpu0.intr_flag = cpu0.base_intr_flag;
	}
}

RTIME rt_get_time(void)
{
	unsigned long flags, count;

	hard_save_flags_and_cli(flags);

	count = 0xffffffff - ctrl_inl(TMU2_TCNT);
	if (count < rt_tsc.hltsc[0]) {
		rt_tsc.hltsc[1]++;
	}
	rt_tsc.hltsc[0] = count;

	hard_restore_flags(flags);

	return rt_tsc.tsc;
}

static unsigned long timer_minimum_value;
void rthal_set_timer(int delay)
{
	unsigned long flags;
	
	hard_save_flags_and_cli(flags);

	{

		if (delay < timer_minimum_value) {
			delay = timer_minimum_value;
		}

		ctrl_outb(ctrl_inb(TMU_TSTR) & ~TMU1_START, TMU_TSTR);

		ctrl_outw(0x0020, TMU1_TCR);
		ctrl_outl(delay,  TMU1_TCOR);
		ctrl_outl(delay,  TMU1_TCNT);

		ctrl_outb(ctrl_inb(TMU_TSTR) | TMU1_START, TMU_TSTR);
	}
	
	hard_restore_flags(flags);
}

static void rthal_timer_handler(void)
{
	timer_handler();

	ctrl_outw(0x0020, TMU1_TCR);
}

void rthal_request_rt_timer(void (*handler)(void))
{
	unsigned long flags;
	RTIME t;

	hard_save_flags_and_cli(flags);

	rt_free_irq(RT_TIMER_IRQ);

	save_rt_timer_handler =
	  (void*)IRQ_DESC[RT_TIMER_IRQ].action->handler;
	IRQ_DESC[RT_TIMER_IRQ].action->handler = rthal_null_handler;

	timer_handler = handler;
	rt_request_irq(RT_TIMER_IRQ, rthal_timer_handler);

	rt_tsc.tsc = 0;

	rt_times.base_tick = TSC_LONG_COUNT;
	rt_times.periodic_tick = TSC_LONG_COUNT;
	rt_times.tick_time = t = rt_get_time();
	rt_times.intr_time = t + (RTIME)rt_times.periodic_tick;
	rt_times.base_time = t + (RTIME)rt_times.base_tick;

	rthal_set_timer(rt_times.base_tick);

	hard_restore_flags(flags);
	
}

#if defined(CONFIG_RTHAL_CALIBRATION)

unsigned long latency_match_reg, setup_time_match_reg;

#define PERIOD 50000
static RTIME next, trap_total, test_period;
static void trap(void); 
static void calibrate_latency(void);
static int cal_end = 0;
#endif

#if defined(CONFIG_PROC_FS) && defined(CONFIG_RT_DEBUG)

void proc_sprintf(char*, off_t*, int*, const char*, ...);
static int proc_calc_metrics(char *page, char **start, off_t off,
			     int count, int *eof, int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int proc_rtinfo_read(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int i, j, len = 0;
	RTIME t, t2;
	char name[32];
	char* p;
	RTH* th;

	proc_sprintf(page, &off, &len, "\n");
	proc_sprintf(page, &off, &len,
		     "RealTime scheduler version %s\n", 
		     RT_SCHED_VERSION);
	proc_sprintf(page, &off, &len, 
		     "Real-Time Hardware Abstraction Layer version %s\n" 
		     "Support kernel version 2.4.20 - 2.4.22\n",
		     RTHAL_VERSION);
	proc_sprintf(page, &off, &len, 
		     "TIMER INTERRUPT LATENCY = %5d nsec\n",
		     (int)LATENCY_MATCH_REG);
	t = rt_get_time();
	for (i = 0; i < 3; i++) {
	  t2 = rt_get_time();
	}
	t2 -= t;
	proc_sprintf(page, &off, &len, 
		     "rt_get_time() = %5d nsec\n", (int)rt_count2nano(t2 / 3));
	proc_sprintf(page, &off, &len, "\n");
	
	proc_sprintf(page, &off, &len, "IRQ: name        ");
	proc_sprintf(page, &off, &len, "intr_count pend_count ");
	proc_sprintf(page, &off, &len, "%20s", "");
	proc_sprintf(page, &off, &len, "intr_time");
	proc_sprintf(page, &off, &len, "\n");
	proc_sprintf(page, &off, &len, "%4s", "");
	proc_sprintf(page, &off, &len, "     pending_time");
	proc_sprintf(page, &off, &len, "    handler_start");
	proc_sprintf(page, &off, &len, "      handler_end");
	proc_sprintf(page, &off, &len, "    end-start");
	proc_sprintf(page, &off, &len, "\n");
	proc_sprintf(page, &off, &len, "%42s", "");
	proc_sprintf(page, &off, &len, "  handler_min");
	proc_sprintf(page, &off, &len, "  handler_max");
	proc_sprintf(page, &off, &len, "\n");
	proc_sprintf(page, &off, &len, "\n");

	for (i = 0; i < NR_IRQS; i++) {
	  if ((!irq_handler0[i].handler) && (!IRQ_DESC[i].action))
	    continue;
	  if (irq_handler0[i].handler) {
	    p = irq_handler0[i].name;
	    if (!p) {
	      p = (char*)IRQ_DESC[i].action->name;
	      if (!p) { p = "RT"; }
	    }
	    sprintf(name, "<%s>", p);
	  }  
	  else {
	    p = (char*)IRQ_DESC[i].action->name;
	    if (!p) { p = ""; }
	    sprintf(name, "[%s]", p);
	  }

	  proc_sprintf(page, &off, &len, "%3d: ", i);
	  proc_sprintf(page, &off, &len, "%-12s", name);
	  proc_sprintf(page, &off, &len, "%10u ",
		       irq_handler0[i].interrupt_count);
	  proc_sprintf(page, &off, &len, "%10u ",
		       irq_handler0[i].pending_count);
	  proc_sprintf(page, &off, &len, "%08x%08x ",
	       (unsigned int)(irq_handler0[i].interrupt_time >> 32),
	       (unsigned int)(irq_handler0[i].interrupt_time & 0xFFFFFFFF));
	  if (irq_handler0[i].pending_time[0]) {
	    if (irq_handler0[i].pending_time[0] <
		irq_handler0[i].interrupt_time) {
	      proc_sprintf(page, &off, &len, "(%10s)", "");
	    }
	    else {
	      t = rt_count2nano(irq_handler0[i].pending_time[0] -
				irq_handler0[i].interrupt_time);
	      proc_sprintf(page, &off, &len, "(%10u)", (unsigned int)t);
	    }
	  }
	  else {
	    t = rt_count2nano(irq_handler0[i].handler_start[0] -
			      irq_handler0[i].interrupt_time);
	    proc_sprintf(page, &off, &len, "(%10u)", (unsigned int)t);
	  }
	  proc_sprintf(page, &off, &len, "\n");

	  for (j = 0; j < IRQ_TIME_LOG_SIZE; j++) {
	    proc_sprintf(page, &off, &len, " [%d] ", j);
	    proc_sprintf(page, &off, &len, "%08x%08x ",
	    (unsigned int)(irq_handler0[i].pending_time[j] >> 32),
	    (unsigned int)(irq_handler0[i].pending_time[j] & 0xFFFFFFFF));
	    proc_sprintf(page, &off, &len, "%08x%08x ",
	    (unsigned int)(irq_handler0[i].handler_start[j] >> 32),
	    (unsigned int)(irq_handler0[i].handler_start[j] & 0xFFFFFFFF));
	    proc_sprintf(page, &off, &len, "%08x%08x ",
	    (unsigned int)(irq_handler0[i].handler_end[j] >> 32),
	    (unsigned int)(irq_handler0[i].handler_end[j] & 0xFFFFFFFF));
	    t = rt_count2nano(irq_handler0[i].handler_end[j] -
			      irq_handler0[i].handler_start[j]);
	    proc_sprintf(page, &off, &len, "(%10u)", (unsigned int)t);
	    proc_sprintf(page, &off, &len, "\n");
	  }

	  proc_sprintf(page, &off, &len, "%43s", "");
	  t = rt_count2nano(irq_handler0[i].handler_time_min);
	  proc_sprintf(page, &off, &len, "(%10u) ", (unsigned int)t);
	  t = rt_count2nano(irq_handler0[i].handler_time_max);
	  proc_sprintf(page, &off, &len, "(%10u)", (unsigned int)t);
	  proc_sprintf(page, &off, &len, "\n");

	  proc_sprintf(page, &off, &len, "\n");
	}

	if (rth_current->name) {
	  proc_sprintf(page, &off, &len,
		       "THREAD:   rth_current = %s\n", rth_current->name);
	}
	else {
	  proc_sprintf(page, &off, &len,
		       "THREAD:   rth_current = %08x\n", rth_current);
	}
	proc_sprintf(page, &off, &len,
		     "            rth_btime = %08x%08x\n",
		     (unsigned int)(rth_btime >> 32),
		     (unsigned int)(rth_btime & 0xFFFFFFFF));
	t = rt_get_time();
	proc_sprintf(page, &off, &len,
		     "        rt_get_time() = %08x%08x\n",
		     (unsigned int)(t >> 32),
		     (unsigned int)(t & 0xFFFFFFFF));
	proc_sprintf(page, &off, &len, "\n");

	proc_sprintf(page, &off, &len, "ready queue:\n");
	proc_sprintf(page, &off, &len, "%10s st prio p ", "name");
	proc_sprintf(page, &off, &len, "%16s ", "cpu_time");
	proc_sprintf(page, &off, &len, "%16s\n", "cpu_count");
	proc_sprintf(page, &off, &len, "%20s ", "");
	proc_sprintf(page, &off, &len, "%16s ", "cpu_start");
	proc_sprintf(page, &off, &len, "%16s ", "cpu_end");
	proc_sprintf(page, &off, &len, "%16s\n", "end-start");
	proc_sprintf(page, &off, &len, "%20s ", "");
	proc_sprintf(page, &off, &len, "%16s ", "period_time");
	proc_sprintf(page, &off, &len, "%16s ", "resume_time");
	proc_sprintf(page, &off, &len, "%16s\n", "yield_time");
	proc_sprintf(page, &off, &len, "\n");

	th = &rth_base;
	do {
	  p = (char*)th->name;
	  if (!p) {
	    p = name;
	    sprintf(name, "%08x", (unsigned int)th);
	  }
	  proc_sprintf(page, &off, &len, "%10s ", p);
	  proc_sprintf(page, &off, &len, "%02x ", th->state);
	  if (th == &rth_base) {
	    proc_sprintf(page, &off, &len, "---- ");
	  }
	  else {
	    proc_sprintf(page, &off, &len, "%4d ", th->priority);
	  }
	  proc_sprintf(page, &off, &len, "%d ", th->policy);
	  if (th == rth_current) {
	    t = rt_get_time();
	    t2 = th->cpu_time + (t - th->cpu_start[0]);
	    proc_sprintf(page, &off, &len, "%08x%08x ",
			 (unsigned int)(t2 >> 32),
			 (unsigned int)(t2 & 0xFFFFFFFF));
	  }
	  else {
	    proc_sprintf(page, &off, &len, "%08x%08x ",
			 (unsigned int)(th->cpu_time >> 32),
			 (unsigned int)(th->cpu_time & 0xFFFFFFFF));
	  }
	  proc_sprintf(page, &off, &len, "%16u", th->cpu_count);
	  proc_sprintf(page, &off, &len, "\n");
	  for (i = 0; i < CPU_TIME_LOG_SIZE; i++) {
	    if ((th == rth_current) && (i == 0)) {
	      t = rt_get_time();
	      proc_sprintf(page, &off, &len, "%17s[%d] ", "", i);
	      proc_sprintf(page, &off, &len, "%08x%08x ",
			   (unsigned int)(th->cpu_start[i] >> 32),
			   (unsigned int)(th->cpu_start[i] & 0xFFFFFFFF));
	      proc_sprintf(page, &off, &len, "%08x%08x ",
			   (unsigned int)(t >> 32),
			   (unsigned int)(t & 0xFFFFFFFF));
	      t = rt_count2nano(t - th->cpu_start[i]);
	      proc_sprintf(page, &off, &len, "%08x%08x ",
			   (unsigned int)(t >> 32),
			   (unsigned int)(t & 0xFFFFFFFF));
	    }
	    else {
	      proc_sprintf(page, &off, &len, "%17s[%d] ", "", i);
	      proc_sprintf(page, &off, &len, "%08x%08x ",
			   (unsigned int)(th->cpu_start[i] >> 32),
			   (unsigned int)(th->cpu_start[i] & 0xFFFFFFFF));
	      proc_sprintf(page, &off, &len, "%08x%08x ",
			   (unsigned int)(th->cpu_end[i] >> 32),
			   (unsigned int)(th->cpu_end[i] & 0xFFFFFFFF));
	      t = rt_count2nano(th->cpu_end[i] - th->cpu_start[i]);
	      proc_sprintf(page, &off, &len, "%08x%08x ",
			   (unsigned int)(t >> 32),
			   (unsigned int)(t & 0xFFFFFFFF));
	    }
	    proc_sprintf(page, &off, &len, "\n");
	  }
	  proc_sprintf(page, &off, &len, "%20s ", "");
	  proc_sprintf(page, &off, &len, "%08x%08x ",
		       (unsigned int)(th->period_time >> 32),
		       (unsigned int)(th->period_time & 0xFFFFFFFF));
	  proc_sprintf(page, &off, &len, "%08x%08x ",
		       (unsigned int)(th->resume_time >> 32),
		       (unsigned int)(th->resume_time & 0xFFFFFFFF));
	  proc_sprintf(page, &off, &len, "%08x%08x ",
		       (unsigned int)(th->yield_time >> 32),
		       (unsigned int)(th->yield_time & 0xFFFFFFFF));
	  proc_sprintf(page, &off, &len, "\n");

	  proc_sprintf(page, &off, &len, "\n");
	} while (th->rnext != &rth_base);

	proc_sprintf(page, &off, &len, "\n");

	proc_sprintf(page, &off, &len, "timed queue:\n");
	proc_sprintf(page, &off, &len, "%10s st prio p ", "name");
	proc_sprintf(page, &off, &len, "%16s ", "cpu_time");
	proc_sprintf(page, &off, &len, "%16s\n", "cpu_count");
	proc_sprintf(page, &off, &len, "%20s ", "");
	proc_sprintf(page, &off, &len, "%16s ", "cpu_start");
	proc_sprintf(page, &off, &len, "%16s ", "cpu_end");
	proc_sprintf(page, &off, &len, "end-start\n");
	proc_sprintf(page, &off, &len, "%20s ", "");
	proc_sprintf(page, &off, &len, "%16s ", "period_time");
	proc_sprintf(page, &off, &len, "%16s ", "resume_time");
	proc_sprintf(page, &off, &len, "%16s\n", "yield_time");
	proc_sprintf(page, &off, &len, "\n");

	th = rth_base.tnext;
	while (th != &rth_base) {
	  p = (char*)th->name;
	  if (!p) {
	    p = name;
	    sprintf(name, "%08x", (unsigned int)th);
	  }
	  proc_sprintf(page, &off, &len, "%10s ", p);
	  proc_sprintf(page, &off, &len, "%02x ", th->state);
	  if (th == &rth_base) {
	    proc_sprintf(page, &off, &len, "---- ");
	  }
	  else {
	    proc_sprintf(page, &off, &len, "%4d ", th->priority);
	  }
	  proc_sprintf(page, &off, &len, "%d ", th->policy);
	  proc_sprintf(page, &off, &len, "%08x%08x ",
		       (unsigned int)(th->cpu_time >> 32),
		       (unsigned int)(th->cpu_time & 0xFFFFFFFF));
	  proc_sprintf(page, &off, &len, "%16u", th->cpu_count);
	  proc_sprintf(page, &off, &len, "\n");
	  for (i = 0; i < CPU_TIME_LOG_SIZE; i++) {
	    proc_sprintf(page, &off, &len, "%17s[%d] ", "", i);
	    proc_sprintf(page, &off, &len, "%08x%08x ",
			 (unsigned int)(th->cpu_start[i] >> 32),
			 (unsigned int)(th->cpu_start[i] & 0xFFFFFFFF));
	    proc_sprintf(page, &off, &len, "%08x%08x ",
			 (unsigned int)(th->cpu_end[i] >> 32),
			 (unsigned int)(th->cpu_end[i] & 0xFFFFFFFF));
	    t = rt_count2nano(th->cpu_end[i] - th->cpu_start[i]);
	    proc_sprintf(page, &off, &len, "%08x%08x ",
			   (unsigned int)(t >> 32),
			   (unsigned int)(t & 0xFFFFFFFF));
	    proc_sprintf(page, &off, &len, "\n");
	  }
	  proc_sprintf(page, &off, &len, "%20s ", "");
	  proc_sprintf(page, &off, &len, "%08x%08x ",
		       (unsigned int)(th->period_time >> 32),
		       (unsigned int)(th->period_time & 0xFFFFFFFF));
	  proc_sprintf(page, &off, &len, "%08x%08x ",
		       (unsigned int)(th->resume_time >> 32),
		       (unsigned int)(th->resume_time & 0xFFFFFFFF));
	  proc_sprintf(page, &off, &len, "%08x%08x ",
		       (unsigned int)(th->yield_time >> 32),
		       (unsigned int)(th->yield_time & 0xFFFFFFFF));
	  proc_sprintf(page, &off, &len, "\n");

	  proc_sprintf(page, &off, &len, "\n");
	}

	return proc_calc_metrics(page, start, off, count, eof, len);
}

#endif

void __init rthal_init(void)
{
	unsigned long flags;
	unsigned int i, j;

	rt_tsc.hltsc[0] = 0;
	rt_tsc.hltsc[1] = 0;

	memset(&rt_calibration, 0, sizeof(rt_calibration));

	rt_calibration.cpu_freq = current_cpu_data.module_clock / 4;

	memset(&rt_times, 0, sizeof(rt_times));

	rthal0 = rthal;

	rthal_init_pending_irq();

	ctrl0.sti  = 0;
	ctrl0.used = 1;

	spin_lock_init(&ctrl0.data_lock);
	spin_lock_init(&ctrl0.ctrl_lock);

	cpu0.intr_flag = IBIT;
	cpu0.base_intr_flag = IBIT;

	for (i = 0; i < NR_IRQS; i++) {

		irq_handler0[i].handler = 0;
#if defined(CONFIG_RT_DEBUG)
		for (j = 0; j < IRQ_TIME_LOG_SIZE; j++) {
			irq_handler0[i].handler_start[j] = 0;
			irq_handler0[i].handler_end[j] = 0;
		}
		irq_handler0[i].handler_time_min = 0;
		irq_handler0[i].handler_time_max = 0;
#endif

		irq_action0[i] = 0;
		irq_chain0[i] = 0;

		irq_ack0[i] = irq_unmask0[i] = irq_end0[i] = rthal_null_irq;

		irq_desc0[i] = IRQ_DESC[i].handler;
		if (irq_desc0[i]) {
			irq_ack0[i] = (IRQ_DESC[i].handler)->ack;
			irq_unmask0[i] = irq_end0[i] =
			  (IRQ_DESC[i].handler)->enable;
		}
		
	}
	{
	       ctrl_outw(TMU2_TCR_INIT, TMU2_TCR);
	       ctrl_outl(TSC_MAX_COUNT, TMU2_TCOR);
	       ctrl_outl(TSC_MAX_COUNT, TMU2_TCNT);
	       ctrl_outb(ctrl_inb(TMU_TSTR) | TMU2_START, TMU_TSTR);
	}

	hard_save_flags_and_cli(flags);

	_rthal_sti_flag = (void *)rthal_sti;

	rthal.do_IRQ           	   = rthal_do_irq;
	rthal.__cli          	   = rthal_cli;
	rthal.__sti                = (void*)rthal_sti;
	rthal.__save_flags         = rthal_save_flags;
	rthal.__restore_flags      = rthal_restore_flags;
	rthal.__save_flags_and_cli = rthal_save_flags_and_cli;
	rthal.__save_and_cli       = rthal_save_and_cli;

	for (i = 0; i < NR_IRQS; i++) {
		if (IRQ_DESC[i].handler) {
			IRQ_DESC[i].handler = &rthal_irq_type;
		}
	}

	hard_restore_flags(flags);
#if defined(CONFIG_RTSCHED)
#if defined(CONFIG_RTHAL_CALIBRATION)
	calibrate_latency();
#endif
	printk("Real-Time Hardware Abstraction Layer version %s\n" 
	       "Support kernel version 2.4.20 - 2.4.22\n", RTHAL_VERSION);
	printk("TIMER INTERRUPT LATENCY = %5d nsec\n", (int)LATENCY_MATCH_REG);

	timer_minimum_value = (unsigned long)rt_nano2count((RTIME)LATENCY_MATCH_REG);
#endif

#if defined(CONFIG_PROC_FS) && defined(CONFIG_RT_DEBUG)
	proc_rtinfo = create_proc_entry("rtinfo", 0, 0);
	if (proc_rtinfo) {
		proc_rtinfo->read_proc = proc_rtinfo_read;
	}
	else {
		printk(KERN_ERR "%s: unable to register /proc/rtinfo\n",
		       __func__);
	}
#endif
}

#if defined(CONFIG_RTHAL_CALIBRATION)
static void calibrate_latency(void)
{
	unsigned long flags;
		
	/* calibrate TIMER INTRRUPT LATENCY */
	/* stop TMU1 */
	ctrl_outb(ctrl_inb(TMU_TSTR) & ~TMU1_START, TMU_TSTR);

	rt_free_irq(RT_TIMER_IRQ);
	rt_request_irq(RT_TIMER_IRQ, trap);

    	ctrl_outw(ctrl_inw(TMU1_TCR) & (~0x0100), TMU1_TCR);
    	ctrl_outw(0x0020, TMU1_TCR);
    
	latency_match_reg = 0;
	test_period = rt_nano2count(PERIOD);
    	next = rt_get_time() + test_period;

	hard_save_flags_and_cli(flags);
    	/* stop TMU1 */
	ctrl_outb(ctrl_inb(TMU_TSTR) & ~TMU1_START, TMU_TSTR);

	ctrl_outw(0x0020, TMU1_TCR);
	ctrl_outl(test_period,  TMU1_TCOR);
	ctrl_outl(test_period,  TMU1_TCNT);
	ctrl_outb(ctrl_inb(TMU_TSTR) | TMU1_START, TMU_TSTR);

	hard_restore_flags(flags);

	while (!cal_end) {
		rt_get_time();
	}

	latency_match_reg = (unsigned long)rt_count2nano(latency_match_reg);
}

static void trap(void)
{
	static int count = 0;
	unsigned long flags;

	trap_total += rt_get_time() - next;	
    	next = rt_get_time() + test_period;

	if (++count >= 4) {
		latency_match_reg = (unsigned long)(trap_total / 4);
		cal_end = 1;
	    	/* stop TMU1 */
    		ctrl_outb(ctrl_inb(TMU_TSTR) & ~TMU1_START, TMU_TSTR);
	    	ctrl_outw(0x0020, TMU1_TCR);
    		ctrl_outl(TSC_MAX_COUNT,  TMU1_TCOR);
	    	ctrl_outl(TSC_MAX_COUNT,  TMU1_TCNT);
	}
	else {
		hard_save_flags_and_cli(flags);
    		/* stop TMU1 */
		ctrl_outb(ctrl_inb(TMU_TSTR) & ~TMU1_START, TMU_TSTR);

		ctrl_outw(0x0020, TMU1_TCR);
		ctrl_outl(test_period,  TMU1_TCOR);
		ctrl_outl(test_period,  TMU1_TCNT);

		ctrl_outb(ctrl_inb(TMU_TSTR) | TMU1_START, TMU_TSTR);
		hard_restore_flags(flags);
	}
}
#endif

#endif /* CONFIG_RTSCHED */

EXPORT_SYMBOL(irq_handler0);

