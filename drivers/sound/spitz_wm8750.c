/*
 * linux/drivers/sound/spitz_wm8750.c
 *
 * wm8750 ctrl funstions for PXA (SHARP)
 *
 *		Copyright (C) 2004 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/drivers/sound/poodle_wm8731.c
 *		Copyright (C) 2002  SHARP
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
 *
*/
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/arch/hardware.h>
#include <asm/mach/arch.h>
#include <asm/arch/i2sc.h>

#include <asm/arch/poodle_audio.h>
#include <asm/arch/poodle_i2sc.h>
#include <asm/arch/spitz_wm8750.h>

#include <asm/sharp_apm.h>
#include <asm/sharp_char.h>

/*** Some declarations ***********************************************/
#define MY_TRACE	0

#define DBG_ALWAYS	0
#define DBG_LEVEL1	1
#define DBG_LEVEL2	2
#define DBG_LEVEL3	3

#ifdef MY_TRACE
#define WM8750_DBGPRINT(level, x, args...)  if ( level <= MY_TRACE ) printk("%s: " x,__FUNCTION__ ,##args)
#else
#define WM8750_DBGPRINT(level, x, args...)  if ( !level ) printk("%s: " x,__FUNCTION__ ,##args)
#endif

#define OPEN_WAIT_CNT		10	// wwait time for i2c open.
#define WM8750_I2C_ADDRESS	(unsigned char)0x01

//#define USE_TREBLE_BOOST

/*** Some valiables **************************************************/

typedef struct {
  int		used;		//used: open(1) close(0)
  int		mode;		//Sound mode
  int		agc;		//Agc: on(1) off(0)
} wm8750_stat_t;

static wm8750_stat_t wm8750_stat;

unsigned short WM8750Regs[WM8750_REG_NUM];	// Except ResetReg

extern int sharp_HPstatus;

unsigned short wm8750_def_table[WM8750_REG_NUM] = {
    0x0097, // 00h: Left Input Volume: WM8750_LeftInputVolume_ADR
    0x0097, // 01h: Right Input Volume: WM8750_RightInputVolume_ADR
    0x0079, // 02h: LOUT1 volume: WM8750_LOUT1Volume_ADR
    0x0079, // 03h: ROUT1 volume: WM8750_ROUT1Volume_ADR
    0x0000, // 04h:x (Reserved): 
    0x0008, // 05h: ADC&DAC Control: WM8750_AdcDacCtl_ADR
    0x0000, // 06h:x (Reserved): 
    0x000a, // 07h: Audio Interface: WM8750_AudioInterface_ADR
    0x0000, // 08h: Sample rate: WM8750_SampleRate_ADR
    0x0000, // 09h:x (Reserved): 
    0x00ff, // 0Ah: Left DAC volume: WM8750_LeftDACVolume_ADR
    0x00ff, // 0Bh: Right DAC volume: WM8750_RightDACVolume_ADR
    0x000f, // 0Ch: Bass control: WM8750_BassCtl_ADR
    0x000f, // 0Dh: Treble control: WM8750_TrebleCtl_ADR
    0x0000, // 0Eh:x (Reserved): 
    0x0000, // 0Fh:x Reset: WM8750_ResetReg_ADR
    0x0000, // 10h: 3D control: WM8750_3DCtl_ADR
    0x007b, // 11h: ALC1: WM8750_ALC1_ADR
    0x0000, // 12h: ALC2: WM8750_ALC2_ADR
    0x0032, // 13h: ALC3: WM8750_ALC3_ADR
    0x0000, // 14h: Noise Gate: WM8750_NoiseGate_ADR
    0x00c3, // 15h: Left ADC volume: WM8750_LeftADCVolume_ADR
    0x00c3, // 16h: Right ADC volume: WM8750_RightADCVolume_ADR
    0x00c0, // 17h: Additional control1: WM8750_AdditionalCtl1_ADR
    0x0000, // 18h: Additional control2: WM8750_AdditionalCtl2_ADR
    0x0000, // 19h: Pwr Mgmt1: WM8750_PwrMgmt1_ADR
    0x0000, // 1Ah: Pwr Mgmt2: WM8750_PwrMgmt2_ADR
    0x0000, // 1Bh: Additional Control3: WM8750_AdditionalCtl3_ADR
    0x0000, // 1Ch:x (Reserved): 
    0x0000, // 1Dh:x (Reserved): 
    0x0000, // 1Eh:x (Reserved): 
    0x0000, // 1Fh: ADC input mode: WM8750_ADCInputMode_ADR
    0x0000, // 20h: ADCL signal path: WM8750_ADCLSignalPath_ADR
    0x0000, // 21h: ADCR signal path: WM8750_ADCRSignalPath_ADR
    0x0050, // 22h: Left out Mix1: WM8750_LeftOutMix1_ADR
    0x0050, // 23h: Left out Mix2: WM8750_LeftOutMix2_ADR
    0x0050, // 24h: Right out Mix1: WM8750_RightOutMix1_ADR
    0x0050, // 25h: Right out Mix2: WM8750_RightOutMix2_ADR
    0x0050, // 26h: Mono out Mix1: WM8750_MonoOutMix1_ADR
    0x0050, // 27h: Mono out Mix2: WM8750_MonoOutMix2_ADR
    0x0079, // 28h: LOUT2 volume: WM8750_LOUT2Volume_ADR
    0x0079, // 29h: ROUT2 volume: WM8750_ROUT2Volume_ADR
    0x0079  // 2Ah: MONOOUT volume: WM8750_NOMOOUTVolume_ADR
};

unsigned int wm8750_volume_headphone[101] = { // for HeadPhone
#if 1
  0,66,66,67,67,68,69,69,70,71,
  71,72,72,73,74,74,75,76,76,77,
  77,78,79,79,80,81,81,82,82,83,
  84,84,85,85,86,87,87,88,89,89,
  90,90,91,92,92,93,94,94,95,95,
  96,97,97,98,98,99,100,100,101,102,
  102,103,103,104,105,105,106,107,107,108,
  108,109,110,110,111,112,112,113,113,114,
  115,115,116,116,117,118,118,119,120,120,
  121,121,122,123,123,124,125,125,126,126,
  127
#else // bottom is too small
  0,48,49,49,50,51,52,53,53,54,
  55,56,57,57,58,59,60,61,61,62,
  63,64,65,65,66,67,68,69,69,70,
  71,72,73,73,74,75,76,77,77,78,
  79,80,81,81,82,83,84,85,85,86,
  87,88,89,89,90,91,92,93,93,94,
  95,96,97,97,98,99,100,101,101,102,
  103,104,105,105,106,107,108,109,109,110,
  111,112,113,113,114,115,116,117,117,118,
  119,120,121,121,122,123,124,125,125,126,
  127
#endif
};

unsigned int wm8750_volume_speaker[101] = { // for Speaker
  0,87,88,88,89,89,89,90,90,91,
  91,91,92,92,93,93,93,94,94,95,
  95,95,96,96,97,97,97,98,98,99,
  99,99,100,100,101,101,101,102,102,103,
  103,103,104,104,105,105,105,106,106,107,
  107,107,108,108,109,109,109,110,110,111,
  111,111,112,112,113,113,113,114,114,115,
  115,115,116,116,117,117,117,118,118,119,
  119,119,120,120,121,121,121,122,122,123,
  123,123,124,124,125,125,125,126,126,127,
  127
};

unsigned int wm8750_input_GAIN[101][3] = {
  {  0, 0, 0 },{  4, 0, 0 },{  5, 0, 0 },{  6, 0, 0 },{  7, 0, 1 }, // 0 - 9
  {  8, 0, 1 },{  9, 0, 1 },{ 10, 0, 1 },{ 11, 0, 1 },{ 12, 0, 1 },
  { 13, 0, 2 },{ 14, 0, 2 },{ 15, 0, 2 },{ 16, 0, 2 },{ 17, 0, 2 }, // 10 - 19
  {  0, 1, 2 },{  1, 1, 2 },{  2, 1, 3 },{  3, 1, 3 },{  4, 1, 3 },
  {  5, 1, 3 },{  6, 1, 3 },{  7, 1, 3 },{  8, 1, 3 },{  9, 1, 4 }, // 20 - 29
  {  0, 2, 4 },{  1, 2, 4 },{  2, 2, 4 },{  3, 2, 4 },{  4, 2, 4 },
  {  5, 2, 5 },{  6, 2, 5 },{  7, 2, 5 },{  8, 2, 5 },{  9, 2, 5 }, // 30 - 39
  { 10, 2, 5 },{ 11, 2, 5 },{  0, 3, 6 },{  1, 3, 6 },{  2, 3, 6 },
  {  3, 3, 6 },{  4, 3, 6 },{  5, 3, 6 },{  6, 3, 6 },{  7, 3, 7 }, // 40 - 49
  {  8, 3, 7 },{  9, 3, 7 },{ 10, 3, 7 },{ 11, 3, 7 },{ 12, 3, 7 },
  { 13, 3, 8 },{ 14, 3, 8 },{ 15, 3, 8 },{ 16, 3, 8 },{ 17, 3, 8 }, // 50 - 59
  { 18, 3, 8 },{ 19, 3, 8 },{ 20, 3, 9 },{ 21, 3, 9 },{ 22, 3, 9 },
  { 23, 3, 9 },{ 24, 3, 9 },{ 25, 3, 9 },{ 26, 3, 9 },{ 27, 3,10 }, // 60 - 69
  { 28, 3,10 },{ 29, 3,10 },{ 30, 3,10 },{ 31, 3,10 },{ 32, 3,10 },
  { 33, 3,11 },{ 34, 3,11 },{ 35, 3,11 },{ 36, 3,11 },{ 37, 3,11 }, // 70 - 79
  { 38, 3,11 },{ 39, 3,11 },{ 40, 3,12 },{ 41, 3,12 },{ 42, 3,12 },
  { 43, 3,12 },{ 44, 3,12 },{ 45, 3,12 },{ 46, 3,12 },{ 47, 3,13 }, // 80 - 89
  { 48, 3,13 },{ 49, 3,13 },{ 50, 3,13 },{ 51, 3,13 },{ 52, 3,13 },
  { 53, 3,14 },{ 54, 3,14 },{ 55, 3,14 },{ 56, 3,14 },{ 57, 3,14 }, // 90 - 99
  { 58, 3,14 },{ 59, 3,14 },{ 60, 3,15 },{ 61, 3,15 },{ 62, 3,15 },
  { 63, 3,15 }
};	


#ifdef CONFIG_PM
#define ADEV_MAIN	0
#define ADEV_HP		1
#define ADEV_VOL	2
#define ADEV_OFFON	3
#define ADEV_INIT	4
static int lockState	= 0;
static void audioLockFCS( int dev, int bLock )
{
  int curLockState = lockState;
  int now,last;
  if (bLock) {
    curLockState |= 0x1<<dev;
  } else {
    curLockState &= ~(0x1<<dev);
  }
  if (curLockState!=lockState) {
    now = (curLockState!=0)?TRUE:FALSE;
    last = (lockState!=0)?TRUE:FALSE;
    if ( now != last ) {
      lock_FCS(LOCK_FCS_SOUND, (curLockState!=0)?TRUE:FALSE);
    }
    lockState = curLockState;
  }
}
#endif // end CONFIG_PM

/*** Config & Setup ******************************************************/
int wm8750_busy(void)
{
  return wm8750_stat.used;
}

int wm8750_init(void)
{
	int i;
	int err = 0;

	wm8750_stat.used = FALSE;
	wm8750_stat.agc = 0;

	for(i=0;i<WM8750_REG_NUM;i++)
		if (((i < 32) && ((1 << i) & WM8750_REG_MASK1)) ||
	    	((i >= 32) && ((1 << (i - 32)) & WM8750_REG_MASK2))){
			WM8750Regs[i] = wm8750_def_table[i];
	}

	err = wm8750_i2c_open();
	if ( err ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
		return err;
	}
	wm8750_i2c_write(WM8750_ResetReg_ADR, 0);					// reset WM8750
	mdelay(1);

	wm8750_i2c_write(WM8750_PwrMgmt1_ADR, 
				( SET_VMID(VREF_STOP) | EN_VREF ));
	mdelay(1);
	wm8750_i2c_write(WM8750_PwrMgmt2_ADR, 
		( EN_DACL | EN_DACR ));
	wm8750_i2c_write(WM8750_PwrMgmt1_ADR, 
		( SET_VMID(VREF_STOP) /* | EN_AINL | EN_AINR */ | EN_VREF ));
	WM8750Regs[WM8750_PwrMgmt1_ADR] =
		( SET_VMID(VREF_STOP) /* | EN_AINL | EN_AINR */ | EN_VREF );

	wm8750_i2c_write(WM8750_PwrMgmt2_ADR, 
		( EN_DACL | EN_DACR | EN_LOUT1 | EN_ROUT1 ));
	WM8750Regs[WM8750_PwrMgmt2_ADR] =
		( EN_DACL | EN_DACR | EN_LOUT1 | EN_ROUT1 );
	wm8750_i2c_write(WM8750_PwrMgmt2_ADR, 
		( EN_DACL | EN_DACR | EN_LOUT1 | EN_ROUT1 | EN_LOUT2 | EN_ROUT2 ));
	WM8750Regs[WM8750_PwrMgmt2_ADR] =
		( EN_DACL | EN_DACR | EN_LOUT1 | EN_ROUT1 | EN_LOUT2 | EN_ROUT2 );
	wm8750_i2c_close();

	return 0;
}

/*** I2C ctrl ************************************************************/
static int wm8750_i2c_open(void)
{
	int err = 0,cnt = 0;
	int start_time;

	while(1) {
    	err = i2c_open(WM8750_I2C_ADDRESS);
		if ( !err ) break;
		start_time = jiffies;
		while (1) {
			schedule();
			if ( start_time - jiffies > 2 ) break;
    	}
		cnt++;
		if ( cnt > OPEN_WAIT_CNT ) {
			return err;
		}
	}
	return err;
}

static void wm8750_i2c_write(unsigned char oreg, unsigned short odata)
{
	unsigned char adr,data;

	adr = ( (oreg << 1) | ( (odata >> 8) & 1 ) );
	data = odata & 0xff;
	if ( i2c_byte_write( WM8750_DEVICE , adr, data ) ) {
		WM8750_DBGPRINT( DBG_LEVEL1 , "i2c write error, retry !");
		if ( i2c_byte_write( WM8750_DEVICE , adr, data ) ) {
			WM8750_DBGPRINT( DBG_LEVEL1 , "error");
		} else {
			WM8750_DBGPRINT( DBG_LEVEL1 , "OK");
		}
	}
}

static void wm8750_i2c_close(void)
{
	i2c_close(WM8750_I2C_ADDRESS);
}


/*** Open & Close ******************************************************/

int wm8750_open(SOUND_SETTINGS *settings)
{
  int i, err = 0;


  WM8750_DBGPRINT(DBG_LEVEL2, "wm8750 open\n");

  if ( wm8750_stat.used ) {
    WM8750_DBGPRINT(DBG_ALWAYS, "wm8750 device is busy now !\n");
    return -EBUSY;
  }

  audioLockFCS(ADEV_MAIN, 1);

  wm8750_stat.used = TRUE;

  err = wm8750_i2c_open();
  if ( err ) {
    WM8750_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }

  if ( wm8750_setup(settings) ) {				// set to shadow regs.
    wm8750_i2c_close();
    audioLockFCS(ADEV_MAIN, 0);
    wm8750_stat.used = FALSE;
    return 1;
  }

  for(i=0;i<WM8750_REG_NUM;i++) {
	if (((i < 32) && ((1 << i) & WM8750_REG_MASK1)) ||
	    ((i >= 32) && ((1 << (i - 32)) & WM8750_REG_MASK2)))
      wm8750_i2c_write(i, WM8750Regs[i]);
  }


  wm8750_i2c_close();

  return 0;
}

int wm8750_close(void)
{
  int err;

  WM8750_DBGPRINT(DBG_LEVEL2 ,"wm8750 close\n");

  if ( wm8750_stat.used ) {
    int err = wm8750_i2c_open();
    if ( err ) {
      WM8750_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    } else {
      WM8750_set_DAC_MUTE(1);
      wm8750_i2c_write(WM8750_PwrMgmt1_ADR,SET_VMID(VREF_STOP)|EN_VREF);
      wm8750_i2c_write(WM8750_PwrMgmt2_ADR,0);
      
      wm8750_i2c_close();
    }
    wm8750_stat.used = FALSE;
  }

  audioLockFCS(ADEV_MAIN, 0);

  return 0;
}

/*** Setup ***********************************************************/
static const unsigned short CODEC_SETTINGS_PLAY[WM8750_PLAY_SET_NUM][2] = {
  { WM8750_LeftInputVolume_ADR ,(INPUT_MUTE_ON | INIT_LINE_VOL) },
  { WM8750_RightInputVolume_ADR ,(INPUT_MUTE_ON | INIT_LINE_VOL) },
  { WM8750_LOUT1Volume_ADR ,(INIT_HP_VOL | VOL_UPDATE) },
  { WM8750_ROUT1Volume_ADR ,(INIT_HP_VOL | VOL_UPDATE) },
  { WM8750_AdcDacCtl_ADR ,
	(DPC_DAC_MUTE_OFF | DEEMPHASIS_MODE(0) | DPC_ADC_HPF_DISABLE ) },
  { WM8750_AudioInterface_ADR ,
	(SLAVE | LRSWAP_DISABLE | LRP_RIGHT_HIGH | IWL_16BIT | I2S_MODE) },
  { WM8750_AdditionalCtl1_ADR, 
	( EN_TSDEN | VSEL_33 ) },
  { WM8750_AdditionalCtl2_ADR, 
    	( EN_ROUT2INV | DACOSR_64) },
  { WM8750_PwrMgmt1_ADR, 
	( SET_VMID(VREF_PLAY) | EN_AINL | EN_AINR | EN_VREF ) },
  { WM8750_PwrMgmt2_ADR, 
    	( EN_DACL | EN_DACR | EN_LOUT1 | EN_ROUT1 | EN_LOUT2 | EN_ROUT2) },
  { WM8750_LeftOutMix1_ADR ,
	(SEL_DAC2MIXER | DEFAULT_MIXER_LEVEL) },
  { WM8750_LeftOutMix2_ADR ,
	( DEFAULT_MIXER_LEVEL) },
  { WM8750_RightOutMix1_ADR ,
	( DEFAULT_MIXER_LEVEL) },
  { WM8750_RightOutMix2_ADR ,
	(SEL_DAC2MIXER | DEFAULT_MIXER_LEVEL) }
};

static const unsigned short CODEC_SETTINGS_REC[WM8750_REC_SET_NUM][2] = {
  { WM8750_LeftInputVolume_ADR ,( INIT_LINE_VOL | VOL_UPDATE) },
  { WM8750_RightInputVolume_ADR ,( INIT_LINE_VOL | VOL_UPDATE) },
  { WM8750_LOUT1Volume_ADR ,(INIT_HP_VOL | VOL_UPDATE) },
  { WM8750_ROUT1Volume_ADR ,(INIT_HP_VOL | VOL_UPDATE) },
  { WM8750_AdcDacCtl_ADR ,
	(DPC_DAC_MUTE_OFF | DEEMPHASIS_MODE(0) | DPC_ADC_HPF_DISABLE ) },
  { WM8750_AudioInterface_ADR ,
	(SLAVE | LRSWAP_DISABLE | LRP_RIGHT_HIGH | IWL_16BIT | I2S_MODE) },
  { WM8750_AdditionalCtl1_ADR, 
	( EN_TSDEN | VSEL_33 ) },
  { WM8750_AdditionalCtl2_ADR, 
	( EN_ROUT2INV ) },
  { WM8750_PwrMgmt1_ADR, 
	( SET_VMID(VREF_PLAY) | EN_AINL | EN_AINR |
		EN_ADCL | EN_ADCR | EN_MICB | EN_VREF ) },
  { WM8750_PwrMgmt2_ADR, 
	( EN_DACL | EN_DACR | EN_ROUT1 | EN_ROUT2 ) },
  { WM8750_ADCInputMode_ADR ,
	( ADC_DS_LIN1 ) },
  { WM8750_LeftOutMix1_ADR ,
	( DEFAULT_MIXER_LEVEL ) },
  { WM8750_LeftOutMix2_ADR ,
	( DEFAULT_MIXER_LEVEL ) },
  { WM8750_RightOutMix1_ADR ,
	( SEL_DAC2MIXER | DEFAULT_MIXER_LEVEL ) },
  { WM8750_RightOutMix2_ADR ,
	( SEL_DAC2MIXER | DEFAULT_MIXER_LEVEL ) }
};

static int wm8750_setup(SOUND_SETTINGS  *setting)
{
	int i, ret = 0;

	if( setting == NULL ) return -1;

	if ( !wm8750_stat.used ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "not opend !\n");
		return -EINVAL;
	}

	wm8750_stat.mode = setting->mode&SOUND_MODE_MASK;
	switch( wm8750_stat.mode )
	{
	case SOUND_PLAY_MODE:
		for(i=0;i<WM8750_PLAY_SET_NUM;i++)
			WM8750Regs[CODEC_SETTINGS_PLAY[i][0]] = CODEC_SETTINGS_PLAY[i][1];
			break;

	case SOUND_REC_MODE:
		{
			for(i=0;i<WM8750_REC_SET_NUM;i++)
			  WM8750Regs[CODEC_SETTINGS_REC[i][0]] = CODEC_SETTINGS_REC[i][1];

			WM8750Regs[WM8750_ADCLSignalPath_ADR] =
							INSEL_MIC | MICBOOST_20dB;
			WM8750Regs[WM8750_ADCRSignalPath_ADR] =
							INSEL_MIC | MICBOOST_20dB;
			break;
		}

		default:
			return -1;
	}

	wm8750_set_hp_vol_shadowreg(WM8750_LOUT1Volume_ADR,setting->output.left);	
	wm8750_set_hp_vol_shadowreg(WM8750_ROUT1Volume_ADR,setting->output.right);

	wm8750_set_linein_vol_shadowreg(WM8750_LeftInputVolume_ADR, setting->input.left);
	wm8750_set_linein_vol_shadowreg(WM8750_RightInputVolume_ADR, setting->input.right);
	return ret;
}


/*** Volume ***********************************************************/
static int wm8750_set_hp_vol_shadowreg(int adr, int vol)
{
	unsigned short reg,volume;
	int state;

	if ( vol == VOLUME_MUTE ) {
		vol = 0;
	}

	if (vol<0) vol=0;
	else if (vol>100) vol=100;

	state = sharp_HPstatus;

	if ( state == HPJACK_STATE_NONE ) {
		vol = wm8750_volume_speaker[ vol ];
	} else {
		vol = wm8750_volume_headphone[ vol ];
	}

	if( ( vol >= MIN_HP_VOL ) && ( vol <= MAX_HP_VOL ) ) {
    			// set volume to shadow regs.
		reg = WM8750Regs[adr];
		volume = reg & HP_VOL_MASK;
		if (volume != vol) {
			reg = ( reg & ~HP_VOL_MASK ) | vol;
			WM8750Regs[adr] = reg;
			return 1;
		}
	}else{
    			// ignore
	}
  return 0;
}

// for Input
// update shadow regs.
static int wm8750_set_linein_vol_shadowreg(int adr, int vol)
{
	unsigned short reg,volume;

	if( vol == VOLUME_MUTE ) {
    	// mute
		reg = WM8750Regs[adr];
		if( !( reg & INPUT_MUTE_ON ) ) {
			reg |= INPUT_MUTE_ON;
			WM8750Regs[adr] = reg;
			return 1;
		}
	}else if ( ( vol >= MIN_LINE_VOL ) && ( vol <= MAX_LINE_VOL ) ) {
                // set volume
		reg = WM8750Regs[adr];
		volume = reg & LINE_VOL_MASK;
		if ( ( reg & INPUT_MUTE_ON ) || ( volume != vol ) ) {
			reg = ( reg & ~( INPUT_MUTE_ON | LINE_VOL_MASK ) ) | vol;
			WM8750Regs[adr] = reg;
			return 1;
		}
	}else {
     			// ignore
	}
	return 0;
}

// for MIC
// update shadow regs.
static int wm8750_set_mic_boost_shadowreg(int boost,int gain,int alcl,unsigned char *modify)
{
	unsigned short reg_l,reg_r;
	int ret = 0;
	reg_l = WM8750Regs[WM8750_ADCLSignalPath_ADR];
	reg_r = WM8750Regs[WM8750_ADCRSignalPath_ADR];

	if ( ((reg_l & MICBOOST_MASK) >> 4) != boost ) {
		WM8750Regs[WM8750_ADCLSignalPath_ADR] = INSEL_MIC | (boost << 4);
		*modify |= 0x01;
		ret = 1;			// set to on
	}
	if ( ((reg_r & MICBOOST_MASK) >> 4) != boost ) {
		WM8750Regs[WM8750_ADCRSignalPath_ADR] = INSEL_MIC | (boost << 4);
		*modify |= 0x02;
		ret = 1;			// set to on
	}

	reg_l = WM8750Regs[WM8750_LeftInputVolume_ADR];
	reg_r = WM8750Regs[WM8750_RightInputVolume_ADR];

	if ( (reg_l & INPUT_VOL_MASK) != gain ) {
		WM8750Regs[WM8750_LeftInputVolume_ADR] = (reg_l & ~INPUT_VOL_MASK) | gain | VOL_UPDATE;
		*modify |= 0x04;
		ret = 1;			// set to on
	}
	if ( (reg_r & INPUT_VOL_MASK) != gain ) {
		WM8750Regs[WM8750_RightInputVolume_ADR] = (reg_r & ~INPUT_VOL_MASK) | gain | VOL_UPDATE;
		*modify |= 0x08;
		ret = 1;			// set to on
    }

	reg_l = WM8750Regs[WM8750_ALC1_ADR];

	if ( (reg_l & ALCL_MASK) != alcl ) {
		WM8750Regs[WM8750_ALC1_ADR] = (reg_l & ~ALCL_MASK) | alcl;
		*modify |= 0x10;
		ret = 1;			// set to on
	}
    return ret;
}

/**********************************/
// for Output/Input
// update wm8750 regs.
static void wm8750_set_volume_reg(int adr, unsigned char modify)
{
  int i;
  unsigned char mask;

  mask = 0x01;

  for( i=0;i<2;i++ ) {
    if( modify & mask ) {
      wm8750_i2c_write( adr+i , WM8750Regs[adr+i] | VOL_UPDATE);
    }
    mask <<= 1;
  }
  return;

}


// update wm8750 regs.
void wm8750_set_mic_gain_boost_reg(unsigned char modify)
{
	int i;
	int adr;
	unsigned char mask;
	mask = 0x01;

	adr = WM8750_ADCLSignalPath_ADR;
	for( i=0;i<2;i++ ) {
		if( modify & mask ) {
			wm8750_i2c_write( adr+i , WM8750Regs[adr+i]);
		}
		mask <<= 1;
	}

	adr = WM8750_LeftInputVolume_ADR;
	for( i=0;i<2;i++ ) {
		if( modify & mask ) {
			wm8750_i2c_write( adr+i , WM8750Regs[adr+i] | VOL_UPDATE);
		}
		mask <<= 1;
	}

	adr = WM8750_ALC1_ADR;
	if( modify & mask ) {
		wm8750_i2c_write( adr , WM8750Regs[adr]);
	}

	return;
}


/**********************************/
int wm8750_set_output_volume(int left, int right)
{
	unsigned char modify;
	int err;
	int state;

	unsigned short set_vol_addr_L = WM8750_LOUT1Volume_ADR;
	unsigned short set_vol_addr_R = WM8750_ROUT1Volume_ADR;

	state = sharp_HPstatus;

	if ( !wm8750_stat.used ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "not opened !\n");
		return -EINVAL;
	}

	err = wm8750_i2c_open();
	if ( err ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
		return err;
	}

	modify = 0;

	if ( state ) { 	// Insert HP
		set_vol_addr_L = WM8750_LOUT1Volume_ADR;
		set_vol_addr_R = WM8750_ROUT1Volume_ADR;
	}else{
		set_vol_addr_L = WM8750_LOUT2Volume_ADR;
		set_vol_addr_R = WM8750_ROUT2Volume_ADR;
	}
  // Left volume
	if ( wm8750_set_hp_vol_shadowreg(set_vol_addr_L, left) != 0 ) {
		modify |= 0x01;
	}
  // Right volume
	if ( wm8750_set_hp_vol_shadowreg(set_vol_addr_R, right) != 0 ) {
		modify |= 0x02;
	}

  // Setup
	wm8750_set_volume_reg(set_vol_addr_L, modify);

	WM8750_set_HP_SP(state);

	wm8750_i2c_close();

	return 0;
}

int wm8750_set_input_volume(int left, int right)
{
	unsigned char modify;
	int err = 0;
        
	if ( !wm8750_stat.used ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "not opened !\n");
    	return -EINVAL;
	}

	err = wm8750_i2c_open();
	if ( err ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
		return err;
	}

	modify = 0;

  // Left volume
	if (wm8750_set_linein_vol_shadowreg(WM8750_LeftInputVolume_ADR,left) != 0){
		modify |= 0x01;
	}

  // Right volume
	if(wm8750_set_linein_vol_shadowreg(WM8750_RightInputVolume_ADR,right)
																		 != 0){
		modify |= 0x02;
	}

  // Setup
	wm8750_set_volume_reg(WM8750_LeftInputVolume_ADR, modify);

	wm8750_i2c_close();

	return err;
}

int wm8750_set_mic_gain_boost(int vol)
{
	int boost,gain,alcl;
	unsigned char modify;
	int err;

	modify = 0;
	if ( vol == VOLUME_MUTE ) {
		// mute
		vol = 0;
	}
	if ( ( vol < 0 ) || ( vol > 100 ) ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "incorrect volume setting !\n");
		return 0;
	}

	if ( !wm8750_stat.used ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "not opend !\n");
		return -EINVAL;
	}

	err = wm8750_i2c_open();
	if ( err ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
		return err;
	}

	gain = wm8750_input_GAIN[ vol ][0];
	boost = wm8750_input_GAIN[ vol ][1];
	alcl = wm8750_input_GAIN[ vol ][2];

	if (wm8750_set_mic_boost_shadowreg(boost,gain,alcl,&modify)){
		wm8750_set_mic_gain_boost_reg(modify);
	}
	wm8750_i2c_close();

	return err;
}

int wm8750_set_freq(int freq)
{
	return 0;
}

static void WM8750_set_DAC_MUTE(int set_state)
{
	if (set_state){	// MUTE ON
		WM8750Regs[WM8750_AdcDacCtl_ADR] |= DPC_DAC_MUTE_OFF;
	}else{
		WM8750Regs[WM8750_AdcDacCtl_ADR] &= DPC_DAC_MUTE_MASK;
	}
	wm8750_i2c_write(WM8750_AdcDacCtl_ADR,
					WM8750Regs[WM8750_AdcDacCtl_ADR]);
	return;
}

static void WM8750_set_HP_SP(int set_state)
{

	if (set_state){	// HP
		WM8750Regs[WM8750_PwrMgmt2_ADR] = EN_DACL | EN_DACR | EN_LOUT1 | EN_ROUT1;
#ifdef USE_TREBLE_BOOST
		WM8750Regs[WM8750_BassCtl_ADR] = DISABLE_BASSCTL;
		WM8750Regs[WM8750_TrebleCtl_ADR] = DISABLE_TRBLCTL;
#endif
	}else{
		WM8750Regs[WM8750_PwrMgmt2_ADR] = EN_DACL | EN_DACR | EN_LOUT2 | EN_ROUT2;
#ifdef USE_TREBLE_BOOST
		WM8750Regs[WM8750_BassCtl_ADR] = ENABLE_BASSCTL;
		WM8750Regs[WM8750_TrebleCtl_ADR] = ENABLE_TRBLCTL;
#endif
	}
	wm8750_i2c_write(WM8750_PwrMgmt2_ADR, WM8750Regs[WM8750_PwrMgmt2_ADR]);
#ifdef USE_TREBLE_BOOST
	wm8750_i2c_write(WM8750_BassCtl_ADR, WM8750Regs[WM8750_BassCtl_ADR]);
	wm8750_i2c_write(WM8750_TrebleCtl_ADR, WM8750Regs[WM8750_TrebleCtl_ADR]);
#endif
	return;
}

/*
 * AGC
 */
static void wm8750_update_agc(int agc)
{
	unsigned short val;
#ifdef CONFIG_PM
	audioLockFCS( ADEV_VOL, 1 );
#endif
	if ( wm8750_stat.mode | SOUND_REC_MODE ) {
		if (agc) {
			WM8750Regs[WM8750_ALC1_ADR] |= SEL_ALC;
			WM8750Regs[WM8750_NoiseGate_ADR] |= NGAT_ON;
		}else{
			WM8750Regs[WM8750_ALC1_ADR] &= ~SEL_ALC;
			WM8750Regs[WM8750_NoiseGate_ADR] &= ~NGAT_ON;
		}
		wm8750_i2c_write(WM8750_ALC1_ADR,WM8750Regs[WM8750_ALC1_ADR]);
		wm8750_i2c_write(WM8750_NoiseGate_ADR,WM8750Regs[WM8750_NoiseGate_ADR]); 	}
#ifdef CONFIG_PM
	audioLockFCS( ADEV_VOL, 0 );
#endif
}

int wm8750_set_agc(int agc)
{
  if ( agc < 0 ) return wm8750_stat.agc;
  agc = !!agc;	/* should be 0 or 1 now */
  if ( wm8750_stat.agc != agc ) {
    wm8750_stat.agc = agc;
    wm8750_update_agc(wm8750_stat.agc);
  }
  return agc;
}


/*** Suspend/Resume  ***********************************************************/
int wm8750_suspend(void)
{
  int err = 0;

	if ( !wm8750_stat.used ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "not opend !\n");
		return -EINVAL;
	}
	err = wm8750_i2c_open();
	if ( err ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
		return err;
	}

	// deactive
	WM8750_set_DAC_MUTE(1);
	wm8750_i2c_write(WM8750_PwrMgmt1_ADR,0);
	wm8750_i2c_write(WM8750_PwrMgmt2_ADR,0);

	wm8750_i2c_close();

	return err;
}

int wm8750_resume(void)
{
	int i, err = 0;

	err = wm8750_i2c_open();
	if ( err ) {
		WM8750_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
		return err;
	}

	wm8750_i2c_write(WM8750_PwrMgmt1_ADR, 
		( SET_VMID(VREF_STOP) | EN_VREF ));
	mdelay(1);
	wm8750_i2c_write(WM8750_PwrMgmt2_ADR, 
		( EN_DACL | EN_DACR ));
	wm8750_i2c_write(WM8750_PwrMgmt1_ADR, 
		( SET_VMID(VREF_STOP) /* | EN_AINL | EN_AINR */ | EN_VREF ));
	wm8750_i2c_write(WM8750_PwrMgmt1_ADR,WM8750Regs[WM8750_PwrMgmt1_ADR]);
	wm8750_i2c_write(WM8750_PwrMgmt2_ADR, WM8750Regs[WM8750_PwrMgmt2_ADR]);

	for(i=0;i<WM8750_REG_NUM;i++) {
		if (((i < 32) && ((1 << i) & WM8750_REG_MASK1)) ||
			((i >= 32) && ((1 << (i - 32)) & WM8750_REG_MASK2)))
			wm8750_i2c_write(i, WM8750Regs[i]);
	}

	wm8750_i2c_close();

  return 0;
}
