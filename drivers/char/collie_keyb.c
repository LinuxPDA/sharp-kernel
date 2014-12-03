/*
 * linux/drivers/char/collie_keyb.c 
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   drivers/char/iris_keyb.c
 *   drivers/char/kbd_linkup.c - Keyboard Driver for Linkup-7200
 *   Copyright (C) Roger Gammans  1998
 *   Written for Linkup 7200 by Xuejun Tao, ISDcorp
 *   Refer to kbd_7110.c (Cirrus Logic 7110 Keyboard driver)
 *
 * ChangeLong:
 *  04-25-2001 Lineo Japan, Inc.
 *  30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
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

#ifdef USE_KBD_IRQ		// could be defined in asm/arch/keyboard.h
#include <asm/irq.h>	// for linkup keyboard interrupt
#endif

#include <asm/arch/irqs.h>
#include <asm/arch/gpio.h>

#ifndef COLLIE_KBD_MAJOR
# define COLLIE_KBD_MAJOR 61
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
static struct pm_dev *collie_kbd_pm_dev;
#endif

/*
 * common logical driver definition
 */
extern void sharppda_kbd_press(int keycode);
extern void sharppda_kbd_release(int keycode);
extern void sharppda_scan_logdriver_init(void);
extern void sharppda_scan_key_gpio(void* dummy);
#define collie_kbd_tick  sharppda_scan_key_gpio

/*
 * physical driver depending on COLLIE target.
 */
void collie_kbd_discharge_all(void)
{
}

void collie_kbd_charge_all(void)
{
					// kstrb_b -> High
	LCM_KSC = CHARGE_VALUE;
}

void collie_kbd_activate_all(void)
{
					// kstrb_b -> Low
	LCM_KSC = DISCHARGE_VALUE;
					// STATE CLEAR
	LCM_KIC &= IRQ_STATE_CLEAR;
}

void collie_kbd_activate_col(int col)
{
	unsigned short nset;
	unsigned short nbset;

	nset = 0xFF & ~(1 << col);
	nbset = (nset << 8) + nset;
	LCM_KSC = nbset;
}

void collie_kbd_reset_col(int col)
{
	unsigned short nbset;

	nbset = ((0xFF & ~(1 << col)) << 8) + 0xFF;
	LCM_KSC = nbset;
}


#ifdef USE_KBD_IRQ

static void collie_kbd_irq(int irq,void *dev_id,struct pt_regs *regs)
{
#if 1
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif
	collie_kbd_tick(NULL);

#if 1  // auto power/light cancel
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

static void inline collie_wakeup_button_send(void)
{
	handle_scancode(WAKEUP_BUTTON_SCANCODE|KBDOWN,1);
	handle_scancode(WAKEUP_BUTTON_SCANCODE|KBUP,0);
}

static void collie_wakeup_button_timer_proc(unsigned long param)
{
	if( GPLR & GPIO_WAKEUP )
		return;
	collie_wakeup_button_send();
}

static struct timer_list  collie_wakeup_button_timer =
	{function:collie_wakeup_button_timer_proc};

static void collie_wakeup_button_irq(int irq,void *dev_id,struct pt_regs *regs)
{
	if( GPLR & GPIO_WAKEUP )
		return;
	del_timer(&collie_wakeup_button_timer);
	collie_wakeup_button_timer.expires = jiffies + WAKEUP_BUTTON_DELAY;
	add_timer(&collie_wakeup_button_timer);
}

static void collie_wakeup_button_init(void)
{
	init_timer(&collie_wakeup_button_timer);

	/* GPOI24(int):IN */
	GPDR &= ~GPIO_WAKEUP;

	/* GPIO24:not Alternate */
	GAFR &= ~GPIO_WAKEUP;

	/* GPIO24:FallingEdge */
	set_GPIO_IRQ_edge(GPIO_WAKEUP, GPIO_FALLING_EDGE );

	if( request_irq(IRQ_GPIO_WAKEUP, collie_wakeup_button_irq,
		SA_INTERRUPT, "WakeupButton", NULL)) {
		printk("Could not allocate Wakeup-Button IRQ!\n");
	}
}

static void collie_wakeup_button_exit()
{
	free_irq(IRQ_GPIO_WAKEUP, NULL);
	del_timer(&collie_wakeup_button_timer);
}
#endif // USE_WAKEUP_BUTTON

#ifdef CONFIG_PM
extern void sharppda_scan_logdriver_suspend(void);
extern void sharppda_scan_logdriver_resume(void);

static int collie_kbd_pm_callback(struct pm_dev *pm_dev,
					pm_request_t req, void *data)
{
        switch (req) {
        case PM_SUSPEND:
		disable_irq(LCM_IRQ_KEY);
		sharppda_scan_logdriver_suspend();
#ifdef USE_WAKEUP_BUTTON
		apm_wakeup_src_mask |= GPIO_WAKEUP;
		disable_irq(IRQ_GPIO_WAKEUP);
		del_timer(&collie_wakeup_button_timer);
#endif // USE_WAKEUP_BUTTON
       		break;
        case PM_RESUME:
		LCM_KCMD = INIT_KCMD;
		LCM_KSC = 0x0000; // default
		LCM_KIC = 0x0000; // default
		enable_irq(LCM_IRQ_KEY);
		sharppda_scan_logdriver_resume();
#ifdef USE_WAKEUP_BUTTON
		if( apm_wakeup_factor & GPIO_WAKEUP )
			collie_wakeup_button_send();
		enable_irq(IRQ_GPIO_WAKEUP);
#endif // USE_WAKEUP_BUTTON
		break;
	}
	return 0;
}
#endif

void collie_kbd_hw_init(void)
{ 
	sharppda_scan_logdriver_init();

	// kstrb_b -> low

	LCM_KCMD = INIT_KCMD;
	LCM_KSC = 0x0000; // default
	LCM_KIC = 0x0000; // default

	if (request_irq(LCM_IRQ_KEY, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) {
		printk("Could not allocate KEYBD IRQ!\n");
	}

#ifdef CONFIG_PM
	collie_kbd_pm_dev = pm_register(PM_SYS_DEV, 0, collie_kbd_pm_callback);
#endif

#ifdef USE_WAKEUP_BUTTON
	collie_wakeup_button_init();
#endif // USE_WAKEUP_BUTTON

	printk("keyboard initilaized.\n");
}

int collie_kbd_translate(unsigned char scancode, unsigned char *keycode_p)
{

	/*
 	* We do this strangley to be more independent of 
 	* our headers..
 	*/
	
	*keycode_p = scancode & ~(KBDOWN | KBUP);

	return 1;
}

