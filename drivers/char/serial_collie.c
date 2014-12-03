/*
 *  linux/drivers/char/serial_collie.c
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *  Based on:
 *    drivers/char/serial_sa1100.c
 *
 *  Modified from serial.c
 *
 *  1/99: Changes for SA1100 internal async UARTs
 *        Hugo Fiennes <altman@empeg.com>
 *  Lots of changes, based on Deborah Wallace's Itsy/Brutus port, but
 *  with fixes which seem to cure repeated byte problems.
 *  This is no longer a 16x50 UART driver...
 *
 *  2/99: Cleanup: Now this can coexist with the generic serial driver
 *	  and cleanly integrate into regular driver location.
 *	  Nicolas Pitre <nico@visuaide.com>
 *
 *  6.6.99: handling Break conditions to break endless loops in rs_interrupt_single
 *          Keith  keith@keith-koep.com
 *
 *  2000/04/09: 
 *	Multi-machine support, further cleanup.
 *	  Nicolas Pitre <nico@cam.org>
 *  2000/06/29:
 * 	Completed serial console support, further cleanup
 * 	  Nicolas Pitre <nico@cam.org>
 *
 *  11-oct-2000:
 *      Added Magic System Request (MAGIC_SYSRQ) support
 *        Erik Mouw <J.A.K.Mouw@its.tudelft.nl>
 *
 *  18-nov-2000:
 *	Turn off IR port on Bitsy, when not in use.
 *	Jamey Hicks <jamey@crl.dec.com>
 *
 * Change Log
 *	30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 */

/*
 * Serial driver configuration section.  Here are the various options:
 *
 * SERIAL_PARANOIA_CHECK
 * 		Check the magic number for the async_structure where
 * 		ever possible.
 */

#undef SERIAL_PARANOIA_CHECK
#define SERIAL_DO_RESTART

/* Set of debugging defines */

#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT

#define RS_ISR_PASS_LIMIT 256

#define SERIAL_INLINE
  
#if defined(MODULE) && defined(SERIAL_DEBUG_MCOUNT)
#define DBG_CNT(s) printk("(%s): [%x] refc=%d, serc=%d, ttyc=%d -> %s\n", \
 kdevname(tty->device), (info->flags), serial_refcount,info->count,tty->count,s)
#else
#define DBG_CNT(s)
#endif

/*
 * End of serial driver configuration section.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#ifdef CONFIG_SERIAL_COLLIE_CONSOLE
#include <linux/console.h>
#endif
#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif

#include <linux/pm.h>

#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
#define SERIAL_FULL
#endif

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>
#include <asm/arch/serial_reg.h>

#ifdef CONFIG_SA1100_COLLIE
#include <asm/arch/tc35143.h>
#include <asm/ucb1200.h>
#endif

#ifdef SERIAL_INLINE
#define _INLINE_ inline
#endif

#ifdef SERIAL_FULL
#ifdef CONFIG_SA1100_HUW_WEBPANEL
#define UART_MSR_CTS 0x01
#define UART_MSR_DCD 0x02
#define UART_MSR_DSR 0x04
#define UART_MSR_RI  0x08
#define UART_MCR_RTS 0x01
#define UART_MCR_DTR 0x02
#ifdef TIOCM_OUT1
#define UART_MCR_OUT1 0x04
#define UART_MCR_OUT2 0x08
#endif
#endif
#ifdef CONFIG_SA1100_COLLIE
#define UART_MSR_CTS  0x01
#define UART_MSR_DCD  0x02
#define UART_MSR_DSR  0x04
#define UART_MSR_RI   0x08
#define UART_MCR_RTS  0x01
#define UART_MCR_DTR  0x02
#define UART_MCR_OUT1 0x04
#define UART_MCR_OUT2 0x08
#endif
#endif


static char *serial_name = "COLLIE serial driver";
static char *serial_version = "1.3";

static DECLARE_TASK_QUEUE(tq_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

#ifdef CONFIG_SERIAL_COLLIE_CONSOLE
static struct console sercons;
static int lsr_break_flag;
#ifdef CONFIG_MAGIC_SYSRQ
static unsigned long break_pressed; /* break, really ... */
#endif /* CONFIG_MAGIC_SYSRQ */
#endif /* CONFIG_SERIAL_COLLIE_CONSOLE */

static void autoconfig(struct serial_state * info, int adv);
static void change_speed(struct async_struct *info);
static void rs_wait_until_sent(struct tty_struct *tty, int timeout);

#define BASE_BAUD (3686400 / 16)
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#if 1
/* We want to stay compatible with the legacy /dev/ttyS0,1,2 devices for now */
/* 
 * For the regular 16x50 serial driver to work with this, we defined
 * SERIAL_DEV_OFFSET to 3 in serial.c.
 */
#define  SERIAL_SA1100_MAJOR	TTY_MAJOR
#define CALLOUT_SA1100_MAJOR	TTYAUX_MAJOR
#define MINOR_START		64
#define TTY_NAME		"ttyS"
#define  CU_NAME		"cua"
#else
/* We've been assigned a range on the "Low-density serial ports" major */
#define  SERIAL_SA1100_MAJOR	204
#define CALLOUT_SA1100_MAJOR	205
#define MINOR_START		5
#define TTY_NAME		"ttySA"
#define  CU_NAME		"cusa"
#endif

#define NR_PORTS		3

#define SA1100_UART1 \
  (struct serial_state){ 0, BASE_BAUD, (unsigned long)&Ser1UTCR0, IRQ_Ser1UART, STD_COM_FLAGS }
#define SA1100_UART2 \
  (struct serial_state){ 0, BASE_BAUD, (unsigned long)&Ser2UTCR0, IRQ_Ser2ICP, STD_COM_FLAGS }
#define SA1100_UART3 \
  (struct serial_state){ 0, BASE_BAUD, (unsigned long)&Ser3UTCR0, IRQ_Ser3UART, STD_COM_FLAGS }

static struct serial_state rs_table[NR_PORTS];
static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

#ifdef CONFIG_PM
static struct pm_dev* sa1100_rs_pm_dev;

static struct {
        u32 ser1utcr0, ser1utcr1, ser1utcr2, ser1utcr3;
        u32 ser3utcr0, ser3utcr1, ser3utcr2, ser3utcr3;
#ifdef CONFIG_IRDA
        u32 ser2utcr0, ser2utcr1, ser2utcr2, ser2utcr3;
#endif
} ser_save;
#endif

#ifdef CONFIG_SA1100_COLLIE
static int irda_open;
#endif

#ifdef SERIAL_FULL
#ifdef CONFIG_SA1100_HUW_WEBPANEL

static void rs_interrupt_cts(int irq, void *dev_id, struct pt_regs * regs);
static void rs_interrupt_dcd(int irq, void *dev_id, struct pt_regs * regs);
static void rs_interrupt_dsr(int irq, void *dev_id, struct pt_regs * regs);

struct huw_irq_desc
{
  int gpio_cts;
  int irq_cts;

  int gpio_dcd;
  int irq_dcd;

  int gpio_dsr;
  int irq_dsr;
};

#define SA1100_UART1_EXT \
  (struct huw_irq_desc){GPIO_GPIO20, IRQ_GPIO20, \
                        GPIO_GPIO17, IRQ_GPIO17, \
                        GPIO_GPIO18, IRQ_GPIO18}
#define SA1100_UART3_EXT \
  (struct huw_irq_desc){GPIO_GPIO15, IRQ_GPIO15, \
                        GPIO_GPIO16, IRQ_GPIO16, \
                        GPIO_GPIO19, IRQ_GPIO19}

static struct huw_irq_desc huw_irq_table[NR_PORTS];

/* helper routines */
static inline struct huw_irq_desc* get_huw_irq_desc(struct serial_state* state)
{
  return &huw_irq_table [state-rs_table];
}

static unsigned char get_MSR(struct async_struct *info)
{
  struct huw_irq_desc* irqdesc = get_huw_irq_desc(info->state);
  register unsigned char result=0;
  if (!(GPLR & irqdesc->gpio_cts))
    result |= UART_MSR_CTS;
  if (!(GPLR & irqdesc->gpio_dcd))
    result |= UART_MSR_DCD;
  if (!(GPLR & irqdesc->gpio_dsr))
    result |= UART_MSR_DSR;
  return result;
}

static unsigned char get_MCR(struct async_struct *info)
{
  register unsigned char result=0;
  switch (info->state->line)
  {
    case 0 :
    {
      if (!(GPLR & GPIO_GPIO14))
        result |= UART_MCR_RTS;
      if (!(BCR_value & BCR_PSIO_DTR3))
        result |= UART_MCR_DTR;
      break;
    }

    case 1 :
    {
      if (!(BCR_value & BCR_PSIO_RTS1))
        result |= UART_MCR_RTS;
      if (!(BCR_value & BCR_PSIO_DTR1))
        result |= UART_MCR_DTR;
      break;
    }
  }

  return result;
}

static void set_MCR(struct async_struct *info, unsigned char val)
{
  switch (info->state->line)
  {
    case 0 :
    {
      if (val & UART_MCR_RTS)
        GPCR |= GPIO_GPIO14;
      else
        GPSR |= GPIO_GPIO14;
      if (val & UART_MCR_DTR)
        BCR_clear(BCR_PSIO_DTR3);
      else
        BCR_set(BCR_PSIO_DTR3);
      break;
    }

    case 1 :
    {
      if (val & UART_MCR_RTS)
        BCR_clear(BCR_PSIO_RTS1);
      else
        BCR_set(BCR_PSIO_RTS1);
      if (val & UART_MCR_DTR)
        BCR_clear(BCR_PSIO_DTR1);
      else
        BCR_set(BCR_PSIO_DTR1);
      break;
    }
  }
}

#endif
#if defined(CONFIG_SA1100_COLLIE)

static void rs_interrupt_cts(int irq, void *dev_id, struct pt_regs * regs);
static void rs_interrupt_dsr(int irq, void *dev_id, struct pt_regs * regs);

struct huw_irq_desc
{
  int gpio_cts;
  int irq_cts;

  int gpio_dsr;
  int irq_dsr;
};

#define SA1100_UART1_EXT \
  (struct huw_irq_desc){LCM_GPIO1, IRQ_LCM_GPIO_CTS, \
                        LCM_GPIO2, IRQ_LCM_GPIO_DSR}
#define SA1100_UART3_EXT \
  (struct huw_irq_desc){LCM_GPIO1, IRQ_LCM_GPIO_CTS, \
                        LCM_GPIO2, IRQ_LCM_GPIO_DSR}

static struct huw_irq_desc huw_irq_table[NR_PORTS];

/* helper routines */
static inline struct huw_irq_desc* get_huw_irq_desc(struct serial_state* state)
{
  return &huw_irq_table [state-rs_table];
}

static unsigned char get_MSR(struct async_struct *info)
{
  struct huw_irq_desc* irqdesc = get_huw_irq_desc(info->state);
  register unsigned char result=0;

  result |= UART_MSR_DCD;

  switch (info->state->line)
  {
#ifdef CONFIG_COLLIE_TS
    case 1 :
      if (LCM_GPL & irqdesc->gpio_cts)
        result |= UART_MSR_CTS;
      if (LCM_GPL & irqdesc->gpio_dsr)
        result |= UART_MSR_DSR;
      break;
#else
    case 0:
      if (LCM_GPL & irqdesc->gpio_cts)
        result |= UART_MSR_CTS;
      if (LCM_GPL & irqdesc->gpio_dsr)
        result |= UART_MSR_DSR;
      break;
#endif
  }

  return result;
}

static unsigned char get_MCR(struct async_struct *info)
{
  register unsigned char result=0;

  switch (info->state->line)
  {
#ifdef CONFIG_COLLIE_TS
    case 1 :
    {
      if (LCM_GPL & LCM_GPIO0)
        result |= UART_MCR_RTS;
      if (LCM_GPL & LCM_GPIO3)
        result |= UART_MCR_DTR;
      break;
    }
#else
    case 0 :
    {
      if (LCM_GPL & LCM_GPIO0)
        result |= UART_MCR_RTS;
      if (LCM_GPL & LCM_GPIO3)
        result |= UART_MCR_DTR;
      break;
    }
#endif
  }

  return result;
}

static void set_MCR(struct async_struct *info, unsigned char val)
{
  switch (info->state->line)
  {
#ifdef CONFIG_COLLIE_TS
    case 1 :
    {
      if (val & UART_MCR_RTS)
        LCM_GPO |= LCM_GPIO0;
      else
        LCM_GPO &= ~LCM_GPIO0;
      if (val & UART_MCR_DTR)
        LCM_GPO |= LCM_GPIO3;
      else
        LCM_GPO &= ~LCM_GPIO3;
      break;
    }
#else
    case 0 :
    {
      if (val & UART_MCR_RTS)
        LCM_GPO |= LCM_GPIO0;
      else
        LCM_GPO &= ~LCM_GPIO0;
      if (val & UART_MCR_DTR)
        LCM_GPO |= LCM_GPIO3;
      else
        LCM_GPO &= ~LCM_GPIO3;
      break;
    }
#endif
  }
}

#endif
#endif

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/* This is to be compatible with struct async_struct from serialP.h
 * using the read_status_mask and ignore_status_mask fields.
 * Since we need them twice we split them into two 16 bits parts.
 * Those fields are int i.e. 32 bits and we only need 8 bits for each part
 * so we're OK.
 * Ex: instead of info->read_status1_mask, use _V1(info->read_status_mask).
 */
#define _V0( mask )	(((unsigned short *)(&(mask)))[0])
#define _V1( mask )	(((unsigned short *)(&(mask)))[1])

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
static DECLARE_MUTEX(tmp_buf_sem);

static inline int serial_paranoia_check(struct async_struct *info,
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

/* These all read/write 32 bits as this is what the SA1100 data book says to do */
static inline unsigned int serial_in(struct async_struct *info, int offset)
{
	return ((volatile unsigned long *)info->port)[offset];
}

static inline void serial_out(struct async_struct *info, int offset, int value)
{
	((volatile unsigned long *)info->port)[offset] = value;
}


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
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;
	
	save_flags_cli(flags);
	if (info->IER & UTCR3_TIE) {
		info->IER &= ~UTCR3_TIE;
		serial_out(info, UTCR3, info->IER);
	}
	restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;
	
	save_flags_cli(flags);
	if (info->xmit.head != info->xmit.tail
	    && info->xmit.buf
	    && !(info->IER & UTCR3_TIE)) {
		info->IER |= UTCR3_TIE;
		serial_out(info, UTCR3, info->IER);
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
static _INLINE_ void rs_sched_event(struct async_struct *info,
				  int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_COLLIE_BH);
}

static _INLINE_ void receive_chars(struct async_struct *info,
				   int *status0, int *status1,
				   struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	int ignored = 0;
	struct	async_icount *icount;

	icount = &info->state->icount;
	do {
		ch = serial_in(info, UART_RX);
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			break;
		*tty->flip.char_buf_ptr = ch;
		icount->rx++;
		
#ifdef SERIAL_DEBUG_INTR
		printk("DR%02x:%02x/%02x...", ch, *status0, *status1);
#endif
		*tty->flip.flag_buf_ptr = 0;
		if ((*status1 & (UTSR1_PRE | UTSR1_FRE | UTSR1_ROR)) ||
		    (*status0 & (UTSR0_RBB | UTSR0_REB))) {
			/*
			 * For statistics only
			 */
			if(*status0 & UTSR0_RBB) {
				icount->brk++;
				/* Clear break signal by writing break bit */
				serial_out(info, UTSR0, UTSR0_RBB);

				/* if we have an outstanding REB signal, clear it already */
 				if(*status0 & UTSR0_REB)
					serial_out(info,UTSR0,UTSR0_REB);

				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
#if defined(CONFIG_SERIAL_COLLIE_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
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
			} else if (*status0 & UTSR0_REB) {
				/* Clear break by writing bit */
			        serial_out(info, UTSR0, UTSR0_REB);
			} else if (*status1 & UTSR1_PRE)
			        icount->parity++;
			else if (*status1 & UTSR1_FRE)
				icount->frame++;
			if (*status1 & UTSR1_ROR)
				icount->overrun++;

			/*
			 * Now check to see if character should be
			 * ignored, and mask off conditions which
			 * should be ignored.
			 */
			if (*status1 & _V1(info->ignore_status_mask)) {
				if (++ignored > 100)
					break;
				goto ignore_char;
			}
			*status1 &= _V1(info->read_status_mask);

#ifdef CONFIG_SERIAL_COLLIE_CONSOLE
			if (info->line == sercons.index) {
				/* Recover the break flag from console xmit */
				*status0 |= lsr_break_flag;
				lsr_break_flag = 0;
			}
#endif
		
			if (*status0 & (UTSR0_RBB)) {
#ifdef SERIAL_DEBUG_INTR
				printk("handling break....");
#endif
				*tty->flip.flag_buf_ptr = TTY_BREAK;
			} else if (*status1 & UTSR1_PRE)
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			else if (*status1 & UTSR1_FRE)
				*tty->flip.flag_buf_ptr = TTY_FRAME;
			if (*status1 & UTSR1_ROR) {
				/*
				 * Overrun is special, since it's
				 * reported immediately, and doesn't
				 * affect the current character
				 */
				if (tty->flip.count < TTY_FLIPBUF_SIZE) {
					tty->flip.count++;
					tty->flip.flag_buf_ptr++;
					tty->flip.char_buf_ptr++;
					*tty->flip.flag_buf_ptr = TTY_OVERRUN;
				} else {
					goto ignore_char;
				}
			}
		}
#if defined(CONFIG_SERIAL_COLLIE_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
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
		tty->flip.flag_buf_ptr++;
		tty->flip.char_buf_ptr++;
		tty->flip.count++;
	ignore_char:
		*status1 = serial_in(info, UTSR1);
		*status0 = serial_in(info, UTSR0);
	} while (*status1 & UTSR1_RNE);
	tty_flip_buffer_push(tty);
}

static _INLINE_ void transmit_chars(struct async_struct *info, int *intr_done)
{
	int count;
	
	if (info->x_char) {
		serial_out(info, UART_TX, info->x_char);
		info->state->icount.tx++;
		info->x_char = 0;
		if (intr_done)
			*intr_done = 0;
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
   	        if (info->IER&UTCR3_TIE) {
		    info->IER &= ~UTCR3_TIE;
		    serial_out(info, UTCR3, info->IER);
                }
 
                /* No more TFS events to service */
		_V0(info->read_status_mask)&=~UTSR0_TFS;
		return;
	}
	
	/* Tried using FIFO (not checking TNF) for fifo fill: still had the
	 * '4 bytes repeated' problem.
	 */
	count = info->xmit_fifo_size;
	while(serial_in(info, UTSR1) & UTSR1_TNF) {
		serial_out(info, UART_TX, info->xmit.buf[info->xmit.tail]);
		info->xmit.tail = (info->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
                info->state->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	};

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
	        if (info->IER & UTCR3_TIE) {
		    info->IER &= ~UTCR3_TIE;
		    serial_out(info, UTCR3, info->IER);
		}

		/* No more TFS events to service */
                _V0(info->read_status_mask)&=~UTSR0_TFS;
	}
}

/*
 * This is the serial driver's interrupt routine for a single port
 */
static void rs_interrupt_single(int irq, void *dev_id, struct pt_regs * regs)
{
	int status0, status1;
	int pass_counter = 0;
	struct async_struct * info;
	
#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt_single(%d)...", irq);
#endif
	
	info = (struct async_struct *)dev_id;
	if (!info || !info->tty)
		return;

	/* We want to know about TFS events for now: we'll clear this later
	   if we have nothing to send */
	_V0(info->read_status_mask)|=  (UTSR0_TFS);
	do {
		status0 = serial_in(info, UTSR0) & _V0(info->read_status_mask);
		status1 = serial_in(info, UTSR1) & _V1(info->read_status_mask);
#ifdef SERIAL_DEBUG_INTR
		printk("status0 = %x (%x), status1 = %x (%x)...", status0, serial_in(info,UTSR0), status1, serial_in(info,UTSR1));
#endif
		if (status1 & UTSR1_RNE)
			receive_chars(info, &status0, &status1, regs);

		if (status0 & UTSR0_TFS)
			transmit_chars(info, 0);

		if (status0 & UTSR0_REB) {
		  /* Clear break signal by writing bit */
		  serial_out(info,UTSR0,UTSR0_REB);
		}

		/* Clear receiver idle state if necessary */
		if (status0 & UTSR0_RID) {
			/* need to write a 1 back to clear it */
			serial_out(info, UTSR0, UTSR0_RID);
		}
		if (pass_counter++ > RS_ISR_PASS_LIMIT) {
			if (status0 || status1) printk("out-status0 = %x, status1 = %x...", status0, status1);
			printk("rs_single loop break.\n");
			break;
		}
		status0 = serial_in(info, UTSR0) & _V0(info->read_status_mask);
		status1 = serial_in(info, UTSR1) & _V1(info->read_status_mask);
#ifdef SERIAL_DEBUG_INTR
		if (status0 || status1) printk("out-status0 = %x, status1 = %x...", status0, status1);
#endif

	} while (status0 || status1);
	info->last_active = jiffies;

#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}

#ifdef SERIAL_FULL
#ifdef CONFIG_SA1100_HUW_WEBPANEL

static void rs_interrupt_cts(int irq, void *dev_id, struct pt_regs * regs)
{
	struct async_struct * info;
	struct async_icount * icount;
	struct huw_irq_desc * irqdesc;
	int                   cts_active;

	info = (struct async_struct *)dev_id;
	if (!info || !info->tty)
		return;

	irqdesc    = get_huw_irq_desc(info->state);
	icount     = &info->state->icount;
	cts_active = get_MSR(info) & UART_MSR_CTS;
	icount->cts++;

	wake_up_interruptible(&info->delta_msr_wait);

	if (info->flags & ASYNC_CTS_FLOW) {
		if (info->tty->hw_stopped) {
 			if (cts_active) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
 				printk("CTS tx start...");
#endif
 				info->tty->hw_stopped = 0;
 				rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
 			}
 		} else {
 			if (!cts_active) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
 				printk("CTS tx stop...");
#endif
 				info->tty->hw_stopped = 1;
 			}
 		}
	}
}

static void rs_interrupt_dcd(int irq, void *dev_id, struct pt_regs * regs)
{
	struct async_struct * info;
	struct async_icount * icount;
	struct huw_irq_desc * irqdesc;
	int                   dcd_active;

	info = (struct async_struct *)dev_id;
	if (!info || !info->tty)
		return;

	irqdesc    = get_huw_irq_desc(info->state);
	icount     = &info->state->icount;
	dcd_active = get_MSR(info) & UART_MSR_DCD;
	icount->dcd++;

	if (info->flags & ASYNC_CHECK_CD) {
#if (defined(SERIAL_DEBUG_OPEN) || defined(SERIAL_DEBUG_INTR))
		printk("ttys%d CD now %s...", info->line,
		       dcd_active ? "on" : "off");
#endif		
		if (dcd_active)
			wake_up_interruptible(&info->open_wait);
		else if (!((info->flags & ASYNC_CALLOUT_ACTIVE) &&
			   (info->flags & ASYNC_CALLOUT_NOHUP))) {
#ifdef SERIAL_DEBUG_OPEN
			printk("doing serial hangup...");
#endif
			tty_hangup(info->tty);
		}
	}
}

static void rs_interrupt_dsr(int irq, void *dev_id, struct pt_regs * regs)
{
	struct async_struct * info;
	struct async_icount * icount;
	struct huw_irq_desc * irqdesc;

	info = (struct async_struct *)dev_id;
	if (!info || !info->tty)
		return;

	irqdesc = get_huw_irq_desc(info->state);
	icount  = &info->state->icount;
	icount->dsr++;
}

#endif

#ifdef CONFIG_SA1100_COLLIE

static void rs_interrupt_cts(int irq, void *dev_id, struct pt_regs * regs)
{
	struct async_struct * info;
	struct async_icount * icount;
	struct huw_irq_desc * irqdesc;
	int                   cts_active;

	info = (struct async_struct *)dev_id;
	if (!info || !info->tty)
		return;

	irqdesc    = get_huw_irq_desc(info->state);
	icount     = &info->state->icount;
	cts_active = get_MSR(info) & UART_MSR_CTS;
	icount->cts++;

	wake_up_interruptible(&info->delta_msr_wait);

	if (info->flags & ASYNC_CTS_FLOW) {
		if (info->tty->hw_stopped) {
 			if (cts_active) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
 				printk("CTS tx start...");
#endif
 				info->tty->hw_stopped = 0;
 				rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
 			}
 		} else {
 			if (!cts_active) {
#if (defined(SERIAL_DEBUG_INTR) || defined(SERIAL_DEBUG_FLOW))
 				printk("CTS tx stop...");
#endif
 				info->tty->hw_stopped = 1;
 			}
 		}
	}
}

static void rs_interrupt_dsr(int irq, void *dev_id, struct pt_regs * regs)
{
	struct async_struct * info;
	struct async_icount * icount;
	struct huw_irq_desc * irqdesc;

	info = (struct async_struct *)dev_id;
	if (!info || !info->tty)
		return;

	irqdesc = get_huw_irq_desc(info->state);
	icount  = &info->state->icount;
	icount->dsr++;
}

#endif

#endif


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
	struct async_struct	*info = (struct async_struct *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
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

static int startup(struct async_struct * info)
{
	unsigned long flags;
	int	retval=0;
	struct serial_state *state= info->state;
	unsigned long page;
#ifdef SERIAL_FULL
#ifdef CONFIG_SA1100_HUW_WEBPANEL
	struct huw_irq_desc *irqdesc=0;
	int                  irqreq=0;  /* number of IRQs already requested */
#endif
#ifdef CONFIG_SA1100_COLLIE
	struct huw_irq_desc *irqdesc=0;
	int                  irqreq=0;  /* number of IRQs already requested */
#endif
#endif

	page = get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	save_flags_cli(flags);

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (!state->port || !state->type) {
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

	retval = request_irq(state->irq, rs_interrupt_single, SA_INTERRUPT,
			     "serial", (void*)info);
	if (retval) {
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR,
					&info->tty->flags);
			retval = 0;
		}
		goto errout;
	}
#ifdef SERIAL_FULL
#ifdef CONFIG_SA1100_HUW_WEBPANEL
	++irqreq;
	irqdesc = get_huw_irq_desc(state);

	retval = request_irq(irqdesc->irq_cts, rs_interrupt_cts, SA_INTERRUPT,
			     "serial_cts", (void*)info);
	if (retval) {
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR,
					&info->tty->flags);
			retval = 0;
		}
		goto errout;
	}
	++irqreq;
	
	retval = request_irq(irqdesc->irq_dcd, rs_interrupt_dcd, SA_INTERRUPT,
			     "serial_dcd", (void*)info);
	if (retval) {
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR,
					&info->tty->flags);
			retval = 0;
		}
		goto errout;
	}
	++irqreq;
	
	retval = request_irq(irqdesc->irq_dsr, rs_interrupt_dsr, SA_INTERRUPT,
			     "serial_dsr", (void*)info);
	if (retval) {
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR,
					&info->tty->flags);
			retval = 0;
		}
		goto errout;
	}
	++irqreq;
#endif
#ifdef CONFIG_SA1100_COLLIE
    switch (info->state->line)
    {
#ifdef CONFIG_COLLIE_TS
    case 1:
#else
    case 0:
#endif
	++irqreq;
	irqdesc = get_huw_irq_desc(state);

	retval = request_irq(irqdesc->irq_cts, rs_interrupt_cts, SA_INTERRUPT,
			     "serial_cts", (void*)info);
	if (retval) {
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR,
					&info->tty->flags);
			retval = 0;
		}
		goto errout;
	}
	++irqreq;
	
	retval = request_irq(irqdesc->irq_dsr, rs_interrupt_dsr, SA_INTERRUPT,
			     "serial_dsr", (void*)info);
	if (retval) {
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR,
					&info->tty->flags);
			retval = 0;
		}
		goto errout;
	}
	++irqreq;
    }
#endif
#endif

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit.head = info->xmit.tail = 0;

	/*
	 * Set up the tty->alt_speed kludge
	 */
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

#ifdef CONFIG_SA1100_COLLIE
	info->tty->termios->c_cflag |= CRTSCTS;
#endif
	
	/*
	 * and set the speed of the serial port
	 */
	change_speed(info);

#ifdef SERIAL_FULL
	info->MCR = 0;
	if (info->tty->termios->c_cflag & CBAUD)
		info->MCR = UART_MCR_DTR | UART_MCR_RTS;
#if defined(CONFIG_SA1100_HUW_WEBPANEL)
	set_MCR(info, info->MCR);
#endif
#if defined(CONFIG_SA1100_COLLIE)
    switch (info->state->line)
    {
#ifdef CONFIG_COLLIE_TS
    case 1:
#else
    case 0:
#endif
      set_MCR(info, info->MCR);
      break;
    }
#endif
#endif

	/*
	 * Finally, enable interrupts
	 */
	info->IER = UTCR3_TXE | UTCR3_RXE | UTCR3_RIE;
	serial_out(info, UTSR0, 0xff);
	serial_out(info, UTCR3, info->IER);

#ifdef SERIAL_FULL
#ifdef CONFIG_SA1100_HUW_WEBPANEL
	enable_irq (irqdesc->irq_cts);
	enable_irq (irqdesc->irq_dcd);
	enable_irq (irqdesc->irq_dsr);
	++irqreq;
#endif
#ifdef CONFIG_SA1100_COLLIE
    switch (info->state->line) {
#ifdef CONFIG_COLLIE_TS
    case 1:
#else
    case 0:
#endif
	enable_irq (irqdesc->irq_cts);
	enable_irq (irqdesc->irq_dsr);
	++irqreq;
	break;
    }
#endif
#endif
	
	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;
	
errout:
#ifdef SERIAL_FULL
#ifdef CONFIG_SA1100_HUW_WEBPANEL
  switch (irqreq)
  {
    case 5 :
	    disable_irq (irqdesc->irq_dsr);
            disable_irq (irqdesc->irq_dcd);
	    disable_irq (irqdesc->irq_cts);
    case 4 :
      free_irq(irqdesc->irq_dsr, (void*)info);
    case 3 :
      free_irq(irqdesc->irq_dcd, (void*)info);
    case 2 :
      free_irq(irqdesc->irq_cts, (void*)info);
    case 1 :
      free_irq(state->irq, (void*)info);
  }
#endif
#ifdef CONFIG_SA1100_COLLIE
  switch (info->state->line) {
#ifdef CONFIG_COLLIE_TS
  case 1:
#else
  case 0:
#endif
    switch (irqreq) {
    case 4 :
	disable_irq (irqdesc->irq_dsr);
	disable_irq (irqdesc->irq_cts);
    case 3:
	free_irq(irqdesc->irq_dsr, (void*)info);
    case 2:
	free_irq(irqdesc->irq_cts, (void*)info);
    case 1:
	free_irq(state->irq, (void*)info);
    }
    break;
  }
#endif
#endif
	restore_flags(flags);
	return retval;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct async_struct * info)
{
	unsigned long	flags;
	struct serial_state *state;
#ifdef SERIAL_FULL
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
	struct huw_irq_desc *irqdesc;
#endif
#endif

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	state = info->state;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....", info->line,
	       state->irq);
#endif
	
	save_flags_cli(flags);

#ifdef SERIAL_FULL
	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */
	wake_up_interruptible(&info->delta_msr_wait);
#endif

	info->IER &= ~(UTCR3_RIE|UTCR3_TIE|UTCR3_BRK);
	serial_out(info, UTCR3, info->IER);	/* disable all intrs */

#ifdef SERIAL_FULL
#ifdef CONFIG_SA1100_HUW_WEBPANEL
  irqdesc = get_huw_irq_desc(state);
  free_irq(irqdesc->irq_dsr, (void*) info);
  free_irq(irqdesc->irq_dcd, (void*) info);
  free_irq(irqdesc->irq_cts, (void*) info);
#endif
#ifdef CONFIG_SA1100_COLLIE
  switch (info->state->line) {
#ifdef CONFIG_COLLIE_TS
  case 1:
#else
  case 0:
#endif
    irqdesc = get_huw_irq_desc(state);
    free_irq(irqdesc->irq_dsr, (void*) info);
    free_irq(irqdesc->irq_cts, (void*) info);
    break;
  }
#endif
#endif	
	free_irq(state->irq, (void*)info);

	if (info->xmit.buf) {
		unsigned long pg = (unsigned long) info->xmit.buf;
		info->xmit.buf = 0;
		free_page(pg);
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

#ifdef CONFIG_SERIAL_COLLIE_CONSOLE
	if (sercons.index != state->line) 
#endif
		serial_out(info, UTCR3, 0);	/* disable UART */


	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct async_struct *info)
{
	unsigned int port;
	int	quot = 0, baud_base, baud;
	unsigned cflag, cval;
	int	bits = 0;
	unsigned long	flags;

	if (!info->tty || !info->tty->termios)
		return;

	cflag = info->tty->termios->c_cflag;
	if (!(port = info->port))
		return;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	      case CS7: cval = 0x00; break;
	      case CS8:
	      default:  cval = UTCR0_DSS; break;
	}
	if (cflag & CSTOPB) {
		cval |= UTCR0_SBS;
	}
	if (cflag & PARENB)
		cval |= UTCR0_PE;
	if (!(cflag & PARODD))
		cval |= UTCR0_OES;

	/* Determine divisor based on baud rate */
	baud = tty_get_baud_rate(info->tty);
	baud_base = info->state->baud_base;
	if (baud == 38400 &&
	    ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST))
		quot = info->state->custom_divisor;
	else {
		if (baud == 134)
			/* Special case since 134 is really 134.5 */
			quot = (2*baud_base / 269);
		else if (baud)
			quot = baud_base / baud;
	}
	/* If the quotient is ever zero, default to 9600 bps */
	if (!quot)
		quot = baud_base / 9600;
	info->quot = quot;
	info->timeout = ((info->xmit_fifo_size*HZ*bits*quot) / baud_base);
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

	/* CTS flow control flag - we have no modem status */
	if (cflag & CRTSCTS) {
		info->flags |= ASYNC_CTS_FLOW;
	} else
		info->flags &= ~ASYNC_CTS_FLOW;
	if (cflag & CLOCAL)
		info->flags &= ~ASYNC_CHECK_CD;
	else {
		info->flags |= ASYNC_CHECK_CD;
	}
	serial_out(info, UTCR3, info->IER);

	/*
	 * Set up parity check flag
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	_V0(info->read_status_mask) = UTSR0_TFS | UTSR0_RFS | UTSR0_RID | UTSR0_EIF;
	_V1(info->read_status_mask) = /*UTSR1_TBY |*/ UTSR1_RNE | /*UTSR1_TNF |*/ UTSR1_ROR;
	if (I_INPCK(info->tty))
		_V1(info->read_status_mask) |= UTSR1_PRE | UTSR1_FRE;

	/* We always need to know about breaks, so we can clear the bits */
	/*if (I_BRKINT(info->tty) || I_PARMRK(info->tty))*/
	_V0(info->read_status_mask) |= UTSR0_RBB | UTSR0_REB;
	
	/*
	 * Characters to ignore
	 */
	_V0(info->ignore_status_mask) = 0;
	_V1(info->ignore_status_mask) = 0;

	if (I_IGNBRK(info->tty)) {
		_V0(info->ignore_status_mask) |= UTSR0_RBB | UTSR0_REB;
		_V0(info->read_status_mask) |= UTSR0_RBB | UTSR0_REB;
		/*
		 * If we're ignore parity and break indicators, ignore 
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->tty)) {
			_V1(info->ignore_status_mask) |= UTSR1_PRE | UTSR1_FRE | UTSR1_ROR;
			_V1(info->read_status_mask) |= UTSR1_PRE | UTSR1_FRE | UTSR1_ROR;
		}
	}

	/* Change the speed */
	{
	  unsigned int status1, IER_copy;
	  save_flags_cli(flags);
	  do {
	    status1 = serial_in(info, UTSR1);
	  } while ( status1 & UTSR1_TBY );
    
	  /* must disable rx and tx enable to switch baud rate */
	  IER_copy = serial_in(info, UTCR3);
	  serial_out(info, UTCR3, 0);

	  /* set the parity, stop bits, data size */
	  serial_out(info, UTCR0, cval);

	  /* set the baud rate */
	  quot -= 1;	/* divisor = divisor - 1 */
	  serial_out(info, UTCR1, (quot >> 8)&0xf);	/* MS of divisor */
	  serial_out(info, UTCR2, quot & 0xff);	/* LS of divisor */

	  /* clear out any errors by writing the line status register */
	  serial_out(info, UTSR0, 0xff);

	  /* enable rx, tx and error interrupts */
	  serial_out(info, UTCR3, IER_copy);

	  restore_flags(flags);
	}
}

static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_put_char"))
		return;

	if (!tty || !info->xmit.buf)
		return;

	save_flags_cli(flags);
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
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
				
	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	if (info->xmit.head == info->xmit.tail
	    || tty->stopped
	    || tty->hw_stopped
	    || !info->xmit.buf)
		return;

  	save_flags_cli(flags);
	if (!(info->IER&UTCR3_TIE)) {
	    info->IER |= UTCR3_TIE;
	    serial_out(info, UTCR3, info->IER);
	}
	restore_flags(flags);
}

static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	struct async_struct *info = (struct async_struct *)tty->driver_data;
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
	    && !tty->hw_stopped) {
		save_flags_cli(flags);
		if (!(info->IER & UTCR3_TIE)) {
			info->IER |= UTCR3_TIE;
			serial_out(info, UTCR3, info->IER);
		}
		restore_flags(flags);
	}
	return ret;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
				
	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;
	save_flags(flags); cli();
	info->xmit.head = info->xmit.tail = 0;
	restore_flags(flags);
	wake_up_interruptible(&tty->write_wait);
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
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_send_char"))
		return;

	info->x_char = ch;
	if (ch) {
		/* Make sure transmit interrupts are on */
		info->IER |= UTCR3_TIE;
		serial_out(info, UTCR3, info->IER);
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
	struct async_struct *info = (struct async_struct *)tty->driver_data;
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
#ifdef SERIAL_FULL
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR &= ~UART_MCR_RTS;
#else /* ZZZ not implemented - no hw lines */
#endif

	save_flags(flags); cli();
#ifdef SERIAL_FULL
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
	set_MCR(info, info->MCR);
#else
	serial_out(info, UART_MCR, info->MCR);
#endif
#endif
	restore_flags(flags);
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
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
#ifdef SERIAL_FULL
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR |= UART_MCR_RTS;
#else /* ZZZ not implemented */
#endif
	save_flags(flags); cli();
#ifdef SERIAL_FULL
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
	set_MCR(info, info->MCR);
#else
	serial_out(info, UART_MCR, info->MCR);
#endif
#endif
	restore_flags(flags);
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

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
static int get_lsr_info(struct async_struct * info, unsigned int *value)
{
	unsigned char status1;
	unsigned int result;
	unsigned long flags;

	save_flags(flags); cli();
	status1 = serial_in(info, UTSR1);
	restore_flags(flags);
	result = ((status1 & UTSR1_TBY) ? 0 : TIOCSER_TEMT);
	return put_user(result,value);
}


static int get_modem_info(struct async_struct * info, unsigned int *value)
{
	unsigned int result=0;
#ifdef SERIAL_FULL
	unsigned char control, status;
	unsigned long flags;

	control = info->MCR;
	save_flags(flags); cli();
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
	status = get_MSR(info);
#else
	status = serial_in(info, UART_MSR);
#endif
	restore_flags(flags);
	result =  ((control & UART_MCR_RTS) ? TIOCM_RTS : 0)
		| ((control & UART_MCR_DTR) ? TIOCM_DTR : 0)
#ifdef TIOCM_OUT1
		| ((control & UART_MCR_OUT1) ? TIOCM_OUT1 : 0)
		| ((control & UART_MCR_OUT2) ? TIOCM_OUT2 : 0)
#endif
		| ((status  & UART_MSR_DCD) ? TIOCM_CAR : 0)
		| ((status  & UART_MSR_RI) ? TIOCM_RNG : 0)
		| ((status  & UART_MSR_DSR) ? TIOCM_DSR : 0)
		| ((status  & UART_MSR_CTS) ? TIOCM_CTS : 0);
#else /* ZZZ no modem lines */
#endif
	return put_user(result,value);
}

static int set_modem_info(struct async_struct * info, unsigned int cmd,
			  unsigned int *value)
{
	int error;
	unsigned int arg;
	unsigned long flags;

	error = get_user(arg, value);
	if (error)
		return error;
	switch (cmd) {
#ifdef SERIAL_FULL
	case TIOCMBIS: 
		if (arg & TIOCM_RTS)
			info->MCR |= UART_MCR_RTS;
		if (arg & TIOCM_DTR)
			info->MCR |= UART_MCR_DTR;
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			info->MCR &= ~UART_MCR_RTS;
		if (arg & TIOCM_DTR)
			info->MCR &= ~UART_MCR_DTR;
		break;
	case TIOCMSET:
		info->MCR = ((info->MCR & ~(UART_MCR_RTS |
					    UART_MCR_DTR))
			     | ((arg & TIOCM_RTS) ? UART_MCR_RTS : 0)
			     | ((arg & TIOCM_DTR) ? UART_MCR_DTR : 0));
		break;
#else /* NO LINES! */
#endif

	default:
	  return -EINVAL;
	}

	save_flags(flags); cli();
#ifdef SERIAL_FULL
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
	set_MCR(info, info->MCR);
#else
	serial_out(info, UART_MCR, info->MCR);
#endif
#endif
	restore_flags(flags);
	return 0;
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
static void rs_break(struct tty_struct *tty, int break_state)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_break"))
		return;

	if (!info->port)
		return;
	save_flags(flags); cli();
	if (break_state == -1)
		serial_out(info, UTCR3,
			   serial_in(info, UTCR3) | UTCR3_BRK);
	else
		serial_out(info, UTCR3,
			   serial_in(info, UTCR3) & ~UTCR3_BRK);
	restore_flags(flags);
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
                    unsigned int cmd, unsigned long arg)
{
        int error;
        struct async_struct * info = (struct async_struct *)tty->driver_data;
        struct async_icount cprev, cnow;        /* kernel counter temps */
        struct serial_icounter_struct *p_cuser; /* user space */
        unsigned long flags;

        if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
                return -ENODEV;

        if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
            (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
            (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
                if (tty->flags & (1 << TTY_IO_ERROR))
                    return -EIO;
        }

        switch (cmd) {
                case TIOCMGET:
                        return get_modem_info(info, (unsigned int *) arg);
                case TIOCMBIS:
                case TIOCMBIC:
                case TIOCMSET:
                        return set_modem_info(info, cmd, (unsigned int *) arg);
                case TIOCGSERIAL:
                        return -EINVAL;
                case TIOCSSERIAL:
			return -EINVAL;
		case TIOCSERCONFIG:
			return -EINVAL;

		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned int *) arg);

		case TIOCSERGSTRUCT:
			if (copy_to_user((struct async_struct *) arg,
					 info, sizeof(struct async_struct)))
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
			p_cuser = (struct serial_icounter_struct *) arg;
			error = put_user(cnow.cts, &p_cuser->cts);
			if (error) return error;
			error = put_user(cnow.dsr, &p_cuser->dsr);
			if (error) return error;
			error = put_user(cnow.rng, &p_cuser->rng);
			if (error) return error;
			error = put_user(cnow.dcd, &p_cuser->dcd);
			if (error) return error;
			error = put_user(cnow.rx, &p_cuser->rx);
			if (error) return error;
			error = put_user(cnow.tx, &p_cuser->tx);
			if (error) return error;
			error = put_user(cnow.frame, &p_cuser->frame);
			if (error) return error;
			error = put_user(cnow.overrun, &p_cuser->overrun);
			if (error) return error;
			error = put_user(cnow.parity, &p_cuser->parity);
			if (error) return error;
			error = put_user(cnow.brk, &p_cuser->brk);
			if (error) return error;
			error = put_user(cnow.buf_overrun, &p_cuser->buf_overrun);
			if (error) return error;
			return 0;

		case TIOCSERGWILD:
		case TIOCSERSWILD:
			/* "setserial -W" is called in Debian boot */
			printk ("TIOCSER?WILD ioctl obsolete, ignored.\n");
			return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (   (tty->termios->c_cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag) 
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info);

	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(tty->termios->c_cflag & CBAUD)) {

#ifdef SERIAL_FULL
	  info->MCR &= ~(UART_MCR_DTR|UART_MCR_RTS);
#else  /* no serial control lines */
#endif
		save_flags(flags); cli();
#ifdef SERIAL_FULL
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
	set_MCR(info, info->MCR);
#else
	serial_out(info, UART_MCR, info->MCR);
#endif
#endif
	
	restore_flags(flags);
	}
	
	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (tty->termios->c_cflag & CBAUD)) {
#ifdef SERIAL_FULL
		info->MCR |= UART_MCR_DTR;
		if (!(tty->termios->c_cflag & CRTSCTS) ||
		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			info->MCR |= UART_MCR_RTS;
		}
#else
#endif
		save_flags(flags); cli();
#ifdef SERIAL_FULL
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
	set_MCR(info, info->MCR);
#else
	serial_out(info, UART_MCR, info->MCR);
#endif
#endif
	restore_flags(flags);
	}
	
	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}

#if 0
	/*
	 * No need to wake up processes in open wait, since they
	 * sample the CLOCAL flag once, and don't recheck it.
	 * XXX  It's not clear whether the current behavior is correct
	 * or not.  Hence, this may change.....
	 */
	if (!(old_termios->c_cflag & CLOCAL) &&
	    (tty->termios->c_cflag & CLOCAL))
		wake_up_interruptible(&info->open_wait);
#endif
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
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct serial_state *state;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;

	state = info->state;
	
	save_flags_cli(flags);
	
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
	info->IER &= ~UTCR3_RIE;
	_V0(info->read_status_mask) &= ~UTSR0_RFS;
	_V1(info->read_status_mask) &= ~UTSR1_RNE;
	if (info->flags & ASYNC_INITIALIZED) {
		serial_out(info, UTCR3, info->IER);
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
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
	restore_flags(flags);

#ifdef CONFIG_SA1100_COLLIE
	if (info->line == 2) {
		irda_open = 0;
		sa1100_irda_set_power_collie(0);	/* power off */
	}
#endif
}

/*
 * rs_wait_until_sent() --- wait until the transmitter is empty
 */
static void rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	unsigned long orig_jiffies;
	int lsr;
	
	if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
		return;

	if (info->state->type == PORT_UNKNOWN)
		return;

	orig_jiffies = jiffies;
	while ((lsr = serial_in(info, UTSR1)) & UTSR1_TBY) {
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
		printk("lsr = %d (jiff=%lu)...", lsr, jiffies);
#endif
		current->state = TASK_INTERRUPTIBLE;
		current->counter = 0;	/* make us low-priority */
		schedule_timeout(HZ/10);
		if (signal_pending(current))
			break;
		if ((jiffies - orig_jiffies) > 1*HZ)
			break;
	}
	current->state = TASK_RUNNING;
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("lsr = %d (jiff=%lu)...done\n", lsr, jiffies);
#endif
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rs_hangup(struct tty_struct *tty)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct serial_state *state = info->state;
	
	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	state = info->state;
	
	rs_flush_buffer(tty);
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
			   struct async_struct *info)
{
	struct serial_state *state = info->state;
	int		retval=0;
	int		do_clocal = 0;
#ifdef SERIAL_FULL
	DECLARE_WAITQUEUE(wait, current);
	int             extra_count = 0;
	unsigned long	flags;
#endif

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


#ifdef SERIAL_FULL
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
		    (tty->termios->c_cflag & CBAUD))
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
		  set_MCR(info, get_MCR(info) |
				   (UART_MCR_DTR | UART_MCR_RTS));
#else
			serial_out(info, UART_MCR,
				   serial_in(info, UART_MCR) |
				   (UART_MCR_DTR | UART_MCR_RTS));
#endif
		restore_flags(flags);
		current->state = TASK_INTERRUPTIBLE;
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
		    (do_clocal || 
#if defined(CONFIG_SA1100_HUW_WEBPANEL) || defined(CONFIG_SA1100_COLLIE)
			 (get_MSR(info) & UART_MSR_DCD)))
#else
		  (serial_in(info, UART_MSR) &
		   UART_MSR_DCD)))
#endif
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
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (extra_count)
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
	       info->line, state->count);
#endif

#else  /* We have no DCD */	
#endif /* DCD stuff we if 0'ed */

	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

static int get_async_struct(int line, struct async_struct **ret_info)
{
	struct async_struct *info;
	struct serial_state *sstate;

	sstate = rs_table + line;
	sstate->count++;
	if (sstate->info) {
		*ret_info = sstate->info;
		return 0;
	}
	info = kmalloc(sizeof(struct async_struct), GFP_KERNEL);
	if (!info) {
		sstate->count--;
		return -ENOMEM;
	}
	memset(info, 0, sizeof(struct async_struct));
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	init_waitqueue_head(&info->delta_msr_wait);
	info->magic = SERIAL_MAGIC;
	info->port = sstate->port;
	info->flags = sstate->flags;
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
 */
static int rs_open(struct tty_struct *tty, struct file * filp)
{
	struct async_struct	*info;
	int 			retval, line;
	unsigned long		page;

	MOD_INC_USE_COUNT;
	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;
	retval = get_async_struct(line, &info);
	if (retval)
		return retval;
	tty->driver_data = info;
	info->tty = tty;
	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->state->count);
#endif
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	if (!tmp_buf) {
		page = get_free_page(GFP_KERNEL);
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
		change_speed(info);
	}
#ifdef CONFIG_SERIAL_COLLIE_CONSOLE
	if (sercons.cflag && sercons.index == line) {
		tty->termios->c_cflag = sercons.cflag;
		sercons.cflag = 0;
		change_speed(info);
	}
#endif
	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef CONFIG_SA1100_COLLIE
	if (line == 2) {
		irda_open = 1;
		sa1100_irda_set_power_collie(3);	/* power on */
	}
#endif

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open ttys%d successful...", info->line);
#endif
	return 0;
}


/*
 * ---------------------------------------------------------------------
 * sa1100_rs_init() and friends
 *
 * sa1100_rs_init() is called at boot-time to initialize the serial driver.
 * ---------------------------------------------------------------------
 */


static inline int baud_to_c(int baudrate)
{
	switch(baudrate) {
	    case 1200:   return B1200;
	    case 2400:   return B2400;
	    case 4800:   return B4800;
	    case 9600:   return B9600;
	    case 19200:  return B19200;
	    case 38400:  return B38400;
	    case 57600:  return B57600;
	    case 115200: return B115200;
	    case 230400: return B230400;
	    default:     return B9600;
	}
}


static void __init assign_ports(void)
{
	if( machine_is_brutus() ) {
		rs_table[0] = SA1100_UART1;
		rs_table[1] = SA1100_UART3;
	} else if( machine_is_yopy() ) {
		rs_table[0] = SA1100_UART3;
	} else if( machine_is_assabet() ) {
		if( !machine_has_neponset() ){
			rs_table[0] = SA1100_UART1;
			rs_table[1] = SA1100_UART3;
		}else{
			rs_table[0] = SA1100_UART3;
			rs_table[1] = SA1100_UART1;
		}
		rs_table[2] = SA1100_UART2;

#ifdef CONFIG_SA1100_COLLIE
	}
	else if( machine_is_collie() ) {
#ifdef CONFIG_COLLIE_TS
		rs_table[0] = SA1100_UART1;
		rs_table[1] = SA1100_UART3;
#else
		rs_table[0] = SA1100_UART3;
		rs_table[1] = SA1100_UART1;
#endif
#if defined(CONFIG_IRDA)
		rs_table[2] = SA1100_UART2;
#endif

#ifdef SERIAL_FULL
#ifdef CONFIG_COLLIE_TS
		huw_irq_table[0] = SA1100_UART1_EXT;
		huw_irq_table[1] = SA1100_UART3_EXT;
#else
		huw_irq_table[0] = SA1100_UART3_EXT;
		huw_irq_table[1] = SA1100_UART1_EXT;
#endif
#endif
#endif
	} else if( machine_is_pangolin() ){
		rs_table[0] = SA1100_UART1;
		rs_table[1] = SA1100_UART3;
		rs_table[2] = SA1100_UART2;
 	}
#ifdef CONFIG_SA1100_HUW_WEBPANEL
	else if( machine_is_huw_webpanel() ){
		rs_table[0] = SA1100_UART3;
		rs_table[1] = SA1100_UART1;
#ifdef SERIAL_FULL
		huw_irq_table[0] = SA1100_UART3_EXT;
		huw_irq_table[1] = SA1100_UART1_EXT;
#endif
 	}
#endif
	else if( machine_is_freebird() ) {
		rs_table[0] = SA1100_UART1;
		rs_table[1] = SA1100_UART3;
		rs_table[2] = SA1100_UART2;
 	} else if( machine_is_empeg() ){
		rs_table[0] = SA1100_UART1;
		rs_table[1] = SA1100_UART3;
		rs_table[2] = SA1100_UART2;
	} else if( machine_is_nanoengine() ){
		rs_table[0] = SA1100_UART1;
		rs_table[1] = SA1100_UART2;
		rs_table[2] = SA1100_UART3;
	} else if( machine_is_pleb() ){
		rs_table[0] = SA1100_UART3;
	} else if( machine_is_graphicsclient() ){
		rs_table[0] = SA1100_UART3;
		rs_table[1] = SA1100_UART1;
		rs_table[2] = SA1100_UART2;
	} else if( machine_is_pfs168() ){
		rs_table[0] = SA1100_UART1;
		rs_table[1] = SA1100_UART3;
		rs_table[2] = SA1100_UART2;
	} else {
		rs_table[0] = SA1100_UART3;
		rs_table[1] = SA1100_UART1;
	}
}


/*
 * This routine is called by sa1100_rs_init() to initialize a specific 
 * serial port.
 */
static void __init autoconfig(struct serial_state * state, int adv)
{
	unsigned long flags;

	state->type = PORT_UNKNOWN;
	
	if (!state->port)
		return;
		
	save_flags_cli(flags);

	switch(state->port) {
	    case (int)&Ser3UTCR0:
		/* uart serial port 3 on sa1100 */
		if( adv )
			printk(TTY_NAME"%d on SA1100 UART3 (irq %d)\n", state->line, state->irq );
		if( machine_is_itsy() ) {
			GPSR = (GPIO_GPIO(23) | GPIO_GPIO(24));
		}
		break;

	    case (int)&Ser1UTCR0:
		/* uart serial port 1 on sa1100 */
		if( adv )
			printk(TTY_NAME"%d on SA1100 UART1 (irq %d)", state->line, state->irq );
		if( machine_is_brutus() ||
		    machine_is_lart() || 
		    machine_is_pleb() ) {
			GAFR |= (GPIO_UART_TXD | GPIO_UART_RXD);
			GPDR |= GPIO_UART_TXD;
			GPDR &= ~GPIO_UART_RXD;
			PPAR |= PPAR_UPR;
		if( adv )
			printk(", using GPIO 14/15\n");
		}
#ifdef CONFIG_SA1100_COLLIE
		else if (machine_is_collie()) {
#ifdef SERIAL_FULL
		  LCM_GPD  |= (LCM_GPIO1 | LCM_GPIO2);
		  LCM_GPD  &= ~(LCM_GPIO0 | LCM_GPIO3);
		  LCM_GPE  |= (LCM_GPIO0 | LCM_GPIO1 | LCM_GPIO2 | LCM_GPIO3);
		  LCM_GRIE |= (LCM_GPIO1 | LCM_GPIO2);
		  LCM_GFIE |= (LCM_GPIO1 | LCM_GPIO2);
#endif

#if 0	// K.Yamade
		  if( adv ) {
			printk("\n");
		  }
#endif

		}
#endif
#ifdef CONFIG_SA1100_HUW_WEBPANEL
		else if (machine_is_huw_webpanel())
		{
#ifdef SERIAL_FULL
			GAFR &= ~(GPIO_UART_TXD | GPIO_UART_RXD);
			GPDR |= GPIO_UART_TXD;
			GPDR &= ~(GPIO_UART_RXD | GPIO_SDLC_SCLK | GPIO_SDLC_AAF | GPIO_UART_SCLK1 | GPIO_SSP_CLK | GPIO_UART_SCLK3);
			PPAR &= ~PPAR_UPR;
			Ser1SDCR0 |= SDCR0_UART;
			printk("setting up GPIOs done (full implementation including RTS/CTS/DSR/DTR)\n");
#else
			GAFR |= (GPIO_UART_TXD | GPIO_UART_RXD);
			GPDR |= GPIO_UART_TXD;
			GPDR &= ~GPIO_UART_RXD;
			PPAR |= PPAR_UPR;
			printk("setting up GPIOs done ");
		if( adv )
			printk(", using GPIO 14/15\n");
#endif
		}
#endif
		else 
		  {
			Ser1SDCR0 |= SDCR0_UART;
			if( adv )
			  printk("\n");
		  }
		break;
	case (int)&Ser2UTCR0:
#ifdef CONFIG_SA1100_EMPEG
	    if( machine_is_empeg() ) {
		/* Check hardware revision */
		if (empeg_hardwarerevision()==5) {
			/* flateric uses external HP endec: program up outputs */
			GPDR|=(EMPEG_SIRSPEED0|EMPEG_SIRSPEED1|EMPEG_SIRSPEED2);

			/* setting the speed will program up the endec correctly */
		} else {
			/* uart serial port 2 on sa1100: HP-SIR */
			Ser2HSCR0=0;
			Ser2HSSR0=0x27; /* Clear bits */
			Ser2UTCR4=UTCR4_HPSIR;
		}
		if( adv )
			printk(TTY_NAME"%d on SA1100 UART2 (irq %d), using IRDA\n", state->line, state->irq );
		break;
	    }
#endif
#ifdef CONFIG_SA1100_ASSABET
	    if( machine_is_assabet() ){
		BCR_clear( 
			BCR_IRDA_FSEL | 
			BCR_IRDA_MD1 | 
			BCR_IRDA_MD0 |
			BCR_TV_IR_DEC );
		Ser2UTCR4 = UTCR4_HSE;
		Ser2HSCR0 = 0;
		Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR | HSSR0_RAB | HSSR0_FRE;
		if( adv )
			printk(TTY_NAME"%d on SA1100 UART2 (irq %d), using IRDA\n", state->line, state->irq );
		break;
	    }
#endif
#ifdef CONFIG_SA1100_COLLIE
	    if( machine_is_collie() ){
		Ser2UTCR4 = UTCR4_HSE;
		Ser2HSCR0 = 0;
		Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR | HSSR0_RAB | HSSR0_FRE;
		if( adv )
			printk(TTY_NAME"%d on SA1100 UART2 (irq %d), using IRDA\n", state->line, state->irq );
		break;
	    }
#endif
#ifdef CONFIG_SA1100_FREEBIRD
#if 0
		if ( machine_is_freebird() ) {
		BCR_clear(
			BCR_FREEBIRD_IRDA_FSEL |
			BCR_FREEBIRD_IRDA_MD1 |
			BCR_FREEBIRD_IRDA_MD0 ); 
		Ser2UTCR4 = UTCR4_HSE;
		Ser2HSCR0 = 0;
		Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR | HSSR0_RAB | HSSR0_FRE;
	    if( adv )
	    printk(TTY_NAME"%d on Freebird UART2 (irq %d), using SIR\n", state->line, state->irq );
	    break;
	    }
#endif
#endif
#ifdef CONFIG_SA1100_BITSY
	    if( machine_is_bitsy() ){
                clr_bitsy_egpio(EGPIO_BITSY_IR_ON);
		Ser2UTCR4 = UTCR4_HSE;
		Ser2HSCR0 = 0;
		Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR | HSSR0_RAB | HSSR0_FRE;
		if( adv )
			printk(TTY_NAME"%d on SA1100 UART2 (irq %d), using IRDA\n", state->line, state->irq );
		break;
	    }
#endif
#ifdef CONFIG_SA1100_NANOENGINE
	    if( machine_is_nanoengine() ){
	      // disable IRDA - used as just a normal serial port
	        Ser2UTCR4=0;
		Ser2HSCR0 = 0;
		printk(TTY_NAME"%d on SA1100 UART2 (irq %d)\n", state->line, state->irq );
		break;
	    }
#endif
#ifdef CONFIG_SA1100_PANGOLIN
	    if( machine_is_pangolin() ){
		// disable IRDA - used as just a normal serial port
		Ser2UTCR4=0;
		Ser2HSCR0 = 0;
		printk(TTY_NAME"%d on SA1100 UART2 (irq %d)\n", state->line, state->irq );
		break;
	    }
#endif
#ifdef CONFIG_SA1100_PFS168
	    if( machine_is_pfs168() ){
		PFS168_SYSCTLA &= 0x08; /* Clear FIR */
		PFS168_SYSCTLB &= 0x0c; /* Set IR mode 0 */
		Ser2UTCR4 = UTCR4_HSE;
		Ser2HSCR0 = 0;
		Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR | HSSR0_RAB | HSSR0_FRE;
		if( adv )
			printk(TTY_NAME"%d on SA1100 UART2 (irq %d), using IRDA\n", state->line, state->irq );
		break;
	    }
#endif
#ifdef CONFIG_SA1100_GRAPHICSCLIENT
	   if( machine_is_graphicsclient() ) {
		// Disable IRDA, use as normal serial (config option?)
		Ser2UTCR4 = 0;
		Ser2HSCR0 = 0;
		printk(TTY_NAME"%d on SA1100 UART2 (irq %d)\n", state->line, state->irq);
		break;
	   }
#endif

	    /* fall through */

	    default:
		printk(TTY_NAME"%d: unsupported device\n", state->line);
		restore_flags(flags);
		return;
	}

	state->type = 1;

	restore_flags(flags);
}

#ifdef CONFIG_SA1100_COLLIE
static inline int sa1100_irda_set_power_collie(int state)
{
	if (state) {
		/*
		 *  0 - off
		 *  1 - short range, lowest power
		 *  2 - medium range, medium power
		 *  3 - maximum range, high power
		 */
		ucb1200_set_io_direction(TC35143_GPIO_IR_ON,
					 TC35143_IODIR_OUTPUT);
		ucb1200_set_io(TC35143_GPIO_IR_ON, TC35143_IODAT_LOW);
		udelay(100);
	}
	else {
		/* OFF */
		ucb1200_set_io_direction(TC35143_GPIO_IR_ON,
					 TC35143_IODIR_OUTPUT);
		ucb1200_set_io(TC35143_GPIO_IR_ON, TC35143_IODAT_HIGH);
	}
	return 0;
}
#endif

#ifdef CONFIG_PM
static int sa1100_rs_pm_callback(struct pm_dev *pm_dev,
				 pm_request_t req, void *data)
{
    switch (req) {
    case PM_SUSPEND:
	ser_save.ser1utcr0 = Ser1UTCR0;
	ser_save.ser1utcr1 = Ser1UTCR1;
	ser_save.ser1utcr2 = Ser1UTCR2;
	ser_save.ser1utcr3 = Ser1UTCR3;
	ser_save.ser3utcr0 = Ser3UTCR0;
	ser_save.ser3utcr1 = Ser3UTCR1;
	ser_save.ser3utcr2 = Ser3UTCR2;
	ser_save.ser3utcr3 = Ser3UTCR3;

#ifdef CONFIG_IRDA
	ser_save.ser2utcr0 = Ser2UTCR0;
	ser_save.ser2utcr1 = Ser2UTCR1;
	ser_save.ser2utcr2 = Ser2UTCR2;
	ser_save.ser2utcr3 = Ser2UTCR3;
#ifdef CONFIG_SA1100_COLLIE
	if (irda_open)
	    sa1100_irda_set_power_collie(0);	/* power off */
#endif
#endif
	break;
    case PM_RESUME:
	Ser1SDCR0 = 1;
	Ser1UTSR0 = 0xFF;
	Ser1UTCR3 = 0;
	Ser1UTCR0 = ser_save.ser1utcr0;
	Ser1UTCR1 = ser_save.ser1utcr1;
	Ser1UTCR2 = ser_save.ser1utcr2;
	Ser1UTCR3 = ser_save.ser1utcr3;
	Ser3UTCR3 = 0;
	Ser3UTSR0 = 0xFF;
	Ser3UTCR0 = ser_save.ser3utcr0;
	Ser3UTCR1 = ser_save.ser3utcr1;
	Ser3UTCR2 = ser_save.ser3utcr2;
	Ser3UTCR3 = ser_save.ser3utcr3;

#ifdef CONFIG_IRDA
#ifdef CONFIG_SA1100_ASSABET
	if( machine_is_assabet() ){
	    BCR_clear( 
		BCR_IRDA_FSEL | 
		BCR_IRDA_MD1 | 
		BCR_IRDA_MD0 |
		BCR_TV_IR_DEC );
	    Ser2UTCR4 = UTCR4_HSE;
	    Ser2HSCR0 = 0;
	    Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR | HSSR0_RAB | HSSR0_FRE;
	}
#endif
#ifdef CONFIG_SA1100_COLLIE
	if (irda_open)
	    sa1100_irda_set_power_collie(3);	/* power on */
	if( machine_is_collie() ){
	    Ser2UTCR4 = UTCR4_HSE;
	    Ser2HSCR0 = 0;
	    Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR | HSSR0_RAB | HSSR0_FRE;
	}
#endif
	Ser2UTCR3 = 0;
	Ser2UTSR0 = 0xFF;
	Ser2UTCR0 = ser_save.ser2utcr0;
	Ser2UTCR1 = ser_save.ser2utcr1;
	Ser2UTCR2 = ser_save.ser2utcr2;
	Ser2UTCR3 = ser_save.ser2utcr3;
#endif
	break;
    }
    return 0;
}
#endif

/*
 * The serial driver boot-time initialization code!
 */
int __init sa1100_rs_init(void)
{
	int i;
	struct serial_state * state;

	printk("%s version %s\n", serial_name, serial_version );

	init_bh(SERIAL_COLLIE_BH, do_serial_bh);
	
	/* Initialize the tty_driver structure */
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.driver_name = "SA1100 serial";
#ifdef CONFIG_DEVFS_FS
	serial_driver.name = "tts/%d";
#else
	serial_driver.name = TTY_NAME;
#endif
	serial_driver.major = SERIAL_SA1100_MAJOR;
	serial_driver.minor_start = MINOR_START;
	serial_driver.num = NR_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag = 
		(baud_to_c(CONFIG_COLLIE_DEFAULT_BAUDRATE) | 
		 CS8 | CREAD | HUPCL | CLOCAL);
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
	serial_driver.send_xchar = rs_send_xchar;
	serial_driver.set_termios = rs_set_termios;
	serial_driver.stop = rs_stop;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;
	serial_driver.break_ctl = rs_break;
	serial_driver.wait_until_sent = rs_wait_until_sent;
	
	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
#ifdef CONFIG_DEVFS_FS
	callout_driver.name = "cua/%d";
#else
	callout_driver.name = CU_NAME;
#endif
	callout_driver.major = CALLOUT_SA1100_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	
	assign_ports();

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
		autoconfig(state, 1);
		if( state->type != PORT_UNKNOWN ){
			tty_register_devfs(&serial_driver, 0,
				serial_driver.minor_start + state->line);
			tty_register_devfs(&callout_driver, 0,
				callout_driver.minor_start + state->line);
		}
	}

#ifdef CONFIG_PM
	sa1100_rs_pm_dev = pm_register(PM_SYS_DEV, 0, sa1100_rs_pm_callback);
#endif

#ifdef CONFIG_SA1100_COLLIE
	irda_open = 0;
#endif
	return 0;
}

module_init(sa1100_rs_init);

void __exit sa1100_rs_cleanup(void) 
{
	unsigned long flags;
	int e1, e2;

	/* printk("Unloading %s: version %s\n", serial_name, serial_version); */
	save_flags_cli(flags);
        remove_bh(SERIAL_COLLIE_BH);
	if ((e1 = tty_unregister_driver(&serial_driver)))
		printk("SERIAL: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&callout_driver)))
		printk("SERIAL: failed to unregister callout driver (%d)\n", 
		       e2);
	restore_flags(flags);

	if (tmp_buf) {
		free_page((unsigned long) tmp_buf);
		tmp_buf = NULL;
	}
}

module_exit(sa1100_rs_cleanup);


/*
 * ------------------------------------------------------------
 * Serial console driver
 * ------------------------------------------------------------
 */
#ifdef CONFIG_SERIAL_COLLIE_CONSOLE

static struct async_struct async_sercons;

static inline void wait_for_xmitr(struct async_struct *info)
{
	unsigned int status0, status1, tmout = 1000000;

	do {
		status0 = serial_in( info, UTSR0 );
		status1 = serial_in( info, UTSR1 );

		if(status0 & UTSR0_RBB)
			lsr_break_flag = UTSR0_RBB;

		if(--tmout == 0)
			break;
	} while((status1 & UTSR1_TNF) != UTSR1_TNF);
}


/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 */
static void serial_console_write(struct console *co, const char *s,
				unsigned count)
{
	static struct async_struct *info = &async_sercons;
	int utcr3;
	unsigned i, flags;

	/*
	 *	First save the IER then disable the interrupts
	 */
	save_flags_cli(flags);
	utcr3 = serial_in( info, UTCR3 );
	serial_out( info, UTCR3, utcr3 & ~(UTCR3_RIE | UTCR3_TIE) );
	restore_flags(flags);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++, s++) {
		wait_for_xmitr(info);

		/*
		 *	Send the character out.
		 *	If a LF, also do CR...
		 */
		serial_out( info, UART_TX, *s );

		if (*s == '\n') {
			wait_for_xmitr(info);
			serial_out( info, UART_TX, '\r' );
		}
	}

	/*
	 * Wait for the tranceiver to empty
	 */
	while( serial_in(info, UTSR1) & UTSR1_TBY );

	/*
	 * Re-enable IRQs
	 */
	serial_out( info, UTCR3, utcr3 );
}

/*
 *	Receive character from the serial port
 */
static int serial_console_wait_key(struct console *co)
{
	static struct async_struct *info = &async_sercons;
	int utcr3;
	unsigned flags;
	int c;

	/*
	 *	First save the IER then disable the interrupts so
	 *	that the real driver for the port does not get the
	 *	character.
	 */
	save_flags_cli(flags);
	utcr3 = serial_in( info, UTCR3 );
	serial_out( info, UTCR3, utcr3 & ~(UTCR3_RIE | UTCR3_TIE) );
	restore_flags(flags);

	/* wait for a character to come in */
	while( !(serial_in( info, UTSR1 ) & UTSR1_RNE) );

	c = serial_in( info, UART_RX );

	/*
	 *	Restore the interrupts
	 */
	serial_out( info, UTCR3, utcr3 );

	return c;
}

static kdev_t serial_console_device(struct console *c)
{
	return MKDEV(SERIAL_SA1100_MAJOR, MINOR_START + c->index);
}

/*
 *	Setup initial baud/bits/parity. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init serial_console_setup(struct console *co, char *options)
{
	struct async_struct *info;
	struct serial_state *state;
	unsigned cval = 0;
	int	baud = CONFIG_COLLIE_DEFAULT_BAUDRATE;
	int	bits = 8;
	int	parity = 'n';
	int	cflag = CREAD | HUPCL | CLOCAL;
	int	quot;
	char	*s;

	if (co->index > NR_PORTS-1)
		return -ENODEV;

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s - '0';
	}

	/*
	 *	Now construct a cflag setting.
	 */
	cflag |= baud_to_c(baud);
	switch(bits) {
		case 7:
			cflag |= CS7;
			break;
		case 8:
		default:
			cflag |= CS8;
			cval |= UTCR0_DSS;
			break;
	}
	switch(parity) {
		case 'o': case 'O':
			cflag |= PARODD;
			cval |= UTCR0_PE|UTCR0_OES;
			break;
		case 'e': case 'E':
			cflag |= PARENB;
			cval |= UTCR0_PE;
			break;
	}
	co->cflag = cflag;

	assign_ports();
	state = rs_table + co->index;
	autoconfig(state, 0);
	if (state->type == PORT_UNKNOWN)
		return -ENODEV;

	info = &async_sercons;
	info->magic = SERIAL_MAGIC;
	info->state = state;
	info->port = state->port;
	info->flags = state->flags;

	/* must disable rx and tx enable to switch baud rate */
	while( serial_in(info, UTSR1) & UTSR1_TBY );
	serial_out(info, UTCR3, 0);

	/* set the parity, stop bits, data size */
	serial_out(info, UTCR0, cval);

	/* set the baud rate */
	quot = state->baud_base/baud - 1;
	serial_out(info, UTCR1, (quot >> 8)&0xf);	/* MS of divisor */
	serial_out(info, UTCR2, quot & 0xff);	/* LS of divisor */

	/* clear out any errors by writing the line status register */
	serial_out(info, UTSR0, 0xff);

	/* enable rx and rx */
	serial_out(info, UTCR3, UTCR3_TXE|UTCR3_RXE);

	return 0;
}

static struct console sercons = {
	name:		TTY_NAME,
	write:		serial_console_write,
	device:		serial_console_device,
	wait_key:	serial_console_wait_key,
	setup:		serial_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1
};

/*
 *	Register console.
 */
void __init collie_rs_console_init(void)
{
	register_console(&sercons);
}
#endif
