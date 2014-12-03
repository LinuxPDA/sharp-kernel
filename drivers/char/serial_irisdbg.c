/*
 *  linux/drivers/char/serial_irisdbg.c
 *
 *  Driver for Debug serial ports on the SHARP Iris-1 board
 *
 *  Based on:
 *    drivers/char/serial_linkup.c
 *
 *  ChangeLog:
 *    03-06-2001 Lineo Japan, Inc. Changed for Emebdix.
 *    03-16-2001                   Updated with SHARP's patch.
 *
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
#include <linux/serial_reg.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>

#include <linux/pm.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/arch/gpio.h>
/* #include <asm/arch/iris_fpga.h> */

#ifdef CONFIG_PM
static struct pm_dev* iris_dbgrs_pm_dev;
#endif

/* #define BAUD_BASE 115200 */
#define SERIAL_LINKUP_MINOR_START 64

static struct tty_driver rs_driver, callout_driver;
static int rs_refcount;
static struct tty_struct *rs_table[1];

static struct termios *rs_termios[1];
static struct termios *rs_termios_locked[1];

#define DEBUGPRINT(s)  /* printk s */

/* --------------------------------------------------
 *       configuration ... needs to be moved
 * -------------------------------------------------- */

#define DBGSERIAL_IRIS_MINOR_START (SERIAL_LINKUP_MINOR_START+4)
#define DBGSERIAL_IRIS_DRIVER_NUM  1

/* --------------------------------------------------
 *         definitions for internal use
 * -------------------------------------------------- */

typedef struct tl16c750regs {
	volatile __u8 rbr_thr;
	__u8 __unused1[3];
	volatile __u8 ier;
	__u8 __unused2[3];
	volatile __u8 iir_fcr;
	__u8 __unused3[3];
	volatile __u8 lcr;
	__u8 __unused4[3];
	volatile __u8 mcr;
	__u8 __unused5[3];
	volatile __u8 lsr; 
	__u8 __unused6[3];
	volatile __u8 msr;
	__u8 __unused7[3];
	volatile __u8 scr;
	__u8 __unused8[3];
} tl16c750regs;

static tl16c750regs *dbg_port_p =
        ((tl16c750regs*)DEBUG_UART_BASE);

#define WBUF_SIZE 1000

typedef struct iris_dbgserial_driver_struct { 
	struct tty_struct *tty;
	int flags;
	struct wait_queue *delta_msr_wait;
	struct async_icount icount;
	char wbuf[WBUF_SIZE];
	char *putp;
	char *getp;
	int x_char;
	int use_count;
	unsigned char ier;
} iris_dbgserial_driver_struct;

static iris_dbgserial_driver_struct uports[DBGSERIAL_IRIS_DRIVER_NUM];

#define INTMASK_RX    0x01
#define INTMASK_TX    0x02
#define INTMASK_LINE  0x04
#define INTMASK_MS    0x08

#define SLEEP_MODE    0x10

#define IIR_INTMASK  0x07
#define IIR_TX       0x02
#define IIR_RX       0x04
#define IIR_RXERR    0x0c
#define IIR_MS       0x00
#define IIR_LINE     0x06

#define MSR_DELTA_CTS	0x01
#define MSR_DELTA_DSR   0x02
#define MSR_TERI        0x04
#define MSR_DELTA_DCD   0x08
#define MSR_CTS         0x10
#define MSR_DSR         0x20
#define MSR_RI          0x40
#define MSR_DCD         0x80

#define MCR_DTR      0x01
#define MCR_RTS      0x02
#define MCR_ACE      0x20

#define LSR_DATARDY   0x01
#define LSR_OVERRUN   0x02
#define LSR_PARITY    0x04
#define LSR_FRAME     0x08
#define LSR_FIFOEMPTY 0x20
#define LSR_TXEMPTY   0x40

#define FCR_FIFOENABLE           0x01
#define FCR_RX_RESET             0x02
#define FCR_TX_RESET             0x04
#define FCR_FIFO64_ENABLE        0x20
#define FCR_RECEIVER_TRIGGER_1   0x00
#define FCR_RECEIVER_TRIGGER_4   0x40
#define FCR_RECEIVER_TRIGGER_8   0x80
#define FCR_RECEIVER_TRIGGER_14  0xc0

#define LCR_BITS5    0x00
#define LCR_BITS6    0x01
#define LCR_BITS7    0x02
#define LCR_BITS8    0x03
#define LCR_STOPBIT  0x04
#define LCR_PARIEN   0x08
#define LCR_PARIEVEN 0x10
#define LCR_DIVISOR_LATCH 0x80

/* these values are written for 3.6MHz Crystal... */
#define DIVLATCH_MSB 0x00
#define DIVLATCH_LSB_115200   0x02
#define DIVLATCH_LSB_57600    0x04
#define DIVLATCH_LSB_38400    0x06
#define DIVLATCH_LSB_19200    0x0c
#define DIVLATCH_LSB_9600     0x18
#define DIVLATCH_LSB_7200     0x20
#define DIVLATCH_LSB_4800     0x30
#define DIVLATCH_LSB_2400     0x60
#define DIVLATCH_LSB_1200     0xc0

/* --------------------------------------------------
 *               for internal use
 * -------------------------------------------------- */

static void iris_enable_dbguart_irq(iris_dbgserial_driver_struct* info,
				    unsigned int mask)
{
	unsigned long flags;

	if( ! info ) return;
	DEBUGPRINT(("Enable DebugUart IRQ (%x) (cur=%x)\n",
		    mask,dbg_port_p->ier));
	save_flags(flags); cli();
	info->ier |= mask;
	dbg_port_p->ier = info->ier;
	restore_flags(flags);
	if ( !(IO_IRQENABLE & (1<<IRQ_INT2)) )  /* not yet enabled */
		enable_irq(IRQ_INT2);
	//DEBUGPRINT(("Current IRQ: UART %x SYS %lx\n",dbg_port_p->ier,IO_IRQENABLE));
}

static void iris_disable_dbguart_irq(iris_dbgserial_driver_struct* info,
				     unsigned int mask)
{
	unsigned long flags;

	if( ! info ) return;
	DEBUGPRINT(("Disabling DebugUart IRQ (%x)\n",mask));
	save_flags(flags); cli();
	info->ier &= ~mask;
	dbg_port_p->ier = info->ier;
	restore_flags(flags);
	if ( ! info->ier )   /* Both RX/TX unmasked */
		disable_irq(IRQ_INT2);  /* disable the whole UART IRQ */
	//DEBUGPRINT(("Current IRQ: UART %x SYS %lx\n",dbg_port_p->ier,IO_IRQENABLE));
}

/* --------------------------------------------------
 *                  modem status
 * -------------------------------------------------- */

static __inline__ void check_modem_status(iris_dbgserial_driver_struct* info)
 /* completed ? */
{
	unsigned char mstat;
	struct async_icount *icount;

	//DEBUGPRINT(("iris check modem status\n"));

	if( ! info ) return;

	mstat = dbg_port_p->msr;

	icount = &(info->icount);

	if( mstat & (MSR_DELTA_CTS|MSR_DELTA_DSR|MSR_DELTA_DCD) ){
		if( ( mstat & MSR_DSR ) ) icount->dsr++;
		if( ( mstat & MSR_CTS ) ) icount->cts++;
		if( ( mstat & MSR_DCD ) ) icount->dcd++;
		if( ( mstat & MSR_RI  ) ) icount->rng++;
		wake_up_interruptible((wait_queue_head_t*)&info->delta_msr_wait);
	}

	if( (info->flags & ASYNC_CHECK_CD) ){
		if( ! (mstat & MSR_DCD) ){
			if( info->tty ) tty_hangup(info->tty);
		}
	}

	if( (info->flags & ASYNC_CTS_FLOW) ){
		if( info->tty ){
			if( info->tty->hw_stopped ){
				if( ( mstat & MSR_CTS) ){
					info->tty->hw_stopped = 0;
				/* may be need to re_sched , WRITE_WAKEUP */
				}
			}
			else{
				if( ! (mstat & MSR_CTS) ){
					info->tty->hw_stopped = 1;
				}
			}
		}
	}
}

/* --------------------------------------------------
 *                 startup / shutdown
 * -------------------------------------------------- */

static inline void rs_set_cflag(iris_dbgserial_driver_struct* info,int cflag);

static int startup(iris_dbgserial_driver_struct* info)
{
	unsigned long flags;
	volatile unsigned char dummy;

	DEBUGPRINT(("iris startup\n"));

	if( ! info ) return(0);

	/* wake up UART */
	dbg_port_p->ier = 0;
	/* clear FIFO */
	dbg_port_p->iir_fcr = (FCR_FIFOENABLE | FCR_RX_RESET | FCR_TX_RESET );
	dbg_port_p->iir_fcr = 0;
	/* clear FIFO */
	dbg_port_p->iir_fcr = FCR_FIFOENABLE;
	/* clear interrupt */
	dummy = dbg_port_p->rbr_thr;
	dummy = dbg_port_p->iir_fcr;
	dummy = dbg_port_p->msr;
	/* init UART */
	dbg_port_p->lcr = LCR_BITS8;

	if( info->tty->termios->c_cflag & CBAUD ){
		save_flags(flags); cli();
		dbg_port_p->mcr |= ( MCR_DTR | MCR_RTS | MCR_ACE  );
		restore_flags(flags);
	}

	/* rs_set_cflag(info, CS8 | B9600 ); */

	return(0);
}

static void shutdown(iris_dbgserial_driver_struct* info)
{
	unsigned long flags;

	DEBUGPRINT(("iris shutdown\n"));

	if( ! info ) return;

	wake_up_interruptible((wait_queue_head_t*)&info->delta_msr_wait);

	if( ! info->tty || ( info->tty->termios->c_cflag & HUPCL ) ){
		save_flags(flags); cli();
		dbg_port_p->mcr &= ~( MCR_DTR | MCR_RTS | MCR_ACE  );
		restore_flags(flags);
	}
}

/* --------------------------------------------------
 *                  handlers
 * -------------------------------------------------- */


static int rs_write_room(struct tty_struct *tty)
{ 
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris write room\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	return( ( info->putp >= info->getp ) ?
		( WBUF_SIZE - (long)(info->putp) + (long)(info->getp) )
		: ( (long)(info->getp) - (long)(info->putp) -1 ) );
}

static void rs_send_xchar(struct tty_struct *tty, char ch)
{
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris send xchar\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	info->x_char = ch;
	iris_enable_dbguart_irq(info,INTMASK_TX);
}

static void rs_throttle(struct tty_struct *tty)
{
	int line;
	iris_dbgserial_driver_struct* info;
	unsigned long flags;

	DEBUGPRINT(("iris throttle\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	if(I_IXOFF(tty))
		rs_send_xchar(tty,STOP_CHAR(tty));
	if (tty->termios->c_cflag & CRTSCTS){
		save_flags(flags); cli();
		dbg_port_p->mcr &= ~(MCR_RTS);
		restore_flags(flags);
	}
}

static void rs_unthrottle(struct tty_struct *tty)
{
	int line;
	iris_dbgserial_driver_struct* info;
	unsigned long flags;

	DEBUGPRINT(("iris unthrottle\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	if(I_IXOFF(tty)) {
		if(info->x_char)
			info->x_char=0;
		else
			rs_send_xchar(tty,START_CHAR(tty));
	}
	if (tty->termios->c_cflag & CRTSCTS){
 		save_flags(flags); cli();
		dbg_port_p->mcr |= MCR_RTS;
		restore_flags(flags);
	}
}

static inline int rs_xmit(iris_dbgserial_driver_struct* info,int ch)
{
	unsigned long flags;

	DEBUGPRINT(("iris xmit\n"));

	if( ! info ) return(0);
	if( (info->putp+1) == info->getp ||
	    ( ( info->putp+1 == (info->wbuf + WBUF_SIZE) )
	      && ( info->getp == info->wbuf ) ) )
		return 0;

	save_flags(flags); cli();
	*info->putp = ch;
	if( ++(info->putp) >= ( info->wbuf + WBUF_SIZE ) )
	  info->putp = info->wbuf; 
	restore_flags(flags);
	/* iris_enable_dbguart_irq(info,INTMASK_TX); */
  
	return 1;
}

static int rs_write(struct tty_struct *tty,int from_user,
		    const u_char *buf,int count)
{  
	int line;
	int i;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris write\n"));

	if( ! tty ) return 0;

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	if(from_user) {
		if(verify_area(VERIFY_READ, buf, count))
			return -EINVAL;
	}
	for(i=0;i<count;i++) {
		char ch;
		if(from_user)
			get_user(ch,buf+i);
		else
			ch = buf[i];
		if(!rs_xmit(info,ch))
		  break;
	}
	iris_enable_dbguart_irq(info,INTMASK_TX);
	return i;
} 

static void rs_put_char(struct tty_struct *tty,u_char ch)
{
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris put char\n"));

	if( ! tty ) return;

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];
	rs_xmit(info,ch);
	iris_enable_dbguart_irq(info,INTMASK_TX);
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	DEBUGPRINT(("iris chars in buffer\n"));
	return( WBUF_SIZE - rs_write_room(tty) );
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	unsigned long flags;
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris flush buffer\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	iris_disable_dbguart_irq(info,INTMASK_TX);

	save_flags(flags); cli();
	info->putp = info->getp = info->wbuf;
	restore_flags(flags);

	//wake_up_interruptible(&tty->write_wait);

	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP))
	    && tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);

	if(info->x_char)
		iris_enable_dbguart_irq(info,INTMASK_TX);
}


static void rs_flush_chars(struct tty_struct *tty)
{
	//unsigned long flags;
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris flush chars\n"));

	if( ! tty || tty->stopped || tty->hw_stopped ) return;

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	//save_flags(flags); cli();
	iris_enable_dbguart_irq(info,INTMASK_TX);
	//restore_flags(flags);
}

static void rs_wait_until_sent(struct tty_struct *tty,int timeout)
{
	int orig_jiffies = jiffies;
	unsigned char flgstat;
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris wait until sent\n"));

	if( ! tty ) return;

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	while( ! ( ( flgstat = dbg_port_p->lsr ) & LSR_FIFOEMPTY )  ){
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
		if( signal_pending(current) )
			break;
		if( timeout && ((orig_jiffies + timeout)<jiffies) )
			break;
	}
	current->state=TASK_RUNNING;
}

static void rs_hangup(struct tty_struct *tty)
{
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris hangup\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	rs_flush_buffer(tty);
	shutdown(info);
}

/* --------------------------------------------------
 *               interrupt handlers
 * -------------------------------------------------- */

static __inline__ void receive_chars(iris_dbgserial_driver_struct* info)
{
	unsigned char flgstat;
	struct async_icount *icount;

	DEBUGPRINT(("iris receive chars\n"));

	if( ! info || ! info->tty ) return;

	icount = &(info->icount);

	while( ( flgstat = dbg_port_p->lsr ) & LSR_DATARDY ){
		char flag = 0;
		unsigned char ch;
		unsigned char st;

		ch = dbg_port_p->rbr_thr;
		st = dbg_port_p->lsr;

		icount->rx++;
		if (st & LSR_OVERRUN)
			tty_insert_flip_char(info->tty,0,TTY_OVERRUN);
		if (st & LSR_PARITY){
			flag = TTY_PARITY;
		}else if (st & LSR_FRAME){
			flag = TTY_FRAME;
		}
		tty_insert_flip_char(info->tty,ch & 0xff,flag);
	}
	iris_enable_dbguart_irq(info,INTMASK_RX);
	tty_flip_buffer_push(info->tty);
}

static __inline__ void transmit_chars(iris_dbgserial_driver_struct* info)
{
	/* unsigned char flgstat; */
	struct async_icount *icount;

	DEBUGPRINT(("iris transmit chars\n"));

	//printk("transmit\n");

	if( ! info ) return;

	icount = &(info->icount);

	if( info->x_char ) {
		dbg_port_p->rbr_thr = info->x_char;
		info->x_char = 0;
		icount->tx++;
	}else if( info->tty->stopped || info->tty->hw_stopped ){
		iris_disable_dbguart_irq(info,INTMASK_TX);
	}else if( info->putp == info->getp ){
		iris_disable_dbguart_irq(info,INTMASK_TX);
	}else{
		dbg_port_p->rbr_thr = *(info->getp);
		icount->tx++;
		if( ++info->getp >= (info->wbuf+WBUF_SIZE) )
			info->getp = info->wbuf;
	}

	if( info->tty )
		wake_up_interruptible(&(info->tty->write_wait));
}

static void rs_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char intstat;
	unsigned char lsrstat;
	/* unsigned long flags; */
	iris_dbgserial_driver_struct* info = NULL;

	DEBUGPRINT(("iris int\n"));

	info = &uports[0];
	if( ! info->tty ){
		//DEBUGPRINT(("no tty\n"));
		iris_disable_dbguart_irq(info,
					 INTMASK_RX|INTMASK_MS|INTMASK_LINE);
	}else{
		intstat = dbg_port_p->iir_fcr & IIR_INTMASK;
		lsrstat = dbg_port_p->lsr;
		if( intstat == IIR_RX || intstat == IIR_RXERR ){
			receive_chars(info);
		}else if( intstat == IIR_MS ){
			check_modem_status(info);
		}else if( intstat == IIR_TX ){
			transmit_chars(info);
		}
#if 0
		if( lsrstat & LSR_DATARDY )
			receive_chars(info);
		check_modem_status(info);
		if( lsrstat & LSR_TXEMPTY )
			transmit_chars(info);
#endif
	}
}

/* --------------------------------------------------
 *             start / stop
 * -------------------------------------------------- */

static void rs_stop(struct tty_struct *tty) /* completed ? */
{
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris stop\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];
	iris_disable_dbguart_irq(info,INTMASK_TX);
}

static void rs_start(struct tty_struct *tty) /* completed ? */
{
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris start\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];
	iris_enable_dbguart_irq(info,INTMASK_TX);
}

/* --------------------------------------------------
 *             open / close
 * -------------------------------------------------- */

static int rs_open(struct tty_struct *tty, struct file *filp)
{
	int line;
	int retval;
	iris_dbgserial_driver_struct* info;
	volatile unsigned char dummy;

	line = MINOR(tty->device) - tty->driver.minor_start;

	DEBUGPRINT(("iris dbgopen %d\n",line));

	if( ( line < 0 ) || ( line >= DBGSERIAL_IRIS_DRIVER_NUM ) ){
		printk("dbgrs_open: No such device! minor=%d\n", MINOR(tty->device));
		return -ENODEV;
	}

	info = &(uports[line]);
	tty->driver_data = NULL;
	info->tty = tty;
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	retval = startup(info);
	if( retval ) return retval;

	/* enable FIFO ... */
	dbg_port_p->iir_fcr |= FCR_FIFOENABLE;

	iris_enable_dbguart_irq(info,INTMASK_RX|INTMASK_MS|INTMASK_LINE);

	/* clear interrupt again for luck */
	dummy = dbg_port_p->lsr;
	dummy = dbg_port_p->rbr_thr;
	dummy = dbg_port_p->iir_fcr;
	dummy = dbg_port_p->msr;

	info->use_count++;
	return 0;
}

static void rs_close(struct tty_struct *tty, struct file *filp)
{ 
	int line;
	//unsigned long flags;
	iris_dbgserial_driver_struct* info;

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	DEBUGPRINT(("iris close %d\n",line));

	//save_flags(flags); cli();
	tty->closing = 1;
	if( ! --info->use_count ){
		rs_wait_until_sent(tty,0);
		iris_disable_dbguart_irq(info,INTMASK_RX|INTMASK_MS|INTMASK_LINE|INTMASK_TX);
		shutdown(info);
		if (tty->driver.flush_buffer){
			tty->driver.flush_buffer(tty);
		}
		if (tty->ldisc.flush_buffer){
			tty->ldisc.flush_buffer(tty);
		}
		dbg_port_p->iir_fcr &= ~(FCR_FIFOENABLE);
	}
	tty->closing = 0;
	//restore_flags(flags);
}

/* --------------------------------------------------
 *                    termios
 * -------------------------------------------------- */

static inline void rs_set_cflag(iris_dbgserial_driver_struct* info,int cflag)
     /* completed ? */
{
	unsigned char lcr = 0;
	unsigned char lsb = 0;

	switch(cflag & CSIZE) {
	case CS5:
		lcr |= LCR_BITS5;
		break;
	case CS6:
		lcr |= LCR_BITS6;
		break;
	case CS7:
		lcr |= LCR_BITS7;
		break;
	case CS8:
	default:
		lcr |= LCR_BITS8;
		break;
	}

	if( cflag & CSTOPB ) lcr |= LCR_STOPBIT;
	if( cflag & PARENB ) lcr |= LCR_PARIEN;
	if( ! ( cflag & PARODD ) ) lcr |= LCR_PARIEVEN;

	switch(cflag & CBAUD) {
	case B1200:
		lsb |= DIVLATCH_LSB_1200;
		break;
	case B2400: 
		lsb |= DIVLATCH_LSB_2400;
		break;
	case B4800: 
		lsb |= DIVLATCH_LSB_4800;
		break;
	case B19200:
		lsb |= DIVLATCH_LSB_19200;
		break;
	case B38400:
		lsb |= DIVLATCH_LSB_38400;
		break;
	case B57600:
		lsb |= DIVLATCH_LSB_57600;
		break;
	case B115200:
		lsb |= DIVLATCH_LSB_115200;
		break;
	case B9600:
	default:
		lsb |= DIVLATCH_LSB_9600;
	}

	dbg_port_p->lcr = lcr;
	dbg_port_p->lcr |= LCR_DIVISOR_LATCH;

	//DEBUGPRINT(("Entering Divisor-Latch lcr=%x\n",dbg_port_p->lcr));

	dbg_port_p->rbr_thr = lsb;
	dbg_port_p->ier = DIVLATCH_MSB;

	//DEBUGPRINT(("Divisor-Latch msb=%x lsb=%x\n",dbg_port_p->ier,dbg_port_p->rbr_thr));

	dbg_port_p->lcr &= ~(LCR_DIVISOR_LATCH);

	//DEBUGPRINT(("Leaving Divisor-Latch lcr=%x\n",dbg_port_p->lcr));
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old)
{
	unsigned long flags;
	unsigned int cflag;
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris termios\n"));

	if( ! tty ) return;

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	if( old && ( tty->termios->c_cflag == old->c_cflag ) )
		return;
	/* change speed */
	cflag = tty->termios->c_cflag;
	rs_set_cflag(info,cflag);
	if( cflag & CRTSCTS ){
		info->flags |= ASYNC_CTS_FLOW;
	}else{
		info->flags &= ~ASYNC_CTS_FLOW;
	}

	/* Handle transition to B0 status */
	if( (old->c_cflag & CBAUD) && ! (tty->termios->c_cflag & CBAUD) ){
		save_flags(flags); cli();
		dbg_port_p->mcr &= ~( MCR_DTR | MCR_RTS | MCR_ACE );
		restore_flags(flags);
	}
	/* Handle transition away from B0 status */
	if ( ! (old->c_cflag & CBAUD) && (tty->termios->c_cflag & CBAUD) ){
		save_flags(flags); cli();
		dbg_port_p->mcr |= (MCR_DTR | MCR_ACE );
		restore_flags(flags);
		if( ! (tty->termios->c_cflag & CRTSCTS)
			|| ! test_bit(TTY_THROTTLED, &tty->flags)) {
			save_flags(flags); cli();
			dbg_port_p->mcr |= MCR_RTS;
			restore_flags(flags);
		}
	}
	/* Handle turning off CRTSCTS */
	if( (old->c_cflag & CRTSCTS) && ! (tty->termios->c_cflag & CRTSCTS)){
		tty->hw_stopped = 0;
		rs_start(tty);
	}
}

/* --------------------------------------------------
 *              ioctl functions
 * -------------------------------------------------- */

static int get_modem_info(iris_dbgserial_driver_struct* info,unsigned int* value) /* completed ? */
{
	unsigned int result = 0;
	unsigned char msr;
	unsigned char mcr;

	DEBUGPRINT(("iris getmodeminfo\n"));

	msr = dbg_port_p->msr;
	mcr = dbg_port_p->mcr;

	result = 
	  (( mcr & MCR_RTS ) ? TIOCM_RTS : 0)
	  | (( mcr & MCR_DTR ) ? TIOCM_DTR : 0)
	  | (( msr & MSR_CTS ) ? TIOCM_CTS : 0)
	  | (( msr & MSR_DSR ) ? TIOCM_DSR : 0)
	  | (( msr & MSR_DCD ) ? TIOCM_CAR : 0)
	  | (( msr & MSR_RI ) ? TIOCM_RNG : 0);

	return put_user(result,value);
}

static int set_modem_info(iris_dbgserial_driver_struct* info, unsigned int cmd,
			  unsigned int *value)  /* completed ? */
{
	int error;
	unsigned int arg;
	unsigned long flags;

	DEBUGPRINT(("iris setmodeminfo\n"));

	error = get_user(arg,value);
	if( error ) return error;

	switch(cmd){
	case TIOCMBIS:
		if(arg & TIOCM_RTS){
			save_flags(flags); cli();
			dbg_port_p->mcr |= MCR_RTS;
			restore_flags(flags);
		}
		if(arg & TIOCM_DTR){
			save_flags(flags); cli();
			dbg_port_p->mcr |= MCR_DTR;
			restore_flags(flags);
		}
		break;
	case TIOCMBIC:
		if(arg & TIOCM_RTS){
			save_flags(flags); cli();
			dbg_port_p->mcr &= ~(MCR_RTS);
			restore_flags(flags);
		}
		if(arg & TIOCM_DTR){
			save_flags(flags); cli();
			dbg_port_p->mcr &= ~(MCR_DTR);
			restore_flags(flags);
		}
		break;
	case TIOCMSET:
		if( arg & TIOCM_RTS ){
			save_flags(flags); cli();
			dbg_port_p->mcr |= MCR_RTS;
			restore_flags(flags);
		}else{
			save_flags(flags); cli();
			dbg_port_p->mcr &= ~(MCR_RTS);
			restore_flags(flags);
		}
		if( arg & TIOCM_DTR ){
			save_flags(flags); cli();
			dbg_port_p->mcr |= MCR_DTR;
			restore_flags(flags);
		}else{ 
			save_flags(flags); cli();
			dbg_port_p->mcr &= ~(MCR_DTR);
			restore_flags(flags);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg) /* completed ? */
{
	int error;
	unsigned long flags;
	struct async_icount cprev, cnow;  /* kernel counter temps */
	struct serial_icounter_struct *p_cuser; /* user space */
	int line;
	iris_dbgserial_driver_struct* info;

	DEBUGPRINT(("iris ioctl\n"));

	line = MINOR(tty->device) - tty->driver.minor_start;
	info = &uports[line];

	switch(cmd){
	case TIOCMGET:
		return get_modem_info(info,(unsigned int *)arg);
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMSET:
		return set_modem_info(info,cmd,(unsigned int *)arg);
	case TIOCMIWAIT:
		save_flags(flags); cli();
		cprev = info->icount;
		restore_flags(flags);
		while(1){
			interruptible_sleep_on((wait_queue_head_t*)&info->delta_msr_wait);
			if (signal_pending(current)) return -ERESTARTSYS;
			save_flags(flags); cli();
			cnow = info->icount;
			restore_flags(flags);
			if( cnow.rng == cprev.rng && cnow.dsr == cprev.dsr && 
			    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts )
			  return -EIO; /* no change => error */
			if( ((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
			    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
			    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
			    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ){
				return 0;
			}
			cprev = cnow;
		}
		/* NOTREACHED */
		break;
	case TIOCGICOUNT:
		//save_flags(flags); cli();
	  	cnow = info->icount;
		//restore_flags(flags);
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
		return 0;
		break;
	default:
	  	return -ENOIOCTLCMD;
	}
	return 0;
}


#ifdef CONFIG_PM
static int iris_dbgrs_pm_callback(struct pm_dev *pm_dev,
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
/* --------------------------------------------------
 *             initialize function
 * -------------------------------------------------- */

/*__initfunc(int rs_irisdbg_init(void)) */

int __init rs_irisdbg_init(void)
{ 
	//unsigned long flags;

	/* ================ setup all ports ============= */
	{
		int i;
		iris_dbgserial_driver_struct* info;
		for(i=0;i<DBGSERIAL_IRIS_DRIVER_NUM;i++){
			info = &(uports[i]);
			info->putp = info->wbuf;
			info->getp = info->wbuf;
			rs_set_cflag(info, CS8 | B9600 );
		}
	}
	/* ================= end ==================== */

	memset(&rs_driver,0,sizeof(rs_driver));
	rs_driver.magic = TTY_DRIVER_MAGIC;
	rs_driver.driver_name = "serial_debug7200";
	rs_driver.name = "ttySD"; 
	rs_driver.major = TTY_MAJOR;
	rs_driver.minor_start = DBGSERIAL_IRIS_MINOR_START;
	rs_driver.num = DBGSERIAL_IRIS_DRIVER_NUM;
	rs_driver.type = TTY_DRIVER_TYPE_SERIAL;
	rs_driver.subtype = SERIAL_TYPE_NORMAL;
	rs_driver.init_termios = tty_std_termios;
	rs_driver.init_termios.c_cflag = B9600|CS8|CREAD|HUPCL|CLOCAL;
	rs_driver.flags = TTY_DRIVER_REAL_RAW;
	rs_driver.refcount = &rs_refcount; 
	rs_driver.table = rs_table;
	rs_driver.termios = rs_termios;
	rs_driver.termios_locked = rs_termios_locked;

	rs_driver.open = rs_open;
	rs_driver.close = rs_close;
	rs_driver.write = rs_write;
	rs_driver.put_char = rs_put_char;
	rs_driver.flush_chars = rs_flush_chars;
	rs_driver.write_room = rs_write_room;
	rs_driver.chars_in_buffer = rs_chars_in_buffer;
	rs_driver.flush_buffer = rs_flush_buffer;
	rs_driver.ioctl = rs_ioctl;
	rs_driver.throttle = rs_throttle;
	rs_driver.unthrottle = rs_unthrottle;
	rs_driver.send_xchar = rs_send_xchar;
       	rs_driver.set_termios = rs_set_termios;
	rs_driver.stop = rs_stop;
	rs_driver.start = rs_start;
	rs_driver.hangup = rs_hangup;
	rs_driver.break_ctl = NULL;
	rs_driver.wait_until_sent = rs_wait_until_sent; 
	rs_driver.read_proc = NULL;

	callout_driver  =  rs_driver;
	callout_driver.name  =  "cuaSD";
	callout_driver.major  =  TTYAUX_MAJOR;
	callout_driver.subtype  =  SERIAL_TYPE_CALLOUT;
	callout_driver.read_proc  =  NULL;
	callout_driver.proc_entry  =  NULL;

	if(request_irq(IRQ_INT2,rs_interrupt,SA_INTERRUPT,
		       "rs_linkup dbg",NULL))
		panic("Couldn't get irq for dgbrs_linkup"); 

	if(tty_register_driver(&rs_driver))
		panic("Couldn't register IRIS debug serial driver\n");
	if(tty_register_driver(&callout_driver))
		panic("Couldn't register IRIS debug callout driver\n");

	/* other init */
	dbg_port_p->ier = 0;

#ifdef CONFIG_PM
	iris_dbgrs_pm_dev = pm_register(PM_SYS_DEV, 0, iris_dbgrs_pm_callback);
#endif
	printk("IRIS debug serial driver registered!\n");

	return 0;
}

module_init(rs_irisdbg_init);

#ifdef CONFIG_DEBUG_SERIAL_IRIS_CONSOLE

/* --------------------------------------------------
 *             console driver
 * -------------------------------------------------- */

static void rs_console_write(struct console *co, const char *s,u_int count)
{
	int i;

	//printk("rs_console_write: s=%s,count=%d\n", s,count);
	iris_disable_dbguart_irq(&(uports[0]),INTMASK_TX);
	for(i=0;i<count;i++) {
		while( dbg_port_p->lsr & LSR_TXEMPTY ){
			dbg_port_p->rbr_thr = s[i];
			if(s[i]=='\n') {
				while(IO_UARTFLG & UARTFLG_UTXFF);
				dbg_port_p->rbr_thr = '\r';
			}
		}
	}
	iris_enable_dbguart_irq(&(uports[0]),INTMASK_TX);
}

static int rs_console_wait_key(struct console *co)
{ 
	int c;

	//printk("rs_console_wait_key: \n");
	iris_disable_dbguart_irq(&(uports[0]),INTMASK_RX);
	while(IO_UARTFLG & UARTFLG_URXFE);
	c=dbg_port_p->rbr_thr;
	iris_enable_dbguart_irq(&(uports[0]),INTMASK_RX);
	return c;
}

static kdev_t rs_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, DBGSERIAL_IRIS_MINOR_START);
}

static int __init rs_dbgconsole_setup(struct console *co, char *options)
{
	int	baud = 9600;
	int	bits = 8;
	int	parity = 'n';
	int	cflag = CREAD | HUPCL | CLOCAL;

	//printk("rs_dbgconsole_setup: options=%s\n", options);

	if(0 && options) {
		char	*s=options;
		baud = simple_strtoul(options, NULL, 10);
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s - '0';
	}

	/*
	 *	Now construct a cflag setting.
	 */
	switch(baud) {
	case 1200: cflag |= B1200; break;
	case 2400: cflag |= B2400; break;
	case 4800: cflag |= B4800; break;
	case 19200: cflag |= B19200; break;
	case 38400: cflag |= B38400; break;
	case 57600: cflag |= B57600; break;
	case 115200: cflag |= B115200; break;
	default: cflag |= B9600; break;
	}
	switch(bits) {
	case 7: cflag |= CS7; break;
	default: cflag |= CS8; break;
	}
	switch(parity) {
	case 'o': 
	case 'O': cflag |= PARODD; break;
	case 'e': 
	case 'E': cflag |= PARENB; break;
	}
	co->cflag = cflag;
	rs_set_cflag(&uports[0],cflag);
	dbg_port_p->iir_fcr |= FCR_FIFOENABLE;
	rs_console_write(NULL,"boot ",5);
	if(options)
		rs_console_write(NULL,options,strlen(options));
	else
	  	rs_console_write(NULL,"no options",10);
	rs_console_write(NULL,"\n",1);

	return 0;
}

static struct console rs_cons = {
	name:		"ttySD",
	write:		rs_console_write,
	device:		rs_console_device,
	wait_key:	rs_console_wait_key,
	setup:		rs_dbgconsole_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init rs_irisdbg_console_init(void)
{
	register_console(&rs_cons);
}

void __init iris_debug_serial_console_init(void)
{
        rs_irisdbg_console_init();
}

#endif /* CONFIG_DEBUG_SERIAL_IRIS_CONSOLE */
