/*
 * arch/arm/mach-pxa/pxa27x_wakeup.c
 * 	Copyright (C) 2004 SHARP
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/acpi.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/proc-fns.h>

#include <asm/arch/irqs.h>
#include <asm/hardirq.h>

#include <asm/sharp_char.h>
#include <asm/arch/keyboard.h>
#include <asm/arch/sharpsl_wakeup.h>
#include <asm/arch/corgi.h>

#define BIT(x) (0x1<<(x))

extern int corgi_wakeup_remocon_hook(void);
extern u32 sharppda_kbd_is_wakeup(void);

#define LOGICAL_WAKEUP_ALWAYS	(IDPM_WAKEUP_ONKEY)
#define LOGICAL_WAKEUP_DEFAULT	(LOGICAL_WAKEUP_ALWAYS | IDPM_WAKEUP_REMOCON | IDPM_WAKEUP_SYNC | IDPM_WAKEUP_HOME | IDPM_WAKEUP_MAIL | IDPM_WAKEUP_ADDRESSBOOK | IDPM_WAKEUP_CALENDAR)
#define VIRTUAL_WAKEUP_ALWAYS	(VIO_POWERON | VIO_GPIO_RESET | VIO_AC | /*VIO_FULLCHARGED |*/ VIO_JACKIN | VIO_RTC | VIO_SYNC | VIO_LOCKSW)
#define VIRTUAL_WAKEUP_DEFAULT  (VIRTUAL_WAKEUP_ALWAYS | VIO_REMOCON)

static u32 logical_wakeup_src_mask = LOGICAL_WAKEUP_DEFAULT;
static u32 logical_wakeup_factor = 0;
static u32 virtual_wakeup_src_mask = VIRTUAL_WAKEUP_DEFAULT;
static u32 virtual_wakeup_factor = 0;

static int last_ac_state = 0;
static int last_jack_state = 0;

void set_logical_wakeup_src_mask(u32 SetValue)
{
        unsigned long apm_wakeup_src_mask = get_virtual_wakeup_src_mask();
	logical_wakeup_src_mask = LOGICAL_WAKEUP_ALWAYS;
	// RTC
	if (SetValue&IDPM_WAKEUP_RTC) {
	  apm_wakeup_src_mask |= VIO_RTC;
	  logical_wakeup_src_mask |= IDPM_WAKEUP_RTC;
	} else apm_wakeup_src_mask &= ~VIO_RTC;
#if 0
	// Record key
	if (SetValue&IDPM_WAKEUP_REC) {
	  apm_wakeup_src_mask |= GPIO_bit(GPIO_RECORD_BTN);
	  logical_wakeup_src_mask |= IDPM_WAKEUP_REC; 
	} else apm_wakeup_src_mask &= ~GPIO_bit(GPIO_RECORD_BTN);
#endif
	// USBD
	if (SetValue&IDPM_WAKEUP_USBD) {
	  apm_wakeup_src_mask |= VIO_USBD;
	  logical_wakeup_src_mask |= IDPM_WAKEUP_USBD;
	} else apm_wakeup_src_mask &= ~VIO_USBD;
	// REMOCON
	if (SetValue&IDPM_WAKEUP_REMOCON) {
	  apm_wakeup_src_mask |= VIO_REMOCON;
	  logical_wakeup_src_mask |= IDPM_WAKEUP_REMOCON;
	} else apm_wakeup_src_mask &= ~VIO_REMOCON;
	// SYNC
	if (SetValue&IDPM_WAKEUP_SYNC) {
	  apm_wakeup_src_mask |= VIO_SYNC;
	  logical_wakeup_src_mask |= IDPM_WAKEUP_SYNC;
	} else apm_wakeup_src_mask &= ~VIO_SYNC;

	set_virtual_wakeup_src_mask(apm_wakeup_src_mask);

	// only Logical
	if (SetValue&IDPM_WAKEUP_AC)		logical_wakeup_src_mask |= IDPM_WAKEUP_AC;
	if (SetValue&IDPM_WAKEUP_CALENDAR)	logical_wakeup_src_mask |= IDPM_WAKEUP_CALENDAR;
	if (SetValue&IDPM_WAKEUP_ADDRESSBOOK)	logical_wakeup_src_mask |= IDPM_WAKEUP_ADDRESSBOOK;
	if (SetValue&IDPM_WAKEUP_MAIL)		logical_wakeup_src_mask |= IDPM_WAKEUP_MAIL;
	if (SetValue&IDPM_WAKEUP_HOME)		logical_wakeup_src_mask |= IDPM_WAKEUP_HOME;

	printk("set_wakeup_src=%08x->%08x[%08x]\n",(u32)SetValue,logical_wakeup_src_mask,virtual_wakeup_src_mask);
}

u32 get_logical_wakeup_src_mask(void)
{
  return logical_wakeup_src_mask;
}

u32 get_logical_wakeup_factor(void)
{
  return logical_wakeup_factor;
}

void wakeup_ready_to_suspend()
{
  u32 wakeup_rise = 0;
  u32 wakeup_fall = 0;
  u32 wakeup2 = 0;

  if (virtual_wakeup_src_mask & VIO_POWERON) {
    wakeup_rise |= BIT(0);
    wakeup_fall |= BIT(0);
    wakeup2 |= BIT(12);
  }
  if (virtual_wakeup_src_mask & VIO_GPIO_RESET) {
    //wakeup_rise |= BIT(1);
    wakeup_fall |= BIT(1);
  }
  if (virtual_wakeup_src_mask & VIO_SDDETECT) {
    wakeup_rise |= BIT(9);
  }
  if (virtual_wakeup_src_mask & VIO_REMOCON) {
    wakeup_rise |= BIT(13);
  }
  if (virtual_wakeup_src_mask & VIO_SYNC) {
    wakeup2 |= BIT(1);
  }
  if (virtual_wakeup_src_mask & VIO_KEYSNS0) {
    wakeup_rise |= BIT(12);
  }
  if (virtual_wakeup_src_mask & VIO_KEYSNS1) {
    wakeup2 |= BIT(2);
  }
  if (virtual_wakeup_src_mask & VIO_KEYSNS2) {
    wakeup2 |= BIT(9);
  }
  if (virtual_wakeup_src_mask & VIO_KEYSNS3) {
    wakeup2 |= BIT(3);
  }
  if (virtual_wakeup_src_mask & VIO_KEYSNS4) {
    wakeup2 |= BIT(4);
  }
  if (virtual_wakeup_src_mask & VIO_KEYSNS5) {
    wakeup2 |= BIT(6);
  }
  if (virtual_wakeup_src_mask & VIO_KEYSNS6) {
    wakeup2 |= BIT(7);
  }
  if (virtual_wakeup_src_mask & VIO_CF0) {
    wakeup2 |= BIT(11);
  }
  if (virtual_wakeup_src_mask & VIO_CF1) {
    wakeup2 |= BIT(10);
  }
  if (virtual_wakeup_src_mask & VIO_USBD) {
    wakeup_rise |= BIT(24);
  }
  if (virtual_wakeup_src_mask & VIO_LOCKSW) {
    wakeup_rise |= BIT(15);
    wakeup_fall |= BIT(15);
  }
  if (virtual_wakeup_src_mask & VIO_JACKIN) {
    wakeup_rise |= BIT(23);
    wakeup_fall |= BIT(23);
  }
  if (virtual_wakeup_src_mask & VIO_FULLCHARGED) {
    wakeup2 |= BIT(18);
  }
  if (virtual_wakeup_src_mask & VIO_RTC) {
    wakeup_rise |= BIT(31);
  }

  // global
  PRER = wakeup_rise;
  PFER = wakeup_fall;
  PWER = wakeup_rise | wakeup_fall;
  PEDR = 0xffffffff; // clear
  // keypad
  PKWR = wakeup2;
  PKSR = 0xffffffff; // clear

  CKEN |= CKEN19_KEY;

  logical_wakeup_factor = 0;
  virtual_wakeup_factor = 0;

  // store last state
  last_ac_state = (AC_IN_STATUS)?1:0;
  last_jack_state = (GPLR(GPIO_HP_IN)&GPIO_bit(GPIO_HP_IN))?1:0;

//  printk("suspend logical_wakeup=%08x\n",logical_wakeup_src_mask);
//  printk("suspend virtual_wakeup=%08x\n",virtual_wakeup_src_mask);
//  printk("suspend PWER=%08x,PKWR=%08x\n",PWER,PKWR);
}

void check_wakeup_virtual()
{
  u32 retval=0;
  u32 pedr = PEDR; // PWER
  u32 pksr = PKSR; // PKWR
  u32 cur_ac_state = (AC_IN_STATUS)?1:0;
  u32 cur_jack_state = (GPLR(GPIO_HP_IN)&GPIO_bit(GPIO_HP_IN))?1:0;
  printk("jack=%d\n",cur_jack_state);

  CKEN &= ~CKEN19_KEY;

  // virtual wakeup factor
  if (pedr & BIT(0)) { // PowerOn
    retval |= VIO_POWERON;
    if ( last_ac_state != cur_ac_state ) {
      retval |= VIO_AC;
      last_ac_state = cur_ac_state;
      printk("AC state changed (%d)\n",cur_ac_state);
    }
  }
  if (pedr & BIT(1)) { // GPIO Reset
    retval |= VIO_GPIO_RESET;
  }
  if (pedr & BIT(2)) { // SDDETECT
    retval |= VIO_SDDETECT;
  }
  if (pedr & BIT(13)) { // REMOCON
    retval |= VIO_REMOCON;
  }
  if (pksr & BIT(1)) { // SYNC
    retval |= VIO_SYNC;
  }
  if (pedr & BIT(12)) { // KEYSNS0
    retval |= VIO_KEYSNS0;
  }
  if (pksr & BIT(2)) { // KEYSNS1
    retval |= VIO_KEYSNS1;
  }
  if (pksr & BIT(9)) { // KEYSNS2
    retval |= VIO_KEYSNS2;
  }
  if (pksr & BIT(3)) { // KEYSNS3
    retval |= VIO_KEYSNS3;
  }
  if (pksr & BIT(4)) { // KEYSNS4
    retval |= VIO_KEYSNS4;
  }
  if (pksr & BIT(6)) { // KEYSNS5
    retval |= VIO_KEYSNS5;
  }
  if (pksr & BIT(7)) { // KEYSNS6
    retval |= VIO_KEYSNS6;
  }
  if (pksr & BIT(11)) { // CF0
    retval |= VIO_CF0;
  }
  if (pksr & BIT(10)) { // CF1
    retval |= VIO_CF1;
  }
  if (pedr & BIT(24)) { // USBD
    retval |= VIO_USBD;
  }
  if (pedr & BIT(15)) { // LOCKSW
    retval |= VIO_LOCKSW;
  }
  if (pedr & BIT(23)) { // JACKIN
    if ( last_jack_state != cur_jack_state ) {
      retval |= VIO_JACKIN;
      last_jack_state = cur_jack_state;
      printk("Jack state changed (%d)\n",cur_jack_state);
    }
  }
  if (pksr & BIT(18)) { // FULLCHARGED
    retval |= VIO_FULLCHARGED;
  }
  //X  if (pedr & BIT(31)) { // RTC
  if ( ( RTSR & 0x1 ) && ( RTSR & RTSR_ALE ) ) { // RTC
    retval |= VIO_RTC;
  }
  virtual_wakeup_factor = retval & virtual_wakeup_src_mask;

  printk("resume virtual_wakeup_factor=%08x (src=%08x)\n",virtual_wakeup_factor,virtual_wakeup_src_mask);
}

void check_wakeup_logical()
{
  int retval = 0;

  // logical wakeup factor
  if ( virtual_wakeup_factor & 
       (VIO_POWERON | VIO_KEYSNS0 | VIO_KEYSNS1 | VIO_KEYSNS2 | VIO_KEYSNS3 |
	VIO_KEYSNS4 | VIO_KEYSNS5 | VIO_KEYSNS6 | VIO_SYNC) ) {
    u32 keyval = sharppda_kbd_is_wakeup();
//    printk("wakeup key state=%08x\n",keyval);
    retval |= keyval;
  }
  if ( virtual_wakeup_factor & VIO_AC ) {
    retval |= IDPM_WAKEUP_AC;
  }
  if ( virtual_wakeup_factor & VIO_REMOCON ) {
    int remoconstate = corgi_wakeup_remocon_hook();
    printk("remocon state = %04x, jack=%04x\n",remoconstate,last_jack_state);
    if (last_jack_state!=0 && remoconstate!=0) {
      retval |= IDPM_WAKEUP_REMOCON;
    }
  }
  if ( virtual_wakeup_factor & VIO_USBD ) {
    retval |= IDPM_WAKEUP_USBD;
  }

  if ( virtual_wakeup_factor & VIO_RTC ) {
    retval |= IDPM_WAKEUP_RTC;
  }

  logical_wakeup_factor = retval & logical_wakeup_src_mask;

  printk("resume logical_wakeup_factor=%08x (src=%08x)\n",logical_wakeup_factor,logical_wakeup_src_mask);
}

void set_virtual_wakeup_src_mask( u32 wakeupsrc )
{
  virtual_wakeup_src_mask = wakeupsrc | VIRTUAL_WAKEUP_ALWAYS;
}

u32 get_virtual_wakeup_src_mask()
{
  return virtual_wakeup_src_mask;
}

u32 get_virtual_wakeup_factor()
{
  return virtual_wakeup_factor;
}

EXPORT_SYMBOL(set_logical_wakeup_src_mask);
EXPORT_SYMBOL(get_logical_wakeup_src_mask);
EXPORT_SYMBOL(get_logical_wakeup_factor);

EXPORT_SYMBOL(wakeup_ready_to_suspend);
EXPORT_SYMBOL(check_wakeup_virtual);
EXPORT_SYMBOL(check_wakeup_logical);

EXPORT_SYMBOL(set_virtual_wakeup_src_mask);
EXPORT_SYMBOL(get_virtual_wakeup_src_mask);
EXPORT_SYMBOL(get_virtual_wakeup_factor);

/* EOF */
