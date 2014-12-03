/*
 * linux/arch/arm/mach-pxa/pxa_nssp.c
 *
 * NSSP read routines for tosa (SHARP)
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <asm/hardware.h>

static spinlock_t pxa_nssp_lock = SPIN_LOCK_UNLOCKED;
static unsigned long flag;
static unsigned char initialized = 0;

void pxa_nssp_output(unsigned char reg, unsigned char data)
{
  int i;
  unsigned char dat = ( ((reg << 5) & 0xe0) | (data & 0x1f) );

  spin_lock_irqsave(&pxa_nssp_lock, flag);

  GPCR(GPIO_TG_SPI_SCLK) |= GPIO_bit(GPIO_TG_SPI_SCLK);
  GPCR(GPIO_TG_SPI_CS) |= GPIO_bit(GPIO_TG_SPI_CS);
  for(i = 0; i < 8; i++) {
    if( !(dat & (1<<(7-i))) )
      GPCR(GPIO_TG_SPI_MOSI) |= GPIO_bit(GPIO_TG_SPI_MOSI);
    else
      GPSR(GPIO_TG_SPI_MOSI) |= GPIO_bit(GPIO_TG_SPI_MOSI);
    GPSR(GPIO_TG_SPI_SCLK) |= GPIO_bit(GPIO_TG_SPI_SCLK);
    GPCR(GPIO_TG_SPI_SCLK) |= GPIO_bit(GPIO_TG_SPI_SCLK);
  }
  GPSR(GPIO_TG_SPI_CS) |= GPIO_bit(GPIO_TG_SPI_CS);
  spin_unlock_irqrestore(&pxa_nssp_lock, flag);
}

void pxa_nssp_init(void)
{
  /* initialize SSP */
  set_GPIO_mode(GPIO_TG_SPI_SCLK | GPIO_OUT);
  set_GPIO_mode(GPIO_TG_SPI_CS | GPIO_OUT);
  set_GPIO_mode(GPIO_TG_SPI_MOSI | GPIO_OUT);
  GPSR(GPIO_TG_SPI_CS) |= GPIO_bit(GPIO_TG_SPI_CS);
  GPCR(GPIO_TG_SPI_SCLK) |= GPIO_bit(GPIO_TG_SPI_SCLK);
}
