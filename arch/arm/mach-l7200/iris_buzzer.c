/*
 *  linux/drivers/char/iris_buzzer.c
 *
 *  Driver for Buzzer devices On SHARP 'Iris'
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
 * physical level drivers
 */

extern void l72xx_enable_buzzer(void);
extern void l72xx_disable_buzzer(void);
extern void l72xx_set_buzzer(unsigned long frequency);
extern void l72xx_kd_mksound(unsigned int hz,unsigned int ticks);

#if 0
#define TOUCHSOUND_HZ     110
#define TOUCHSOUND_TICKS  (HZ/75)
#endif

#define TOUCHSOUND_HZ     440
#define TOUCHSOUND_TICKS  (HZ/60)

#ifdef DECLARE_WAITQUEUE
static wait_queue_head_t play_wait;
#else	
static struct wait_queue *play_wait;
#endif	

static int now_sounding = 0;
static int now_soundid = 0;

static void mksound_timeout(unsigned long __unused)
{
  now_sounding = 0;
  now_soundid = 0;
}

int iris_play_sound_by_id(int soundid,int volume)
{
  static struct timer_list mksound_timer = { function: mksound_timeout };

  DEBUGPRINT(("iris_play_sound_by_id XG %d %d\n",soundid,volume));
  switch( soundid ){
  case SHARP_BUZ_TOUCHSOUND:
  case SHARP_BUZ_KEYSOUND:
  case SHARP_BUZ_SCHEDULE_ALARM:
  case SHARP_BUZ_DAILY_ALARM:
  case SHARP_BUZ_GOT_MAIL:
  case SHARP_PDA_WARNSOUND:
  case SHARP_PDA_ERRORSOUND:
  case SHARP_PDA_CRITICALSOUND:
    if( now_sounding && now_soundid == soundid ) return;
    del_timer(&mksound_timer);
#if 0
    l72xx_set_buzzer(TOUCHSOUND_HZ);
    l72xx_enable_buzzer();
    mdelay(50);
    /* mdelay(TOUCHSOUND_MSECS); */
    l72xx_disable_buzzer();
#endif
#if 1
    l72xx_enable_buzzer();
    l72xx_kd_mksound(TOUCHSOUND_HZ,TOUCHSOUND_TICKS);
#endif
    now_sounding = 1;
    now_soundid = soundid;
    mksound_timer.expires = jiffies + HZ/5;
    add_timer(&mksound_timer);
    break;
  default:
    return -EINVAL;
    break;
  }
  return 0;
}

int iris_buzzer_supported(int which)
{
  DEBUGPRINT(("iris_buzzer_supported %d\n",which));
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

int iris_play_sound_by_hz(unsigned int hz,unsigned int msecs,int volume)
{
  unsigned long j;
  if( hz ){
    l72xx_enable_buzzer();
    l72xx_set_buzzer(hz);
    j = HZ * msecs / 1000;
    interruptible_sleep_on_timeout(&play_wait,j);
  }
  l72xx_disable_buzzer();
  return 0;
}

int iris_buzzer_dev_init(void)
{
  init_waitqueue_head(&play_wait);
  return 0;
}

int iris_suspend_buzzer(void)
{
  l72xx_disable_buzzer();
  return 0;
}

int iris_standby_buzzer(void)
{
  l72xx_disable_buzzer();
  return 0;
}

int iris_resume_buzzer(void)
{
  return 0;
}


