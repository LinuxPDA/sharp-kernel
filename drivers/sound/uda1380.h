/* uda1380.h : Audio driver head file
 *
 * Copyright (C) 2002 Steve Lin (stevelin168@hotmail.com)
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
 */

#ifndef __UDA_1380_H
#define __UDA_1380_H

// constant of pll of register 'mode'

#define UC_PLL_6K_12K			0		// 6.25K to 12.5K
#define UC_PLL_12K_25K			1		// 12.5K to 25K
#define UC_PLL_25K_50K			2		// 25K to 50K
#define UC_PLL_50K_100K			3		// 50K to 100K

/////////////////////////////////////////////////
// constant of sys_div of register 'mode'

#define UC_SYS_DIV_256FS		0
#define UC_SYS_DIV_384FS		1
#define UC_SYS_DIV_512FS		2
#define UC_SYS_DIV_768FS		3

/////////////////////////////////////////////////
// constant of output format of register 'iis_setting'

#define UC_OUT_FORMAT_IIS		0		// IIS
#define UC_OUT_FORMAT_LSB_16	1		// LSB 16 bits
#define UC_OUT_FORMAT_LSB_18	2		// LSB 18 bits
#define UC_OUT_FORMAT_LSB_20	3		// LSB 20 bits
#define UC_OUT_FORMAT_MSB		4		// MSB

/////////////////////////////////////////////////
// constant of input format of register 'iis_setting'

#define UC_IN_FORMAT_IIS		0		// IIS
#define UC_IN_FORMAT_LSB_16		1		// LSB 16 bits
#define UC_IN_FORMAT_LSB_18		2		// LSB 18 bits
#define UC_IN_FORMAT_LSB_20		3		// LSB 20 bits
#define UC_IN_FORMAT_LSB_24		4		// LSB 24 bits
#define UC_IN_FORMAT_MSB		5		// MSB

/////////////////////////////////////////////////
// constant of mode of register 'bass_treble' (BT means Bass,Treble)

#define UC_BT_MODE_FLAT			0		// this will make Bass,Treble disabled
#define UC_BT_MODE_MIN			1		// this will make Bass,Treble disabled
#define UC_BT_MODE_MAX			3		// this will make Bass,Treble disabled

/////////////////////////////////////////////////
// constant of de_ch1, de_ch2 of register 'mute'

#define UC_DEEMPHASIS_OFF		0
#define UC_DEEMPHASIS_32K		1
#define UC_DEEMPHASIS_44K		2
#define UC_DEEMPHASIS_48K		3
#define UC_DEEMPHASIS_96K		4

/////////////////////////////////////////////////
// constant of os of register 'misc'
#define UC_OVER_SAMPLE_SINGLE	0
#define UC_OVER_SAMPLE_DOUBLE	1
#define UC_OVER_SAMPLE_QUAD		2

/////////////////////////////////////////////////
// constant of sd_val of register 'misc'
#define UC_SILENCE_DETECT_3200		0
#define UC_SILENCE_DETECT_4800		1
#define UC_SILENCE_DETECT_9600		2
#define UC_SILENCE_DETECT_19200		3


//-------------------------------------------------------------------
struct _mode_s
{
			unsigned short	pll		: 2;		// input freqency range, referenced to const UC_PLL_xx
			unsigned short	sys_div	: 2;		// referenced to const UC_SYS_DIV_256FS, 384FS, 512FS, 768FS
			unsigned short	dac_clk	: 1;		// 0->base on SYSCLK (default), 1-> based on WSPLL
			unsigned short	adc_clk	: 1;		// 0->base on SYSCLK (default), 1-> based on WSPLL
			unsigned short			: 2;
			unsigned short	en_int	: 1;
			unsigned short	en_dac	: 1;
			unsigned short	en_dec	: 1;
			unsigned short	en_adc	: 1;
			unsigned short			: 1;
			unsigned short	ev		: 3;		// special control bits for manufactor, must be 0 here
}mode_s;

union _mode_u	
	{
		unsigned short	reg_val;				// value of the following struct
		struct  _mode_s	mode_s;
}mode_u;
		
typedef struct _U1380_MODE
{
	unsigned short	reg_num;					// to know which register number is
	union   _mode_u	mode_u;
} U1380_MODE;

//-------------------------------------------------------------------
struct _iis_s
{
			unsigned short	out_format		: 3;	// referenced to const UC_OUT_FORMAT_xx
			unsigned short					: 1;
			unsigned short	master_slave	: 1;	// 0->slave, 1-> master (default)
			unsigned short					: 1;
			unsigned short	source			: 1;	// 0-> decimator (default), 1-> mixer
			unsigned short					: 1;
			unsigned short	in_format		: 3;	// referenced to const UC_IN_FORMAT_xx
			unsigned short					: 5;
}iis_s;

union _iis_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_iis_s	iis_s;
}iis_u;	

typedef struct _U1380_IIS_SETTING
{
	unsigned short	reg_num;					// to know which register number is
	union	_iis_u	iis_u;
} U1380_IIS_SETTING;

//-------------------------------------------------------------------
struct	_power_s
{
			unsigned short	adc_r_on	: 1;	// ADC right
			unsigned short	pga_r_on	: 1;	// PGA right
			unsigned short	adc_l_on	: 1;	// ADC left
			unsigned short	pga_l_on	: 1;	// PGA left
			unsigned short	lna_on		: 1;
			unsigned short				: 1;
			unsigned short	avc_on		: 1;
			unsigned short	avc_en		: 1;
			unsigned short	bias_on		: 1;
			unsigned short				: 1;
			unsigned short	dac_on		: 1;
			unsigned short				: 2;
			unsigned short	hp_on		: 1;		// head phone
			unsigned short				: 1;
			unsigned short	pll_on		: 1;
}power_s;

union	_power_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_power_s	power_s;
}power_u;
		
typedef struct _U1380_PON_SETTING
{
	unsigned short	reg_num;					// to know which register number is
	union	_power_u	power_u;
} U1380_PON_SETTING;

//-------------------------------------------------------------------
struct	_analog_s
{
			unsigned short	left	: 5;
			unsigned short			: 3;
			unsigned short	right	: 5;
			unsigned short			: 3;
}analog_s;

union	_analog_u	
{
		unsigned short	reg_val;				// value of the following struct
		struct _analog_s	analog_s;	
}amalog_u;

typedef struct _U1380_ANALOG_SETTING
{
	unsigned short	reg_num;					// to know which register number is
	union	_analog_u	analog_u;
} U1380_ANALOG_SETTING;

//-------------------------------------------------------------------
struct	_reserved_s
{
			unsigned short	rsv0	: 3;		// must be 2 (see 1380 spec table 11)
			unsigned short			: 5;
			unsigned short	rsv1	: 3;		// must be 2 (see 1380 spec table 11)
			unsigned short			: 5;
}reserved_s;

union	_reserved_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_reserved_s	reserved_s;
}reserved_u;

typedef struct _U1380_RESERVED
{
	unsigned short	reg_num;					// to know which register number is
	union	_reserved_u	reserved_u;
} U1380_RESERVED;

//-------------------------------------------------------------------
struct	_master_vol_s
{
			unsigned short	left	: 8;
			unsigned short	right	: 8;
}master_vol_s;

union	_master_vol_u
{
		unsigned short	reg_val;				// value of the following struct
		struct 	_master_vol_s master_vol_s;
}master_vol_u;

typedef struct _U1380_MASTER_VOL
{
	unsigned short	reg_num;					// to know which register number is
	union	_master_vol_u	master_vol_u;
} U1380_MASTER_VOL;

//-------------------------------------------------------------------
struct	_mixer_vol_s
{
			unsigned short	channel1	: 8;
			unsigned short	channel2	: 8;
}mixer_vol_s;

union	_mixer_vol_u
{
		unsigned short	reg_val;				// value of the following struct
		struct 	_mixer_vol_s	mixer_vol_s;
}mixer_vol_u;
		
typedef struct _U1380_MIXER_VOL
{
	unsigned short	reg_num;					// to know which register number is
	union	_mixer_vol_u	mixer_vol_u;
} U1380_MIXER_VOL;

//-------------------------------------------------------------------
struct	_bass_treble_s
{
			unsigned short	bass_r		: 4;	// bass right
			unsigned short	treble_r	: 2;	// treble right
			unsigned short				: 2;
			unsigned short	bass_l		: 4;	// bass left
			unsigned short	treble_l	: 2;	// treble left
			unsigned short	mode		: 2;	// referenced to const UC_BT_MODE_xx
}bass_treble_s;

union	_bass_treble_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_bass_treble_s	bass_treble_s;
}_bass_treble_u;
			
typedef struct _U1380_BASS_TREBLE
{
	unsigned short	reg_num;					// to know which register number is
	union	_bass_treble_u	bass_treble_u;
} U1380_BASS_TREBLE;

//-------------------------------------------------------------------
struct	_mute_s
{
			unsigned short	de_ch1		: 3;	// deemphasis referenced to const UC_DEEMPHASIS_xx
			unsigned short	channel1	: 1;	// mute channel1
			unsigned short				: 4;
			unsigned short	de_ch2		: 3;	// deemphasis referenced to const UC_DEEMPHASIS_xx
			unsigned short	channel2	: 1;	// mute channel1
			unsigned short				: 2;
			unsigned short	all			: 1;	// mute 2 channels
			unsigned short				: 1;
}mute_s;

union	_mute_u
{
		unsigned short	reg_val;				// value of the following struct
		struct 	_mute_s	mute_s;
}mute_u;
		
typedef struct _U1380_MUTE
{
	unsigned short	reg_num;					// to know which register number is
	union	_mute_u		mute_u;
} U1380_MUTE;

//-------------------------------------------------------------------
struct	_misc_s
{
			unsigned short	os				: 2;	// referenced to const UC_OVER_SAMPLE_xx
			unsigned short					: 2;
			unsigned short	sd_val			: 2;	// referenced to const UC_SILENCE_DETECT_xx
			unsigned short	sd_on			: 1;	// silence detect on,off
			unsigned short	silence			: 1;	// force DAC output to silence
			unsigned short					: 4;
			unsigned short	mix				: 1;
			unsigned short	mix_pos			: 1;
			unsigned short	sel_ns			: 1;	// 0-> 3rd order, 1-> 5th order
			unsigned short	dac_polarity	: 1;
}misc_s;

union	_misc_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_misc_s		misc_s;
}misc_u;
	
typedef struct _U1380_MISC_SETTING
{
	unsigned short	reg_num;					// to know which register number is
	union	_misc_u	misc_u;
} U1380_MISC_SETTING;

//-------------------------------------------------------------------
struct	_decimator_vol_s
{
			unsigned short	right	: 8;
			unsigned short	left	: 8;
}decimator_vol_s;
	
union	_decimator_vol_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_decimator_vol_s	decimator_vol_s;
}decimator_vol_u;
	
typedef struct _U1380_DECIMATOR_VOL
{
	unsigned short	reg_num;					// to know which register number is
	union	_decimator_vol_u	decimator_vol_u;
} U1380_DECIMATOR_VOL;

//-------------------------------------------------------------------
struct	_pga_s
{
			unsigned short	gain_l	: 4;
			unsigned short			: 4;
			unsigned short	gain_r	: 4;
			unsigned short			: 3;
			unsigned short	mute	: 1;
}pga_s;
	
union	_pga_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_pga_s	pga_s;
}pga_u;
		
typedef struct _U1380_PGA_SETTING
{
	unsigned short	reg_num;					// to know which register number is
	union	_pga_u	pga_u;
} U1380_PGA_SETTING;

//-------------------------------------------------------------------
struct	_adc_s
{
			unsigned short	hp_en_dec		: 1;
			unsigned short	hp_skip_gain	: 1;
			unsigned short	sel_mic			: 1;
			unsigned short	sel_lna			: 1;
			unsigned short					: 4;
			unsigned short	vga_ctrl	: 4;
			unsigned short	adcpol_inv	: 1;
			unsigned short					: 3;
}adc_s;

union	_adc_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_adc_s	adc_s;
}adc_u;
	
typedef struct _U1380_ADC_SETTING
{
	unsigned short	reg_num;					// to know which register number is
	union	_adc_u	adc_u;
} U1380_ADC_SETTING;

//-------------------------------------------------------------------
struct	_agc_s
{
			unsigned short	enable	: 1;
			unsigned short			: 1;
			unsigned short	level	: 2;
			unsigned short			: 4;
			unsigned short	time	: 3;
			unsigned short			: 5;
}agc_s;

union	_agc_u
{
		unsigned short	reg_val;				// value of the following struct
		struct 	_agc_s	agc_s;
}agc_u;
	
typedef struct _U1380_AGC_SETTING
{
	unsigned short	reg_num;					// to know which register number is
	union	_agc_u	agc_u;
} U1380_AGC_SETTING;

//-------------------------------------------------------------------
typedef struct _U1380_RESET
{
	unsigned short	reg_num;					// to know which register number is
	unsigned short	reg_val;					// value of the following struct
} U1380_RESET;


//-------------------------------------------------------------------
struct	_interpolator_s
{
			unsigned short	mute_ch1		: 1;
			unsigned short	mute_ch2		: 1;
			unsigned short	mute			: 1;
			unsigned short	silence_ch1_l	: 1;	// silence detected
			unsigned short	silence_ch1_r	: 1;	// silence detected
			unsigned short	silence_ch2_l	: 1;	// silence detected
			unsigned short	silence_ch2_r	: 1;	// silence detected
			unsigned short					: 1;
			unsigned short	hp_detect_l		: 1;	// headphone driver short-circuit detection left
			unsigned short	hp_detect_r		: 1;	// headphone driver short-circuit detection right
			unsigned short	hp_detect_v		: 1;	// short-circuit detection virtual ampiler
			unsigned short					: 5;
}interpolator_s;
	
union	_interpolator_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_interpolator_s	interpolator_s;
}interpolator_u;	

typedef struct _U1380_INTERPOLATOR
{
	unsigned short	reg_num;					// to know which register number is
	union	_interpolator_u	interpolator_u;
} U1380_INTERPOLATOR;

//-------------------------------------------------------------------
struct	_decimator_s
{
			unsigned short	overflow	: 1;
			unsigned short				: 1;
			unsigned short	mute_adc	: 1;
			unsigned short				: 1;
			unsigned short	agc_state	: 1;
			unsigned short				: 11;
}decimator_s;

union	_decimator_u
{
		unsigned short	reg_val;				// value of the following struct
		struct	_decimator_s	decimator_s;
}decimator_u;
			
typedef struct _U1380_DECIMATOR
{
	unsigned short	reg_num;					// to know which register number is
	union	_decimator_u	decimator_u;
} U1380_DECIMATOR;


//===================================================================
typedef struct _UDA1380REGS
{
	// for write
	U1380_MODE				mode;
	U1380_IIS_SETTING		iis;
	U1380_PON_SETTING		power;
	U1380_ANALOG_SETTING	analog;
	U1380_RESERVED			reserved;		// reserved, but have to fill correct value
	U1380_MASTER_VOL		master_vol;
	U1380_MIXER_VOL			mixer_vol;
	U1380_BASS_TREBLE		bass_treble;
	U1380_MUTE				mute;
	U1380_MISC_SETTING		misc;
	U1380_DECIMATOR_VOL		decimator_vol;
	U1380_PGA_SETTING		pga;
	U1380_ADC_SETTING		adc;
	U1380_AGC_SETTING		agc;
	U1380_RESET				reset;

	// for read
	U1380_INTERPOLATOR	interpolator;
	U1380_DECIMATOR		decimator;
} UDA1380REGS;


#endif // __UDA_1380_H
