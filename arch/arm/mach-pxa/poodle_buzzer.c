/*
 * arch/arm/mach-pxa/poodle_buzzer.c
 *
 * PXA buzzer ctrl for Poodle (SHARP)
 *
 * Based on collie_buzzer
 *
 * Copyright (C) 2002  SHARP
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
 * ChangeLog:
 *	16-Jan-2003 SHARP sleep_on -> interruptible_sleep_on
 * 
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/poll.h>
#include <linux/major.h>
#include <linux/config.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/sound.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/pm.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/soundcard.h>
#include <asm/proc/cache.h>

#include <asm/sharp_char.h>



#undef DEBUG
//#define DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif

/*** sound data *********************************************************/
#include "poodle_key.h"
#include "poodle_tap.h"
#include "poodle_alarm.h"

/*** Some declarations ***********************************************/
static DECLARE_WAIT_QUEUE_HEAD(buzzer_proc);
static int now_playing = 0 ;
static int buzzer_soundid = 0;
static int repeat_sound = 0;


// following functions include poodle audio driver.
int audio_buzzer_write(const char *,int);
int audio_buzzer_intmode(const short *,int,int);
int audio_buzzer_release(void);
int audio_buzzer_open(int);

/*** buzzer ******************************************************************/
int poodle_buz_buffer_init()
{
}

int poodle_play_sound_by_id(int soundid,int volume)
{

  switch( soundid ){

  case SHARP_BUZ_TOUCHSOUND:
    if ( wm8731_busy() && !repeat_sound ) {
    } else {
      while (buzzer_soundid) schedule();
      buzzer_soundid = soundid;
      wake_up(&buzzer_proc);
    }
    break;

  case SHARP_BUZ_KEYSOUND:
    if ( wm8731_busy() && !repeat_sound ) {
    } else {
      while (buzzer_soundid) schedule();
      buzzer_soundid = soundid;
      wake_up(&buzzer_proc);
    }
    break;

  case SHARP_BUZ_SCHEDULE_ALARM:
    if ( wm8731_busy() ) {
      buzzer_soundid = soundid;
      wake_up(&buzzer_proc);
    } else {
      while (buzzer_soundid) schedule();
      buzzer_soundid = soundid;
      wake_up(&buzzer_proc);
    }
    break;

  case SHARP_PDA_WARNSOUND:
    break;

  case SHARP_BUZ_GOT_MAIL:
  case SHARP_BUZ_DAILY_ALARM:
  case SHARP_PDA_ERRORSOUND:
  case SHARP_PDA_CRITICALSOUND:
    break;

  default:
    return -EINVAL;
    break;
  }

  return 0;
}
int poodle_buzzer_supported(int which)
{
  switch( which ){
  case SHARP_BUZ_TOUCHSOUND:
  case SHARP_BUZ_KEYSOUND:
  case SHARP_BUZ_SCHEDULE_ALARM:
  case SHARP_BUZ_DAILY_ALARM:
  case SHARP_BUZ_GOT_MAIL:
  case SHARP_PDA_WARNSOUND:
  case SHARP_PDA_ERRORSOUND:
  case SHARP_PDA_CRITICALSOUND:
    break;
  default:
    return -EINVAL;
    break;
  }
  return 4;
}

int poodle_play_sound_by_hz(unsigned int hz,unsigned int msecs,int volume)
{
  return 0;
}

int current_freq;
static void change_freq( int new_freq )
{
  if (current_freq!=new_freq) {
    audio_buzzer_release();
    audio_buzzer_open(new_freq);
    current_freq = new_freq;
  }
}

static void poodle_buzzer_thread(void)
{
  // daemonize();
  strcpy(current->comm, "buzzer");
  sigfillset(&current->blocked);

  while(1) {
    if (buzzer_soundid==0) {
      //while(buzzer_soundid==0)
	interruptible_sleep_on(&buzzer_proc);
    } else {
      if (wm8731_busy()) {
	if (buzzer_soundid==SHARP_BUZ_SCHEDULE_ALARM) {
	  //audio_buzzer_intmode(alarm_data,sizeof(alarm_data),44100);
	  audio_buzzer_intmode(alarm_data,sizeof(alarm_data),8000);
	}
	buzzer_soundid=0;
	continue;
      }
      now_playing = 1;
      current_freq = key_freq;
      audio_buzzer_open(current_freq);
      while ( buzzer_soundid!=0 ) {
	int tmpid = buzzer_soundid;
	buzzer_soundid=0;

	switch( tmpid ) {
	
	case SHARP_BUZ_TOUCHSOUND:
	  repeat_sound = 1;
	  change_freq(tap_freq);
	  audio_buzzer_write(tap_data,sizeof(tap_data));
	  audio_buzzer_sync();
	  break;

	case SHARP_BUZ_KEYSOUND:
	  repeat_sound = 1;
	  change_freq(key_freq);
	  audio_buzzer_write(key_data,sizeof(key_data));
	  audio_buzzer_sync();	
	  break;

	case SHARP_BUZ_SCHEDULE_ALARM:
	  {
	    int alarm_data_size = sizeof(alarm_data);
	    unsigned char *alarm_data0 = alarm_data;
	    int cnt;

	    repeat_sound = 0;
	  //change_freq(44100);
	    change_freq(8000);
	    while(1) {
	      cnt = audio_buzzer_write(alarm_data0,alarm_data_size);
	      audio_buzzer_sync();
	      if ( cnt <= 0 ) break;
	      alarm_data_size -= cnt;
	      alarm_data0 += cnt;
	    }
	    break;
	  }

	case SHARP_PDA_WARNSOUND:
	case SHARP_BUZ_GOT_MAIL:
	case SHARP_BUZ_DAILY_ALARM:
	case SHARP_PDA_ERRORSOUND:
	case SHARP_PDA_CRITICALSOUND:
	  repeat_sound = 0;
	  break;

	default:
	  repeat_sound = 0;
	  break;
	} // switch
      } // while loop
      audio_buzzer_release();
      repeat_sound = 0;
      now_playing = 0;
    }
  } // while(1) loop
}


int poodle_buzzer_dev_init(void)
{
  kernel_thread(poodle_buzzer_thread,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

  return 0;
}

int poodle_suspend_buzzer(void)
{
#if 0
  printk("poodle_suspend_buzzer\n");
  while ( now_playing ) {
    schedule();
  }
#endif
  return 0;
}

int poodle_resume_buzzer(void)
{
  return 0;
}

void poodle_stop_sound()
{
}





