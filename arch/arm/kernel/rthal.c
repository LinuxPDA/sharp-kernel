/*
 * linux/arch/arm/kernel/rthal.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * I/O routine and setup routines for arm architecture.
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
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/rt_sched.h>

#include <asm/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/proc/hard_system.h>

#define	RTHAL_VERSION	"1.1.1"

static void linux_cli(void)
{
	hard_cli();
}

static void linux_sti(void)
{
	hard_sti();
}

static unsigned int linux_save_flags(void)
{
	int flags;
	hard_save_flags(flags);
	return flags;
}

static unsigned int linux_save_flags_and_cli(void)
{
	int flags;
	hard_save_flags_cli(flags);
	return flags;
}

static void linux_restore_flags(unsigned int flags)
{
	hard_restore_flags(flags);
}

static void linux_fcli(void)
{
	hard_clf();
}

static void linux_fsti(void)
{
	hard_stf();
}

extern void asm_do_IRQ(int irq, struct pt_regs *regs);

struct rt_hal rthal = {
	asm_do_IRQ,
	NULL,
	NULL,
	linux_cli,
	linux_sti,
	linux_save_flags,
	linux_restore_flags,
	linux_save_flags_and_cli,
	linux_fcli,
	linux_fsti,
};

#define MAX_PENDING_IRQS 256
#define INC_WRAP(x,y) do { x = ++(x) & ((y)-1); } while (0)

static struct rt_hal rthal0;

struct rt_ctrl {
	volatile int in, out;
	volatile int losts;
	volatile int pending_irq[MAX_PENDING_IRQS];
	volatile struct pt_regs pending_regs[MAX_PENDING_IRQS];
	volatile unsigned int used;
	volatile unsigned int sti;
	spinlock_t hard_lock;
	spinlock_t data_lock;
	spinlock_t ctrl_lock;
} ctrl0;

struct rt_cpu {
	volatile unsigned long intr_flag;
	volatile unsigned long base_intr_flag;
} cpu0;

static struct rt_handler {
	volatile unsigned long status;
	void                   (*handler)(void);
} irq_handler0[NR_IRQS];

static unsigned long irq_action0[NR_IRQS];
static int irq_chain0[NR_IRQS];

static void (*irq_mask_and_ack0[NR_IRQS])(rt_irq_t);
static void (*irq_mask0[NR_IRQS])(rt_irq_t);
static void (*irq_unmask0[NR_IRQS])(rt_irq_t);
static void (*irq_end0[NR_IRQS])(rt_irq_t);

static unsigned int rthal_sti(void);

static void rthal_null_irq(rt_irq_t irq)
{
}

static inline int rthal_have_pending_irq( void )
{
	return ( ctrl0.pending_irq[ctrl0.out] != -1 );
}

static inline rt_irq_t rthal_pending_irq( void )
{
	return ctrl0.pending_irq[ctrl0.out];
}

static inline void rthal_clear_pending_irq( rt_irq_t irq )
{
	ctrl0.pending_irq[ctrl0.out] = -1;
	INC_WRAP( ctrl0.out, MAX_PENDING_IRQS );
}

static inline void rthal_init_pending_irq( void )
{
	int i;

	ctrl0.in = ctrl0.out = ctrl0.losts = 0;
	for ( i = 0; i < MAX_PENDING_IRQS; i++ )
		ctrl0.pending_irq[i] = -1;
}

static inline int rthal_test_and_set_sti(volatile int *addr)
{
	unsigned int oldval;
	
	oldval = *addr;
	*addr = 1;
	return oldval;
}

static inline void rthal_clear_sti(volatile int *addr)
{
	*addr = 0;
}

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

void rt_save_flags(unsigned long *flags)
{
	hard_save_flags(*flags);
}

void rt_restore_flags(unsigned long flags)
{
	if (flags & IBIT) {
		hard_cli();
	} else {
		hard_sti();
	}
}

static inline struct pt_regs* rthal_pending_regs(void)
{
	return (struct pt_regs*)(&ctrl0.pending_regs[ctrl0.out]);
}

void rt_pending_irq(rt_irq_t irq, struct pt_regs *regs)
{
	int i;
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&ctrl0.data_lock);
#if 0
	for ( i = 0; i < MAX_PENDING_IRQS; i++ ) { 
		if (ctrl0.pending_irq[i] == irq)
			return;
	}
#endif
	if (ctrl0.pending_irq[ctrl0.in] != -1) {
		ctrl0.losts++;
	}
	ctrl0.pending_irq[ctrl0.in] = irq;
	ctrl0.pending_regs[ctrl0.in] = *regs;
	INC_WRAP( ctrl0.in, MAX_PENDING_IRQS );
	rt_spin_unlock_irqrestore(flags, &ctrl0.ctrl_lock);
}

void rt_mask_ack_irq(rt_irq_t irq)
{
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&ctrl0.ctrl_lock);

	if (irq_mask_and_ack0[irq]) {
		irq_mask_and_ack0[irq](irq);
	}

	rt_spin_unlock_irqrestore(flags, &ctrl0.ctrl_lock);
}

void rt_mask_irq(rt_irq_t irq)
{
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&ctrl0.ctrl_lock);

	if (irq_mask0[irq]) {
		irq_mask0[irq](irq);
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

unsigned int rt_startup_irq(rt_irq_t irq)
{
	rt_unmask_irq(irq);
}

void rt_shutdown_irq(rt_irq_t irq)
{
	rt_mask_ack_irq(irq);
}

void rt_enable_irq(rt_irq_t irq)
{
	rt_unmask_irq(irq);
}

void rt_disable_irq(rt_irq_t irq)
{
	rt_mask_irq(irq);
}

static asmlinkage unsigned int rthal_do_irq(int irq, struct pt_regs* regs)
{
	unsigned int ret = 0;

#if 1
	if (irq == RT_TIMER_IRQ) {
		if (irq_handler0[irq].handler) {
			((void (*)(int))irq_handler0[irq].handler)(irq);
		}

		if ((ctrl0.used) && (cpu0.intr_flag)) {
			ret = rthal_sti();
		}
		hard_cli();
		return ret;
	}
#if 0
	else if (irq == TIMER_IRQ) {

		if ((!ctrl0.used || cpu0.intr_flag) && !in_interrupt()) {
			unsigned long save_intr_flag = cpu0.intr_flag;
			if (irq_mask_and_ack0[irq])
				irq_mask_and_ack0[irq](irq);
			cpu0.intr_flag = 0;

			rthal0.do_IRQ(irq, regs);

			cpu0.intr_flag = save_intr_flag;
			if (irq_unmask0[irq])
				irq_unmask0[irq](irq);

			hard_cli();
			return 1;
		}
	}
#else
	else 
#endif

#endif
	if ((irq >= 0) && (irq < NR_IRQS)) {
		rt_spin_lock(&ctrl0.ctrl_lock);

		if (irq_mask_and_ack0[irq]) {
			irq_mask_and_ack0[irq](irq);
		}

		rt_spin_unlock(&ctrl0.ctrl_lock);

		if (irq_handler0[irq].handler) {
			((void (*)(int))irq_handler0[irq].handler)(irq);
			if (irq_unmask0[irq]) {
				irq_unmask0[irq](irq);
			}
		}
#if 1
		else if ((ctrl0.used) && (cpu0.intr_flag) &&
			 !rthal_have_pending_irq()) {
			unsigned long save_intr_flag = cpu0.intr_flag;
			cpu0.intr_flag = 0;
			hard_sti();
			rthal0.do_IRQ(irq, regs);
			cpu0.intr_flag = save_intr_flag;
			hard_cli();
			return 1;
		}
#endif
		else {
			rt_pending_irq(irq, regs);
		}

		if ((ctrl0.used) && (cpu0.intr_flag)) {
			hard_sti();
			ret = rthal_sti();
		}

	}
	hard_cli();
	return ret;
}

static void rthal_cli(void)
{
	cpu0.intr_flag = 0;
}

static unsigned int rthal_sti(void)
{
 	rt_irq_t irq;
	struct pt_regs regs;
      	struct rt_cpu* cpu;
	unsigned int ret = 0;
	 
	cpu = &cpu0;
	cpu->intr_flag = IBIT;

	while (!rthal_test_and_set_sti(&ctrl0.sti)) {
	       	while (rthal_have_pending_irq()) {
			rt_spin_lock_irq(&(ctrl0.data_lock));
			if ( (irq = rthal_pending_irq()) != -1 ) {
				regs = *rthal_pending_regs();
				rthal_clear_pending_irq(irq);
				rt_spin_unlock_irq(&(ctrl0.data_lock));
				cpu->intr_flag = 0;

				rthal0.do_IRQ(irq, &regs);
				hard_sti();

				cpu->intr_flag = IBIT;
				ret = 1;
			}
			else {
				rt_spin_unlock_irq(&(ctrl0.data_lock));
			}
		}
		rthal_clear_sti(&ctrl0.sti);
		if ( !rthal_have_pending_irq() ) break;
	}

	return ret;
}

asmlinkage int rthal_irqs_disabled(void)
{
    return cpu0.intr_flag ? 0 : 1;
}

static unsigned int rthal_save_flags(void)
{
	unsigned long flags;

	hard_save_flags(flags);
	flags = (flags & ~IBIT) | (cpu0.intr_flag  ? 0 : IBIT);
	return flags;
}

static void rthal_restore_flags(unsigned int flags)
{
	if (flags & IBIT) {
		cpu0.intr_flag = 0;
	}
	else {
		rthal_sti();
	}
}

static unsigned int rthal_save_flags_and_cli(void)
{
	unsigned long flags;

	flags = rthal_save_flags();
	cpu0.intr_flag = 0;
	return flags;
}

static void rthal_enable_irq(rt_irq_t irq)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_unmask0[irq]) {
		irq_unmask0[irq](irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}

static void rthal_disable_irq(rt_irq_t irq)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_mask0[irq]) {
		irq_mask0[irq](irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}

static void rthal_ack_irq(unsigned int irq)
{
	rt_spin_lock_irq(&ctrl0.ctrl_lock);
	if (irq_mask_and_ack0[irq]) {
		irq_mask_and_ack0[irq](irq);
	}
	rt_spin_unlock_irq(&ctrl0.ctrl_lock);
}

int rt_request_irq(rt_irq_t irq, void (*handler)(void))
{
	unsigned long flags;

	if ((irq >= NR_IRQS) || !(handler))
		return -EINVAL;

	if (irq_handler0[irq].handler)
		return -EBUSY;

	flags = hard_lock_all();

	IRQ_DESC[irq].mask_ack = rthal_null_irq;
	IRQ_DESC[irq].mask = rthal_null_irq;
	IRQ_DESC[irq].unmask = rthal_null_irq;
	
	irq_handler0[irq].status  = 0;
	irq_handler0[irq].handler = handler;

	irq_end0[irq] = rthal_null_irq;

	hard_unlock_all(flags);

	return 0;
}

int rt_free_irq(rt_irq_t irq)
{
	unsigned long flags;

	if ((irq >= NR_IRQS) || !(irq_handler0[irq].handler))
		return -EINVAL;

	flags = hard_lock_all();

	IRQ_DESC[irq].mask_ack = rthal_ack_irq;
	IRQ_DESC[irq].mask = rthal_disable_irq;
	IRQ_DESC[irq].unmask = rthal_enable_irq;

	irq_handler0[irq].status  = 0;
	irq_handler0[irq].handler = 0;

	irq_end0[irq] = irq_unmask0[irq];

	hard_unlock_all(flags);

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

void __init rthal_init_common(char *mach_version)
{
	int i;
     	unsigned long flags;

	rthal0 = rthal;

	rthal_init_pending_irq();

	ctrl0.sti = 0;
	ctrl0.used = 1;

	spin_lock_init(&ctrl0.hard_lock);
	spin_lock_init(&ctrl0.data_lock);
	spin_lock_init(&ctrl0.ctrl_lock);

	cpu0.intr_flag = cpu0.base_intr_flag =IBIT;

	for (i = 0; i < NR_IRQS; i++) {

		irq_handler0[i].status  = 0;
		irq_handler0[i].handler = 0;

		irq_action0[i] = 0;
		irq_chain0[i]  = 0;

		irq_mask_and_ack0[i] = IRQ_DESC[i].mask_ack;
		irq_mask0[i] = IRQ_DESC[i].mask;
		irq_unmask0[i] = irq_end0[i] = IRQ_DESC[i].unmask;
	}

	flags = hard_lock_all();

	rthal.do_IRQ           	   = rthal_do_irq;
	rthal.disint               = rthal_cli;
	rthal.enint                = (void *)rthal_sti;
	rthal.getflags             = rthal_save_flags;
	rthal.setflags             = rthal_restore_flags;
	rthal.getflags_and_cli     = rthal_save_flags_and_cli;

	for (i = 0; i < NR_IRQS; i++) {
		IRQ_DESC[i].mask_ack = rthal_ack_irq;
		IRQ_DESC[i].mask = rthal_disable_irq;
		IRQ_DESC[i].unmask = rthal_enable_irq;
	}

	printk("Real-Time Hardware Abstraction Layer version %s%s\n"
	       "Support kernel version 2.4.20\n", RTHAL_VERSION, mach_version); 

	hard_unlock_all(flags);
}
