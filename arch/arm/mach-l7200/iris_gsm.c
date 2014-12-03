/*
 *  linux/drivers/char/iris_gsm.c
 *
 *  Driver for GSM I/O Ports On SHARP PDA Iris
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
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
#include <linux/poll.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>
#include <asm/arch/gpio.h>

#include <asm/arch/fpga.h>

#define DEBUGPRINT(s)   /* printk s */

/* Currently , head phone microphone is connected to GPIO PB4. But GPIO Port B
   doesn't have Edge-Detect Interrupt Logic . And LinkUp GPIO register has no
   Lower-Level Interrupt. So , we cannot get interrupt when head phone microphone
   is removed ... */
#define HEADPHONEMIC_IRQ_AVAIL

/* Carkit GPIO is deleted , and it means Carkit is ON that USB(GA WSRC4) and
   UART2(GA WSRC3) signals are both ON....  */
#define CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART

#ifdef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART
/* If USB-Detect Interrupt Handling Needs To Be Here , define this ... */
#define NEED_USB_DETECT_IRQ_HERE
#endif

/* Iris1 QA4 or later , PA6 is used to identify the type of inserted equipment */
#define HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW


/*
 * GPIOs...
 */

#ifdef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART
#define CARKIT_DETECT  (bitPA6)
#endif /* CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */
#define HEADPHONEMIC   (bitPB4)

#ifdef HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW
#define JACKTYPE   (bitPA6)
#endif /* HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW */


/*
 * operations....
 */

extern void gsmcarkit_status_changed(void);


#include <asm/sharp_keycode.h>
#include <asm/arch/keyboard.h>

static int put_event_as_keycode(int keycode)
{
  handle_scancode(KBSCANCDE(keycode,KBDOWN),1);
  handle_scancode(KBSCANCDE(keycode,KBUP),0);
  return 0;
}

static int carkit_status_changed(int on)
{
  //printk("carkit_status_changed %d\n",on);
  put_event_as_keycode(SLKEY_CARKIT);
  //printk("putted keycode\n",on);
  gsmcarkit_status_changed();
  //printk("carkit status changed done\n",on);
  return 0;
}

static int synchro_status_changed(int on,int is_usb)
{
  //printk("synchro status_changed %d %d\n",on,is_usb);
  if( on )  put_event_as_keycode(SLKEY_SYNCSTART);
  return 0;
}


#ifdef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART

#define USB_GA_BIT  (bitWSRC4)
#define UART_GA_BIT  (bitWSRC3)

static int check_ga(int bit)
{
  if( IO_FPGA_LSEL & bit ){
    /* Currentry HI level trigger , So 1->1 convert */
    return ( ( IO_FPGA_RawStat & bit ) ? 1 : 0 );
  }else{
    /* Currentry LO level trigger , so wsrc is inverted. */
    return ( ( IO_FPGA_RawStat & bit ) ? 0 : 1 );
  }
}

#undef USBPWR_INVERTED_ON_TARGET /* define , if use on Target */

int iris_read_usb_sync(void)
{
  int val1 = 0;
  int val2 = 0;
  int i;
  for(i=0;i<10000;i++){
    val1 = check_ga(USB_GA_BIT);
    mdelay(1);
    val2 = check_ga(USB_GA_BIT);
    if( val1 == val2 ) break;
  }
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
  return ( val1 ? 1 : 0 );
#else /* ! USBPWR_INVERTED_ON_TARGET */
  return ( val1 ? 0 : 1 );
#endif /* ! USBPWR_INVERTED_ON_TARGET */
}

int iris_read_uart_sync(void)
{
  int val1 = 0;
  int val2 = 0;
  int i;
  for(i=0;i<10000;i++){
    val1 = check_ga(UART_GA_BIT);
    mdelay(1);
    val2 = check_ga(UART_GA_BIT);
    if( val1 == val2 ) break;
  }
  return ( val1 ? 1 : 0 );
}

int iris_read_carkit_val(void)
{
  int usb = 0;
  int uart = 0;
  usb = iris_read_usb_sync();
  uart = iris_read_uart_sync();
  return ( ( usb && uart ) ? 1 : 0 );
}

#define IS_CARKIT_EXISTS(usb,uart)   ((usb) && (uart))

#ifdef NEED_USB_DETECT_IRQ_HERE

void turn_usb_ga_irq(void)
{
  if( check_ga(USB_GA_BIT) ){
    /* Currently USB Inserted(=HI) , So , LO Next. */
    set_fpga_int_trigger_lowlevel(USB_GA_BIT);
  }else{
    /* Currently USB Not-inserted(=LO) , So , HI Next. */
    set_fpga_int_trigger_highlevel(USB_GA_BIT);
  }
}

#endif

#else /* ! CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART ---- OLD ! */
int iris_read_carkit_val(void)
{
  int val = 0;
  int i;
  for(i=0;i<10000;i++){
    val = GET_PADR(CARKIT_DETECT);
    mdelay(10);
    if( val == GET_PADR(CARKIT_DETECT) ) break;
  }
  return ( val ? 1 : 0 );
}
#endif /* ! CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART ---- OLD ! */

int iris_read_headphonemic_status(void)
{
  int val = 0;
  int i;
  int jacktype = 0;
  for(i=0;i<10000;i++){
    val = GET_PBDR(HEADPHONEMIC);
    jacktype = GET_PADR(JACKTYPE);
    mdelay(10);
    if( val == GET_PBDR(HEADPHONEMIC) ) break;
  }
  if( ! val ) return( IRIS_AUDIO_EXT_IS_NONE );
#ifndef HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW /* OLD ver. */
  return IRIS_AUDIO_EXT_IS_HEADPHONEMIC;
#endif HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW /* OLD ver. */
  return ( jacktype ? IRIS_AUDIO_EXT_IS_EXTSPEAKER : IRIS_AUDIO_EXT_IS_HEADPHONEMIC );
}


#ifdef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART
static int old_usb_val = 0;
static int old_uart_val = 0;
static int old_carkit_val = 0;

static void check_port(unsigned long __unused)
{
  int new_usb_val;
  int new_uart_val;
  int new_carkit_val;
  new_usb_val = iris_read_usb_sync();
  new_uart_val = iris_read_uart_sync();
  new_carkit_val = IS_CARKIT_EXISTS(new_usb_val,new_uart_val);
  printk("GSMCTL INT: %d %d %d\n",new_usb_val,new_uart_val,new_carkit_val);
  if( new_carkit_val ){
    if( ! old_carkit_val ){
      /* Carkit Newly Connected */
      carkit_status_changed(1);
    }else{
      /* Carkit still connected */
    }
  }else{
    if( old_carkit_val ){
      /* Carkit Newly Removed */
      carkit_status_changed(0);
    }else{
      /* Carkit still Removed */
    }
    if( new_usb_val ){
      if( ! old_usb_val ){
	/* USB Newly Connected */
	synchro_status_changed(1,1);
      }else{
	/* USB still connected */
      }
    }else{
      if( old_usb_val ){
	/* USB Newly Removed */
	if( ! old_carkit_val ){
	  synchro_status_changed(0,1);
	}
      }else{
	/* USB still Removed */
      }
    }
    if( new_uart_val ){
      if( ! old_uart_val ){
	/* UART Newly Connected */
	synchro_status_changed(1,0);
      }else{
	/* UART still connected */
      }
    }else{
      if( old_uart_val ){
	/* UART Newly Removed */
	if( ! old_carkit_val ){
	  synchro_status_changed(0,0);
	}
      }else{
	/* UART still Removed */
      }
    }
  }
  old_usb_val = new_usb_val;
  old_usb_val = new_uart_val;
  old_carkit_val = new_carkit_val;
}

static struct timer_list check_port_timer = {
  function: check_port
};


void iris_gsmctl_uart2_gpio_handler(void)
{
  //printk("deleting timer\n");
  del_timer(&check_port_timer);
  check_port_timer.expires = jiffies+HZ;
  //printk("adding timer\n");
  add_timer(&check_port_timer);
  //printk("timer done\n");
}

void iris_gsmctl_usb_gpio_handler(void)
{
  //printk("deleting timer\n");
  del_timer(&check_port_timer);
  check_port_timer.expires = jiffies+HZ;
  //printk("adding timer\n");
  add_timer(&check_port_timer);
  //printk("timer done\n");
}
EXPORT_SYMBOL(iris_gsmctl_uart2_gpio_handler);
EXPORT_SYMBOL(iris_gsmctl_usb_gpio_handler);

#endif /* CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */


static void re_check_sync_status(int notify)
{
  old_usb_val = iris_read_usb_sync();
  old_uart_val = iris_read_uart_sync();
  old_carkit_val = IS_CARKIT_EXISTS(old_usb_val,old_uart_val);
  if( old_carkit_val ){
    /* Carkit is connected at boot */
    if( notify ) carkit_status_changed(1);
  }else{
    if( old_usb_val ){
      /* USB is connected at boot */
      if( notify ) synchro_status_changed(1,1);
    }
    if( old_uart_val ){
      /* UART is connected at boot */
      if( notify ) synchro_status_changed(1,0);
    }
  }
}


#ifdef HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW

#endif /* HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW */


#ifdef HEADPHONEMIC_IRQ_AVAIL

#define enable_mic_irq()  enable_pb_irq(HEADPHONEMIC)
#define disable_mic_irq() disable_pb_irq(HEADPHONEMIC)

static void mic_port_timer_handler(unsigned long __unused);

static struct timer_list mic_port_timer = { function: mic_port_timer_handler };

static int mic_current_status = 0;
static int mic_open_count = 0;

static void mic_update_event(int new_status)
{
  gsmcarkit_status_changed();
}

static void mic_port_timer_handler(unsigned long __unused)
{
  int curstat = iris_read_headphonemic_status();
  del_timer(&mic_port_timer);
  if( curstat != mic_current_status ){
    mic_update_event(curstat);
    mic_current_status = curstat;
  }
  if( curstat == IRIS_AUDIO_EXT_IS_NONE ){
    /* NO external device. GET_PBDR(HEADPHONEMIC) == 0 */
    enable_mic_irq();
  }else{
    /* External device exists. GET_PBDR(HEADPHONEMIC) != 0 */
    disable_mic_irq();
    mic_port_timer.expires = jiffies+HZ;
    add_timer(&mic_port_timer);
  }
}

static void start_micirq_check(void)
{
  if( ! mic_open_count ){
    mic_current_status = iris_read_headphonemic_status();
  }
  mic_open_count++;
  mic_port_timer_handler(0);
}

static void stop_micirq_check(void)
{
  mic_open_count--;
  del_timer(&mic_port_timer);
  disable_pb_irq(HEADPHONEMIC);
}







#endif /* HEADPHONEMIC_IRQ_AVAIL */



/*
 *  interrupts
 */

extern int iris_now_suspended;

static void gsm_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  int changed = 0;
  if( iris_now_suspended ) return;
#ifndef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART
  DEBUGPRINT(("gsm_interrupt PAINT=%x\n",IO_PAINT));
  if( ! GET_PAINT(CARKIT_DETECT) && ! GET_PBINT(HEADPHONEMIC) ){
    /* this is not my interrupt */
    return;
  }
  DEBUGPRINT(("SHARP Gsm IRQ\n"));
  if( GET_PAINT(CARKIT_DETECT) ){
    disable_pa_irq(CARKIT_DETECT);
    SET_PAECLR(CARKIT_DETECT);
    changed = 1;
    SET_PADE_HI(CARKIT_DETECT);
    SET_PAEENR_HI(CARKIT_DETECT);
    enable_pa_irq(CARKIT_DETECT);
  }
#else /* CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */
#ifdef NEED_USB_DETECT_IRQ_HERE
  if( IO_FPGA_Status & USB_GA_BIT ){
    iris_gsmctl_usb_gpio_handler();
    //printk("NEED_USB_DETECT_IRQ_HERE\n");
    turn_usb_ga_irq();
    clear_fpga_int(USB_GA_BIT);
    enable_fpga_irq(USB_GA_BIT);
  }
#endif /* NEED_USB_DETECT_IRQ_HERE */
#endif /* CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */
#ifdef HEADPHONEMIC_IRQ_AVAIL
  if( GET_PBINT(HEADPHONEMIC) ){
    mic_port_timer_handler(0);
  }
#endif /* HEADPHONEMIC_IRQ_AVAIL */
  if( changed ) gsmcarkit_status_changed();
}

/*
 *  i/o
 */

int iris_sharpgsm_open(struct inode *inode, struct file *filp)
{
#ifndef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART
  SET_PAECLR(CARKIT_DETECT);
  SET_PADE_HI(CARKIT_DETECT);
  SET_PAEENR_HI(CARKIT_DETECT);
  enable_pa_irq(CARKIT_DETECT);
#endif /* CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */

#ifdef HEADPHONEMIC_IRQ_AVAIL
  start_micirq_check();
#endif /* HEADPHONEMIC_IRQ_AVAIL */
#if defined(HEADPHONEMIC_IRQ_AVAIL) || ! defined(CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART)
  if ( request_irq(IRQ_GPIO,gsm_interrupt,SA_SHIRQ,"sharp gsm_gpio",gsm_interrupt) )
    return -ENODEV; 
#endif /* HEADPHONEMIC_IRQ_AVAIL || ! CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */
 return 0;
}

int iris_sharpgsm_release(struct inode *inode, struct file *filp)
{
#ifndef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART
  disable_pa_irq(CARKIT_DETECT);
#endif /* CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */
#ifdef HEADPHONEMIC_IRQ_AVAIL
  stop_micirq_check();
#endif /* HEADPHONEMIC_IRQ_AVAIL */
#if defined(HEADPHONEMIC_IRQ_AVAIL) || ! defined(CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART)
  free_irq(IRQ_GPIO, gsm_interrupt);
#endif /* HEADPHONEMIC_IRQ_AVAIL || ! CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */
  return 0;
}

int iris_sharpgsm_ioctl(struct inode *inode,
			struct file *filp,
			unsigned int command,
			unsigned long arg)
{
  switch(command){
  case SHARP_IRIS_GETSYNCSTATUS:
    {
      int error;
      sharp_irisext_status* puser = (sharp_irisext_status*)arg;
      int usb;
      int uart;
      int carkit = 0;
      DEBUGPRINT(("sharpgsm irisext status\n"));
      usb = iris_read_usb_sync();
      uart = iris_read_uart_sync();
      if( IS_CARKIT_EXISTS(usb,uart) ){
	usb = 0;
	uart = 0;
	carkit = 1;
      }
      error = put_user(usb,&(puser->usb));
      if( error ) return error;
      error = put_user(uart,&(puser->uart));
      if( error ) return error;
      error = put_user(carkit,&(puser->carkit));
      if( error ) return error;
      return 0;
    }
    break;
  case SHARP_IRIS_RECHECKDEVICE:
    {
      re_check_sync_status(1);
      return 0;
    }
    break;
  default:
    break;
  }
  return -EINVAL;
}

int iris_sharpgsm_resume(void)
{
}

int iris_sharpgsm_init(void)
{
  SET_PB_IN(HEADPHONEMIC);

#ifdef HEADPHONEMIC_IRQ_AVAIL
  disable_pb_irq(HEADPHONEMIC);
#endif /* HEADPHONEMIC_IRQ_AVAIL */

#ifdef HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW
  SET_PA_IN(JACKTYPE);
  disable_pa_irq(JACKTYPE);
#endif /* HEADPHONE_OR_SPEAKER_MUST_BE_CONFIGURED_BY_SW */

#ifdef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART
  if ( request_irq(IRQ_INTF,gsm_interrupt,SA_SHIRQ,"sharp gsm_gpio",gsm_interrupt) )
    return -ENODEV; 
#endif /* CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART */

#ifdef CARKIT_SIGNAL_CONSISTS_OF_USB_AND_UART
  re_check_sync_status(0);
#endif

#ifdef NEED_USB_DETECT_IRQ_HERE
  turn_usb_ga_irq();
  clear_fpga_int(USB_GA_BIT);
  enable_fpga_irq(USB_GA_BIT);
#endif /* NEED_USB_DETECT_IRQ_HERE */
}

/*
 *   end of source
 */
