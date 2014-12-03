/*
 *  linux/drivers/char/sharpsl_led.c
 *
 *  Driver for LED devices On SHARP 'Poodle/Corgi'
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
 *    based on collie_led.c Aug. 26 2002 LINEO
 *    12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *     1-Nov-2003 Sharp Corporation   for Tosa
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
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#if defined(CONFIG_ARCH_PXA_CORGI)
#include <asm/arch/corgi.h>
#endif
#if defined(CONFIG_ARCH_PXA_TOSA)
#include <asm/arch/tosa.h>
#endif
#include <asm/arch/hardware.h>

#include <linux/timer.h>

#include <asm/sharp_char.h>

#define DEBUGPRINT(s)   /* printk s */

/*
 *
 * Physical level drivers
 *
 */




#if defined(CONFIG_ARCH_PXA_POODLE)
void sharpsl_charge_err_off(void)
{
  set_led_status(SHARP_LED_CHARGER,LED_CHARGER_ERROR);
}
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
static int led_suspended=0;
void sharpsl_charge_wait_ms(void)
{
  unsigned long time = OSCR;

  while(1) {
    if ( ( OSCR - time ) > 3700 ) break;
  }

}

void sharpsl_charge_wait(int ms)
{
  while(1) {
    if ( --ms < 0 ) break;
    sharpsl_charge_wait_ms();
  }
}

#if defined (CONFIG_ARCH_PXA_TOSA)
void sharpsl_charge_err_off(void)
{
  while(1) {
    set_scoop_jc_gpio(SCP_LED_ORANGE);
    sharpsl_charge_wait( 250 );
    reset_scoop_jc_gpio(SCP_LED_ORANGE);
    sharpsl_charge_wait( 250 );
    if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN))!=0) break;
  }
}
#else // for Corgi
void sharpsl_charge_err_off(void)
{
  while(1) {
    GPSR(GPIO_LED_ORANGE) = GPIO_bit(GPIO_LED_ORANGE);
    sharpsl_charge_wait( 250 );
    GPCR(GPIO_LED_ORANGE) = GPIO_bit(GPIO_LED_ORANGE);
    sharpsl_charge_wait( 250 );
    if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN))==0) break;
  }
}
#endif
#endif

#if defined(CONFIG_ARCH_PXA_POODLE)
#define	LED_ONOFF_MASK (LCM_LPT_TOFL|LCM_LPT_TOFH)
#define	LED_BLNK_VAL(ON,OFF) (LCM_LPT_TOH(ON)|LCM_LPT_TOL(OFF))
#define	LED_BLNK_MASK (LED_ONOFF_MASK|LED_BLNK_VAL(7,7))
#define	LED_OFF(REG) ((REG)|=LCM_LPT_TOFL)
#define	LED_ON(REG) ((REG)=((REG)&~LED_ONOFF_MASK)|LCM_LPT_TOFH)
#define	LED_BLNK(REG,X,Y) ((REG)=((REG)&~LED_BLNK_MASK)|LED_BLNK_VAL(X,Y))
#endif

#if defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
struct timer_list orange_led_blink_timer;
struct st_orange_blink {
	int blink_switch;
	long blink_interval;
} st_orange = { 0, 0 };

#define BLINK_FAST		1
#define BLINK_SLOW		2
#define ORANGE_LED_SW		1
#define GREEN_LED_SW		2
#if defined (CONFIG_ARCH_PXA_TOSA)
#define BLUE_LED_SW		3
#define WLAN_LED_SW		4
#define LED_NUM			4

static int get_ledport( int sw )
{
  int port=-1;
  switch(sw) {
  case ORANGE_LED_SW:
    port = SCP_LED_ORANGE;
    break;
  case GREEN_LED_SW:
    port = SCP_LED_GREEN;
    break;
  case BLUE_LED_SW:
    port = SCP_LED_BLUE;
    break;
  case WLAN_LED_SW:
    port = SCP_LED_WLAN;
    break;
  }
  return port;
}

static void set_led( int sw, int on )
{
  int port=get_ledport(sw);
  if (port==-1) {
    printk(__FUNCTION__ ": unknown LED %d\n",sw);
    return;
  }
  if (on) set_scoop_jc_gpio(port);
  else reset_scoop_jc_gpio(port);
  //  printk("setled(%d,%d)\n",sw,on);
}

static int get_led( int sw )
{
  int port=get_ledport(sw);
  if (port==-1) {
    printk(__FUNCTION__ ": unknown LED %d\n",sw);
    return;
  }
  return (SCP_JC_REG_GPWR&port)?1:0;
}

#define LED_OFF(n)	set_led(n,0)
#define LED_ON(n)	set_led(n,1)
#define LED_STATUS(n)	get_led(n)
static int led_resume_buffer=0;
#endif

static void blink_orange_led(unsigned long data)
{
	struct st_orange_blink *orange = (struct st_orange_blink *)data;

	if (orange->blink_switch == 0) {
#if defined (CONFIG_ARCH_PXA_TOSA)
	        LED_ON(ORANGE_LED_SW);
#else
		GPSR(GPIO_LED_ORANGE) = GPIO_bit(GPIO_LED_ORANGE);
#endif
		orange->blink_switch = 1;
	} else {
#if defined (CONFIG_ARCH_PXA_TOSA)
	        LED_OFF(ORANGE_LED_SW);
#else
		GPCR(GPIO_LED_ORANGE) = GPIO_bit(GPIO_LED_ORANGE);
#endif
		orange->blink_switch = 0;
	}

	init_timer(&orange_led_blink_timer);
	orange_led_blink_timer.function = blink_orange_led;
	orange_led_blink_timer.expires = jiffies + orange->blink_interval;
	add_timer(&orange_led_blink_timer);
}

void orange_led_start_blink(int interval)
{
	switch (interval) {
	case BLINK_FAST:
		st_orange.blink_interval = HZ / 4;
		break;
	case BLINK_SLOW:
		st_orange.blink_interval = (4*HZ)/5 + (2*HZ)/25;
		break;
	default :
		return;
	}

	orange_led_blink_timer.data = (unsigned long)&st_orange;
	blink_orange_led((unsigned long)&st_orange);
}

void orange_led_stop_blink(void)
{
	if (st_orange.blink_interval == 0)
		return;
	st_orange.blink_switch = 0;
	st_orange.blink_interval = 0;
	del_timer(&orange_led_blink_timer);
}

#if ! defined(CONFIG_ARCH_PXA_TOSA)
#define	LED_OFF(n)	( (n == ORANGE_LED_SW) ? (GPCR(GPIO_LED_ORANGE) = GPIO_bit(GPIO_LED_ORANGE)) : (reset_scoop_gpio(SCP_LED_GREEN)) )

#define	LED_ON(n)	( (n == ORANGE_LED_SW) ? (GPSR(GPIO_LED_ORANGE) = GPIO_bit(GPIO_LED_ORANGE)) : (set_scoop_gpio(SCP_LED_GREEN)) )
#endif

#define	LED_BLNK(n)	orange_led_start_blink(n)
#endif /* CONFIG_ARCH_PXA_CORGI */

/* LED0: battery, power etc... */

static void led0_off(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	LED_OFF(LCM_LPT0);
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	LED_OFF(ORANGE_LED_SW);
	orange_led_stop_blink();
#endif
}

static void led0_on(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	LED_ON(LCM_LPT0);
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	LED_ON(ORANGE_LED_SW);
#endif
}

static void led0_fastblink(void)
{
	led0_off();
#if defined(CONFIG_ARCH_PXA_POODLE)
	LED_BLNK(LCM_LPT0,2,2);
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	LED_BLNK(BLINK_FAST);
#endif
}

static void led0_slowblink(void)
{
	led0_off();
#if defined(CONFIG_ARCH_PXA_POODLE)
	LED_BLNK(LCM_LPT0,7,7);
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	LED_BLNK(BLINK_SLOW);
#endif
}

/* LED1: mail and other... */

static void led1_off(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	LED_OFF(LCM_LPT1);
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	LED_OFF(GREEN_LED_SW);
#endif
}

static void led1_on(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	LED_ON(LCM_LPT1);
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	LED_ON(GREEN_LED_SW);
#endif
}

static void led1_flashon(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	led1_off();
	LED_BLNK(LCM_LPT1,1,7);
#endif
}

static void led1_flashoff(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	led1_off();
	LED_BLNK(LCM_LPT1,7,1);
#endif
}

static void led1_veryfastblink(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	led1_off();
	LED_BLNK(LCM_LPT1,1,1);
#endif
}

static void led1_fastblink(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	led1_off();
	LED_BLNK(LCM_LPT1,2,2);
#endif
}

static void led1_normalblink(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	led1_off();
	LED_BLNK(LCM_LPT1,4,4);
#endif
}

static void led1_slowblink(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	led1_off();
	LED_BLNK(LCM_LPT1,7,7);
#endif
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
	if( player->phys_turn )
		(*(player->phys_turn))(player->top[player->curpos].ledstate);
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
 *  LED Brink Pattern for Poodle/Corgi
 *
 */

#define SHARPSLLED_OFF              0
#define SHARPSLLED_ON               1
#define SHARPSLLED_FLASHON          2
#define SHARPSLLED_FLASHOFF         3
#define SHARPSLLED_VERYFASTBLINK    4
#define SHARPSLLED_FASTBLINK        5
#define SHARPSLLED_NORMALBLINK      6
#define SHARPSLLED_SLOWBLINK        7
#define SHARPSLLED_STATE_LAST       (SHARPSLLED_SLOWBLINK)

static sharpled_pattern_item sharpsl_ledpat_static[] = {
  {
	ledstate: SHARPSLLED_ON ,
	expires: -1,
	next: -1
  }
};

static sharpled_pattern_item sharpsl_ledpat_flashon[] = {
#if defined(CONFIG_ARCH_PXA_POODLE)
  {
	ledstate: SHARPSLLED_FLASHON ,
	expires: -1,
	next: -1
  }
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
  {
	ledstate: SHARPSLLED_ON ,
	expires: (HZ/10 + HZ/50),
	next: 1
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: ((4*HZ)/5 + (2*HZ)/25),
	next: 0
  }
#endif
};

static sharpled_pattern_item sharpsl_ledpat_flashoff[] = {
#if defined(CONFIG_ARCH_PXA_POODLE)
  {
	ledstate: SHARPSLLED_FLASHOFF ,
	expires: -1,
	next: -1
  }
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
  {
	ledstate: SHARPSLLED_ON ,
	expires: ((4*HZ)/5 + (2*HZ)/25),
	next: 1
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: (HZ/10 + HZ/50),
	next: 0
  }
#endif
};

static sharpled_pattern_item sharpsl_ledpat_veryfastblink[] = {
#if defined(CONFIG_ARCH_PXA_POODLE)
  {
	ledstate: SHARPSLLED_VERYFASTBLINK ,
	expires: -1,
	next: -1
  }
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
  {
	ledstate: SHARPSLLED_ON ,
	expires: (HZ/10 + HZ/50),
	next: 1
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: (HZ/10 + HZ/50),
	next: 0
  }
#endif
};

static sharpled_pattern_item sharpsl_ledpat_fastblink[] = {
#if defined(CONFIG_ARCH_PXA_POODLE)
  {
	ledstate: SHARPSLLED_FASTBLINK ,
	expires: -1,
	next: -1
  }
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
  {
	ledstate: SHARPSLLED_ON ,
	expires: HZ/4,
	next: 1
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: HZ/4,
	next: 0
  }
#endif
};

static sharpled_pattern_item sharpsl_ledpat_normalblink[] = {
#if defined(CONFIG_ARCH_PXA_POODLE)
  {
	ledstate: SHARPSLLED_NORMALBLINK ,
	expires: -1,
	next: -1
  }
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
  {
	ledstate: SHARPSLLED_ON ,
	expires: HZ/2,
	next: 1
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: HZ/2,
	next: 0
  }
#endif
};

static sharpled_pattern_item sharpsl_ledpat_slowblink[] = {
#if defined(CONFIG_ARCH_PXA_POODLE)
  {
	ledstate: SHARPSLLED_SLOWBLINK ,
	expires: -1,
	next: -1
  }
#elif defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
  {
	ledstate: SHARPSLLED_ON ,
	expires: ((4*HZ)/5 + (2*HZ)/25),
	next: 1
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: ((4*HZ)/5 + (2*HZ)/25),
	next: 0
  }
#endif
};

static sharpled_pattern_item sharpsl_ledpat_softblink[] = {
  {
	ledstate: SHARPSLLED_ON ,
	expires: HZ/10,
	next: 1
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: HZ/5,
	next: 2
  },
  {
	ledstate: SHARPSLLED_ON ,
	expires: HZ/10,
	next: 3
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: HZ/5,
	next: 4
  },
  {
	ledstate: SHARPSLLED_ON ,
	expires: HZ/10,
	next: 5
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires:  HZ*2,
	next: 0
  }
};

static sharpled_pattern_item sharpsl_ledpat_softflash[] = {
  {
	ledstate: SHARPSLLED_ON ,
	expires: HZ/20,
	next: 1
  },
  {
	ledstate: SHARPSLLED_OFF ,
	expires: HZ*2 -HZ/20,
	next: 0
  }
};


static void (*sharpsl_led_turn_table[(SHARPSLLED_STATE_LAST+1)])(void) = {
	&led1_off,
	&led1_on,
	&led1_flashon,
	&led1_flashoff,
	&led1_veryfastblink,
	&led1_fastblink,
	&led1_normalblink,
	&led1_slowblink,
};


static void sharpsl_led_turn(unsigned long ledstate)
{
	if( ledstate > SHARPSLLED_STATE_LAST ) return;
	//DEBUGPRINT(("sharpsl_led_turn %ld\n",ledstate));
	(*(sharpsl_led_turn_table[ledstate]))();
}

static sharpled_pattern_player sharpsl_led1_player = {
	phys_turn: &sharpsl_led_turn,
	phys_off: &led1_off
};

/*
 *
 * Interface to SHARP LED common driver
 *
 */

static int sharpslled_internal_logical[(SHARP_LED_WHICH_MAX+1)];

static void sharpsl_led0_process(void)
{
	int *leds = sharpslled_internal_logical;
	if( leds[SHARP_LED_COLLIE_0] ) {
		switch( leds[SHARP_LED_COLLIE_0] ) {
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
	if( leds[SHARP_LED_ACSTATUS] == LED_AC_CONNECTED ) {
#else
	if( 1 ) {
#endif	/* delete ac status */
		switch( leds[SHARP_LED_CHARGER] ) {
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

#if defined(CONFIG_ARCH_PXA_TOSA)
typedef struct {
  int status;
  int blink_switch;
  long blink_interval;
  struct timer_list timer;
} LED_BLINK;
static LED_BLINK st_led_status[LED_NUM+1];
static int led_status_initialized=0; 

static void led_on(int led)
{
  del_timer(&st_led_status[led].timer);
  st_led_status[led].status=1;
  LED_ON(led);
}
static void led_off(int led)
{
  del_timer(&st_led_status[led].timer);
  st_led_status[led].status=0;
  LED_OFF(led);
}

static void led_blink_func(unsigned long data)
{
  int led = data;
  if (st_led_status[led].blink_switch==0 && led_suspended==0 ) {
    LED_ON(led);
    st_led_status[led].blink_switch=1;
  } else {
    LED_OFF(led);
    st_led_status[led].blink_switch=0;
  }
  init_timer(&st_led_status[led].timer);
  st_led_status[led].timer.function = led_blink_func;
  st_led_status[led].timer.expires = jiffies + st_led_status[led].blink_interval;
  st_led_status[led].timer.data = led;
  add_timer(&st_led_status[led].timer);
}

static void led_start_blink(int led, int interval)
{
  del_timer(&st_led_status[led].timer);
  switch (interval) {
  case BLINK_SLOW:
    st_led_status[led].blink_interval = (4*HZ)/5 + (2*HZ)/25;
    st_led_status[led].status = 2;
    break;
  case BLINK_FAST:
    st_led_status[led].blink_interval = HZ / 4;
    st_led_status[led].status = 3;
    break;
  default :
    return;
  }

  st_led_status[led].blink_switch=0;
  led_blink_func(led);
}

static void sharpsl_led_process_ex(int led_sw)
{
  int *leds = sharpslled_internal_logical;
  int status=-1;
  switch(led_sw) {
  case BLUE_LED_SW:
    status = leds[SHARP_LED_BLUETOOTH];
    break;
  case WLAN_LED_SW:
    status = leds[SHARP_LED_WLAN];
    break;
  }
  if ( status == -1 || status == st_led_status[led_sw].status ) return;
  if (led_suspended) status=0; // force off
  switch(status) {
  case 0: // off
    led_off(led_sw);
    return;
  case 1: // on
    led_on(led_sw);
    return;
  case 2: // blink
    led_start_blink(led_sw,BLINK_SLOW);
    return;
  case 3: // blink fast
    led_start_blink(led_sw,BLINK_FAST);
    return;
  }
  led_off(led_sw);
}
#endif

static sharpled_pattern_item* decide_physical_led(void)
{
	int *leds = sharpslled_internal_logical;
	DEBUGPRINT(("\n[%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d]\n",
	      sharpslled_internal_logical[0],
	      sharpslled_internal_logical[1],
	      sharpslled_internal_logical[2],
	      sharpslled_internal_logical[3],
	      sharpslled_internal_logical[4],
	      sharpslled_internal_logical[5],
	      sharpslled_internal_logical[6],
	      sharpslled_internal_logical[7],
	      sharpslled_internal_logical[8],
	      sharpslled_internal_logical[9],
	      sharpslled_internal_logical[10],
	      sharpslled_internal_logical[11],
	      sharpslled_internal_logical[12],
	      sharpslled_internal_logical[13],
	      sharpslled_internal_logical[14],
	      sharpslled_internal_logical[15]));

	/******* 1st: controlled *******/
#if defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	if( (leds[SHARP_LED_COLLIE_1] > LED_COLLIE_1_ON) &&
	    (leds[SHARP_LED_PDA] == LED_PDA_OFF) )
		return NULL;
#endif
	switch( leds[SHARP_LED_COLLIE_1] ) {
	case LED_COLLIE_1_OFF:
		return NULL;
	case LED_COLLIE_1_ON:
		return sharpsl_ledpat_static;
	case LED_COLLIE_1_FLASHON:
		return sharpsl_ledpat_flashon;
	case LED_COLLIE_1_FLASHOFF:
		return sharpsl_ledpat_flashoff;
	case LED_COLLIE_1_VFSTBLINK:
		return sharpsl_ledpat_veryfastblink;
	case LED_COLLIE_1_FASTBLINK:
		return sharpsl_ledpat_fastblink;
	case LED_COLLIE_1_NORMBLINK:
		return sharpsl_ledpat_normalblink;
	case LED_COLLIE_1_SLOWBLINK:
		return sharpsl_ledpat_slowblink;
	case LED_COLLIE_1_SOFTBLINK:
		return (leds[SHARP_LED_PDA] != LED_PDA_OFF) ?
			sharpsl_ledpat_softblink : NULL;
	case LED_COLLIE_1_SOFTFLASH:
		return (leds[SHARP_LED_PDA] != LED_PDA_OFF) ?
			sharpsl_ledpat_softflash : NULL;
	}

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	/* A3-1:R:SHARP_LED_CHARGER=LED_CHARGER_ERROR */

#if defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	/* A3-non-spec:G:turn off */
	if( leds[SHARP_LED_PDA] == LED_PDA_OFF )
		return NULL;
#endif

	/* A3-2:G:SHARP_LED_SALARM=LED_SALARM_ON */
	if( (leds[SHARP_LED_DALARM] == LED_DALARM_ON) ||
	    (leds[SHARP_LED_SALARM] == LED_SALARM_ON) )
	    return sharpsl_ledpat_normalblink;

#if defined(CONFIG_ARCH_PXA_POODLE)
	/* A3-non-spec:G:turn off */
	if( leds[SHARP_LED_PDA] == LED_PDA_OFF )
		return NULL;
#endif 

	/* A3-3:G:SHARP_LED_MAIL_EXISTS=LED_MAIL_NEWMAIL_EXISTS */
	if( leds[SHARP_LED_MAIL_EXISTS] == LED_MAIL_NEWMAIL_EXISTS )
	    return sharpsl_ledpat_static;

	/* A3-4:R:SHARP_LED_CHARGER=LED_CHARGER_CHARGING */

	/* A3-non-spec:G:screen off */
	if( leds[SHARP_LED_PDA] == LED_PDA_SUSPENDED )
		return sharpsl_ledpat_softflash;
#else /* For SL-5000D,5500 */
	/******* 2nd: turn off *******/
	if( leds[SHARP_LED_PDA] == LED_PDA_OFF )
		return NULL;

	/******* 3rd: error blink *******/
	if( (leds[SHARP_LED_MAIL_SEND] == LED_SENDMAIL_ERROR) ||
	    (leds[SHARP_LED_COMM] == LED_COMM_ERROR) ||
	    (leds[SHARP_LED_BROWSER] == LED_BROWSER_ERROR) )
		return sharpsl_ledpat_fastblink;

	/******* 4th: online *******/
	if( (leds[SHARP_LED_MAIL_SEND] == LED_SENDMAIL_SENDING) ||
	    (leds[SHARP_LED_COMM] == LED_COMM_ONLINE) ||
	    (leds[SHARP_LED_BROWSER] == LED_BROWSER_ONLINE) )
		return sharpsl_ledpat_static;

	/******* 5th: infomation *******/
	if( (leds[SHARP_LED_MAIL_EXISTS] != LED_MAIL_NO_UNREAD_MAIL) ||
	    (leds[SHARP_LED_DALARM] != LED_DALARM_OFF) ||
	    (leds[SHARP_LED_SALARM] != LED_SALARM_OFF) )
		return sharpsl_ledpat_softblink;

	/******* 6th: screen off *******/
	if( leds[SHARP_LED_PDA] == LED_PDA_SUSPENDED )
		return sharpsl_ledpat_softflash;
#endif

	/******* default: LED off *******/
	return NULL;
}

/*
 *  for coverage test.
 */



/*
 *  Common Interface
 */

int sharpsl_led_supported(int which) /* return -EINVAL if unspoorted , otherwise return >= 0 value */
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

/* set LED status to directed value. */
int sharpsl_turn_led_status(int which, int status)
{
	sharpled_pattern_item* pat;
	if( which < 0 || which > SHARP_LED_WHICH_MAX ) return -EINVAL;
	sharpslled_internal_logical[which] = status;
	sharpsl_led0_process();
#if defined(CONFIG_ARCH_PXA_TOSA)
	{
	  int i;
	  for (i=3; i<=LED_NUM; i++) {
	    sharpsl_led_process_ex(i);
	  }
	}
#endif
	if( curpat ) sharpled_pattern_end(&sharpsl_led1_player);
	if( ( pat = decide_physical_led() ) != NULL ){
		sharpled_pattern_start(&sharpsl_led1_player,pat);
	}
	curpat = pat;
	return 0;
}

int sharpsl_init_led(void)
{
#if defined(CONFIG_ARCH_PXA_TOSA)
  if (!led_status_initialized) {
    memset(&st_led_status,0,sizeof(st_led_status));
    led_status_initialized=1;
  }
#endif
	return 0;
}
int sharpsl_suspend_led(void)
{
#if defined(CONFIG_ARCH_PXA_TOSA)
  led_suspended=1;
  {
    int i;
    led_resume_buffer = 0;
    for (i=1; i<=LED_NUM; i++) {
      if (LED_STATUS(i)) {
	led_resume_buffer |= 0x1<<i;
	LED_OFF(i);
      }
    }
  }
#endif
	return 0;
}
int sharpsl_resume_led(void)
{
#if defined(CONFIG_ARCH_PXA_TOSA)
  led_suspended=0;
  {
    int i;
    for (i=1; i<=LED_NUM; i++) {
      if (led_resume_buffer & (0x1<<i)) {
	LED_ON(i);
      }
    }
    led_resume_buffer = 0;
  }
#endif
	return 0;
}
