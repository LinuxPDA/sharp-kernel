/*
 *  M62332 D/A converter
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/arch/m62332.h>

void m62332_sendbit(int bit)
{
	LCM_DAC &= ~(LCM_DAC_SCLOEB);
	udelay(DAC_LOW_SETUP_TIME);	/* 300 nsec */
	udelay(DAC_DATA_HOLD_TIME);	/* 300 nsec */
	LCM_DAC &= ~(LCM_DAC_SDAOEB);
	udelay(DAC_LOW_SETUP_TIME);		/* 300 nsec */
	udelay(DAC_SCL_LOW_HOLD_TIME);		/* 4.7 usec */

	if (bit & 1) {
		LCM_DAC |= LCM_DAC_SDAOEB;
		udelay(DAC_HIGH_SETUP_TIME);	/* 1000 nsec */
	} else {
		LCM_DAC &= ~(LCM_DAC_SDAOEB);
		udelay(DAC_LOW_SETUP_TIME);	/* 300 nsec */
	}

	udelay(DAC_DATA_SETUP_TIME);		/* 250 nsec */
	LCM_DAC |= LCM_DAC_SCLOEB;
	udelay(DAC_HIGH_SETUP_TIME);		/* 1000 nsec */
	udelay(DAC_SCL_HIGH_HOLD_TIME);		/*  4.0 usec */
}

int m62332_senddata(unsigned int dac_data, int channel)
{
	int i;
	unsigned char data;

	/* Start */
	udelay(DAC_BUS_FREE_TIME);		/* 5.0 usec */
	LCM_DAC |= LCM_DAC_SCLOEB | LCM_DAC_SDAOEB;
	udelay(DAC_HIGH_SETUP_TIME);		/* 1000 nsec */
	udelay(DAC_SCL_HIGH_HOLD_TIME);		/* 4.0 usec */
	LCM_DAC &= ~(LCM_DAC_SDAOEB);
	udelay(DAC_START_HOLD_TIME);		/* 5.0 usec */
	udelay(DAC_DATA_HOLD_TIME);		/* 300 nsec */

	/* Send slave address and W bit (LSB is W bit) */
	data = (M62332_SLAVE_ADDR << 1) | M62332_W_BIT;
	for (i = 1; i <= 8; i++) {
		m62332_sendbit(data >> (8 - i));
	}

	/* Check A bit */
	LCM_DAC &= ~(LCM_DAC_SCLOEB);
	udelay(DAC_LOW_SETUP_TIME);	/* 300 nsec */
	udelay(DAC_SCL_LOW_HOLD_TIME);  /* 4.7 usec */
	LCM_DAC &= ~(LCM_DAC_SDAOEB);
	udelay(DAC_LOW_SETUP_TIME);		/* 300 nsec */
	LCM_DAC |= LCM_DAC_SCLOEB;
	udelay(DAC_HIGH_SETUP_TIME);		/* 1000 nsec */
	udelay(DAC_SCL_HIGH_HOLD_TIME);		/* 4.7 usec */
	if (LCM_DAC & LCM_DAC_SDAOEB) {		/* High is error */
		printk("m62332_senddata Error 1\n");
		return -1;
	}

	/* Send Sub address (LSB is channel select) */
	/*    channel = 0 : ch1 select              */
	/*            = 1 : ch2 select              */
	data = M62332_SUB_ADDR + channel;
	for (i = 1; i <= 8; i++) {
		m62332_sendbit(data >> (8 - i));
	}

	/* Check A bit */
	LCM_DAC &= ~(LCM_DAC_SCLOEB);
	udelay(DAC_LOW_SETUP_TIME);	/* 300 nsec */
	udelay(DAC_SCL_LOW_HOLD_TIME);  /* 4.7 usec */
	LCM_DAC &= ~(LCM_DAC_SDAOEB);
	udelay(DAC_LOW_SETUP_TIME);		/* 300 nsec */
	LCM_DAC |= LCM_DAC_SCLOEB;
	udelay(DAC_HIGH_SETUP_TIME);		/* 1000 nsec */
	udelay(DAC_SCL_HIGH_HOLD_TIME);		/* 4.7 usec */
	if (LCM_DAC & LCM_DAC_SDAOEB) {		/* High is error */
		printk("m62332_senddata Error 2\n");
		return -1;
	}

	/* Send DAC data */
	for (i = 1; i <= 8; i++) {
		m62332_sendbit(dac_data >> (8 - i));
	}

	/* Check A bit */
	LCM_DAC &= ~(LCM_DAC_SCLOEB);
	udelay(DAC_LOW_SETUP_TIME);	/* 300 nsec */
	udelay(DAC_SCL_LOW_HOLD_TIME);  /* 4.7 usec */
	LCM_DAC &= ~(LCM_DAC_SDAOEB);
	udelay(DAC_LOW_SETUP_TIME);		/* 300 nsec */
	LCM_DAC |= LCM_DAC_SCLOEB;
	udelay(DAC_HIGH_SETUP_TIME);		/* 1000 nsec */
	udelay(DAC_SCL_HIGH_HOLD_TIME);		/* 4.7 usec */
	if (LCM_DAC & LCM_DAC_SDAOEB) {		/* High is error */
		printk("m62332_senddata Error 3\n");
		return -1;
	}

	/* stop */
	LCM_DAC &= ~(LCM_DAC_SCLOEB);
	udelay(DAC_LOW_SETUP_TIME);	/* 300 nsec */
	udelay(DAC_SCL_LOW_HOLD_TIME);  /* 4.7 usec */
	LCM_DAC |= LCM_DAC_SCLOEB;
	udelay(DAC_HIGH_SETUP_TIME);		/* 1000 nsec */
	udelay(DAC_SCL_HIGH_HOLD_TIME);		/* 4 usec */
	LCM_DAC |= LCM_DAC_SDAOEB;
	udelay(DAC_HIGH_SETUP_TIME);		/* 1000 nsec */
	udelay(DAC_SCL_HIGH_HOLD_TIME);		/* 4 usec */
	
	//	LCM_DAC &= ~(LCM_DAC_SCLOEB | LCM_DAC_SDAOEB);
	LCM_DAC |= (LCM_DAC_SCLOEB | LCM_DAC_SDAOEB);
	udelay(DAC_LOW_SETUP_TIME);		/* 1000 nsec */
	udelay(DAC_SCL_LOW_HOLD_TIME);		/* 4.7 usec */

	return 0;
}
