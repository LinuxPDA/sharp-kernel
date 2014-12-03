/*
 * linux/include/asm-sh/rthal.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on linux/include/asm-sh/system.h
 * Copyright (C) 1999, 2000  Niibe Yutaka  &  Kaz Kojima
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

#ifndef __ASM_SH_RTHAL_H
#define __ASM_SH_RTHAL_H 1

#include <asm/ptrace.h>
#include <asm/irq.h>

#define RTHAL_VERSION	"1.1.1"

#ifdef CONFIG_SMP
#error not support SMP
#endif

#define CPU_FREQ             (rt_calibration.cpu_freq)

#if !defined(CONFIG_RTHAL_CALIBRATION)
#define LATENCY_MATCH_REG    CONFIG_LATENCY_MATCH_REG
#define SETUP_TIME_MATCH_REG CONFIG_SETUP_TIME_MATCH_REG

#else
extern unsigned long latency_match_reg, setup_time_match_reg;

#define LATENCY_MATCH_REG    latency_match_reg
#define SETUP_TIME_MATCH_REG setup_time_match_reg
#endif

#define RT_TIMER_IRQ         TMU1_IRQ
#define RT_TIMER_FREQ        CPU_FREQ
#define RT_TIMER_LATENCY     LATENCY_MATCH_REG
#define RT_TIMER_SETUP       SETUP_TIME_MATCH_REG

#define IBIT 0x00f0

#define IRQ_DESC irq_desc

struct rt_hal {
	unsigned int  (*do_IRQ)(int, struct pt_regs*);
	void          (*__cli)(void);
	void          (*__sti)(void);
	void          (*__save_flags)(unsigned long*);
	void          (*__restore_flags)(unsigned long);
	void          (*__save_flags_and_cli)(unsigned long*);
	unsigned long (*__save_and_cli)(void);
} __attribute__ ((__aligned__ (32)));

extern struct rt_hal         rthal;

extern struct rt_calibration rt_calibration;
extern struct rt_times       rt_times;

#define __cli()                  rthal.__cli();
#define __sti()                  rthal.__sti();
#define __save_flags(x)          rthal.__save_flags(&x)
#define __restore_flags(x)       rthal.__restore_flags(x)
#define __save_flags_and_cli(x)  rthal.__save_flags_and_cli(&x)
#define __save_and_cli()         rthal.__save_and_cli()

static __inline__ void hard_sti(void)
{
	unsigned long __dummy0, __dummy1;

	__asm__ __volatile__("stc	sr, %0\n\t"
			     "and	%1, %0\n\t"
			     "stc	r6_bank, %1\n\t"
			     "or	%1, %0\n\t"
			     "ldc	%0, sr"
			     : "=&r" (__dummy0), "=r" (__dummy1)
			     : "1" (~0x000000f0)
			     : "memory");
}

static __inline__ void hard_cli(void)
{
	unsigned long __dummy;

	__asm__ __volatile__("stc	sr, %0\n\t"
			     "or	#0xf0, %0\n\t"
			     "ldc	%0, sr"
			     : "=&z" (__dummy)
			     : /* no inputs */
			     : "memory");
}

#define hard_save_flags(x) \
	__asm__("stc sr, %0; and #0xf0, %0" : "=&z" (x) :/**/: "memory" )

static __inline__ unsigned long hard_save_and_cli(void)
{
	unsigned long flags, __dummy;

	__asm__ __volatile__("stc	sr, %1\n\t"
			     "mov	%1, %0\n\t"
			     "or	#0xf0, %0\n\t"
			     "ldc	%0, sr\n\t"
			     "mov	%1, %0\n\t"
			     "and	#0xf0, %0"
			     : "=&z" (flags), "=&r" (__dummy)
			     :/**/
			     : "memory" );
	return flags;
}

#define hard_save_flags_and_cli(x) \
	x = (__extension__ ({ \
	unsigned long __dummy, __flags;	\
	__asm__ __volatile__( \
		"stc	sr, %1\n\t" \
		"mov	%1, %0\n\t" \
		"or	#0xf0, %0\n\t" \
		"ldc	%0, sr" \
		: "=&z" (__dummy), "=&r" (__flags) \
		: /**/ \
		: "memory"); (__flags & 0x000000f0); }))


#define hard_restore_flags(x) do { 			\
	if ((x & 0x000000f0) != 0x000000f0)		\
		hard_sti();				\
} while (0)

static inline unsigned long long ullmul(unsigned long m0, unsigned long m1)
{
	return (unsigned long long)m0 * (unsigned long long)m1;
}

static inline unsigned long long ulldiv(unsigned long long ull,
					unsigned long uld, unsigned long *r)
{
	unsigned long long q;

	q = ull/(unsigned long long)uld;
	*r = (unsigned long)(ull - q * (unsigned long long)uld);

	return q;	
}

static inline int imuldiv(unsigned long i, unsigned long mult,
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

	return ((r + r) < div) ? q : q + 1;
}

static inline unsigned long long llimd(unsigned long long ull,
				       unsigned long mult, unsigned long div)
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

extern void rthal_sw_thread(void*, void*);
#define rthal_switch_thread(new) rthal_sw_thread(&rth_current, (new))

#if defined(__SH3__)

#define	rthal_init_thread_stack()		\
do {						\
	th->stack -= 21;			\
						\
	*(th->stack +  0) = 0;			\
	*(th->stack +  1) = 0;			\
	*(th->stack +  2) = 0;			\
	*(th->stack +  3) = 0;			\
	*(th->stack +  4) = 0;			\
	*(th->stack +  5) = 0;			\
	*(th->stack +  6) = 0;			\
	*(th->stack +  7) = 0;			\
	*(th->stack +  8) = 0;			\
	*(th->stack +  9) = (int) data;		\
	*(th->stack + 10) = (int) rt_thread;	\
	*(th->stack + 11) = 0;			\
	*(th->stack + 12) = 0;			\
	*(th->stack + 13) = 0;			\
	*(th->stack + 14) = 0;			\
	*(th->stack + 15) = 0;			\
	*(th->stack + 16) = (int) rth_startup;	\
	*(th->stack + 17) = 0x40000000;		\
	*(th->stack + 18) = 0;			\
	*(th->stack + 19) = 0;			\
	*(th->stack + 20) = 0;			\
} while (0)

#else

#define	rthal_init_thread_stack()		\
do {						\
	th->stack -= 39;			\
						\
	*(th->stack + 0) = 0;			\
	*(th->stack + 1) = 0;			\
	*(th->stack + 2) = 0;			\
	*(th->stack + 3) = 0;			\
	*(th->stack + 4) = 0;			\
	*(th->stack + 5) = 0;			\
	*(th->stack + 6) = 0;			\
	*(th->stack + 7) = 0;			\
	*(th->stack + 8) = 0;			\
	*(th->stack + 9) = 0;			\
	*(th->stack + 10) = 0;			\
	*(th->stack + 11) = 0;			\
	*(th->stack + 12) = 0;			\
	*(th->stack + 13) = 0;			\
	*(th->stack + 14) = 0;			\
	*(th->stack + 15) = 0;			\
	*(th->stack + 16) = 0;			\
	*(th->stack + 17) = 0;			\
	*(th->stack + 18) = 0;			\
	*(th->stack + 19) = 0;			\
	*(th->stack + 20) = 0;			\
	*(th->stack + 21) = 0;			\
	*(th->stack + 22) = 0;			\
	*(th->stack + 23) = 0;			\
	*(th->stack + 24) = 0;			\
	*(th->stack + 25) = 0;			\
	*(th->stack + 26) = 0;			\
	*(th->stack + 27) = (int) data;		\
	*(th->stack + 28) = (int) rt_thread;	\
	*(th->stack + 29) = 0;			\
	*(th->stack + 30) = 0;			\
	*(th->stack + 31) = 0;			\
	*(th->stack + 32) = 0;			\
	*(th->stack + 33) = 0;			\
	*(th->stack + 34) = (int) rth_startup;	\
	*(th->stack + 35) = 0x40000000;		\
	*(th->stack + 36) = 0;			\
	*(th->stack + 37) = 0;			\
	*(th->stack + 38) = 0;			\
} while (0)

#if 0
#define	rthal_init_thread_stack()		\
do {						\
	th->stack -= 55;			\
						\
	*(th->stack + 0) = 0;			\
	*(th->stack + 1) = 0;			\
	*(th->stack + 2) = 0;			\
	*(th->stack + 3) = 0;			\
	*(th->stack + 4) = 0;			\
	*(th->stack + 5) = 0;			\
	*(th->stack + 6) = 0;			\
	*(th->stack + 7) = 0;			\
	*(th->stack + 8) = 0;			\
	*(th->stack + 9) = 0;			\
	*(th->stack + 10) = 0;			\
	*(th->stack + 11) = 0;			\
	*(th->stack + 12) = 0;			\
	*(th->stack + 13) = 0;			\
	*(th->stack + 14) = 0;			\
	*(th->stack + 15) = 0;			\
	*(th->stack + 16) = 0;			\
	*(th->stack + 17) = 0;			\
	*(th->stack + 18) = 0;			\
	*(th->stack + 19) = 0;			\
	*(th->stack + 20) = 0;			\
	*(th->stack + 21) = 0;			\
	*(th->stack + 22) = 0;			\
	*(th->stack + 23) = 0;			\
	*(th->stack + 24) = 0;			\
	*(th->stack + 25) = 0;			\
	*(th->stack + 26) = 0;			\
	*(th->stack + 27) = 0;			\
	*(th->stack + 28) = 0;			\
	*(th->stack + 29) = 0;			\
	*(th->stack + 30) = 0;			\
	*(th->stack + 31) = 0;			\
	*(th->stack + 32) = 0;			\
	*(th->stack + 33) = 0;			\
	*(th->stack + 34) = 0;			\
	*(th->stack + 35) = 0;			\
	*(th->stack + 36) = 0;			\
	*(th->stack + 37) = 0;			\
	*(th->stack + 38) = 0;			\
	*(th->stack + 39) = 0;			\
	*(th->stack + 40) = 0;			\
	*(th->stack + 41) = 0;			\
	*(th->stack + 42) = 0;			\
	*(th->stack + 43) = (int) data;		\
	*(th->stack + 44) = (int) rt_thread;	\
	*(th->stack + 45) = 0;			\
	*(th->stack + 46) = 0;			\
	*(th->stack + 47) = 0;			\
	*(th->stack + 48) = 0;			\
	*(th->stack + 49) = 0;			\
	*(th->stack + 50) = (int) rth_startup;	\
	*(th->stack + 51) = 0x40000000;		\
	*(th->stack + 52) = 0;			\
	*(th->stack + 53) = 0;			\
	*(th->stack + 54) = 0;			\
} while (0)
#endif

#endif

#endif
