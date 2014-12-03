/*
 *  linux/include/asm-arm/arch-pxa/discovery_battery.h
 *  
 *  (C) Copyright 2002 Steve Lin (stevelin168@hotmail.com)
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * modify: TX/RX I2C for DEMO data is ok.
 *
 */

#include <asm/semaphore.h>

#define ADC_REQ_ID	(u32)(&battery_device)


#define PRINTK_BATT		0 /* if 1:print Debug message , 0: no print */

#ifdef CONFIG_SABINAL_DISCOVERY_DVT
#define POLL_MCU		0 /* if 1:contact micron , 0:don't polling micron */
#define	AC_IN_LEVEL		0 /* if 1:has AC_IN=High, 0:has AC_IN=Low */
#define	MUX_EN_LEVEL	0 /* if 1: high active, 0:low active */
#define	BATT_SAM_LEVEL	1 /* if 1: high for pre-mv&mv 0:for dvt&dvt2 */
#endif

#ifdef CONFIG_SABINAL_DISCOVERY_DVT2
#define POLL_MCU		1 /* if 1:contact micron , 0:don't polling micron */
#define	AC_IN_LEVEL		1 /* if 1:has AC_IN=High, 0:has AC_IN=Low */
#define	MUX_EN_LEVEL	0 /* if 1: high active, 0:low active */
#define	BATT_SAM_LEVEL	0 /* if 1: high for pre-mv&mv 0:for dvt&dvt2 */
#endif

#ifdef CONFIG_SABINAL_DISCOVERY_PreMV
#define POLL_MCU		1 /* if 1:contact micron , 0:don't polling micron */
#define	AC_IN_LEVEL		1 /* if 1:has AC_IN=High, 0:has AC_IN=Low */
#define	MUX_EN_LEVEL	1 /* if 1: high active, 0:low active */
#define	BATT_SAM_LEVEL	1 /* if 1: high for pre-mv&mv 0:for dvt&dvt2 */
#endif


/* for internal GPIO setting IRQ_ASIC3_AC_IN */
//#define GPIO_INT_0			0x00

typedef struct BatteryThresh {
	int high;
	int low;
	int verylow;
} BATTERY_THRESH;

BATTERY_THRESH  discovery_main_battery_thresh_fl = {  20, 10, 0 };
BATTERY_THRESH  discovery_main_battery_thresh_nofl = {  378, 364, 362 };

#define ConvRevise(x)          		( ( ad_revise * x ) / 652 )
#define COLLIE_PM_TICK         		( 100 )						/* 1sec */
#define	DISCOVERY_MINUTE_TICKTIME	( 60 * COLLIE_PM_TICK )		/* 60 sec */

#define COLLIE_APO_DEFAULT		( ( 3 * 60 ) * COLLIE_PM_TICK )	// 3 min
#define COLLIE_LPO_DEFAULT		(  20 * COLLIE_PM_TICK )	// 20 sec
#define	DISCOVERY_APO_TICKTIME		(  5  * COLLIE_PM_TICK )

unsigned int APOCnt = COLLIE_APO_DEFAULT;
unsigned int LPOCnt = COLLIE_LPO_DEFAULT;

#define RCSR_HWR	0x00000001	/* Hard Reset                       */
#define RCSR_SMR	0x00000004	/* Sleep mode Reset                 */



/* parameters of limits of battery */
//#define	back_in_flag		1	/* 1:back 0:no back */
//#define IMIN_LIMIT			( 70 + back_in_flag? 30:0 )
//#define	IMIN_LIMIT			0x2b0	/* temperail :0.42v 0.07v */
#define	IMIN_LIMIT			0x158	/* temperail :0.42v 0.07v */
#define BATT_CHARGE_START   0x600
#define collie_charge_volt	465		/* charge : check charge 3.0V */
#define	Q_stage				0	/* in Q_stage */
#define T_stage				1	/* in T_stage */
#define	R_stage				2	/* in R_stage */

/*** gloabal variables ************************************************************/
int charge_status = 0;			/* charge status  1 : charge  0: not charge */
static unsigned int		battery_index,RCu;
static int	msglevel;
static unsigned int		Charge_time=0;
unsigned char ad_revise = 0;
static unsigned char	battery_off_flag = 0;	/* charge : suspend while get adc */
static unsigned char	i2c_order=0;
static unsigned char	Read_Ebatt_flag=0;
static unsigned char	IQTR_stage = Q_stage;	/* initial for Q_stage */
static unsigned char	imin_count = 0;			/* for battery full charging 30 sec count */
static unsigned char	temp_count = 0;
static short	Tx=0;
static unsigned short	Vthx=0;
static unsigned short	E2MI=0;				/* backpack's ext-battery's current */
static unsigned short 	VMAX=0;
static unsigned char	poll_flag=0;
#define	First_read		(1<<0)				/* after first read it set */
#define	BattBad_flag	(1<<1)				/* set if battery fault */
#define	driver_min_curr	(1<<2)				/* set when imin event happened */

unsigned short	driver_waste; /* for driver insert of power consumption pin */
unsigned short	touch_panel_intr=0;
unsigned short	keyboard_intr=0;
struct semaphore i2c_sem;

#if 1 //ned
#define	Volume_val		(1<<0) /* bit 2,1,0 */
#define	Bright_val		(1<<3) /* bit 5,4,3 */
#define	SD_waste		(1<<6)
#define	IrDA_waste		(1<<7)
#define	Flite_waste		(1<<8)
#define	Audio_waste		(1<<9)
#define	BackPack_waste	(1<<10)
#define	NFLED_on_waste	(1<<11)
#define	NFLED_blink_waste	(1<<12)
#define	Volume_mask		0x07		/* 0b00000111 */
#define	Bright_mask		0x38		/* 0b00111000 */
#define	NFLED_mask		0x1800		/* 0b0001100000000000; bit 12, 11 */
#endif //ned


/*---------------------- BLOB usage ------------------------*/
#define	flag_wake_next	(1<<8)	//0x100		/* bit8 */
#define	flag_first_poll	(1<<9)	//0x200		/* bit9, 1:for 1st into blob or new ac-in*/
#define	flag_min_curr	(1<<10)	//0x400		/* bit10 */
#define	flag_part_wake	(1<<11)	//0x800		/* bit11 */
#define	quick_off_on	(1<<12)	//0x1000	/* bit12, 0:temp, 1:imin */

#define	Reg_blob_ctrl	ASIC3_SLEEP_PARAM2
#define	Reg_RCL			ASIC3_SLEEP_PARAM3
#define	Reg_ini_volt	ASIC3_LEDASTC_CH_LED
#define	Reg_ini_temp	ASIC3_LEDASTC_NOTIFY_LED
#define	Reg_minc_temp	ASIC3_SLEEP_PARAM5
#define	Reg_1Read_DAC	ASIC3_SLEEP_PARAM4

/**********************/
/* I2C Bus definition */
/**********************/
#define	MICRON_DEVICE_ADDR	0x4A
/* IO CONTROL 00~1F */
#define	CMD_CF_3V_ON		0x01
/* READ INFORMATION */
#define	CMD_READ_EBAT		0x21
#define	CMD_READ_IMIN		0x22
#define	CMD_READ_TEMP		0x23
#define	CMD_READ_BATTID		0x24
#define	CMD_READ_STATUS		0x25
/* GET PIN STATUS */
#define	CMD_PIN_STATUS		0x41
#define	STUFF_DATA			0xd4 //0xff
#define	CF_3V_OPEN			0x01 //turn on
#define	CF_3V_CLOSE			0x00 //turn off


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
#define		num_8200	8200
#define		num_9100	9100
#define		num_2293	2293
#define		num_2500	2500
#define		num_10000	10000
#define		num_25000	25000
#define		num_3600	3600		/* 3600 * 1000 */
#define		num_4096	4096
#define		num_190000	190000
#define		num_810000	810000
#define		num_820000	820000
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
#define		MUX_CHL		6u		/* channel of MUX */
#define		BATT_CHL	2u		/* channel of BATTery */

#define		Temper_V47	0x449	/* 0.670v (47"C) */
#define		Temper_V50	0x3e7	/* 0.610v (50"C) */
#define		BattBad_V	0x614	/* 0.95v */
#define		ReCharge_V	0x68E	/* 4.1v  */
#define		Min_120		120		/* 120 minutes */
#define		hour_8		480		/* 60*8 minutes */



/* for Volume paramters */
const char Vol_S[] = {0, 2, 4, 6, 8, 10};
/* for Bright paramters */
const char Bri_S[] = {0, 25, 50, 75, 100};

const char BattPerCent[]={ 100,80,60,40,20,/*,10*/5,0 };

#define	Qty_PrCnt	sizeof(BattPerCent)/sizeof(char)


typedef	struct BattIndex
{
	unsigned short	batt_v;
	unsigned short	vimval[Qty_PrCnt];
} BATTINDEX, *PBATTINDEX;

#if 0
/*	Vim * (1000 factor)  */
const BATTINDEX BattIndexTab[]=
{
	{T_P10, { 4120, 3950, 3850, 3780, 3720, 3700, 3700 } },
	{T_P05, { 4110, 3930, 3830, 3760, 3710, 3680, 3680 } },
	{T_P00, { 4110, 3900, 3810, 3740, 3700, 3650, 3650 } },
	{T_N05, { 4080, 3890, 3800, 3720, 3670, 3630, 3630 } },
	{T_N10, { 4060, 3880, 3790, 3710, 3640, 3610, 3610 } },
	{T_NON, { 4050, 3870, 3780, 3700, 3630, 3600, 3600 } },
//	{T_NON, { 0000, 0000, 0000, 0000, 0000, 0000, 0000 } },
};
#endif

const BATTINDEX BattIndexTab[]=
{
	{T_P10, { 4120, 3950, 3850, 3780, 3720, 3650, 3650 } },
	{T_P05, { 4110, 3930, 3830, 3760, 3710, 3630, 3630 } },
	{T_P00, { 4110, 3900, 3810, 3740, 3700, 3600, 3600 } },
	{T_N05, { 4080, 3890, 3800, 3720, 3670, 3580, 3580 } },
	{T_N10, { 4060, 3880, 3790, 3710, 3640, 3570, 3570 } },
	{T_NON, { 4050, 3870, 3780, 3700, 3630, 3560, 3560 } },
//	{T_NON, { 0000, 0000, 0000, 0000, 0000, 0000, 0000 } },
};

typedef	struct DeltaVim
{
	unsigned short	deltavim[Qty_PrCnt];
} DELTAVIM, *PDELTAVIM;

#if 0
const DELTAVIM DeltavimTab[]=
{
	{{   80,  170,  100,   70,   60,   20,    20}},
	{{   80,  180,  100,   70,   50,   30,    30 }},
	{{   80,  110,   90,   70,   40,   50,    50 }},
	{{   80,  190,   90,   80,   50,   40,    40 }},
	{{   80,  160,   90,   80,   70,   30,    30 }},
	{{   80,  180,   90,   80,   70,   30,    30 }},
	{{    0,    0,    0,    0,    0,    0,     0 }}, //T_NON = -127
};

const DELTAVIM DeltavimTab[]=
{
	{{   80,  170,  100,   70,   60,   70,    20}},
	{{   80,  180,  100,   70,   50,   80,    30 }},
	{{   80,  210,   90,   70,   40,   100,    50 }},
	{{   80,  190,   90,   80,   50,   90,    40 }},
	{{   80,  180,   90,   80,   70,   70,    30 }},
	{{   80,  180,   90,   80,   70,   70,    30 }},
	{{    0,    0,    0,    0,    0,    0,     0 }}, //T_NON = -127
};
#endif

const DELTAVIM DeltavimTab[]=
{
	{{   320,  170,  100,   70,   60,   90,    20}},
	{{   320,  180,  100,   70,   50,   110,    30 }},
	{{   320,  210,   90,   70,   40,   130,    50 }},
	{{   320,  190,   90,   80,   50,   120,    40 }},
	{{   320,  180,   90,   80,   70,   90,    30 }},
	{{   320,  180,   90,   80,   70,   90,    30 }},
	{{    0,    0,    0,    0,    0,    0,     0 }}, //T_NON = -127
};

typedef	struct TemperTx
{
	unsigned short	batt_temp;
	short		Tx_val;
} TEMPERTX, *PTEMPERTX;

const TEMPERTX TemperTxTab[]=
{
	{ 294, 50},{ 330, 47},{ 356, 45},{ 767, 25},
	{1305, 10},{1348,  9},{1392,  8},{1436,  7},
	{1481,  6},{1526,  5},{1571,  4},{1618,  3},
	{1665,  2},{1712,  1},{1759,  0},{1806, -1},
	{1853, -2},{1900, -3},{1948, -4},{1994, -5},
	{2041, -6},{2087, -7},{2133, -8},{2178, -9},
	{2223,-10},{0000,-11},
};

typedef	struct STR_tv_tx	/* Temperature_Voltage, Tx */
{
	unsigned short	tv;
	short	tx;
} TV_TX, *PTV_TX;
#if 0
const TV_TX TV_TX_Tab[]=
{
	{1305, 10},{1348,  9},{1392,  8},{1436,  7},{1481,  6},{1526,  5},{1571,  4},{1618,  3},
	{1665,  2},{1712,  1},{1759,  0},{1806, -1},{1853, -2},{1900, -3},{1948, -4},{1994, -5},
	{2041, -6},{2087, -7},{2133, -8},{2178, -9},{2223,-10},{65535,-11},
};
#endif
#if 1
const TV_TX TV_TX_Tab[]=
{
	{1709, 10},{1738,  9},{1766,  8},{1794,  7},{1822,  6},{1849,  5},{1875,  4},{1901,  3},
	{1927,  2},{1951,  1},{1975,  0},{1999, -1},{2022, -2},{2044, -3},{2065, -4},{2086, -5},
	{2106, -6},{2126, -7},{2144, -8},{2162, -9},{2180,-10},{65535,-11},
};
#endif
#define	QTY_TV_TX	(sizeof(TV_TX_Tab)/sizeof(TV_TX))


typedef	struct Ebatt_Info
{
	unsigned short	command;
	unsigned short	info;
} EBATT_INFO, *PEBATT_INFO;

#if 0
const EBATT_INFO EBATT_INFO_TAB[]=
{
	{CMD_READ_EBAT,STUFF_DATA},
	{CMD_READ_IMIN,STUFF_DATA},
	{CMD_READ_TEMP,STUFF_DATA},
	{CMD_READ_BATTID,STUFF_DATA},
	{CMD_READ_STATUS,STUFF_DATA},
};
#else
EBATT_INFO EBATT_INFO_TAB[5];
#endif
#define	Qty_EBATT_CMD (sizeof(EBATT_INFO_TAB)/sizeof(EBATT_INFO))

