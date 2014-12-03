/*
 *  drivers/char/serial_iris.c
 *
 *  Driver for serial ports on the SHARP Iris-1 board
 *
 *  Based on drivers/char/serial_linkup.c (LINKUP-7200 chip)
 *
 *  ChangeLog:
 *   04-04-2001: Lineo Japan, Inc. ...
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

#include <linux/tqueue.h>
#include <linux/pm.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/arch/gpio.h>
#include <asm/arch/fpga.h>

#ifdef CONFIG_PM
static struct pm_dev* iris_rs_pm_dev;
#endif

#define IRQ_NINF IRQ_INTF
#define IRQ_U1   IRQ_UART_1
#define IRQ_U2   IRQ_UART_2

#define FRMERR  0x01  /* UART framing error */
#define PARERR  0x02  /* UART parity error */
#define OVERR   0x04  /* UART overrun error */

#define BRDIV   0x00000fff  /* Bit rate divisor */
#if 0
#define BR_115200    0x001
#define BR_76800     0x002
#define BR_57600     0x003
#define BR_38400     0x005
#define BR_19200     0x00B
#define BR_9600      0x017
#define BR_4800      0x02F
#define BR_2400      0x05F
#define BR_1200      0x0BF
#endif

#define BREAK	0x00001  /* Set Tx high */
#define PRTEN   0x00002  /* Parity enable */
#define EVENPRT 0x00004  /* Even parity */
#define XSTOP   0x00008  /* Extra stop bit */
#define FIFOEN  0x00010  /* Enable FIFO */
#define WRDLEN  0x00060  /* Word length */
#define WRDLEN_SHIFT   	5
#define WL_5		0x3     /*   5 bits */
#define WL_6		0x2     /*   6 bits */
#define WL_7		0x1     /*   7 bits */
#define WL_8		0x0     /*   8 bits */

#define	UARTEN	0x000001	/* UART enable bit	*/

#define UCTS	0x0001	/* UART Clear-To-Send (CTS)	*/
#define UDDR	0x0002	/* UART Data-Set_Ready(DSR)	*/
#define UDCD	0x0004	/* UART Data-Carrier-Detect(DCD)*/
#define UBUSY	0x0008	/* UART transmitter busy	*/
#define URXFE	0x0010	/* UART receiver FIFO empty	*/
#define UTXFF	0x0020	/* UART transmit FIFO full	*/

#define UART_TXINT	0x01	/* bit for TXINT */
#define UART_RXINT	0x02	/* bit for RXINT */
#define UART_RXERRINT	0x04	/* bit for RXERRINT */
#define UART_MSINT	0x08	/* bit for MSINT */
#define UART_UDINT	0x10	/* bit for UDINT */

#define BAUD_BASE 115200
#define SERIAL_L7200_MAJOR 204
#define SERIAL_L7200_MINOR 0
#define SERIAL_L7200_AUX_MAJOR 205
/* #define SERIAL_LINKUP_MINOR_START 64 */
#define SERIAL_LINKUP_MINOR_START SERIAL_L7200_MINOR

#undef RING_DISABLE  /* workaround for RI (SWB16) hardware illness */
#undef DSR2_DISABLE

#undef USE_SERIAL_BOTTOM_HALF /* this feature is not done . Because , UART-INT is not cleared before exitting from top-half ( fast-interrupt-handler ) , and top-half continues to be called before immediate-bh ( or serial-bh ) is called , then UART bottom half is never called ! */

#define GSM_IOCTL_CODE_BASE 0x56a0
#define GSM_IOCTL_CODE_GET_PC6 (GSM_IOCTL_CODE_BASE+0)
#define GSM_IOCTL_CODE_GET_PC7 (GSM_IOCTL_CODE_BASE+1)
#define GSM_IOCTL_CODE_SET_PC6 (GSM_IOCTL_CODE_BASE+2)
#define GSM_IOCTL_CODE_SET_PC7 (GSM_IOCTL_CODE_BASE+3)
#define GSM_IOCTL_CODE_GET_PC0 (GSM_IOCTL_CODE_BASE+4)
#define GSM_IOCTL_CODE_SET_PC0 (GSM_IOCTL_CODE_BASE+5)

/* --------------------------------------------------
 *       configuration ... needs to be moved
 * -------------------------------------------------- */

#define SERIAL_IRIS_MINOR_START SERIAL_LINKUP_MINOR_START
#define SERIAL_IRIS_DRIVER_NUM  2

/* --------------------------------------------------
 *         static variables
 * -------------------------------------------------- */

static struct tty_driver rs_driver, callout_driver;
static int rs_refcount;

static struct tty_struct *rs_table[SERIAL_IRIS_DRIVER_NUM];
static struct termios *rs_termios[SERIAL_IRIS_DRIVER_NUM];
static struct termios *rs_termios_locked[SERIAL_IRIS_DRIVER_NUM];

#ifdef USE_SERIAL_BOTTOM_HALF
static DECLARE_TASK_QUEUE(tq_serial);
#endif /* USE_SERIAL_BOTTOM_HALF */

#undef SERIAL_IRIS_DEBUG
#define DEBUGPRINT(s)  /* printk s */

/* --------------------------------------------------
 *         definitions for internal use
 * -------------------------------------------------- */

#ifdef CONFIG_IRIS_TS0
#define UART_ID_DEFAULT  (ID_UART1)
#define	SERIAL_MINOR	SERIAL_LINKUP_MINOR_START
#define	IRIS_UARTFLG	IO_UARTFLG
#define	IRIS_UARTDR	IO_UARTDR
#define	IRIS_UARTCON	IO_UARTCON
#else
#define UART_ID_DEFAULT  (ID_UART2)
#define	SERIAL_MINOR	(SERIAL_LINKUP_MINOR_START + 1)
#define	IRIS_UARTFLG	IO_UARTFLG2
#define	IRIS_UARTDR	IO_UARTDR2
#define	IRIS_UARTCON	IO_UARTCON2
#endif


#define TX             0x01
#define RX             0x02

#define INTMASK_TX     0x01
#define INTMASK_RX     0x02
#define INTMASK_MS     0x08

#define UART1_RTS      (bitPC2)
#define UART1_DTR      (bitPC1)
#define UART1_RI       (bitWSRC1)
#define UART2_RTS      (bitPC4)
#define UART2_CTS      (bitPC3)
#define UART2_DSR      (bitWSRC3)

#define UART_nCTS      (UCTS)
#define UART_nDSR      (UDDR)
#define UART_nDCD      (UDCD)

#define UART1_CTRL_PC6  (bitPC6)
#define UART1_CTRL_PC7  (bitPC7)
#define UART1_CTRL_PC0  (bitPC0)

#if defined(CONFIG_IRIS)
#undef DEBUG_ON_SDBBOARD  /* define this for debugging on SDB board */
#else
#define DEBUG_ON_SDBBOARD  /* define this for debugging on SDB board */
#endif

#ifndef DEBUG_ON_SDBBOARD   /* Iris-1 Target Version */
#define UART_SET_PCDR_LO(bits)  SET_PCDR_HI((bits))
#define UART_SET_PCDR_HI(bits)  SET_PCDR_LO((bits))
#define UART_GET_PCDR(bits)     (~(GET_PCDR((bits))) & (bits))
#define UART_GET_PCINT(bits)    GET_PCINT((bits))
#else /* for debug on L7205 SDB */
#define DBGRTS         0x04
#define DBGDTR         0x08
#define UART_SET_PCDR_LO(bits)  add_debug_port_data( ( ((bits) & UART1_RTS) ? DBGRTS : 0 ) | ( ((bits) & UART1_DTR) ? DBGDTR : 0 ) );
#define UART_SET_PCDR_HI(bits)  del_debug_port_data( ( ((bits) & UART1_RTS) ? DBGRTS : 0 ) | ( ((bits) & UART1_DTR) ? DBGDTR : 0 ) );
#define UART_GET_PCDR(bits)     (((get_debug_port_data() & DBGRTS) ? 0 : ((bits) & UART1_RTS)) | ((get_debug_port_data() & DBGDTR) ? 0 : ((bits) & UART1_DTR)))
#define UART_GET_PCINT(bits)    (0)
#endif

typedef enum {
  ID_UART1
  ,ID_UART2
  /* ,ID_UART3 */ /* not implemened on Iris-1 */
} iris_uart_id;

typedef struct iris_uart_regs {
  __u32 dr;
  __u32 rxstat;
  __u32 h_ubrlcr;
  __u32 m_ubrlcr;
  __u32 l_ubrlcr;
  __u32 uartcon;
  __u32 uartflg;
  __u32 uartintstat;
  __u32 uartintmask;
} iris_uart_regs;

#define WBUF_SIZE 1000

typedef struct iris_serial_driver_struct {
  iris_uart_id uart_id;
  unsigned char bits_rts;
  unsigned char bits_dtr;
  unsigned char bits_cts;
  struct tty_struct *tty;
  int flags;
  wait_queue_head_t delta_msr_wait;
  struct async_icount icount;
  char wbuf[WBUF_SIZE];
  char *putp;
  char *getp;
  int x_char;
  int use_count;
#ifdef USE_SERIAL_BOTTOM_HALF
  struct tq_struct tqueue;
#endif /* USE_SERIAL_BOTTOM_HALF */
} iris_serial_driver_struct;

#ifdef USE_SERIAL_BOTTOM_HALF
struct tq_struct gpio_tqueue;
struct tq_struct ninf_tqueue;
#endif /* USE_SERIAL_BOTTOM_HALF */

static iris_serial_driver_struct uports[SERIAL_IRIS_DRIVER_NUM] = {
  { ID_UART1 , UART1_RTS , UART1_DTR , 0 },
  { ID_UART2 , UART2_RTS , 0 , UART2_CTS }
};

#define SERIAL_GPIO_INT_BITS (UART2_CTS)

extern iris_gsmctl_uart2_gpio_handler();

/* --------------------------------------------------
 *               for internal use
 * -------------------------------------------------- */

static int fpga_wsrc_initialized = 0;
static int fpga_wsrc_dsr_initialized = 0;

static int iris_ring_ringing(void)
{
  int i;
  int val;
  if( ! fpga_wsrc_initialized ){
    DEBUGPRINT(("iris_ring_ringing first time check\n"));
    set_fpga_int_trigger_highlevel(UART1_RI);
    fpga_wsrc_initialized = 1;
  }
  for(i=0;i<10000;i++){
    val = IO_FPGA_RawStat & UART1_RI;
    mdelay(1);
    if( ( IO_FPGA_RawStat & UART1_RI ) == val ) break;
  } 
  if( IO_FPGA_LSEL & UART1_RI ){
    /* Currentry HI level trigger */
    return ( val ? 1 : 0 ); /* HI if Ringing */
  }else{
    /* Currentry LO level trigger , so wsrc is inverted. */
    return ( val ? 0 : 1 ); /* HI if Ringing */
  }
}

static void iris_turn_ring_trigger(void)
{
  if( iris_ring_ringing() ){
    DEBUGPRINT(("RINGING....\n"));
    clear_fpga_int(UART1_RI);
    set_fpga_int_trigger_lowlevel(UART1_RI);
    clear_fpga_int(UART1_RI);
  }else{ /* not ringing */
    DEBUGPRINT(("NOT RINGING....\n"));
    clear_fpga_int(UART1_RI);
    set_fpga_int_trigger_highlevel(UART1_RI);
    clear_fpga_int(UART1_RI);
  }
  fpga_wsrc_initialized = 1;
}

static int iris_dsr2_active(void)
{
  int i;
  int val;
  if( ! fpga_wsrc_dsr_initialized ){
    DEBUGPRINT(("iris_dsr2_active first time check\n"));
    set_fpga_int_trigger_highlevel(UART2_DSR);
    fpga_wsrc_dsr_initialized = 1;
  }
  for(i=0;i<10000;i++){
    val = IO_FPGA_RawStat & UART2_DSR;
    mdelay(1);
    if( ( IO_FPGA_RawStat & UART2_DSR ) == val ) break;
  } 
  if( IO_FPGA_LSEL & UART2_DSR ){
    /* Currentry HI level trigger */
    return ( val ? 0 : 1 ); /* LO if Active */
  }else{
    /* Currentry LO level trigger , so wsrc is inverted. */
    return ( val ? 1 : 0 ); /* LO if Active */
  }
}

static void iris_turn_dsr2_trigger(void)
{
  if( iris_dsr2_active() ){
    DEBUGPRINT(("DSR2 Active...\n"));
    clear_fpga_int(UART2_DSR);
    set_fpga_int_trigger_highlevel(UART2_DSR);
    clear_fpga_int(UART2_DSR);
  }else{ /* not ringing */
    DEBUGPRINT(("DSR2 NOT ACTIVE....\n"));
    clear_fpga_int(UART2_DSR);
    set_fpga_int_trigger_lowlevel(UART2_DSR);
    clear_fpga_int(UART2_DSR);
  }
  fpga_wsrc_dsr_initialized = 1;
}

static void iris_enable_gpio_irq(iris_uart_id which)
{
  unsigned long flags;

  switch(which){
  case ID_UART2:
    /* enable_pc_irq(UART2_CTS); */
#ifndef DSR2_DISABLE
    iris_turn_dsr2_trigger();
    enable_fpga_irq(UART2_DSR);
#endif
    break;
  case ID_UART1:
  default:
    save_flags(flags); cli();
#ifndef RING_DISABLE  /* workaround for RI (SWB16) hardware illness */
    //clear_fpga_int(UART1_RI);
    //set_fpga_int_trigger_highlevel(UART1_RI);
    //clear_fpga_int(UART1_RI);
    iris_turn_ring_trigger();
#endif /* RING_DISABLE *//* workaround for RI (SWB16) hardware illness */
    restore_flags(flags);
#ifndef RING_DISABLE  /* workaround for RI (SWB16) hardware illness */
    enable_fpga_irq(UART1_RI);
#endif /* RING_DISABLE *//* workaround for RI (SWB16) hardware illness */
    break;
  }
}

static void iris_disable_gpio_irq(iris_uart_id which)
{
  switch(which){
  case ID_UART2:
    /* disable_pc_irq(UART2_CTS); */
#ifndef DSR2_DISABLE
    disable_fpga_irq(UART2_DSR);
#endif
    break;
  case ID_UART1:
  default:
#ifndef RING_DISABLE  /* workaround for RI (SWB16) hardware illness */
    disable_fpga_irq(UART1_RI);
#endif /* RING_DISABLE *//* workaround for RI (SWB16) hardware illness */
    break;
  }
}

static void iris_enable_uart_irq(iris_uart_id which,unsigned int mask)
{
  switch(which){
  case ID_UART2:
    IO_UARTINTMASK2 |= mask;
    if ( !(IO_IRQENABLE & (1<<IRQ_U2)) )  /* not yet enabled */
      enable_irq(IRQ_U2);
    break;
  case ID_UART1:
  default:
    IO_UARTINTMASK |= mask;
    if ( !(IO_IRQENABLE & (1<<IRQ_U1)) )  /* not yet enabled */
      enable_irq(IRQ_U1); /* enable whole UART IRQ */
    break;
  }
  return;
}

#define linkup_enable_uart_irq(dir) iris_enable_uart_irq(UART_ID_DEFAULT,(dir))

static void iris_disable_uart_irq(iris_uart_id which,unsigned int mask)
{
  switch(which){
  case ID_UART2:
    IO_UARTINTMASK2 &= ~mask;
    if ( !(IO_UARTINTMASK2 & (INTMASK_TX | INTMASK_RX | INTMASK_MS) ))   /* Both RX/TX unmasked */
      disable_irq(IRQ_U2);  /* disable the whole UART IRQ */
    break;
  case ID_UART1:
  default:
    IO_UARTINTMASK &= ~mask;
    if ( !(IO_UARTINTMASK & (INTMASK_TX | INTMASK_RX | INTMASK_MS) ))   /* Both RX/TX unmasked */
      disable_irq(IRQ_U1);  /* disable the whole UART IRQ */
    break;
  }
  return;
}

#define linkup_disable_uart_irq(dir) iris_disable_uart_irq(UART_ID_DEFAULT,(dir))

/* --------------------------------------------------
 *                  modem status
 * -------------------------------------------------- */

static __inline__ void check_modem_status(iris_serial_driver_struct* info) /* completed ? */
{
  unsigned char mstat;
  unsigned char intstat;
  //unsigned short wsrc;
  struct async_icount *icount;
  int wakeup_needed = 0;

  //DEBUGPRINT(("iris check modem status\n"));

  if( (! info) || info->uart_id != ID_UART1 ) return; /* CTS , DSR , DCD are only supported on UART1 */

  mstat = IO_UARTFLG;
  intstat = IO_UARTINTSTAT;
  //wsrc = IO_FPGA_RawStat;

  if( (intstat & UART_MSINT) ){
    icount = &(info->icount);
    if( (mstat & UART_nDSR) ) icount->dsr++;
    if( (mstat & UART_nCTS) ) icount->cts++;
    if( (mstat & UART_nDCD) ) icount->dcd++;
    //if( ( wsrc & UART1_RI ) ) icount->rng++;
    //wake_up_interruptible((wait_queue_head_t*)&info->delta_msr_wait);
    wakeup_needed = 1;
  }

  if( iris_ring_ringing() ){
    icount = &(info->icount);
    //printk("ringing detected\n");
    icount->rng++;
    wakeup_needed = 1;
  }

  if( wakeup_needed )
    wake_up_interruptible((wait_queue_head_t*)&info->delta_msr_wait);

  if( (info->flags & ASYNC_CHECK_CD) ){
    if( ! (mstat & UART_nDCD) ){
      if( info->tty ) tty_hangup(info->tty);
    }
  }

  if( (info->flags & ASYNC_CTS_FLOW) ){
    if( info->tty ){
      if( info->tty->hw_stopped ){
	if( ! (mstat & UART_nCTS) ){
	  info->tty->hw_stopped = 0;
	  /* may be need to re_sched , WRITE_WAKEUP */
	}
      }else{
	if( ! (mstat & UART_nCTS) ){
	  info->tty->hw_stopped = 1;
	}
      }
    }
  }
}

/* --------------------------------------------------
 *                 startup / shutdown
 * -------------------------------------------------- */

static int startup(iris_serial_driver_struct* info)

{
  unsigned long flags;

  DEBUGPRINT(("iris startup\n"));

  if( ! info ) return(0);


  if( info->tty->termios->c_cflag & CBAUD ){
    save_flags(flags); cli();
#if 1 /* DTR/RTS on UART1 is normalized after IRIS1 tech-sample on 2001/10 */
    UART_SET_PCDR_HI( info->bits_rts | info->bits_dtr );
#else /* before IRIS1 QA3 */
    if( info->uart_id == ID_UART1 ){ /* UART1 DTR/RTS are inverted */
      UART_SET_PCDR_LO( info->bits_rts | info->bits_dtr ); 
    }else{
      UART_SET_PCDR_HI( info->bits_rts | info->bits_dtr );
    }
#endif /* before IRIS1 QA3 */
    restore_flags(flags);
  }

  return(0);
}

static void shutdown(iris_serial_driver_struct* info)
{
  unsigned long flags;

  DEBUGPRINT(("iris shutdown\n"));
  //printk("iris shutdown %d\n",info->uart_id);

  if( ! info ) return;
  wake_up_interruptible((wait_queue_head_t*)&info->delta_msr_wait);

  if( ! info->tty || ( info->tty->termios->c_cflag & HUPCL ) ){
    save_flags(flags); cli();
#if 1 /* DTR/RTS on UART1 is normalized after IRIS1 tech-sample on 2001/10 */
    UART_SET_PCDR_LO( info->bits_rts | info->bits_dtr );
#else /* before IRIS1 QA3 */
    if( info->uart_id == ID_UART1 ){ /* UART1 DTR/RTS are inverted */
      UART_SET_PCDR_HI( info->bits_rts | info->bits_dtr );
    }else{
      UART_SET_PCDR_LO( info->bits_rts | info->bits_dtr );
    }
#endif /* before IRIS1 QA3 */
    restore_flags(flags);
  }

}

/* --------------------------------------------------
 *                  handlers
 * -------------------------------------------------- */


static int rs_write_room(struct tty_struct *tty)
{
  int line;
  iris_serial_driver_struct* info;

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
  iris_serial_driver_struct* info;

  DEBUGPRINT(("iris send xchar\n"));

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];

  info->x_char = ch;
  iris_enable_uart_irq(info->uart_id,INTMASK_TX);
}

static void rs_throttle(struct tty_struct *tty)
{
  int line;
  iris_serial_driver_struct* info;
  unsigned long flags;

  DEBUGPRINT(("iris throttle\n"));

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];

  if(I_IXOFF(tty))
    rs_send_xchar(tty,STOP_CHAR(tty));
  if (tty->termios->c_cflag & CRTSCTS){
    save_flags(flags); cli();
    UART_SET_PCDR_LO( info->bits_rts );
    restore_flags(flags);
  }

}

static void rs_unthrottle(struct tty_struct *tty)
{
  int line;
  iris_serial_driver_struct* info;
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
    UART_SET_PCDR_HI( info->bits_rts );
    restore_flags(flags);
  }
}

static inline int rs_xmit(iris_serial_driver_struct* info,int ch)
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

  iris_enable_uart_irq(info->uart_id,INTMASK_TX);
  return 1;
}

static int rs_write(struct tty_struct *tty,int from_user,
		    const u_char *buf,int count)
{  
  int line;
  int i;
  iris_serial_driver_struct* info;

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
  return i;
} 

static void rs_put_char(struct tty_struct *tty,u_char ch)
{
  int line;
  iris_serial_driver_struct* info;

  DEBUGPRINT(("iris put char\n"));

  if( ! tty ) return;

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];
  rs_xmit(info,ch);
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
  iris_serial_driver_struct* info;

  DEBUGPRINT(("iris flush buffer\n"));

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];


  iris_disable_uart_irq(info->uart_id,INTMASK_TX);

  save_flags(flags); cli();
  info->putp = info->getp = info->wbuf;
  restore_flags(flags);

  //wake_up_interruptible(&tty->write_wait);

  if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP))
      && tty->ldisc.write_wakeup)
    (tty->ldisc.write_wakeup)(tty);

  if(info->x_char)
    iris_enable_uart_irq(info->uart_id,INTMASK_TX);
}


static void rs_flush_chars(struct tty_struct *tty)
{
  //unsigned long flags;
  int line;
  iris_serial_driver_struct* info;

  DEBUGPRINT(("iris flush chars\n"));

  if( ! tty || tty->stopped || tty->hw_stopped ) return;

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];

  //save_flags(flags); cli();
  iris_enable_uart_irq(info->uart_id,INTMASK_TX);
  //restore_flags(flags);
}

static void rs_wait_until_sent(struct tty_struct *tty,int timeout)
{
  int orig_jiffies = jiffies;
  unsigned char flgstat;
  int line;
  iris_serial_driver_struct* info;

  DEBUGPRINT(("iris wait until sent\n"));

  if( ! tty ) return;

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];

  while( ( flgstat = ( info->uart_id == ID_UART2 ? IO_UARTFLG2 : IO_UARTFLG ) ) & UBUSY ){
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
  iris_serial_driver_struct* info;

  DEBUGPRINT(("iris hangup\n"));
  //printk("iris hangup\n");

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];

  rs_flush_buffer(tty);
  shutdown(info);
}

/* --------------------------------------------------
 *               interrupt handlers
 * -------------------------------------------------- */

static __inline__ void receive_chars(iris_serial_driver_struct* info)
{
  unsigned char flgstat;
  struct async_icount *icount;

  DEBUGPRINT(("iris receive chars\n"));

  if( ! info || ! info->tty ) return;

  icount = &(info->icount);

  while( ! ( ( flgstat = ( info->uart_id == ID_UART2 ? IO_UARTFLG2 : IO_UARTFLG ) ) & URXFE ) ){
    char flag = 0;
    unsigned char ch;
    unsigned char st;

    if( info->uart_id == ID_UART2 ){
      ch = IO_UARTDR2;
      st = IO_RXSTAT2;
    }else{
      ch = IO_UARTDR;
      st = IO_RXSTAT;
    }

    icount->rx++;
    if (st & OVERR){
      /* printk("!"); */
      /* entire contents of the FIFO are invalid , and should be cleared */
      tty_insert_flip_char(info->tty,0,TTY_OVERRUN);
      if( info->uart_id == ID_UART2 ){
	while( ! ( ( flgstat = IO_UARTFLG2 ) & URXFE ) ){
	  ch = IO_UARTDR2;
	  st = IO_RXSTAT2;
	}
      }else{
	while( ! ( ( flgstat = IO_UARTFLG ) & URXFE ) ){
	  ch = IO_UARTDR;
	  st = IO_RXSTAT;
	}
      }
      continue;
    }
    if (st & PARERR){
      /* printk("#"); */
      flag = TTY_PARITY;
    }else if (st & FRMERR){
      /* printk("$"); */
      flag = TTY_FRAME;
    }
    tty_insert_flip_char(info->tty,ch & 0xff,flag);
  }
  tty_flip_buffer_push(info->tty);
}

static __inline__ void transmit_chars(iris_serial_driver_struct* info)
{
  unsigned char flgstat;
  struct async_icount *icount;

  //DEBUGPRINT(("iris transmit chars\n"));

  if( ! info ) return;

  icount = &(info->icount);

  while( ! ( ( flgstat = ( info->uart_id == ID_UART2 ? IO_UARTFLG2 : IO_UARTFLG ) ) & UTXFF ) ){
    if( info->x_char ) {
      if( info->uart_id == ID_UART2 ){
	IO_UARTDR2 = info->x_char;
      }else{
	IO_UARTDR = info->x_char;
      }
      info->x_char = 0;
      icount->tx++;
      continue;
    }
    if( (info->putp == info->getp) || info->tty->stopped || info->tty->hw_stopped ){
      iris_disable_uart_irq(info->uart_id,INTMASK_TX);
      break;
    }
    if( info->uart_id == ID_UART2 ){
      IO_UARTDR2 = *(info->getp);
    }else{
      IO_UARTDR = *(info->getp);
    }
    icount->tx++;
    if( ++info->getp >= (info->wbuf+WBUF_SIZE) )
      info->getp = info->wbuf;
  }
  if( info->tty ) {
    wake_up_interruptible((wait_queue_head_t*)&(info->tty->write_wait));
  }
}

static void do_interrupt_handling(void* data)
{
  int irq = (int)data;
  unsigned char intstat;
  iris_serial_driver_struct* info = NULL;

  DEBUGPRINT(("iris int bh\n"));

  if( irq == IRQ_U1 || irq == IRQ_U2 ){
    if( irq == IRQ_U2 ){
      info = &uports[ID_UART2];
      intstat = IO_UARTINTSTAT2;
    }else{ /* IRQ_U1 */
      info = &uports[ID_UART1];
      intstat = IO_UARTINTSTAT;
    }
    if( ! info->tty ){
      if( intstat & UART_RXINT ){
	int ch;
	int st;
	if( info->uart_id == ID_UART2 ){
	  while( ! ( IO_UARTFLG2 & URXFE ) ){
	    ch = IO_UARTDR2;
	    st = IO_RXSTAT2;
	  }
	}else{
	  while( ! ( IO_UARTFLG & URXFE ) ){
	    ch = IO_UARTDR;
	    st = IO_RXSTAT;
	  }
	}
      }
#if 0
      if( intstat & UART_TXINT ){
	if( info->uart_id == ID_UART2 ){
	  IO_UARTDR2 = 0;
	}else{
	  IO_UARTDR = 0;
	}
      }
#endif
      iris_disable_gpio_irq(info->uart_id);
      iris_disable_uart_irq(info->uart_id,INTMASK_RX);
      iris_disable_uart_irq(info->uart_id,INTMASK_MS);
    }else{
      if( intstat & UART_MSINT ){
	check_modem_status(info);
      }
      if( intstat & UART_RXINT ){
	receive_chars(info);
      }
      if( intstat & UART_TXINT ){
	transmit_chars(info);
      }
      if( irq == IRQ_U2 ){
	IO_UARTINTSTAT2 = 0xff; /* clear all */
      }else{ /* IRQ_U1 */
	IO_UARTINTSTAT = 0xff; /* clear all */
      }
    }
  }else if( irq == IRQ_GPIO ){
    /* ========= all GPIO interrupts comes here ! needs to be considered =========== */
    if( ! UART_GET_PCINT(SERIAL_GPIO_INT_BITS) ) return;
    /* --------- Currently , Only UART2-CTS needs to be considered for GPIO Inerrupt ------------ */
    DEBUGPRINT(("uart GPIO\n"));
    info = &(uports[ID_UART2]);
    iris_disable_gpio_irq(info->uart_id);
    if( info && info->tty ) check_modem_status(info);
    iris_enable_gpio_irq(info->uart_id);
  }else if( irq == IRQ_NINF ){ 
    /* --------- Currently , Only UART1-RNG needs to be considered for NINTF Inerrupt ------------ */
    extern int iris_now_suspended;
    if( iris_now_suspended ) return;
    DEBUGPRINT(("uart FPGA\n"));
    if( IO_FPGA_Status & UART1_RI ){
      DEBUGPRINT(("    this is mine\n"));
      info = &(uports[ID_UART1]);
      if( info && info->tty ) check_modem_status(info);
#ifndef RING_DISABLE  /* workaround for RI (SWB16) hardware illness */
      disable_fpga_irq(UART1_RI);
      iris_turn_ring_trigger();
      clear_fpga_int(UART1_RI);
      enable_fpga_irq(UART1_RI);
#endif /* RING_DISABLE *//* workaround for RI (SWB16) hardware illness */
    }
    if( IO_FPGA_Status & UART2_DSR ){
      DEBUGPRINT(("    this is mine 2\n"));
      info = &(uports[ID_UART2]);
      if( info && info->tty ) check_modem_status(info);
      iris_gsmctl_uart2_gpio_handler();
#ifndef DSR2_DISABLE
      disable_fpga_irq(UART2_DSR);
      iris_turn_dsr2_trigger();
      clear_fpga_int(UART2_DSR);
      enable_fpga_irq(UART2_DSR);
#endif
    }
  }
}

#ifdef USE_SERIAL_BOTTOM_HALF
static void do_serial_bh(void)
{
  DEBUGPRINT(("serial bh"));
  run_task_queue(&tq_serial);
}
#endif /* USE_SERIAL_BOTTOM_HALF */

static void rs_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  DEBUGPRINT(("iris int\n"));

#ifdef USE_SERIAL_BOTTOM_HALF
  if( irq == IRQ_U1 ){
    queue_task(&(uports[ID_UART1].tqueue),&tq_serial);
  }else if( irq == IRQ_U2 ){
    queue_task(&(uports[ID_UART2].tqueue),&tq_serial);
  }else if( irq == IRQ_GPIO ){
    queue_task(&(gpio_tqueue),&tq_immediate);
  }else if( irq == IRQ_NINF ){ 
    queue_task(&(ninf_tqueue),&tq_immediate);
  }
  mark_bh(SERIAL_BH);
#else /* USE_SERIAL_BOTTOM_HALF */
  do_interrupt_handling((void*)irq);
#endif /* USE_SERIAL_BOTTOM_HALF */
}

/* --------------------------------------------------
 *             start / stop
 * -------------------------------------------------- */

static void rs_stop(struct tty_struct *tty) /* completed ? */
{
  int line;
  iris_serial_driver_struct* info;

  DEBUGPRINT(("iris stop\n"));

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];
  iris_disable_uart_irq(info->uart_id,INTMASK_TX);
}

static void rs_start(struct tty_struct *tty) /* completed ? */
{
  int line;
  iris_serial_driver_struct* info;

  DEBUGPRINT(("iris start\n"));

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];
  iris_enable_uart_irq(info->uart_id,INTMASK_TX);
}

/* --------------------------------------------------
 *             open / close
 * -------------------------------------------------- */

static int rs_open(struct tty_struct *tty, struct file *filp)
{
  int line;
  int retval;
  iris_serial_driver_struct* info;

  line = MINOR(tty->device) - tty->driver.minor_start;

  DEBUGPRINT(("iris open %d\n",line));

  if( ( line < 0 ) || ( line >= SERIAL_IRIS_DRIVER_NUM ) ){
    printk("rs_open: No such device! major=%d minor=%d\n", MAJOR(tty->device),MINOR(tty->device));
    return -ENODEV;
  }

  info = &(uports[line]);
  tty->driver_data = NULL;
  info->tty = tty;
  info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

  retval = startup(info);
  if( retval ) return retval;

  if( info->uart_id == ID_UART2 ){
    IO_UARTCON2 |= UARTEN;
  }else{
    IO_UARTCON |= UARTEN;
  }

  iris_enable_uart_irq(info->uart_id,INTMASK_RX);
  iris_enable_uart_irq(info->uart_id,INTMASK_MS);

  iris_enable_gpio_irq(info->uart_id);

  info->use_count++;

#ifdef SERIAL_IRIS_DEBUG
  /* =========== for debug ========== */
  switch( info->uart_id ){
  case ID_UART2:
    printk("serial iris open : UART2\n");
    printk("LCR H:%x M:%x L:%d\n",IO_H_UBRLCR2,IO_M_UBRLCR2,IO_L_UBRLCR2);
    printk("CON:%x FLG:%x MASK:%x\n",IO_UARTCON2,IO_UARTFLG2,IO_UARTINTMASK2);
    break;
  case ID_UART1:
  default:
    printk("serial iris open : UART1\n");
    printk("LCR H:%x M:%x L:%d\n",IO_H_UBRLCR,IO_M_UBRLCR,IO_L_UBRLCR);
    printk("CON:%x FLG:%x MASK:%x\n",IO_UARTCON,IO_UARTFLG,IO_UARTINTMASK);
    break;
  }
#endif

  return 0;
}

static void rs_close(struct tty_struct *tty, struct file *filp)
{
  int line;
  //unsigned long flags;
  iris_serial_driver_struct* info;

  line = MINOR(tty->device) - tty->driver.minor_start;
  info = &uports[line];

  DEBUGPRINT(("iris close %d\n",line));

  //save_flags(flags); cli();
  tty->closing = 1;
  if( ! --info->use_count ){
    rs_wait_until_sent(tty,0);
    iris_disable_uart_irq(info->uart_id,INTMASK_RX);
    iris_disable_uart_irq(info->uart_id,INTMASK_MS);
    iris_disable_uart_irq(info->uart_id,INTMASK_TX);
    iris_disable_gpio_irq(info->uart_id);
    //printk("rs_close shutdown\n");
    shutdown(info);
    if (tty->driver.flush_buffer){
      tty->driver.flush_buffer(tty);
    }
    if (tty->ldisc.flush_buffer){
      tty->ldisc.flush_buffer(tty);
    }
    if( info->uart_id == ID_UART2 ){
      IO_UARTCON2 &= ~UARTEN;
    }else{
      IO_UARTCON &= ~UARTEN;
    }
  }
  tty->closing = 0;
  //restore_flags(flags);
}

/* --------------------------------------------------
 *                    termios
 * -------------------------------------------------- */

static inline void rs_set_cflag(iris_serial_driver_struct* info,int cflag) /* completed ? */
{
  int cr_h, cr_l;

  switch(cflag & CSIZE) {
  case CS5:
    cr_h = WL_5;
    break;
  case CS6:
    cr_h = WL_6;
    break;
  case CS7:
    cr_h=WL_7;
    break;
  case CS8:
  default:
    cr_h = WL_8;
    break;
  }
  cr_h <<= WRDLEN_SHIFT;

  if( cflag & CSTOPB ) cr_h |= XSTOP;
  if( cflag & PARENB ) cr_h |= PRTEN;
  if( ! ( cflag & PARODD ) ) cr_h |= EVENPRT;

  cr_h |= FIFOEN;

  switch(cflag & CBAUD) {
  case B1200:
    cr_l = BR_1200;
    break;
  case B2400:
    cr_l = BR_2400;
    break;
  case B4800:
    cr_l = BR_4800;
    break;
  case B19200:
    cr_l = BR_19200;
    break;
  case B38400:
    cr_l = BR_38400;
    break;
  case B57600:
    cr_l = BR_57600;
    break;
  case B115200:
    cr_l = BR_115200;
    break;
  default:
    cr_l = BR_9600;
  }
  switch( info->uart_id ){
  case ID_UART2:
    IO_L_UBRLCR2 = cr_l & 0xFF;
    IO_H_UBRLCR2 = cr_h & 0xFF;
    break;
  case ID_UART1:
  default:
    IO_L_UBRLCR = cr_l & 0xFF;
    IO_H_UBRLCR = cr_h & 0xFF;
    break;
  }
#ifdef SERIAL_IRIS_DEBUG
  /* =========== for debug ========== */
  switch( info->uart_id ){
  case ID_UART2:
    printk("serial iris cflag : UART2\n");
    printk("LCR H:%x M:%x L:%d\n",IO_H_UBRLCR2,IO_M_UBRLCR2,IO_L_UBRLCR2);
    printk("CON:%x FLG:%x MASK:%x\n",IO_UARTCON2,IO_UARTFLG2,IO_UARTINTMASK2);
    break;
  case ID_UART1:
  default:
    printk("serial iris cflag : UART1\n");
    printk("LCR H:%x M:%x L:%d\n",IO_H_UBRLCR,IO_M_UBRLCR,IO_L_UBRLCR);
    printk("CON:%x FLG:%x MASK:%x\n",IO_UARTCON,IO_UARTFLG,IO_UARTINTMASK);
    break;
  }
#endif
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old)
{
  unsigned long flags;
  unsigned int cflag;
  int line;
  iris_serial_driver_struct* info;

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
    UART_SET_PCDR_LO( info->bits_rts | info->bits_dtr );
    restore_flags(flags);
  }
  /* Handle transition away from B0 status */
  if ( ! (old->c_cflag & CBAUD) && (tty->termios->c_cflag & CBAUD) ){
    save_flags(flags); cli();
    UART_SET_PCDR_HI( info->bits_dtr );
    restore_flags(flags);
    if( ! (tty->termios->c_cflag & CRTSCTS)
	|| ! test_bit(TTY_THROTTLED, &tty->flags)) {
      save_flags(flags); cli();
      UART_SET_PCDR_HI( info->bits_rts );
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

static int get_modem_info(iris_serial_driver_struct* info,unsigned int* value) /* completed ? */
{
  unsigned int result = 0;
  unsigned char mstat;
  unsigned char gpio_pc;
  //unsigned long flags;
  unsigned short wsrc;

  DEBUGPRINT(("iris getmodeminfo\n"));

  switch(info->uart_id){
  case ID_UART2:
    //save_flags(flags); cli();
    gpio_pc = UART_GET_PCDR(UART2_RTS|UART2_CTS);
    //restore_flags(flags);
    result = 
      (( gpio_pc & UART2_RTS ) ? TIOCM_RTS : 0)
      | (( iris_dsr2_active() ) ? TIOCM_DSR : 0)
      | (( gpio_pc & UART2_CTS ) ? TIOCM_CTS : 0);
    break;
  case ID_UART1:
  default:
    //save_flags(flags); cli();
    gpio_pc = UART_GET_PCDR(UART1_RTS|UART1_DTR);
    mstat = IO_UARTFLG;
    wsrc = IO_FPGA_RawStat;
    //printk("uart1 gpio(%x) ms(%x) wsrc(%x)\n",gpio_pc,mstat,wsrc);
    //printk("uart1 fpga EEN(%x) , ESEL(%x) , DE(%x) , LSEL(%x) , ECL(%x) , ENABLE(%x) , RAWSTAT(%x) , STATUS(%x) , OUTLEVEL(%x)\n",IO_FPGA_EEN,IO_FPGA_ESEL,IO_FPGA_DE,IO_FPGA_LSEL,IO_FPGA_ECL,IO_FPGA_Enable,IO_FPGA_RawStat,IO_FPGA_Status,IO_FPGA_OutLevel);
    //restore_flags(flags);
    result = 
      (( gpio_pc & UART1_RTS ) ? TIOCM_RTS : 0)
      | (( gpio_pc & UART1_DTR ) ? TIOCM_DTR : 0)
      | (( mstat & UART_nCTS ) ? TIOCM_CTS : 0)
      | (( mstat & UART_nDSR ) ? TIOCM_DSR : 0)
      | (( mstat & UART_nDCD ) ? TIOCM_CAR : 0)
      //| (( wsrc & UART1_RI ) ? TIOCM_RNG : 0);
      | ( iris_ring_ringing() ? TIOCM_RNG : 0);
    break;
  }
  return put_user(result,value);
}

static int set_modem_info(iris_serial_driver_struct* info, unsigned int cmd,
			  unsigned int *value)  /* completed ? */
{
  int error;
  unsigned int arg;
  unsigned long flags;

  DEBUGPRINT(("iris setmodeminfo\n"));
  //printk("iris shutdown\n");

  error = get_user(arg,value);
  if( error ) return error;

  switch(cmd){
  case TIOCMBIS:
    if(arg & TIOCM_RTS){
      save_flags(flags); cli();
      UART_SET_PCDR_HI( info->bits_rts );
      //printk("PCDR= %lx\n",IO_PCDR);
      restore_flags(flags);
    }
    if(arg & TIOCM_DTR){
      save_flags(flags); cli();
      UART_SET_PCDR_HI( info->bits_dtr );
      restore_flags(flags);
    }
    break;
  case TIOCMBIC:
    if(arg & TIOCM_RTS){
      save_flags(flags); cli();
      UART_SET_PCDR_LO( info->bits_rts );
      //printk("PCDR= %lx\n",IO_PCDR);
      restore_flags(flags);
    }
    if(arg & TIOCM_DTR){
      save_flags(flags); cli();
      UART_SET_PCDR_LO( info->bits_dtr );
      restore_flags(flags);
    }
    break;
  case TIOCMSET:
    if( arg & TIOCM_RTS ){
      save_flags(flags); cli();
      UART_SET_PCDR_HI( info->bits_rts );
      restore_flags(flags);
    }else{
      save_flags(flags); cli();
      UART_SET_PCDR_LO( info->bits_rts);
      restore_flags(flags);
    }
    if( arg & TIOCM_DTR ){
      save_flags(flags); cli();
      UART_SET_PCDR_HI( info->bits_dtr );
      restore_flags(flags);
    }else{ 
      save_flags(flags); cli();
      UART_SET_PCDR_LO( info->bits_dtr );
      restore_flags(flags);
    }
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

static int get_gsm_modem_info(iris_serial_driver_struct* info,unsigned int cmd,unsigned int* value) /* completed ? */
{
  unsigned char gpio_pc;
  unsigned int result = 0;
  DEBUGPRINT(("iris get gsm modem\n"));

  /* only supported for UART1 */
  if( (! info) || info->uart_id != ID_UART1 ) return -EINVAL;

  gpio_pc = UART_GET_PCDR(UART1_CTRL_PC6|UART1_CTRL_PC7|UART1_CTRL_PC0);
  switch(cmd){
  case GSM_IOCTL_CODE_GET_PC6:
    result = ( gpio_pc & UART1_CTRL_PC6 ? 0 : 1 );
    break;
  case GSM_IOCTL_CODE_GET_PC7:
    result = ( gpio_pc & UART1_CTRL_PC7 ? 0 : 1 );
    break;
  case GSM_IOCTL_CODE_GET_PC0:
    result = ( gpio_pc & UART1_CTRL_PC0 ? 0 : 1 );
    break;
  default:
    return -EINVAL;
  }
  return put_user(result,value);
}

static int set_gsm_modem_info(iris_serial_driver_struct* info, unsigned int cmd,
			      unsigned int *value)
{
  int error;
  unsigned int arg;
  unsigned long flags;

  DEBUGPRINT(("iris set gsm modem\n"));
  //printk("iris set gsm modem\n");

  /* only supported for UART1 */
  if( (! info) || info->uart_id != ID_UART1 ) return -EINVAL;

  error = get_user(arg,value);
  if( error ) return error;

  switch(cmd){
  case GSM_IOCTL_CODE_SET_PC6:
    save_flags(flags); cli();
    if( arg == 1 ){
      //printk("IOCTL_SET_PC6 1\n");
      UART_SET_PCDR_LO(UART1_CTRL_PC6);
    }else{
      //printk("IOCTL_SET_PC6 0\n");
      UART_SET_PCDR_HI(UART1_CTRL_PC6);
    }
    restore_flags(flags);
    break;
  case GSM_IOCTL_CODE_SET_PC7:
    save_flags(flags); cli();
    if( arg == 1 ){
      //printk("IOCTL_SET_PC7 1\n");
      UART_SET_PCDR_LO(UART1_CTRL_PC7);
    }else{
      //printk("IOCTL_SET_PC7 0\n");
      UART_SET_PCDR_HI(UART1_CTRL_PC7);
    }
    restore_flags(flags);
    break;
#if 1 /* PC0 becomes INPUT-PORT on IRIS1-tech-sample on 2001/10 */
  case GSM_IOCTL_CODE_SET_PC0:
    save_flags(flags); cli();
    if( arg == 1 ){
      printk("IOCTL_SET_PC0 1\n");
      UART_SET_PCDR_LO(UART1_CTRL_PC0);
    }else{
      printk("IOCTL_SET_PC7 0\n");
      UART_SET_PCDR_HI(UART1_CTRL_PC0);
    }
    restore_flags(flags);
    break;
#endif
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
  iris_serial_driver_struct* info;

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
  case GSM_IOCTL_CODE_GET_PC6:
  case GSM_IOCTL_CODE_GET_PC7:
  case GSM_IOCTL_CODE_GET_PC0:
    return get_gsm_modem_info(info,cmd,(unsigned int*)arg);
    break;
  case GSM_IOCTL_CODE_SET_PC6:
  case GSM_IOCTL_CODE_SET_PC7:
#if 0 /* PC0 becomes INPUT-PORT on IRIS1-tech-sample on 2001/10 */
  case GSM_IOCTL_CODE_SET_PC0:
#endif
    return set_gsm_modem_info(info,cmd,(unsigned int*)arg);
    break;
  default:
    return -ENOIOCTLCMD;
  }
  return 0;
}

#ifdef CONFIG_PM

int iris_gsm_modem_ctrl_pm_callback(struct pm_dev *pm_dev,
				    pm_request_t req, void *data)
{
  static int suspended = 0;
  static int save_pc6 = 0;
  static int save_pc7 = 0;
#if 0 /* PC0 becomes INPUT-PORT on IRIS1-tech-sample on 2001/10 */
  static int save_pc0 = 0;
#endif
  switch (req) {
  case PM_BLANK:
  case PM_STANDBY:
  case PM_SUSPEND:
    if( suspended ) break;
    {
      //printk("gsm PM_SUSPEND\n");
      /* set Standby Register , following Current Status */
      if( GET_PCDR(UART1_CTRL_PC6) ){
	//printk("set PC6\n");
	SET_PCSBSR_HI(UART1_CTRL_PC6);
	save_pc6 = 1;
      }else{
	//printk("unset PC6\n");
	SET_PCSBSR_LO(UART1_CTRL_PC6);
	save_pc6 = 0;
      }
      if( GET_PCDR(UART1_CTRL_PC7) ){
	//printk("set PC7\n");
	SET_PCSBSR_HI(UART1_CTRL_PC7);
	save_pc7 = 1;
      }else{
	//printk("unset PC6\n");
	SET_PCSBSR_LO(UART1_CTRL_PC7);
	save_pc7 = 0;
      }
#if 0 /* PC0 becomes INPUT-PORT on IRIS1-tech-sample on 2001/10 */
      if( GET_PCDR(UART1_CTRL_PC0) ){
	//printk("set PC0\n");
	SET_PCSBSR_HI(UART1_CTRL_PC0);
	save_pc0 = 1;
      }else{
	//printk("set PC0\n");
	SET_PCSBSR_LO(UART1_CTRL_PC0);
	save_pc0 = 0;
      }
#endif
    }
    suspended = 1;
    break;
  case PM_UNBLANK:
  case PM_RESUME:
    if( ! suspended ) break;
    {
      //printk("gsm PM_RESUME\n");
      /* set Standby Register LO , to Force to be LO at Emergency Suspend */
#if 1 /* PC0 becomes INPUT-PORT on IRIS1-tech-sample on 2001/10 */
      SET_PCSBSR_LO(UART1_CTRL_PC6|UART1_CTRL_PC7);
#else /* before IRIS1 QA3 */
      SET_PCSBSR_LO(UART1_CTRL_PC6|UART1_CTRL_PC7|UART1_CTRL_PC0);
#endif
      if( save_pc6 ){
	SET_PCDR_HI(UART1_CTRL_PC6);
      }else{
	SET_PCDR_LO(UART1_CTRL_PC6);
      }
      if( save_pc7 ){
	SET_PCDR_HI(UART1_CTRL_PC7);
      }else{
	SET_PCDR_LO(UART1_CTRL_PC7);
      }
#if 0 /* PC0 becomes INPUT-PORT on IRIS1-tech-sample on 2001/10 */
      if( save_pc0 ){
	SET_PCDR_HI(UART1_CTRL_PC0);
      }else{
	SET_PCDR_LO(UART1_CTRL_PC0);
      }
#endif
    }
    suspended = 0;
    break;
  }
  return 0;
}

static int iris_rs_pm_callback(struct pm_dev *pm_dev,
			       pm_request_t req, void *data)
{
  static int suspended = 0;
  static int save_u1_rts = 0;
  static int save_u1_dtr = 0;
  static int save_u2_rts = 0;
  switch (req) {
  case PM_STANDBY:
  case PM_SUSPEND:
    if( suspended ) break;
    {
      //printk("ser PM_SUSPEND\n");
      if( GET_PCDR(UART1_RTS) ){
	//printk("set UART1_RTS\n");
	SET_PCSBSR_HI(UART1_RTS);
	save_u1_rts = 1;
      }else{
	//printk("unset UART1_RTS\n");
	SET_PCSBSR_LO(UART1_RTS);
	save_u1_rts = 0;
      }
      if( GET_PCDR(UART1_DTR) ){
	//printk("set UART1_DTR\n");
	SET_PCSBSR_HI(UART1_DTR);
	save_u1_dtr = 1;
      }else{
	//printk("unset UART1_DTR\n");
	SET_PCSBSR_LO(UART1_DTR);
	save_u1_dtr = 0;
      }
      if( GET_PCDR(UART2_RTS) ){
	//printk("set UART2_RTS\n");
	SET_PCSBSR_HI(UART2_RTS);
	save_u2_rts = 1;
      }else{
	//printk("unset UART2_RTS\n");
	SET_PCSBSR_LO(UART2_RTS);
	save_u2_rts = 0;
      }
    }
    suspended = 1;
    break;
  case PM_RESUME:
    if( ! suspended ) break;
    {
      //printk("ser PM_RESUME\n");
      /* UART1/2 RTS/DTR are Activated if LO */
#if 1 /* DTR/RTS on UART1 is normalized after IRIS1 tech-sample on 2001/10 */
      SET_PCSBSR_HI(UART1_RTS|UART1_DTR); /* UART1 DTR/RTS are inverted */
      SET_PCSBSR_HI(UART2_RTS);
#else /* before IRIS1 QA3 */
      SET_PCSBSR_LO(UART1_RTS|UART1_DTR); /* UART1 DTR/RTS are inverted */
      SET_PCSBSR_HI(UART2_RTS);
#endif /* before IRIS1 QA3 */
      if( save_u1_rts ){
	SET_PCDR_HI(UART1_RTS);
      }else{
	SET_PCDR_LO(UART1_RTS);
      }
      if( save_u1_dtr ){
	SET_PCDR_HI(UART1_DTR);
      }else{
	SET_PCDR_LO(UART1_DTR);
      }
      if( save_u2_rts ){
	SET_PCDR_HI(UART2_RTS);
      }else{
	SET_PCDR_LO(UART2_RTS);
      }
    }
    suspended = 0;
    break;
  }
  iris_gsm_modem_ctrl_pm_callback(pm_dev,req,data);
  return 0;
}
#endif

/* --------------------------------------------------
 *             initialize function
 * -------------------------------------------------- */

static int __init iris_rs_init(void)
{ 
  //unsigned long flags;

  /* ================ setup all ports ============= */
  {
    int i;
    iris_serial_driver_struct* info;
    for(i=0;i<SERIAL_IRIS_DRIVER_NUM;i++){
      info = &(uports[i]);
      init_waitqueue_head((wait_queue_head_t*)&info->delta_msr_wait);
      info->putp = info->wbuf;
      info->getp = info->wbuf;
#ifdef USE_SERIAL_BOTTOM_HALF
      info->tqueue.sync = 0;
      info->tqueue.routine = do_interrupt_handling;
#endif /* USE_SERIAL_BOTTOM_HALF */
#if 1 /* DTR/RTS on UART1 is normalized after IRIS1 tech-sample on 2001/10 */
      UART_SET_PCDR_LO( info->bits_rts | info->bits_dtr ); 
#else /* before IRIS1 QA3 */
      if( info->uart_id == ID_UART1 ){ /* UART1 DTR/RTS are inverted */
	UART_SET_PCDR_HI( info->bits_rts | info->bits_dtr ); 
      }else{
	UART_SET_PCDR_LO( info->bits_rts | info->bits_dtr ); 
      }
#endif /* before IRIS1 QA3 */
      rs_set_cflag(info,CS8 | B9600);
    }
#ifdef USE_SERIAL_BOTTOM_HALF
    uports[ID_UART1].tqueue.data = (void*)IRQ_U1;
    uports[ID_UART2].tqueue.data = (void*)IRQ_U2;
    gpio_tqueue.sync = 0;
    ninf_tqueue.sync = 0;
    gpio_tqueue.routine = do_interrupt_handling;
    ninf_tqueue.routine = do_interrupt_handling;
    gpio_tqueue.data = (void*)IRQ_GPIO;
    ninf_tqueue.data = (void*)IRQ_NINF;
    init_bh(SERIAL_BH,do_serial_bh);
#endif /* USE_SERIAL_BOTTOM_HALF */

  }
  /* ================= end ==================== */

  memset(&rs_driver,0,sizeof(rs_driver));
  rs_driver.magic = TTY_DRIVER_MAGIC;
  rs_driver.driver_name = "serial_linkup7200";
  rs_driver.name = "ttyLU"; 
  rs_driver.major = SERIAL_L7200_MAJOR;
  rs_driver.minor_start = SERIAL_IRIS_MINOR_START;
  rs_driver.num = SERIAL_IRIS_DRIVER_NUM;
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
  callout_driver.name  =  "cuaLU";
  callout_driver.major  =  SERIAL_L7200_AUX_MAJOR;
  callout_driver.subtype  =  SERIAL_TYPE_CALLOUT;
  callout_driver.read_proc  =  NULL;
  callout_driver.proc_entry  =  NULL;

  if(request_irq(IRQ_U1,rs_interrupt,SA_INTERRUPT,"rs_linkup_irq1",NULL))
    panic("Couldn't get irq for rs1_linkup"); 

  if(request_irq(IRQ_U2,rs_interrupt,SA_INTERRUPT,"rs_linkup_irq2",NULL))
    panic("Couldn't get irq for rs2_linkup"); 

#ifndef DEBUG_ON_SDBBOARD   /* Iris-1 Target Version */
  if(request_irq(IRQ_GPIO,rs_interrupt,SA_SHIRQ,"rs_linkup_gpio",rs_interrupt))
    panic("Couldn't get irq for gpio_linkup"); 
#ifndef RING_DISABLE  /* workaround for RI (SWB16) hardware illness */
  if(request_irq(IRQ_NINF,rs_interrupt,SA_SHIRQ,"rs_linkup_wsrc",rs_interrupt))
    panic("Couldn't get irq for wsrc_linkup");
#endif /* RING_DISABLE *//* workaround for RI (SWB16) hardware illness */
#endif /* DEBUG_ON_SDBBOARD */

  if(tty_register_driver(&rs_driver))
    panic("Couldn't register LINKUP-7200 serial driver\n");
  if(tty_register_driver(&callout_driver))
    panic("Couldn't register LINKUP-7200 callout driver\n");

  DEBUGPRINT(("rs_iris_init: LINKUP-7200 IRIS serial driver registered! initial 9600bps\n"));

  printk("IRIS L7200 serial driver registered.\n");

#ifdef CONFIG_PM
  iris_rs_pm_dev = pm_register(PM_SYS_DEV, 0, iris_rs_pm_callback);
#endif
    
  /* setup control lines */

  //save_flags(flags); cli();
  SET_PC_OUT(UART2_RTS|UART1_RTS|UART1_DTR);
  SET_PC_IN(UART2_CTS);
  SET_PCSBSR_HI(UART1_RTS|UART1_DTR|UART2_RTS);
  SET_PC_OUT(UART1_CTRL_PC6|UART1_CTRL_PC7);
#if 1 /* PC0 becomes INPUT-PORT on IRIS1-tech-sample on 2001/10 */
  SET_PC_IN(UART1_CTRL_PC0);
  disable_pc_irq(UART1_CTRL_PC0);
#endif
#ifndef RING_DISABLE  /* workaround for RI (SWB16) hardware illness */
  //set_fpga_int_trigger_highlevel(UART1_RI);
  clear_fpga_int(UART1_RI);
  iris_turn_ring_trigger();
  clear_fpga_int(UART1_RI);
#endif /* RING_DISABLE *//* workaround for RI (SWB16) hardware illness */
  //restore_flags(flags);

  return 0;
}

module_init(iris_rs_init);

#ifdef CONFIG_SERIAL_IRIS_CONSOLE

/* --------------------------------------------------
 *             console driver
 * -------------------------------------------------- */

void rs_console_write(struct console *co, const char *s,u_int count)
{
  int i;

  DEBUGPRINT(("rs_console_write: s=%s,count=%d\n", s,count));
  linkup_disable_uart_irq(TX);
  for(i=0;i<count;i++) {
    while(IRIS_UARTFLG & UTXFF);
    IRIS_UARTDR=s[i];
    if(s[i]=='\n') {
      while(IRIS_UARTFLG & UTXFF);
      IRIS_UARTDR='\r';
    }
  }
  linkup_enable_uart_irq(TX);
}

static int rs_console_wait_key(struct console *co)
{ 
  int c;

  DEBUGPRINT(("rs_console_wait_key: \n"));
  linkup_disable_uart_irq(RX);
  while(IRIS_UARTFLG & URXFE);
  c=IRIS_UARTDR;
  linkup_enable_uart_irq(RX);
  return c;
}

static kdev_t rs_console_device(struct console *c)
{
  return MKDEV(TTY_MAJOR, SERIAL_MINOR);
}

static int __init rs_console_setup(struct console *co, char *options)
{
  int	baud = 9600;
  int	bits = 8;
  int	parity = 'n';
  int	cflag = CREAD | HUPCL | CLOCAL;

  DEBUGPRINT(("rs_console_setup: options=%s\n", options));
#if 0
  if(0 && options) {
    char	*s=options;
    baud = simple_strtoul(options, NULL, 10);
    while(*s >= '0' && *s <= '9')
      s++;
    if (*s) parity = *s++;
    if (*s) bits   = *s - '0';
  }
#endif

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
  rs_set_cflag(&(uports[UART_ID_DEFAULT]),cflag);
  IRIS_UARTCON |= UARTEN;
  rs_console_write(NULL,"boot ",5);
  if(options)
    rs_console_write(NULL,options,strlen(options));
  else
    rs_console_write(NULL,"no options",10);
  rs_console_write(NULL,"\n",1);

  return 0;
}

static struct console rs_cons = {
  name:     "ttyI",
  write:    rs_console_write,
  device:   rs_console_device,
  wait_key: rs_console_wait_key,
  setup:    rs_console_setup,
  flags:    CON_PRINTBUFFER,
  index:    -1,
};

void __init iris_serial_console_init(void)
{
  register_console(&rs_cons);
}

#endif /* CONFIG_SERIAL_IRIS_CONSOLE */

/* =======================================================
 *  end of source
 * ======================================================= */

