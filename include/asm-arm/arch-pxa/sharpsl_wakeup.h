/*
 * include/asm-arm/arch-pxa/sharpsl_wakeup.h
 * 	Copyright (C) 2003 SHARP
 */

#ifndef __SHARPSL_WAKEUP_H__
#define __SHARPSL_WAKEUP_H__

#define IDPM_WAKEUP_ONKEY	(0x1<<0)
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

// extern unsigned long logical_wakeup_src_mask;

// ========================================

#define VIO_POWERON	(0x1<<0)	// ON-KEY & AC-IN & KEY-IN
#define VIO_GPIO_RESET	(0x1<<1)
#define VIO_SDDETECT	(0x1<<9)
#define VIO_REMOCON	(0x1<<13)
#define VIO_AC		(0x1<<15)
#define VIO_SYNC	(0x1<<16)
#define VIO_KEYSNS0	(0x1<<17)
#define VIO_KEYSNS1	(0x1<<18)
#define VIO_KEYSNS2	(0x1<<19)
#define VIO_KEYSNS3	(0x1<<20)
#define VIO_KEYSNS4	(0x1<<21)
#define VIO_KEYSNS5	(0x1<<22)
#define VIO_KEYSNS6	(0x1<<23)
#define VIO_CF0		(0x1<<24)
#define VIO_CF1		(0x1<<25)
#define VIO_USBD	(0x1<<26)
#define VIO_LOCKSW	(0x1<<27)
#define VIO_JACKIN	(0x1<<28)
#define VIO_FULLCHARGED	(0x1<<29)
#define VIO_RTC		(0x1<<31)

void set_logical_wakeup_src_mask(u32 SetValue);
u32 get_logical_wakeup_src_mask(void);
u32 get_logical_wakeup_factor(void);

void wakeup_ready_to_suspend(void);
void check_wakeup_virtual(void);
void check_wakeup_logical(void);

void set_wakeup_src_mask( u32 wakeupsrc ); 
u32 get_virtual_wakeup_src_mask(void);
u32 get_virtual_wakeup_factor(void);


#endif // end __SHARPSL_WAKEUP_H__
