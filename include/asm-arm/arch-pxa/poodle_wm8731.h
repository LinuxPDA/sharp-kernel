/*
 * linux/drivers/sound/poodle_wm8731.h
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
#ifndef _WM8731_H_
#define _WM8731_H_


#define WM8731_REG_NUM  9

#define TRUE  1
#define FALSE 0

#define WM8731_DEV_ADR0 0x1a                // CSB = Low
#define WM8731_DEV_ADR1 0x1b                // CSB = High
#define WM8731_DEVICE   WM8731_DEV_ADR1

// Register address
#define WM8731_LeftLineIn_ADR		0x00
#define WM8731_RightLineIn_ADR		0x01
#define WM8731_LeftHdpOut_ADR		0x02
#define WM8731_RightHdpOut_ADR		0x03
#define WM8731_AnalogAudioPath_ADR	0x04
#define WM8731_DigitalAudioPath_ADR	0x05
#define WM8731_PowDownCtl_ADR		0x06
#define WM8731_DigitalIfFormat_ADR	0x07
#define WM8731_SamplingCtl_ADR		0x08
#define WM8731_ActiveCtl_ADR		0x09
#define WM8731_ResetReg_ADR		0x0f


// Volume ctrl registers ( Left Line in / Right Line in / Left HP out / Right HP out )
//  (0000000),  (0000001),  (0000010),  (0000011)
#define INPUT_MUTE_ON			( 1 << 7 )
#define INPUT_MUTE_OFF			( 0 )
#define ZERO_CROSS_UPDATE		( 1 << 7 )        
#define SIMUL_UPDATE			( 1 << 8 )

// Initial value
#define INIT_HP_VOL			(121)	// Initial Volume 0dB
#define INIT_LINE_VOL			(23)	// Initial Volume 0dB
#define MAX_HP_VOL			(127)
#define MIN_HP_VOL			(0)
#define MAX_LINE_VOL			(31)
#define MIN_LINE_VOL			(0)
#define INITIAL_PORT_DIR		(0x2705)
#define HP_VOL_MASK			(0x7f)
#define LINE_VOL_MASK			(0x1f)

// Analog Audio path ctrl register (0000100)
#define SIDETONE_ENABLE			(1<<5)
#define SIDETONE_DISABLE		( 0 )
#define ST_ATTN_6dB			( (0) << 6 )
#define ST_ATTN_9dB			( (1) << 6 )
#define ST_ATTN_12dB			( (2) << 6 )
#define ST_ATTN_15dB			( (3) << 6 )
#define DAC_OFF				( 0 )
#define DAC_SELECT			( 1 << 4 )
#define BYPASS_ENABLE			( 1 << 3 )
#define BYPASS_DISABLE			( 0 )
#define INPUT_MIC			( 1 << 2 )
#define INPUT_LINE			( 0 << 2 )
#define MIC_MUTE_ON			( 1 << 1 )
#define MIC_MUTE_OFF			( 0 )
#define MIC_BOOST_ON			( 1 )
#define MIC_BOOST_OFF			( 0 )

// Digital Audio path ctrl register  (0000101)
#define DPC_DAC_MUTE_ON			( 1U << 3 )
#define DPC_DAC_MUTE_OFF		( 0U )
#define DPC_DAC_MUTE_MASK		(~ ( 1U << 3 ) )
#define DEEMPHASIS_MODE(x)		( (( x ) & 3U ) << 1 )
#define DPC_DEEMPHASIS_MASK		(~ ( 3U << 1 ) )
#define DPC_ADC_HPF_ENABLE		( 1U )
#define DPC_ADC_HPF_DISABLE		( 0U )
#define DPC_INIT			( 0U )

// Power Down control register  (0000110)
#define PD_ALL				( 1U << 7 )
#define PD_CLK				( 1U << 6 )
#define PD_OSC				( 1U << 5 )
#define PD_OUT				( 1U << 4 )
#define PD_DAC				( 1U << 3 )
#define PD_ADC				( 1U << 2 )
#define PD_MIC				( 1U << 1 )
#define PD_LIN				( 1U << 0 )

#define PD_DEC_ACTIVE			( PD_OSC | PD_ADC | PD_MIC | PD_LIN )
#define PD_ENC_ACTIVE			( PD_OSC | PD_DAC | PD_MIC )
#define PD_SHTDWN			( PD_ALL )

// Digital Audio Interface Format register  (0000111)
#define SLAVE				( 0 )
#define MASTER				( 1U << 6 )
#define I2S_MODE			( 2U )
#define LEFT_JUSTIFIED			( 1U )
#define RIGHT_JUSTIFIED			( 0U )
#define IWL_16BIT			( 0 )
#define IWL_20BIT			( 1U << 2 )
#define IWL_24BIT			( 2U << 2 )
#define IWL_32BIT			( 3U << 2 )
#define LRSWAP_DISABLE			( 0U )
#define LRSWAP_ENABLE			( 1U << 5 )
#define LRP_RIGHT_HIGH			( 0U )
#define LRP_RIGHT_LOW			( 1U )

// Sampling ctrl register defines  (0001000)
#define SRC_CLKOUT_DIV			( 1 << 7 )
#define SRC_CLKIN_DIV			( 1 << 6 )
#define SRC_BOTH_44KHZ			( 8 << 2 )
#define SRC_BOTH_48KHZ			( 0 << 2 )
#define SRC_BOTH_8KHZ			( 3 << 2 )
#define SRC_OSR_256FS			( 0 )		// 12.288MHz/48kHz,11.2896MHz/44.1kHz
#define SRC_OSR_384FS			( 1 << 1 )	// 18.432MHz/48kHz,16.9344MHz/44.1kHz
#define SRC_OSR_250FS			( 0 )		// 12MHz/48 kHz
#define SRC_OSR_272FS			( 1 << 1 )	// 12MHz/44.1kHz
#define SRC_USB_MODE			( 1 )
#define SRC_NORMAL_MODE			( 0 )
#define DIGITAL_IF_ACTIVE		( 1 )		// digital interface active
#define DIGITAL_IF_INACTIVE		( 0 )

int wm8731_busy(void);
int wm8731_init(void);
int wm8731_open(SOUND_SETTINGS *);
int wm8731_close(void);
void wm8731_set_volume(int,int);
void wm8731_input_volume(int,int);
int wm8731_suspend(void);
int wm8731_setup(SOUND_SETTINGS *);

int wm8731_i2c_open(void);
int wm8731_i2c_write(unsigned char,unsigned short);
int wm8731_i2c_close(void);

int wm8731_set_hp_vol_shadowreg(int,int);
int wm8731_set_linein_vol_shadowreg(int,int);
int wm8731_set_mic_boost_shadowreg(int);
void wm8731_set_mic_gain_boost_reg(unsigned char);
void wm8731_set_volume_reg(int,unsigned char);
int wm8731_set_output_volume(int,int);
int wm8731_set_input_volume(int,int);
int wm8731_set_mic_gain_boost(int);
int wm8731_resume(void);

#endif
