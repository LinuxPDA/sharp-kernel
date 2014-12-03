/*
 *  M62332 D/A converter
 *
 *  (C) Copyright 2001 Lineo Japan, Inc.
 * 
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#ifndef __ASM_ARCH_M62332_H
#define __ASM_ARCH_M62332_H 1

#include <asm/hardware.h>
#include <asm/delay.h>

/*
 * M62332 output channel selection
 */
#define M62332_EVR_CH	1	/* M62332 volume channel number  */
				/*   0 : CH.1 , 1 : CH. 2        */
/*
 * DAC send data
 */
#define	M62332_SLAVE_ADDR	0x4e	/* Slave address  */
#define	M62332_W_BIT		0x00	/* W bit (0 only) */
#define	M62332_SUB_ADDR		0x00	/* Sub address    */
#define	M62332_A_BIT		0x00	/* A bit (0 only) */

/*
 * DAC setup and hold times (expressed in us)
 */
#define DAC_BUS_FREE_TIME	5	/*   4.7 us */
#define DAC_START_SETUP_TIME	5	/*   4.7 us */
#define DAC_STOP_SETUP_TIME	4	/*   4.0 us */
#define DAC_START_HOLD_TIME	5	/*   4.7 us */
#define DAC_SCL_LOW_HOLD_TIME	5	/*   4.7 us */
#define DAC_SCL_HIGH_HOLD_TIME	4	/*   4.0 us */
#define DAC_DATA_SETUP_TIME	1	/*   250 ns */
#define DAC_DATA_HOLD_TIME	1	/*   300 ns */
#define DAC_LOW_SETUP_TIME	1	/*   300 ns */
#define DAC_HIGH_SETUP_TIME	1	/*  1000 ns */

extern void m62332_sendbit(int bit);
extern int  m62332_senddata(unsigned int dac_data, int channel);

#endif /* __ASM_ARCH_M62332_H */
