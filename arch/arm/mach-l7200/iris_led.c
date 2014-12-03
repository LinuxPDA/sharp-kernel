/*
 *  linux/drivers/char/iris_led.c
 *
 *  Driver for LED devices On SHARP 'Iris'
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

#define LED_HARD_BLINK_CTRL (bitPE0)
#define PHONE_COLOR_RED    (bitPB3)
#define PHONE_COLOR_GREEN  (bitPB2)

static void led1_off(void)
{
  SET_PE_OUT(LED_HARD_BLINK_CTRL);
  SET_PB_OUT(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
  SET_PEDR_LO(LED_HARD_BLINK_CTRL);
  SET_PBDR_LO(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
  SET_PESBSR_LO(LED_HARD_BLINK_CTRL);
  SET_PBSBSR_LO(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
}

static void led1_green(void)
{
  led1_off();
  SET_PBDR_HI(PHONE_COLOR_GREEN);
  SET_PBSBSR_HI(PHONE_COLOR_GREEN);
}

static void led1_red(void)
{
  led1_off();
  SET_PBDR_HI(PHONE_COLOR_RED);
  SET_PBSBSR_HI(PHONE_COLOR_RED);
}

static void led1_amber(void)
{
  led1_off();
  SET_PBDR_HI(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
  SET_PBSBSR_HI(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
}

static void led1_green_hardblink(void)
{
  led1_off();
  SET_PEDR_HI(LED_HARD_BLINK_CTRL);
  SET_PESBSR_HI(LED_HARD_BLINK_CTRL);
  SET_PBDR_HI(PHONE_COLOR_GREEN);
  SET_PBSBSR_HI(PHONE_COLOR_GREEN);
}

static void led1_red_hardblink(void)
{
  led1_off();
  SET_PEDR_HI(LED_HARD_BLINK_CTRL);
  SET_PESBSR_HI(LED_HARD_BLINK_CTRL);
  SET_PBDR_HI(PHONE_COLOR_RED);
  SET_PBSBSR_HI(PHONE_COLOR_RED);
}

static void led1_amber_hardblink(void)
{
  led1_off();
  SET_PEDR_HI(LED_HARD_BLINK_CTRL);
  SET_PESBSR_HI(LED_HARD_BLINK_CTRL);
  SET_PBDR_HI(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
  SET_PBSBSR_HI(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
}

/*
 *
 *   LED Blink Pattern Generator ( indep. of arch ? )
 *
 */

typedef struct sharpled_pattern_item {
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
 *  LED Brink Pattern for Iris
 *
 */

#define IRISLED_OFF              0
#define IRISLED_RED_HARDBLINK    1
#define IRISLED_RED_SOFTON       2
#define IRISLED_GREEN_HARDBLINK  3
#define IRISLED_GREEN_SOFTON     4
#define IRISLED_AMBER_HARDBLINK  5
#define IRISLED_AMBER_SOFTON     6
#define IRISLED_STATE_LAST       (IRISLED_AMBER_SOFTON)

static sharpled_pattern_item iris_ledpat_red_hardblink[] = {
  {
    ledstate: IRISLED_RED_HARDBLINK ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item iris_ledpat_green_hardblink[] = {
  {
    ledstate: IRISLED_GREEN_HARDBLINK ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item iris_ledpat_amber_hardblink[] = {
  {
    ledstate: IRISLED_AMBER_HARDBLINK ,
    expires: -1,
    next: -1
  }
};


static sharpled_pattern_item iris_ledpat_red_static[] = {
  {
    ledstate: IRISLED_RED_SOFTON ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item iris_ledpat_green_static[] = {
  {
    ledstate: IRISLED_GREEN_SOFTON ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item iris_ledpat_amber_static[] = {
  {
    ledstate: IRISLED_AMBER_SOFTON ,
    expires: -1,
    next: -1
  }
};

static sharpled_pattern_item iris_ledpat_red_softblink[] = {
  {
    ledstate: IRISLED_RED_SOFTON ,
    expires: HZ/10,
    next: 1
  },
  {
    ledstate: IRISLED_OFF ,
    expires: HZ/5,
    next: 2
  },
  {
    ledstate: IRISLED_RED_SOFTON ,
    expires: HZ/10,
    next: 3
  },
  {
    ledstate: IRISLED_OFF ,
    expires: HZ/5,
    next: 4
  },
  {
    ledstate: IRISLED_RED_SOFTON ,
    expires: HZ/10,
    next: 5
  },
  {
    ledstate: IRISLED_OFF ,
    expires:  HZ*2,
    next: 0
  }
};

static sharpled_pattern_item iris_ledpat_green_softblink[] = {
  {
    ledstate: IRISLED_GREEN_SOFTON ,
    expires: HZ/10,
    next: 1
  },
  {
    ledstate: IRISLED_OFF ,
    expires: HZ/5,
    next: 2
  },
  {
    ledstate: IRISLED_GREEN_SOFTON ,
    expires: HZ/10,
    next: 3
  },
  {
    ledstate: IRISLED_OFF ,
    expires: HZ/5,
    next: 4
  },
  {
    ledstate: IRISLED_GREEN_SOFTON ,
    expires: HZ/10,
    next: 5
  },
  {
    ledstate: IRISLED_OFF ,
    expires:  HZ*2,
    next: 0
  }
};

static sharpled_pattern_item iris_ledpat_amber_softblink[] = {
  {
    ledstate: IRISLED_AMBER_SOFTON ,
    expires: HZ/10,
    next: 1
  },
  {
    ledstate: IRISLED_OFF ,
    expires: HZ/5,
    next: 2
  },
  {
    ledstate: IRISLED_AMBER_SOFTON ,
    expires: HZ/10,
    next: 3
  },
  {
    ledstate: IRISLED_OFF ,
    expires: HZ/5,
    next: 4
  },
  {
    ledstate: IRISLED_AMBER_SOFTON ,
    expires: HZ/10,
    next: 5
  },
  {
    ledstate: IRISLED_OFF ,
    expires:  HZ*2,
    next: 0
  }
};

static void (*iris_led_turn_table[(IRISLED_STATE_LAST+1)])(void) = {
  &led1_off,
  &led1_red_hardblink,
  &led1_red,
  &led1_green_hardblink,
  &led1_green,
  &led1_amber_hardblink,
  &led1_amber,
};


static void iris_led_turn(unsigned long ledstate)
{
  if( ledstate > IRISLED_STATE_LAST ) return;
  //DEBUGPRINT(("iris_led_turn %ld\n",ledstate));
  (*(iris_led_turn_table[ledstate]))();
}

static sharpled_pattern_player iris_led1_player = {
  phys_turn: &iris_led_turn,
  phys_off: &led1_off
};

/*
 *
 * Interface to SHARP LED common driver
 *
 */

static int irisled_internal_logical[(SHARP_LED_WHICH_MAX+1)];

static sharpled_pattern_item* decide_physical_led(void)
{
  int color = 0; /* 0 for NONE , 1 for RED , 2 for GREEN , 3 for AMBER */
  int blink = 0; /* 0 for NONE , 1 for HARD , 2 for SOFT */
  /* turn off LED , in case of OUT-OF-AREA , except battery charging */
  DEBUGPRINT(("\n[%d %d %d %d %d %d %d %d %d %d %d %d]\n",
	      irisled_internal_logical[0],
	      irisled_internal_logical[1],
	      irisled_internal_logical[2],
	      irisled_internal_logical[3],
	      irisled_internal_logical[4],
	      irisled_internal_logical[5],
	      irisled_internal_logical[6],
	      irisled_internal_logical[7],
	      irisled_internal_logical[8],
	      irisled_internal_logical[9],
	      irisled_internal_logical[10],
	      irisled_internal_logical[11]));
  if( irisled_internal_logical[SHARP_LED_PDA] == LED_PDA_OFF ){
    if( irisled_internal_logical[SHARP_LED_CHARGER] == LED_CHARGER_CHARGING ){
      return(iris_ledpat_amber_static);
    }else if( irisled_internal_logical[SHARP_LED_CHARGER] == LED_CHARGER_ERROR ){
      return(iris_ledpat_red_static);
    }else{
      return NULL;
    }
  }
  if( irisled_internal_logical[SHARP_LED_CHARGER] == LED_CHARGER_ERROR ){
    return(iris_ledpat_red_static);
  }
  /* decide color , regarding to Battery Status */
  switch( irisled_internal_logical[SHARP_LED_BATTERY] ){
  case LED_BATTERY_VERY_LOW:
    DEBUGPRINT(("BATTERY VERYLOW\n"));
    color = 1;
    break;
  case LED_BATTERY_GOOD:
  case LED_BATTERY_LOW:
    color = 2;
  default:
    break;
  }
  /* color is amber , in case of off-charging */
  if( irisled_internal_logical[SHARP_LED_CHARGER] ){
    DEBUGPRINT(("BATTERY charging\n"));
    color = 3;
  }

  if( ! irisled_internal_logical[SHARP_LED_PHONE_RSSI] ){
    if( ! irisled_internal_logical[SHARP_LED_SALARM]
	&&  ! irisled_internal_logical[SHARP_LED_DALARM] ){
      if( irisled_internal_logical[SHARP_LED_CHARGER] ){
	return(iris_ledpat_amber_static);
      }else{
	return NULL;
      }
    }else{
      switch(color){
      case 1:
	return(iris_ledpat_red_softblink);
      case 3:
	return(iris_ledpat_amber_softblink);
      case 2:
      default:
	return(iris_ledpat_green_softblink);
	break;
      }
    }
  }
  if( ! irisled_internal_logical[SHARP_LED_PHONE_IN] 
      && ! irisled_internal_logical[SHARP_LED_PHONE_DIAL]
      && ! irisled_internal_logical[SHARP_LED_MAIL_EXISTS]
      && ! irisled_internal_logical[SHARP_LED_SALARM]
      &&  ! irisled_internal_logical[SHARP_LED_DALARM] ){
    switch(color){
    case 1:
      return(iris_ledpat_red_hardblink);
    case 3:
      return(iris_ledpat_amber_hardblink);
    case 2:
    default:
      return(iris_ledpat_green_hardblink);
      break;
    }
  }
  if( irisled_internal_logical[SHARP_LED_PHONE_IN]
      || irisled_internal_logical[SHARP_LED_MAIL_EXISTS]
      || irisled_internal_logical[SHARP_LED_SALARM]
      || irisled_internal_logical[SHARP_LED_DALARM] ){
    switch(color){
    case 1:
      return(iris_ledpat_red_softblink);
    case 3:
      return(iris_ledpat_amber_softblink);
    case 2:
    default:
      return(iris_ledpat_green_softblink);
      break;
    }
  }
  if( irisled_internal_logical[SHARP_LED_PHONE_DIAL] == LED_DIAL_HOLDING ){
    switch(color){
    case 1:
      return(iris_ledpat_red_softblink);
    case 3:
      return(iris_ledpat_amber_softblink);
    case 2:
    default:
      return(iris_ledpat_green_softblink);
      break;
    }
  }
  if( ! irisled_internal_logical[SHARP_LED_PHONE_IN] 
      && irisled_internal_logical[SHARP_LED_PHONE_DIAL] == LED_DIAL_DIALING
      && ! irisled_internal_logical[SHARP_LED_MAIL_EXISTS]
      && ! irisled_internal_logical[SHARP_LED_SALARM]
      &&  ! irisled_internal_logical[SHARP_LED_DALARM] ){
    switch(color){
    case 1:
      return(iris_ledpat_red_hardblink);
    case 3:
      return(iris_ledpat_amber_hardblink);
    case 2:
    default:
      return(iris_ledpat_green_hardblink);
      break;
    }
  }
  return NULL;
#if 0
  if( ! irisled_internal_logical[SHARP_LED_PHONE_RSSI] ){
    /* not in area... */
    if( irisled_internal_logical[SHARP_LED_CHARGER] ){
      /* charging */
      DEBUGPRINT(("OUT OF AREA CHARGING\n"));
      color = 3;
      blink = 0;
      goto RETURN_PATTERN;
    }else{
      /* not charging */
      DEBUGPRINT(("OUT OF AREA\n"));
      return NULL;
    }
  }
  /* decide color , regarding to Battery Status */
  switch( irisled_internal_logical[SHARP_LED_BATTERY] ){
  case LED_BATTERY_VERY_LOW:
    DEBUGPRINT(("BATTERY VERYLOW\n"));
    color = 1;
    break;
  case LED_BATTERY_GOOD:
  case LED_BATTERY_LOW:
    color = 2;
  default:
    break;
  }
  /* color is amber , in case of off-charging */
  if( irisled_internal_logical[SHARP_LED_PDA] == LED_PDA_OFF ){
    if( irisled_internal_logical[SHARP_LED_CHARGER] ){
      DEBUGPRINT(("BATTERY charging\n"));
      color = 3;
    }
  }
  /* hard-blink , default */
  blink = 1;
  /* if special event exists , soft-blink */
  if( irisled_internal_logical[SHARP_LED_PHONE_IN] ){
    /* Incoming Call */
    DEBUGPRINT(("INCOMING CALL\n"));
    blink = 2;
  }
  if( irisled_internal_logical[SHARP_LED_MAIL_EXISTS] ){
    /* New Mail */
    DEBUGPRINT(("INCOMING MAIL\n"));
    blink = 2;
  }
  if( irisled_internal_logical[SHARP_LED_DALARM] ){
    /* daily alarm */
    DEBUGPRINT(("DAILY ALARM\n"));
    blink = 2;
  }
  if( irisled_internal_logical[SHARP_LED_SALARM] ){
    /* schedule alarm */
    DEBUGPRINT(("SCHEDULE ALARM\n"));
    blink = 2;
  }
  if( irisled_internal_logical[SHARP_LED_PHONE_DIAL] == LED_DIAL_HOLDING ){
    /* holding */
    DEBUGPRINT(("HOLDING\n"));
    blink = 2;
  }
  /* map from color/blink to pattern struct */
 RETURN_PATTERN:
  DEBUGPRINT(("Color = %d , Blink = %d\n",color,blink));
  switch(color){
  case 1: /* red */
    switch(blink){
    case 1: /* hard */
      return(iris_ledpat_red_hardblink);
    case 2: /* soft */
      return(iris_ledpat_red_softblink);
    case 0: /* static */
    default:
      return(iris_ledpat_red_static);
    }
  case 2: /* green */
    switch(blink){
    case 1: /* hard */
      return(iris_ledpat_green_hardblink);
    case 2: /* soft */
      return(iris_ledpat_green_softblink);
    case 0: /* static */
    default:
      return(iris_ledpat_green_static);
    }
  case 3: /* amber */
    switch(blink){
    case 1: /* hard */
      return(iris_ledpat_amber_hardblink);
    case 2: /* soft */
      return(iris_ledpat_amber_softblink);
    case 0: /* static */
    default:
      return(iris_ledpat_amber_static);
    }
  case 0: /* none */
  default:
    return NULL;
    break;
  }
  return NULL;
#endif
}

/*
 *  for coverage test.
 */



/*
 *  Common Interface
 */

int iris_led_supported(int which) /* return -EINVAL if unspoorted , otherwise return >= 0 value */
{
  switch(which){
  case SHARP_LED_PDA:               /* PDA status */
    return 2; /* Running (RUNNING) / Off (ERROR) */
  case SHARP_LED_BATTERY:           /* main battery status */
    return 3; /* Battery OK(GOOD) / Charged(LOW) / Low(VERY_LOW) */
  case SHARP_LED_CHARGER:
    return 2; /* Charger OFF (OFF) / ON (ON) */
  case SHARP_LED_PHONE_RSSI:        /* phone status (RSSI...) */
    return 2; /* In Area (IN) / Out Of Area (OUT) */
  case SHARP_LED_PHONE_IN:          /* phone status (incoming..) */
    return 2; /* Waiting (WAITING) / Incoming (INCOMING) */
  case SHARP_LED_PHONE_DIAL:
    return 3; /* None (OFF) / Dialing (DIAL) / Holding (HOLD) */
  case SHARP_LED_MAIL_EXISTS:
    return 2; /* New Mail/SMS Exists (EXISTS) / NO (NO_UNREAD_MAIL) */
  case SHARP_LED_DALARM:            /* daily alarm */
  case SHARP_LED_SALARM:            /* schedule alarm */
    return 2; /* Event Exist / Event Not Exist  */
  default:
    return -EINVAL;
    break;
  }
  return 2;
}

sharpled_pattern_item* curpat = NULL;

int iris_turn_led_status(int which,int status) /* set LED status to directed value. */
{
  sharpled_pattern_item* pat;
  if( which < 0 || which > SHARP_LED_WHICH_MAX ) return -EINVAL;
  irisled_internal_logical[which] = status;
  if( curpat ) sharpled_pattern_end(&iris_led1_player);
  if( ( pat = decide_physical_led() ) != NULL ){
    sharpled_pattern_start(&iris_led1_player,pat);
  }
  curpat = pat;
  return 0;
}

int iris_init_led(void)
{
  return 0;
}

int iris_suspend_led(void)
{
  /* force LED off */
  sharpled_pattern_end(&iris_led1_player);
  led1_off();
  curpat = NULL;
  return 0;
}
int iris_standby_led(void)
{
  /* do nothing */
  return 0;
}
int iris_resume_led(void)
{
  /* restore if necessary */
  return 0;
}
