/*
 *  linux/include/asm-arm/arch-cotulla/sharpsl_battery.h
 *  
 *  (C) Copyright 2002 HTC, Inc.
 *  
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 * modify: TX/RX I2C for DEMO data is ok.
 *
 * ChangeLog:
 *	21-Aug-2002 Lineo Japan, Inc.  for 2.4.18
 *	26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 *
 */

/* Temperature parameters definition */
#define		num_0		0
#define		num_2		2
#define		num_3		3
#define		num_4		4
#define		num_5		5
#define		num_6		6
#define		num_10		10
#define		num_19		19
#define		num_20		20
#define		num_30		30
#define		num_35		35
#define		num_36		36
#define		num_38		38
#define		num_50		50
#define		num_60		60
#define		num_80		80
#define		num_90		90
#define		num_100		100
#define		num_175		175
#define		num_190		190
#define		num_200		200
#define		num_900		900
#define		num_360		360
#define		num_3600	3600
#define		num_500		500
#define		num_5000	5000
#define		num_k		1000
#define		num_1000	1000
#define		num_1900	1900
#define		num_9000	9000
#define		num_8100	8100
#define		num_9100	9100
#define		num_2293	2293
#define		num_2500	2500
#define		num_10000	10000
#define		num_25000	25000
#define		num_3600	3600		/* 3600 * 1000 */
#define		num_4096	4096
#define		num_190000	190000
#define		num_810000	810000
#define		num_910000	910000
#define		num_x3d		0x3d
#define		num_x62		0x62
#define		num_x10		0x42
#define		num_x49		0x49		/* 0.045v for diff temperature */



#define		T_P10		1709
#define		T_P05		1849
#define		T_P00		1975
#define		T_N05		2086
#define		T_N10		2180
#define		T_NON		65535

#define		Pbase_k		60		/* 0.06 x 1000 */
#define		Psm_k		50		/* 50 : SD card */
#define		Pim_k		50		/* 50 : IrDA */
#define		Plm_k		100		/* 100 : Back-lite */
#define		Pam_k		200		/* 200 : Audio */
#if defined(CONFIG_ARCH_PXA_POODLE)
#define		MUX_CHL		6u		/* channel of MUX */
#define		BATT_CHL	2u		/* channel of BATTery */
#elif defined(CONFIG_ARCH_PXA_CORGI)
#define		BATT_AD		4u		/* channel of BATTery */
#define		BATT_THM	2u		/* channel of BATTery */
#define		JK_VAD		6u		/* channel of BATTery */
#elif defined(CONFIG_ARCH_PXA_TOSA)
#define		BATT_VC		0x4		/* channel of BAT V CAUTION */
#define		BATT_TH		0x5		/* channel of BAT TH */
#define		BATT_V		0x6		/* channel of BAT V */
#define		BU_V		0x7		/* channel of BU V */
#endif

#define		Temper_V47	0x449	/* 0.670v (47"C) */
#define		Temper_V50	0x3e7	/* 0.610v (50"C) */
#define		BattBad_V	0x614	/* 0.95v */
#define		ReCharge_V	0x666	/* 1.0v  */
#define		Min_120		120		/* 120 minutes */
#define		hour_8		480		/* 60*8 minutes */
