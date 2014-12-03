/*
 * linux/include/asm-arm/arch-pxa/ads7846_ts.h
 *
 * Copyright (C) 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Change Log
 *	09-Jun-2004 Lineo Solutions, Inc.  for Spitz
 */


/*#define TS_MAJOR		55*/		/* default device major */
#define TS_MAJOR		11		/* for sharp_ts */
//#define GPIO_PENIRQ		0x00080000  //gpio51 0x00080000
#ifdef CONFIG_SABINAL_DISCOVERY
#define GPIO_TP_INT             (51)
#define IRQ_GPIO_TP_INT         IRQ_GPIO(51)
#endif
#if defined(CONFIG_ARCH_PXA_SPITZ)
#define X_AXIS_MAX		3900
#define X_AXIS_MIN		150
#define Y_AXIS_MAX		3900
#define Y_AXIS_MIN		190
#else
#define X_AXIS_MAX		3830
#define X_AXIS_MIN		150
#define Y_AXIS_MAX		3830
#define Y_AXIS_MIN		190
#endif


/* ADS7846 Touch Screen Controller bit shift constants */

#define ADSCTRL_PD0_SH		0	// PD0 bit
#define ADSCTRL_PD1_SH		1	// PD1 bit
#define ADSCTRL_DFR_SH		2	// SER/DFR bit
#define ADSCTRL_MOD_SH		3	// Mode bit
#define ADSCTRL_ADR_SH		4	// Address setting
#define ADSCTRL_STS_SH		7	// Start bit


/* ADS7846 Touch Screen Controller bit mask constants */

#define ADSCTRL_PD0_MSK			(1u << ADSCTRL_PD0_SH)
#define ADSCTRL_PD1_MSK			(1u << ADSCTRL_PD0_SH)

typedef struct ts_pos_s {
	unsigned long xd;
	unsigned long yd;
} ts_pos_t;
