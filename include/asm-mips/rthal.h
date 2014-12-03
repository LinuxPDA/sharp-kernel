/*
 * linux/include/asm-mips/rthal.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on linux/include/asm-mips/system.h
 * Copyright (C) 1994 - 1999 by Ralf Baechle
 * Copyright (C) 1996 by Paul M. Antoine
 * Copyright (C) 1994 - 1999 by Ralf Baechle
 * Copyright (C) 2000 MIPS Technologies, Inc.
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

#ifndef __ASM_MIPS_RTHAL_H
#define __ASM_MIPS_RTHAL_H 1

#include <asm/ptrace.h>
#include <asm/irq.h>

#ifdef CONFIG_SMP
#error not support SMP
#endif

#define CPU_FREQ             (rt_calibration.cpu_freq)
#define LATENCY_MATCH_REG    3000
#define SETUP_TIME_MATCH_REG 500

#define RT_TIMER_IRQ         7

#define RT_TIMER_FREQ        CPU_FREQ
#define RT_TIMER_LATENCY     LATENCY_MATCH_REG
#define RT_TIMER_SETUP       SETUP_TIME_MATCH_REG

#define IFLAG 9

#define IRQ_DESC irq_desc

struct rt_hal {
	unsigned int  (*do_IRQ)(int, struct pt_regs*);
	void          (*_cli)(void);
	void          (*_sti)(void);
	void          (*_save_flags)(unsigned long*);
	void          (*_restore_flags)(unsigned long);
	void          (*_save_flags_and_cli)(unsigned long*);
} __attribute__ ((__aligned__ (32)));

extern struct rt_hal         rthal;
extern struct rt_calibration rt_calibration;
extern struct rt_times       rt_times;

#define __cli()                  rthal._cli();
#define __sti()                  rthal._sti();
#define __save_flags(x)          rthal._save_flags(&x)
#define __restore_flags(x)       rthal._restore_flags(x)
#define __save_flags_and_cli(x)  rthal._save_flags_and_cli(&x)
#define __save_and_cli(x)        rthal._save_flags_and_cli(&x)

__asm__ (
	".macro\t__sti\n\t"
	".set\tpush\n\t"
	".set\treorder\n\t"
	".set\tnoat\n\t"
	"mfc0\t$1,$12\n\t"
	"ori\t$1,0x1f\n\t"
	"xori\t$1,0x1e\n\t"
	"mtc0\t$1,$12\n\t"
	".set\tpop\n\t"
	".endm");

extern __inline__ void
hard_sti(void)
{
	__asm__ __volatile__(
		"__sti"
		: /* no outputs */
		: /* no inputs */
		: "memory");
}

__asm__ (
	".macro\t__cli\n\t"
	".set\tpush\n\t"
	".set\tnoat\n\t"
	"mfc0\t$1,$12\n\t"
	"ori\t$1,1\n\t"
	"xori\t$1,1\n\t"
	".set\tnoreorder\n\t"
	"mtc0\t$1,$12\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	".set\tpop\n\t"
	".endm");

extern __inline__ void
hard_cli(void)
{
	__asm__ __volatile__(
		"__cli"
		: /* no outputs */
		: /* no inputs */
		: "memory");
}

__asm__ (
	".macro\t__save_flags flags\n\t"
	".set\tpush\n\t"
	".set\treorder\n\t"
	"mfc0\t\\flags, $12\n\t"
	".set\tpop\n\t"
	".endm");

#define hard_save_flags(x)						\
__asm__ __volatile__(							\
	"__save_flags %0"						\
	: "=r" (x))

__asm__(".macro\t__restore_flags flags\n\t"
	".set\tnoreorder\n\t"
	".set\tnoat\n\t"
	"mfc0\t$1, $12\n\t"
	"andi\t\\flags, 1\n\t"
	"ori\t$1, 1\n\t"
	"xori\t$1, 1\n\t"
	"or\t\\flags, $1\n\t"
	"mtc0\t\\flags, $12\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	".set\tat\n\t"
	".set\treorder\n\t"
	".endm");

#define hard_restore_flags(flags)					\
do {									\
	unsigned long __tmp1;						\
									\
	__asm__ __volatile__(						\
		"__restore_flags\t%0"					\
		: "=r" (__tmp1)						\
		: "0" (flags)						\
		: "memory");						\
} while(0)

__asm__ (
	".macro\t__save_flags_and_cli result\n\t"
	".set\tpush\n\t"
	".set\treorder\n\t"
	".set\tnoat\n\t"
	"mfc0\t\\result, $12\n\t"
	"ori\t$1, \\result, 1\n\t"
	"xori\t$1, 1\n\t"
	".set\tnoreorder\n\t"
	"mtc0\t$1, $12\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	".set\tpop\n\t"
	".endm");

#define hard_save_flags_and_cli(x)					\
__asm__ __volatile__(							\
	"__save_flags_and_cli\t%0"					\
	: "=r" (x)							\
	: /* no inputs */						\
	: "memory")

#define hard_save_and_cli(x)						\
__asm__ __volatile__(							\
	"__save_flags_and_cli\t%0"					\
	: "=r" (x)							\
	: /* no inputs */						\
	: "memory")


static inline unsigned long long ullmul(unsigned long m0, unsigned long m1)
{

	unsigned long long __res;

	__asm__ __volatile__ (
			"multu\t%2,%3\n\t"
			"mflo\t%0\n\t"
			"mfhi\t%1\n\t"
			: "=r" (((unsigned long *)&__res)[0]),
			  "=r" (((unsigned long *)&__res)[1])
			: "r" (m0), "r" (m1));

	return(__res);

} /* End function - ullmul */



// One of the silly thing of 32 bits MIPS, no 64 by 32 bits divide.
static inline unsigned long long ulldiv(unsigned long long ull,
				unsigned long uld, unsigned long *r)
{
	unsigned long long q, rf;
	unsigned long qh, rh, ql, qf;

	q = 0;
	rf = (unsigned long long)(0xFFFFFFFF - (qf = 0xFFFFFFFF / uld) * uld)
									+ 1ULL;

	while (ull >= uld) {
		((unsigned long *)&q)[1] +=
			(qh = ((unsigned long *)&ull)[1] / uld);

		rh = ((unsigned long *)&ull)[1] - qh * uld;
		q += rh * (unsigned long long)qf +
			(ql = ((unsigned long *)&ull)[0] / uld);

		ull = rh * rf + (((unsigned long *)&ull)[0] - ql * uld);
	}

	*r = ull;
	return(q);
}  /* End function - ulldiv */

static inline int imuldiv(int i, int mult, int div)
{
	unsigned long q, r;

	q = ulldiv(ullmul(i, mult), div, &r);

	return (r + r) > div ? q + 1 : q;
}  /* End function - imuldiv */

static inline unsigned long long llimd(unsigned long long ull,
				unsigned long mult, unsigned long div)
{
	unsigned long long low;
	unsigned long q, r;

	low  = ullmul(((unsigned long *)&ull)[0], mult);	
	q = ulldiv( ullmul(((unsigned long *)&ull)[1], mult) +
		((unsigned long *)&low)[1], div, (unsigned long *)&low);
	low = ulldiv(low, div, &r);
	((unsigned long *)&low)[1] += q;

	return (r + r) > div ? low + 1 : low;
}  /* End function - llimd */


/*
 * This has been copied from the Linux kernel source for ffz and modified
 * to return the position of the first non zero bit. - Stevep
 *
 * ffnz = Find First Non Zero in word. Undefined if no one exists,
 * so code should check against ~0xffffffffUL first..
 *
 */
extern __inline__ unsigned long ffnz(unsigned long word)
{
	unsigned int    __res;
	unsigned int    __mask = 1;

	__asm__ (
		".set\tnoreorder\n\t"
		".set\tnoat\n\t"
		"move\t%0,$0\n"
		"1:\tand\t$1,%2,%1\n\t"
		"bnez\t$1,2f\n\t"
		"sll\t%1,1\n\t"
		"bnez\t%1,1b\n\t"
		"addiu\t%0,1\n\t"
		".set\tat\n\t"
		".set\treorder\n"
		"2:\n\t"
		: "=&r" (__res), "=r" (__mask)
		: "r" (word), "1" (__mask)
		: "$1");

	return __res;
} /* End function - ffnz */


extern void rthal_sw_thread(void*, void*);
#define rthal_switch_thread(new) rthal_sw_thread(&rth_current, (new))

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

#endif /* __ASM_MIPS_RTHAL_H */
