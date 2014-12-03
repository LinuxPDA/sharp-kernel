/*
 *  linux/include/asm-arm/arch-pxa/keyboard_discovery.h : Discovery keyborad driver
 *  
 *  Copyright (C) 2002 Richard Rau (richard.rau@msa.hinet.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *  
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 *  Based on:
 *  linux/include/asm-arm/arch-sa1100/keyboard_collie.h
 *    Created by Lineo Japan, Inc.
 *  
 */
#ifndef __ASM_ARCH_KEYBOARD_COLLIE_H
#define __ASM_ARCH_KEYBOARD_COLLIE_H

#include <linux/spinlock.h>
#include <asm/arch/hardware.h>

/* =========================================================
 * !!! CAUTION !!!
 * Iris board without Keyboard Enhancements makes PA5 INTR
 * all times. So , you should disable PA5 INTR on such boards.
 * Define this option to run on such board.
 * ========================================================= */
#define COLLIE_WITHOUT_KEY_ENH_WORKAROUND
/* =========================================================
 * CAUTION ends.
 * ========================================================= */

/*
 * My driver now supports both keyboard interrupt driven or timer driven
 * Both modes work although keyboard interrupt mode is highly recommended.
 */
#define USE_KBD_IRQ	/* Use keyboard interrupt instead of timer */

#define KB_ROWS		16
#define KB_COLS		8

#define KBD_COL9_IS_USED_FOR_SIC
    /* KBDCOL9 Hardware is used as SIC SYS_CLK , so , it cannot be used */

#define ALL_AUX_COLS	0x00		/* no AUX Cols */

#define set_bits(var, mask, bits)	(var) = (((var) & ~(mask)) | (bits))

/*
 *  Be sure to change the if you increase the
 *  number of kbd rows...
 */
#define KEYCODE(r,c)	( ((c)<<4) + (r) + 1 )
#define	NR_KEYCODES	(KEYCODE(KB_ROWS-1,KB_COLS-1)+1)

#define KB_ROWMASK(r) (1 << (r))

 /*
  * KB_DELAY is used to allow the matrix 
  * to stabilize.., value is determined via
  * experimentation. 
  */

#define KB_DELAY   8

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
extern int  collie_kbd_translate(unsigned char sc,unsigned char *keycode_p);
#define kbd_translate(sc,kc,rm)  collie_kbd_translate(sc,kc)
/*
 *#define kbd_sysrq_xlate()          (1)
 */
#define kbd_pretranslate(x,y)      (1)
#define kbd_unexpected_up(kc)        (0x80)
#define kbd_setkeycode(sc,kc)   (-EINVAL)
#define kbd_getkeycode(sc)      (-EINVAL)

extern void collie_kbd_hw_init(void);
#define kbd_init_hw()		 collie_kbd_hw_init()

extern void collie_kbd_cleartable(void);

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


/* data structure for raw keyboard event */

#if 0 /* no more used */

typedef enum {
  COLLIEKBD_KEYUP,
  COLLIEKBD_KEYDOWN,
} collie_kbd_event_type;
typedef struct collie_kbd_event_struct {
  collie_kbd_event_type which;
  int keycode;
  int resolved_keycode;
} collie_kbd_event_struct;

typedef enum {
  COLLIE_SCANKEY_2NDF,
  COLLIE_SCANKEY_3RDF
} collie_kbd_feature_keys;
typedef struct collie_kbd_scan_struct {
  collie_kbd_feature_keys which;
  int scan_result;
} collie_kbd_scan_struct;

/* IOCTL code for keyboard event */
#define COLLIEKBD_IOCTL_CODE_BASE 0x56a0
#define COLLIEKBD_GETEVENT    (COLLIEKBD_IOCTL_CODE_BASE+1)
#define COLLIEKBD_WAITEVENT   (COLLIEKBD_IOCTL_CODE_BASE+2)
#define COLLIEKBD_SCANKEY     (COLLIEKBD_IOCTL_CODE_BASE+3)
#define COLLIEKBD_RESETMODIF  (COLLIEKBD_IOCTL_CODE_BASE+4)

/*
 *   Wrappered Keycodes for raw-access driver
 */
#define RAWKEY_01     1           /* menu */
#define RAWKEY_02     2           /* 2nd */
#define RAWKEY_03     3           /* space */
#define RAWKEY_04     4           /* BS */
#define RAWKEY_05     5           /* sym -- NumLock */
#define RAWKEY_06     6           /* shift -- caps */
#define RAWKEY_07     7           /* y -- euro */
#define RAWKEY_08     8           /* x -- cut */
#define RAWKEY_09     9           /* c -- copy */
#define RAWKEY_10     10          /* v -- paste */
#define RAWKEY_11     11          /* b -- ? */
#define RAWKEY_12     12          /* n -- , */
#define RAWKEY_13     13          /* m -- . */
#define RAWKEY_14     14          /* shift */
#define RAWKEY_15     15          /* return */
#define RAWKEY_16     16          /* a -- _ */
#define RAWKEY_17     17          /* s -- ! */
#define RAWKEY_18     18          /* d -- a(um) */
#define RAWKEY_19     19          /* f -- u(um) */
#define RAWKEY_20     20          /* g -- o(um) */
#define RAWKEY_21     21          /* h -- A(um) */ 
#define RAWKEY_22     22          /* j -- U(um) */
#define RAWKEY_23     23          /* k -- O(um) */
#define RAWKEY_24     24          /* l -- sz */
#define RAWKEY_25     25          /* q -- 1 */
#define RAWKEY_26     26          /* w -- 2 */
#define RAWKEY_27     27          /* e -- 3 */
#define RAWKEY_28     28          /* r -- 4 */
#define RAWKEY_29     29          /* t -- 5 */
#define RAWKEY_30     30          /* z -- 6 */
#define RAWKEY_31     31          /* u -- 7 */
#define RAWKEY_32     32          /* i -- 8 */
#define RAWKEY_33     33          /* o -- 9 */
#define RAWKEY_34     34          /* p -- 0 */
#define RAWKEY_35     35          /* Phone */
#define RAWKEY_36     36          /* Esc */
#define RAWKEY_37     37          /* Left */
#define RAWKEY_38     38          /* Up */
#define RAWKEY_39     39          /* Down */
#define RAWKEY_40     40          /* Right */
#define RAWKEY_41     41          /* Enter */
#define RAWKEY_42     42          /* Mail */
#define RAWKEY_43     43          /* Home */
#define RAWKEY_44     44          /* ? */
#define RAWKEY_45     45          /* , */
#define RAWKEY_46     46          /* . */
#define RAWKEY_47     47          /*  _ */
#define RAWKEY_48     48          /* ! */
#define RAWKEY_49     49          /* a(um) */
#define RAWKEY_50     50          /* u(um) */
#define RAWKEY_51     51          /* o(um) */
#define RAWKEY_52     52          /* A(um) */ 
#define RAWKEY_53     53          /* U(um) */
#define RAWKEY_54     54          /* O(um) */
#define RAWKEY_55     55          /* sz */
#define RAWKEY_56     56          /* 1 */
#define RAWKEY_57     57          /* 2 */
#define RAWKEY_58     58          /* 3 */
#define RAWKEY_59     59          /* 4 */
#define RAWKEY_60     60          /* 5 */
#define RAWKEY_61     61          /* 6 */
#define RAWKEY_62     62          /* 7 */
#define RAWKEY_63     63          /* 8 */
#define RAWKEY_64     64          /* 9 */
#define RAWKEY_65     65          /* 0 */
#define RAWKEY_66     66          /* euro */
#define RAWKEY_67     67          /* caps */
#define RAWKEY_68     68          /* cut */
#define RAWKEY_69     69          /* copy */
#define RAWKEY_70     70          /* paste */
#define RAWKEY_71     71          /* tab */
#define RAWKEY_72     72          /* NumLock */
#define RAWKEY_73     73          /* Control */
#define RAWKEY_74     74          /* < */
#define RAWKEY_75     75          /* > */
#define RAWKEY_76     76          /* { */
#define RAWKEY_77     77          /* } */
#define RAWKEY_78     78          /* ` */
#define RAWKEY_79     79          /* \ */
#define RAWKEY_80     80          /* ; */
#define RAWKEY_81     81          /* : */
#define RAWKEY_82     82          /* [ */
#define RAWKEY_83     83          /* ] */
#define RAWKEY_84     84          /* ' */
#define RAWKEY_85     85          /* " */
#define RAWKEY_86     86          /* | */
#define RAWKEY_87     87          /* = */
#define RAWKEY_88     88          /* - */
#define RAWKEY_89     89          /* _ */
#define RAWKEY_90     90          /* + */
#define RAWKEY_91     91          /* / */
#define RAWKEY_92     92          /* ( */
#define RAWKEY_93     93          /* ) */
#define RAWKEY_94     94          /* @ */
#define RAWKEY_95     95          /* # */
#define RAWKEY_96     96          /* $ */
#define RAWKEY_97     97          /* % */
#define RAWKEY_98     98          /* ^ */
#define RAWKEY_99     99          /* & */
#define RAWKEY_100   100          /* * */
#endif /* no more used */

/*
 *   row , col values for feature keys
 */
#define COLLIE_KEYPOS_2nd_ROW   0
#define COLLIE_KEYPOS_2nd_COL   1

#define COLLIE_KEYPOS_NUM_ROW   0
#define COLLIE_KEYPOS_NUM_COL   8

#define COLLIE_KEYPOS_LSHIFT_ROW 1
#define COLLIE_KEYPOS_LSHIFT_COL 0

#define COLLIE_KEYPOS_RSHIFT_ROW 1
#define COLLIE_KEYPOS_RSHIFT_COL 8

/*
 *   number of modifier status keys resolved on colliekb driver
 */
#define COLLIEKB_MODIFIERS  5 /* for CapsLock / NumLock / 2ND / LSHIFT / RSHIFT */

#endif

#if 0
/* in standard QT environment */
#define	Ap_key0			87	/* menu   */
#define	Ap_key1			88	/* home   */
#define	Ap_key2			39	/* ok     */
#define	Ap_key3			27	/* cancel */
#define	Up_key			11	/* up     */
#define	Down_key		12	/* down   */
#define	Left_key		13	/* left   */
#define	Right_key		14	/* right  */
#define	Act_key			91	/* action */
#endif

#if 1 /* enable when sharp QT in */
/* in Sharp QT environment */
#define	Ap_key0			29	/* menu   */
#define	Ap_key1			40	/* home   */
#define	Ap_key2			39	/* ok     */
#define	Ap_key3			34	/* cancel */
#define	Up_key			0x23	/* up     */
#define	Down_key		0x26	/* down   */
#define	Left_key		0x25	/* left   */
#define	Right_key		0x24	/* right  */
#define	Act_key			91	/* action */
#define Sync_key		0x75	/* sync key */
#endif //ned

#define	Key_mask		0x3ff	/* filter */
#define	key_qty			9		/* total keys qty */
#define Ap_mask			0x00f	/* Ap keys filter */
#define	Dir_mask		0x0f0	/* Direction keys filter */
#define	Act_mask		0x300	/* Action key filter */

#define	Ap0_Code		0x00e	/* menu   */
#define	Ap1_Code		0x00d	/* home   */
#define	Ap2_Code		0x00b	/* ok     */
#define	Ap3_Code		0x007	/* cancel */
#define	Down_Code		0x060	/* down   */
#define	Up_Code			0x090	/* up     */
#define	Left_Code		0x050	/* left   */
#define	Right_Code		0x0a0	/* right  */
#define	Act_Code		0x200	/* action */
#define Release_Code	0xf0	/* release code */

#define	total_ap_keys	4

#define	DIR0_KEY		UP_KEY		/* level trigger */
#define	DIR1_KEY		DOWN_KEY	/* level trigger */
#define	DIR2_KEY		LEFT_KEY	/* edge trigger  */
#define	DIR3_KEY		RIGHT_KEY	/* edge trigger  */


/* All ASIC3 key input type is EDGE */
#define	All_EDGE	1	/* if 0:for direct_key is level trigger */












