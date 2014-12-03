/*
 *  linux/drivers/char/sharp_lowkey.c
 *
 *  Logical GPIO Keyboard driver includeing
 *  Hard-Key to Soft-Key Mapper for SHARP PDA
 *
 * Copyright (C) 2001  SHARP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  ChangeLog:
 *   04-16-2001 Lineo Japan, Inc.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <asm/arch/keyboard.h>
#include <asm/uaccess.h>
#include <linux/tqueue.h>
#include <linux/kbd_ll.h>
#include <linux/slab.h>
//#include <linux/spinlock.h>
#include <linux/delay.h>

#define DEBUGPRINT(s)   /* printk s */
#define CUS_DPRINTK(x, args...)  /* printk(__FUNCTION__ ": " x,##args) */


#ifndef USE_KBD_IRQ
# warning "this is not tested."
#endif

#ifdef USE_KBD_IRQ		// could be defined in asm/arch/keyboard.h
# include <asm/irq.h>		/* for linkup keyboard interrupt */
#endif

/*
 * physical driver definition
 */
#if defined(CONFIG_IRIS)
# define WAIT_CHATTERING_DELAY    udelay(100)
extern void iris_kbd_discharge_all(void);
extern void iris_kbd_activate_col(int col);
extern void iris_kbd_activate_all(void);
# define DISCHARGE_ALL            iris_kbd_discharge_all()
# define CHARGE_ALL
# define WAIT_DISCHARGE_DELAY     udelay(KB_DELAY)
# define ACTIVATE_COL(c)          iris_kbd_activate_col((c))
# define RESET_COL(c)
# define WAIT_ACTIVATE_DELAY      udelay(KB_DELAY)
# define GET_ROWS_STATUS(c)       (KB_ROW) /* ignore col# and read PA reg. */
# define DRIVE_ALL_COLS           iris_kbd_activate_all()
# ifdef KBD_COL9_IS_USED_FOR_SIC  /* Avoid using KBDCOL9 , because it is used as SIC SYS_CLK */
#  define GET_REAL_COL_FROM_COL(c)    ( (c) >= 9 ? (c) + 2 : (c) )
# else /* ! KBD_COL9_IS_USED_FOR_SIC */
#  define GET_REAL_COL_FROM_COL(c)    (c)
# endif /* ! KBD_COL9_IS_USED_FOR_SIC */
# define RAW_MAP "iris_rawmap.h"

#elif defined(CONFIG_SA1100_COLLIE)
# define WAIT_CHATTERING_DELAY    udelay(100)
extern void collie_kbd_discharge_all(void);
extern void collie_kbd_charge_all(void);
extern void collie_kbd_activate_col(int col);
extern void collie_kbd_reset_col(int col);
extern void collie_kbd_activate_all(void);
# define DISCHARGE_ALL            collie_kbd_discharge_all()
# define CHARGE_ALL               collie_kbd_charge_all()
# define WAIT_DISCHARGE_DELAY     udelay(KB_DELAY)
# define ACTIVATE_COL(c)          collie_kbd_activate_col((c))
# define RESET_COL(c)             collie_kbd_reset_col((c))
# define WAIT_ACTIVATE_DELAY      udelay(KB_DELAY)
# define GET_ROWS_STATUS(c)       ~(LCM_KIB) /* ignore col# and read PA reg. */
# define DRIVE_ALL_COLS           collie_kbd_activate_all()
# define GET_REAL_COL_FROM_COL(c)    (c)
# ifdef CONFIG_COLLIE_TS
#  define RAW_MAP "collie_rawmap.h"
# elif defined(CONFIG_COLLIE_G)
#  define RAW_MAP "collie_rawmapG.h"
# else
#  define RAW_MAP "collie_rawmap1.h"
# endif

#else
#error "not implemented"
#endif

#if ! defined(KB_COLS) || ! defined(KB_ROWS)
#error "KB_COLS or KB_ROWS not defined"
#endif

/*
 *   keyboard scancode mapper.
 */
#include <asm/sharp_keycode.h>

#if defined(CONFIG_IRIS) || defined(CONFIG_SA1100_COLLIE)
#include RAW_MAP
#else
#error "not implemented"
#endif

/*
 *  This is the mapper for Hard-Key to Soft-Key conversion
 */

int sharp_cur_keycode[(NR_KEYCODES+1)];
int sharp_cur_status;

kbd_keyinfo sharppda_kbdstate[(NR_KEYCODES+1)]; /* this map is expoted... */

static int keysdown = 0;
static struct tq_struct kbdhw_task;

/*
 * for panel-touch sound
 */
extern void sharpbuz_make_keytouch_sound(void);
extern void sharpkbdctl_stat_changed(void);

/*
 * Keyboard mapping routine.
 */

int keypress_map(int hardkey)
{
  int softkey;
  unsigned char* pmodmap;
  int modif_ref = DOWN_NOP;
  int next_status = sharp_cur_status;

  /* ======== get soft keycode ========= */
  pmodmap = state_to_keymap[sharp_cur_status];
  softkey = pmodmap[hardkey];
  sharp_cur_keycode[hardkey] = softkey;

  /* ======== check automaton-map ======== */
  modif_ref = down_modifs_ref[hardkey];
  if( modif_ref != DOWN_NOP && modif_ref < DOWN_TABLES_SIZE ){
    /* ======== need to check state machine ========= */
    unsigned char* statemap;
    statemap = down_tables[modif_ref];
    next_status = statemap[sharp_cur_status];
  }

  if( sharp_cur_status != next_status ){
    sharp_cur_status = next_status;
    sharpkbdctl_stat_changed();
  }else{
    sharp_cur_status = next_status;
  }
  return softkey;
}

int keyrelease_map(int hardkey)
{
  int softkey;
  int modif_ref = UP_NOP;
  int next_status = sharp_cur_status;

  /* ======== get soft keycode ========= */
  softkey = sharp_cur_keycode[hardkey];
  sharp_cur_keycode[hardkey] = KEY_IGN;

  /* ======== check automaton-map ======== */
  modif_ref = up_modifs_ref[hardkey];
  if( modif_ref != UP_NOP && modif_ref < UP_TABLES_SIZE ){
    /* ======== need to check state machine ========= */
    unsigned char* statemap;
    statemap = up_tables[modif_ref];
    next_status = statemap[sharp_cur_status];
  }
    
  if( sharp_cur_status != next_status ){
    sharp_cur_status = next_status;
    sharpkbdctl_stat_changed();
  }else{
    sharp_cur_status = next_status;
  }
  return softkey;
}

/*
 * Keyboard Repeat , including key-mapping
 */
#include <linux/kd.h>

int (*mach_kbdrate) (struct kbd_repeat *);


#define DEFAULT_KEYB_REP_DELAY  (HZ*2)
#define DEFAULT_KEYB_REP_RATE   (HZ)

static void sharppda_kbd_rep( unsigned long ignore );
static unsigned char rep_scancode = 0;
static unsigned int key_repeat_delay = DEFAULT_KEYB_REP_DELAY;
static unsigned int key_repeat_rate  = DEFAULT_KEYB_REP_RATE;

static struct timer_list sharppda_kbd_rep_timer = { function: sharppda_kbd_rep };

static void sharppda_kbd_rep( unsigned long ignore )
{
  unsigned long flags;
  /* Disable keyboard for the time we call handle_scancode(), else a race
   * in the keyboard tty queue may happen */
  //DEBUGPRINT(("?"));
  save_flags(flags); cli();
  del_timer( &sharppda_kbd_rep_timer );
  /* A keyboard int may have come in before we disabled the irq, so
   * double-check whether rep_scancode is still != 0 */
  if (rep_scancode) {
    init_timer(&sharppda_kbd_rep_timer);
    sharppda_kbd_rep_timer.expires = jiffies + key_repeat_rate;
    add_timer( &sharppda_kbd_rep_timer );
    DEBUGPRINT(("kbdrep_down %d (%x)\n",rep_scancode,rep_scancode));
#if 0 /* 2001-12-19 detele: irregular repeat in QT */
    handle_scancode(rep_scancode, 1);
#endif
#if 0
    handle_scancode(KBSCANCDE(rep_scancode,KBUP),0);	/* always up */
    handle_scancode(KBSCANCDE(rep_scancode,KBDOWN),1);
#endif
  }
  restore_flags(flags);
  //DEBUGPRINT(("!"));
}

static int sharppda_kbd_kbdrate( struct kbd_repeat *k )
{
  DEBUGPRINT(("sharppda_kbd_kbdrate\n"));
  if (k->delay > 0) {
    /* convert from msec to jiffies */
    key_repeat_delay = (k->delay * HZ + 500) / 1000;
    if (key_repeat_delay < 1)
      key_repeat_delay = 1;
  }
  if (k->rate > 0) {
    key_repeat_rate = (k->rate * HZ + 500) / 1000;
    if (key_repeat_rate < 1)
      key_repeat_rate = 1;
  }
  k->delay = key_repeat_delay * 1000 / HZ;
  k->rate  = key_repeat_rate  * 1000 / HZ;
  return( 0 );
}

/*
 * Keyboard Press/Release function , including key-mapping , for HOLD-modified Key
 */

#if !defined (CONFIG_SA1100_COLLIE)
static unsigned long key_hold_delay = HZ;
#else // for Collie
static unsigned long key_hold_delay = 75 * HZ / 100;
#endif

static unsigned char holdkey_pending = 0;
static int holdkey_pendinghardkey = 0;
static int holdkey_pendingmappedkey = 0;
static void sharppda_kbd_hold( unsigned long ignore );
static struct timer_list sharppda_kbd_hold_timer = { function: sharppda_kbd_hold };

static shappda_holdkey_info **custom_holdkey_info = NULL;

static void sharppda_kbd_hold( unsigned long ignore )
{
  int hold_keycode;
  unsigned long flags;
  shappda_holdkey_info *hold_infop = NULL;
  save_flags(flags); cli();
  del_timer(&sharppda_kbd_hold_timer);
  DEBUGPRINT(("sharppda_kbd_hold\n"));
  CUS_DPRINTK("enter\n");
  if( holdkey_pending && holdkey_info ){
    CUS_DPRINTK("pending exists\n");
    if( holdkey_pendinghardkey ){
      CUS_DPRINTK("holdkey_pendinghardkey\n");
      hold_infop = holdkey_info[holdkey_pendinghardkey];
      if( custom_holdkey_info ){ /* use Customozed HoldKey */
	CUS_DPRINTK("customized holdkey\n");
	hold_infop = custom_holdkey_info[holdkey_pendinghardkey];
      }
      if( ! hold_infop ){
	DEBUGPRINT(("NO VALID INFO found for %x\n",holdkey_pendinghardkey));
      }else if( hold_infop->type == HOLDKEYTYPE_CHAR ){
	DEBUGPRINT(("HOLD eventtype is HOLDKEYTYPE_CHAR for %x\n",holdkey_pendinghardkey));
	hold_keycode = hold_infop->character;
	DEBUGPRINT(("Pending Key Exists for Hard-Key %x.\n",holdkey_pendinghardkey));
	DEBUGPRINT(("Current Keycode is %x , %x\n",
	       holdkey_pendingmappedkey,sharp_cur_keycode[holdkey_pendinghardkey]));
	DEBUGPRINT(("Changing to HOLD-KEY %x\n",hold_keycode));
	sharp_cur_keycode[holdkey_pendinghardkey] = hold_keycode;
	DEBUGPRINT(("kbdhold %d (%x)\n",KBSCANCDE(hold_keycode,KBDOWN),KBSCANCDE(hold_keycode,KBDOWN)));
	handle_scancode(KBSCANCDE(hold_keycode,KBDOWN), 1);
	del_timer(&sharppda_kbd_rep_timer);
	init_timer(&sharppda_kbd_rep_timer);
	sharppda_kbd_rep_timer.expires = jiffies + key_repeat_delay;
/* 2001.09.05 Nami. delete hold-key repeat */
/* delete hold-key repeat on Iris , too. */
#if 0
	add_timer( &sharppda_kbd_rep_timer );
#endif
	rep_scancode = KBSCANCDE(hold_keycode,KBDOWN);
#if 0
	rep_scancode = hold_keycode;
#endif
      }else if( hold_infop->type == HOLDKEYTYPE_FUNC ){
	DEBUGPRINT(("HOLD eventtype is HOLDKEYTYPE_FUNC for %x\n",holdkey_pendinghardkey));
	if( hold_infop->func ){
	  DEBUGPRINT(("Executing FUNC\n"));
	  (*(hold_infop->func))();
	}
      }else{
	DEBUGPRINT(("NO VALID TYPE found for %x\n",holdkey_pendinghardkey));
      }
    }
  }
  holdkey_pending = 0;
  holdkey_pendinghardkey = 0;
  holdkey_pendingmappedkey = 0;
  DEBUGPRINT(("Pending Key Released.\n"));
  restore_flags(flags);
}



/*
 * Keyboard Press/Release function , including key-mapping
 */

void sharppda_kbd_press(int keycode)
{
  unsigned long flags;
  unsigned char newpress = 0;
  int keycode_mapped = keycode;

//printk("kbd_press=%d\n",keycode);

  spin_lock_irqsave(&kbd_spinlock,flags);
  if (!sharppda_kbdstate[keycode].in) {
    keysdown++; 
    newpress = 1;
  }

  /*
   * We approximate a retriggable monostable
   * action.
   */
    
  sharppda_kbdstate[keycode].in=1;
  spin_unlock_irqrestore(&kbd_spinlock,flags);

  /* We only need to ensure keysdown consistent */

  /* Map Keycode */
  if( newpress ){
    keycode_mapped = keypress_map(keycode);
  }

  /* put into upper layer */
  if (newpress){
    shappda_holdkey_info **use_holdkey_info;
    use_holdkey_info = holdkey_info;
    if( custom_holdkey_info ){
      CUS_DPRINTK("customized holdkey\n");
      use_holdkey_info = custom_holdkey_info;
    }
    if( use_holdkey_info && use_holdkey_info[keycode] ){
      CUS_DPRINTK("keycode found\n");
      if( ! holdkey_pending ){
	DEBUGPRINT(("HOLD keyinfo found for %x. mapped=%x\n",keycode,keycode_mapped));
	holdkey_pendinghardkey = keycode;
	holdkey_pendingmappedkey = keycode_mapped;
	holdkey_pending = 1;
	del_timer(&sharppda_kbd_hold_timer);
	init_timer(&sharppda_kbd_hold_timer);
#ifdef HOLDKEY_INFO_HAS_TIMEOUT
	if( use_holdkey_info[keycode]->timeout )
	  {
	    unsigned long diff =  use_holdkey_info[keycode]->timeout * HZ / 1000;
	    CUS_DPRINTK("has timeout %d\n",use_holdkey_info[keycode]->timeout);
	    sharppda_kbd_hold_timer.expires = jiffies + diff;
	  }
	else{ /* set default value */
	  CUS_DPRINTK("default timeout\n");
	  sharppda_kbd_hold_timer.expires = jiffies + key_hold_delay;
	}
#else
	sharppda_kbd_hold_timer.expires = jiffies + key_hold_delay;
#endif
	add_timer( &sharppda_kbd_hold_timer );
	DEBUGPRINT(("holding...\n"));
	return;
      }else{
	DEBUGPRINT(("HOLD keyinfo found for %d but already pending...\n",keycode));
      }
    }
    DEBUGPRINT(("kbdpress %d (%x)\n",KBSCANCDE(keycode_mapped,KBDOWN),KBSCANCDE(keycode_mapped,KBDOWN)));
    handle_scancode(KBSCANCDE(keycode_mapped,KBDOWN), 1);
    del_timer(&sharppda_kbd_rep_timer);
    init_timer(&sharppda_kbd_rep_timer);
    sharppda_kbd_rep_timer.expires = jiffies + key_repeat_delay;
    add_timer( &sharppda_kbd_rep_timer );
    rep_scancode = KBSCANCDE(keycode_mapped,KBDOWN);
#if 0
    rep_scancode = keycode_mapped;
#endif
  }
}

void sharppda_kbd_release(int keycode)
{
  unsigned long flags;
  unsigned char newrelease = 0;
  int keycode_mapped = keycode;

//printk("kbd_release=%d\n",keycode);
  spin_lock_irqsave(&kbd_spinlock,flags);
  if (sharppda_kbdstate[keycode].in) {
    keysdown--; 
    newrelease = 1;
  }

  sharppda_kbdstate[keycode].in=0;
  spin_unlock_irqrestore(&kbd_spinlock,flags);

  /* We only need to ensure keysdown consistent */


  /* Map Keycode */
  if( newrelease ){
    keycode_mapped = keyrelease_map(keycode);
  }
  
  if (newrelease){
    if( holdkey_pending ){
      DEBUGPRINT(("HOLD pending.\n"));
      if( holdkey_pendinghardkey == keycode ){
	DEBUGPRINT(("HOLDed key is same as pending-key. %x\n",keycode));
	DEBUGPRINT(("Current MappedKey is %x.\n",holdkey_pendingmappedkey));
	DEBUGPRINT(("kbdrel hold %d (%x)\n",KBSCANCDE(holdkey_pendingmappedkey,KBDOWN),KBSCANCDE(holdkey_pendingmappedkey,KBDOWN)));
	handle_scancode(KBSCANCDE(holdkey_pendingmappedkey,KBDOWN), 1);
	del_timer(&sharppda_kbd_hold_timer);
	holdkey_pending = 0;
	holdkey_pendinghardkey = 0;
	holdkey_pendingmappedkey = 0;
	DEBUGPRINT(("HOLDed key is released.\n"));
      }
    }
    DEBUGPRINT(("kbdrel nom %d (%x)\n",KBSCANCDE(keycode_mapped,KBUP),KBSCANCDE(keycode_mapped,KBUP)));
    handle_scancode(KBSCANCDE(keycode_mapped,KBUP), 0);
    if( rep_scancode == KBSCANCDE(keycode_mapped,KBDOWN) ){
#if 0
    if( rep_scancode == keycode_mapped ){
#endif
      rep_scancode = 0;
      //DEBUGPRINT(("%%"));
      del_timer( &sharppda_kbd_rep_timer );
    }
  }

}

/*
 * Keyboard Scan routine , using GPIO
 */

#ifdef CONFIG_IRIS
#define PHONEKEY_ROW   5
#define PHONEKEY_COL   0
#define DISPKEY_ROW    5
#define DISPKEY_COL    2
  int wakeup_from_disp_and_ignore_first_disp = 0;
  int wakeup_from_phone_and_ignore_first_phone = 0;
#endif
#ifdef CONFIG_SA1100_COLLIE /* PowerOnKeyLock */
#define	SLCKL_MASK(N)		(1<<(N))
#define	SLCKL_MASK_SUSPEND	SLCKL_MASK(0)
#define	SLCKL_KEY_POWER		KEYCODE(0,0)
#define SLCKL_MASK_POWER	SLCKL_MASK(1)
#define	SLCKL_ALLMASK		(SLCKL_MASK_POWER)
static int wakeup_powerkey_lock = 0;
extern u32 apm_wakeup_factor;
#endif /* CONFIG_SA1100_COLLIE PowerOnKeyLock */

void sharppda_scan_key_gpio(void* dummy)
{
  int col,row;
  int rowd;
  unsigned long flags;
  int real_col;
  int count = 0;

  WAIT_CHATTERING_DELAY;

  spin_lock_irqsave(&kbd_spinlock,flags);

  CHARGE_ALL;

  for(col=0;col<KB_COLS;col++){
    real_col = GET_REAL_COL_FROM_COL(col);
    /*
     * Discharge the output driver capacitatance
     * in the keyboard matrix. (Yes it is significant..)
     */

    DISCHARGE_ALL;
    WAIT_DISCHARGE_DELAY;
    ACTIVATE_COL(real_col);

    WAIT_ACTIVATE_DELAY;
    rowd = GET_ROWS_STATUS(real_col);
    for( row = 0 ; row < KB_ROWS ; row++ ){
      if( rowd & KB_ROWMASK(row) ){
#ifdef CONFIG_IRIS
	if( wakeup_from_disp_and_ignore_first_disp ){
	  if( col == DISPKEY_COL && row == DISPKEY_ROW ){
	    count++;
	    continue;
	  }
	}
	if( wakeup_from_phone_and_ignore_first_phone ){
	  if( col == PHONEKEY_COL && row == PHONEKEY_ROW ){
	    count++;
	    continue;
	  }
	}
#endif
#ifdef CONFIG_SA1100_COLLIE /* PowerOnKeyLock */
	if( wakeup_powerkey_lock ){
	  if( KEYCODE(row,col) == SLCKL_KEY_POWER ){
	    if( wakeup_powerkey_lock & 
		(SLCKL_MASK_POWER|SLCKL_MASK_SUSPEND) ){
	      count++;
	      continue;
	    }
	  }
	}
#endif /* CONFIG_SA1100_COLLIE PowerOnKeyLock */
	sharppda_kbd_press(KEYCODE(row,col));
	count++;
      }
#ifdef CONFIG_IRIS
      else if( wakeup_from_disp_and_ignore_first_disp ){
	if( col == DISPKEY_COL && row == DISPKEY_ROW ){
	  wakeup_from_disp_and_ignore_first_disp = 0;
	  sharppda_kbd_release(KEYCODE(row,col));
	  continue;
	}
      }
      else if( wakeup_from_phone_and_ignore_first_phone ){
	if( col == PHONEKEY_COL && row == PHONEKEY_ROW ){
	  wakeup_from_phone_and_ignore_first_phone = 0;
	  sharppda_kbd_release(KEYCODE(row,col));
	  continue;
	}
      }
#endif
#ifdef CONFIG_SA1100_COLLIE /* PowerOnKeyLock */
      else{
	if( wakeup_powerkey_lock ){
	  if( KEYCODE(row,col) == SLCKL_KEY_POWER ){
	    if( wakeup_powerkey_lock & SLCKL_MASK_POWER )
	      wakeup_powerkey_lock &= ~SLCKL_MASK_POWER;
	  }
	}
	if( sharppda_kbdstate[KEYCODE(row, col)].in ){
	  sharppda_kbd_release(KEYCODE(row,col));
	}
      }
#else /* !CONFIG_SA1100_COLLIE PowerOnKeyLock */
      else if (sharppda_kbdstate[KEYCODE(row, col)].in){
	sharppda_kbd_release(KEYCODE(row,col));
      }
#endif /* CONFIG_SA1100_COLLIE PowerOnKeyLock */


    }
	RESET_COL(real_col);
  }	/* end of for */


#ifdef USE_KBD_IRQ
  /*
   * drive all for next interrupts
   */
  DRIVE_ALL_COLS;
#endif
    
  /*
   * Re-queue ourselves
   */
#ifdef CONFIG_SA1100_COLLIE /* PowerOnKeyLock */
  if( !(wakeup_powerkey_lock & SLCKL_MASK_SUSPEND) )
#endif /* CONFIG_SA1100_COLLIE PowerOnKeyLock */
#ifdef USE_KBD_IRQ
  if ( count > 0 )
#endif
    queue_task(&kbdhw_task,&tq_timer);

  spin_unlock_irqrestore(&kbd_spinlock,flags);

}

/*
 * Logical Keyboard Driver initialization
 */

void sharppda_scan_logdriver_init(void)
{
int ni;
  kbdhw_task.routine = sharppda_scan_key_gpio;
  kbdhw_task.sync = 0;
  mach_kbdrate = sharppda_kbd_kbdrate;
#ifndef USE_KBD_IRQ
  queue_task(&kbdhw_task,&tq_timer);
#endif
    for (ni=0;ni<NR_KEYCODES;ni++){
	sharppda_kbdstate[ni].in=0;
    }
}

void sharppda_scan_logdriver_suspend(void)
{
#ifdef CONFIG_SA1100_COLLIE /* PowerOnKeyLock */
  wakeup_powerkey_lock = SLCKL_MASK_SUSPEND;
#endif /* CONFIG_SA1100_COLLIE PowerOnKeyLock */
  del_timer( &sharppda_kbd_rep_timer );
  del_timer( &sharppda_kbd_hold_timer );
  rep_scancode = 0;
  holdkey_pending = 0;
  holdkey_pendinghardkey = 0;
  holdkey_pendingmappedkey = 0;
#ifdef CONFIG_IRIS
  wakeup_from_disp_and_ignore_first_disp = 0;
  wakeup_from_phone_and_ignore_first_phone = 0;
#endif
}

void sharppda_scan_logdriver_resume(void)
{
#ifdef CONFIG_SA1100_COLLIE /* PowerOnKeyLock */
  wakeup_powerkey_lock = SLCKL_ALLMASK;
  /* (apm_wakeup_factor & GPIO_ON_KEY) ? SLCKL_ALLMASK : 0; */
  queue_task(&kbdhw_task,&tq_timer);

#else /* !CONFIG_SA1100_COLLIE PowerOnKeyLock */
  int ni;
#ifndef USE_KBD_IRQ
  queue_task(&kbdhw_task,&tq_timer);
#endif
  for (ni=0;ni<NR_KEYCODES;ni++){
    sharppda_kbdstate[ni].in=0;
  }

#ifdef CONFIG_IRIS
  {
    int count = 0;
    if( wakeup_from_disp_and_ignore_first_disp ){
      sharppda_kbdstate[KEYCODE(DISPKEY_ROW,DISPKEY_COL)].in = 1;
      count++;
    }
    if( wakeup_from_phone_and_ignore_first_phone ){
      sharppda_kbdstate[KEYCODE(PHONEKEY_ROW,PHONEKEY_COL)].in = 1;
      count++;
    }
    if( count ){
      queue_task(&kbdhw_task,&tq_timer);
    }
  }
#endif /* CONFIG_IRIS */

#ifdef USE_KBD_IRQ
  /*
   * drive all for next interrupts
   */
  DRIVE_ALL_COLS;
#endif
#endif /* CONFIG_SA1100_COLLIE PowerOnKeyLock */
}



/*
 * External Modifier Status Functions
 */

int sharppda_kbd_extmodif_showstatus(int which)
{
#ifdef EXTMODIF_SHOWSTAT_KEYMAX
  unsigned char* pmodmap;
  if( which >= EXTMODIF_SHOWSTAT_KEYMAX ) return -EINVAL;
  if( ! extmodif_status_tables ) return -EINVAL;
  if( ! ( pmodmap = extmodif_status_tables[which] ) ) return -EINVAL;
  return(pmodmap[sharp_cur_status]);
#else /* ! EXTMODIF_SHOWSTAT_KEYMAX */
  return -EINVAL; /* not supported on this arch. */
#endif /* ! EXTMODIF_SHOWSTAT_KEYMAX */
}

int sharppda_kbd_extmodif_togglestatus(int which)
{
#ifdef EXTMODIF_MOVESTAT_KEYMAX
  unsigned char* pmodmap;
  if( which >= EXTMODIF_MOVESTAT_KEYMAX ) return -EINVAL;
  if( ! extmodif_move_tables ) return -EINVAL;
  if( ! ( pmodmap = extmodif_move_tables[which] ) ) return -EINVAL;
  DEBUGPRINT(("moving from %d to %d\n",sharp_cur_status,pmodmap[sharp_cur_status]));
  if( sharp_cur_status != pmodmap[sharp_cur_status] ){
    sharp_cur_status = pmodmap[sharp_cur_status];
    sharpkbdctl_stat_changed();
  }else{
    sharp_cur_status = pmodmap[sharp_cur_status];
  }
  return 0;
#else /* ! EXTMODIF_MOVESTAT_KEYMAX */
  return -EINVAL; /* not supported on this arch. */
#endif /* ! EXTMODIF_MOVESTAT_KEYMAX */
}

unsigned long sharppda_kbd_extmodif_packstatus(void)
{
#ifdef EXTMODIF_SHOWSTAT_KEYMAX
  unsigned long val = 0;
  int i;
  unsigned char* pmodmap;
  if( ! extmodif_status_tables ) return 0;
  for(i=0;i<EXTMODIF_SHOWSTAT_KEYMAX;i++){
    pmodmap = extmodif_status_tables[i];
    if( pmodmap[sharp_cur_status] ){
      val |= ( 1 << i );
    }
  }
  return val;
#else /* ! EXTMODIF_SHOWSTAT_KEYMAX */
  return 0;
#endif /* ! EXTMODIF_SHOWSTAT_KEYMAX */
}

/*
 * SL-Keycode Search Functions
 */

static int find_hardcode_from_slcode(int stateno,unsigned char sl_char)
{
  int i;
  unsigned char* pmodmap;
  if( stateno < 0 || stateno >= STATE_NUMS_TOTAL ) return -EINVAL;
  pmodmap = state_to_keymap[stateno];
  for(i=0;i<(NR_KEYCODES+1);i++){
    if( pmodmap[i] == sl_char ) return i;
  }
  return -EINVAL;
}


/*
 * HoldKey Customizing Functions
 */

static void delete_custom_holdkey_info(void)
{
  CUS_DPRINTK("\n");
  if( custom_holdkey_info ){
    int i;
    for(i=0;i<(NR_KEYCODES+1);i++){
      if( custom_holdkey_info[i] ){
	kfree(custom_holdkey_info[i]);
	custom_holdkey_info[i] = NULL;
      }
    }
    kfree(custom_holdkey_info);
    custom_holdkey_info = NULL;
  }
}

static int duplicate_holdkey_info(void)
{
  int i;
  CUS_DPRINTK("\n");
  if( ! custom_holdkey_info ){
    custom_holdkey_info = (shappda_holdkey_info**)kmalloc(sizeof(shappda_holdkey_info*)*(NR_KEYCODES+1),GFP_KERNEL);
    if( ! custom_holdkey_info ) goto FAIL_SAFE;
    memset(custom_holdkey_info,0,sizeof(shappda_holdkey_info*)*(NR_KEYCODES+1));
  }
  for(i=0;i<(NR_KEYCODES+1);i++){
    if( holdkey_info[i] ){
      if( ! custom_holdkey_info[i] ){
	custom_holdkey_info[i] = (shappda_holdkey_info*)kmalloc(sizeof(shappda_holdkey_info),GFP_KERNEL);
	if( ! custom_holdkey_info[i] ) goto FAIL_SAFE;
      }
      memcpy(custom_holdkey_info[i],holdkey_info[i],sizeof(shappda_holdkey_info));
    }else{
      if( custom_holdkey_info[i] ){
	memset(custom_holdkey_info[i],0,sizeof(shappda_holdkey_info));
	kfree(custom_holdkey_info[i]);
	custom_holdkey_info[i] = NULL;
      }
    }
  }
  return 0;
 FAIL_SAFE:
  delete_custom_holdkey_info();
  return -ENOMEM;
}

static int add_custom_holdkey_info(int i)
{
  int error;
  CUS_DPRINTK(" %d(%x)\n",i,i);
  if( i < 0 || i >= (NR_KEYCODES+1) ) return -EINVAL;
  if( ! custom_holdkey_info ){
    if( ( error = duplicate_holdkey_info() ) ) return error;
  }
  if( ! custom_holdkey_info[i] ){
    custom_holdkey_info[i] = (shappda_holdkey_info*)kmalloc(sizeof(shappda_holdkey_info),GFP_KERNEL);
    if( ! custom_holdkey_info[i] ) goto FAIL_SAFE;
    if( holdkey_info[i] )
      memcpy(custom_holdkey_info[i],holdkey_info[i],sizeof(shappda_holdkey_info));
    else
      memset(custom_holdkey_info[i],0,sizeof(shappda_holdkey_info));
  }
  return 0;
 FAIL_SAFE:
  return -ENOMEM;
}

static int del_custom_holdkey_info(int i)
{
  int error;
  CUS_DPRINTK(" %d(%x)\n",i,i);
  if( i < 0 || i >= (NR_KEYCODES+1) ) return -EINVAL;
  if( ! custom_holdkey_info ){
    if( ( error = duplicate_holdkey_info() ) ) return error;
  }
  if( custom_holdkey_info[i] ){
    memset(custom_holdkey_info[i],0,sizeof(shappda_holdkey_info));
    kfree(custom_holdkey_info[i]);
    custom_holdkey_info[i] = NULL;
  }
  return 0;
 FAIL_SAFE:
  return -ENOMEM;
}

static int restore_one_holdkey_info(int i)
{
  int error;
  CUS_DPRINTK(" %d(%x)\n",i,i);
  if( i < 0 || i >= (NR_KEYCODES+1) ) return -EINVAL;
  if( ! custom_holdkey_info ) return 0;
  if( holdkey_info[i] ){
    if( ( error = add_custom_holdkey_info(i) ) ) return error;
    memcpy(custom_holdkey_info[i],holdkey_info[i],sizeof(shappda_holdkey_info));
  }else{
    if( ( error = del_custom_holdkey_info(i) ) ) return error;
  }
  return 0;
}

int restore_all_holdkey_info(void)
{
  CUS_DPRINTK("\n");
  if( ! custom_holdkey_info ) return 0;
  delete_custom_holdkey_info();
  return 0;
}

int customize_holdkey_char_info(int hardcode,int sl_char)
{
  int error;
  CUS_DPRINTK(" %d(%x) %d(%x)\n",hardcode,hardcode,sl_char,sl_char);
  if( ! custom_holdkey_info ){
    if( ( error = duplicate_holdkey_info() ) ) return error;
  }
  if( ( error = add_custom_holdkey_info(hardcode) ) ) return error;
  custom_holdkey_info[hardcode]->type = HOLDKEYTYPE_CHAR;
  custom_holdkey_info[hardcode]->character = sl_char;
  return 0;
}

int customize_holdkey_char_info_by_slkey(unsigned char slcode,int sl_char)
{
  int hardcode;
  CUS_DPRINTK(" %d(%x) %d(%x)\n",slcode,slcode,sl_char,sl_char);
  hardcode = find_hardcode_from_slcode(0,slcode);
  if( hardcode < 0 ) return -EINVAL;
  return customize_holdkey_char_info(hardcode,sl_char);
}

int customize_del_holdkey_info(int hardcode)
{
  CUS_DPRINTK(" %d(%x)\n",hardcode,hardcode);
  return del_custom_holdkey_info(hardcode);
}

int customize_del_holdkey_info_by_slkey(int slcode)
{
  int hardcode;
  CUS_DPRINTK(" %d(%x)\n",slcode,slcode);
  hardcode = find_hardcode_from_slcode(0,slcode);
  if( hardcode < 0 ) return -EINVAL;
  return del_custom_holdkey_info(hardcode);
}

int customize_restore_holdkey_info(int hardcode)
{
  CUS_DPRINTK(" %d(%x)\n",hardcode,hardcode);
  return restore_one_holdkey_info(hardcode);
}

int customize_restore_holdkey_info_by_slkey(int slcode)
{
  int hardcode;
  CUS_DPRINTK(" %d(%x)\n",slcode,slcode);
  hardcode = find_hardcode_from_slcode(0,slcode);
  if( hardcode < 0 ) return -EINVAL;
  return restore_one_holdkey_info(hardcode);
}


int sharppda_kbd_set_hold_threshold(int msecs)
{
  if( holdkey_info ){
    key_hold_delay = (unsigned long)msecs * HZ / 1000;
#ifdef HOLDKEY_INFO_HAS_TIMEOUT
    if( ! custom_holdkey_info ) duplicate_holdkey_info();
    if( custom_holdkey_info ){
      int i;
      for(i=0;i<=NR_KEYCODES;i++){
	if( custom_holdkey_info[i] ){
	  custom_holdkey_info[i]->timeout = msecs;
	}
      }
    }else{
      int i;
      for(i=0;i<=NR_KEYCODES;i++){
	if( holdkey_info[i] ){
	  holdkey_info[i]->timeout = msecs;
	}
      }
    }
#endif
    return 0;
  }else{
    return -EINVAL; /* not supported on this architecture */
  }
}

int sharppda_kbd_set_hold_threshold_group(int group,int msecs)
{
#ifdef HOLDKEY_INFO_HAS_TIMEOUT
  if( ! custom_holdkey_info ) duplicate_holdkey_info();
  if( custom_holdkey_info ){
    int i;
    for(i=0;i<=NR_KEYCODES;i++){
      if( custom_holdkey_info[i] && custom_holdkey_info[i]->setgroup == group ){
	custom_holdkey_info[i]->timeout = msecs;
      }
    }
    return 0;
  }else if( holdkey_info ){
    int i;
    for(i=0;i<=NR_KEYCODES;i++){
      if( holdkey_info[i] && holdkey_info[i]->setgroup == group ){
	holdkey_info[i]->timeout = msecs;
      }
    }
    return 0;
  }else{
    return -EINVAL; /* not supported on this architecture */
  }
#endif
  return -EINVAL; /* not supported on this architecture */
}

/*
 *   end of source
 */
