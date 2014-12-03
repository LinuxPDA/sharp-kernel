/*
 *  linux/include/asm/sharp_ts.h
 */

#ifndef __ASM_SHARP_TS_H_INCLUDED
#define __ASM_SHARP_TS_H_INCLUDED

#include <linux/major.h>

#ifndef TS_MAJOR
#define TS_MAJOR 55
#endif

typedef struct
{
  long	x;
  long	y;
  long	pressure;
  long long timestump;
} tsEvent;

typedef struct
{
  long	x;
  long	y;
} tsPos;

typedef struct
{
  tsPos	rd_lt;
  tsPos	rd_rt;
  tsPos	rd_lb;
  tsPos	rd_rb;
  tsPos	ex_lt;
  tsPos	ex_rt;
  tsPos	ex_lb;
  tsPos	ex_rb;
} tsCalibPos;

#define SAMPLE_BUF_SIZE	256

typedef struct
{
  int     is_locked;
  int     head;
  int     count;
  tsEvent buf[SAMPLE_BUF_SIZE];
  tsEvent last_pendown_buf;
} tsSampleQueue;

typedef struct
{
  long	x;
  long	y;
  long	status;
  long long	timestump;
} tsReadBuf;

#endif /* __ASM_SHARP_TS_H_INCLUDED */

