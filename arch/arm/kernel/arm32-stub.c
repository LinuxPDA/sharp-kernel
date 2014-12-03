/*
 * arch/arm/kernel/arm32-stub.c -- stub for GDB remote debugging protocol
 *
 * Originally written by Glenn Engel, Lake Stevens Instrumental Division, HP
 * Contributed by HP Systems
 *
 * Modified for usermode ARM debugging by Steve Longerbeam, Communica Inc.
 *
 * Modified for ARMlinux on Psion Series 5 machines by Noel Cragg
 * <noel@red-bean.com>
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL  Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   (signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
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
 * <checksum>    :: <two hex digits computed as mod 256 sum of <packet info>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/pgalloc.h>
#if 0
#include <asm/assembler.h>	/* for I_BIT -- wrong somehow? */
#else
#undef I_BIT
#define I_BIT (1 << 27)
#undef F_BIT
#define F_BIT (1 << 26)
#endif

/* write and read single characters (this code currently lives in
   drivers/char/serial_7110.c) */
extern int putDebugChar (char c);
extern int getDebugChar (void);

/* BUFMAX defines the maximum number of characters in inbound/outbound
   buffers -- at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400

static char initialized  = 0;	/* nonzero means we've been initialized */

static int remote_debug = 0;	/* nonzero shows protocol debug messages */

static int check_fault = 0;	/* flag to check for a memory fault */

static volatile int mem_err = 0; /* indicate mem error for mem2hex/hex2mem */

static const char hexchars[] = "0123456789abcdef";

/* saved user-mode registers, in the order GDB expects them */
struct
{
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  int r8;
  int r9;
  int sl;
  int fp;
  int ip;
  int sp;
  int lr;
  int pc;
  unsigned char f[8][12];
  int fps;
  int ps;
}
__attribute__ ((packed)) user_regs;

#define NUMREGBYTES sizeof(user_regs)

#define flush_i_cache() asm(" mcr p15, 0, r0, c7, c5, 0")

void breakinst(void);
#define BREAKPOINT() \
	asm(".globl breakinst"); \
        asm("breakinst: .word 0xe7ffdefe");


/**********************************************************************/
/**********************************************************************/


/* return the int value of a hex character */

int
hex (char ch)
{
  if ((ch >= 'a') && (ch <= 'f'))
    return (ch - 'a' + 10);
  if ((ch >= '0') && (ch <= '9'))
    return (ch - '0');
  if ((ch >= 'A') && (ch <= 'F'))
    return (ch - 'A' + 10);
  return (-1);
}


/* get a packet */

void
getpacket (char *buffer)
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int i;
  int count;
  char ch;

  if (remote_debug)
    printk ("getpacket");

  do
    {
      /* scan for the sequence $<data>#<checksum> */

      if (remote_debug)
	printk (":");
      /* wait around for the start character, ignore all other characters */
      while ((ch = (getDebugChar () & 0x7f)) != '$');
      checksum = 0;
      xmitcsum = -1;

      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < BUFMAX)
	{
	  ch = getDebugChar () & 0x7f;
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}
      buffer[count] = 0;

      if (ch == '#')
	{
	  xmitcsum = hex (getDebugChar () & 0x7f) << 4;
	  xmitcsum += hex (getDebugChar () & 0x7f);
#if 0
	  if (remote_debug && (checksum != xmitcsum))
	    {
	      printk ("bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
		      checksum, xmitcsum, buffer);
	    }
#endif

	  if (checksum != xmitcsum)
	    putDebugChar ('-');	/* failed checksum */
	  else
	    {
	      putDebugChar ('+');	/* successful transfer */
	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putDebugChar (buffer[0]);
		  putDebugChar (buffer[1]);
		  /* remove sequence chars from buffer */
		  count = strlen (buffer);
		  for (i = 3; i <= count; i++)
		    buffer[i - 3] = buffer[i];
		}
	    }
	}
    }
  while (checksum != xmitcsum);

  if (remote_debug)
    printk (" $%s#%c%c\n",
	    buffer, hexchars[checksum >> 4], hexchars[checksum % 16]);
}


/* send a packet */

void
putpacket (char *buffer)
{
  unsigned char checksum;
  int count;
  char ch;

  /* $<packet info>#<checksum> */
  do
    {
      putDebugChar ('$');
      checksum = 0;
      count = 0;

      while ((ch = buffer[count]))
	{
	  putDebugChar (ch);
	  checksum += ch;
	  count += 1;
	}

      putDebugChar ('#');
      putDebugChar (hexchars[checksum >> 4]);
      putDebugChar (hexchars[checksum % 16]);

      if (remote_debug)
	printk ("putpacket: $%s#%c%c\n",
		buffer, hexchars[checksum >> 4], hexchars[checksum % 16]);

    }
  while ((getDebugChar () & 0x7f) != '+');
}

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];
static short error;


/* Convert the memory pointed to by mem into hex, placing result in
   buf.  Return a pointer to the last char put in buf (null).  If
   MAY_FAULT is non-zero, then we should set mem_err in response to a
   fault; if zero treat a fault like any other fault in the stub.  */

char *
mem2hex (char *mem, char *buf, int count, int may_fault)
{
  int i;
  unsigned char ch;

  if (may_fault)
    check_fault = 1;
  for (i = 0; i < count; i++)
    {
      ch = *mem++;		/* this may cause a fault */
      if (may_fault && mem_err)
	return (buf);
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch % 16];
    }
  *buf = 0;
  if (may_fault)
    check_fault = 0;
  return (buf);
}


/* Convert the hex array pointed to by buf into binary to be placed in
   mem.  Return a pointer to the character AFTER the last byte
   written.  */

char *
hex2mem (char *buf, char *mem, int count, int may_fault)
{
  int i;
  unsigned char ch;

  if (may_fault)
    check_fault = 1;
  for (i = 0; i < count; i++)
    {
      ch = hex (*buf++) << 4;
      ch = ch + hex (*buf++);
      *mem++ = ch;		/* this may cause a fault */
      if (may_fault && mem_err)
	return (mem);
    }
  if (may_fault)
    check_fault = 0;
  return (mem);
}


/* convert an ascii hex representation to an int */

int
hexToInt (char **ptr, int *intValue)
{
  int numChars = 0;
  int hexValue;

  *intValue = 0;

  while (**ptr)
    {
      hexValue = hex (**ptr);
      if (hexValue >= 0)
	{
	  *intValue = (*intValue << 4) | hexValue;
	  numChars++;
	}
      else
	break;

      (*ptr)++;
    }

  return (numChars);
}


/* Functions to tweak the SPSR. */

static inline int
get_psr (void)
{
  int psr;
  asm volatile ("mrs %0, cpsr"
		: "=r" (psr)
		: /* no inputs */ );
  return psr;
}

static inline void
set_psr (int psr)
{
  asm volatile ("msr cpsr, %0"
		: /* no outputs */
		: "r" (psr));
}


/* This function does all command processing for interfacing to gdb. */

void
handle_exception (int sigval, struct pt_regs *regs)
{
  static int in_gdb = 0;
  int addr, length;
  unsigned long psr;
  char *ptr;

//  flush_cache_all();

  if (remote_debug)
    printk ("handle_exception(%d)\n", sigval);

  if (check_fault)
    {
      /* We must have tried to set or get a memory location that
         triggered a fault.  Set the error flag, increment the PC past
         the bad instruction, and return to the stub. */

      mem_err = 1;

      /* Speaking of incrementing the PC here, I'm not quite sure why
         I should have to do it, since the processor is supposed to
         set the R14_svc to the bad instruction + 4.  I guess there's
         something going on here which I haven't tracked down yet.

         For example, if I examine a memory location which isn't
         mapped by the MMU, a data access exception is raised and this
         function gets called.  The PC is at the instruction which is
         trying to dereference the memory location.  I expect that
         there's some other code somewhere which munges the PC before
         we see it, because in most cases you'll want to get the
         fault, swap in the memory, and try again, no?  */

      regs->ARM_pc += 4;
      return;
    }

  if (in_gdb++)
    {
      printk ("handle_exception called recursively -- returning...\n");
      return;
    }

  /* By default, the processor will increment the PC by 4 when an
     exception occurs.  Most of the time we don't want that, since we
     want the PC to point to the location of the breakpoint (or we'll
     confuse GDB). */
  if (regs->ARM_pc == (unsigned long)breakinst) {
    regs->ARM_pc += 4;
  }
#if 0
  else {
    regs->ARM_pc -= 4;
  }
#endif

  if (remote_debug)
    show_regs (regs);

  /* disable interrupts */
  psr = get_psr ();
  set_psr (psr | I_BIT | F_BIT);

  if (remote_debug)
    printk ("psr before=%08lx after=%08lx\n", psr, psr | I_BIT | F_BIT);

  /* Copy the stuff in REGS to USER_REGS (that is, the registers
     stored in the order in which GDB expects for the 'g' command).
     (In supervisor mode we don't care about floating point registers
     since the kernel doesn't use floating point math.  We're skipping
     those registers, therefore.) */

  user_regs.r0 = (int) regs->ARM_r0;
  user_regs.r1 = (int) regs->ARM_r1;
  user_regs.r2 = (int) regs->ARM_r2;
  user_regs.r3 = (int) regs->ARM_r3;
  user_regs.r4 = (int) regs->ARM_r4;
  user_regs.r5 = (int) regs->ARM_r5;
  user_regs.r6 = (int) regs->ARM_r6;
  user_regs.r7 = (int) regs->ARM_r7;
  user_regs.r8 = (int) regs->ARM_r8;
  user_regs.r9 = (int) regs->ARM_r9;
  user_regs.sl = (int) regs->ARM_r10;
  user_regs.fp = (int) regs->ARM_fp;
  user_regs.ip = (int) regs->ARM_ip;
  user_regs.sp = (int) regs->ARM_sp;
  user_regs.lr = (int) regs->ARM_lr;
  user_regs.pc = (int) regs->ARM_pc;
  user_regs.ps = (int) regs->ARM_cpsr;

  /* reply to host that an exception has occurred */
#if 0
  /* old style */
  remcomOutBuffer[0] = 'S';
  remcomOutBuffer[1] = hexchars[sigval >> 4];
  remcomOutBuffer[2] = hexchars[sigval % 16];
  remcomOutBuffer[3] = 0;
  putpacket (remcomOutBuffer);
#else
  /* new style -- faster because GDB doesn't have to ask for
     additional registers when stepping */
  ptr = remcomOutBuffer;

  /* Send trap type (converted to signal) */
  *ptr++ = 'T';
  *ptr++ = hexchars[sigval >> 4];
  *ptr++ = hexchars[sigval & 0xf];

  /* order of registers sent to GDB... */
#define GDBREG_FP (11)
#define GDBREG_IP (12)
#define GDBREG_SP (13)
#define GDBREG_LR (14)
#define GDBREG_PC (15)
#define GDBREG_PS (25)

  /* PC */
  *ptr++ = hexchars[GDBREG_PC >> 4];
  *ptr++ = hexchars[GDBREG_PC & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((char *) &user_regs.pc, ptr, 4, 0);
  *ptr++ = ';';

  /* FP */
  *ptr++ = hexchars[GDBREG_FP >> 4];
  *ptr++ = hexchars[GDBREG_FP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((char *) &user_regs.fp, ptr, 4, 0);
  *ptr++ = ';';

  /* SP */
  *ptr++ = hexchars[GDBREG_SP >> 4];
  *ptr++ = hexchars[GDBREG_SP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((char *) &user_regs.sp, ptr, 4, 0);
  *ptr++ = ';';

  /* PS */
  *ptr++ = hexchars[GDBREG_PS >> 4];
  *ptr++ = hexchars[GDBREG_PS & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((char *) &user_regs.ps, ptr, 4, 0);
  *ptr++ = ';';
  *ptr++ = 0;
  putpacket (remcomOutBuffer);	/* send it off... */
#endif
  while (1)
    {
      error = 0;
      remcomOutBuffer[0] = 0;
      getpacket (remcomInBuffer);
      switch (remcomInBuffer[0])
	{
	case '?':
	  /* print last signal */
	  remcomOutBuffer[0] = 'S';
	  remcomOutBuffer[1] = hexchars[sigval >> 4];
	  remcomOutBuffer[2] = hexchars[sigval % 16];
	  remcomOutBuffer[3] = 0;
	  break;

	case 'd':
	  /* toggle debug */
	  remote_debug = !(remote_debug);
	  break;

	case 'g':
	  /* read registers */
	  mem2hex ((char *) &user_regs, remcomOutBuffer, NUMREGBYTES, 0);
	  break;

	case 'G':
	  /* set registers */
	  hex2mem (&remcomInBuffer[1], (char *) &user_regs, NUMREGBYTES, 0);

	  regs->ARM_r0 = (unsigned long) user_regs.r0;
	  regs->ARM_r1 = (unsigned long) user_regs.r1;
	  regs->ARM_r2 = (unsigned long) user_regs.r2;
	  regs->ARM_r3 = (unsigned long) user_regs.r3;
	  regs->ARM_r4 = (unsigned long) user_regs.r4;
	  regs->ARM_r5 = (unsigned long) user_regs.r5;
	  regs->ARM_r6 = (unsigned long) user_regs.r6;
	  regs->ARM_r7 = (unsigned long) user_regs.r7;
	  regs->ARM_r8 = (unsigned long) user_regs.r8;
	  regs->ARM_r9 = (unsigned long) user_regs.r9;
	  regs->ARM_r10 = (unsigned long) user_regs.sl;
	  regs->ARM_fp = (unsigned long) user_regs.fp;
	  regs->ARM_ip = (unsigned long) user_regs.ip;
	  regs->ARM_sp = (unsigned long) user_regs.sp;
	  regs->ARM_lr = (unsigned long) user_regs.lr;
	  regs->ARM_pc = (unsigned long) user_regs.pc;
	  regs->ARM_cpsr = (unsigned long) user_regs.ps;

	  strcpy (remcomOutBuffer, "OK");
	  break;

	case 'm':
	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	  /* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
	  ptr = &remcomInBuffer[1];
	  if (hexToInt (&ptr, &addr))
	    if (*(ptr++) == ',')
	      if (hexToInt (&ptr, &length))
		{
		  ptr = 0;
		  mem_err = 0;
		  mem2hex ((char *) addr, remcomOutBuffer, length, 1);
		  if (mem_err)
		    {
		      strcpy (remcomOutBuffer, "E03");
		      if (remote_debug)
			printk ("memory fault");
		    }
		}

	  if (ptr)
	    {
	      strcpy (remcomOutBuffer, "E01");
	      if (remote_debug)
		printk ("malformed read memory command: %s", remcomInBuffer);
	    }
	  break;

	case 'M':
	  /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	  /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
	  ptr = &remcomInBuffer[1];
	  if (hexToInt (&ptr, &addr))
	    if (*(ptr++) == ',')
	      if (hexToInt (&ptr, &length))
		if (*(ptr++) == ':')
		  {
		    mem_err = 0;
		    hex2mem (ptr, (char *) addr, length, 1);

		    if (mem_err)
		      {
			strcpy (remcomOutBuffer, "E03");
			if (remote_debug)
			  printk ("memory fault");
		      }
		    else
		      {
			strcpy (remcomOutBuffer, "OK");
		      }

		    ptr = 0;
		  }
	  if (ptr)
	    {
	      strcpy (remcomOutBuffer, "E02");
	      if (remote_debug)
		printk ("malformed write memory command: %s", remcomInBuffer);
	    }
	  break;

	case 'k':		/* 'kill' just makes the program continue */
	case 'c':
	  /* cAA..AA    Continue from address AA..AA(optional) */
	  /* try to read optional parameter, pc unchanged if no parm */
	  ptr = &remcomInBuffer[1];
	  if (hexToInt (&ptr, &addr))
	    user_regs.pc = addr;

	  /* Need to flush the instruction cache here, as we may
	     have deposited a breakpoint, and the icache probably
	     has no way of knowing that a data ref to some location
	     may have changed something that is in the instruction
	     cache.  */
	  //asm (" mcr p15, 0, r0, c7, c5, 0");
	  flush_cache_all();

	  /* re-enable interrupts (possibly) */
	  set_psr (psr);

	  in_gdb--;
	  return;
	  break;

	default:
	  /* all other commands are unrecognized and return an empty
	     packet as per the protocol */
	  break;

	}			/* switch */

      /* reply to the request */
      putpacket (remcomOutBuffer);
    }

//  flush_cache_all();
}


/* This function is used to set up exception handlers for tracing and
   breakpoints.  In this version of the stub, there's not much here,
   since alternate code has been compiled in when the CONFIG_REMOTE_DEBUG
   symbol is defined.  */

void set_debug_traps (void)
{
  if (initialized)
    return;

  /* In case GDB is started before us, ack any packets (presumably
     "$?#xx") sitting there.  */
  putDebugChar ('+');

  initialized = 1;
}


/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger. */

void
breakpoint (void)
{
  if (initialized) {
    BREAKPOINT ();
  }
  flush_cache_all();
}
