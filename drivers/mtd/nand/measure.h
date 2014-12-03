/*
 * drivers/mtd/nand/measure.h
 *
 * Copyright (C) 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * $Id: measure.h,v 1.1.2.3 2003/01/14 09:29:14 soka Exp $
 */

#ifndef JFFS2_MEASURE_H
#define JFFS2_MEASURE_H

#ifdef DEBUG_PROC
#include <linux/time.h>


#define DEFINE_MEASUREMENT_VAR(name) struct timeval tv_##name
#define DEFINE_COUNTER_VAR(name) int name

#define START_MEASUREMENT(name)	\
struct timeval tv1_##name;	\
do_gettimeofday(& tv1_##name)

#define COUNTER_INC(name) do { extern int name; name++; } while (0)

#define ACCUMULATE_ELAPSED_TIME(name)	\
{	\
    extern struct timeval tv_##name;	\
    struct timeval tv2_##name;	\
    do_gettimeofday(& tv2_##name);	\
    long usec, sec;		\
    usec = tv2_##name.tv_usec - tv1_##name.tv_usec;	\
    sec = tv2_##name.tv_sec - tv1_##name.tv_sec;	\
    if (usec < 0) {	\
	sec--;	\
	usec += 1000000;	\
    }	\
    tv_##name.tv_usec += usec;	\
    tv_##name.tv_sec += sec;	\
    while (tv_##name.tv_usec >= 1000000) {	\
	tv_##name.tv_sec++;	\
	tv_##name.tv_usec -= 1000000;	\
    }	\
}

#define CLEAR_TIME_VER(name) tv_##name.tv_sec = tv_##name.tv_usec = 0

#define PRINT_ELAPSED_TIME(name, buf, len)	\
{	\
    extern struct timeval tv_##name;	\
    len += sprintf(buf+len, #name " %lu.%06lu\n", tv_##name.tv_sec, tv_##name.tv_usec);	\
}

#else
#define DEFINE_MEASUREMENT_VAR(name)
#define DEFINE_COUNTER_VAR(name)
#define START_MEASUREMENT(name)
#define COUNTER_INC(name)
#define ACCUMULATE_ELAPSED_TIME(name)
#define CLEAR_TIME_VER(name)
#define PRINT_ELAPSED_TIME(name, buf, len)
#endif


#endif
