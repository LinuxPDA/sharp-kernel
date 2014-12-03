/*
 * SA1110 Assembler Power Management Routines
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
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
 * Change Log
 *	12-Nov-2001 Lineo Japan, Inc.
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/hardware.h>
#include <asm/arch/power.h>

/* 
 * cpu_sa1100_do_suspend()
 * 
 * Causes sa11x0 to enter sleep state
 * 
 */

#ifdef CONFIG_XIP_ROM
	.bss
#else
	.text
#endif


	.global	sleep_param
	.global	sleep_param_p

sleep_param:	.word	0		@ virtual address of parameter array
sleep_param_p:	.word	0		@ physical address of parameter array

#ifdef CONFIG_XIP_ROM

	.align 5

mmu_switch:	.space	8

	.text
#endif

	
ENTRY(cpu_sa1100_do_suspend)
	@ save registers on stack
	stmfd	sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}

	@ load virtual address for sleep_param array
#ifdef CONFIG_XIP_ROM
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
	str	r1, [r0, #(SLEEP_PARAM_CP15_R1*4)]
	mrc 	p15, 0, r1, c2, c0, 0
	str	r1, [r0, #(SLEEP_PARAM_CP15_R2*4)]
	mrc 	p15, 0, r1, c3, c0, 0
	str	r1, [r0, #(SLEEP_PARAM_CP15_R3*4)]
	mrc 	p15, 0, r1, c5, c0, 0
	str	r1, [r0, #(SLEEP_PARAM_CP15_R5*4)]
	mrc 	p15, 0, r1, c6, c0, 0
	str	r1, [r0, #(SLEEP_PARAM_CP15_R6*4)]
	mrc 	p15, 0, r1, c13, c0, 0
	str	r1, [r0, #(SLEEP_PARAM_CP15_R13*4)]

	@ clean data cache and invalidate WB
	bl	cpu_sa1100_cache_clean_invalidate_all

	@ disable clock switching
	mcr	p15, 0, r1, c15, c2, 2
	
	@ save address of sleep_param in r4
	mov	r0, r4

        @ Adjust memory timing before lowering CPU clock
	@ Clock speed ajdustment without changing memory timing makes
	@ CPU hang in some cases
        ldr     r0, =MDREFR
        ldr     r1, [r0]
        orr     r1, r1, #MDREFR_K1DB2
        str     r1, [r0]
	
	@ delay 90us and set CPU PLL to lowest speed
	@ fixes resume problem on high speed SA1110
	mov	r0, #90
	bl	SYMBOL_NAME(udelay)
	ldr	r0, =PPCR
	mov	r1, #0
	str	r1, [r0]
	mov	r0, #90
	bl	SYMBOL_NAME(udelay)


/* setup up register contents for jump to page containing SA1110 SDRAM controller bug fix suspend code
 *
 * r0 points to MSC0 register
 * r1 points to MSC1 register
 * r2 points to MSC2 register
 * r3 is MSC0 value
 * r4 is MSC1 value
 * r5 is MSC2 value
 * r6 points to MDREFR register
 * r7 is first MDREFR value
 * r8 is second MDREFR value
 * r9 is pointer to MDCNFG register
 * r10 is MDCNFG value
 * r11 is third MDREFR value
 * r12 is pointer to PMCR register
 * r13 is PMCR value (1)
 *
 */

	ldr	r0, =MSC0
	ldr	r1, =MSC1
	ldr	r2, =MSC2

        ldr     r3, [r0]
        bic     r3, r3, #FMsk(MSC_RT)
        bic     r3, r3, #FMsk(MSC_RT)<<16

        ldr     r4, [r1]
        bic     r4, r4, #FMsk(MSC_RT)
        bic     r4, r4, #FMsk(MSC_RT)<<16

        ldr     r5, [r2]
        bic     r5, r5, #FMsk(MSC_RT)
        bic     r5, r5, #FMsk(MSC_RT)<<16

        ldr     r6, =MDREFR

        ldr     r7, [r6]
        bic     r7, r7, #0x0000FF00
        bic     r7, r7, #0x000000F0
        orr     r8, r7, #MDREFR_SLFRSH

        ldr     r9, =MDCNFG
        ldr     r10, [r9]

#if defined(CONFIG_SA1100_COLLIE) || defined(CONFIG_SA1100_ASSABET)
        bic     r10, r10, #(MDCNFG_SA1110_DE0+MDCNFG_SA1110_DE1)
        bic     r10, r10, #(MDCNFG_SA1110_DE2+MDCNFG_SA1110_DE3)  
#else
        bic     r10, r10, #(MDCNFG_DE0+MDCNFG_DE1)
        bic     r10, r10, #(MDCNFG_DE2+MDCNFG_DE3)  
#endif

        bic     r11, r8, #MDREFR_SLFRSH
        bic     r11, r11, #MDREFR_E1PIN

        ldr     r12, =PMCR

        mov     r13, #PMCR_SF

	b	sa1110_sdram_controller_fix

	.align 5
sa1110_sdram_controller_fix:

	@ Step 1 clear RT field of all MSCx registers
	str 	r3, [r0]
	str	r4, [r1]
	str	r5, [r2]

	@ Step 2 clear DRI field in MDREFR
	str	r7, [r6]

	@ Step 3 set SLFRSH bit in MDREFR
	str	r8, [r6]

	@ Step 4 clear DE bis in MDCNFG
	str	r10, [r9]

	@ Step 5 clear DRAM refresh control register
	str	r11, [r6]

	@ Wow, now the hardware suspend request pins can be used, that makes them functional for 
	@ about 7 ns out of the	entire time that the CPU is running!

	@ Step 6 set force sleep bit in PMCR

	str	r13, [r12]
	
20:
	b	20b			@ loop waiting for sleep

/*
 * cpu_sa1100_resume()
 *
 * entry point from bootloader into kernel during resume
 * 
 */

	.align 5
ENTRY(cpu_sa1100_resume)

#ifdef CONFIG_SA1100_COLLIE
        @ enable clock switching
	mcr	p15, 0, r0, c15, c1, 2
#endif

	@ load physical address of sleep_param into r0
#ifdef CONFIG_XIP_ROM
	ldr	r0, =sleep_param_p
#else
	adr	r0, sleep_param_p
#endif
	ldr	r0, [r0]

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

#ifdef	CONFIG_XIP_ROM
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
#ifdef CONFIG_XIP_ROM
	ldr	r0, =sleep_param
	ldr	r0, [r0]
#else
	ldr	r0, sleep_param
#endif

	ldr	r1, [r0, #(SLEEP_PARAM_CP15_R13*4)]
	mcr	p15, 0, r1, c13, c0, 0

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

	@ Restore rest of Coprocessor state
	ldr	r1, [r0, #(SLEEP_PARAM_CP15_R5*4)]
	mcr 	p15, 0, r1, c5, c0 ,0
	
	ldr	r1, [r0, #(SLEEP_PARAM_CP15_R6*4)]
	mcr 	p15, 0, r1, c6, c0 ,0

	@ restore cpsr
	ldr	r1, [r0, #(SLEEP_PARAM_CPSR*4)]
	msr	cpsr, r1

	@ return to caller
	ldmfd	sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, pc}


	
#ifdef CONFIG_APM_CPU_IDLE
	.align 5
	.text

ENTRY(cpu_sa1100_idle)

	ldr	r1, =0xf0000000
	mcr	p15, 0, r0, c15, c2, 2
	ldr	r0,[r1]
	mcr	p15, 0, r0, c15, c8, 2
	nop

	mov	pc ,lr	

	
	.align 5
	.text

ENTRY(cpu_sa1100_idle_exit)

	mcr     p15, 0, r0, c15, c1, 2

	mov	pc ,lr	
	
#endif

