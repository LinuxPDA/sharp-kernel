/*
 * linux/drivers/char/spitz_keyb.c 
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   drivers/char/tosa_keyb.c 
 *   drivers/char/corgi_keyb.c
 *   drivers/char/collie_keyb.c
 *   drivers/char/iris_keyb.c
 *   drivers/char/kbd_linkup.c - Keyboard Driver for Linkup-7200
 *   Copyright (C) Roger Gammans  1998
 *   Written for Linkup 7200 by Xuejun Tao, ISDcorp
 *   Refer to kbd_7110.c (Cirrus Logic 7110 Keyboard driver)
 *
 * ChangeLong:
 *  04-25-2001 Lineo Japan, Inc.
 *  30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 *  12-Dec-2002 Sharp Corporation
 *      1-Nov-2003 Sharp Corporation   for Tosa
 *  20-Aug-2004 Sharp Corporation for Spitz
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <asm/arch/keyboard.h>
#include <asm/uaccess.h>
#include <linux/tqueue.h>
#include <linux/kbd_ll.h>
#include <linux/delay.h>
#include <asm/arch/pxa-regs.h>
#include <asm/sharp_keycode.h>

#ifdef USE_KBD_IRQ		// could be defined in asm/arch/keyboard.h
#include <asm/irq.h>	// for linkup keyboard interrupt
#endif

#include <asm/arch/irqs.h>

#ifndef SPITZ_KBD_MAJOR
# define SPITZ_KBD_MAJOR 61
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
static struct pm_dev *spitz_kbd_pm_dev;
#endif

/*
 * common logical driver definition
 */
extern void sharppda_kbd_press(int keycode);
extern void sharppda_kbd_release(int keycode);
extern void sharppda_scan_logdriver_init(void);
extern void sharppda_scan_key_gpio(void* dummy);
#define spitz_kbd_tick  sharppda_scan_key_gpio

static int strobe2gpio[] = {
	GPIO_KEY_STROBE0,
	GPIO_KEY_STROBE1,
	GPIO_KEY_STROBE2,
	GPIO_KEY_STROBE3,
	GPIO_KEY_STROBE4,
	GPIO_KEY_STROBE5,
	GPIO_KEY_STROBE6,
	GPIO_KEY_STROBE7,
	GPIO_KEY_STROBE8,
	GPIO_KEY_STROBE9,
	GPIO_KEY_STROBE10,
	GPIO_KEY_STROBE11,
};

static int sense2gpio[] = {
	GPIO_KEY_SENSE0,
	GPIO_KEY_SENSE1,
	GPIO_KEY_SENSE2,
	GPIO_KEY_SENSE3,
	GPIO_KEY_SENSE4,
	GPIO_KEY_SENSE5,
	GPIO_KEY_SENSE6,
	GPIO_KEY_SENSE7,
};

/*
 * physical driver depending on SPITZ target.
 */
void spitz_kbd_discharge_all(void)
{
	// STROBE All HiZ
	GPCR0  =  GPIO_G0_STROBE_BIT;
	GPDR0 &= ~GPIO_G0_STROBE_BIT;
	GPCR1  =  GPIO_G1_STROBE_BIT;
	GPDR1 &= ~GPIO_G1_STROBE_BIT;
	GPCR2  =  GPIO_G2_STROBE_BIT;
	GPDR2 &= ~GPIO_G2_STROBE_BIT;
	GPCR3  =  GPIO_G3_STROBE_BIT;
	GPDR3 &= ~GPIO_G3_STROBE_BIT;
}

void spitz_kbd_charge_all(void)
{
}

void spitz_kbd_activate_all(void)
{
	// STROBE ALL -> High
	GPSR0  =  GPIO_G0_STROBE_BIT;
	GPDR0 |=  GPIO_G0_STROBE_BIT;
	GPSR1  =  GPIO_G1_STROBE_BIT;
	GPDR1 |=  GPIO_G1_STROBE_BIT;
	GPSR2  =  GPIO_G2_STROBE_BIT;
	GPDR2 |=  GPIO_G2_STROBE_BIT;
	GPSR3  =  GPIO_G3_STROBE_BIT;
	GPDR3 |=  GPIO_G3_STROBE_BIT;

	udelay(KB_DISCHARGE_DELAY);

	// STATE CLEAR
	GEDR0 = GPIO_G0_SENSE_BIT;
	GEDR1 = GPIO_G1_SENSE_BIT;
	GEDR2 = GPIO_G2_SENSE_BIT;
	GEDR3 = GPIO_G3_SENSE_BIT;
}

void spitz_kbd_activate_col(int col)
{
	int gpio = strobe2gpio[col];
	GPDR0 &= ~GPIO_G0_STROBE_BIT;
	GPDR1 &= ~GPIO_G1_STROBE_BIT;
	GPDR2 &= ~GPIO_G2_STROBE_BIT;
	GPDR3 &= ~GPIO_G3_STROBE_BIT;
	if (gpio >= 0) {
		GPSR(gpio) = GPIO_bit(gpio);
		GPDR(gpio) |= GPIO_bit(gpio);
	}
}

void spitz_kbd_reset_col(int col)
{
	int gpio = strobe2gpio[col];
	GPDR0 &= ~GPIO_G0_STROBE_BIT;
	GPDR1 &= ~GPIO_G1_STROBE_BIT;
	GPDR2 &= ~GPIO_G2_STROBE_BIT;
	GPDR3 &= ~GPIO_G3_STROBE_BIT;
	if (gpio >= 0) {
		GPCR(gpio) = GPIO_bit(gpio);
		GPDR(gpio) |= GPIO_bit(gpio);
	}
}


#ifdef USE_KBD_IRQ

static void spitz_kbd_irq(int irq,void *dev_id,struct pt_regs *regs)
{
#ifdef CONFIG_APM
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif
	spitz_kbd_tick(NULL);

#if CONFIG_APM  // auto power/light cancel
	autoPowerCancel = 0;
	autoLightCancel = 0;
#endif

}
#endif

static void sharppda_scan_hinge();
#define HINGE_WAIT		1	/* 10ms */
#define HINGE_STABLE_COUNT	4	/* Xms x 4 */
static int last_hinge_state = 0;
static int hinge_count = 0;
static struct timer_list hinge_delay_timer = { function: sharppda_scan_hinge };

int read_hinge_state()
{
  int ret = 0;
  if (GPLR(GPIO_SWA) & GPIO_bit(GPIO_SWA)) ret |= 0x01;
  if (GPLR(GPIO_SWB) & GPIO_bit(GPIO_SWB)) ret |= 0x02;
  //  printk("read_hinge_state(%x)\n",ret);
  return ret;
}

static void sharppda_scan_hinge()
{
  int cur_hinge_state = read_hinge_state();

  if (cur_hinge_state == last_hinge_state) {
    if (++hinge_count >= HINGE_STABLE_COUNT) {
      handle_scancode(SLKEY_HINGEMOVED|KBDOWN , 1);
      handle_scancode(SLKEY_HINGEMOVED|KBUP   , 0);
      enable_irq(IRQ_GPIO_SWA);
      enable_irq(IRQ_GPIO_SWB);
      return;
    }
  } else {
    hinge_count = 0;
    last_hinge_state = cur_hinge_state;
  }
  init_timer(&hinge_delay_timer);
  hinge_delay_timer.expires = jiffies + HINGE_WAIT;
  add_timer(&hinge_delay_timer);
}

static void spitz_hinge_irq(int irq,void *dev_id,struct pt_regs *regs)
{
  //  printk("spitz_hinge_irq\n");
  if (last_hinge_state == read_hinge_state()) return;
  disable_irq(IRQ_GPIO_SWA);
  disable_irq(IRQ_GPIO_SWB);
  hinge_count = 0;
  sharppda_scan_hinge();
}

#ifdef CONFIG_PM
extern void sharppda_scan_logdriver_suspend(void);
extern void sharppda_scan_logdriver_resume(void);

extern int spitz_initial_keypress;

static int spitz_kbd_pm_callback(struct pm_dev *pm_dev,
					pm_request_t req, void *data)
{
	int i;
	int view_mode;

	switch (req) {
	case PM_SUSPEND:
		for (i = 0; i < KEY_SENSE_NUM; i++) {
			int gpio = sense2gpio[i];
			if (gpio >= 0)
				disable_irq(IRQ_GPIO(gpio));
		}
		disable_irq(IRQ_GPIO_ON_KEY);
		disable_irq(IRQ_GPIO_SYNC);
		disable_irq(IRQ_GPIO_SWA);
		disable_irq(IRQ_GPIO_SWB);
		sharppda_scan_logdriver_suspend();
		break;
	case PM_RESUME:
		// STROBE ALL -> High
		GPSR0  =  GPIO_G0_STROBE_BIT;
		GPDR0 |=  GPIO_G0_STROBE_BIT;
		GPSR1  =  GPIO_G1_STROBE_BIT;
		GPDR1 |=  GPIO_G1_STROBE_BIT;
		GPSR2  =  GPIO_G2_STROBE_BIT;
		GPDR2 |=  GPIO_G2_STROBE_BIT;
		GPSR3  =  GPIO_G3_STROBE_BIT;
		GPDR3 |=  GPIO_G3_STROBE_BIT;
		for (i = 0; i < KEY_SENSE_NUM; i++) {
			int gpio = sense2gpio[i];
			if (gpio >= 0)
				enable_irq(IRQ_GPIO(gpio));
		}
		view_mode = read_hinge_state();
		enable_irq(IRQ_GPIO_ON_KEY);
		enable_irq(IRQ_GPIO_SYNC);
		enable_irq(IRQ_GPIO_SWA);
		enable_irq(IRQ_GPIO_SWB);
		sharppda_scan_logdriver_resume();
		spitz_initial_keypress = -1;
		if (view_mode != 0x3)
		  spitz_hinge_irq(0,0,0);
		break;
	}
	return 0;
}
#endif

void spitz_kbd_hw_init(void)
{ 
	int i;

	sharppda_scan_logdriver_init();

	// STROBE ALL -> High
	// SENSE ALL -> In
	GPSR0  =  GPIO_G0_STROBE_BIT;
	GPSR1  =  GPIO_G1_STROBE_BIT;
	GPSR2  =  GPIO_G2_STROBE_BIT;
	GPSR3  =  GPIO_G3_STROBE_BIT;
	GPDR0 &= ~GPIO_G0_SENSE_BIT;
	GPDR1 &= ~GPIO_G1_SENSE_BIT;
	GPDR2 &= ~GPIO_G2_SENSE_BIT;
	GPDR3 &= ~GPIO_G3_SENSE_BIT;
	GPDR0 |=  GPIO_G0_STROBE_BIT;
	GPDR1 |=  GPIO_G1_STROBE_BIT;
	GPDR2 |=  GPIO_G2_STROBE_BIT;
	GPDR3 |=  GPIO_G3_STROBE_BIT;
	GEDR0 =  GPIO_G0_SENSE_BIT;
	GEDR1 =  GPIO_G1_SENSE_BIT;
	GEDR2 =  GPIO_G2_SENSE_BIT;
	GEDR3 =  GPIO_G3_SENSE_BIT;
	for (i = 0; i < KEY_STROBE_NUM; i++) {
		int gpio = strobe2gpio[i];
		if (gpio >= 0)
			GAFR(gpio) &= ~GAFR_bit(gpio);
	}
	for (i = 0; i < KEY_SENSE_NUM; i++) {
		int gpio = sense2gpio[i];
		if (gpio >= 0)
			GAFR(gpio) &= ~GAFR_bit(gpio);
	}

	/* SENSE:RisingEdge */
	for (i = 0; i < KEY_SENSE_NUM; i++) {
		int gpio = sense2gpio[i];
		if (gpio >= 0) {
			set_GPIO_IRQ_edge(gpio, GPIO_RISING_EDGE);
			if (request_irq(IRQ_GPIO(gpio), spitz_kbd_irq,
					SA_INTERRUPT, "keyboard", NULL)) {
				printk("Could not allocate KEYBD IRQ%d!\n", i);
			}
		}
	}

	// Power&Rec Button
	set_GPIO_IRQ_edge(GPIO_ON_KEY, GPIO_RISING_EDGE);
	set_GPIO_IRQ_edge(GPIO_SYNC, GPIO_RISING_EDGE);
	set_GPIO_IRQ_edge(GPIO_SWA, GPIO_BOTH_EDGES);
	set_GPIO_IRQ_edge(GPIO_SWB, GPIO_BOTH_EDGES);
	if (request_irq(IRQ_GPIO_ON_KEY, spitz_kbd_irq, SA_INTERRUPT, "On key", NULL) ||
	    request_irq(IRQ_GPIO_SWA, spitz_hinge_irq, SA_INTERRUPT, "Switch A", NULL) ||
	    request_irq(IRQ_GPIO_SWB, spitz_hinge_irq, SA_INTERRUPT, "Switch B", NULL) ||
	    request_irq(IRQ_GPIO_SYNC, spitz_kbd_irq, SA_INTERRUPT, "Sync key", NULL)) {
	  printk("Could not allocate KEYBD IRQ%d!\n", i);
	}


#ifdef CONFIG_PM
	spitz_kbd_pm_dev = pm_register(PM_SYS_DEV, 0, spitz_kbd_pm_callback);
#endif

	printk("keyboard initilaized.\n");
}

int spitz_kbd_translate(unsigned char scancode, unsigned char *keycode_p)
{

	/*
 	* We do this strangley to be more independent of 
 	* our headers..
 	*/
	
	*keycode_p = scancode & ~(KBDOWN | KBUP);

	return 1;
}

