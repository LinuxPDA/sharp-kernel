/*
 * arch/mips/kernel/gdb-low.S
 *
 * gdb-low.S contains the low-level trap handler for the GDB stub.
 *
 * Copyright (C) 1995 Andreas Busse
 *
 * $Id: gdb-low.S,v 1.3 2001/04/20 16:34:45 mated Exp $
 *
 *  Nov 16, 2000.
 *  Modifications to support kernel FPU emulator added.
 *  Change to allow "pass-through" of traps coming from user mode.
 *
 *  Kevin D. Kissell, kevink@mips.org and Carsten Langgaard, carstenl@mips.com
 *  Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 *************************************************************************/

#include <linux/config.h>
#include <linux/sys.h>

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/offset.h>
#include <asm/gdb-stub.h>

/*
 * The low level trap handler
 */
		.align 	5
		NESTED(trap_low, GDB_FR_SIZE, sp)
 		.set	noat
		.set 	noreorder

		mfc0	k0,CP0_STATUS
		sll	k0,27     		/* extract K/U bit */
		bgez	k0,1f
		move	k1,sp

		/*
		 * Trap from user mode - invoke user exception handler
		 * set up in gdb-stub.c
		 */
		mfc0	k1,CP0_CAUSE
		andi	k1,k1,0x7c
		la	k0,kgdb_user_exception_handler
		addu	k0,k0,k1
		lw	k0,0(k0)
		nop
		jr	k0
		nop
		

		/* Dead code, kept around in case needed */
		/*
		 * Called from user mode, new stack
		 */
		lui	k1,%hi(kernelsp)
		lw	k1,%lo(kernelsp)(k1)
1:
		move	k0,sp
		subu	sp,k1,GDB_FR_SIZE
		sw	k0,GDB_FR_REG29(sp)
		sw	v0,GDB_FR_REG2(sp)

/*
 * First save the CP0 and special registers
 */

		mfc0	v0,CP0_STATUS
		sw	v0,GDB_FR_STATUS(sp)
		mfc0	v0,CP0_CAUSE
		sw	v0,GDB_FR_CAUSE(sp)
		mfc0	v0,CP0_EPC
		sw	v0,GDB_FR_EPC(sp)
		mfc0	v0,CP0_BADVADDR
		sw	v0,GDB_FR_BADVADDR(sp)
		mfhi	v0
		sw	v0,GDB_FR_HI(sp)
		mflo	v0
		sw	v0,GDB_FR_LO(sp)

/*
 * Now the integer registers
 */

		sw	zero,GDB_FR_REG0(sp)		/* I know... */
		sw	$1,GDB_FR_REG1(sp)
		/* v0 already saved */
		sw	v1,GDB_FR_REG3(sp)
		sw	a0,GDB_FR_REG4(sp)
		sw	a1,GDB_FR_REG5(sp)
		sw	a2,GDB_FR_REG6(sp)
		sw	a3,GDB_FR_REG7(sp)
		sw	t0,GDB_FR_REG8(sp)
		sw	t1,GDB_FR_REG9(sp)
		sw	t2,GDB_FR_REG10(sp)
		sw	t3,GDB_FR_REG11(sp)
		sw	t4,GDB_FR_REG12(sp)
		sw	t5,GDB_FR_REG13(sp)
		sw	t6,GDB_FR_REG14(sp)
		sw	t7,GDB_FR_REG15(sp)
		sw	s0,GDB_FR_REG16(sp)
		sw	s1,GDB_FR_REG17(sp)
		sw	s2,GDB_FR_REG18(sp)
		sw	s3,GDB_FR_REG19(sp)
		sw	s4,GDB_FR_REG20(sp)
		sw	s5,GDB_FR_REG21(sp)
		sw	s6,GDB_FR_REG22(sp)
		sw	s7,GDB_FR_REG23(sp)
		sw	t8,GDB_FR_REG24(sp)
		sw	t9,GDB_FR_REG25(sp)
		sw	k0,GDB_FR_REG26(sp)
		sw	k1,GDB_FR_REG27(sp)
		sw	gp,GDB_FR_REG28(sp)
		/* sp already saved */
		sw	fp,GDB_FR_REG30(sp)
		sw	ra,GDB_FR_REG31(sp)

		CLI				/* disable interrupts */
		
/*
 * Followed by the floating point registers
 */
#ifdef CONFIG_MIPS_FPU_EMULATOR

/*
 * Unfortunately, we cannot count on Status.CU1 to tell us if
 * the FP Emulator is in use.  A software flag must be created
 * and made available.  Until then, we must assume that the FPU
 * emulator is "live".
 */
		.set	at
		lw	t0,cpuoptions
		andi	t0,t0,0x10;
		beq	t0,$0,10f
		nop
#endif
		mfc0	v0,CP0_STATUS		/* FPU enabled? */
		srl	v0,v0,16
		andi	v0,v0,(ST0_CU1 >> 16)

		beqz	v0,2f			/* disabled, skip */
		nop

#ifdef CONFIG_MIPS_FPU_EMULATOR
		b	11f;
10:
	/* Here we cheat a bit, "knowing" how the FP registers are stored */
	/* and assuming that $28 is an uncorrupted current task pointer.  */
		addiu	t0,$28,THREAD_FPU
	/* GDB 4.x for MIPS assumes "FR=0" FPU model, "even" FP regs only */
		.set	noat
		lw	t1,0(t0)
		sw	t1,GDB_FR_FPR0(sp)
		lw	t1,4(t0)
		sw	t1,GDB_FR_FPR1(sp)
		lw	t1,16(t0)
		sw	t1,GDB_FR_FPR2(sp)
		lw	t1,20(t0)
		sw	t1,GDB_FR_FPR3(sp)
		lw	t1,32(t0)
		sw	t1,GDB_FR_FPR4(sp)
		lw	t1,36(t0)
		sw	t1,GDB_FR_FPR5(sp)
		lw	t1,48(t0)
		sw	t1,GDB_FR_FPR6(sp)
		lw	t1,52(t0)
		sw	t1,GDB_FR_FPR7(sp)
		lw	t1,64(t0)
		sw	t1,GDB_FR_FPR8(sp)
		lw	t1,68(t0)
		sw	t1,GDB_FR_FPR9(sp)
		lw	t1,80(t0)
		sw	t1,GDB_FR_FPR10(sp)
		lw	t1,84(t0)
		sw	t1,GDB_FR_FPR11(sp)
		lw	t1,96(t0)
		sw	t1,GDB_FR_FPR12(sp)
		lw	t1,100(t0)
		sw	t1,GDB_FR_FPR13(sp)
		lw	t1,112(t0)
		sw	t1,GDB_FR_FPR14(sp)
		lw	t1,116(t0)
		sw	t1,GDB_FR_FPR15(sp)
		lw	t1,128(t0)
		sw	t1,GDB_FR_FPR16(sp)
		lw	t1,132(t0)
		sw	t1,GDB_FR_FPR17(sp)
		lw	t1,144(t0)
		sw	t1,GDB_FR_FPR18(sp)
		lw	t1,148(t0)
		sw	t1,GDB_FR_FPR19(sp)
		lw	t1,160(t0)
		sw	t1,GDB_FR_FPR20(sp)
		lw	t1,164(t0)
		sw	t1,GDB_FR_FPR21(sp)
		lw	t1,176(t0)
		sw	t1,GDB_FR_FPR22(sp)
		lw	t1,180(t0)
		sw	t1,GDB_FR_FPR23(sp)
		lw	t1,192(t0)
		sw	t1,GDB_FR_FPR24(sp)
		lw	t1,196(t0)
		sw	t1,GDB_FR_FPR25(sp)
		lw	t1,208(t0)
		sw	t1,GDB_FR_FPR26(sp)
		lw	t1,212(t0)
		sw	t1,GDB_FR_FPR27(sp)
		lw	t1,224(t0)
		sw	t1,GDB_FR_FPR28(sp)
		lw	t1,228(t0)
		sw	t1,GDB_FR_FPR29(sp)
		lw	t1,240(t0)
		sw	t1,GDB_FR_FPR30(sp)
		lw	t1,244(t0)
		sw	t1,GDB_FR_FPR31(sp)

		/* OK, this is cheating... */
		lw	t1,256(t0)
		sw	t1,GDB_FR_FSR(sp)
		lw	t1,260(t0)
		sw	t1,GDB_FR_FIR(sp)
		b 2f
		nop
11:
#endif
		swc1	$0,GDB_FR_FPR0(sp)
		swc1	$1,GDB_FR_FPR1(sp)
		swc1	$2,GDB_FR_FPR2(sp)
		swc1	$3,GDB_FR_FPR3(sp)
		swc1	$4,GDB_FR_FPR4(sp)
		swc1	$5,GDB_FR_FPR5(sp)
		swc1	$6,GDB_FR_FPR6(sp)
		swc1	$7,GDB_FR_FPR7(sp)
		swc1	$8,GDB_FR_FPR8(sp)
		swc1	$9,GDB_FR_FPR9(sp)
		swc1	$10,GDB_FR_FPR10(sp)
		swc1	$11,GDB_FR_FPR11(sp)
		swc1	$12,GDB_FR_FPR12(sp)
		swc1	$13,GDB_FR_FPR13(sp)
		swc1	$14,GDB_FR_FPR14(sp)
		swc1	$15,GDB_FR_FPR15(sp)
		swc1	$16,GDB_FR_FPR16(sp)
		swc1	$17,GDB_FR_FPR17(sp)
		swc1	$18,GDB_FR_FPR18(sp)
		swc1	$19,GDB_FR_FPR19(sp)
		swc1	$20,GDB_FR_FPR20(sp)
		swc1	$21,GDB_FR_FPR21(sp)
		swc1	$22,GDB_FR_FPR22(sp)
		swc1	$23,GDB_FR_FPR23(sp)
		swc1	$24,GDB_FR_FPR24(sp)
		swc1	$25,GDB_FR_FPR25(sp)
		swc1	$26,GDB_FR_FPR26(sp)
		swc1	$27,GDB_FR_FPR27(sp)
		swc1	$28,GDB_FR_FPR28(sp)
		swc1	$29,GDB_FR_FPR29(sp)
		swc1	$30,GDB_FR_FPR30(sp)
		swc1	$31,GDB_FR_FPR31(sp)

/*
 * FPU control registers
 */

		mfc1	v0,CP1_STATUS
		sw	v0,GDB_FR_FSR(sp)
		mfc1	v0,CP1_REVISION
		sw	v0,GDB_FR_FIR(sp)

/*
 * Current stack frame ptr
 */

2:
		sw	sp,GDB_FR_FRP(sp)

/*
 * CP0 registers (R4000/R4400 unused registers skipped)
 */

		mfc0	v0,CP0_INDEX
		sw	v0,GDB_FR_CP0_INDEX(sp)
		mfc0	v0,CP0_RANDOM
		sw	v0,GDB_FR_CP0_RANDOM(sp)
		mfc0	v0,CP0_ENTRYLO0
		sw	v0,GDB_FR_CP0_ENTRYLO0(sp)
		mfc0	v0,CP0_ENTRYLO1
		sw	v0,GDB_FR_CP0_ENTRYLO1(sp)
		mfc0	v0,CP0_CONTEXT
		sw	v0,GDB_FR_CP0_CONTEXT(sp)
		mfc0	v0,CP0_PAGEMASK
		sw	v0,GDB_FR_CP0_PAGEMASK(sp)
		mfc0	v0,CP0_WIRED
		sw	v0,GDB_FR_CP0_WIRED(sp)
		mfc0	v0,CP0_ENTRYHI
		sw	v0,GDB_FR_CP0_ENTRYHI(sp)
		mfc0	v0,CP0_PRID
		sw	v0,GDB_FR_CP0_PRID(sp)

		.set	at

/*
 * Continue with the higher level handler
 */

		move	a0,sp

		jal	handle_exception
		 nop

/*
 * Restore all writable registers, in reverse order
 */

		.set	noat

		lw	v0,GDB_FR_CP0_ENTRYHI(sp)
		lw	v1,GDB_FR_CP0_WIRED(sp)
		mtc0	v0,CP0_ENTRYHI
		mtc0	v1,CP0_WIRED
		lw	v0,GDB_FR_CP0_PAGEMASK(sp)
		lw	v1,GDB_FR_CP0_ENTRYLO1(sp)
		mtc0	v0,CP0_PAGEMASK
		mtc0	v1,CP0_ENTRYLO1
		lw	v0,GDB_FR_CP0_ENTRYLO0(sp)
		lw	v1,GDB_FR_CP0_INDEX(sp)
		mtc0	v0,CP0_ENTRYLO0
		lw	v0,GDB_FR_CP0_CONTEXT(sp)
		mtc0	v1,CP0_INDEX
		mtc0	v0,CP0_CONTEXT
		

/*
 * Next, the floating point registers
 */
#ifdef CONFIG_MIPS_FPU_EMULATOR
		.set	at
		lw	t0,cpuoptions
		andi	t0,t0,0x10
		beq	t0,$0,20f
		nop
#endif
		mfc0	v0,CP0_STATUS	/* check if the FPU is enabled */
		srl	v0,v0,16
		andi	v0,v0,(ST0_CU1 >> 16)

		beqz	v0,3f			/* disabled, skip */
		nop
#ifdef CONFIG_MIPS_FPU_EMULATOR
		b	22f
20:
		addiu	t0,$28,THREAD_FPU
		.set	noat
		lw	t1,GDB_FR_FPR31(sp)
		sw	t1,244(t0)
		lw	t1,GDB_FR_FPR30(sp)
		sw	t1,240(t0)
		lw	t1,GDB_FR_FPR29(sp)
		sw	t1,228(t0)
		lw	t1,GDB_FR_FPR28(sp)
		sw	t1,224(t0)
		lw	t1,GDB_FR_FPR27(sp)
		sw	t1,212(t0)
		lw	t1,GDB_FR_FPR26(sp)
		sw	t1,208(t0)
		lw	t1,GDB_FR_FPR25(sp)
		sw	t1,196(t0)
		lw	t1,GDB_FR_FPR24(sp)
		sw	t1,192(t0)
		lw	t1,GDB_FR_FPR23(sp)
		sw	t1,180(t0)
		lw	t1,GDB_FR_FPR22(sp)
		sw	t1,176(t0)
		lw	t1,GDB_FR_FPR21(sp)
		sw	t1,164(t0)
		lw	t1,GDB_FR_FPR20(sp)
		sw	t1,160(t0)
		lw	t1,GDB_FR_FPR19(sp)
		sw	t1,148(t0)
		lw	t1,GDB_FR_FPR18(sp)
		sw	t1,144(t0)
		lw	t1,GDB_FR_FPR17(sp)
		sw	t1,132(t0)
		lw	t1,GDB_FR_FPR16(sp)
		sw	t1,128(t0)
		lw	t1,GDB_FR_FPR15(sp)
		sw	t1,116(t0)
		lw	t1,GDB_FR_FPR14(sp)
		sw	t1,112(t0)
		lw	t1,GDB_FR_FPR13(sp)
		sw	t1,100(t0)
		lw	t1,GDB_FR_FPR12(sp)
		sw	t1,96(t0)
		lw	t1,GDB_FR_FPR11(sp)
		sw	t1,84(t0)
		lw	t1,GDB_FR_FPR10(sp)
		sw	t1,80(t0)
		lw	t1,GDB_FR_FPR9(sp)
		sw	t1,68(t0)
		lw	t1,GDB_FR_FPR8(sp)
		sw	t1,64(t0)
		lw	t1,GDB_FR_FPR7(sp)
		sw	t1,52(t0)
		lw	t1,GDB_FR_FPR6(sp)
		sw	t1,48(t0)
		lw	t1,GDB_FR_FPR5(sp)
		sw	t1,36(t0)
		lw	t1,GDB_FR_FPR4(sp)
		sw	t1,32(t0)
		lw	t1,GDB_FR_FPR3(sp)
		sw	t1,20(t0)
		lw	t1,GDB_FR_FPR2(sp)
		sw	t1,16(t0)
		lw	t1,GDB_FR_FPR1(sp)
		sw	t1,4(t0)
		lw	t1,GDB_FR_FPR0(sp)
		sw	t1,0(t0)
		b	3f
		nop
22:
#endif
		lwc1	$31,GDB_FR_FPR31(sp)
		lwc1	$30,GDB_FR_FPR30(sp)
		lwc1	$29,GDB_FR_FPR29(sp)
		lwc1	$28,GDB_FR_FPR28(sp)
		lwc1	$27,GDB_FR_FPR27(sp)
		lwc1	$26,GDB_FR_FPR26(sp)
		lwc1	$25,GDB_FR_FPR25(sp)
		lwc1	$24,GDB_FR_FPR24(sp)
		lwc1	$23,GDB_FR_FPR23(sp)
		lwc1	$22,GDB_FR_FPR22(sp)
		lwc1	$21,GDB_FR_FPR21(sp)
		lwc1	$20,GDB_FR_FPR20(sp)
		lwc1	$19,GDB_FR_FPR19(sp)
		lwc1	$18,GDB_FR_FPR18(sp)
		lwc1	$17,GDB_FR_FPR17(sp)
		lwc1	$16,GDB_FR_FPR16(sp)
		lwc1	$15,GDB_FR_FPR15(sp)
		lwc1	$14,GDB_FR_FPR14(sp)
		lwc1	$13,GDB_FR_FPR13(sp)
		lwc1	$12,GDB_FR_FPR12(sp)
		lwc1	$11,GDB_FR_FPR11(sp)
		lwc1	$10,GDB_FR_FPR10(sp)
		lwc1	$9,GDB_FR_FPR9(sp)
		lwc1	$8,GDB_FR_FPR8(sp)
		lwc1	$7,GDB_FR_FPR7(sp)
		lwc1	$6,GDB_FR_FPR6(sp)
		lwc1	$5,GDB_FR_FPR5(sp)
		lwc1	$4,GDB_FR_FPR4(sp)
		lwc1	$3,GDB_FR_FPR3(sp)
		lwc1	$2,GDB_FR_FPR2(sp)
		lwc1	$1,GDB_FR_FPR1(sp)
		lwc1	$0,GDB_FR_FPR0(sp)

/*
 * Now the CP0 and integer registers
 */

3:
		mfc0	t0,CP0_STATUS
		ori	t0,0x1f
		xori	t0,0x1f
		mtc0	t0,CP0_STATUS

		lw	v0,GDB_FR_STATUS(sp)
		lw	v1,GDB_FR_EPC(sp)
		mtc0	v0,CP0_STATUS
		mtc0	v1,CP0_EPC
		lw	v0,GDB_FR_HI(sp)
		lw	v1,GDB_FR_LO(sp)
		mthi	v0
		mtlo	v0
		lw	ra,GDB_FR_REG31(sp)
		lw	fp,GDB_FR_REG30(sp)
		lw	gp,GDB_FR_REG28(sp)
		lw	k1,GDB_FR_REG27(sp)
		lw	k0,GDB_FR_REG26(sp)
		lw	t9,GDB_FR_REG25(sp)
		lw	t8,GDB_FR_REG24(sp)
		lw	s7,GDB_FR_REG23(sp)
		lw	s6,GDB_FR_REG22(sp)
		lw	s5,GDB_FR_REG21(sp)
		lw	s4,GDB_FR_REG20(sp)
		lw	s3,GDB_FR_REG19(sp)
		lw	s2,GDB_FR_REG18(sp)
		lw	s1,GDB_FR_REG17(sp)
		lw	s0,GDB_FR_REG16(sp)
		lw	t7,GDB_FR_REG15(sp)
		lw	t6,GDB_FR_REG14(sp)
		lw	t5,GDB_FR_REG13(sp)
		lw	t4,GDB_FR_REG12(sp)
		lw	t3,GDB_FR_REG11(sp)
		lw	t2,GDB_FR_REG10(sp)
		lw	t1,GDB_FR_REG9(sp)
		lw	t0,GDB_FR_REG8(sp)
		lw	a3,GDB_FR_REG7(sp)
		lw	a2,GDB_FR_REG6(sp)
		lw	a1,GDB_FR_REG5(sp)
		lw	a0,GDB_FR_REG4(sp)
		lw	v1,GDB_FR_REG3(sp)
		lw	v0,GDB_FR_REG2(sp)
		lw	$1,GDB_FR_REG1(sp)
		lw	sp,GDB_FR_REG29(sp)		/* Deallocate stack */

		.set	mips3
		eret
		.set	mips0
		.set	at
		.set	reorder
		END(trap_low)
