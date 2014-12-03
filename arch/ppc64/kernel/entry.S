/*
 *  arch/ppc/kernel/entry.S
 *
 *  
 *
 *  PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *  Rewritten by Cort Dougan (cort@cs.nmt.edu) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@cs.nmt.edu>
 *  Adapted for Power Macintosh by Paul Mackerras.
 *  Low-level exception handlers and MMU support
 *  rewritten by Paul Mackerras.
 *    Copyright (C) 1996 Paul Mackerras.
 *  MPC8xx modifications Copyright (C) 1997 Dan Malek (dmalek@jlc.net).
 *
 *  This file contains the system call entry code, context switch
 *  code, and exception/interrupt return code for PowerPC.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *	
 */


#include "ppc_asm.h"
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <linux/errno.h>
#include <linux/sys.h>
#include <linux/config.h>

#undef SHOW_SYSCALLS
#undef SHOW_SYSCALLS_TASK

#ifdef SHOW_SYSCALLS_TASK
	.data
show_syscalls_task:
	.long	-1
#endif

/*
 * Handle a system call.
 */
	.text
_GLOBAL(DoSyscall)
	mfspr	r30,SPRG3	/* Get PACA */
	ld	r29,PACACURRENT(r30)
	std	r0,THREAD+LAST_SYSCALL(r29)
	ld	r11,_CCR(r1)	/* Clear SO bit in CR */
	lis	r10,0x1000
	andc	r11,r11,r10
	std	r11,_CCR(r1)
#ifdef SHOW_SYSCALLS
#ifdef SHOW_SYSCALLS_TASK
	LOADBASE(r31,show_syscalls_task)
	ld	r31,show_syscalls_task@l(r31)
	cmp	0,r29,r31
	bne	1f
#endif
	LOADADDR(r3,7f)
	ld	r4,GPR0(r1)
	ld	r5,GPR3(r1)
	ld	r6,GPR4(r1)
	ld	r7,GPR5(r1)
	ld	r8,GPR6(r1)
	ld	r9,GPR7(r1)
	bl	.printk
	LOADADDR(r3,77f)
	ld	r4,GPR8(r1)
	ld	r5,GPR9(r1)
	mr	r6,r29
	bl	.printk
	ld	r0,GPR0(r1)
	ld	r3,GPR3(r1)
	ld	r4,GPR4(r1)
	ld	r5,GPR5(r1)
	ld	r6,GPR6(r1)
	ld	r7,GPR7(r1)
	ld	r8,GPR8(r1)
1:
#endif /* SHOW_SYSCALLS */
	cmpi	0,r0,0x7777	/* Special case for 'sys_sigreturn' */
	beq-	10f
      	cmpi	0,r0,0x6666	/* Special case for 'sys_rt_sigreturn' */
	beq-	16f

	/* MIKEC: I changed TASK_PTRACE to 64-bit here.  Should I have? 
	 *        TASK_PTRACE is aka task_struct.ptrace (in linux/sched.h)
         *        which is an unsigned long 
	 */
	ld	r10,TASK_PTRACE(r29)
	andi.	r10,r10,PT_TRACESYS
	bne-	50f
	cmpli	0,r0,NR_syscalls
	bge-	66f
/* Ken Aaker: Need to vector to 32 Bit or default sys_call_table here,
 *            based on caller's run-mode / personality.
 *
 */
#ifdef CONFIG_BINFMT_ELF32
	ld	r10,THREAD+THREAD_FLAGS(r29)
	andi.	r10,r10,PPC_FLAG_32BIT
	beq+	15f
	LOADADDR(r10,.sys_call_table32)
/* Now mung the first 4 parameters into shape, by making certain that
 * the high bits (most significant 32 bits in 64 bit reg) are 0
 * for the first 4 parameter regs(3-6).
 */
	clrldi	r3,r3,32
	clrldi	r4,r4,32
	clrldi	r5,r5,32
	clrldi	r6,r6,32
	b	17f
15:
#endif
	LOADADDR(r10,.sys_call_table)
17:
	slwi	r0,r0,3
	ldx	r10,r10,r0	/* Fetch system call handler [ptr] */
	cmpi	0,r10,0
	beq-	66f
	mtlr	r10
	addi	r9,r1,STACK_FRAME_OVERHEAD
	blrl			/* Call handler */
_GLOBAL(ret_from_syscall_1)
20:	std	r3,RESULT(r1)	/* Save result */
#ifdef SHOW_SYSCALLS
#ifdef SHOW_SYSCALLS_TASK
	/* MIKEC: Will r29 (current) be preserved through any method of getting
                  to ret_from_syscall_1 ?? */
	cmp	0,r29,r31
	bne	91f
#endif
	mr	r4,r3
	LOADADDR(r3,79f)
	bl	.printk
	ld	r3,RESULT(r1)
91:
#endif
	li	r10,-_LAST_ERRNO
	cmpl	0,r3,r10
	blt	30f
	neg	r3,r3
	cmpi	0,r3,ERESTARTNOHAND
	bne	22f
	li	r3,EINTR
22:	ld	r10,_CCR(r1)	/* Set SO bit in CR */
	oris	r10,r10,0x1000
	std	r10,_CCR(r1)
30:	std	r3,GPR3(r1)	/* Update return value */
	b	.ret_from_except
66:	li	r3,ENOSYS
	b	22b
/* sys_sigreturn */
10:	addi	r3,r1,STACK_FRAME_OVERHEAD
	bl	.sys_sigreturn
	cmpi	0,r3,0		/* Check for restarted system call */
	bge	.ret_from_except
	b	20b
/* sys_rt_sigreturn */
16:	addi	r3,r1,STACK_FRAME_OVERHEAD
	bl	.sys_rt_sigreturn
	cmpi	0,r3,0		/* Check for restarted system call */
	bge	.ret_from_except
	b	20b
        
/* Traced system call support */
50:	bl	.syscall_trace
	ld	r0,GPR0(r1)	/* Restore original registers */
	ld	r3,GPR3(r1)
	ld	r4,GPR4(r1)
	ld	r5,GPR5(r1)
	ld	r6,GPR6(r1)
	ld	r7,GPR7(r1)
	ld	r8,GPR8(r1)
	ld	r9,GPR9(r1)
	cmpli	0,r0,NR_syscalls
	bge-	66f
#ifdef CONFIG_BINFMT_ELF32
	ld	r10,THREAD+THREAD_FLAGS(r29)
	andi.	r10,r10,PPC_FLAG_32BIT
	beq+	55f
	LOADADDR(r10,.sys_call_table32)
/* Now mung the first 4 parameters into shape, by making certain that
 * the high bits (most significant 32 bits in 64 bit reg) are 0
 * for the first 4 parameter regs(3-6).
 */
	clrldi	r3,r3,32
	clrldi	r4,r4,32
	clrldi	r5,r5,32
	clrldi	r6,r6,32
	b	57f
55:
#endif
	LOADADDR(r10,.sys_call_table)
57:
	slwi	r0,r0,3
	ldx	r10,r10,r0	/* Fetch system call handler [ptr] */
	cmpi	0,r10,0
	beq-	66f
	mtlr	r10
	addi	r9,r1,STACK_FRAME_OVERHEAD
	blrl			/* Call handler */
_GLOBAL(ret_from_syscall_2)
	std	r3,RESULT(r1)	/* Save result */	
	std	r3,GPR0(r1)	/* temporary gross hack to make strace work */
	li	r10,-_LAST_ERRNO
	cmpl	0,r3,r10
	blt	60f
	neg	r3,r3
	cmpi	0,r3,ERESTARTNOHAND
	bne	57f
	li	r3,EINTR
57:	ld	r10,_CCR(r1)	/* Set SO bit in CR */
	oris	r10,r10,0x1000
	std	r10,_CCR(r1)
60:	std	r3,GPR3(r1)	/* Update return value */
	bl	.syscall_trace
	b	.ret_from_except
66:	li	r3,ENOSYS
	b	57b
#ifdef SHOW_SYSCALLS
7:	.string	"syscall %d(%x, %x, %x, %x, %x, "
77:	.string	"%x, %x), current=%p\n"
79:	.string	" -> %x\n"
	.align	2,0
#endif

/*
 * This routine switches between two different tasks.  The process
 * state of one is saved on its kernel stack.  Then the state
 * of the other is restored from its kernel stack.  The memory
 * management hardware is updated to the second process's state.
 * Finally, we can return to the second process, via ret_from_except.
 * On entry, r3 points to the THREAD for the current task, r4
 * points to the THREAD for the new task.
 *
 * Note: there are two ways to get to the "going out" portion
 * of this code; either by coming in via the entry (_switch)
 * or via "fork" which must set up an environment equivalent
 * to the "_switch" path.  If you change this (or in particular, the
 * SAVE_REGS macro), you'll have to change the fork code also.
 *
 * The code which creates the new task context is in 'copy_thread'
 * in arch/ppc/kernel/process.c
 */	
_GLOBAL(_switch)
	stdu	r1,-INT_FRAME_SIZE(r1)
	ld	r6,0(r1)
	std	r6,GPR1(r1)
	/* r3-r13 are caller saved -- Cort */
	SAVE_GPR(2, r1)
	SAVE_8GPRS(14, r1)
	SAVE_10GPRS(22, r1)
	mflr	r20		/* Return to switch caller */
	mfmsr	r22
	li	r6,MSR_FP	/* Disable floating-point */
	andc	r22,r22,r6
	std	r20,_NIP(r1)
	std	r22,_MSR(r1)
	std	r20,_LINK(r1)
	mfcr	r20
	mfctr	r22
	mfspr	r23,XER
	std	r20,_CCR(r1)
	std	r22,_CTR(r1)
	std	r23,_XER(r1)
	li	r6,0x0ff0
	std	r6,TRAP(r1)
	std	r1,KSP(r3)	/* Set old stack pointer */

	mfspr	r5,SPRG3	/* Get Paca */
	ld	r3,PACACURRENT(r5)	/* Get old 'current' for return value */

	addi	r6,r4,-THREAD	/* Convert THREAD to 'current' */
	std	r6,PACACURRENT(r5)	/* Set new 'current' */

	ld	r1,KSP(r4)	/* Load new stack pointer */
	ld	r9,_MSR(r1)	/* Returning to user mode? */
	andi.	r9,r9,MSR_PR
	beq+	10f		/* if not, don't adjust kernel stack */
8:	addi	r7,r1,INT_FRAME_SIZE	/* size of frame */
	std	r7,KSP(r4)	/* save kernel stack pointer */
	std	r1,PACAKSAVE(r5)	/* save exception stack pointer */
10:	ld	r7,_CTR(r1)
	ld	r6,_LINK(r1)
	mtctr	r7
	mtlr	r6
	ld	r7,_XER(r1)
	ld	r6,_CCR(r1)
	mtspr	XER,r7
	mtcrf	0xFF,r6
	/* r3-r13 are destroyed -- Cort */
	REST_GPR(14, r1)
	REST_8GPRS(15, r1)
	REST_8GPRS(23, r1)
	REST_GPR(31, r1)
	ld	r7,_NIP(r1)	/* Restore environment */

	mfmsr	r6		/* Get current interrupt state */
	rldicl	r6,r6,48,1
	rldicl	r6,r6,16,0      /* clear MSR_EE */
	mtmsrd	r6		/* Update machine state */
	
	ld	r6,_MSR(r1)
	ld	r2,GPR2(r1)
	ld	r1,GPR1(r1)
	mtspr	SRR0,r7
	mtspr	SRR1,r6

        /* sync required to force memory operations on this processor */
        /* to complete before the current thread gives up control.    */
	SYNC
	rfid

#ifdef CONFIG_SMP	
_GLOBAL(ret_from_smpfork)
	bl	.schedule_tail
	b	.ret_from_except
#endif

_GLOBAL(ret_from_except)
#ifdef CONFIG_PPC_ISERIES
	ld	r5,_MSR(r1)
	andi.	r5,r5,MSR_EE
	beq	4f
irq_recheck:
	mfmsr	r5
	andi.	r5,r5,MSR_EE
	bne	4f
	/*
	 * Check for pending interrupts (iSeries)
	 */
	CHECKANYINT(r4,r5)
	beq+	4f	/* skip do_IRQ if no interrupts */

	addi	r3,r1,STACK_FRAME_OVERHEAD
	bl	.do_IRQ
	b	irq_recheck	/* loop back and handle more */
4:
#endif
_GLOBAL(do_bottom_half_ret)
	ld	r3,_MSR(r1)	/* Returning to user mode? */
	andi.	r3,r3,MSR_PR
	beq+	restore		/* if so, check need_resched and signals */
_GLOBAL(ret_to_user_hook)
	nop
	/* NEED_RESCHED is a volatile long (64-bits) */ 
	mfspr	r4,SPRG3		/* current task's PACA */
	ld	r4,PACACURRENT(r4)	/* Get 'current' */
	ld	r3,NEED_RESCHED(r4)
	cmpi	0,r3,0		/* check need_resched flag */
	beq+	7f
	bl	.schedule
	/* SIGPENDING is an int (32-bits) */
7:	
	mfspr	r4,SPRG3		/* current task's PACA */
	ld	r4,PACACURRENT(r4)	/* Get 'current' */
	lwz	r5,SIGPENDING(r4) /* Check for pending unblocked signals */
	cmpwi	0,r5,0
	beq+	restore
	li	r3,0
	addi	r4,r1,STACK_FRAME_OVERHEAD
	bl	.do_signal
_GLOBAL(do_signal_ret)
restore:	
	ld	r3,_CTR(r1)
	ld	r0,_LINK(r1)
	mtctr	r3
	mtlr	r0
	ld	r3,_XER(r1)
	mtspr	XER,r3
	REST_8GPRS(5, r1)
	REST_10GPRS(13, r1)
	REST_8GPRS(23, r1)
	REST_GPR(31, r1)
	
	/* make sure we hard disable here, even if rtl is active, to protect
	 * SRR[01] and SPRG2 -- Cort 
	 */
	mfmsr	r0		/* Get current interrupt state */
	rldicl	r0,r0,48,1
	rldicl	r0,r0,16,0      /* clear MSR_EE */
	mtmsrd	r0		/* Update machine state */
#ifdef CONFIG_PPC_ISERIES
	ld	r0,_MSR(r1)
	andi.	r3,r0,MSR_EE
	beq+	1f

	CHECKANYINT(r4,r3)
	bne-	irq_recheck
	
1:	
#endif
	stdcx.	r0,0,r1		/* to clear the reservation */

	/* if returning to user mode, save kernel SP */
	ld	r0,_MSR(r1)
	andi.	r0,r0,MSR_PR
	beq+	1f
	addi	r0,r1,INT_FRAME_SIZE	/* size of frame */
	mfspr	r4,SPRG3		/* current task's PACA */
	ld	r2,PACACURRENT(r4)	/* Get 'current' */
	std	r0,THREAD+KSP(r2)	/* save kernel stack pointer */
	std	r1,PACAKSAVE(r4)	/* save exception stack pointer */
1:
	ld	r0,_MSR(r1)
	mtspr	SRR1,r0
	ld	r2,_CCR(r1)
	mtcrf	0xFF,r2
	ld	r2,_NIP(r1)
	mtspr	SRR0,r2
	ld	r0,GPR0(r1)
	ld	r2,GPR2(r1)
	ld	r3,GPR3(r1)
	ld	r4,GPR4(r1)
	ld	r1,GPR1(r1)

        /* sync required to force memory operations on this processor */
        /* to complete before the current thread gives up control.    */
	SYNC
	rfid

#ifdef CONFIG_PPC_ISERIES
/*
 * Fake an interrupt from kernel mode.
 * This is used when enable_irq loses an interrupt.
 * We only fill in the stack frame minimally.
 */
_GLOBAL(fake_interrupt)
	mflr	r0
	std	r0,16(r1)
	stdu	r1,-(INT_FRAME_SIZE)(r1)
	std	r0,_NIP(r1)
	std	r0,_LINK(r1)
	mfmsr	r3
	std	r3,_MSR(r1)
	li	r0,0x0fac
	std	r0,TRAP(r1)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	li	r4,1
	bl	.do_IRQ
	addi	r1,r1,INT_FRAME_SIZE
	ld	r0,16(r1)
	mtlr	r0
	blr

/*
 * Fake a decrementer from kernel mode.
 * This is used when the decrementer pops in
 * the hypervisor.  We only fill in the stack
 * frame minimally
 */
_GLOBAL(fake_decrementer)
	mflr	r0
	std	r0,16(r1)
	stdu	r1,-(INT_FRAME_SIZE)(r1)
	std	r0,_NIP(r1)
	std	r0,_LINK(r1)
	mfmsr	r3
	std	r3,_MSR(r1)
	li	r0,0x0fac
	std	r0,TRAP(r1)
	addi	r3,r1,STACK_FRAME_OVERHEAD
	bl	.timer_interrupt
	addi	r1,r1,INT_FRAME_SIZE
	ld	r0,16(r1)
	mtlr	r0
	blr
#endif
/*
 * On CHRP, the Run-Time Abstraction Services (RTAS) have to be
 * called with the MMU off.
 *
 * In addition, we need to be in 32b mode, at least for now.
 * 
 * Note: r3 is an input parameter to rtas, so don't trash it...
 */
_GLOBAL(enter_rtas)
	mflr	r0
	std	r0,16(r1)
        stdu	r1,-INT_FRAME_SIZE(r1)	// Save SP and create stack space

	/* Because RTAS is running in 32b mode, it clobbers the high order half
	 * of all registers that it saves.  We therefore save those registers
	 * RTAS might touch to the stack.  (r0, r3-r13 are caller saved)
   	 */
	SAVE_GPR(2, r1)         // Save the TOC
	SAVE_8GPRS(14, r1)      // Save the non-volatiles
	SAVE_10GPRS(22, r1)     // ditto

	mfcr	r4
	std	r4,_CCR(r1)
	mfctr	r5
	std	r5,_CTR(r1)
	mfspr	r6,XER
	std	r6,_XER(r1)
	mfdar	r7
	std	r7,_DAR(r1)
	mfdsisr	r8
	std	r8,_DSISR(r1)
	mfsrr0	r9
	std	r9,_DSISR(r1)

	/* Unfortunatly, the stack pointer and the MSR are also clobbered, so they
         * are saved in the PACA (SPRG3) which allows us to restore our original
         * state after RTAS returns.
         */
	mfspr	r13,SPRG3	 /* Get PACA */
	std	r1,PACAR1(r13)
	mfmsr	r6
        std	r6,PACASAVEDMSR(r13)

	/* Setup our real return addr */	
	SET_REG_TO_LABEL(r4,.rtas_return_loc)
	SET_REG_TO_CONST(r9,KERNELBASE)
	sub	r4,r4,r9
       	mtlr	r4

	li	r0,0
	ori	r0,r0,MSR_EE|MSR_SE|MSR_BE
	andc	r0,r6,r0
	
        li      r9,1
        rldicr  r9,r9,MSR_SF_LG,(63-MSR_SF_LG)
	ori	r9,r9,MSR_IR|MSR_DR|MSR_FE0|MSR_FE1|MSR_FP
	andc	r6,r0,r9
	sync			/* disable interrupts so SRR0/1 */
	mtmsrd	r0		/* don't get trashed */

	SET_REG_TO_LABEL(r4,rtas)
	ld	r5,RTASENTRY(r4)		/* get the rtas->entry value */
	ld	r4,RTASBASE(r4)			/* get the rtas->base value */
	
	mtspr	SRR0,r5
	mtspr	SRR1,r6
	rfid

_STATIC(rtas_return_loc)
	/* relocation is off at this point */
	mfspr	r4,SPRG3	        /* Get PACA */
	SET_REG_TO_CONST(r5, KERNELBASE)
        sub     r4,r4,r5                /* RELOC the PACA base pointer */
        
        ld	r1,PACAR1(r4)           /* Restore our SP */
	LOADADDR(r3,.rtas_restore_regs)
        ld	r4,PACASAVEDMSR(r4)     /* Restore our MSR */

	mtspr	SRR0,r3
	mtspr	SRR1,r4
	rfid

_STATIC(rtas_restore_regs)
	/* relocation is on at this point */
	REST_GPR(2, r1)         // Restore the TOC
	REST_8GPRS(14, r1)      // Restore the non-volatiles
	REST_10GPRS(22, r1)     // ditto

	ld	r4,_CCR(r1)
	mtcr	r4

	ld	r5,_CTR(r1)
	mtctr	r5

	ld	r6,_XER(r1)
	mtspr	XER,r6

	ld	r7,_DAR(r1)
	mtdar	r7

	ld	r8,_DSISR(r1)
	mtdsisr	r8

	ld	r9,_DSISR(r1)
	mtsrr0	r9

        addi	r1,r1,INT_FRAME_SIZE
	ld	r0,16(r1)	/* get return address */

	mtlr    r0
        blr			/* return to caller */


_GLOBAL(enter_prom)
	mflr	r0
	std	r0,16(r1)
        stdu	r1,-INT_FRAME_SIZE(r1)	// Save SP and create stack space

	/* Because PROM is running in 32b mode, it clobbers the high order half
	 * of all registers that it saves.  We therefore save those registers
	 * PROM might touch to the stack.  (r0, r3-r13 are caller saved)
   	 */
	SAVE_8GPRS(2, r1)       // Save the TOC & incoming param(s)
	SAVE_8GPRS(14, r1)      // Save the non-volatiles
	SAVE_10GPRS(22, r1)     // ditto

	mfcr	r4
	std	r4,_CCR(r1)
	mfctr	r5
	std	r5,_CTR(r1)
	mfspr	r6,XER
	std	r6,_XER(r1)
	mfdar	r7
	std	r7,_DAR(r1)
	mfdsisr	r8
	std	r8,_DSISR(r1)
	mfsrr0	r9
	std	r9,_DSISR(r1)
	mfmsr	r10
	std	r10,_MSR(r1)

	/* Unfortunatly, the stack pointer is also clobbered, so it is saved
	 * in the SPRG2 which allows us to restore our original state after
	 * PROM returns.
         */
	mtspr	SPRG2,r1

        /* put a relocation offset into r3 */
        bl      .reloc_offset
	LOADADDR(r12,prom)
	sub	r12,r12,r3
	ld	r12,PROMENTRY(r12)		/* get the prom->entry value */
	mtlr	r12

        mfmsr   r11                      /* grab the current MSR */
        li      r12,1
        rldicr  r12,r12,MSR_SF_LG,(63-MSR_SF_LG)
        andc    r11,r11,r12
        li      r12,1
        rldicr  r12,r12,MSR_ISF_LG,(63-MSR_ISF_LG)
        andc    r11,r11,r12
        mtmsrd  r11
        SYNC

	REST_8GPRS(2, r1)       // Restore the TOC & incoming param(s)
	REST_8GPRS(14, r1)      // Restore the non-volatiles
	REST_10GPRS(22, r1)     // ditto
	blrl

	mfspr	r1,SPRG2
	ld	r6,_MSR(r1)
	mtmsrd	r6
        SYNC

	REST_GPR(2, r1)         // Restore the TOC
	REST_8GPRS(14, r1)      // Restore the non-volatiles
	REST_10GPRS(22, r1)     // ditto

	ld	r4,_CCR(r1)
	mtcr	r4

	ld	r5,_CTR(r1)
	mtctr	r5

	ld	r6,_XER(r1)
	mtspr	XER,r6

	ld	r7,_DAR(r1)
	mtdar	r7

	ld	r8,_DSISR(r1)
	mtdsisr	r8

	ld	r9,_DSISR(r1)
	mtsrr0	r9

        addi	r1,r1,INT_FRAME_SIZE
	ld	r0,16(r1)	/* get return address */

	mtlr    r0
        blr			/* return to caller */

