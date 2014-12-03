/*
 *  linux/arch/arm/mach-katana/time.c
 *
 *  Copyright (c) 2002 by MediaQ, Incorporated.
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
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>

#include <asm/arch/platform.h>	//MQ stuff
#include <asm/arch/uncompress.h>


extern int (*set_rtc)(void);

static int katana_set_rtc(void)
{
	MQ9000REGS	*pMQReg = (MQ9000REGS *)MQ9000_REGS_VBASE;
	unsigned long	year, month, day, hour, minute, second;

	second = xtime.tv_sec % 60;
        minute = xtime.tv_sec / 60;
	hour = minute / 60;
	minute = minute % 60;
	day = hour / 24;
        hour = hour % 24;
	month = day / 30;
	day = day & 30;
	year = month / 12;
	month = month % 12;

	pMQReg->rt.rt04.Control &= ~MQ_RTC_RTM_ENABLE;
	pMQReg->rt.rt00.TimeOfDay = (hour << 16) | (minute << 8) | second;
	pMQReg->rt.rt01.Date = (year << 24) | (month << 16)
				| (day << 8) | (day / 7);
	pMQReg->rt.rt04.Control |= MQ_RTC_RTM_ENABLE;

	return 1;
}

static int katana_rtc_init(void)
{
	//Use TimeOfDay Timer for 1Hz clk.
	//_rt registers block are initialized in setup_timer() of time.h.
	//Here, we have to set/clear individual bits.
	//*** Scratch the above comment.  It looks like this routine
	MQ9000REGS	*pMQReg = (MQ9000REGS *)MQ9000_REGS_VBASE;

	//disable periodic rtc first and clr interrupt status
	pMQReg->rt.rt00.TimeOfDay = 0;
	pMQReg->rt.rt01.Date = 0;
	pMQReg->rt.rt04.Control |= MQ_RTC_RTM_ENABLE | MQ_RTC_L1_RATIO
			| MQ_RTC_HEX_FORMAT;

	//read it back.  Should be zero.
	xtime.tv_sec = 0;

	set_rtc = katana_set_rtc;

	return 0;
}

__initcall(katana_rtc_init);
