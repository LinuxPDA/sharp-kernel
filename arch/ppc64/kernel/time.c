/*
 * 
 * Common time routines among all ppc machines.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu) to merge
 * Paul Mackerras' version and mine for PReP and Pmac.
 * MPC8xx/MBX changes by Dan Malek (dmalek@jlc.net).
 * Converted for 64-bit by Mike Corrigan
 *
 * First round of bugfixes by Gabriel Paubert (paubert@iram.es)
 * to make clock more stable (2.4.0-test5). The only thing
 * that this code assumes is that the timebases have been synchronized
 * by firmware on SMP and are never stopped (never do sleep
 * on SMP then, nap and doze are OK).
 *
 * TODO (not necessarily in this file):
 * - improve precision and reproducibility of timebase frequency
 * measurement at boot time.
 * - get rid of xtime_lock for gettimeofday (generic kernel problem
 * to be implemented on all architectures for SMP scalability and
 * eventually implementing gettimeofday without entering the kernel).
 * - put all time/clock related variables in a single structure
 * to minimize number of cache lines touched by gettimeofday()
 * - for astronomical applications: add a new function to get
 * non ambiguous timestamps even around leap seconds. This needs
 * a new timestamp format and a good name.
 *
 *
 * The following comment is partially obsolete (at least the long wait
 * is no more a valid reason):
 * Since the MPC8xx has a programmable interrupt timer, I decided to
 * use that rather than the decrementer.  Two reasons: 1.) the clock
 * frequency is low, causing 2.) a long wait in the timer interrupt
 *		while ((d = get_dec()) == dval)
 * loop.  The MPC8xx can be driven from a variety of input clocks,
 * so a number of assumptions have been made here because the kernel
 * parameter HZ is a constant.  We assume (correctly, today :-) that
 * the MPC8xx on the MBX board is driven from a 32.768 kHz crystal.
 * This is then divided by 4, providing a 8192 Hz clock into the PIT.
 * Since it is not possible to get a nice 100 Hz clock out of this, without
 * creating a software PLL, I have set HZ to 128.  -- Dan
 *
 * 1997-09-10  Updated NTP code according to technical memorandum Jan '96
 *             "A Kernel Model for Precision Timekeeping" by Dave Mills
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
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
#include <linux/time.h>
#include <linux/init.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/nvram.h>
#include <asm/cache.h>
#include <asm/machdep.h>

#include <asm/time.h>

void smp_local_timer_interrupt(struct pt_regs *);

/* keep track of when we need to update the rtc */
time_t last_rtc_update;
extern rwlock_t xtime_lock;
extern int piranha_simulator;

unsigned long tb_ticks_per_jiffy = 5000000;	/* initial sane values */
unsigned long tb_ticks_per_usec  = 500;
unsigned long tb_to_us;
spinlock_t rtc_lock = SPIN_LOCK_UNLOCKED;

extern unsigned long wall_jiffies;
extern unsigned long lpEvent_count;
extern int smp_tb_synchronized;

static long time_offset = 0;

extern unsigned long prof_cpu_mask;
extern unsigned int * prof_buffer;
extern unsigned long prof_len;
extern unsigned long prof_shift;
extern char _stext;

static inline void ppc_do_profile (unsigned long nip)
{
	if (!prof_buffer)
		return;

	/*
	 * Only measure the CPUs specified by /proc/irq/prof_cpu_mask.
	 * (default is all CPUs.)
	 */
	if (!((1<<smp_processor_id()) & prof_cpu_mask))
		return;

	nip -= (unsigned long) &_stext;
	nip >>= prof_shift;
	/*
	 * Don't ignore out-of-bounds EIP values silently,
	 * put them into the last histogram slot, so if
	 * present, they will show up as a sharp peak.
	 */
	if (nip > prof_len-1)
		nip = prof_len-1;
	atomic_inc((atomic_t *)&prof_buffer[nip]);
}

static __inline__ void timer_check_rtc(void)
{
		/*
		 * update the rtc when needed, this should be performed on the
		 * right fraction of a second. Half or full second ?
		 * Full second works on mk48t59 clocks, others need testing.
		 * Note that this update is basically only used through
		 * the adjtimex system calls. Setting the HW clock in
		 * any other way is a /dev/rtc and userland business.
		 * This is still wrong by -0.5/+1.5 jiffies because of the
		 * timer interrupt resolution and possible delay, but here we
		 * hit a quantization limit which can only be solved by higher
		 * resolution timers and decoupling time management from timer
		 * interrupts. This is also wrong on the clocks
		 * which require being written at the half second boundary.
		 * We should have an rtc call that only sets the minutes and
		 * seconds like on Intel to avoid problems with non UTC clocks.
		 */
		if ( (time_status & STA_UNSYNC) == 0 &&
		     xtime.tv_sec - last_rtc_update >= 659 &&
		     abs(xtime.tv_usec - (1000000-1000000/HZ)) < 500000/HZ &&
		     jiffies - wall_jiffies == 1) {
		  	if (ppc_md.set_rtc_time(xtime.tv_sec+1 + time_offset) == 0)
				last_rtc_update = xtime.tv_sec+1;
			else
				/* Try again one minute later */
				last_rtc_update += 60;
		}

}

/*
 * For iSeries shared processors, we have to let the hypervisor
 * set the hardware decrementer.  We set a virtual decrementer
 * in the ItLpPaca and call the hypervisor if the virtual
 * decrementer is less than the current value in the hardware
 * decrementer. (almost always the new decrementer value will
 * be greater than the current hardware decementer so the hypervisor
 * call will not be needed)
 */

/*
 * timer_interrupt - gets called when the decrementer overflows,
 * with interrupts disabled.
 * We set it up to overflow again in 1/HZ seconds.
 */
int timer_interrupt(struct pt_regs * regs)
{
	int next_dec;
	unsigned long cur_tb;

	struct Paca * paca = (struct Paca *)mfspr(SPRG3);
	unsigned long cpu = paca->xPacaIndex;
	struct ItLpQueue * lpq;

	irq_enter(cpu);

	paca = (struct Paca *)mfspr(SPRG3);
	
	if (!user_mode(regs))
		ppc_do_profile(instruction_pointer(regs));

	while ( paca->next_jiffy_update_tb <= (cur_tb = get_tb()) ) {

#ifdef CONFIG_SMP
		smp_local_timer_interrupt(regs);
#endif	
		if ( cpu == 0 ) {	
			write_lock(&xtime_lock);
			do_timer(regs);
			timer_check_rtc();
			write_unlock(&xtime_lock);
		}
		paca->next_jiffy_update_tb += tb_ticks_per_jiffy;
	}
	next_dec = paca->next_jiffy_update_tb - cur_tb;
	if ( next_dec > paca->default_decr )
		next_dec = paca->default_decr;
	set_dec(next_dec);
 
	if (ppc_md.heartbeat && !ppc_md.heartbeat_count--)
		ppc_md.heartbeat();

	paca->xLpPaca.xIntDword.xFields.xDecrInt = 0;
	lpq = paca->lpQueuePtr;
	if ( lpq )
		lpEvent_count += ItLpQueue_process( lpq, regs );
	
	irq_exit(cpu);

	if (softirq_pending(cpu))
		do_softirq();

	return 1; /* lets ret_from_int know we can do checks */
}

/*
 * This version of gettimeofday has microsecond resolution.
 */
void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;
	unsigned long delta, lost_ticks, usec, sec;

	read_lock_irqsave(&xtime_lock, flags);
	sec = xtime.tv_sec;
	usec = xtime.tv_usec;

	delta = tb_ticks_per_jiffy - ( xPaca[0].next_jiffy_update_tb - get_tb() );

#ifdef CONFIG_SMP
	/* As long as timebases are not in sync, gettimeofday can only
	 * have jiffy resolution on SMP.
	 */
	if (!smp_tb_synchronized)
		delta = 0;
#endif /* CONFIG_SMP */
	lost_ticks = jiffies - wall_jiffies;
	read_unlock_irqrestore(&xtime_lock, flags);

	/* Convert the lost ticks into micro-seconds. */
	usec += ( ( tb_ticks_per_jiffy * lost_ticks ) + delta ) / tb_ticks_per_usec;
	while (usec > 1000000) {
	  	sec++;
		usec -= 1000000;
	}
	tv->tv_sec = sec;
	tv->tv_usec = usec;
}

void do_settimeofday(struct timeval *tv)
{
	unsigned long flags;
	long int tb_delta, new_usec, new_sec;

	write_lock_irqsave(&xtime_lock, flags);
	/* Updating the RTC is not the job of this code. If the time is
	 * stepped under NTP, the RTC will be update after STA_UNSYNC
	 * is cleared. Tool like clock/hwclock either copy the RTC
	 * to the system time, in which case there is no point in writing
	 * to the RTC again, or write to the RTC but then they don't call
	 * settimeofday to perform this operation. Note also that
	 * we don't touch the decrementer since:
	 * a) it would lose timer interrupt synchronization on SMP
	 * (if it is working one day)
	 * b) it could make one jiffy spuriously shorter or longer
	 * which would introduce another source of uncertainty potentially
	 * harmful to relatively short timers.
	 */

	/* This works perfectly on SMP only if the tb are in sync but 
	 * guarantees an error < 1 jiffy even if they are off by eons,
	 * still reasonable when gettimeofday resolution is 1 jiffy.
	 */

	tb_delta = tb_ticks_per_jiffy - ( xPaca[0].next_jiffy_update_tb - get_tb() );
	tb_delta += (jiffies - wall_jiffies) * tb_ticks_per_jiffy;

	new_sec = tv->tv_sec;
	new_usec = tv->tv_usec - tb_delta / tb_ticks_per_usec;
	while (new_usec <0) {
		new_sec--; 
		new_usec += 1000000;
	}
	xtime.tv_usec = new_usec;
	xtime.tv_sec = new_sec;

	/* In case of a large backwards jump in time with NTP, we want the 
	 * clock to be updated as soon as the PLL is again in lock.
	 */
	last_rtc_update = new_sec - 658;

	time_adjust = 0;                /* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_state = TIME_ERROR;        /* p. 24, (a) */
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irqrestore(&xtime_lock, flags);
}

extern int do_sys_settimeofday(struct timeval *tv, struct timezone *tz);

void __init time_init(void)
{
	time_t sec, old_sec;
	unsigned long old_stamp, stamp, elapsed;
	/* This function is only called on the boot processor */
	unsigned long flags;

        if (ppc_md.time_init != NULL)
                time_offset = ppc_md.time_init();

        ppc_md.calibrate_decr();

	/* Now that the decrementer is calibrated, it can be used in case the 
	 * clock is stuck, but the fact that we have to handle the 601
	 * makes things more complex. Repeatedly read the RTC until the
	 * next second boundary to try to achieve some precision...
	 */
	stamp = get_tb();
	sec = ppc_md.get_rtc_time();
	elapsed = 0;
	if ( ! piranha_simulator ) {
	do {
		old_stamp = stamp; 
		old_sec = sec;
		stamp = get_tb();
		elapsed += stamp - old_stamp;
		sec = ppc_md.get_rtc_time();
	} while ( sec == old_sec && elapsed < 2*HZ*tb_ticks_per_jiffy);
	if (sec==old_sec) {
		printk("Warning: real time clock seems stuck!\n");
	}
	}
	write_lock_irqsave(&xtime_lock, flags);
	xtime.tv_sec = sec;
	last_jiffy_stamp(0) = stamp;
	xtime.tv_usec = 0;
	/* No update now, we just read the time from the RTC ! */
	last_rtc_update = xtime.tv_sec;
	write_unlock_irqrestore(&xtime_lock, flags);
	/* Not exact, but the timer interrupt takes care of this */
	set_dec(tb_ticks_per_jiffy);

	/* If platform provided a timezone (pmac), we correct the time
	 * using do_sys_settimeofday() which in turn calls warp_clock()
	 */
        if (time_offset) {
        	struct timezone tz;
        	tz.tz_minuteswest = -time_offset / 60;
        	tz.tz_dsttime = 0;
        	do_sys_settimeofday(NULL, &tz);
        }
}

#define TICK_SIZE tick
#define FEBRUARY	2
#define	STARTOFTIME	1970
#define SECDAY		86400L
#define SECYR		(SECDAY * 365)
#define	leapyear(year)		((year) % 4 == 0)
#define	days_in_year(a) 	(leapyear(a) ? 366 : 365)
#define	days_in_month(a) 	(month_days[(a) - 1])

static int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/*
 * This only works for the Gregorian calendar - i.e. after 1752 (in the UK)
 */
void GregorianDay(struct rtc_time * tm)
{
	int leapsToDate;
	int lastYear;
	int day;
	int MonthOffset[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

	lastYear=tm->tm_year-1;

	/*
	 * Number of leap corrections to apply up to end of last year
	 */
	leapsToDate = lastYear/4 - lastYear/100 + lastYear/400;

	/*
	 * This year is a leap year if it is divisible by 4 except when it is
	 * divisible by 100 unless it is divisible by 400
	 *
	 * e.g. 1904 was a leap year, 1900 was not, 1996 is, and 2000 will be
	 */
	if((tm->tm_year%4==0) &&
	   ((tm->tm_year%100!=0) || (tm->tm_year%400==0)) &&
	   (tm->tm_mon>2))
	{
		/*
		 * We are past Feb. 29 in a leap year
		 */
		day=1;
	}
	else
	{
		day=0;
	}

	day += lastYear*365 + leapsToDate + MonthOffset[tm->tm_mon-1] +
		   tm->tm_mday;

	tm->tm_wday=day%7;
}

void to_tm(int tim, struct rtc_time * tm)
{
	register int    i;
	register long   hms, day;

	day = tim / SECDAY;
	hms = tim % SECDAY;

	/* Hours, minutes, seconds are easy */
	tm->tm_hour = hms / 3600;
	tm->tm_min = (hms % 3600) / 60;
	tm->tm_sec = (hms % 3600) % 60;

	/* Number of years in days */
	for (i = STARTOFTIME; day >= days_in_year(i); i++)
		day -= days_in_year(i);
	tm->tm_year = i;

	/* Number of months in days left */
	if (leapyear(tm->tm_year))
		days_in_month(FEBRUARY) = 29;
	for (i = 1; day >= days_in_month(i); i++)
		day -= days_in_month(i);
	days_in_month(FEBRUARY) = 28;
	tm->tm_mon = i;

	/* Days are what is left over (+1) from all that. */
	tm->tm_mday = day + 1;

	/*
	 * Determine the day of week
	 */
	GregorianDay(tm);
}

/* Auxiliary function to compute scaling factors */
/* Actually the choice of a timebase running at 1/4 the of the bus
 * frequency giving resolution of a few tens of nanoseconds is quite nice.
 * It makes this computation very precise (27-28 bits typically) which
 * is optimistic considering the stability of most processor clock
 * oscillators and the precision with which the timebase frequency
 * is measured but does not harm.
 */
/*unsigned mulhwu_scale_factor(unsigned inscale, unsigned outscale) { */
/*	unsigned mlt=0, tmp, err; */
	/* No concern for performance, it's done once: use a stupid
	 * but safe and compact method to find the multiplier.
	 */
/*
  	for (tmp = 1U<<31; tmp != 0; tmp >>= 1) {
  		if (mulhwu(inscale, mlt|tmp) < outscale) mlt|=tmp;
  	}
*/
	/* We might still be off by 1 for the best approximation.
	 * A side effect of this is that if outscale is too large
	 * the returned value will be zero.
	 * Many corner cases have been checked and seem to work,
	 * some might have been forgotten in the test however.
	 */
/*
  	err = inscale*(mlt+1);
  	if (err <= inscale/2) mlt++;
  	return mlt;
  }
*/
