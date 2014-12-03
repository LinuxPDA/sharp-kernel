/*
 * linux/drivers/sound/tosa_wm9712.h
 *
 * 	(C) Copyright 2004 Lineo Solutions, Inc.
 *
 * 	Sound Driver for WM9712 codec device header file
 * 
 * Base on:
 * 	linux/drivers/sound/poodle_wm8731.h
 * 	
 * 	Copyright (C) 2002  SHARP
 *
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
#ifndef _WM9712_H_
#define _WM9712_H_

#define TRUE  1
#define FALSE 0

// Line input volume ctrl registers
#define LINEIN_MUTE_H			(1 << 15)	//Headphone
#define LINEIN_MUTE_S			(1 << 14)	//Speaker
#define LINEIN_MUTE_P			(1 << 13)	//Phone
#define LINEIN_MUTE_ALL			( LINEIN_MUTE_H | \
					  LINEIN_MUTE_S | \
					  LINEIN_MUTE_P )

// Headphone volume ctrl
#define HP_MUTE				(1 << 15)
#define HP_ZC_ENABLE			(1 << 7)

// Speaker volume ctrl
#define SP_MUTE				(1 << 15)
#define SP_SRC				(1 << 8)
#define SP_ZC_ENABLE			(1 << 7)
#define SP_INV				(1 << 6)

// Initial value
#define INIT_HP_VOL			(0)	// Initial Volume 0dB
#define INIT_SP_VOL			(0)	// Initial Volume 0dB
#define INIT_LINE_VOL			(8)	// Initial Volume 0dB
#define INIT_DAC_VOL			(8)	// Initial Volume 0dB
#define MAX_HP_VOL			(63)
#define MIN_HP_VOL			(0)
#define MAX_SP_VOL			(63)
#define MIN_SP_VOL			(0)
#define MAX_LINE_VOL			(31)
#define MIN_LINE_VOL			(0)
#define MAX_DAC_VOL			(31)
#define MIN_DAC_VOL			(0)

//DAC Volume control
#define DAC_MUTE_H			(1 << 15)	//Headphone
#define DAC_MUTE_S			(1 << 14)	//Speaker
#define DAC_MUTE_P			(1 << 13)	//Phone
#define DAC_MUTE_ALL			( DAC_MUTE_H | \
					  DAC_MUTE_S | \
					  DAC_MUTE_P )

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
#define MIC_MUTE			( (1 << 13) | (1 << 14) )
#define REC_SELECT_MASK			( (1 << 10) | (1 << 9) | (1 << 8) | \
					  (1 <<  2) | (1 << 1) | (1 << 0) )
#define REC_SELECT_LINEIN		( 4 )
#define MIC_BOOST			( 1 << 7 )

// Digital Audio path ctrl register  (0000101)
#define DPC_DAC_MUTE_ON			( 1U << 3 )
#define DPC_DAC_MUTE_OFF		( 0U )
#define DPC_DAC_MUTE_MASK		(~ ( 1U << 3 ) )
#define DEEMPHASIS_MODE(x)		( (( x ) & 3U ) << 1 )
#define DPC_DEEMPHASIS_MASK		(~ ( 3U << 1 ) )
#define DPC_ADC_HPF_ENABLE		( 1U )
#define DPC_ADC_HPF_DISABLE		( 0U )
#define DPC_INIT			( 0U )

// Power Consumption
#define NUM_OF_WM9712_DEV	( 4 )
#define WM9712_DEV_TS		( 0 )
#define WM9712_DEV_AUDIO	( 1 )
#define WM9712_DEV_AUDIOIN	( 2 )
#define WM9712_DEV_TMP		( 3 )

#define WM9712_PWR_OFF		( 0 )
#define WM9712_PWR_REC		( 1 )
#define WM9712_PWR_PLAY		( 2 )
#define WM9712_PWR_REC_HP	( 3 )
#define WM9712_PWR_PLAY_HP	( 4 )
#define WM9712_PWR_REC_MIC	( 5 )
#define WM9712_PWR_PLAY_MIC	( 6 )
#define WM9712_PWR_TP_WAIT	( 7 )
#define WM9712_PWR_TP_CONV	( 8 )
#define WM9712_PWR_ENMICBIAS	( 9 )
#define WM9712_PWR_FULL		( 10 )

extern void wm9712_power_mode(int, int);
#define wm9712_power_mode_ts(m)		wm9712_power_mode(WM9712_DEV_TS, m)
#define wm9712_power_mode_audio(m)	wm9712_power_mode(WM9712_DEV_AUDIO, m)
#define wm9712_power_mode_audioin(m)	wm9712_power_mode(WM9712_DEV_AUDIOIN, m)
#define wm9712_power_mode_tmp(m)	wm9712_power_mode(WM9712_DEV_TMP, m)
#ifdef CONFIG_PM
extern void lock_FCS_AC97(int, int);
#define lock_FCS_AC97_ts(m)	lock_FCS_AC97(WM9712_DEV_TS, m)
#define lock_FCS_AC97_audio(m)	lock_FCS_AC97(WM9712_DEV_AUDIO, m)
#endif	/* CONFIG_PM */

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

typedef struct {
  int left;
  int right;
} GAIN_SETTINGS;

typedef struct {
  unsigned short mode;
  GAIN_SETTINGS output;
  GAIN_SETTINGS input;
  int frequency;
} SOUND_SETTINGS;

// mode
#define SOUND_PLAY_MODE		(0x0001)
#define SOUND_REC_MODE		(0x0002)
#define SOUND_MODE_MASK		(0x0003)

// volume
enum {
  VOLUME_IGNORE = -2,
  VOLUME_MUTE = -1
};
#define MAX_VOLUME		MAX_HP_VOL
#define MIN_VOLUME		MIN_HP_VOL
#define MAX_INPUT_VOLUME	MAX_LINE_VOL
#define MIN_INPUT_VOLUME	MIN_LINE_VOL

int wm9712_busy(void);
int wm9712_init(void);
void wm9712_exit(void);
int wm9712_open(SOUND_SETTINGS *);
int wm9712_close(void);
int wm9712_set_freq(SOUND_SETTINGS *);
int wm9712_set_output_volume(int, int);
int wm9712_set_input_gain(int, int);
int wm9712_set_agc(int);
void wm9712_suspend(void);
void wm9712_resume(void);
void wm9712_update_jack_state(void);
void wm9712_checkjack_sleep(void);
void wm9712_checkjack_wakeup(void);

#endif	/* _WM9712_H_ */
