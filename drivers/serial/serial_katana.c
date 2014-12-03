/*
 *  linux/drivers/char/serial_katana.c
 *
 *  Driver for KATANA serial ports
 *
 *  Based on drivers/serial/serial_amba.c
 *
 *  Copyright (C) 2003 MediaQ, Inc.
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
 * Changelog:
 *	14-02-2003: LINEO Japan, Inc.
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
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

#if defined(CONFIG_SERIAL_KATANA_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>

#include <asm/hardware/serial_katana.h>
#include <asm/arch/platform.h>
#include <asm/arch/MQK_Uart.h>
#include <asm/arch/uncompress.h>

#if defined(CONFIG_SERIAL_KATANA_CONSOLE)
static struct console katana_console;
static int lsr_break_flag;
#endif

//UART1 is already initialized by bootloader.

#define USE_U2_U3	//use U2 and U3
#define HAS_UART_INT	//interrupt support
#define	MQ_TX_ALL	//transmit everything in one short

#ifdef USE_U2_U3
#define UART_NR		3
#else
#define	UART_NR		1
#endif

typedef volatile struct MQKTUART1RegBlock {
	unsigned long	cntrl0;
	unsigned long	cntrl1;
	unsigned long	cntrl2;
	unsigned long	data;
	unsigned long	status;
} MQKTUART1Reg;

#define	USE_SA1100_SER
#ifdef USE_SA1100_SER
//HSU Temporary borrow SA1100's numbers
#define SERIAL_KATANA_MAJOR	204
#define SERIAL_KATANA_MINOR	5
#define SERIAL_KATANA_NR	UART_NR

#define CALLOUT_KATANA_NAME	"cusa"
#define CALLOUT_KATANA_MAJOR	205
#define CALLOUT_KATANA_MINOR	5
#define CALLOUT_KATANA_NR	UART_NR

#else	//USE_SA1100_SER

//HSU Temporary borrow AMBA's numbers
#define SERIAL_KATANA_MAJOR	204
#define SERIAL_KATANA_MINOR	16
#define SERIAL_KATANA_NR	UART_NR

#define CALLOUT_KATANA_NAME	"cukt"
#define CALLOUT_KATANA_MAJOR	205
#define CALLOUT_KATANA_MINOR	16
#define CALLOUT_KATANA_NR	UART_NR
#endif

static struct tty_driver normal, callout;
static struct tty_struct *katana_table[UART_NR];
static struct termios *katana_termios[UART_NR], *katana_termios_locked[UART_NR];
#ifdef SUPPORT_SYSRQ
static struct console katana_console;
#endif

#define KATANA_ISR_PASS_LIMIT	256

/*
 * Access macros for the KATANA UARTs
 */
#define	U1_OFFSET	MQ_REG_PHYS_OFFSET(MQ9000_UART1_BASE)
#define	U2_OFFSET	MQ_REG_PHYS_OFFSET(MQ9000_UART2_BASE)
#define	U3_OFFSET	MQ_REG_PHYS_OFFSET(MQ9000_UART3_BASE)

#define UART_PORT_SIZE		64
	//memory region to access our uart regs.  64 bytes is more than enough.

/*
 * Our private driver data mappings.
 */
#define drv_old_status	driver_priv

//status of DCD, DSR and CTS
//**** Missing the followings for Modem status ****
#define	MQ_DTR_ON	0x80000000	//has to come from dtr_gpio_enable
#define	MQ_DTR_INTR	0x80000000	//dtr_gpio_intStat
#define	MQ_DSR_ON	0x40000000	//has to come from dsr_gpio_enable
#define	MQ_DSR_INTR	0x40000000	//dsr_gpio_intStat

#define	STATUS_CTS	MQ_CTS_INTR
#define	STATUS_DCD	MQ_DCD_INTR
#define	STATUS_DSR	MQ_DSR_INTR
#define	STATUS_DTR	MQ_DSR_INTR
#if (STATUS_CTS & STATUS_DCD & STATUS_DSR & STATUS_DTR)
error STATUS_xxx bits overlapped each other
#endif


#ifdef USE_U2_U3

static void katana_uart_stop_tx2(struct uart_port *port, u_int from_tty)
{
	//???? Supposed to be clearing tx interrupt enable mask bit
	MQKTUARTReg *pUART = (MQKTUARTReg *)(port->membase);
	pUART->inCntrl &= ~MQ_TXFIFO_INTR;
}

static void katana_uart_tx_chars2(struct uart_info *);

static void katana_uart_start_tx2(struct uart_port *port, u_int nonempty, u_int from_tty)
{
	//???? Supposed to be setting tx interrupt enable mask bit
	if (nonempty) {
		MQKTUARTReg *pUART = (MQKTUARTReg *)(port->membase);
		pUART->inStat = MQ_TXFIFO_INTR;
		pUART->inCntrl |= MQ_TXFIFO_INTR;
	}
}

static void katana_uart_stop_rx2(struct uart_port *port)
{
	//???? Supposed to be clearing rx interrupt enable mask bit
	MQKTUARTReg *pUART = (MQKTUARTReg *)(port->membase);
	pUART->inCntrl &= ~MQ_RXFIFO_INTR;
}

static void katana_uart_enable_ms2(struct uart_port *port)
{
	//???? What to do here ??????
	/*
	cr = UART_GET_CR(port);
	cr |= AMBA_UARTCR_MSIE;
	UART_PUT_CR(port, cr);
	*/
}

#ifdef HAS_UART_INT
static void
#ifdef SUPPORT_SYSRQ
katana_uart_rx_chars2(struct uart_info *info, struct pt_regs *regs)
#else
katana_uart_rx_chars2(struct uart_info *info)
#endif
{
	unsigned long	ch, status, max_count = 256;
	struct tty_struct *tty = info->tty;
	struct uart_port *port = info->port;
	MQKTUARTReg *pUART = (MQKTUARTReg *)(port->membase);
//	unsigned char *pch = (unsigned char *)(tty->flip.char_buf_ptr);	//TEMP
//	unsigned char  buf[64];						//TEMP

	//check if fifo has something
	//HSU: write the code this way instead of assigning status in the
	//  while statement to avoid compiler optimization.
	status = pUART->inStat;
	while ( !(status & MQ_RXFIFO_EMPTY) && max_count-- )
	{
		pUART->rxBufSize = 0xff;
		ch = pUART->rxData;
		pUART->inStat = (MQ_RXFIFO_INTR
			//| MQ_RXBRADK_INTR
			| MQ_RXMISC_INTR | 0xf8000000
			);
				//clr rx fifo int
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
		{
			tty->flip.tqueue.routine((void *)tty);
			if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			{
				printk(KERN_WARNING "TTY_DONT_FLIP set\n");
				return;
			}
		}

		/*
		 * Note that the error handling code is
		 * out of the main execution path
		 */
		*tty->flip.char_buf_ptr = (unsigned char)ch;
		*tty->flip.flag_buf_ptr = TTY_NORMAL;
		port->icount.rx++;

		if (ch & MQ_RXBREAK_ERR)		//raw break
		{
			port->icount.brk++;
			if (uart_handle_break(info, &katana_console))
				goto ignore_char2;
		}
		else if (ch & MQ_RXPARITY_ERR)	//parity err
			port->icount.parity++;
		else if (ch & MQ_RXFRAME_ERR)		//frame err
			port->icount.frame++;
		if (status & MQ_RXMISC_INTR)
			port->icount.overrun++;
				//Some special condition?, treat as overrun

		ch &= port->read_status_mask;

		if (ch & MQ_RXBREAK_ERR)
			*tty->flip.flag_buf_ptr = TTY_BREAK;
		else if (ch & MQ_RXPARITY_ERR)
			*tty->flip.flag_buf_ptr = TTY_PARITY;
		else if (ch & MQ_RXFRAME_ERR)
			*tty->flip.flag_buf_ptr = TTY_FRAME;

		if (uart_handle_sysrq_char(info, ch, regs))
			goto ignore_char2;

		//port->unused[0] location to contain read_status_mask for
		//  MQ_RXBRADK_INTR and/or MQ_RXMISC_INTR if they are set
		//  in change_speed2() routine. port->unused[1] has
		//  corresponding read_ignore_mask
		//
		if ((ch & port->ignore_status_mask) == 0
			&& (status & port->unused[1]) == 0 )
		{
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if ((status & MQ_RXMISC_INTR)
			 && tty->flip.count < TTY_FLIPBUF_SIZE)
		{
			/*
			 * Overrun is special, since it's reported
			 * immediately, and doesn't affect the current
			 * character
			 */
			*tty->flip.char_buf_ptr++ = 0;
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
			tty->flip.count++;
		}
	ignore_char2:
		status = pUART->inStat;
#if 0
		if ( (unsigned char)ch != '\n' || (unsigned char)ch != '\r' )
		{
			status = pUART->inStat;
			while ( (status & MQ_RXFIFO_EMPTY) )
			{
				status = pUART->inStat;
				barrier();
			}
		}
#endif
	}
#if 0
//TEMP - begin
	buf[0] = 'r';
	buf[1] = 'x';
	buf[2] = ':';
	max_count = 256 - max_count;
	strncpy( &(buf[3]), pch, max_count );
	pch = &(buf[3]);
	while( pch != &(buf[max_count + 3]) )
	{
		if (*pch == 0xd)
			*pch = '\\';
		else if ( *pch == 0xa )
			*pch = '\?';
		pch++;
	}
	sprintf(pch, ":%x:%d\n", status, max_count);
	puts2( &(buf[0]), 1 );
//TEMP - end
#endif
	tty_flip_buffer_push(tty);
	return;
}
#endif	//HAS_UART_INT

#ifdef HAS_UART_INT
static void katana_uart_tx_chars2(struct uart_info *info)
{
	MQKTUARTReg	*pUART;
	struct uart_port *port = info->port;
	//int count;

	pUART = (MQKTUARTReg *)(port->membase);

	if (port->x_char) {
		while( !(pUART->inStat & MQ_TXFIFO_EMPTY) )
			barrier();
		pUART->txBufSize = 0xff;
		pUART->txData = port->x_char;
		port->icount.tx++;
		port->x_char = 0;
		//pUART->inStat = MQ_TXFIFO_INTR;	//clear it
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
		katana_uart_stop_tx2(port, 0);
		pUART->inStat = MQ_TXFIFO_INTR;	//clear it
		return;
	}

	//Simply keep sending them out for now!
#ifdef MQ_TX_ALL	//HSU always sent everything out
	do {
		while( !(pUART->inStat & MQ_TXFIFO_EMPTY) )
			barrier();
		pUART->txBufSize = 0xff;
		pUART->txCntrl |= (MQ_TX_RESET | MQ_TXFIFO_RESET);
		pUART->txCntrl &= ~(MQ_TX_RESET | MQ_TXFIFO_RESET);
		pUART->txData = info->xmit.buf[info->xmit.tail];
#if 0 // LINEO Japan, inc. Feb.08 2003
		while( !(pUART->inStat & MQ_TXFIFO_EMPTY) )
			barrier();
#endif
		info->xmit.tail = (info->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	} while (1);
#else
	count = port->fifosize >> 1;
	do {
		while( !(pUART->inStat & MQ_TXFIFO_EMPTY) )
			barrier();
		pUART->txBufSize = 1;
		pUART->txData = info->xmit.buf[info->xmit.tail];
		info->xmit.tail = (info->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	} while (--count > 0);
#endif

	if (CIRC_CNT(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE) <
			WAKEUP_CHARS)
		uart_event(info, EVT_WRITE_WAKEUP);

	if (info->xmit.head == info->xmit.tail)
		katana_uart_stop_tx2(info->port, 0);
	pUART->inStat = MQ_TXFIFO_INTR;	//clear it
}

#ifdef USE_UART_MODEM
static void katana_uart_modem_status2(struct uart_info *info)
{
	struct uart_port *port = info->port;
	MQKTUARTReg	*pUART = (MQKTUARTReg *)(port->membase);
	unsigned long status, delta;

	//UART_PUT_ICR(port, 0); ???????

	status = pUART->inStat & (MQ_CTS_INTR | MQ_DCD_INTR);
	//status |= dtr_gpio;
	delta = status ^ (info->drv_old_status
		 & (MQ_CTS_INTR | MQ_DCD_INTR | MQ_DTR_INTR | MQ_DSR_INTR);
	info->drv_old_status = status;

	if (!delta)
		return;

	if (delta & MQ_DCD_INTR)
		uart_handle_dcd_change(info, status & MQ_DCD_INTR);

	if (delta & MQ_DSR_INTR)
		port->icount.dsr++;

	if (delta & MQ_CTS_INTR)
		uart_handle_cts_change(info, status & MQ_CTS_INTR);

	wake_up_interruptible(&info->delta_msr_wait);
}
#endif	//USE_UART_MODEM
#endif	//HAS_UART_INT

#ifdef HAS_UART_INT
static void katana_uart_int2(int irq, void *dev_id, struct pt_regs *regs)
{
	struct uart_info *info = dev_id;
	MQKTUARTReg	*pUART = (MQKTUARTReg *)(info->port->membase);
	unsigned long status, pass_counter = 256;
				//256 is arbituary number ????

#if 1	//Use check on FIFO empty rather than int status bits
	status = (volatile unsigned long)(pUART->inStat);
	//while ( (status & (MQ_RXFIFO_EMPTY | MQ_TXFIFO_EMPTY))
	//	!= (MQ_RXFIFO_EMPTY | MQ_TXFIFO_EMPTY) )
	while ( !(status & MQ_RXFIFO_EMPTY) || (status & MQ_TXFIFO_INTR) )
	{
		//if ( !(status & MQ_RXFIFO_EMPTY) )
		if ( status & MQ_RXFIFO_INTR )
	#ifdef SUPPORT_SYSRQ
			katana_uart_rx_chars2(info, regs);
	#else
			katana_uart_rx_chars2(info);
	#endif
		//HSU: Cannot check fifo empty here! Hopefully txfifo_intr
		//     status is stable enough
		//if ( !(status & MQ_TXFIFO_EMPTY) )
		if ( status & MQ_TXFIFO_INTR ) {
			while( !(pUART->inStat & MQ_TXFIFO_EMPTY) )
				barrier();
			katana_uart_tx_chars2(info);
		}
	#ifdef USE_UART_MODEM_STATUS
		if ( status & MQ_MODEM_INTR )
			katana_uart_modem_status2(info);
	#endif

		if (pass_counter-- == 0)
			break;
		status = (volatile unsigned long)(pUART->inStat);
	}
#else
	status = pUART->inStat;
	while ( status & 
		(MQ_RXFIFO_INTR | MQ_RXMISC_INTR
		 | MQ_TXFIFO_INTR
		 | MQ_MODEM_INTR) )
	{
		if ( status & (MQ_RXFIFO_INTR | MQ_RXMISC_INTR) )
	#ifdef SUPPORT_SYSRQ
			katana_uart_rx_chars2(info, regs);
	#else
			katana_uart_rx_chars2(info);
	#endif
		if ( status & MQ_TXFIFO_INTR )
			katana_uart_tx_chars2(info);
	#ifdef USE_UART_MODEM_STATUS
		if ( status & MQ_MODEM_INTR )
			katana_uart_modem_status2(info);
	#endif

		if (pass_counter-- == 0)
			break;
		status = pUART->inStat;
	}
#endif
}
#endif	//HAS_UART_INT

static u_int katana_uart_tx_empty2(struct uart_port *port)
{
	MQKTUARTReg	*pUART;
	pUART = (MQKTUARTReg *)(port->membase);
	
	return ((pUART->inStat & MQ_TXFIFO_EMPTY) != 0);
}

static u_int katana_uart_get_mctrl2(struct uart_port *port)
{
	unsigned int result = TIOCM_CD;
#ifdef USE_UART_MODEM
	unsigned long status;

	MQKTUARTReg	*pUART;
	pUART = (MQKTUARTReg *)(port->membase);

	status = pUART->inStatus;
	if ( status & MQ_DCD_INTR )
		result |= TIOCM_CAR;
	//*** if ( dtr_gpio & MQ_DSR_INTR ) //must come from GPIO????
	//	result |= TIOCM_DSR;
	if ( status & MQ_CTS_INTR )
		result |= TIOCM_CTS;
#endif	//USE_UART_MODEM
	return result;
}

static void katana_uart_set_mctrl2(struct uart_port *port, u_int mctrl)
{
#ifdef USE_UART_MODEM
	MQKTUARTReg	*pUART;
	unsigned long	cntrl;
	//unsigned long	gpio = 0;
	pUART = (MQKTUARTReg *)(port->membase);

	cntrl = pUART->rxCntrl;
	if (mctrl & TIOCM_RTS)
		cntrl |= MQ_RXRTS_ON;
	else
		cntrl &= ~MQ_RXRTS_ON;

	//if (mctrl & TIOCM_DTR)
	//	gpio |= MQ_DTR_ON;
	//else
	//	gpio &= ~MQ_DTR_ON;

	pUART->rxCntrl = cntrl;
	//*** dtr_gpio = gpio;
#endif	//USE_UART_MODEM
}

static void katana_uart_break_ctl2(struct uart_port *port, int break_state)
{
	MQKTUARTReg	*pUART;
	unsigned 	long cntrl;
	pUART = (MQKTUARTReg *)(port->membase);

	cntrl = pUART->txCntrl;
	if (break_state == -1)
		cntrl |= MQ_TXBREAK_ON;
	else
		cntrl &= ~MQ_TXBREAK_ON;
	pUART->txCntrl = cntrl;
}

static int katana_uart_startup2(struct uart_port *port, struct uart_info *info)
{
	MQKTUARTReg	*pUART;
	int retval = 0;

	pUART = (MQKTUARTReg *)(port->membase);

	//pUART->rxBufSize = 1;	//Do we need this here too???
	pUART->rxBufSize = 0xff;	//Do we need this here too???

#ifdef HAS_UART_INT
	retval = request_irq(port->irq, katana_uart_int2, 0, "katana", info);
	if (retval)
		return retval;

	#ifdef HAS_UART_MODEM
	//status of DCD, DSR and CTS
	info->drv_old_status = pUART->inStat & (MQ_CTS_INTR | MQ_DCD_INTR);
	//info->drv_old_status |= dsr_gpio & MQ_DSR_INTR;
	//info->drv_old_status |= dsr_gpio & MQ_DTR_INTR;
	#else
	info->drv_old_status = 0;	//no modem stuff??
	#endif

	//HSU$$$$$$$: copy this to change_speed2 too!
	pUART->inCntrl = MQ_RXFIFO_INTR		//rx int
			| MQ_RXMISC_INTR;	//  special err (over run err)

#endif	//HAS_UART_INT

	return 0;
}

static void katana_uart_shutdown2(struct uart_port *port, struct uart_info *info)
{
	MQKTUARTReg	*pUART = (MQKTUARTReg *)(port->membase);
	/*
	 * Free the interrupt
	 */
	free_irq(port->irq, info);

	/*
	 * disable all interrupts, disable the port
	 */

//printk("katana_uart_shutdown2\n");
//puts2("katana_uart_shutdown2\n", 1);
	pUART->inCntrl = 0;
#if 0	//TEMP
	pUART->txCntrl = 0;
	pUART->rxCntrl = 0;
#endif
}

static void katana_uart_change_speed2(struct uart_port *port, u_int cflag,
		u_int iflag, u_int quot)
{
	MQKTUARTReg	*pUART = (MQKTUARTReg *)(port->membase);
	unsigned long 	txCntrl = 0, rxCntrl = 0;
	unsigned long  	flags;
	unsigned long	divisor, ps, mp, div, tdclk, rdclk;

	// port->unused[0] location to contain read_status_mask for
	//  MQ_RXBRADK_INTR and/or MQ_RXMISC_INTR if they are set
	//  and port->unused[1] location to contain read_ignore_mask

	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5: txCntrl |= MQ_TXCHAR_5; break;
	case CS6: txCntrl |= MQ_TXCHAR_6; break;
	case CS7: txCntrl |= MQ_TXCHAR_7; break;
	default:  txCntrl |= MQ_TXCHAR_8; break; // CS8
	}
	if (cflag & CSTOPB)
		txCntrl |= MQ_TXSTOP_BIT2;	//2 stop bits set
	if (cflag & PARENB)
	{
		txCntrl |= MQ_TXPARITY_ON;	//parity enabled
		rxCntrl |= MQ_RXPARITY_ON;	//parity enabled
		if (!(cflag & PARODD))
		{
			txCntrl |= MQ_TXEVEN_PARITY;	//set even parity
			rxCntrl |= MQ_RXEVEN_PARITY;	//set even parity
		}
	}
	if (port->fifosize > 1)
		/* do what here? */ ;

	port->unused[0] = MQ_RXMISC_INTR;	//want overrun err
	port->read_status_mask = 0;		//init to 0
	if (iflag & INPCK)
		port->read_status_mask |= MQ_RXPARITY_ERR | MQ_RXFRAME_ERR;
						//want frame and parity err
	if (iflag & (BRKINT | PARMRK))
		port->read_status_mask |= MQ_RXBREAK_ERR;
						//want break

	/*
	 * Characters to ignore
	 */
	port->ignore_status_mask = 0;
	if (iflag & IGNPAR)
		port->ignore_status_mask |= MQ_RXPARITY_ERR | MQ_RXFRAME_ERR;
						//ignore frame and parity err
	port->unused[1] = 0;
	if (iflag & IGNBRK) {
		port->ignore_status_mask |= MQ_RXBREAK_ERR; //ignore break
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (iflag & IGNPAR)
			port->unused[1] |= MQ_RXMISC_INTR; // overrun
	}

	/*
	 * Ignore all characters if CREAD is not set.
	 */
	//HSU: need to handle this case where CREAD is not set
	//if ((cflag & CREAD) == 0)
	//	port->ignore_status_mask |= UART_DUMMY_RSR_RX;

	/* Set baud rate */

	/* first, disable everything */
	save_flags(flags); cli();

	/* ftr */
	switch (cflag & (CBAUD | CBAUDEX)) {
	case B38400:	divisor = MQ_PS_38400;	break;
	case B57600:	divisor = MQ_PS_57600;	break;
	case B115200:	divisor = MQ_PS_115200;	break;
	default:	divisor = MQ_PS_38400;
	}

	ps = (divisor << 8) & MQ_BAUD_PS_MASK;
	mp = (divisor << 8) & MQ_BAUD_MP_MASK;
	rdclk = (divisor << 9) & MQ_BAUD_RDCLK_MASK;
	tdclk = (divisor << 10) & MQ_BAUD_TDCLK_MASK;
	div = (divisor >> 16) | (divisor & MQ_BAUD_RDIV_MASK);

	pUART->txCntrl |= (MQ_TX_RESET | MQ_TXFIFO_RESET);
	pUART->rxCntrl |= (MQ_RX_RESET | MQ_RXFIFO_RESET);

	pUART->clock1 &= ~MQ_UART_ENABLE;

	//pUART->txCntrl = MQ_TX_ENABLE | (1 << 8) | txCntrl;
	pUART->txCntrl = MQ_TX_ENABLE | (MQ_TX_THRESHOLD << 8) | txCntrl;
	pUART->rxCntrl = MQ_RX_ENABLE | (MQ_RX_THRESHOLD << 8) | rxCntrl;

	pUART->clock1 &= ~(MQ_BAUD_PS_MASK | MQ_BAUD_MP_MASK |
				MQ_BAUD_TDCLK_MASK |MQ_BAUD_RDCLK_MASK);
	pUART->clock1 |= (MQ_TXBAUD_ON | MQ_RXBAUD_ON |
				ps | mp | tdclk | rdclk | MQ_PLL2_UART_RCLK);
	pUART->clock2 = div;

	//no modem stuff yet!!!!!!!!
#ifdef USE_UART_MODEM
	// hardware flow control
	pUART->txCntrl &= ~MQ_TXCTS_ON;
	pUART->rxCntrl &= ~MQ_RXDCD_ON;

	if ((port->flags & ASYNC_HARDPPS_CD) ||
	    (cflag & CRTSCTS) || !(cflag & CLOCAL)) {
		pUART->txCntrl |= MQ_TXCTS_ON;
		pUART->rxCntrl |= MQ_RXDCD_ON;
	}
#endif

#if 0 // LINEO Japan, inc. Feb.08 2003
	pUART->inCntrl = MQ_TXFIFO_INTR
			| MQ_RXFIFO_INTR	//rx int
			| MQ_RXMISC_INTR	//  special err
			| MQ_TXDONE_INTR;
#else
	pUART->inCntrl = MQ_TXFIFO_INTR
			| MQ_RXFIFO_INTR	//rx int
			| MQ_RXMISC_INTR;	//  special err
#endif

	pUART->rxBufSize = 0xff;
	pUART->clock1 |= MQ_UART_ENABLE;
	restore_flags(flags);
}

static const char *katana_uart_type2(struct uart_port *port)
{
	return port->type == PORT_KATANA ? "SER_KATANA" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'
 */
static void katana_uart_release_port2(struct uart_port *port)
{
	release_mem_region(port->mapbase, UART_PORT_SIZE);
}

/*
 * Request the memory region(s) being used by 'port'
 */
static int katana_uart_request_port2(struct uart_port *port)
{
	return request_mem_region(port->mapbase, UART_PORT_SIZE, "serial_katana")
			!= NULL ? 0 : -EBUSY;
}

/*
 * Configure/autoconfigure the port.
 */
static void katana_uart_config_port2(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_KATANA;
		katana_uart_request_port2(port);
	}
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int katana_uart_verify_port2(struct uart_port *port,
			struct serial_struct *ser)
{
	int ret = 0;
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_KATANA)
		ret = -EINVAL;
	if (ser->irq < 0 || ser->irq >= NR_IRQS)
		ret = -EINVAL;
	//if (ser->baud_base < 9600)
	if (ser->baud_base < 115200)	//**** Only at 115200
		ret = -EINVAL;
	return ret;
}

static struct uart_ops katana_pops2 = {
	tx_empty:	katana_uart_tx_empty2,
	set_mctrl:	katana_uart_set_mctrl2,
	get_mctrl:	katana_uart_get_mctrl2,
	stop_tx:	katana_uart_stop_tx2,
	start_tx:	katana_uart_start_tx2,
	stop_rx:	katana_uart_stop_rx2,
	enable_ms:	katana_uart_enable_ms2,
	break_ctl:	katana_uart_break_ctl2,
	startup:	katana_uart_startup2,
	shutdown:	katana_uart_shutdown2,
	change_speed:	katana_uart_change_speed2,
	type:		katana_uart_type2,
	release_port:	katana_uart_release_port2,
	request_port:	katana_uart_request_port2,
	config_port:	katana_uart_config_port2,
	verify_port:	katana_uart_verify_port2,
};
#endif	//USE_U2_U3

static void katana_uart_stop_tx(struct uart_port *port, u_int from_tty)
{
	//???? Supposed to be clearing tx interrupt enable mask bit
	MQKTUART1Reg *pUART = (MQKTUART1Reg *)(port->membase);
	pUART->cntrl2 &= ~(1 << 9);	//tx fifo thres int
	pUART->status |= (1 << 1);	//clr tx fifo thres int
}

static void katana_uart_start_tx(struct uart_port *port, u_int nonempty, u_int from_tty)
{
	//???? Supposed to be setting tx interrupt enable mask bit
	if (nonempty) {
		MQKTUART1Reg *pUART = (MQKTUART1Reg *)(port->membase);
		pUART->status |= (1 << 1);	//clr tx fifo thres int
		pUART->cntrl2 &= ~(0x1f << 18);	//tx fifo ithres int at 0
		pUART->cntrl2 |= (1 << 9);	//tx fifo ithres nt
	}
}

static void katana_uart_stop_rx(struct uart_port *port)
{
	MQKTUART1Reg *pUART = (MQKTUART1Reg *)(port->membase);
	pUART->cntrl2 &= ~(1 << 8);	//rx fifo ithres nt
	pUART->status &= ~(1 << 0);	//clr rx fifo thres int
}

static void katana_uart_enable_ms(struct uart_port *port)
{
	//???? What to do here ??????
	/*
	cr = UART_GET_CR(port);
	cr |= AMBA_UARTCR_MSIE;
	UART_PUT_CR(port, cr);
	*/
}

#ifdef HAS_UART_INT
static void
#ifdef SUPPORT_SYSRQ
katana_uart_rx_chars(struct uart_info *info, struct pt_regs *regs)
#else
katana_uart_rx_chars(struct uart_info *info)
#endif
{
	//UART1
	unsigned long	status, max_count = 256;
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	struct uart_port *port = info->port;
	MQKTUART1Reg *pUART = (MQKTUART1Reg *)(port->membase);
//	unsigned char *pch = (unsigned char *)(tty->flip.char_buf_ptr);	//TEMP
//	unsigned char  buf[64];						//TEMP

	//check if fifo has something
	status = pUART->status;
	while ( (status & 0x3f000) && max_count-- )
	{
		ch = (unsigned char)(pUART->data);
		pUART->status |= 0x3d;	//clr raw break, parity err, frame err
					//  overrun err and rx fifo int
////$$$ TEMP - start
		//quickly disable rx and renable rx again
		//pUART->cntrl0 = 0x5;
		//pUART->cntrl0 = 0xd;
////$$$ TEMP - end
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
		{
			tty->flip.tqueue.routine((void *)tty);
			if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			{
				printk(KERN_WARNING "TTY_DONT_FLIP set\n");
				return;
			}
		}

		//status = pUART->status;	//read status before data

		/*
		 * Note that the error handling code is
		 * out of the main execution path
		 */
		*tty->flip.char_buf_ptr = ch;
		*tty->flip.flag_buf_ptr = TTY_NORMAL;
		port->icount.rx++;

		if (status & (1 << 5) )		//raw break
		{
			port->icount.brk++;
			if (uart_handle_break(info, &katana_console))
				goto ignore_chars;
		}
		else if (status & (1 << 3))	//parity err
			port->icount.parity++;
		else if (status & (1 << 4))	//frame err
			port->icount.frame++;
		if (status & (1 << 2))		//overrun err
			port->icount.overrun++;

		status &= port->read_status_mask;

#ifdef CONFIG_SERIAL_KATANA_CONSOLE
		if (port->line == katana_console.index) {
			/* Recover the break flag from console xmit */
			status |= lsr_break_flag;
			lsr_break_flag = 0;
		}
#endif

		if (status & (1 << 5))
			*tty->flip.flag_buf_ptr = TTY_BREAK;
		else if (status & (1 << 3))
			*tty->flip.flag_buf_ptr = TTY_PARITY;
		else if (status & (1 << 4))
			*tty->flip.flag_buf_ptr = TTY_FRAME;

		if (uart_handle_sysrq_char(info, ch, regs))
			goto ignore_chars;

		if ((status & port->ignore_status_mask) == 0)
		{
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if ((status & (1 << 2)) &&	//overrun err
		    tty->flip.count < TTY_FLIPBUF_SIZE)
		{
			/*
			 * Overrun is special, since it's reported
			 * immediately, and doesn't affect the current
			 * character
			 */
			*tty->flip.char_buf_ptr++ = 0;
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
			tty->flip.count++;
		}
	ignore_chars:
		status = pUART->status;
	}
#if 0
//TEMP - begin
	buf[0] = 'r';
	buf[1] = 'x';
	buf[2] = ':';
	max_count = 256 - max_count;
	strncpy( &(buf[3]), pch, max_count );
	pch = &(buf[3]);
	while( pch != &(buf[max_count + 3]) )
	{
		if (*pch == 0xd)
			*pch = '\\';
		else if ( *pch == 0xa )
			*pch = '\?';
		pch++;
	}
	sprintf(pch, ":%x:%d\n", status, max_count);
	puts2( &(buf[0]), 1 );
//TEMP - end
#endif
	tty_flip_buffer_push(tty);
	return;
}
#endif	//HAS_UART_INT

#ifdef HAS_UART_INT
/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(struct uart_port *port)
{
	MQKTUART1Reg *pUART1 = (MQKTUART1Reg *)(port->membase);
	unsigned int status;
	unsigned long orgt = jiffies;

	do {
		status = pUART1->status;

		if (status & (1 << 5))
			lsr_break_flag = 1 << 5;
		
		if (jiffies - orgt >= 1) // over 10[ms]
			break;
	} while (!(status & 0x80));
}

static void katana_uart_tx_chars(struct uart_info *info)
{
	MQKTUART1Reg	*pUART;
	struct uart_port *port = info->port;
	int count = 0;

	pUART = (MQKTUART1Reg *)(port->membase);

	if (port->x_char) {
		pUART->data = (unsigned long)(port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		//pUART->status |= 0x2;	//clr tx fifo thres int
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
		katana_uart_stop_tx(port, 0);
		pUART->status |= 0x2;	//clr tx fifo thres int
		return;
	}

#ifdef MQ_TX_ALL	//HSU always sent everything out
	do {
		pUART->data = (unsigned long)(info->xmit.buf[info->xmit.tail]);
		count++;
#if 0
		while( !(pUART->status & 0x80) )
			barrier();
#endif
		info->xmit.tail = (info->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (count == 16) break; // FIFO full
		if (info->xmit.head == info->xmit.tail)
			break;
	} while (1);
#else
	count = port->fifosize >> 1;
	do {
		pUART->data = (unsigned long)(info->xmit.buf[info->xmit.tail]);
		info->xmit.tail = (info->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	} while (--count > 0);
#endif

	if (CIRC_CNT(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE) <
			WAKEUP_CHARS)
		uart_event(info, EVT_WRITE_WAKEUP);

	if (info->xmit.head == info->xmit.tail)
		katana_uart_stop_tx(info->port, 0);

	pUART->status |= 0x2;	//clr tx fifo thres int
}

#ifdef USE_UART_MODEM
static void katana_uart_modem_status(struct uart_info *info)
{
#if 0
	struct uart_port *port = info->port;
	unsigned int status, delta;

	UART_PUT_ICR(port, 0);

	status = UART_GET_FR(port) & AMBA_UARTFR_MODEM_ANY;

	delta = status ^ info->drv_old_status;
	info->drv_old_status = status;

	if (!delta)
		return;

	if (delta & AMBA_UARTFR_DCD)
		uart_handle_dcd_change(info, status & AMBA_UARTFR_DCD);

	if (delta & AMBA_UARTFR_DSR)
		port->icount.dsr++;

	if (delta & AMBA_UARTFR_CTS)
		uart_handle_cts_change(info, status & AMBA_UARTFR_CTS);

	wake_up_interruptible(&info->delta_msr_wait);
#endif
}
#endif	//USE_UART_MODEM
#endif	//HAS_UART_INT

#ifdef HAS_UART_INT
static void katana_uart_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct uart_info *info = dev_id;
	MQKTUART1Reg	*pUART = (MQKTUART1Reg *)(info->port->membase);
	unsigned long status, pass_counter = 256;
				//256 is arbituary number ????

	status = pUART->status;
	do {
		if (status & (1 << 0))	//rx fifo thres int
#ifdef SUPPORT_SYSRQ
			katana_uart_rx_chars(info, regs);
#else
			katana_uart_rx_chars(info);
#endif
		if (status & (1 << 1)) {	//tx fifo thres int
			wait_for_xmitr(info->port); // tx FIFO empty
			katana_uart_tx_chars(info);
		}

	#ifdef USE_UART_MODEM
		//if (status & xxx)
		//	katana_uart_modem_status(info);
	#endif

		if (pass_counter-- == 0)
			break;

		status = pUART->status;
	} while (status & 0x3L);	//wait till tx and/or rx fifo thres int
	//mdelay(1);
}
#endif	//HAS_UART_INT

static u_int katana_uart_tx_empty(struct uart_port *port)
{
	MQKTUART1Reg	*pUART;
	pUART = (MQKTUART1Reg *)(port->membase);
	return ((pUART->status & 0x80) != 0);
}

static u_int katana_uart_get_mctrl(struct uart_port *port)
{
	unsigned int result = TIOCM_CD;
#if 0
	unsigned int status;

	status = UART_GET_FR(port);
	if (status & AMBA_UARTFR_DCD)
		result |= TIOCM_CAR;
	if (status & AMBA_UARTFR_DSR)
		result |= TIOCM_DSR;
	if (status & AMBA_UARTFR_CTS)
		result |= TIOCM_CTS;

#endif
	return result;
}

static void katana_uart_set_mctrl(struct uart_port *port, u_int mctrl)
{
#if 0
	u_int ctrls = 0, ctrlc = 0;

	if (mctrl & TIOCM_RTS)
		ctrlc |= PORT_CTRLS_RTS(port);
	else
		ctrls |= PORT_CTRLS_RTS(port);

	if (mctrl & TIOCM_DTR)
		ctrlc |= PORT_CTRLS_DTR(port);
	else
		ctrls |= PORT_CTRLS_DTR(port);

	__raw_writel(ctrls, SC_CTRLS);
	__raw_writel(ctrlc, SC_CTRLC);
#endif
}

static void katana_uart_break_ctl(struct uart_port *port, int break_state)
{
#if 0
	unsigned int lcr_h;

	lcr_h = UART_GET_LCRH(port);
	if (break_state == -1)
		lcr_h |= AMBA_UARTLCR_H_BRK;
	else
		lcr_h &= ~AMBA_UARTLCR_H_BRK;
	UART_PUT_LCRH(port, lcr_h);
#endif
}

static int katana_uart_startup(struct uart_port *port, struct uart_info *info)
{
#ifdef HAS_UART_INT
	MQKTUART1Reg	*pUART = (MQKTUART1Reg *)(port->membase);
	int retval;
	retval = request_irq(port->irq, katana_uart_int, 0, "katana", info);
	if (retval)
		return retval;

	info->drv_old_status = 0;	//no modem stuff??

	/*
	 * Finally, enable interrupts
	 */

	pUART->cntrl0 = 0;	//disable to clear fifo
	pUART->cntrl2 |= (1 << 8)	//receive int only
//		| (16L << 12)		//rx fifo thres
		| (1L << 12)		//rx fifo thres
//		| (16L << 18)		//tx fifo thres
	#ifdef MQ_TX_ALL
		| (0L << 18)		//tx fifo thres immediate interrupt
	#else
		| (8L << 18)		//tx fifo thres
	#endif
		;
	pUART->cntrl0 = 0xd;	//enable UART1

#endif	//HAS_UART_INT
	return 0;
}

static void katana_uart_shutdown(struct uart_port *port, struct uart_info *info)
{
#ifdef HAS_UART_INT
	MQKTUART1Reg	*pUART = (MQKTUART1Reg *)(port->membase);
	/*
	 * Free the interrupt
	 */
	free_irq(port->irq, info);

	/*
	 * disable all interrupts, disable the port
	 */
	
	pUART->cntrl2 = 0;
	//*****For debugging purposes, don't shut down UART1 yet
	//pUART->cntrl0 = 0;

	/* disable break condition and fifos */
#endif
}

static void katana_uart_change_speed(struct uart_port *port, u_int cflag, u_int iflag, u_int quot)
{
	MQKTUART1Reg	*pUART = (MQKTUART1Reg *)(port->membase);
	unsigned long cntrl2 = 0;
	unsigned long  flags;

#if DEBUG
	printk("katana_uart_set_cflag(0x%x) called\n", cflag);
#endif
	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5: cntrl2 |= 0x6; break;
	case CS6: cntrl2 |= 0x4; break;
	case CS7: cntrl2 |= 0x2; break;
	default:  cntrl2 |= 0x0; break; // CS8
	}
	if (cflag & CSTOPB)
		cntrl2 |= (1 << 3);	//2 stop bits set
	if (cflag & PARENB) {
		cntrl2 |= (1 << 4);	//parity enabled
		if (!(cflag & PARODD))
			cntrl2 |= (1 << 5);	//set even parity
	}
	if (port->fifosize > 1)
		/* do what here? */ ;

	port->read_status_mask = (1 << 2);	//want overrun err
	if (iflag & INPCK)
		port->read_status_mask |= (1 << 4) | (1 << 3);
						//want frame and parity err
	if (iflag & (BRKINT | PARMRK))
		port->read_status_mask |= (1 << 5) | (1 << 6);
						//want break and break int?

	/*
	 * Characters to ignore
	 */
	port->ignore_status_mask = 0;
	if (iflag & IGNPAR)
		port->ignore_status_mask |= (1 << 4) | (1 << 3);
						//ignore frame and parity err
	if (iflag & IGNBRK) {
		port->ignore_status_mask |= (1 << 5); //ignore break
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (iflag & IGNPAR)
			port->ignore_status_mask |= (1 << 2); // overrun
	}
/* printk("k_cp1:%d,%o,%o,%x,%x\n",
	port->fifosize,
	cflag, iflag,
	port->ignore_status_mask, port->read_status_mask);
printk("k_cp1-2:%d,%lx\n",quot,cntrl2);
*/

	/*
	 * Ignore all characters if CREAD is not set.
	 */
	//HSU: need to handle this case where CREAD is not set
	//if ((cflag & CREAD) == 0)
	//	port->ignore_status_mask |= UART_DUMMY_RSR_RX;

	/* first, disable everything */
	save_flags(flags); cli();

	pUART->cntrl0 = 0;
#if 0
	//no modem stuff
	old_cr = UART_GET_CR(port) & ~AMBA_UARTCR_MSIE;

	if ((port->flags & ASYNC_HARDPPS_CD) ||
	    (cflag & CRTSCTS) || !(cflag & CLOCAL))
		old_cr |= AMBA_UARTCR_MSIE;

	UART_PUT_CR(port, 0);
#endif

	//See startup() on this cntrl2 setup
	cntrl2 |= (1 << 8)	//receive int only
//		| (16L << 12)		//rx fifo thres
		| (1L << 12)		//rx fifo thres
//		| (16L << 18)		//tx fifo thres
	#ifdef MQ_TX_ALL
		| (0L << 18)		//tx fifo thres immediate interrupt
	#else
		| (8L << 18)		//tx fifo thres
	#endif
		;
	pUART->cntrl2 = cntrl2;

	/* Set baud rate */
	quot -= 1;
	pUART->cntrl1 = quot;

	pUART->cntrl0 = 0xd;

	restore_flags(flags);
}

static const char *katana_uart_type(struct uart_port *port)
{
	return port->type == PORT_KATANA ? "SER_KATANA" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'
 */
static void katana_uart_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, UART_PORT_SIZE);
}

/*
 * Request the memory region(s) being used by 'port'
 */
static int katana_uart_request_port(struct uart_port *port)
{
	return request_mem_region(port->mapbase, UART_PORT_SIZE, "serial_katana")
			!= NULL ? 0 : -EBUSY;
}

/*
 * Configure/autoconfigure the port.
 */
static void katana_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_KATANA;
		katana_uart_request_port(port);
	}
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int katana_uart_verify_port(struct uart_port *port,
			struct serial_struct *ser)
{
	int ret = 0;
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_KATANA)
		ret = -EINVAL;
	if (ser->irq < 0 || ser->irq >= NR_IRQS)
		ret = -EINVAL;
	//We will support < 9600 for UART1
	if (ser->baud_base < 9600)
		ret = -EINVAL;
	return ret;
}

static struct uart_ops katana_pops = {
	tx_empty:	katana_uart_tx_empty,
	set_mctrl:	katana_uart_set_mctrl,
	get_mctrl:	katana_uart_get_mctrl,
	stop_tx:	katana_uart_stop_tx,
	start_tx:	katana_uart_start_tx,
	stop_rx:	katana_uart_stop_rx,
	enable_ms:	katana_uart_enable_ms,
	break_ctl:	katana_uart_break_ctl,
	startup:	katana_uart_startup,
	shutdown:	katana_uart_shutdown,
	change_speed:	katana_uart_change_speed,
	type:		katana_uart_type,
	release_port:	katana_uart_release_port,
	request_port:	katana_uart_request_port,
	config_port:	katana_uart_config_port,
	verify_port:	katana_uart_verify_port,
};

static struct uart_port katana_ports[UART_NR] = {
	{
		membase:	(void *)(MQ9000_REGS_VBASE + U1_OFFSET),
		mapbase:	MQ9000_UART1_BASE,
		iotype:		SERIAL_IO_MEM,
		irq:		IRQNO_UART1,
		//uartclk:	14745600,	//actually 133MHz for us
		uartclk:	139460608,	//actually 133MHz for us
		//fifosize:	16,
		fifosize:	0,
		//fifosize:	256,	//use big fifosize so that no timeout
		ops:		&katana_pops,
		flags:		ASYNC_BOOT_AUTOCONF,
	},
#ifdef USE_U2_U3
	{
		membase:	(void *)(MQ9000_REGS_VBASE + U2_OFFSET),
		mapbase:	MQ9000_UART2_BASE,
		iotype:		SERIAL_IO_MEM,
		irq:		IRQNO_UART2,
		//uartclk:	14745600,	//actually 133MHz for us
		//uartclk:	3686400,	//fake it
		uartclk:	139460608,	//actually 133MHz for us
		//fifosize:	0,
		fifosize:	16,
		ops:		&katana_pops2,
		flags:		ASYNC_BOOT_AUTOCONF,
	},
	{
		membase:	(void *)(MQ9000_REGS_VBASE + U3_OFFSET),
		mapbase:	MQ9000_UART3_BASE,
		iotype:		SERIAL_IO_MEM,
		irq:		IRQNO_UART3,
		//uartclk:	14745600,	//actually 133MHz for us
		//uartclk:	3686400,	//fake it
		uartclk:	139460608,	//actually 133MHz for us
		fifosize:	0,
		ops:		&katana_pops2,
		flags:		ASYNC_BOOT_AUTOCONF,
	}
#endif	//USE_U2_U3
};

#ifdef CONFIG_SERIAL_KATANA_CONSOLE
#ifdef used_and_not_const_char_pointer
static int katana_uart_console_read(struct uart_port *port,
			char *s, u_int count)
{
#if 1
	struct uart_port *port = katana_ports + co->index;
	unsigned long *pReg = (unsigned long *)(port->membase);
	unsigned long status;
	int c;

	printk("katana_uart_console_read() called\n");

	c = 0;
	while (c < count) {
		status = *(pReg + 4);
		if ( status & 0x1c )
			(void)*(pReg + 3);	//error, skip fifo entry
		else
		{
			status &= 0x1F000;
			if ( status )
			{
				*s++ = (unsigned char)(*(pReg + 3));
				c++;
			}
			else
				break;	//nothing left in fifo
		}
	}
	return c;	//Don't do any rx for now, return 0 count

#else
	unsigned int status;
	int c;
#if DEBUG
	printk("katana_uart_console_read() called\n");
#endif

	c = 0;
	while (c < count) {
		status = UART_GET_FR(port);
		if (UART_RX_DATA(status)) {
			*s++ = UART_GET_CHAR(port);
			c++;
		} else {
			// nothing more to get, return
			return c;
		}
	}
	// return the count
	return c;
#endif
}
#endif	//used_and_not_const_char_pointer

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
extern int kgdb_in;
#endif

static void katana_uart_console_write(struct console *co,
			const char *s, u_int count)
{
	struct uart_port *port = katana_ports + co->index;
	int i;

#ifdef CONFIG_KGDB

        int checksum;

    	/* This call only does a trap the first time it is
	 * called, and so is safe to do here unconditionally
	 */

	if (kgdb_in) {

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
#ifdef USE_U2_U3
	if ( port->mapbase != MQ9000_UART1_BASE )
	{
		MQKTUARTReg	*pUART;

		pUART = (MQKTUARTReg *)(port->membase);
		for (i = 0; i < count; i++) {
			while( !(pUART->inStat & MQ_TXFIFO_EMPTY) )
				barrier();
			//pUART->txBufSize = 1;
			pUART->txBufSize = 0xff;
			pUART->txData = s[i];
			if (s[i] == '\n')
			{
				while( !(pUART->inStat & MQ_TXFIFO_EMPTY) )
					barrier();
				//pUART->txBufSize = 1;
				pUART->txBufSize = 0xff;
				pUART->txData = '\r';
			}
		}
	}
	else
#endif	//USE_U2_U3
	{

		MQKTUART1Reg	*pUART1;

		pUART1 = (MQKTUART1Reg *)(port->membase);
		for (i = 0; i < count; i++) {
			wait_for_xmitr(port);
#if 0
			while( !(pUART1->status & 0x80) )
				barrier();
#endif
			pUART1->data = s[i];
			if (s[i] == '\n')
			{
				wait_for_xmitr(port);
#if 0
				while( !(pUART1->status & 0x80) )
					barrier();
#endif
				pUART1->data = '\r';
			}
		}
	}
	}
}

static kdev_t katana_uart_console_device(struct console *co)
{
//puts_hex2("katana_uart_console_device :", co->index, 1);
	return MKDEV(SERIAL_KATANA_MAJOR, SERIAL_KATANA_MINOR + co->index);
}

static void __init
katana_uart_console_get_options(struct uart_port *port, int *baud,
			int *parity, int *bits)
{
#ifdef USE_U2_U3
	if ( port->mapbase != MQ9000_UART1_BASE )
	{
		//must be UART2 and UART3
		MQKTUARTReg *pUART = (MQKTUARTReg *)(port->membase);
		if ( (pUART->txCntrl & MQ_TX_ENABLE)
			&& (pUART->rxCntrl & MQ_RX_ENABLE) )
		{
			//*baud = 115200;	//hardcode it for now
			*baud = 38400;	//hardcode it for now
			*parity = 'n';
			*bits = 8;
		}
	}
	else
#endif	//USE_U2_U3

	{
		MQKTUART1Reg *pUART1 = (MQKTUART1Reg *)(port->membase);
		if ( pUART1->cntrl0 & 0xd )
				//if xmit/receive and UART, then enabled
		{
			*parity = 'n';
			if ( pUART1->cntrl2 & (1<<4) )
			{
				if ( pUART1->cntrl2 & (1<<5) )
					*parity = 'e';
				else
					*parity = 'o';
			}
			switch ( pUART1->cntrl2 & 0x6 )
			{
				case 1: *bits = 7; break;
				case 2: *bits = 6; break;
				case 3: *bits = 5; break;
				default:
				case 0: *bits = 8; break;
			}
			*baud = port->uartclk /
				(16 * ((pUART1->cntrl1 & 0x3FFFF) + 1));
		}
	}
}

#if 0
static int katana_uart_console_wait_key(struct console *co)
{
	struct uart_port *port = katana_ports + co->index;
	MQKTUART1Reg	*pUART = (MQKTUART1Reg *)(port->membase);
	unsigned long flags;
	unsigned long cntrl2, status;
	unsigned int  ch;

	/*
	 * Save UTCR3 and disable interrupts
	 */
	save_flags(flags);
	cli();
	cntrl2 = pUART->cntrl2;

	pUART->cntrl2 = cntrl2 & ~((1 << 8) | (1 << 9));
		//clr tx and rx fifo thrs int
	restore_flags(flags);

	/*
	 * Wait for a character
	 */
	do {
		status = pUART->status;
	} while (!(status & 0x3F000));	//check if rx fifo has data
	ch = (unsigned char)(pUART->data);

	/*
	 * Restore UTCR3
	 */
	pUART->cntrl2 = cntrl2;

	return ch;
}
#endif	//0

static int __init katana_uart_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 38400;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	port = uart_get_console(katana_ports, UART_NR, co);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		katana_uart_console_get_options(port, &baud, &parity, &bits);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console katana_console = {
#ifdef USE_SA1100_SER
	name:		"ttySA",
#else
	name:		"ttyKT",
#endif
	write:		katana_uart_console_write,
#ifdef used_and_not_const_char_pointer
	read:		katana_uart_console_read,
#endif
	device:		katana_uart_console_device,
#if 0	//ndef USE_U2_U3
	wait_key:	katana_uart_console_wait_key,
#endif
	setup:		katana_uart_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init katana_uart_console_init(void)
{
	register_console(&katana_console);
}

#define KATANA_CONSOLE	&katana_console
#else	//CONFIG_SERIAL_KATANA_CONSOLE
#define KATANA_CONSOLE	NULL
#endif	//CONFIG_SERIAL_KATANA_CONSOLE

static struct uart_driver katana_reg = {
	owner:			THIS_MODULE,
	normal_major:		SERIAL_KATANA_MAJOR,
#ifdef USE_SA1100_SER
#ifdef CONFIG_DEVFS_FS
	normal_name:		"ttySA%d",
	callout_name:		"cusa%d",
#else
	normal_name:		"ttySA",
	callout_name:		"cusa",
#endif
#else	//USE_SA1100_SER
#ifdef CONFIG_DEVFS_FS
	normal_name:		"ttyKT%d",
	callout_name:		"cukt%d",
#else
	normal_name:		"ttyKT",
	callout_name:		"cukt",
#endif
#endif	//USE_SA1100_SER
	normal_driver:		&normal,
	callout_major:		CALLOUT_KATANA_MAJOR,
	callout_driver:		&callout,
	table:			katana_table,
	termios:		katana_termios,
	termios_locked:		katana_termios_locked,
	minor:			SERIAL_KATANA_MINOR,
	nr:			UART_NR,
	port:			katana_ports,
	cons:			KATANA_CONSOLE,
};

static int __init katana_uart_init(void)
{
	return uart_register_driver(&katana_reg);
}

static void __exit katana_uart_exit(void)
{
	uart_unregister_driver(&katana_reg);
}

#ifdef CONFIG_KGDB

/* 
 *  Takes:
 *	ttyS - integer specifying which serial port to use for debugging
 *	baud - baud rate of specified serial port
 *  Returns:
 *	port for use by the gdb serial driver
 */
//struct serial_state *
struct uart_port *
gdb_serial_setup(int ttyS, int baud)
{
	struct uart_port *port;
        struct serial_state *ser;
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

	port = katana_ports + ttyS;
	quot = (port->uartclk / (16 * baud));
	port->ops->change_speed(port, cflag, 0, quot);
	return port;
}

#endif /* CONFIG_KGDB */

module_init(katana_uart_init);
module_exit(katana_uart_exit);

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("MediaQ, Inc.");
MODULE_DESCRIPTION("KATANA (MQ9000) serial port driver");
MODULE_LICENSE("GPL");
