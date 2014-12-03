/*
 * include/asm-arm/arch-pxa/sharpsl_wakeup.h
 * 	Copyright (C) 2003 SHARP
 */

#ifndef __SHARPSL_WAKEUP_H__
#define __SHARPSL_WAKEUP_H__


#define IDPM_WAKEUP_AC		(0x1<<1)
#define IDPM_WAKEUP_SYNC	(0x1<<3)
#define IDPM_WAKEUP_REMOCON	(0x1<<4)
#define IDPM_WAKEUP_REC		(0x1<<10)
#define IDPM_WAKEUP_JACKET	(0x1<<16)
#define IDPM_WAKEUP_USBD	(0x1<<17)
#define IDPM_WAKEUP_CALENDAR	(0x1<<25)
#define IDPM_WAKEUP_ADDRESSBOOK	(0x1<<26)
#define IDPM_WAKEUP_MAIL	(0x1<<27)
#define IDPM_WAKEUP_MENU	(0x1<<28)
#define IDPM_WAKEUP_HOME	(0x1<<29)
#define IDPM_WAKEUP_RTC		(0x1<<31)

extern unsigned long logical_wakeup_src_mask;


#endif // end __SHARPSL_WAKEUP_H__
