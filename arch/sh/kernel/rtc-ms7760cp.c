/*
 * arch arch/sh/kernel/rtc-ms7760cp.c
 *
 * Based on arch/sh/kernel/rtc-7760se.c
 * Copyright (c) 2003	Takeo Takahashi
 *
 * SH7760 Solution Engine RTC routines.
 *
 * Some code taken from rtc.c:
 *  Copyright (C) 2000  Philipp Rumpf <prumpf@tux.org>
 *  Copyright (C) 1999  Tetsuya Okada & Niibe Yutaka
 *
 */

#include <linux/time.h>
#include <asm/io.h>
#include <asm/ms7760cp.h>

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

void ms7760cp_rtc_init(void)
{
	unsigned char ch;

	printk("RTC: Resetting to 1 Jan 2000.\n");
	ch = 0x01;	/* RTC stop */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);
	ch = 0;
	h8_out(&ch, 1, SH7760SE_RTC_SECCNT);
	h8_out(&ch, 1, SH7760SE_RTC_MINCNT);
	h8_out(&ch, 1, SH7760SE_RTC_HRCNT);
	ch = 0;
	h8_out(&ch, 1, SH7760SE_RTC_WKCNT);
	ch = 1;
	h8_out(&ch, 1, SH7760SE_RTC_DAYCNT);
	h8_out(&ch, 1, SH7760SE_RTC_MONCNT);
	ch = 0;
	h8_out(&ch, 1, SH7760SE_RTC_YRCNT);
	ch = 0x21;	/* update */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);
	ch = 0;
	h8_out(&ch, 1, SH7760SE_RTC_SECAR);
	h8_out(&ch, 1, SH7760SE_RTC_MINAR);
	h8_out(&ch, 1, SH7760SE_RTC_HRAR);
	h8_out(&ch, 1, SH7760SE_RTC_WKAR);
	h8_out(&ch, 1, SH7760SE_RTC_DAYAR);
	h8_out(&ch, 1, SH7760SE_RTC_MONAR);
	ch = 0x00;	/* clear status register */
	h8_out(&ch, 1, SH7760SE_RTC_RTCSR);
	ch = 0x00;	/* start */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);

#if 0	// check
	{
	unsigned int prev_second, now_second;
	int i;
	printk("RTC: checking second ");
	h8_in(&ch, 1, SH7760SE_RTC_SECCNT);
	prev_second = (unsigned int)ch; 
	BCD_TO_BIN(prev_second);
	for (i=0; i<3; i++) {
		h8_in(&ch, 1, SH7760SE_RTC_SECCNT);
		now_second = (unsigned int)ch; 
		BCD_TO_BIN(now_second);
		if (now_second > prev_second) {
			printk("-> %d", now_second);
			prev_second = now_second;
		}
	}
	printk(" -> ok.\n");
	}
#endif
}

void ms7760cp_rtc_gettimeofday(struct timeval *tv)
{
	unsigned int sec, min, hr, wk, day, mon, yr;
	unsigned char ch;

again:
	h8_in(&ch, 1, SH7760SE_RTC_SECCNT);
	sec = (unsigned int)ch; 
	h8_in(&ch, 1, SH7760SE_RTC_MINCNT);
	min = (unsigned int)ch; 
	h8_in(&ch, 1, SH7760SE_RTC_HRCNT);
	hr = (unsigned int)ch; 
	h8_in(&ch, 1, SH7760SE_RTC_WKCNT);
	wk = (unsigned int)ch; 
	h8_in(&ch, 1, SH7760SE_RTC_DAYCNT);
	day = (unsigned int)ch; 
	h8_in(&ch, 1, SH7760SE_RTC_MONCNT);
	mon = (unsigned int)ch; 
	h8_in(&ch, 1, SH7760SE_RTC_YRCNT);
	yr = (unsigned int)ch; 

	BCD_TO_BIN(yr);
	BCD_TO_BIN(mon);
	BCD_TO_BIN(day);
	BCD_TO_BIN(hr);
	BCD_TO_BIN(min);
	BCD_TO_BIN(sec);

	if (yr > 99 || mon < 1 || mon > 12 || day > 31 || day < 1 ||
	    hr > 23 || min > 59 || sec > 59) {
		printk(KERN_ERR "RTC: invalid value.\n");
		ms7760cp_rtc_init();
		goto again;
	}
	tv->tv_sec = mktime(0x20 * 100 + yr, mon, day, hr, min, sec);
	tv->tv_usec = 0;
}

int ms7760cp_rtc_settimeofday(const struct timeval *tv)
{
	unsigned long nowtime = tv->tv_sec;
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned char ch;

	ch = 0x01;	/* RTC stop */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);

	h8_in(&ch, 1, SH7760SE_RTC_MINCNT);
	cmos_minutes = ch;
	BCD_TO_BIN(cmos_minutes);

	/*
	 * since we're only adjusting minutes and seconds,
	 * don't interfere with hour overflow. This avoids
	 * messing with unknown time zones but requires your
	 * RTC not to be off by more than 15 minutes
	 */
	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15)/30) & 1)
		real_minutes += 30;	/* correct for half hour time zone */
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		BIN_TO_BCD(real_seconds);
		BIN_TO_BCD(real_minutes);
		ch = (unsigned char)real_seconds;
		h8_out(&ch, 1, SH7760SE_RTC_SECCNT);
		ch = (unsigned char)real_minutes;
		h8_out(&ch, 1, SH7760SE_RTC_MINCNT);
	} else {
		printk(KERN_WARNING
		       "set_rtc_time: can't update from %d to %d\n",
		       cmos_minutes, real_minutes);
		retval = -1;
	}
	ch = 0x21;	/* update */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);
	ch = 0x00;	/* start */
	h8_out(&ch, 1, SH7760SE_RTC_RTCCR);

	return retval;
}
