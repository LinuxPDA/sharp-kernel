/*
 * linux/drivers/sound/pxa-i2s.c
 *
 * 32bit sound driver for Cotulla (SHARP)
 *
 * Based on pxa-audio.c
 * Based on dmasound_iris.c (IRIS)
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
 *	16-Jan-2003 SHARP sleep_on -> interruptible_sleep_on
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/sound.h>
#include <linux/soundcard.h>

#include <asm/mach/arch.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>
#include <linux/delay.h>

#include "pxa-audio.h"

#include <asm/arch/poodle_audio.h>
#include <asm/arch/poodle_wm8731.h>
#include <asm/arch/poodle_i2sc.h>

#include <linux/pm.h>

/*** Some declarations ***********************************************/
#define MY_TRACE	0

#define DBG_ALWAYS	0
#define DBG_LEVEL1	1
#define DBG_LEVEL2	2
#define DBG_LEVEL3	3

#ifdef MY_TRACE
#define PXAI2S_DBGPRINT(level, x, args...)  if ( level <= MY_TRACE ) printk("%s: " x,__FUNCTION__ ,##args)
#else
#define PXAI2S_DBGPRINT(level, x, args...)  if ( !level ) printk("%s: " x,__FUNCTION__ ,##args)
#endif


#define I2SSND_CORGI		1

#define SND_DEV_STATUS		6  /* /dev/sndstat   for Compatible for Collie */

#define AUDIO_NBFRAGS_DEFAULT	8
#define AUDIO_FRAGSIZE_DEFAULT	8192

#define MAX_DMA_SIZE		4096
#define DMA_DESC_SIZE		sizeof(pxa_dma_desc)

#define CORGI_DEFAULT_FREQUENCY	44100
#define CORGI_I2S_ADDRESS		(unsigned char)0x01

#define IOCTL_IN(arg, ret) \
	do { int error = get_user(ret, (int *)(arg)); \
		if (error) return error; \
	} while (0)
#define IOCTL_OUT(arg, ret)	ioctl_return((int *)(arg), ret)

#define WAIT_INIT_PLAY	( ( 2 * 1000 ) / 10 )
#define WAIT_PLAY1	( ( 50 ) / 10 )
#define WAIT_PLAY2	( ( 30 ) / 10 )

#define AUDIO_DATA	0x00


enum { CORGI_HP_AUDIO=0, CORGI_HP_BUZZER, CORGI_HP_SOUNDER, CORGI_HP_HP, CORGI_HP_BOTH, CORGI_HP_MIC, CORGI_HP_HEADSET};

#define AUDIO_BUFFER_SETUP_NOCLEAR	1
//#undef AUDIO_BUFFER_SETUP_NOCLEAR
#define AUDIO_ALWAYS_SP_ON			1
//#undef AUDIO_ALWAYS_SP_ON
//#define AUDIO_SP_DELAY_OFF		1
#undef AUDIO_SP_DELAY_OFF
#define SOUND_SUPPORT_REMOCON		1
#define DMA_NO_INITIALIZED		1
//#undef DMA_NO_INITIALIZED
#define ALARM_NO_MALLOC		1

#ifdef AUDIO_SP_DELAY_OFF
#define AUDIO_SP_DELAY_TIME	(HZ*15)
#endif

#ifdef SOUND_SUPPORT_REMOCON
#include <asm/sharp_char.h>
#endif
/**** valiables **********************************************************/
#ifdef CONFIG_PM
static struct pm_dev* pxa_sound_pm_dev;
#endif

static ssize_t (*ct_func)(const u_char *, size_t, u_char *, ssize_t *, ssize_t) = NULL;
static ssize_t (*ct_read_func)(const u_char *, size_t, u_char *, ssize_t *, ssize_t) = NULL;


static DECLARE_WAIT_QUEUE_HEAD(hp_proc);
static DECLARE_WAIT_QUEUE_HEAD(hp_queue);

static int isPXAI2SReady = 0;
static int isChkHPstatus = 0;
int corgi_HPstatus = HPJACK_STATE_UNDETECT;
#ifdef SOUND_SUPPORT_REMOCON
struct timer_list remocon_chk_timers;
#endif

static int corgi_intmode = 0;
static int corgi_intmode_speed = 0;
static int corgi_intmode_direct = 0;
static short *int_data = NULL ;
static short *int_data_org = NULL ;
#if ALARM_NO_MALLOC
static int corgi_intmode_cur = 0;
static int corgi_intmode_step = 0;
#endif
#ifdef AUDIO_SP_DELAY_OFF
struct timer_list speaker_timers;
#endif

/**** Prototype *******************************************************/
static void CorgiInit(void);
static int CorgiSetVolume(int);
static int CorgiSetStereo(int);
static int CorgiSetFormat(int);
static int CorgiSetGain(int);
static int CorgiGetGain(void);
static int CorgiSetFormat(int);
static int CorgiGetVolume(void);
static int CorgiSetFreq(int);
static int mixer_ioctl( struct inode *, struct file *,unsigned int, unsigned long);
static void corgi_hp_mode(int mode);
static void corgi_intmode_mix(unsigned char *,int);
#ifdef CONFIG_PM
static int CorgiPMCallBack(struct pm_dev *,pm_request_t, void *);
#endif
#ifdef AUDIO_BUFFER_SETUP_NOCLEAR
static int __audio_setup_buf(audio_stream_t * s, int is_clr);
#endif
#ifdef AUDIO_SP_DELAY_OFF
static void corgi_sp_delay_off(void);
static void corgi_sounder_timer(unsigned long dummy);
#endif
#ifdef SOUND_SUPPORT_REMOCON
static void corgi_remocon_chk_timer(unsigned long dummy);
extern int	get_remocon_state( void );
#endif


/**** Machine definitions ************************************************/
typedef struct {
	ssize_t (*ct_ulaw)(const u_char *, size_t, u_char *, ssize_t *, ssize_t);
	ssize_t (*ct_alaw)(const u_char *, size_t, u_char *, ssize_t *, ssize_t);
	ssize_t (*ct_s8)(const u_char *, size_t, u_char *, ssize_t *, ssize_t);
	ssize_t (*ct_u8)(const u_char *, size_t, u_char *, ssize_t *, ssize_t);
	ssize_t (*ct_s16be)(const u_char *, size_t, u_char *, ssize_t *, ssize_t);
	ssize_t (*ct_u16be)(const u_char *, size_t, u_char *, ssize_t *, ssize_t);
	ssize_t (*ct_s16le)(const u_char *, size_t, u_char *, ssize_t *, ssize_t);
	ssize_t (*ct_u16le)(const u_char *, size_t, u_char *, ssize_t *, ssize_t);
} TRANS;

typedef struct {
	int type;
	void (*init)(void);
	void (*silence)(void);
	int (*setFormat)(int);
	int (*setVolume)(int);
	int (*getVolume)(void);
	int (*setStereo)(int);
	int (*setBass)(int);
	int (*setTreble)(int);
	int (*setGain)(int);
	int (*getGain)(void);
	int (*setFreq)(int);
	void (*play)(void);
} PLATFORM;

static PLATFORM machCorgi = {
	I2SSND_CORGI,		// int type
	CorgiInit,		// void  init(void)
	NULL,			// void  silence(void)
	CorgiSetFormat,	// int   setFormat(int)
	CorgiSetVolume,	// int   setVolume(int)
	CorgiGetVolume,	// int   getVolume(void)
	CorgiSetStereo,	// int   setStereo(int)
	NULL,			// int   setBass(int)
	NULL,			// int   setTreble(int)
	CorgiSetGain,		// int   setGain(int)
	CorgiGetGain,		// int   getGain(void)
	CorgiSetFreq,		// int   setFreq(int)
	NULL,			// void  play(void)
};


/**** /dev/ setting ********************************************/
/*	/dev/dsp	*/
struct sound_settings {
  PLATFORM  mach;	/* platform dependent things */
  TRANS *trans;		/* supported translations */
  TRANS *trans_read;	/* supported translations */

  int format;		/* AFMT_* */
  int stereo;		/* 0 = mono, 1 = stereo */
  int size;		/* 8/16 bit*/
  int freq;		/* freq */

  int volume_left;	/* volume (range is 0 - 100%) */
  int volume_right;
  int gain_left;	/* input gain (range is 0 - 100%) */
  int gain_right;

  int bass;		/* tone (not support) */
  int treble;

  /* device independ */
  int gain_shift;	/* input gain shift */
  int gain;		/* input gain */
  int hw_freq;		/* hw freq */

};
static struct sound_settings sound;

/*	/dev/mixer	*/
struct sound_mixer {
  int dev_mixer;
  int busy;
};
static struct sound_mixer pxa_i2s_codec;

/*	/dev/sndstat	*/
struct sound_state {
  int dev_state;
  int busy;
  char buf[512];
  int len, ptr;
};
static struct sound_state pxa_i2s_state;


static audio_stream_t pxa_audio_out = {
	name:			"I2S audio out",
	dcmd:			DCMD_TXPCDR,
	drcmr:			&DRCMRTXSADR,
	dev_addr:		__PREG(I2S_SADR),
};

static audio_stream_t pxa_audio_in = {
	name:			"I2S audio in",
	dcmd:			DCMD_RXPCDR,
	drcmr:			&DRCMRRXSADR,
	dev_addr:		__PREG(I2S_SADR),
};

static audio_state_t pxa_audio_state = {
	output_stream:		&pxa_audio_out,
	input_stream:		&pxa_audio_in,
	sem:			__MUTEX_INITIALIZER(pxa_audio_state.sem),
};

typedef struct _input_gain_t {
  int shift;
  int gain;
} input_gain_t;

static input_gain_t input_gain[7] = {
  {  1 ,  0 },
  {  2 ,  0 },
  {  0 ,  1 },
  {  1 ,  1 },
  {  2 ,  1 },
  {  3 ,  1 },
  {  4 ,  1 }
};

#ifdef AUDIO_SP_DELAY_OFF
static void corgi_sp_delay_off(void)
{
  if ( !corgi_HPstatus ) { 	// Not Insert HP
	  del_timer(&speaker_timers);
	  speaker_timers.function = corgi_sounder_timer;
	  speaker_timers.expires = jiffies + AUDIO_SP_DELAY_TIME;
	  add_timer(&speaker_timers);
  }
}

static void corgi_sounder_timer(unsigned long dummy)
{
	PXAI2S_DBGPRINT(DBG_LEVEL2, "Sounder off\n");
	reset_scoop_gpio( SCP_APM_ON );
	del_timer(&speaker_timers);
}
#endif

// FIXME !
/**** data convert ****************************************************/

/* 16 bit mu-law */

static short ulaw2dma16[] = {
	-32124,	-31100,	-30076,	-29052,	-28028,	-27004,	-25980,	-24956,
	-23932,	-22908,	-21884,	-20860,	-19836,	-18812,	-17788,	-16764,
	-15996,	-15484,	-14972,	-14460,	-13948,	-13436,	-12924,	-12412,
	-11900,	-11388,	-10876,	-10364,	-9852,	-9340,	-8828,	-8316,
	-7932,	-7676,	-7420,	-7164,	-6908,	-6652,	-6396,	-6140,
	-5884,	-5628,	-5372,	-5116,	-4860,	-4604,	-4348,	-4092,
	-3900,	-3772,	-3644,	-3516,	-3388,	-3260,	-3132,	-3004,
	-2876,	-2748,	-2620,	-2492,	-2364,	-2236,	-2108,	-1980,
	-1884,	-1820,	-1756,	-1692,	-1628,	-1564,	-1500,	-1436,
	-1372,	-1308,	-1244,	-1180,	-1116,	-1052,	-988,	-924,
	-876,	-844,	-812,	-780,	-748,	-716,	-684,	-652,
	-620,	-588,	-556,	-524,	-492,	-460,	-428,	-396,
	-372,	-356,	-340,	-324,	-308,	-292,	-276,	-260,
	-244,	-228,	-212,	-196,	-180,	-164,	-148,	-132,
	-120,	-112,	-104,	-96,	-88,	-80,	-72,	-64,
	-56,	-48,	-40,	-32,	-24,	-16,	-8,	0,
	32124,	31100,	30076,	29052,	28028,	27004,	25980,	24956,
	23932,	22908,	21884,	20860,	19836,	18812,	17788,	16764,
	15996,	15484,	14972,	14460,	13948,	13436,	12924,	12412,
	11900,	11388,	10876,	10364,	9852,	9340,	8828,	8316,
	7932,	7676,	7420,	7164,	6908,	6652,	6396,	6140,
	5884,	5628,	5372,	5116,	4860,	4604,	4348,	4092,
	3900,	3772,	3644,	3516,	3388,	3260,	3132,	3004,
	2876,	2748,	2620,	2492,	2364,	2236,	2108,	1980,
	1884,	1820,	1756,	1692,	1628,	1564,	1500,	1436,
	1372,	1308,	1244,	1180,	1116,	1052,	988,	924,
	876,	844,	812,	780,	748,	716,	684,	652,
	620,	588,	556,	524,	492,	460,	428,	396,
	372,	356,	340,	324,	308,	292,	276,	260,
	244,	228,	212,	196,	180,	164,	148,	132,
	120,	112,	104,	96,	88,	80,	72,	64,
	56,	48,	40,	32,	24,	16,	8,	0,
};

/* 16 bit A-law */

static short alaw2dma16[] = {
	-5504,	-5248,	-6016,	-5760,	-4480,	-4224,	-4992,	-4736,
	-7552,	-7296,	-8064,	-7808,	-6528,	-6272,	-7040,	-6784,
	-2752,	-2624,	-3008,	-2880,	-2240,	-2112,	-2496,	-2368,
	-3776,	-3648,	-4032,	-3904,	-3264,	-3136,	-3520,	-3392,
	-22016,	-20992,	-24064,	-23040,	-17920,	-16896,	-19968,	-18944,
	-30208,	-29184,	-32256,	-31232,	-26112,	-25088,	-28160,	-27136,
	-11008,	-10496,	-12032,	-11520,	-8960,	-8448,	-9984,	-9472,
	-15104,	-14592,	-16128,	-15616,	-13056,	-12544,	-14080,	-13568,
	-344,	-328,	-376,	-360,	-280,	-264,	-312,	-296,
	-472,	-456,	-504,	-488,	-408,	-392,	-440,	-424,
	-88,	-72,	-120,	-104,	-24,	-8,	-56,	-40,
	-216,	-200,	-248,	-232,	-152,	-136,	-184,	-168,
	-1376,	-1312,	-1504,	-1440,	-1120,	-1056,	-1248,	-1184,
	-1888,	-1824,	-2016,	-1952,	-1632,	-1568,	-1760,	-1696,
	-688,	-656,	-752,	-720,	-560,	-528,	-624,	-592,
	-944,	-912,	-1008,	-976,	-816,	-784,	-880,	-848,
	5504,	5248,	6016,	5760,	4480,	4224,	4992,	4736,
	7552,	7296,	8064,	7808,	6528,	6272,	7040,	6784,
	2752,	2624,	3008,	2880,	2240,	2112,	2496,	2368,
	3776,	3648,	4032,	3904,	3264,	3136,	3520,	3392,
	22016,	20992,	24064,	23040,	17920,	16896,	19968,	18944,
	30208,	29184,	32256,	31232,	26112,	25088,	28160,	27136,
	11008,	10496,	12032,	11520,	8960,	8448,	9984,	9472,
	15104,	14592,	16128,	15616,	13056,	12544,	14080,	13568,
	344,	328,	376,	360,	280,	264,	312,	296,
	472,	456,	504,	488,	408,	392,	440,	424,
	88,	72,	120,	104,	24,	8,	56,	40,
	216,	200,	248,	232,	152,	136,	184,	168,
	1376,	1312,	1504,	1440,	1120,	1056,	1248,	1184,
	1888,	1824,	2016,	1952,	1632,	1568,	1760,	1696,
	688,	656,	752,	720,	560,	528,	624,	592,
	944,	912,	1008,	976,	816,	784,	880,	848,
};


static ssize_t corgi_ct_law(const u_char *userPtr, size_t userCount,
			     u_char frame[], ssize_t *frameUsed,
			     ssize_t frameLeft)
{
	short *table = sound.format == AFMT_MU_LAW ? ulaw2dma16: alaw2dma16;
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int val, stereo = sound.stereo;

	frameLeft >>= 2;
	if (stereo)
		userCount >>= 1;
	used = count = min(userCount, frameLeft);
	while (count > 0) {
	  u_char data;
	  if (get_user(data, userPtr++)) {
	    return -EFAULT;
	  }
	  val = table[data];
	  *p++ = val;		/* Left Ch. */
	  if (stereo) {
	    if (get_user(data, userPtr++)) {
	      return -EFAULT;
	    }
	    val = table[data];
	  }
	  *p++ = val;		/* Right Ch. */
	  count--;
	}
	*frameUsed += used * 4;
	return stereo? used * 2: used;
}


static ssize_t corgi_ct_u8(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
  int ch = (sound.stereo)?2:1;
  unsigned char *src = (unsigned char *)userPtr;
  short *dstct = (short *)frame;
  short tmpval;

  if ( sound.freq != sound.hw_freq ) {
    int frameMax = frameLeft >> 2;
    int arrayMax = (userCount>>(ch-1));
    int userMax = ( arrayMax * sound.hw_freq ) / sound.freq;
    int samplingCount = min(frameMax,userMax);
    int whichLimit = (samplingCount==frameMax)?1:0; // which is minimum?
    short *dstend = (short*)((int)frame+samplingCount*2*2);
    unsigned char *srcval;
    int idx;
      
    int cur=0,step = (sound.freq << 8 ) / sound.hw_freq; // fixed point (x256)

    while (dstct<dstend) {
      idx = (cur>>8);
      if (idx>=arrayMax) {
		  idx=arrayMax-1;
      }
      srcval = &src[idx*ch];
      if (get_user(tmpval,srcval)) {
	return -EFAULT;
      }
      *dstct++ = (tmpval-128)<<8;
      if (ch>1) srcval++;
      if (get_user(tmpval,srcval)) {
	return -EFAULT;
      }
      *dstct++ = (tmpval-128)<<8;
      cur += step;
    }

    *frameUsed = samplingCount*2*2; // // dma is always stereo 16bit
    if (whichLimit) { // dma limit
      return (cur>>8)*ch; // byte
    } else { // user buffer limit
      return userCount; // byte
    }

  } else { // hw_freq == freq
    int frameMax = frameLeft >> 2;
    int userMax = userCount >> (ch-1);
    int idx,samplingCount = min(frameMax,userMax);

    for (idx=0; idx<samplingCount; idx++) {
      if (get_user(tmpval,src)) {
	return -EFAULT;
      }
      *dstct++ = (tmpval-128)<<8;
      if (ch>1) src++;
      if (get_user(tmpval,src++)) {
	return -EFAULT;
      }
      *dstct++ = (tmpval-128)<<8;
    }

    *frameUsed = samplingCount * 2 * 2; // dma is always stereo 16bit
    return samplingCount * ch;
  }
}


static ssize_t corgi_ct_s16(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
  int ch = (sound.stereo)?2:1;
  short *src = (short *)userPtr;
  short *dstct = (short *)frame;

  if ( sound.freq != sound.hw_freq ) {
    int frameMax = frameLeft >> 2;
    int arrayMax = (userCount>>ch);
    int userMax = ( arrayMax * sound.hw_freq ) / sound.freq;
    int samplingCount = min(frameMax,userMax);
    int whichLimit = (samplingCount==frameMax)?1:0; // which is minimum?
    short *dstend = (short*)((int)frame+samplingCount*2*2);
    short *srcval;
    int idx;
      
    int cur=0,step = (sound.freq << 8 ) / sound.hw_freq; // fixed point (x256)

    while (dstct<dstend) {
      idx = (cur>>8);
      if (idx>=arrayMax) {
		  idx=arrayMax-1;
      }
      srcval = &src[idx*ch];
      if (get_user(*dstct++,srcval)) {
	return -EFAULT;
      }
      if (ch>1) srcval++;
      if (get_user(*dstct++,srcval)) {
	return -EFAULT;
      }
      cur += step;
    }

    *frameUsed = samplingCount*2*2; // // dma is always stereo 16bit
    if (whichLimit) { // dma limit
      return (cur>>8)*ch*2; // byte
    } else { // user buffer limit
      return userCount; // byte
    }

  } else { // hw_freq == freq
    int frameMax = frameLeft >> 2;
    int userMax = userCount >> ch;
    int samplingCount = min(frameMax,userMax);

    if (ch>1) { // stereo
	if (copy_from_user(dstct,src,samplingCount * ch * 2)) {
	    return -EFAULT;
	}
    } else { // mono
	int idx;
	for (idx=0; idx<samplingCount; idx++) {
	  if (get_user(*dstct++,src) || get_user(*dstct++,src++)) {
	    return -EFAULT;
	  }
	}
    }

    *frameUsed = samplingCount * 2 * 2; // dma is always stereo 16bit
    return samplingCount * ch * 2;
  }
}

static TRANS transCorgi = {
  corgi_ct_law,corgi_ct_law,0,corgi_ct_u8,
  0,0,corgi_ct_s16,0
};

/**** data convert for recording ****************************************************/

static ssize_t corgi_ct_read_u8(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
  int ch = (sound.stereo)?2:1;
  unsigned char *usrbuf = (unsigned char *)userPtr;
  short *dmabuf = (short *)frame;
  short tmpval;

  if ( sound.freq != sound.hw_freq ) {
    int arrayMax = (frameLeft >> 2);
    int dmaMax = (arrayMax * sound.freq) / sound.hw_freq; // dma is always stereo 16bit
    int userMax = userCount >> (ch-1); // 8bit
    int samplingCount = min(dmaMax,userMax);
    int whichLimit = (samplingCount==dmaMax)?1:0; // which is minimum?
    unsigned char *usrend = (unsigned char*)((int)usrbuf+samplingCount*ch);
    short *srcval;
    int idx;
    int cur=0,step = (sound.hw_freq << 8 ) / sound.freq; // fixed point (x256)
    dmaMax = frameLeft>>2; // array max index

    while (usrbuf<usrend) {
      idx = (cur>>8);
      if (idx>=arrayMax) { // over flow?
	idx = arrayMax-1;
      }
      srcval = &dmabuf[idx*2];
      tmpval = (*srcval>>(8-sound.gain_shift));
      if (tmpval>127) tmpval = 127;
      else if (tmpval<-128) tmpval = -128;
      if (put_user((unsigned char)(tmpval+128),usrbuf++)) {
	return -EFAULT;
      }
      srcval++;
      if (ch>1) {
	tmpval = (*srcval>>(8-sound.gain_shift));
	if (tmpval>127) tmpval = 127;
	else if (tmpval<-128) tmpval = -128;
	if (put_user((unsigned char)(tmpval+128),usrbuf++)) {
	  return -EFAULT;
	}
      }
      cur += step;
    }
    *frameUsed = (whichLimit)?(frameLeft):((cur>>8) * 2 * 2); //  dma is always stereo
    return samplingCount * ch;

  } else { // hw_freq == freq
    int dmaMax = frameLeft >> 2; // dma is always stereo
    int userMax = userCount >> ch;
    int ct,samplingCount = min(dmaMax,userMax);

    for (ct=0; ct<samplingCount; ct++) {
      tmpval = (*dmabuf>>(8-sound.gain_shift));
      if (tmpval>127) tmpval = 127;
      else if (tmpval<-128) tmpval = -128;
      if (put_user((unsigned char)(tmpval+128),usrbuf++)) { // left channel
	return -EFAULT;
      }
      dmabuf++;
      if (ch>1) { // right channel
	tmpval = (*dmabuf>>(8-sound.gain_shift));
	if (tmpval>127) tmpval = 127;
	else if (tmpval<-128) tmpval = -128;
	if (put_user((unsigned char)(tmpval+128),usrbuf++)) {
	  return -EFAULT;
	}
      }
      dmabuf++;
    }
    *frameUsed = samplingCount * 2 * 2; // dma is always stereo
    return samplingCount * ch;
  }
}

static ssize_t corgi_ct_read_s16(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
  int ch = (sound.stereo)?2:1;
  short *usrbuf = (short *)userPtr;
  short *dmabuf = (short *)frame;

  if ( sound.freq != sound.hw_freq ) {
    int arrayMax = (frameLeft >> 2);
    int dmaMax = (arrayMax * sound.freq) / sound.hw_freq; // dma is always stereo
    int userMax = userCount >> ch;
    int samplingCount = min(dmaMax,userMax);
    int whichLimit = (samplingCount==dmaMax)?1:0; // which is minimum?
    short *usrend = (short*)((int)usrbuf+samplingCount*ch*2);
    short *srcval;
    int idx;
    int cur=0,step = (sound.hw_freq << 8 ) / sound.freq; // fixed point (x256)
    dmaMax = frameLeft>>2; // array max index

    while (usrbuf<usrend) {
      idx = (cur>>8);
      if (idx>=arrayMax) { // over flow?
	idx = arrayMax-1;
      }
      srcval = &dmabuf[idx*2];
      if (put_user((*srcval<<sound.gain_shift),usrbuf++)) {
	return -EFAULT;
      }
      srcval++;
      if (ch>1) {
	if (put_user((*srcval<<sound.gain_shift),usrbuf++)) {
	  return -EFAULT;
	}
      }
      cur += step;
    }
    *frameUsed = (whichLimit)?(frameLeft):((cur>>8) * 2 * 2); //  dma is always stereo
    return samplingCount * ch * 2;

  } else { // hw_freq == freq
    int dmaMax = frameLeft >> 2; // dma is always stereo
    int userMax = userCount >> ch;
    int ct,samplingCount = min(dmaMax,userMax);

    for (ct=0; ct<samplingCount; ct++) {
      if (put_user((*dmabuf<<sound.gain_shift),usrbuf++)) { // left channel
	return -EFAULT;
      }
      dmabuf++;
      if (ch>1) { // right channel
	if (put_user((*dmabuf<<sound.gain_shift),usrbuf++)) {
	  return -EFAULT;
	}
      }
      dmabuf++;
    }
    *frameUsed = samplingCount * 2 * 2; // dma is always stereo
    return samplingCount * ch * 2;
  }
}

static TRANS transReadCorgi = {
  0,0,0,corgi_ct_read_u8,
  0,0,corgi_ct_read_s16,0
};

/**** low level stuff ****************************************************/

/*** Common stuff ********************************************************/
static inline int ioctl_return(int *addr, int value)
{
	if (value < 0) {
		return(value);
	}
	return put_user(value, addr)? -EFAULT: 0;
}

static inline void corgi_i2s_write(unsigned int data)
{
  if ( I2S_SASR0 & SASR0_TUR )
    I2S_SAICR = SASR0_TUR;
  while( ( I2S_SASR0 & SASR0_TNF ) == 0 ) {};
  I2S_SADR = data;
}


/*
 * This function frees all buffers
 */
#define audio_clear_buf pxa_audio_clear_buf

void pxa_audio_clear_buf(audio_stream_t * s)
{
	DECLARE_WAITQUEUE(wait, current);
	int frag;

	if (!s->buffers)
		return;

	/* Ensure DMA isn't running */
	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(&s->stop_wq, &wait);
	DCSR(s->dma_ch) = DCSR_STOPIRQEN;
	schedule();
	remove_wait_queue(&s->stop_wq, &wait);

	/* free DMA buffers */
	for (frag = 0; frag < s->nbfrags; frag++) {
		audio_buf_t *b = &s->buffers[frag];
		if (!b->master)
			continue;
		consistent_free(b->data, b->master, b->dma_desc->dsadr);
	}

	/* free descriptor ring */
	if (s->buffers->dma_desc)
		consistent_free(s->buffers->dma_desc, 
				s->nbfrags * s->descs_per_frag * DMA_DESC_SIZE,
				s->dma_desc_phys);

	/* free buffer structure array */
	kfree(s->buffers);
	s->buffers = NULL;
}

/*
 * This function allocates the DMA descriptor array and buffer data space
 * according to the current number of fragments and fragment size.
 */
static int audio_setup_buf(audio_stream_t * s)
#ifdef AUDIO_BUFFER_SETUP_NOCLEAR
{
	return __audio_setup_buf(s, 1);
}

static int __audio_setup_buf(audio_stream_t * s, int is_clr)
#endif
{
	pxa_dma_desc *dma_desc;
	dma_addr_t dma_desc_phys;
	int nb_desc, frag, i, buf_size = 0;
	char *dma_buf = NULL;
	dma_addr_t dma_buf_phys = 0;


	PXAI2S_DBGPRINT(DBG_LEVEL2, "Enter audio_setup buffer\n");

	if (s->buffers)
		return -EBUSY;

	/* Our buffer structure array */
	s->buffers = kmalloc(sizeof(audio_buf_t) * s->nbfrags, GFP_KERNEL);
	if (!s->buffers)
		goto err;
	memzero(s->buffers, sizeof(audio_buf_t) * s->nbfrags);

	/* 
	 * Our DMA descriptor array:
	 * for Each fragment we have one checkpoint descriptor plus one 
	 * descriptor per MAX_DMA_SIZE byte data blocks.
	 */
	nb_desc = (1 + (s->fragsize + MAX_DMA_SIZE - 1)/MAX_DMA_SIZE) * s->nbfrags;
	dma_desc = consistent_alloc(GFP_KERNEL,
				    nb_desc * DMA_DESC_SIZE,
				    &dma_desc_phys);
	if (!dma_desc)
		goto err;
	s->descs_per_frag = nb_desc / s->nbfrags;
	s->buffers->dma_desc = dma_desc;
	s->dma_desc_phys = dma_desc_phys;
	for (i = 0; i < nb_desc - 1; i++)
		dma_desc[i].ddadr = dma_desc_phys + (i + 1) * DMA_DESC_SIZE;
	dma_desc[i].ddadr = dma_desc_phys;


	/* Our actual DMA buffers */
	for (frag = 0; frag < s->nbfrags; frag++) {
		audio_buf_t *b = &s->buffers[frag];

		/*
		 * Let's allocate non-cached memory for DMA buffers.
		 * We try to allocate all memory at once.
		 * If this fails (a common reason is memory fragmentation),
		 * then we'll try allocating smaller buffers.
		 */
		if (!buf_size) {
			buf_size = (s->nbfrags - frag) * s->fragsize;
			do {
				dma_buf = consistent_alloc(GFP_KERNEL,
							   buf_size, 
							   &dma_buf_phys);
				if (!dma_buf)
					buf_size -= s->fragsize;
			} while (!dma_buf && buf_size);
			if (!dma_buf)
				goto err;
			b->master = buf_size;
#ifdef AUDIO_BUFFER_SETUP_NOCLEAR
			if( is_clr ){
				memzero(dma_buf, buf_size);
			}
#else
			memzero(dma_buf, buf_size);
#endif
		}

		/* 
		 * Set up our checkpoint descriptor.  Since the count 
		 * is always zero, we'll abuse the dsadr and dtadr fields
		 * just in case this one is picked up by the hardware
		 * while processing SOUND_DSP_GETPTR.
		 */
		dma_desc->dsadr = dma_buf_phys;
		dma_desc->dtadr = dma_buf_phys;
		dma_desc->dcmd = DCMD_ENDIRQEN;
		if (s->output && !s->mapped)
			dma_desc->ddadr |= DDADR_STOP;
		b->dma_desc = dma_desc++;

		/* set up the actual data descriptors */
		for (i = 0; (i * MAX_DMA_SIZE) < s->fragsize; i++) {
			dma_desc[i].dsadr = (s->output) ?
				(dma_buf_phys + i*MAX_DMA_SIZE) : s->dev_addr;
			dma_desc[i].dtadr = (s->output) ?
				s->dev_addr : (dma_buf_phys + i*MAX_DMA_SIZE);
			dma_desc[i].dcmd = s->dcmd |
				((s->fragsize < MAX_DMA_SIZE) ?
					s->fragsize : MAX_DMA_SIZE);

			PXAI2S_DBGPRINT(DBG_LEVEL3, "dsadr[%8x],dtadr[%8x],dcmd[%8x]\n",
					dma_desc[i].dsadr,dma_desc[i].dtadr,dma_desc[i].dcmd);

		}
		dma_desc += i;

		/* handle buffer pointers */
		b->data = dma_buf;
		dma_buf += s->fragsize;
		dma_buf_phys += s->fragsize;
		buf_size -= s->fragsize;
	}

	s->usr_frag = s->dma_frag = 0;
	s->bytecount = 0;
	s->fragcount = 0;
	sema_init(&s->sem, (s->output) ? s->nbfrags : 0);
	return 0;

err:
	PXAI2S_DBGPRINT(DBG_ALWAYS, "unable to allocate audio memory\n ");
	audio_clear_buf(s);
	return -ENOMEM;
}

/*
 * Our DMA interrupt handler
 */
static void audio_dma_irq(int ch, void *dev_id, struct pt_regs *regs)
{
	audio_stream_t *s = dev_id;
	u_int dcsr;

	dcsr = DCSR(ch);
	DCSR(ch) = dcsr & ~DCSR_STOPIRQEN;


	PXAI2S_DBGPRINT(DBG_LEVEL2, "dcsr(ch) = %8x(%d)\n",dcsr,ch);

	if (!s->buffers) {
	  PXAI2S_DBGPRINT(DBG_ALWAYS, "I2S DMA: wow... received IRQ for channel %d but no buffer exists\n", ch);
	  return;
	}

	if (dcsr & DCSR_BUSERR)
	  PXAI2S_DBGPRINT(DBG_ALWAYS, "I2S DMA: bus error interrupt on channel %d\n", ch);

	if (dcsr & DCSR_ENDINTR) {
		u_long cur_dma_desc;
		u_int cur_dma_frag;

		/* 
		 * Find out which DMA desc is current.  Note that DDADR
		 * points to the next desc, not the current one.
		 */
		cur_dma_desc = DDADR(ch) - s->dma_desc_phys - DMA_DESC_SIZE;

		/*
		 * Let the compiler nicely optimize constant divisors into
		 * multiplications for the common cases which is much faster.
		 * Common cases: x = 1 + (1 << y) for y = [0..3]
		 */
		switch (s->descs_per_frag) {
		case 2:  cur_dma_frag = cur_dma_desc / (2*DMA_DESC_SIZE); break;
		case 3:  cur_dma_frag = cur_dma_desc / (3*DMA_DESC_SIZE); break;
		case 5:  cur_dma_frag = cur_dma_desc / (5*DMA_DESC_SIZE); break;
		case 9:  cur_dma_frag = cur_dma_desc / (9*DMA_DESC_SIZE); break;
		default: cur_dma_frag =
			    cur_dma_desc / (s->descs_per_frag * DMA_DESC_SIZE);
		}

		/* Account for possible wrap back of cur_dma_desc above */
		if (cur_dma_frag >= s->nbfrags)
			cur_dma_frag = s->nbfrags - 1;

		while (s->dma_frag != cur_dma_frag) {
			if (!s->mapped) {
				/* 
				 * This fragment is done - set the checkpoint
				 * descriptor to STOP until it is gets
				 * processed by the read or write function.
				 */
				s->buffers[s->dma_frag].dma_desc->ddadr |= DDADR_STOP;
				up(&s->sem);
			}
			if (++s->dma_frag >= s->nbfrags)
				s->dma_frag = 0;

			/* Accounting */
			s->bytecount += s->fragsize;
			s->fragcount++;
		}

		/* ... and for polling processes */
		wake_up(&s->frag_wq);
	}

	if ((dcsr & DCSR_STOPIRQEN) && (dcsr & DCSR_STOPSTATE))
		wake_up(&s->stop_wq);
}

/*
 * Validate and sets up buffer fragments, etc.
 */
static int audio_set_fragments(audio_stream_t *s, int val)
{
	if (s->mapped || DCSR(s->dma_ch) & DCSR_RUN)
		return -EBUSY;
	if (s->buffers)
		audio_clear_buf(s);
	s->nbfrags = (val >> 16) & 0x7FFF;
	val &= 0xffff;
	if (val < 5)
		val = 5;
	if (val > 15)
		val = 15;
	s->fragsize = 1 << val;
	if (s->nbfrags < 2)
		s->nbfrags = 2;
	if (s->nbfrags * s->fragsize > 256 * 1024)
		s->nbfrags = 256 * 1024 / s->fragsize;
	if (audio_setup_buf(s))
		return -ENOMEM;
	return val|(s->nbfrags << 16);
}


/*
 * The fops functions
 */

static int audio_write(struct file *file, const char *buffer,
		       size_t count, loff_t * ppos)
{
	const char *buffer0 = buffer;
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s = state->output_stream;
	int ret = 0;
	ssize_t uUsed, bUsed, bLeft = 0;
	u_char *dest;


	PXAI2S_DBGPRINT(DBG_LEVEL2, "Enter audio_write\n");

	// wait playing int sound.
	while(corgi_intmode_direct)
	  schedule();

	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (s->mapped)
		return -ENXIO;
#ifdef AUDIO_BUFFER_SETUP_NOCLEAR
	if (!s->buffers && __audio_setup_buf(s,0))
		return -ENOMEM;
#else
	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;
#endif

	while (count > 0) {
		audio_buf_t *b = &s->buffers[s->usr_frag];


	        PXAI2S_DBGPRINT(DBG_LEVEL3, "audio_write count = %d\n",count);

		/* Grab a fragment */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&s->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&s->sem))
				break;
		}

		/* Feed the current buffer */
		dest = b->data + b->offset;
		bUsed = 0;
		bLeft = s->fragsize - b->offset;

		if (ct_func) {
		  uUsed = ct_func(buffer, count, dest, &bUsed, bLeft);
		} else {
		  return -EFAULT;
		}

		if ( corgi_intmode ) {
		  corgi_intmode_mix(dest,bUsed);
		}

		if (uUsed < 0) {
		  up(&s->sem);
		  return -EFAULT;
		}
		b->offset += bUsed;
		buffer += uUsed;
		count -= uUsed;

		if (b->offset < s->fragsize) {
			up(&s->sem);
			break;
		}
		/* 
		 * Activate DMA on current buffer.
		 * We unlock this fragment's checkpoint descriptor and
		 * kick DMA if it is idle.  Using checkpoint descriptors
		 * allows for control operations without the need for 
		 * stopping the DMA channel if it is already running.
		 */
		b->offset = 0;
		b->dma_desc->ddadr &= ~DDADR_STOP;
#ifdef DMA_NO_INITIALIZED
		if ((DCSR(s->dma_ch) & DCSR_STOPSTATE)||
			!(DCSR(s->dma_ch) & DCSR_RUN)){
		  DDADR(s->dma_ch) = b->dma_desc->ddadr;
		  DCSR(s->dma_ch) = DCSR_RUN;
		}
#else
		if (DCSR(s->dma_ch) & DCSR_STOPSTATE) {
		  DDADR(s->dma_ch) = b->dma_desc->ddadr;
		  DCSR(s->dma_ch) = DCSR_RUN;
		}
#endif

		/* move the index to the next fragment */
		if (++s->usr_frag >= s->nbfrags)
			s->usr_frag = 0;
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	return ret;
}


static int audio_read(struct file *file, char *buffer,
		      size_t count, loff_t * ppos)
{
	char *buffer0 = buffer;
	audio_state_t *state = file->private_data;
	audio_stream_t *s = state->input_stream;
	int ret = 0;
	ssize_t uUsed, bUsed, bLeft = 0;
	u_char *dest;

	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (s->mapped)
		return -ENXIO;
	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;

	while (count > 0) {
		audio_buf_t *b = &s->buffers[s->usr_frag];

		/* prime DMA */
#ifdef DMA_NO_INITIALIZED
		if ((DCSR(s->dma_ch) & DCSR_STOPSTATE)||
			!(DCSR(s->dma_ch) & DCSR_RUN)){
			DDADR(s->dma_ch) = 
				s->buffers[s->dma_frag].dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}
#else
		if (DCSR(s->dma_ch) & DCSR_STOPSTATE) {
			DDADR(s->dma_ch) = 
				s->buffers[s->dma_frag].dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}
#endif

		/* Wait for a buffer to become full */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&s->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&s->sem))
				break;
		}

		/* Grab data from current buffer */

		dest = b->data + b->offset;
		bUsed = 0;
		bLeft = s->fragsize - b->offset;

		if (ct_read_func) {
		  uUsed = ct_read_func(buffer, count, dest, &bUsed, bLeft);
		} else {
		  return -EFAULT;
		}

		if (uUsed < 0) {
		  up(&s->sem);
		  return -EFAULT;
		}
		b->offset += bUsed;
		buffer += uUsed;
		count -= uUsed;

		if (b->offset < s->fragsize) {
			up(&s->sem);
			break;
		}

		/* 
		 * Make this buffer available for DMA again.
		 * We unlock this fragment's checkpoint descriptor and
		 * kick DMA if it is idle.  Using checkpoint descriptors
		 * allows for control operations without the need for 
		 * stopping the DMA channel if it is already running.
		 */
		b->offset = 0;
		b->dma_desc->ddadr &= ~DDADR_STOP;

		/* move the index to the next fragment */
		if (++s->usr_frag >= s->nbfrags)
			s->usr_frag = 0;
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	return ret;
}


static int audio_sync(struct file *file)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *s = state->output_stream;
	audio_buf_t *b;
	pxa_dma_desc *final_desc;
	u_long dcmd_save = 0;
	DECLARE_WAITQUEUE(wait, current);

	if (!(file->f_mode & FMODE_WRITE) || !s->buffers || s->mapped)
		return 0;

	/*
	 * Send current buffer if it contains data.  Be sure to send
	 * a full sample count.
	 */
	final_desc = NULL;
	b = &s->buffers[s->usr_frag];
	if (b->offset &= ~3) {
		final_desc = &b->dma_desc[1 + b->offset/MAX_DMA_SIZE];
		b->offset &= (MAX_DMA_SIZE-1);
		dcmd_save = final_desc->dcmd;
		final_desc->dcmd = b->offset | s->dcmd | DCMD_ENDIRQEN; 
		final_desc->ddadr |= DDADR_STOP;
		b->offset = 0;
		b->dma_desc->ddadr &= ~DDADR_STOP;

#ifdef DMA_NO_INITIALIZED
		if ((DCSR(s->dma_ch) & DCSR_STOPSTATE)||
			!(DCSR(s->dma_ch) & DCSR_RUN)){
			DDADR(s->dma_ch) = b->dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}
#else
		if (DCSR(s->dma_ch) & DCSR_STOPSTATE) {
			DDADR(s->dma_ch) = b->dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}
#endif
	}

	/* Wait for DMA to complete. */
	set_current_state(TASK_INTERRUPTIBLE);
#if 0
	/* 
	 * The STOPSTATE IRQ never seem to occur if DCSR_STOPIRQEN is set
	 * along wotj DCSR_RUN.  Silicon bug?
	 */
	add_wait_queue(&s->stop_wq, &wait);
	DCSR(s->dma_ch) |= DCSR_STOPIRQEN;
	schedule();
#else 
	add_wait_queue(&s->frag_wq, &wait);
	while ((DCSR(s->dma_ch) & DCSR_RUN) && !signal_pending(current)) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
#endif
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&s->frag_wq, &wait);

	corgi_intmode = 0;

	/* Restore the descriptor chain. */
	if (final_desc) {
		final_desc->dcmd = dcmd_save;
		final_desc->ddadr &= ~DDADR_STOP;
		b->dma_desc->ddadr |= DDADR_STOP;
	}
	return 0;
}


static unsigned int audio_poll(struct file *file,
			       struct poll_table_struct *wait)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	unsigned int mask = 0;

	if (file->f_mode & FMODE_READ) {
		/* Start audio input if not already active */
		if (!is->buffers && audio_setup_buf(is))
			return -ENOMEM;
#ifdef DMA_NO_INITIALIZED
		if ((DCSR(is->dma_ch) & DCSR_STOPSTATE)||
			!(DCSR(is->dma_ch) & DCSR_RUN)){
			DDADR(is->dma_ch) = 
				is->buffers[is->dma_frag].dma_desc->ddadr;
			DCSR(is->dma_ch) = DCSR_RUN;
		}
#else
		if (DCSR(is->dma_ch) & DCSR_STOPSTATE) {
			DDADR(is->dma_ch) = 
				is->buffers[is->dma_frag].dma_desc->ddadr;
			DCSR(is->dma_ch) = DCSR_RUN;
		}
#endif
		poll_wait(file, &is->frag_wq, wait);
	}

	if (file->f_mode & FMODE_WRITE) {
		if (!os->buffers && audio_setup_buf(os))
			return -ENOMEM;
		poll_wait(file, &os->frag_wq, wait);
	}

	if (file->f_mode & FMODE_READ)
		if (( is->mapped && is->bytecount > 0) ||
		    (!is->mapped && atomic_read(&is->sem.count) > 0))
			mask |= POLLIN | POLLRDNORM;

	if (file->f_mode & FMODE_WRITE)
		if (( os->mapped && os->bytecount > 0) ||
		    (!os->mapped && atomic_read(&os->sem.count) > 0))
			mask |= POLLOUT | POLLWRNORM;

	return mask;
}


static int audio_ioctl( struct inode *inode, struct file *file,
			uint cmd, ulong arg)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;
	long val;
	long fmt;
	int data;

	switch (cmd) {
	case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *)arg);

	case SNDCTL_DSP_GETBLKSIZE:
		if (file->f_mode & FMODE_WRITE)
			return put_user(os->fragsize, (int *)arg);
		else
			return put_user(is->fragsize, (int *)arg);

	case SNDCTL_DSP_GETCAPS:
		val = DSP_CAP_REALTIME|DSP_CAP_TRIGGER|DSP_CAP_MMAP;
		if (is && os)
			val |= DSP_CAP_DUPLEX;
		return put_user(val, (int *)arg);

	case SNDCTL_DSP_SETFRAGMENT:
		if (get_user(val, (long *) arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			int ret = audio_set_fragments(is, val);
			if (ret < 0)
				return ret;
			ret = put_user(ret, (int *)arg);
			if (ret)
				return ret;
		}
		if (file->f_mode & FMODE_WRITE) {
			int ret = audio_set_fragments(os, val);
			if (ret < 0)
				return ret;
			ret = put_user(ret, (int *)arg);
			if (ret)
				return ret;
		}
		return 0;

	case SNDCTL_DSP_SYNC:
		return audio_sync(file);

	case SNDCTL_DSP_SETDUPLEX:
		return 0;

	case SNDCTL_DSP_POST:
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
		val = 0;
		if (file->f_mode & FMODE_READ && DCSR(is->dma_ch) & DCSR_RUN)
			val |= PCM_ENABLE_INPUT;
		if (file->f_mode & FMODE_WRITE && DCSR(os->dma_ch) & DCSR_RUN)
			val |= PCM_ENABLE_OUTPUT;
		return put_user(val, (int *)arg);

	case SNDCTL_DSP_SETTRIGGER:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			if (val & PCM_ENABLE_INPUT) {
				if (!is->buffers && audio_setup_buf(is))
					return -ENOMEM;
				if (!(DCSR(is->dma_ch) & DCSR_RUN)) {
					audio_buf_t *b = &is->buffers[is->dma_frag];
					DDADR(is->dma_ch) = b->dma_desc->ddadr;
					DCSR(is->dma_ch) = DCSR_RUN;
				}
			} else {
				DCSR(is->dma_ch) = 0;
			}
		}
		if (file->f_mode & FMODE_WRITE) {
			if (val & PCM_ENABLE_OUTPUT) {
				if (!os->buffers && audio_setup_buf(os))
					return -ENOMEM;
				if (!(DCSR(os->dma_ch) & DCSR_RUN)) {
					audio_buf_t *b = &os->buffers[os->dma_frag];
					DDADR(os->dma_ch) = b->dma_desc->ddadr;
					DCSR(os->dma_ch) = DCSR_RUN;
				}
			} else {
				DCSR(os->dma_ch) = 0;
			}
		}
		return 0;

	case SNDCTL_DSP_GETOSPACE:
	    {
		audio_buf_info inf = { 0, };
		audio_stream_t *s = os;
		int j,k;

		if ((s == os && !(file->f_mode & FMODE_WRITE)))
			return -EINVAL;
		if (!s->buffers && audio_setup_buf(s))
			return -ENOMEM;

		j = s->usr_frag;
		k = s->dma_frag;
		if ( atomic_read(&s->sem.count) > 0  ) {
		  if ( j > k ) {
		    inf.fragments = s->nbfrags - ( j - k );
		  } else if ( j < k ) {
		    inf.fragments = k - j;
		  } else {
		    inf.fragments = s->nbfrags;
		  }
		}
		inf.bytes = s->fragsize * inf.fragments;

		inf.fragsize = s->fragsize;
		inf.fragstotal = s->nbfrags;
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }

	case SNDCTL_DSP_GETISPACE:
	    {
		audio_buf_info inf = { 0, };
		audio_stream_t *s = is;
		int j,k;

		if ((s == is && !(file->f_mode & FMODE_READ)))
			return -EINVAL;
		if (!s->buffers && audio_setup_buf(s))
			return -ENOMEM;

		j = s->usr_frag;
		k = s->dma_frag;

		if ( atomic_read(&s->sem.count) <= 0  ) {
		    inf.fragments = 0;
		} else {
		    inf.fragments = atomic_read(&s->sem.count);
		}

		inf.bytes = s->fragsize * inf.fragments;

		inf.fragsize = s->fragsize;
		inf.fragstotal = s->nbfrags;
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }



	case SNDCTL_DSP_GETOPTR:
	case SNDCTL_DSP_GETIPTR:
	    {
		count_info inf = { 0, };
		audio_stream_t *s = (cmd == SNDCTL_DSP_GETOPTR) ? os : is;
		dma_addr_t ptr;
		int bytecount, offset, flags;

		if ((s == is && !(file->f_mode & FMODE_READ)) ||
		    (s == os && !(file->f_mode & FMODE_WRITE)))
			return -EINVAL;
		if (DCSR(s->dma_ch) & DCSR_RUN) {
			audio_buf_t *b;
			save_flags_cli(flags);
			ptr = (s->output) ? DSADR(s->dma_ch) : DTADR(s->dma_ch);
			b = &s->buffers[s->dma_frag];
			offset = ptr - b->dma_desc->dsadr;
			if (offset >= s->fragsize)
				offset = s->fragsize - 4;
		} else {
			save_flags(flags);
			offset = 0;
		}
		inf.ptr = s->dma_frag * s->fragsize + offset;
		bytecount = s->bytecount + offset;
		s->bytecount = -offset;
		inf.blocks = s->fragcount;
		s->fragcount = 0;
		restore_flags(flags);
		if (bytecount < 0)
			bytecount = 0;
		inf.bytes = bytecount;
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }

	case SNDCTL_DSP_NONBLOCK:
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_RESET:
		if (file->f_mode & FMODE_WRITE) 
			audio_clear_buf(os);
		if (file->f_mode & FMODE_READ)
			audio_clear_buf(is);
		return 0;

	//
	case SNDCTL_DSP_SPEED:
	  audio_sync(file);
	  IOCTL_IN(arg, data);
	  return IOCTL_OUT(arg, (*sound.mach.setFreq)(data));
	case SNDCTL_DSP_STEREO:
	  audio_sync(file);
	  IOCTL_IN(arg, data);
	  return IOCTL_OUT(arg, (*sound.mach.setStereo)(data));
	case SOUND_PCM_WRITE_CHANNELS:
	  audio_sync(file);
	  IOCTL_IN(arg, data);
	  return IOCTL_OUT(arg, (*sound.mach.setStereo)(data-1)+1);
	case SNDCTL_DSP_SETFMT:
	  audio_sync(file);
	  IOCTL_IN(arg, data);
	  return IOCTL_OUT(arg, (*sound.mach.setFormat)(data));
	case SNDCTL_DSP_GETFMTS:
	  fmt = 0;
	  if (sound.trans) {
	    if (sound.trans->ct_ulaw)
	      fmt |= AFMT_MU_LAW;
	    if (sound.trans->ct_alaw)
	      fmt |= AFMT_A_LAW;
	    if (sound.trans->ct_s8)
	      fmt |= AFMT_S8;
	    if (sound.trans->ct_u8)
	      fmt |= AFMT_U8;
	    if (sound.trans->ct_s16be)
	      fmt |= AFMT_S16_BE;
	    if (sound.trans->ct_u16be)
	      fmt |= AFMT_U16_BE;
	    if (sound.trans->ct_s16le)
	      fmt |= AFMT_S16_LE;
	    if (sound.trans->ct_u16le)
	      fmt |= AFMT_U16_LE;
	  }
	  return IOCTL_OUT(arg, fmt);
	//

	default:
		return mixer_ioctl(inode, file, cmd, arg);
	}

	return 0;
}


static int audio_mmap(struct file *file, struct vm_area_struct *vma)
{
	audio_state_t *state = file->private_data;
	audio_stream_t *s;
	unsigned long size, vma_addr;
	int i, ret;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (vma->vm_flags & VM_WRITE) {
		if (!state->wr_ref)
			return -EINVAL;;
		s = state->output_stream;
	} else if (vma->vm_flags & VM_READ) {
		if (!state->rd_ref)
			return -EINVAL;
		s = state->input_stream;
	} else return -EINVAL;

	if (s->mapped)
		return -EINVAL;
	size = vma->vm_end - vma->vm_start;
	if (size != s->fragsize * s->nbfrags)
		return -EINVAL;
	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;
	vma_addr = vma->vm_start;
	for (i = 0; i < s->nbfrags; i++) {
		audio_buf_t *buf = &s->buffers[i];
		if (!buf->master)
			continue;
		ret = remap_page_range(vma_addr, buf->dma_desc->dsadr,
				       buf->master, vma->vm_page_prot);
		if (ret)
			return ret;
		vma_addr += buf->master;
	}
	for (i = 0; i < s->nbfrags; i++)
		s->buffers[i].dma_desc->ddadr &= ~DDADR_STOP;
	s->mapped = 1;
	return 0;
}


static int audio_release(struct inode *inode, struct file *file)
{
	audio_state_t *state = file->private_data;
	int err;


	down(&state->sem);

	//  HP & sounder & mic off
#if (defined AUDIO_ALWAYS_SP_ON) || (defined AUDIO_SP_DELAY_OFF)
	reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R | SCP_MIC_BIAS );
#ifdef AUDIO_SP_DELAY_OFF
	corgi_sp_delay_off();
#endif
#else
	reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R | SCP_APM_ON | SCP_MIC_BIAS );
#endif


	if (file->f_mode & FMODE_READ) {
		audio_clear_buf(state->input_stream);
		*state->input_stream->drcmr = 0;
		pxa_free_dma(state->input_stream->dma_ch);
		state->rd_ref = 0;
	}

	if (file->f_mode & FMODE_WRITE) {
		audio_sync(file);
		audio_clear_buf(state->output_stream);
		*state->output_stream->drcmr = 0;
		pxa_free_dma(state->output_stream->dma_ch);
		state->wr_ref = 0;
	}

	// Close wm8731
	err = wm8731_close();
	if ( err ) {
	  PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not close wm8731\n");
	  return err;
	}

	// Close I2S
	err = i2s_close( CORGI_I2S_ADDRESS );
	if ( err ) {
	  PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not close i2s\n");
	  return err;
	}

	up(&state->sem);
	return 0;
}


int audio_open(struct inode *inode, struct file *file )
{
	audio_state_t  *state = &pxa_audio_state;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	SOUND_SETTINGS settings;
	int err;
	int now;

	while(1) {
	  if ( isPXAI2SReady ) break;
	  schedule();
	}

	down(&state->sem);

	/* access control */
	err = -ENODEV;
	if ((file->f_mode & FMODE_WRITE) && !os)
		goto out;
	if ((file->f_mode & FMODE_READ) && !is)
		goto out;
	err = -EBUSY;

	if ( state->wr_ref || state->rd_ref )
		goto out;

	/* request DMA channels */
	if (file->f_mode & FMODE_WRITE) {
	  err = pxa_request_dma(os->name, DMA_PRIO_LOW, audio_dma_irq, os);
	  if ( err < 0 )
	    goto out;
	  os->dma_ch = err;
	}
	if (file->f_mode & FMODE_READ) {
	  err = pxa_request_dma(is->name, DMA_PRIO_LOW, audio_dma_irq, is);
	  if (err < 0) {
	    if (file->f_mode & FMODE_WRITE) {
	      *os->drcmr = 0;
	      pxa_free_dma(os->dma_ch);
	    }
	    goto out;
	  }
	  is->dma_ch = err;
	}

	file->private_data	= state;

	if ((file->f_mode & FMODE_WRITE)) {
	  state->wr_ref = 1;
	  os->fragsize = AUDIO_FRAGSIZE_DEFAULT;
	  os->nbfrags = AUDIO_NBFRAGS_DEFAULT;
	  os->output = 1;
	  os->mapped = 0;
	  init_waitqueue_head(&os->frag_wq);
	  init_waitqueue_head(&os->stop_wq);
	  *os->drcmr = os->dma_ch | DRCMR_MAPVLD;

	  settings.mode = SOUND_PLAY_MODE;
	}
	if (file->f_mode & FMODE_READ) {
	  state->rd_ref = 1;
	  is->fragsize = AUDIO_FRAGSIZE_DEFAULT;
	  is->nbfrags = AUDIO_NBFRAGS_DEFAULT;
	  is->output = 0;
	  is->mapped = 0;
	  init_waitqueue_head(&is->frag_wq);
	  init_waitqueue_head(&is->stop_wq);
	  *is->drcmr = is->dma_ch | DRCMR_MAPVLD;

	  settings.mode = SOUND_REC_MODE;
	}

	if ((MINOR(inode->i_rdev) & 0x0f) == SND_DEV_AUDIO) {
	  CorgiSetFreq(8000);
	  CorgiSetStereo(0);
	  CorgiSetFormat(AFMT_MU_LAW);
	}

	settings.output.left  = sound.volume_left;
	settings.output.right = sound.volume_right;
	settings.input.left   = sound.gain_left;
	settings.input.right  = sound.gain_right;
	settings.frequency    = sound.hw_freq;

	// Open wm8731
	err = wm8731_open(&settings);
	if ( err ) {
	  PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not open wm8731\n");
	  goto error;
	}

	// Open I2S
	//err = i2s_open( CORGI_I2S_ADDRESS , CORGI_DEFAULT_FREQUENCY);
	err = i2s_open( CORGI_I2S_ADDRESS , sound.hw_freq );
	if ( err ) {
	  wm8731_close();
	  PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not open i2s\n");
	  goto error;
	}

#ifdef AUDIO_SP_DELAY_OFF
	del_timer(&speaker_timers);
#endif

#if 0	// debug
    reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R );	//  HP off
    set_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R );	//  HP on
    mdelay(100);
    reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R );	//  HP off
#endif

#if 1
        // play 50msec
	now = jiffies;
	while(1) {
	  corgi_i2s_write(AUDIO_DATA);
	  if ( jiffies > ( now + WAIT_PLAY1 ) ) break;
	}
#endif

	if( settings.mode == SOUND_REC_MODE ){
		if(file->f_mode & FMODE_WRITE){
			// Read & Write means earphone mic mode
			corgi_hp_mode( CORGI_HP_HEADSET );
		}else{
			// mic mode
			corgi_hp_mode( CORGI_HP_MIC );
		}
	}else{
		corgi_hp_mode(CORGI_HP_AUDIO);
	}

	err = 0;
out:
	up(&state->sem);
	return err;

error:
	up(&state->sem);
	if ( ( file->f_mode & FMODE_WRITE ) && ( os->dma_ch >= 0 ) ) {
	  *os->drcmr = 0;
	  pxa_free_dma(os->dma_ch);
	}
	if ( ( file->f_mode & FMODE_READ ) && ( is->dma_ch >= 0 ) ) {
	  *is->drcmr = 0;
	  pxa_free_dma(is->dma_ch);
	}
	state->wr_ref = 0;
	state->rd_ref = 0;
	return err;
}


/*** Platform *************************************************************/
static int CorgiSetFormat(int format)
{
  int size;


  switch (format) {
  case AFMT_QUERY:
    return(sound.format);
  case AFMT_MU_LAW:
    size = 8;
    ct_func = sound.trans->ct_ulaw;
    break;
  case AFMT_A_LAW:
    size = 8;
    ct_func = sound.trans->ct_alaw;
    break;
#if 0 // unsupport
  case AFMT_S8:
    size = 8;
    ct_func = sound.trans->ct_s8;
    break;
  case AFMT_S16_BE:
    size = 16;
    ct_func = sound.trans->ct_s16be;
    break;
  case AFMT_U16_BE:
    size = 16;
    ct_func = sound.trans->ct_u16be;
    break;
  case AFMT_U16_LE:
    size = 16;
    ct_func = sound.trans->ct_u16le;
    break;
#endif
  case AFMT_U8:
    size = 8;
    ct_func = sound.trans->ct_u8;
    ct_read_func = sound.trans_read->ct_u8;
    break;
  case AFMT_S16_LE:
    size = 16;
    ct_func = sound.trans->ct_s16le;
    ct_read_func = sound.trans_read->ct_s16le;
    break;
  default: /* :-) */
    size = 16;
    format = AFMT_S16_LE;

  }

  sound.format = format;
  sound.size = size;

  return format;
}

static void CorgiVolumeMuteTemp(void)
{
  // If sound driver is opened , so update volume.
  if ( wm8731_busy() ) {
    wm8731_set_output_volume(0,0);
  }
}

static void CorgiUpdateVolume(void)
{
  // If sound driver is opened , so update volume.
  if ( wm8731_busy() ) {
    wm8731_set_output_volume(sound.volume_left, sound.volume_right);
  }
}

static int CorgiSetVolume(int volume)
{
  sound.volume_left = volume & 0xff;
  if ( sound.volume_left > 100 ) sound.volume_left = 100;

  sound.volume_right = ( (volume & 0xff00) >> 8);
  if ( sound.volume_right > 100 ) sound.volume_right = 100;

  CorgiUpdateVolume();

}

static int CorgiGetVolume(void)
{
  return ( sound.volume_right << 8 | sound.volume_left );
}

static int CorgiSetStereo(int stereo)
{
  if (stereo < 0) {
    return(sound.stereo);
  }

  stereo = !!stereo;    /* should be 0 or 1 now */
  sound.stereo = stereo;
  return(stereo);
}

static int CorgiSetFreq(int speed)
{
  if (speed < 0) {
    return(sound.freq);
  }
  sound.freq = speed;
  switch (sound.freq) {
  case 8000:
  case 44100:
  case 48000:
    sound.hw_freq = sound.freq;
    break;
  case 11025:
  case 22050:
    sound.hw_freq = 44100;
    break;
  case 12000:
  case 16000:
  case 32000:
  case 24000:
    sound.hw_freq = 48000;
    break;
  default:
    sound.hw_freq = 44100;
    break;
  }

  if ( wm8731_busy() ) {
    int err;

    wm8731_set_freq(sound.hw_freq);
    err = i2s_close( CORGI_I2S_ADDRESS );
    if ( err ) {
      PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not close i2s\n");
    }
    err = i2s_open( CORGI_I2S_ADDRESS , sound.hw_freq );
    if ( err ) {
      PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not open i2s\n");
    }
  }

  return(sound.freq);
}

static void CorgiUpdateGain(void)
{
  sound.gain       = input_gain[ ( ( 6 * sound.gain_left ) / 100 ) ].gain;
  sound.gain_shift = input_gain[ ( ( 6 * sound.gain_left ) / 100 ) ].shift;

  if ( wm8731_busy() ) {
    wm8731_set_mic_gain_boost(sound.gain);
  }

}

static int CorgiSetGain(int gain)
{
  sound.gain_left = gain & 0xff;
  if ( sound.gain_left > 100 ) sound.gain_left = 100;

  sound.gain_right = ( (gain & 0xff00) >> 8);
  if ( sound.gain_right > 100 ) sound.gain_right = 100;

  CorgiUpdateGain();

  return 1;
}

static int CorgiGetGain(void)
{
  return ( sound.gain_right << 8 | sound.gain_left );
}


#ifdef SOUND_SUPPORT_REMOCON
void Corgi_hp_in_interrupt(void)
#else
static void Corgi_hp_in_interrupt(int irq, void *dummy, struct pt_regs *fp)
#endif

{
  if ( !isPXAI2SReady ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Currently audio driver is not ready\n");
    return;
  }

  PXAI2S_DBGPRINT(DBG_LEVEL2, "Corgi_hp_interrupt\n");
  wake_up(&hp_proc);
}


static void corgi_hp_mode(int mode)
{
  int state;

  if ( !isPXAI2SReady ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Currently audio driver is not ready\n");
    return;
  }

#if 0
  if (isChkHPstatus ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Currently hp thread is running.. so skip !\n");
    return;
  }
#endif

	if(( mode == CORGI_HP_AUDIO )||( mode == CORGI_HP_BUZZER )){
		if( corgi_HPstatus < 0 ){
			corgi_HPstatus = GPLR(GPIO_HP_IN) & GPIO_bit(GPIO_HP_IN);
#ifdef SOUND_SUPPORT_REMOCON
			if( corgi_HPstatus == HPJACK_STATE_NONE ){
				if( get_remocon_state() == HPJACK_STATE_REMOCON ){
					corgi_HPstatus = HPJACK_STATE_REMOCON;
				}
			}
#endif
		}
		state = corgi_HPstatus;

		if ( state ) { 	// Insert HP
			PXAI2S_DBGPRINT(DBG_LEVEL1, "Insert HP\n");
			mode = CORGI_HP_HP;
		} else {	// Not Insert HP
			PXAI2S_DBGPRINT(DBG_LEVEL1, "Not Insert HP\n");
			mode = CORGI_HP_SOUNDER;
		}
	}
	switch( mode ){
	case CORGI_HP_SOUNDER:
		set_scoop_gpio( SCP_APM_ON );					// Speaker On
		reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R );	//  HP off
		break;
	case CORGI_HP_HP:
		reset_scoop_gpio( SCP_APM_ON );				// Speaker Off
		set_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R );	//  HP on
		break;
	case CORGI_HP_BOTH:
		set_scoop_gpio( SCP_APM_ON | SCP_MUTE_L | SCP_MUTE_R );	//  All on
		break;
	case CORGI_HP_MIC:
		set_scoop_gpio( SCP_MIC_BIAS );	//  MIC bias on
		reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R | SCP_APM_ON );
		break;
	case CORGI_HP_HEADSET:
		set_scoop_gpio( SCP_MIC_BIAS | SCP_MUTE_R );	//  MIC bias on
		reset_scoop_gpio( SCP_MUTE_L | SCP_APM_ON );
		break;
	case CORGI_HP_AUDIO:
	case CORGI_HP_BUZZER:
	default:
		return;
	}

  CorgiUpdateVolume();
}

#ifdef SOUND_SUPPORT_REMOCON
static void corgi_remocon_chk_timer(unsigned long dummy)
{
	PXAI2S_DBGPRINT(DBG_LEVEL2, "Watch HP state again\n");

	if( get_remocon_state() != HPJACK_STATE_REMOCON ){
		corgi_HPstatus = HPJACK_STATE_NONE;
		wake_up(&hp_proc);
	}
}
#endif
static void corgi_hp_thread(void)
{
  int count,err;
  int state;
  audio_state_t  *audio_state = &pxa_audio_state;

  // daemonize();
  strcpy(current->comm, "snd_hp");
  sigfillset(&current->blocked);


  while(1) {
    isChkHPstatus = 0;
    interruptible_sleep_on(&hp_proc);

    isChkHPstatus = 1;

#ifdef SOUND_SUPPORT_REMOCON
    if( corgi_HPstatus == HPJACK_STATE_REMOCON ){
      // remocon ?
	  if( get_remocon_state() != HPJACK_STATE_REMOCON ){
		// watch later
		del_timer(&remocon_chk_timers);
		remocon_chk_timers.function = corgi_remocon_chk_timer;
		remocon_chk_timers.expires = jiffies + (HZ/10);
		add_timer(&remocon_chk_timers);
	  }
      continue;
	}else{
		if( get_remocon_state() == HPJACK_STATE_REMOCON ){
			// remocon detected
			corgi_HPstatus = HPJACK_STATE_REMOCON;
		}
	}
#endif

#ifdef AUDIO_SP_DELAY_OFF
    del_timer(&speaker_timers);
#endif

	if ( audio_state->wr_ref || audio_state->rd_ref ){
	  // sound is playing
	  reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R | SCP_APM_ON | SCP_MIC_BIAS );	//  HP, Speaker & Mic off

	  // volume zero
	  CorgiVolumeMuteTemp();
	}

    while(1) {

      state = GPLR(GPIO_HP_IN) & GPIO_bit(GPIO_HP_IN);
      count = 0;
      err = 0;
      while(1) {
	interruptible_sleep_on_timeout((wait_queue_head_t*)&hp_queue, 5 );

	if ( state == ( GPLR(GPIO_HP_IN) & GPIO_bit(GPIO_HP_IN) ) ) count++;
	else {
	  err++;
	  state = GPLR(GPIO_HP_IN) & GPIO_bit(GPIO_HP_IN);
	  count = 0;
	}

	if ( count > 10 ) break;
	if ( err > 50 ) break;
#ifdef SOUND_SUPPORT_REMOCON
	if( get_remocon_state() == HPJACK_STATE_REMOCON ){
	  // remocon detected
	  corgi_HPstatus = HPJACK_STATE_REMOCON;
	  break;
	}
#endif
      }
      if ( count > 10 ) {
	if ( state ) { 	// Insert HP
	  PXAI2S_DBGPRINT(DBG_LEVEL1, "Insert HP\n");
	  corgi_HPstatus = HPJACK_STATE_HEADPHONE;
	} else {	// Not Insert HP
	  PXAI2S_DBGPRINT(DBG_LEVEL1, "Not Insert HP\n");
	  corgi_HPstatus = HPJACK_STATE_NONE;
	}
      } else {
	  PXAI2S_DBGPRINT(DBG_LEVEL1, "??? HP\n");
#ifdef SOUND_SUPPORT_REMOCON
	  if( get_remocon_state() != HPJACK_STATE_REMOCON ){
	    corgi_HPstatus = HPJACK_STATE_NONE;
	  }
#else
	  corgi_HPstatus = HPJACK_STATE_NONE;
#endif
      }
      break;

    }
#ifdef SOUND_SUPPORT_REMOCON
    if( corgi_HPstatus == HPJACK_STATE_NONE ){
      // check remocon
      if( get_remocon_state() == HPJACK_STATE_REMOCON ){
		PXAI2S_DBGPRINT(DBG_LEVEL1, "found remocon\n");
		corgi_HPstatus = HPJACK_STATE_REMOCON;
      }
    }
#endif
    // sound is not playing , so HP/SP be disabled.
	if ( !audio_state->wr_ref && !audio_state->rd_ref ) continue;

	if( audio_state->rd_ref ){
		// mic mode
		corgi_hp_mode((audio_state->wr_ref)?
				  CORGI_HP_HEADSET:CORGI_HP_MIC);
	}else{
		corgi_hp_mode((corgi_HPstatus == HPJACK_STATE_NONE)?
				  CORGI_HP_SOUNDER : CORGI_HP_HP);
	}
  }
}


static void corgi_init_thread(void)
{
  SOUND_SETTINGS settings;
  int err;
  int now;
  settings.mode = SOUND_PLAY_MODE;

  strcpy(current->comm, "snd_init");


  isPXAI2SReady = 0;

  settings.output.left  = sound.volume_left;
  settings.output.right = sound.volume_right;
  settings.input.left   = sound.gain_left;
  settings.input.right  = sound.gain_right;
  settings.frequency    = sound.hw_freq;

  // Open wm8731
  err = wm8731_open(&settings);
  if ( err ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not open wm8731\n");
  }

  // Open I2S
  err = i2s_open( CORGI_I2S_ADDRESS , CORGI_DEFAULT_FREQUENCY);
  if ( err ) {
    //wm8731_close();
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not open i2s\n");
  }

  // play 2sec
  now = jiffies;
  while(1) {
	corgi_i2s_write(AUDIO_DATA);
	if ( jiffies > ( now + WAIT_INIT_PLAY ) ) break;
  }

  // Close wm8731
  err = wm8731_close();
  if ( err ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not close wm8731\n");
  }

  // Close I2S
  err = i2s_close( CORGI_I2S_ADDRESS );
  if ( err ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not close i2s\n");
  }

  isPXAI2SReady = 1;
}

static void CorgiInit(void)
{

  // MUTE
  reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R | SCP_APM_ON | SCP_MIC_BIAS );

  if ( wm8731_init() ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Fail Init\n");
  }

#ifndef SOUND_SUPPORT_REMOCON
  /* GPIO15:Both Edge */
  set_GPIO_IRQ_edge(GPIO_HP_IN, GPIO_BOTH_EDGES);

  /* Register interrupt handler */
  if ( request_irq(GPIO_HP_IN, Corgi_hp_in_interrupt,
		   SA_INTERRUPT, "audio hp", Corgi_hp_in_interrupt)) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "%s: request_irq(%d) failed.\n", IRQ_GPIO_HP_IN);
  }
#endif

  /* Make threads */
  kernel_thread(corgi_hp_thread,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  kernel_thread(corgi_init_thread,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

}


/*** Buzzer *******************************************************/
#if ALARM_NO_MALLOC
int audio_buzzer_intmode(unsigned short *buffer,int count,int speed)
{
  int idx,i;
  unsigned long	snddata;
  audio_state_t  *state = &pxa_audio_state;
  audio_stream_t *os = state->output_stream;
  audio_stream_t *s = os;

  int_data_org = buffer;
  corgi_intmode = (count * sound.hw_freq / speed)&(~3);
  corgi_intmode_speed = speed;
  corgi_intmode_cur = 0;
  corgi_intmode_step = (speed << 8 ) / sound.hw_freq; // fixed point (x256)
  corgi_intmode_direct = 1;

    // pause during play or recording , so kick this place.
    if ( ( ( DCSR(s->dma_ch) & DCSR_STOPSTATE ) && ( state->wr_ref ) ) ||
	 ( ( !state->wr_ref ) && ( state->rd_ref ) ) )
    {
	  for(i=0;i<(corgi_intmode/4);i++) {
		  idx = (corgi_intmode_cur>>8);
		  snddata = (buffer[idx*2]|(buffer[idx*2+1]<<16));
		  corgi_intmode_cur += corgi_intmode_step;
		  corgi_i2s_write(snddata);
	  }

      corgi_intmode = 0;
      int_data_org = NULL;
	}
  corgi_intmode_direct = 0;
  return 1;
}
#else
int audio_buzzer_intmode(unsigned short *buffer,int count,int speed)
{
  int samplingCount = count * sound.hw_freq / speed;
  int idx,i ;
  int cur=0,step = (speed << 8 ) / sound.hw_freq; // fixed point (x256)
  unsigned short *dstct;

  int_data = (unsigned short *)kmalloc(count * 6,GFP_KERNEL);
  if ( !int_data ) return 0; 
  int_data_org = int_data;

  samplingCount &= ~0x3;

  dstct = int_data;
  for(i=0;i<(samplingCount/4);i++) {
      idx = (cur>>8);
      *dstct++ = buffer[idx*2];
      *dstct++ = buffer[idx*2+1];
      cur += step;
  }

  corgi_intmode = samplingCount;
  corgi_intmode_speed = speed;
  corgi_intmode_direct = 1;

  {
    audio_state_t  *state = &pxa_audio_state;
    audio_stream_t *os = state->output_stream;
    audio_stream_t *s = os;

    // pause during play or recording , so kick this place.
    if ( ( ( DCSR(s->dma_ch) & DCSR_STOPSTATE ) && ( state->wr_ref ) ) ||
	 ( ( !state->wr_ref ) && ( state->rd_ref ) ) )
    {
      int i;
      for(i=0;i<(corgi_intmode/4);i++) {
	corgi_i2s_write(*(unsigned long *)int_data);
	int_data++;
	int_data++;
      }
      corgi_intmode = 0;
      kfree(int_data_org);
      int_data_org = NULL;
    }
  }

  corgi_intmode_direct = 0;
  return 1;
}
#endif

#if ALARM_NO_MALLOC
static void corgi_intmode_mix(unsigned char *dest,int bUsed)
{
  int i;
  short *cdata = (short *)dest;
  int cnt = min(corgi_intmode,bUsed);
  int idx;

  for(i=0;i<(cnt/4);i++) {
	  idx = (corgi_intmode_cur>>8);
	  *cdata++ = int_data_org[idx*2];
	  *cdata++ = int_data_org[idx*2+1];
	  corgi_intmode_cur += corgi_intmode_step;
  }

  corgi_intmode -= cnt;
  if ( corgi_intmode <= 0 ) {
	  corgi_intmode = 0;
	  int_data_org = NULL;
  }
  return cnt;
}
#else
static void corgi_intmode_mix(unsigned char *dest,int bUsed)
{
  int i;
  short *cdata = (short *)dest;
  int cnt = min(corgi_intmode,bUsed);

  for(i=0;i<(cnt/4);i++) {
    *cdata = *int_data;
    int_data++;
    cdata++;
    *cdata = *int_data;
    int_data++;
    cdata++;
  }

  corgi_intmode -= cnt;
  if ( corgi_intmode <= 0 ) {
    corgi_intmode = 0;
    if ( int_data_org ) {
      kfree(int_data_org);
      int_data_org = NULL;
    }
  }
}
#endif

int audio_buzzer_write(const char *buffer,int count)
{
  const char *buffer0 = buffer;
  audio_state_t  *state = &pxa_audio_state;
  audio_stream_t *s = state->output_stream;
  int chunksize, ret = 0;


#ifdef AUDIO_BUFFER_SETUP_NOCLEAR
  if (!s->buffers && __audio_setup_buf(s,0))
    return -ENOMEM;
#else
  if (!s->buffers && audio_setup_buf(s))
    return -ENOMEM;
#endif

  while (count > 0) {
    audio_buf_t *b = &s->buffers[s->usr_frag];

    chunksize = s->fragsize - b->offset;
    if (chunksize > count)
      chunksize = count;
    memcpy(b->data + b->offset, buffer, chunksize);

    b->offset += chunksize;
    buffer += chunksize;
    count -= chunksize;

    if (b->offset < s->fragsize) {
      up(&s->sem);
      break;
    }

    b->offset = 0;
    b->dma_desc->ddadr &= ~DDADR_STOP;
#ifdef DMA_NO_INITIALIZED
    if ((DCSR(s->dma_ch) & DCSR_STOPSTATE)||
	!(DCSR(s->dma_ch) & DCSR_RUN)){
      DDADR(s->dma_ch) = b->dma_desc->ddadr;
      DCSR(s->dma_ch) = DCSR_RUN;
    }
#else
    if (DCSR(s->dma_ch) & DCSR_STOPSTATE) {
      DDADR(s->dma_ch) = b->dma_desc->ddadr;
      DCSR(s->dma_ch) = DCSR_RUN;
    }
#endif

    /* move the index to the next fragment */
    if (++s->usr_frag >= s->nbfrags)
      s->usr_frag = 0;
  }

  if ((buffer - buffer0))
    ret = buffer - buffer0;

  return ret;
}

void audio_buzzer_sync(void)
{
  audio_state_t  *state = &pxa_audio_state;
  audio_stream_t *s = state->output_stream;
  audio_buf_t *b;
  pxa_dma_desc *final_desc;
  u_long dcmd_save = 0;
  int err;
  DECLARE_WAITQUEUE(wait, current);


  //audio_sync(file);

	/*
	 * Send current buffer if it contains data.  Be sure to send
	 * a full sample count.
	 */
	final_desc = NULL;
	b = &s->buffers[s->usr_frag];
	if (b->offset &= ~3) {
		final_desc = &b->dma_desc[1 + b->offset/MAX_DMA_SIZE];
		b->offset &= (MAX_DMA_SIZE-1);
		dcmd_save = final_desc->dcmd;
		final_desc->dcmd = b->offset | s->dcmd | DCMD_ENDIRQEN; 
		final_desc->ddadr |= DDADR_STOP;
		b->offset = 0;
		b->dma_desc->ddadr &= ~DDADR_STOP;
#ifdef DMA_NO_INITIALIZED
		if ((DCSR(s->dma_ch) & DCSR_STOPSTATE)||
		    !(DCSR(s->dma_ch) & DCSR_RUN)){
			DDADR(s->dma_ch) = b->dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}
#else
		if (DCSR(s->dma_ch) & DCSR_STOPSTATE) {
			DDADR(s->dma_ch) = b->dma_desc->ddadr;
			DCSR(s->dma_ch) = DCSR_RUN;
		}
#endif
	}


  /* Wait for DMA to complete. */
  set_current_state(TASK_INTERRUPTIBLE);

  add_wait_queue(&s->frag_wq, &wait);
  while ((DCSR(s->dma_ch) & DCSR_RUN) && !signal_pending(current)) {
    schedule();
    set_current_state(TASK_INTERRUPTIBLE);
  }

  set_current_state(TASK_RUNNING);
  remove_wait_queue(&s->frag_wq, &wait);
}

int audio_buzzer_release(void)
{
  audio_state_t  *state = &pxa_audio_state;
  audio_stream_t *s = state->output_stream;
  int err;
  DECLARE_WAITQUEUE(wait, current);


  //audio_sync(file);
  /* Wait for DMA to complete. */
  set_current_state(TASK_INTERRUPTIBLE);

  add_wait_queue(&s->frag_wq, &wait);
  while ((DCSR(s->dma_ch) & DCSR_RUN) && !signal_pending(current)) {
    schedule();
    set_current_state(TASK_INTERRUPTIBLE);
  }

  set_current_state(TASK_RUNNING);
  remove_wait_queue(&s->frag_wq, &wait);

#if (defined AUDIO_ALWAYS_SP_ON) || (defined AUDIO_SP_DELAY_OFF)
  reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R );	//  HP & Speaker off
#ifdef AUDIO_SP_DELAY_OFF
  corgi_sp_delay_off();
#endif
#else
  reset_scoop_gpio( SCP_MUTE_L | SCP_MUTE_R | SCP_APM_ON );	//  HP & Speaker off
#endif

  audio_clear_buf(state->output_stream);
  *state->output_stream->drcmr = 0;
  pxa_free_dma(state->output_stream->dma_ch);
  state->wr_ref = 0;

#if ALARM_NO_MALLOC
	// wait playing int sound.
	while(corgi_intmode_direct)
	  schedule();

	corgi_intmode = 0;
    int_data_org = NULL;
#else
  if ( int_data_org ) {
    kfree(int_data_org);
    int_data_org = NULL;
  }
#endif

  // Close wm8731
  err = wm8731_close();
  if ( err ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not close wm8731\n");
    return err;
  }

  // Close I2S
  err = i2s_close( CORGI_I2S_ADDRESS );
  if ( err ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not close i2s\n");
    return err;
  }

  return 0;
}

int audio_buzzer_open(int speed)
{
  audio_state_t  *state = &pxa_audio_state;
  audio_stream_t *os = state->output_stream;
  SOUND_SETTINGS settings;
  int err = 0;
  int now;

  while(1) {
    if ( isPXAI2SReady ) break;
    schedule();
  }

  if ( state->wr_ref || state->rd_ref )
    goto out;

  state->wr_ref = 1;

  /* request DMA channels */
  err = pxa_request_dma(os->name, DMA_PRIO_LOW, audio_dma_irq, os);
  if ( err < 0 )
    goto error;
  os->dma_ch = err;

  os->fragsize = AUDIO_FRAGSIZE_DEFAULT;
  os->nbfrags = AUDIO_NBFRAGS_DEFAULT;
  os->output = 1;
  os->mapped = 0;
  init_waitqueue_head(&os->frag_wq);
  init_waitqueue_head(&os->stop_wq);
  *os->drcmr = os->dma_ch | DRCMR_MAPVLD;

  settings.mode = SOUND_PLAY_MODE;
  settings.output.left  = sound.volume_left;
  settings.output.right = sound.volume_right;
  settings.input.left   = sound.gain_left;
  settings.input.right  = sound.gain_right;
  settings.frequency    = speed;

  // Open wm8731
  err = wm8731_open(&settings);
  if ( err ) {
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not open wm8731\n");
    goto error;
  }

  sound.hw_freq = speed;
  // Open I2S
  err = i2s_open( CORGI_I2S_ADDRESS , sound.hw_freq );
  if ( err ) {
    wm8731_close();
    PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not open i2s\n");
    goto error;
  }

#ifdef AUDIO_SP_DELAY_OFF
	del_timer(&speaker_timers);
#endif

#if 0
  // play 50msec
  now = jiffies;
  while(1) {
    corgi_i2s_write(AUDIO_DATA);
    if ( jiffies > ( now + WAIT_PLAY1 ) ) break;
  }
#endif

  corgi_hp_mode(CORGI_HP_BUZZER);

  err = 0;
out:
  return err;

error:
  *os->drcmr = 0;
  pxa_free_dma(os->dma_ch);
  state->wr_ref = 0;
  return err;
}


/*****************************************************************/
/*
 * /dev/sndstat
 *  Audio Stat stuff
 *
 */
static int state_open(struct inode *inode, struct file *file)
{
  char *buffer = pxa_i2s_state.buf;
  int len = 0;

  if (pxa_i2s_state.busy) {
    return -EBUSY;
  }

  MOD_INC_USE_COUNT;
  pxa_i2s_state.ptr = 0;
  pxa_i2s_state.busy = 1;

  len += sprintf(buffer+len, "  CORGI I2S sound driver:\n");

  len += sprintf(buffer+len, "\tsound.format = 0x%x", sound.format);
  switch (sound.format) {
  case AFMT_MU_LAW:
    len += sprintf(buffer+len, " (mu-law)");
    break;
  case AFMT_A_LAW:
    len += sprintf(buffer+len, " (A-law)");
    break;
  case AFMT_U8:
    len += sprintf(buffer+len, " (unsigned 8 bit)");
    break;
  case AFMT_S8:
    len += sprintf(buffer+len, " (signed 8 bit)");
    break;
  case AFMT_S16_BE:
    len += sprintf(buffer+len, " (signed 16 bit big)");
    break;
  case AFMT_U16_BE:
    len += sprintf(buffer+len, " (unsigned 16 bit big)");
    break;
  case AFMT_S16_LE:
    len += sprintf(buffer+len, " (signed 16 bit little)");
    break;
  case AFMT_U16_LE:
    len += sprintf(buffer+len, " (unsigned 16 bit little)");
    break;
  }
  len += sprintf(buffer+len, "\n");
  len += sprintf(buffer+len, "\tsound.speed = %dHz \n",sound.freq);
  len += sprintf(buffer+len, "\tsound.stereo = 0x%x (%s)\n",
		       sound.stereo, sound.stereo ? "stereo" : "mono");
  pxa_i2s_state.len = len;

  return 0;
}


static int state_release(struct inode *inode, struct file *file)
{
  pxa_i2s_state.busy = 0;
  MOD_DEC_USE_COUNT;
  return 0;
}


static ssize_t state_read(struct file *file, char *buf, size_t count,
			  loff_t *ppos)
{
  int n = pxa_i2s_state.len - pxa_i2s_state.ptr;
  if (n > count)
    n = count;
  if (n <= 0) {
    return 0;
  }
  if (copy_to_user(buf, &pxa_i2s_state.buf[pxa_i2s_state.ptr], n)) {
    return -EFAULT;
  }
  pxa_i2s_state.ptr += n;
  return n;
}

static struct file_operations state_fops =
{
  llseek:	no_llseek,
  read:		state_read,
  open:		state_open,
  release:	state_release,
};

static void __init state_init(void)
{
  // Regist /dev/sndstat
  pxa_i2s_state.dev_state = register_sound_special(&state_fops, SND_DEV_STATUS);
  if ( pxa_i2s_state.dev_state < 0 ) {
    return;
  }
  pxa_i2s_state.busy = 0;
}


/*****************************************************************/
/*
 *  /dev/mixer
 *  Audio Mixer stuff
 *
 */

static int mixer_ioctl( struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
  int data;

  switch (cmd) {
  case SOUND_MIXER_READ_DEVMASK:
    return IOCTL_OUT(arg, (SOUND_MASK_VOLUME|SOUND_MASK_MIC) );
  case SOUND_MIXER_READ_RECMASK:
    return IOCTL_OUT(arg, 0);
  case SOUND_MIXER_READ_STEREODEVS:
    return IOCTL_OUT(arg, SOUND_MASK_VOLUME);
  case SOUND_MIXER_READ_CAPS:
    return IOCTL_OUT(arg, 0);
  case SOUND_MIXER_WRITE_VOLUME:
    IOCTL_IN(arg, data);
    (*sound.mach.setVolume)(data);
  case SOUND_MIXER_READ_VOLUME:
    return IOCTL_OUT(arg, (*sound.mach.getVolume)());
  case SOUND_MIXER_READ_TREBLE:
    return IOCTL_OUT(arg, 0);
  case SOUND_MIXER_WRITE_TREBLE:
    return IOCTL_OUT(arg, 0);
  case SOUND_MIXER_READ_SPEAKER:
    return IOCTL_OUT(arg, 0);
  case SOUND_MIXER_WRITE_SPEAKER:
    return IOCTL_OUT(arg, 0);

  case SOUND_MIXER_WRITE_MIC:
    IOCTL_IN(arg, data);
    //printk("<mic data>:%x\n",data);
    CorgiSetGain(data);
  case SOUND_MIXER_READ_MIC:
    return IOCTL_OUT(arg, CorgiGetGain());

  }

  return -EINVAL;

}

static int mixer_open(struct inode *inode, struct file *file)
{
  if ( 	pxa_i2s_codec.busy  ) {
    return -EBUSY;
  }

  MOD_INC_USE_COUNT;
  pxa_i2s_codec.busy = 1;
  return 0;
}


static int mixer_release(struct inode *inode, struct file *file)
{
  pxa_i2s_codec.busy = 0;
  MOD_DEC_USE_COUNT;
  return 0;
}


static struct file_operations corgi_mixer_fops = {
        ioctl:		mixer_ioctl,
	llseek:		no_llseek,
	open:    	mixer_open,
	release: 	mixer_release,
	owner:		THIS_MODULE
};

static void __init mixer_init(void)
{
  // Regist /dev/mixer.
  pxa_i2s_codec.dev_mixer = register_sound_mixer(&corgi_mixer_fops, -1);
  if ( pxa_i2s_codec.dev_mixer < 0 ) {
    return;
  }
  pxa_i2s_codec.busy = 0;
}

/*****************************************************************/
/*
 *  /dev/dsp
 *  Audio stuff
 */

static struct file_operations corgi_audio_fops =
{
  llseek:	no_llseek,
  write:	audio_write,
  read:		audio_read,
  poll:		audio_poll,
  ioctl:	audio_ioctl,
  open:		audio_open,
  release:	audio_release,
  mmap:		audio_mmap,
};

static void __init sq_init(void)
{
  // Regist /dev/dsp.
  pxa_audio_state.dev_dsp = register_sound_dsp(&corgi_audio_fops, -1);
  if ( pxa_audio_state.dev_dsp < 0 ) {
    return;
  }

  pxa_audio_state.wr_ref = 0;
  pxa_audio_state.rd_ref = 0;

  // Set Default Settings
  sound.format = AFMT_S16_LE;
  sound.stereo = 1;
  sound.freq   = 44100;
  sound.hw_freq = 44100;
  sound.size   = 16;
  sound.trans  = &transCorgi;
  sound.trans_read  = &transReadCorgi;

  sound.volume_left = 80;
  sound.volume_right = 80;

  sound.gain_left = 40;
  sound.gain_right = 40;


  // Change Format
  (*sound.mach.setFormat)(sound.format);

}




/*** Power Management ****************************************************/
#ifdef CONFIG_PM
static int CorgiPMCallBack(struct pm_dev *pm_dev,pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
	  {
	    int err;

	    if ( pxa_audio_state.rd_ref || pxa_audio_state.wr_ref ) {
	      wm8731_suspend();
	      err = i2s_close( CORGI_I2S_ADDRESS );
	      if ( err ) {
		PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not close i2s\n");
	      }
	    }

	    if ( pxa_audio_state.rd_ref != 0 ) {
	      audio_clear_buf(pxa_audio_state.input_stream);
	      *pxa_audio_state.input_stream->drcmr = 0;
	      pxa_free_dma(pxa_audio_state.input_stream->dma_ch);
	    }

	    if ( pxa_audio_state.wr_ref != 0 ) {
	      audio_clear_buf(pxa_audio_state.output_stream);
	      *pxa_audio_state.output_stream->drcmr = 0;
	      pxa_free_dma(pxa_audio_state.output_stream->dma_ch);
	    }

	  }
//	  corgi_HPstatus = HPJACK_STATE_UNDETECT;
#ifdef SOUND_SUPPORT_REMOCON
	  del_timer(&remocon_chk_timers);
#endif
#ifdef AUDIO_SP_DELAY_OFF
	  corgi_sounder_timer(0);
#endif

	  break;

	case PM_RESUME:
	  {
	    int err;

	    /* request DMA channels */
	    if ( pxa_audio_state.wr_ref != 0 ) {
	      init_waitqueue_head(&pxa_audio_state.output_stream->frag_wq);
	      init_waitqueue_head(&pxa_audio_state.output_stream->stop_wq);
	      *pxa_audio_state.output_stream->drcmr = pxa_audio_state.output_stream->dma_ch | DRCMR_MAPVLD;
	      err = pxa_request_dma(pxa_audio_state.output_stream->name, DMA_PRIO_LOW,
				    audio_dma_irq, pxa_audio_state.output_stream);

	      if ( err < 0 ) {
		PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not get DMA\n");
	      }
	      pxa_audio_state.output_stream->dma_ch = err;
	    }

	    if ( pxa_audio_state.rd_ref != 0 ) {
	      init_waitqueue_head(&pxa_audio_state.input_stream->frag_wq);
	      init_waitqueue_head(&pxa_audio_state.input_stream->stop_wq);
	      *pxa_audio_state.input_stream->drcmr = pxa_audio_state.input_stream->dma_ch | DRCMR_MAPVLD;

	      err = pxa_request_dma(pxa_audio_state.input_stream->name, DMA_PRIO_LOW,
				    audio_dma_irq, pxa_audio_state.input_stream);
	      if ( err < 0 ) {
		PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not get DMA\n");
	      }
	      pxa_audio_state.input_stream->dma_ch = err;
	    }

	    if ( pxa_audio_state.rd_ref || pxa_audio_state.wr_ref ) {
	      wm8731_resume();
	      err = i2s_open( CORGI_I2S_ADDRESS , CORGI_DEFAULT_FREQUENCY);
	      if ( err ) {
		PXAI2S_DBGPRINT(DBG_ALWAYS, "Can not open i2s\n");
	      }
	    }
#ifdef SOUND_SUPPORT_REMOCON
		// update status
		if( isChkHPstatus == 0 ){
		  PXAI2S_DBGPRINT(DBG_LEVEL1, "current hp status %d\n", corgi_HPstatus);
		  if( GPLR(GPIO_HP_IN) & GPIO_bit(GPIO_HP_IN) ){
			corgi_HPstatus = HPJACK_STATE_HEADPHONE;
		  }else{
			if( get_remocon_state() == HPJACK_STATE_REMOCON ){
			  corgi_HPstatus = HPJACK_STATE_REMOCON;
			}else{
			  corgi_HPstatus = HPJACK_STATE_NONE;
			}
		  }
		  PXAI2S_DBGPRINT(DBG_LEVEL1, "status changed to %d\n", corgi_HPstatus);
		}
#endif
	  break;
	  }
	}
	return 0;
}
#endif


/*** Config & Setup *********************************************************/
static int __init pxa_i2s_init(void)
{
	sound.mach = machCorgi;

	printk("Corgi audio driver initialize\n");

	(*sound.mach.init)();

	/* Set default settings. */
	sq_init();

	/* Set up /dev/sndstat. */
	state_init();

	/* Set up /dev/mixer. */
	mixer_init();


#ifdef CONFIG_PM
	pxa_sound_pm_dev = pm_register(PM_SYS_DEV, 0, CorgiPMCallBack);
#endif

	return 0;
}

static void __exit pxa_i2s_exit(void)
{
  // FIXME : release buffer

  if ( pxa_audio_state.dev_dsp >= 0 ) 
    unregister_sound_dsp(pxa_audio_state.dev_dsp);
  if ( pxa_i2s_codec.dev_mixer >= 0 ) 
    unregister_sound_mixer(pxa_i2s_codec.dev_mixer);
  if ( pxa_i2s_state.dev_state >= 0 )
    unregister_sound_special(pxa_i2s_state.dev_state);

}

module_init(pxa_i2s_init);
module_exit(pxa_i2s_exit);


