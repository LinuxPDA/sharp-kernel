/*
 * linux/drivers/char/l7200_keyb.c
 *
 * Keyboard driver for LinkUp 7200
 *
 * Copyright (C) 2001 Lineo, Inc.
 *
 * Based on:
 *   drivers/char/kbd_linkup.c - K
 *   Copyright (C) Roger Gammans  1998
 *   Written for Linkup 7200 by Xuejun Tao, ISDcorp
 *   Refer to kbd_7110.c (Cirrus Logic 7110 Keyboard driver)
 *
 * ChangeLog:
 *   03-20-2001 Lineo Japan, Inc. ...
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/tqueue.h>
#include <linux/kbd_ll.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/pm.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/arch/hardware.h>
#include <asm/arch/keyboard.h>

#ifdef CONFIG_PM
static struct pm_dev* l7200_kbd_pm_dev;
#endif

#define KB_ROWS		8
#define KB_COLS		14
#define KB_BASIC_COLS	12

#define AUX_COL_12	(1 << 4)	/* col 12 output	*/
#define AUX_COL_12_DIS	(1 << 5)	/* col 12 disable (1=disabled)	*/
#define AUX_COL_13	(1 << 6)	/* col 13 output	*/
#define AUX_COL_13_DIS	(1 << 7)	/* col 13 disable (1=disabled)	*/

#define ALL_AUX_COLS	0xF0		/* Bit mask for col 12/13	*/

#define set_bits(var, mask, bits)	(var) = (((var) & ~(mask)) | (bits))

#define KEYCODE(r,c)	( ((r)<<4) + (c) + 1 )
#define NR_KEYCODES	128
#define KB_ROWMASK(r)   (1 << (r))
#define KB_DELAY        16
#define KB_AUX_COL	(*(volatile unsigned char *)AUX_BASE)
#define KB_ROW 	        IO_PADR 

typedef struct { int in; } kbd_keyinfo;

static kbd_keyinfo kbdstate[NR_KEYCODES];

#if NR_KEYCODES>0x80 
#error Out of bits for scancode.. 
#endif

#define  KBUP    (0x80)
#define  KBDOWN  (0)

#define  KBSCANCDE(x,y) ((x) | (y))

static int keysdown = 0;

static struct tq_struct kbdhw_task;

spinlock_t kbd_spinlock;

static void l7200_kbd_press(int keycode)
{
	unsigned char newpress = 0;
	unsigned long flags;

	spin_lock_irqsave(&kbd_spinlock, flags);
	if (!kbdstate[keycode].in) {
		keysdown++; 
		newpress = 1;
	}

	kbdstate[keycode].in = 1;
	spin_unlock_irqrestore(&kbd_spinlock, flags);
  
	if (newpress) {
		handle_scancode(KBSCANCDE(keycode, KBDOWN), 1);
	}
}

static void l7200_kbd_release(int keycode)
{
	unsigned char newrelease = 0;
	unsigned long flags;

	spin_lock_irqsave(&kbd_spinlock, flags);
	if (kbdstate[keycode].in) {
		keysdown--; 
		newrelease = 1;
	}

	kbdstate[keycode].in = 0;
	spin_unlock_irqrestore(&kbd_spinlock, flags);
  
	if (newrelease) {
		handle_scancode(KBSCANCDE(keycode, KBUP), 0);
	}
}

static void l7200_kbd_tick(void* dummy)
{
	int col, row, rowd, count = 0;
	unsigned long flags;

	spin_lock_irqsave(&kbd_spinlock, flags);

	for (col = 0; col < KB_COLS; col++) {

		set_bits(KB_AUX_COL, ALL_AUX_COLS, 0);  
		udelay(KB_DELAY);

		set_bits(IO_KBKSR, KBDSCAN, KBSC_X);
		set_bits(KB_AUX_COL, ALL_AUX_COLS,
			AUX_COL_12_DIS | AUX_COL_13_DIS );

		if (col < KB_BASIC_COLS) {
			set_bits(IO_KBKSR, KBDSCAN, KBSC_COL0 + col);
		}
		else {
			if (col == 12) {
				set_bits(KB_AUX_COL, ALL_AUX_COLS,
					 AUX_COL_12|AUX_COL_13_DIS);
			}
			else { /* col 13 */ 
				set_bits(KB_AUX_COL, ALL_AUX_COLS,
					 AUX_COL_13|AUX_COL_12_DIS);
			}
		}
		udelay(KB_DELAY);

		rowd = KB_ROW;
		for (row = 0; row < KB_ROWS; row++) {
			if (rowd & KB_ROWMASK(row)) {
				l7200_kbd_press(KEYCODE(row, col));
				count++;
			}
			else {
				if (kbdstate[KEYCODE(row, col)].in) {
					l7200_kbd_release(KEYCODE(row, col));
				}
			}
		}
        }

	set_bits(IO_KBKSR, KBDSCAN, KBSC_HI);
	set_bits(KB_AUX_COL, ALL_AUX_COLS, AUX_COL_12|AUX_COL_13);  

	if (count > 0) {
		queue_task(&kbdhw_task, &tq_timer);
	}

	spin_unlock_irqrestore(&kbd_spinlock, flags);
}


void l7200_kbd_enable_irq(void)
{
	IO_PAIMR = 0xFF;
	enable_irq(IRQ_GPIO);
}

void l7200_kbd_disable_irq(void)
{
	IO_PAIMR = 0x00;
	disable_irq(IRQ_GPIO);
}

static void l7200_kbd_irq(int irq,void *dev_id,struct pt_regs *regs)
{
	l7200_kbd_disable_irq();

	l7200_kbd_tick(NULL);
	IO_PAIMR  = 0;
	IO_PAECLR = 0xFF;

	l7200_kbd_enable_irq();
}

int l7200_kbd_translate(unsigned char scancode,
			unsigned char* keycode_p,
			int rm)
{
	*keycode_p = scancode & ~(KBDOWN | KBUP);
	return 1;
}

static void l7200_kbd_power_off()
{
}

static void l7200_kbd_power_on()
{
	IO_PAECLR = 0xFF;
	IO_PAIMR  = 0x00;
	IO_PAESNR = 0x00;
	IO_PAEENR = 0xFF;
}

#ifdef CONFIG_PM
static int l7200_kbd_pm_callback(struct pm_dev *pm_dev,
				 pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
		l7200_kbd_power_off();
		break;
	case PM_RESUME:
		l7200_kbd_power_on();
                break;
	}
	return 0;
}
#endif

void l7200_kbd_init_hw(void) 
{
	spin_lock_init(&kbd_spinlock);

	kbdhw_task.routine = l7200_kbd_tick;
	kbdhw_task.sync = 0;

	if (request_irq(IRQ_GPIO, l7200_kbd_irq,
			SA_INTERRUPT, "keyboard", NULL)) {
		printk("l7200_kbd_hw_init: unable to allocate interrupt\n");
		return;
	}

#ifdef CONFIG_PM
	l7200_kbd_pm_dev = pm_register(PM_SYS_DEV, 0, l7200_kbd_pm_callback);
#endif
	l7200_kbd_power_on();

	l7200_kbd_enable_irq();

	printk("keyboard initialized.\n");
}
