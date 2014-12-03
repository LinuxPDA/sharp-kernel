/* $Id: entry.S,v 1.4 2001/04/24 16:52:10 randy Exp $
 *
 *  linux/arch/sh/entry.S
 *
 *  Copyright (C) 1999, 2000  Niibe Yutaka
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/sys.h>
#include <linux/linkage.h>
#include <linux/config.h>

/*
 * Define this to turn on compatibility with the previous
 * system call ABI.  This feature is not properly maintained.
 */
#undef COMPAT_OLD_SYSCALL_ABI

! NOTE:
! GNU as (as of 2.9.1) changes bf/s into bt/s and bra, when the address
! to be jumped is too far, but it causes illegal slot exception.

/*	
 * entry.S contains the system-call and fault low-level handling routines.
 * This also contains the timer-interrupt handler, as well as all interrupts
 * and faults that can result in a task-switch.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call.
 *
 * NOTE: This code uses a convention that instructions in the delay slot
 * of a transfer-control instruction are indented by an extra space, thus:
 *
 *    jmp	@$k0	    ! control-transfer instruction
 *     ldc	$k1, $ssr   ! delay slot
 *
 * Stack layout in 'ret_from_syscall':
 * 	ptrace needs to have all regs on the stack.
 *	if the order here is changed, it needs to be
 *	updated in ptrace.c and ptrace.h
 *
 *	$r0
 *      ...
 *	$r15 = stack pointer
 *	$spc
 *	$pr
 *	$ssr
 *	$gbr
 *	$mach
 *	$macl
 *	syscall #
 *
 */

/*
 * These are offsets into the task-struct.
 */
flags		=  4
sigpending	=  8
need_resched	= 20
tsk_ptrace	= 24

PT_TRACESYS  = 0x00000002
PF_USEDFPU   = 0x00100000

ENOSYS = 38
EINVAL = 22


TRA     = 0xffffffd0
EXPEVT  = 0xffffffd4
INTEVT  = 0xffffffd8
MMU_TEA = 0xfffffffc

/* Offsets to the stack */
R0         =  0		/* Return value. New ABI also arg4 */
R1         =  4     	/* New ABI: arg5 */
R2         =  8     	/* New ABI: arg6 */
R3         =  12     	/* New ABI: syscall_nr */
R4         =  16     	/* New ABI: arg0 */
R5         =  20     	/* New ABI: arg1 */
R6         =  24     	/* New ABI: arg2 */
R7         =  28     	/* New ABI: arg3 */
SP	   =  (15*4)
SR	   =  (16*4+8)
SYSCALL_NR =  (16*4+6*4)


#define k0	r0
#define k1	r1
#define k2	r2
#define k3	r3
#define k4	r4

#define current		r7
#define g_imask		r6
#define k_current	r7
#define k_g_imask	r6
#define k_ex_code	r2

/*
 * Kernel mode register usage:
 *	k0	scratch
 *	k1	scratch
 *	k2	scratch (Exception code)
 *	k3	scratch (Return address)
 *	k4	scratch
 *	k5	reserved
 *	k6	Global Interrupt Mask (0--15 << 4)
 *	k7	CURRENT (pointer to current task)
 */

#define STI()				\
	mov.l	_INV_IMASK, $r11;	\
	stc	$sr, $r10;		\
	and	$r11, $r10;		\
!FIXME	stc	$k_g_imask, $r11;	\
	or	$r11, $r10;		\
	ldc	$r10, $sr

call_dpf:
	rts
        nop
	 
	.align 2
_INV_IMASK:
	.long	0xffffff0f	! ~(IMASK)

#if defined(CONFIG_DEBUG_KERNEL_WITH_GDB_STUB) || defined(CONFIG_SH_STANDARD_BIOS)
	.align	2
debug_kernel:
	mov.l	@$r15+, $r0
	mov.l	@$r15+, $r1
	mov.l	@$r15+, $r2
	mov.l	@$r15+, $r3
	mov.l	@$r15+, $r4
	mov.l	@$r15+, $r5
	mov.l	@$r15+, $r6
	mov.l	@$r15+, $r7
	stc	$sr, $r8
	mov.l	1f, $r9
	or	$r9, $r8
	ldc	$r8, $sr
	mov.l	@$r15+, $r8
	mov.l	@$r15+, $r9
	mov.l	@$r15+, $r10
	mov.l	@$r15+, $r11
	mov.l	@$r15+, $r12
	mov.l	@$r15+, $r13
	mov.l	@$r15+, $r14
	mov.l	@$r15+, $k0
	ldc.l	@$r15+, $spc
	lds.l	@$r15+, $pr
	mov.l	@$r15+, $k1
	ldc.l	@$r15+, $gbr
	lds.l	@$r15+, $mach
	lds.l	@$r15+, $macl
	mov	$k0, $r15

	mov.l	2f, $k0
	jmp	@$k0
	 ldc	$k1, $ssr
	.align	2
1:	.long	0x300000f0
2:	.long	CONFIG_GDB_STUB_VBR + 0x100
#endif

	.align	2
debug_trap:	
#if defined(CONFIG_DEBUG_KERNEL_WITH_GDB_STUB) || defined(CONFIG_SH_STANDARD_BIOS)
	mov	#SR, $r0
	mov.l	@($r0,$r15), $r0
	shll	$r0
	shll	$r0		
	bt/s	debug_kernel
#endif
	 mov.l	@$r15, $r0
	mov.l	1f, $r8
	jmp	@$r8
	 nop

	.align	2
1:	.long	SYMBOL_NAME(break_point_trap_software)

	.align	2
error:	

!FIXME	STI()
	mov.l	1f, $r0
	jmp	@$r0
	 nop
	.align	2
1:	.long	SYMBOL_NAME(do_exception_error)


ENTRY(ret_from_fork)
	bra	SYMBOL_NAME(ret_from_syscall)
	 add	#4, $r15

/*
 * New syscall interface:
 *
 *	Syscall #: R3
 *	Arguments #0 to #3: R4--R7
 *	Arguments #4 to #6: R0, R1, R2
 *	TRA: (number of arguments + 0x10) x 4
 *
 * This code also handles delegating other traps to the BIOS/gdb stub
 * according to:
 *
 * Trap number
 * (TRA>>2) 	    Purpose
 * -------- 	    -------
 * 0x0-0xf  	    old syscall ABI
 * 0x10-0x1f  	    new syscall ABI
 * 0x20-0xff  	    delegated through debug_trap to BIOS/gdb stub.
 *
 * Note: When we're first called, the TRA value must be shifted
 * right 2 bits in order to get the value that was used as the "trapa"
 * argument.
 */

system_call:
	mov.l	__TRA, $r9
	mov.l	@$r9, $r8

	mov	#0x20, $r9
	extu.b	$r9, $r9
	shll2	$r9
	cmp/hs	$r9, $r8
	bt	debug_trap

	mov	#SYSCALL_NR, $r14
	add	$r15, $r14

	add	#-0x40, $r8
	shlr2	$r8
	shll8	$r8
	shll8	$r8
	mov	$r3, $r10
	or	$r8, $r10
	mov.l	$r10, @$r14
!FIXME	STI()

!FIXME	stc	$k_current, $r11
	mov.l	@(tsk_ptrace,$r11), $r10
	mov	#PT_TRACESYS, $r11
	tst	$r11, $r10
	bt	5f
	mov.l	__syscall_trace, $r11
	jsr	@$r11	    	    
	nop

	mov.l	@(R4,$r15), $r4
	mov.l	@(R5,$r15), $r5
	mov.l	@(R6,$r15), $r6
	mov.l	@(R7,$r15), $r7
	mov.l	@(R3,$r15), $r3
	mov.l	__syscall_ret_trace, $r10
	bra	6f
	 lds	$r10, $pr

5:	mov.l	__syscall_ret, $r10
	lds	$r10, $pr

6:	mov	$r3, $r9
	mov.l	__n_sys, $r10
	cmp/hs	$r10, $r9
	bf	2f
	rts
  	mov	#-ENOSYS, $r0

2:	shll2	$r9
	mov.l	__sct, $r11
	add	$r11, $r9
	mov.l	@$r9, $r11
	jmp	@$r11
	 nop

syscall_ret_trace:
	mov.l	$r0, @(R0,$r15)
	mov.l	__syscall_trace, $r1
	mova	SYMBOL_NAME(ret_from_syscall), $r0
	jmp	@$r1
	 lds	$r0, $pr

	.align	2
__TRA:	.long	TRA
__syscall_trace:
    	.long	SYMBOL_NAME(syscall_trace)
__n_sys:.long	NR_syscalls
__sct:	.long	SYMBOL_NAME(sys_call_table)
__syscall_ret_trace:
	.long	syscall_ret_trace
__syscall_ret:
	.long	SYMBOL_NAME(syscall_ret)

	.align	2
reschedule:
	mova	SYMBOL_NAME(ret_from_syscall), $r0
	mov.l	1f, $r1
	jmp	@$r1
	 lds	$r0, $pr
	.align	2
1:	.long	SYMBOL_NAME(schedule)

ENTRY(ret_from_irq)
	mov	#SR, $r0
	mov.l	@($r0,$r15), $r0
	shll	$r0
	shll	$r0
	bt	restore_all

!FIXME	STI()
	bra	ret_with_reschedule
	 nop

ENTRY(ret_from_exception)
	mov	#SR, $r0
	mov.l	@($r0,$r15), $r0
	shll	$r0
	shll	$r0		
	bt	restore_all	

!FIXME	STI()
	bra	ret_from_syscall
	 nop
	.align	2
__INV_IMASK:
	.long	0xffffff0f	! ~(IMASK)

	.align 2
#ifdef COMPAT_OLD_SYSCALL_ABI
old_abi_syscall_ret:
	add	$r8, $r15
	/* fall through */
#endif
syscall_ret:
	mov.l	$r0, @(R0,$r15)
	/* fall through */

ENTRY(ret_from_syscall)
	mov.l	__irq_stat, $r0
	mov.l	@$r0, $r1
	mov.l	@(4,$r0), $r2
	tst	$r2, $r1
	bt	ret_with_reschedule
handle_softirq:
	mov.l	__do_softirq, $r0
	jsr	@$r0
	 nop
ret_with_reschedule:
!FIXME	stc	$k_current, $r1
	mov.l	@(need_resched,$r1), $r0
	tst	#0xff, $r0
	bf	reschedule
	mov.l	@(sigpending,$r1), $r0
	tst	#0xff, $r0
	bt	restore_all
signal_return:
	mov	$r15, $r4
	mov	#0, $r5
	mov.l	__do_signal, $r1
	mova	restore_all, $r0
	jmp	@$r1
	 lds	$r0, $pr
	.align	2
__do_signal:
	.long	SYMBOL_NAME(do_signal)
__irq_stat:
	.long	SYMBOL_NAME(irq_stat)
__do_softirq:
	.long	SYMBOL_NAME(do_softirq)

	.align 2
restore_all:

	mov.l	@$r15+, $r0
	mov.l	@$r15+, $r1
	mov.l	@$r15+, $r2
	mov.l	@$r15+, $r3
	mov.l	@$r15+, $r4
	mov.l	@$r15+, $r5
	mov.l	@$r15+, $r6
	mov.l	@$r15+, $r7

	stc	$sr, $r8
	mov.l	__blrb_flags, $r9
	or	$r9, $r8
	ldc	$r8, $sr

	mov.l	@$r15+, $r8
	mov.l	@$r15+, $r9
	mov.l	@$r15+, $r10
	mov.l	@$r15+, $r11
	mov.l	@$r15+, $r12
	mov.l	@$r15+, $r13
	mov.l	@$r15+, $r14
	mov.l	@$r15+, $k4
	ldc.l	@$r15+, $spc
	lds.l	@$r15+, $pr
	mov.l	@$r15+, $k3
	ldc.l	@$r15+, $gbr
	lds.l	@$r15+, $mach
	lds.l	@$r15+, $macl
	add	#4, $r15

	mov	$k3, $k2
	mov.l	1f, $k1
	stc	$sr, $k0
	and	$k1, $k0
	mov.l	2f, $k1
	and	$k1, $k2
	or	$k0, $k2

	mov	$k3, $k0
	shlr2	$k0
	and	#0x3c, $k0
	cmp/eq	#0x3c, $k0
	bt/s	7f
	 shll2	$k0
	mov	$g_imask, $k0

7:	or	$k0, $k2
	ldc	$k2, $ssr
	mov	$k4, $r15
	rte
	 nop

	.align	2
__blrb_flags:	.long	0x30000000
#if defined(__SH4__)
__fpu_prepare_fd:
	.long	SYMBOL_NAME(fpu_prepare_fd)
__init_task_flags:
	.long	SYMBOL_NAME(init_task_union)+4
__PF_USEDFPU:
	.long	PF_USEDFPU
#endif
1:	.long	0x00008000	! FD
2:	.long	0xffff7f0f	! ~(IMASK+FD)
3:	.long	0x00080000	! SZ=0, PR=1

! Exception Vector Base
!
!	Should be aligned page boundary.
!
	.balign 	4096,0,4096
ENTRY(vbr_base)
	.long	0
!
	.balign 	256,0,256
general_exception:
	mov.l	1f, $k2
	mov.l	2f, $k3
	bra	handle_exception
	 mov.l	@$k2, $k2
	.align	2
2:	.long	SYMBOL_NAME(ret_from_exception)
1:	.long	EXPEVT
!
!
	.balign 	1024,0,1024
tlb_miss:
	mov.l	1f, $k2
	mov.l	4f, $k3
	bra	handle_exception
	 mov.l	@$k2, $k2
!
	.balign 	512,0,512
interrupt:
	mov.l	2f, $k2
	mov.l	3f, $k3
	bra	handle_exception
	 mov.l	@$k2, $k2

	.align	2
1:	.long	EXPEVT
2:	.long	INTEVT
3:	.long	SYMBOL_NAME(ret_from_irq)
4:	.long	SYMBOL_NAME(ret_from_exception)


handle_exception:
	! Using k0, k1 for scratch registers (r0_bank1, r1_bank),
	! save all registers onto stack.
	!
	stc	$ssr, $k0
	shll	$k0	
	shll	$k0
#if defined(__SH4__)
	bf/s	8f	
	 mov	$r15, $k0
	/* It's a kernel to kernel transition. */
	/* Is the FPU disabled? */
	mov.l	2f, $k1
	stc	$ssr, $k0
	tst	$k1, $k0
	mov.l	4f, $k1
	bf/s	9f	
	 mov	$r15, $k0
	! FPU is enabled, save it
	! /* XXX: Need to save another bank of FPU if all FPU feature is used */
	! /* Currently it's not the case for GCC (only udivsi3_i4, divsi3_i4) */
	sts.l	$fpul,  @-$r15
	sts.l	$fpscr, @-$r15
	mov.l	6f, $k1
	lds	$k1, $fpscr
	mov.l	3f, $k1
	fmov.s	$fr15, @-$r15
	fmov.s	$fr14, @-$r15
	fmov.s	$fr13, @-$r15
	fmov.s	$fr12, @-$r15
	fmov.s	$fr11, @-$r15
	fmov.s	$fr10, @-$r15
	fmov.s	$fr9, @-$r15
	fmov.s	$fr8, @-$r15
	fmov.s	$fr7, @-$r15
	fmov.s	$fr6, @-$r15
	fmov.s	$fr5, @-$r15
	fmov.s	$fr4, @-$r15
	fmov.s	$fr3, @-$r15
	fmov.s	$fr2, @-$r15
	fmov.s	$fr1, @-$r15
	bra 9f
	 fmov.s	$fr0, @-$r15
#else
	mov.l	3f, $k1
	bt/s	9f	
	 mov	$r15, $k0
#endif
8:	/* User space to kernel */
	mov	#0x20, $k1
	shll8	$k1			! $k1 <= 8192 == THREAD_SIZE
	add	$current, $k1
	mov	$k1, $r15		! change to kernel stack
	!
	mov.l	4f, $k1			! let kernel release FPU
9:	! Save the user registers on the stack.
	! At this point, k1 should have been set to the new SR value
  	mov	#-1, $k4
	mov.l	$k4, @-$r15		! syscall_nr (default: -1)
	!
	sts.l	$macl, @-$r15
	sts.l	$mach, @-$r15
	stc.l	$gbr, @-$r15
	stc.l	$ssr, @-$r15
	sts.l	$pr, @-$r15
	stc.l	$spc, @-$r15
	!
	lds	$k3, $pr		! Set the return address to pr
	!
	mov.l	$k0, @-$r15	! save orignal stack
	mov.l	$r14, @-$r15
	mov.l	$r13, @-$r15
	mov.l	$r12, @-$r15
	mov.l	$r11, @-$r15
	mov.l	$r10, @-$r15
	mov.l	$r9, @-$r15
	mov.l	$r8, @-$r15
	!
	stc	$sr, $r8	! Back to normal register bank, and
	or	$k1, $r8	! Block all interrupts, may release FPU
	mov.l	5f, $k1
	and	$k1, $r8	! ...
	ldc	$r8, $sr	! ...changed here.
	!
	mov.l	$r7, @-$r15
	mov.l	$r6, @-$r15
	mov.l	$r5, @-$r15
	mov.l	$r4, @-$r15
	mov.l	$r3, @-$r15
	mov.l	$r2, @-$r15
	mov.l	$r1, @-$r15
	mov.l	$r0, @-$r15
	! Then, dispatch to the handler, according to the exception code.
!FIXME	stc	$k_ex_code, $r8
	shlr2	$r8
	shlr	$r8
	mov.l	1f, $r9
	add	$r8, $r9
	mov.l	@$r9, $r9
	jmp	@$r9
	 nop
	.align	2
1:	.long	SYMBOL_NAME(exception_handling_table)
2:	.long	0x00008000	! FD=1
3:	.long	0x000000f0	! FD=0, IMASK=15
4:	.long	0x000080f0	! FD=1, IMASK=15
5:	.long	0xcfffffff	! RB=0, BL=0
6:	.long	0x00080000	! SZ=0, PR=1

none:
	rts
	 nop

.data
ENTRY(exception_handling_table)
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error	! address_error_load        (filled by trap_init)
	.long	error	! address_error_store       (filled by trap_init)
#if defined(__SH4__)
	.long	SYMBOL_NAME(do_fpu_error)
#else
	.long	error	! fpu_exception
#endif
	.long	error
	.long	system_call			! Unconditional Trap
	.long	error	! reserved_instruction      (filled by trap_init)
	.long	error	! illegal_slot_instruction  (filled by trap_init)
ENTRY(nmi_slot)
	.long	none				! Not implemented yet
ENTRY(user_break_point_trap)
	.long	break_point_trap
ENTRY(interrupt_table)
	! external hardware
	.long	SYMBOL_NAME(do_IRQ)	! 0000
	.long	SYMBOL_NAME(do_IRQ)	! 0001
	.long	SYMBOL_NAME(do_IRQ)	! 0010
	.long	SYMBOL_NAME(do_IRQ)	! 0011
	.long	SYMBOL_NAME(do_IRQ)	! 0100
	.long	SYMBOL_NAME(do_IRQ)	! 0101
	.long	SYMBOL_NAME(do_IRQ)	! 0110
	.long	SYMBOL_NAME(do_IRQ)	! 0111
	.long	SYMBOL_NAME(do_IRQ)	! 1000
	.long	SYMBOL_NAME(do_IRQ)	! 1001
	.long	SYMBOL_NAME(do_IRQ)	! 1010
	.long	SYMBOL_NAME(do_IRQ)	! 1011
	.long	SYMBOL_NAME(do_IRQ)	! 1100
	.long	SYMBOL_NAME(do_IRQ)	! 1101
	.long	SYMBOL_NAME(do_IRQ)	! 1110
	.long	error
	! Internal hardware
	.long	SYMBOL_NAME(do_IRQ)	! TMU0 tuni0
	.long	SYMBOL_NAME(do_IRQ)	! TMU1 tuni1
	.long	SYMBOL_NAME(do_IRQ)	! TMU2 tuni2
	.long	SYMBOL_NAME(do_IRQ)	!      ticpi2
	.long	SYMBOL_NAME(do_IRQ)	! RTC  ati
	.long	SYMBOL_NAME(do_IRQ)	!      pri
	.long	SYMBOL_NAME(do_IRQ)	!      cui
	.long	SYMBOL_NAME(do_IRQ)	! SCI  eri
	.long	SYMBOL_NAME(do_IRQ)	!      rxi
	.long	SYMBOL_NAME(do_IRQ)	!      txi
	.long	SYMBOL_NAME(do_IRQ)	!      tei
	.long	SYMBOL_NAME(do_IRQ)	! WDT  iti
	.long	SYMBOL_NAME(do_IRQ)	! REF  rcmi
	.long	SYMBOL_NAME(do_IRQ)	!      rovi
	.long	SYMBOL_NAME(do_IRQ)
	.long	SYMBOL_NAME(do_IRQ)
#if  defined(CONFIG_CPU_SUBTYPE_SH7707) || defined(CONFIG_CPU_SUBTYPE_SH7709)
	.long	SYMBOL_NAME(do_IRQ)	! 32 IRQ  irq0
	.long	SYMBOL_NAME(do_IRQ)	! 33      irq1
	.long	SYMBOL_NAME(do_IRQ)	! 34      irq2
	.long	SYMBOL_NAME(do_IRQ)	! 35      irq3
	.long	SYMBOL_NAME(do_IRQ)	! 36      irq4
	.long	SYMBOL_NAME(do_IRQ)	! 37      irq5
	.long	SYMBOL_NAME(do_IRQ)	! 38
	.long	SYMBOL_NAME(do_IRQ)	! 39
	.long	SYMBOL_NAME(do_IRQ)	! 40 PINT pint0-7
	.long	SYMBOL_NAME(do_IRQ)	! 41      pint8-15
	.long	SYMBOL_NAME(do_IRQ)	! 42
	.long	SYMBOL_NAME(do_IRQ)	! 43
	.long	SYMBOL_NAME(do_IRQ)	! 44
	.long	SYMBOL_NAME(do_IRQ)	! 45
	.long	SYMBOL_NAME(do_IRQ)	! 46
	.long	SYMBOL_NAME(do_IRQ)	! 47
	.long	SYMBOL_NAME(do_IRQ)	! 48 DMAC dei0
	.long	SYMBOL_NAME(do_IRQ)	! 49      dei1
	.long	SYMBOL_NAME(do_IRQ)	! 50      dei2
	.long	SYMBOL_NAME(do_IRQ)	! 51      dei3
	.long	SYMBOL_NAME(do_IRQ)	! 52 IrDA eri1
	.long	SYMBOL_NAME(do_IRQ)	! 53      rxi1
	.long	SYMBOL_NAME(do_IRQ)	! 54      bri1
	.long	SYMBOL_NAME(do_IRQ)	! 55      txi1
	.long	SYMBOL_NAME(do_IRQ)	! 56 SCIF eri2
	.long	SYMBOL_NAME(do_IRQ)	! 57      rxi2
	.long	SYMBOL_NAME(do_IRQ)	! 58      bri2
	.long	SYMBOL_NAME(do_IRQ)	! 59      txi2
	.long	SYMBOL_NAME(do_IRQ)	! 60 ADC  adi
#if defined(CONFIG_CPU_SUBTYPE_SH7707)
	.long   SYMBOL_NAME(do_IRQ)	! 61 LCDC lcdi
	.long   SYMBOL_NAME(do_IRQ)	! 62 PCC  pcc0i
	.long   SYMBOL_NAME(do_IRQ)	! 63      pcc1i
#endif
#elif defined(__SH4__)
	.long	SYMBOL_NAME(do_IRQ)	! Hitachi UDI
	.long	SYMBOL_NAME(do_IRQ)	! GPIO
	.long	SYMBOL_NAME(do_IRQ)	! DMAC dmte0
	.long	SYMBOL_NAME(do_IRQ)	!      dmte1
	.long	SYMBOL_NAME(do_IRQ)	!      dmte2
	.long	SYMBOL_NAME(do_IRQ)	!      dmte3
	.long	SYMBOL_NAME(do_IRQ)	!      dmae
	.long	SYMBOL_NAME(do_IRQ)
	.long	SYMBOL_NAME(do_IRQ)	! SCIF eri
	.long	SYMBOL_NAME(do_IRQ)	!      rxi
	.long	SYMBOL_NAME(do_IRQ)	!      bri
	.long	SYMBOL_NAME(do_IRQ)	!      txi
	.long	error
	.long	error
	.long	error
	.long	error
	.long	SYMBOL_NAME(do_fpu_state_restore)
	.long	SYMBOL_NAME(do_fpu_state_restore)
#endif

ENTRY(sys_call_table)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 0  -  old "setup()" system call*/
	.long SYMBOL_NAME(sys_exit)
	.long SYMBOL_NAME(sys_fork)
	.long SYMBOL_NAME(sys_read)
	.long SYMBOL_NAME(sys_write)
	.long SYMBOL_NAME(sys_open)		/* 5 */
	.long SYMBOL_NAME(sys_close)
	.long SYMBOL_NAME(sys_waitpid)
	.long SYMBOL_NAME(sys_creat)
	.long SYMBOL_NAME(sys_link)
	.long SYMBOL_NAME(sys_unlink)		/* 10 */
	.long SYMBOL_NAME(sys_execve)
	.long SYMBOL_NAME(sys_chdir)
	.long SYMBOL_NAME(sys_time)
	.long SYMBOL_NAME(sys_mknod)
	.long SYMBOL_NAME(sys_chmod)		/* 15 */
	.long SYMBOL_NAME(sys_lchown16)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old break syscall holder */
	.long SYMBOL_NAME(sys_stat)
	.long SYMBOL_NAME(sys_lseek)
	.long SYMBOL_NAME(sys_getpid)		/* 20 */
	.long SYMBOL_NAME(sys_mount)
	.long SYMBOL_NAME(sys_oldumount)
	.long SYMBOL_NAME(sys_setuid16)
	.long SYMBOL_NAME(sys_getuid16)
	.long SYMBOL_NAME(sys_stime)		/* 25 */
	.long SYMBOL_NAME(sys_ptrace)
	.long SYMBOL_NAME(sys_alarm)
	.long SYMBOL_NAME(sys_fstat)
	.long SYMBOL_NAME(sys_pause)
	.long SYMBOL_NAME(sys_utime)		/* 30 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old stty syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old gtty syscall holder */
	.long SYMBOL_NAME(sys_access)
	.long SYMBOL_NAME(sys_nice)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 35 */		/* old ftime syscall holder */
	.long SYMBOL_NAME(sys_sync)
	.long SYMBOL_NAME(sys_kill)
	.long SYMBOL_NAME(sys_rename)
	.long SYMBOL_NAME(sys_mkdir)
	.long SYMBOL_NAME(sys_rmdir)		/* 40 */
	.long SYMBOL_NAME(sys_dup)
	.long SYMBOL_NAME(sys_pipe)
	.long SYMBOL_NAME(sys_times)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old prof syscall holder */
	.long SYMBOL_NAME(sys_brk)		/* 45 */
	.long SYMBOL_NAME(sys_setgid16)
	.long SYMBOL_NAME(sys_getgid16)
	.long SYMBOL_NAME(sys_signal)
	.long SYMBOL_NAME(sys_geteuid16)
	.long SYMBOL_NAME(sys_getegid16)	/* 50 */
	.long SYMBOL_NAME(sys_acct)
	.long SYMBOL_NAME(sys_umount)		/* recycled never used phys() */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old lock syscall holder */
	.long SYMBOL_NAME(sys_ioctl)
	.long SYMBOL_NAME(sys_fcntl)		/* 55 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old mpx syscall holder */
	.long SYMBOL_NAME(sys_setpgid)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old ulimit syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_olduname */
	.long SYMBOL_NAME(sys_umask)		/* 60 */
	.long SYMBOL_NAME(sys_chroot)
	.long SYMBOL_NAME(sys_ustat)
	.long SYMBOL_NAME(sys_dup2)
	.long SYMBOL_NAME(sys_getppid)
	.long SYMBOL_NAME(sys_getpgrp)		/* 65 */
	.long SYMBOL_NAME(sys_setsid)
	.long SYMBOL_NAME(sys_sigaction)
	.long SYMBOL_NAME(sys_sgetmask)
	.long SYMBOL_NAME(sys_ssetmask)
	.long SYMBOL_NAME(sys_setreuid16)	/* 70 */
	.long SYMBOL_NAME(sys_setregid16)
	.long SYMBOL_NAME(sys_sigsuspend)
	.long SYMBOL_NAME(sys_sigpending)
	.long SYMBOL_NAME(sys_sethostname)
	.long SYMBOL_NAME(sys_setrlimit)	/* 75 */
	.long SYMBOL_NAME(sys_old_getrlimit)
	.long SYMBOL_NAME(sys_getrusage)
	.long SYMBOL_NAME(sys_gettimeofday)
	.long SYMBOL_NAME(sys_settimeofday)
	.long SYMBOL_NAME(sys_getgroups16)	/* 80 */
	.long SYMBOL_NAME(sys_setgroups16)
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_oldselect */
	.long SYMBOL_NAME(sys_symlink)
	.long SYMBOL_NAME(sys_lstat)
	.long SYMBOL_NAME(sys_readlink)		/* 85 */
	.long SYMBOL_NAME(sys_uselib)
	.long SYMBOL_NAME(sys_swapon)
	.long SYMBOL_NAME(sys_reboot)
	.long SYMBOL_NAME(old_readdir)
	.long SYMBOL_NAME(old_mmap)		/* 90 */
	.long SYMBOL_NAME(sys_munmap)
	.long SYMBOL_NAME(sys_truncate)
	.long SYMBOL_NAME(sys_ftruncate)
	.long SYMBOL_NAME(sys_fchmod)
	.long SYMBOL_NAME(sys_fchown16)		/* 95 */
	.long SYMBOL_NAME(sys_getpriority)
	.long SYMBOL_NAME(sys_setpriority)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old profil syscall holder */
	.long SYMBOL_NAME(sys_statfs)
	.long SYMBOL_NAME(sys_fstatfs)		/* 100 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* ioperm */
	.long SYMBOL_NAME(sys_socketcall)
	.long SYMBOL_NAME(sys_syslog)
	.long SYMBOL_NAME(sys_setitimer)
	.long SYMBOL_NAME(sys_getitimer)	/* 105 */
	.long SYMBOL_NAME(sys_newstat)
	.long SYMBOL_NAME(sys_newlstat)
	.long SYMBOL_NAME(sys_newfstat)
	.long SYMBOL_NAME(sys_uname)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 110 */ /* iopl */
	.long SYMBOL_NAME(sys_vhangup)
	.long SYMBOL_NAME(sys_ni_syscall)	/* idle */
	.long SYMBOL_NAME(sys_ni_syscall)	/* vm86old */
	.long SYMBOL_NAME(sys_wait4)
	.long SYMBOL_NAME(sys_swapoff)		/* 115 */
	.long SYMBOL_NAME(sys_sysinfo)
	.long SYMBOL_NAME(sys_ipc)
	.long SYMBOL_NAME(sys_fsync)
	.long SYMBOL_NAME(sys_sigreturn)
	.long SYMBOL_NAME(sys_clone)		/* 120 */
	.long SYMBOL_NAME(sys_setdomainname)
	.long SYMBOL_NAME(sys_newuname)
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_modify_ldt */
	.long SYMBOL_NAME(sys_adjtimex)
	.long SYMBOL_NAME(sys_mprotect)		/* 125 */
	.long SYMBOL_NAME(sys_sigprocmask)
	.long SYMBOL_NAME(sys_create_module)
	.long SYMBOL_NAME(sys_init_module)
	.long SYMBOL_NAME(sys_delete_module)
	.long SYMBOL_NAME(sys_get_kernel_syms)	/* 130 */
	.long SYMBOL_NAME(sys_quotactl)
	.long SYMBOL_NAME(sys_getpgid)
	.long SYMBOL_NAME(sys_fchdir)
	.long SYMBOL_NAME(sys_bdflush)
	.long SYMBOL_NAME(sys_sysfs)		/* 135 */
	.long SYMBOL_NAME(sys_personality)
	.long SYMBOL_NAME(sys_ni_syscall)	/* for afs_syscall */
	.long SYMBOL_NAME(sys_setfsuid16)
	.long SYMBOL_NAME(sys_setfsgid16)
	.long SYMBOL_NAME(sys_llseek)		/* 140 */
	.long SYMBOL_NAME(sys_getdents)
	.long SYMBOL_NAME(sys_select)
	.long SYMBOL_NAME(sys_flock)
	.long SYMBOL_NAME(sys_msync)
	.long SYMBOL_NAME(sys_readv)		/* 145 */
	.long SYMBOL_NAME(sys_writev)
	.long SYMBOL_NAME(sys_getsid)
	.long SYMBOL_NAME(sys_fdatasync)
	.long SYMBOL_NAME(sys_sysctl)
	.long SYMBOL_NAME(sys_mlock)		/* 150 */
	.long SYMBOL_NAME(sys_munlock)
	.long SYMBOL_NAME(sys_mlockall)
	.long SYMBOL_NAME(sys_munlockall)
	.long SYMBOL_NAME(sys_sched_setparam)
	.long SYMBOL_NAME(sys_sched_getparam)   /* 155 */
	.long SYMBOL_NAME(sys_sched_setscheduler)
	.long SYMBOL_NAME(sys_sched_getscheduler)
	.long SYMBOL_NAME(sys_sched_yield)
	.long SYMBOL_NAME(sys_sched_get_priority_max)
	.long SYMBOL_NAME(sys_sched_get_priority_min)  /* 160 */
	.long SYMBOL_NAME(sys_sched_rr_get_interval)
	.long SYMBOL_NAME(sys_nanosleep)
	.long SYMBOL_NAME(sys_mremap)
	.long SYMBOL_NAME(sys_setresuid16)
	.long SYMBOL_NAME(sys_getresuid16)	/* 165 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* vm86 */
	.long SYMBOL_NAME(sys_query_module)
	.long SYMBOL_NAME(sys_poll)
	.long SYMBOL_NAME(sys_nfsservctl)
	.long SYMBOL_NAME(sys_setresgid16)	/* 170 */
	.long SYMBOL_NAME(sys_getresgid16)
	.long SYMBOL_NAME(sys_prctl)
	.long SYMBOL_NAME(sys_rt_sigreturn)
	.long SYMBOL_NAME(sys_rt_sigaction)
	.long SYMBOL_NAME(sys_rt_sigprocmask)	/* 175 */
	.long SYMBOL_NAME(sys_rt_sigpending)
	.long SYMBOL_NAME(sys_rt_sigtimedwait)
	.long SYMBOL_NAME(sys_rt_sigqueueinfo)
	.long SYMBOL_NAME(sys_rt_sigsuspend)
	.long SYMBOL_NAME(sys_pread)		/* 180 */
	.long SYMBOL_NAME(sys_pwrite)
	.long SYMBOL_NAME(sys_chown16)
	.long SYMBOL_NAME(sys_getcwd)
	.long SYMBOL_NAME(sys_capget)
	.long SYMBOL_NAME(sys_capset)           /* 185 */
	.long SYMBOL_NAME(sys_sigaltstack)
	.long SYMBOL_NAME(sys_sendfile)
	.long SYMBOL_NAME(sys_ni_syscall)	/* streams1 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* streams2 */
	.long SYMBOL_NAME(sys_vfork)            /* 190 */
	.long SYMBOL_NAME(sys_getrlimit)
	.long SYMBOL_NAME(sys_mmap2)
	.long SYMBOL_NAME(sys_truncate64)
	.long SYMBOL_NAME(sys_ftruncate64)
	.long SYMBOL_NAME(sys_stat64)		/* 195 */
	.long SYMBOL_NAME(sys_lstat64)
	.long SYMBOL_NAME(sys_fstat64)
	.long SYMBOL_NAME(sys_lchown)
	.long SYMBOL_NAME(sys_getuid)
	.long SYMBOL_NAME(sys_getgid)		/* 200 */
	.long SYMBOL_NAME(sys_geteuid)
	.long SYMBOL_NAME(sys_getegid)
	.long SYMBOL_NAME(sys_setreuid)
	.long SYMBOL_NAME(sys_setregid)
	.long SYMBOL_NAME(sys_getgroups)	/* 205 */
	.long SYMBOL_NAME(sys_setgroups)
	.long SYMBOL_NAME(sys_fchown)
	.long SYMBOL_NAME(sys_setresuid)
	.long SYMBOL_NAME(sys_getresuid)
	.long SYMBOL_NAME(sys_setresgid)	/* 210 */
	.long SYMBOL_NAME(sys_getresgid)
	.long SYMBOL_NAME(sys_chown)
	.long SYMBOL_NAME(sys_setuid)
	.long SYMBOL_NAME(sys_setgid)
	.long SYMBOL_NAME(sys_setfsuid)		/* 215 */
	.long SYMBOL_NAME(sys_setfsgid)
	.long SYMBOL_NAME(sys_pivot_root)
	.long SYMBOL_NAME(sys_mincore)
	.long SYMBOL_NAME(sys_madvise)
	.long SYMBOL_NAME(sys_getdents64)	/* 220 */
	.long SYMBOL_NAME(sys_fcntl64)

	/*
	 * NOTE!! This doesn't have to be exact - we just have
	 * to make sure we have _enough_ of the "sys_ni_syscall"
	 * entries. Don't panic if you notice that this hasn't
	 * been shrunk every time we add a new system call.
	 */
	.rept NR_syscalls-221
		.long SYMBOL_NAME(sys_ni_syscall)
	.endr

/* End of entry.S */
