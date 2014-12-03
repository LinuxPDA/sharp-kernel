/*
 *  OS Timer interface for Linux on MQ9000 (ARM922T core)
 *
 *  Copyright (c) 2003 MediaQ, Inc.
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
 *  Changelog:
 *	14-02-2003: LINEO Japan, Inc.
 *
 */

#include <linux/rtc.h>
#include <asm/arch/mq9000_rtc.h>

#define USE_PTM_SLOW_OSC
//#define	TIMER_INTERVAL	327
#define	TIMER_INTERVAL	LATCH
#define TICKS2USECS(x)	(((x) * 10000) / TIMER_INTERVAL)


static int rtdayofweek(struct rtc_time *tm)
{
	int d = (tm->tm_year - 70) * 365
			+ LEAPS_THRU_END_OF(tm->tm_year - 1 + 1900)
			- LEAPS_THRU_END_OF(1970 - 1);
	d += dayofyear(tm->tm_year, tm->tm_mon, tm->tm_mday) - 1;
	return ((4 + d) % 7);
}

static unsigned long rttimeofday(struct rtc_time *tm)
{
	int rt;
	rt = (tm->tm_hour << 16) & 0xff0000;
	rt |= (tm->tm_min << 8) & 0xff00;
	rt |= tm->tm_sec & 0xff;
	return rt;
}

static unsigned long rtdate(struct rtc_time *tm)
{
	int rt;
	rt = ((tm->tm_year - 70) << 24) & 0xff000000;
	rt |= ((tm->tm_mon + 1) << 16) & 0xff0000;
	rt |= (tm->tm_mday << 8) & 0xff00;
	rt |= (rtdayofweek(tm) + 1) & 0xff;
	return rt;
}

static void sec2calendar(unsigned long t, struct rtc_time *ptm)
{
	long days, month, year, rem;

	days = t / 86400;
	rem = t % 86400;
	ptm->tm_hour = rem / 3600;
	rem %= 3600;
	ptm->tm_min = rem / 60;
	ptm->tm_sec = rem % 60;
	ptm->tm_wday = (4 + days) % 7;

	year = 1970 + days / 365;
	days -= ((year - 1970) * 365
			+ LEAPS_THRU_END_OF (year - 1)
			- LEAPS_THRU_END_OF (1970 - 1));
	if (days < 0) {
		year -= 1;
		days += 365 + is_leap(year);
	}
	ptm->tm_year = year - 1900;
	ptm->tm_yday = days + 1;

	month = 0;
	if (days >= 31) {
		days -= 31;
		month++;
		if (days >= (28 + is_leap(year))) {
			days -= (28 + is_leap(year));
			month++;
			while (days >= days_in_mo[month]) {
				days -= days_in_mo[month];
				month++;
			}
		}
	}
	ptm->tm_mon = month;
	ptm->tm_mday = days + 1;
}

static unsigned long __init mq9000_get_rtc_time(void)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	struct rtc_time tm;
	decodetime(pMQRegs->rt.rt00.TimeOfDay, pMQRegs->rt.rt01.Date, &tm);
	return calendar2sec(&tm);
}

static int mq9000_set_rtc(void)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	unsigned long current_time = xtime.tv_sec;
	struct rtc_time tm;

	if (pMQRegs->rt.rt08.IntEnable & MQ_RTC_RTM_INT) {
		/* make sure not to forward the clock over an alarm */
		unsigned long alarm, sec;
		decodetime(pMQRegs->rt.rt02.AlarmTime,
				pMQRegs->rt.rt03.AlarmDate, &tm);
		alarm = calendar2sec(&tm);
		decodetime(pMQRegs->rt.rt00.TimeOfDay,
					pMQRegs->rt.rt01.Date, &tm);
		sec = calendar2sec(&tm);

		if (current_time >= alarm && alarm >= sec)
			return -ERESTARTSYS;
	}
	sec2calendar(current_time, &tm);
	pMQRegs->rt.rt00.TimeOfDay = rttimeofday(&tm);
	pMQRegs->rt.rt01.Date = rtdate(&tm);
	return 0;
}

/* IRQs are disabled before entering here from do_gettimeofday() */
static unsigned long mq9000_gettimeoffset(void)
{
	unsigned long	ticks1;
	MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	ticks1 = pMQRegs->rt.rt07.TMStatus & 0xFFFF;
	if ( (pMQRegs->rt.rt09.IntStatus & MQ_RTC_PTM_INT) 
			|| (ticks1 > TIMER_INTERVAL) )
		ticks1 = TIMER_INTERVAL;
	return TICKS2USECS(ticks1);
}

static void mq9000_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	unsigned long status;
	unsigned long flags;

	do_leds();
	local_irq_save(flags);
	do_timer(regs);

	//pMQRegs->rt.rt08.IntEnable &= ~MQ_RTC_PTM_INT;	//disable int first
		//Disable int only disable int status to in04.Rawstatus etc !!!
		// Need Disable PTM entirely to stop countdown!!!

	do {
		pMQRegs->rt.rt09.IntStatus =  MQ_RTC_PTM_INT;
		status = pMQRegs->rt.rt09.IntStatus;
	} while ( status & MQ_RTC_PTM_INT );

//HSU: seems to be working without reloading TIMER_INTERVAL
	pMQRegs->rt.rt05.PeriodicTM = TIMER_INTERVAL;
	     //??? need to reload the PTM counter. ???
	     //??? Reload is unnecessary? Counter cycle itself. ?????????

	//pMQRegs->rt.rt08.IntEnable |= MQ_RTC_PTM_INT;	//re-enable int after
		//If disable PTM entirely, need to renable PTM enable bit
		//in rt04 bit 2.

	local_irq_restore(flags);
	do_set_rtc();
	do_profile(regs);
}

static inline void setup_timer(void)
{
	//We are to assign PTM registers here directly instead of
	//  setting bits of it only.  The RTC and WTC are in the
	//  same set of registers but they will be turned on later
	//  by setting/clearing individual bits.

	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	gettimeoffset = mq9000_gettimeoffset;
	set_rtc = mq9000_set_rtc;
#if 1 // This is for a test
	pMQRegs->rt.rt04.Control |= MQ_RTC_HEX_FORMAT; // reverse logic???
	pMQRegs->rt.rt00.TimeOfDay = 0x00000000;
	// Year Count is 00 -> 1970, Day of the Week must be 05 -> Thu.
	pMQRegs->rt.rt01.Date = 0x00010105;
	while(!(pMQRegs->rt.rt04.Control & MQ_RTC_HEX_FORMAT));
#endif
	xtime.tv_sec = mq9000_get_rtc_time();
	timer_irq.handler = mq9000_timer_interrupt;

	pMQRegs->rt.rt05.PeriodicTM = TIMER_INTERVAL;

	#ifdef USE_PTM_SLOW_OSC
	pMQRegs->rt.rt04.Control &= ~(MQ_RTC_PTM_FOSC);//slow osc at 32.7kHz
	pMQRegs->rt.rt04.Control |= MQ_RTC_HEX_FORMAT; // reverse logic???
	//pMQRegs->rt.rt04.Control |= 0xff00;	//for slow osc
	pMQRegs->rt.rt04.Control |= 0x8000;	//for calendar (why???)
	#else
	pMQRegs->rt.rt04.Control |= (MQ_RTC_PTM_FOSC);//fast osc at 25MHz
	#endif

	//pMQRegs->rt.rt08.IntEnable |= MQ_RTC_PTM_INT;
//Somehow Multi-ICE says rt08 is set to 0xF???
//  Does this mean kernel only works if 0xF is set here???
//  0xF always see bit 1 in rt09 being generated

	pMQRegs->rt.rt08.IntEnable = MQ_RTC_PTM_INT;
			//?????? PTM_INT enable seems to be useless ??????

	pMQRegs->rt.rt09.IntStatus = 0x0000007f; //clr interrupt status

	//**** If we are to process second level interrupt in our handler,
	//**** e.g., to use both WTM and PTM, we need to include the handling
	//**** routine in the ONE handler.
	setup_arm_irq(IRQNO_RTCPRD, &timer_irq);

	pMQRegs->rt.rt04.Control |= MQ_RTC_PTM_ENABLE;
				//let's enable after all handlers are setup
}
