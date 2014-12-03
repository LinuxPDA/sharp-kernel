/*
 * Minimalist Kernel Debugger - Architecture Dependent Stack Traceback
 *
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) Scott Lurndal (slurn@engr.sgi.com)
 * Copyright (C) Scott Foehner (sfoehner@engr.sgi.com)
 * Copyright (C) Srinivasa Thirumalachar (sprasad@engr.sgi.com)
 *
 * See the file LIA-COPYRIGHT for additional information.
 *
 * Written March 1999 by Scott Lurndal at Silicon Graphics, Inc.
 *
 * Modifications from:
 *      Richard Bass                    1999/07/20
 *              Many bug fixes and enhancements.
 *      Scott Foehner
 *              Port to ia64
 *      Srinivasa Thirumalachar
 *              RSE support for ia64
 *	Masahiro Adegawa                1999/12/01
 *		'sr' command, active flag in 'ps'
 *	Scott Lurndal			1999/12/12
 *		Significantly restructure for linux2.3
 *	Keith Owens			2000/05/23
 *		KDB v1.2
 *
 */

#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/kdb.h>
#include <linux/kdbprivate.h>
#include <linux/ptrace.h>	/* for STACK_FRAME_OVERHEAD */
#include <asm/system.h>
#include "privinst.h"

void systemreset(struct pt_regs *regs)
{
	udbg_printf("Oh no!\n");
	kdb(KDB_REASON_OOPS, 0, (kdb_eframe_t) regs);
	for (;;);
}

/* Copy a block of memory using kdba_getword().
 * This is not efficient.
 */
static void kdba_getmem(unsigned long addr, void *p, int size)
{
	unsigned char *dst = (unsigned char *)p;
	while (size > 0) {
		*dst++ = kdba_getword(addr++, 1);
		size--;
	}
}


/*
 * kdba_bt_stack_ppc
 *
 *	kdba_bt_stack with ppc specific parameters.
 *	Specification as kdba_bt_stack plus :-
 *
 * Inputs:
 *	As kba_bt_stack plus
 *	regs_esp If 1 get esp from the registers (exception frame), if 0
 *		 get esp from kdba_getregcontents.
 */

static int
kdba_bt_stack_ppc(struct pt_regs *regs, kdb_machreg_t *addr, int argcount,
		   struct task_struct *p, int regs_esp)
{
	kdb_machreg_t	esp,eip,ebp;
	kdb_symtab_t	symtab, *sym;
	kdbtbtable_t	tbtab;
	/* declare these as raw ptrs so we don't get func descriptors */
	extern void *ret_from_except, *ret_from_syscall_1, *do_bottom_half_ret;

	/*
	 * The caller may have supplied an address at which the
	 * stack traceback operation should begin.  This address
	 * is assumed by this code to point to a return-address
	 * on the stack to be traced back.
	 *
	 * The end result of this will make it appear as if a function
	 * entitled '<unknown>' was called from the function which
	 * contains return-address.
	 */
	if (addr) {
		eip = 0;
		esp = *addr;
		ebp=0;
	} else {
		ebp=regs->link;
		eip = regs->nip;
		if (regs_esp)
			esp = regs->gpr[1];
		else
			kdba_getregcontents("esp", regs, &esp);
	}

	kdb_printf("          SP                 PC         Function(args)\n");

	/* (Ref: 64-bit PowerPC ELF ABI Spplement; Ian Lance Taylor, Zembu Labs).
	 A PPC stack frame looks like this:

	 High Address
	 Back Chain
	 FP reg save area
	 GP reg save area
	 Local var space
	 Parameter save area		(SP+48)
	 TOC save area		(SP+40)
	 link editor doubleword	(SP+32)
	 compiler doubleword		(SP+24)
	 LR save			(SP+16)
	 CR save			(SP+8)
	 Back Chain			(SP+0)

	 Note that the LR (ret addr) may not be saved in the *current* frame if
	 no functions have been called from the current function.
	 */

	/*
	 * Run through the activation records and print them.
	 */
	while (1) {
		kdbnearsym(eip, &symtab);
		kdb_printf("0x%016lx  0x%016lx  ", esp, eip);
		kdba_find_tb_table(eip, &tbtab);
		sym = symtab.sym_name ? &symtab : &tbtab.symtab; /* use fake symtab if necessary */
		if (sym->sym_name) {
			kdb_printf("%s", sym->sym_name);
			if (eip - sym->sym_start > 0)
				kdb_printf("+0x%lx", eip - sym->sym_start);
		}
		kdb_printf("\n");
		if (eip == (kdb_machreg_t)ret_from_except ||
		    eip == (kdb_machreg_t)ret_from_syscall_1 ||
		    eip == (kdb_machreg_t)do_bottom_half_ret) {
			/* pull exception regs from the stack */
			struct pt_regs eregs;
			kdba_getmem(esp+STACK_FRAME_OVERHEAD, &eregs, sizeof(eregs));
			kdb_printf("  [exception: %lx regs 0x%lx]\n", eregs.trap, esp+STACK_FRAME_OVERHEAD);
			esp = eregs.gpr[1];
			eip = eregs.nip;
			if (!esp)
				break;
		} else {
			esp = kdba_getword(esp, 8);
			if (!esp)
				break;
			eip = kdba_getword(esp+16, 8);	/* saved lr */
		}
	}

	return 0;
}

/*
 * kdba_bt_stack
 *
 *	This function implements the 'bt' command.  Print a stack
 *	traceback.
 *
 *	bt [<address-expression>]   (addr-exp is for alternate stacks)
 *	btp <pid>		     (Kernel stack for <pid>)
 *
 * 	address expression refers to a return address on the stack.  It
 *	may be preceeded by a frame pointer.
 *
 * Inputs:
 *	regs	registers at time kdb was entered.
 *	addr	Pointer to Address provided to 'bt' command, if any.
 *	argcount
 *	p	Pointer to task for 'btp' command.
 * Outputs:
 *	None.
 * Returns:
 *	zero for success, a kdb diagnostic if error
 * Locking:
 *	none.
 * Remarks:
 *	mds comes in handy when examining the stack to do a manual
 *	traceback.
 */

int
kdba_bt_stack(struct pt_regs *regs, kdb_machreg_t *addr, int argcount,
	      struct task_struct *p)
{
	return(kdba_bt_stack_ppc(regs, addr, argcount, p, 0));
}

int
kdba_bt_process(struct task_struct *p, int argcount)
{
	struct pt_regs taskregs;

	memset(&taskregs, 0, sizeof(taskregs));
	if (p->thread.regs == NULL)
	{
		struct pt_regs regs;
		asm volatile ("std	0,0(%0)\n\
                               std	1,8(%0)\n\
                               std	2,16(%0)\n\
                               std	3,24(%0)\n\
                               std	4,32(%0)\n\
                               std	5,40(%0)\n\
                               std	6,48(%0)\n\
                               std	7,56(%0)\n\
                               std	8,64(%0)\n\
                               std	9,72(%0)\n\
                               std	10,80(%0)\n\
                               std	11,88(%0)\n\
                               std	12,96(%0)\n\
                               std	13,104(%0)\n\
                               std	14,112(%0)\n\
                               std	15,120(%0)\n\
                               std	16,128(%0)\n\
                               std	17,136(%0)\n\
                               std	18,144(%0)\n\
                               std	19,152(%0)\n\
                               std	20,160(%0)\n\
                               std	21,168(%0)\n\
                               std	22,176(%0)\n\
                               std	23,184(%0)\n\
                               std	24,192(%0)\n\
                               std	25,200(%0)\n\
                               std	26,208(%0)\n\
                               std	27,216(%0)\n\
                               std	28,224(%0)\n\
                               std	29,232(%0)\n\
                               std	30,240(%0)\n\
                               std	31,248(%0)" : : "b" (&regs));
		regs.nip = regs.link = ((unsigned long *)regs.gpr[1])[1];
		regs.msr = get_msr();
		regs.ctr = get_ctr();
		regs.xer = get_xer();
		regs.ccr = get_cr();
		regs.trap = 0;
		taskregs = regs;
	}
	else
	{
		taskregs.nip = p->thread.regs->nip;
		taskregs.gpr[1] = p->thread.regs->gpr[1];
		taskregs.link = p->thread.regs->link;

	}
	kdb_printf("taskregs.gpr[1]=%lx\n", taskregs.gpr[1]);
	if (taskregs.gpr[1] < (unsigned long)p ||
	    taskregs.gpr[1] >= (unsigned long)p + 8192) {
		kdb_printf("Stack is not in task_struct, backtrace not available\n");
		return(0);
	}
	kdb_printf("About to backtrace process!\n");
	return kdba_bt_stack_ppc(&taskregs, NULL, argcount, p, 1);

}
