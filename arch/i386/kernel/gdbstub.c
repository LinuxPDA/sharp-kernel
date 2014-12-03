/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

/*
 * Copyright (C) 2000-2001 VERITAS Software Corporation.
 */
/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  Updated by:	     Amit Kale<akale@veritas.com>
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *  Origianl kgdb, compatibility with 2.1.xx kernel by David Grothe <dave@gcom.com>
 *  Integrated into 2.2.5 kernel by Tigran Aivazian <tigran@sco.com>
 *      thread support,
 *      support for multiple processors,
 *  	support for ia-32(x86) hardware debugging,
 *  	Console support,
 *  	handling nmi watchdog
 *  	Amit S. Kale ( akale@veritas.com )
 *
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing an int 3.
 *
 *************
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
 ****************************************************************************/

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <asm/vm86.h>
#include <asm/system.h>
#include <asm/ptrace.h>			/* for linux pt_regs struct */
#include <linux/gdb.h>
#ifdef CONFIG_GDB_CONSOLE
#include <linux/console.h>
#endif
#include <linux/init.h>

/************************************************************************
 *
 * external low-level support routines
 */
typedef void (*Function)(void);           /* pointer to a function */

/* Thread reference */
typedef unsigned char threadref[8];

extern int putDebugChar(int);   /* write a single character      */
extern int getDebugChar(void);   /* read and return a single char */

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
/* Longer buffer is needed to list all threads */
#define BUFMAX 1024

static char initialized;  /* boolean flag. != 0 means we've been initialized */

static const char hexchars[]="0123456789abcdef";

/* Number of bytes of registers.  */
#define NUMREGBYTES 64
/*
 * Note that this register image is in a different order than
 * the register image that Linux produces at interrupt time.
 *
 * Linux's register image is defined by struct pt_regs in ptrace.h.
 * Just why GDB uses a different order is a historical mystery.
 */
enum regnames {_EAX,		/* 0 */
	       _ECX,		/* 1 */
	       _EDX,		/* 2 */
	       _EBX,		/* 3 */
	       _ESP,		/* 4 */
	       _EBP,		/* 5 */
	       _ESI,		/* 6 */
	       _EDI,		/* 7 */
	       _PC 		/* 8 also known as eip */,
	       _PS		/* 9 also known as eflags */,
	       _CS,		/* 10 */
	       _SS,		/* 11 */
	       _DS,		/* 12 */
	       _ES,		/* 13 */
	       _FS,		/* 14 */
	       _GS};		/* 15 */



/***************************  ASSEMBLY CODE MACROS *************************/
/* 									   */

#define BREAKPOINT() asm("   int $3");

/* Put the error code here just in case the user cares.  */
int gdb_i386errcode;
/* Likewise, the vector number here (since GDB only gets the signal
   number through the usual means, and that's not very specific).  */
int gdb_i386vector = -1;

#if KGDB_MAX_NO_CPUS != 8
#error change the definition of slavecpulocks
#endif

static spinlock_t slavecpulocks[KGDB_MAX_NO_CPUS] = { SPIN_LOCK_UNLOCKED,
	SPIN_LOCK_UNLOCKED, SPIN_LOCK_UNLOCKED, SPIN_LOCK_UNLOCKED,
	SPIN_LOCK_UNLOCKED, SPIN_LOCK_UNLOCKED, SPIN_LOCK_UNLOCKED,
	SPIN_LOCK_UNLOCKED };
volatile int procindebug[KGDB_MAX_NO_CPUS];

volatile unsigned kgdb_step = 0;

volatile unsigned kgdb_lock = 0;

enum gdb_bptype
{
	bp_breakpoint = '0',
	bp_hardware_breakpoint,
	bp_write_watchpoint,
	bp_read_watchpoint,
	bp_access_watchpoint
};

enum gdb_bpstate
{
	bp_disabled,
	bp_enabled
};

#define BREAK_INSTR_SIZE	1

struct gdb_breakpoint
{
	unsigned int		bpt_addr;
	unsigned char		saved_instr[BREAK_INSTR_SIZE];
	enum gdb_bptype 	type;
	enum gdb_bpstate	state;
};

typedef struct gdb_breakpoint gdb_breakpoint_t;

#define MAX_BREAKPOINTS	16

gdb_breakpoint_t kgdb_break[MAX_BREAKPOINTS];
static char gdb_bpt_instr[BREAK_INSTR_SIZE] = {0xcc};

static void kgdb_usercode (void)
{
}

int hex(char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}


/* scan for the sequence $<data>#<checksum>     */
void getpacket(char * buffer)
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int  i;
  int  count;
  char ch;

  do {
    /* wait around for the start character, ignore all other characters */
    while ((ch = (getDebugChar() & 0x7f)) != '$');
    checksum = 0;
    xmitcsum = -1;

    count = 0;

    /* now, read until a # or end of buffer is found */
    while (count < BUFMAX) {
      ch = getDebugChar() & 0x7f;
      if (ch == '#') break;
      checksum = checksum + ch;
      buffer[count] = ch;
      count = count + 1;
      }
    buffer[count] = 0;

    if (ch == '#') {
      xmitcsum = hex(getDebugChar() & 0x7f) << 4;
      xmitcsum += hex(getDebugChar() & 0x7f);

      if (checksum != xmitcsum) putDebugChar('-');  /* failed checksum */
      else {
	 putDebugChar('+');  /* successful transfer */
	 /* if a sequence char is present, reply the sequence ID */
	 if (buffer[2] == ':') {
	    putDebugChar( buffer[0] );
	    putDebugChar( buffer[1] );
	    /* remove sequence chars from buffer */
	    count = strlen(buffer);
	    for (i=3; i <= count; i++) buffer[i-3] = buffer[i];
	 }
      }
    }
  } while (checksum != xmitcsum);

}

/* send the packet in buffer.  */


void putpacket(char * buffer)
{
  unsigned char checksum;
  int  count;
  char ch;

  /*  $<packet info>#<checksum>. */
  do {
  putDebugChar('$');
  checksum = 0;
  count    = 0;

  while ((ch=buffer[count])) {
    if (! putDebugChar(ch)) return;
    checksum += ch;
    count += 1;
  }

  putDebugChar('#');
  putDebugChar(hexchars[checksum >> 4]);
  putDebugChar(hexchars[checksum % 16]);

  } while ((getDebugChar() & 0x7f) != '+');

}

static char  remcomInBuffer[BUFMAX];
static char  remcomOutBuffer[BUFMAX];
static short error;

static void regs_to_gdb_regs(int *gdb_regs, struct pt_regs *regs)
{
    gdb_regs[_EAX] =  regs->eax;
    gdb_regs[_EBX] =  regs->ebx;
    gdb_regs[_ECX] =  regs->ecx;
    gdb_regs[_EDX] =  regs->edx;
    gdb_regs[_ESI] =  regs->esi;
    gdb_regs[_EDI] =  regs->edi;
    gdb_regs[_EBP] =  regs->ebp;
    gdb_regs[ _DS] =  regs->xds;
    gdb_regs[ _ES] =  regs->xes;
    gdb_regs[ _PS] =  regs->eflags;
    gdb_regs[ _CS] =  regs->xcs;
    gdb_regs[ _PC] =  regs->eip;
    gdb_regs[_ESP] =  (int) (&regs->esp) ;
    gdb_regs[ _SS] =  __KERNEL_DS;
    gdb_regs[ _FS] =  0xFFFF;
    gdb_regs[ _GS] =  0xFFFF;
} /* regs_to_gdb_regs */

static void gdb_regs_to_regs(int *gdb_regs, struct pt_regs *regs)
{
    regs->eax	=     gdb_regs[_EAX] ;
    regs->ebx	=     gdb_regs[_EBX] ;
    regs->ecx	=     gdb_regs[_ECX] ;
    regs->edx	=     gdb_regs[_EDX] ;
    regs->esi	=     gdb_regs[_ESI] ;
    regs->edi	=     gdb_regs[_EDI] ;
    regs->ebp	=     gdb_regs[_EBP] ;
    regs->xds	=     gdb_regs[ _DS] ;
    regs->xes	=     gdb_regs[ _ES] ;
    regs->eflags=     gdb_regs[ _PS] ;
    regs->xcs	=     gdb_regs[ _CS] ;
    regs->eip	=     gdb_regs[ _PC] ;
#if 0					/* can't change these */
    regs->esp	=     gdb_regs[_ESP] ;
    regs->xss	=     gdb_regs[ _SS] ;
    regs->fs	=     gdb_regs[ _FS] ;
    regs->gs	=     gdb_regs[ _GS] ;
#endif

} /* gdb_regs_to_regs */

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int kgdb_memerr = 0;
volatile int kgdb_memerr_expected = 0;
static volatile int kgdb_memerr_cnt = 0;
static          int garbage_loc = -1 ;

int
get_char (char *addr)
{
  return *addr;
}

void
set_char ( char *addr, int val)
{
  *addr = val;
}

static void
get_mem (char *addr,
	 unsigned char *buf,
	 int count)
{
	while (count) {
		*buf++ = get_char(addr++);
		if (kgdb_memerr)
			return;
		count--;
	}
}

static void
set_mem (char *addr,
	 char *buf,
	 int count)
{
	while (count) {
		set_char(addr++,*buf++);
		if (kgdb_memerr)
			return;
		count--;
	}
}

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
/* If MAY_FAULT is non-zero, then we should set kgdb_memerr in response to
   a fault; if zero treat a fault like any other fault in the stub.  */
char* mem2hex( char* mem,
	       char* buf,
	       int   count,
	       int may_fault)
{
      int i;
      unsigned char ch;

      if (may_fault)
      {
          kgdb_memerr_expected = 1 ;
          kgdb_memerr = 0 ;
      }
      for (i=0;i<count;i++) {

	  ch = get_char (mem++);

	  if (may_fault && kgdb_memerr)
	  {
	    *buf = 0 ;				/* truncate buffer */
	    return (buf);
	  }
          *buf++ = hexchars[ch >> 4];
          *buf++ = hexchars[ch % 16];
      }
      *buf = 0;
      if (may_fault)
	  kgdb_memerr_expected = 0 ;
      return(buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
char* hex2mem( char* buf,
	       char* mem,
	       int   count,
	       int may_fault)
{
      int i;
      unsigned char ch;

      if (may_fault)
      {
          kgdb_memerr_expected = 1 ;
          kgdb_memerr = 0 ;
      }
      for (i=0;i<count;i++) {
          ch = hex(*buf++) << 4;
          ch = ch + hex(*buf++);
	  set_char (mem++, ch);

	  if (may_fault && kgdb_memerr)
	  {
	    return (mem);
	  }
      }
      if (may_fault)
	  kgdb_memerr_expected = 0 ;
      return(mem);
}


/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
int hexToInt(char **ptr, int *intValue)
{
    int numChars = 0;
    int hexValue;

    *intValue = 0;

    while (**ptr)
    {
        hexValue = hex(**ptr);
        if (hexValue >=0)
        {
            *intValue = (*intValue <<4) | hexValue;
            numChars ++;
        }
        else
            break;

        (*ptr)++;
    }

    return (numChars);
}

#ifdef CONFIG_KGDB_THREAD
static int
stubhex (
     int ch)
{
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  return -1;
}


static int
stub_unpack_int (
     char *buff,
     int fieldlength)
{
  int nibble;
  int retval = 0;

  while (fieldlength)
    {
      nibble = stubhex (*buff++);
      retval |= nibble;
      fieldlength--;
      if (fieldlength)
	retval = retval << 4;
    }
  return retval;
}
#endif

static char *
pack_hex_byte (
     char *pkt,
     int byte)
{
  *pkt++ = hexchars[(byte >> 4) & 0xf];
  *pkt++ = hexchars[(byte & 0xf)];
  return pkt;
}

#define BUF_THREAD_ID_SIZE 16

#ifdef CONFIG_KGDB_THREAD
static char *
pack_threadid (
     char *pkt,
     threadref *id)
{
  char *limit;
  unsigned char *altid;

  altid = (unsigned char *) id;
  limit = pkt + BUF_THREAD_ID_SIZE;
  while (pkt < limit)
    pkt = pack_hex_byte (pkt, *altid++);
  return pkt;
}

static char *
unpack_byte (
     char *buf,
     int *value)
{
  *value = stub_unpack_int (buf, 2);
  return buf + 2;
}

static char *
unpack_threadid (
     char *inbuf,
     threadref *id)
{
  char *altref;
  char *limit = inbuf + BUF_THREAD_ID_SIZE;
  int x, y;

  altref = (char *) id;

  while (inbuf < limit)
    {
      x = stubhex (*inbuf++);
      y = stubhex (*inbuf++);
      *altref++ = (x << 4) | y;
    }
  return inbuf;
}
#endif

void
int_to_threadref (
     threadref *id,
     int value)
{
  unsigned char *scan;

  scan = (unsigned char *) id;
  {
    int i = 4;
    while (i--)
      *scan++ = 0;
  }
  *scan++ = (value >> 24) & 0xff;
  *scan++ = (value >> 16) & 0xff;
  *scan++ = (value >> 8) & 0xff;
  *scan++ = (value & 0xff);
}

#ifdef CONFIG_KGDB_THREAD
static int
threadref_to_int (
     threadref *ref)
{
  int i, value = 0;
  unsigned char *scan;

  scan = (char *) ref;
  scan += 4;
  i = 4;
  while (i-- > 0)
    value = (value << 8) | ((*scan++) & 0xff);
  return value;
}


struct task_struct *
getthread (
	int pid)
{
	struct task_struct *thread;
	thread = find_task_by_pid(pid);
	if (thread) {
		return thread;
	}
	thread = &init_task;
	do {
		if (thread->pid == pid) {
			return thread;
		}
		thread = thread->next_task;
	} while (thread != &init_task);
	return NULL;
}
#endif

struct hw_breakpoint {
	unsigned enabled;
	unsigned type;
	unsigned len;
	unsigned addr;
} breakinfo[4] = { { enabled:0 }, { enabled:0 }, { enabled:0 }, { enabled:0 }};

void correct_hw_break( void )
{
	int breakno;
	int correctit;
	int breakbit;
	unsigned dr7;

	asm volatile ( 
		"movl %%db7, %0\n"
		: "=r" (dr7)
		: );
	do
	{
		unsigned addr0, addr1, addr2, addr3;
		asm volatile (
			"movl %%db0, %0\n"
			"movl %%db1, %1\n"
			"movl %%db2, %2\n"
			"movl %%db3, %3\n"
			: "=r" (addr0), "=r" (addr1), "=r" (addr2),
			"=r" (addr3) : );
	} while (0);
	correctit = 0;
	for (breakno = 0; breakno < 3; breakno++) {
		breakbit = 2 << (breakno << 1);
		if (!(dr7 & breakbit) && breakinfo[breakno].enabled) {
			correctit = 1;
			dr7 |= breakbit;
			dr7 &= ~(0xf0000 << (breakno << 2));
			dr7 |= (((breakinfo[breakno].len << 2) |
			       breakinfo[breakno].type) << 16) <<
			       (breakno << 2);
			switch (breakno) {
			case 0:
				asm volatile ("movl %0, %%dr0\n"
					: 
					: "r" (breakinfo[breakno].addr) );
				break;

			case 1:
				asm volatile ("movl %0, %%dr1\n"
					: 
					: "r" (breakinfo[breakno].addr) );
				break;

			case 2:
				asm volatile ("movl %0, %%dr2\n"
					: 
					: "r" (breakinfo[breakno].addr) );
				break;

			case 3:
				asm volatile ("movl %0, %%dr3\n"
					: 
					: "r" (breakinfo[breakno].addr) );
				break;
			}
		} else if ((dr7 & breakbit) && !breakinfo[breakno].enabled){
			correctit = 1;
			dr7 &= ~breakbit;
			dr7 &= ~(0xf0000 << (breakno << 2));
		}
	}
	if (correctit) {
		asm volatile ( "movl %0, %%db7\n"
			       :
			       : "r" (dr7));
	}
}

int remove_hw_break(
	unsigned breakno)
{
	if (!breakinfo[breakno].enabled) {
		return -1;
	}
	breakinfo[breakno].enabled = 0;
	return 0;
}

int set_hw_break(
	unsigned breakno,
	unsigned type,
	unsigned len,
	unsigned addr)
{
	if (breakinfo[breakno].enabled) {
		return -1;
	}
	breakinfo[breakno].enabled = 1;
	breakinfo[breakno].type = type;
	breakinfo[breakno].len = len;
	breakinfo[breakno].addr = addr;
	return 0;
}

static int
set_break (
	unsigned addr)
{
	int i, breakno = -1;

	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == bp_enabled) &&
		    (kgdb_break[i].bpt_addr == addr)) {
		    	breakno = -1;
		    	break;
		}

		if (kgdb_break[i].state == bp_disabled) {
			if ((breakno == -1) || (kgdb_break[i].bpt_addr == addr))
				breakno = i;
		}
	}
	if (breakno == -1)
		return -1;

	get_mem((char *)addr,kgdb_break[breakno].saved_instr,BREAK_INSTR_SIZE);
	if (kgdb_memerr)
		return -1;

	set_mem((char *)addr,gdb_bpt_instr,BREAK_INSTR_SIZE);
	if (kgdb_memerr)
		return -1;

	kgdb_break[breakno].state = bp_enabled;
	kgdb_break[breakno].type = bp_breakpoint;
	kgdb_break[breakno].bpt_addr = addr;

	return 0;
}

static int
remove_break (unsigned addr)
{
	int i;

	for (i=0; i<MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == bp_enabled) &&
		    (kgdb_break[i].bpt_addr == addr)) {
		    	set_mem((char *)addr, kgdb_break[i].saved_instr,
					BREAK_INSTR_SIZE);
			if (kgdb_memerr)
				return -1;
			kgdb_break[i].state = bp_disabled;
			return 0;
		}
	}
	return -1;
}

static void
clear_disabled_break()
{
		int i;
		for (i=0; i<MAX_BREAKPOINTS; i++) {
				if (kgdb_break[i].state == bp_disabled)
						kgdb_break[i].bpt_addr = 0;
		}
}

void gdb_wait(struct pt_regs *regs)
{
	unsigned flags;
	int processor;
	
	local_irq_save(flags);
	processor = smp_processor_id();
	procindebug[processor] = 1;
	current->thread.kgdbregs = regs;

	/* Wait till master processor goes completely into the debugger */
	while (!procindebug[kgdb_lock - 1]) {
		int i = 10;	/* an arbitrary number */

		while (--i)
			asm volatile ("nop": : : "memory");

	}

	/* Wait till master processor is done with debugging */
	spin_lock(slavecpulocks + processor);
	correct_hw_break();

	/* Signal the master processor that we are done */
	procindebug[processor] = 0;
	spin_unlock(slavecpulocks + processor);
	local_irq_restore(flags);
}

void
printexceptioninfo(
	int exceptionNo,
	int errorcode,
	char *buffer)
{
	unsigned dr6;
	int i;
	switch (exceptionNo) {
	case 1:			/* debug exception */
		break;
	case 3:			/* breakpoint */
		sprintf(buffer, "Software breakpoint");
		return;
	default:
		sprintf(buffer, "Details not available");
		return;
	}
	asm volatile ("movl %%db6, %0\n"
		      : "=r" (dr6)
		      : );
	if (dr6 & 0x4000) {
		sprintf(buffer, "Single step");
		return;
	}
	for (i = 0; i < 4; ++i) {
		if (dr6 & (1 << i)) {
			sprintf(buffer, "Hardware breakpoint %d", i);
			return;
		}
	}
	sprintf(buffer, "Unknown trap");
	return;
}

/*
 * This function does all command procesing for interfacing to gdb.
 *
 * NOTE:  The INT nn instruction leaves the state of the interrupt
 *        enable flag UNCHANGED.  That means that when this routine
 *        is entered via a breakpoint (INT 3) instruction from code
 *        that has interrupts enabled, then interrupts will STILL BE
 *        enabled when this routine is entered.  The first thing that
 *        we do here is disable interrupts so as to prevent recursive
 *        entries and bothersome serial interrupts while we are
 *        trying to run the serial port in polled mode.
 *
 * For kernel version 2.1.xx the cli() actually gets a spin lock so
 * it is always necessary to do a restore_flags before returning
 * so as to let go of that lock.
 */
int handle_exception(int exceptionVector,
		     int signo,
		     int err_code,
		     struct pt_regs *linux_regs)
{
	struct task_struct	*usethread = NULL;
	int			addr, length;
	int			breakno, breaktype;
	char			*ptr;
	int			newPC;
	unsigned long		flags;
	int 			gdb_regs[NUMREGBYTES/4];
	int			i;
	int			dr6;
	int			ret;
	int			maxbpt, bpt_done;
	unsigned long 		bptaddr;
	struct pt_regs		tempregs;
#ifdef CONFIG_KGDB_THREAD
	int			nothreads;
	int			maxthreads;
	int			threadid;
	threadref		thref;
	struct task_struct	*thread = NULL;
#endif
	unsigned		procid;

#define	regs	(*linux_regs)

	/*
	 * If the entry is not from the kernel then return to the Linux
	 * trap handler and let it process the interrupt normally.
	 */
	if ((linux_regs->eflags & VM_MASK) || (3 & linux_regs->xcs)) {
		return(0);
	}

	if (kgdb_memerr_expected)
	{
		/*
		 * This fault occured because of the get_char or set_char
		 * routines.  These two routines use either eax of edx to
		 * indirectly reference the location in memory that they
		 * are working with.  For a page fault, when we return
		 * the instruction will be retried, so we have to make
		 * sure that these registers point to valid memory.
		 */
		kgdb_memerr = 1 ;		/* set mem error flag */
		kgdb_memerr_expected = 0 ;
		kgdb_memerr_cnt++ ;	/* helps in debugging */
		regs.eax = (long) &garbage_loc ;	/* make valid address */
		regs.edx = (long) &garbage_loc ;	/* make valid address */
		return(0) ;
	}

	/* Hold kgdb_lock */
	procid = smp_processor_id();
	while (cmpxchg(&kgdb_lock, 0, (procid + 1)) != 0) {
		int i = 25;	/* an arbitrary number */

		while (--i)
			asm volatile ("nop": : : "memory");
	}

	kgdb_step = 0;

	local_irq_save(flags);

	/* Disable hardware debugging while we are in kgdb */
	__asm__("movl %0,%%db7"
		: /* no output */
		: "r" (0));

	for (i = 0; i < smp_num_cpus; i++) {
		spin_lock(&slavecpulocks[i]);
	}

	/* spin_lock code is good enough as a barrier so we don't
	 * need one here */
	procindebug[smp_processor_id()] = 1;

	/* Master processor is completely in the debugger */

	gdb_i386vector  = exceptionVector;
	gdb_i386errcode = err_code ;

	/* reply to host that an exception has occurred */
	remcomOutBuffer[0] = 'S';
	remcomOutBuffer[1] =  hexchars[signo >> 4];
	remcomOutBuffer[2] =  hexchars[signo % 16];
	remcomOutBuffer[3] = 0;

	putpacket(remcomOutBuffer);

	while (1==1) {
		error = 0;
		remcomOutBuffer[0] = 0;
		getpacket(remcomInBuffer);
		switch (remcomInBuffer[0]) {
		case '?' : 
			remcomOutBuffer[0] = 'S';
			remcomOutBuffer[1] =  hexchars[signo >> 4];
			remcomOutBuffer[2] =  hexchars[signo % 16];
			remcomOutBuffer[3] = 0;
			break;
		case 'g' : /* return the value of the CPU registers */
			if (!usethread || usethread == current) {
				regs_to_gdb_regs(gdb_regs, &regs) ;
			} else {
				memset(gdb_regs, 0, NUMREGBYTES);
				if (usethread->thread.kgdbregs) {
					kgdb_memerr_expected = 1 ;
					kgdb_memerr = 0;
					get_char((char *)usethread->thread.kgdbregs);
					kgdb_memerr_expected = 0;
					if (kgdb_memerr) {
						*(((char *)&tempregs) + i) = '\0';
						gdb_regs[_PC] = kgdb_usercode;
					} else {
						regs_to_gdb_regs(gdb_regs,
							usethread->thread.kgdbregs);
					}
				} else {
					gdb_regs[_PC] = kgdb_usercode;
				}
			}
			mem2hex((char*) gdb_regs, remcomOutBuffer, NUMREGBYTES, 0);
			break;
		case 'G' : /* set the value of the CPU registers - return OK */
			hex2mem(&remcomInBuffer[1], (char*) gdb_regs, NUMREGBYTES, 0);
			if (!usethread || usethread == current) {
				gdb_regs_to_regs(gdb_regs, &regs) ;
				strcpy(remcomOutBuffer,"OK");
			} else {
				strcpy(remcomOutBuffer,"E00");
			}
			break;

		/* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
		case 'm' :
			/* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
			ptr = &remcomInBuffer[1];
			if (hexToInt(&ptr,&addr))
			if (*(ptr++) == ',')
			if (hexToInt(&ptr,&length))
			{
				ptr = 0;
				mem2hex((char*) addr, remcomOutBuffer, length, 1);
				if (kgdb_memerr) {
					strcpy (remcomOutBuffer, "E03");
				}
			}

			if (ptr)
			{
				strcpy(remcomOutBuffer,"E01");
			}
			break;

	      /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	      case 'M' :
				/* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
				ptr = &remcomInBuffer[1];
				if (hexToInt(&ptr,&addr))
				if (*(ptr++) == ',')
					if (hexToInt(&ptr,&length))
						if (*(ptr++) == ':')
						{
							hex2mem(ptr, (char*) addr, length, 1);

							if (kgdb_memerr) {
								strcpy (remcomOutBuffer, "E03");
							} else {
								strcpy(remcomOutBuffer,"OK");
							}

							ptr = 0;
						}
				if (ptr)
				{
					strcpy(remcomOutBuffer,"E02");
				}
				break;

	     /* cAA..AA    Continue at address AA..AA(optional) */
	     /* sAA..AA   Step one instruction from AA..AA(optional) */
	     /* D	  Detach from debugger */
	     case 'c' :
	     case 's' :
	     case 'D' :

			/* try to read optional parameter, pc unchanged if no parm */
			ptr = &remcomInBuffer[1];
			if (hexToInt(&ptr,&addr))
			{
				regs.eip = addr;
			}

			newPC = regs.eip ;

			/* clear the trace bit */
			regs.eflags &= 0xfffffeff;

			/* set the trace bit if we're stepping */
			if (remcomInBuffer[0] == 's') {
				regs.eflags |= 0x100;
				kgdb_step = 1;
			}
			if (remcomInBuffer[0] == 'D') {
				clear_disabled_break();
				strcpy(remcomOutBuffer,"+");
				putpacket(remcomOutBuffer);
			}

			asm volatile ("movl %%db6, %0\n"
				: "=r" (dr6)
				: );
			if (!(dr6 & 0x4000)) {
				for (breakno = 0; breakno < 4; ++breakno) {
					if (dr6 & (1 << breakno)) {
						if (breakinfo[breakno].type == 0) {
							/* Set restore flag */
							regs.eflags |= 0x10000;
							break;
						}
					}
				}
			}
			correct_hw_break();
			asm volatile ( 
				"movl %0, %%db6\n"
				: 
				: "r" (0) );

			procindebug[smp_processor_id()] = 0;

			/* Signal the slave processors to quit from
			 * the debugger */
			for (i = 0; i < smp_num_cpus; i++) {
				spin_unlock(&slavecpulocks[i]);
			}

			/* Wait till all the processors have quit
			 * from the debugger */

			for (i = 0; i < smp_num_cpus; i++) {
				while (procindebug[i]) {
					int j = 10; /* an arbitrary number */

					while (--j) {
						asm volatile ("nop"
							: : : "memory");
					}
				}
			}

			/* Release kgdb_lock */
			asm volatile (
#ifndef CONFIG_X86_OOSTORE
			"movb $0,%0"
#else
			"lock movb $0,%0"
#endif
			: "=m" (kgdb_lock) : : "memory");

			local_irq_restore(flags) ;
			return(0) ;

		/* kill the program */
		case 'k' :  /* do nothing */
		break;

		/* query */
		case 'q' :
			switch (remcomInBuffer[1]) {
#ifdef CONFIG_KGDB_THREAD
			case 'L':	
				/* List threads */
				unpack_byte(remcomInBuffer+3, &maxthreads);
				unpack_threadid(remcomInBuffer+5, &thref);

				remcomOutBuffer[0] = 'q';
				remcomOutBuffer[1] = 'M';
				remcomOutBuffer[4] = '0';
				pack_threadid(remcomOutBuffer+5, &thref);	

				threadid = threadref_to_int(&thref);
				for (nothreads = 0;
					nothreads < maxthreads && threadid< PID_MAX;
					threadid++ )
				{
					read_lock(&tasklist_lock);
					thread = getthread(threadid);
					read_unlock(&tasklist_lock);
					if (thread) {
						int_to_threadref(&thref, threadid);
						pack_threadid(remcomOutBuffer+21+nothreads*16,
							      &thref);
						nothreads++;
					}
				}
				if (threadid == PID_MAX) {
					remcomOutBuffer[4] = '1';
				}
				pack_hex_byte(remcomOutBuffer+2, nothreads);
				remcomOutBuffer[21+nothreads*16] = '\0';
				break;

			case 'C':
				/* Current thread id */
				remcomOutBuffer[0] = 'Q';
				remcomOutBuffer[1] = 'C';
				threadid = current->pid;
				int_to_threadref(&thref, threadid);
				pack_threadid(remcomOutBuffer+2, &thref);	
				remcomOutBuffer[18] = '\0';
				break;
#endif

			case 'E':
				/* Print exception info */
				printexceptioninfo(exceptionVector, err_code, remcomOutBuffer);
				break;
			}
			break;

#ifdef CONFIG_KGDB_THREAD
		/* task related */
		case 'H' :	 
			switch (remcomInBuffer[1]) {
				case 'g':
					ptr = &remcomInBuffer[2];
					hexToInt(&ptr, &threadid);
					thread = getthread(threadid);
					if (!thread) {
						remcomOutBuffer[0] = 'E';
						remcomOutBuffer[1] = '\0';
						break;
					}
					usethread = thread;
					/* follow through */
				case 'c':
					remcomOutBuffer[0] = 'O';
					remcomOutBuffer[1] = 'K';
					remcomOutBuffer[2] = '\0';
				break;
			}
			break;

		/* Query thread status */
		case 'T':
			ptr = &remcomInBuffer[1];
			hexToInt(&ptr, &threadid);
			thread = getthread(threadid);
			if (thread) {
				remcomOutBuffer[0] = 'O';
				remcomOutBuffer[1] = 'K';
				remcomOutBuffer[2] = '\0';
			} else {
				remcomOutBuffer[0] = 'E';
				remcomOutBuffer[1] = '\0';
			}
			break;
#endif

		case 'Y':
			ptr = &remcomInBuffer[1];
			hexToInt(&ptr, &breakno);
			ptr++;
			hexToInt(&ptr, &breaktype);
			ptr++;
			hexToInt(&ptr, &length);
			ptr++;
			hexToInt(&ptr, &addr);
			if (set_hw_break(breakno & 0x3, breaktype & 0x3 , length & 0x3, addr)
				== 0) {
				strcpy(remcomOutBuffer, "OK");
			} else {
				strcpy(remcomOutBuffer, "ERROR");
			}
			break;

		/* Remove hardware breakpoint */      
		case 'y':
			ptr = &remcomInBuffer[1];
			hexToInt(&ptr, &breakno);
			if (remove_hw_break(breakno & 0x3) == 0) {
				strcpy(remcomOutBuffer, "OK");
			} else {
				strcpy(remcomOutBuffer, "ERROR");
			}
			break;
		case 'Z':
		case 'z':
			ptr = &remcomInBuffer[1];
			if (*ptr++ != bp_breakpoint)
				break;
			if (*(ptr++) != ',') {
				strcpy(remcomOutBuffer, "ERROR");
				break;
			}
			hexToInt(&ptr, &addr);
			if (remcomInBuffer[0] == 'Z')
				ret = set_break(addr);
			else
				ret = remove_break(addr);

			if (ret == 0) {
				strcpy(remcomOutBuffer, "OK");
			} else {
				strcpy(remcomOutBuffer, "ERROR");
			}
			break;

		} /* switch */

		/* reply to the request */
		putpacket(remcomOutBuffer);
	}
}

/* this function is used to set up exception handlers for tracing and
   breakpoints */
void set_debug_traps(void)
{
	int i = 0;
	/*
	 * linux_debug_hook is defined in traps.c.  We store a pointer
	 * to our own exception handler into it.
	 */
	linux_debug_hook = handle_exception ;

	for (i=0; i<MAX_BREAKPOINTS; i++) {
			kgdb_break[i].bpt_addr = 0;
			kgdb_break[i].state = bp_disabled;
	}

	/*
	 * In case GDB is started before us, ack any packets (presumably
	 * "$?#xx") sitting there.  */
	putDebugChar ('+');

	initialized = 1;
}

/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger. */

void breakpoint(void)
{
	if (initialized)
		BREAKPOINT();
}

#ifdef CONFIG_GDB_CONSOLE
char	gdbconbuf[BUFMAX];

void gdb_console_write(struct console *co, const char *s,
				unsigned count)
{
	int	i;
	int	wcount;
	char	*bufptr;

	if (!gdb_initialized) {
		return;
	}
	gdbconbuf[0] = 'O';
	bufptr = gdbconbuf + 1;
	while (count > 0) {
		if ((count << 1) > (BUFMAX - 2)) {
			wcount = (BUFMAX - 2) >> 1;
		} else {
			wcount = count;
		}
		count -= wcount;
		for (i = 0; i < wcount; i++) {
			bufptr = pack_hex_byte(bufptr, s[i]);
		}
		*bufptr = '\0';
		s += wcount;

		putpacket(gdbconbuf);

	}
}
#endif
static int __init kgdb_opt_gdb(void)
{
	gdb_enter = 1;
	return 1;
}
static int __init kgdb_opt_gdbttyS(char *str)
{
	gdb_ttyS = simple_strtoul(str,NULL,10);
	return 1;
}
static int __init kgdb_opt_gdbbaud(char *str)
{
	gdb_baud = simple_strtoul(str,NULL,10);
	return 1;
}

/*
 * Sequence of these lines has to be maintained because gdb option is a prefix
 * of the other two options
 */

__setup("gdbttyS=", kgdb_opt_gdbttyS);
__setup("gdbbaud=", kgdb_opt_gdbbaud);
__setup("gdb", kgdb_opt_gdb);
