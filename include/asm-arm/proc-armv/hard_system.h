/*
 */
#ifndef __ASM_PROC_HARD_SYSTEM_H
#define __ASM_PROC_HARD_SYSTEM_H

#include <linux/config.h>

#define hard_save_flags_and_cli(x) hard_save_flags_cli(x)

/*
 * Save the current interrupt enable state & disable IRQs
 */
#define hard_save_flags_cli(x)					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ save_flags_cli\n"	\
"	orr	%1, %0, #128\n"					\
"	msr	cpsr_c, %1"					\
	: "=r" (x), "=r" (temp)					\
	:							\
	: "memory");						\
	})
	
/*
 * Enable IRQs
 */
#define hard_sti()							\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ sti\n"		\
"	bic	%0, %0, #128\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory");						\
	})

/*
 * Disable IRQs
 */
#define hard_cli()							\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ cli\n"		\
"	orr	%0, %0, #128\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory");						\
	})

/*
 * Enable FIQs
 */
#define hard_stf()							\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ stf\n"		\
"	bic	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory");						\
	})

/*
 * Disable FIQs
 */
#define hard_clf()							\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ clf\n"		\
"	orr	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory");						\
	})

/*
 * save current IRQ & FIQ state
 */
#define hard_save_flags(x)						\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ save_flags\n"		\
	  : "=r" (x)						\
	  :							\
	  : "memory")

/*
 * restore saved IRQ & FIQ state
 */
#define hard_restore_flags(x)					\
	__asm__ __volatile__(					\
	"msr	cpsr_c, %0		@ restore_flags\n"	\
	:							\
	: "r" (x)						\
	: "memory")

#endif
