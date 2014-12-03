/*
 * linux/drivers/char/tosa_keyb.c 
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
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

#ifdef USE_KBD_IRQ		// could be defined in asm/arch/keyboard.h
#include <asm/irq.h>	// for linkup keyboard interrupt
#endif

#include <asm/arch/irqs.h>

#ifndef TOSA_KBD_MAJOR
# define TOSA_KBD_MAJOR 61
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
static struct pm_dev *corgi_kbd_pm_dev;
#endif

/*
 * common logical driver definition
 */
extern void sharppda_kbd_press(int keycode);
extern void sharppda_kbd_release(int keycode);
extern void sharppda_scan_logdriver_init(void);
extern void sharppda_scan_key_gpio(void* dummy);
#define tosa_kbd_tick  sharppda_scan_key_gpio

/*
 * physical driver depending on TOSA target.
 */
void tosa_kbd_discharge_all(void)
{
	// STROBE All HiZ
	GPCR1  = GPIO_HIGH_STROBE_BIT;
	GPDR1 &= ~GPIO_HIGH_STROBE_BIT;
	GPCR2  = GPIO_LOW_STROBE_BIT;
	GPDR2 &= ~GPIO_LOW_STROBE_BIT;
}

void tosa_kbd_charge_all(void)
{
}

void tosa_kbd_activate_all(void)
{
	// STROBE ALL -> High
	GPSR1  = GPIO_HIGH_STROBE_BIT;
	GPDR1 |= GPIO_HIGH_STROBE_BIT;
	GPSR2  = GPIO_LOW_STROBE_BIT;
	GPDR2 |= GPIO_LOW_STROBE_BIT;

	udelay(KB_DISCHARGE_DELAY);

	// STATE CLEAR
	GEDR2 |= GPIO_ALL_SENSE_BIT;
}

void tosa_kbd_activate_col(int col)
{
  if (col<=5) {
	// STROBE col -> High, not col -> HiZ
	GPSR1 = GPIO_STROBE_BIT(col);
 	GPDR1 = (GPDR1 & ~GPIO_HIGH_STROBE_BIT) | GPIO_STROBE_BIT(col);
  } else {
	// STROBE col -> High, not col -> HiZ
	GPSR2 = GPIO_STROBE_BIT(col);
 	GPDR2 = (GPDR2 & ~GPIO_LOW_STROBE_BIT) | GPIO_STROBE_BIT(col);
  }
}

void tosa_kbd_reset_col(int col)
{
  if (col<=5) {
	// STROBE col -> Low
	GPCR1 = GPIO_STROBE_BIT(col);
	// STROBE col -> out, not col -> HiZ
	GPDR1 = (GPDR1 & ~GPIO_HIGH_STROBE_BIT) | GPIO_STROBE_BIT(col);
  } else {
	// STROBE col -> Low
	GPCR2 = GPIO_STROBE_BIT(col);
	// STROBE col -> out, not col -> HiZ
	GPDR2 = (GPDR2 & ~GPIO_LOW_STROBE_BIT) | GPIO_STROBE_BIT(col);
    
  }
}


#ifdef USE_KBD_IRQ

static void tosa_kbd_irq(int irq,void *dev_id,struct pt_regs *regs)
{
#ifdef CONFIG_APM
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif
	tosa_kbd_tick(NULL);

#if CONFIG_APM  // auto power/light cancel
	autoPowerCancel = 0;
	autoLightCancel = 0;
#endif

}
#endif


#ifdef CONFIG_PM
extern void sharppda_scan_logdriver_suspend(void);
extern void sharppda_scan_logdriver_resume(void);

extern int tosa_initial_keypress;

static int tosa_kbd_pm_callback(struct pm_dev *pm_dev,
					pm_request_t req, void *data)
{
	int i;

	switch (req) {
	case PM_SUSPEND:
		for (i = 0; i < KEY_SENSE_NUM; i++) {
			disable_irq(IRQ_GPIO_KEY_SENSE(i));
		}
		disable_irq(IRQ_GPIO_ON_KEY);
		disable_irq(IRQ_GPIO_RECORD_BTN);
		disable_irq(IRQ_GPIO_SYNC);
		sharppda_scan_logdriver_suspend();
		break;
	case PM_RESUME:
		// STROBE ALL -> High
		GPSR1  = GPIO_HIGH_STROBE_BIT;
		GPSR2  = GPIO_LOW_STROBE_BIT;
		GPDR1 |= GPIO_HIGH_STROBE_BIT;
		GPDR2 |= GPIO_LOW_STROBE_BIT;
		for (i = 0; i < KEY_SENSE_NUM; i++) {
			enable_irq(IRQ_GPIO_KEY_SENSE(i));
		}
		enable_irq(IRQ_GPIO_ON_KEY);
		enable_irq(IRQ_GPIO_RECORD_BTN);
		enable_irq(IRQ_GPIO_SYNC);
		sharppda_scan_logdriver_resume();
		tosa_initial_keypress = -1;
		break;
	}
	return 0;
}
#endif

void tosa_kbd_hw_init(void)
{ 
	int i;

	sharppda_scan_logdriver_init();

	// STROBE ALL -> High
	// SENSE ALL -> In
	GPSR1    = GPIO_HIGH_STROBE_BIT;
	GPSR2    = GPIO_LOW_STROBE_BIT;
	GPDR2   &= ~GPIO_ALL_SENSE_BIT;
	GPDR1   |= GPIO_HIGH_STROBE_BIT;
	GPDR2   |= GPIO_LOW_STROBE_BIT;
	GEDR2   |= GPIO_ALL_SENSE_BIT;
	GAFR1_U &= ~GAFR_HIGH_STROBE_BIT;
	GAFR2_L &= ~(GAFR_LOW_STROBE_BIT | GAFR_ALL_SENSE_BIT);

	/* SENSE:RisingEdge */
	for (i = 0; i < KEY_SENSE_NUM; i++) {
		set_GPIO_IRQ_edge(GPIO_KEY_SENSE(i), GPIO_RISING_EDGE);
		if (request_irq(IRQ_GPIO_KEY_SENSE(i), tosa_kbd_irq,
						SA_INTERRUPT, "keyboard", NULL)) {
			printk("Could not allocate KEYBD IRQ%d!\n", i);
		}
	}
	// Power&Rec Button
	set_GPIO_IRQ_edge(GPIO_ON_KEY, GPIO_FALLING_EDGE);
	set_GPIO_IRQ_edge(GPIO_RECORD_BTN, GPIO_FALLING_EDGE);
	set_GPIO_IRQ_edge(GPIO_SYNC, GPIO_FALLING_EDGE);
	if (request_irq(IRQ_GPIO_ON_KEY, tosa_kbd_irq, SA_INTERRUPT, "On key", NULL) ||
	    request_irq(IRQ_GPIO_RECORD_BTN, tosa_kbd_irq, SA_INTERRUPT, "Record key", NULL) ||
	    request_irq(IRQ_GPIO_SYNC, tosa_kbd_irq, SA_INTERRUPT, "Sync key", NULL)) {
	  printk("Could not allocate KEYBD IRQ%d!\n", i);
	}


#ifdef CONFIG_PM
	corgi_kbd_pm_dev = pm_register(PM_SYS_DEV, 0, tosa_kbd_pm_callback);
#endif

	printk("keyboard initilaized.\n");
}

int tosa_kbd_translate(unsigned char scancode, unsigned char *keycode_p)
{

	/*
 	* We do this strangley to be more independent of 
 	* our headers..
 	*/
	
	*keycode_p = scancode & ~(KBDOWN | KBUP);

	return 1;
}

