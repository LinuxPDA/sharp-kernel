/*
 *  linux/include/asm-arm/arch-pxa/keyboard_tosa.h
 *  
 *  (C) Copyright 2004 Lineo Solutions, Inc.
 *  
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 * Based on:
 *
 *  linux/include/asm-arm/arch-pxa/keyboard_tosa.h
 *
 *  (C) Copyright 2001 Lineo Japan, Inc.
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 *  Based on:
 *  linux/include/asm-arm/arch-sa1100/keyboard_collie.h
 *  include/asm-arm/arch-linkup/keyboard.h
 *    Created by Xuejun Tao 2000, ISDCorp  www.isdcorp.com
 *
 * Changelog:
 *   04-13-2001 Lineo Japan, Inc.
 *   04-25-2001 Lineo Japan, Inc.
 *   10-23-2002 Sharp Corporation
 */
#ifndef __ASM_ARCH_KEYBOARD_SPITZ_H
#define __ASM_ARCH_KEYBOARD_SPITZ_H

#include <linux/spinlock.h>
#include <asm/arch/hardware.h>

/* =========================================================
 * !!! CAUTION !!!
 * Iris board without Keyboard Enhancements makes PA5 INTR
 * all times. So , you should disable PA5 INTR on such boards.
 * Define this option to run on such board.
 * ========================================================= */
#define SPITZ_WITHOUT_KEY_ENH_WORKAROUND
/* =========================================================
 * CAUTION ends.
 * ========================================================= */

/*
 * My driver now supports both keyboard interrupt driven or timer driven
 * Both modes work although keyboard interrupt mode is highly recommended.
 */
#define USE_KBD_IRQ	/* Use keyboard interrupt instead of timer */

#define KB_ROWS		8
#define KB_COLS		12

#define KBD_COL9_IS_USED_FOR_SIC
    /* KBDCOL9 Hardware is used as SIC SYS_CLK , so , it cannot be used */

#define ALL_AUX_COLS	0x00		/* no AUX Cols */

#define set_bits(var, mask, bits)	(var) = (((var) & ~(mask)) | (bits))

/*
 *  Be sure to change the if you increase the
 *  number of kbd rows...
 */
#define KEYCODE(r,c)	( ((r)<<4) + (c) + 1 )
//X #define	NR_KEYCODES	(KEYCODE(KB_ROWS-1,KB_COLS-1)+1)
#define KB_ROWMASK(r) (1 << (r))

 /*
  * KB_DELAY is used to allow the matrix 
  * to stabilize.., value is determined via
  * experimentation. 
  */

#define KB_DISCHARGE_DELAY	10
#define KB_ACTIVATE_DELAY	10

typedef struct {
    int in;   /* If the key down */
} kbd_keyinfo;

#define  KBUP    (0x80)
#define  KBDOWN  (0)

#define  KBSCANCDE(x,y) ((x) | (y))

#define CHARGE_VALUE 0x00FF
#define DISCHARGE_VALUE 0x0000
#define IRQ_STATE_CLEAR 0xFEFF
#define INIT_KCMD 0x0001



/*
 * We have a spinlock we use to ensure that keysdown
 * is consisent with kbd_state[]
 *
 * This is prolly overkill since the arm doesn't support SMP.
 */
// Yes, it is - WA  static spinlock_t kbd_spinlock;
extern spinlock_t kbd_spinlock;

 /*
  * #define for functions we can't make use of 
  */

#define kbd_leds(x)
#define kbd_setleds(x)
#define kbd_getledstate          (0)
extern int  spitz_kbd_translate(unsigned char sc,unsigned char *keycode_p);
#define kbd_translate(sc,kc,rm)  spitz_kbd_translate(sc,kc)
/*
 *#define kbd_sysrq_xlate()          (1)
 */
#define kbd_pretranslate(x,y)      (1)
#define kbd_unexpected_up(kc)        (0x80)
#define kbd_setkeycode(sc,kc)   (-EINVAL)
#define kbd_getkeycode(sc)      (-EINVAL)

extern void spitz_kbd_hw_init(void);
#define kbd_init_hw()		 spitz_kbd_hw_init()

extern void spitz_kbd_cleartable(void);

/*
 * I need to do something better for these two...
 * Sometime v. soon. I don't like these at all, as they 
 *   
 * don't look like fn calls. 
 */

#define kbd_disable_irq()        { \
                                    int flags; \
                                    spin_lock_irqsave(&kbd_spinlock,flags);


#define  kbd_enable_irq()           spin_unlock_irqrestore(&kbd_spinlock,flags); \
                               }


#if 0
/*
 *   row , col values for feature keys
 */
#define SPITZ_KEYPOS_2nd_ROW   0
#define SPITZ_KEYPOS_2nd_COL   1

#define SPITZ_KEYPOS_NUM_ROW   0
#define SPITZ_KEYPOS_NUM_COL   8

#define SPITZ_KEYPOS_LSHIFT_ROW 1
#define SPITZ_KEYPOS_LSHIFT_COL 0

#define SPITZ_KEYPOS_RSHIFT_ROW 1
#define SPITZ_KEYPOS_RSHIFT_COL 8

/*
 *   number of modifier status keys resolved on corgikb driver
 */
#define SPITZKB_MODIFIERS  5 /* for CapsLock / NumLock / 2ND / LSHIFT / RSHIFT */

#endif

#endif
