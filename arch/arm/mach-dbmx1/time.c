/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/arch/arm/mach-db1mx1/time.c
 *
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>

extern int (*set_rtc)(void);

static int dbmx1_set_rtc(void)
{
	return 0;
}

static int dbmx1_rtc_init(void)
{
	xtime.tv_sec = 0;

	set_rtc = dbmx1_set_rtc;

	return 0;
}

__initcall(dbmx1_rtc_init);
