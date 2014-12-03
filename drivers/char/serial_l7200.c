/*
 * linux/drivers/char/serial_l7200.c
 *
 * Drvier for the serial port on LinkUp 7200.
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on drivers/char/serial_21285.c
 * Driver for the serial port on the 21285 StrongArm-110 core logic chip.
 * Based on drivers/char/serial.c
 *
 *  ChangeLog:
 *    03-16-2001 Lineo Japan, Inc. ...
 *    04-16-2001         pm.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/console.h>

#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <asm/serial.h>

#include <linux/pm.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>

#undef SERIAL_FULL

#ifdef CONFIG_PM
static struct pm_dev* l7200_rs_pm_dev;
#endif

static struct serial_state rs_table[RS_TABLE_SIZE] = {
	SERIAL_PORT_DFNS
};
#define NR_PORTS (sizeof(rs_table)/sizeof(struct serial_state))

#define SERIAL_L7200_NAME	"ttyLU"
#define SERIAL_L7200_MAJOR	204
#define SERIAL_L7200_MINOR	0

#define SERIAL_L7200_AUX_NAME	"cuaLU"
#define SERIAL_L7200_AUX_MAJOR	205
#define SERIAL_L7200_AUX_MINOR	0

#define WBUF_SIZE 1000

struct uport {
        unsigned long   base;
        int		irq;
        int		use_count;
        unsigned short	flag;
        char		wbuf[WBUF_SIZE];
        char		*putp, *getp;
        char		xchar;
        unsigned char   mcr;
};

static struct tty_driver  l7200_rs_driver, callout_driver;
static int                l7200_rs_refcount;

static struct tty_struct* l7200_tty_table[NR_PORTS];
static struct termios*    l7200_rs_termios[NR_PORTS];
static struct termios*    l7200_rs_termios_locked[NR_PORTS];

static struct uport	  uart[NR_PORTS];

#ifdef  SERIAL_FULL

#define UARTFLG_CTS 0x01
#define UARTFLG_DDR 0x02
#define UARTFLG_DCD 0x04
#define MCR_RTS     0x01
#define MCR_DTR     0x02

#ifdef TIOCM_OUT1
#define MCR_OUT1    0x04
#define MCR_OUT2    0x08
#endif

static unsigned char get_MSR(int dev)
{
	struct uport* up = &uart[dev];
	switch (dev) {
	case 0:	return __IOB(up->base + UARTFLG) & 0x07;
	case 1: return UARTFLG_DCD;
	}
	return 0;
}

static unsigned char get_MCR(int dev)
{
	struct uport* up = &uart[dev];
	switch (dev) {
	case 0:
	case 1:
		return up->mcr;  
	}
	return 0;
}

static unsigned char set_MCR(int dev, unsigned char val)
{
	struct uport* up = &uart[dev];
	switch (dev) {
	case 0:
	case 1:
		up->mcr = val;
		break;
	}
	return 0;
}

#endif

void l7200_rs_enable_irq(int dev, int dir)
{
	struct uport* up = &uart[dev];

	if ( !up->base )
		return;

	__IOB(up->base + UARTINTMASK) |= dir;
	if (!(IO_IRQENABLE & (1 << up->irq))) {
		enable_irq(up->irq);
	}
}

void l7200_rs_disable_irq(int dev, int dir)
{
	struct uport* up = &uart[dev];

	if ( !up->base )
		return;

	__IOB(up->base + UARTINTMASK) &= ~dir;
	if (!(__IOB(up->base+UARTINTMASK) | UART_TXINT | UART_RXINT)) {
		disable_irq(up->irq);
	}
}

static int l7200_rs_write_room(struct tty_struct* tty)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up = &uart[dev];

	return (up->putp >= up->getp) ?
		(WBUF_SIZE - (long)up->putp + (long)up->getp) :
		((long)up->getp - (long)up->putp - 1);
}

static void l7200_rs_send_xchar(struct tty_struct* tty, char ch)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up = &uart[dev];

	up->xchar = ch;
	l7200_rs_enable_irq(dev, UART_TXINT);
}

static void l7200_rs_throttle(struct tty_struct* tty)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up = &uart[dev];

	if (I_IXOFF(tty)) {
		l7200_rs_send_xchar(tty, STOP_CHAR(tty));
	}
#ifdef SERIAL_FULL
	if (tty->termios->c_cflag & CRTSCTS) {
		set_MCR(dev, up->mcr & ~MCR_RTS);
	}
#endif
}

static void l7200_rs_unthrottle(struct tty_struct* tty)

{	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up = &uart[dev];

	if (I_IXOFF(tty)) {
		if (up->xchar) {
			up->xchar = 0;
		}
		else {
			l7200_rs_send_xchar(tty, START_CHAR(tty));
		}
	}
#ifdef SERIAL_FULL
	if (tty->termios->c_cflag & CRTSCTS) {
		set_MCR(dev, up->mcr | MCR_RTS);
	}
#endif
}

static int get_modem_info(int dev, unsigned int* value)
{
	struct uport* up = &uart[dev];
	unsigned int result=0;
#ifdef SERIAL_FULL
	unsigned char control, status;

	control = up->mcr;
	status  = get_MSR(dev);

	result =  ((control & MCR_RTS) ? TIOCM_RTS : 0)
		| ((control & MCR_DTR) ? TIOCM_DTR : 0)
#ifdef TIOCM_OUT1
		| ((control & MCR_OUT1) ? TIOCM_OUT1 : 0)
		| ((control & MCR_OUT2) ? TIOCM_OUT2 : 0)
#endif
		| ((status & UARTFLG_DCD) ? TIOCM_CAR : 0)
		| ((status & UARTFLG_DDR) ? TIOCM_DSR : 0)
		| ((status & UARTFLG_CTS) ? TIOCM_CTS : 0);
#endif
	return put_user(result, value);
}

static int set_modem_info(int dev, unsigned int cmd, unsigned int* value)
{
	struct uport* up = &uart[dev];
	unsigned int arg;
	int error;

	error = get_user(arg, value);
	if (error)
		return error;
	switch (cmd) {
#ifdef SERIAL_FULL
	case TIOCMBIS: 
		if (arg & TIOCM_RTS)
			set_MCR(dev, up->mcr | MCR_RTS);
		if (arg & TIOCM_DTR)
			set_MCR(dev, up->mcr | MCR_DTR);
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			set_MCR(dev, up->mcr & ~MCR_RTS);
		if (arg & TIOCM_DTR)
			set_MCR(dev, up->mcr & ~MCR_DTR);
		break;
	case TIOCMSET:
		set_MCR(dev, (up->mcr & ~(MCR_RTS | MCR_DTR))
			     | ((arg & TIOCM_RTS) ? MCR_RTS : 0)
			     | ((arg & TIOCM_DTR) ? MCR_DTR : 0));
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int l7200_rs_ioctl(struct tty_struct* tty,
			  struct file*  file,
			  unsigned int  cmd,
			  unsigned long arg)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;

        if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
            (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
            (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
                if (tty->flags & (1 << TTY_IO_ERROR))
                    return -EIO;
        }

        switch (cmd) {
        case TIOCMGET:
                return get_modem_info(dev, (unsigned int*)arg);
        case TIOCMBIS:
        case TIOCMBIC:
        case TIOCMSET:
                return set_modem_info(dev, cmd, (unsigned int*)arg);
        case TIOCGSERIAL:
                return -EINVAL;
        case TIOCSSERIAL:
		return -EINVAL;
	case TIOCSERGETLSR:
		return 0;
	case TIOCSERGSTRUCT:	
		return 0;
	case TIOCMIWAIT:	
		return 0;
	case TIOCGICOUNT:
		return 0;
	case TIOCSERGWILD:
	case TIOCSERSWILD:
#ifdef DEBUG
		printk ("TIOCSER?WILD ioctl obsolete, ignored.\n");
#endif
		return 0;
	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static void l7200_rs_interrupt(int irq, void *dev_id, struct pt_regs* regs)
{
	int dev = irq - IRQ_UART_1;
	struct uport* up = &uart[dev];
 	struct tty_struct* tp = l7200_tty_table[dev];
	int irqst = __IOB(up->base + UARTINTSTAT);

	if (irqst & UART_RXINT) {
		if (!up->flag) {
			l7200_rs_disable_irq(dev, UART_RXINT);
			return;
		}
		while (!(__IOB(up->base + UARTFLG) & UARTFLG_URXFE)) {
			unsigned char ch = __IOB(up->base + UARTDR);
			unsigned char st = __IOB(up->base + RXSTAT);
			char flag = 0;

			if (st & RXSTAT_OVR_ERR) {
				tty_insert_flip_char(tp, 0, TTY_OVERRUN);
			}
			if (st & RXSTAT_PAR_ERR) {
				flag = TTY_PARITY;
			}
			else if (st & RXSTAT_FRM_ERR) {
				flag = TTY_FRAME;
			}
			tty_insert_flip_char(tp, ch & 0xff, flag);
		}
		tty_flip_buffer_push(tp);
	}
	if (irqst & UART_TXINT) {
		while (!(__IOB(up->base + UARTFLG) & UARTFLG_UTXFF)) {
			if (up->xchar) {
				__IOB(up->base + UARTDR) = up->xchar;
				up->xchar = 0;
				continue;
			}
			if (up->putp == up->getp) {
				l7200_rs_disable_irq(dev, UART_TXINT);
				break;
			}
			__IOB(up->base + UARTDR) = *up->getp;
			if (++up->getp >= up->wbuf + WBUF_SIZE) {
				up->getp = up->wbuf;
			}
		}
		if (up->flag) {
			wake_up_interruptible(&tp->write_wait);
		}
	}
}

static inline int l7200_rs_xmit(int dev, int ch)
{
	struct uport* up = &uart[dev];

	if (up->putp + 1 == up->getp ||
	    ((up->putp + 1 == up->wbuf + WBUF_SIZE) && (up->getp == up->wbuf)))
		return 0;
	*up->putp = ch;
	if (++up->putp >= up->wbuf + WBUF_SIZE) {
		up->putp = up->wbuf;
	}
	l7200_rs_enable_irq(dev, UART_TXINT);
	return 1;
}

static int l7200_rs_write(struct tty_struct* tty, int from_user,
			  const u_char* buf, int count)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	int i;

	if (from_user && verify_area(VERIFY_READ, buf, count))
		return -EINVAL;

	for (i = 0; i < count; i++) {
		char ch;
		if (from_user)
			__get_user(ch, buf + i);
		else
			ch = buf[i];
		if (!l7200_rs_xmit(dev, ch))
			break;
	}
	return i;
}

static void l7200_rs_put_char(struct tty_struct* tty, u_char ch)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;

	l7200_rs_xmit(dev, ch);
}

static int l7200_rs_chars_in_buffer(struct tty_struct* tty)
{
	return WBUF_SIZE - l7200_rs_write_room(tty);
}

static void l7200_rs_flush_buffer(struct tty_struct* tty)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up = &uart[dev];

#ifdef SERIAL_FULL
	if (tty->stopped || tty->hw_stopped)
		return;
#endif

	l7200_rs_disable_irq(dev, UART_TXINT);
	up->putp = up->getp = up->wbuf;
	if (up->xchar) {
		l7200_rs_enable_irq(dev, UART_TXINT);
	}
}

static inline void l7200_rs_set_cflag(int dev, int cflag)
{
	struct uport* up = &uart[dev];
	int cr_h, cr_l;
	
	switch (cflag & CSIZE) {
	case CS5: cr_h = UBRLCR_LEN5; break;
	case CS6: cr_h = UBRLCR_LEN6; break;
	case CS7: cr_h = UBRLCR_LEN7; break;
	default:  cr_h = UBRLCR_LEN8; break;
	}

	if (cflag & CSTOPB)    { cr_h |= UBRLCR_STP2; }
	if (cflag & PARENB)    { cr_h |= UBRLCR_PEN; }
	if (!(cflag & PARODD)) { cr_h |= UBRLCR_EVEN; }

	switch (cflag & CBAUD) {
	case B1200:   cr_l = BR_1200;   break;
	case B2400:   cr_l = BR_2400;   break;
	case B4800:   cr_l = BR_4800;   break;
	case B9600:   cr_l = BR_9600;   break;
	case B19200:  cr_l = BR_19200;  break;
	case B38400:  cr_l = BR_38400;  break;
	case B57600:  cr_l = BR_57600;  break;
	case B115200: cr_l = BR_115200; break;
	default:      cr_l = BR_9600;
	}

	if ( up->base ) {
		__IOB(up->base + L_UBRLCR) = cr_l & 0xFF;
		__IOB(up->base + H_UBRLCR) = cr_h & 0xFF;
	}
	else {
		IO_L_UBRLCR = cr_l & 0xFF;
		IO_H_UBRLCR = cr_h & 0xFF;
	}
}

static void l7200_rs_start(struct tty_struct* tty)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;

	l7200_rs_enable_irq(dev, UART_TXINT);
}

static void l7200_rs_set_termios(struct tty_struct* tty, struct termios* old)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up = &uart[dev];

	if (old && tty->termios->c_cflag == old->c_cflag)
		return;

#ifdef SERIAL_FULL
	if ((old->c_cflag & CBAUD) &&
	    !(tty->termios->c_cflag & CBAUD)) {
		set_MCR(dev, up->mcr & ~(MCR_DTR|MCR_RTS));
	}

	if (!(old->c_cflag & CBAUD) &&
	    (tty->termios->c_cflag & CBAUD)) {
		set_MCR(dev, up->mcr | MCR_DTR);
		if (!(tty->termios->c_cflag & CRTSCTS) ||
		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			set_MCR(dev, up->mcr | MCR_RTS);
		}
	}
	
	/* Handle turning off CRTSCTS */
	if ((old->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		l7200_rs_start(tty);
	}
#else
	l7200_rs_set_cflag(dev, tty->termios->c_cflag);
	if (dev) {
		tty->termios->c_cflag |= CLOCAL;
		tty->termios->c_cflag &= ~CRTSCTS;
	}
#endif
}

static void l7200_rs_stop(struct tty_struct* tty)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	l7200_rs_disable_irq(dev, UART_TXINT);
}

static void l7200_rs_hangup(struct tty_struct* tty)
{
	l7200_rs_flush_buffer(tty);
}

static void l7200_rs_break(struct tty_struct *tty, int break_state)
{
#if 0
	if (break_state == -1) {
	  /* send break */
	}
	else {
	}
#endif
}

static void l7200_rs_wait_until_sent(struct tty_struct* tty, int timeout)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up = &uart[dev];
	int orig_jiffies = jiffies;

	while (__IOB(up->base + UARTFLG) & UARTFLG_UBUSY) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
	current->state = TASK_RUNNING;
}

static int l7200_rs_open(struct tty_struct* tty, struct file *filp)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up;

	if (dev >= NR_PORTS) {
		return -ENODEV;
	}

	MOD_INC_USE_COUNT;
	up = &uart[dev];
	tty->driver_data = NULL;
	up->flag++;
	__IOB(up->base + UARTCON) |= UARTCON_UARTEN;
#ifdef SERIAL_FULL
	set_MCR(dev, get_MCR(dev) | MCR_DTR | MCR_RTS);
#endif
	l7200_rs_enable_irq(dev, UART_RXINT);
	up->use_count++;

	return 0;
}

static void l7200_rs_close(struct tty_struct* tty, struct file *filp)
{
	int dev = MINOR(tty->device) - tty->driver.minor_start;
	struct uport* up;

	if (dev >= NR_PORTS)
		return;

	up = &uart[dev];
	if (!--up->use_count) {
		l7200_rs_wait_until_sent(tty, 0);
		l7200_rs_disable_irq(dev, UART_RXINT);
		l7200_rs_disable_irq(dev, UART_TXINT);
		__IOB(up->base + UARTCON) &= ~UARTCON_UARTEN;
		up->flag = 0;
	}

	MOD_DEC_USE_COUNT;
}

#ifdef CONFIG_PM
static int l7200_rs_pm_callback(struct pm_dev *pm_dev,
				pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
		break;
	case PM_RESUME:
                break;
	}
	return 0;
}
#endif

static int __init l7200_rs_init(void)
{
	int i;
  
	memset(&l7200_rs_driver, 0, sizeof(l7200_rs_driver));
	l7200_rs_driver.magic           = TTY_DRIVER_MAGIC;
	l7200_rs_driver.driver_name     = "serial_linkup7200";
	l7200_rs_driver.name            = "ttyS"; 
	l7200_rs_driver.name            = SERIAL_L7200_NAME;
	l7200_rs_driver.major           = SERIAL_L7200_MAJOR;
	l7200_rs_driver.minor_start     = SERIAL_L7200_MINOR;
	l7200_rs_driver.num             = NR_PORTS;
	l7200_rs_driver.type            = TTY_DRIVER_TYPE_SERIAL;
	l7200_rs_driver.subtype         = SERIAL_TYPE_NORMAL;
	l7200_rs_driver.init_termios    = tty_std_termios;
	l7200_rs_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	l7200_rs_driver.flags           = TTY_DRIVER_REAL_RAW;
	l7200_rs_driver.refcount        = &l7200_rs_refcount; 
	l7200_rs_driver.table           = l7200_tty_table;
	l7200_rs_driver.termios         = l7200_rs_termios;
	l7200_rs_driver.termios_locked  = l7200_rs_termios_locked;

	l7200_rs_driver.open            = l7200_rs_open;
	l7200_rs_driver.close           = l7200_rs_close;
	l7200_rs_driver.write           = l7200_rs_write;
	l7200_rs_driver.put_char        = l7200_rs_put_char;
	l7200_rs_driver.flush_chars     = NULL;
	l7200_rs_driver.write_room      = l7200_rs_write_room;
	l7200_rs_driver.chars_in_buffer = l7200_rs_chars_in_buffer;
	l7200_rs_driver.flush_buffer    = l7200_rs_flush_buffer;
	l7200_rs_driver.ioctl           = l7200_rs_ioctl; 
	l7200_rs_driver.throttle        = l7200_rs_throttle;
	l7200_rs_driver.unthrottle      = l7200_rs_unthrottle;
	l7200_rs_driver.send_xchar      = l7200_rs_send_xchar;
	l7200_rs_driver.set_termios     = l7200_rs_set_termios;
	l7200_rs_driver.stop            = l7200_rs_stop;
	l7200_rs_driver.start           = l7200_rs_start;
	l7200_rs_driver.hangup          = l7200_rs_hangup;
	l7200_rs_driver.break_ctl       = l7200_rs_break;
	l7200_rs_driver.wait_until_sent = l7200_rs_wait_until_sent; 
	l7200_rs_driver.read_proc       = NULL;

	memset(&callout_driver, 0, sizeof(callout_driver));
	callout_driver                  = l7200_rs_driver;
	callout_driver.name             = SERIAL_L7200_AUX_NAME;
	callout_driver.major            = SERIAL_L7200_AUX_MAJOR;
	callout_driver.subtype          = SERIAL_TYPE_CALLOUT;

	for (i = 0; i < NR_PORTS; i++)  {
		struct uport* up = &uart[i];

		up->base   = rs_table[i].port;
		up->irq       = rs_table[i].irq;
		up->use_count = 0;
		up->flag      = 0;
		up->putp = up->getp = up->wbuf;
		up->xchar     = 0;
		if (request_irq(up->irq, l7200_rs_interrupt,
				SA_INTERRUPT, "serial", NULL)) {
			up->irq = -1;
			panic("Couldn't get LinkUp 7200 uart%d irq\n", i);
		}

		printk(KERN_INFO "ttyLU%1d at 0x%08lx (irq = %d)\n",
		       i, up->base, up->irq);
	}

	if (tty_register_driver(&l7200_rs_driver)) {
		printk(KERN_ERR
		       "Couldn't register Linkup 7200 serial driver\n");
	}
	if (tty_register_driver(&callout_driver)) {
		printk(KERN_ERR
		       "Couldn't register LinkUp 7200 callout driver\n");
	}

#ifdef CONFIG_PM
	l7200_rs_pm_dev = pm_register(PM_SYS_DEV, 0, l7200_rs_pm_callback);
#endif

	return 0;
}

static void __exit l7200_rs_fini(void)
{
	unsigned long flags;
	int ret, i;

	save_flags(flags);
	cli();
	ret = tty_unregister_driver(&callout_driver);
	if (ret) {
		printk(KERN_ERR
		       "Unable to unregister LinkUp 7200 callout driver "
		       "(%d)\n", ret);
	}
	ret = tty_unregister_driver(&l7200_rs_driver);
	if (ret) {
		printk(KERN_ERR
		       "Unable to unregister LinkUp 7200 driver (%d)\n", ret);

	}
	for (i = 0; i < NR_PORTS; i++)  {
		struct uport* up = &uart[i];
		if (up->irq >= 0) {
			free_irq(up->irq, NULL);
		}
	}
	restore_flags(flags);
}

module_init(l7200_rs_init);
module_exit(l7200_rs_fini);

#ifdef CONFIG_SERIAL_L7200_CONSOLE

static void l7200_rs_console_write(struct console* co,
				   const char* s, u_int count)
{
	int i;

	l7200_rs_disable_irq(0, UART_TXINT);
	for (i = 0; i < count; i++) {
		while (IO_UARTFLG & UARTFLG_UTXFF) {}
		IO_UARTDR = s[i];
		if (s[i] == '\n') {
			while (IO_UARTFLG & UARTFLG_UTXFF) {}
			IO_UARTDR = '\r';
		}
	}
	l7200_rs_enable_irq(0, UART_TXINT);
}

static int l7200_rs_console_wait_key(struct console* co)
{
	int c;

	l7200_rs_disable_irq(0, UART_RXINT);
	while (IO_UARTFLG & UARTFLG_URXFE) {}
	c = IO_UARTDR;
	l7200_rs_enable_irq(0, UART_RXINT);
	return c;
}

static kdev_t l7200_rs_console_device(struct console *c)
{
	return MKDEV(SERIAL_L7200_MAJOR, SERIAL_L7200_MINOR);
}

static int __init l7200_rs_console_setup(struct console *co, char *options)
{
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int cflag = CREAD | HUPCL | CLOCAL;

	if (options) {
		char *s = options;
		baud = simple_strtoul(options, NULL, 10);
		while (*s >= '0' && *s <= '9') {
			s++;
		}
		if (*s) {
			parity = *s++;
		}
		if (*s) {
			bits = *s - '0';
		}
	}

	switch (baud) {
	case 1200:   cflag |= B1200;   break;
	case 2400:   cflag |= B2400;   break;
	case 4800:   cflag |= B4800;   break;
	case 9600:   cflag |= B9600;   break;
	case 19200:  cflag |= B19200;  break;
	case 38400:  cflag |= B38400;  break;
	case 57600:  cflag |= B57600;  break;
	case 115200: cflag |= B115200; break;
	default:     cflag |= B9600;
	}
	switch (bits) { 
	case 5:  cflag |= CS5; break;
	case 6:  cflag |= CS6; break;
	case 7:  cflag |= CS7; break;
	default: cflag |= CS8;
	}
	switch (parity) {
	case 'o': case 'O': cflag |= PARODD; break;
	case 'e': case 'E': cflag |= PARENB; break;
	}

	co->cflag = cflag;

	l7200_rs_set_cflag(0, cflag);

	IO_UARTCON |= UARTCON_UARTEN;

	l7200_rs_console_write(NULL, "boot ", 5);
	if (options)
		l7200_rs_console_write(NULL, options, strlen(options));
	else
		l7200_rs_console_write(NULL, "no options", 10);
	l7200_rs_console_write(NULL, "\n", 1);

	return 0;
}

static struct console l7200_rs_cons =
{
	name:		SERIAL_L7200_NAME,
	write:		l7200_rs_console_write,
	device:		l7200_rs_console_device,
	wait_key:	l7200_rs_console_wait_key,
	setup:		l7200_rs_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init l7200_rs_console_init(void)
{
	register_console(&l7200_rs_cons);
}

#endif /* CONFIG_SERIAL_L7200_CONSOLE */
