/*
 * linux/drivers/char/corgi_keyb.c 
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
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

#ifndef CORGI_KBD_MAJOR
# define CORGI_KBD_MAJOR 61
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
#define corgi_kbd_tick  sharppda_scan_key_gpio

/*
 * physical driver depending on CORGI target.
 */
void corgi_kbd_discharge_all(void)
{
	// STROBE All HiZ
	GPCR2  = GPIO_ALL_STROBE_BIT;
	GPDR2 &= ~GPIO_ALL_STROBE_BIT;
}

void corgi_kbd_charge_all(void)
{
}

void corgi_kbd_activate_all(void)
{
	// STROBE ALL -> High
	GPSR2  = GPIO_ALL_STROBE_BIT;
	GPDR2 |= GPIO_ALL_STROBE_BIT;

	udelay(KB_DISCHARGE_DELAY);

	// STATE CLEAR
	GEDR1 |= GPIO_HIGH_SENSE_BIT;
	GEDR2 |= GPIO_LOW_SENSE_BIT;
}

void corgi_kbd_activate_col(int col)
{
	// STROBE col -> High, not col -> HiZ
	GPSR2 = GPIO_STROBE_BIT(col);
	GPDR2 = (GPDR2 & ~GPIO_ALL_STROBE_BIT) | GPIO_STROBE_BIT(col);
}

void corgi_kbd_reset_col(int col)
{
	// STROBE col -> Low
	GPCR2 = GPIO_STROBE_BIT(col);
	// STROBE col -> out, not col -> HiZ
	GPDR2 = (GPDR2 & ~GPIO_ALL_STROBE_BIT) | GPIO_STROBE_BIT(col);
}


#ifdef USE_KBD_IRQ

static void corgi_kbd_irq(int irq,void *dev_id,struct pt_regs *regs)
{
#ifdef CONFIG_APM
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif
	corgi_kbd_tick(NULL);

#if CONFIG_APM  // auto power/light cancel
	autoPowerCancel = 0;
	autoLightCancel = 0;
#endif

}
#endif

#define	USE_WAKEUP_BUTTON
#ifdef USE_WAKEUP_BUTTON
#include <asm/sharp_keycode.h>
#define	WAKEUP_BUTTON_SCANCODE	SLKEY_SYNCSTART	/* No.117 */
#define	WAKEUP_BUTTON_DELAY	(HZ*100/1000)
extern u32 apm_wakeup_src_mask;
extern u32 apm_wakeup_factor;

static void inline corgi_wakeup_button_send(void)
{
	handle_scancode(WAKEUP_BUTTON_SCANCODE|KBDOWN,1);
	handle_scancode(WAKEUP_BUTTON_SCANCODE|KBUP,0);
}

static void corgi_wakeup_button_timer_proc(unsigned long param)
{
	if( GPLR(GPIO_WAKEUP) & GPIO_bit(GPIO_WAKEUP) )
		return;
	corgi_wakeup_button_send();
}

static struct timer_list  corgi_wakeup_button_timer =
	{function:corgi_wakeup_button_timer_proc};

static void corgi_wakeup_button_irq(int irq,void *dev_id,struct pt_regs *regs)
{
	if( GPLR(GPIO_WAKEUP) & GPIO_bit(GPIO_WAKEUP) )
		return;
	del_timer(&corgi_wakeup_button_timer);
	corgi_wakeup_button_timer.expires = jiffies + WAKEUP_BUTTON_DELAY;
	add_timer(&corgi_wakeup_button_timer);
}

static void corgi_wakeup_button_init(void)
{
	init_timer(&corgi_wakeup_button_timer);

	/* GPOI24(int):IN */
	GPDR(GPIO_WAKEUP) &= ~GPIO_bit(GPIO_WAKEUP);

	/* GPIO24:not Alternate */
	set_GPIO_mode(GPIO_WAKEUP | GPIO_IN);

	/* GPIO24:FallingEdge */
	set_GPIO_IRQ_edge(GPIO_WAKEUP, GPIO_FALLING_EDGE );

	if( request_irq(IRQ_GPIO_WAKEUP, corgi_wakeup_button_irq,
		SA_INTERRUPT, "WakeupButton", NULL)) {
		printk("Could not allocate Wakeup-Button IRQ!\n");
	}
}

static void corgi_wakeup_button_exit()
{
	free_irq(IRQ_GPIO_WAKEUP, NULL);
	del_timer(&corgi_wakeup_button_timer);
}
#endif // USE_WAKEUP_BUTTON

#ifdef CONFIG_PM
extern void sharppda_scan_logdriver_suspend(void);
extern void sharppda_scan_logdriver_resume(void);

extern int corgi_initial_keypress;

static int corgi_kbd_pm_callback(struct pm_dev *pm_dev,
					pm_request_t req, void *data)
{
	int i;

	switch (req) {
	case PM_SUSPEND:
		for (i = 0; i < KEY_SENSE_NUM; i++) {
			disable_irq(IRQ_GPIO_KEY_SENSE(i));
		}
		sharppda_scan_logdriver_suspend();
#ifdef USE_WAKEUP_BUTTON
		apm_wakeup_src_mask |= GPIO_bit(GPIO_WAKEUP);
		disable_irq(IRQ_GPIO_WAKEUP);
		del_timer(&corgi_wakeup_button_timer);
#endif // USE_WAKEUP_BUTTON
		break;
	case PM_RESUME:
		// STROBE ALL -> High
		GPSR2  = GPIO_ALL_STROBE_BIT;
		GPDR2 |= GPIO_ALL_STROBE_BIT;
		for (i = 0; i < KEY_SENSE_NUM; i++) {
			enable_irq(IRQ_GPIO_KEY_SENSE(i));
		}
		sharppda_scan_logdriver_resume();
		corgi_initial_keypress = -1;
#ifdef USE_WAKEUP_BUTTON
		if( apm_wakeup_factor & GPIO_bit(GPIO_WAKEUP) )
			corgi_wakeup_button_send();
		enable_irq(IRQ_GPIO_WAKEUP);
#endif // USE_WAKEUP_BUTTON
		break;
	}
	return 0;
}
#endif

void corgi_kbd_hw_init(void)
{ 
	int i;

	sharppda_scan_logdriver_init();

	// STROBE ALL -> High
	// SENSE ALL -> In
	GPSR2    = GPIO_ALL_STROBE_BIT;
	GPDR1   &= ~GPIO_HIGH_SENSE_BIT;
	GPDR2   &= ~GPIO_LOW_SENSE_BIT;
	GPDR2   |= GPIO_ALL_STROBE_BIT;
	GEDR1   |= GPIO_HIGH_SENSE_BIT;
	GEDR2   |= GPIO_LOW_SENSE_BIT;
	GAFR1_U &= ~GAFR_HIGH_SENSE_BIT;
	GAFR2_L &= ~(GAFR_LOW_SENSE_BIT | GAFR_ALL_STROBE_BIT);

	/* SENSE:RisingEdge */
	for (i = 0; i < KEY_SENSE_NUM; i++) {
		set_GPIO_IRQ_edge(GPIO_KEY_SENSE(i), GPIO_RISING_EDGE);
		if (request_irq(IRQ_GPIO_KEY_SENSE(i), corgi_kbd_irq,
						SA_INTERRUPT, "keyboard", NULL)) {
			printk("Could not allocate KEYBD IRQ%d!\n", i);
		}
	}

#ifdef CONFIG_PM
	corgi_kbd_pm_dev = pm_register(PM_SYS_DEV, 0, corgi_kbd_pm_callback);
#endif

#ifdef USE_WAKEUP_BUTTON
	corgi_wakeup_button_init();
#endif // USE_WAKEUP_BUTTON

	printk("keyboard initilaized.\n");
}

int corgi_kbd_translate(unsigned char scancode, unsigned char *keycode_p)
{

	/*
 	* We do this strangley to be more independent of 
 	* our headers..
 	*/
	
	*keycode_p = scancode & ~(KBDOWN | KBUP);

	return 1;
}

