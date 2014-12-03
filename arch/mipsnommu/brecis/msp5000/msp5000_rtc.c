/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * RTC routines for Brecis style attached I2C chip.
 *  NOTE: FIXME: not implemented yet
 *
 */

#include <linux/spinlock.h>
#include <linux/mc146818rtc.h>
#include <asm/brecis/brecis.h>

int a5000clk_seconds;	// global used for a5000 routines

static unsigned char brecis_rtc_read_data(unsigned long addr)
{
#if 0 // ...MaTed--- RTC is on I2C bus - not implemented
	volatile unsigned int *rtc_adr_reg = (void *)BRECIS_RTC_ADR_REG;
	volatile unsigned int *rtc_dat_reg = (void *)BRECIS_RTC_DAT_REG;

	*rtc_adr_reg = addr;

	return *rtc_dat_reg;
#else	// 0
	return 0;
#endif // 0
}

static void brecis_rtc_write_data(unsigned char data, unsigned long addr)
{
#if 0 // ...MaTed--- RTC is on I2C bus - not implemented
	volatile unsigned int *rtc_adr_reg = (void *)BRECIS_RTC_ADR_REG;
	volatile unsigned int *rtc_dat_reg = (void *)BRECIS_RTC_DAT_REG;

	*rtc_adr_reg = addr;
	*rtc_dat_reg = data;
#else	// 0
	return;
#endif // 0
}

static int brecis_rtc_bcd_mode(void)
{
	return 0;
}

struct rtc_ops brecis_rtc_ops = {
	&brecis_rtc_read_data,
	&brecis_rtc_write_data,
	&brecis_rtc_bcd_mode};
