/*
 * linux/drivers/sound/tosa_wm9712.c
 *
 * wm9712 ctrl funstions for PXA (SHARP)
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * Based on:
 * 	linux/drivers/sound/poodle_wm8731.c
 * 	wm8731 ctrl funstions for PXA (SHARP)
 * 	Copyright (C) 2002  SHARP
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
 */
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ac97_codec.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>

#ifdef CONFIG_PM
#include <asm/sharp_apm.h>
#endif	/* CONFIG_PM */

#include <asm/arch/tosa_wm9712.h>

/************************************************************
 * Debug
 ************************************************************/
#define DBG_LEVEL	0

#define DBG_L0          0
#define DBG_L1          1
#define DBG_L2          2
#define DBG_L3          3

#ifdef DBG_LEVEL
#define DEBUG(level, x, args...) \
	if ( level <= DBG_LEVEL ) printk("%s: " x,__FUNCTION__ ,##args)
#else
#define DEBUG(level, x, args...) \
	if ( !level ) printk("%s: " x,__FUNCTION__ ,##args)
#endif  /* DBG_LEVEL */

//X#define DISABLE_JACKCHECK_PLAYING
//X#define USE_HARDWARE_LMIX

/*** Some valiables **************************************************/
typedef struct {
  unsigned char		used;		//used: open(1) close(0)
  unsigned char		jack;		//Jack: in(1) out(0)
  unsigned char		mic;		// Mic headPhone(1) no mic(0)
  unsigned char		last_jack_state; // jack state
  unsigned char		open_play;
  unsigned char		open_rec;
  unsigned short	mode;		//Sound mode
  int			out_left;	//Volume out left
  int			out_right;	//Volume out right
  int			in_left;	//Volume in left
  int			in_right;	//Volume in right
  int			rate;		//Sampling rate
  int			agc;		//Agc: on(1) off(0)
  int			vol0;
#ifdef DISABLE_JACKCHECK_PLAYING
  int			need_after_check; // check jack after playing
#endif
#ifdef CONFIG_PM
  int			suspend;	//Suspend: on(1) off(0)
#endif	/* CONFIG_PM */
} wm9712_stat_t;

static wm9712_stat_t wm9712_stat;
static unsigned char ac97_on = 0;
int wm9712_mono_out = 0; // mix output data into mono
//static spinlock_t ear_in_lock = SPIN_LOCK_UNLOCKED;
//static struct tq_struct wm9712_queue;
static struct timer_list hp_wakeup_timer;
#define HP_WAKEUP_DELAY		(HZ*3)

static int wm9712_setup(SOUND_SETTINGS *);
static void wm9712_update_agc(int);

#define MAKEJACKSTATE(jack,mic)	((jack)*2+(mic))
#define INVALID_JACKSTATE	-1
#define DEV_OUTALL	0
#define DEV_SP		1
#define DEV_HP		2
#define DEV_HPL		3
#define DEV_MIC		4

#define BIT_ZC		(0x1<<7)
#define BIT_INV		(0x1<<6)
#define VOLL_SHIFT	8
#define VOLL_MASK	(0x3f<<VOLL_SHIFT)
#define VOLR_MASK	(0x3f)
#define VOLL_MUTE	(0x2000)
#define MASK_DACVOL	(0x1f1f)
#define LDACVOLSHIFT	8
#define RDACVOLSHIFT	0
#define BOOSTSHIFT	14
#define MASK_BOOST	(0x1<<BOOSTSHIFT)
#define MASK_RECVOL	(0x3f3f)
#define RECVOLLSHIFT	8
#define RECVOLRSHIFT	0
#define DACVOL_MASK	0x1F1F

#define RECSL_MASK		0x0707
#define RECSL_BITS_MIC		0x0000
#define RECSL_BITS_LINEIN	0x0304
#define D2H_BIT		(0x1<<15)
#define D2S_BIT		(0x1<<14)
#define AC97_GPIO1	(0x1<<1)
#define AC97_GPIO5	(0x1<<5)
#define ALCM_MASK	0x0c00
#define ALCM_MUTE	0x0c00
#define ALCM_HP		0x0800
#define ALC_MASK	0xc080
#define ALC_ON		0x4080
#define MS_MASK		0x0060
#define MS_MIC2		0x0040
#define MUTE_INPUT	(0x1<<15)
#define BIT_VRA		(0x1<<0)

#define DEFAULT_OUT_VOLUME	(75)
#define DEFAULT_IN_VOLUME	(75)
#define DEFAULT_RATE		(8000)
#define JACK_POLLING_WAIT	(5)

static DECLARE_WAIT_QUEUE_HEAD(hp_queue);
static DECLARE_WAIT_QUEUE_HEAD(hp_proc);
static int volume_table_initialized = FALSE;
static unsigned short SpVolToReal[101];
static unsigned short HpVolToReal[101];
static unsigned short MicVolToReal[101];
static unsigned short DacVolToReal[101];
static unsigned short  VolCnvTbl[38][2] __initdata = {
  {8,31}, {8,31}, {8,30}, {8,29}, {8,28}, // 0 - 4
  {8,27}, {8,26}, {8,25}, {8,24}, {8,23}, // 5 - 9
  {8,22}, {8,21}, {8,20}, {8,19}, {8,18}, // 10 - 14
  {8,17}, {8,16}, {8,15}, {8,14}, {8,13}, // 15 - 19
  {8,12}, {8,11}, {8,10}, {8, 9}, {8, 8}, // 20 - 24
  {8, 7}, {8, 6}, {8, 5}, {8, 4}, {8, 3}, // 25 - 29
  {8, 2}, {8, 1}, {8, 0}, {7, 0}, {6, 0}, // 30 - 34
  {5, 0}, {4, 0}, {3, 0}, }; // 35 - 37

static unsigned short VolInitTbl[101] __initdata = {
  0,  1,  5,  9, 11, 13, 14, 15, 16, 17,
  18, 19, 19, 20, 21, 21, 22, 22, 23, 23,
  23, 24, 24, 25, 25, 25, 25, 26, 26, 26,
  27, 27, 27, 27, 28, 28, 28, 28, 28, 29,
  29, 29, 29, 29, 30, 30, 30, 30, 30, 30,
  31, 31, 31, 31, 31, 31, 31, 32, 32, 32,
  32, 32, 32, 32, 33, 33, 33, 33, 33, 33,
  33, 33, 33, 34, 34, 34, 34, 34, 34, 34,
  34, 34, 34, 35, 35, 35, 35, 35, 35, 35,
  35, 35, 35, 35, 36 ,36, 36, 36, 36, 36,
  37 };

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

/************************************************************
 * Config & Setup
 ************************************************************/
int wm9712_busy(void)
{
  return wm9712_stat.used;
}

void wm9712_checkjack_sleep(void)
{
  interruptible_sleep_on(&hp_proc);
}

void wm9712_checkjack_wakeup(void)
{
  wake_up(&hp_proc);
}

static int check_hp_jack(void)
{
  unsigned short gpio_status;
  int count = 0, err = 0;
  int state;

  gpio_status = ac97_read(AC97_GPIO_STATUS);
  state = !(gpio_status & AC97_GPIO1); //Active L

  while( !(count>10 || err>50) ) {
    if (wm9712_stat.suspend) mdelay(JACK_POLLING_WAIT);
    else interruptible_sleep_on_timeout((wait_queue_head_t*)&hp_queue, JACK_POLLING_WAIT ); // wait

    gpio_status = ac97_read(AC97_GPIO_STATUS);
    if (state == !(gpio_status & AC97_GPIO1)) {
      count++;
    } else {
      err++;
      state = !(gpio_status & AC97_GPIO1);
      count=0;
    }
  }

  if ( count > 10 ) {
    wm9712_stat.jack = (state)?TRUE:FALSE;
  } else { // error
    wm9712_stat.jack = FALSE;
  }
  return wm9712_stat.jack;
}

static int check_hp_mic(void)
{
  unsigned short gpio_status;
  int count = 0, err = 0;
  volatile int state;
  int LchMute;

  if (!wm9712_stat.jack) {
    wm9712_stat.mic = FALSE;
    return FALSE;
  }
#ifdef DISABLE_JACKCHECK_PLAYING
  if (wm9712_stat.open_play) { // cannot check mic while playing
    wm9712_stat.need_after_check=TRUE;
    return wm9712_stat.mic;
  }
  wm9712_stat.need_after_check=FALSE;
#endif

  wm9712_power_mode_tmp(WM9712_PWR_ENMICBIAS); // MICBIAS on
  LchMute = !(TC6393_SYS_REG(TC6393_SYS_GPODSR1) & TC6393_L_MUTE);
  if (!LchMute) TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_L_MUTE;    // set L_Mute

  gpio_status = ac97_read(AC97_GPIO_STATUS);
  state = (gpio_status & AC97_GPIO5); //Active H

  while( !(count>10 || err>50) ) {
    if (wm9712_stat.suspend) mdelay(JACK_POLLING_WAIT);
    else interruptible_sleep_on_timeout((wait_queue_head_t*)&hp_queue, JACK_POLLING_WAIT ); // wait

    gpio_status = ac97_read(AC97_GPIO_STATUS);
    if (state == (gpio_status & AC97_GPIO5)) {
      count++;
    } else {
      err++;
      state = (gpio_status & AC97_GPIO5);
      count=0;
    }
  }

  wm9712_power_mode_tmp(WM9712_PWR_OFF); // restore MICBIAS
  if (!LchMute) TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_L_MUTE; // restore L_Mute

  if ( count > 10 ) {
    wm9712_stat.mic = (state)?TRUE:FALSE;
  } else { // error
    wm9712_stat.mic = FALSE;
  }
  return wm9712_stat.mic;
}

static void set_mute_registers( int dev, int mute )
{
  unsigned short val,val2;
#ifdef USE_HARDWARE_LMIX
  unsigned short val3;
#endif

  switch(dev) {
  case DEV_MIC:
    if (mute) {
          ac97_bit_set(AC97_RECORD_GAIN, MUTE_INPUT);
    } else {
          ac97_bit_clear(AC97_RECORD_GAIN, MUTE_INPUT);
    }
    break;
  case DEV_OUTALL:
    set_mute_registers(DEV_SP,mute);
    set_mute_registers(DEV_HP,mute);
    break;
  case DEV_SP: // speaker
    val = ac97_read(AC97_MASTER_VOL_STEREO);
    val2 = ac97_read(AC97_DAC_VOL);
    if (mute || wm9712_stat.vol0) {
      val |= AC97_MUTE;
      val2 |= D2S_BIT;
    } else {
      val &= ~AC97_MUTE;
      val2 &= ~D2S_BIT;
    }
    ac97_write(AC97_MASTER_VOL_STEREO,val);
    ac97_write(AC97_DAC_VOL,val2);
    break;
  case DEV_HP: // headphone
    val = ac97_read(AC97_HEADPHONE_VOL);
    val2 = ac97_read(AC97_DAC_VOL);
    if (mute || wm9712_stat.vol0) {
      val |= AC97_MUTE;
      val2 |= D2H_BIT;
    } else {
      val &= ~AC97_MUTE;
      val2 &= ~D2H_BIT;
    }
    ac97_write(AC97_HEADPHONE_VOL,val);
    ac97_write(AC97_DAC_VOL,val2);
    break;
  case DEV_HPL: // headphone Lch only
    val = ac97_read(AC97_HEADPHONE_VOL);
#ifdef USE_HARDWARE_LMIX
    val2 = ac97_read(AC97_SIDETONE);
    val3 = ac97_read(AC97_DAC_VOL);
#endif
    if (mute) {
      wm9712_mono_out=1;
      val |= VOLL_MUTE; // mute bit
#ifdef USE_HARDWARE_LMIX
      val2 &= ~ALCM_MASK; // SP Mix to Rch
      val2 |= ALCM_HP;
      val3 |= D2H_BIT;
      val3 &= ~D2S_BIT;
      if (!wm9712_stat.open_rec) set_mute_registers(DEV_MIC,FALSE);
#endif
      TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_L_MUTE;      // Mute Lch
    } else {
      wm9712_mono_out=0;
      val &= ~VOLL_MASK;
      val |= (val&VOLR_MASK)<<VOLL_SHIFT;
#ifdef USE_HARDWARE_LMIX
      val2 &= ~ALCM_MASK; // mute SP Mix
      val2 |= ALCM_MUTE;
      val3 &= ~D2H_BIT;
      val3 &= ~D2S_BIT;
      if (!wm9712_stat.open_rec) set_mute_registers(DEV_MIC,TRUE);
#endif
      TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_L_MUTE;      // Mute Lch off
    }
    ac97_write(AC97_HEADPHONE_VOL,val);
#ifdef USE_HARDWARE_LMIX
    ac97_write(AC97_SIDETONE,val2)
    ac97_write(AC97_DAC_VOL,val3);
#endif
    break;
  }
}

static void set_audio_registers( int jackmode )
{
  char play = wm9712_stat.open_play;
  char rec = wm9712_stat.open_rec;
  unsigned short val;

  switch( jackmode ) {
  case MAKEJACKSTATE(0,0): // Speaker (no HP)
  case MAKEJACKSTATE(0,1):
    // record from OnBoard Mic
    val = ac97_read(AC97_RECORD_SELECT);
    val &= ~RECSL_MASK;
    val |= RECSL_BITS_MIC;
    ac97_write(AC97_RECORD_SELECT,val);
    // power down
    wm9712_power_mode_audio((play)?WM9712_PWR_PLAY:WM9712_PWR_OFF);
    wm9712_power_mode_audioin((rec)?WM9712_PWR_REC:WM9712_PWR_OFF);
    // mute
    set_mute_registers(DEV_HP,TRUE);
    set_mute_registers(DEV_HPL,FALSE);
    set_mute_registers(DEV_SP,FALSE);
    break;
  case MAKEJACKSTATE(1,0): // Stereo HP
    // record from OnBoard Mic
    val = ac97_read(AC97_RECORD_SELECT);
    val &= ~RECSL_MASK;
    val |= RECSL_BITS_MIC;
    ac97_write(AC97_RECORD_SELECT,val);
    // power down
    wm9712_power_mode_audio((play)?WM9712_PWR_PLAY_HP:WM9712_PWR_OFF);
    wm9712_power_mode_audioin((rec)?WM9712_PWR_REC_HP:WM9712_PWR_OFF);
    // mute
    set_mute_registers(DEV_SP,TRUE);
    set_mute_registers(DEV_HPL,FALSE);
    set_mute_registers(DEV_HP,FALSE);
    break;
  case MAKEJACKSTATE(1,1): // Mic HP
    // record from HP Mic
    val = ac97_read(AC97_RECORD_SELECT);
    val &= ~RECSL_MASK;
    val |= RECSL_BITS_LINEIN;
    ac97_write(AC97_RECORD_SELECT,val);
    // power down
    wm9712_power_mode_audio((play)?WM9712_PWR_PLAY_MIC:WM9712_PWR_OFF);
    wm9712_power_mode_audioin((rec)?WM9712_PWR_REC_MIC:WM9712_PWR_OFF);
    // mute
    set_mute_registers(DEV_SP,TRUE);
    set_mute_registers(DEV_HPL,TRUE);
    set_mute_registers(DEV_HP,FALSE);
    break;
  }
}

void wm9712_update_jack_state()
{
  int cur_jack_state;

#ifdef CONFIG_PM
  audioLockFCS( ADEV_HP, 1 );
#endif
  check_hp_jack();
  check_hp_mic();
  cur_jack_state = MAKEJACKSTATE(wm9712_stat.jack,wm9712_stat.mic);
  if (wm9712_stat.last_jack_state == cur_jack_state) {
#ifdef CONFIG_PM
    audioLockFCS( ADEV_HP, 0 );
#endif
    return;
  }
  
  DEBUG(DBG_L0, "HP jack state changed (Jack=%d,Mic=%d)\r\n",wm9712_stat.jack,wm9712_stat.mic);

  set_audio_registers(cur_jack_state);
  wm9712_stat.last_jack_state = cur_jack_state;
#ifdef CONFIG_PM
  audioLockFCS( ADEV_HP, 0 );
#endif
}

static void wm9712_ear_in_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  wm9712_checkjack_wakeup();
}


void wm9712_exit(void)
{
  set_mute_registers(DEV_OUTALL,TRUE);
  
  free_irq(IRQ_GPIO_EAR_IN, NULL);

  wm9712_power_mode_audio(WM9712_PWR_OFF);
  wm9712_power_mode_audioin(WM9712_PWR_OFF);
  wm9712_power_mode_tmp(WM9712_PWR_OFF);

  pxa_ac97_put(&ac97_on);
}


int wm9712_init(void)
{
  unsigned short val;

  printk("w9712_init\n");

#ifdef CONFIG_PM
  audioLockFCS( ADEV_INIT, 1 );
#endif
  if (!volume_table_initialized) {
    int i;
    // initialize status 
    memset(&wm9712_stat,0,sizeof(wm9712_stat));
    wm9712_stat.out_left = DEFAULT_OUT_VOLUME;
    wm9712_stat.out_right = DEFAULT_OUT_VOLUME;
    wm9712_stat.in_left = DEFAULT_IN_VOLUME;
    wm9712_stat.in_right = DEFAULT_IN_VOLUME;
    wm9712_stat.rate = DEFAULT_RATE;

    // set volume table
    // volume '90' is 0dB, max volume is 7.5dB
    for (i=0; i<=100; i++) {
      SpVolToReal[i] = VolCnvTbl[VolInitTbl[i]][1];
      HpVolToReal[i] = VolCnvTbl[VolInitTbl[i]][1];
      DacVolToReal[i] = VolCnvTbl[VolInitTbl[i]][0];
      MicVolToReal[i] = 0xf * i / 100;
    }
    volume_table_initialized=TRUE;
  }
  wm9712_stat.last_jack_state = INVALID_JACKSTATE;
  pxa_ac97_get(&codec, &ac97_on);
  wm9712_power_mode_audio(WM9712_PWR_OFF);
  wm9712_power_mode_audioin(WM9712_PWR_OFF);
  wm9712_power_mode_tmp(WM9712_PWR_OFF);


  // set default values in wm9712 registers
  // 0x02 MASTER_VOLUME ... ZC=1, INV=1
  val = ac97_read(AC97_MASTER_VOL_STEREO);
  val |= BIT_ZC|BIT_INV;
  ac97_write(AC97_MASTER_VOL_STEREO,val);

  // 0x04 HEADPHONE_VOLUME ... ZC=1
  val = ac97_read(AC97_HEADPHONE_VOL);
  val |= BIT_ZC;
  ac97_write(AC97_HEADPHONE_VOL,val);

  // 0x0E MIC VOLUME ... set MS
  val = ac97_read(AC97_MIC_VOL);
  val &= ~MS_MASK;
  val |= MS_MIC2;
  ac97_write(AC97_MIC_VOL,val);

  // 0x1A RECORD_SELECT ... set BOOST
  val = ac97_read(AC97_RECORD_SELECT);
  val |= MASK_BOOST;
  ac97_write(AC97_RECORD_SELECT,val);

  // 0x1C RECORD_GAIN ... set ZC=1
  val = ac97_read(AC97_RECORD_GAIN);
  val |= BIT_ZC;
  ac97_write(AC97_RECORD_GAIN,val);

  // 0x2A AC97_EXTENDED_STATUS set VRA=1
  val = ac97_read(AC97_EXTENDED_STATUS);
  val |= BIT_VRA;
  ac97_write(AC97_EXTENDED_STATUS,val);

  // set default volume
  wm9712_set_output_volume(wm9712_stat.out_left, wm9712_stat.out_right);
  wm9712_set_input_gain(wm9712_stat.in_left, wm9712_stat.in_right);

  // set default rate
  ac97_set_dac_rate(codec, (unsigned int)wm9712_stat.rate);
  ac97_set_adc_rate(codec, (unsigned int)wm9712_stat.rate);

  wm9712_update_jack_state();
  init_timer(&hp_wakeup_timer);
  hp_wakeup_timer.function = wm9712_checkjack_wakeup;

#ifdef CONFIG_PM
  audioLockFCS( ADEV_INIT, 0 );
#endif

  set_GPIO_IRQ_edge(GPIO_HP_IN, GPIO_BOTH_EDGES);
  if( request_irq(IRQ_GPIO_EAR_IN, wm9712_ear_in_interrupt, SA_INTERRUPT, "ear_in", NULL) ) {
    DEBUG(DBG_L0, "IRQ error %d\n", IRQ_GPIO_EAR_IN);
    return -EBUSY;
  }

  return 0;
}

/************************************************************
 * Open & Close
 ************************************************************/
int wm9712_open(SOUND_SETTINGS *settings)
{
  DEBUG(DBG_L2, "wm9712 open\n");

  if ( wm9712_stat.used ) {
    DEBUG(DBG_L0, "wm9712 device is busy now !\n");
    return -EBUSY;
  }

  wm9712_stat.used = TRUE;

#ifdef CONFIG_PM
  audioLockFCS( ADEV_MAIN, 1 );
#endif	/* CONFIG_PM */

  if ( wm9712_setup(settings) < 0 ) {
    wm9712_close();
    wm9712_stat.used = FALSE;
    return -EINVAL;
  }

  return 0;
}

int wm9712_close(void)
{
  DEBUG(DBG_L2 ,"wm9712 close\n");

  //  set_mute_registers(DEV_OUTALL,TRUE);
  set_mute_registers(DEV_MIC,TRUE);

  wm9712_stat.used = FALSE;
  wm9712_stat.open_play = FALSE;
  wm9712_stat.open_rec = FALSE;
  
  wm9712_power_mode_audio(WM9712_PWR_OFF);
  wm9712_power_mode_audioin(WM9712_PWR_OFF);
  wm9712_power_mode_tmp(WM9712_PWR_OFF);

#ifdef CONFIG_PM
  audioLockFCS( ADEV_MAIN, 0 );
#endif	/* CONFIG_PM */

#ifdef DISABLE_JACKCHECK_PLAYING
  if (wm9712_stat.need_after_check) {
    wm9712_checkjack_wakeup();
  }
#endif

  return 0;
}

/************************************************************
 * Setup
 ************************************************************/
static int wm9712_setup(SOUND_SETTINGS *setting)
{
  int ret = 0;

  if( setting == NULL ) return -1;

  if ( !wm9712_stat.used ) {
    DEBUG(DBG_L0, "not opened !\n");
    return -EINVAL;
  }

  wm9712_stat.mode = setting->mode & SOUND_MODE_MASK;
  if (wm9712_stat.mode & SOUND_PLAY_MODE) {
    DEBUG(DBG_L2, "Sound Play Setting\n");
    //    wm9712_set_output_volume(setting->output.left, setting->output.right);
    wm9712_stat.open_play=TRUE;
  }
  if (wm9712_stat.mode & SOUND_REC_MODE) {
    DEBUG(DBG_L2, "Sound Rec Mode\n");
    set_mute_registers(DEV_MIC,FALSE);
    //    wm9712_set_input_gain(setting->input.left, setting->input.right);
    wm9712_stat.open_rec=TRUE;
  }
  if (!wm9712_stat.open_play && !wm9712_stat.open_rec) {
    DEBUG(DBG_L0, "Sound mode(%x) is not supported !\n", wm9712_stat.mode);
    return -EINVAL;
  }

  set_audio_registers(wm9712_stat.last_jack_state);

  if ( (ret = wm9712_set_freq(setting)) < 0 ) return ret;

  return 0;
}

// for Freq.
int wm9712_set_freq(SOUND_SETTINGS *setting)
{
  int ret = 0;
  unsigned int actual_val;
  wm9712_stat.rate = setting->frequency;
  DEBUG(DBG_L2, "rate = %d\n", wm9712_stat.rate);

  if ( !wm9712_stat.used ) {
    DEBUG(DBG_L0, "not opened !\n");
    return -EINVAL;
  }
  if ( wm9712_stat.rate < 8000 ) wm9712_stat.rate = 8000;
  else if ( wm9712_stat.rate > 48000 ) wm9712_stat.rate = 48000; 
  
  if (wm9712_stat.open_play) {
    actual_val = ac97_set_dac_rate(codec, (unsigned int)wm9712_stat.rate);
    if ( actual_val != wm9712_stat.rate) {
      DEBUG(DBG_L1,"Set DAC rate error: %d->%d\n",wm9712_stat.rate,actual_val);
    }
  }
  if (wm9712_stat.open_rec) {
    actual_val = ac97_set_adc_rate(codec, (unsigned int)wm9712_stat.rate);
    if ( actual_val != wm9712_stat.rate) {
      DEBUG(DBG_L1,"Set ADC rate error: %d->%d\n",wm9712_stat.rate,actual_val);
    }
  }

  DEBUG(DBG_L2, "Sampling rate: %d\n", wm9712_stat.rate);
  
  return ret;
}


/************************************************************
 * Volume
 ************************************************************/

int wm9712_set_output_volume(int left, int right)
{
  unsigned short real_left_vol,real_right_vol,val;

#ifdef CONFIG_PM
  audioLockFCS( ADEV_VOL, 1 );
#endif
  // speaker volume
  real_left_vol = SpVolToReal[left];
  real_right_vol = SpVolToReal[right];

  val = ac97_read(AC97_MASTER_VOL_STEREO);
  val &= ~(VOLL_MASK|VOLR_MASK);
  val |= (real_left_vol << VOLL_SHIFT) | real_right_vol;
  ac97_write(AC97_MASTER_VOL_STEREO,val);

  // head phone volume
  real_left_vol = HpVolToReal[left];
  real_right_vol = HpVolToReal[right];

  val = ac97_read(AC97_HEADPHONE_VOL);
  val &= ~(VOLL_MASK|VOLR_MASK);
  if (wm9712_stat.mic) { // minimize Lch volume
    val |= VOLL_MASK | real_right_vol;
  } else {
    val |= (real_left_vol << VOLL_SHIFT) | real_right_vol;
  }
  ac97_write(AC97_HEADPHONE_VOL,val);

  // pcm volume
  real_left_vol = DacVolToReal[left];
  real_right_vol = DacVolToReal[right];

  val = ac97_read(AC97_DAC_VOL);
  val &= ~DACVOL_MASK;
  val |= (real_left_vol << VOLL_SHIFT) | real_right_vol;
  ac97_write(AC97_DAC_VOL,val);

  wm9712_stat.out_left = left;
  wm9712_stat.out_right = right;

  // volume 0 means mute
  if ( (left==0 && right==0) && wm9712_stat.vol0!=TRUE) {
    wm9712_stat.vol0 = TRUE;
    set_mute_registers(DEV_OUTALL,TRUE);
  }
  if ( (left>0 || right>0) && wm9712_stat.vol0==TRUE) {
    wm9712_stat.vol0 = FALSE;
    set_audio_registers(wm9712_stat.last_jack_state);
  }

#ifdef CONFIG_PM
  audioLockFCS( ADEV_VOL, 0 );
#endif
  return 0;
}

int wm9712_set_input_gain(int left, int right)
{
  unsigned short real_left_vol,real_right_vol,val;

#ifdef CONFIG_PM
  audioLockFCS( ADEV_VOL, 1 );
#endif

  real_left_vol = MicVolToReal[left];
  real_right_vol = MicVolToReal[left]; // Rch use left volume

  val = ac97_read(AC97_RECORD_GAIN);
  val &= ~(VOLL_MASK|VOLR_MASK);
  val |= (real_left_vol << VOLL_SHIFT) | real_right_vol;
  ac97_write(AC97_RECORD_GAIN,val);

  wm9712_stat.in_left = left;
  wm9712_stat.in_right = right;

#ifdef CONFIG_PM
  audioLockFCS( ADEV_VOL, 0 );
#endif

  return 0;
}

/*
 * AGC
 */
static void wm9712_update_agc(int agc)
{
  unsigned short val;
#ifdef CONFIG_PM
  audioLockFCS( ADEV_VOL, 1 );
#endif
  if ( wm9712_stat.mode | SOUND_REC_MODE ) {
    val = ac97_read(AC97_NOISE_CTL);
    val &= ~ALC_MASK;
    if (agc) val |= ALC_ON;
    ac97_write(AC97_NOISE_CTL,val);
  }
#ifdef CONFIG_PM
  audioLockFCS( ADEV_VOL, 0 );
#endif

}

int wm9712_set_agc(int agc)
{
  if ( agc < 0 ) return wm9712_stat.agc;
  agc = !!agc;	/* should be 0 or 1 now */
  if ( wm9712_stat.agc != agc ) {
    wm9712_stat.agc = agc;
    wm9712_update_agc(wm9712_stat.agc);
  }
  return agc;
}

/*** Suspend/Resume  ***********************************************************/
#ifdef CONFIG_PM
void wm9712_suspend(void)
{
  DEBUG(DBG_L2, "in\n");
#ifdef CONFIG_PM
  audioLockFCS( ADEV_OFFON, 1 );
#endif
  wm9712_stat.suspend = 1;
  wm9712_exit();
#ifdef CONFIG_PM
  audioLockFCS( ADEV_OFFON, 0 );
#endif
  DEBUG(DBG_L2, "out\n");
}

void wm9712_resume(void)
{
  DEBUG(DBG_L2, "in\n");
#ifdef CONFIG_PM
  audioLockFCS( ADEV_OFFON, 1 );
#endif
  wm9712_init();
  if (wm9712_stat.open_play) {
    ac97_set_dac_rate(codec, (unsigned int)wm9712_stat.rate);
  }
  if (wm9712_stat.open_rec) {
    set_mute_registers(DEV_MIC,FALSE);
    ac97_set_adc_rate(codec, (unsigned int)wm9712_stat.rate);
  }
  wm9712_stat.suspend = 0;
  //X  wm9712_checkjack_wakeup();
  hp_wakeup_timer.expires = jiffies + HP_WAKEUP_DELAY;
  add_timer(&hp_wakeup_timer);
#ifdef CONFIG_PM
  audioLockFCS( ADEV_OFFON, 0 );
#endif
  DEBUG(DBG_L2, "out\n");
}
#endif	/* CONFIG_PM */
