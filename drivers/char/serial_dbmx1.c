/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/drivers/char/serial_dbmx1.c
 *
 * Based on:
 *  linux/drivers/char/serial.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 1992, 1993, 1994, 1995, 1996, 1997, 
 * 		1998, 1999  Theodore Ts'o
 *
 *  Extensively rewritten by Theodore Ts'o, 8/16/92 -- 9/14/92.  Now
 *  much more extensible to support other serial cards based on the
 *  16450/16550A UART's.  Added support for the AST FourPort and the
 *  Accent Async board.  
 *
 *  set_serial_info fixed to set the flags, custom divisor, and uart
 * 	type fields.  Fix suggested by Michael K. Johnson 12/12/92.
 *
 *  11/95: TIOCMIWAIT, TIOCGICOUNT by Angelo Haritsis <ah@doc.ic.ac.uk>
 *
 *  03/96: Modularised by Angelo Haritsis <ah@doc.ic.ac.uk>
 *
 *  rs_set_termios fixed to look also for changes of the input
 *      flags INPCK, BRKINT, PARMRK, IGNPAR and IGNBRK.
 *                                            Bernd Anhäupl 05/17/96.
 *
 *  1/97:  Extended dumb serial ports are a config option now.  
 *         Saves 4k.   Michael A. Griffith <grif@acm.org>
 * 
 *  8/97: Fix bug in rs_set_termios with RTS
 *        Stanislav V. Voronyi <stas@uanet.kharkov.ua>
 *
 *  3/98: Change the IRQ detection, use of probe_irq_o*(),
 *	  suppress TIOCSERGWILD and TIOCSERSWILD
 *	  Etienne Lorrain <etienne.lorrain@ibm.net>
 *
 *  4/98: Added changes to support the ARM architecture proposed by
 * 	  Russell King
 *
 *  5/99: Updated to include support for the XR16C850 and ST16C654
 *        uarts.  Stuart MacDonald <stuartm@connecttech.com>
 *
 *  8/99: Generalized PCI support added.  Theodore Ts'o
 * 
 *  3/00: Rid circular buffer of redundant xmit_cnt.  Fix a
 *	  few races on freeing buffers too.
 *	  Alan Modra <alan@linuxcare.com>
 *
 *  5/00: Support for the RSA-DV II/S card added.
 *	  Kiyokazu SUTO <suto@ks-and-ks.ne.jp>
 * 
 *  6/00: Remove old-style timer, use timer_list
 *        Andrew Morton <andrewm@uow.edu.au>
 *
 *  7/00: Support Timedia/Sunix/Exsys PCI cards
 *
 *  7/00: fix some returns on failure not using MOD_DEC_USE_COUNT.
 *	  Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * 10/00: add in optional software flow control for serial console.
 *	  Kanoj Sarcar <kanoj@sgi.com>  (Modified by Theodore Ts'o)
 *
 * 02/02: Fix for AMD Elan bug in transmit irq routine, by
 *        Christer Weinigel <wingel@hog.ctrl-c.liu.se>,
 *        Robert Schwebel <robert@schwebel.de>,
 *        Juergen Beisert <jbeisert@eurodsn.de>,
 *        Theodore Ts'o <tytso@mit.edu>
 */

static char *serial_version = "1.00";
static char *serial_revdate = "2003-04-08";

/*
 * Serial driver configuration section.  Here are the various options:
 *
 * SERIAL_PARANOIA_CHECK
 * 		Check the magic number for the async_structure where
 * 		ever possible.
 */

#include <linux/config.h>
#include <linux/version.h>

#ifdef CONFIG_KGDB
#include <linux/gdb.h>
#endif

#undef SERIAL_PARANOIA_CHECK
#define CONFIG_SERIAL_NOPAUSE_IO
#define SERIAL_DO_RESTART

/* Set of debugging defines */

#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
#undef SERIAL_DEBUG_PCI
#undef SERIAL_DEBUG_AUTOCONF

/* Sanity checks */

#ifdef MODULE
#undef CONFIG_SERIAL_DBMX1_CONSOLE
#endif

#define RS_STROBE_TIME (10*HZ)
#define RS_ISR_PASS_LIMIT 256

#if defined(__i386__) && (defined(CONFIG_M386) || defined(CONFIG_M486))
#define SERIAL_INLINE
#endif
  
/*
 * End of serial driver configuration section.
 */

#include <linux/module.h>

#include <linux/types.h>
#ifdef LOCAL_HEADERS
#include "serial_local.h"
#else
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <asm/arch/serial_dbmx1.h>
#define LOCAL_VERSTRING ""
#endif

#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#if (LINUX_VERSION_CODE >= 131343)
#include <linux/init.h>
#endif
#if (LINUX_VERSION_CODE >= 131336)
#include <asm/uaccess.h>
#endif
#include <linux/delay.h>
#ifdef CONFIG_SERIAL_DBMX1_CONSOLE
#include <linux/console.h>
#endif
#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif

/*
 * All of the compatibilty code so we can compile serial.c against
 * older kernels is hidden in serial_compat.h
 */
#if defined(LOCAL_HEADERS) || (LINUX_VERSION_CODE < 0x020317) /* 2.3.23 */
#include "serial_compat.h"
#endif

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>

#include "serial_dbmx1.h"

#define SERIAL_DEV_OFFSET	0

#ifdef SERIAL_INLINE
#define _INLINE_ inline
#else
#define _INLINE_
#endif

static char *serial_name = "Serial driver";

static DECLARE_TASK_QUEUE(tq_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct timer_list serial_timer;

/* serial subtype definitions */
#ifndef SERIAL_TYPE_NORMAL
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
#endif

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

/*
 * IRQ_timeout		- How long the timeout should be for each IRQ
 * 				should be after the IRQ has been active.
 */

static struct dbmx1_async_struct *IRQ_ports[NR_IRQS];
static int IRQ_timeout[NR_IRQS];
#ifdef CONFIG_SERIAL_DBMX1_CONSOLE
static struct console sercons;
static int lsr_break_flag;
#endif
#if defined(CONFIG_SERIAL_DBMX1_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
static unsigned long break_pressed; /* break, really ... */
#endif

static unsigned detect_uart_irq (struct serial_state * state);
static void autoconfig(struct serial_state * state);
static void change_speed(struct dbmx1_async_struct *info, struct termios *old);
static void rs_wait_until_sent(struct tty_struct *tty, int timeout);

#define PORT_DBMX1	1
/*
 * Here we define the default xmit fifo size used for each type of
 * UART
 */
static struct serial_uart_config uart_config[] = {
	{ "unknown", 1, 0 }, 
	{ "DBMX1", 16, 0 }, 
	{ 0, 0}
};

static struct serial_state rs_table[RS_TABLE_SIZE] = {
	SERIAL_PORT_DFNS	/* Defined in serial.h */
};

#define NR_PORTS	(sizeof(rs_table)/sizeof(struct serial_state))

#ifndef PREPARE_FUNC
#define PREPARE_FUNC(dev)  (dev->prepare)
#define ACTIVATE_FUNC(dev)  (dev->activate)
#define DEACTIVATE_FUNC(dev)  (dev->deactivate)
#endif

#define HIGH_BITS_OFFSET ((sizeof(long)-sizeof(int))*8)

static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];


#if defined(MODULE) && defined(SERIAL_DEBUG_MCOUNT)
#define DBG_CNT(s) printk("(%s): [%x] refc=%d, serc=%d, ttyc=%d -> %s\n", \
 kdevname(tty->device), (info->flags), serial_refcount,info->count,tty->count,s)
#else
#define DBG_CNT(s)
#endif

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the copy_from_user blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *tmp_buf;
#ifdef DECLARE_MUTEX
static DECLARE_MUTEX(tmp_buf_sem);
#else
static struct semaphore tmp_buf_sem = MUTEX;
#endif


static inline int serial_paranoia_check(struct dbmx1_async_struct *info,
					kdev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null async_struct for (%s) in %s\n";

	if (!info) {
		printk(badinfo, kdevname(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}

static _INLINE_ unsigned int serial_in(struct dbmx1_async_struct *info, int offset)
{
	switch (info->io_type) {
	case SERIAL_IO_MEM:
		return readb((unsigned long) info->iomem_base +
			     (offset<<info->iomem_reg_shift));
	default:
		return inl(info->port + offset);
	}
}

static _INLINE_ void serial_out(struct dbmx1_async_struct *info, int offset,
				int value)
{
	switch (info->io_type) {
	case SERIAL_IO_MEM:
		writeb(value, (unsigned long) info->iomem_base +
			      (offset<<info->iomem_reg_shift));
		break;
	default:
		outl(value, info->port+offset);
	}
}

/*
 * We used to support using pause I/O for certain machines.  We
 * haven't supported this for a while, but just in case it's badly
 * needed for certain old 386 machines, I've left these #define's
 * in....
 */
#define serial_inp(info, offset)		serial_in(info, offset)
#define serial_outp(info, offset, value)	serial_out(info, offset, value)


/*
 * ------------------------------------------------------------
 * rs_stop() and rs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void rs_stop(struct tty_struct *tty)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;
	
	save_flags(flags); cli();
	if (info->ucr1 & UCR1_TRDYEN) {
		info->ucr1 &= ~UCR1_TRDYEN;
		serial_out(info, UCR1, info->ucr1);
	}
	restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;
	
	save_flags(flags); cli();
	if (info->xmit.head != info->xmit.tail
	    && info->xmit.buf
	    && !(info->ucr1 & UCR1_TRDYEN)) {
		info->ucr1 |= UCR1_TRDYEN;
		serial_out(info, UCR1, info->ucr1);
	}
	restore_flags(flags);
}

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * rs_interrupt().  They were separated out for readability's sake.
 *
 * Note: rs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * rs_interrupt() should try to keep the interrupt handler as fast as
 * possible.  After you are done making modifications, it is not a bad
 * idea to do:
 * 
 * gcc -S -DKERNEL -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer serial.c
 *
 * and look at the resulting assemble code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static _INLINE_ void rs_sched_event(struct dbmx1_async_struct *info,
				  int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static _INLINE_ void receive_chars(struct dbmx1_async_struct *info,
				 int *status, struct pt_regs * regs)
{
	struct tty_struct *tty = info->tty;
	unsigned long ch;
	struct	async_icount *icount;
	int	i;

	icount = &info->state->icount;
	for (i = 0; i < 16; i++) {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			tty->flip.tqueue.routine((void *) tty);
			if (tty->flip.count >= TTY_FLIPBUF_SIZE)
				return;		// if TTY_DONT_FLIP is set
		}
		ch = serial_inp(info, URXDn(i));
		if (!(ch & URXDn_CHARRDY))
		    break;
		*status = ch & 0xff00;
		ch &= 0xff;
		*tty->flip.char_buf_ptr = ch;
		icount->rx++;
		
#ifdef SERIAL_DEBUG_INTR
		printk("DR%02x:%02x...", ch, *status);
#endif
		*tty->flip.flag_buf_ptr = 0;
		if (*status & URXDn_ERR) {
			/*
			 * For statistics only
			 */
			if (*status & URXDn_BRK) {
				*status &= ~(URXDn_FRMERR | URXDn_PRERR);
				icount->brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
#if defined(CONFIG_SERIAL_DBMX1_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
				if (info->line == sercons.index) {
					if (!break_pressed) {
						break_pressed = jiffies;
						goto ignore_char;
					}
					break_pressed = 0;
				}
#endif
				if (info->flags & ASYNC_SAK)
					do_SAK(tty);
			} else if (*status & URXDn_PRERR)
				icount->parity++;
			else if (*status & URXDn_FRMERR)
				icount->frame++;
			if (*status & URXDn_OVRRUN)
				icount->overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			*status &= info->read_status_mask;

#ifdef CONFIG_SERIAL_DBMX1_CONSOLE
			if (info->line == sercons.index) {
				/* Recover the break flag from console xmit */
				*status |= lsr_break_flag;
				lsr_break_flag = 0;
			}
#endif
			if (*status & (URXDn_BRK)) {
#ifdef SERIAL_DEBUG_INTR
				printk("handling break....");
#endif
				*tty->flip.flag_buf_ptr = TTY_BREAK;
			} else if (*status & URXDn_PRERR)
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			else if (*status & URXDn_FRMERR)
				*tty->flip.flag_buf_ptr = TTY_FRAME;
		}
#if defined(CONFIG_SERIAL_DBMX1_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
		if (break_pressed && info->line == sercons.index) {
			if (ch != 0 &&
			    time_before(jiffies, break_pressed + HZ*5)) {
				handle_sysrq(ch, regs, NULL, NULL);
				break_pressed = 0;
				goto ignore_char;
			}
			break_pressed = 0;
		}
#endif
		if ((*status & info->ignore_status_mask) == 0) {
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if ((*status & URXDn_OVRRUN) &&
		    (tty->flip.count < TTY_FLIPBUF_SIZE)) {
			/*
			 * Overrun is special, since it's reported
			 * immediately, and doesn't affect the current
			 * character
			 */
			*tty->flip.flag_buf_ptr = TTY_OVERRUN;
			tty->flip.count++;
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
		}
#if defined(CONFIG_SERIAL_DBMX1_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
	ignore_char:
#endif
	}
#if (LINUX_VERSION_CODE > 131394) /* 2.1.66 */
	tty_flip_buffer_push(tty);
#else
	queue_task_irq_off(&tty->flip.tqueue, &tq_timer);
#endif	
}

static _INLINE_ void transmit_chars(struct dbmx1_async_struct *info, int *intr_done)
{
	int count;

	if (info->x_char) {
		serial_outp(info, UTXnD(0), info->x_char);
		info->state->icount.tx++;
		info->x_char = 0;
		if (intr_done)
			*intr_done = 0;
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
		info->ucr1 &= ~UCR1_TRDYEN;
		serial_out(info, UCR1, info->ucr1);
		return;
	}
	
	for (count = 0; count < info->xmit_fifo_size; count++) {
		serial_out(info, UTXnD(count), info->xmit.buf[info->xmit.tail]);
		info->xmit.tail = (info->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
		info->state->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	}
	
	if (CIRC_CNT(info->xmit.head,
		     info->xmit.tail,
		     SERIAL_XMIT_SIZE) < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

#ifdef SERIAL_DEBUG_INTR
	printk("THRE...");
#endif
	if (intr_done)
		*intr_done = 0;

	if (info->xmit.head == info->xmit.tail) {
		info->ucr1 &= ~UCR1_TRDYEN;
		serial_out(info, UCR1, info->ucr1);
	}
}

static _INLINE_ void check_modem_status(struct dbmx1_async_struct *info)
{
	int	status;
	struct	async_icount *icount;
	
#ifndef CONFIG_DBMX1_TPM102
	status = serial_in(info, UART_MSR);

	if (status & UART_MSR_ANY_DELTA) {
		icount = &info->state->icount;
		/* update input line counters */
		if (status & UART_MSR_TERI)
			icount->rng++;
		if (status & UART_MSR_DDSR)
			icount->dsr++;
		if (status & UART_MSR_DDCD) {
			icount->dcd++;
#ifdef CONFIG_HARD_PPS
			if ((info->flags & ASYNC_HARDPPS_CD) &&
			    (status & UART_MSR_DCD))
				hardpps();
#endif
		}
		if (status & UART_MSR_DCTS)
			icount->cts++;
		wake_up_interruptible(&info->delta_msr_wait);
	}

	if ((info->flags & ASYNC_CHECK_CD) && (status & UART_MSR_DDCD)) {
#if (defined(SERIAL_DEBUG_OPEN) || defined(SERIAL_DEBUG_INTR))
		printk("ttys%d CD now %s...", info->line,
		       (status & UART_MSR_DCD) ? "on" : "off");
#endif		
		if (status & UART_MSR_DCD)
			wake_up_interruptible(&info->open_wait);
		else if (!((info->flags & ASYNC_CALLOUT_ACTIVE) &&
			   (info->flags & ASYNC_CALLOUT_NOHUP))) {
#ifdef SERIAL_DEBUG_OPEN
			printk("doing serial hangup...");
#endif
			if (info->tty)
				tty_hangup(info->tty);
		}
	}
	if (info->flags & ASYNC_CTS_FLOW) {
		if (info->tty->hw_stopped) {
			if (status & UART_MSR_CTS) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx start...");
#endif
				info->tty->hw_stopped = 0;
				info->ucr1 |= UCR1_TRDYEN;
				serial_out(info, UCR1, info->ucr1);
				rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
				return;
			}
		} else {
			if (!(status & UART_MSR_CTS)) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
				printk("CTS tx stop...");
#endif
				info->tty->hw_stopped = 1;
				info->ucr1 &= ~UCR1_TRDYEN;
				serial_out(info, UCR1, info->ucr1);
			}
		}
	}
#endif // CONFIG_DBMX1_TPM102
}


/*
 * This is the serial driver's interrupt routine for a single port
 */
static void rs_interrupt_single(int irq, void *dev_id, struct pt_regs * regs)
{
	int status, usr2;
	int pass_counter = 0;
	struct dbmx1_async_struct * info;
	
#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt_single(%d)...", irq);
#endif

	info = IRQ_ports[irq];
	if (!info || !info->tty)
		return;

	usr2 = serial_in(info, USR2);
	do {
		status = serial_inp(info, USR1);
#ifdef SERIAL_DEBUG_INTR
		printk("status = %x...", status);
#endif
		if (status & USR1_RRDY)
			receive_chars(info, &status, regs);
		check_modem_status(info);
		if (pass_counter++ > RS_ISR_PASS_LIMIT) {
#if SERIAL_DEBUG_INTR
			printk("rs_single loop break.\n");
#endif
			break;
		}
		usr2 = serial_in(info, USR2);
#ifdef SERIAL_DEBUG_INTR
		printk("IIR = %x...", iir);
#endif
	} while (usr2 & USR2_RDR);
	info->last_active = jiffies;
#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}

static void rs_interrupt_tx(int irq, void *dev_id, struct pt_regs * regs)
{
	int status;
	struct dbmx1_async_struct * info;

	info = IRQ_ports[irq + 1];
	if (!info || !info->tty)
		return;
	status = serial_inp(info, USR1);
	if (status & USR1_TRDY)
		transmit_chars(info, 0);
}

/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * rs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using rs_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct dbmx1_async_struct	*info = (struct dbmx1_async_struct *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
#ifdef SERIAL_HAVE_POLL_WAIT
		wake_up_interruptible(&tty->poll_wait);
#endif
	}
}

/*
 * This subroutine is called when the RS_TIMER goes off.  It is used
 * by the serial driver to handle ports that do not have an interrupt
 * (irq=0).  This doesn't work very well for 16450's, but gives barely
 * passable results for a 16550A.  (Although at the expense of much
 * CPU overhead).
 */
static void rs_timer(unsigned long dummy)
{
	static unsigned long last_strobe;
	struct dbmx1_async_struct *info;
	unsigned int	i;
	unsigned long flags;

	if ((jiffies - last_strobe) >= RS_STROBE_TIME) {
		for (i=0; i < NR_IRQS; i++) {
			info = IRQ_ports[i];
			if (!info)
				continue;
			save_flags(flags); cli();
			rs_interrupt_single(i, NULL, NULL);
			restore_flags(flags);
		}
	}
	last_strobe = jiffies;
	mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);

	if (IRQ_ports[0]) {
		save_flags(flags); cli();
		rs_interrupt_single(0, NULL, NULL);
		restore_flags(flags);

		mod_timer(&serial_timer, jiffies + IRQ_timeout[0]);
	}
}

/*
 * ---------------------------------------------------------------
 * Low level utility subroutines for the serial driver:  routines to
 * figure out the appropriate timeout for an interrupt chain, routines
 * to initialize and startup a serial port, and routines to shutdown a
 * serial port.  Useful stuff like that.
 * ---------------------------------------------------------------
 */

/*
 * This routine figures out the correct timeout for a particular IRQ.
 * It uses the smallest timeout of all of the serial ports in a
 * particular interrupt chain.  Now only used for IRQ 0....
 */
static void figure_IRQ_timeout(int irq)
{
	struct	dbmx1_async_struct	*info;
	int	timeout = 60*HZ;	/* 60 seconds === a long time :-) */

	info = IRQ_ports[irq];
	if (!info) {
		IRQ_timeout[irq] = 60*HZ;
		return;
	}
	while (info) {
		if (info->timeout < timeout)
			timeout = info->timeout;
		info = info->next_port;
	}
	if (!irq)
		timeout = timeout / 2;
	IRQ_timeout[irq] = (timeout > 3) ? timeout-2 : 1;
}

static int startup(struct dbmx1_async_struct * info)
{
	unsigned long flags;
	int	retval=0;
	void (*handler)(int, void *, struct pt_regs *);
	struct serial_state *state= info->state;
	unsigned long page;

	page = get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	save_flags(flags); cli();

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (!CONFIGURED_SERIAL_PORT(state) || !state->type) {
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		goto errout;
	}
	if (info->xmit.buf)
		free_page(page);
	else
		info->xmit.buf = (unsigned char *) page;

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttys%d (irq %d)...", info->line, state->irq);
#endif

#ifndef CONFIG_DBMX1_TPM102
	if (uart_config[state->type].flags & UART_STARTECH) {
		/* Wake up UART */
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, UART_EFR_ECB);
		/*
		 * Turn off LCR == 0xBF so we actually set the IER
		 * register on the XR16C850
		 */
		serial_outp(info, UART_LCR, 0);
		serial_outp(info, UCR1, 0);
		/*
		 * Now reset LCR so we can turn off the ECB bit
		 */
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, 0);
		/*
		 * For a XR16C850, we need to set the trigger levels
		 */
		if (state->type == PORT_16850) {
			serial_outp(info, UART_FCTR, UART_FCTR_TRGD |
					UART_FCTR_RX);
			serial_outp(info, UART_TRG, UART_TRG_96);
			serial_outp(info, UART_FCTR, UART_FCTR_TRGD |
					UART_FCTR_TX);
			serial_outp(info, UART_TRG, UART_TRG_96);
		}
		serial_outp(info, UART_LCR, 0);
	}

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */
	if (uart_config[state->type].flags & UART_CLEAR_FIFO) {
		serial_outp(info, UART_FCR, UART_FCR_ENABLE_FIFO);
		serial_outp(info, UART_FCR, (UART_FCR_ENABLE_FIFO |
					     UART_FCR_CLEAR_RCVR |
					     UART_FCR_CLEAR_XMIT));
		serial_outp(info, UART_FCR, 0);
	}

	/*
	 * Clear the interrupt registers.
	 */
	(void) serial_inp(info, UART_LSR);
	(void) serial_inp(info, UART_RX);
	(void) serial_inp(info, UART_IIR);
	(void) serial_inp(info, UART_MSR);

	/*
	 * At this point there's no way the LSR could still be 0xFF;
	 * if it is, then bail out, because there's likely no UART
	 * here.
	 */
	if (!(info->flags & ASYNC_BUGGY_UART) &&
	    (serial_inp(info, UART_LSR) == 0xff)) {
		printk("ttyS%d: LSR safety check engaged!\n", state->line);
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR, &info->tty->flags);
		} else
			retval = -ENODEV;
		goto errout;
	}
#endif // CONFIG_DBMX1_TPM102
	
	/*
	 * Allocate the IRQ if necessary
	 */
	if (state->irq && (!IRQ_ports[state->irq] ||
			  !IRQ_ports[state->irq]->next_port)) {
		if (IRQ_ports[state->irq]) {
			retval = -EBUSY;
			goto errout;
		} else 
			handler = rs_interrupt_single;

		retval = request_irq(state->irq, handler, SA_SHIRQ,
				     "serial", &IRQ_ports[state->irq]);
		if (!retval)
		    retval = request_irq(state->irq - 1, rs_interrupt_tx,
					 SA_SHIRQ,
					 "serial_tx", &IRQ_ports[state->irq]);
		if (retval) {
			if (capable(CAP_SYS_ADMIN)) {
				if (info->tty)
					set_bit(TTY_IO_ERROR,
						&info->tty->flags);
				retval = 0;
			}
			goto errout;
		}
	}

	/*
	 * Insert serial port into IRQ chain.
	 */
	info->prev_port = 0;
	info->next_port = IRQ_ports[state->irq];
	if (info->next_port)
		info->next_port->prev_port = info;
	IRQ_ports[state->irq] = info;
	figure_IRQ_timeout(state->irq);

#ifndef CONFIG_DBMX1_TPM102
	/*
	 * Now, initialize the UART 
	 */
	serial_outp(info, UART_LCR, UART_LCR_WLEN8);	/* reset DLAB */
#endif // CONFIG_DBMX1_TPM102

	info->ucr2 = UCR2_SRST | UCR2_RXEN | UCR2_TXEN | UCR2_WS | UCR2_IRTS;
	info->ucr3 = 0;
	info->uts = 0;
	if (info->tty->termios->c_cflag & CBAUD) {
		info->ucr2 |= UCR2_CTS;
		info->ucr3 |= UCR3_DSR;
	}
	serial_outp(info, UCR2, info->ucr2);
	serial_outp(info, UCR3, info->ucr3);
	
	/*
	 * Finally, enable interrupts
	 */
	info->ucr1 = UCR1_UARTEN | UCR1_UARTCLKEN | UCR1_RRDYEN;
	serial_outp(info, UCR1, info->ucr1);	/* enable interrupts */

#ifndef CONFIG_DBMX1_TPM102
	/*
	 * And clear the interrupt registers again for luck.
	 */
	(void)serial_inp(info, UART_LSR);
	(void)serial_inp(info, UART_RX);
	(void)serial_inp(info, UART_IIR);
	(void)serial_inp(info, UART_MSR);
#endif // CONFIG_DBMX1_TPM102

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit.head = info->xmit.tail = 0;

	/*
	 * Set up serial timers...
	 */
	mod_timer(&serial_timer, jiffies + 2*HZ/100);

	/*
	 * Set up the tty->alt_speed kludge
	 */
#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
	if (info->tty) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			info->tty->alt_speed = 57600;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			info->tty->alt_speed = 115200;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			info->tty->alt_speed = 230400;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			info->tty->alt_speed = 460800;
	}
#endif
	
	/*
	 * and set the speed of the serial port
	 */
	change_speed(info, 0);

	info->flags |= ASYNC_INITIALIZED;
#ifdef CONFIG_KGDB
	info->flags &= ~ASYNC_GDB;
#endif
	restore_flags(flags);
	return 0;
	
errout:
	restore_flags(flags);
	return retval;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct dbmx1_async_struct * info)
{
	unsigned long	flags;
	struct serial_state *state;
	int		retval;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	state = info->state;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....", info->line,
	       state->irq);
#endif
	
	save_flags(flags); cli(); /* Disable interrupts */

	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */
	wake_up_interruptible(&info->delta_msr_wait);
	
	/*
	 * First unlink the serial port from the IRQ chain...
	 */
	if (info->next_port)
		info->next_port->prev_port = info->prev_port;
	if (info->prev_port)
		info->prev_port->next_port = info->next_port;
	else
		IRQ_ports[state->irq] = info->next_port;
	figure_IRQ_timeout(state->irq);
	
	/*
	 * Free the IRQ, if necessary
	 */
	if (state->irq && (!IRQ_ports[state->irq] ||
			  !IRQ_ports[state->irq]->next_port)) {
		if (IRQ_ports[state->irq]) {
			free_irq(state->irq, &IRQ_ports[state->irq]);
			retval = request_irq(state->irq, rs_interrupt_single,
					     SA_SHIRQ, "serial",
					     &IRQ_ports[state->irq]);
			
			if (retval)
				printk("serial shutdown: request_irq: error %d"
				       "  Couldn't reacquire IRQ.\n", retval);
		} else {
#ifdef CONFIG_KGDB
			if (info->flags & ASYNC_GDB) {
			    	free_irq(state->irq, NULL);
			    	free_irq(state->irq - 1, NULL);
			} else {
#endif
				free_irq(state->irq, &IRQ_ports[state->irq]);
				free_irq(state->irq - 1,
					 &IRQ_ports[state->irq]);
#ifdef CONFIG_KGDB
			}
#endif
		}
	}

	if (info->xmit.buf) {
		unsigned long pg = (unsigned long) info->xmit.buf;
		info->xmit.buf = 0;
		free_page(pg);
	}

	info->ucr1 = UCR1_UARTEN | UCR1_UARTCLKEN;
	serial_out(info, UCR1, info->ucr1);	/* disable all intrs */
#ifndef CONFIG_DBMX1_TPM102
	/* disable break condition */
	serial_out(info, UART_LCR, serial_inp(info, UART_LCR) & ~UART_LCR_SBC);
#endif // CONFIG_DBMX1_TPM102
	
	if (!info->tty || (info->tty->termios->c_cflag & HUPCL)) {
		info->ucr2 &= ~UCR2_CTS;
		info->ucr3 &= ~UCR3_DSR;
	}
	serial_out(info, UCR2, info->ucr2);
	serial_out(info, UCR3, info->ucr3);
	serial_out(info, UCR4, UCR4_DREN);
	serial_out(info, UFCR, (2 << 10) | (5 << 7) | (1 << 0));

#ifndef CONFIG_DBMX1_TPM102
	/* disable FIFO's */	
	serial_outp(info, UART_FCR, (UART_FCR_ENABLE_FIFO |
				     UART_FCR_CLEAR_RCVR |
				     UART_FCR_CLEAR_XMIT));
	serial_outp(info, UART_FCR, 0);


	(void)serial_in(info, UART_RX);    /* read data port to reset things */
#endif // CONFIG_DBMX1_TPM102
	
	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

#ifndef CONFIG_DBMX1_TPM102
	if (uart_config[info->state->type].flags & UART_STARTECH) {
		/* Arrange to enter sleep mode */
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, UART_EFR_ECB);
		serial_outp(info, UART_LCR, 0);
		serial_outp(info, UCR1, UART_IERX_SLEEP);
		serial_outp(info, UART_LCR, 0xBF);
		serial_outp(info, UART_EFR, 0);
		serial_outp(info, UART_LCR, 0);
	}
	if (info->state->type == PORT_16750) {
		/* Arrange to enter sleep mode */
		serial_outp(info, UCR1, UART_IERX_SLEEP);
	}
#endif // CONFIG_DBMX1_TPM102
	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

#ifdef CONFIG_KGDB
void dbmx1_shutdown_for_gdb(struct async_struct * info)
{
    shutdown(info) ;
}
#endif

#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300,
	600, 1200, 1800, 2400, 4800, 9600, 19200,
	38400, 57600, 115200, 230400, 460800, 0 };

static int tty_get_baud_rate(struct tty_struct *tty)
{
	struct dbmx1_async_struct * info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned int cflag, i;

	cflag = tty->termios->c_cflag;

	i = cflag & CBAUD;
	if (i & CBAUDEX) {
		i &= ~CBAUDEX;
		if (i < 1 || i > 2) 
			tty->termios->c_cflag &= ~CBAUDEX;
		else
			i += 15;
	}
	if (i == 15) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			i += 1;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			i += 2;
	}
	return baud_table[i];
}
#endif

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct dbmx1_async_struct *info,
			 struct termios *old_termios)
{
	int	quot = 0, baud_base, baud;
	unsigned cflag, cval, fcr = 0;
	int	bits;
	unsigned long	flags;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	if (!CONFIGURED_SERIAL_PORT(info))
		return;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	      case CS7: cval = 0; bits = 9; break;
	      default: cval = UCR2_WS; bits = 10; break;
	      }
	if (cflag & CSTOPB) {
		cval |= UCR2_STPB;
		bits++;
	}
	if (cflag & (PARENB | PARODD)) {
		cval |= UCR2_PREN;
		bits++;
	}
	if (cflag & PARENB)
		cval |= UCR2_PROE;

	/* Determine divisor based on baud rate */
	baud = tty_get_baud_rate(info->tty);
	if (!baud)
		baud = 9600;	/* B0 transition handled in rs_set_termios */
	baud_base = info->state->baud_base;
	/* If the quotient is zero refuse the change */
	if (!baud_base && old_termios) {
		info->tty->termios->c_cflag &= ~CBAUD;
		info->tty->termios->c_cflag |= (old_termios->c_cflag & CBAUD);
		baud = tty_get_baud_rate(info->tty);
		if (!baud)
			baud = 9600;
	}
	quot = baud_base / baud;
	info->quot = quot;
	info->timeout = ((info->xmit_fifo_size*HZ*bits*quot) / baud_base);
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

	/* Set up FIFO's */
	if (uart_config[info->state->type].flags & UART_USE_FIFO) {
		if ((info->state->baud_base / quot) < 2400)
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_1;
		else
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_8;
	}
	if (info->state->type == PORT_16750)
		fcr |= UART_FCR7_64BYTE;
	
	/* CTS flow control flag and modem status interrupts */
	if (cflag & CRTSCTS) {
		info->flags |= ASYNC_CTS_FLOW;
	} else
		info->flags &= ~ASYNC_CTS_FLOW;
	if (cflag & CLOCAL)
		info->flags &= ~ASYNC_CHECK_CD;
	else {
		info->flags |= ASYNC_CHECK_CD;
	}

	/*
	 * Set up parity check flag
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	info->read_status_mask = URXDn_OVRRUN | URXDn_CHARRDY;
	if (I_INPCK(info->tty))
		info->read_status_mask |= URXDn_FRMERR | URXDn_PRERR;
	if (I_BRKINT(info->tty) || I_PARMRK(info->tty))
		info->read_status_mask |= URXDn_BRK;
	
	/*
	 * Characters to ignore
	 */
	info->ignore_status_mask = 0;
	if (I_IGNPAR(info->tty))
		info->ignore_status_mask |= URXDn_PRERR | URXDn_FRMERR;
	if (I_IGNBRK(info->tty)) {
		info->ignore_status_mask |= URXDn_BRK;
		/*
		 * If we're ignore parity and break indicators, ignore 
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->tty))
			info->ignore_status_mask |= URXDn_OVRRUN;
	}
	/*
	 * !!! ignore all characters if CREAD is not set
	 */
	if ((cflag & CREAD) == 0)
		info->ignore_status_mask |= URXDn_CHARRDY;
	info->ucr2 = (info->ucr2 & ~(UCR2_WS | UCR2_STPB | UCR2_PROE |
				     UCR2_PREN)) | cval;
	save_flags(flags); cli();
	serial_out(info, UCR2, info->ucr2);
	serial_out(info, UBIR, baud / 100);
	serial_out(info, UBMR, baud_base / 100);
	restore_flags(flags);
}

static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_put_char"))
		return;

	if (!tty || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	if (CIRC_SPACE(info->xmit.head,
		       info->xmit.tail,
		       SERIAL_XMIT_SIZE) == 0) {
		restore_flags(flags);
		return;
	}

	info->xmit.buf[info->xmit.head] = ch;
	info->xmit.head = (info->xmit.head + 1) & (SERIAL_XMIT_SIZE-1);
	restore_flags(flags);
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;
				
	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	if (info->xmit.head == info->xmit.tail
	    || tty->stopped
	    || tty->hw_stopped
	    || !info->xmit.buf)
		return;

	save_flags(flags); cli();
	info->ucr1 |= UCR1_TRDYEN;
	serial_out(info, UCR1, info->ucr1);
	restore_flags(flags);
}

static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;
				
	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if (!tty || !info->xmit.buf || !tmp_buf)
		return 0;

	save_flags(flags);
	if (from_user) {
		down(&tmp_buf_sem);
		while (1) {
			int c1;
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      SERIAL_XMIT_SIZE);
			if (count < c)
				c = count;
			if (c <= 0)
				break;

			c -= copy_from_user(tmp_buf, buf, c);
			if (!c) {
				if (!ret)
					ret = -EFAULT;
				break;
			}
			cli();
			c1 = CIRC_SPACE_TO_END(info->xmit.head,
					       info->xmit.tail,
					       SERIAL_XMIT_SIZE);
			if (c1 < c)
				c = c1;
			memcpy(info->xmit.buf + info->xmit.head, tmp_buf, c);
			info->xmit.head = ((info->xmit.head + c) &
					   (SERIAL_XMIT_SIZE-1));
			restore_flags(flags);
			buf += c;
			count -= c;
			ret += c;
		}
		up(&tmp_buf_sem);
	} else {
		cli();
		while (1) {
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      SERIAL_XMIT_SIZE);
			if (count < c)
				c = count;
			if (c <= 0) {
				break;
			}
			memcpy(info->xmit.buf + info->xmit.head, buf, c);
			info->xmit.head = ((info->xmit.head + c) &
					   (SERIAL_XMIT_SIZE-1));
			buf += c;
			count -= c;
			ret += c;
		}
		restore_flags(flags);
	}
	if (info->xmit.head != info->xmit.tail
	    && !tty->stopped
	    && !tty->hw_stopped
	    && !(info->ucr1 & UCR1_TRDYEN)) {
		info->ucr1 |= UCR1_TRDYEN;
		serial_out(info, UCR1, info->ucr1);
	}
	return ret;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;
	save_flags(flags); cli();
	info->xmit.head = info->xmit.tail = 0;
	restore_flags(flags);
	wake_up_interruptible(&tty->write_wait);
#ifdef SERIAL_HAVE_POLL_WAIT
	wake_up_interruptible(&tty->poll_wait);
#endif
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void rs_send_xchar(struct tty_struct *tty, char ch)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_send_char"))
		return;

	info->x_char = ch;
	if (ch) {
		/* Make sure transmit interrupts are on */
		info->ucr1 |= UCR1_TRDYEN;
		serial_out(info, UCR1, info->ucr1);
	}
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void rs_throttle(struct tty_struct * tty)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));

	if (tty->termios->c_cflag & CRTSCTS)
		info->ucr2 &= ~UCR2_CTS;

	save_flags(flags); cli();
	serial_outp(info, UCR2, info->ucr2);
	restore_flags(flags);
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}
	if (tty->termios->c_cflag & CRTSCTS)
		info->ucr2 |= UCR2_CTS;
	save_flags(flags); cli();
	serial_outp(info, UCR2, info->ucr2);
	restore_flags(flags);
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct dbmx1_async_struct * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct tmp;
	struct serial_state *state = info->state;
   
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = state->type;
	tmp.line = state->line;
	tmp.port = state->port;
	if (HIGH_BITS_OFFSET)
		tmp.port_high = state->port >> HIGH_BITS_OFFSET;
	else
		tmp.port_high = 0;
	tmp.irq = state->irq;
	tmp.flags = state->flags;
	tmp.xmit_fifo_size = state->xmit_fifo_size;
	tmp.baud_base = state->baud_base;
	tmp.close_delay = state->close_delay;
	tmp.closing_wait = state->closing_wait;
	tmp.custom_divisor = state->custom_divisor;
	tmp.hub6 = state->hub6;
	tmp.io_type = state->io_type;
	if (copy_to_user(retinfo,&tmp,sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_serial_info(struct dbmx1_async_struct * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
 	struct serial_state old_state, *state;
	unsigned int		i,change_irq,change_port;
	int 			retval = 0;
	unsigned long		new_port;

	if (copy_from_user(&new_serial,new_info,sizeof(new_serial)))
		return -EFAULT;
	state = info->state;
	old_state = *state;

	new_port = new_serial.port;
	if (HIGH_BITS_OFFSET)
		new_port += (unsigned long) new_serial.port_high << HIGH_BITS_OFFSET;

	change_irq = new_serial.irq != state->irq;
	change_port = (new_port != ((int) state->port)) ||
		(new_serial.hub6 != state->hub6);
  
	if (!capable(CAP_SYS_ADMIN)) {
		if (change_irq || change_port ||
		    (new_serial.baud_base != state->baud_base) ||
		    (new_serial.type != state->type) ||
		    (new_serial.close_delay != state->close_delay) ||
		    (new_serial.xmit_fifo_size != state->xmit_fifo_size) ||
		    ((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (state->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		state->flags = ((state->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		state->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	new_serial.irq = irq_cannonicalize(new_serial.irq);

	if ((new_serial.irq >= NR_IRQS) || (new_serial.irq < 0) || 
	    (new_serial.baud_base < 9600)|| (new_serial.type != PORT_DBMX1)) {
		return -EINVAL;
	}

	if ((new_serial.type != state->type) ||
	    (new_serial.xmit_fifo_size <= 0))
		new_serial.xmit_fifo_size =
			uart_config[new_serial.type].dfl_xmit_fifo_size;

	/* Make sure address is not already in use */
	if (new_serial.type) {
		for (i = 0 ; i < NR_PORTS; i++)
			if ((state != &rs_table[i]) &&
			    (rs_table[i].io_type == SERIAL_IO_PORT) &&
			    (rs_table[i].port == new_port) &&
			    rs_table[i].type)
				return -EADDRINUSE;
	}

	if ((change_port || change_irq) && (state->count > 1))
		return -EBUSY;

	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */

	state->baud_base = new_serial.baud_base;
	state->flags = ((state->flags & ~ASYNC_FLAGS) |
			(new_serial.flags & ASYNC_FLAGS));
	info->flags = ((state->flags & ~ASYNC_INTERNAL_FLAGS) |
		       (info->flags & ASYNC_INTERNAL_FLAGS));
	state->custom_divisor = new_serial.custom_divisor;
	state->close_delay = new_serial.close_delay * HZ/100;
	state->closing_wait = new_serial.closing_wait * HZ/100;
#if (LINUX_VERSION_CODE > 0x20100)
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
#endif
	info->xmit_fifo_size = state->xmit_fifo_size =
		new_serial.xmit_fifo_size;

	if ((state->type != PORT_UNKNOWN) && state->port) {
		release_region(state->port,0x100);
	}
	state->type = new_serial.type;
	if (change_port || change_irq) {
		/*
		 * We need to shutdown the serial port at the old
		 * port/irq combination.
		 */
		shutdown(info);
		state->irq = new_serial.irq;
		info->port = state->port = new_port;
		info->hub6 = state->hub6 = new_serial.hub6;
		if (info->hub6)
			info->io_type = state->io_type = SERIAL_IO_HUB6;
		else if (info->io_type == SERIAL_IO_HUB6)
			info->io_type = state->io_type = SERIAL_IO_PORT;
	}
	if ((state->type != PORT_UNKNOWN) && state->port) {
		request_region(state->port,0x100,"serial(set)");
	}

	
check_and_exit:
	if ((!state->port && !state->iomem_base) || !state->type)
		return 0;
	if (info->flags & ASYNC_INITIALIZED) {
		if (((old_state.flags & ASYNC_SPD_MASK) !=
		     (state->flags & ASYNC_SPD_MASK)) ||
		    (old_state.custom_divisor != state->custom_divisor)) {
#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
				info->tty->alt_speed = 57600;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
				info->tty->alt_speed = 115200;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
				info->tty->alt_speed = 230400;
			if ((state->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
				info->tty->alt_speed = 460800;
#endif
			change_speed(info, 0);
		}
	} else
		retval = startup(info);
	return retval;
}


/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space. 
 */
static int get_lsr_info(struct dbmx1_async_struct * info, unsigned int *value)
{
	unsigned char status;
	unsigned int result;
	unsigned long flags;

#ifndef CONFIG_DBMX1_TPM102
	save_flags(flags); cli();
	status = serial_in(info, UART_LSR);
	restore_flags(flags);
	result = ((status & UART_LSR_TEMT) ? TIOCSER_TEMT : 0);

	/*
	 * If we're about to load something into the transmit
	 * register, we'll pretend the transmitter isn't empty to
	 * avoid a race condition (depending on when the transmit
	 * interrupt happens).
	 */
	if (info->x_char || 
	    ((CIRC_CNT(info->xmit.head, info->xmit.tail,
		       SERIAL_XMIT_SIZE) > 0) &&
	     !info->tty->stopped && !info->tty->hw_stopped))
		result &= ~TIOCSER_TEMT;

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
#endif // CONFIG_DBMX1_TPM102
	return 0;
}


static int get_modem_info(struct dbmx1_async_struct * info, unsigned int *value)
{
	unsigned long ucr2, ucr3, usr1, usr2;
	unsigned int result;
	unsigned long flags;

	save_flags(flags); cli();
	ucr2 = serial_in(info, UCR2);
	ucr3 = serial_in(info, UCR3);
	usr1 = serial_in(info, USR1);
	usr2 = serial_in(info, USR2);
	restore_flags(flags);
	result =  ((ucr2 & UCR2_CTS) ? TIOCM_RTS : 0)
		| ((ucr3 & UCR3_DSR) ? TIOCM_DTR : 0)
		| ((ucr3 & UCR3_DCD) ? TIOCM_CAR : 0)
		| ((ucr3 & UCR3_RI) ? TIOCM_RNG : 0)
		| ((usr2 & USR2_DTRF) ? TIOCM_DSR : 0)
		| ((usr1 & USR1_RTSS) ? TIOCM_CTS : 0);

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}

static int set_modem_info(struct dbmx1_async_struct * info, unsigned int cmd,
			  unsigned int *value)
{
	unsigned int arg;
	unsigned long flags;

	if (copy_from_user(&arg, value, sizeof(int)))
		return -EFAULT;

	switch (cmd) {
	case TIOCMBIS: 
		if (arg & TIOCM_RTS)
			info->ucr2 |= UCR2_CTS;
		if (arg & TIOCM_DTR)
			info->ucr3 |= UCR3_DSR;
		if (arg & TIOCM_LOOP)
			info->uts |= UTS_LOOP;
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			info->ucr2 &= ~UCR2_CTS;
		if (arg & TIOCM_DTR)
			info->ucr3 &= ~UCR3_DSR;
		if (arg & TIOCM_LOOP)
			info->uts &= ~UTS_LOOP;
		break;
	case TIOCMSET:
		info->ucr2 &= ~UCR2_CTS;
		info->ucr3 &= ~UCR3_DSR;
		info->uts &= ~UTS_LOOP;
		if (arg & TIOCM_RTS)
			info->ucr2 |= UCR2_CTS;
		if (arg & TIOCM_DTR)
			info->ucr3 |= UCR3_DSR;
		if (arg & TIOCM_LOOP)
			info->uts |= UTS_LOOP;
		break;
	default:
		return -EINVAL;
	}
	save_flags(flags); cli();
	serial_out(info, UCR2, info->ucr2);
	serial_out(info, UCR3, info->ucr3);
	serial_out(info, UTS, info->uts);
	restore_flags(flags);
	return 0;
}

static int do_autoconfig(struct dbmx1_async_struct * info)
{
	int irq, retval;
	
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	
	if (info->state->count > 1)
		return -EBUSY;
	
	shutdown(info);

	autoconfig(info->state);
	if ((info->state->flags & ASYNC_AUTO_IRQ) &&
	    (info->state->port != 0  || info->state->iomem_base != 0) &&
	    (info->state->type != PORT_UNKNOWN)) {
		irq = detect_uart_irq(info->state);
		if (irq > 0)
			info->state->irq = irq;
	}

	retval = startup(info);
	if (retval)
		return retval;
	return 0;
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
static void send_break(	struct dbmx1_async_struct * info, int duration)
{
	if (!CONFIGURED_SERIAL_PORT(info))
		return;
	current->state = TASK_INTERRUPTIBLE;
	current->timeout = jiffies + duration;
	cli();
	info->LCR |= UART_LCR_SBC;
	serial_out(info, UART_LCR, info->LCR);
	schedule();
	info->LCR &= ~UART_LCR_SBC;
	serial_out(info, UART_LCR, info->LCR);
	sti();
}
#else
static void rs_break(struct tty_struct *tty, int break_state)
{
	struct dbmx1_async_struct * info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_break"))
		return;

	if (!CONFIGURED_SERIAL_PORT(info))
		return;
	save_flags(flags); cli();
	if (break_state == -1)
		info->LCR |= UART_LCR_SBC;
	else
		info->LCR &= ~UART_LCR_SBC;
#ifndef CONFIG_DBMX1_TPM102
	serial_out(info, UART_LCR, info->LCR);
#endif // CONFIG_DBMX1_TPM102
	restore_flags(flags);
}
#endif

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct dbmx1_async_struct * info = (struct dbmx1_async_struct *)tty->driver_data;
	struct async_icount cprev, cnow;	/* kernel counter temps */
	struct serial_icounter_struct icount;
	unsigned long flags;
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
	int retval, tmp;
#endif
	
	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}
	
	switch (cmd) {
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
		case TCSBRK:	/* SVID version: non-zero arg --> no break */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
			if (!arg) {
				send_break(info, HZ/4);	/* 1/4 second */
				if (signal_pending(current))
					return -EINTR;
			}
			return 0;
		case TCSBRKP:	/* support for POSIX tcsendbreak() */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
			send_break(info, arg ? arg*(HZ/10) : HZ/4);
			if (signal_pending(current))
				return -EINTR;
			return 0;
		case TIOCGSOFTCAR:
			tmp = C_CLOCAL(tty) ? 1 : 0;
			if (copy_to_user((void *)arg, &tmp, sizeof(int)))
				return -EFAULT;
			return 0;
		case TIOCSSOFTCAR:
			if (copy_from_user(&tmp, (void *)arg, sizeof(int)))
				return -EFAULT;

			tty->termios->c_cflag =
				((tty->termios->c_cflag & ~CLOCAL) |
				 (tmp ? CLOCAL : 0));
			return 0;
#endif
		case TIOCMGET:
			return get_modem_info(info, (unsigned int *) arg);
		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			return set_modem_info(info, cmd, (unsigned int *) arg);
		case TIOCGSERIAL:
			return get_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSSERIAL:
			return set_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSERCONFIG:
			return do_autoconfig(info);

		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned int *) arg);

		case TIOCSERGSTRUCT:
			if (copy_to_user((struct dbmx1_async_struct *) arg,
					 info, sizeof(struct dbmx1_async_struct)))
				return -EFAULT;
			return 0;
				
		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
 		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
		case TIOCMIWAIT:
			save_flags(flags); cli();
			/* note the counters on entry */
			cprev = info->state->icount;
			restore_flags(flags);
			while (1) {
				interruptible_sleep_on(&info->delta_msr_wait);
				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				save_flags(flags); cli();
				cnow = info->state->icount; /* atomic copy */
				restore_flags(flags);
				if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr && 
				    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
					return -EIO; /* no change => error */
				if ( ((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
				     ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
				     ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
				     ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
					return 0;
				}
				cprev = cnow;
			}
			/* NOTREACHED */

		/* 
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
		case TIOCGICOUNT:
			save_flags(flags); cli();
			cnow = info->state->icount;
			restore_flags(flags);
			icount.cts = cnow.cts;
			icount.dsr = cnow.dsr;
			icount.rng = cnow.rng;
			icount.dcd = cnow.dcd;
			icount.rx = cnow.rx;
			icount.tx = cnow.tx;
			icount.frame = cnow.frame;
			icount.overrun = cnow.overrun;
			icount.parity = cnow.parity;
			icount.brk = cnow.brk;
			icount.buf_overrun = cnow.buf_overrun;
			
			if (copy_to_user((void *)arg, &icount, sizeof(icount)))
				return -EFAULT;
			return 0;
		case TIOCSERGWILD:
		case TIOCSERSWILD:
			/* "setserial -W" is called in Debian boot */
			printk ("TIOCSER?WILD ioctl obsolete, ignored.\n");
			return 0;

#ifdef CONFIG_KGDB
		case TIOCGDB:
			gdb_ttyS = MINOR(tty->device) & 0x03F ;
			gdb_baud = tty_get_baud_rate(tty) ;
			return gdb_hook();
#endif

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct dbmx1_async_struct *info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long flags;
	unsigned int cflag = tty->termios->c_cflag;
	
	if (   (cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag) 
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info, old_termios);

	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(cflag & CBAUD)) {
		info->ucr2 &= ~UCR2_CTS;
		info->ucr3 &= ~UCR3_DSR;
		save_flags(flags); cli();
		serial_outp(info, UCR2, info->ucr2);
		serial_outp(info, UCR3, info->ucr3);
		restore_flags(flags);
	}
	
	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (cflag & CBAUD)) {
		info->ucr3 &= ~UCR3_DSR;
		if (!(tty->termios->c_cflag & CRTSCTS) || 
		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			info->ucr2 &= ~UCR2_CTS;
		}
		save_flags(flags); cli();
		serial_outp(info, UCR2, info->ucr2);
		serial_outp(info, UCR3, info->ucr3);
		restore_flags(flags);
	}
	
	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}
}

/*
 * ------------------------------------------------------------
 * rs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void rs_close(struct tty_struct *tty, struct file * filp)
{
	struct dbmx1_async_struct * info = (struct dbmx1_async_struct *)tty->driver_data;
	struct serial_state *state;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;

	state = info->state;
	
	save_flags(flags); cli();
	
	if (tty_hung_up_p(filp)) {
		DBG_CNT("before DEC-hung");
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
	
#ifdef SERIAL_DEBUG_OPEN
	printk("rs_close ttys%d, count = %d\n", info->line, state->count);
#endif
	if ((tty->count == 1) && (state->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  state->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("rs_close: bad serial port count; tty->count is 1, "
		       "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		printk("rs_close: bad serial port count for ttys%d: %d\n",
		       info->line, state->count);
		state->count = 0;
	}
	if (state->count) {
		DBG_CNT("before DEC-2");
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
	info->flags |= ASYNC_CLOSING;
	restore_flags(flags);
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ASYNC_NORMAL_ACTIVE)
		info->state->normal_termios = *tty->termios;
	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->state->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
#ifndef CONFIG_DBMX1_TPM102
	info->IER &= ~UART_IER_RLSI;
#endif // CONFIG_DBMX1_TPM102
	info->read_status_mask &= ~URXDn_CHARRDY;
	if (info->flags & ASYNC_INITIALIZED) {
#ifndef CONFIG_DBMX1_TPM102
		serial_out(info, UCR1, info->ucr1);
#endif // CONFIG_DBMX1_TPM102
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		rs_wait_until_sent(tty, info->timeout);
	}
	shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	if (info->blocked_open) {
		if (info->close_delay) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
}

/*
 * rs_wait_until_sent() --- wait until the transmitter is empty
 */
static void rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct dbmx1_async_struct * info = (struct dbmx1_async_struct *)tty->driver_data;
	unsigned long orig_jiffies, char_time;
	int usr1, usr2;
	
	if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
		return;

	if (info->state->type == PORT_UNKNOWN)
		return;

	if (info->xmit_fifo_size == 0)
		return; /* Just in case.... */

	orig_jiffies = jiffies;
	/*
	 * Set the check interval to be 1/5 of the estimated time to
	 * send a single character, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 * 
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */
	char_time = (info->timeout - HZ/50) / info->xmit_fifo_size;
	char_time = char_time / 5;
	if (char_time == 0)
		char_time = 1;
	if (timeout && timeout < char_time)
		char_time = timeout;
	/*
	 * If the transmitter hasn't cleared in twice the approximate
	 * amount of time to send the entire FIFO, it probably won't
	 * ever clear.  This assumes the UART isn't doing flow
	 * control, which is currently the case.  Hence, if it ever
	 * takes longer than info->timeout, this is probably due to a
	 * UART bug of some kind.  So, we clamp the timeout parameter at
	 * 2*info->timeout.
	 */
	if (!timeout || timeout > 2*info->timeout)
		timeout = 2*info->timeout;
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("In rs_wait_until_sent(%d) check=%lu...", timeout, char_time);
	printk("jiff=%lu...", jiffies);
#endif
	while (!((usr1 = serial_inp(info, USR1)) & USR1_TRDY) &&
	       !((usr2 = serial_inp(info, USR2)) & USR2_TXFE)) {
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
		printk("usr1 = %d usr2 = %d (jiff=%lu)...",
		       usr1, usr2, jiffies);
#endif
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(char_time);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("usr1 = %d usr2 = %d (jiff=%lu)...done\n", usr1, usr2, jiffies);
#endif
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rs_hangup(struct tty_struct *tty)
{
	struct dbmx1_async_struct * info = (struct dbmx1_async_struct *)tty->driver_data;
	struct serial_state *state = info->state;
	
	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	state = info->state;
	
	rs_flush_buffer(tty);
	if (info->flags & ASYNC_CLOSING)
		return;
	shutdown(info);
	info->event = 0;
	state->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct dbmx1_async_struct *info)
{
	DECLARE_WAITQUEUE(wait, current);
	struct serial_state *state = info->state;
	int		retval;
	int		do_clocal = 0, extra_count = 0;
	unsigned long	flags;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		return ((info->flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
#else
		return -EAGAIN;
#endif
	}

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}
	
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ASYNC_CALLOUT_ACTIVE) {
		if (state->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}
	
	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, state->count is dropped by one, so that
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttys%d, count = %d\n",
	       state->line, state->count);
#endif
	save_flags(flags); cli();
	if (!tty_hung_up_p(filp)) {
		extra_count = 1;
		state->count--;
	}
	restore_flags(flags);
	info->blocked_open++;
	while (1) {
		save_flags(flags); cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD)) {
			serial_out(info, UCR2,
				   serial_inp(info, UCR2) | UCR2_CTS);
			serial_out(info, UCR3,
				   serial_inp(info, UCR3) | UCR3_DSR);
		}
		restore_flags(flags);
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING) &&
		    (do_clocal || (info->line == 0 ? 1 :
				   (serial_in(info, UCR3) & UCR3_DCD))))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttys%d, count = %d\n",
		       info->line, state->count);
#endif
		schedule();
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&info->open_wait, &wait);
	if (extra_count)
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
	       info->line, state->count);
#endif
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

static int get_async_struct(int line, struct dbmx1_async_struct **ret_info)
{
	struct dbmx1_async_struct *info;
	struct serial_state *sstate;

	sstate = rs_table + line;
	sstate->count++;
	if (sstate->info) {
		*ret_info = sstate->info;
		return 0;
	}
	info = kmalloc(sizeof(struct dbmx1_async_struct), GFP_KERNEL);
	if (!info) {
		sstate->count--;
		return -ENOMEM;
	}
	memset(info, 0, sizeof(struct dbmx1_async_struct));
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	init_waitqueue_head(&info->delta_msr_wait);
	info->magic = SERIAL_MAGIC;
	info->port = sstate->port;
	info->flags = sstate->flags;
	info->io_type = sstate->io_type;
	info->iomem_base = sstate->iomem_base;
	info->iomem_reg_shift = sstate->iomem_reg_shift;
	info->xmit_fifo_size = sstate->xmit_fifo_size;
	info->line = line;
	info->tqueue.routine = do_softint;
	info->tqueue.data = info;
	info->state = sstate;
	if (sstate->info) {
		kfree(info);
		*ret_info = sstate->info;
		return 0;
	}
	*ret_info = sstate->info = info;
	return 0;
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 *
 * Note that on failure, we don't decrement the module use count - the tty
 * later will call rs_close, which will decrement it for us as long as
 * tty->driver_data is set non-NULL. --rmk
 */
static int rs_open(struct tty_struct *tty, struct file * filp)
{
	struct dbmx1_async_struct	*info;
	int 			retval, line;
	unsigned long		page;

	MOD_INC_USE_COUNT;
	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS)) {
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}
	retval = get_async_struct(line, &info);
	if (retval) {
		MOD_DEC_USE_COUNT;
		return retval;
	}
	tty->driver_data = info;
	info->tty = tty;
	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->state->count);
#endif
#if (LINUX_VERSION_CODE > 0x20100)
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;
#endif

	/*
	 *	This relies on lock_kernel() stuff so wants tidying for 2.5
	 */
	if (!tmp_buf) {
		page = get_zeroed_page(GFP_KERNEL);
		if (!page)
			return -ENOMEM;
		if (tmp_buf)
			free_page(page);
		else
			tmp_buf = (unsigned char *) page;
	}

	/*
	 * If the port is the middle of closing, bail out now
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		return ((info->flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
#else
		return -EAGAIN;
#endif
	}

	/*
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval)
		return retval;

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("rs_open returning after block_til_ready with %d\n",
		       retval);
#endif
		return retval;
	}

	if ((info->state->count == 1) &&
	    (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->state->normal_termios;
		else 
			*tty->termios = info->state->callout_termios;
		change_speed(info, 0);
	}
#ifdef CONFIG_SERIAL_DBMX1_CONSOLE
	if (sercons.cflag && sercons.index == line) {
		tty->termios->c_cflag = sercons.cflag;
		sercons.cflag = 0;
		change_speed(info, 0);
	}
#endif
	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open ttys%d successful...", info->line);
#endif
	return 0;
}

/*
 * /proc fs routines....
 */

static inline int line_info(char *buf, struct serial_state *state)
{
	struct dbmx1_async_struct *info = state->info, scr_info;
	char	stat_buf[30];
	unsigned long ucr2, ucr3, usr1, usr2;
	int	ret;
	unsigned long flags;

	/*
	 * Return zero characters for ports not claimed by driver.
	 */
	if (state->type == PORT_UNKNOWN) {
		return 0;	/* ignore unused ports */
	}

	ret = sprintf(buf, "%d: uart:%s port:%lX irq:%d",
		      state->line, uart_config[state->type].name, 
		      (state->port ? state->port : (long)state->iomem_base),
		      state->irq);

	/*
	 * Figure out the current RS-232 lines
	 */
	if (!info) {
		info = &scr_info;	/* This is just for serial_{in,out} */

		info->magic = SERIAL_MAGIC;
		info->port = state->port;
		info->flags = state->flags;
		info->hub6 = state->hub6;
		info->io_type = state->io_type;
		info->iomem_base = state->iomem_base;
		info->iomem_reg_shift = state->iomem_reg_shift;
		info->quot = 0;
		info->tty = 0;
	}
	save_flags(flags); cli();
	ucr2 = info->ucr2;
	ucr3 = info->ucr3;
	usr1 = info->usr1;
	usr2 = info->usr2;
	if (info == &scr_info) {
		ucr2 = serial_in(info, UCR2);
		ucr3 = serial_in(info, UCR3);
		usr1 = serial_in(info, USR1);
		usr2 = serial_in(info, USR2);
	}
	restore_flags(flags); 

	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (ucr2 & UCR2_CTS)
		strcat(stat_buf, "|RTS");
	if (usr1 & USR1_RTSS)
		strcat(stat_buf, "|CTS");
	if (ucr3 & UCR3_DSR)
		strcat(stat_buf, "|DTR");
	if (usr2 & USR2_DTRF)
		strcat(stat_buf, "|DSR");
	if (ucr3 & UCR3_DCD)
		strcat(stat_buf, "|CD");
	if (ucr3 & UCR3_RI)
		strcat(stat_buf, "|RI");

	if (info->quot) {
		ret += sprintf(buf+ret, " baud:%d",
			       state->baud_base / info->quot);
	}

	ret += sprintf(buf+ret, " tx:%d rx:%d",
		      state->icount.tx, state->icount.rx);

	if (state->icount.frame)
		ret += sprintf(buf+ret, " fe:%d", state->icount.frame);
	
	if (state->icount.parity)
		ret += sprintf(buf+ret, " pe:%d", state->icount.parity);
	
	if (state->icount.brk)
		ret += sprintf(buf+ret, " brk:%d", state->icount.brk);	

	if (state->icount.overrun)
		ret += sprintf(buf+ret, " oe:%d", state->icount.overrun);

	/*
	 * Last thing is the RS-232 status lines
	 */
	ret += sprintf(buf+ret, " %s\n", stat_buf+1);
	return ret;
}

static int rs_read_proc(char *page, char **start, off_t off, int count,
		 int *eof, void *data)
{
	int i, len = 0, l;
	off_t	begin = 0;

	len += sprintf(page, "serinfo:1.0 driver:%s%s revision:%s\n",
		       serial_version, LOCAL_VERSTRING, serial_revdate);
	for (i = 0; i < NR_PORTS && len < 4000; i++) {
		l = line_info(page + len, &rs_table[i]);
		len += l;
		if (len+begin > off+count)
			goto done;
		if (len+begin < off) {
			begin += len;
			len = 0;
		}
	}
	*eof = 1;
done:
	if (off >= len+begin)
		return 0;
	*start = page + (off-begin);
	return ((count < begin+len-off) ? count : begin+len-off);
}

/*
 * ---------------------------------------------------------------------
 * rs_init() and friends
 *
 * rs_init() is called at boot-time to initialize the serial driver.
 * ---------------------------------------------------------------------
 */

/*
 * This routine prints out the appropriate serial driver version
 * number, and identifies which options were configured into this
 * driver.
 */
static char serial_options[] __initdata =
#ifdef SERIAL_OPT
       " enabled\n";
#else
       " no serial options enabled\n";
#endif
#undef SERIAL_OPT

static _INLINE_ void show_serial_version(void)
{
 	printk(KERN_INFO "%s version %s%s (%s) with%s", serial_name,
	       serial_version, LOCAL_VERSTRING, serial_revdate,
	       serial_options);
}

/*
 * This routine detect the IRQ of a serial port by clearing OUT2 when
 * no UART interrupt are requested (IER = 0) (*GPL*). This seems to work at
 * each time, as long as no other device permanently request the IRQ.
 * If no IRQ is detected, or multiple IRQ appear, this function returns 0.
 * The variable "state" and the field "state->port" should not be null.
 */
static unsigned detect_uart_irq (struct serial_state * state)
{
	int irq;
	struct dbmx1_async_struct scr_info; /* serial_{in,out} because HUB6 */

	scr_info.magic = SERIAL_MAGIC;
	scr_info.state = state;
	scr_info.port = state->port;
	scr_info.flags = state->flags;
	scr_info.io_type = state->io_type;
	scr_info.iomem_base = state->iomem_base;
	scr_info.iomem_reg_shift = state->iomem_reg_shift;

	/* forget possible initially masked and pending IRQ */
	probe_irq_off(probe_irq_on());
	return (irq > 0)? irq : 0;
}

/*
 * This routine is called by rs_init() to initialize a specific serial
 * port.  It determines what type of UART chip this serial port is
 * using: 8250, 16450, 16550, 16550A.  The important question is
 * whether or not this UART is a 16550A or not, since this will
 * determine whether or not we can use its FIFO features or not.
 */
static void autoconfig(struct serial_state * state)
{
	struct dbmx1_async_struct *info, scr_info;

	state->type = PORT_UNKNOWN;
	if (state->port)
		state->type = PORT_DBMX1;

#ifdef SERIAL_DEBUG_AUTOCONF
	printk("Testing ttyS%d (0x%04lx, 0x%04x)...\n", state->line,
	       state->port, (unsigned) state->iomem_base);
#endif
	
	if (!CONFIGURED_SERIAL_PORT(state))
		return;
		
#if 0
	info = &scr_info;	/* This is just for serial_{in,out} */

	info->magic = SERIAL_MAGIC;
	info->state = state;
	info->port = state->port;
	info->flags = state->flags;
	info->io_type = state->io_type;
	info->iomem_base = state->iomem_base;
	info->iomem_reg_shift = state->iomem_reg_shift;
#endif

	state->xmit_fifo_size =	uart_config[state->type].dfl_xmit_fifo_size;

	if (state->type == PORT_UNKNOWN)
		return;

	if (state->port) {
		request_region(state->port,0x100,"serial(auto)");
	}
}

int dbmx1_register_serial(struct serial_struct *req);
void dbmx1_unregister_serial(int line);

#if (LINUX_VERSION_CODE > 0x20100)
EXPORT_SYMBOL(dbmx1_register_serial);
EXPORT_SYMBOL(dbmx1_unregister_serial);
#else
static struct symbol_table serial_syms = {
#include <linux/symtab_begin.h>
	X(dbmx1_register_serial),
	X(dbmx1_unregister_serial),
#include <linux/symtab_end.h>
};
#endif


/*
 * The serial driver boot-time initialization code!
 */
static int __init dbmx1_rs_init(void)
{
	int i;
	struct serial_state * state;

	init_bh(SERIAL_BH, do_serial_bh);
	init_timer(&serial_timer);
	serial_timer.function = rs_timer;
	mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);

	for (i = 0; i < NR_IRQS; i++) {
		IRQ_ports[i] = 0;
		IRQ_timeout[i] = 0;
	}
#ifdef CONFIG_SERIAL_DBMX1_CONSOLE
	/*
	 *	The interrupt of the serial console port
	 *	can't be shared.
	 */
	if (sercons.flags & CON_CONSDEV) {
		for(i = 0; i < NR_PORTS; i++)
			if (i != sercons.index &&
			    rs_table[i].irq == rs_table[sercons.index].irq)
				rs_table[i].irq = 0;
	}
#endif
	show_serial_version();

	/* Initialize the tty_driver structure */
	
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
#if (LINUX_VERSION_CODE > 0x20100)
	serial_driver.driver_name = "serial";
#endif
#if (LINUX_VERSION_CODE > 0x2032D && defined(CONFIG_DEVFS_FS))
	serial_driver.name = "tts/%d";
#else
	serial_driver.name = "ttyS";
#endif
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64 + SERIAL_DEV_OFFSET;
	serial_driver.name_base = SERIAL_DEV_OFFSET;
	serial_driver.num = NR_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;

	serial_driver.open = rs_open;
	serial_driver.close = rs_close;
	serial_driver.write = rs_write;
	serial_driver.put_char = rs_put_char;
	serial_driver.flush_chars = rs_flush_chars;
	serial_driver.write_room = rs_write_room;
	serial_driver.chars_in_buffer = rs_chars_in_buffer;
	serial_driver.flush_buffer = rs_flush_buffer;
	serial_driver.ioctl = rs_ioctl;
	serial_driver.throttle = rs_throttle;
	serial_driver.unthrottle = rs_unthrottle;
	serial_driver.set_termios = rs_set_termios;
	serial_driver.stop = rs_stop;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;
#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
	serial_driver.break_ctl = rs_break;
#endif
#if (LINUX_VERSION_CODE >= 131343)
	serial_driver.send_xchar = rs_send_xchar;
	serial_driver.wait_until_sent = rs_wait_until_sent;
	serial_driver.read_proc = rs_read_proc;
#endif
	
	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
#if (LINUX_VERSION_CODE > 0x2032D && defined(CONFIG_DEVFS_FS))
	callout_driver.name = "cua/%d";
#else
	callout_driver.name = "cua";
#endif
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;
#if (LINUX_VERSION_CODE >= 131343)
	callout_driver.read_proc = 0;
	callout_driver.proc_entry = 0;
#endif

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	
	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
		state->magic = SSTATE_MAGIC;
		state->line = i;
		state->type = PORT_UNKNOWN;
		state->custom_divisor = 0;
		state->close_delay = 5*HZ/10;
		state->closing_wait = 30*HZ;
		state->callout_termios = callout_driver.init_termios;
		state->normal_termios = serial_driver.init_termios;
		state->icount.cts = state->icount.dsr = 
			state->icount.rng = state->icount.dcd = 0;
		state->icount.rx = state->icount.tx = 0;
		state->icount.frame = state->icount.parity = 0;
		state->icount.overrun = state->icount.brk = 0;
		state->irq = irq_cannonicalize(state->irq);
		if (state->hub6)
			state->io_type = SERIAL_IO_HUB6;
		if (state->port && check_region(state->port,0x100))
			continue;
#ifdef CONFIG_MCA			
		if ((state->flags & ASYNC_BOOT_ONLYMCA) && !MCA_bus)
			continue;
#endif			
		if (state->flags & ASYNC_BOOT_AUTOCONF)
			autoconfig(state);
	}
	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
		if (state->type == PORT_UNKNOWN)
			continue;
		if (   (state->flags & ASYNC_BOOT_AUTOCONF)
		    && (state->flags & ASYNC_AUTO_IRQ)
		    && (state->port != 0 || state->iomem_base != 0))
			state->irq = detect_uart_irq(state);
		if (state->io_type == SERIAL_IO_MEM) {
			printk(KERN_INFO"ttyS%02d%s at 0x%p (irq = %d) is a %s\n",
	 		       state->line + SERIAL_DEV_OFFSET,
			       (state->flags & ASYNC_FOURPORT) ? " FourPort" : "",
			       state->iomem_base, state->irq,
			       uart_config[state->type].name);
		}
		else {
			printk(KERN_INFO "ttyS%02d%s at 0x%04lx (irq = %d) is a %s\n",
	 		       state->line + SERIAL_DEV_OFFSET,
			       (state->flags & ASYNC_FOURPORT) ? " FourPort" : "",
			       state->port, state->irq,
			       uart_config[state->type].name);
		}
		tty_register_devfs(&serial_driver, 0,
				   serial_driver.minor_start + state->line);
		tty_register_devfs(&callout_driver, 0,
				   callout_driver.minor_start + state->line);
	}
	return 0;
}

/*
 * This is for use by architectures that know their serial console 
 * attributes only at run time. Not to be invoked after rs_init().
 */
int __init dbmx1_early_serial_setup(struct serial_struct *req)
{
	int i = req->line;

	if (i >= NR_IRQS)
		return(-ENOENT);
	rs_table[i].magic = 0;
	rs_table[i].baud_base = req->baud_base;
	rs_table[i].port = req->port;
	if (HIGH_BITS_OFFSET)
		rs_table[i].port += (unsigned long) req->port_high << 
							HIGH_BITS_OFFSET;
	rs_table[i].irq = req->irq;
	rs_table[i].flags = req->flags;
	rs_table[i].close_delay = req->close_delay;
	rs_table[i].io_type = req->io_type;
	rs_table[i].hub6 = req->hub6;
	rs_table[i].iomem_base = req->iomem_base;
	rs_table[i].iomem_reg_shift = req->iomem_reg_shift;
	rs_table[i].type = req->type;
	rs_table[i].xmit_fifo_size = req->xmit_fifo_size;
	rs_table[i].custom_divisor = req->custom_divisor;
	rs_table[i].closing_wait = req->closing_wait;
	return(0);
}

/*
 * register_serial and unregister_serial allows for 16x50 serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
 
/**
 *	register_serial - configure a 16x50 serial port at runtime
 *	@req: request structure
 *
 *	Configure the serial port specified by the request. If the
 *	port exists and is in use an error is returned. If the port
 *	is not currently in the table it is added.
 *
 *	The port is then probed and if neccessary the IRQ is autodetected
 *	If this fails an error is returned.
 *
 *	On success the port is ready to use and the line number is returned.
 */
 
int dbmx1_register_serial(struct serial_struct *req)
{
	int i;
	unsigned long flags;
	struct serial_state *state;
	struct dbmx1_async_struct *info;
	unsigned long port;

	port = req->port;
	if (HIGH_BITS_OFFSET)
		port += (unsigned long) req->port_high << HIGH_BITS_OFFSET;

	save_flags(flags); cli();
	for (i = 0; i < NR_PORTS; i++) {
		if ((rs_table[i].port == port) &&
		    (rs_table[i].iomem_base == req->iomem_base))
			break;
	}
	if (i == NR_PORTS) {
		for (i = 0; i < NR_PORTS; i++)
			if ((rs_table[i].type == PORT_UNKNOWN) &&
			    (rs_table[i].count == 0))
				break;
	}
	if (i == NR_PORTS) {
		restore_flags(flags);
		return -1;
	}
	state = &rs_table[i];
	if (rs_table[i].count) {
		restore_flags(flags);
		printk("Couldn't configure serial #%d (port=%ld,irq=%d): "
		       "device already open\n", i, port, req->irq);
		return -1;
	}
	state->irq = req->irq;
	state->port = port;
	state->flags = req->flags;
	state->io_type = req->io_type;
	state->iomem_base = req->iomem_base;
	state->iomem_reg_shift = req->iomem_reg_shift;
	if (req->baud_base)
		state->baud_base = req->baud_base;
	if ((info = state->info) != NULL) {
		info->port = port;
		info->flags = req->flags;
		info->io_type = req->io_type;
		info->iomem_base = req->iomem_base;
		info->iomem_reg_shift = req->iomem_reg_shift;
	}
	autoconfig(state);
	if (state->type == PORT_UNKNOWN) {
		restore_flags(flags);
		printk("dbmx1_register_serial(): autoconfig failed\n");
		return -1;
	}
	restore_flags(flags);

	if ((state->flags & ASYNC_AUTO_IRQ) && CONFIGURED_SERIAL_PORT(state))
		state->irq = detect_uart_irq(state);

       printk(KERN_INFO "ttyS%02d at %s 0x%04lx (irq = %d) is a %s\n",
	      state->line + SERIAL_DEV_OFFSET,
	      state->iomem_base ? "iomem" : "port",
	      state->iomem_base ? (unsigned long)state->iomem_base :
	      state->port, state->irq, uart_config[state->type].name);
	tty_register_devfs(&serial_driver, 0,
			   serial_driver.minor_start + state->line); 
	tty_register_devfs(&callout_driver, 0,
			   callout_driver.minor_start + state->line);
	return state->line + SERIAL_DEV_OFFSET;
}

/**
 *	unregister_serial - deconfigure a 16x50 serial port
 *	@line: line to deconfigure
 *
 *	The port specified is deconfigured and its resources are freed. Any
 *	user of the port is disconnected as if carrier was dropped. Line is
 *	the port number returned by register_serial().
 */

void dbmx1_unregister_serial(int line)
{
	unsigned long flags;
	struct serial_state *state = &rs_table[line];

	save_flags(flags); cli();
	if (state->info && state->info->tty)
		tty_hangup(state->info->tty);
	state->type = PORT_UNKNOWN;
	printk(KERN_INFO "ttyS%02d unloaded\n", state->line);
	/* These will be hidden, because they are devices that will no longer
	 * be available to the system. (ie, PCMCIA modems, once ejected)
	 */
	tty_unregister_devfs(&serial_driver,
			     serial_driver.minor_start + state->line);
	tty_unregister_devfs(&callout_driver,
			     callout_driver.minor_start + state->line);
	restore_flags(flags);
}

static void __exit dbmx1_rs_fini(void) 
{
	unsigned long flags;
	int e1, e2;
	int i;
	struct dbmx1_async_struct *info;

	/* printk("Unloading %s: version %s\n", serial_name, serial_version); */
	del_timer_sync(&serial_timer);
	save_flags(flags); cli();
        remove_bh(SERIAL_BH);
	if ((e1 = tty_unregister_driver(&serial_driver)))
		printk("serial: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&callout_driver)))
		printk("serial: failed to unregister callout driver (%d)\n", 
		       e2);
	restore_flags(flags);

	for (i = 0; i < NR_PORTS; i++) {
		if ((info = rs_table[i].info)) {
			rs_table[i].info = NULL;
			kfree(info);
		}
		if ((rs_table[i].type != PORT_UNKNOWN) && rs_table[i].port) {
			release_region(rs_table[i].port, 0x100);
		}
	}
	if (tmp_buf) {
		unsigned long pg = (unsigned long) tmp_buf;
		tmp_buf = NULL;
		free_page(pg);
	}
	
}

module_init(dbmx1_rs_init);
module_exit(dbmx1_rs_fini);


/*
 * ------------------------------------------------------------
 * Serial console driver
 * ------------------------------------------------------------
 */
#ifdef CONFIG_SERIAL_DBMX1_CONSOLE

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

static struct dbmx1_async_struct async_sercons;

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(struct dbmx1_async_struct *info)
{
	unsigned int tmout = 1000000;

	while (!(serial_in(info, USR1) & USR1_TRDY)) {
		if (--tmout == 0)
			break;
	}
}

#ifdef CONFIG_KGDB
static const char hexchars[] = "0123456789abcdef";

static __inline__ char highhex(int  x)
{
	return hexchars[(x >> 4) & 0xf];
}

static __inline__ char lowhex(int  x)
{
	return hexchars[x & 0xf];
}
extern int dbmx1_kgdb_in;
#endif

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 *
 *	The console must be locked when we get here.
 */
static void dbmx1_serial_console_write(struct console *co, const char *s,
				       unsigned count)
{
	static struct dbmx1_async_struct *info = &async_sercons;
	int ucr1;
	unsigned i;

#ifdef CONFIG_KGDB

        int checksum;

    	/* This call only does a trap the first time it is
	 * called, and so is safe to do here unconditionally
	 */

	if (dbmx1_kgdb_in) {

	    /*  $<packet info>#<checksum>. */
	    do {
		unsigned char c;
		write_char('$');
		write_char('O'); /* 'O'utput to console */
		checksum = 'O';

		for (i=0; i<count; i++) { /* Don't use run length encoding */
			int h, l;

			c = *s++;
			h = highhex(c);
			l = lowhex(c);
			write_char(h);
			write_char(l);
			checksum += h + l;
		}
		write_char('#');
		write_char(highhex(checksum));
		write_char(lowhex(checksum));
	    } while  (read_char() != '+');
	}
	else
#endif
	{
	/*
	 *	First save the IER then disable the interrupts
	 */
	ucr1 = serial_in(info, UCR1);
	serial_out(info, UCR1, ucr1 & ~UCR1_TRDYEN);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++, s++) {
		wait_for_xmitr(info);

		/*
		 *	Send the character out.
		 *	If a LF, also do CR...
		 */
		serial_out(info, UTXnD(0), *s);
		if (*s == 10) {
			wait_for_xmitr(info);
			serial_out(info, UTXnD(0), 13);
		}
	}

	/*
	 *	Finally, Wait for transmitter & holding register to empty
	 * 	and restore the IER
	 */
	wait_for_xmitr(info);
	serial_out(info, UCR1, ucr1);
	}
}

static kdev_t dbmx1_serial_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

/*
 *	Setup initial baud/bits/parity/flow control. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init dbmx1_serial_console_setup(struct console *co, char *options)
{
	static struct dbmx1_async_struct *info;
	struct serial_state *state;
	unsigned cval;
	int	baud = 9600;
	int	bits = 8;
	int	parity = 'n';
	int	doflow = 0;
	int	cflag = CREAD | HUPCL | CLOCAL;
	char	*s;

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s++ - '0';
		if (*s) doflow = (*s++ == 'r');
	}

	/*
	 *	Now construct a cflag setting.
	 */
	switch(baud) {
		case 1200:
			cflag |= B1200;
			break;
		case 2400:
			cflag |= B2400;
			break;
		case 4800:
			cflag |= B4800;
			break;
		case 19200:
			cflag |= B19200;
			break;
		case 38400:
			cflag |= B38400;
			break;
		case 57600:
			cflag |= B57600;
			break;
		case 115200:
			cflag |= B115200;
			break;
		case 9600:
		default:
			cflag |= B9600;
			/*
			 * Set this to a sane value to prevent a divide error
			 */
			baud  = 9600;
			break;
	}
	switch(bits) {
		case 7:
			cflag |= CS7;
			break;
		default:
		case 8:
			cflag |= CS8;
			break;
	}
	switch(parity) {
		case 'o': case 'O':
			cflag |= PARODD;
			break;
		case 'e': case 'E':
			cflag |= PARENB;
			break;
	}
	co->cflag = cflag;

	/*
	 *	Divisor, bytesize and parity
	 */
	state = rs_table + co->index;
	if (doflow)
		state->flags |= ASYNC_CONS_FLOW;
	info = &async_sercons;
	info->magic = SERIAL_MAGIC;
	info->state = state;
	info->port = state->port;
	info->flags = state->flags;
	info->io_type = state->io_type;
	info->iomem_base = state->iomem_base;
	info->iomem_reg_shift = state->iomem_reg_shift;
	cval = UCR2_SRST | UCR2_RXEN | UCR2_TXEN | UCR2_CTS | UCR2_IRTS;
	if ((cflag & CS8) == CS8)
		cval |= UCR2_WS;
	if (cflag & CSTOPB)
		cval |= UCR2_STPB;
	if (cflag & (PARENB | PARODD))
		cval |= UCR2_PREN;
	if (cflag & PARENB)
		cval |= UCR2_PROE;

	/*
	 *	Disable UART interrupts, set DTR and RTS high
	 *	and set speed.
	 */
#if 0
printk("UCR1  : %08x\n", serial_inp(info, UCR1));
printk("UCR2  : %08x\n", serial_inp(info, UCR2));
printk("UCR3  : %08x\n", serial_inp(info, UCR3));
printk("UCR4  : %08x\n", serial_inp(info, UCR4));
printk("UFCR  : %08x\n", serial_inp(info, UFCR));
printk("USR1  : %08x\n", serial_inp(info, USR1));
printk("USR2  : %08x\n", serial_inp(info, USR2));
printk("UESC  : %08x\n", serial_inp(info, UESC));
printk("UTIM  : %08x\n", serial_inp(info, UTIM));
printk("UBIR  : %08x\n", serial_inp(info, UBIR));
printk("UBMR  : %08x\n", serial_inp(info, UBMR));
printk("UBRC  : %08x\n", serial_inp(info, UBRC));
printk("BIPR1 : %08x\n", serial_inp(info, BIPR1));
printk("BIPR2 : %08x\n", serial_inp(info, BIPR2));
printk("BIPR3 : %08x\n", serial_inp(info, BIPR3));
printk("BIPR4 : %08x\n", serial_inp(info, BIPR4));
printk("BMPR1 : %08x\n", serial_inp(info, BMPR1));
printk("BMPR2 : %08x\n", serial_inp(info, BMPR2));
printk("BMPR3 : %08x\n", serial_inp(info, BMPR3));
printk("BMPR4 : %08x\n", serial_inp(info, BMPR4));
printk("UTS   : %08x\n", serial_inp(info, UTS));
#endif
	serial_out(info, UCR1, UCR1_UARTEN | UCR1_UARTCLKEN);
	serial_out(info, UCR2, cval);
	serial_out(info, UCR3, UCR3_DSR);
	serial_out(info, UCR4, UCR4_DREN);
	serial_out(info, UFCR, (2 << 10) | (5 << 7) | (1 << 0));
	serial_out(info, UESC, '+');
	serial_out(info, UTIM, 0);
	serial_out(info, UBIR, baud / 100);
	serial_out(info, UBMR, state->baud_base / 100);

	return 0;
}

static struct console sercons = {
	name:		"ttyS",
	write:		dbmx1_serial_console_write,
	device:		dbmx1_serial_console_device,
	setup:		dbmx1_serial_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

/*
 *	Register console.
 */
void __init dbmx1_serial_console_init(void)
{
	register_console(&sercons);
}
#endif

#ifdef CONFIG_KGDB
/* 
 *  Takes:
 *	ttyS - integer specifying which serial port to use for debugging
 *	baud - baud rate of specified serial port
 *  Returns:
 *	port for use by the gdb serial driver
 */
struct serial_state *
dbmx1_gdb_serial_setup(int ttyS, int baud)
{
	static struct dbmx1_async_struct info;
        struct serial_state *state;
        unsigned cval;
        int     bits = 8;
        int     parity = 'n';
        int     cflag = CREAD | HUPCL | CLOCAL;
        int     quot = 0;

        /*
         *      Now construct a cflag setting.
         */
        switch(baud) {
                case 1200:
                        cflag |= B1200;
                        break;
                case 2400:
                        cflag |= B2400;
                        break;
                case 4800:
                        cflag |= B4800;
                        break;
                case 19200:
                        cflag |= B19200;
                        break;
                case 38400:
                        cflag |= B38400;
                        break;
                case 57600:
                        cflag |= B57600;
                        break;
                case 115200:
                        cflag |= B115200;
                        break;
                case 9600:
                default:
                        cflag |= B9600;
                        break;
        }
        switch(bits) {
                case 7:
                        cflag |= CS7;
                        break;
                default:
                case 8:
                        cflag |= CS8;
                        break;
        }
        switch(parity) {
                case 'o': case 'O':
                        cflag |= PARODD;
                        break;
                case 'e': case 'E':
                        cflag |= PARENB;
                        break;
        }

        /*
         *      Divisor, bytesize and parity
         */

        state = rs_table + ttyS;
	info.magic = SERIAL_MAGIC;
	info.state = state;
	info.port = state->port;
	info.flags = state->flags;
	info.io_type = state->io_type;
	info.iomem_base = state->iomem_base;
	info.iomem_reg_shift = state->iomem_reg_shift;
	cval = UCR2_SRST | UCR2_RXEN | UCR2_TXEN | UCR2_CTS | UCR2_IRTS;
	if ((cflag & CS8) == CS8)
		cval |= UCR2_WS;
	if (cflag & CSTOPB)
		cval |= UCR2_STPB;
	if (cflag & (PARENB | PARODD))
		cval |= UCR2_PREN;
	if (cflag & PARENB)
		cval |= UCR2_PROE;

        /*
         *      Disable UART interrupts, set DTR and RTS high
         *      and set speed.
         */
	serial_out(&info, UCR1, UCR1_UARTEN | UCR1_UARTCLKEN);
	serial_out(&info, UCR2, cval);
	serial_out(&info, UCR3, UCR3_DSR);
	serial_out(&info, UCR4, UCR4_DREN);
	serial_out(&info, UFCR, (2 << 10) | (5 << 7) | (1 << 0));
	serial_out(&info, UESC, '+');
	serial_out(&info, UTIM, 0);
	serial_out(&info, UBIR, baud / 100);
	serial_out(&info, UBMR, state->baud_base / 100);

        return state;
}

#endif /* CONFIG_KGDB */
