/*
 *  linux/arch/arm/mach-l7200/sysid.c
 *
 *  L72xx SysID routine
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
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

/*
 *  l72xx_read_sysid : returns sysid value
 *
 *  output ex. L7205 ---> 0x7205
 *             L7210 ---> 0x7210
 */
unsigned int l72xx_read_sysid(void)
{
  return 0x0ffff & ( IO_SYS_ID >> 12 );
}

/*
 *  l72xx_read_sysid : returns revision value
 *
 *  output ex. L7205 revBB ---> 0x021
 *             L7210 revAB ---> 0x001
 */

unsigned int l72xx_read_revision(void)
{
  return 0x0fff & IO_SYS_ID;
}

/*
 *  l72xx_read_pll : returns pll clock value
 *
 *  output ex. 3.6864MHz --> 3
 *            18.432MHz  --> 18
 *            147.456MHz --> 147
 *            other ?    --> 0
 */

#define PLLMUX_3M6  0x00
#define PLLMUX_18M  0x05
#define PLLMUX_36M  0x0a
#define PLLMUX_73M  0x14
#define PLLMUX_129M 0x23
#define PLLMUX_140M 0x26
#define PLLMUX_147M 0x28

unsigned int l72xx_read_pll(void)
{
  unsigned long val;
  val = ( IO_SYS_CONFIG_CURRENT >> 9 ) & 0x3f;
  switch(val){
  case PLLMUX_3M6:
  case PLLMUX_18M:
  case PLLMUX_36M:
  case PLLMUX_73M:
  case PLLMUX_129M:
  case PLLMUX_140M:
  case PLLMUX_147M:
    return(val);
    break;
  default:
    printk("Unknown PLLMUX Value (1)\n");
    return(0);
    break;
  }
  printk("Unknown PLLMUX Value (2)\n");
  return(0);
}


/*
 *  end of source
 */
