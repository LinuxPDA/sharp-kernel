/*
 * linux/arch/mips/vr41xx/common/rthal.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * I/O routine and setup routines for vr41xx architecture.
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

extern unsigned int do_IRQ(int, struct pt_regs*);

struct rt_hal rthal = {
	do_IRQ,
	linux_cli,
	linux_sti,
	linux_save_flags,
	linux_restore_flags,
	linux_save_flags_and_cli,
};

#if defined(CONFIG_RTSCHED)


#define MAX_PENDING_IRQS  256
#define INC_WRAP(x,y) do { x = ++(x) & ((y)-1); } while (0)

static struct rt_hal rthal0;

struct rt_ctrl {
	volatile int          in;
	volatile int          out;
	volatile int          losts;
	volatile int          pending_irq[MAX_PENDING_IRQS];
	volatile int          accept_irq[NR_IRQS];
	volatile int          multi_irq[NR_IRQS];
	volatile unsigned int used;
	volatile unsigned int sti;
	spinlock_t            hard_lock;
	spinlock_t            data_lock;
	spinlock_t            ctrl_lock;
} ctrl0;

struct rt_cpu {
	volatile unsigned long intr_flag;
	volatile unsigned long base_intr_flag;
} cpu0;

static struct rt_handler {
	volatile unsigned long dest_status;
	void                   (*handler)(void);
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

static int last_c2; 

#define C2_LATCH 0xFFFE

static unsigned int rthal_sti(void);

void rt_spin_lock(spinlock_t* lock)
{
}

void rt_spin_unlock(spinlock_t* lock)
{
}

void rt_spin_lock_irq(spinlock_t* lock)
{
	hard_cli();
	rt_spin_lock(lock);
}

void rt_spin_unlock_irq(spinlock_t* lock)
{
	rt_spin_unlock(lock);
	hard_sti();
}

unsigned int rt_spin_lock_irqsave(spinlock_t* lock)
{
	unsigned long flags;
	hard_save_flags_and_cli(flags);
	rt_spin_lock(lock);
	return flags;
}

void rt_spin_unlock_irqrestore(unsigned long flags, spinlock_t* lock)
{
	rt_spin_unlock(lock);
	hard_restore_flags(flags);
}

void rt_cli(void)
{
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
	switch (flags) {
	case (1 << IFLAG) | 1:
		hard_sti();
		break;
	case (1 << IFLAG) | 0:
		hard_sti();
		break;
	case (0 << IFLAG) | 1:
		break;
	case (0 << IFLAG) | 0:
		break;
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

void rt_pending_irq(rt_irq_t irq)
{
	if (ctrl0.pending_irq[ctrl0.in] != -1) {
		ctrl0.losts++;
	}
	ctrl0.pending_irq[ctrl0.in] = irq;
	INC_WRAP(ctrl0.in, MAX_PENDING_IRQS);
}

static inline void rthal_clear_pending_irq(rt_irq_t irq)
{
	ctrl0.pending_irq[ctrl0.out] = -1;
	INC_WRAP(ctrl0.out, MAX_PENDING_IRQS);
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
#if 1
		irq_desc->disable(irq);
#else
		irq_desc->enable(irq);
#endif
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


	if (irq == RT_TIMER_IRQ) {
		if (irq_handler0[irq].handler) {
			((void (*)(int))irq_handler0[irq].handler)(irq);
		}

		if ((ctrl0.used) && (cpu0.intr_flag)) {	
			return rthal_sti();
		}
	}
	else if ((irq >= 0) && (irq < NR_IRQS)) {
		rt_spin_lock(&ctrl0.ctrl_lock);

		if (irq_ack0[irq]) {
			irq_ack0[irq](irq);
		}
		rt_spin_unlock(&ctrl0.ctrl_lock);
	
		if (irq_handler0[irq].handler) {
			((void (*)(int))irq_handler0[irq].handler)(irq);
			if (irq_unmask0[irq]) {
				irq_unmask0[irq](irq);
			}
			rt_spin_lock_irq(&ctrl0.data_lock);
		}
		else {
			rt_spin_lock(&ctrl0.data_lock);
			if (ctrl0.multi_irq[irq] || !ctrl0.accept_irq[irq]) {
				rt_pending_irq(irq);
				ctrl0.accept_irq[irq] = 1;
			}	
		}

		if ((ctrl0.used) && (cpu0.intr_flag)) {
			rt_spin_unlock_irq(&(ctrl0.data_lock));
			return rthal_sti();
		}
		else {
			rt_spin_unlock(&(ctrl0.data_lock));
		}
	}

	return 0;
}

static void rthal_cli(void)
{
	cpu0.intr_flag = 0;
}

static inline int rthal_test_and_set_sti(volatile int* addr)
{
	unsigned int oldval;
	
	oldval = *addr;
	*addr = 1;
	return oldval;
}

static inline void rthal_clear_sti(volatile int* addr)
{
	*addr = 0;
}

static unsigned int rthal_sti(void)
{
      	struct rt_cpu* cpu = &cpu0;
	unsigned int rc = 0;
 	rt_irq_t irq;
	struct pt_regs rt_regs;
	 
	cpu->intr_flag = (1<<IFLAG);

	while (!rthal_test_and_set_sti(&ctrl0.sti)) {
	       	while (rthal_have_pending_irq()) {
			rt_spin_lock_irq(&(ctrl0.data_lock));
			if ((irq = rthal_pending_irq()) != -1) {
				rthal_clear_pending_irq(irq);
				rt_spin_unlock_irq(&(ctrl0.data_lock));
				cpu->intr_flag = 0;

				rthal0.do_IRQ(irq, &rt_regs);

				cpu->intr_flag = (1<<IFLAG);
				rc = 1;
				ctrl0.accept_irq[irq] = 0;
			}
			else {
				rt_spin_unlock_irq(&(ctrl0.data_lock));
			}

		}
		rthal_clear_sti(&ctrl0.sti);
		if (!rthal_have_pending_irq()) break;
	}

	return rc;
}

static void rthal_save_flags(unsigned long* fp)
{
	unsigned long flags;

	hard_save_flags(flags);
	*fp = (flags & ~(1<<IFLAG)) | (cpu0.intr_flag  ? 0 : (1<<IFLAG));
}

static void rthal_restore_flags(unsigned long flags)
{
	if (flags & (1<<IFLAG)) {
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
	rt_disable_irq(irq);
}

static void rthal_ack_irq(unsigned int irq)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_desc0[irq]->ack) {
		irq_desc0[irq]->ack(irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}

static void rthal_end_irq(unsigned int irq)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_desc0[irq]->end) {
		irq_desc0[irq]->end(irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}

static struct hw_interrupt_type rthal_irq_type = { 
	typename:	"RTHAL",
	startup:	rt_startup_irq,
	shutdown:	rt_shutdown_irq,
	enable:		rthal_enable_irq,
	disable:	rthal_disable_irq,
	ack:		rthal_ack_irq,
	end:		rthal_end_irq,
	set_affinity:	NULL,
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
	
	irq_handler0[irq].dest_status = 0;
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

	irq_handler0[irq].dest_status = 0;
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

	save_and_cli(flags);

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
	save_and_cli(flags);

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
	RTIME t = 0;
	/*
	 * ....
	 */
	return t;
}

void rthal_set_timer(unsigned int delay)
{
	/*
	 * ....
	 */
}

static void rthal_timer_handler(void)
{
	rt_get_time();

	timer_handler();
}

void rthal_request_rt_timer(void (*handler)(void))
{
	unsigned long flags;
	int count;

	hard_save_flags_and_cli(flags);

	save_rt_timer_handler =
	  (void*)IRQ_DESC[RT_TIMER_IRQ].action->handler;
	IRQ_DESC[RT_TIMER_IRQ].action->handler = rthal_null_handler;

	timer_handler = handler;
	rt_request_irq(RT_TIMER_IRQ, rthal_timer_handler);

	rt_times.tick_time     = rt_get_time();
	rt_times.base_tick     =
	  imuldiv(LATCH, rt_calibration.cpu_freq, CPU_FREQ);
	rt_times.intr_time     = rt_times.tick_time + rt_times.base_tick;
	rt_times.base_time     = rt_times.tick_time + rt_times.base_tick;
	rt_times.periodic_tick = rt_times.base_tick;
	
	rthal_set_timer(rt_times.periodic_tick);

	hard_restore_flags(flags);
}

void __init rthal_init(void)
{
	unsigned long flags;
	unsigned int i;

	memset(&rt_calibration, 0, sizeof(rt_calibration));

	rt_calibration.cpu_freq = 1193180 /* ??? */;

	memset(&rt_times, 0, sizeof(rt_times));

	rthal0 = rthal;

	rthal_init_pending_irq();

	ctrl0.sti  = 0;
	ctrl0.used = 1;

	spin_lock_init(&ctrl0.data_lock);
	spin_lock_init(&ctrl0.hard_lock);
	spin_lock_init(&ctrl0.ctrl_lock);

	cpu0.intr_flag = (1<<IFLAG);
	cpu0.base_intr_flag = (1<<IFLAG);

	for (i = 0; i < NR_IRQS; i++) {

		irq_handler0[i].dest_status = 0;
		irq_handler0[i].handler = 0;

		irq_action0[i] = 0;
		irq_chain0[i] = 0;

		irq_ack0[i] = irq_unmask0[i] = irq_end0[i] = rthal_null_irq;

		irq_desc0[i] = IRQ_DESC[i].handler;
		if (irq_desc0[i]) {
			irq_ack0[i] = (IRQ_DESC[i].handler)->ack;
			irq_unmask0[i] = irq_end0[i] =
			  (IRQ_DESC[i].handler)->enable;
		}
		
		ctrl0.accept_irq[i] = 0;
		ctrl0.multi_irq[i] = 1;
	}

	hard_save_flags_and_cli(flags);

	rthal.do_IRQ              = rthal_do_irq;
	rthal._cli          	  = rthal_cli;
	rthal._sti                = (void*)rthal_sti;
	rthal._save_flags         = rthal_save_flags;
	rthal._restore_flags      = rthal_restore_flags;
	rthal._save_flags_and_cli = rthal_save_flags_and_cli;

	for (i = 0; i < NR_IRQS; i++) {
		if (IRQ_DESC[i].handler) {
			IRQ_DESC[i].handler = &rthal_irq_type;
		}
	}

	hard_restore_flags(flags);
}

#endif /* CONFIG_RTSCHED */

