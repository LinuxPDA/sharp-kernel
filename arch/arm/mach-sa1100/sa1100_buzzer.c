/*
 *  linux/drivers/char/sa1100_buzzer.c
 *
 *  Driver for STBUZ Interface
 *
 *  ChangeLog:
 *    04-16-2001 Lineo Japan, Inc.
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

#define DEBUGPRINT(s)   /* printk s */

/*
 * physical level drivers
 */

#define UART_CLK           3686400L /* 3.6MHz */

#define BUZZER_MODE_BZTOG        0x000 /* bit 9 */
#define BUZZER_MODE_UNDERFLOW    0x200 /* bit 9 */
#define BUZZER_TOGGLE            0x100 /* bit 8 */
#define TIMER_PSCE_STBUZ         0x400 /* bit 10 */
#define TIMER_PSCE_TIMER1        0x000 /* bit 10 */
#define TIMER_PERIODIC           0x040 /* bit 6 */
#define TIMER_ENABLE             0x080 /* bit 7 */
#define PRESCALE_div1            0x000 /* bit 2,3 */
#define PRESCALE_div16           0x002 /* bit 2,3 */
#define PRESCALE_div256          0x004 /* bit 2,3 */
#define PRESCALE_BITS            0x006 /* bit 2,3 */

void sa1100_enable_buzzer(void)
{
#if 0
  IO_TIMER1CONTROL |= TIMER_ENABLE;
#endif
}

void sa1100_disable_buzzer(void)
{
#if 0
  IO_TIMER1CONTROL &= ~TIMER_ENABLE;
#endif
}

void sa1100_set_buzzer(unsigned long frequency)
{
  unsigned long nload = ( UART_CLK / frequency ) >> 1;
  int prescale = PRESCALE_div1;
  if( nload & 0xffff0000 ){
    nload = nload >> 4;
    prescale = PRESCALE_div16;
  }
  if( nload & 0xffff0000 ){
    nload = nload >> 4;
    prescale = PRESCALE_div256;
  }
#if 0
  IO_TIMER1CONTROL &= ~BUZZER_TOGGLE;
  IO_TIMER1LOAD = nload & 0x0000ffff;
  DEBUGPRINT(("LVAL = %ld (%lx) scaling = %x\n",IO_TIMER1LOAD,IO_TIMER1LOAD,prescale));
  IO_TIMER1CONTROL &= ~(PRESCALE_BITS);
  IO_TIMER1CONTROL |= prescale;
  IO_TIMER1CONTROL |= (TIMER_PSCE_TIMER1|BUZZER_MODE_UNDERFLOW|TIMER_PERIODIC);
#endif
}

static int now_sounding = 0;

void sa1100_kd_nosound(unsigned long __unused)
{
  sa1100_disable_buzzer();
  now_sounding = 0;
}

void sa1100_kd_mksound(unsigned int hz,unsigned int ticks)
{
  static struct timer_list mksound_timer = { function: sa1100_kd_nosound };
  if( hz ){
    sa1100_enable_buzzer();
    sa1100_set_buzzer(hz);
    if( ticks ){
      if( ! now_sounding ){
	mksound_timer.expires = jiffies + ticks;
	add_timer(&mksound_timer);
	now_sounding = 1;
      }
    }
  }else{
    sa1100_disable_buzzer();
    now_sounding = 0;
  }
}


