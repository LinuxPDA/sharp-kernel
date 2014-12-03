/*
 *  linux/arch/arm/mach-l7200/iris_charger.c
 *
 *  Driver for Battery Charger On SHARP PDA Iris
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
#include <linux/pm.h>
#include <linux/tqueue.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>
#include <asm/arch/gpio.h>
#include <asm/arch/fpga.h>

//#define DEBUG	1
#ifdef DEBUG
#define DEBUGPRINT(s)   printk s
#define DPRINTK(x, args...)  printk(__FUNCTION__ ": " x,##args)
#else
#define DEBUGPRINT(s)   
#define DPRINTK(x, args...)
#endif

#define IRIS_CHARGER_FLOW_VERSION   2

typedef struct pt_regs	PT_REGS;

extern int iris_battery_charge_full;	/* 1 : full / 0:  */

extern int iris_read_battery_mV(void);

/*
 * GPIOs...
 */

#define CHARGER_START  (bitPB5)
#define AC_INPUT       (bitWSRC2)
#define BATTERY_COVER  (bitWSRC8)
#define FULL_CHARGED   (bitWSRC9)

static int leveledport_isHI(int bitwsrc)
{
  int i;
  int val;
  if( IO_FPGA_EEN & bitwsrc ){
    set_fpga_int_trigger_highlevel(bitwsrc);
  }
  for(i=0;i<10000;i++){
    val = IO_FPGA_RawStat & bitwsrc;
    udelay(500);
    if( ( IO_FPGA_RawStat & bitwsrc ) == val ) break;
  }
  if( IO_FPGA_LSEL & bitwsrc ){
    /* Currentry HI level trigger , So 1->1 convert */
    return ( val ? 1 : 0 );
  }else{
    /* Currentry LO level trigger , so wsrc is inverted. */
    return ( val ? 0 : 1 );
  }
}

static void turn_port_trigger(int bitwsrc)
{
  if( leveledport_isHI(bitwsrc) ){
    /* Currently HI level. Next interrupt is LO */
    set_fpga_int_trigger_lowlevel(bitwsrc);
  }else{
    /* Currently LO level. Next interrupt is HI */
    set_fpga_int_trigger_highlevel(bitwsrc);
  }
}

int charger_is_charging(void)
{
  return ( GET_PBDR(CHARGER_START) ? 1 : 0 );
}

static void charger_on(void)
{
  DPRINTK("ON\n");
  SET_PB_OUT(CHARGER_START);
  SET_PBDR_HI(CHARGER_START);
}

static void charger_off(void)
{
  DPRINTK("OFF\n");
  SET_PB_OUT(CHARGER_START);
  SET_PBDR_LO(CHARGER_START);
  /* SET_PBDR_HI(CHARGER_START); */
}

static int fullfill_irq_is_enabled = 0;
static int battery_checking = 0;

static void fullfill_irq_on(void)
{
  unsigned long flags;
  save_flags(flags); cli();
  fullfill_irq_is_enabled = 1;
  if( ! battery_checking ){
    enable_fpga_irq(FULL_CHARGED);
  }else{
    /* irq is enabled later when 'restore_fullfill_irq_for_batcheck' */
  }
  restore_flags(flags);
}

static void fullfill_irq_off(void)
{
  unsigned long flags;
  save_flags(flags); cli();
  fullfill_irq_is_enabled = 0;
  disable_fpga_irq(FULL_CHARGED);
  restore_flags(flags);
}

void disable_fullfill_irq_for_batcheck(void)
{
  unsigned long flags;
  save_flags(flags); cli();
  if( ! battery_checking ){
    /* disable fullfill irq */
    disable_fpga_irq(FULL_CHARGED);
  }
  battery_checking++;
  restore_flags(flags);
}

void restore_fullfill_irq_for_batcheck(void)
{
  unsigned long flags;
  save_flags(flags); cli();
  battery_checking--;
  if( ! battery_checking ){
    /* restore fullfill irq */
    if( fullfill_irq_is_enabled ){
      enable_fpga_irq(FULL_CHARGED);
    }
  }
  restore_flags(flags);
}


#include <asm/sharp_char.h>
extern int set_led_status(int which,int status);

void (*set_charge_led_hook)(int) = NULL;

static void set_charge_led(int status)
{
  set_led_status(SHARP_LED_CHARGER,status);
  if( set_charge_led_hook ) (*set_charge_led_hook)(status);
}



static struct timer_list iris_charger_timer;
static struct timer_list iris_timeout_timer;
static struct timer_list iris_recharge_timer;


#define NO_CHARGING      0
#define PHASE1_RUNNING   1
#define PHASE2_RUNNING   2
#define PHASE3_RUNNING   3
#define PHASE4_RUNNING   4

#define ERROR_PHASE    -1
#define ERROR_CHARGER  -2
#define ERROR_BAD_BAT  -3

static int charging_status = NO_CHARGING;

#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
#define CHARGER_IC_BAD_VALUE      4400  /* mV */
#else
#define CHARGER_IC_BAD_VALUE      4500  /* mV */
#endif
#define FULLFILLED_VOLTAGE        3900  /* mV */
#define RE_CHARGE_VOLTAGE         3700  /* mV */
#define CHARGER_BATTERY_BAD_VALUE 2500  /* mV */

#define RE_CHARGE_CHECK_INTERVAL     5 /* mins */


#ifdef DEBUG
#define PHASE3_CHARGE_TIMEOUT   1      /* minutes */
#define PHASE2_CHARGE_TIMEOUT   3      /* minutes */
#define PHASE1_CHARGE_TIMEOUT   30     /* secs */
#define PHASE2_PRECHARGE_TIMEOUT 30    /* secs */
#define PHASE3_DISCHARGE_TIMEOUT 30    /* secs */
#else /* ! DEBUG */
#define PHASE3_CHARGE_TIMEOUT   (20)   /* minutes */
#define PHASE2_CHARGE_TIMEOUT   (60*5) /* minutes */
#define PHASE1_CHARGE_TIMEOUT   60     /* secs */
#define PHASE2_PRECHARGE_TIMEOUT 60    /* secs */
#define PHASE3_DISCHARGE_TIMEOUT 30    /* secs */
#endif /* DEBUG */


static int phase2_wait = 0;
static int phase3_wait = 0;
static int interrupt_count = 0;

static void charge_timeout(void);
void quit_charger(int status);
int start_charger(void);
#if IRIS_CHARGER_FLOW_VERSION < 2 /* ver1 or earlier */
static void recharge_check(void);
#endif /* IRIS_CHARGER_FLOW_VERSION */
static void charger_interrupt(int irq,void* dev_id,PT_REGS* reg);

static void charge_timeout(void)
{
  DPRINTK("start\n");
  if( charging_status != PHASE2_RUNNING
      && charging_status != PHASE3_RUNNING
#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
      && charging_status != PHASE4_RUNNING
#endif /* IRIS_CHARGER_FLOW_VERSION > 1 */
      ){
    charging_status = ERROR_PHASE;
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("status error\n");
    return;
  }
  del_timer(&iris_timeout_timer);
  if( (--phase2_wait) > 0 ){
    iris_timeout_timer.function = charge_timeout;
    iris_timeout_timer.expires = jiffies + HZ*60;
    add_timer(&iris_timeout_timer);
    DPRINTK("wait again (left=%d)\n",phase2_wait);
    return;
  }
  /* this means timeout */
#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  DPRINTK("timeout, restart\n");
  /* re-start charger */
  charger_off();
  charger_on();
  /* re-set timeout */
  if( charging_status == PHASE3_RUNNING ){
    /* back to phase3 -> phase2 */
    charging_status = PHASE2_RUNNING;
  }else{
    /* phase2 -> phase2 , phase4 -> phase4 */
  }
  phase2_wait = PHASE2_CHARGE_TIMEOUT;
  iris_timeout_timer.function = charge_timeout;
  iris_timeout_timer.expires = jiffies + HZ*60;
  DPRINTK("wait for timeout\n");
  add_timer(&iris_timeout_timer);
  return;
#else /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
  DPRINTK("timeout-off\n");
  quit_charger(LED_CHARGER_ERROR);
  /* set re-charge check */
  iris_recharge_timer.function = recharge_check;
  iris_recharge_timer.expires = jiffies + HZ*60*RE_CHARGE_CHECK_INTERVAL;
  add_timer(&iris_recharge_timer);
  return;
#endif /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
}

static void charge_phase3_5(void)
{
  int v;
  DPRINTK("start\n");
  if( charging_status != PHASE3_RUNNING ){
    charging_status = ERROR_PHASE;
    quit_charger(LED_CHARGER_ERROR);
    DPRINTK("status error\n");
    return;
  }
  del_timer(&iris_charger_timer);
  if( (--phase3_wait) > 0 ){
    /* not timeout. wait again */
    DPRINTK("not timeout (left %d)\n",phase3_wait);
    iris_charger_timer.function = charge_phase3_5;
    iris_charger_timer.expires = jiffies + HZ*60;
    add_timer(&iris_charger_timer);
    return;
  }
#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  charger_on();
  fullfill_irq_off(); /* disable irq */
  free_irq(IRQ_INTF, charger_interrupt);
  set_charge_led(LED_CHARGER_OFF);
  charging_status = PHASE4_RUNNING;
  return;
#else /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
  /* timeout... check voltage again */
  charger_off();
  v = iris_read_battery_mV();
  DPRINTK("curvol=%d\n",v);
  if( v > FULLFILLED_VOLTAGE ){
    /* charging fully done */
    fullfill_irq_off(); /* disable_fpga_irq(FULL_CHARGED); */
    free_irq(IRQ_INTF, charger_interrupt);
    del_timer(&iris_timeout_timer);
    del_timer(&iris_charger_timer);
    phase2_wait = 0;
    phase3_wait = 0;
    set_charge_led(LED_CHARGER_OFF);
    charging_status = PHASE4_RUNNING;  /* should not off , while AC is ONLINE . So go P4 */
    iris_battery_charge_full = 1;
    DPRINTK("DONE!\n",v);
    /* set re-charge check */
    iris_recharge_timer.function = recharge_check;
    iris_recharge_timer.expires = jiffies + HZ*60*RE_CHARGE_CHECK_INTERVAL;
    add_timer(&iris_recharge_timer);
    return;
  }
  /* not full charged yet. charge again... */
  if( leveledport_isHI(AC_INPUT) ){
    /* but... AC is not connected. Why ? */
    DEBUGPRINT(("phase3::not connected to AC\n"));
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("out of AC\n");
    return;
  }
  charger_on();
 wait_again:
  DPRINTK("wait again\n",v);
  iris_charger_timer.function = charge_phase3_5;
  iris_charger_timer.expires = jiffies + HZ*60;
  add_timer(&iris_charger_timer);
  return;  
#endif /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
}


#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
static void charge_phase3_1(void)
{
  int v;
  DPRINTK("start\n");
  if( charging_status != PHASE3_RUNNING ){
    charging_status = ERROR_PHASE;
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("status error\n");
    return;
  }
  /* check battery bad value */
  v = iris_read_battery_mV();
  DPRINTK("curvol=%d\n",v);
  if( v < CHARGER_BATTERY_BAD_VALUE ){
    /* this is bad condition */
    charging_status = ERROR_BAD_BAT;
    quit_charger(LED_CHARGER_ERROR);
    DPRINTK("bad battery\n");
    return;
  }
  if( v < FULLFILLED_VOLTAGE ){
    /* not full-charged */
    /* back to phase2 */
    charging_status = PHASE2_RUNNING;
    charger_on();
    fullfill_irq_on(); /* enable full-fill irq.... */
    return;
  }
  /* full charged */
  charger_on(); /* turn on charger */
  /* wait for timer */
  phase3_wait = PHASE3_CHARGE_TIMEOUT;
  del_timer(&iris_charger_timer);
  iris_charger_timer.function = charge_phase3_5;
  iris_charger_timer.expires = jiffies + HZ*60;
  add_timer(&iris_charger_timer);
}

#endif /* IRIS_CHARGER_FLOW_VERSION > 1 */


static void charge_phase3(void)
{
  DPRINTK("start\n");
  if( charging_status != PHASE2_RUNNING ){
    charging_status = ERROR_PHASE;
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("status error\n");
    return;
  }
  charging_status = PHASE3_RUNNING;
#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  charger_off();
  /* wait for a minute for discharge... */
  del_timer(&iris_charger_timer);
  iris_charger_timer.function = charge_phase3_1;
  iris_charger_timer.expires = jiffies + HZ*PHASE3_DISCHARGE_TIMEOUT;
  add_timer(&iris_charger_timer);
  return;
#else /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
  phase3_wait = PHASE3_CHARGE_TIMEOUT;
  del_timer(&iris_charger_timer);
  iris_charger_timer.function = charge_phase3_5;
  iris_charger_timer.expires = jiffies + HZ*60;
  add_timer(&iris_charger_timer);
  return;
#endif /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
}


#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
static void charge_phase2_5(void)
{
  int v;
  DPRINTK("start\n");
  if( charging_status != PHASE2_RUNNING ){
    charging_status = ERROR_PHASE;
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("status error\n");
    return;
  }
  /* check battery exist or not */
  v = iris_read_battery_mV();
  DPRINTK("curvol=%d\n",v);
  if( v < CHARGER_BATTERY_BAD_VALUE ){
    charging_status = ERROR_BAD_BAT;
    quit_charger(LED_CHARGER_ERROR);
    DPRINTK("bad battery\n");
    return;
  }
  /* enable full-fill irq.... */
  fullfill_irq_on();
  /* continued by background.... */
}
#endif /* IRIS_CHARGER_FLOW_VERSION < 2 *//* ver1 or earlier */


static void charge_phase2(void)
{
  DPRINTK("start\n");
  if( charging_status != PHASE1_RUNNING ){
    charging_status = ERROR_PHASE;
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("status error\n");
    return;
  }
  charging_status = PHASE2_RUNNING;
  /* start main charge */
  phase2_wait = PHASE2_CHARGE_TIMEOUT;
  if( leveledport_isHI(AC_INPUT) ){
    /* but... AC is not connected. Why ? */
    DEBUGPRINT(("phase2::not connected to AC\n"));
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("out of AC\n");
    return;
  }
  charger_on();
  set_charge_led(LED_CHARGER_CHARGING);
  del_timer(&iris_timeout_timer);
  iris_timeout_timer.function = charge_timeout;
  iris_timeout_timer.expires = jiffies + HZ*60;
  DPRINTK("wait for timeout\n");
  add_timer(&iris_timeout_timer);
#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  del_timer(&iris_charger_timer);
  iris_charger_timer.function = charge_phase2_5;
  iris_charger_timer.expires = jiffies + HZ*PHASE2_PRECHARGE_TIMEOUT;
  DPRINTK("wait for 2_5\n");
  add_timer(&iris_charger_timer);
  return;
#else /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
  /* wait for full-charged-irq */
  fullfill_irq_on(); /* enable_fpga_irq(FULL_CHARGED); */
  return;
#endif /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
}

#if IRIS_CHARGER_FLOW_VERSION < 2 /* ver1 or earlier */
static void charge_phase1_5(void)
{
  int v;
  DPRINTK("start\n");
  if( charging_status != PHASE1_RUNNING ){
    charging_status = ERROR_PHASE;
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("status error\n");
    return;
  }
  /* check battery exist or not */
  charger_off();
  v = iris_read_battery_mV();
  DPRINTK("curvol=%d\n",v);
  if( v < CHARGER_BATTERY_BAD_VALUE ){
    charging_status = ERROR_BAD_BAT;
    quit_charger(LED_CHARGER_ERROR);
    DPRINTK("bad battery\n");
    return;
  }
  /* goto phase2 charge */
  charge_phase2();
}
#endif /* IRIS_CHARGER_FLOW_VERSION < 2 *//* ver1 or earlier */

static void charge_phase1(void)
{
  int v;
  DPRINTK("start\n");
  if( charging_status != NO_CHARGING ){
    charging_status = ERROR_PHASE;
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("status error\n");
    return;
  }
  charging_status = PHASE1_RUNNING;
  /* read charger IC status */
  if( leveledport_isHI(AC_INPUT) ){
    /* but... AC is not connected. Why ? */
    DEBUGPRINT(("phase1::not connected to AC\n"));
    quit_charger(LED_CHARGER_OFF);
    DPRINTK("out of AC\n");
    return;
  }
  charger_on();
  v = iris_read_battery_mV();
  DPRINTK("curvol=%d\n",v);
  if( v > CHARGER_IC_BAD_VALUE ){
    charging_status = ERROR_CHARGER;
    quit_charger(LED_CHARGER_ERROR);
    DPRINTK("bad adaptor %d\n",v);
    return;
  }
  interrupt_count = 0;
#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  /* got phase2 charge */
  charge_phase2();
  return;
#else /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
  charger_off();
  /* check battery exist or not */
  v = iris_read_battery_mV();
  DPRINTK("curvol=%d\n",v);
  if( v < CHARGER_BATTERY_BAD_VALUE ){
    /* no battery or not charged enough. */
    /* charge on and wait for 60sec */
    charger_on();
    del_timer(&iris_charger_timer);
    iris_charger_timer.function = charge_phase1_5;
    iris_charger_timer.expires = jiffies + HZ*PHASE1_CHARGE_TIMEOUT;
    DPRINTK("wait for 1_5\n");
    add_timer(&iris_charger_timer);
  }else{
    /* goto phase2 charge */
    charge_phase2();
  }
  return;
#endif /* IRIS_CHARGER_FLOW_VERSION <= 1 */ /* ver 1 */
}

#if IRIS_CHARGER_FLOW_VERSION < 2 /* ver1 or earlier */
static void recharge_check(void)
{
  int v;
  DPRINTK("start\n");
  del_timer(&iris_recharge_timer);
  if( charging_status != NO_CHARGING ){
    DPRINTK("charging\n");
    return;
  }
  /* check battery */
  charger_off();
  v = iris_read_battery_mV();
  DPRINTK("curvol=%d\n",v);
  if( v < RE_CHARGE_VOLTAGE ){
    DPRINTK("low battery. re-charge\n");
    start_charger();
    return;
  }
  /* next timeout */
  iris_recharge_timer.function = recharge_check;
  iris_recharge_timer.expires = jiffies + HZ*60*RE_CHARGE_CHECK_INTERVAL;
  add_timer(&iris_recharge_timer);
}
#endif /* IRIS_CHARGER_FLOW_VERSION < 2 */

extern int iris_now_suspended;

static void charger_interrupt(int irq,void* dev_id,PT_REGS* reg)
{
  if( iris_now_suspended ) return;
  if( IO_FPGA_Status & FULL_CHARGED ){
    DEBUGPRINT(("Detected Full Charged\n"));
    interrupt_count++;
    clear_fpga_int(FULL_CHARGED);
    fullfill_irq_off(); /* disable_fpga_irq(FULL_CHARGED); */
    charge_phase3();
  }
}

static int suspended = 0;

int start_charger(void)
{
  int err;
  if( suspended ) return;
  if( charging_status > NO_CHARGING ){
    DEBUGPRINT(("Already Started.\n"));
    return 0;
  }
  /* init SSP for voltage check */
  iris_init_ssp_sys();
  /* end of init-SSP */
  init_timer(&iris_charger_timer);
  del_timer(&iris_recharge_timer);
  DEBUGPRINT(("start charger\n"));
  if( charging_status < 0 ){
    /* clear error */
    charging_status = NO_CHARGING;
  }
  if( leveledport_isHI(AC_INPUT) ){
    DEBUGPRINT(("not connected to AC\n"));
    return -EINVAL;
  }
  fullfill_irq_off(); /* disable_fpga_irq(FULL_CHARGED); */
  err = request_irq(IRQ_INTF, charger_interrupt, SA_SHIRQ, "charger", charger_interrupt);
  set_fpga_int_trigger_highlevel(FULL_CHARGED);
  if( err ){
    DEBUGPRINT(("irq request failed\n"));
    return err;
  }
  DEBUGPRINT(("charger start\n"));
  charge_phase1();
  return 0;
}

void quit_charger(int status)
{
  charger_off();
  fullfill_irq_off(); /* disable_fpga_irq(FULL_CHARGED); */
  free_irq(IRQ_INTF, charger_interrupt);
  del_timer(&iris_timeout_timer);
  del_timer(&iris_charger_timer);
  phase2_wait = 0;
  phase3_wait = 0;
  charging_status = NO_CHARGING;
  set_charge_led(status);
  DEBUGPRINT(("charger end\n"));
}


void terminate_charger(void)
{
  if( charging_status > NO_CHARGING ){
    quit_charger(LED_CHARGER_OFF);
  }
  set_charge_led(LED_CHARGER_OFF);
  charger_off();
}


/*
 * operations....
 */

#include <linux/miscdevice.h>


#ifdef CONFIG_PM

static struct pm_dev *battery_pm_dev; /* registered PM device */

static struct timer_list timer_func_list;

static void resume_start_charger_wrapper(ulong c)
{
  del_timer(&timer_func_list);
  start_charger();
}

static int Iris_battery_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
  switch (req) {
  case PM_STANDBY:
  case PM_BLANK:
  case PM_SUSPEND:
    if( suspended ) break;
    /* test... always turn off charger */
    quit_charger(LED_CHARGER_OFF);
    suspended = 1;
    break;
  case PM_UNBLANK:
  case PM_RESUME:
    if( ! suspended ) break;
    /* test... always turn on charger */
    init_timer(&timer_func_list);
    timer_func_list.expires = jiffies+HZ*2;
    timer_func_list.data = 0;
    timer_func_list.function = start_charger;
    add_timer(&timer_func_list);
    suspended = 0;
    break;
  }
  return 0;
}
#endif

#define IRIS_CHARGER_GETSTATUS 0x10

typedef struct {
  int phase;
  int onoff;
} iris_charger_status;

static int do_ioctl(struct inode *inode,struct file *file,
		    unsigned int cmd,unsigned long arg)
{
  int error;
  switch( cmd ) {
  case IRIS_CHARGER_GETSTATUS:
    {
      int onoff;
      iris_charger_status* p_cuser = (iris_charger_status*)arg;
      if( ! p_cuser ) return -EINVAL;
      error = put_user(charging_status,&p_cuser->phase);
      if( error ) return error;
      onoff = charger_is_charging();
      error = put_user(onoff,&p_cuser->onoff);
      if( error ) return error;
    }
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

static unsigned int do_poll(struct file* filep, poll_table* wait)
{
  return 0;
}

static ssize_t do_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
  return count;
}

static int do_fasync(int fd, struct file* filep, int mode)
{
  return 0;
}

int do_open(struct inode *inode, struct file *filp)
{
  return 0;
}

int do_release(struct inode *inode, struct file *filp)
{
  return 0;
}

#ifndef BATTERY_MINOR_DEV
#define BATTERY_MINOR_DEV  241
#endif /* ! BATTERY_MINOR_DEV */

static struct file_operations iris_battery_fops = {
  owner:	THIS_MODULE,
  read:		do_read,
  poll:		do_poll,
  fasync:       do_fasync,
  ioctl:	do_ioctl,
  open:		do_open,
  release:	do_release,
};

static struct miscdevice battery_device = {
  BATTERY_MINOR_DEV,
  "battery",
  &iris_battery_fops
};

int __init Iris_battery_init(void)
{
#ifdef CONFIG_PM
  battery_pm_dev = pm_register(PM_SYS_DEV, 0, Iris_battery_pm_callback);
#endif
  charging_status = NO_CHARGING;
  misc_register(&battery_device);
  DEBUGPRINT(("Iris battery charger initialized\n"));
  return 0;
}

module_init(Iris_battery_init);



#ifdef MODULE
int init_module(void)
{
	Iris_battery_init();
	return 0;
}

void cleanup_module(void)
{
}
#endif /* MODULE */

/*
 *   end of source
 */
