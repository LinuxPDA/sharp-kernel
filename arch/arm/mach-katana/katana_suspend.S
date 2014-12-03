/*
 * MQ9000(KATANA) Assembler Power Management Routines
 *
 * (C) Copyright 2003 Lineo uSolutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on linux/arch/arm/mach-sa1100/suspend.S
 *
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2001-12-06: Cliff Brake         Initial code
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/hardware.h>
#include <asm/arch/power.h>

#define PM04R	0xd8005810
#define PM05R	0xd8005814

	.macro debug_number, rn
	ldr	r3, =0xd8004000
1:      ldr     r2, [r3, #0x10]
        tst     r2, #1 << 7
        beq     1b
	mov	r2, #\rn
	strb	r2, [r3, #0x0c]
	mov	r2, #0x0d
	strb	r2, [r3, #0x0c]
	mov	r2, #0x0a
	strb	r2, [r3, #0x0c]
	.endm

/* 
 * cpu_katana_do_suspend()
 * 
 * Causes sa11x0 to enter sleep state
 * 
 */

#ifdef CONFIG_XIP_KERNEL
	.bss
#else
	.text
#endif


	.global	sleep_param
	.global	sleep_param_p

sleep_param:	.word	0		@ virtual address of parameter array
sleep_param_p:	.word	0		@ physical address of parameter array

MMU_CTL_MASK:				.word		0xFFFF0000	
MMU_TTB_MASK:				.word		0x00003FFF	

#ifdef CONFIG_XIP_KERNEL

	.align 5

mmu_switch:	.space	8

	.text
#endif

	
ENTRY(cpu_katana_do_suspend)
	@ save registers on stack
	stmfd	sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}

	debug_number 0x30
	@ load virtual address for sleep_param array
#ifdef CONFIG_XIP_KERNEL
	ldr	r0, =sleep_param
	ldr	r0, [r0]
#else
	ldr	r0, sleep_param
#endif

	@ save cpsr
	mrs	r1, cpsr
	str	r1, [r0, #(SLEEP_PARAM_CPSR*4)]

	@ save register for all modes 
	@ we are currently in SVC mode
	str	r13, [r0, #(SLEEP_PARAM_SVC_R13*4)]
	str	r14, [r0, #(SLEEP_PARAM_SVC_R14*4)]
	mrs	r1, spsr
	str	r1, [r0, #(SLEEP_PARAM_SVC_SPSR*4)]
	
	@ usr mode
	mrs	r3, cpsr	@ save for later
	bic	r1, r3, #0x1F
	orr	r1, r1, #0xDF
	msr	cpsr, r1
	str	r13, [r0, #(SLEEP_PARAM_USER_R13*4)]
	str	r14, [r0, #(SLEEP_PARAM_USER_R14*4)]
	
	@ abort mode
	mrs	r1, cpsr
	bic	r1, r1, #0x1F
	orr	r1, r1, #0xD7
	msr	cpsr, r1
	str	r13, [r0, #(SLEEP_PARAM_ABORT_R13*4)]
	str	r14, [r0, #(SLEEP_PARAM_ABORT_R14*4)]
	mrs	r1, spsr
	str	r1, [r0, #(SLEEP_PARAM_ABORT_SPSR*4)]

	@ undef mode
	mrs	r1, cpsr
	bic	r1, r1, #0x1F
	orr	r1, r1, #0xDB
	msr	cpsr, r1
	str	r13, [r0, #(SLEEP_PARAM_UNDEF_R13*4)]
	str	r14, [r0, #(SLEEP_PARAM_UNDEF_R14*4)]
	mrs	r1, spsr
	str	r1, [r0, #(SLEEP_PARAM_UNDEF_SPSR*4)]

	@ irq mode
	mrs	r1, cpsr
	bic	r1, r1, #0x1F
	orr	r1, r1, #0xD2
	msr	cpsr, r1
	str	r13, [r0, #(SLEEP_PARAM_IRQ_R13*4)]
	str	r14, [r0, #(SLEEP_PARAM_IRQ_R14*4)]
	mrs	r1, spsr
	str	r1, [r0, #(SLEEP_PARAM_IRQ_SPSR*4)]

	@ fiq mode
	mrs	r1, cpsr
	bic	r1, r1, #0x1F
	orr	r1, r1, #0xD1
	msr	cpsr, r1
	str	r8, [r0, #(SLEEP_PARAM_FIQ_R8*4)]
	str	r9, [r0, #(SLEEP_PARAM_FIQ_R9*4)]
	str	r10, [r0, #(SLEEP_PARAM_FIQ_R10*4)]
	str	r11, [r0, #(SLEEP_PARAM_FIQ_R11*4)]
	str	r12, [r0, #(SLEEP_PARAM_FIQ_R12*4)]
	str	r13, [r0, #(SLEEP_PARAM_FIQ_R13*4)]
	str	r14, [r0, #(SLEEP_PARAM_IRQ_R14*4)]
	mrs	r1, spsr
	str	r1, [r0, #(SLEEP_PARAM_IRQ_SPSR*4)]

	@ go back to svc mode
	mrs	r1, cpsr
	bic	r1, r1, #0x1F
	orr	r1, r1, #0xD3
	msr	cpsr, r1

	@ save coprocessor registers
	mrc 	p15, 0, r1, c1, c0, 0
	ldr     r2, MMU_CTL_MASK               @ mask off the undefined bits
	bic     r1, r1, r2
	str	r1, [r0, #(SLEEP_PARAM_CP15_R1*4)]
	mrc 	p15, 0, r1, c2, c0, 0
	ldr     r2, MMU_TTB_MASK               @ mask off the undefined bits
	bic     r1, r1, r2
	str	r1, [r0, #(SLEEP_PARAM_CP15_R2*4)]
	mrc 	p15, 0, r1, c3, c0, 0
	str	r1, [r0, #(SLEEP_PARAM_CP15_R3*4)]

	@ clean data cache and invalidate WB
	bl	cpu_arm922_cache_clean_invalidate_all

	@ go to standby state
	ldr	r0, =PM04R
	mov	r1, #1
	str	r1, [r0]
	
	@ldr	r1, =0xd8005800
	@ldr	r2, [r1, #0x00]
	@bic	r2, r2, #0xf0
	@str	r2, [r1, #0x00]
20:
	@add	r1, r1, #1
	@tst	r1, #1 << 23
	@bne	20b
	debug_number 0x41
	b	20b			@ loop waiting for standby

/*
 * cpu_katana_resume()
 *
 * entry point from bootloader into kernel during resume
 * 
 */

	.align 5
ENTRY(cpu_katana_resume)

	@ load physical address of sleep_param into r0
#ifdef CONFIG_XIP_KERNEL
	ldr	r0, =sleep_param_p
	ldr	r0, [r0]
#else
	adr	r0, sleep_param_p
#endif

	@ restore cp15_r3, domain id
	ldr	r1, [r0, #(SLEEP_PARAM_CP15_R3*4)]
	mcr 	p15, 0, r1, c3, c0 ,0

	@ restore cp15_r2, translation table base address
	ldr	r1, [r0, #(SLEEP_PARAM_CP15_R2*4)]
	mcr 	p15, 0, r1, c2, c0 ,0

	mov	r1, #0
	mcr	p15, 0, r1, c8, c7, 0   	@ flush I+D TLBs
	mcr	p15, 0, r1, c7, c7, 0		@ flush I&D cache 
	
	@ get saved cp15 r1 (control register)
	ldr	r1, [r0, #(SLEEP_PARAM_CP15_R1*4)]

	@ get address to jump to after turning on MMU
	ldr	r2, =resume_after_mmu 

#ifdef	CONFIG_XIP_KERNEL
	adr	r3, resume_turn_on_mmu
	ldr	r4, =mmu_switch
	ldr	r0, [r3]
	str	r0, [r4]
	ldr	r0, [r3, #4]
	str	r0, [r4, #4]
	cmp	r2, #0				
	mov	pc, r4
#else
	cmp	r2, #0				

	b	resume_turn_on_mmu
#endif

	.align 5
resume_turn_on_mmu:

	@ turn on mmu
	mcr 	p15, 0, r1, c1, c0 ,0

	@ jump to resume_after_mmu	
	mov	pc, r2
	nop
	nop

	.align 5
resume_after_mmu:
	
	@ load virtual address for sleep_param array
#ifdef CONFIG_XIP_KERNEL
	ldr	r0, =sleep_param
	ldr	r0, [r0]
#else
	ldr	r0, sleep_param
#endif

	@ Restore the rest of the CPU state
	
	@ svc
	ldr	r13, [r0, #(SLEEP_PARAM_SVC_R13*4)]
	ldr	r14, [r0, #(SLEEP_PARAM_SVC_R14*4)]
	ldr	r1, [r0, #(SLEEP_PARAM_SVC_SPSR*4)]
	msr	spsr, r1 

	@ usr
	mrs     r1, cpsr			
	bic     r1, r1, #0x1F		
	orr	r1, r1, #0xDF
	msr     cpsr, r1	
	ldr	r13, [r0, #(SLEEP_PARAM_USER_R13*4)]
	ldr	r14, [r0, #(SLEEP_PARAM_USER_R14*4)]

	@ abort
	mrs     r1, cpsr
	bic     r1, r1, #0x1F
	orr	r1, r1, #0xD7
	msr     cpsr, r1	
	ldr	r13, [r0, #(SLEEP_PARAM_ABORT_R13*4)]
	ldr	r14, [r0, #(SLEEP_PARAM_ABORT_R14*4)]
	ldr	r1, [r0, #(SLEEP_PARAM_ABORT_SPSR*4)]
	msr	spsr, r1

	@ undef
	mrs     r1, cpsr
	bic     r1, r1, #0x1F
	orr	r1, r1, #0xDB
	msr     cpsr, r1
	ldr	r13, [r0, #(SLEEP_PARAM_UNDEF_R13*4)]
	ldr	r14, [r0, #(SLEEP_PARAM_UNDEF_R14*4)]
	ldr	r1, [r0, #(SLEEP_PARAM_UNDEF_SPSR*4)]
	msr	spsr, r0

	@ irq
	mrs     r1, cpsr
	bic     r1, r1, #0x1F
	orr	r1, r1, #0xD2
	msr     cpsr, r1
	ldr	r13, [r0, #(SLEEP_PARAM_IRQ_R13*4)]
	ldr	r14, [r0, #(SLEEP_PARAM_IRQ_R14*4)]
	ldr	r1, [r0, #(SLEEP_PARAM_IRQ_SPSR*4)]
	msr	spsr, r1

	@ fiq
	mrs     r1, cpsr
	bic     r1, r1, #0x1F
	orr	r1, r1, #0xD1
	msr     cpsr, r1
	ldr	r8, [r0, #(SLEEP_PARAM_FIQ_R8*4)]
	ldr	r9, [r0, #(SLEEP_PARAM_FIQ_R9*4)]
	ldr	r10, [r0, #(SLEEP_PARAM_FIQ_R10*4)]
	ldr	r11, [r0, #(SLEEP_PARAM_FIQ_R11*4)]
	ldr	r12, [r0, #(SLEEP_PARAM_FIQ_R12*4)]
	ldr	r13, [r0, #(SLEEP_PARAM_FIQ_R13*4)]
	ldr	r14, [r0, #(SLEEP_PARAM_FIQ_R14*4)]
	ldr	r1, [r0, #(SLEEP_PARAM_FIQ_SPSR*4)]
	msr	spsr, r1

	@ switch to svc mode
	mrs     r1, cpsr
	bic     r1, r1, #0x1F
	orr	r1, r1, #0xD3
	msr     cpsr, r1

	@ restore cpsr
	ldr	r1, [r0, #(SLEEP_PARAM_CPSR*4)]
	msr	cpsr, r1

	@ return to caller
	ldmfd	sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, pc}

	
#ifdef CONFIG_APM_CPU_IDLE
	.align 5
	.text

ENTRY(cpu_katana_idle)

	ldr	r0, =PM05R
	mov	r1, #1
	str	r1, [r0]
	nop

	mov	pc ,lr	
#endif

msg_str:	.asciz  "debug\n\r"
		.align
