/*
 * linux/include/asm-arm/arch-dbmx2/rthal.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *	rtai/include/asm-arm/rtai.h
 */
/* 020222 asm-arm/rtai.h
COPYRIGHT (C) 2002 Thomas Gleixner, autronix automation (gleixner@autronix.de) 
COPYRIGHT (C) 2002 Guennadi Liakhovetski, DSA GmbH (gl@dsa-ac.de)
COPYRIGHT (C) 2002 Wolfgang Müller (wolfgang.mueller@dsa-ac.de)
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

#ifndef __ASM_ARM_ARCH_DBMX2_RTHAL_H
#define __ASM_ARM_ARCH_DBMX2_RTHAL_H 1

#include <linux/config.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <asm/mach/irq.h>
#include <linux/bitops.h>

#include <asm/ptrace.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/arch/irqs.h>

#ifdef CONFIG_SMP
#error not support SMP
#endif

#define NR_RT_CPUS      1
#define RT_NR_TRAPS     32

#define NR_GLOBAL_IRQS  NR_IRQS

#define hard_cpu_id() (0)
#define save_cr0_and_clts(x)
#define restore_cr0(x)

//#define LATENCY_MATCH_REG	140000
//#define SETUP_TIME_MATCH_REG	70000
//#define LATENCY_MATCH_REG	3000
//#define SETUP_TIME_MATCH_REG	7000

#if !defined(CONFIG_RTHAL_CALIBRATION)
#define LATENCY_MATCH_REG    CONFIG_LATENCY_MATCH_REG
#define SETUP_TIME_MATCH_REG CONFIG_SETUP_TIME_MATCH_REG

#else
extern unsigned long latency_match_reg, setup_time_match_reg;

#define LATENCY_MATCH_REG    latency_match_reg
#define SETUP_TIME_MATCH_REG setup_time_match_reg
#endif

#define LATENCY_TICKS     LATENCY_MATCH_REG/(1000000000/FREQ_SYS_CLK)
#define SETUP_TIME_TICKS  SETUP_TIME_MATCH_REG/(1000000000/FREQ_SYS_CLK)

#define TIMER_IRQ		INT_GPT1
#define RT_TIMER_IRQ		INT_GPT2
#define RT_TIMER_FREQ		(rt_calibration.cpu_freq)
#define RT_TIMER_LATENCY	LATENCY_MATCH_REG
#define RT_TIMER_SETUP		SETUP_TIME_MATCH_REG

#define IBIT 0x0080

extern struct rt_calibration rt_calibration;
extern struct rt_times       rt_times;

typedef struct sh_fpu_env { unsigned long fpu_reg[1]; } FPU_ENV;
#ifndef CONFIG_RT_FPU_SUPPORT
#define enable_fpu()
#endif

#define IRQ_DESC irq_desc

static inline unsigned long long ullmul(unsigned long m0,
					unsigned long m1)
{
	return (unsigned long long)m0 * (unsigned long long)m1;
}

static inline unsigned long long ulldiv(unsigned long long ull,
					unsigned long uld,
					unsigned long* r)
{
	unsigned long long q;

	q = ull/(unsigned long long)uld;
	*r = (unsigned long)(ull - q * (unsigned long long)uld);

	return q;	
}

static inline int imuldiv(unsigned long i,
			  unsigned long mult,
			  unsigned long div)
{
	unsigned long q, r;
	unsigned long long m;

	if (mult == div)
		return i;

	m = ((unsigned long long)i * (unsigned long long)mult);
	q = (unsigned long)(m / (unsigned long long)div);
	r = (unsigned long)(m - (unsigned long long)q *
			    (unsigned long long)div );

	return (r + r) < div ? q : q + 1;
}

static inline unsigned long long llimd(unsigned long long ull,
				       unsigned long mult,
				       unsigned long div)
{
	unsigned long long low, high, q;
	unsigned long r;

	low  = ullmul(((unsigned long*)&ull)[0], mult);
	high = ullmul(((unsigned long*)&ull)[1], mult);
	q = ulldiv(high, div, &r) << 32;
	high = ((unsigned long long)r) << 32;
	q += ulldiv(high + low, div , &r);

	return ((r + r) < div) ? q : q + 1;
}

#define ffnz(ul) (ffs(ul)-1)

static inline unsigned long hard_lock_all(void)
{
	unsigned long flags;
	hard_save_flags_and_cli(flags);
	return flags;
}

static inline void hard_unlock_all(unsigned long flags)
{
	hard_restore_flags(flags);
}

void          rt_request_timer(void (*)(void), unsigned int, int);
void          rt_free_timer(void);

//void          rt_switch_cpu(rt_cpuid_t, int);
void          rt_set_timer_delay(int);

//extern RTIME  rdtsc(void);

#define ffnz(ul) (ffs(ul)-1)

extern void rthal_sw_thread(void*, void*);
#define rthal_switch_thread(new) rthal_sw_thread(&rth_current, (new))

static inline unsigned long current_control(void)
{
	unsigned long control;
	asm volatile ("mrc p15, 0, %0, c3, c0" : "=r" (control));
	return control;
}

#define rthal_init_thread_stack()			\
do {							\
	unsigned long init_flags;			\
	unsigned long init_control;			\
							\
	hard_save_flags(init_flags);			\
	init_control = current_control();		\
							\
	th->stack -=  +15;				\
							\
	*(th->stack   +14) = (int) rth_startup;		\
	*(th->stack   +03) = (int) data;		\
	*(th->stack   +02) = (int) rt_thread;		\
	*(th->stack   +01) = (int) init_flags;		\
	*(th->stack   +00) = (int) init_control;	\
} while (0)

#endif /* __ASM_ARM_ARCH_DBMX2_RTHAL_H */
