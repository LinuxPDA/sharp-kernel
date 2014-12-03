/*
 *  arch/ppc64/kernel/head.S
 *
 *  
 *
 *  PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Rewritten by Cort Dougan (cort@cs.nmt.edu) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@cs.nmt.edu>
 *  Adapted for Power Macintosh by Paul Mackerras.
 *  Low-level exception handlers and MMU support
 *  rewritten by Paul Mackerras.
 *    Copyright (C) 1996 Paul Mackerras.
 *
 *  Adapted for 64bit PowerPC by Dave Engebretsen, Peter Bergner, and
 *    Mike Corrigan {engebret|bergner|mikejc}@us.ibm.com 
 *
 *  This file contains the low-level support and setup for the
 *  PowerPC-64 platform, including trap and interrupt dispatch.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *	
 */

#define SECONDARY_PROCESSORS

#include "ppc_asm.h"
#include "ppc_defs.h"
#include <asm/processor.h>
#include <asm/page.h>
#include <linux/config.h>
#include <asm/mmu.h>

/*
 * We layout physical memory as follows:
 * 0x0000 - 0x00ff : Secondary processor spin code  
 * 0x0100 - 0x2fff : pSeries Interrupt prologs
 * 0x3000 - 0x3fff : Interrupt support
 * 0x4000 - 0x4fff : NACA  
 * 0x5000 - 0x5fff : Initial segment table
 * 0x6000          : iSeries and common interrupt prologs
 *
 */	

/*
 * Entering into this code we make the following assumptions:
 *  For pSeries:
 *   1. The MMU is off & open firmware is running in real mode.
 *   2. The kernel is entered at __start
 *
 *  For iSeries:
 *   1. The MMU is on (as it always is for iSeries)
 *   2. The kernel is entered at SystemReset_Iseries
 */
	
        .text
        .globl  _stext
_stext:
_STATIC(__start)
	b .__start_initialization_pSeries

	/* At offset 0x20, there is a pointer to iSeries LPAR data. 
	 * This is required by the hypervisor */
	. = 0x20
	.llong hvReleaseData-KERNELBASE

	/* At offset 0x28 and 0x30 are offsets to the msChunks
	 * array (used by the iSeries LPAR debugger to do translation
	 * between physical addresses and absolute addresses) and
	 * to the pidhash table (also used by the debugger) */
	.llong msChunks-KERNELBASE
	.llong pidhash-KERNELBASE

	/* Secondary processors spin on this value until it goes to 1. */
        .globl  __secondary_hold_spinloop
__secondary_hold_spinloop:
        .llong  0x0
	
	/* Secondary processors write this value with their cpu # */
	/* after they enter the spin loop immediatly below.       */
        .globl  __secondary_hold_acknowledge
__secondary_hold_acknowledge:
        .llong  0x0

	. = 0x60
/*
 * The following code is used on pSeries to hold secondary processors
 * in a spin loop after they have been freed from OpenFirmware, but
 * before the bulk of the kernel has been relocated.  This code
 * is relocated to physical address 0x60 before prom_init is run.
 * All of it must fit below the first exception vector at 0x100.
 */
_GLOBAL(__secondary_hold)
        /* Grab our cpu# */
        mr      r24,r3

        /* Tell the master cpu we're here */
        /* Relocation is off & we are located at an address less */
	/* than 0x100, so only need to grab low order offset.    */
	li	r3,__secondary_hold_acknowledge@l
        std     r24,0(r3)

	/* Get the address on which we are going to spin. */
	li	r3,__secondary_hold_spinloop@l

	/* All secondary cpu's wait here until told to start. */
100:    ld      r4,0(r3)
        cmpdi   0,r4,1
        bne     100b

#ifdef CONFIG_SMP
        mr      r3,r24
        b       .pseries_secondary_smp_init
#else
	BUG_OPCODE
#endif
	
/* 
 * The following macros define the code that appears as
 * the prologue to each of the exception handlers.  They
 * are split into two parts to allow a single kernel binary
 * to be used for pSeries, and iSeries.
 */

/*
 * We make as much of the exception code common between native Pseries
 * and Iseries LPAR implementations as possible.
 */

/*
 * This is the start of the interrupt handlers for Pseries
 * This code runs with relocation off.
 */

#define EXCEPTION_PROLOG_PSERIES(label)                                      \
	mtspr	SPRG2,r20;		    /* use SPRG2 as scratch reg   */ \
	mfspr	r20,SPRG3;		    /* get Paca virt addr         */ \
	clrldi	r20,r20,12;		    /* convert virt to real addr  */ \
	std	r21,PACAR21(r20);	    /* Save GPR21 in Paca         */ \
	mfspr	r21,SRR0;	    	    /* EA of interrupted instr    */ \
	std	r21,LPPACA+LPPACASRR0(r20); /* Set SRR0 in ItLpPaca       */ \
	mfspr	r21,SRR1;         	    /* machine state at interrupt */ \
	std	r21,LPPACA+LPPACASRR1(r20); /* Set SRR1 in ItLpPaca       */ \
	SET_REG_TO_LABEL(r21,label);	    /* Get addr to branch to      */ \
	mtspr	SRR0,r21;                                                    \
	li	r20,0x5;                    /* Turn on 64b, 64bints       */ \
        sldi	r20,r20,61;                                                  \
	ori	r20,r20,0x30;               /* for generic handlers       */ \
	mfmsr	r21;		            /* Turn on IR & DR            */ \
	or	r21,r21,r20;                /* for generic handlers       */ \
	mtspr	SRR1,r21;                                                    \
	rfid				    /* Jump to generic handlers  */
/* 
 * This is the start of the interrupt handlers for i_series
 * This code runs with relocation on.
 */	

#define EXCEPTION_PROLOG_ISERIES	\
	mtspr	SPRG2,r20;		/* use SPRG2 as scratch reg */\
	mfspr	r20,SPRG3;		/* get Paca */\
	std	r21,PACAR21(r20)	/* Save GPR21 in Paca */

#define EXCEPTION_PROLOG_COMMON		\
	mfspr	r20,SPRG3;		/* get Paca virt addr         */ \
        std     r22,PACAR22(r20);       /* Save GPR22 in Paca */\
        mfcr    r22;                    \
	ld	r21,LPPACA+LPPACASRR1(r20); /* Get SRR1 from ItLpPaca     */ \
	andi.	r21,r21,MSR_PR;		/* Set CR for later branch */ \
	beq+    1f; \
        ld      r21,PACAKSAVE(r20);     /* exception stack to use */\
	b       2f; \
1:                                                                 \
        subi    r21,r1,INT_FRAME_SIZE;/* alloc exc. frame */ \
2:                                                                 \
	std	r22,_CCR(r21);		/* save CR in stackframe */\
	ld	r22,PACAR21(r20);	/* Get GPR21 from Paca */\
	std	r22,GPR21(r21);		/* Save GPR21 in stackframe */\
	ld	r22,PACAR22(r20);	/* Get GPR22 from Paca */\
	std	r22,GPR22(r21);		/* Save GPR22 in stackframe */\
	std	r23,GPR23(r21);		/* Save GPR23 in stackframe */\
	mfspr	r22,SPRG2;		/* Get GPR20 from SPRG2 */\
	std	r22,GPR20(r21);		/* Save GPR20 in stackframe */\
	mflr	r22;		\
	std	r22,_LINK(r21);	\
	mfctr	r22;		\
	std	r22,_CTR(r21);	\
	mfspr	r22,XER;	\
	std	r22,_XER(r21);	\
	ld	r22,LPPACA+LPPACASRR0(r20); /* Get SRR0 from ItLpPaca */\
	ld	r23,LPPACA+LPPACASRR1(r20); /* Get SRR1 from ItLpPaca */\
	SAVE_8GPRS(0, r21);	\
	SAVE_4GPRS(8, r21);	\
	SAVE_2GPRS(12, r21);	\
	ld	r2,PACATOC(r20); \
	std	r1,0(r21);	\
	mr	r1,r21
/*
 * Note: code which follows this uses cr0.eq (set if from kernel),
 * r21, r22 (SRR0), and r23 (SRR1).
 */

/*
 * Exception vectors.
 */
#define STD_EXCEPTION_PSERIES(n, label )	\
	. = n;					\
	.globl label##_Pseries;			\
label##_Pseries:				\
	EXCEPTION_PROLOG_PSERIES( label##_common )

#define STD_EXCEPTION_ISERIES( label )		\
	.globl label##_Iseries;			\
label##_Iseries:				\
	EXCEPTION_PROLOG_ISERIES;		\
	b	label##_common

#define STD_EXCEPTION_COMMON( trap, label, hdlr )	\
	.globl label##_common;			\
label##_common:					\
	EXCEPTION_PROLOG_COMMON;		\
	addi	r3,r1,STACK_FRAME_OVERHEAD;	\
	SET_REG_TO_CONST(r20, MSR_KERNEL);	\
	li	r6,trap;			\
	bl	.transfer_to_handler; 		\
	.llong	hdlr;				\
	.llong	.ret_from_except

/* 
 * Start of pSeries system interrupt routines
 */
	. = 0x100
	.globl __start_interupts
__start_interupts:

	STD_EXCEPTION_PSERIES( 0x100, SystemReset )
	STD_EXCEPTION_PSERIES( 0x200, MachineCheck )
	STD_EXCEPTION_PSERIES( 0x300, DataAccess )
	STD_EXCEPTION_PSERIES( 0x380, DataAccessSLB )
	STD_EXCEPTION_PSERIES( 0x400, InstructionAccess )
	STD_EXCEPTION_PSERIES( 0x480, InstructionAccessSLB )
	STD_EXCEPTION_PSERIES( 0x500, HardwareInterrupt )
	STD_EXCEPTION_PSERIES( 0x600, Alignment )
	STD_EXCEPTION_PSERIES( 0x700, ProgramCheck )
	STD_EXCEPTION_PSERIES( 0x800, FPUnavailable )
	STD_EXCEPTION_PSERIES( 0x900, Decrementer )
	STD_EXCEPTION_PSERIES( 0xa00, Trap_0a )
	STD_EXCEPTION_PSERIES( 0xb00, Trap_0b ) 
	STD_EXCEPTION_PSERIES( 0xc00, SystemCall )
	STD_EXCEPTION_PSERIES( 0xd00, SingleStep ) 
	STD_EXCEPTION_PSERIES( 0xe00, Trap_0e )
	STD_EXCEPTION_PSERIES( 0xf00, PerformanceMonitor )
	
	. = 0x4000
	.globl __end_interupts
	.globl __start_naca
__end_interupts:
__start_naca:
	.globl xNaca
	/* Save space for naca.
	 * The first dword of the Naca is required by iSeries LPAR to
	 * point to itVpdAreas.  On pSeries native, this value is not used.
	 */
	.llong itVpdAreas
	.llong 0x0
	.llong 0x0
	.llong xPaca

	/* 
	 * Space for the initial segment table
	 * For LPAR, the hypervisor must fill in at least one entry
	 * before we get control (with relocate on)
	 */

	. = 0x5000
	.globl __end_naca
	.globl __start_stab
__end_naca:
__start_stab:


	. = 0x6000
	.globl __end_stab
__end_stab:

	/*
	 * The iSeries LPAR map is at this fixed address
	 * so that the HvReleaseData structure can address
	 * it with a 32-bit offset.
	 *
	 * The VSID values below are dependent on the 
	 * VSID generation algorithm.  See include/asm/mmu_context.h.
	 */
	
	.llong	1		/* # ESIDs to be mapped by hypervisor         */
	.llong	1		/* # memory ranges to be mapped by hypervisor */
	.llong	5		/* Page # of segment table within load area   */
	.llong	0		/* Reserved */
	.llong  0		/* Reserved */
	.llong  0		/* Reserved */
	.llong	0		/* Reserved */
	.llong	0		/* Reserved */
	.llong	0x0c00000000	/* ESID to map (Kernel at EA = 0xC000000000000000) */
	.llong	0x06a99b4b14    /* VSID to map (Kernel at VA = 0x6a99b4b140000000) */
	.llong	8192		/* # pages to map (32 MB) */
	.llong	0		/* Offset from start of loadarea to start of map */
	.llong	0x0006a99b4b140000	/* VPN of first page to map */

	. = 0x6100

/***  ISeries-LPAR interrupt handlers ***/

	STD_EXCEPTION_ISERIES( MachineCheck )
	STD_EXCEPTION_ISERIES( DataAccess )
	STD_EXCEPTION_ISERIES( DataAccessSLB )
	STD_EXCEPTION_ISERIES( InstructionAccess )
	STD_EXCEPTION_ISERIES( InstructionAccessSLB )
	STD_EXCEPTION_ISERIES( HardwareInterrupt )
	STD_EXCEPTION_ISERIES( Alignment )
	STD_EXCEPTION_ISERIES( ProgramCheck )
	STD_EXCEPTION_ISERIES( FPUnavailable )
	STD_EXCEPTION_ISERIES( Decrementer )
	STD_EXCEPTION_ISERIES( Trap_0a )
	STD_EXCEPTION_ISERIES( Trap_0b )
	STD_EXCEPTION_ISERIES( SystemCall )
	STD_EXCEPTION_ISERIES( SingleStep )
	STD_EXCEPTION_ISERIES( Trap_0e )
	STD_EXCEPTION_ISERIES( PerformanceMonitor )

	.globl SystemReset_Iseries
SystemReset_Iseries:
	mfspr	25,SPRG3		/* Get Paca address */
	lhz	r24,PACAPACAINDEX(r25)	/* Get processor # */
	cmpi	0,r24,0			/* Are we processor 0? */
	beq	.__start_initialization_iSeries	/* Start up the first processor */
	mfspr	r4,CTRLF
	li	r5,RUNLATCH		/* Turn off the run light */
	andc	r4,r4,r5
	mtspr	CTRLT,r4

1:
	HMT_LOW
#ifdef CONFIG_SMP
	lbz	r23,PACAPROCSTART(r25)	/* Test if this processor 
					 * should start */
	sync
	LOADADDR(r3,current_set)
	sldi	r28,r24,4		/* get current_set[cpu#] */
	ldx	r3,r3,r28
	addi	r1,r3,TASK_UNION_SIZE
	subi	r1,r1,STACK_FRAME_OVERHEAD

	cmpi	0,r23,0
	beq	iseries_secondary_smp_loop	/* Loop until told to go */
#ifdef SECONDARY_PROCESSORS
	bne	.__secondary_start		/* Loop until told to go */
#endif
iseries_secondary_smp_loop:
	/* Let the Hypervisor know we are alive */
	/* 8002 is a call to HvCallCfg::getLps, a harmless Hypervisor function */
	lis	r3,0x8002
	rldicr	r0,r3,32,15		/* r0 = (r3 << 32) & 0xffff000000000000 */
	rldicl	r3,r3,0,48		/* r3 = r3 & 0x000000000000ffff */
	or	r3,r3,r0		/* r3 = r3 | r0                 */
#else /* CONFIG_SMP */
	/* Yield the processor.  This is required for non-SMP kernels
	   which are running on multi-threaded machines. */
	lis	r3,0x8000
	rldicr	r3,r3,32,15		/* r3 = (r3 << 32) & 0xffff000000000000 */
	addi	r3,r3,18		/* r3 = 0x8000000000000012 which is "yield" */
	li	r4,0			/* "yield timed" */
	li	r5,-1			/* "yield forever" */
#endif /* CONFIG_SMP */
	li	r0,-1			/* r0=-1 indicates a Hypervisor call */
	sc				/* Invoke the hypervisor via a system call */
	mfspr	r25,SPRG3		/* Put r25 back ???? */
	b	1b			/* If SMP not configured, secondaries
					 * loop forever */

/*** Common interrupt handlers ***/

	STD_EXCEPTION_COMMON( 0x100, SystemReset, .SystemResetException )
	STD_EXCEPTION_COMMON( 0x200, MachineCheck, .MachineCheckException )
	STD_EXCEPTION_COMMON( 0x900, Decrementer, .timer_interrupt ) 
	STD_EXCEPTION_COMMON( 0xa00, Trap_0a, .UnknownException )
	STD_EXCEPTION_COMMON( 0xb00, Trap_0b, .UnknownException )
	STD_EXCEPTION_COMMON( 0xd00, SingleStep, .SingleStepException )
	STD_EXCEPTION_COMMON( 0xe00, Trap_0e, .UnknownException )
	STD_EXCEPTION_COMMON( 0xf00, PerformanceMonitor, .PerformanceMonitorException )

/* r20 is in SPRG2, 
   r21 is in the PACA
*/
	.globl DataAccess_common
DataAccess_common:
        mfcr    r20
	mfspr	r21,DAR
	srdi    r21,r21,60
	cmpi	0,r21,0xc
	bne     3f

	/* Segment faulted on a bolted segment. Go off and map that segment. */
	b       .do_stab_bolted

3:	mtcr    r20
	EXCEPTION_PROLOG_COMMON
	mfspr	r20,DSISR
	std	r20,_DSISR(r21)
	andis.	r0,r20,0xa450		/* weird error? */
	bne	1f			/* if not, try to put a PTE */
	mfspr	r3,DAR			/* into the hash table */
	std	r3,_DAR(r21)
	rlwinm	r4,r20,32-23,29,29	/* DSISR_STORE -> _PAGE_RW */

	andis.	r0,r20,0x0020		/* Is it a page table fault? */
	beq	2f			/* If so handle it */
	li	r4,0x300                /* Trap number */
	bl	.do_stab_SI
	b	1f	

2:	bl	.do_hash_page_DSI 	/* Try to handle as hpte fault */
1:
	ld 	r4,_DAR(r21)
	ld 	r5,_DSISR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD

	SET_REG_TO_CONST(r20, MSR_KERNEL)
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	li	r6,0x300
	bl	.transfer_to_handler
	.llong	.do_page_fault
	.llong	.ret_from_except

	.globl DataAccessSLB_common
DataAccessSLB_common:
	EXCEPTION_PROLOG_COMMON
	mfspr	r3,DAR
	li      r4,0x380                /* Exception vector  */
	bl	.ste_allocate
	or.	r3,r3,r3		/* Check return code */
	bne     1f                      /* Branch on failure */

	/* SLB Allocation success - Return. */
	ld	r3,_CCR(r21) 
	ld	r4,_LINK(r21)
	ld	r5,_CTR(r21)
	ld	r6,_XER(r21)

	mtcrf	0xff,r3
	mtlr	r4
	mtctr	r5
	mtspr	XER,r6
	REST_8GPRS(0,r21)
	REST_4GPRS(8,r21)
	REST_2GPRS(12,r21)
	mtspr	SRR1,r23
	mtspr	SRR0,r22
	ld	r20,GPR20(r21)
	ld	r22,GPR22(r21)
	ld	r23,GPR23(r21)
	ld	r21,GPR21(r21)
	RFID

1:
	mfspr	r4,DAR
	std	r4,_DAR(r21)
	mfspr	r5,DSISR
	std	r5,_DSISR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD

	SET_REG_TO_CONST(r20, MSR_KERNEL)
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	li	r6,0x380
	bl	.transfer_to_handler
	.llong	.do_page_fault
	.llong	.ret_from_except

	.globl InstructionAccess_common
InstructionAccess_common:
	EXCEPTION_PROLOG_COMMON

	andis.	r0,r23,0x0020		/* no ste found? */
	beq	2f
	mr	r3,r22			/* SRR0 at interrupt */
	li	r4,0x400		/* Trap number       */
	bl	.do_stab_SI
	b	1f

2:	andis.	r0,r23,0x4000		/* no pte found? */
	beq	1f			/* if so, try to put a PTE */
	mr	r3,r22			/* into the hash table */
	bl	.do_hash_page_ISI	/* Try to handle as hpte fault */
1:
	mr	r4,r22
	mr	r5,r23
	addi	r3,r1,STACK_FRAME_OVERHEAD
	SET_REG_TO_CONST(r20, MSR_KERNEL)
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	li	r6,0x400
	bl	.transfer_to_handler
	.llong	.do_page_fault
	.llong	.ret_from_except

	.globl InstructionAccessSLB_common
InstructionAccessSLB_common:
	EXCEPTION_PROLOG_COMMON
	mr      r3,r22                  /* SRR0 = NIA        */
	li	r4,0x480                /* Exception vector  */
	bl	.ste_allocate
	or.	r3,r3,r3		/* Check return code */
	bne     1f                      /* Branch on failure */

	/* SLB Allocation success - Return. */
	ld	r3,_CCR(r21) 
	ld	r4,_LINK(r21)
	ld	r5,_CTR(r21)
	ld	r6,_XER(r21)

	mtcrf	0xff,r3
	mtlr	r4
	mtctr	r5
	mtspr	XER,r6
	REST_8GPRS(0,r21)
	REST_4GPRS(8,r21)
	REST_2GPRS(12,r21)
	mtspr	SRR1,r23
	mtspr	SRR0,r22
	ld	r20,GPR20(r21)
	ld	r22,GPR22(r21)
	ld	r23,GPR23(r21)
	ld	r21,GPR21(r21)
	RFID

1:
	mfspr	r4,DAR
	std	r4,_DAR(r21)
	mfspr	r5,DSISR
	std	r5,_DSISR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD

	SET_REG_TO_CONST(r20, MSR_KERNEL)
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	li	r6,0x380
	bl	.transfer_to_handler
	.llong	.do_page_fault
	.llong	.ret_from_except

	.globl HardwareInterrupt_common
HardwareInterrupt_common:
	EXCEPTION_PROLOG_COMMON
HardwareInterrupt_entry:
	addi	r3,r1,STACK_FRAME_OVERHEAD
	SET_REG_TO_CONST(r20, MSR_KERNEL)
	li	r6,0x500
	bl	.transfer_to_handler
	.llong	.do_IRQ
	.llong	.ret_from_except

	.globl Alignment_common
Alignment_common:
	EXCEPTION_PROLOG_COMMON
	mfspr	r4,DAR
	std	r4,_DAR(r21)
	mfspr	r5,DSISR
	std	r5,_DSISR(r21)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	SET_REG_TO_CONST(r20, MSR_KERNEL)
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	li	r6,0x600
	bl	.transfer_to_handler
	.llong	.AlignmentException
	.llong	.ret_from_except

	.globl ProgramCheck_common
ProgramCheck_common:
	EXCEPTION_PROLOG_COMMON
	addi	r3,r1,STACK_FRAME_OVERHEAD
	SET_REG_TO_CONST(r20, MSR_KERNEL)
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	li	r6,0x700
	bl	.transfer_to_handler
	.llong	.ProgramCheckException
	.llong	.ret_from_except

	.globl FPUnavailable_common
FPUnavailable_common:
	EXCEPTION_PROLOG_COMMON
	bne	.load_up_fpu		/* if from user, just load it up */
	SET_REG_TO_CONST(r20, MSR_KERNEL)
	li	r6,0x800
	bl	.transfer_to_handler	/* if from kernel, take a trap */
	.llong	.KernelFP
	.llong	.ret_from_except

	.globl SystemCall_common
SystemCall_common:
	EXCEPTION_PROLOG_COMMON
#ifdef CONFIG_PPC_ISERIES
	cmpi	0,r0,0x5555		/* Special syscall to handle pending */
	bne+	1f			/* interrupts */
	andi.	r6,r23,MSR_PR		/* Only allowed from kernel */
	beq+	HardwareInterrupt_entry
1:
#endif
	std	r3,ORIG_GPR3(r21)
	SET_REG_TO_CONST(r20, MSR_KERNEL)
	rlwimi	r20,r23,0,16,16		/* copy EE bit from saved MSR */
	li	r6,0xC00
	bl	.transfer_to_handler
	.llong	.DoSyscall
	.llong	.ret_from_except

_GLOBAL(do_hash_page_ISI)
	li	r4,0
_GLOBAL(do_hash_page_DSI)
	rlwimi	r4,r23,32-13,30,30	/* Insert MSR_PR as _PAGE_USER */
	ori	r4,r4,1			/* add _PAGE_PRESENT */

	mflr	r21			/* Save LR in r21 */
					/* r21 restored later from r1 */
	/*
	 * r3 contains the faulting address
	 * r4 contains the required access permissions
	 *
	 * at return r3 = 0 for success
	 */

	bl	.hash_page		/* build HPTE if possible */
	mtlr	r21			/* restore LR */
	mr	r21,r1			/* restore r21 */

	or.	r3,r3,r3		/* Check return code */
	bnelr				/* Return to DSI or ISI on failure */

	/*
	 * The HPTE has been created/updated. Restore and retry the faulting inst
	 */

	ld	r3,_CCR(r21)
	ld	r4,_LINK(r21)
	ld	r5,_CTR(r21)
	ld	r6,_XER(r21)
	mtcrf	0xff,r3
	mtlr	r4
	mtctr	r5
	mtspr	XER,r6
	REST_8GPRS(0,r21)
	REST_4GPRS(8,r21)
	REST_2GPRS(12,r21)
	mtspr	SRR1,r23
	mtspr	SRR0,r22
	ld	r20,GPR20(r21)
	ld	r22,GPR22(r21)
	ld	r23,GPR23(r21)
	ld	r21,GPR21(r21)
	rfid

/* orig r20 is in SPRG2, 
   orig r21 is in the PACA
   r20 contains CCR

   r22 needs to be saved
   r1 needs to be saved
   CCR needs to be saved
*/
_GLOBAL(do_stab_bolted)
	mfsprg  r21,3
	std     r22,PACAR22(r21)
	std     r1,PACAR1(r21)
	stw     r20,PACACCR(r21)
	mfspr	r21,DAR

	/* (((ea >> 28) & 0x1fff) << 15) | (ea >> 60) */
	rldicl  r20,r21,36,32   /* Permits a full 32b of ESID */
	rldicr  r20,r20,15,48
	rldicl  r21,r21,4,60
        or      r20,r20,r21

	li      r21,9           /* VSID_RANDOMIZER */
	rldicr	r21,r21,32,31
	oris    r21,r21,58231
	ori     r21,r21,39831

	mulld   r20,r20,r21
	clrldi  r20,r20,28      /* r20 = vsid */

	mfsprg  r21,3
	ld      r21,PACASTABVIRT(r21)

	/* Hash to the primary group */
	mfspr	r22,DAR
	rldicl  r22,r22,36,59
	rldicr  r22,r22,7,56
	or      r21,r21,r22     /* r21 = first ste of the group */

        /* Search the primary group for a free entry */
	li      r22,0
1:	
	ld      r1,0(r21)      /* Test valid bit of the current ste   */
	rldicl  r1,r1,57,63
	cmpwi   r1,0
	bne     2f
	ld      r1,8(r21)      /* Get the current vsid part of the ste */
	rldimi  r1,r20,12,0    /* Insert the new vsid value            */
	std     r1,8(r21)      /* Put new entry back into the stab     */
	eieio                  /* Order vsid update                    */
	ld      r1,0(r21)      /* Get the esid part of the ste         */
	mfspr	r20,DAR        /* Get the new esid                     */
	rldicl  r20,r20,36,28  /* Permits a full 36b of ESID           */
	rldimi  r1,r20,28,0    /* Insert the new esid value            */
	ori     r1,r1,144      /* Turn on valid and kp                 */
	std     r1,0(r21)      /* Put new entry back into the stab     */
	sync                   /* Order the update                     */
	b       3f
2:	
	addi    r22,r22,1
	addi    r21,r21,16
	cmpldi  r22,7
	ble     1b

	/* Stick for only searching the primary group for now.          */
	/* At least for now, we use a very simple random castout scheme */
	/* Use the TB as a random number ;  OR in 1 to avoid entry 0    */
	mftb    r22
	andi.   r22,r22,7
	ori	r22,r22,1
	sldi	r22,r22,4

	/* r21 currently points to and ste one past the group of interest */
	/* make it point to the randomly selected entry                   */
	subi	r21,r21,128
	ori	r21,r21,r22      /* r21 is the entry to invalidate        */

	isync                    /* mark the entry invalid                */
	ld      r1,0(r21)
	li      r22,-129
	and     r1,r1,r22
	std     r1,0(r21)
	sync

	ld      r1,8(r21)
	rldimi  r1,r20,12,0
	std     r1,8(r21)
	eieio

	ld      r1,0(r21)      /* Get the esid part of the ste         */
	mr      r22,r1
	mfspr	r20,DAR        /* Get the new esid                     */
	rldicl  r20,r20,36,32  /* Permits a full 32b of ESID           */
	rldimi  r1,r20,28,0    /* Insert the new esid value            */
	ori     r1,r1,144      /* Turn on valid and kp                 */
	std     r1,0(r21)      /* Put new entry back into the stab     */

	rldicl  r22,r22,36,28
	rldicr  r22,r22,28,35
	slbie   r22
	sync

3:	
	/* All done -- return from exception. */
	mfsprg  r20,3                       /* Load the PACA pointer  */

	ld	r22,LPPACA+LPPACASRR0(r20); /* Get SRR0 from ItLpPaca */
	ld	r21,LPPACA+LPPACASRR1(r20); /* Get SRR1 from ItLpPaca */
	mtspr	SRR1,r21
	mtspr	SRR0,r22

	lwz     r21,PACACCR(r20)            /* Restore the clobbered regs */
	mtcr    r21
	ld      r1, PACAR1(r20)
	ld      r21,PACAR21(r20)
	ld      r22,PACAR22(r20)
	mfspr   r20,SPRG2
	rfid

_GLOBAL(do_stab_SI)
	mflr	r21			/* Save LR in r21 */
					/* r21 restored later from r1 */
	/*
	 * r3 contains the faulting address
	 * r4 contains the required access permissions
	 *
	 * at return r3 = 0 for success
	 */

	bl	.ste_allocate		/* build STE if possible */
	mtlr	r21			/* restore LR */
	mr	r21,r1			/* restore r21 */

	or.	r3,r3,r3		/* Check return code */
	bnelr				/* Return to DSI or ISI on failure */

	/*
	 * The STE has been created/updated. Restore and retry the faulting inst
	 */

	ld	r3,_CCR(r21)
	ld	r4,_LINK(r21)
	ld	r5,_CTR(r21)
	ld	r6,_XER(r21)

	mtcrf	0xff,r3
	mtlr	r4
	mtctr	r5
	mtspr	XER,r6
	REST_8GPRS(0,r21)
	REST_4GPRS(8,r21)
	REST_2GPRS(12,r21)
	mtspr	SRR1,r23
	mtspr	SRR0,r22
	ld	r20,GPR20(r21)
	ld	r22,GPR22(r21)
	ld	r23,GPR23(r21)
	ld	r21,GPR21(r21)
	rfid

/*
 * This code finishes saving the registers to the exception frame
 * and jumps to the appropriate handler for the exception.  Address
 * translation is already on.
 */
_GLOBAL(transfer_to_handler)
/* 
 * Save the rest of the registers into the pt_regs structure 
 */
	std	r22,_NIP(r21)
	std	r23,_MSR(r21)
	std	r6,TRAP(r21)
	ld	r6,GPR6(r21)
	SAVE_2GPRS(14, r21)
	SAVE_4GPRS(16, r21)
	SAVE_8GPRS(24, r21)
/*
 * Clear the RESULT field
 */
	li	r22,0
	std	r22,RESULT(r21)
/*
 * Test if from user state; result will be tested later 
 */
	andi.	r23,r23,MSR_PR		/* Set CR for later branch */
/*
 * Indicate that r1 contains the kernel stack and 
 * get the Kernel TOC and CURRENT pointers from the Paca
 */
	mfspr	r23,SPRG3		/* Get PACA */
	std	r22,PACAKSAVE(r23)	/* r1 is now kernel sp */
	ld	r2,PACATOC(r23)		/* Get Kernel TOC pointer */
	ld	r22,PACACURRENT(r23)	/* Get CURRENT */
/* 
 * If from user state, update THREAD.regs
 */
	beq	2f			/* Modify THREAD.regs if from user */
	addi	r24,r1,STACK_FRAME_OVERHEAD
	std	r24,THREAD+PT_REGS(r22)
2:
/*
 * Since we store 'current' in the PACA now, we don't need to 
 * set it here.  When r2 was used as 'current' it had to be
 * set here because it could have been changed by the user.
 */

/*
 * Check for kernel stack overflow
 */
	addi	r24,r22,TASK_STRUCT_SIZE /* check for kernel stack overflow */
	cmpld	0,r1,r22
	cmpld	1,r1,r24
	crand	1,1,4
	bgt-	.stack_ovf		/* if r22 < r1 < r22+TASK_STRUCT_SIZE */

/*
 * Get the handler address and its return address and
 * rfid to the handler
 */
/* MIKEC: Do we really need to rfid here?  Since we are already running
          with relocate on, could we just branch to the handler?
          We also will be turning on MSR.EE for some interrupts here
*/
	mflr	r23
	ld	r24,0(r23)		/* virtual address of handler */
	ld	r23,8(r23)		/* where to go when done */
	mtspr	SRR0,r24
	mtspr	SRR1,r20
	mtlr	r23
	rfid				/* jump to handler */

#ifdef CONFIG_SMP
/*
 * On pSeries, secondary processors spin in the following code.
 * At entry, r3 = this processor's number (in Linux terms, not hardware).
 */ 
_GLOBAL(pseries_secondary_smp_init)

	/* turn on 64-bit mode */
	bl	.enable_64b_mode
	isync

	/* Set up a Paca value for this processor. */
	LOADADDR(r24, xPaca) 		 /* Get base vaddr of Paca array  */
	mulli	r25,r3,PACA_SIZE	 /* Calculate vaddr of right Paca */
	add	r25,r25,r24              /* for this processor.           */

	mtspr	SPRG3,r25		 /* Save vaddr of Paca in SPRG3   */
	mr	r24,r3			 /* __secondary_start needs cpu#  */
	
1:
	HMT_LOW
	lbz	r23,PACAPROCSTART(r25)	 /* Test if this processor should */
					 /* start.                        */
	sync
	LOADADDR(r3,current_set)
	addi	r3,r3,8
	sldi	r28,r24,4		/* get current_set[cpu#] */
	ldx	r1,r3,r28

	cmpi	0,r23,0
#ifdef SECONDARY_PROCESSORS
	bne	.__secondary_start
#endif
	b 	1b			 /* Loop until told to go         */
#endif /* CONFIG_SMP */

_GLOBAL(__start_initialization_iSeries)

	LOADADDR(r1,init_task_union)
	addi	r1,r1,TASK_UNION_SIZE
	li	r0,0
	stdu	r0,-STACK_FRAME_OVERHEAD(r1)

	LOADADDR(r2,__toc_start)
	addi	r2,r2,0x4000
	addi	r2,r2,0x4000

	LOADADDR(r9,naca)
	SET_REG_TO_CONST(r4, KERNELBASE)
	addi	r4,r4,0x4000
	std	r4,0(r9)		/* set the naca pointer */

	/* Get the pointer to the segment table */
        ld	r6,PACA(r4)             /* Get the base Paca pointer       */
	ld	r4,PACASTABVIRT(r6)

	li	r3,1			/* Indicate to skip adding esid C to table */
	bl	.stab_initialize

	b	.start_here_common
	
_GLOBAL(__start_initialization_pSeries)
	mr	r31,r3			/* save parameters */
	mr	r30,r4
	mr	r29,r5
	mr	r28,r6
	mr	r27,r7
	mr	r26,r8                  /* YABOOT: debug_print() routine */
	mr	r25,r9                  /* YABOOT: debug_delay() routine */
	mr	r24,r10                 /* YABOOT: debug_prom() routine */

	bl	.enable_64b_mode

	/* put a relocation offset into r3 */
	bl	.reloc_offset

	LOADADDR(r2,__toc_start)
	addi    r2,r2,0x4000
	addi    r2,r2,0x4000

	/* Relocate the TOC from a virt addr to a real addr */	
	sub	r2,r2,r3

	/* setup the naca pointer which is needed by prom_init            */
	LOADADDR(r9,naca)
	sub	r9,r9,r3                /* addr of the variable naca      */

	SET_REG_TO_CONST(r4, KERNELBASE)
	sub	r4,r4,r3  		
	addi	r4,r4,0x4000
	std	r4,0(r9)		/* set the value of naca          */

	/* DRENG / PPPBBB Fix the following comment!!! -Peter */
	/* The following copies the first 0x100 bytes of code from the    */
	/* load addr to physical addr 0x0.  This code causes secondary    */
	/* processors to spin until a flag in the PACA is set.  This      */
	/* is done at this time rather than with the entire kernel        */
	/* relocation which is done below because we need to cause the    */
	/* processors to spin on code that is not going to move while OF  */
	/* is still alive. Although the spin code is not actually run on  */
	/* a uniprocessor, we always do this copy.                        */
	SET_REG_TO_CONST(r4, KERNELBASE)/* Src addr                       */
	sub	r4,r4,r3  		/* current address of __start     */
                                        /*        the source addr         */
	li	r3,0                    /* Dest addr                      */
	li	r5,0x100 		/* # bytes of memory to copy      */
	li	r6,0			/* Destination offset             */
	bl	.copy_and_flush		/* copy the first 0x100 bytes     */

	mr	r3,r31
	mr	r4,r30
	mr	r5,r29
	mr	r6,r28
	mr	r7,r27
	mr	r8,r26
	mr	r9,r25
	mr	r10,r24
	
	bl	.prom_init

	li	r24,0			/* cpu # */

/* 
 * At this point, r3 contains the physical address we are running at, 
 * returned by prom_init()
 */
_STATIC(__after_prom_start)

/*
 * We need to run with __start at physical address 0.
 * This will leave some code in the first 256B of
 * real memory, which are reserved for software use.
 * The remainder of the first page is loaded with the fixed 
 * interrupt vectors.  The next two pages are filled with
 * unknown exception placeholders. 
 * 
 * Note: This process overwrites the OF exception vectors.
 *       r26 == relocation offset
 *       r27 == KERNELBASE
 */
	bl	.reloc_offset
	mr	r26,r3    
	SET_REG_TO_CONST(r27,KERNELBASE)

	li	r3,0                    /* target addr */

	sub	r4,r27,r26 		/* source addr */
					/* current address of _start   */
                                        /*   i.e. where we are running */
                                        /*        the source addr      */

	LOADADDR(r5,copy_to_here)	/* # bytes of memory to copy      */
	sub	r5,r5,r27

	li	r6,0x100		/* Start offset, the first 0x100  */
					/* bytes were copied earlier.	  */

	bl	.copy_and_flush		/* copy the first n bytes         */
					/* this includes the code being   */
					/* executed here.                 */

	li	r0,4f@l			/* Jump to the copy of this code  */
	mtctr	r0			/* that we just made              */
	bctr
	
4:	LOADADDR(r9,rtas)
	sub	r9,r9,r26
	ld	r5,RTASBASE(r9)		/* get the value of rtas->base */
	ld	r9,RTASSIZE(r9)		/* get the value of rtas->size */
	bl	.copy_and_flush		/* copy upto rtas->base */
        add     r6,r6,r9		/* then skip over rtas->size bytes */

	LOADADDR(r5,klimit)
	sub	r5,r5,r26
	ld	r5,0(r5)		/* get the value of klimit */
	sub	r5,r5,r27
	bl	.copy_and_flush		/* copy the rest */
	b	.start_here_pSeries

/*
 * Copy routine used to copy the kernel to start at physical address 0
 * and flush and invalidate the caches as needed.
 * r3 = dest addr, r4 = source addr, r5 = copy limit, r6 = start offset
 * on exit, r3, r4, r5 are unchanged, r6 is updated to be >= r5.
 *
 * Note: this routine *only* clobbers r0, r6 and lr
 */
_STATIC(copy_and_flush)
	addi	r5,r5,-8
	addi	r6,r6,-8
4:	li	r0,16                   /* Use the least common      */
					/* denominator cache line    */
                                        /* size.  This results in    */
					/* extra cache line flushes  */
					/* but operation is correct. */
					/* Can't get cache line size */
					/* from NACA as it is being  */
					/* moved too.                */

	mtctr	r0			/* put # words/line in ctr */
3:	addi	r6,r6,8			/* copy a cache line */
	ldx	r0,r6,r4
	stdx	r0,r6,r3
	bdnz	3b
	dcbst	r6,r3			/* write it to memory */
	sync
	icbi	r6,r3			/* flush the icache line */
	cmpld	0,r6,r5
	blt	4b
	sync
	addi	r5,r5,8
	addi	r6,r6,8
	blr

.align 8
copy_to_here:
	
/*
 * On kernel stack overflow, load up an initial stack pointer
 * and call StackOverflow(regs), which should not return.
 */
_STATIC(stack_ovf)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	LOADADDR(r1,init_task_union)
	addi	r1,r1,TASK_UNION_SIZE-STACK_FRAME_OVERHEAD
	LOADADDR(r24,StackOverflow)
	SET_REG_TO_CONST(r20, MSR_KERNEL);
	mtspr	SRR0,r24
	mtspr	SRR1,r20
	rfid

/*
 * Disable FP for the task which had the FPU previously,
 * and save its floating-point registers in its thread_struct.
 * Enables the FPU for use in the kernel on return.
 * On SMP we know the fpu is free, since we give it up every
 * switch.  -- Cort
 */
_STATIC(load_up_fpu)
	mfmsr	r5                      /* grab the current MSR */
	ori	r5,r5,MSR_FP
	mtmsrd  r5			/* enable use of fpu now */
	SYNC
/*
 * For SMP, we don't do lazy FPU switching because it just gets too
 * horrendously complex, especially when a task switches from one CPU
 * to another.  Instead we call giveup_fpu in switch_to.
 *
 */
#ifndef CONFIG_SMP
	LOADBASE(r3,last_task_used_math)
	ld	r4,last_task_used_math@l(r3)
	cmpi	0,r4,0
	beq	1f
	addi	r4,r4,THREAD	       /* want THREAD of last_task_used_math */
	SAVE_32FPRS(0, r4)
	mffs	fr0
	stfd	fr0,THREAD_FPSCR-4(r4)
	ld	r5,PT_REGS(r4)
	ld	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	li	r20,MSR_FP|MSR_FE0|MSR_FE1
	andc	r4,r4,r20		/* disable FP for previous task */
	std	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#endif /* CONFIG_SMP */
	/* enable use of FP after return */
	ori	r23,r23,MSR_FP|MSR_FE0|MSR_FE1
	mfspr	r5,SPRG3		/* current task's PACA */
	ld	r5,PACACURRENT(r5)	/* Get 'current' */
	addi	r5,r5,THREAD		/* Get THREAD */
	lfd	fr0,THREAD_FPSCR-4(r5)
	mtfsf	0xff,fr0
	REST_32FPRS(0, r5)
#ifndef CONFIG_SMP
	subi	r4,r5,THREAD		/* Back to 'current' */
	std	r4,last_task_used_math@l(r3)
#endif /* CONFIG_SMP */
	/* restore registers and return */
	ld	r3,_CCR(r21)
	ld	r4,_LINK(r21)
	mtcrf	0xff,r3
	mtlr	r4
	REST_2GPRS(1, r21)
	REST_4GPRS(3, r21)
	/* we haven't used ctr or xer */
	mtspr	SRR1,r23
	mtspr	SRR0,r22
	REST_GPR(20, r21)
	REST_2GPRS(22, r21)
	ld	r21,GPR21(r21)
	rfid
	
/*
 * FP unavailable trap from kernel - print a message, but let
 * the task use FP in the kernel until it returns to user mode.
 */
_GLOBAL(KernelFP)
	ld	r3,_MSR(r1)
	ori	r3,r3,MSR_FP
	std	r3,_MSR(r1)		/* enable use of FP after return */
	LOADADDR(r3,86f)
	mfspr	r4,SPRG3		/* Get PACA */
	ld	r4,PACACURRENT(r4)	/* current */
	ld	r5,_NIP(r1)
	b	.ret_from_except
86:	.string	"floating point used in kernel (task=%p, pc=%x)\n"
	.align	4

/*
 * giveup_fpu(tsk)
 * Disable FP for the task given as the argument,
 * and save the floating-point registers in its thread_struct.
 * Enables the FPU for use in the kernel on return.
 */
_GLOBAL(giveup_fpu)
	mfmsr	r5
	ori	r5,r5,MSR_FP
	mtmsrd	r5			/* enable use of fpu now */
	SYNC
	cmpi	0,r3,0
	beqlr-				/* if no previous owner, done */
	addi	r3,r3,THREAD	        /* want THREAD of task */
	ld	r5,PT_REGS(r3)
	cmpi	0,r5,0
	SAVE_32FPRS(0, r3)
	mffs	fr0
	stfd	fr0,THREAD_FPSCR-4(r3)
	beq	1f
	ld	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
	li	r3,MSR_FP|MSR_FE0|MSR_FE1
	andc	r4,r4,r3		/* disable FP for previous task */
	std	r4,_MSR-STACK_FRAME_OVERHEAD(r5)
1:
#ifndef CONFIG_SMP
	li	r5,0
	LOADBASE(r4,last_task_used_math)
	std	r5,last_task_used_math@l(r4)
#endif /* CONFIG_SMP */
	blr



#ifdef CONFIG_SMP
/* 
 * This function is called after the master CPU has released the
 * secondary processors.  The execution environment is relocation off.
 * The Paca for this processor has the following fields initialized at
 * this point:	
 *   1. Processor number
 *   2. Segment table pointer (virtual address)
 * On entry the following are set:
 *   r24   = cpu# (in Linux terms)   
 *   r25   = Paca virtual address
 *   SPRG3 = Paca virtual address
 */
_GLOBAL(__secondary_start) 

	HMT_MEDIUM			/* Set thread priority to MEDIUM */

        /* set up the TOC (virtual address) */
	LOADADDR(r2,__toc_start)
	addi    r2,r2,0x4000
	addi    r2,r2,0x4000

	std	r2,PACATOC(r25)
	li	r6,0
	std	r6,PACAKSAVE(r25)

#ifndef CONFIG_PPC_ISERIES
	/* Initialize the page table pointer register. */
	LOADADDR(r6,_SDR1)
	ld	r6,0(r6)		/* get the value of _SDR1 */
	mtspr	SDR1,r6			/* set the htab location  */
#endif
	/* Initialize the segment table subsystem.                          */
	ld	r4,PACASTABVIRT(r25)    /* get addr of segment table        */
	li	r3,0			/* 0 -> include esid 0xC00000000    */
	std	r3,0(r1)                /* Zero the stack frame pointer     */
	bl	.stab_initialize

	LOADADDR(r3,current_set)
	sldi	r28,r24,4		/* get current_set[cpu#] */
	ldx	r6,r3,r28
	std	r6,PACACURRENT(r25)
	addi	r1,r6,TASK_UNION_SIZE
	subi	r1,r1,STACK_FRAME_OVERHEAD

	ld	r3,PACASTABREAL(r25)    /* get raddr of segment table       */
	ori	r4,r3,1			/* turn on valid bit                */

#ifdef CONFIG_PPC_ISERIES
	li	r0,-1			/* hypervisor call */
	li	r3,1
	sldi	r3,r3,63		/* 0x8000000000000000 */
	ori	r3,r3,4			/* 0x8000000000000004 */
	sc				/* HvCall_setASR */
#else
	mtasr	r4			/* set the stab location            */
#endif
	slbia

	li	r7,0
	mtlr	r7

	/* enable MMU and jump to start_secondary */
	LOADADDR(r3,.start_secondary)
	SET_REG_TO_CONST(r4, MSR_KERNEL)
	mtspr	SRR0,r3
	mtspr	SRR1,r4
	rfid
#endif /* CONFIG_SMP */

/*
 * This subroutine clobbers r11, r12 and the LR 
 */
_GLOBAL(enable_64b_mode)
        mfmsr   r11                      /* grab the current MSR */
        li      r12,1
        rldicr  r12,r12,MSR_SF_LG,(63-MSR_SF_LG)
        or      r11,r11,r12
        li      r12,1
        rldicr  r12,r12,MSR_ISF_LG,(63-MSR_ISF_LG)
        or      r11,r11,r12
        mtmsrd  r11
        SYNC
	blr

/*
 * This subroutine clobbers r11, r12 and the LR 
 */
_GLOBAL(enable_32b_mode)
        mfmsr   r11                      /* grab the current MSR */
        li      r12,1
        rldicr  r12,r12,MSR_SF_LG,(63-MSR_SF_LG)
	andc	r11,r11,r12
        li      r12,1
        rldicr  r12,r12,MSR_ISF_LG,(63-MSR_ISF_LG)
	andc	r11,r11,r12
        mtmsrd  r11                     
        SYNC
	blr


/*
 * This is where the main kernel code starts.
 */
_STATIC(start_here_pSeries)

#ifdef CONFIG_SMP
        /* All secondary cpus are now spinning on a common
         * spinloop, release them all now so they can start
         * to spin on their individual Paca spinloops.
         * For non SMP kernels, the secondary cpus never
         * get out of the common spinloop.
         */
	li	r3,1
        LOADADDR(r5,__secondary_hold_spinloop)
	tophys(r4,r5)
        std     r3,0(r4)
#endif

	/* get a new offset, now that the kernel has moved. */
        bl	.reloc_offset
	mr	r26,r3     

	/* The following gets the stack and TOC set up with the regs */
        /* pointing to the real addr of the kernel stack.  This is   */
	/* all done to support the C function call below which sets  */
	/* up the htab.  This is done because we have relocated the  */
	/* kernel but are still running in real mode. */

	/* real ptr to current */
	LOADADDR(r3,init_task_union)
	sub	r3,r3,r26
	
	/* set up a stack pointer (physical address) */
	addi	r1,r3,TASK_UNION_SIZE
	li	r0,0
	stdu	r0,-STACK_FRAME_OVERHEAD(r1)

        /* set up the TOC (physical address) */
	LOADADDR(r2,__toc_start)
	addi    r2,r2,0x4000
	addi    r2,r2,0x4000
	sub	r2,r2,r26

	/* setup the naca pointer which is needed by *tab_initialize       */
	LOADADDR(r6,naca)
	sub	r6,r6,r26                /* addr of the variable naca      */
	li	r27,0x4000
	std	r27,0(r6)	 	 /* set the value of naca          */

	/* Init naca->debug_switch so it can be used in stab & htab init.  */ 
	bl	.ppcdbg_initialize

	/* Get the pointer to the segment table which is used by           */
        /* stab_initialize                                                 */
        ld	r6,PACA(r27)            /* Get the base Paca pointer       */
	sub	r6,r6,r26		/* convert to physical addr         */
	mtspr	SPRG3,r6		/* PPPBBB: Temp... -Peter */
	ld	r4,PACASTABREAL(r6)
	ori	r3,r4,1			/* turn on valid bit                */
	mtasr	r3			/* set the stab location            */

	/* Initialize an initial memory mapping and turn on relocation.   */
	li	r3,0			/* 0 -> include esid 0xC00000000 */
        slbia
	bl	.stab_initialize
	bl	.htab_initialize
	
	LOADADDR(r6,_SDR1)	
	sub	r6,r6,r26
	ld	r6,0(r6)		/* get the value of _SDR1 */
	mtspr	SDR1,r6			/* set the htab location  */

	LOADADDR(r3,.start_here_common)
	SET_REG_TO_CONST(r4, MSR_KERNEL)
	mtspr	SRR0,r3
	mtspr	SRR1,r4
	rfid

	/* This is where all platforms converge execution */
_STATIC(start_here_common)
	/* relocation is on at this point */

	/* Clear out the BSS */
	LOADADDR(r11,_end)

	LOADADDR(r8,__bss_start)	
	
	sub	r11,r11,r8        /* bss size                        */
	addi	r11,r11,7         /* round up to an even double word */
	rldicl. r11,r11,61,3      /* shift right by 3                */
	beq	4f
	addi	r8,r8,-8
	li	r0,0
	mtctr	r11		  /* zero this many doublewords      */
3:	stdu	r0,8(r8)
	bdnz	3b
4:

	/* The following code sets up the SP and TOC now that we are */
	/* running with translation enabled. */

	/* ptr to current */
	LOADADDR(r3,init_task_union)
	
	/* set up the stack */
	addi	r1,r3,TASK_UNION_SIZE
	li	r0,0
	stdu	r0,-STACK_FRAME_OVERHEAD(r1)

        /* set up the TOC */
	LOADADDR(r2,__toc_start)
	addi    r2,r2,0x4000
	addi    r2,r2,0x4000

	/* setup the naca pointer                                         */
	LOADADDR(r9,naca)
	
	SET_REG_TO_CONST(r8, KERNELBASE)
	addi	r8,r8,0x4000
	std	r8,0(r9)		/* set the value of the naca ptr  */

	bl	.naca_init		/* Set initial values in the naca */
	
        LOADADDR(r4,naca)               /* Get Naca ptr address           */
	ld	r4,0(r4)                /* Get the location of the naca   */
        ld	r4,PACA(r4)             /* Get the base Paca pointer      */
	mtspr	SPRG3,r4
	
	/* ptr to current */
	LOADADDR(r3,init_task_union)
	std	r3,PACACURRENT(r4)
	
	std	r2,PACATOC(r4)
	li	r5,0
	std	r0,PACAKSAVE(r4)
/*
 *   Register          old definition              new definition
 *
 *   SPRG0             temp - used to save gpr     reserved for hypervisor
 *   SPRG1             temp - used to save gpr     unused 
 *   SPRG2             0 or kernel stack frame     temp - used to save gpr
 *   SPRG3             Linux thread                virt addr of Paca
 */

	/*	
	 * Restore the parms passed in from the bootloader. 
	 */
	mr	r3,r31
	mr	r4,r30
	mr	r5,r29
	mr	r6,r28
	mr	r7,r27

	bl	.setup_system

	/* Load up the kernel context */
5:
	bl .start_kernel
	
/*
 * We put a few things here that have to be page-aligned.
 * This stuff goes at the beginning of the data segment,
 * which is page-aligned.
 */
	.data
	.align  12
	.globl	sdata
sdata:
	.globl	empty_zero_page
empty_zero_page:
	.space	4096

	.globl	swapper_pg_dir
swapper_pg_dir:
	.space	4096	

	.globl	ioremap_dir
ioremap_dir:
	.space	4096

	.globl	bolted_dir
bolted_dir:
	.space	4096

/* 4096 * 31 bytes of storage */
	.globl	stab_array
stab_array:
	.space	131072
/*
 * This space gets a copy of optional info passed to us by the bootstrap
 * Used to pass parameters into the kernel like root=/dev/sda1, etc.
 */	
	.globl	cmd_line
cmd_line:
	.space	512 
