/*
 *  linux/drivers/char/collie_led.c
 *
 *  Driver for LED devices On SHARP 'Collie'
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
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <linux/timer.h>
#include <asm/arch/gpio.h>

#include <asm/sharp_char.h>

#define DEBUGPRINT(s)   /* printk s */

/*
 *
 * Physical level drivers
 *
 */

#define	LED_ONOFF_MASK (LCM_LPT_TOFL|LCM_LPT_TOFH)
#define	LED_BLNK_VAL(ON,OFF) (LCM_LPT_TOH(ON)|LCM_LPT_TOL(OFF))
#define	LED_BLNK_MASK (LED_ONOFF_MASK|LED_BLNK_VAL(7,7))
#define	LED_OFF(REG) ((REG)|=LCM_LPT_TOFL)
#define	LED_ON(REG) ((REG)=((REG)&~LED_ONOFF_MASK)|LCM_LPT_TOFH)
#define	LED_BLNK(REG,X,Y) ((REG)=((REG)&~LED_BLNK_MASK)|LED_BLNK_VAL(X,Y))

/* LED0: battery, power etc... */

static void led0_off(void)
{
	LED_OFF(LCM_LPT0);
}

static void led0_on(void)
{
	LED_ON(LCM_LPT0);
}

static void led0_fastblink(void)
{
	led0_off();
	LED_BLNK(LCM_LPT0,2,2);
}

static void led0_slowblink(void)
{
	led0_off();
	LED_BLNK(LCM_LPT0,7,7);
}

/* LED1: mail and other... */

static void led1_off(void)
{
	LED_OFF(LCM_LPT1);
}

static void led1_on(void)
{
	LED_ON(LCM_LPT1);
}

static void led1_flashon(void)
{
	led1_off();
	LED_BLNK(LCM_LPT1,1,7);
}

static void led1_flashoff(void)
{
	led1_off();
	LED_BLNK(LCM_LPT1,7,1);
}

static void led1_veryfastblink(void)
{
	led1_off();
	LED_BLNK(LCM_LPT1,1,1);
}

static void led1_fastblink(void)
{
	led1_off();
	LED_BLNK(LCM_LPT1,2,2);
}

static void led1_normalblink(void)
{
	led1_off();
	LED_BLNK(LCM_LPT1,4,4);
}

static void led1_slowblink(void)
{
	led1_off();
	LED_BLNK(LCM_LPT1,7,7);
}

/*
 *
 *   LED Blink Pattern Generator ( indep. of arch ? )
 *
 */

typedef const struct sharpled_pattern_item {
  unsigned long ledstate;
  int expires;
  int next;
} sharpled_pattern_item;

typedef struct sharpled_pattern_player {
  int curpos;
  sharpled_pattern_item* top;
  struct timer_list timer;
  void (*phys_turn)(unsigned long ledstate);
  void (*phys_off)(void);
} sharpled_pattern_player;

static void sharpled_pattern_play(unsigned long data)
{
  int next;
  sharpled_pattern_player* player = (sharpled_pattern_player*) data;
  //DEBUGPRINT(("patplay\n"));
  if( ! player ) return;
  if( ! player->top ) return;
  if( player->phys_turn ) (*(player->phys_turn))(player->top[player->curpos].ledstate);
  next = player->top[player->curpos].next;
  //DEBUGPRINT(("cur = %d , next = %d exp = %d\n",player->curpos,next,player->top[player->curpos].expires));
  if( next >= 0 && player->top[player->curpos].expires > 0 ){
    //DEBUGPRINT(("expire = %d\n",player->top[player->curpos].expires));
    player->timer.expires = jiffies + player->top[player->curpos].expires;
    add_timer(&(player->timer));
    player->curpos = next;
  }else{
    //DEBUGPRINT(("that's all\n"));
  }
}

static void sharpled_pattern_start(sharpled_pattern_player* player,
				   sharpled_pattern_item* patterns)
{
  //DEBUGPRINT(("patstart\n"));
  if( ! player ) return;
  if( ! patterns ) return;
  player->curpos = 0;
  player->top = patterns;
  player->timer.function = sharpled_pattern_play;
  player->timer.data = (unsigned long)player;
  sharpled_pattern_play(player->timer.data);
}

static void sharpled_pattern_end(sharpled_pattern_player* player)
{
  //DEBUGPRINT(("patend\n"));
  if( ! player ) return;
  del_timer(&(player->timer));
  if( player->phys_off ) (*(player->phys_off))();
}  


/*
 *
 *  LED Brink Pattern for Collie
 *
 */

#define COLLELED_OFF              0
#define COLLELED_ON               1
#define COLLELED_FLASHON          2
#define COLLELED_FLASHOFF         3
#define COLLELED_VERYFASTBLINK    4
#define COLLELED_FASTBLINK        5
#define COLLELED_NORMALBLINK      6
#define COLLELED_SLOWBLINK        7
#define COLLELED_STATE_LAST       (COLLELED_SLOWBLINK)

static sharpled_pattern_item collie_ledpat_static[] = {
  {
    ledstate: COLLELED_ON ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item collie_ledpat_flashon[] = {
  {
    ledstate: COLLELED_FLASHON ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item collie_ledpat_flashoff[] = {
  {
    ledstate: COLLELED_FLASHOFF ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item collie_ledpat_veryfastblink[] = {
  {
    ledstate: COLLELED_VERYFASTBLINK ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item collie_ledpat_fastblink[] = {
  {
    ledstate: COLLELED_FASTBLINK ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item collie_ledpat_normalblink[] = {
  {
    ledstate: COLLELED_NORMALBLINK ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item collie_ledpat_slowblink[] = {
  {
    ledstate: COLLELED_SLOWBLINK ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item collie_ledpat_softblink[] = {
  {
    ledstate: COLLELED_ON ,
    expires: HZ/10,
    next: 1
  },
  {
    ledstate: COLLELED_OFF ,
    expires: HZ/5,
    next: 2
  },
  {
    ledstate: COLLELED_ON ,
    expires: HZ/10,
    next: 3
  },
  {
    ledstate: COLLELED_OFF ,
    expires: HZ/5,
    next: 4
  },
  {
    ledstate: COLLELED_ON ,
    expires: HZ/10,
    next: 5
  },
  {
    ledstate: COLLELED_OFF ,
    expires:  HZ*2,
    next: 0
  }
};

static sharpled_pattern_item collie_ledpat_softflash[] = {
  {
    ledstate: COLLELED_ON ,
    expires: HZ/20,
    next: 1
  },
  {
    ledstate: COLLELED_OFF ,
    expires: HZ*2 -HZ/20,
    next: 0
  }
};


static void (*collie_led_turn_table[(COLLELED_STATE_LAST+1)])(void) = {
  &led1_off,
  &led1_on,
  &led1_flashon,
  &led1_flashoff,
  &led1_veryfastblink,
  &led1_fastblink,
  &led1_normalblink,
  &led1_slowblink,
};


static void collie_led_turn(unsigned long ledstate)
{
  if( ledstate > COLLELED_STATE_LAST ) return;
  //DEBUGPRINT(("collie_led_turn %ld\n",ledstate));
  (*(collie_led_turn_table[ledstate]))();
}

static sharpled_pattern_player collie_led1_player = {
  phys_turn: &collie_led_turn,
  phys_off: &led1_off
};

/*
 *
 * Interface to SHARP LED common driver
 *
 */

static int collieled_internal_logical[(SHARP_LED_WHICH_MAX+1)];

static void collie_led0_process(void)
{
  int *leds = collieled_internal_logical;
  if( leds[SHARP_LED_COLLIE_0] )
  {
    switch( leds[SHARP_LED_COLLIE_0] )
    {
    case LED_COLLIE_0_OFF:
      led0_off();
      return;
    case LED_COLLIE_0_ON:
      led0_on();
      return;
    case LED_COLLIE_0_FASTBLINK:
      led0_fastblink();
      return;
    case LED_COLLIE_0_SLOWBLINK:
      led0_slowblink();
      return;
    }
  }
#if 0	/* delete ac status */
  if( leds[SHARP_LED_ACSTATUS] == LED_AC_CONNECTED )
#else
  if( 1 )
#endif	/* delete ac status */
  {
    switch( leds[SHARP_LED_CHARGER] )
    {
    case LED_CHARGER_CHARGING:
      led0_on();
      return;
    case LED_CHARGER_ERROR:
      led0_fastblink();
      return;
    }
  }
  led0_off();
}

static sharpled_pattern_item* decide_physical_led(void)
{
  int status;
  int *leds = collieled_internal_logical;
  DEBUGPRINT(("\n[%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d]\n",
	      collieled_internal_logical[0],
	      collieled_internal_logical[1],
	      collieled_internal_logical[2],
	      collieled_internal_logical[3],
	      collieled_internal_logical[4],
	      collieled_internal_logical[5],
	      collieled_internal_logical[6],
	      collieled_internal_logical[7],
	      collieled_internal_logical[8],
	      collieled_internal_logical[9],
	      collieled_internal_logical[10],
	      collieled_internal_logical[11],
	      collieled_internal_logical[12],
	      collieled_internal_logical[13],
	      collieled_internal_logical[14],
	      collieled_internal_logical[15]));

  /******* 1st: controlled *******/
  switch( leds[SHARP_LED_COLLIE_1] )
  {
  case LED_COLLIE_1_OFF:
    return NULL;
  case LED_COLLIE_1_ON:
    return collie_ledpat_static;
  case LED_COLLIE_1_FLASHON:
    return collie_ledpat_flashon;
  case LED_COLLIE_1_FLASHOFF:
    return collie_ledpat_flashoff;
  case LED_COLLIE_1_VFSTBLINK:
    return collie_ledpat_veryfastblink;
  case LED_COLLIE_1_FASTBLINK:
    return collie_ledpat_fastblink;
  case LED_COLLIE_1_NORMBLINK:
    return collie_ledpat_normalblink;
  case LED_COLLIE_1_SLOWBLINK:
    return collie_ledpat_slowblink;
  case LED_COLLIE_1_SOFTBLINK:
    return (leds[SHARP_LED_PDA] != LED_PDA_OFF) ?
      collie_ledpat_softblink : NULL;
  case LED_COLLIE_1_SOFTFLASH:
    return (leds[SHARP_LED_PDA] != LED_PDA_OFF) ?
      collie_ledpat_softflash : NULL;
  }

  /******* 2nd: turn off *******/
  if( leds[SHARP_LED_PDA] == LED_PDA_OFF )
    return NULL;

  /******* 3rd: error blink *******/
  if( (leds[SHARP_LED_MAIL_SEND] == LED_SENDMAIL_ERROR) ||
      (leds[SHARP_LED_COMM] == LED_COMM_ERROR) ||
      (leds[SHARP_LED_BROWSER] == LED_BROWSER_ERROR) )
    return collie_ledpat_fastblink;

  /******* 4th: online *******/
  if( (leds[SHARP_LED_MAIL_SEND] == LED_SENDMAIL_SENDING) ||
      (leds[SHARP_LED_COMM] == LED_COMM_ONLINE) ||
      (leds[SHARP_LED_BROWSER] == LED_BROWSER_ONLINE) )
    return collie_ledpat_static;

  /******* 5th: infomation *******/
  if( (leds[SHARP_LED_MAIL_EXISTS] != LED_MAIL_NO_UNREAD_MAIL) ||
      (leds[SHARP_LED_DALARM] != LED_DALARM_OFF) ||
      (leds[SHARP_LED_SALARM] != LED_SALARM_OFF) )
    return collie_ledpat_softblink;

  /******* 6th: screen off *******/
  if( leds[SHARP_LED_PDA] == LED_PDA_SUSPENDED )
    return collie_ledpat_softflash;

  /******* default: LED off *******/
  return NULL;
}

/*
 *  for coverage test.
 */



/*
 *  Common Interface
 */

int collie_led_supported(int which) /* return -EINVAL if unspoorted , otherwise return >= 0 value */
{
  switch(which){
  case SHARP_LED_PDA:		/* PDA status */
    return 3;
  case SHARP_LED_BATTERY:	/* main battery status */
    return 3;
#if 0	/* delete ac status */
  case SHARP_LED_ACSTATUS:	/* AC line status */
    return 2;
#endif	/* delete ac status */
  case SHARP_LED_CHARGER:	/* charger status */
    return 3;
  case SHARP_LED_MAIL_EXISTS:	/* mail status (exists or not) */
    return 2;
  case SHARP_LED_MAIL_SEND:	/* mail status (sending...) */
    return 3;
  case SHARP_LED_DALARM:	/* daily alarm */
  case SHARP_LED_SALARM:	/* schedule alarm */
    return 2;
  case SHARP_LED_COLLIE_0:	/* battery LED control */
    return 5;
  case SHARP_LED_COLLIE_1:	/* mail LED control */
    return 11;
  case SHARP_LED_COMM:		/* communication status */
    return 3;
  case SHARP_LED_BROWSER:	/* WWW browser status */
    return 3;
  default:
    return -EINVAL;
    break;
  }
  return 2;
}

sharpled_pattern_item* curpat = NULL;

int collie_turn_led_status(int which,int status) /* set LED status to directed value. */
{
  sharpled_pattern_item* pat;
  if( which < 0 || which > SHARP_LED_WHICH_MAX ) return -EINVAL;
  collieled_internal_logical[which] = status;
  collie_led0_process();
  if( curpat ) sharpled_pattern_end(&collie_led1_player);
  if( ( pat = decide_physical_led() ) != NULL ){
    sharpled_pattern_start(&collie_led1_player,pat);
  }
  curpat = pat;
  return 0;
}

int collie_init_led(void)
{
  return 0;
}
int collie_suspend_led(void)
{
  return 0;
}
int collie_resume_led(void)
{
  return 0;
}
