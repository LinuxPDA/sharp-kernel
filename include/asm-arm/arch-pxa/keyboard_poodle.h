/*
 *  linux/include/asm-arm/arch-pxa/keyboard_poodle.h
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
 */
#ifndef __ASM_ARCH_KEYBOARD_POODLE_H
#define __ASM_ARCH_KEYBOARD_POODLE_H

#include <linux/spinlock.h>
#include <asm/arch/hardware.h>

/* =========================================================
 * !!! CAUTION !!!
 * Iris board without Keyboard Enhancements makes PA5 INTR
 * all times. So , you should disable PA5 INTR on such boards.
 * Define this option to run on such board.
 * ========================================================= */
#define POODLE_WITHOUT_KEY_ENH_WORKAROUND
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
extern int  poodle_kbd_translate(unsigned char sc,unsigned char *keycode_p);
#define kbd_translate(sc,kc,rm)  poodle_kbd_translate(sc,kc)
/*
 *#define kbd_sysrq_xlate()          (1)
 */
#define kbd_pretranslate(x,y)      (1)
#define kbd_unexpected_up(kc)        (0x80)
#define kbd_setkeycode(sc,kc)   (-EINVAL)
#define kbd_getkeycode(sc)      (-EINVAL)

extern void poodle_kbd_hw_init(void);
#define kbd_init_hw()		 poodle_kbd_hw_init()

extern void poodle_kbd_cleartable(void);

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
  POODLEKBD_KEYUP,
  POODLEKBD_KEYDOWN,
} poodle_kbd_event_type;
typedef struct poodle_kbd_event_struct {
  poodle_kbd_event_type which;
  int keycode;
  int resolved_keycode;
} poodle_kbd_event_struct;

typedef enum {
  POODLE_SCANKEY_2NDF,
  POODLE_SCANKEY_3RDF
} poodle_kbd_feature_keys;
typedef struct poodle_kbd_scan_struct {
  poodle_kbd_feature_keys which;
  int scan_result;
} poodle_kbd_scan_struct;

/* IOCTL code for keyboard event */
#define POODLEKBD_IOCTL_CODE_BASE 0x56a0
#define POODLEKBD_GETEVENT    (POODLEKBD_IOCTL_CODE_BASE+1)
#define POODLEKBD_WAITEVENT   (POODLEKBD_IOCTL_CODE_BASE+2)
#define POODLEKBD_SCANKEY     (POODLEKBD_IOCTL_CODE_BASE+3)
#define POODLEKBD_RESETMODIF  (POODLEKBD_IOCTL_CODE_BASE+4)

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
#define POODLE_KEYPOS_2nd_ROW   0
#define POODLE_KEYPOS_2nd_COL   1

#define POODLE_KEYPOS_NUM_ROW   0
#define POODLE_KEYPOS_NUM_COL   8

#define POODLE_KEYPOS_LSHIFT_ROW 1
#define POODLE_KEYPOS_LSHIFT_COL 0

#define POODLE_KEYPOS_RSHIFT_ROW 1
#define POODLE_KEYPOS_RSHIFT_COL 8

/*
 *   number of modifier status keys resolved on poodlekb driver
 */
#define POODLEKB_MODIFIERS  5 /* for CapsLock / NumLock / 2ND / LSHIFT / RSHIFT */

#endif
