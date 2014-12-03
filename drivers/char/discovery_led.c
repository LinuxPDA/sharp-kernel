/*
 *  linux/drivers/char/discovery_led.c
 *
 *  Driver for LED devices On SHARP 'SL-A300'
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
 *	27-AUG-2002 Edward Chen, Steve Lin for Discovery
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
#include <asm/sharp_char.h>
#include <asm/arch/discovery_asic3.h>
#include <asm/arch/power_consumption.h>

#define	MSG_LED		0	/* print debug message 1:print 0:no need */
#define DEBUGPRINT(s)   /* printk s */

#define LED_PRIORITY_OFF				0
#define LED_PRIORITY_CHARGING			1
#define LED_PRIORITY_NEW_MAIL			2
#define LED_PRIORITY_SCHEDULE			3
#define LED_PRIORITY_CHARGE_ERROR		4


/*
 *
 * Physical level drivers
 *
 */

static void led0_off(void) //RED
{
#if	MSG_LED
	printk(" in led0_off() \n");
#endif
	ASIC3_LEDPTS_CH_LED = 0;
	ASIC3_LEDCNTL_CH_LED &= ~GPIO_P4; /* LEDEN off */
}

static void led0_on(void)
{
#if	MSG_LED
	printk(" in led0_on() \n");
#endif
	ASIC3_LEDPTS_CH_LED	= 1;
	ASIC3_LEDDTS_CH_LED	= 2;
	ASIC3_LEDCNTL_CH_LED |= GPIO_P4; /* LEDEN on */
}

static void led0_fastblink(void)
{
#if	MSG_LED
	printk(" in led0_fastblink() \n");
#endif
	ASIC3_LEDPTS_CH_LED	= 0x8;
	ASIC3_LEDDTS_CH_LED	= 0x4;
	ASIC3_LEDCNTL_CH_LED |= GPIO_P4; /* LEDEN on */
}

static void led0_slowblink(void)
{
#if	MSG_LED
	printk(" in led0_slowblink() \n");
#endif
	ASIC3_LEDPTS_CH_LED	= 0x20;
	ASIC3_LEDDTS_CH_LED	= 0x4;
	ASIC3_LEDCNTL_CH_LED |= GPIO_P4; /* LEDEN on */
}

/* LED1: mail and other... */
static void led1_off(void)
{
#if	MSG_LED
	printk(" in led0_off() \n");
#endif
	ASIC3_LEDPTS_NOTIFY_LED = 0;
	ASIC3_LEDCNTL_NOTIFY_LED &= ~GPIO_P4; /* LEDEN off */
}

static void led1_on(void)
{
#if	MSG_LED
	printk(" in led0_on() \n");
#endif
	ASIC3_LEDPTS_NOTIFY_LED	= 1;
	ASIC3_LEDDTS_NOTIFY_LED	= 2;
	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_P4; /* LEDEN on */
}

static void led1_flashon(void)
{
}

static void led1_flashoff(void)
{
}

static void led1_veryfastblink(void)
{
}

static void led1_fastblink(void)
{
#if	MSG_LED
	printk(" in led0_fastblink() \n");
#endif
	ASIC3_LEDPTS_NOTIFY_LED	= 0x8;
	ASIC3_LEDDTS_NOTIFY_LED	= 0x4;
	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_P4; /* LEDEN on */
}

static void led1_normalblink(void)
{
}

static void led1_slowblink(void)
{
#if	MSG_LED
	printk(" in led0_slowblink() \n");
#endif
	ASIC3_LEDPTS_NOTIFY_LED	= 0x20;
	ASIC3_LEDDTS_NOTIFY_LED	= 0x4;
	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_P4; /* LEDEN on */
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
  if( ! player ) return;
  if( ! player->top ) return;
  if( player->phys_turn ) (*(player->phys_turn))(player->top[player->curpos].ledstate);
  next = player->top[player->curpos].next;
  if( next >= 0 && player->top[player->curpos].expires > 0 ){
    player->timer.expires = jiffies + player->top[player->curpos].expires;
    add_timer(&(player->timer));
    player->curpos = next;
  }else{
  }
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

/*
 *
 * Interface to SHARP LED common driver
 *
 */

static int collieled_internal_logical[(SHARP_LED_WHICH_MAX+1)];

static int nLEDPriorityCurrent = LED_PRIORITY_OFF;	
//-----------------------------------------------------------------------------
//
// Update LED status
// There are two LED for charging, schedule and mail. one of two could be on or 
// blinking at the same time.
//
//  entry :
//    LED update status 
//  return :
//
#define printf
static int UpdateLEDStatus(int nLEDPriority)
{
	printf("+UpdateLEDStatus (%d)\r\n", nLEDPriority);
	if (nLEDPriority != nLEDPriorityCurrent)
	{
		printf("Turn off all LEDs\r\n");
		led0_off();
		led1_off();
		clr_driver_waste_bits(NFLED_blink_waste | NFLED_on_waste);
		switch(nLEDPriority)
		{
			case LED_PRIORITY_CHARGE_ERROR:
				printf("orange LED blink\r\n");
				led0_fastblink();
				break;

			case LED_PRIORITY_SCHEDULE:
				printf("green LED blink\r\n");
				led1_fastblink();
				set_driver_waste_bits(NFLED_blink_waste);
				break;

			case LED_PRIORITY_NEW_MAIL:
				printf("green LED on\r\n");
				led1_on();
				set_driver_waste_bits(NFLED_on_waste);
				break;

			case LED_PRIORITY_CHARGING:
				printf("orange LED on\r\n");
				led0_on();
				break;
		}
		
		nLEDPriorityCurrent = nLEDPriority;
	}

	printf("-UpdateLEDStatus\r\n");
	return(nLEDPriorityCurrent);
}
//-----------------------------------------------------------------------------
//
// There are many combinations of LED behavior, below is detail description
// battery charging : orange led on => priority 1
// battery charge full : orange led off
// battery charge off : orange led off
// battery charge error : orange led blinking ==> priority 4
// schedule alarm on : green led blinking ==> priority 3
// schedule alarm off : green led off
// new mail exist : green led on ==> priority 2
// no new mail : green led of
// priority number : bigger is higher priority
//
static void collie_led0_process(void)
{
	int *leds = collieled_internal_logical;
	int nLEDCharge, nLEDSchedule, nLEDMail, nLEDUpdate;

	printf("+collie_led0_process\r\n");
	nLEDCharge = leds[SHARP_LED_CHARGER];
	nLEDSchedule = leds[SHARP_LED_SALARM];
	nLEDMail = leds[SHARP_LED_MAIL_EXISTS];
	nLEDUpdate = LED_PRIORITY_OFF;

	if (nLEDCharge == LED_CHARGER_ERROR)
	{
		printf("LED_PRIORITY_CHARGE_ERROR\r\n");
		nLEDUpdate = LED_PRIORITY_CHARGE_ERROR;
	}
	else if (nLEDSchedule == LED_SALARM_ON)
	{
		printf("LED_PRIORITY_SCHEDULE\r\n");
		nLEDUpdate = LED_PRIORITY_SCHEDULE;
	}
	else if (nLEDMail == LED_MAIL_NEWMAIL_EXISTS)
	{
		printf("LED_PRIORITY_NEW_MAIL\r\n");
		nLEDUpdate = LED_PRIORITY_NEW_MAIL;
	}
	else if (nLEDCharge == LED_CHARGER_CHARGING)
	{
		printf("LED_PRIORITY_CHARGING\r\n");
		nLEDUpdate = LED_PRIORITY_CHARGING;
	}

	UpdateLEDStatus(nLEDUpdate);

	printf("-collie_led0_process\r\n");
	return;
}


/*
 *  Common Interface
 */
#if 1//ned
int collie_led_supported(int which) /* return -EINVAL if unspoorted , otherwise return >= 0 value */
{
  switch(which){
  case SHARP_LED_PDA:		/* PDA status */
    return 3;
  case SHARP_LED_BATTERY:	/* main battery status */
    return 3;
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
#endif //ned


int collie_turn_led_status(int which,int status) /* set LED status to directed value. */
{
	sharpled_pattern_item* pat;
	if( which < 0 || which > SHARP_LED_WHICH_MAX ) return -EINVAL;
	collieled_internal_logical[which] = status;
	collie_led0_process();

	return 0;
}

int collie_init_led(void)
{
	return 0;
}


/* for sleep mode */
int collie_suspend_led(void)
{
	ASIC3_LEDPTS_CH_LED = 0;
	ASIC3_LEDCNTL_CH_LED &= ~GPIO_P4; /* LEDEN off */
	ASIC3_LEDPTS_NOTIFY_LED = 0;
	ASIC3_LEDCNTL_NOTIFY_LED &= ~GPIO_P4; /* LEDEN off */
  return 0;
}


int collie_resume_led(void)
{
	/* 1. Alternative function */
	ASIC3_GPIO_ALT_C |= (GPIO_P0+GPIO_P1+GPIO_P2);

	/* 2. clk_32768hz */
	ASIC3_CLOCK_CDEXCDCX |= GPIO_P13;

	/* 2. clk LED0,LED1 */
	ASIC3_CLOCK_CDEXCDCX &= ~(GPIO_P6+GPIO_P7);

	/* 3. CH LED */
	ASIC3_LEDCNTL_CH_LED |= GPIO_V1;
	ASIC3_LEDCNTL_CH_LED &= ~GPIO_P4; /* LEDEN off */
	ASIC3_LEDCNTL_CH_LED &= ~GPIO_P5;
	ASIC3_LEDCNTL_CH_LED |= GPIO_P6;

	ASIC3_LEDPTS_CH_LED |= GPIO_V1;
	
	ASIC3_LEDDTS_CH_LED |= 0;
	
	/* 3. Notify LED */
	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_V1;
	ASIC3_LEDCNTL_NOTIFY_LED &= ~GPIO_P4;
	ASIC3_LEDCNTL_NOTIFY_LED &= ~GPIO_P5;
	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_P6;

	ASIC3_LEDPTS_NOTIFY_LED |= GPIO_V1;
	
	ASIC3_LEDDTS_NOTIFY_LED = 0;

	/* 4. clk LED0,LED1 */
	ASIC3_CLOCK_CDEXCDCX |= GPIO_P6+GPIO_P7;

	/* 5. CH/NOTIFY LEDEN */
//	ASIC3_LEDCNTL_CH_LED |= GPIO_P4;
//	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_P4;

	/* test to close */
	ASIC3_LEDPTS_CH_LED = 0; /* off */
	ASIC3_LEDPTS_NOTIFY_LED = 0; /* off */

	set_led_status(SHARP_LED_PDA,LED_PDA_RUNNING);
	
	return 0;
}
