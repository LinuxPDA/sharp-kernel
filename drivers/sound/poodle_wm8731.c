/*
 * linux/drivers/sound/poodle_wm8731.c
 *
 * wm8731 ctrl funstions for PXA (SHARP)
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
#include <asm/arch/poodle_wm8731.h>
#include <asm/arch/poodle_i2sc.h>

#include <asm/sharp_apm.h>


/*** Some declarations ***********************************************/
#define MY_TRACE	0

#define DBG_ALWAYS	0
#define DBG_LEVEL1	1
#define DBG_LEVEL2	2
#define DBG_LEVEL3	3

#ifdef MY_TRACE
#define WM8731_DBGPRINT(level, x, args...)  if ( level <= MY_TRACE ) printk("%s: " x,__FUNCTION__ ,##args)
#else
#define WM8731_DBGPRINT(level, x, args...)  if ( !level ) printk("%s: " x,__FUNCTION__ ,##args)
#endif

#define OPEN_WAIT_CNT		10	// wwait time for i2c open.
#define WM8731_I2C_ADDRESS	(unsigned char)0x01

/*** Some valiables **************************************************/
static int isInUseWM8731;
unsigned short WM8731Regs[WM8731_REG_NUM];	// Except ResetReg


#ifdef CONFIG_SOUND_CORGI
extern int corgi_HPstatus;

unsigned int wm8731_volume_in_HP[31] = {
    0,  65,  90,  94,  97, 100, 101, 102, 103, 104, 105, 106, 107, 107, 108,
  108, 109, 110, 110, 111, 112, 112, 113, 113, 114, 114, 115, 115, 116, 117
};

unsigned int wm8731_volume_out_HP[31] = {
    0,  50,  65,  70,  75,  80,  84,  87,  90,  92,  94,  95,  97,  99, 100,
  102, 104, 106, 108, 110, 111, 112, 113, 115, 116, 118, 120, 122, 125, 127
};

#else

unsigned int wm8731_volume_in_HP[31] = {
    0,  40,  48,  55,  61,  66,  70,  74,  78,  81,  84,  86,  89,  91,  94,
   96,  98, 100, 101, 103, 105, 106, 108, 109, 111, 112, 113, 115, 116, 117
};

unsigned int wm8731_volume_out_HP[31] = {
    0,  50,  58,  65,  71,  76,  80,  84,  88,  91,  94,  96,  99, 101, 104,
  106, 108, 110, 111, 113, 115, 116, 118, 119, 121, 122, 123, 125, 126, 127
};
#endif

/*** Config & Setup ******************************************************/
int wm8731_busy(void)
{
  return isInUseWM8731;
}

int wm8731_init(void)
{
  int i;
  int err = 0;

  isInUseWM8731 = FALSE;

  for(i=0;i<WM8731_REG_NUM;i++)
    WM8731Regs[i] = 0;

  err = wm8731_i2c_open();
  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }
  wm8731_i2c_write(WM8731_ResetReg_ADR, 0);					// reset WM8731
  mdelay(1);
  wm8731_i2c_write(WM8731_PowDownCtl_ADR, ( PD_OSC | PD_CLK | PD_DAC | PD_ADC | PD_MIC | PD_LIN | PD_OUT )); // Power Down WM8731
  wm8731_i2c_close();

  return 0;
}

/*** I2C ctrl ************************************************************/
static int wm8731_i2c_open(void)
{
  int err = 0,cnt = 0;
  int start_time;

  //  i2c_init();

  while(1) {
    err = i2c_open(WM8731_I2C_ADDRESS);
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

static int wm8731_i2c_write(unsigned char oreg, unsigned short odata)
{
  unsigned char adr,data;

  adr = ( (oreg << 1) | ( (odata >> 8) & 1 ) );
  data = odata & 0xff;
  if ( i2c_byte_write( WM8731_DEVICE , adr, data ) ) {
    WM8731_DBGPRINT( DBG_LEVEL1 , "i2c write error, retry !");
    if ( i2c_byte_write( WM8731_DEVICE , adr, data ) ) {
      WM8731_DBGPRINT( DBG_LEVEL1 , "error");
    } else {
      WM8731_DBGPRINT( DBG_LEVEL1 , "OK");
    }
  }

}

static int wm8731_i2c_close(void)
{
  i2c_close(WM8731_I2C_ADDRESS);
}


/*** Open & Close ******************************************************/

int wm8731_open(SOUND_SETTINGS *settings)
{
  int i, err = 0;


  WM8731_DBGPRINT(DBG_LEVEL2, "wm8731 open\n");

  if ( isInUseWM8731 ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "wm8731 device is busy now !\n");
    return -EBUSY;
  }

  err = wm8731_i2c_open();
  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }

  isInUseWM8731 = TRUE;

  lock_FCS(LOCK_FCS_SOUND, 1);

  wm8731_i2c_write(WM8731_ResetReg_ADR, 0);			// reset WM8731
  if ( wm8731_setup(settings) ) {				// set to shadow regs.
    wm8731_i2c_close();
    isInUseWM8731 = FALSE;
    return 1;
  }


  wm8731_i2c_write(WM8731_PowDownCtl_ADR, WM8731Regs[WM8731_PowDownCtl_ADR]);	// Power Down WM8731

  for(i=0;i<WM8731_REG_NUM;i++) {
    if ( i == WM8731_PowDownCtl_ADR ) continue;	// PowerDown reg skip.
    wm8731_i2c_write(i, WM8731Regs[i]);
  }

  // active
  wm8731_i2c_write(WM8731_ActiveCtl_ADR, ( WM8731Regs[WM8731_ActiveCtl_ADR] | DIGITAL_IF_ACTIVE ) );

  wm8731_i2c_close();



  return 0;
}


int wm8731_close(void)
{
  int err;

  WM8731_DBGPRINT(DBG_LEVEL2 ,"wm8731 close\n");

  err = wm8731_suspend();

  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not suspend wm8731\n");
    return err;
  }

  isInUseWM8731 = FALSE;

  lock_FCS(LOCK_FCS_SOUND, 0);


  return 0;
}

/*** Setup ***********************************************************/
static const unsigned short CODEC_SETTINGS_PLAY[WM8731_REG_NUM] = {
#if 1
  ( INPUT_MUTE_ON | INIT_LINE_VOL ),
  ( INPUT_MUTE_ON | INIT_LINE_VOL ),
  ( INIT_HP_VOL ),
  ( INIT_HP_VOL ),
#else
  ( SIMUL_UPDATE | INPUT_MUTE_ON | INIT_LINE_VOL ),
  ( SIMUL_UPDATE | INPUT_MUTE_ON | INIT_LINE_VOL ),
  ( SIMUL_UPDATE | INIT_HP_VOL ),
  ( SIMUL_UPDATE | INIT_HP_VOL ),
#endif
  ( DAC_SELECT | SIDETONE_DISABLE | BYPASS_DISABLE | INPUT_LINE | MIC_MUTE_ON | MIC_BOOST_OFF ),
  ( DPC_DAC_MUTE_OFF | DEEMPHASIS_MODE(0) | DPC_ADC_HPF_DISABLE ),
  ( PD_OSC | PD_CLK | PD_ADC | PD_MIC | PD_LIN ),
  ( SLAVE | LRSWAP_DISABLE | LRP_RIGHT_HIGH | IWL_16BIT | I2S_MODE ),
  ( SRC_BOTH_44KHZ | SRC_OSR_256FS | SRC_NORMAL_MODE )
};

static const unsigned short CODEC_SETTINGS_REC[WM8731_REG_NUM] = {
#if 1
  ( INIT_LINE_VOL ),
  ( INIT_LINE_VOL ),
  ( INIT_HP_VOL ),
  ( INIT_HP_VOL ),
#else
  ( SIMUL_UPDATE | INIT_LINE_VOL ),
  ( SIMUL_UPDATE | INIT_LINE_VOL ),
  ( SIMUL_UPDATE | INIT_HP_VOL ),
  ( SIMUL_UPDATE | INIT_HP_VOL ),
#endif
  ( DAC_OFF | SIDETONE_DISABLE | BYPASS_DISABLE | INPUT_LINE | MIC_MUTE_ON | MIC_BOOST_ON ),
  ( DPC_DAC_MUTE_OFF | DEEMPHASIS_MODE(0) | DPC_ADC_HPF_DISABLE ),
  ( PD_OSC | PD_CLK | PD_DAC | PD_MIC ),
  ( SLAVE | LRSWAP_DISABLE | LRP_RIGHT_HIGH | IWL_16BIT | I2S_MODE ),
  ( SRC_BOTH_44KHZ | SRC_OSR_256FS | SRC_NORMAL_MODE )
};

static int wm8731_setup(SOUND_SETTINGS  *setting)
{
  int i, ret = 0;


  if( setting == NULL ) return -1;

  if ( !isInUseWM8731 ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "not opend !\n");
    return -EINVAL;
  }

  switch( setting->mode&SOUND_MODE_MASK ){
  case SOUND_PLAY_MODE:
    // Copy initial value
    for(i=0;i<WM8731_REG_NUM;i++)
      WM8731Regs[i] = CODEC_SETTINGS_PLAY[i];
    break;

  case SOUND_REC_MODE:
    {
      // Copy initial value
      for(i=0;i<WM8731_REG_NUM;i++)
	WM8731Regs[i] = CODEC_SETTINGS_REC[i];

      WM8731Regs[WM8731_AnalogAudioPath_ADR] &= ~MIC_MUTE_ON;
      WM8731Regs[WM8731_AnalogAudioPath_ADR] |= INPUT_MIC;
      WM8731Regs[WM8731_PowDownCtl_ADR] &= ~PD_MIC;
      WM8731Regs[WM8731_PowDownCtl_ADR] |= PD_LIN;

      // enable play , because we have to play while recording.
      WM8731Regs[WM8731_AnalogAudioPath_ADR] |= DAC_SELECT;
      WM8731Regs[WM8731_PowDownCtl_ADR] &= ~PD_DAC;
	break;
    }

  default:
    return -1;
  }

  wm8731_set_hp_vol_shadowreg(WM8731_LeftHdpOut_ADR, setting->output.left);	// vol setting
  wm8731_set_hp_vol_shadowreg(WM8731_RightHdpOut_ADR, setting->output.right);

  wm8731_set_linein_vol_shadowreg(WM8731_LeftLineIn_ADR, setting->input.left);	// vol setting
  wm8731_set_linein_vol_shadowreg(WM8731_RightLineIn_ADR, setting->input.right);

  wm8731_set_freq_shadowreg(setting->frequency);

  return ret;
}


/*** Volume ***********************************************************/
// for Output
// update shadow regs.
static int wm8731_set_hp_vol_shadowreg(int adr, int vol)
{
  unsigned short reg,volume;
  int state;
        
  if ( vol == VOLUME_MUTE ) {
    // mute
    vol = 0;
  }

  if ( ( vol < 0 ) || ( vol > 100 ) ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "incorrect volume setting !\n");
    return 0;
  }

  //  WM8731_DBGPRINT(DBG_ALWAYS, "vol(%d) = %d\n", ( ( 29 * vol ) / 100 ) ,vol);


#ifdef CONFIG_SOUND_CORGI
  state = corgi_HPstatus;
#else
  state = GPLR(GPIO_HP_IN) & GPIO_bit(GPIO_HP_IN);
#endif

  if ( !state ) { 	// Insert HP
    vol = wm8731_volume_in_HP[ ( ( 29 * vol ) / 100 ) ];
  } else {
    vol = wm8731_volume_out_HP[ ( ( 29 * vol ) / 100 ) ];
  }


  if( ( vol >= MIN_HP_VOL ) && ( vol <= MAX_HP_VOL ) ) {
    // set volume to shadow regs.
    reg = WM8731Regs[adr];
    volume = reg & HP_VOL_MASK;
    if (volume != vol) {
      reg = ( reg & ~HP_VOL_MASK ) | vol;
      WM8731Regs[adr] = reg;
      return 1;
    }
  } else {
    // ignore
  }

  return 0;
}

// for Input
// update shadow regs.
static int wm8731_set_linein_vol_shadowreg(int adr, int vol)
{
  unsigned short reg,volume;
       
  if( vol == VOLUME_MUTE ) {
    // mute
    reg = WM8731Regs[adr];
    if( !( reg & INPUT_MUTE_ON ) ) {
      reg |= INPUT_MUTE_ON;
      WM8731Regs[adr] = reg;
      return 1;
    }
  } else if ( ( vol >= MIN_LINE_VOL ) && ( vol <= MAX_LINE_VOL ) ) {
                // set volume
    reg = WM8731Regs[adr];
    volume = reg & LINE_VOL_MASK;
    if ( ( reg & INPUT_MUTE_ON ) || ( volume != vol ) ) {
      reg = ( reg & ~( INPUT_MUTE_ON | LINE_VOL_MASK ) ) | vol;
      WM8731Regs[adr] = reg;
      return 1;
    }
  } else {
     // ignore
  }
  return 0;

}


// for MIC
// update shadow regs.
static int wm8731_set_mic_boost_shadowreg(int on)
{
  unsigned short reg,volume;

  reg = WM8731Regs[WM8731_AnalogAudioPath_ADR];

  if ( on ) {
    if ( reg & MIC_BOOST_ON ) {
      return 0;			// already on
    } else {
      WM8731Regs[WM8731_AnalogAudioPath_ADR] |= MIC_BOOST_ON;
      return 1;			// set to on
    }
  } else {
    if ( reg & MIC_BOOST_ON ) {
      WM8731Regs[WM8731_AnalogAudioPath_ADR] &= ~MIC_BOOST_ON;
      return 1;			// set to off
    } else {
      return 0;			// already off
    }
  }

}

// for Freq.
static void wm8731_set_freq_shadowreg(int freq)
{
  switch (freq) {
  case 8000:
      WM8731Regs[WM8731_SamplingCtl_ADR] = ( SRC_BOTH_48KHZ | SRC_OSR_256FS | SRC_NORMAL_MODE );
      //WM8731Regs[WM8731_SamplingCtl_ADR] = ( SRC_BOTH_8KHZ | SRC_OSR_256FS | SRC_NORMAL_MODE );
      break;

  case 44100:
      WM8731Regs[WM8731_SamplingCtl_ADR] = ( SRC_BOTH_44KHZ | SRC_OSR_256FS | SRC_NORMAL_MODE );
      break;

  case 48000:
      WM8731Regs[WM8731_SamplingCtl_ADR] = ( SRC_BOTH_48KHZ | SRC_OSR_256FS | SRC_NORMAL_MODE );
      break;

  default:
      WM8731Regs[WM8731_SamplingCtl_ADR] = ( SRC_BOTH_44KHZ | SRC_OSR_256FS | SRC_NORMAL_MODE );
      break;
  }

}

/**********************************/
// for Output/Input
// update wm8731 regs.
static void wm8731_set_volume_reg(int adr, unsigned char modify)
{
  int i;
  unsigned char mask;

  mask = 0x01;

  for( i=0;i<2;i++ ) {
    if( modify & mask ) {
      wm8731_i2c_write( adr+i , WM8731Regs[adr+i] );
    }
    mask <<= 1;
  }
  return;

}


// update wm8731 regs.
static void wm8731_set_mic_gain_boost_reg(unsigned char modify)
{
  if( modify ) {
    wm8731_i2c_write( WM8731_AnalogAudioPath_ADR , WM8731Regs[WM8731_AnalogAudioPath_ADR] );
  }

  return;
}


/**********************************/
int wm8731_set_output_volume(int left, int right)
{
  unsigned char modify;
  int err;

        
  if ( !isInUseWM8731 ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "not opened !\n");
    return -EINVAL;
  }

  err = wm8731_i2c_open();
  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }

  modify = 0;

  // Left volume
  if( wm8731_set_hp_vol_shadowreg(WM8731_LeftHdpOut_ADR, left) != 0 ) {
    modify |= 0x01;
  }

  // Right volume
  if( wm8731_set_hp_vol_shadowreg(WM8731_RightHdpOut_ADR, right) != 0 ) {
    modify |= 0x02;
  }

  // Setup
  wm8731_set_volume_reg(WM8731_LeftHdpOut_ADR, modify);

  wm8731_i2c_close();

  return 0;
}


int wm8731_set_input_volume(int left, int right)
{
  unsigned char modify;
  int err = 0;
        
  if ( !isInUseWM8731 ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "not opened !\n");
    return -EINVAL;
  }

  err = wm8731_i2c_open();
  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }

  modify = 0;

  // Left volume
  if( wm8731_set_linein_vol_shadowreg(WM8731_LeftLineIn_ADR, left) != 0 ) {
    modify |= 0x01;
  }

  // Right volume
  if( wm8731_set_linein_vol_shadowreg(WM8731_RightLineIn_ADR, right) != 0 ) {
    modify |= 0x02;
  }

  // Setup
  wm8731_set_volume_reg(WM8731_LeftLineIn_ADR, modify);

  wm8731_i2c_close();

  return err;

}


int wm8731_set_mic_gain_boost(int on)
{
  unsigned char modify;
  int err;


  if ( !isInUseWM8731 ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "not opend !\n");
    return -EINVAL;
  }

  err = wm8731_i2c_open();
  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }

  modify = 0;

  if ( wm8731_set_mic_boost_shadowreg(on) != 0 ) {
    modify |= 0x01;
  }

  // Setup
  wm8731_set_mic_gain_boost_reg(modify);

  wm8731_i2c_close();

  return err;
}


int wm8731_set_freq(int freq)
{
  int err;


  WM8731_DBGPRINT(DBG_LEVEL2, "freq = %d\n",freq);

  if ( !isInUseWM8731 ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "not opened !\n");
    return -EINVAL;
  }

  err = wm8731_i2c_open();
  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }

  wm8731_set_freq_shadowreg(freq);

  // Setup
  wm8731_i2c_write( WM8731_SamplingCtl_ADR , WM8731Regs[WM8731_SamplingCtl_ADR] );

  wm8731_i2c_close();

  return err;
}


/*** Suspend/Resume  ***********************************************************/
int wm8731_suspend(void)
{
  int err = 0;

  if ( !isInUseWM8731 ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "not opend !\n");
    return -EINVAL;
  }

  err = wm8731_i2c_open();
  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }

  // deactive
  wm8731_i2c_write(WM8731_ActiveCtl_ADR, 0 );
  mdelay(1);

  wm8731_i2c_write(WM8731_ResetReg_ADR, 0);					// reset WM8731
  mdelay(1);
  wm8731_i2c_write(WM8731_PowDownCtl_ADR, ( PD_OUT ));
  mdelay(1);
  wm8731_i2c_write(WM8731_PowDownCtl_ADR, ( PD_OSC | PD_CLK | PD_OUT | PD_DAC | PD_ADC | PD_MIC | PD_LIN )); // Power Down WM8731

  wm8731_i2c_close();

  return err;
}


int wm8731_resume(void)
{
  int i, err = 0;

  err = wm8731_i2c_open();
  if ( err ) {
    WM8731_DBGPRINT(DBG_ALWAYS, "can not open i2c device\n");
    return err;
  }

  wm8731_i2c_write(WM8731_PowDownCtl_ADR, WM8731Regs[WM8731_PowDownCtl_ADR]);	// Power Down WM8731

  for(i=0;i<WM8731_REG_NUM;i++) {
    if ( i == WM8731_PowDownCtl_ADR ) continue;	// PowerDown reg skip.
    wm8731_i2c_write(i, WM8731Regs[i]);
  }

  // active
  wm8731_i2c_write(WM8731_ActiveCtl_ADR, ( WM8731Regs[WM8731_ActiveCtl_ADR] | DIGITAL_IF_ACTIVE ) );

  wm8731_i2c_close();


  return 0;
}
