/*
 *  linux/drivers/char/tosa_logkey.c
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/drivers/char/corgi_logkey.c
 *
 * Based on:
 *  linux/drivers/char/poodle_logkey.c
 *  linux/drivers/char/collie_logkey.c
 *  linux/drivers/char/sharp_logkey.c
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
 *   30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 *   12-Dec-2002 Sharp Corporation
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <asm/arch/keyboard_tosa.h>
#include <asm/uaccess.h>
#include <linux/tqueue.h>
#include <linux/kbd_ll.h>
#include <linux/slab.h>
//#include <linux/spinlock.h>
#include <linux/delay.h>

#define DEBUGPRINT(s)	/* printk s */
#define CUS_DPRINTK(x, args...)	/* printk(__FUNCTION__ ": " x,##args) */

#ifdef LOGICAL_WAKEUP_SRC
#include <asm/arch/sharpsl_wakeup.h>
#endif
#define KEYMASK_ON		(0x1<<0)
#define KEYMASK_REC		(0x1<<1)
#define KEYMASK_SYNC		(0x1<<2)
#define KEYMASK_ADDRESS		(0x1<<3)
#define KEYMASK_CALENDAR	(0x1<<4)
#define KEYMASK_MENU		(0x1<<5)
#define KEYMASK_MAIL		(0x1<<6)

#ifndef USE_KBD_IRQ
# warning "this is not tested."
#endif

#ifdef USE_KBD_IRQ		// could be defined in asm/arch/keyboard.h
# include <asm/irq.h>		/* for linkup keyboard interrupt */
#endif

/*
 * physical driver definition
 */
# define WAIT_CHATTERING_DELAY    udelay(100)
extern void tosa_kbd_discharge_all(void);
extern void tosa_kbd_charge_all(void);
extern void tosa_kbd_activate_col(int col);
extern void tosa_kbd_reset_col(int col);
extern void tosa_kbd_activate_all(void);
# define DISCHARGE_ALL            tosa_kbd_discharge_all()
# define CHARGE_ALL               tosa_kbd_charge_all()
# define WAIT_DISCHARGE_DELAY     udelay(KB_DISCHARGE_DELAY)
# define ACTIVATE_COL(c)          tosa_kbd_activate_col((c))
# define RESET_COL(c)             tosa_kbd_reset_col((c))
# define WAIT_ACTIVATE_DELAY      udelay(KB_ACTIVATE_DELAY)
# define GET_ROWS_STATUS(c)       ((GPLR2 & GPIO_ALL_SENSE_BIT) >> GPIO_ALL_SENSE_RSHIFT)
# define DRIVE_ALL_COLS           tosa_kbd_activate_all()
# define GET_REAL_COL_FROM_COL(c)    (c)
# define RAW_MAP "tosa_rawmap.h"
# define RAW_MAP_ROLLOVER "tosa_rawmap_rollover.h"


static int isBrb = 0;
static int checkBrb(void)
{
  int rowd;

  DISCHARGE_ALL;
  WAIT_DISCHARGE_DELAY;
  
  rowd = GET_ROWS_STATUS(1);
  if (rowd!=0) return 1;
  else return 0;
}


#if ! defined(KB_COLS) || ! defined(KB_ROWS)
#error "KB_COLS or KB_ROWS not defined"
#endif

/*
 *   keyboard scancode mapper.
 */
#include <asm/sharp_keycode.h>

#define CHECK_ROLLOVER 1

#define CHECK_CHUTTER_BOUNCE 1

/*
	    sharppda_kbdstate.in - state
			       0 - not pressed
		       PRESSING1 - pressing (check chutter)
			       : - pressing (check chutter)
	           CHECK_CHUTTER - pressing (check chutter)
	            JUST_PRESSED - just pressed (throw key down)
	                 PRESSED - pressed
	              RELEASING1 - releasing (check bounce)
			       : - releasing (check bounce)
          PRESSED + CHECK_BOUNCE - releasing (check bounce)
                   JUST_RELEASED - just released (throw key up)
*/
#define CHECK_CHUTTER (2)	/* * 10ms */
#define CHECK_BOUNCE (4)	/* * 10ms */
#define NOTPRESSED (0)
#define PRESSING1 (1)
#define JUST_PRESSED (CHECK_CHUTTER+1)
#define PRESSED (CHECK_CHUTTER+2)
#define RELEASING1 (CHECK_CHUTTER+3)
#define JUST_RELEASED (CHECK_CHUTTER+CHECK_BOUNCE+3)
#define ROLLOVERERROR (CHECK_CHUTTER+CHECK_BOUNCE+4)
#define IS_PRESSING(a)	(((a) >= PRESSING1) && ((a) <= CHECK_CHUTTER))
#define IS_RELEASING(a)	(((a) >= RELEASING1) && ((a) <= PRESSED + CHECK_BOUNCE))
#define CHUTTER_BOUNCE_LOG 0

#include RAW_MAP
#if CHECK_ROLLOVER
#include RAW_MAP_ROLLOVER
#endif

/*
 *  This is the mapper for Hard-Key to Soft-Key conversion
 */

int sharp_cur_keycode[(NR_KEYCODES+1)];
int sharp_cur_status;
int tosa_initial_keypress = -1;

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

static unsigned long key_hold_delay = 120 * HZ / 100;

static unsigned char holdkey_pending = 0;
static int holdkey_pendinghardkey = 0;
static int holdkey_pendingmappedkey = 0;
static void sharppda_kbd_hold( unsigned long ignore );
static struct timer_list sharppda_kbd_hold_timer = { function: sharppda_kbd_hold };

static shappda_holdkey_info **custom_holdkey_info = NULL;

#if CHECK_ROLLOVER
// rollover_state 
//   0 .. not detected roll over error in previous key scan
//   1 .. detected roll over error in previous key scan
static int rollover_state = 0;

static unsigned char rep_hardkey = 0;
#endif

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
				rep_scancode = KBSCANCDE(hold_keycode,KBDOWN);
#if CHECK_ROLLOVER
				rep_hardkey = holdkey_pendinghardkey;
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

//	printk("kbd_press=%d\n",keycode);

	spin_lock_irqsave(&kbd_spinlock,flags);
#if CHECK_CHUTTER_BOUNCE
	if (IS_RELEASING(sharppda_kbdstate[keycode].in)) {
		// detected bounce ?
		sharppda_kbdstate[keycode].in = PRESSED;
#if CHUTTER_BOUNCE_LOG
		printk("BOUNCE? keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
	}
	else if (IS_PRESSING(sharppda_kbdstate[keycode].in) ||
	    (sharppda_kbdstate[keycode].in == NOTPRESSED)) {
		// pressing
		sharppda_kbdstate[keycode].in++;
#if CHUTTER_BOUNCE_LOG
		printk("PRESSING keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
	}
	if (sharppda_kbdstate[keycode].in == JUST_PRESSED) {
		keysdown++;
		newpress = 1;
		sharppda_kbdstate[keycode].in = PRESSED;
#if CHUTTER_BOUNCE_LOG
		printk("JUST_PRESSED keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
	}
#else
	if (!sharppda_kbdstate[keycode].in) {
		keysdown++; 
		newpress = 1;
#if CHECK_ROLLOVER
		sharppda_kbdstate[keycode].in=1;
#endif
	}
#endif

	/*
	 * We approximate a retriggable monostable
	 * action.
	 */

#if !CHECK_ROLLOVER
	sharppda_kbdstate[keycode].in=1;
#endif
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
		//		printk("handle_scankey=%x\n",KBSCANCDE(keycode_mapped,KBDOWN));
		del_timer(&sharppda_kbd_rep_timer);
		init_timer(&sharppda_kbd_rep_timer);
		sharppda_kbd_rep_timer.expires = jiffies + key_repeat_delay;
		add_timer( &sharppda_kbd_rep_timer );
		rep_scancode = KBSCANCDE(keycode_mapped,KBDOWN);
#if CHECK_ROLLOVER
		rep_hardkey = keycode;
#endif
	}
}

#if CHECK_ROLLOVER
void sharppda_kbd_press_rollover(int keycode)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_spinlock,flags);
#if CHECK_CHUTTER_BOUNCE
	if (sharppda_kbdstate[keycode].in == NOTPRESSED) {
		keysdown++; 
		sharppda_kbdstate[keycode].in=ROLLOVERERROR;
#if CHUTTER_BOUNCE_LOG
		printk("ROLLOVERERROR keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
	}
#else
	if (!sharppda_kbdstate[keycode].in) {
		keysdown++; 
		sharppda_kbdstate[keycode].in=2;
	}
#endif
	// key is down but not handle_scancode()
	spin_unlock_irqrestore(&kbd_spinlock,flags);
}

void sharppda_kbd_press_force_rollover(int keycode)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_spinlock,flags);
#if CHECK_CHUTTER_BOUNCE
	if ((sharppda_kbdstate[keycode].in != PRESSED) &&
	    (sharppda_kbdstate[keycode].in != ROLLOVERERROR)) {
		keysdown++; 
	}
	sharppda_kbdstate[keycode].in=ROLLOVERERROR;
#if CHUTTER_BOUNCE_LOG
	printk("force ROLLOVERERROR keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
#else
	if (!sharppda_kbdstate[keycode].in) {
		keysdown++; 
	}
	sharppda_kbdstate[keycode].in=2;
#endif
	// key is down but not handle_scancode()
	spin_unlock_irqrestore(&kbd_spinlock,flags);
}
#endif

void sharppda_kbd_release(int keycode)
{
	unsigned long flags;
	unsigned char newrelease = 0;
	int keycode_mapped = keycode;

//printk("kbd_release=%d\n",keycode);
	spin_lock_irqsave(&kbd_spinlock,flags);
#if CHECK_CHUTTER_BOUNCE
	if (IS_PRESSING(sharppda_kbdstate[keycode].in)) {
		// detected chutter ?
		sharppda_kbdstate[keycode].in = NOTPRESSED;
#if CHUTTER_BOUNCE_LOG
		printk("CHUTTER? keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
	}
	else if (IS_RELEASING(sharppda_kbdstate[keycode].in) ||
		 (sharppda_kbdstate[keycode].in == PRESSED)) {
		// releasing
		sharppda_kbdstate[keycode].in++;
#if CHUTTER_BOUNCE_LOG
		printk("RELEASING keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
	}
	else if (sharppda_kbdstate[keycode].in == ROLLOVERERROR) {
		keysdown--;
		sharppda_kbdstate[keycode].in = NOTPRESSED;
#if CHUTTER_BOUNCE_LOG
		printk("was ROLLOVERERROR keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
	}
	if (sharppda_kbdstate[keycode].in == JUST_RELEASED) {
		keysdown--;
		newrelease = 1;
		sharppda_kbdstate[keycode].in = NOTPRESSED;
#if CHUTTER_BOUNCE_LOG
		printk("JUST_RELEASED keycode %d state %d\n", keycode, sharppda_kbdstate[keycode].in);
#endif
	}
#else
	if (sharppda_kbdstate[keycode].in) {
		keysdown--; 
#if CHECK_ROLLOVER
		if (sharppda_kbdstate[keycode].in==1) {
#endif
		newrelease = 1;
#if CHECK_ROLLOVER
		}
#endif
	}
#endif

#if !CHECK_CHUTTER_BOUNCE
	sharppda_kbdstate[keycode].in=0;
#endif
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
		//		printk("handle_scankey=%x\n",KBSCANCDE(keycode_mapped,KBUP));
		if( rep_scancode == KBSCANCDE(keycode_mapped,KBDOWN) ){
			rep_scancode = 0;
#if CHECK_ROLLOVER
			rep_hardkey = 0;
#endif
			//DEBUGPRINT(("%%"));
			del_timer( &sharppda_kbd_rep_timer );
		}
	}

}

/*
 * Keyboard Scan routine , using GPIO
 */


#if CHECK_ROLLOVER
#define MAX_KEY_PRESS (6)
#endif

static void port_key_delay_proc( unsigned long port_key );
#define PORT_KEY_NUM	3
#define IDPORT_ON	0
#define IDPORT_REC	1
#define IDPORT_SYNC	2
static int port_key_setting[PORT_KEY_NUM] = { 30, 90, 5  }; // -1:disable, 0~: x10ms wait for detect
static int port_key_delay_state[PORT_KEY_NUM] = {NOTPRESSED,NOTPRESSED,NOTPRESSED};
static struct timer_list port_key_delay_timer = { function: port_key_delay_proc };
static int port_key_pending = 0;

static void reset_port_key_delay(void)
{
  int i;
  del_timer(&port_key_delay_timer);
  for (i=0; i<PORT_KEY_NUM; i++) port_key_delay_state[i]=NOTPRESSED;
  port_key_pending = 0;
}

// num = 0:On key, 1:Rec key, 2:Sync key
// val = -1:disable, 0~: x10ms wait for detect
int set_port_key_setting( int num, int val )
{
  if (num<0 || num>=PORT_KEY_NUM) return -1;
  port_key_setting[num] = val;
  port_key_delay_state[num] = NOTPRESSED;	
  port_key_pending = 0;
  return 0;
}
int get_port_key_setting( int num )
{
  if (num<0 || num>=PORT_KEY_NUM) return -1;
  return port_key_setting[num];
}

static int read_port_key_status_raw(void)
{
  int val=0;
  // Power
  if ((GPLR0 & GPIO_bit(GPIO_ON_KEY))==0) val |= KEYMASK_ON;
  // Record
  if ((GPLR0 & GPIO_bit(GPIO_RECORD_BTN))==0) val |= KEYMASK_REC;
  // Sync
  if ((GPLR0 & GPIO_bit(GPIO_SYNC))==0) val |= KEYMASK_SYNC;
  return val;
}

static void port_key_delay_proc( unsigned long port_key )
{
  int val=read_port_key_status_raw();

  switch( port_key ) {
  case IDPORT_ON:
    if (port_key_delay_state[IDPORT_ON]!=PRESSING1 || (val&KEYMASK_ON) ) {
      port_key_delay_state[IDPORT_ON] = PRESSED;
    } else {
      port_key_delay_state[IDPORT_ON] = NOTPRESSED;
    }
    break;
  case IDPORT_REC:
    if (port_key_delay_state[IDPORT_REC]!=PRESSING1 || (val&KEYMASK_REC) ) {
      port_key_delay_state[IDPORT_REC] = PRESSED;
    } else {
      port_key_delay_state[IDPORT_REC] = NOTPRESSED;
    }
    break;
  case IDPORT_SYNC:
    if (port_key_delay_state[IDPORT_SYNC]!=PRESSING1 || (val&KEYMASK_SYNC) ) {
      port_key_delay_state[IDPORT_SYNC] = PRESSED;
    } else {
      port_key_delay_state[IDPORT_SYNC] = NOTPRESSED;
    }
    break;
  }
  port_key_pending = 0;
}

static int read_port_key_status(void)
{
  int ret=0,val=read_port_key_status_raw();

  // Power
  if (val & KEYMASK_ON) { // press
    if (port_key_setting[IDPORT_ON]==0) { // normal key
      ret |= KEYMASK_ON;
    } else if (port_key_setting[IDPORT_ON]>0) { // delay key
      if (port_key_delay_state[IDPORT_ON]==PRESSED) { // already detected
	ret |= KEYMASK_ON;
      } else if (port_key_delay_state[IDPORT_ON]==NOTPRESSED) { // start timer
	reset_port_key_delay();
	port_key_delay_state[IDPORT_ON]=PRESSING1;
	init_timer(&port_key_delay_timer);
	port_key_delay_timer.expires = jiffies + port_key_setting[IDPORT_ON];
	port_key_delay_timer.data = IDPORT_ON;
	add_timer(&port_key_delay_timer);
	port_key_pending = 1;
	return ret; // ignore following key
      }
    }
  } else { // release
    if (port_key_delay_state[IDPORT_ON]!=NOTPRESSED && port_key_setting[IDPORT_ON]>0) {
      del_timer(&port_key_delay_timer);
      port_key_pending = 0;
      port_key_delay_state[IDPORT_ON]=NOTPRESSED;
    }
  }


  // Record
  if (val & KEYMASK_REC) { // press
    if (port_key_setting[IDPORT_REC]==0) { // normal key
      ret |= KEYMASK_REC;
    } else if (port_key_setting[IDPORT_REC]>0) { // delay key
      if (port_key_delay_state[IDPORT_REC]==PRESSED) { // already detected
	ret |= KEYMASK_REC;
      } else if (port_key_delay_state[IDPORT_REC]==NOTPRESSED) { // start timer
	reset_port_key_delay();
	port_key_delay_state[IDPORT_REC]=PRESSING1;
	init_timer(&port_key_delay_timer);
	port_key_delay_timer.expires = jiffies + port_key_setting[IDPORT_REC];
	port_key_delay_timer.data = IDPORT_REC;
	add_timer(&port_key_delay_timer);
	port_key_pending = 1;
	return ret; // ignore following key
      }
    }
  } else { // release
    if (port_key_delay_state[IDPORT_REC]!=NOTPRESSED && port_key_setting[IDPORT_REC]>0) {
      del_timer(&port_key_delay_timer);
      port_key_pending = 0;
      port_key_delay_state[IDPORT_REC]=NOTPRESSED;
    }
  }

  // Sync
  if (val & KEYMASK_SYNC) { // press
    if (port_key_setting[IDPORT_SYNC]==0) { // normal key
      ret |= KEYMASK_SYNC;
    } else if (port_key_setting[IDPORT_SYNC]>0) { // delay key
      if (port_key_delay_state[IDPORT_SYNC]==PRESSED) { // already detected
	ret |= KEYMASK_SYNC;
      } else if (port_key_delay_state[IDPORT_SYNC]==NOTPRESSED) { // start timer
	reset_port_key_delay();
	port_key_delay_state[IDPORT_SYNC]=PRESSING1;
	init_timer(&port_key_delay_timer);
	port_key_delay_timer.expires = jiffies + port_key_setting[IDPORT_SYNC];
	port_key_delay_timer.data = IDPORT_SYNC;
	add_timer(&port_key_delay_timer);
	port_key_pending = 1;
	return ret; // ignore following key
      }
    }
  } else { // release
    if (port_key_delay_state[IDPORT_SYNC]!=NOTPRESSED && port_key_setting[IDPORT_SYNC]>0) {
      del_timer(&port_key_delay_timer);
      port_key_pending = 0;
      port_key_delay_state[IDPORT_SYNC]=NOTPRESSED;
    }
  }

  return ret;
}

void sharppda_scan_key_gpio(void* dummy)
{
	int col,row;
	int rowd;
	unsigned long flags;
	int real_col;
	int count = 0;
#if CHECK_ROLLOVER
	int pressed_key[MAX_KEY_PRESS + 1];
	int i, rollover_count = 0, rollover_error = 0;
	unsigned long rollover_flag = 0;
#endif
#if CHECK_CHUTTER
	int loop;
#endif

	WAIT_CHATTERING_DELAY;

	spin_lock_irqsave(&kbd_spinlock,flags);

	CHARGE_ALL;

	for(col=0;col<KB_COLS+1;col++){
		real_col = GET_REAL_COL_FROM_COL(col);
		/*
		 * Discharge the output driver capacitatance
		 * in the keyboard matrix. (Yes it is significant..)
		 */
		if (real_col<KB_COLS) { // matrix key
		  DISCHARGE_ALL;
		  WAIT_DISCHARGE_DELAY;
		  ACTIVATE_COL(real_col);
		  
		  WAIT_ACTIVATE_DELAY;
		  rowd = GET_ROWS_STATUS(real_col);

#if 1 // for BRB
		  if (isBrb) {
		    rowd &= 0x0000000f;
		  }
#endif
		} else { // GPIO port key
		  rowd = read_port_key_status();
		}
		for( row = 0 ; row < KB_ROWS ; row++ ){

			if( rowd & KB_ROWMASK(row) ){
#if CHECK_ROLLOVER
				if ((rawkeytable_table_NormalLower[KEYCODE(row, col)] != KEY_IGN)) {
					pressed_key[count] = KEYCODE(row, col);
					rollover_flag |= rollover_flag_table[KEYCODE(row, col)];
					if (count < MAX_KEY_PRESS) {
						count++;
					}
					else {
						sharppda_kbd_press_rollover(KEYCODE(row,col));
					}
				}
#else
				sharppda_kbd_press(KEYCODE(row,col));
				count++;
#endif
			}
			else{
				if( sharppda_kbdstate[KEYCODE(row, col)].in ){
					sharppda_kbd_release(KEYCODE(row,col));
				}
			}
		}
		if (real_col<KB_COLS) {
		  RESET_COL(real_col);
		}
	}	/* end of for */

#if CHECK_ROLLOVER
	if (count >= MAX_KEY_PRESS) {
		rollover_error = 1;
	}
	else {
		rollover_count = count;
		if (ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_LSHIFT)) {
			rollover_count--;
		}
		if (ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_RSHIFT)) {
			rollover_count--;
		}
		if (ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_FUNCTION)) {
			rollover_count--;
		}
		if ((rollover_count > 3) ||
			((rollover_count == 3) &&
			 (!ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_TAB) ||
			  ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_DIRECT_ON)) &&
			 (!ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_VERTICAL) ||
			  !ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_HORIZONTAL) ||
			  !ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_ENTQUESPC)))) {
			rollover_error = 1;
		}
	}
	//	printk("count=%d,rollover_count=%d,rollover_error=%d\n",count,rollover_count,rollover_error);
	if (rollover_error == 0) {
#if 0
		if (ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_VERTICAL) &&
			ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_HORIZONTAL) &&
			ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_ENTQUESPC)) {
			printk("keyboard : vertical and horizontal and [Enter, ?, Space] pressed\n");
		}
#endif
		for (i = 0; i < count; i++) {
			sharppda_kbd_press(pressed_key[i]);
		}
		rollover_state = 0;
	}
	else {
		if (rollover_state == 0) {
#if 0
			printk("keyboard : roll over error!\n");
			if (count >= MAX_KEY_PRESS) {
				printk("keyboard : %d key pressed\n", count);
			}
			else if (rollover_count > 3) {
				printk("keyboard : %d rollover key pressed\n", rollover_count);
			}
			else if (ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_TAB) &&
					 ROLLOVER_CHECK_FLAG(rollover_flag, ROLLOVER_DIRECT_ON)) {
				printk("keyboard : TAB and direct ON key pressed\n");
			}
#endif
			// stop repeat
			if (rep_hardkey) {
#if CHECK_CHUTTER_BOUNCE
				for (loop = 0; loop <= CHECK_BOUNCE; loop++) {
					sharppda_kbd_release(rep_hardkey);
				}
#else
				sharppda_kbd_release(rep_hardkey);
#endif
				sharppda_kbd_press_rollover(rep_hardkey);
			}
		}
		for (i = 0; i < count; i++) {
			sharppda_kbd_press_rollover(pressed_key[i]);
		}
		rollover_state = 1;
	}
#endif

#ifdef USE_KBD_IRQ
	/*
	 * drive all for next interrupts
	 */
	DRIVE_ALL_COLS;
#endif

	// press OK key to erase flash
	if (tosa_initial_keypress<0) {
#if CHECK_CHUTTER_BOUNCE
		if ((sharppda_kbdstate[0x38].in==PRESSED) ||
		    IS_PRESSING(sharppda_kbdstate[0x38].in)) {
#else
#if CHECK_ROLLOVER
		if (sharppda_kbdstate[0x38].in==1) {
#else
		if (sharppda_kbdstate[0x38].in) {
#endif
#endif
			tosa_initial_keypress = 1;
		} else {
			tosa_initial_keypress = 0;
		}
	}


	/*
	 * Re-queue ourselves
	 */
#if CHECK_CHUTTER_BOUNCE
	for (loop = 0; loop < NR_KEYS; loop++) {
		if (port_key_pending) {
		  queue_task(&kbdhw_task,&tq_timer);
		  break;
		}
		if (sharppda_kbdstate[loop].in != NOTPRESSED) {
			queue_task(&kbdhw_task,&tq_timer);
			break;
		}
	}
#else
#ifdef USE_KBD_IRQ
	if ( count > 0 )
#endif
	  queue_task(&kbdhw_task,&tq_timer);
#endif
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
    isBrb = checkBrb();
    init_timer(&port_key_delay_timer);
}

void sharppda_scan_logdriver_suspend(void)
{
	del_timer( &sharppda_kbd_rep_timer );
	del_timer( &sharppda_kbd_hold_timer );
	rep_scancode = 0;
#if CHECK_ROLLOVER
	rep_hardkey = 0;
#endif
	holdkey_pending = 0;
	holdkey_pendinghardkey = 0;
	holdkey_pendingmappedkey = 0;
}

void sharppda_scan_logdriver_resume(void)
{
	queue_task(&kbdhw_task,&tq_timer);
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

#if 0
static int sharppda_kbd_keyscan(void)
{
    unsigned long flags;
    int row, col, rowd, press_row, press_col, real_col;

    WAIT_CHATTERING_DELAY;

    spin_lock_irqsave(&kbd_spinlock,flags);

    press_col = -1;
    press_row = -1;
    CHARGE_ALL;
    for (col = 0; col < KB_COLS; col++) {
	real_col = (GET_REAL_COL_FROM_COL(col));
	DISCHARGE_ALL;
	WAIT_DISCHARGE_DELAY;
	ACTIVATE_COL(real_col);

	WAIT_ACTIVATE_DELAY;
	rowd = GET_ROWS_STATUS(real_col);
	for (row = 0; row < KB_ROWS; row++) {
	    if (rowd & KB_ROWMASK(row)) {
		if (press_row >= 0) {
		    press_row = -1;
		    col = KB_COLS;
		    break;
		}
		press_row = row;
		press_col = col;
	    }
	}
	RESET_COL(real_col);
    }

    if (press_col != 0)
	press_row = -1;

#ifdef USE_KBD_IRQ
    DRIVE_ALL_COLS;
#endif

    spin_unlock_irqrestore(&kbd_spinlock,flags);

    return press_row;
}

static int sharppda_kbd_keycheck(int row, int col)
{
    unsigned long flags;
    int press, real_col;

    WAIT_CHATTERING_DELAY;

    spin_lock_irqsave(&kbd_spinlock,flags);

    CHARGE_ALL;
    real_col = (GET_REAL_COL_FROM_COL(col));
    DISCHARGE_ALL;
    WAIT_DISCHARGE_DELAY;
    ACTIVATE_COL(real_col);

    WAIT_ACTIVATE_DELAY;
    if (GET_ROWS_STATUS(real_col) & KB_ROWMASK(row))
	press = 1;
    else
	press = 0;
    RESET_COL(real_col);

#ifdef USE_KBD_IRQ
    DRIVE_ALL_COLS;
#endif

    spin_unlock_irqrestore(&kbd_spinlock,flags);

    return press;
}

#define KEYBOARD_WAKEUP_WAIT	300

static void sharppda_kbd_holdkeycheck(int row, int col)
{
    int count;

    for (count = 0; count < KEYBOARD_WAKEUP_WAIT; count++) {
	if (!sharppda_kbd_keycheck(row, col))
	    break;
	mdelay(1);
    }
    if (row != 7)	/* Power key */
	sharppda_kbd_press(KEYCODE(row, col));
    if (count >= KEYBOARD_WAKEUP_WAIT) {
	sharppda_kbd_press(KEYCODE(row, col));
	sharppda_kbd_hold(0);
    }
}
#endif

static void sharppda_kbd_press_repeat( int code )
{
  int i;
  for (i=0; i<JUST_PRESSED; i++) sharppda_kbd_press(code);
}
static void sharppda_kbd_release_repeat( int code )
{
  int i;
  for (i=0; i<JUST_RELEASED-PRESSED; i++) sharppda_kbd_release(code);
}

int sharppda_kbd_is_wakeup(void)
{
  unsigned long flags;
  int count = 0, err = 0;
  int cur_portkey,last_portkey=0;
  int cur_matrixkey,last_matrixkey=0;
  const int strobe_num = 4;
  const int success_count = 10;
  
  while( !(count>success_count || err>50) ) {
    // read GPIO(On,Rec,Sync) key
    cur_portkey = read_port_key_status_raw();

    // read Strobe4(Address,Calendar,Menu,Mail) key
    spin_lock_irqsave(&kbd_spinlock,flags);
    DISCHARGE_ALL;
    WAIT_DISCHARGE_DELAY;
    ACTIVATE_COL(strobe_num);
    WAIT_ACTIVATE_DELAY;
    cur_matrixkey = GET_ROWS_STATUS(strobe_num);
    spin_unlock_irqrestore(&kbd_spinlock,flags);
    // compare with last state
    if (cur_portkey != last_portkey ||
	cur_matrixkey != last_matrixkey) {
      count = 0;
      err++;
      last_portkey = cur_portkey;
      last_matrixkey = cur_matrixkey;
    } else {
      count++;
    }
    mdelay(10);
  }
  if (count>success_count) {
    printk("key interrupt on (%04x,%04x)\n",last_portkey,last_matrixkey);
    if (last_portkey & KEYMASK_ON) { // On ... only status change
      sharppda_kbdstate[KEYCODE(0, KB_COLS)].in = PRESSED;
      port_key_delay_state[IDPORT_ON]=PRESSED;
    }
    if (last_portkey & KEYMASK_REC) { // Rec
      if (logical_wakeup_src_mask&IDPM_WAKEUP_REC) {
	if (port_key_setting[IDPORT_REC]>=0) {
	  sharppda_kbd_press_repeat(KEYCODE(1, KB_COLS));
	  sharppda_kbd_release_repeat(KEYCODE(1, KB_COLS));
	}
      } else last_portkey &= ~KEYMASK_REC;
    }
    if (last_portkey & KEYMASK_SYNC) { // Sync
      if (logical_wakeup_src_mask&IDPM_WAKEUP_SYNC) {
	if (port_key_setting[IDPORT_SYNC]>=0) {
	  sharppda_kbd_press_repeat(KEYCODE(2, KB_COLS));
	  sharppda_kbd_release_repeat(KEYCODE(2, KB_COLS));
	}
      } else last_portkey &= ~KEYMASK_SYNC;
    }
    if (last_matrixkey & KEYMASK_ADDRESS) { // Address
      if (logical_wakeup_src_mask&IDPM_WAKEUP_ADDRESSBOOK) {
	sharppda_kbd_press_repeat(KEYCODE(3, strobe_num));
	sharppda_kbd_release_repeat(KEYCODE(3, strobe_num));
      } else last_matrixkey &= ~KEYMASK_ADDRESS;
    }
    if (last_matrixkey & KEYMASK_CALENDAR) { // Calendar
      if (logical_wakeup_src_mask&IDPM_WAKEUP_CALENDAR) {
	sharppda_kbd_press_repeat(KEYCODE(4, strobe_num));
	sharppda_kbd_release_repeat(KEYCODE(4, strobe_num));
      } else last_matrixkey &= ~KEYMASK_CALENDAR;
    }
    if (last_matrixkey & KEYMASK_MENU) { // Menu
      if (logical_wakeup_src_mask&IDPM_WAKEUP_MENU) {
	sharppda_kbd_press_repeat(KEYCODE(5, strobe_num));
	sharppda_kbd_release_repeat(KEYCODE(5, strobe_num));
      } else last_matrixkey &= ~KEYMASK_MENU;
    }
    if (last_matrixkey & KEYMASK_MAIL) { // Mail (Holdkey event)
      if (logical_wakeup_src_mask&IDPM_WAKEUP_MAIL) {
	sharppda_kbd_press_repeat(KEYCODE(6, strobe_num));
	sharppda_kbd_hold(0);
      } else last_matrixkey &= ~KEYMASK_MAIL;
    }
    return (last_portkey || last_matrixkey)?1:0;
  }
  return 0;
}

#define MAX_RESET_KEY_COMBINATION 3

#define RESET_KEY_NONE	(0)
#define RESET_KEY_FUNC	(KEYCODE(5, 10))
#define RESET_KEY_HOME	(KEYCODE(4, 5))
#define RESET_KEY_A		(KEYCODE(2, 0))
#define RESET_KEY_B		(KEYCODE(4, 2))
#define RESET_KEY_C		(KEYCODE(3, 2))
#define RESET_KEY_D		(KEYCODE(2, 2))
#define RESET_KEY_K		(KEYCODE(0, 5))
#define RESET_KEY_M		(KEYCODE(5, 3))
#define RESET_KEY_P		(KEYCODE(0, 7))
#define RESET_KEY_Q		(KEYCODE(1, 0))
#define RESET_KEY_R		(KEYCODE(4, 1))
#define RESET_KEY_T		(KEYCODE(1, 2))
#define RESET_KEY_Z		(KEYCODE(3, 0))

static int reset_key_combination[][MAX_RESET_KEY_COMBINATION] = {
	{RESET_KEY_FUNC,	RESET_KEY_HOME,	RESET_KEY_NONE},	/* Fn + Home */
	{RESET_KEY_P,		RESET_KEY_D,	RESET_KEY_NONE},	/* P + D */
	{RESET_KEY_M,		RESET_KEY_D,	RESET_KEY_NONE},	/* M + D */
	{RESET_KEY_R,		RESET_KEY_D,	RESET_KEY_NONE},	/* R + D */
	{RESET_KEY_FUNC,	RESET_KEY_P,	RESET_KEY_D},		/* Fn + P + D */
	{RESET_KEY_FUNC,	RESET_KEY_M,	RESET_KEY_D},		/* Fn + M + D */
	{RESET_KEY_Q,		RESET_KEY_T,	RESET_KEY_NONE},	/* Q + T */
	{RESET_KEY_C,		RESET_KEY_D,	RESET_KEY_NONE},	/* C + D */
	{RESET_KEY_Z,		RESET_KEY_D,	RESET_KEY_NONE},	/* Z + D */
	{RESET_KEY_K,		RESET_KEY_D,	RESET_KEY_NONE},	/* K + D */
	{RESET_KEY_A,		RESET_KEY_B,	RESET_KEY_NONE},	/* A + B */
	{RESET_KEY_B,		RESET_KEY_D,	RESET_KEY_NONE},	/* B + D */
	{RESET_KEY_A,		RESET_KEY_D,	RESET_KEY_NONE},	/* A + D */
	{RESET_KEY_NONE,	RESET_KEY_NONE,	RESET_KEY_NONE}		/* End Mark */
};

static int sharppda_kbd_resetcheck_one(void)
{
    unsigned long flags;
    int row, col, rowd, real_col, i, j, k, press_num = 0, pressed, set_num;
	int press_key[MAX_RESET_KEY_COMBINATION] = {0, 0, 0};

    WAIT_CHATTERING_DELAY;
    spin_lock_irqsave(&kbd_spinlock,flags);
    CHARGE_ALL;
    for (col = 0; col < KB_COLS; col++) {
		real_col = (GET_REAL_COL_FROM_COL(col));
		DISCHARGE_ALL;
		WAIT_DISCHARGE_DELAY;
		ACTIVATE_COL(real_col);
		WAIT_ACTIVATE_DELAY;
		rowd = GET_ROWS_STATUS(real_col);
		for (row = 0; row < KB_ROWS; row++) {
			if (rowd & KB_ROWMASK(row)) {
				if (press_num >= MAX_RESET_KEY_COMBINATION) {
					return 0;	/* too many keys are pressed */
				}
				press_key[press_num++] = KEYCODE(row, col);
			}
		}
		RESET_COL(real_col);
    }

#ifdef USE_KBD_IRQ
    DRIVE_ALL_COLS;
#endif

    spin_unlock_irqrestore(&kbd_spinlock,flags);

	for (i = 0; reset_key_combination[i][0] != 0; i++) {
		set_num = 0;
		for (j = 0; j < MAX_RESET_KEY_COMBINATION; j++) {
			if (reset_key_combination[i][j] == RESET_KEY_NONE) continue;
			set_num++;
			pressed = 0;
			for (k = 0; k < press_num; k++) {
				if (press_key[k] == reset_key_combination[i][j]) {
					pressed = 1;
					break;
				}
			}
			if (!pressed) break;
		}
		if ((j == MAX_RESET_KEY_COMBINATION) && (set_num == press_num)) {
			return i + 1;
		}
	}

    return 0;
}

int sharppda_kbd_resetcheck(void)
{
	int state, i;

	state = sharppda_kbd_resetcheck_one();
#if CHECK_CHUTTER_BOUNCE
	if (!state) return 0;
	for (i = 0; i < CHECK_CHUTTER; i++) {
		mdelay(5);
		if (sharppda_kbd_resetcheck_one() != state) return 0;
	}
#endif
	return state;
}
/*
 *   end of source
 */
