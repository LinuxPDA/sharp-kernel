/*
 *  linux/drivers/char/at91rm9200us3.c
 *
 *  Driver for at91rm9200 serial port
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Copyright (C) 2002 Atmel Rousset
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
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
#include <linux/init.h>
#include <linux/circ_buf.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <asm/arch/AT91RM9200_USART.h>

#if defined(CONFIG_SERIAL_AT91US3_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>

#include <asm/arch/hardware.h>

#define SERIAL_AT91US3_MAJOR  TTY_MAJOR
#define SERIAL_AT91US3_MINOR  64


#define CALLOUT_AT91US3_MAJOR TTYAUX_MAJOR
#define CALLOUT_AT91US3_MINOR 64


/*
 * Access macros for the AT91US3DK USART3s
 */

#define AT91C_PDC_TXTEN (1 << 8)
#define UART_GET_UTCR(p)	readl(&(((AT91PS_USART)(p)->membase)->US_CR))
#define UART_GET_UTMR(p)	readl(&(((AT91PS_USART)(p)->membase)->US_MR))
#define UART_GET_UTCSR(p)	readl(&(((AT91PS_USART)(p)->membase)->US_CSR))
#define UART_GET_CHAR(p)	readl(&(((AT91PS_USART)(p)->membase)->US_RHR))
#define UART_GET_UTIMR(p)	readl(&(((AT91PS_USART)(p)->membase)->US_IMR))
#define UART_GET_UTBRGR(p)	readl(&(((AT91PS_USART)(p)->membase)->US_BRGR))
#define UART_GET_TPR(p)     readl(&(((AT91PS_USART)(p)->membase)->US_TPR))
#define UART_GET_PTSR(p)     readl(&(((AT91PS_USART)(p)->membase)->US_PTSR))

#define UART_PUT_UTCR(p,v)	writel(v,&(((AT91PS_USART)(p)->membase)->US_CR))
#define UART_PUT_UTMR(p,v)	writel(v,&(((AT91PS_USART)(p)->membase)->US_MR))
#define UART_PUT_CHAR(p,v)	writel(v,&(((AT91PS_USART)(p)->membase)->US_THR))
#define UART_PUT_UTIER(p,v)	writel(v,&(((AT91PS_USART)(p)->membase)->US_IER))
#define UART_PUT_UTIDR(p,v)	writel(v,&(((AT91PS_USART)(p)->membase)->US_IDR))
#define UART_PUT_UTBRGR(p,v)    writel(v,&(((AT91PS_USART)(p)->membase)->US_BRGR))
#define UART_PUT_PDC_PTCR(p, v) writel(v,&(((AT91PS_USART)(p)->membase)->US_PTCR))
#define UART_SET_PDC(p,v,l)    {writel(v,&(((AT91PS_USART)(p)->membase)->US_TPR));\
                                writel(l,&(((AT91PS_USART)(p)->membase)->US_TCR));}
#define UART_SET_NPDC(p,v,l)   {writel(v,&(((AT91PS_USART)(p)->membase)->US_TNPR));\
                                writel(l,&(((AT91PS_USART)(p)->membase)->US_TNCR));}

#define UART_PORT_SIZE		SZ_16K
#define UART_PORT_DBGU_SIZE	512

#define AT91US3_ISR_PASS_LIMIT	256

static int at91us3_verify_port(struct uart_port *port, struct serial_struct *ser);
static const char *at91us3_type(struct uart_port *port);
static void at91us3_config_port(struct uart_port *port, int flags);
static int at91us3_request_port(struct uart_port *port);
static void at91us3_release_port(struct uart_port *port);
static void at91us3_change_speed(struct uart_port *port, u_int cflag, u_int iflag, u_int quot);
static void at91us3_set_mctrl(struct uart_port *port, u_int mctrl);
static u_int at91us3_get_mctrl(struct uart_port *port);
static void at91us3_set_mctrl(struct uart_port *port, u_int mctrl);
static u_int at91us3_tx_empty(struct uart_port *port);
static u_int at91us3_tx_empty(struct uart_port *port);
static void at91us3_int(int irq, void *dev_id, struct pt_regs *regs);
static void at91us3_tx_chars(struct uart_info *info);
static void at91us3_enable_ms(struct uart_port *port);
static void at91us3_break_ctl(struct uart_port *port, int break_state);
static void at91us3_rx_chars(struct uart_info *info, struct pt_regs *regs);
static void at91us3_stop_rx(struct uart_port *port);
static void at91us3_start_tx(struct uart_port *port, u_int nonempty, u_int from_tty);
static void at91us3_stop_tx(struct uart_port *port, u_int from_tty);
static int at91us3_startup(struct uart_port *port, struct uart_info *info);
static void at91us3_shutdown(struct uart_port *port, struct uart_info *info);

static struct uart_ops at91us3_pops = {
	tx_empty:	at91us3_tx_empty,
	set_mctrl:	at91us3_set_mctrl,
	get_mctrl:	at91us3_get_mctrl,
	stop_tx:	at91us3_stop_tx,
	start_tx:	at91us3_start_tx,
	stop_rx:	at91us3_stop_rx,
	enable_ms:	at91us3_enable_ms,
	break_ctl:	at91us3_break_ctl,
	startup:	at91us3_startup,
	shutdown:	at91us3_shutdown,
	change_speed:	at91us3_change_speed,
	type:		at91us3_type,
	release_port:	at91us3_release_port,
	request_port:	at91us3_request_port,
	config_port:	at91us3_config_port,
	verify_port:	at91us3_verify_port,
};

#define UART_NR		                5  /* 4 USART3's and one debug port */
struct uart_port at91us3_ports[UART_NR];

static struct tty_driver normal, callout;
static struct tty_struct *at91us3_table[UART_NR];
static struct termios *at91us3_termios[UART_NR], *at91us3_termios_locked[UART_NR];
#ifdef SUPPORT_SYSRQ
static struct console at91us3_console;
#endif


// ============================================================
// Private definitions for AT91RM9200DK board
// ============================================================
#ifdef CONFIG_ARCH_AT91RM9200DK

#define PORT_AT91US3      PORT_AT91RM9200

void __init at91rm9200dk_register_us0(void)
{
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->PIOA_PDR = AT91C_PIO_PA17 | AT91C_PIO_PA18 |
	                                            AT91C_PIO_PA19 | AT91C_PIO_PA20 |
	                                            AT91C_PIO_PA21;
	at91us3_ports[0].membase  = (AT91PS_USART)AT91C_VA_BASE_US0;
	at91us3_ports[0].mapbase  = (int) AT91C_BASE_US0;
	at91us3_ports[0].irq      = AT91C_ID_US0;
	at91us3_ports[0].iotype   = SERIAL_IO_MEM;
	at91us3_ports[0].flags    = ASYNC_BOOT_AUTOCONF;
	at91us3_ports[0].uartclk  = AT91C_MASTER_CLK;
	at91us3_ports[0].ops      = &at91us3_pops;
	at91us3_ports[0].fifosize = 0;
}

void __init at91rm9200dk_register_us1(void)
{
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->PIOB_PDR = AT91C_PIO_PB18 | AT91C_PIO_PB19 |
	                                            AT91C_PIO_PB20 | AT91C_PIO_PB21 |
						    AT91C_PIO_PB23 | AT91C_PIO_PB24 |
						    AT91C_PIO_PB25 | AT91C_PIO_PB26;
	at91us3_ports[1].membase  = (AT91PS_USART)AT91C_VA_BASE_US1;
	at91us3_ports[1].mapbase  = (int) AT91C_BASE_US1;
	at91us3_ports[1].irq      = AT91C_ID_US1;
	at91us3_ports[1].iotype   = SERIAL_IO_MEM;
	at91us3_ports[1].flags    = ASYNC_BOOT_AUTOCONF;
	at91us3_ports[1].uartclk  = AT91C_MASTER_CLK;
	at91us3_ports[1].ops      = &at91us3_pops;
	at91us3_ports[1].fifosize = 0;

	at91us3_enable_ms(&at91us3_ports[1]);
}

void __init at91rm9200dk_register_us2(void)
{
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->PIOA_PDR = AT91C_PIO_PA22 | AT91C_PIO_PA23;
	at91us3_ports[2].membase  = (AT91PS_USART)AT91C_VA_BASE_US2;
	at91us3_ports[2].mapbase  = (int) AT91C_BASE_US2;
	at91us3_ports[2].irq      = AT91C_ID_US2;
	at91us3_ports[2].iotype   = SERIAL_IO_MEM;
	at91us3_ports[2].flags    = ASYNC_BOOT_AUTOCONF;
	at91us3_ports[2].uartclk  = AT91C_MASTER_CLK;
	at91us3_ports[2].ops      = &at91us3_pops;
	at91us3_ports[2].fifosize = 0;
}

void __init at91rm9200dk_register_us3(void)
{
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->PIOA_PDR = AT91C_PIO_PA5 | AT91C_PIO_PA6 ;
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->PIOA_BSR = AT91C_PIO_PA5 | AT91C_PIO_PA6;
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->PIOB_PDR = AT91C_PIO_PA1 | AT91C_PIO_PA2 ;
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->PIOB_BSR = AT91C_PIO_PA1 | AT91C_PIO_PA2;
	at91us3_ports[3].membase  = (AT91PS_USART)AT91C_VA_BASE_US3;
	at91us3_ports[3].mapbase  = (int) AT91C_BASE_US3;
	at91us3_ports[3].irq      = AT91C_ID_US3;
	at91us3_ports[3].iotype   = SERIAL_IO_MEM;
	at91us3_ports[3].flags    = ASYNC_BOOT_AUTOCONF;
	at91us3_ports[3].uartclk  = AT91C_MASTER_CLK;
	at91us3_ports[3].ops      = &at91us3_pops;
	at91us3_ports[3].fifosize = 0;
}

void __init at91rm9200dk_register_dbgu(void)
{
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->PIOA_PDR = AT91C_PIO_PA30 | AT91C_PIO_PA31;
	at91us3_ports[4].membase  = (AT91PS_USART)AT91C_VA_BASE_DBGU;
	at91us3_ports[4].mapbase  = (int) AT91C_BASE_DBGU;
	at91us3_ports[4].irq      = AT91C_ID_SYS;
	at91us3_ports[4].iotype   = SERIAL_IO_MEM;
	at91us3_ports[4].flags    = ASYNC_BOOT_AUTOCONF;
	at91us3_ports[4].uartclk  = AT91C_MASTER_CLK;
	at91us3_ports[4].ops      = &at91us3_pops;
	at91us3_ports[4].fifosize = 0;
}

#endif




/*
 * interrupts disabled on entry
 */
static void at91us3_stop_tx(struct uart_port *port, u_int from_tty)
{
	UART_PUT_UTIDR(port, AT91C_US_TXEMPTY);
	port->read_status_mask &= ~AT91C_US_TXEMPTY;
}

/*
 * interrupts may not be disabled on entry
 */
static void at91us3_start_tx(struct uart_port *port, u_int nonempty, u_int from_tty)
{

	if (nonempty) {
	        unsigned long flags;

		local_irq_save(flags);
		port->read_status_mask |= AT91C_US_TXEMPTY;
		UART_PUT_UTIER(port, AT91C_US_TXEMPTY);
		local_irq_restore(flags);
	}
}

/*
 * Interrupts enabled
 */
static void at91us3_stop_rx(struct uart_port *port)
{
	UART_PUT_UTIDR(port, AT91C_US_RXRDY);
}

static void at91us3_enable_ms(struct uart_port *port)
{
	unsigned int mode;
	mode = UART_GET_UTMR(port);
	mode &=  ~AT91C_US_USMODE;
	mode |= AT91C_US_USMODE_MODEM ;
	UART_PUT_UTMR(port,mode);
	UART_PUT_UTIER(port, AT91C_US_RIIC | AT91C_US_DSRIC | AT91C_US_DCDIC | AT91C_US_CTSIC );

}

static void
at91us3_rx_chars(struct uart_info *info, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	unsigned int status, ch, flg, ignored = 0;
	struct uart_port *port = info->port;

	status = UART_GET_UTCSR(port);
	while (status & (AT91C_US_RXRDY)) {
		ch = UART_GET_CHAR(port);
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		port->icount.rx++;

		flg = TTY_NORMAL;

		/*
		 * note that the error handling code is
		 * out of the main execution path
		 */
		if (status & (AT91C_US_PARE | AT91C_US_FRAME | AT91C_US_OVRE))
			goto handle_error;

		if (uart_handle_sysrq_char(info, ch, regs))
			goto ignore_char;

	error_return:
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
	ignore_char:
		status = UART_GET_UTCSR(port);
	}
 out:
	tty_flip_buffer_push(tty);
	return;

 handle_error:
	if (status & (AT91C_US_PARE | AT91C_US_FRAME | AT91C_US_OVRE))
	        UART_PUT_UTCR(port, AT91C_US_RSTSTA);  /* clear error */
	if (status & (AT91C_US_PARE))
	        port->icount.parity++;
	else if (status & (AT91C_US_FRAME))
		port->icount.frame++;
	if (status & (AT91C_US_OVRE))
		port->icount.overrun++;

	if (status & port->ignore_status_mask) {
		if (++ignored > 100)
			goto out;
		goto ignore_char;
	}

	status &= port->read_status_mask;

        UART_PUT_UTCR(port, AT91C_US_RSTSTA);  /* clear error */
	if (status & AT91C_US_PARE)
		flg = TTY_PARITY;
	else if (status & AT91C_US_FRAME)
		flg = TTY_FRAME;

	if (status & AT91C_US_OVRE) {
		/*
		 * overrun does *not* affect the character
		 * we read from the FIFO
		 */
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		ch = 0;
		flg = TTY_OVERRUN;
	}
#ifdef SUPPORT_SYSRQ
	info->sysrq = 0;
#endif
	goto error_return;
}

static void at91us3_tx_chars(struct uart_info *info)
{
	struct uart_port *port = info->port;
	u_int bytes_to_send;
	u_int circ_cnt_to_end;
	u_int xmit_pt;

	// Update the tail pointer
	if (UART_GET_UTCSR(port) & AT91C_US_TXEMPTY) {
		xmit_pt = (bus_to_virt((u_int)UART_GET_TPR(port)) - (u_int)info->xmit.buf);
		if (xmit_pt > info->xmit.tail)
			port->icount.tx += (xmit_pt - info->xmit.tail);
		else
			port->icount.tx += (xmit_pt + (UART_XMIT_SIZE -1 - info->xmit.tail));

		info->xmit.tail = xmit_pt;
	}

	if (port->x_char) {
	        UART_PUT_CHAR(port, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	bytes_to_send    = CIRC_CNT(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE);

	if (bytes_to_send <= WAKEUP_CHARS)
		uart_event(info, EVT_WRITE_WAKEUP);
	else
		bytes_to_send -= WAKEUP_CHARS;

	if (!bytes_to_send || info->tty->stopped || info->tty->hw_stopped) {
		at91us3_stop_tx(info->port, 0);
		return;
	}

	// Init PDC and next PDC
	xmit_pt = info->xmit.tail + bytes_to_send;
	if (xmit_pt < UART_XMIT_SIZE) {
		UART_SET_PDC (port, virt_to_bus(info->xmit.buf + info->xmit.tail), bytes_to_send);
		UART_SET_NPDC(port, virt_to_bus(info->xmit.buf + xmit_pt), 0);
	}
	else {
		circ_cnt_to_end = CIRC_CNT_TO_END(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE);
		UART_SET_PDC(port, virt_to_bus(info->xmit.buf + info->xmit.tail), circ_cnt_to_end);
		UART_SET_NPDC(port, virt_to_bus(info->xmit.buf), bytes_to_send - circ_cnt_to_end);
	}
}

static void at91us3_int(int irq, void *dev_id, struct pt_regs *regs)
{
        struct uart_info *info = dev_id;
	struct uart_port *port = info->port;
        unsigned int status, unmask_status, pass_counter = 0;


	unmask_status = UART_GET_UTCSR(port);
        status =  unmask_status & UART_GET_UTIMR(port);
	if (status) {
	         status &= port->read_status_mask | ~AT91C_US_TXEMPTY;
		 do {
			 if (status & AT91C_US_RXRDY) {
				 at91us3_rx_chars(info, regs);
			 }

		      	 /* Clear the relevent break bits */
			 if (status & AT91C_US_RXBRK){
			           UART_PUT_UTCR(port, status & AT91C_US_RSTSTA);

			           port->icount.brk++;

#ifdef SUPPORT_SYSRQ
			           if (port->line == at91us3_console.index &&
				       !info->sysrq) {
				           info->sysrq = jiffies + HZ*5;
			       }
#endif
			  }
			  if (status & AT91C_US_TXEMPTY){
			           at91us3_tx_chars(info);
			  }
		       if (status & AT91C_US_DCDIC){
			       uart_handle_dcd_change(info, unmask_status & AT91C_US_DCD);
 		       }
 		       if (unmask_status & AT91C_US_CTSIC){
 			       uart_handle_cts_change(info, unmask_status & AT91C_US_CTS);
 		       }
 		       if (unmask_status & AT91C_US_DSRIC){
 			       port->icount.dsr++;
 		       }
 		       if (status & AT91C_US_RIIC){
 			       port->icount.rng++;
 		       }
 		       if (status & ( AT91C_US_DCDIC | AT91C_US_CTSIC | AT91C_US_DSRIC | AT91C_US_RIIC) ){
 			       wake_up_interruptible(&info->delta_msr_wait);
 		       }
			  if (pass_counter++ > AT91US3_ISR_PASS_LIMIT)
			           break;
			  status = UART_GET_UTCSR(port);
			  status &= port->read_status_mask | ~AT91C_US_TXEMPTY;
		 }
		 while (status & (AT91C_US_TXEMPTY | AT91C_US_RXRDY));
	}
}

/*
 * Return TIOCSER_TEMT when transmitter is not busy.
 */
static u_int at91us3_tx_empty(struct uart_port *port)
{
        return UART_GET_UTCSR(port) & AT91C_US_TXEMPTY ? TIOCSER_TEMT : 0;
}

static u_int at91us3_get_mctrl(struct uart_port *port)
{
	u_int ret = 0;
 	unsigned int status,mode;

 	mode = UART_GET_UTMR(port);
 	if ( ( mode & AT91C_US_USMODE ) & AT91C_US_USMODE_MODEM ) {
 		status=UART_GET_UTCSR(port);
 		if (status & AT91C_US_DCD)
 			ret |= TIOCM_CD;
 		if (status & AT91C_US_CTS)
 			ret |= TIOCM_CTS;
 		if (status & AT91C_US_DSR)
 			ret |= TIOCM_DSR;
 		if (status & AT91C_US_RI)
 			ret |= TIOCM_RI;
 	} else {
 		ret = TIOCM_CD | TIOCM_CTS | TIOCM_DSR;

 	}

	return ret;
}

static void at91us3_set_mctrl(struct uart_port *port, u_int mctrl)
{
	unsigned int control,mode;
 	mode = UART_GET_UTMR(port);
 	control = 0;

 	if ( ( mode & AT91C_US_USMODE ) & AT91C_US_USMODE_MODEM ) {
 		if (mctrl & TIOCM_RTS)
 			control |= AT91C_US_RTSEN;
 		else
 			control |= AT91C_US_RTSDIS;

 		if (mctrl & TIOCM_DTR)
 			control |= AT91C_US_DTREN;

 		else
 			control |=  AT91C_US_DTRDIS;
 		UART_PUT_UTCR(port,control);
 	}
}

/*
 * Interrupts always disabled.
 */
static void at91us3_break_ctl(struct uart_port *port, int break_state)
{
	UART_PUT_UTCR(port, AT91C_US_STPBRK);
}

static int at91us3_startup(struct uart_port *port, struct uart_info *info)
{
	/* Allocate the IRQ */
	int retval;
	retval = request_irq(port->irq, at91us3_int, SA_SHIRQ, "serial_at91us3", info);
	if (retval) {
		printk("***at91us3_startup2 Error: can't get irq\n");
		return retval;
	}

	/* Enable peripheral clock if required */
	if (port->irq != AT91C_ID_SYS)
		((AT91PS_SYS)AT91C_VA_BASE_SYS)->PMC_PCER = (1 << port->irq);

	port->read_status_mask = AT91C_US_RXRDY | AT91C_US_TXEMPTY | AT91C_US_OVRE |
		AT91C_US_FRAME | AT91C_US_PARE | AT91C_US_RXBRK;

	UART_SET_PDC(port, virt_to_bus(info->xmit.buf + info->xmit.tail), 0);
	UART_SET_NPDC(port, virt_to_bus(info->xmit.buf + info->xmit.tail), 0);

	/* Finally, clear and enable interrupts */
	UART_PUT_UTIDR(port, -1);
	UART_PUT_UTCR(port, AT91C_US_TXEN | AT91C_US_RXEN);  /* enable xmit & rcvr */
	UART_PUT_PDC_PTCR(port, AT91C_PDC_TXTEN);  /* enable xmit & rcvr */
	UART_PUT_UTIER(port, AT91C_US_RXRDY );  /* do receive only */
	return 0;
}

static void at91us3_shutdown(struct uart_port *port, struct uart_info *info)
{
	/* Disable peripheral clock if required */
	if (port->irq != AT91C_ID_SYS)
		((AT91PS_SYS)AT91C_VA_BASE_SYS)->PMC_PCDR = (1 << port->irq);

	/* Free the interrupt */
	free_irq(port->irq, info);

	/* Disable all interrupts, port and break condition.  */
	UART_PUT_UTCR(port, AT91C_US_RSTSTA);
	UART_PUT_UTIDR(port, -1);
}

static void at91us3_change_speed(struct uart_port *port, u_int cflag, u_int iflag, u_int quot)
{
	unsigned long flags;
	u_int utmr;
        int imr;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS7:	utmr = 0;		break;
	default:	utmr = AT91C_US_CHRL_8_BITS;	break;
	}
	if (cflag & CSTOPB)
		utmr |= AT91C_US_NBSTOP_1_BIT;
	if (cflag & PARENB) {
		if (!(cflag & PARODD))
                  utmr |= AT91C_US_PAR_ODD;
		else
                  utmr |= AT91C_US_PAR_EVEN;
	}
        else
                  utmr |= AT91C_US_PAR_NONE;

	port->read_status_mask |= AT91C_US_OVRE;
	if (iflag & INPCK)
		port->read_status_mask |= AT91C_US_FRAME | AT91C_US_PARE;
	if (iflag & (BRKINT | PARMRK))
		port->read_status_mask |= AT91C_US_RXBRK;

	/*
	 * Characters to ignore
	 */
	port->ignore_status_mask = 0;
	if (iflag & IGNPAR)
		port->ignore_status_mask |= (AT91C_US_FRAME | AT91C_US_PARE);
	if (iflag & IGNBRK) {
		port->ignore_status_mask |= AT91C_US_RXBRK;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (iflag & IGNPAR)
			port->ignore_status_mask |= AT91C_US_OVRE;
	}

	/* first, disable interrupts and drain transmitter */
	local_irq_save(flags);
	imr = UART_GET_UTIMR(port);  /* get interrupt mask */
	UART_PUT_UTIDR(port, -1);
	local_irq_restore(flags);
	while (!(UART_GET_UTCSR(port) & AT91C_US_TXEMPTY));

	/* then, disable everything */
	UART_PUT_UTCR(port, AT91C_US_TXDIS | AT91C_US_RXDIS);

	/* set the parity, stop bits and data size */
	UART_PUT_UTMR(port, utmr);
	/* set the baud rate */
	UART_PUT_UTBRGR(port, quot);

	UART_PUT_UTCR(port, AT91C_US_TXEN | AT91C_US_RXEN);

        UART_PUT_UTIER(port, imr);  /* set them back the way they were */
}

static const char *at91us3_type(struct uart_port *port)
{
        return port->type == PORT_AT91US3 ? "AT91US3" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'.
 */
static void at91us3_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, UART_PORT_SIZE);
}

/*
 * Request the memory region(s) being used by 'port'.
 */
static int at91us3_request_port(struct uart_port *port)
{
 	return request_mem_region(port->mapbase,
		  port->membase == (void *) &(((AT91PS_SYS)AT91C_VA_BASE_SYS)->DBGU_CR) ? UART_PORT_DBGU_SIZE : UART_PORT_SIZE,
		  "serial_at91us3") != NULL ? 0 : -EBUSY;
}

/*
 * Configure/autoconfigure the port.
 */
static void at91us3_config_port(struct uart_port *port, int flags)
{
	if (!(flags & UART_CONFIG_TYPE))
		printk("at91us3_config_port Error, wrong type\n");
        else
		if (at91us3_request_port(port) == 0)
			port->type = PORT_AT91US3;
		else
			printk("at91us3_config_port Error, can't get port \n");
}

/*
 * Verify the new serial_struct (for TIOCSSERIAL).
 * The only change we allow are to the flags and type, and
 * even then only between PORT_AT91US3 and PORT_UNKNOWN
 */
static int at91us3_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	int ret = 0;
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_AT91US3)
		ret = -EINVAL;
	if (port->irq != ser->irq)
		ret = -EINVAL;
	if (ser->io_type != SERIAL_IO_MEM)
		ret = -EINVAL;
	if (port->uartclk / 16 != ser->baud_base)
		ret = -EINVAL;
	if ((void *)port->mapbase != ser->iomem_base)
		ret = -EINVAL;
	if (port->iobase != ser->port)
		ret = -EINVAL;
	if (ser->hub6 != 0)
		ret = -EINVAL;
	return ret;
}

#ifdef CONFIG_SERIAL_AT91US3_CONSOLE

/*
 * Interrupts are disabled on entering
 */
static void at91us3_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = at91us3_ports + co->index;
	u_int status, i;
        int imr;

	/*
	 *	First, save UTCR3 and then disable interrupts
	 */
	imr = UART_GET_UTIMR(port);  /* get interrupt mask */
	UART_PUT_UTIDR(port, AT91C_US_RXRDY | AT91C_US_TXEMPTY);

	/*
	 *	Now, do each character
	 */

	do {
		status = UART_GET_UTCSR(port);
	} while (!(status & AT91C_US_TXEMPTY));

	for (i = 0; i < count; i++) {
		do {
			status = UART_GET_UTCSR(port);
		} while (!(status & AT91C_US_TXRDY));
		UART_PUT_CHAR(port, s[i]);
		if (s[i] == '\n') {
			do {
				status = UART_GET_UTCSR(port);
			} while (!(status & AT91C_US_TXRDY));
			UART_PUT_CHAR(port, '\r');
		}
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore UTCR3
	 */
	do {
		status = UART_GET_UTCSR(port);
	} while (!(status & AT91C_US_TXRDY));
        UART_PUT_UTIER(port, imr);  /* set them back the way they were */
}

static kdev_t at91us3_console_device(struct console *co)
{
	return MKDEV(SERIAL_AT91US3_MAJOR,SERIAL_AT91US3_MINOR + co->index);
}

/*
 * If the port was already initialised (eg, by a boot loader), try to determine
 * the current setup.
 */
static void __init
at91us3_console_get_options(struct uart_port *port, int *baud, int *parity, int *bits)
{
	unsigned int utcr3, utmr, quot;

	utcr3 = UART_GET_UTCR(port) & (AT91C_US_RXEN | AT91C_US_TXEN);
	if (utcr3 == (AT91C_US_RXEN | AT91C_US_TXEN)) {
		/* ok, the port was enabled */

		utmr = UART_GET_UTMR(port) & AT91C_US_PAR;

		*parity = 'n';
                if (utmr == AT91C_US_PAR_EVEN)
		        *parity = 'e';
                else
		        if (utmr == AT91C_US_PAR_ODD)
			        *parity = 'o';
	}

	utmr = UART_GET_UTMR(port) & AT91C_US_CHRL;
	if (utmr == AT91C_US_CHRL_8_BITS)
		*bits = 8;
	else
		*bits = 7;

	quot = UART_GET_UTBRGR(port);
	*baud = port->uartclk / (16 * (quot));
}

#ifndef CONFIG_AT91US3_DEFAULT_BAUDRATE
#define CONFIG_AT91US3_DEFAULT_BAUDRATE	115200
#endif

static int __init
at91us3_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = CONFIG_AT91US3_DEFAULT_BAUDRATE;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	port = uart_get_console(at91us3_ports, UART_NR, co);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		at91us3_console_get_options(port, &baud, &parity, &bits);

	UART_PUT_UTCR(port, AT91C_US_TXEN | AT91C_US_RXEN);  /* enable xmit & rcvr */
	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console at91us3_console = {
	name:		"ttyS",
	write:		at91us3_console_write,
	device:		at91us3_console_device,
	setup:		at91us3_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init at91us3_rs_console_init(void)
{
	register_console(&at91us3_console);
}

#define AT91US3_CONSOLE	&at91us3_console
#else
#define AT91US3_CONSOLE	NULL
#endif

static struct uart_driver at91us3_reg = {
	owner:			THIS_MODULE,
	normal_major:		SERIAL_AT91US3_MAJOR,
#ifdef CONFIG_DEVFS_FS
	normal_name:		"ttyS%d",
	callout_name:		"cua%d",
#else
	normal_name:		"ttyS",
	callout_name:		"cua",
#endif
	normal_driver:		&normal,
	callout_major:		CALLOUT_AT91US3_MAJOR,
	callout_driver:		&callout,
	table:			at91us3_table,
	termios:		at91us3_termios,
	termios_locked:		at91us3_termios_locked,
	minor:			SERIAL_AT91US3_MINOR,
	nr:			UART_NR,
	port:			at91us3_ports,
	cons:			AT91US3_CONSOLE,
};

static int __init at91us3_serial_init(void)
{
	return uart_register_driver(&at91us3_reg);
}

static void __exit at91us3_serial_exit(void)
{
	uart_unregister_driver(&at91us3_reg);
}

module_init(at91us3_serial_init);
module_exit(at91us3_serial_exit);

EXPORT_NO_SYMBOLS;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atmel Rousset");
MODULE_DESCRIPTION("AT91US3 USART3 serial driver");
