/*! 
 * @file	mx2uart.c
 * @brief       Mx2 ADS Board Uart Driver
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
 * Copyright (C) 2002 Motorola Semiconductors HK Ltd
 *
 * @auther:         Frank Li (zhili@motorola.com)
 * @date:           10/11/2003
 * 
 */

/*********************************************************************************************************
**
* Copyright  (C) 2001 Motorola Cooperations, All rights reserved
* File Name:      uart_mx1.c
* Description:    This is the source code for UART hardware subroutines
* Auther:         Charles Pan (Charlesp@sc.mcel.mot.com)
* Date:           12/17/2001
* 
*********************************************************************************************************
***************************************************************************************
* MODIFICATION HISTORY:
*
* Date	         Person	          Change
* 12/17/01       Charles Pan      Initial version  
*						
*******************************************************************************************************/


#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>

#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/reboot.h>
#include <linux/keyboard.h>
#include <linux/init.h>


#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/delay.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <linux/pm.h>

//#include <asm/arch/memory.h>
#include "Tahiti_def.h"
//#define UART_DEBUG

//#define UART_2 	1

typedef unsigned int    U32;    /* unsigned 32 bit data */
typedef volatile U32 * VP_U32;

#define used_and_not_const_char_pointer
//#define IRDA

#include "mx2uart.h"

#ifndef	DMA_MODE_READ
#define DMA_MODE_READ	0
#endif

#ifndef DMA_MODE_WRITE
#define DMA_MODE_WRITE   1
#endif

#define TIOCSETDMA	0x54fe
#define TIOCSETISR 	0x54ff

//typedef void (*callback_t)();
//typedef void (*err_callback_t)(int error_type);

#ifdef CONFIG_MX2_INT_UART_CONSOLE
static struct console mx2uart_cons;
#endif

#define MX2_MAX_UART 4

static struct dbmx_serial dbmx_soft[MX2_MAX_UART];

struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct tty_struct *serial_table[MX2_MAX_UART];
static struct termios *serial_termios[MX2_MAX_UART];
static struct termios *serial_termios_locked[MX2_MAX_UART];

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#ifdef UART_DEBUG
#define TRACE printk
#else
#define TRACE 1?0:printk
#endif 

/*! the baudrate table*/
static int baud_table[] = 
{
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000,
	921600, 1000000, 1152000, 0
};

/*!
 *  tmp_buf is used as a temporary buffer by serial_write.  We need to
 *  lock it in case the memcpy_fromfs blocks while swapping in a page,
 *  and some other program tries to do a serial write at the same time.
 *  Since the lock will only come under contention when the system is
 *  swapping and available memory is low, it makes sense to share one
 *  buffer across all the serial ports, since it significantly saves
 *  memory if large numbers of serial ports are open.
 */
static unsigned char tmp_buf[SERIAL_XMIT_SIZE]; /* This is cheating */

DECLARE_MUTEX(tmp_buf_sem);

DECLARE_TASK_QUEUE(tq_serial);



/*! forware declaraction */
static void do_serial_bh(void);
static int rs_open(struct tty_struct *tty, struct file *filp);
void rs_close(struct tty_struct *tty, struct file *filp);
//static void rs_close_1(struct tty_struct *tty, struct file *filp); 
static int rs_write(struct tty_struct *tty, int from_user, const unsigned char *buf,
		    int count );

static void rs_flush_buffer(struct tty_struct *tty);
static int rs_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg );
static void rs_throttle(struct tty_struct *tty);
static void rs_unthrottle(struct tty_struct *tty);
static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios);
static void rs_start(struct tty_struct *tty);
static void rs_hangup(struct tty_struct *tty);
static void rs_set_ldisc(struct tty_struct *tty);
static int rs_write_room(struct tty_struct *tty);
static int rs_chars_in_buffer(struct tty_struct *tty);
static void do_softint(void *private_);
static void do_serial_hangup(void *private_);

static inline int serial_paranoia_check(struct dbmx_serial *info,
					dev_t device, const char *routine);

static void shutdown(struct dbmx_serial *info);
static void change_speed(struct dbmx_serial *info);
static int startup(struct dbmx_serial *info);
static int block_til_ready(struct tty_struct *tty, struct file *filp,
			struct dbmx_serial *info);
static void send_break(struct dbmx_serial *info, int duration);
static int get_serial_info(struct dbmx_serial * info, struct serial_struct * retinfo);
static int set_serial_info(struct dbmx_serial * info, struct serial_struct * new_info);
static inline void dbmx_rtsdtr(struct dbmx_serial *ss, int set);
static void baud_rate_setting(int baudrate);
static inline void status_handle(struct dbmx_serial *info, unsigned short status);


static  void receive_chars(int irq, void *dev_id, struct pt_regs *regs);
static  void transmit_chars(int irq, void *dev_id, struct pt_regs *regs);


static void int_uart(int irq, void *dev_id, struct pt_regs *regs);


static void wakeup_interrupt1(int irq, void *dev_id, struct pt_regs *regs); 
static void wakeup_interrupt0(int irq, void *dev_id, struct pt_regs *regs); 

void rs_dma_interrupt(void);
void rs_dma_err_interrupt(void);
static inline void rs_sched_event(struct dbmx_serial *info, int event);




static int  uart_pm_callback(struct pm_dev *pmdev, pm_request_t rqst, void *data);

static void mx2uart_init(void);

//#ifdef	UNIT_TEST
int dma_write_flag_1 = 0;
int dma_write_flag_2 = 0;
int dma_read_flag = 0;
int uart2_irda=0;
int uart2_busy=0;
unsigned char * physical_add_1;
unsigned char * physical_add_2;
int write_channum_1, read_channum_1;
int write_channum_2, read_channum_2;
int having_configured_uart = 0;
unsigned char * dma_receive_buf_1;
unsigned char * dma_receive_buf_2;
unsigned char * r_physical_add_1;
unsigned char * r_physical_add_2;
//DECLARE_WAIT_QUEUE_HEAD(my_queue);
wait_queue_head_t my_queue;
struct pm_dev *pmdev;
//#endif
//Karen add
unsigned long g_mpctl_value,g_ipclk_value;
//Karen end

//#define _reg_AITC_INTSRCL       (*((volatile unsigned long *)(IO_ADDRESS(0x22304C))))


#define SERIAL_MX1_NR	1


#define	TTYS0_MINOR		64
//#define TTYS1_MINOR		65
//#define TTYS2_MINOR		66
//#define TTYS3_MINOR 		67

struct mx1_port
{
        unsigned int            uart_base;
        unsigned int            irq;
        unsigned int            uartclk;
        unsigned int            fifosize;
        unsigned int            tiocm_support;
        void (*set_mctrl)(struct mx1_port *, u_int mctrl);
};


/*setup timer*/
static struct timer_list serial_timer_1;
static struct timer_list serial_timer_2;

#define RS_STROBE_TIME (2*HZ)

/******************************************************************************

 Function Name    : rsmx_init

 Input Parameters : NONE.

 Output Parameters:

 Value returned   : 1		error occurrs
 		    0		ok

 Description      : this is the initialization routine for the driver .

 Cautions         :

 Prev Condition   :

 Post Condition   :

******************************************************************************/

static int __init rsmx_init(void)
{


	int flags;
	struct dbmx_serial *info;
	int i;

	mx2uart_init();

	printk("UART driver version 0.3.6\n");
	//TRACE("H ");
	//set up base handler, and timer table
	init_bh(SERIAL_BH, do_serial_bh);

	//Initialize the tty_driver structure

	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.name = "ttyS%d";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = MX2_MAX_UART;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag = B115200 | CS8 |CREAD | HUPCL | CLOCAL;// | CRTSCTS;

	/*added for debug*/
	//serial_driver.init_termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE) ;
	serial_driver.init_termios.c_lflag &= ~ICANON;

	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
#ifdef UNIT_TEST
	TRACE(KERN_ERR "serial_driver.refcount: %d\n",*(serial_driver.refcount));
#endif
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;


	serial_driver.open = rs_open;
	serial_driver.close = rs_close;
	serial_driver.write = rs_write;
	//serial_driver.flush_chars = rs_flush_chars;
	serial_driver.write_room = rs_write_room;
	serial_driver.chars_in_buffer = rs_chars_in_buffer;
	serial_driver.flush_buffer = rs_flush_buffer;
	serial_driver.ioctl = rs_ioctl;
	serial_driver.throttle = rs_throttle;
	serial_driver.unthrottle = rs_unthrottle;
	serial_driver.set_termios = rs_set_termios;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;
	serial_driver.set_ldisc = rs_set_ldisc;


	// * 	 * The callout device is just like normal device except for
	// * 	 * major number and the subtype code.

	callout_driver = serial_driver;
	callout_driver.name = "cua%d";
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");

	save_flags(flags); cli( );

	for(i = 0; i < MX2_MAX_UART; i++)
	{
		info = &dbmx_soft[i];
		dbmx_soft[i].magic = SERIAL_MAGIC;
		dbmx_soft[i].port = 0xE400A000+0x1000*i;
		dbmx_soft[i].tty = 0;
		dbmx_soft[i].irq = 0x40;
		dbmx_soft[i].custom_divisor = 16;
		dbmx_soft[i].close_delay = 50;
		dbmx_soft[i].closing_wait = S_CLOSING_WAIT_NONE;//3000;
		dbmx_soft[i].x_char = 0;
		dbmx_soft[i].event = 0;
		dbmx_soft[i].count = 0;
		dbmx_soft[i].blocked_open = 0;
		dbmx_soft[i].tqueue.routine = do_softint;
		dbmx_soft[i].tqueue.data = info;
		dbmx_soft[i].tqueue_hangup.routine = do_serial_hangup;
		dbmx_soft[i].tqueue_hangup.data = info;
		dbmx_soft[i].callout_termios =callout_driver.init_termios;
		dbmx_soft[i].normal_termios = serial_driver.init_termios;
		init_waitqueue_head(&dbmx_soft[i].open_wait);
		init_waitqueue_head(&dbmx_soft[i].close_wait);
		dbmx_soft[i].line = 0;
		dbmx_soft[i].is_cons = 0;
		dbmx_soft[i].xmit_buf = 0;
		/*dma related thing*/
		dbmx_soft[i].dma_enable = 0;
		dbmx_soft[i].dma_write = 0;
		//init_waitqueue_head (&my_queue);
		//info -> wq = my_queue;	
		dbmx_soft[i].flags = 0;// S_SPLIT_TERMIOS;
	}	

	// disable the DREN bit (Data Ready interrupt enable) before requesting IRQs
//	_reg_UART1_UCR4 &= ~0x1; //bit0 


	if (request_irq(INT_UART1,
			int_uart,
			IRQ_FLG_STD,
			"DBMX_UART", NULL))
		panic("Unable to attach DBMX serial interrupt1\n");

	if (request_irq(INT_UART2,
			int_uart,
			IRQ_FLG_STD,
			"DBMX_UART", NULL))
		panic("Unable to attach DBMX serial interrupt2\n");

	if (request_irq(INT_UART3,
			int_uart,
			IRQ_FLG_STD,
			"DBMX_UART",NULL))
		panic("Unable to attach DBMX serial interrupt3\n");

	if (request_irq(INT_UART4,
			int_uart,
			IRQ_FLG_STD,
			"DBMX_UART",NULL))
		panic("Unable to attach DBMX serial interrupt4\n");


	restore_flags(flags);
	
	
	
	return 0;

}

/******************************************************************************
 *
 *  Function Name    : rsmx_fini
 *
 *  Input Parameters : NONE.
 *
 *  Output Parameters:
 *
 *  Value returned   : 1		error occurrs
 *      	       0		ok
 *
 *  Description      : this is the exit routine for the driver .
 *
 *  Cautions         :
 *
 *  Prev Condition   :
 *
 *  Post Condition   :
 *
 *******************************************************************************/
static void rsmx_fini(void)
{
	unsigned long flags;

	save_flags(flags); cli( );
	remove_bh(SERIAL_BH);
	if (tty_unregister_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_unregister_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	restore_flags(flags);
	/*free the interrupt handler*/
	#if 0
	free_irq(UART_IRQ_NUM1, NULL);
	free_irq(UART_IRQ_NUM2, NULL);
	free_irq(UART_2_IRQ_NUM1, NULL);
	free_irq(UART_2_IRQ_NUM2, NULL);
	free_irq(UART1_WAKEUP_INT_NUM, NULL);
	free_irq(UART2_WAKEUP_INT_NUM, NULL);
	TRACE( "GOOD BYE\n");
	#endif
}

/*init the module*/
module_init(rsmx_init);

/*terminate the module*/
module_exit(rsmx_fini);

#ifdef 	MODULE
void cleanup_module(void)
{
	rsmx_fini();
}
#endif	/*MODULE*/


/******************************************************************************
 *
 *  Function Name    : rs_start
 * 
 *  Input Parameters : tty	the structure associated with the state of the tty
 * 
 *  Output Parameters:  
 * 
 *  Value returned   : NONE.	
 *       		                        
 *  Description      : this routine notifies the tty driver that it resumes sending 
 *  		       characters to the tty device.
 * 
 *  Cautions         :
 * 
 *  Prev Condition   :
 * 
 *  Post Condition   :
 * 
 ********************************************************************************/
static void rs_start(struct tty_struct *tty)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty -> driver_data;
	unsigned long flags;
	int uart;

	if(serial_paranoia_check(info, tty -> device, "rs_start"))
		return;

	uart=MINOR(tty -> device) - TTYS0_MINOR+UART_1;
	if (!(_reg_UART_USR1(uart) & RTSS))
		return;

	save_flags(flags);cli( );

	if (info->xmit_cnt && info->xmit_buf && !(_reg_UART_UCR2(uart) & TXEN))
	{
		_reg_UART_UCR2(uart) |= TXEN;
		_reg_UART_UCR1(uart) |= (UCR1_TRDYEN | UCR1_TXMPTYEN);
	}	
	restore_flags(flags);
}


/******************************************************************************
 *  
 *  Function Name    : rs_hangup 
 *  
 *  Input Parameters : tty	the structure associated with the state of the tty
 *  
 *  Output Parameters:  
 *  
 *  Value returned   : NONE.	
 *        		                        
 *  Description      : this routine notifies the tty driver that it should hangup the 
 *  		       tty device.
 *  
 *  Cautions         :
 *  
 *  Prev Condition   :
 * 
 *  Post Condition   :
 *  
 *********************************************************************************/
static void rs_hangup(struct tty_struct *tty)
{
	struct dbmx_serial * info = (struct dbmx_serial *)tty->driver_data;
	//TRACE("^^^,%x\n",info->tty);
	//return ;
	
	
	
	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;
		
	rs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(S_NORMAL_ACTIVE|S_CALLOUT_ACTIVE);
	TRACE("set tty to 0\n");
	info->tty = 0;
	
	wake_up_interruptible(&info->open_wait);
}

/******************************************************************************
 *   
 *  Function Name    : rs_set_termios 
 *    
 *  Input Parameters : tty		the structure associated with the state of the tty
 *  		       old_termios	the original termios settings	
 *    
 *  Output Parameters:  
 *    
 *  Value returned   : NONE.	
 *          		                        
 *  Description      : this routine allows the tty driver to be notified when device's
 *  		       termios settings have changed
 *    		           
 *  Cautions         :
 *    
 *  Prev Condition   :
 *   
 *  Post Condition   :
 *    
 **********************************************************************************/
static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;
	change_speed(info);
	if ((old_termios->c_cflag & CRTSCTS) && !(tty->termios->c_cflag & CRTSCTS))
	{
		tty->hw_stopped = 0;
		rs_start(tty);
	}
			
}


/******************************************************************************
 *    
 *  Function Name    : rs_throttle 
 *      
 *  Input Parameters : tty		the structure associated with the state of the tty	
 *      
 *  Output Parameters:  
 *      
 *  Value returned   : NONE.	
 *            		                        
 *  Description      : this routine is called by the upper-layer tty to signal that incoming
 *  		       characters should be throttled.
 *      		           
 *  Cautions         :
 *      
 *  Prev Condition   :
 *     
 *  Post Condition   :
 *      
 ***********************************************************************************/
static void rs_throttle(struct tty_struct *tty)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;
	unsigned long flags;
	int uart;
	
	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;

	if (I_IXOFF(tty))
		info->x_char = STOP_CHAR(tty);

	/* Turn off CTS line (do this atomic) */
	save_flags(flags); cli( );
	uart=MINOR(tty->device)-TTYS0_MINOR+UART_1;

	_reg_UART_UCR4(uart) &= ~DREN;
		//_reg_UART2_UCR1 &= ~IDEN;
	_reg_UART_UCR1(uart) &= ~RRDYEN;

	restore_flags(flags);
}


/******************************************************************************
 *
 *  Function Name    : rs_unthrottle
 *
 *  Input Parameters : tty		the structure associated with the state of the tty
 *
 *  Output Parameters:
 *
 *  Value returned   : NONE.
 *
 *  Description      : this routine is called by the upper-layer tty to signal that incoming
 *    		       characters should be unthrottled.
 *
 *  Cautions         :
 *
 *  Prev Condition   :
 *
 *  Post Condition   :
 *
 ************************************************************************************/
static void rs_unthrottle(struct tty_struct *tty)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;
	unsigned long flags;
	int uart;
#ifdef UART_DEBUG
	TRACE(KERN_ERR "here i am to unthrottle the device........\n");
#endif
	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;

	if (I_IXOFF(tty))
	{
		if (info->x_char)
			info->x_char = 0;
		else
			info->x_char = START_CHAR(tty);
	}
	
	save_flags(flags); cli( );
	/* Assert CTS line (do this atomic),what happens when it's not throttled before? */
	uart=tty->device-TTYS0_MINOR+UART_1;

	_reg_UART_UCR4(uart) |= DREN;
	
	restore_flags(flags);
}


/******************************************************************************
 *
 *  Function Name    : rs_open
 *
 *  Input Parameters : tty		the structure associated with the state of the tty
 *  		       filp		the structure reflecting the file associated with
 *  		       			the device being opened.
 *
 *  Output Parameters:
 *
 *  Value returned   : 0		opened without error.	or
 *  		       			error code.
 *
 *  Description      : This routine is called when a particuliar tty device is opened.
 *
 *  Cautions         :
 *
 *  Prev Condition   :
 *
 *  Post Condition   :
 *
 *************************************************************************************/
static int rs_open(struct tty_struct *tty, struct file *filp)
{
	struct dbmx_serial	*info = NULL;
	int retval, line;

	TRACE("just before getting minor\n");

	line = MINOR(tty->device) - tty->driver.minor_start;

#ifdef UART_DEBUG
	//tty -> icanon = 0;
	//TRACE("tty -> icanon:       %d\n",tty -> icanon);
//	line = 0;
#endif  /*UART_DEBUG*/
//	if (line != 0) /* we have exactly one */
//		return -ENODEV;

	TRACE("tty->device %d\n",tty->device);

	info=dbmx_soft+line;

	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;
#ifdef UART_DEBUG		
	TRACE(KERN_ERR " after the check\n");
#endif
	info->count++;
	tty->driver_data = info;
	TRACE("set tty to %x\n",tty);
	info->tty = tty;
	
#ifdef CONFIG_MX2_INT_UART_CONSOLE
       /*
        * Copy across the serial console cflag setting
        */
	if (mx2uart_cons.cflag && mx2uart_cons.index == line)
	{
		tty->termios->c_cflag = mx2uart_cons.cflag;
		mx2uart_cons.cflag = 0;
	}
#endif
	/* Start up serial port*/
	retval = startup(info);
	if (retval)
		return retval;
	retval = block_til_ready(tty, filp, info);
	
	if (retval)
	{
		return retval;
	}

	if ((info->count == 1) && (info->flags & S_SPLIT_TERMIOS))
	{
		TRACE(KERN_ERR "HI BOY, I HOPE TO BE HERE FOR THE FIRST TIME\n");
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else
			*tty->termios = info->callout_termios;
		change_speed(info);
	
	}
	info->session = current->session;
	info->pgrp = current->pgrp;

TRACE("just after getting minor\n");

		return 0;
												
}

/******************************************************************************
 *          
 *  Function Name    : rs_close 
 *        
 *  Input Parameters : tty		the structure associated with the state of the tty
 *    		       filp		the structure reflecting the file associated with
 *    		       			the device being opened.
 *            
 *  Output Parameters:  
 *            
 *  Value returned   : NONE
 *                  		                        
 *  Description      : This routine is called when a particuliar tty device is closed.
 *            		           
 *  Cautions         :
 *            
 *  Prev Condition   :
 *          
 *  Post Condition   :
 *            
 **************************************************************************************/
void rs_close(struct tty_struct *tty, struct file *filp)
{
	struct dbmx_serial * info = (struct dbmx_serial *)tty->driver_data;
	unsigned long flags;

	TRACE("closing the device.....\n");
	
	
	
	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;
				
	save_flags(flags); cli();
				
	if (tty_hung_up_p(filp))
	{
		restore_flags(flags);
		return;
	}

	if ((tty->count == 1) && (info->count != 1))
	{
		/*	 * Uh, oh.  tty->count is 1, which means that the tty
		 *	 * structure will be freed.  Info->count should always
		 * 	 * be one in these conditions.  If it's greater than
		 *	 * one, we've got real problems, since it means the
		 * 	 * serial port won't be shutdown.
		 * 	 */
		TRACE("rs_close: bad serial port count; tty->count is 1, " "info->count is %d\n",
		info->count);
		info->count = 1;
	}
	if (--info->count < 0)
	{
		TRACE("rs_close: bad serial port count for ttyS%d: %d\n",info->line, info->count);
		info->count = 0;
	}
	if (info->count)
	{
		restore_flags(flags);
		return;
	}

	info->flags |= S_CLOSING;
	/* 	 * Save the termios structure, since this port may have
	 * 	 * separate termios for callout and dialin.
	 * 	 */
	if (info->flags & S_NORMAL_ACTIVE)
		info->normal_termios = *tty->termios;
	if (info->flags & S_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;
	/*
	 * 	 * Now we wait for the transmit buffer to clear; and we notify 
	 * 	 * the line discipline to only process XON/XOFF characters.
	 * 	 */
	tty->closing = 1;

	if (info->closing_wait != S_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
														/*
	  *	 * At this point we stop accepting input.  To do this, we
	  * 	 * disable the receive line status interrupts, and tell the
	  *  	 * interrupt driver to stop checking the data ready bit in the
	  * 	 * line status register.
	  * 	 */

#if 0
	if(MINOR(tty -> device) == TTYS0_MINOR)
	{
//		_reg_UART1_UCR2 &= ~RXEN;
//		_reg_UART1_UCR4 &= ~DREN;
	}
	else if(MINOR(tty -> device) == TTYS1_MINOR)
	{
		_reg_UART2_UCR2 &= ~RXEN;
		_reg_UART2_UCR4 &= ~DREN;
		uart2_busy = 0;
	}
	else if(MINOR(tty -> device) == TTYS2_MINOR)
	{
		_reg_UART2_UCR2 &= ~RXEN;
		_reg_UART2_UCR4 &= ~DREN;
		uart2_irda = 0;
		uart2_busy = 0;
	}	

#endif	
	/*delete the timer no matter if there is */
	//del_timer(&serial_timer_1);
	//del_timer(&serial_timer_2);
	
	shutdown(info);

	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);

	


	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);

	tty->closing = 0;
	info->event = 0;
	TRACE("c tty to 0\n");
	info->tty = 0;
	/*if (tty->ldisc.num != ldiscs[N_TTY].num)
	{
		if (tty->ldisc.close)
			(tty->ldisc.close)(tty);
		tty->ldisc = ldiscs[N_TTY];
		tty->termios->c_line = N_TTY;
		if (tty->ldisc.open)
			(tty->ldisc.open)(tty);
	} */  
	if (info->blocked_open)
	{
		if (info->close_delay)
		{
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
TRACE("1\n");
	info->flags &= ~(S_NORMAL_ACTIVE|S_CALLOUT_ACTIVE|S_CLOSING);
TRACE("2\n");
	wake_up_interruptible(&info->close_wait);
TRACE("3\n");
	restore_flags(flags);
TRACE("4\n");
}


/******************************************************************************
 *            
 *  Function Name    : rs_flush_buffer 
 *          
 *  Input Parameters : tty		the structure associated with the state of the tty
 *              
 *  Output Parameters:  
 *              
 *  Value returned   : NONE.
 *                    		                        
 *  Description      : This routine flushes the transmit buffer ring.
 *             		           
 *  Cautions         :
 *              
 *  Prev Condition   :
 *            
 *  Post Condition   :
 *              
 **************************************************************************************/
static void rs_flush_buffer(struct tty_struct *tty)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;
		
		
	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;
	cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	sti();

	wake_up_interruptible(&tty->write_wait);
	


	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
				
}


/******************************************************************************
 *              
 *  Function Name    : rs_flush_chars 
 *            
 *  Input Parameters : tty		the structure associated with the state of the tty
 *                
 *  Output Parameters:  
 *                
 *  Value returned   : NONE.
 *                      		                        
 *  Description      : This routine is called by the kernel after it has written a series
 *  		       of characters to the tty device using rs_write( ).
 *               		           
 *  Cautions         :
 *                
 *  Prev Condition   :
 *              
 *  Post Condition   :
 *                
 ***************************************************************************************/
#if 0
static void rs_flush_chars(struct tty_struct *tty)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;
	unsigned long flags;

#ifdef UART_DEBUG
	TRACE(KERN_ERR "ohhhhh, give me the flush of the chars....\n");
#endif
	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;
	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped || !info->xmit_buf)
		return;
	/* Enable transmitter */
	save_flags(flags); cli();
	if(MINOR(tty -> device) == TTYS0_MINOR)
	{
		_reg_UART1_UCR2 |= TXEN;
		_reg_UART1_UCR1 |= (UCR1_TRDYEN | UCR1_TXMPTYEN) ;
	}
	else if(MINOR(tty -> device) == TTYS1_MINOR)
	{
		_reg_UART2_UCR2 |= TXEN;
		_reg_UART2_UCR1 |= (UCR1_TRDYEN | UCR1_TXMPTYEN) ;
	}
	//if (USR1 & TRDY)
	//{
		/* Send char */
	//	UTXD = info->xmit_buf[info->xmit_tail++];
	//	info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
	//	info->xmit_cnt--;
	//}
	restore_flags(flags);
}
#endif


/******************************************************************************
 *                
 *  Function Name    : rs_write_room 
 *              
 *  Input Parameters : tty		the structure associated with the state of the tty
 *                  
 *  Output Parameters:  
 *                  
 *  Value returned   : the remained free space in the buffer ring.
 *                        		                        
 *  Description      : this routine returns the numbers of characters the tty driver will
 *  		       accept for queuing to be written.
 *                 		           
 *  Cautions         :
 *                  
 *  Prev Condition   :
 *                
 *  Post Condition   :
 *                  
 ****************************************************************************************/
static int rs_write_room(struct tty_struct *tty)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;
	int	ret;
						
	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
						
}


/******************************************************************************
 *                  
 *  Function Name    : rs_ioctl 
 *                
 *  Input Parameters : tty		the structure associated with the state of the tty.
 *  		       file		the structure reflecting the file associated with
 *  		        		the device being opened.
 *  		       cmd		the controlling command to the device.
 *  		       arg		the argument for the specific command.
 *  		        
 *                    
 *  Output Parameters:  
 *                    
 *  Value returned   : 0		the command is executed without error.	or
 *  		                        error code.
 *                          		                        
 *  Description      : this routine allows the tty driver to implement device specific I/O controls.
 *                   		           
 *  Cautions         :
 *                    
 *  Prev Condition   :
 *                  
 *  Post Condition   :
 *                    
 *****************************************************************************************/
static int rs_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg )
{
	int error;
	struct dbmx_serial * info = (struct dbmx_serial *)tty->driver_data;
	int retval;
	int uart;
	//unsigned char * r_physical_add; 

	TRACE("rs_ioctl \n");

	uart=MINOR(tty->device)-TTYS0_MINOR+UART_1;

	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
		return -ENODEV;


	TRACE("rs_ioctl 12\n");

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGWILD)  &&
	    (cmd != TIOCSERSWILD) && (cmd != TIOCSERGSTRUCT))
	{
		if (tty->flags & (1 << TTY_IO_ERROR))
			    return -EIO;
	}
	TRACE("rs_ioctl 13\n");					
	switch (cmd)
	{
		case TCSBRK:	/* SVID version: non-zero arg --> no break */
			TRACE("TCSBRK \n");
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (!arg)
			send_break(info, HZ/4);	/* 1/4 second */
				return 0;
		case TCSBRKP:	/* support for POSIX tcsendbreak() */
		TRACE("TCSBRKP \n");
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			send_break(info, arg ? arg*(HZ/10) : HZ/4);
			return 0;
		case TIOCGSOFTCAR:
		TRACE("TIOCGSOFTCAR \n");
			error = verify_area(VERIFY_WRITE, (void *) arg,sizeof(long));
			if (error)
				return error;
			put_user(C_CLOCAL(tty) ? 1 : 0,(unsigned long *) arg);
			return 0;
		case TIOCSSOFTCAR:
		TRACE("TIOCSSOFTCAR \n");
			get_user(arg, (unsigned long *) arg);
			tty->termios->c_cflag = ((tty->termios->c_cflag & ~CLOCAL) | 
						(arg ? CLOCAL : 0));
			return 0;
		case TIOCGSERIAL:
		TRACE("TIOCGSERIAL \n");
			error = verify_area(VERIFY_WRITE, (void *) arg,sizeof(struct serial_struct));
			if (error)
				return error;
			return get_serial_info(info, (struct serial_struct *) arg);
		case TIOCSSERIAL:
		TRACE("TIOCSSERIAL \n");
			return set_serial_info(info, (struct serial_struct *) arg);
	

		case TIOCSETISR:
		TRACE("TIOCSETISR \n");
				_reg_UART_UCR4(uart) |= 1;

				info -> dma_enable = 0;
				break;
			
		default:
			return -ENOIOCTLCMD;
	}
	return 0;
}


/******************************************************************************
 *                    
 *  Function Name    : rs_write 
 *                  
 *  Input Parameters : tty		the structure associated with the state of the tty.
 *    		       from_user	flag showing if the data is from the user space.
 *    		       buf		the pointer to the data to be written.
 *    		       count		the number of data to be written.
 *    		        
 *                      
 *  Output Parameters:  
 *                      
 *  Value returned   : 			actuall number of data written.
 *                            		                        
 *  Description      : this routine is called by the kernel to write a series of characters to 
 *  		       the tty device.
 *                     		           
 *  Cautions         :
 *                      
 *  Prev Condition   :
 *                    
 *  Post Condition   :
 *                      
 ******************************************************************************************/
static int rs_write(struct tty_struct *tty, int from_user, const unsigned char *buf, int count )
{
	int	c, total = 0;
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;
	unsigned long flags;
	int uart;	
//TRACE("rs_write\n");
	
	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if (!tty || !info->xmit_buf)
		return 0;

	//if (info->xmit_cnt > SERIAL_XMIT_SIZE)
	//	return 0;
	
	
	uart=MINOR(tty->device)-TTYS0_MINOR+UART_1;
	if (!(_reg_UART_USR1(uart) & RTSS))
		return count;

	save_flags(flags);

	while (1)
	{

		cli();		
		c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
					SERIAL_XMIT_SIZE - info->xmit_head));
		if (c <= 0)
			break;
		if (from_user)
		{
			down(&tmp_buf_sem);
			copy_from_user(tmp_buf, buf, c);
			c = MIN(c, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - info->xmit_head));
			memcpy(info->xmit_buf + info->xmit_head, tmp_buf, c);
			up(&tmp_buf_sem);
		}
		else
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
		info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt += c;
		restore_flags(flags);
		buf += c;
		count -= c;
		total += c;
	}
	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped)
	{
//		TRACE(KERN_ERR "CAN I ENABLE THE TRANSMITTER NOW?\n");
		if(!info->dma_enable)
		{
			/* Enable transmitter */
			cli();	
			//TRACE("the count of data is: %d\n", info->xmit_cnt);

			uart=MINOR(tty->device)-TTYS0_MINOR+UART_1;

			_reg_UART_UCR2(uart) |= TXEN;
			_reg_UART_UCR1(uart) |= UCR1_TRDYEN | UCR1_TXMPTYEN;
			
			restore_flags(flags);
		}
	}
	restore_flags(flags);
	
	return total;
										
}


/******************************************************************************
 *                      
 *  Function Name    : rs_set_ldisc 
 *                    
 *  Input Parameters : tty		the structure associated with the state of the tty.
 *      		        
 *                        
 *  Output Parameters:  
 *                        
 *  Value returned   : NONE.
 *                              		                        
 *  Description      : this routine allows the tty driver to be notified when the tty's termios
 *  		       settings have changed.
 *                       		           
 *  Cautions         :
 *                        
 *  Prev Condition   :
 *                      
 *  Post Condition   :
 *                        
 *******************************************************************************************/
static void rs_set_ldisc(struct tty_struct *tty)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_set_ldisc"))
		return;

	info->is_cons = (tty->termios->c_line == N_TTY);
	TRACE("ttyS%d console mode %s\n", info->line, info->is_cons ? "on" : "off");
			
}



/******************************************************************************
 *                        
 *  Function Name    : rs_chars_in_buffer 
 *                     
 *  Input Parameters : tty		the structure associated with the state of the tty.
 *        		        
 *                          
 *  Output Parameters:  
 *                          
 *  Value returned   : the number of characters in the buffer ring.
 *                                		                        
 *  Description      : this routine returns the number of characters remaining to be sent
 *  		       in the buffer ring.
 *                         		           
 *  Cautions         :
 *                          
 *  Prev Condition   :
 *                        
 *  Post Condition   :
 *                         
 *******************************************************************************************/
static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct dbmx_serial *info = (struct dbmx_serial *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
		
}


/******************************************************************************
 *                          
 *  Function Name    : rs_wait_until_sent 
 *                       
 *  Input Parameters : tty		the structure associated with the state of the tty.
 *          	       timeout		the time limit to wait	        
 *                            
 *  Output Parameters:  
 *                            
 *  Value returned   : NONE.
 *                                  		                        
 *  Description      : this routine wait until data in the transmitter FIFO is empty.
 *                           		           
 *  Cautions         :
 *                            
 *  Prev Condition   :
 *                          
 *  Post Condition   :
 *                           
 ********************************************************************************************/
#if 0
static int rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct dbmx_serial *info = tty -> driver_data;
	unsigned long orig_jiffies;

	if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
		return 1;
	
	orig_jiffies = jiffies;
		return 0;	

}
#endif


/******************************************************************************
 *                            
 *  Function Name    : receive_chars 
 *                         
 *  Input Parameters : irq		the IRQ number.
 *  		       dev_id		the deivce identifier.
 *            	       regs		a pointer to the Kernel Mode stack area containing
 *            	       			the registers saved right after the interrupt occurred.
 *                              
 *  Output Parameters:  
 *                              
 *  Value returned   : NONE.
 *                                    		                        
 *  Description      : this is the generic interrupt handler for receivingo.
 *                             		           
 *  Cautions         :
 *                              
 *  Prev Condition   :
 *                            
 *  Post Condition   :
 *                             
 *********************************************************************************************/
static void mx2_receive_chars(int irq, void *dev_id, struct pt_regs *regs)
{
	struct dbmx_serial *info = &dbmx_soft[0];
	struct tty_struct *tty;
	unsigned long rx,flag,i;
	int uart;

	uart=get_uart_by_irqnum(irq);
	info=dbmx_soft+(uart-UART_1);

	tty = info->tty;
	
	//TRACE("rx handler.\n");
	//for(i=0;i<100000;i++);
//	TRACE(KERN_ERR "Character received : 0x%08x\n", (unsigned int )rx);
	
	//isrflag = 1;

	if(!tty){
		rx = _reg_UART_URXD(uart);
		//TRACE("BYE BYE INTERRUPT %x\n",rx);
		goto clear_and_exit;
		}
		//TRACE("try to flip %x\n",tty->flip.count);
		
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
		{	_reg_UART_UCR4(uart) &= ~DREN;
			test_and_set_bit(TTY_THROTTLED, &tty->flags);
			queue_task(&tty->flip.tqueue, &tq_timer);
			TRACE("try to flip %x\n",tty->flip.count);
			return;
		}
	//TRACE("before to read \n");
	/*
	 * 	 * This do { } while() loop will get ALL chars out of Rx FIFO 
	 *       */
	do
	{       
		//for(i=0;i<100000;i++);
		flag = _reg_UART_USR2(uart);
		
		//TRACE("try to read %x\n",flag);
		
		if(!(flag & 1))break;
		
		rx = _reg_UART_URXD(uart);
		
		TRACE("read ok %x\n",rx);
	#if 0	
		if(rx & PRERR)
		{
			*tty->flip.flag_buf_ptr++ = TTY_PARITY;
			status_handle(info, rx);
		}
		else if(rx & OVRRUN)
		{
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
			status_handle(info, rx);
		}
		else if(rx & FRMERR)
		{
			*tty->flip.flag_buf_ptr++ = TTY_FRAME;
			status_handle(info, rx);
		}
		else
	#endif	
		//{
			*tty->flip.flag_buf_ptr++ = 0; /* XXX */
		//}
		*tty->flip.char_buf_ptr++ = (unsigned char)rx;
		tty->flip.count++;
		
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
		{	_reg_UART_UCR4(uart) &= ~DREN;
			test_and_set_bit(TTY_THROTTLED, &tty->flags);
			queue_task(&tty->flip.tqueue, &tq_timer);
			return;
		}
		
	}while(1);
	
	queue_task(&tty->flip.tqueue, &tq_timer);
	

//	TRACE(KERN_ERR "BYE BYE INTERRUPT\n");

clear_and_exit:
	return ;

}

/******************************************************************************
 *                              
 *  Function Name    : mx2_transmit_chars 
 *                           
 *  Input Parameters : irq		the IRQ number.
 *    		       dev_id		the deivce identifier.
 *             	       regs		a pointer to the Kernel Mode stack area containing
 *             	       			the registers saved right after the interrupt occurred.
 *                                
 *  Output Parameters:  
 *                                
 *  Value returned   : NONE.
 *                                      		                        
 *  Description      : this is the generic interrupt handler for transmitting.
 *                               		           
 *  Cautions         :
 *                                
 *  Prev Condition   :
 *                              
 *  Post Condition   :
 *                               
 **********************************************************************************************/
void mx2_transmit_chars(int irq, void *dev_id, struct pt_regs *regs)
{
	int i;
	struct dbmx_serial *info = &dbmx_soft[0];
	int uart;
	
	uart=get_uart_by_irqnum(irq);
	info=dbmx_soft+(uart-UART_1);
		
	//TRACE("trans  \n");

	if (info->x_char)
	{
		/* Send next char */
		_reg_UART_UTXD(uart) = (unsigned long)info->x_char;
		info->x_char = 0;
		goto clear_and_return;
	}
	//TRACE("trans  1\n");
	
	if((info->xmit_cnt <= 0) || info->tty->stopped)
	{
		/* That's peculiar... TX ints off */
		
		_reg_UART_UCR1(uart) &= ~(UCR1_TRDYEN | UCR1_TXMPTYEN);
		//TRACE("_reg_UART1_UCR1 %x \n",_reg_UART_UCR1(uart));
		goto clear_and_return;
	}
	//TRACE("trans  2\n");

	/* Send char */
//	while(info->xmit_cnt)//&& sendd)
//	{
		_reg_UART_UTXD(uart) = (unsigned long)(info->xmit_buf[info->xmit_tail++]);
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
		
//		if(!(_reg_UART_USR1(uart)&0x2000)) break;
//	}	

	//TRACE("trans  3\n");


	if (info->xmit_cnt < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

		

	if(info->xmit_cnt <= 0)
	{
		/* All done for now... TX ints off */
		_reg_UART_UCR1(uart) &= ~(UCR1_TRDYEN | UCR1_TXMPTYEN);
		goto clear_and_return;
	}
	
		//TRACE("trans  4\n");
	
clear_and_return:
	/* Clear interrupt (should be auto)*/
	return;
								
}

/******************************************************************************
 *                                
 *  Function Name    : rs_dma_interrupt 
 *                            
 *  Input Parameters : info	struct showing serial port state.
 *                                  
 *  Output Parameters:  
 *                                  
 *  Value returned   : NONE.
 *                                        		                        
 *  Description      : this routine is the DMA interrupt routine.
 *                                 		           
 *  Cautions         :
 *                                  
 *  Prev Condition   :
 *                                
 *  Post Condition   :	
 *                                 
 ***********************************************************************************************/
 
static int get_uart_by_irqnum(int irq)
{
	switch(irq)
	{
		case INT_UART1:return UART_1;
		case INT_UART2:return UART_2;
		case INT_UART3:return UART_3;
		case INT_UART4:return UART_4;
	}
	return UART_1;
}

static void wakeup_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int uart;
	uart=get_uart_by_irqnum(irq);
	
//	TRACE("interrupt for wake up occurres...\n");

	/*re enable UART module */
	_reg_UART_UCR1(uart) |= UARTEN;
	_reg_UART_UCR2(uart) |= (TXEN | RXEN);
	
	/*disable this interrupt*/
	_reg_UART_UCR3(uart) &= ~AWAKEN;
	/*clear the interrupt bit*/
	_reg_UART_USR1(uart) |= AWAKE;
}


/**callback function for power management*/
static int  uart_pm_callback(struct pm_dev *pmdev, pm_request_t rqst, void *data)
{
	int uart=*(int *)data;
	switch(rqst)
	{
		
		case PM_SUSPEND:
//			TRACE("uart callback2 suspend.\n\r");
			if(_reg_UART_USR2(uart) & RDR)
				return 1;
			else
			{
				_reg_UART_UCR1(uart) &= ~UARTEN;
				_reg_UART_UCR2(uart) &= ~(TXEN | RXEN);

				if(dbmx_soft[1].dma_enable)
				{
					//disable_dma(read_channum_2);
					//disable_dma(write_channum_2);
				}
				/*nable the asynchronous wake up interrupt */
				_reg_UART_UCR3(uart) |= AWAKEN;
			}
			
			break;
			
		case PM_RESUME:
//			TRACE("uart callback2 resume.\n\r");
			_reg_UART_UCR1(uart) |= UARTEN;
			_reg_UART_UCR2(uart) |= (TXEN | RXEN);
			/*disable the wake up interrupt */
			_reg_UART_UCR3(uart) &= ~AWAKEN;
			if(dbmx_soft[1].dma_enable)
			{
				//enable_dma(read_channum_2);
				//enable_dma(write_channum_2);
			}
			break;
	}
	return 0;
}



/******************************************************************************
 *                                  
 *  Function Name    : rs_sched_event 
 *                              
 *  Input Parameters : info	struct showing serial port state.
 *  		       event	the event type that wakes up certain task queue.
 *                                    
 *  Output Parameters:  
 *                                    
 *  Value returned   : NONE.
 *                                          		                        
 *  Description      : this routine is used by the interrupt handler to schedule processing in the 
 *  		       software interrupt portion of the driver.
 *                                   		           
 *  Cautions         :
 *                                    
 *  Prev Condition   :
 *                                  
 *  Post Condition   :
 *                                   
 ************************************************************************************************/
static inline void rs_sched_event(struct dbmx_serial *info, int event)
{
	info->event |= 1 << event;
#ifdef UART_DEBUG
	TRACE(KERN_ERR " in rs_sched_event\n");
#endif
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}



/******************************************************************************
 *                                    
 *  Function Name    : do_serial_bh 
 *                                
 *  Input Parameters : NONE.
 *                                      
 *  Output Parameters:  
 *                                      
 *  Value returned   : NONE.
 *                                            		                        
 *  Description      : this routine is to handle the bottom half processing for the serial driver.
 *                                     		           
 *  Cautions         :
 *                                      
 *  Prev Condition   :
 *                                    
 *  Post Condition   :
 *                                     
 *************************************************************************************************/
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}


/******************************************************************************
 *                                      
 *  Function Name    : do_softint 
 *                                  
 *  Input Parameters : private		the private data for the serial port.
 *                                        
 *  Output Parameters:  
 *                                        
 *  Value returned   : NONE.
 *                                              		                        
 *  Description      : this is the actual routine which is executed in the SERIAL_BH.
 *                                       		           
 *  Cautions         :
 *                                        
 *  Prev Condition   :
 *                                      
 *  Post Condition   :
 *                                       
 **************************************************************************************************/
static void do_softint(void *private_)
{
	struct dbmx_serial	*info = (struct dbmx_serial *)private_;
	struct tty_struct	*tty;


	tty = info -> tty;
	if(!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event))
	{

		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);


		wake_up_interruptible(&tty->write_wait);

	}

}



/******************************************************************************
 *                                        
 *  Function Name    : do_serial_hangup 
 *                                    
 *  Input Parameters : private		the private data for the serial port.
 *                                          
 *  Output Parameters:  
 *                                          
 *  Value returned   : NONE.
 *                                                		                        
 *  Description      : this is the actual routine which is executed in scheduler tqueue for hangup.
 *                                         		           
 *  Cautions         :
 *                                          
 *  Prev Condition   :
 *                                        
 *  Post Condition   :
 *                                 
 ***************************************************************************************************/
static void do_serial_hangup(void *private_)
{
	struct dbmx_serial *info = (struct dbmx_serial *) private_;
	struct tty_struct	*tty;
	TRACE("do_serial_hangup");			
	tty = info->tty;
	if (!tty)
		return;
	

	tty_hangup(tty);
}




/********************************************************************************************
 * HERE BEGINS THE INTERNAL ROUTINE
 * **********************************************************************************************/

/******************************************************************************
 *                                        
 *  Function Name    : shutdown 
 *                                    
 *  Input Parameters : info	struct showing serial port state.
 *                                         
 *  Output Parameters:  
 *                                          
 *  Value returned   : NONE.
 *                                                		                        
 *  Description      : this routine shuts down the serial port, disable the interrupt.
 *                                         		           
 *  Cautions         :
 *                                          
 *  Prev Condition   :
 *                                        
 *  Post Condition   :
 *                                         
 ***********************************************************************************************/
static void shutdown(struct dbmx_serial *info)
{
	unsigned long	flags;

	if (!(info->flags & S_INITIALIZED))
		return;

	save_flags(flags); cli(); /* Disable interrupts */
	
	if (info->xmit_buf)
	{
		//free_page((unsigned long) info->xmit_buf);
		TRACE("free uart page\n");
		//info->xmit_buf = 0;
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);
	
	info->flags &= ~S_INITIALIZED;
	
	restore_flags(flags);
}



/******************************************************************************

 Function Name    : serial_paranoia_check 

 Input Parameters : info	the information controlling the operation of the driver.
 		    device	the tty device.
		    routine	the routine associated info.

 Output Parameters:  

 Value returned   : 1		error occurrs
 		    0		ok	
                    
 Description      : this routine is used to check the validation of magic number and info.

 Cautions         :

 Prev Condition   :

 Post Condition   :

******************************************************************************/
static inline int serial_paranoia_check(struct dbmx_serial *info,
							dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic = "Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo = "Warning: null dbmx_serial for (%d, %d) in %s\n";

	if (!info)
	{
		TRACE(badinfo, MAJOR(device), MINOR(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC)
	{
		TRACE(badmagic, MAJOR(device), MINOR(device), routine);
		return 1;
	}
#endif
	return 0;
}


/******************************************************************************
 *
 *  Function Name    : change_speed
 *
 *  Input Parameters : info	struct showing serial port state.
 *
 *  Output Parameters:
 *
 *  Value returned   : NONE.
 *
 *  Description      : this routine sets the specified baud rate for a serial port.
 *
 *  Cautions         :
 `*
 *  Prev Condition   :
 *
 *  Post Condition   :
 *
 ************************************************************************************************/
static void change_speed(struct dbmx_serial *info)
{
	unsigned cflag;
	//unsigned long ucr2;
	int i = 0;
	unsigned short port;
	unsigned long reg,ubir,ubmr,temp;

	int uart=0;
	uart=MINOR(info -> tty -> device) - TTYS0_MINOR;
	if(uart<0)
		return;
	if(uart>=4)
		return;

//	TRACE(KERN_ERR "_reg_UART2_UCR2:::: 0x%x\n", (int)_reg_UART2_UCR2);

	if(!info -> tty || !info -> tty -> termios)
		return;

	cflag = info -> tty -> termios -> c_cflag;
	if (!(port = info->port))
		return;
		
	i = cflag & CBAUD;
	if(i & CBAUDEX)
	{
		i = (i & ~CBAUDEX) + B38400;
	}
	info -> baud = baud_table[i];
	
//Karen add


//	printk("presc=%d, bclkdiv =%d, ipdiv=%d, g_mpctl_value=%d,p_ipclk=%d \n",presc,bclkdiv,ipdiv,g_mpctl_value,g_ipclk_value);


/*
		mpctl = _reg_CRM_MPCTL0;
		cscr = _reg_CRM_CSCR;

	    temp = _reg_CRM_CSCR;
		temp &= ~0x0000c000;
		temp |= presc<<14;
	    _reg_CRM_CSCR = temp;        

	    temp = _reg_CRM_CSCR;
		temp &= ~0x00003C00;
		temp |= bclkdiv<<10; 		
	    _reg_CRM_CSCR = temp;     	

	    temp = _reg_CRM_CSCR;
	    temp &= ~0x00000200;
	    temp |= ipdiv<<9; 		
	    _reg_CRM_CSCR = temp;     	

		_reg_CRM_MPCTL0 = g_mpctl_value;
		_reg_CRM_MPCTL0 = 0x007b1C73;
    	_reg_CRM_CSCR |=0x00200000;

    	//wait for the MPLLRESTART bit self clear
    	while (_reg_CRM_CSCR & 0x00200000);
    	
    ubir = 0xF;
	ubmr = 10000;
	temp = (1152*16)/(g_ipclk_value/2);
	ubir = baud_table[i]*temp/115200;
*/
    ubir = 0xF;
	ubmr = 10000;
	temp = (1152*16)/(g_ipclk_value/2);
	ubir = baud_table[i]*temp/115200;
    	
//Karen modify

/*	_reg_CRM_MPCTL0 = 0x007b1C73;
	*(VP_U32)CRM_CSCR |=0x00200000;

	//wait for the MPLLRESTART bit self clear
	while (*(VP_U32)CRM_CSCR & 0x00200000);
	
  	ubir = 0xF;
	ubmr = 10000;
//	temp = (1152*16)/(g_ipclk_value/2);
//	ubir = baud_table[i]*temp/115200;
  	ubir = baud_table[i]*0x346/115200;
*/
//Kaern end
    	
/*
	mpctl = _reg_CRM_MPCTL0;
	printk("mpctl = 0x%x ****************\n",mpctl);
	cscr = _reg_CRM_CSCR;
	printk("cscr = 0x%x ******************\n",cscr);
	if(mpctl == 0x3811c89)//240MHz
	{
		if((cscr & 0x0000c000) == 0)//flck 240
		{
			if((cscr & 0x00003C00) == 0x00000800)//bclk 80
			{
			  	ubir = 0xF;
			  	ubmr = 10000;
				ubir = baud_table[i]*922/115200;
			}
			else if((cscr & 0x00003C00) == 0x00000c00)//bclk 60
			{
			  	ubir = 0xF;
			  	ubmr = 10000;
				ubir = baud_table[i]*1229/115200;
			}
		}
		else if((cscr & 0x0000c000) == 0x00004000)//fclk 120
		{
			ubmr = 10000;
			ubir = baud_table[i]*1229/115200;
		}
	}
	else if(mpctl == 0x2141d70)//258MHz
	{
		if((cscr & 0x0000c000) == 0)//flck 258
		{
			if((cscr & 0x00003C00) == 0x00000800)//bclk 86
			{
			  	ubmr = 10000;
				ubir = baud_table[i]*858/115200;
			}
			else if((cscr & 0x00003C00) == 0x00000c00)//bclk 64
			{
			  	ubmr = 10000;
				ubir = baud_table[i]*1153/115200;
			}
		}
		else if((cscr & 0x0000c000) == 0x00004000)//fclk 129
		{
			ubmr = 10000;
			ubir = baud_table[i]*1153/115200;
		}
	}
	else if(mpctl == 0x007b1C73)//266MHz
	{
		if((cscr & 0x0000c000) == 0)//flck 266
		{
			if((cscr & 0x00003C00) == 0x00000800)//bclk 88
			{
			  	ubmr = 10000;
				ubir = baud_table[i]*838/115200;
			}
			else if((cscr & 0x00003C00) == 0x00000c00)//bclk 66
			{
			  	ubmr = 10000;
				ubir = baud_table[i]*1118/115200;
			}
		}
		else if((cscr & 0x0000c000) == 0x00004000)//fclk 133
		{
			ubmr = 10000;
			ubir = baud_table[i]*1118/115200;
		}
	}
	else if(mpctl == 0x272216d)//288MHz
	{
		if((cscr & 0x0000c000) == 0)//flck 288MHz
		{
			if((cscr & 0x00003C00) == 0x00000800)//bclk 96
			{
			  	ubmr = 10000;
				ubir = baud_table[i]*769/115200;
			}
			else if((cscr & 0x00003C00) == 0x00000c00)//bclk 72
			{
			  	ubmr = 10000;
				ubir = baud_table[i]*1025/115200;
			}
		}
		else if((cscr & 0x0000c000) == 0x00004000)//fclk 144
		{
			ubmr = 10000;
			ubir = baud_table[i]*1025/115200;
		}
}
*/
//Karen end




/*disable the transmitter and the receiver*/
		_reg_UART_UCR2(uart) &= ~(TXEN |RXEN);

		/*added gradually*/
	 	_reg_UART_UCR2(uart) &= ~(PREN | PROE | STPB| WS | IRTS);
		if((cflag & CSIZE) == CS8)
			_reg_UART_UCR2(uart) |= WS;
		if(cflag & CSTOPB)
			_reg_UART_UCR2(uart) |= STPB;
		if(cflag & PARENB)
			_reg_UART_UCR2(uart) |= PREN;
		if(cflag & PARODD)
			_reg_UART_UCR2(uart) |= PROE;
		if(cflag & CRTSCTS)
		{
			TRACE(KERN_ERR "enabling the hardware flowcontrol\n");
			_reg_UART_UCR2(uart) &= ~IRTS;
			_reg_UART_UCR2(uart) |= CTSC;
		}
		else
		{
			//_reg_UART2_UCR2 |= IRTS;
			_reg_UART_UCR2(uart) &= ~CTSC;
		}
		/*enable the transmitter and the receiver*/
		_reg_UART_UCR2(uart) |= (TXEN | RXEN);
		
	  	_reg_UART_UBIR(uart) = ubir-1;
	  	_reg_UART_UBMR(uart) = ubmr-1;
		
		//printk("ubir %d ubmr %d\n",ubir,ubmr);
	 
	  	//Set RX fifo level to 20
	  	//_reg_UART2_UFCR = 0x50000A94;
	  	_reg_UART_UCR1(uart) |= RRDYEN;
	  	_reg_UART_UCR4(uart) |= DREN;
		//_reg_UART2_UCR1 |= 0x0c00;	//set idle level to 32		

	  
	/*end of add*/
//	_reg_UART1_UCR2 = 0x00004027;
	/*set the flag for haveing configured*/
	having_configured_uart = 1;
	return;

}

/******************************************************************************
 *                                              
 *  Function Name    : startup 
 *                                        
 *  Input Parameters : info	struct showing serial port state.
 *  
 *  Output Parameters:  
 *                                               
 *  Value returned   : 0	success with the function.	or
 *  				error code.
 *                                                     		                        
 *  Description      : this routine initializes a serial port.
 *                                              		           
 *  Cautions         :
 *                                                
 *  Prev Condition   :
 *                                              
 *  Post Condition   :
 *                                               
 **************************************************************************************************/
static int startup(struct dbmx_serial *info)
{
	unsigned long flags;
	int i;
	int uart;

		
	if (info->flags & S_INITIALIZED)
		return 0;

	TRACE(KERN_ERR "BEFORE REQUEST\n"); 

	/*allocate a block of memory for dma transmit and  receive*/
	uart=MINOR(info->tty->device)- TTYS0_MINOR + UART_1;
				

		if (!info->xmit_buf)
		{
			info->xmit_buf = (unsigned char *)__get_free_pages(GFP_KERNEL | GFP_DMA,1);
			if (!info->xmit_buf)
				return -ENOMEM;
		}
		
#ifdef UART_DEBUG	
	TRACE(KERN_ERR "AFTER REQUEST\n");
#endif
	save_flags(flags); cli();


//#ifdef UART_DEBUG
//	TRACE(KERN_ERR "after resetting the fifo\n");
//	TRACE(KERN_ERR "_reg_UART1_UCR2 value right after reset: 0x%x\n", (int)_reg_UART1_UCR2);
//#endif
		
		info->xmit_fifo_size = 1;
		
	
#ifdef UART_DEBUG
	//_reg_UART1_UCR1 |= UCR1_TRDYEN | UCR1_TXMPTYEN;	
#endif
	if (info->tty)
		test_and_clear_bit(TTY_IO_ERROR, &info->tty->flags);

	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
#ifdef UART_DEBUG	
	TRACE(KERN_ERR "before changing speed\n");
#endif
	/* and set the speed of the serial port */
	change_speed(info);

	info->flags |= S_INITIALIZED;
	restore_flags(flags);
	return 0;
											
}


/******************************************************************************
 *                                                
 *  Function Name    : block_til_ready 
 *                                          
 *  Input Parameters : tty	the structure associated with the state of the tty.
 *  		       filp 	the structure reflecting the file associated with
 *  		       		the device being opened.
 *  		        
 *    		       info	struct showing serial port state.
 *    
 *  Output Parameters:  
 *                                                 
 *  Value returned   : 0	success with the function.	or
 *    				error code.
 *                                                      		                        
 *  Description      : this routine blocks to wait for the carrier_detect and the line
 *  		       to become free.
 *                                                		           
 *  Cautions         :
 *                                                  
 *  Prev Condition   :
 *                                                
 *  Post Condition   :
 *                                                 
 **************************************************************************************************/
static int block_til_ready(struct tty_struct *tty, struct file *filp, struct dbmx_serial *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int	retval;
	int	do_clocal = 0;

	/*
	 * 	 * If the device is in the middle of being closed, then block
	 * 	 * until it's done, and then try again.
	 * 	 	 	 */
	if (info->flags & S_CLOSING)
	{
		interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & S_HUP_NOTIFY)
			return -EAGAIN;
		else
			return -ERESTARTSYS;
#else
		return -EAGAIN;
#endif
	}

	/*
	 * 	 * If this is a callout device, then just make sure the normal
	 * 	 	 * device isn't being used.
	 * 	 	 	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) 
	{
		if (info->flags & S_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & S_CALLOUT_ACTIVE) && (info->flags & S_SESSION_LOCKOUT) &&
		(info->session != current->session))
		        return -EBUSY;
		if ((info->flags & S_CALLOUT_ACTIVE) && (info->flags & S_PGRP_LOCKOUT) &&
		(info->pgrp != current->pgrp))
		        return -EBUSY;
		info->flags |= S_CALLOUT_ACTIVE;
			return 0;
	}
					
	/*
	* 	 * If non-blocking mode is set, or the port is not enabled,
	* 	 	 * then make the check up front and then exit.
	* 	 	 	 */
	if ((filp->f_flags & O_NONBLOCK) || (tty->flags & (1 << TTY_IO_ERROR)))
	{
		if (info->flags & S_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= S_NORMAL_ACTIVE;
			return 0;
	}

	if (info->flags & S_CALLOUT_ACTIVE)
	{
		if (info->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	}
	else
	{
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}
							
	/*
	 * 	 * Block waiting for the carrier detect and the line to become
	 * 	 * free (i.e., not in use by the callout).  While we are in
	 * 	 * this loop, info->count is dropped by one, so that
	 * 	 * rs_close() knows when to free things.  We restore it upon
	 * 	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
	info->count--;
	info->blocked_open++;
	while (1)
	{
		cli();
		if (!(info->flags & S_CALLOUT_ACTIVE))
			dbmx_rtsdtr(info, 1);
		sti();
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) || !(info->flags & S_INITIALIZED))
		{
#ifdef SERIAL_DO_RESTART
			if (info->flags & S_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & S_CALLOUT_ACTIVE) && !(info->flags & S_CLOSING) && do_clocal)
			break;
		if (signal_pending(current))
		{
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		info->count++;
	info->blocked_open--;
	if (retval)
		return retval;
	info->flags |= S_NORMAL_ACTIVE;
	return 0;

}


/******************************************************************************
 *                                                  
 *  Function Name    : send_break 
 *                                            
 *  Input Parameters : info		struct showing serial port state.
 *  		       duration		the time period during which BREAK sent to the recever. 
 *  Output Parameters:  
 *                                                   
 *  Value returned   : 0	success with the function.	or
 *      			error code.
 *                                                        		                        
 *  Description      : this routine is used to send BREAK to the receiver for some period of time.
 *                                                  		           
 *  Cautions         :
 *
 *  Prev Condition   :
 *                                                  
 *  Post Condition   :
 *                                                   
 **************************************************************************************************/
static void send_break(struct dbmx_serial *info, int duration)
{
	unsigned long flags;
	int uart=UART_1;

	if(!info -> port)
		return;
	current -> state = TASK_INTERRUPTIBLE;
	TRACE("send_break\n");
	save_flags(flags);
	cli();

	uart=MINOR(info->tty->device)- TTYS0_MINOR +UART_1;

	_reg_UART_UCR1(uart) |= SNDBRK;
	/*give up the CPU*/
	schedule_timeout(duration);
	_reg_UART_UCR1(uart) &= ~ SNDBRK;
}



/** Sets or clears DTR/RTS on the requested line */
static inline void dbmx_rtsdtr(struct dbmx_serial *ss, int set)
{
	if (set)
	{
		/* set the RTS/DTR line */
	}
	else
	{
		/* clear it */
	}
	return;
}



/**get serial infomation*/
static int get_serial_info(struct dbmx_serial * info, struct serial_struct * retinfo)
{
	struct serial_struct tmp;
		  
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.port = info->port;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	copy_to_user(retinfo,&tmp,sizeof(*retinfo));
	return 0;
}


/**set the serial port with the new information*/
static int set_serial_info(struct dbmx_serial * info, struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	struct dbmx_serial old_info;
	int retval = 0;

	if (!new_info)
		return -EFAULT;
	copy_from_user(&new_serial,new_info,sizeof(new_serial));
	old_info = *info;

	if (!suser())
	{
		if ((new_serial.baud_base != info->baud_base) ||
		    (new_serial.type != info->type) ||
		    (new_serial.close_delay != info->close_delay) ||
		    ((new_serial.flags & ~S_USR_MASK) != (info->flags & ~S_USR_MASK)))
			return -EPERM;
		info->flags = ((info->flags & ~S_USR_MASK) | (new_serial.flags & S_USR_MASK));
		info->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}
	if (info->count > 1)
		return -EBUSY;
	/*
	 * 	 * OK, past this point, all the error checking has been done.
	 * 	 * At this point, we start making changes.....
	 * 	 */
	info->baud_base = new_serial.baud_base;
	info->flags = ((info->flags & ~S_FLAGS) |
	(new_serial.flags & S_FLAGS));
	info->type = new_serial.type;
	info->close_delay = new_serial.close_delay;
	info->closing_wait = new_serial.closing_wait;

check_and_exit:
	retval = startup(info);
	return retval;
}


/**status handling when receiving*/
static inline void status_handle(struct dbmx_serial *info, unsigned short status)
{
	TRACE("ERR OCCURRED WHEN RECEIVING!!!!!!!\n");
	return;
}

void MX21_UartSetting(void)
{

 	unsigned long temp;
 	unsigned long mpctl,cscr,presc,bclkdiv,ipdiv;
		
	//only config MPCTL0 and poll the MPLLRESTART when FMCR [31:30] (CLKMODE[1:0]) = b11/b01/b10
	// PLL non bypass mode
//	if (*(VP_U32)(IO_ADDRESS(SYS_FMCR)) & 0xC0000000)
//	{
		// 255.9999476MHz/ (2 [PRESC = b01, /2] * 2 [BCLKDIV=b0001, /2] * 2 [IPDIV=b1, /2]) 
		// = ipg_clk = ipg_per_clk (31.99999346MHz)
		// HCLK = 63.99998691MHz 
		// FLCK = 127.9999738MHz 
	mpctl = _reg_CRM_MPCTL0;
	if(mpctl != 0x007b1C73)
	{
		presc = (mpctl>>10)&0x3;
		bclkdiv = (mpctl>>16)&0xf;
		ipdiv = (mpctl>>26)&0x1;

	    temp = _reg_CRM_CSCR;
		temp &= ~0x0000c000;
		temp |= presc<<14;
	    _reg_CRM_CSCR = temp;        

	    temp = _reg_CRM_CSCR;
		temp &= ~0x00003C00;
		temp |= bclkdiv<<10; 		
	    _reg_CRM_CSCR = temp;     	

	    temp = _reg_CRM_CSCR;
	    temp &= ~0x00000200;
	    temp |= ipdiv<<9; 		
	    _reg_CRM_CSCR = temp;     	

		_reg_CRM_MPCTL0 = g_mpctl_value;
//		_reg_CRM_MPCTL0 = 0x007b1C73;
    	_reg_CRM_CSCR |=0x00200000;

    	//wait for the MPLLRESTART bit self clear
    	while (_reg_CRM_CSCR & 0x00200000);
	}	
//	}
	//PLL bypass mode (MPLL = 128MHz)
/*	else
	{
		// 128MHz/ (1 [PRESC = b00, /1] * 2 [BCLKDIV=b0001, /2] * 2 [IPDIV=b1, /2]) 
		// = ipg_clk = ipg_per_clk (32MHz)
		// HCLK = 64MHz 
		// FLCK = 128MHz 
		
	    temp = *(VP_U32)(IO_ADDRESS(CRM_CSCR));
	    temp |= 0x0000C000;
	    *(VP_U32)(IO_ADDRESS(CRM_CSCR)) = temp;        //set PRESC = b11 (i.e. /4) will change to /1 later

	    temp = *(VP_U32)(IO_ADDRESS(CRM_CSCR));
	    temp |= 0x00000200;
	    *(VP_U32)(IO_ADDRESS(CRM_CSCR)) = temp;     	//set IPDIV = 1

	    temp = *(VP_U32)(IO_ADDRESS(CRM_CSCR));
	    temp &= ~0x00003C00;
//	    temp |= 0x00000400;
	    *(VP_U32)(IO_ADDRESS(CRM_CSCR)) = temp;        // set BCLKDIV = 1 (i.e. /2)

	 
	    *(VP_U32)(IO_ADDRESS(CRM_CSCR)) &= ~0x0000C000; //set back PRESC = b00 (i.e./1)

		// No Meaning as ipg_clk = ipg_per_clk
	    // Set up PLL and Clock controller module
        // set PERDIV1 to b0011 i.e. /4 (***no meaning for TO1 as ipg_clk = ipg_per_clk, for TO2 only)	   
	    *(VP_U32)(IO_ADDRESS(CRM_PCDR)) |= 0x300;    
	    *(VP_U32)(IO_ADDRESS(CRM_PCDR)) &= ~0xC00;              
		
	}
*/	
    //enable clock for HCLK BROM and UART1
    	*(VP_U32)(IO_ADDRESS(CRM_PCCR0)) |= 0x1000000f;

	// software reset
	*(VP_U32)(IO_ADDRESS(UART1_CR2)) = 0x0;
	
	//write 1 to RXDMUXSEL = 1
	*(VP_U32)(IO_ADDRESS(UART1_CR3)) |= 0x4;
	
	// software reset
	*(VP_U32)(IO_ADDRESS(UART2_CR2)) = 0x0;
	
	//write 1 to RXDMUXSEL = 1
	*(VP_U32)(IO_ADDRESS(UART2_CR3)) |= 0x4;	
	
	// software reset
	*(VP_U32)(IO_ADDRESS(UART3_CR2)) = 0x0;
	
	//write 1 to RXDMUXSEL = 1
	*(VP_U32)(IO_ADDRESS(UART3_CR3)) |= 0x4;
	
	// software reset
	*(VP_U32)(IO_ADDRESS(UART4_CR2)) = 0x0;
	
	//write 1 to RXDMUXSEL = 1
	*(VP_U32)(IO_ADDRESS(UART4_CR3)) |= 0x4;	
	
	//=================================================================
	// Set up GPIO/IOMUX for UART1  
	*(VP_U32)(IO_ADDRESS(GPIOE_GIUS)) &= 0xFFFF0FFF;	// clear bit 12-bit 15 of GIUS_E
	*(VP_U32)(IO_ADDRESS(GPIOE_GPR)) &= 0xFFFF0FFF;	// clear bit 12-bit 15 of GPR_E

	//=================================================================
	
	_reg_GPIO_GIUS(GPIOE) &= ~0x00000d8;	//port E pin 3,4,6,7 for uart2
	_reg_GPIO_GPR(GPIOE) &= ~0x00000d8;	//port E pin 3,4,6,7 for primary function
	_reg_GPIO_GIUS(GPIOE) &= ~0x00000300;	//port E pin 8-9 for uart3
	_reg_GPIO_GPR(GPIOE) &= ~0x00000300;	//port E pin 8-9 for primary function
	_reg_GPIO_GIUS(GPIOB) &= ~0x30000000;	//port E pin 28-29 for uart4
	_reg_GPIO_GPR(GPIOB) |= 0x30000000;	//port E pin 28-29 for alternate function

//#ifdef CONFIG_ARCH_MX2ADS
	*((volatile uint16_t *)0xE3800000) |= 0x0180;
//#endif
}

void MX21_InitInternalUART(int uart)
{
	unsigned long bir;
	//software reset
	_reg_UART_UCR2(uart)=0x61E6;
	_reg_UART_UCR1(uart)=0;
	_reg_UART_UCR3(uart)=0x4;
	_reg_UART_UCR4(uart)=0x8000;

	// configure appropriate pins for UART1_RX, UART1_TX, UART2_RX and UART2_TX		

	_reg_UART_UCR1(uart)=0x0005;
	//UCR2 = CTSC,TXEN,RXEN=1,reset, ignore IRTS bit, WS = 1 , 
	//8 bit tx and rx,1 stop bit, disable parity
	_reg_UART_UCR2(uart)=0x6026;
	_reg_UART_UCR3(uart)=0x4;
	_reg_UART_UCR4(uart)=0x8000;

	//=================================================================
	// Set up reference freq divide for UART module 
	// MX2 only support 16MHz output
	// PerCLK1(31.99998691MHz)/UFCR[RFDIV] (2)= 15.99999673MHz
	_reg_UART_UFCR(uart)=0x0A01;
	//=================================================================

	//clear loopback bit
	_reg_UART_UTS(uart) = 0x0000;

	//reference frequency is 16MHz
	
	//BIR_115200 = 1151
	//NUM  = 1151+1		DENOM = 9999+1
	//thus NUM/DENOM = 0.1152 --> gives baud rate: 115200		
	//for 88MHz Bclk //576;// for 128M //BIR_115200;//for 64M // configure UBIR
	// configure UBMR
	
//Karen modify
	bir = (1152*16)/(g_ipclk_value/2);

	_reg_UART_UBIR(uart) = bir;
//	_reg_UART_UBIR(uart) = 837;
	_reg_UART_UBMR(uart) = 9999;
//Karen end
	
}

static void mx2uart_init(void)
{
	int uart;	
//Karen add
 	unsigned long mpctl,presc,bclkdiv,ipdiv;
	volatile signed long mpll,fref;
	volatile int mfi,mfn,mfd,pd,redundant;
	unsigned long UPCTL0;
	volatile signed long fout_trial;
	signed long deviation;
	
	mpctl = _reg_CRM_MPCTL0;
	if(mpctl != 0x007b1C73)
	{
		mpll = mpctl & 0x3ff;
	
		fref =  16777;//multiply by 1000

		for(mfi = 5; mfi < 15; mfi++)
		{
			for(mfd = 0; mfd < 1023; mfd++)
			{
				for(mfn = 0; mfn < 1022; mfn++)
				{
					for(pd = 0; pd < 15; pd++)
					{
						if(	mfn <= mfd )
						{
					//remove redundancy
							if(	(mfn == 0) && (mfd != 0))
							{
								redundant++;
							}
							else
							{

						//apply equation
								fout_trial = (33554 * (((mfi*10 + (mfn*10) / (mfd + 1))*10) / ((pd + 1)*10)))/10000;

						//find % deviation
								if(fout_trial <= mpll)
									deviation = mpll - fout_trial;
								else
									deviation = fout_trial-mpll;
							
								if(deviation <= 2)
								{
							//generate UPCTL0 / MPCTL0 register value

									UPCTL0 = (pd  & 0x000F) << 26 |
										 (mfd & 0x03FF) << 16 |
										 (mfi & 0x000F) << 10 |
										 (mfn & 0x03FF);
								
							//write result into file
//								printk("%d\t%d\t%d\t%d\t0x%08x\t%d%%\t%d\n", MFI, MFN, MFD, PD, UPCTL0, deviation, fout_trial);

									goto getvalue;
								}
							}
						}
					}
				}
			}
		}

getvalue:
	presc = ((mpctl>>10)&0x3) + 1;
	bclkdiv = ((mpctl>>16)&0xf) + 1;
	ipdiv = ((mpctl>>26)&0x1) + 1;
	g_mpctl_value =	UPCTL0; 
	g_ipclk_value = ((fout_trial/presc)/bclkdiv)/ipdiv;
	}
	else
	g_ipclk_value = 44;
//Karen end	
//Karen modify
	MX21_UartSetting();
	for(uart=UART_1;uart<=UART_4;uart++)
		MX21_InitInternalUART(uart);
//Karen end
}


#ifdef CONFIG_MX2_INT_UART_CONSOLE

/* setup the mx1 uart port as a console*/

/**
 * This code is currently never used; console->read is never called.
 * Therefore, although we have an implementation, we don't use it.
 * FIXME: the "const char *s" should be fixed to "char *s" some day.
 * (when the definition in include/linux/console.h is also fixed)
 */

static int mx2uart_console_read(struct console *co, const char *s, unsigned long count)
{
	char *w;
	int c;
	unsigned long rx;
	int uart=UART_1;
	
#ifdef UART_DEBUG
	TRACE("mx2uart_console_read() called\n");
#endif

	c = 0;
	w = (char *)s;

	rx = _reg_UART_URXD(uart);
	do
	{
		c++;
		*w++ = (char)rx; 
		if(c == count)
			break;
	}while((rx = _reg_UART_URXD(uart)) & CHARRDY);

	//return the count
	return c;
		
}

/*
 * 	Enable UART
 * 	Aaron, 02Dec03
 */
static void enableUART(int uUart)
{

    _reg_UART_UCR1(uUart) |= UARTEN;
    _reg_UART_UCR2(uUart) |= TXEN;

}

/**
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void mx2uart_console_write(struct console *co, const char *s, unsigned long count)
{
	int i;
	int uart=UART_1;
	int temp_state;
	int terminerReady;
	
	if( _reg_UART_USR1(uart) & RTSS )
	{
	    _reg_UART_UCR2(uart) &= ~IRTS;
	    terminerReady = 1;
	}
	else
	{
	    /* Disable the transmitter ready interrupt and FIFO empty interrupt,
	     * Ignore the RTS pin
	     * Aaron Shen, 19 Dec 2003
	     */ 
 	    _reg_UART_UCR1(uart) &= ~(TRDYEN | TXMPTYEN);
	    _reg_UART_UCR2(uart) |= IRTS;
	    terminerReady = 0;
	}
	
	/*
	 *	First disable the interrupts if it is enabled
 	*/
	temp_state = _reg_UART_UCR1(uart);
	_reg_UART_UCR1(uart) &= ~(UCR1_TRDYEN | UCR1_TXMPTYEN); 

	/*
 	*	Now, do each character
 	*/
	for (i = 0; i < count; i++) 
	{
		do
		{
			enableUART(uart);
		}while(!(_reg_UART_USR2(uart) & TXDC) && terminerReady);
		if( terminerReady)
		{
			_reg_UART_UTXD(uart) = (unsigned long)s[i];
			if (s[i] == '\n')
			{
				do
				{
					enableUART(uart);	
				} while (!(_reg_UART_USR2(uart) & TXDC) && terminerReady);
				_reg_UART_UTXD(uart) = '\r';
			}
		}
	}

	/*
 	*	Finally, wait for transmitter to become empty
	*	and restore the TCR
 	*/
	do
	{
		enableUART(uart);	
	} while (!(_reg_UART_USR2(uart) & TXDC) && terminerReady);
	/*reenable interrupts*/
	_reg_UART_UCR1(uart) = temp_state;
}

/*
 *	Receive character from the serial port
 */
static int mx2uart_console_wait_key(struct console *co)
{
	unsigned long rx;
	int uart=UART_1;
	do
	{
		;
	}while (!((rx = _reg_UART_URXD(uart)) & CHARRDY));
	return rx;
}

static kdev_t mx2uart_console_device(struct console *c)
{
	return MKDEV(4, 64 );
}

static int __init mx2uart_console_setup(struct console *co, char *options)
{

	
	return 0;
}


static struct console mx2uart_cons =
{
	name:           "MX2",
	write:          mx2uart_console_write,
#ifdef used_and_not_const_char_pointer
	read:           mx2uart_console_read,
#endif
	device:         mx2uart_console_device,
//	wait_key:       mx2uart_console_wait_key,
	setup:          mx2uart_console_setup,
	flags:          CON_PRINTBUFFER,
	index:          -1,
};

void __init mx2uart_console_init(void)
{
	mx2uart_init();

	register_console(&mx2uart_cons);
}

#endif /* CONFIG_MX2_INT_UART_CONSOLE */


/**
 * @brief		UART interrupt handle
 *
 * @param irq		Interrupt Num
 *	
 * @param dev_id	
 *
 */
static void int_uart(int irq, void *dev_id, struct pt_regs *regs)
{
	int uartnum=UART_1;
	unsigned long statues;
	uartnum=get_uart_by_irqnum(irq);
	
	statues = _reg_UART_USR2(uartnum);
	
	if( statues & RDR)
		mx2_receive_chars( irq, dev_id, regs);
	else if ( statues & TXDC)
		mx2_transmit_chars(irq, dev_id, regs);


}	
