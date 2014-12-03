/*
 * linux/include/asm-arm/arch-pxa/spitz_wm8750.h
 *
 *  Copyright (C) 2004 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/include/asm-arm/arch-pxa/poodle_wm8731.h
 *
 * Sound Driver for WM8731 codec device header file
 *
 *      
 * Copyright (C) 2002  SHARP
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
 * Change Log
 *
 */
#ifndef _WM8750_H_
#define _WM8750_H_

#define WM8750_REG_NUM		43

#define WM8750_PLAY_SET_NUM		15
#define WM8750_REC_SET_NUM		16

#define WM8750_REG_MASK1	0x8FFF3DAF
#define WM8750_REG_MASK2	0x7FF


#define TRUE  1
#define FALSE 0

//#define WM8750_DEV_ADR0 0x1a              // CSB = Low
#define WM8750_DEV_ADR1 0x1b                // CSB = High
#define WM8750_DEVICE   WM8750_DEV_ADR1
//#define WM8750_DEVICE   WM8750_DEV_ADR0

// Register address
#define WM8750_LeftInputVolume_ADR	0x00
#define WM8750_RightInputVolume_ADR	0x01
#define WM8750_LOUT1Volume_ADR		0x02
#define WM8750_ROUT1Volume_ADR		0x03

//#define WM8750_Reserved_ADR		0x04
#define WM8750_AdcDacCtl_ADR		0x05
//#define WM8750_Reserved_ADR		0x06
#define WM8750_AudioInterface_ADR	0x07

#define WM8750_SampleRate_ADR		0x08
//#define WM8750_Reserved_ADR		0x09
#define WM8750_LeftDACVolume_ADR	0x0a
#define WM8750_RightDACVolume_ADR	0x0b

#define WM8750_BassCtl_ADR		0x0c
#define	WM8750_TrebleCtl_ADR		0x0d
//#define WM8750_Reserved_ADR		0x0e
#define WM8750_ResetReg_ADR		0x0f

#define WM8750_3DCtl_ADR		0x10
#define	WM8750_ALC1_ADR			0x11
#define WM8750_ALC2_ADR			0x12
#define WM8750_ALC3_ADR			0x13

#define WM8750_NoiseGate_ADR		0x14
#define	WM8750_LeftADCVolume_ADR	0x15
#define WM8750_RightADCVolume_ADR	0x16
#define WM8750_AdditionalCtl1_ADR	0x17

#define WM8750_AdditionalCtl2_ADR	0x18
#define	WM8750_PwrMgmt1_ADR		0x19
#define WM8750_PwrMgmt2_ADR		0x1a
#define WM8750_AdditionalCtl3_ADR	0x1b

//#define WM8750_Reserved_ADR		0x1c
//#define WM8750_Reserved_ADR		0x1d
//#define WM8750_Reserved_ADR		0x1e
#define WM8750_ADCInputMode_ADR		0x1f

#define WM8750_ADCLSignalPath_ADR	0x20
#define	WM8750_ADCRSignalPath_ADR	0x21
#define WM8750_LeftOutMix1_ADR		0x22
#define WM8750_LeftOutMix2_ADR		0x23

#define WM8750_RightOutMix1_ADR		0x24
#define	WM8750_RightOutMix2_ADR		0x25
#define WM8750_MonoOutMix1_ADR		0x26
#define WM8750_MonoOutMix2_ADR		0x27

#define WM8750_LOUT2Volume_ADR		0x28
#define	WM8750_ROUT2Volume_ADR		0x29
#define	WM8750_NOMOOUTVolume_ADR	0x2a

// Volume ctrl register
#define INPUT_MUTE_ON			( 1 << 7 )
#define INPUT_MUTE_OFF			( 0 )
#define ZERO_CROSS_UPDATE		( 1 << 7 )        
#define SIMUL_UPDATE			( 1 << 8 )

// Initial value
#define INIT_HP_VOL				(121)	// Initial Volume 0dB
#define INIT_LINE_VOL			(33)	// Initial Volume 7.5dB
#define MAX_HP_VOL				(127)
#define MIN_HP_VOL				(0)
#define MAX_LINE_VOL			(63)
#define MIN_LINE_VOL			(0)
#define HP_VOL_MASK				(0x7f)
#define ALCL_MASK				(0xf)
#define INPUT_VOL_MASK			(0x3f)
#define LINE_VOL_MASK			(0x3f)

// Analog Audio path ctrl register
#define INPUT_MIC				( 3 << 6 )
#define INPUT_LINE				( 0 << 6 )
#define MIC_BOOST_ON			( 1 << 4)
#define MIC_BOOST_OFF			( 0 )
#define NGAT_ON					( 1 )

// Digital Audio path ctrl register
#define DPC_DAC_MUTE_ON			( 1U << 3 )
#define DPC_DAC_MUTE_OFF		( 0U )
#define DPC_DAC_MUTE_MASK		(~ ( 1U << 3 ) )
#define DEEMPHASIS_MODE(x)		( (( x ) & 3U ) << 1 )
#define DPC_DEEMPHASIS_MASK		(~ ( 3U << 1 ) )
#define DPC_ADC_HPF_ENABLE		( 1U )
#define DPC_ADC_HPF_DISABLE		( 0U )
#define DPC_INIT				( 0U )

// Digital Audio Interface Format register
#define SLAVE					( 0 )
#define MASTER					( 1U << 6 )
#define I2S_MODE				( 2U )
#define LEFT_JUSTIFIED			( 1U )
#define RIGHT_JUSTIFIED			( 0U )
#define IWL_16BIT				( 0 )
#define IWL_20BIT				( 1U << 2 )
#define IWL_24BIT				( 2U << 2 )
#define IWL_32BIT				( 3U << 2 )
#define LRSWAP_DISABLE			( 0U )
#define LRSWAP_ENABLE			( 1U << 5 )
#define LRP_RIGHT_HIGH			( 0U )
#define LRP_RIGHT_LOW			( 1U )

#define SRC_USB_MODE			( 1 )
#define DIGITAL_IF_ACTIVE		( 1 )		// digital interface active
#define DIGITAL_IF_INACTIVE		( 0 )


// LOUT/ROUT Volume register	(00010101 & 00010110)
#define	VOL_UPDATE				( 1U << 8 )

// ADC Input Mode register		(00010101 & 00010110)
#define	ADC_DS_LIN1				( 0U )
#define	ADC_DS_LIN2				( 1U << 8 )

// ADC Digital Volume register  (00010101 & 00010110)
#define	ADCVOL_UPDATE			( 1U << 8 )
#define	REC_ADCVOL				0xC3;	// 0db

// ADC Signal Path Control register  (00100000 & 00100001)
#define	INSEL_INPUT1			( 0U )
#define	INSEL_INPUT2			( 1U << 6 )
#define	INSEL_INPUT3			( 2U << 6 )
#define	INSEL_MIC				( 3U << 6 )
#define	INSEL_MASK			    ( 3U << 6 )
#define	MICBOOST_OFF			( 0U )
#define	MICBOOST_13dB			( 1U << 4 )
#define	MICBOOST_20dB			( 2U << 4 )
#define	MICBOOST_29dB			( 3U << 4 )
#define	MICBOOST_MASK			( 3U << 4 )

// Additional Control
#define EN_TSDEN			( 1U << 8 )
#define VSEL_33				( 3U << 6 )
#define EN_HPSWEN			( 1U << 6 )
#define EN_HPSWPOL			( 1U << 5 )
#define EN_ROUT2INV			( 1U << 4 )
#define EN_LRCM				( 1U << 2 )
#define EN_TRI				( 1U << 3 )
#define ADCOSR_64			( 1U << 1 )
#define DACOSR_64			( 1U  )

// Power Management
#define	SET_VMID(x)		( (( x ) & 3U ) << 7 )
#define VREF_PLAY			0x01
#define VREF_STOP			0x02
#define EN_VREF				( 1U << 6 )
#define EN_AINL				( 1U << 5 )
#define EN_AINR				( 1U << 4 )
#define EN_ADCL				( 1U << 3 )
#define EN_ADCR				( 1U << 2 )
#define EN_MICB				( 1U << 1 )
#define EN_DIGENB			( 1U )

#define EN_DACL				( 1U << 8 )
#define EN_DACR				( 1U << 7 )
#define EN_LOUT1			( 1U << 6 )
#define EN_ROUT1			( 1U << 5 )
#define EN_LOUT2			( 1U << 4 )
#define EN_ROUT2			( 1U << 3 )
#define EN_MONO				( 1U << 2 )
#define EN_OUT3				( 1U << 1)

// ALC Control
#define SEL_ALC				( 3U << 7)

// L-R Mixer Control
#define MIXSEL_MIC				( 3U )
#define	SEL_DAC2MIXER			( 1U << 8 )
#define	SEL_MUX2MIXER			( 1U << 7 )
#define	DEFAULT_MIXER_LEVEL		( 5U << 4 )

// Digital Mono Mix
#define ENABLE_BASSCTL			0x0080
#define DISABLE_BASSCTL			0x000f
#define ENABLE_TRBLCTL			0x0040
#define DISABLE_TRBLCTL			0x000f

int wm8750_busy(void);
int wm8750_init(void);
int wm8750_open(SOUND_SETTINGS *);
int wm8750_close(void);
void wm8750_set_volume(int,int);
void wm8750_input_volume(int,int);
int wm8750_suspend(void);
int wm8750_setup(SOUND_SETTINGS *);

int wm8750_i2c_open(void);
void wm8750_i2c_write(unsigned char,unsigned short);
void wm8750_i2c_close(void);

int wm8750_set_hp_vol_shadowreg(int,int);
int wm8750_set_linein_vol_shadowreg(int,int);
int wm8750_set_mic_boost_shadowreg(int,int,int,unsigned char *);
void wm8750_set_mic_gain_boost_reg(unsigned char);
void wm8750_set_volume_reg(int,unsigned char);
int wm8750_set_output_volume(int,int);
int wm8750_set_input_volume(int,int);
int wm8750_set_mic_gain_boost(int);
int wm8750_resume(void);

#endif
