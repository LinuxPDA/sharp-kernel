/*
 *  RTC interface for Linux on MQ9000 (ARM922T core)
 *
 *  Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _MQ9000_RTC_H_
#define _MQ9000_RTC_H_

#include <linux/rtc.h>
#include <asm/arch/MQ9000Regs.h>

static const unsigned char days_in_mo[] =
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define is_leap(year) \
	((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

static unsigned long dayofyear(int year, int month, int day)
{
	int m, d = 0;
	for (m=0; m<month; m++)
		d += days_in_mo[m];
	if (m > 1)
		d += is_leap(year + 1900);
	d += day;
	return (unsigned long)d;
}

static void decodetime(unsigned long t1, unsigned long t2,
			struct rtc_time *tval)
{
	tval->tm_hour = (t1 & 0xff0000) >> 16;
	tval->tm_min = (t1 & 0xff00) >> 8;
	tval->tm_sec = t1 & 0xff;

	tval->tm_year = ((t2 & 0xff000000) >> 24) + 70;
	tval->tm_mon = ((t2 & 0xff0000) >> 16) - 1;;
	tval->tm_mday = (t2 & 0xff00) >> 8;
	tval->tm_wday = (t2 & 0xff) - 1;

	tval->tm_yday = dayofyear(tval->tm_year, tval->tm_mon, tval->tm_mday);
}

/*
 * Converts seconds since 1970-01-01 00:00:00 to Gregorian date.
 */
static unsigned long calendar2sec(struct rtc_time *ptm)
{
	int d;
	unsigned long sec;

	d = (ptm->tm_year - 70) * 365
			+ LEAPS_THRU_END_OF(ptm->tm_year - 1 + 1900)
			- LEAPS_THRU_END_OF(1970 - 1);
	d += dayofyear(ptm->tm_year, ptm->tm_mon, ptm->tm_mday) - 1;
	sec = (d * 86400) + (ptm->tm_hour * 3600) +
				(ptm->tm_min * 60) + ptm->tm_sec;

	return sec;
}

#endif // _MQ9000_RTC_H_
