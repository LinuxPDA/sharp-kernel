/* 
 *  arch/mips/kernel/gdb-stub.c
 *
 *  Originally written by Glenn Engel, Lake Stevens Instrument Division
 *
 *  Contributed by HP Systems
 *
 *  Modified for SPARC by Stu Grossman, Cygnus Support.
 *
 *  Modified for Linux/MIPS (and MIPS in general) by Andreas Busse
 *  Send complaints, suggestions etc. to <andy@waldorf-gmbh.de>
 *
 *  Copyright (C) 1995 Andreas Busse
 *
 **************************************************************************
 *  16 Nov, 2000.
 *  Further MIPS enhancement
 *  Change to allow "pass-through" of exceptions coming from user mode.
 *  Chris Johnson (Cobalt Networks) and Kevin Kissell, kevink@mips.com
 *
 *  Kevin D. Kissell, kevink@mips.org and Carsten Langgaard, carstenl@mips.com
 *  Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 **************************************************************************
 *
 * $Id: gdb-stub.c,v 1.3 2001/04/20 16:34:45 mated Exp $
 */

/*
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a BREAK instruction.
 *
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 *    bBB..BB	    Set baud rate to BB..BB		   OK or BNN, then sets
 *							   baud rate
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 * 
 *  ==============
 *  MORE EXAMPLES:
 *  ==============
 *
 *  For reference -- the following are the steps that one
 *  company took (RidgeRun Inc) to get remote gdb debugging
 *  going. In this scenario the host machine was a PC and the
 *  target platform was a Galileo EVB64120A MIPS evaluation
 *  board.
 *   
 *  Step 1:
 *  First download gdb-5.0.tar.gz from the internet.
 *  and then build/install the package.
 * 
 *  Example:
 *    $ tar zxf gdb-5.0.tar.gz
 *    $ cd gdb-5.0
 *    $ ./configure --target=mips-linux-elf
 *    $ make
 *    $ install
 *    $ which mips-linux-elf-gdb
 *    /usr/local/bin/mips-linux-elf-gdb
 * 
 *  Step 2:
 *  Configure linux for remote debugging and build it.
 * 
 *  Example:
 *    $ cd ~/linux
 *    $ make menuconfig <go to "Kernel Hacking" and turn on remote debugging>
 *    $ make dep; make vmlinux
 * 
 *  Step 3:
 *  Download the kernel to the remote target and start
 *  the kernel running. It will promptly halt and wait 
 *  for the host gdb session to connect. It does this
 *  since the "Kernel Hacking" option has defined 
 *  CONFIG_REMOTE_DEBUG which in turn enables your calls
 *  to:
 *     set_debug_traps();
 *     breakpoint();
 * 
 *  Step 4:
 *  Start the gdb session on the host.
 * 
 *  Example:
 *    $ mips-linux-elf-gdb vmlinux
 *    (gdb) set remotebaud 115200
 *    (gdb) target remote /dev/ttyS1
 *    ...at this point you are connected to 
 *       the remote target and can use gdb
 *       in the normal fasion. Setting 
 *       breakpoints, single stepping,
 *       printing variables, etc.
 *
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/gdb-stub.h>
#include <asm/inst.h>

/*
 * external low-level support routines
 */

extern void fltr_set_mem_err(void);
extern void trap_low(void);

/*
 * Function pointers which must be initialized by platform code
 * before entering kernel debugger.
 */

static int dummy_put_char(char c) { return 1; }
static char dummy_get_char(void) { return 0; }

int (*putDebugChar)(char c) = dummy_put_char; /* write a single character */
char (*getDebugChar)(void) = dummy_get_char; /* read and return a char */

/*
 * breakpoint and test functions
 */
extern void breakpoint(void);
extern void breakinst(void);
extern void adel(void);

/*
 * local prototypes
 */

static void getpacket(char *buffer);
static void putpacket(char *buffer);
static int computeSignal(int tt);
static int hex(unsigned char ch);
static int hexToInt(char **ptr, int *intValue);
static unsigned char *mem2hex(char *mem, char *buf, int count, int may_fault);
void handle_exception(struct gdb_regs *regs);

/*
 * BUFMAX defines the maximum number of characters in inbound/outbound buffers
 * at least NUMREGBYTES*2 are needed for register packets
 */
#define BUFMAX 2048

static char input_buffer[BUFMAX];
static char output_buffer[BUFMAX];
static int initialized=0;	/* !0 means we've been initialized */
static const char hexchars[]="0123456789abcdef";

/*
 * Debug-of-debug output, which can be multiplexed with GDB
 * output so long as putDebugChar() sets the high bit, which
 * is stripped by gdb then used for its own protocol.  GDB
 * forwards output without the high but set onward to the
 * user session.  If we are forced to use a common port for
 * console and debug, this is very convienient.
 */
#ifdef DEBUG_REMOTE_DEBUG
#define dbg_out printk
#else
#define dbg_out if(0) printk
#endif

/*
 * Convert ch from a hex digit to an int
 */
static int hex(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

/*
 * scan for the sequence $<data>#<checksum>
 */
static void getpacket(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	unsigned char ch;

	do {
		/*
		 * wait around for the start character,
		 * ignore all other characters
		 */
		while ((ch = (getDebugChar() & 0x7f)) != '$') {
			dbg_out(" kgtp skip %c\n", ch);
		}
		dbg_out("got start\n");

		checksum = 0;
		xmitcsum = -1;
		count = 0;
	
		/*
		 * now, read until a # or end of buffer is found
		 */
		while (count < BUFMAX) {
			ch = getDebugChar() & 0x7f;
			dbg_out("kgtp keep %c\n", ch);
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}

		if (count >= BUFMAX)
			continue;

		buffer[count] = 0;

		if (ch == '#') {
			xmitcsum = hex(getDebugChar() & 0x7f) << 4;
			xmitcsum |= hex(getDebugChar() & 0x7f);

			if (checksum != xmitcsum) {
				dbg_out(" kgtp nak\n");
				putDebugChar('-');	/* failed checksum */
			} else {
				dbg_out(" kgtp ack\n");
				putDebugChar('+'); /* successful transfer */

				/*
				 * if a sequence char is present,
				 * reply the sequence ID
				 */
				if (buffer[2] == ':') {
					putDebugChar(buffer[0]);
					putDebugChar(buffer[1]);

					/*
					 * remove sequence chars from buffer
					 */
					count = strlen(buffer);
					for (i=3; i <= count; i++)
						buffer[i-3] = buffer[i];
				}
			}
		}
	} while (checksum != xmitcsum);

	dbg_out("kgetpacket read \"%s\"\n", buffer);
}

/*
 * send the packet in buffer.
 */
static void putpacket(char *buffer)
{
	unsigned char checksum;
	int count;
	unsigned char ch;

	/*
	 * $<packet info>#<checksum>.
	 */

	dbg_out(" ksend \"%s\"\n", buffer);

	for(;;) {
		putDebugChar('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count]) != 0) {
			if (!(putDebugChar(ch)))
				return;
			checksum += ch;
			count += 1;
		}

		putDebugChar('#');
		putDebugChar(hexchars[checksum >> 4]);
		putDebugChar(hexchars[checksum & 0xf]);

		ch = getDebugChar() & 0x7f;

		if(ch == '+') {
			dbg_out("kptpk ack\n");
			break;
		} 
		else if (ch == '-') dbg_out("kptpk nak\n");
		else dbg_out("kptpk bad ack/nak '%c'\n", ch);
	}
}


/*
 * Handshake with mips/mm/fault.c:do_page_fault()
 * to deal with bad memory addresses.
 */

volatile int debugmem_got_flt;
int debugmem_flt_set;

#define CATCH_DEBUG_FAULTS	debugmem_got_flt = 0; \
				debugmem_flt_set = 1;

#define	RELEASE_DEBUG_FAULTS	debugmem_got_flt = 0; \
				debugmem_flt_set = 0;

#define TAKE_DEBUG_FAULTS	RELEASE_DEBUG_FAULTS

#define	DEBUG_OP_FAULTED	debugmem_got_flt


/*
 * Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 * If MAY_FAULT is non-zero, then we will handle memory faults by returning
 * a 0, else treat a fault like any other fault in the stub.
 */
static unsigned char *mem2hex(char *mem, char *buf, int count, int may_fault)
{
	unsigned char ch;

	if(may_fault) {
		CATCH_DEBUG_FAULTS;
	} else {
		TAKE_DEBUG_FAULTS;
	}
	while (count-- > 0 && !DEBUG_OP_FAULTED) {
		ch = *(mem++);
		*buf++ = hexchars[ch >> 4];
		*buf++ = hexchars[ch & 0xf];
	}

	if(DEBUG_OP_FAULTED) buf = 0;
	else *buf = 0;

	RELEASE_DEBUG_FAULTS;

	return buf;
}

/*
 * convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written
 */
static char *hex2mem(char *buf, char *mem, int count, int may_fault)
{
	int i;
	unsigned char ch;
	char *startadr = mem;

	if(may_fault) {
		CATCH_DEBUG_FAULTS;
	} else {
		TAKE_DEBUG_FAULTS;
	}

	for (i=0; i<count && !DEBUG_OP_FAULTED; i++)
	{
		ch = hex(*buf++) << 4;
		ch |= hex(*buf++);
		*(mem++) = ch;
	}
	/* 
	 * Since we may have written to instruction space via 
	 * the data path, the icache needs to be flushed here.
	 */
	flush_icache_range((unsigned long)startadr, 
			   (unsigned long)startadr+count);

	if (DEBUG_OP_FAULTED) mem = 0;

	RELEASE_DEBUG_FAULTS;

	return mem;
}

/*
 * This table contains the mapping between MIPS hardware trap types, and
 * signals, which are primarily what GDB understands.  It also indicates
 * which hardware traps we need to commandeer when initializing the stub.
 */
static struct hard_trap_info {
	unsigned char tt;		/* MIPS R3K/R4K Trap type code */
	unsigned char signo;		/* Signal that we map this trap into */
} hard_trap_info[] = {
	/* 
	 * Note that address error exceptions are not dealt with by default,
	 * as these apparently actually happen in the kernel and are cleaned 
	 * up by the instruction emulator.  TODO: work out a clean interface
	 * with the kernel trap handler to catch these cases as well.
	 */

	{ 4, SIGBUS }, 			/* address error (load) */
	{ 5, SIGBUS },			/* address error (store) */
	{ 6, SIGBUS },			/* instruction bus error */
	{ 7, SIGBUS },			/* data bus error */
	{ 9, SIGTRAP },			/* break */
	{ 10, SIGILL },			/* reserved instruction */
	/* 
	 * Coprocessor unusable is used to manage FP context switching,
	 * and is dangerous to play with.
	 */
	{ 11, SIGILL },			/* CP unusable */
	{ 12, SIGFPE },			/* overflow */
	{ 13, SIGTRAP },		/* trap */
	{ 14, SIGSEGV },		/* virtual instruction cache coherency */
	{ 15, SIGFPE },			/* floating point exception */
	{ 23, SIGSEGV },		/* watch */
	{ 31, SIGSEGV },		/* virtual data cache coherency */
	{ 0, 0}				/* Must be last */
};

/*
 * global mask to override kernel debugger taking over exception
 * vectors.  Default value is 0x00008830, leaving address errors,
 * coprocessor unusable, and floating point exceptions to the kernel.
 * This is less crucial with kernel/user mode filter in place.
 * value can be patched before kgdb init.  A boot command line interface
 * would be convenient...
 */
long kgdb_trap_mask = 0x000008830;

/*
 * Array to store default exception handler addresses.
 * trap_low() will invoke these handlers if CPU is in
 * user mode, so that only kernel mode traps invoke the
 * kernel debugger. 
 */
void * kgdb_user_exception_handler[32];

/*
 * Set up exception handlers for tracing and breakpoints
 */
void set_debug_traps(void)
{
	struct hard_trap_info *ht;
	unsigned long flags;

	if(!initialized) {
		save_and_cli(flags);
		for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
		    if(!(kgdb_trap_mask & (1 << ht->tt)))
			kgdb_user_exception_handler[ht->tt]
			    = set_except_vector(ht->tt, trap_low);
		initialized = 1;
		restore_flags(flags);
	}
}

void	sync_debug_input(void)
{
	unsigned char c;
	/*
	 * In case GDB is started before us, ack any packets
	 * (presumably "$?#xx") sitting there.
	 */
	while((c = getDebugChar()) != '$') {}
	while((c = getDebugChar()) != '#') {}
	c = getDebugChar(); /* eat first csum byte */
	c = getDebugChar(); /* eat second csum byte */
	putDebugChar('+'); /* ack it */
}

/*
 * Trap handler for memory errors.  This just sets mem_err to be non-zero.  It
 * assumes that %l1 is non-zero.  This should be safe, as it is doubtful that
 * 0 would ever contain code that could mem fault.  This routine will skip
 * past the faulting instruction after setting mem_err.
 */
extern void fltr_set_mem_err(void)
{
	/* FIXME: Needs to be written... */
}

/*
 * Convert the MIPS hardware trap type code to a Unix signal number.
 */
static int computeSignal(int tt)
{
	struct hard_trap_info *ht;

	for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
		if (ht->tt == tt)
			return ht->signo;

	return SIGHUP;		/* default for things we don't know about */
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int hexToInt(char **ptr, int *intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue < 0)
			break;

		*intValue = (*intValue << 4) | hexValue;
		numChars ++;

		(*ptr)++;
	}

	return (numChars);
}


#if 1
/*
 * Print registers (on target console)
 * Used only to debug the stub...
 */
void show_gdbregs(struct gdb_regs * regs)
{
	/*
	 * Saved main processor registers
	 */
	dbg_out("$0 : %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       regs->reg0, regs->reg1, regs->reg2, regs->reg3,
               regs->reg4, regs->reg5, regs->reg6, regs->reg7);
	dbg_out("$8 : %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       regs->reg8, regs->reg9, regs->reg10, regs->reg11,
               regs->reg12, regs->reg13, regs->reg14, regs->reg15);
	dbg_out("$16: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       regs->reg16, regs->reg17, regs->reg18, regs->reg19,
               regs->reg20, regs->reg21, regs->reg22, regs->reg23);
	dbg_out("$24: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       regs->reg24, regs->reg25, regs->reg26, regs->reg27,
	       regs->reg28, regs->reg29, regs->reg30, regs->reg31);

	/*
	 * Saved cp0 registers
	 */
	dbg_out("epc  : %08lx\nStatus: %08lx\nCause : %08lx\n",
	       regs->cp0_epc, regs->cp0_status, regs->cp0_cause);
}
#endif /* undead code */

/*
 * We single-step by setting breakpoints. When an exception
 * is handled, we need to restore the instructions hoisted
 * when the breakpoints were set.
 *
 * This is where we save the original instructions.
 */
static struct gdb_bp_save {
	unsigned int addr;
        unsigned int val;
} step_bp[2];

#define BP 0x0000000d  /* break opcode */

/*
 * Set breakpoint instructions for single stepping.
 */
static void single_step(struct gdb_regs *regs)
{
	mips_instruction insn;
	unsigned int targ;
	int is_branch, is_cond, i;

	targ = regs->cp0_epc;
	is_branch = is_cond = 0;

	insn = *(mips_instruction *)targ;

	switch (MIPSInst_OPCODE(insn)) {
	/*
	 * jr and jalr are in r_format format.
	 */
	case spec_op:
		switch (MIPSInst_FUNC(insn)) {
		case jalr_op:
		case jr_op:
			targ = *(&regs->reg0 + MIPSInst_RS(insn));
			is_branch = 1;
			break;
		}
		break;

	/*
	 * This group contains:
	 * bltz_op, bgez_op, bltzl_op, bgezl_op,
	 * bltzal_op, bgezal_op, bltzall_op, bgezall_op.
	 */
	case bcond_op:
		is_branch = is_cond = 1;
		targ += 4 + (MIPSInst_SIMM(insn) << 2);
		break;

	/*
	 * These are unconditional and in j_format.
	 */
	case jal_op:
	case j_op:
		is_branch = 1;
		targ += 4;
		targ >>= 28;
		targ <<= 28;
		targ |= (MIPSInst_JTARGET(insn) << 2);
		break;

	/*
	 * These are conditional.
	 */
	case beq_op:
	case beql_op:
	case bne_op:
	case bnel_op:
	case blez_op:
	case blezl_op:
	case bgtz_op:
	case bgtzl_op:
	case cop0_op:
	case cop1_op:
	case cop2_op:
	case cop1x_op:
		is_branch = is_cond = 1;
		targ += 4 + (MIPSInst_SIMM(insn) << 2);
		break;
	}
				
	if (is_branch) {
		i = 0;
		if (is_cond && targ != (regs->cp0_epc + 8)) {
			step_bp[i].addr = regs->cp0_epc + 8;
			step_bp[i++].val = *(unsigned *)(regs->cp0_epc + 8);
			*(unsigned *)(regs->cp0_epc + 8) = BP;
		}
		step_bp[i].addr = targ;
		step_bp[i].val  = *(unsigned *)targ;
		*(unsigned *)targ = BP;
	} else {
		step_bp[0].addr = regs->cp0_epc + 4;
		step_bp[0].val  = *(unsigned *)(regs->cp0_epc + 4);
		*(unsigned *)(regs->cp0_epc + 4) = BP;
	}
}

/*
 *  If asynchronously interrupted by gdb, then we need to set a breakpoint
 *  at the interrupted instruction so that we wind up stopped with a 
 *  reasonable stack frame.
 */
static struct gdb_bp_save async_bp;

void set_async_breakpoint(unsigned int epc)
{
	async_bp.addr = epc;
	async_bp.val  = *(unsigned *)epc;
	*(unsigned *)epc = BP;
	flush_cache_all();
}


/*
 * This function does all command processing for interfacing to gdb.  It
 * returns 1 if you should skip the instruction at the trap address, 0
 * otherwise.
 */
void handle_exception (struct gdb_regs *regs)
{
	int trap;			/* Trap type */
	int sigval;
	int addr;
	int length;
	char *ptr;
	unsigned long *stack;
	static int init = 0;

#if 1	
	dbg_out("in handle_exception()\n");
	show_gdbregs(regs);
#endif
	
	/*
	 * First check trap type. If this is CP_UNUSABLE and CP_ID is 1,
	 * the simply switch the FPU on and return since this is no error
	 * condition. kernel/traps.c does the same.
	 * FIXME: This doesn't work yet, so we don't catch CP_UNUSABLE
	 * traps for now.
	 */
	trap = (regs->cp0_cause & 0x7c) >> 2;
	dbg_out("trap=%d\n",trap);
	if (trap == 11) {
		if (((regs->cp0_cause >> CAUSEB_CE) & 3) == 1) {
			memset(&current->thread.fpu, 0,
				sizeof(union mips_fpu_union));
			regs->cp0_status |= ST0_CU1;
			/*
			 * More of the logic in trap.c do_cpu()
			 * would need to be replicated here for
			 * this stuff to work.
			 */
			return;
		}
	}

	if(!init) {
		sync_debug_input();
		init = 1;
	} 

	/*
	 * If we're in breakpoint() increment the PC
	 */
	if (trap == 9 && regs->cp0_epc == (unsigned long)breakinst) {
		/*
		 * In order to handle branches and delay slots,
		 * a function like __compute_return_epc() needs
		 * to be invoked.  That function as it exists
		 * in Linux 2.2 assumes a pt_regs context, among
		 * other things.  To Be Fixed.  XXXXX
		 */
		regs->cp0_epc += 4;
	}

	/*
	 * If we were single_stepping, restore the opcodes hoisted
	 * for the breakpoint[s].
	 */
	if (step_bp[0].addr) {
		*(unsigned *)step_bp[0].addr = step_bp[0].val;
		step_bp[0].addr = 0;
		    
		if (step_bp[1].addr) {
			*(unsigned *)step_bp[1].addr = step_bp[1].val;
			step_bp[1].addr = 0;
		}
	}

	/*
	 * If we were interrupted asynchronously by gdb, then a
	 * breakpoint was set at the EPC of the interrupt so
	 * that we'd wind up here with an interesting stack frame.
	 */
	if (async_bp.addr) {
		*(unsigned *)async_bp.addr = async_bp.val;
		async_bp.addr = 0;
	}

	stack = (long *)regs->reg29;			/* stack ptr */
	sigval = computeSignal(trap);

	/*
	 * reply to host that an exception has occurred
	 */
	ptr = output_buffer;

	/*
	 * Send trap type (converted to signal)
	 */
	*ptr++ = 'T';
	*ptr++ = hexchars[sigval >> 4];
	*ptr++ = hexchars[sigval & 0xf];

	/*
	 * Send Error PC
	 */
	*ptr++ = hexchars[REG_EPC >> 4];
	*ptr++ = hexchars[REG_EPC & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((char *)&regs->cp0_epc, ptr, 4, 0);
	*ptr++ = ';';

	/*
	 * Send frame pointer
	 */
	*ptr++ = hexchars[REG_FP >> 4];
	*ptr++ = hexchars[REG_FP & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((char *)&regs->reg30, ptr, 4, 0);
	*ptr++ = ';';

	/*
	 * Send stack pointer
	 */
	*ptr++ = hexchars[REG_SP >> 4];
	*ptr++ = hexchars[REG_SP & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((char *)&regs->reg29, ptr, 4, 0);
	*ptr++ = ';';

	*ptr++ = 0;
	putpacket(output_buffer);	/* send it off... */

	/*
	 * Wait for input from remote GDB
	 */
	while (1) {
		output_buffer[0] = 0;
		getpacket(input_buffer);

		switch (input_buffer[0])
		{
		case '?':
			output_buffer[0] = 'S';
			output_buffer[1] = hexchars[sigval >> 4];
			output_buffer[2] = hexchars[sigval & 0xf];
			output_buffer[3] = 0;
			break;

		case 'd':
			/* toggle debug flag */
			break;

		/*
		 * Return the value of the CPU registers
		 */
		case 'g':
			(void) mem2hex(KGDB_USERREGS_START(regs), output_buffer,
					KGDB_USERREGS_BYTES, 0);
			break;
	  
		/*
		 * set the value of the CPU registers - return OK
		 * XXXXX: Experimental Code from CJ at Cobalt Networks
		 */
		case 'G':
			{
				struct gdb_regs newregs;
	
				ptr = hex2mem(&input_buffer[1],
						KGDB_USERREGS_START(&newregs),
						KGDB_USERREGS_BYTES, 0);
				if(ptr) {
					memcpy(KGDB_USERREGS_START(regs),
						KGDB_USERREGS_START(&newregs),
						KGDB_USERREGS_BYTES);
					strcpy(output_buffer, "OK");
				} else {
					strcpy(output_buffer, "ENN");
				}
			}
			break;
		/*
		 * mAA..AA,LLLL  Read LLLL bytes at address AA..AA
		 */
		case 'm':
			ptr = &input_buffer[1];

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)) {
				if (mem2hex((char *)addr, output_buffer, length, 1))
					break;
				strcpy (output_buffer, "E03");
			} else
				strcpy(output_buffer,"E01");
			break;

		/*
		 * MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK
		 */
		case 'M': 
			ptr = &input_buffer[1];

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)
				&& *ptr++ == ':') {
				if (hex2mem(ptr, (char *)addr, length, 1))
					strcpy(output_buffer, "OK");
				else
					strcpy(output_buffer, "E03");
			}
			else
				strcpy(output_buffer, "E02");
			break;

		/*
		 * cAA..AA    Continue at address AA..AA(optional)
		 */
		case 'c':    
			/* try to read optional parameter, pc unchanged if no parm */

			ptr = &input_buffer[1];
			if (hexToInt(&ptr, &addr))
				regs->cp0_epc = addr;
	  
			/*
			 * Need to flush the instruction cache here, as we may
			 * have deposited a breakpoint, and the icache probably
			 * has no way of knowing that a data ref to some location
			 * may have changed something that is in the instruction
			 * cache.
			 * NB: We flush both caches, just to be sure...
			 */

			flush_cache_all();
			return;
			/* NOTREACHED */
			break;


		/*
		 * kill the program
		 */
		case 'k' :
			break;		/* do nothing */


		/*
		 * Reset the whole machine (FIXME: system dependent)
		 */
		case 'r':
			break;


		/*
		 * Step to next instruction
		 */
		case 's':
			/*
			 * There is no single step insn in the MIPS ISA, so we
			 * use breakpoints and continue, instead.
			 */
			single_step(regs);
			flush_cache_all();
			return;
			/* NOTREACHED */

		/*
		 * Set baud rate (bBB)
		 * FIXME: Needs to be written
		 */
		case 'b':
		{
#if 0				
			int baudrate;
			extern void set_timer_3();

			ptr = &input_buffer[1];
			if (!hexToInt(&ptr, &baudrate))
			{
				strcpy(output_buffer,"B01");
				break;
			}

			/* Convert baud rate to uart clock divider */

			switch (baudrate)
			{
				case 38400:
					baudrate = 16;
					break;
				case 19200:
					baudrate = 33;
					break;
				case 9600:
					baudrate = 65;
					break;
				default:
					baudrate = 0;
					strcpy(output_buffer,"B02");
					goto x1;
			}

			if (baudrate) {
				putpacket("OK");	/* Ack before changing speed */
				set_timer_3(baudrate); /* Set it */
			}
#endif
		}
		break;

		}			/* switch */

		/*
		 * reply to the request
		 */

		putpacket(output_buffer);

	} /* while */
}

/*
 * This function will generate a breakpoint exception.  It is used at the
 * beginning of a program to sync up with a debugger and can be used
 * otherwise as a quick means to stop program execution and "break" into
 * the debugger.
 */
void breakpoint(void)
{
	if (!initialized)
		return;

	__asm__ __volatile__("
			.globl	breakinst
			.set	noreorder
			nop
breakinst:		break
			nop
			.set	reorder
	");
}

void adel(void)
{
	__asm__ __volatile__("
			.globl	adel
			la	$8,0x80000001
			lw	$9,0($8)
	");
}
