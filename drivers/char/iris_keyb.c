/*
 * linux/drivers/char/iris_keyb.c 
 *
 * Based on:
 *   drivers/char/kbd_linkup.c - Keyboard Driver for Linkup-7200
 *   Copyright (C) Roger Gammans  1998
 *   Written for Linkup 7200 by Xuejun Tao, ISDcorp
 *   Refer to kbd_7110.c (Cirrus Logic 7110 Keyboard driver)
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <asm/arch/keyboard.h>
#include <asm/uaccess.h>
#include <linux/tqueue.h>
#include <linux/pm.h>
#include <linux/kbd_ll.h>
//#include <linux/spinlock.h>
#include <linux/delay.h>

#ifdef USE_KBD_IRQ		// could be defined in asm/arch/keyboard.h
#include <asm/irq.h>		/* for linkup keyboard interrupt */
#endif

#include <asm/arch/irqs.h>
#include <asm/arch/gpio.h>

#ifndef IRISKBD_MAJOR
# define IRISKBD_MAJOR 61
#endif

#define DEBUGPRINT(s)  /* printk s */


#ifndef IRIS_WITHOUT_KEY_ENH_WORKAROUND /* for Iris final version */
#define KBD_PABITS (bitPA0|bitPA1|bitPA2|bitPA3|bitPA4|bitPA5)
#else /* IRIS_WITHOUT_KEY_ENH_WORKAROUND *//* for Boards without key-enh. */
#define KBD_PABITS (bitPA0|bitPA1|bitPA2|bitPA3|bitPA4)
#endif

/*
 * common logical driver definition
 */
extern void sharppda_kbd_press(int keycode);
extern void sharppda_kbd_release(int keycode);
extern void sharppda_scan_logdriver_init(void);
extern void sharppda_scan_key_gpio(void* dummy);
#define iris_kbd_tick  sharppda_scan_key_gpio

#ifdef CONFIG_PM
static struct pm_dev *iris_kbd_pm_dev;
#endif

/*
 * physical driver depending on IRIS target.
 */
void iris_kbd_discharge_all(void)
{
  set_bits(IO_KBKSR, KBDSCAN, KBSC_LO);
#if KB_COLS >= KB_BASIC_COLS
  del_debug_port_data(ALL_AUX_COLS); /* common i/f version */
#endif
}

void iris_kbd_activate_all(void)
{
  set_bits(IO_KBKSR, KBDSCAN, KBSC_HI);
#if KB_COLS >= KB_BASIC_COLS
  add_debug_port_data(ALL_AUX_COLS & (AUX_COL_12|AUX_COL_13));
  del_debug_port_data(ALL_AUX_COLS & (AUX_COL_12_DIS|AUX_COL_13_DIS));
#endif
}

void iris_kbd_activate_col(int col)
{
#if KB_COLS >= KB_BASIC_COLS
  if ( col < KB_BASIC_COLS)
#endif
    set_bits(IO_KBKSR, KBDSCAN, KBSC_COL0+col);
#if KB_COLS >= KB_BASIC_COLS
  else{	/* Aux cols 12 or 13 */
    if ( col == 12){  /* col 12 */
      add_debug_port_data(ALL_AUX_COLS & (AUX_COL_12|AUX_COL_13_DIS));
      del_debug_port_data(ALL_AUX_COLS & (AUX_COL_12_DIS|AUX_COL_13));
    }
    else{  /* col 13 */
      add_debug_port_data(ALL_AUX_COLS & (AUX_COL_13|AUX_COL_12_DIS));
      del_debug_port_data(ALL_AUX_COLS & (AUX_COL_13_DIS|AUX_COL_12));
    }
  }
#endif
}

#ifdef USE_KBD_IRQ
static void iris_kbd_enable_irq(void)
{
  enable_pa_irq(KBD_PABITS); /* shared GPIO version */
  return;
}

static void iris_kbd_disable_irq(void)
{
  disable_pa_irq(KBD_PABITS); /* shared GPIO version */
  return;
}

int iris_kbdctl_keyboard_disabled = 1;

static void iris_kbd_irq(int irq,void *dev_id,struct pt_regs *regs)
{
#if 1
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif

  if( ! GET_PAINT(KBD_PABITS) ){
    return;
  }

  iris_kbd_disable_irq();
  if( ! iris_kbdctl_keyboard_disabled ) iris_kbd_tick(NULL);
  DISABLE_PA_IRQ(KBD_PABITS);
  SET_PAECLR(KBD_PABITS);
  iris_kbd_enable_irq();

#if 1  // auto power/light cancel
	autoPowerCancel = 0;
	autoLightCancel = 0;
#endif

}
#endif


#ifdef CONFIG_PM

extern void sharppda_scan_logdriver_suspend(void);
extern void sharppda_scan_logdriver_resume(void);


static int iris_kbd_pm_callback(struct pm_dev *pm_dev,
				pm_request_t req, void *data)
{
  static int suspended = 0;
  static unsigned long save_kbksr = 0;
	switch (req) {
	case PM_STANDBY:
	case PM_BLANK:
	case PM_SUSPEND:
	  //printk("kbd standby:");
	  if( suspended ) break;
	  save_kbksr = IO_KBKSR;
	  sharppda_scan_logdriver_suspend();
	  iris_kbd_discharge_all();
	  udelay(10);
	  iris_kbd_disable_irq();
	  //printk("kbd standby: done");
#ifdef USE_KBD_IRQ
//  		free_irq(IRQ_GPIO, iris_kbd_irq);
#endif
	  suspended = 1;
		break;
	case PM_UNBLANK:
	case PM_RESUME:
	  //printk("kbd resume:");
	  if( ! suspended ) break;
	  IO_KBDMR &= ~(1 << 12);	/* keyboard mode */
	  iris_kbd_disable_irq();
	  iris_kbd_discharge_all();
	  udelay(10);
	  IO_KBKSR = save_kbksr;
	  udelay(10);
	  iris_kbd_enable_irq();
	  sharppda_scan_logdriver_resume();
	  //printk("kbd resume: done");
#ifdef USE_KBD_IRQ
//		request_irq(IRQ_GPIO, iris_kbd_irq,
//				SA_INTERRUPT, "keyboard", iris_kbd_irq);
#endif
	  suspended = 0;
                break;
	}
	return 0;
}
#endif

void iris_kbd_hw_init(void)
{ 

  sharppda_scan_logdriver_init();

#ifdef USE_KBD_IRQ
  /* for linkup using keyboard interrupt instead of time interrupt */
  /* printk("Use interrupt mode to handle keyboard input\n"); */
  if (request_irq(IRQ_GPIO, iris_kbd_irq,
		  SA_SHIRQ, "keyboard",iris_kbd_irq )) {
    printk("iris_kbd_hw_init: unable to allocate interrupt\n");
    return;
  }

  SET_PAEENR_HI(KBD_PABITS);
  /* printk("KBD : registers\n"); */
  SET_PA_IN(KBD_PABITS);
  SET_PAECLR(KBD_PABITS);
  ADD_PA_IRQMASK(KBD_PABITS);
  SET_PAESNR_LO(KBD_PABITS);
  SET_PAEENR_HI(KBD_PABITS);
  /* printk("KBD : enable irq\n"); */
  iris_kbd_enable_irq();
  /* printk("KBD : enable irq done\n"); */
#endif

#ifdef CONFIG_PM
  iris_kbd_pm_dev = pm_register(PM_SYS_DEV, 0, iris_kbd_pm_callback);
#endif

  printk("keyboard initilaized.\n");
}

int iris_kbd_translate(unsigned char scancode, unsigned char *keycode_p)
{

	/*
 	* We do this strangley to be more independent of 
 	* our headers..
 	*/
	
	*keycode_p = scancode & ~(KBDOWN | KBUP);

	return 1;
}

