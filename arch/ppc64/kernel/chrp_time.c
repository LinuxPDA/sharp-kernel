/*
 *  linux/arch/i386/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *
 * Adapted for PowerPC (PreP) by Gary Thomas
 * Modified by Cort Dougan (cort@cs.nmt.edu)
 *  copied and modified from intel version
 * Modified for PPC64 by PPC64 Team, Copyright IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/kernel_stat.h>
#include <linux/mc146818rtc.h>
#include <linux/init.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/nvram.h>
#include <asm/prom.h>
#include <asm/init.h>
#include <asm/time.h>
/*#include "time.h" */

static int nvram_as1 = NVRAM_AS1;
static int nvram_as0 = NVRAM_AS0;
static int nvram_data = NVRAM_DATA;

long __init chrp_time_init(void)
{
	struct device_node *rtcs;
	int base;

	rtcs = find_compatible_devices("rtc", "pnpPNP,b00");
	if (rtcs == NULL || rtcs->addrs == NULL) {
		return 0;
	}
	base = rtcs->addrs[0].address;
	nvram_as1 = 0;
	nvram_as0 = base;
	nvram_data = base + 1;

        return 0;
}

int __chrp chrp_cmos_clock_read(int addr)
{
	int retval;

#if 1   /* DRENG -- see chrp_time_init, chrp_cmos_clock_write
         * I think this is broken in general on 64b --
         * nvram_as0 = -1 in 64b format
  	 */
	if (nvram_as1 != 0) {
		outb(addr>>8, nvram_as1);
        }
	outb(addr, nvram_as0);
	retval = inb(nvram_data);
	return retval;
#else
	return(-1);
#endif
}

void __chrp chrp_cmos_clock_write(unsigned long val, int addr)
{
#if 1 /* DRENG -- see chrp_cmos_clock_read */
	if (nvram_as1 != 0)
		outb(addr>>8, nvram_as1);
	outb(addr, nvram_as0);
	outb(val, nvram_data);
	return;
#else
  	return;
#endif
}

/*
 * Set the hardware clock. -- Cort
 */
int __chrp chrp_set_rtc_time(unsigned long nowtime)
{
	unsigned char save_control, save_freq_select;
	struct rtc_time tm;

	to_tm(nowtime, &tm);

	save_control = chrp_cmos_clock_read(RTC_CONTROL); /* tell the clock it's being set */

	chrp_cmos_clock_write((save_control|RTC_SET), RTC_CONTROL);

	save_freq_select = chrp_cmos_clock_read(RTC_FREQ_SELECT); /* stop and reset prescaler */
	
	chrp_cmos_clock_write((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

        tm.tm_year -= 1900;
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BIN_TO_BCD(tm.tm_sec);
		BIN_TO_BCD(tm.tm_min);
		BIN_TO_BCD(tm.tm_hour);
		BIN_TO_BCD(tm.tm_mon);
		BIN_TO_BCD(tm.tm_mday);
		BIN_TO_BCD(tm.tm_year);
	}
	chrp_cmos_clock_write(tm.tm_sec,RTC_SECONDS);
	chrp_cmos_clock_write(tm.tm_min,RTC_MINUTES);
	chrp_cmos_clock_write(tm.tm_hour,RTC_HOURS);
	chrp_cmos_clock_write(tm.tm_mon,RTC_MONTH);
	chrp_cmos_clock_write(tm.tm_mday,RTC_DAY_OF_MONTH);
	chrp_cmos_clock_write(tm.tm_year,RTC_YEAR);
	
	/* The following flags have to be released exactly in this order,
	 * otherwise the DS12887 (popular MC146818A clone with integrated
	 * battery and quartz) will not reset the oscillator and will not
	 * update precisely 500 ms later. You won't find this mentioned in
	 * the Dallas Semiconductor data sheets, but who believes data
	 * sheets anyway ...                           -- Markus Kuhn
	 */
	chrp_cmos_clock_write(save_control, RTC_CONTROL);
	chrp_cmos_clock_write(save_freq_select, RTC_FREQ_SELECT);

	if ( (time_state == TIME_ERROR) || (time_state == TIME_BAD) )
		time_state = TIME_OK;
	return 0;
}

unsigned long __chrp chrp_get_rtc_time(void)
{
	unsigned int year, mon, day, hour, min, sec;
	int i;

	/* The Linux interpretation of the CMOS clock register contents:
	 * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
	 * RTC registers show the second which has precisely just started.
	 * Let's hope other operating systems interpret the RTC the same way.
	 */
	/* read RTC exactly on falling edge of update flag */
	for (i = 0 ; i < 1000000 ; i++)	/* may take up to 1 second... */
		if (chrp_cmos_clock_read(RTC_FREQ_SELECT) & RTC_UIP)
			break;
	for (i = 0 ; i < 1000000 ; i++)	/* must try at least 2.228 ms */
		if (!(chrp_cmos_clock_read(RTC_FREQ_SELECT) & RTC_UIP))
			break;
	do { /* Isn't this overkill ? UIP above should guarantee consistency */
		sec = chrp_cmos_clock_read(RTC_SECONDS);
		min = chrp_cmos_clock_read(RTC_MINUTES);
		hour = chrp_cmos_clock_read(RTC_HOURS);
		day = chrp_cmos_clock_read(RTC_DAY_OF_MONTH);
		mon = chrp_cmos_clock_read(RTC_MONTH);
		year = chrp_cmos_clock_read(RTC_YEAR);
	} while (sec != chrp_cmos_clock_read(RTC_SECONDS));
	if (!(chrp_cmos_clock_read(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
	  {
	    BCD_TO_BIN(sec);
	    BCD_TO_BIN(min);
	    BCD_TO_BIN(hour);
	    BCD_TO_BIN(day);
	    BCD_TO_BIN(mon);
	    BCD_TO_BIN(year);
	  }
	if ((year += 1900) < 1970)
		year += 100;
	return mktime(year, mon, day, hour, min, sec);
}

extern void setup_default_decr(void);

void __init chrp_calibrate_decr(void)
{
	struct device_node *cpu;
	int *fp;
	unsigned long freq;

	/*
	 * The cpu node should have a timebase-frequency property
	 * to tell us the rate at which the decrementer counts.
	 */
	freq = 16666000;		/* hardcoded default */
	cpu = find_type_devices("cpu");
	if (cpu != 0) {
		fp = (int *) get_property(cpu, "timebase-frequency", NULL);
		if (fp != 0)
			freq = *fp;
	}
        printk("time_init: decrementer frequency = %lu.%.6lu MHz\n",
		freq/1000000, freq%1000000 );

	tb_ticks_per_jiffy = freq / HZ;
	tb_ticks_per_usec = freq / 1000000;
	setup_default_decr();
}
