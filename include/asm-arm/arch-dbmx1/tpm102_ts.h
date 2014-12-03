/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * linux/include/asm-arm/arch-dbmx1/tpm102_ts.h
 *
 */

#define TS_MAJOR		11

#define X_AXIS_MAX		3780
#define X_AXIS_MIN		380
#define Y_AXIS_MAX		3750
#define Y_AXIS_MIN		370

/* Packet Template */
#define	NULLPACK	0x00
#define	XSER_PENIRQ	0xD4
#define	YSER_PENIRQ	0x94
#define	XDIFF_PENIRQ	0xD0
#define	YDIFF_PENIRQ	0x90
#define	XDIFF_FULLSPD	0xD3
#define	YDIFF_FULLSPD	0x93
#define	XDIFF_NOIRQ	0xD1
#define	YDIFF_NOIRQ	0x91
