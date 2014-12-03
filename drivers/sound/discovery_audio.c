/*
 * linux/drivers/sound/discovery_audio.c
 *
 * 16bit sound driver for Sabinal (SHARP)
 *
 * Based on dmasound_iris.c (IRIS)
 *
 *      
 * Copyright (C) 2001  SHARP
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
 *  23-Oct-2001 SHARP
 *      tune hardware control method
 *  12-Nov-2001 Lineo Japan, Inc.
 *  2002 Steve Lin modify for Discovery.
 *  13-Sep-2002 Alamy Liu, Eric Lai
 *      Full Duplex mode support
 *      Continue to work after Suspend/Resume
 *      Stereo/Mono bug fix
 *  23-Sep-2002 : Stereo (Left/Right) Volumn - Alamy Liu
 *  30-Sep-2002 : 1/2 read() return value bug fix - Eric Lai
 *  01-Oct-2002 : Mic Gain parameter - Alamy Liu
 *  xx-Oct-2002 Alamy Liu
 *	Mic Gain value error after suspend/resume
 *      Return value of ioctl(SNDCTL_DSP_CHANNELS)
 *  17-Oct-2002 : add ioctl(SNDCTL_DSP_GETOSPACE) - Alamy Liu
 *  12-Feb-2001 SHARP
 *      Use all fragment buffer size
 *      Fix: dead-lock in sq_fsync
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/poll.h>
#include <linux/major.h>
#include <linux/config.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/sound.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/pm.h>
#include <linux/proc_fs.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/soundcard.h>
#include <asm/proc/cache.h>

/* Deborah */
#include <asm/arch-cotulla/cotulla.h>
#ifdef ASIC1
#include <asm/arch/discovery_asic.h>
#include <asm/arch/discovery_gpio.h>
#endif
#include <asm/arch/discovery_asic3.h>
#include <asm/arch/irqs.h>
#include "uda1380.h"
#include <asm/arch-cotulla/i2sc.h>
#include <asm/arch/power_consumption.h>

#ifdef ASIC1
extern void set_discovery_egpio1(unsigned short);
extern void clr_discovery_egpio1(unsigned short);
extern void set_discovery_egpio2(unsigned char);
extern void clr_discovery_egpio2(unsigned char);
#endif

extern void cotulla_stop_dma(unsigned int);
extern void Write_I2C_WORD (unsigned char byDeviceAddress,unsigned char byRegisterAddress,unsigned short wData);
extern unsigned short READ_I2C_WORD(unsigned char byDeviceAddress,unsigned char byRegisterAddress);

void Cotulla_Play_Codec_Init(void);
void Cotulla_Record_Codec_Init(void);
void Cotulla_FullDuplex_Codec_Init(void);	// Added by Alamy for O_RDWR
void Codec_1380_DAC_on(void);
void Codec_1380_ADC_on(void);
void Codec_1380_DAC_off(void);
void Codec_1380_ADC_off(void);

void CODEC_SetVolume(int uVolumeR, int uVolumeL);

typedef enum {
	RECORDING,
	PLAYBACK,
	FULLDUPLEX
} access_type_t;
void Cotulla_audio_power_on( access_type_t type );
static void Cotulla_OP_PLV_on(void);
static void Cotulla_OP_RDV_on(void);
void detect_edge_and_set(void);

#ifdef CONFIG_PROC_FS
static void discovery_audio_proc_register(void);
static void discovery_audio_proc_unregister(void);
static int audio_stat_proc_read(char *buf, char **start, off_t offset, int len);
#endif

#define SADR    0xF0400080
#define W1380_CODEC_ADDRESS 0x30
//#define AUDIO_DEVICE_ADDR 0x30
/* */
#define VOLUME_VALUE    0xCCCCCCCC              // use maximum sound


static unsigned short cotulla_volume = 0;
static int cotulla_igain = 0;
static int cotulla_under_playing = 0;
static int cotulla_under_recording= 0 ;
static int cotulla_battery_critical_low = 0;
static int cotulla_do_writing = 0;
static int cotulla_do_reading= 0;
static int cotulla_do_sync= 0;


#define COTULLA_RECORDING   (cotulla_under_recording)

#define DEBUG_MSG	0

extern unsigned short driver_waste;

static int Audio_Clock;
int cotulla_dmasound_irq = -1;
int cotulla_dmarecording_irq = -1;
#define COTULLA_SOUND_DMA_CHANNEL   (cotulla_dmasound_irq)
#define COTULLA_RECORDING_DMA_CHANNEL   (cotulla_dmarecording_irq)

#ifdef MODULE
static int sq_unit = -1;
static int mixer_unit = -1;
static int state_unit = -1;
static int irq_installed = 0;
#endif /* MODULE */

#ifdef CONFIG_PM
static struct pm_dev* cotulla_sound_pm_dev;
#endif

#undef DEBUG
//#define DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif

#define SND_NDEVS   256 /* Number of supported devices */
#define SND_DEV_CTL 0   /* Control port /dev/mixer */
#define SND_DEV_SEQ 1   /* Sequencer output /dev/sequencer (FM
                   synthesizer and MIDI output) */
#define SND_DEV_MIDIN   2   /* Raw midi access */
#define SND_DEV_DSP 3   /* Digitized voice /dev/dsp */
#define SND_DEV_AUDIO   4   /* Sparc compatible /dev/audio */
#define SND_DEV_DSP16   5   /* Like /dev/dsp but 16 bits/sample */
#define SND_DEV_STATUS  6   /* /dev/sndstat */
/* #7 not in use now. Was in 2.4. Free for use after v3.0. */
#define SND_DEV_SEQ2    8   /* /dev/sequencer, level 2 interface */
#define SND_DEV_SNDPROC 9   /* /dev/sndproc for programmable devices */
#define SND_DEV_PSS SND_DEV_SNDPROC



/*** Some declarations ***********************************************/
#define DMASND_TT   1
#define DMASND_FALCON   2
#define DMASND_AMIGA    3
#define DMASND_AWACS    4
#define DMASND_IRIS 5
#define DMASND_COTULLA  6

#define DMASND_MAX_TRANS_SIZE	4096

/*********for recording*******************************/
#define AUDIO_FMT_MASK      (AFMT_S16_LE|AFMT_U8)
#define AUDIO_FMT_DEFAULT   (AFMT_S16_LE)
#define AUDIO_CHANNELS_DEFAULT  2
#define AUDIO_RATE_DEFAULT  8000
#define AUDIO_IGAIN_DEFAULT 3
#define AUDIO_IAMP_DEFAULT  0x7
#define AUDIO_NBFRAGS_DEFAULT   8
#define AUDIO_FRAGSIZE_DEFAULT 8192
/*****************************************************/

#ifdef MODULE
static int catchRadius = 0;
#endif

/*Deborah*/
static int cotulla_resume = 0;
//static int cotulla_suspend = 0;
/* */

/* Current specs for incoming audio data */
static u_int audio_igain = 0;
static int audio_fmt;
static u_int audio_fragsize;
static u_int audio_nbfrags;


// curr_access_flag added by Alamy
//static volatile int curr_access_flag;	// Current access mode flag : 0, O_RDONLY, O_WRONLY, O_RDWR
static volatile int audio_wr_refcount;  /* nbr of concurrent open() for playback */
static volatile int audio_rd_refcount;   /* nbr of concurrent open() for recording */
#define audio_active        (audio_rd_refcount)

#if 1
static DECLARE_WAIT_QUEUE_HEAD(open_queue);

#define SIGNAL_RECEIVED (signal_pending(current))
#define ONE_SECOND  HZ  /* in jiffies (100ths of a second) */
#define SLEEP(queue, time_limit) \
    interruptible_sleep_on_timeout((wait_queue_head_t*)&queue, (time_limit));
#define WAKE_UP(queue)  (wake_up_interruptible((wait_queue_head_t*)&queue))
#endif

#define TRACE 0
#if TRACE
#define TRACE_ON    0
#define TRACE_SEM   0
#define TRACE_SENDDATA  0
#define TRACE_PM    0
#define TRACE_AMP   0
#define TRACE_DAC   0
#define TRACE_OP_SHDN   0
#define TRACE_READ	0	// Debug flag added by Alamy
#define TRACE_WRITE 1
#define TRACE_MUTE  0
#define TRACE_CLOCK 0
#define TRACE_PAIF  0
#define TRACE_SSP   0
#define TRACE_VOLUME    1
#define TRACE_MIC   0
#define TRACE_INTERRUPT 0
int cLevel = 0;
char    *pLevel[16] = {
    /*  0 */"",
    /*  1 */" ",
    /*  2 */"  ",
    /*  3 */"   ",
    /*  4 */"    ",
    /*  5 */"     ",
    /*  6 */"      ",
    /*  7 */"       ",
    /*  8 */"        ",
    /*  9 */"         ",
    /* 10 */"          ",
    /* 11 */"           ",
    /* 12 */"            ",
    /* 13 */"             ",
    /* 14 */"              ",
    /* 15 */"               "
};
char *
indent(int level)
{
  return (level < 16) ? pLevel[level] : pLevel[15];
}

#define P_ID    (current->tgid)

#define ENTER(f,fn) {if(f)printk("%ld:%s+[%d]%s\n",jiffies,indent(cLevel),P_ID,(fn));cLevel++;}

#define LEAVE(f,fn) {cLevel--;if(f)printk("%ld:%s-[%d]%s\n",jiffies,indent(cLevel),P_ID,(fn));}
#else   /* ! TRACE */
#define ENTER(f,fn)
#define LEAVE(f,fn)
#endif  /* end TRACE */

/*
 * DAC power management
 */
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0) || \
    defined(CONFIG_COLLIE_TR1) || defined(CONFIG_COLLIE_DEV)
#define DAC_OFF_WITH_DEVICE_OFF     1
#undef  HARD_MUTE_CTRL_DISABLE      
#else
#undef  DAC_OFF_WITH_DEVICE_OFF
#undef  HARD_MUTE_CTRL_DISABLE      
#endif


#define TRY_DELAY_OFF
#ifdef TRY_DELAY_OFF
static DECLARE_WAIT_QUEUE_HEAD(delay_off);
struct semaphore df_sem;
/*
 * delay execution
 */
static unsigned int DelayedFlag = 0;
#define DELAY_DAC_OFF       0x1
#define DELAY_HARD_MUTE_ON  0x2

////////////////////
#define VOLUME_SCALES   32
#define GAIN_SCALES	49


unsigned char Gain_Scale_Map[101] = 
{
	0,0,0,
	1,1,1,
	2,2,
	3,3,
	4,4,
	5,5,
	6,6,
	7,7,
	8,8,
	9,9,
	10,10,
	11,11,
	12,12,
	13,13,
	14,14,
	15,15,
	16,16,
	17,17,
	18,18,
	19,19,
	20,20,
	21,21,
	22,22,
	23,23,
	24,24,
	25,25,
	26,26,
	27,27,
	28,28,
	29,29,
	30,30,
	31,31,
	32,32,
	33,33,
	34,34,
	35,35,
	36,36,
	37,37,
	38,38,
	39,39,
	40,40,
	41,41,
	42,42,
	43,43,
	44,44,
	45,45,
	46,46,
	47,47,
	48,48,48,
};

unsigned char GAIN_TABLE[GAIN_SCALES] = 
{
	0x0,
	0x1,
	0x2,
	0x3,
	0x4,
	0x5,
	0x6,
	0x7,
	0x8,
	0x9,
	0xa,
	0xb,
	0xc,
	0xd,
	0xe,
	0xf,
	0x10,
	0x11,
	0x12,
	0x13,
	0x14,
	0x15,
	0x16,
	0x17,
	0x18,
	0x19,
	0x1a,
	0x1b,
	0x1c,
	0x1d,
	0x1e,
	0x1f,
	0x20,
	0x21,
	0x22,
	0x23,
	0x24,
	0x25,
	0x26,
	0x27,
	0x28,
	0x29,
	0x2a,
	0x2b,
	0x2c,
	0x2d,
	0x2e,
	0x2f,
	0x30,
};

unsigned char Scale_Map[101] = 
{
	31,31,31,31 ,
	30,30,30,30,
	29,29,29,
	28,28,28,
	27,27,27,
	26,26,26,
	25,25,25,
	24,24,24,
	23,23,23,
	22,22,22,
	21,21,21,
	20,20,20,
	19,19,19,
	18,18,18,
	17,17,17,
	16,16,16,
	15,15,15,
	14,14,14,
	13,13,13,
	12,12,12,
	11,11,11,
	10,10,10,
	9,9,9,
	8,8,8,
	7,7,7,
	6,6,6,
	5,5,5,
	4,4,4,
	3,3,3,
	2,2,2,
	1,1,1,1,
	0,0,0,0,
};

unsigned char VOLUME_TABLE[VOLUME_SCALES] =
{
	0x00,			// 0dB				//0x01			//  0dB			0xFFFFFFFF
	0x01,			// -0.25dB			//0x01,			//  0dB
	0x02,			// -0.5dB			//0x01,			//  0dB
	0x03,			// -0.75dB			//0x02,			// -1dB
	0x04,			// -1dB				//0x02,			// -1dB
	0x06,			// -1.5dB			//0x02,			// -1dB
	0x08,			// -2dB				//0x03,			// -2dB			0xCCCCCCCC
	0x0A,			// -2.5dB			//0x03,			// -2dB
	0x0C,			// -3dB				//0x04,			// -3dB
	0x0E,			// -3.5dB			//0x04,			// -3dB
	0x10,			// -4dB				//0x05,			// -4dB
	0x12,			// -4.5dB			//0x05,			// -4dB
	0x14,			// -5dB				//0x06,			// -5dB			0x99999999
	0x16,			// -5.5dB			//0x06,			// -5dB
	0x18,			// -6dB				//0x07,			// -6dB
	0x1C,			// -7dB				//0x08,			// -7dB
	0x20,			// -8dB				//0x09,			// -8dB
	0x24,			// -9dB				//0x0A,			// -9dB
	0x28,			// -10dB			//0x0B,			// -10dB
	0x2C,			// -11dB			//0x0C,			// -11dB		0x66666666
	0x34,			// -13dB			//0x0E,			// -13dB
	0x3C,			// -15dB			//0x10,			// -15dB
	0x44,			// -17dB			//0x12,			// -17dB
	0x50,			// -20dB			//0x15,			// -20dB
	0x5C,			// -23dB			//0x18,			// -23dB
	0x68,			// -26dB			//0x1B,			// -26dB		0x33333333
	0x74,			// -29dB			//0x1E,			// -29dB
	0x84,			// -33dB			//0x22,			// -33dB
	0x94,			// -37dB			//0x26,			// -37dB
	0xB4,			// -45dB			//0x2E,			// -45dB
	0xD4,			// -53dB			//0x36,			// -53dB
//	0xFF,			// -maxdB			//0x3F,			// -maxdB		0x00000000
	0xFC			// -maxdB
};
    

static inline void ResetDelayAll(void)
{   
    DelayedFlag = 0;
}   
    
static inline int isDelayedExist(void)
{   
    return DelayedFlag;
}   
    
static inline void SetDelay(unsigned int flag)
{
    DelayedFlag |= flag;
}

static inline void ResetDelay(unsigned int flag)
{
    DelayedFlag &= ~flag;
}

static inline unsigned int isDelayed(unsigned int flag)
{
    return DelayedFlag & flag;
}
#endif


/*
 * Buffer Management
 */

typedef struct {
    int size;               /* buffer size */
    char *start;            /* points to actual buffer */
    dma_addr_t dma_addr;    /* physical buffer address */
    struct semaphore sem;   /* down before touching the buffer */
    int dma_size;           /* allocated dma buffer size */
    int dma_count;          /* dma buffer divided count */
} audio_buf_t;

typedef struct {
    audio_buf_t *buffers;   /* pointer to audio buffer structures */
    audio_buf_t *buf;       /* current buffer used by read/write */
    u_int buf_idx;          /* index for the pointer above... */
    u_int fragsize;         /* fragment i.e. buffer size */
    u_int nbfrags;          /* nbr of fragments i.e. buffers */
    dmach_t dma_ch;         /* DMA channel ID */ // D
} audio_stream_t;

static audio_stream_t output_stream;
static audio_stream_t input_stream;

#define NEXT_BUF(_s_,_b_) { \
    (_s_)->_b_##_idx++; \
    (_s_)->_b_##_idx %= (_s_)->nbfrags; \
    (_s_)->_b_ = (_s_)->buffers + (_s_)->_b_##_idx; }
                    

static ssize_t (*ct_func)(const u_char *, size_t, u_char *, ssize_t *, ssize_t) = NULL;
static ssize_t (*ct_read_func)(const u_char *, size_t, u_char *, ssize_t *, ssize_t) = NULL;

#ifdef MODULE
MODULE_PARM(catchRadius, "i");
#endif
MODULE_PARM(numBufs, "i");
MODULE_PARM(bufSize, "i");


#define IOCTL_IN(arg, ret) \
    do { int error = get_user(ret, (int *)(arg)); \
        if (error) return error; \
    } while (0)
#define IOCTL_OUT(arg, ret) ioctl_return((int *)(arg), ret)
/*** Some low level helpers **************************************************/

int clock_set_data[6] = {   
        0x0C, 0x0D, 0x1A, 0x24,0x34,0x48,
};


/* 16 bit mu-law */

static short ulaw2dma16[] = {
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364, -9852,  -9340,  -8828,  -8316,
    -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
    -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
    -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
    -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
    -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
    -1372,  -1308,  -1244,  -1180,  -1116,  -1052,  -988,   -924,
    -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
    -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
    -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
    -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
    -120,   -112,   -104,   -96,    -88,    -80,    -72,    -64,
    -56,    -48,    -40,    -32,    -24,    -16,    -8, 0,
    32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
    23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
    15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
    11900,  11388,  10876,  10364,  9852,   9340,   8828,   8316,
    7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
    5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
    3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
    2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
    1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
    1372,   1308,   1244,   1180,   1116,   1052,   988,    924,
    876,    844,    812,    780,    748,    716,    684,    652,
    620,    588,    556,    524,    492,    460,    428,    396,
    372,    356,    340,    324,    308,    292,    276,    260,
    244,    228,    212,    196,    180,    164,    148,    132,
    120,    112,    104,    96, 88, 80, 72, 64,
    56, 48, 40, 32, 24, 16, 8,  0,
};

/* 16 bit A-law */

static short alaw2dma16[] = {
    -5504,  -5248,  -6016,  -5760,  -4480,  -4224,  -4992,  -4736,
    -7552,  -7296,  -8064,  -7808,  -6528,  -6272,  -7040,  -6784,
    -2752,  -2624,  -3008,  -2880,  -2240,  -2112,  -2496,  -2368,
    -3776,  -3648,  -4032,  -3904,  -3264,  -3136,  -3520,  -3392,
    -22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
    -30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
    -11008, -10496, -12032, -11520, -8960,  -8448,  -9984,  -9472,
    -15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
    -344,   -328,   -376,   -360,   -280,   -264,   -312,   -296,
    -472,   -456,   -504,   -488,   -408,   -392,   -440,   -424,
    -88,    -72,    -120,   -104,   -24,    -8, -56,    -40,
    -216,   -200,   -248,   -232,   -152,   -136,   -184,   -168,
    -1376,  -1312,  -1504,  -1440,  -1120,  -1056,  -1248,  -1184,
    -1888,  -1824,  -2016,  -1952,  -1632,  -1568,  -1760,  -1696,
    -688,   -656,   -752,   -720,   -560,   -528,   -624,   -592,
    -944,   -912,   -1008,  -976,   -816,   -784,   -880,   -848,
    5504,   5248,   6016,   5760,   4480,   4224,   4992,   4736,
    7552,   7296,   8064,   7808,   6528,   6272,   7040,   6784,
    2752,   2624,   3008,   2880,   2240,   2112,   2496,   2368,
    3776,   3648,   4032,   3904,   3264,   3136,   3520,   3392,
    22016,  20992,  24064,  23040,  17920,  16896,  19968,  18944,
    30208,  29184,  32256,  31232,  26112,  25088,  28160,  27136,
    11008,  10496,  12032,  11520,  8960,   8448,   9984,   9472,
    15104,  14592,  16128,  15616,  13056,  12544,  14080,  13568,
    344,    328,    376,    360,    280,    264,    312,    296,
    472,    456,    504,    488,    408,    392,    440,    424,
    88, 72, 120,    104,    24, 8,  56, 40,
    216,    200,    248,    232,    152,    136,    184,    168,
    1376,   1312,   1504,   1440,   1120,   1056,   1248,   1184,
    1888,   1824,   2016,   1952,   1632,   1568,   1760,   1696,
    688,    656,    752,    720,    560,    528,    624,    592,
    944,    912,    1008,   976,    816,    784,    880,    848,
};


/*** Translations ************************************************************/

static ssize_t collie_ct_law(const u_char *userPtr, size_t userCount,
                 u_char frame[], ssize_t *frameUsed,
                 ssize_t frameLeft);
static ssize_t collie_ct_s8(const u_char *userPtr, size_t userCount,
                u_char frame[], ssize_t *frameUsed,
                ssize_t frameLeft);
static ssize_t collie_ct_u8(const u_char *userPtr, size_t userCount,
                u_char frame[], ssize_t *frameUsed,
                ssize_t frameLeft);
static ssize_t collie_ct_s16(const u_char *userPtr, size_t userCount,
                 u_char frame[], ssize_t *frameUsed,
                 ssize_t frameLeft);
static ssize_t collie_ct_u16(const u_char *userPtr, size_t userCount,
                 u_char frame[], ssize_t *frameUsed,
                 ssize_t frameLeft);

/*** Machine definitions *****************************************************/


typedef struct {
    int type;
    void *(*dma_alloc)(unsigned int, int);
    void (*dma_free)(void *, unsigned int);
    int (*irqinit)(void);
#ifdef MODULE
    void (*irqcleanup)(void);
#endif /* MODULE */
    void (*init)(void);
    void (*silence)(void);
    int (*setFormat)(int);
    int (*setVolume)(int);
    int (*setBass)(int);
    int (*setTreble)(int);
    int (*setGain)(int);
    void (*play)(void);
} MACHINE;


/*** Low level stuff *********************************************************/


typedef struct {
    int format;     /* AFMT_* */
    int stereo;     /* 0 = mono, 1 = stereo */
    int size;       /* 8/16 bit*/
    int speed;      /* speed */
} SETTINGS;


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


struct sound_settings {
    MACHINE mach;       /* machine dependent things */
    SETTINGS hard;      /* hardware settings */
    SETTINGS soft;      /* software settings */
    SETTINGS dsp;       /* /dev/dsp default settings */
    TRANS *trans;       /* supported translations */
    TRANS *trans_read;  /* supported translations for record */
    int volume_left;    /* volume (range is machine dependent) */
    int volume_right;
    int bass;       /* tone (range is machine dependent) */
    int treble;
    int gain;
    int minDev;     /* minor device number currently open */
};

static struct sound_settings sound;

#ifdef CONFIG_PM
extern int autoPowerCancel;
#endif

static void Cotulla_Set_Volume(unsigned long volume);
static int Cotulla_Get_Volume(void);
static void Cotulla_Set_Igain(unsigned long gain);
static int Cotulla_Get_Igain(void);
static void Cotulla_disable_sound(void);
static int CotullaIrqInit(void);
static int CotullaGetSamp(void);

#ifdef CONFIG_PM
#if 0
static void Collie_clock_stop(void);
static void Collie_FS8KLPF_stop(void);
#endif
#endif

static int CotullaIrqInit(void);
static int CotullaGetSamp(void);

#ifdef MODULE
static void CotullaIrqCleanUp(void);
#endif /* MODULE */
static void CotullaSilence(void);
static void CotullaInit(void);
static int CotullaSetFormat(int format);
static void Cotulla_sq_interrupt(void*, int);
static int sq_allocate_buffers(audio_stream_t*);
static void sq_release_buffers(audio_stream_t*);

/*** Mid level stuff *********************************************************/
static void sound_silence(void);
static void sound_init(void);
static int sound_set_format(int format);
static int sound_set_speed(int speed);
static int sound_set_stereo(int stereo);

/*
 * /dev/mixer abstraction
 */

struct sound_mixer {
    int busy;
};

static struct sound_mixer mixer;

/*
 * /dev/sndstat
 */

struct sound_state {
    int busy;
    char buf[512];
    int len, ptr;
};

static void Cotulla_I2S_Init(void);
static void Cotulla_I2C_Init(u8 byDeviceAddress);
static struct sound_state state;

/*** Common stuff ********************************************************/

static long long sound_lseek(struct file *file, long long offset, int orig);
static inline int ioctl_return(int *addr, int value)
{
    ENTER(TRACE_ON,"ioctl_return");
    if (value < 0) {
        LEAVE(TRACE_ON,"ioctl_return");
        return(value);
    }

    LEAVE(TRACE_ON,"ioctl_return");
    return put_user(value, addr)? -EFAULT: 0;
}


static inline void set_dma_divided_buffer(audio_stream_t *s, audio_buf_t *b, dmach_t channel, int fsize)
{
	int	dmasize;
	dma_addr_t	daddr;

	b->dma_count = fsize >> 12;	// fsize/DMASND_MAX_TRANS_SIZE;
	if( fsize & (DMASND_MAX_TRANS_SIZE-1) )
	  b->dma_count++;

	daddr = b->dma_addr;
	while( fsize > 0 ){
		dmasize = min( fsize, DMASND_MAX_TRANS_SIZE );
		cotulla_dma_queue_buffer(channel,
								 (void *) b, daddr, dmasize);
		fsize -= dmasize;
		daddr += dmasize;
	}
}


/*** Config & Setup **********************************************************/


void dmasound_init(void);
void dmasound_setup(char *str, int *ints);


/*** Translations ************************************************************/


/* ++TeSche: radically changed for new expanding purposes...
 *
 * These two routines now deal with copying/expanding/translating the samples
 * from user space into our buffer at the right frequency. They take care about
 * how much data there's actually to read, how much buffer space there is and
 * to convert samples into the right frequency/encoding. They will only work on
 * complete samples so it may happen they leave some bytes in the input stream
 * if the user didn't write a multiple of the current sample size. They both
 * return the number of bytes they've used from both streams so you may detect
 * such a situation. Luckily all programs should be able to cope with that.
 *
 * I think I've optimized anything as far as one can do in plain C, all
 * variables should fit in registers and the loops are really short. There's
 * one loop for every possible situation. Writing a more generalized and thus
 * parameterized loop would only produce slower code. Feel free to optimize
 * this in assembler if you like. :)
 *
 * I think these routines belong here because they're not yet really hardware
 * independent, especially the fact that the Falcon can play 16bit samples
 * only in stereo is hardcoded in both of them!
 *
 * ++geert: split in even more functions (one per format)
 */

static ssize_t collie_ct_law(const u_char *userPtr, size_t userCount,
                 u_char frame[], ssize_t *frameUsed,
                 ssize_t frameLeft)
{
	short *table = sound.soft.format == AFMT_MU_LAW ? ulaw2dma16: alaw2dma16;
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int val, stereo = sound.soft.stereo;

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

static ssize_t collie_ct_s8(const u_char *userPtr, size_t userCount,
              u_char frame[], ssize_t *frameUsed,
              ssize_t frameLeft)
{
    ssize_t count, used;
    short *p = (short *) &frame[*frameUsed];
    int val, stereo = sound.soft.stereo;

    ENTER(TRACE_ON,"collie_ct_s8");
    frameLeft >>= 2;
    if (stereo)
        userCount >>= 1;
    used = count = min(userCount, frameLeft);
    if (!COTULLA_RECORDING) {
        while (count > 0) {
            u_char data;

            if (get_user(data, userPtr++)) {
                LEAVE(TRACE_ON,"collie_ct_s8");
                return -EFAULT;
            }
            val = ( data - 0x80 );
            *p++ = val;     /* Left Ch. */
            if ( stereo ) {
                if ( get_user(data, userPtr++)) {
                    LEAVE(TRACE_ON,"collie_ct_s8");
                    return -EFAULT;
                }
                val = ( data - 0x80 );
            }
            *p++ = val;     /* Right Ch. */
            count--;
        }
    } else {
        while (count > 0) {
            u_char data;
            int ave;

            if (get_user(data, userPtr++)) {
                LEAVE(TRACE_ON,"collie_ct_s8");
                return -EFAULT;
            }
            val = ( data - 0x80 );
            ave = val;      /* Left Ch. */
            if ( stereo ) {
                if ( get_user(data, userPtr++)) {
                    LEAVE(TRACE_ON,"collie_ct_s8");
                    return -EFAULT;
                }
                val = ( data - 0x80 );
            }
            ave += val;     /* Right Ch. */
            ave >>= 1;
            *p++ = 0;       /* Left Ch. */
            *p++ = ave;     /* Right Ch. */
            count--;
        }
    }
    *frameUsed += used * 4;
    LEAVE(TRACE_ON,"collie_ct_s8");
    return stereo? used * 2: used;
}


static ssize_t collie_ct_u8(const u_char *userPtr, size_t userCount,
              u_char frame[], ssize_t *frameUsed,
              ssize_t frameLeft)
{
	int ch = (sound.soft.stereo)?2:1;
	unsigned char *src = (unsigned char *)userPtr;
	short *dstct = (short *)frame;
	short tmpval;
	int frameMax = frameLeft >> 2;
    int userMax = userCount >> (ch-1);
    int idx,samplingCount = min(frameMax,userMax);

    for (idx=0; idx<samplingCount; idx++) {
		if (get_user(tmpval,src++)) {
			return -EFAULT;
		}
		if (ch>1){
			*dstct++ = (tmpval-128)<<8;
			if (get_user(tmpval,src++)) {
				return -EFAULT;
			}
		}else if( COTULLA_RECORDING ){
			*dstct++ = 0;
			*dstct++ = (tmpval-128)<<8;
		}else{
			*dstct++ = (tmpval-128)<<8;
			*dstct++ = (tmpval-128)<<8;
		}
	}

	*frameUsed = samplingCount * 2 * 2; // dma is always stereo 16bit
	return samplingCount * ch;
}


static ssize_t collie_ct_s16(const u_char *userPtr, size_t userCount,
               u_char frame[], ssize_t *frameUsed,
               ssize_t frameLeft)
{
	int ch = (sound.soft.stereo)?2:1;
	short *src = (short *)userPtr;
	short *dstct = (short *)frame;
    int frameMax = frameLeft >> 2;
    int userMax = userCount >> ch;
    int samplingCount = min(frameMax,userMax);
	short tmpval;

    if (ch>1) { // stereo
		if (copy_from_user(dstct,src,samplingCount * 4)) {
			return -EFAULT;
		}
	} else { // mono
		int idx;
		for (idx=0; idx<samplingCount; idx++) {
			if (get_user(tmpval,src++)) {
				return -EFAULT;
			}
			if( COTULLA_RECORDING ){
				*dstct++ = 0;
				*dstct++ = tmpval;
			}else{
				*dstct++ = tmpval;
				*dstct++ = tmpval;
			}
		}
	}

    *frameUsed = samplingCount * 2 * 2; // dma is always stereo 16bit
    return samplingCount * ch * 2;
}

static ssize_t collie_ct_u16(const u_char *userPtr, size_t userCount,
               u_char frame[], ssize_t *frameUsed,
               ssize_t frameLeft)
{
    ssize_t count, used;
    int mask = (sound.soft.format == AFMT_U16_LE? 0x0080: 0x8000);
    int stereo = sound.soft.stereo;
    short *fp = (short *) &frame[*frameUsed];
    short *up = (short *) userPtr;

    ENTER(TRACE_ON,"collie_ct_u16");
    frameLeft >>= 2;
    userCount >>= (stereo? 2: 1);
    used = count = min(userCount, frameLeft);
    if (!COTULLA_RECORDING) {
        while (count > 0) {
            int data;
            int temp;
            if (get_user(data, up++)) {
                LEAVE(TRACE_ON,"collie_ct_u16");
                return -EFAULT;
            }
            data ^= mask;
            *fp++ = data;           /* Left Ch. */
            if (stereo) {
                if (get_user(temp, up++)) {
                    LEAVE(TRACE_ON,"collie_ct_u16");
                    return -EFAULT;
                }
                temp ^= mask;
                data  = temp;
                data ^= mask;
            }
            *fp++ = data;           /* Right Ch. */
            count--;
        }
    } else {
        while (count > 0) {
            int data;
            int temp;
            int ave;
            if (get_user(data, up++)) {
                LEAVE(TRACE_ON,"collie_ct_u16");
                return -EFAULT;
            }
            data ^= mask;
            ave = data;         /* Left Ch. */
            if (stereo) {
                if (get_user(temp, up++)) {
                    LEAVE(TRACE_ON,"collie_ct_u16");
                    return -EFAULT;
                }
                temp ^= mask;
                data  = temp;
                data ^= mask;
            }
            ave += data;
            ave >>= 1;
            *fp++ = 0;          /* Left Ch. */
            *fp++ = ave;            /* Right Ch. */
            count--;
        }
    }
    *frameUsed += used * 4;
    LEAVE(TRACE_ON,"collie_ct_u16");
    return stereo? used * 4: used * 2;
}

static TRANS transCotulla = {
    collie_ct_law, collie_ct_law, collie_ct_s8, collie_ct_u8,
    collie_ct_s16, collie_ct_u16, collie_ct_s16, collie_ct_u16
};


static ssize_t collie_ct_read_u8(const u_char *userPtr, size_t userCount,
              u_char frame[], ssize_t *frameUsed,
              ssize_t frameLeft)
{
	char *usrbuf = (char *)userPtr;
    short *dmabuf = (short *)frame;
	int val, ch = (sound.soft.stereo)?2:1;
    int dmaMax = frameLeft >> 2; // dma is always stereo
    int userMax = userCount >> (ch-1);
    int ct,samplingCount = min(dmaMax,userMax);

    ENTER(TRACE_ON,"collie_ct_read_u8");

    for (ct=0; ct<samplingCount; ct++) {
		val = (*dmabuf >> 8);
		if( val > 127 ) val = 127;
		else if( val < -128 ) val = -128;
		
		if (put_user((char)(val+128),usrbuf++)) { // left channel
			return -EFAULT;
		}
		dmabuf++;
		if (ch>1) { // right channel
			val = (*dmabuf >> 8);
			if( val > 127 ) val = 127;
			else if( val < -128 ) val = -128;

			if (put_user((char)(val+128),usrbuf++)) {
				return -EFAULT;
			}
		}
		dmabuf++;
    }
    LEAVE(TRACE_ON,"collie_ct_read_u8");
    *frameUsed = samplingCount * 2 * 2; // dma is always stereo
    return samplingCount * ch;
}

static ssize_t collie_ct_read_s16(const u_char *userPtr, size_t userCount,
              u_char frame[], ssize_t *frameUsed,
              ssize_t frameLeft)
{
	short *usrbuf = (short *)userPtr;
    short *dmabuf = (short *)frame;
	int ch = (sound.soft.stereo)?2:1;
    int dmaMax = frameLeft >> 2; // dma is always stereo
    int userMax = userCount >> ch;
    int ct,samplingCount = min(dmaMax,userMax);

    ENTER(TRACE_ON,"collie_ct_read_s16");

	if (ch>1) { // stereo
		if (copy_to_user(dmabuf,usrbuf,samplingCount<<2)) {
			return -EFAULT;
		}
	}else{
		for (ct=0; ct<samplingCount; ct++) {
			if (put_user(*dmabuf,usrbuf++)) { // left channel
				return -EFAULT;
			}
			dmabuf += 2;
		}
	}
    LEAVE(TRACE_ON,"collie_ct_read_s16");
    *frameUsed = samplingCount * 2 * 2; // dma is always stereo
    return samplingCount * ch * 2;
}



static TRANS transReadCotulla = {
    0, 0, 0, collie_ct_read_u8,
    0, 0, collie_ct_read_s16, 0
};

/*** Low level stuff *********************************************************/
static void Cotulla_i2sc_clock_enable(void)
{
	CKEN |= 0x4100;
}

static void Cotulla_i2sc_clock_disable(void)
{
	CKEN &= ~0x4100;
}

static void Cotulla_Set_Igain(unsigned long gain)
{
	unsigned int	gainR, gainL;

	ENTER(TRACE_ON,"Cotulla_Set_Igain");

	gainR = (gain & 0xff00) >> 8;
	gainL = gain & 0xff;
	if (gainR > 100)	gainR = 100;
	if (gainL > 100)	gainL = 100;

	audio_igain = (gainR << 8) | gainL;

	LEAVE(TRACE_ON,"Cotulla_Set_Igain");
}

static int Cotulla_Get_Igain(void)
{    
	ENTER(TRACE_ON,"Cotulla_Get_Igain");
	LEAVE(TRACE_ON,"Cotulla_Get_Igain");
	return audio_igain;
}

static void Cotulla_Set_Volume(unsigned long volume)
{
	ENTER(TRACE_ON, "Enter : Cotulla_Set_Volume()");


	sound.volume_left = volume & 0xff;
	if ( sound.volume_left > 100 ) sound.volume_left = 100;

	sound.volume_right = ((volume & 0xff00) >> 8);
	if ( sound.volume_right > 100 ) sound.volume_right = 100;

	LEAVE(TRACE_ON, "Leave : Cotulla_Set_Volume()");
}


static int Cotulla_Get_Volume(void)
{
	ENTER(TRACE_ON,"Cotulla_Get_Volume");
	LEAVE(TRACE_ON,"Cotulla_Get_Volume");
	return ( sound.volume_right << 8 | sound.volume_left );
}


static void wait_ms(int ten_ms)
{
	ENTER(TRACE_ON,"wait_ms");
	LEAVE(TRACE_ON,"wait_ms");
	schedule_timeout(ten_ms);
}


static inline void Cotulla_audio_clock_on(void)
{
	CKEN |= 0x4100;
}

static inline void Cotulla_audio_clock_off(void)
{
	CKEN &= ~0x4100;
}

static inline void Cotulla_set_audio_clock(void)
{
	ENTER(TRACE_CLOCK, "Cotulla_set_audio_clock_");
	Audio_Clock  = CotullaGetSamp();
	I2S_SADIV = Audio_Clock;
	LEAVE(TRACE_CLOCK, "Cotulla_set_audio_clock");
}

static void Cotulla_OP_RDV_on(void)
{
	ENTER(TRACE_OP_SHDN,"Cotulla_OP_RDV_on");
	Cotulla_input_gain_on();
	LEAVE(TRACE_OP_SHDN,"Cotulla_OP_RDV_on");
}

static void Cotulla_OP_PLV_on(void)
{
	ENTER(TRACE_OP_SHDN,"Cotulla_OP_PLV_on");
	Cotulla_volume_on();
	LEAVE(TRACE_OP_SHDN,"Cotulla_OP_PLV_on");
}


void CODEC_SetIgain(unsigned long gain)
{
	UDA1380REGS CodecReg;
	unsigned char	gainR, gainL;
	unsigned char	gainReg;

	// According to SL-A300's Hardware spec. MIC uses only Left channel

	gainR = (gain & 0xff00) >> 8;
	gainL = gain & 0x00ff;

	Cotulla_CODEC_REGS_Init(&CodecReg);
	gainReg = GAIN_TABLE[Gain_Scale_Map[gainR]];
	CodecReg.decimator_vol.decimator_vol_u.decimator_vol_s.right = gainReg;
	gainReg = GAIN_TABLE[Gain_Scale_Map[gainL]];
	CodecReg.decimator_vol.decimator_vol_u.decimator_vol_s.left = gainReg;
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.decimator_vol.reg_num,
				   CodecReg.decimator_vol.decimator_vol_u.reg_val);
}

/*
  CODEC_SetVolume() rewirte by Alamy
  Support Stereo volume setting
*/
void CODEC_SetVolume(int uVolumeR, int uVolumeL)
{
	unsigned char	ucVol=0;
	UDA1380REGS	CodecReg;
    
	ENTER(TRACE_VOLUME, "Enter : CODEC_SetVolume()");

	Cotulla_CODEC_REGS_Init( &CodecReg );

	// Reg 10H : Master volume control
	ucVol   = VOLUME_TABLE[Scale_Map[uVolumeR]];
	CodecReg.master_vol.master_vol_u.master_vol_s.right = ucVol;
	ucVol	= VOLUME_TABLE[Scale_Map[uVolumeL]];
	CodecReg.master_vol.master_vol_u.master_vol_s.left = ucVol;

	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.master_vol.reg_num,
				   CodecReg.master_vol.master_vol_u.reg_val);

	LEAVE(TRACE_VOLUME, "Leave : CODEC_SetVolume()");
}


static inline void Cotulla_volume_init(void)
{
   	ENTER(TRACE_VOLUME, "Cotulla_volume_init");
	cotulla_volume = 0;
	LEAVE(TRACE_VOLUME, "Cotulla_volume_init");
}

static inline void Cotulla_input_gain_on(void)
{
	ENTER(TRACE_VOLUME, "Cotulla_input_gain_on");

	if( cotulla_igain != audio_igain) {
		CODEC_SetIgain(audio_igain);
		cotulla_igain = audio_igain;
	}

	LEAVE(TRACE_VOLUME, "Cotulla_input_gain_on");
}

/*
  Cotulla_volume_on() rewrite by Alamy
  Fix Stereo Left/Right channel issue (Right channel has no effect)
*/
static inline void Cotulla_volume_on( void )
{
unsigned char vol_temp;
unsigned short	setting_volume;

	ENTER(TRACE_VOLUME, "Cotulla_volume_on");

	driver_waste &= ~VOLUME_MASK;
	vol_temp = (unsigned char)(sound.volume_left/20);
	driver_waste |= vol_temp;

	// If Battery is Critical low, Limit max volume to 40
	if (cotulla_battery_critical_low) {
		if (sound.volume_right > 40)
			sound.volume_right = 40;
		if (sound.volume_left > 40)
			sound.volume_left = 40;
	}

	// Set CODEC only when volume value is different
	setting_volume = (sound.volume_right << 8) | sound.volume_left;
	if (cotulla_volume != setting_volume) {
		CODEC_SetVolume(sound.volume_right, sound.volume_left);
		cotulla_volume = setting_volume;
	}

    	LEAVE(TRACE_VOLUME, "Cotulla_volume_on");
}


static inline void Cotulla_volume_off(void)
{
	ENTER(TRACE_VOLUME, "Cotulla_volume_off");
	if (cotulla_volume) {
		cotulla_volume = 0;
	}
	LEAVE(TRACE_VOLUME, "Cotulla_volume_off");
}


static inline void Cotulla_sound_hard_init(void)
{
    	ENTER(TRACE_ON, "Cotulla_sound_hard_init(I2S, I2C init)");
    
    	Cotulla_I2C_Init(W1380_CODEC_ADDRESS);
    	Cotulla_I2S_Init();
    	Cotulla_volume_init();
    	udelay(10);

    	LEAVE(TRACE_ON, "Cotulla_sound_hard_init(I2S, I2C init)");
}

static void Cotulla_i2sc_play_power_saving(void)
{
	UDA1380REGS CodecReg;
	Cotulla_CODEC_REGS_Init(&CodecReg);
	
	CodecReg.iis.iis_u.reg_val = 0x0000;	// 0x01H, set codec to slave mode
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.iis.reg_num, CodecReg.iis.iis_u.reg_val);
	CodecReg.mute.mute_u.reg_val = 0x4800;	//0x13h
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.mute.reg_num, CodecReg.mute.mute_u.reg_val);
	CodecReg.power.power_u.reg_val = 0x0100;	// 0x02H, turn on only bias power
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);
	CodecReg.mode.mode_u.reg_val = 0x0000;	// 0x00H 
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.mode.reg_num, CodecReg.mode.mode_u.reg_val);

	I2C_ICR &= ~I2C_ICR_SCLE;
	I2C_ICR &= ~I2C_ICR_IUE;	// i2c and clock disable 
	I2S_SACR0 &= ~0x0001;    	// i2s clk disable


	GPDR(0) |= 0xD0000000;
	GPDR(1) |= 0x00000001; 		// set 28-32 gpio output

	GPCR(0) = 0xD0000000;
	GPCR(1) = 0x00000001; 		// set gpio 28-32 to low
	
	
	GPAFR0_U &= ~0xff000000;	// set alternate function to 0
	GPAFR1_L &= ~0x01;
}

static void Cotulla_i2sc_record_power_saving(void)
{
	UDA1380REGS CodecReg;

	DPRINTK("+Cotulla_i2sc_unit_power_saving\n");
	Cotulla_CODEC_REGS_Init(&CodecReg);
	
	CodecReg.iis.iis_u.reg_val = 0x0000;		// 0x01H, set codec to slave mode
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.iis.reg_num, CodecReg.iis.iis_u.reg_val);

	CodecReg.pga.pga_u.reg_val = 0x8000;
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.pga.reg_num,CodecReg.pga.pga_u.reg_val);

	CodecReg.power.power_u.reg_val = 0x0100;	// 0x02H, turn on only bias power
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);
	CodecReg.mode.mode_u.reg_val = 0x0000;		// 0x00H 
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.mode.reg_num, CodecReg.mode.mode_u.reg_val);

	udelay(5000);
	I2C_ICR &= ~I2C_ICR_SCLE;
	I2C_ICR &= ~I2C_ICR_IUE;	// i2c and clock disable 
	I2S_SACR0 &= ~0x0001;    	// i2s clk disable

	GPDR(0) |= 0xD0000000;
	GPDR(1) |= 0x00000001; 		// set 28-32 gpio output

	GPCR(0) = 0xD0000000;
	GPCR(1) = 0x00000001; 		// set gpio 28-32 to low
	
	GPAFR0_U &= ~0xff000000;	// set alternate function to 0
	GPAFR1_L &= ~0x01;
	DPRINTK("-Cotulla_i2sc_unit_power_saving\n");
}

static void Cotulla_i2sc_unit_power_saving(void)
{
	UDA1380REGS CodecReg;

	DPRINTK("+Cotulla_i2sc_unit_power_saving\n");
	Cotulla_CODEC_REGS_Init(&CodecReg);
	
	CodecReg.iis.iis_u.reg_val = 0x0000;	// 0x01H, set codec to slave mode
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.iis.reg_num, CodecReg.iis.iis_u.reg_val);
	CodecReg.power.power_u.reg_val = 0x0100;	// 0x02H, turn on only bias power
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);

	CodecReg.mute.mute_u.reg_val = 0x4800;	//0x13h
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.mute.reg_num, CodecReg.mute.mute_u.reg_val);
	
	CodecReg.mode.mode_u.reg_val = 0x0000;	// 0x00H 
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
				   CodecReg.mode.reg_num, CodecReg.mode.mode_u.reg_val);

	I2C_ICR &= ~I2C_ICR_SCLE;
	I2C_ICR &= ~I2C_ICR_IUE;	// i2c and clock disable 
	I2S_SACR0 &= ~0x0001;    	// i2s clk disable

	GPDR(0) |= 0xD0000000;
	GPDR(1) |= 0x00000001; 		// set 28-32 gpio output

	GPCR(0) = 0xD0000000;
	GPCR(1) = 0x00000001; 		// set gpio 28-32 to low

	GPAFR0_U &= ~0xff000000;	// set alternate function to 0
	GPAFR1_L &= ~0x01;
	DPRINTK("-Cotulla_i2sc_unit_power_saving\n");
}

static void Cotulla_disable_sound(void)
{
	ENTER(TRACE_ON,"Cotulla_disable_sound");
	// Comment by Alamy. It seems that cotulla_dma_stop() is useless
	// because we have cotulla_dma_flush_all()
	cotulla_dma_stop(COTULLA_SOUND_DMA_CHANNEL);
	cotulla_dma_stop(COTULLA_RECORDING_DMA_CHANNEL);
	cotulla_dma_flush_all(COTULLA_SOUND_DMA_CHANNEL);
	cotulla_dma_flush_all(COTULLA_RECORDING_DMA_CHANNEL);
#ifndef TRY_DELAY_OFF
	Collie_volume_half_off();
	Collie_hard_mute_on();
	Collie_soft_mute_on();

	Collie_ssp_off();

#endif
	LEAVE(TRACE_ON,"Collie_disable_sound");
}

static void Cotulla_HP_on(void)
{
	UDA1380REGS CodecReg;

	ENTER(TRACE_ON,"Cotulla_HP_on");
	Cotulla_CODEC_REGS_Init(&CodecReg);
	CodecReg.power.power_u.reg_val = 0x2500;
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
		       CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);
	LEAVE(TRACE_ON,"Cotulla_HP_on");
}


static void Cotulla_HP_off(void)
{
	UDA1380REGS CodecReg;

	ENTER(TRACE_ON,"Cotulla_HP_off");
	Cotulla_CODEC_REGS_Init(&CodecReg);
	CodecReg.power.power_u.reg_val = 0x0500;
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
		       CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);
	LEAVE(TRACE_ON,"Cotulla_HP_off");
}


static void CotullaSilence(void)
{
	ENTER(TRACE_ON,"CotullaSilence");
	Cotulla_disable_sound();
	Cotulla_MIC_off();
	Cotulla_SPEAKER_off();
	LEAVE(TRACE_ON,"CotullaSilence");
}


static int CotullaGetSamp(void)
{
    ENTER(TRACE_ON,"CotullaGetSamp");
    switch (sound.soft.speed) {
    case 8000:
        LEAVE(TRACE_ON,"CotullaGetSamp");
        return clock_set_data[5];
    case 44100:
        LEAVE(TRACE_ON,"CotullaGetSamp");
        return clock_set_data[1];
    case 22050:
        LEAVE(TRACE_ON,"CotullaGetSamp");
        return clock_set_data[2];
    case 11025:
        LEAVE(TRACE_ON,"CotullaGetSamp");
        return clock_set_data[4];
    case 48000:
        LEAVE(TRACE_ON,"CotullaGetSamp");
        return clock_set_data[0];
    case 16000:
        LEAVE(TRACE_ON,"CollieGetSamp");
        return clock_set_data[3];
    default:
        printk("Cotulla sound: Illegal sound rate %d\n", sound.soft.speed);
        LEAVE(TRACE_ON,"CotullaGetSamp");
        return clock_set_data[4];
    }
}

static inline void Cotulla_AUD_on(void)
{
	ENTER(TRACE_ON,"Cotulla_AUD_on");
#ifdef ASIC1
	set_discovery_egpio1(EGPIO1_DISCOVERY_AUD_PWR_ON);
#endif
    	ASIC3_GPIO_DIR_B |= AUD_PWR_ON;	// set pin output
	ASIC3_GPIO_PIOD_B |= AUD_PWR_ON;
	LEAVE(TRACE_ON,"Cotulla_AUD_on");
}

static inline void Cotulla_AUD_off(void)
{
	ENTER(TRACE_ON,"Cotulla_AUD_off");
#ifdef ASIC1
	clr_discovery_egpio1(EGPIO1_DISCOVERY_AUD_PWR_ON);
#endif
    	ASIC3_GPIO_DIR_B |= AUD_PWR_ON;	// set pin output
	ASIC3_GPIO_PIOD_B &= ~AUD_PWR_ON;
	LEAVE(TRACE_ON,"Cotulla_AUD_off");
}

static inline void Cotulla_SPEAKER_on(void)
{
	ENTER(TRACE_ON,"Cotulla_SPEAKER_on");
#ifdef ASIC1
	clr_discovery_egpio1(EGPIO1_DISCOVERY_SPK_ON);  //active low 
#endif
	ASIC3_GPIO_DIR_B |= SPK_ON;	// set pin output
	ASIC3_GPIO_PIOD_B |= SPK_ON;	// active high in Asic3
	LEAVE(TRACE_ON,"Cotulla_SPEAKER_on");
}

static inline void Cotulla_SPEAKER_off(void)
{
	ENTER(TRACE_ON,"Cotulla_SPEAKER_off");
#ifdef ASIC1
	set_discovery_egpio1(EGPIO1_DISCOVERY_SPK_ON);  
#endif
	ASIC3_GPIO_DIR_B |= SPK_ON;	// set pin output
	ASIC3_GPIO_PIOD_B &= ~SPK_ON;	// active high in Asic3
	LEAVE(TRACE_ON,"Cotulla_SPEAKER_off");
}

int Cotulla_HP_Detect(void)
{
	ASIC3_GPIO_DIR_D &= ~HEADPHONE_IN;	//set pin input
	if (ASIC3_GPIO_PIOD_D & HEADPHONE_IN)
		return 1;
	else
		return 0;	
}

static inline void Cotulla_MIC_on(void)
{
	ENTER(TRACE_ON,"Cotulla_MIC_on");
	ASIC3_GPIO_DIR_B |= HP_MIC;	// set pin output
	ASIC3_GPIO_PIOD_B &= ~HP_MIC; // set mic function for recording for DVT
	LEAVE(TRACE_ON,"Cotulla_MIC_on");
}

static inline void Cotulla_MIC_off(void)
{
	ENTER(TRACE_ON,"Cotulla_MIC_off");
	ASIC3_GPIO_DIR_B |= HP_MIC;	// set pin output
	ASIC3_GPIO_PIOD_B |= HP_MIC;  //for DVT
	LEAVE(TRACE_ON,"Cotulla_MIC_off");
}

void Cotulla_CODEC_REGS_Init(UDA1380REGS *pCodecReg)
{
    pCodecReg->mode.reg_num = 0x00;         // address
    pCodecReg->mode.mode_u.reg_val = 0;     // value
    pCodecReg->iis.reg_num = 0x01;          
    pCodecReg->iis.iis_u.reg_val = 0;
    pCodecReg->power.reg_num = 0x02;        
    pCodecReg->power.power_u.reg_val = 0;   
    pCodecReg->analog.reg_num = 0x03;       
    pCodecReg->analog.analog_u.reg_val = 0;
    pCodecReg->reserved.reg_num = 0x04;     
    pCodecReg->reserved.reserved_u.reg_val = 0; 
    pCodecReg->master_vol.reg_num = 0x10;   
    pCodecReg->master_vol.master_vol_u.reg_val = 0;     
    pCodecReg->mixer_vol.reg_num = 0x11;    
    pCodecReg->mixer_vol.mixer_vol_u.reg_val = 0;
    pCodecReg->bass_treble.reg_num = 0x12;  
    pCodecReg->bass_treble.bass_treble_u.reg_val = 0;
    pCodecReg->mute.reg_num = 0x13;         
    pCodecReg->mute.mute_u.reg_val = 0;
    pCodecReg->misc.reg_num = 0x14;         
    pCodecReg->misc.misc_u.reg_val = 0;
    pCodecReg->decimator_vol.reg_num = 0x20;
    pCodecReg->decimator_vol.decimator_vol_u.reg_val = 0;   
    pCodecReg->pga.reg_num = 0x21;          
    pCodecReg->pga.pga_u.reg_val = 0;
    pCodecReg->adc.reg_num = 0x22;          
    pCodecReg->adc.adc_u.reg_val = 0;
    pCodecReg->agc.reg_num = 0x23;          
    pCodecReg->agc.agc_u.reg_val = 0;
    pCodecReg->reset.reg_num = 0x7F;        
    pCodecReg->reset.reg_val = 0;           
                                            
    pCodecReg->interpolator.reg_num = 0x18; 
    pCodecReg->decimator.reg_num = 0x28;    

}   

static void Cotulla_I2C_Init(u8 byDeviceAddress)
{
    
    ENTER(TRACE_ON,"Cotulla_I2C_Init");
    I2C_ICR |= I2C_ICR_UR;  // reset
    I2C_ISR = 0;    // clear all ISR
    I2C_ICR &= ~I2C_ICR_UR; // clear reset

	I2C_ICR &= ~I2C_ICR_FM;  // slow mode (0=100K,1=400K)
    
    // disable all I2C interrupts   
    I2C_ICR &= ~I2C_ICR_SADIE;
    I2C_ICR &= ~I2C_ICR_ALDIE;
    I2C_ICR &= ~I2C_ICR_SSDIE;
    I2C_ICR &= ~I2C_ICR_BEIE;
    I2C_ICR &= ~I2C_ICR_IRFIE;
    I2C_ICR &= ~I2C_ICR_ITEIE;
    
    I2C_ISAR = (char)byDeviceAddress;

    while(I2C_ISR & I2C_ISR_IBB)    // wait bus busy
            ;

    LEAVE(TRACE_ON,"Cotulla_I2C_Init");
}

void Cotulla_Record_Codec_Init(void)
{
    UDA1380REGS CodecReg;
    ENTER(TRACE_ON,"Cotulla_Record_Codec_Init");
    CKEN |= 0x4100; 
    Cotulla_CODEC_REGS_Init(&CodecReg);
    CodecReg.iis.iis_u.iis_s.in_format = UC_IN_FORMAT_IIS;
    CodecReg.iis.iis_u.iis_s.out_format = UC_OUT_FORMAT_IIS;
    Write_I2C_WORD(W1380_CODEC_ADDRESS,
		   CodecReg.iis.reg_num, CodecReg.iis.iis_u.reg_val);
    
    CodecReg.mode.mode_u.mode_s.adc_clk = 1;//0;//1;
    CodecReg.mode.mode_u.mode_s.en_adc = 1;//0;//1;
    CodecReg.mode.mode_u.mode_s.sys_div = UC_SYS_DIV_256FS; // clock div value
    CodecReg.mode.mode_u.mode_s.en_dac = 1;                 // enable
    CodecReg.mode.mode_u.mode_s.dac_clk = 1;//0;//1;
    CodecReg.mode.mode_u.mode_s.pll = UC_PLL_25K_50K;           // default setting is 25K to 50K
    CodecReg.mode.mode_u.mode_s.en_int = 1;//0;//1;
    CodecReg.mode.mode_u.mode_s.en_dec = 1;
    CodecReg.mode.mode_u.reg_val = 0x0C02;
    Write_I2C_WORD(W1380_CODEC_ADDRESS,
		   CodecReg.mode.reg_num, CodecReg.mode.mode_u.reg_val);

    CodecReg.power.power_u.power_s.adc_r_on = 1;//0;//1;    
    CodecReg.power.power_u.power_s.adc_l_on = 1;
    CodecReg.power.power_u.power_s.lna_on = 1;
    CodecReg.power.power_u.power_s.bias_on = 1;
    CodecReg.power.power_u.power_s.dac_on = 1;
    CodecReg.power.power_u.power_s.hp_on = 1;//0;//1;
    CodecReg.power.power_u.power_s.pll_on = 1;//0;//1;
    CodecReg.power.power_u.reg_val = 0x0114;
    Write_I2C_WORD(W1380_CODEC_ADDRESS,CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);

    wait_ms(500);

    CodecReg.decimator_vol.decimator_vol_u.reg_val = 0x00; // 0x20h
    Write_I2C_WORD(W1380_CODEC_ADDRESS,
		   CodecReg.decimator_vol.reg_num,
		   CodecReg.decimator_vol.decimator_vol_u.reg_val);

    CodecReg.adc.adc_u.adc_s.sel_lna = 1;                   // select line in
    CodecReg.adc.adc_u.adc_s.sel_mic = 1;                   // select to microphone
    CodecReg.adc.adc_u.reg_val = 0x050C;//0x050C;	// 0x22h
    Write_I2C_WORD(W1380_CODEC_ADDRESS,
		   CodecReg.adc.reg_num, CodecReg.adc.adc_u.reg_val);

    CodecReg.agc.agc_u.agc_s.enable = 0;//1;//0;//1;        // 0x0x23h            // enable automatic gain control
    Write_I2C_WORD(W1380_CODEC_ADDRESS,
		   CodecReg.agc.reg_num, CodecReg.agc.agc_u.reg_val);

    CodecReg.pga.pga_u.reg_val = 0x00;
    Write_I2C_WORD(W1380_CODEC_ADDRESS,
		   CodecReg.pga.reg_num,CodecReg.pga.pga_u.reg_val);

    LEAVE(TRACE_ON,"Cotulla_Record_Codec_Init");
}


void Cotulla_Play_Codec_Init(void)
{
    UDA1380REGS CodecReg;

    ENTER(TRACE_ON,"Cotulla_Play_Codec_Init");
    CKEN |= 0x4100; 
    Cotulla_CODEC_REGS_Init(&CodecReg);
    CodecReg.iis.iis_u.iis_s.in_format = UC_IN_FORMAT_IIS;
    CodecReg.iis.iis_u.iis_s.out_format = UC_OUT_FORMAT_IIS;
    Write_I2C_WORD(W1380_CODEC_ADDRESS,CodecReg.iis.reg_num, CodecReg.iis.iis_u.reg_val);

    CodecReg.mode.mode_u.mode_s.sys_div = UC_SYS_DIV_256FS; // clock div value
    CodecReg.mode.mode_u.mode_s.en_dac = 1;                 // enable
    CodecReg.mode.mode_u.mode_s.dac_clk = 1;
    CodecReg.mode.mode_u.mode_s.pll = UC_PLL_25K_50K;           // default setting is 25K to 50K
    CodecReg.mode.mode_u.mode_s.en_int = 1;
    CodecReg.mode.mode_u.mode_s.en_dec = 1;
    CodecReg.mode.mode_u.reg_val = 0x0302;
    Write_I2C_WORD(W1380_CODEC_ADDRESS,
		   CodecReg.mode.reg_num, CodecReg.mode.mode_u.reg_val);

    if (! (ASIC3_GPIO_PSTS_D & HEADPHONE_IN) ){
      Cotulla_SPEAKER_on();
      CodecReg.power.power_u.reg_val = 0x0500;
      Write_I2C_WORD(W1380_CODEC_ADDRESS,
		     CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);
    }
    else{
      Cotulla_SPEAKER_off();
      Cotulla_HP_on();
    }

    wait_ms(100);

    CodecReg.mute.mute_u.mute_s.all = 0;                        // no mute
    CodecReg.mute.mute_u.mute_s.channel1 = 0;                   // channel 1 no mute
    CodecReg.mute.mute_u.reg_val = 0x0800;
    Write_I2C_WORD(W1380_CODEC_ADDRESS,
		   CodecReg.mute.reg_num, CodecReg.mute.mute_u.reg_val);
 
    LEAVE(TRACE_ON,"Cotulla_Play_Codec_Init");
}

/*
  Cotulla_FullDuplex_Codec_Init() created by Alamy
  Set Codec to support Recording/Playback at the same time.
  Reference to UDA1830TT/UDA1380HN/N2 SSA-Audio Codec Data Sheet.

  *** Make sure I2S System Clock is working ***
*/
void Cotulla_FullDuplex_Codec_Init(void)
{
UDA1380REGS	CodecReg;

	ENTER(TRACE_ON, "Enter : Cotulla_FullDuplex_Codec_Init()");

	CKEN |= 0x4100;		// Just copied from Cotulla_Play_Codec_Init()
	Cotulla_CODEC_REGS_Init( &CodecReg );	// Refresh all value to Zero

	/*
	  REG 01H : IIS I/O settings

	  Slave mode (master_slave = 0, useless to set)
	  Input format : I2S (default)
	  Output format : I2S (default)
	*/
	CodecReg.iis.iis_u.iis_s.in_format = UC_IN_FORMAT_IIS;
	CodecReg.iis.iis_u.iis_s.out_format = UC_OUT_FORMAT_IIS;
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
		CodecReg.iis.reg_num, CodecReg.iis.iis_u.reg_val);

	/*
	  REG 00H : WS PLL settings; clock divider; clock selectors

	  WSPLL input freq. range : 25-50 KHz
	  Dividers for system clock input : 256fs
	  DAC Clock : Enable
	  ADC Clock : Enable
	  1x command : Enable (write to 1x registers)
	  2x command : Enable (write to 2x registers)
	*/
	CodecReg.mode.mode_u.mode_s.pll = UC_PLL_25K_50K;
	CodecReg.mode.mode_u.mode_s.sys_div = UC_SYS_DIV_256FS;
	CodecReg.mode.mode_u.mode_s.en_int = 1;		// Enable 1x cmds
	CodecReg.mode.mode_u.mode_s.en_dac = 1;		// Enable DAC Clock
	CodecReg.mode.mode_u.mode_s.en_dec = 1;		// Enable 2x cmds
	CodecReg.mode.mode_u.mode_s.en_adc = 1;		// Enable ADC Clock
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
		CodecReg.mode.reg_num, CodecReg.mode.mode_u.reg_val);

	/*
	  REG 02H : PON settings

	  Power On for each component
	  Left ADC, LNA, BIAS(RefPWR), DAC, HeadPhone
	*/
	CodecReg.power.power_u.power_s.adc_l_on = 1;
	CodecReg.power.power_u.power_s.lna_on = 1;
	CodecReg.power.power_u.power_s.bias_on = 1;
	CodecReg.power.power_u.power_s.dac_on = 1;
	CodecReg.power.power_u.power_s.hp_on = 1;
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
		CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);

	/*
	  REG 13H : Master Mute and ( (Ch1 & Ch2) de-emphasis & mute )

	  Master Mute : Disable (all = 0, useless to set)
	  Channel 1 Mute : Disable (channel1 = 0, useless to set)
	  Channel 1 De-Emphasis : OFF
	  Channel 2 Mute : Enable
	  Channel 2 De-Emphasis : OFF
	*/
	CodecReg.mute.mute_u.mute_s.de_ch1 = UC_DEEMPHASIS_OFF;
	CodecReg.mute.mute_u.mute_s.de_ch2 = UC_DEEMPHASIS_OFF;
	CodecReg.mute.mute_u.mute_s.channel2 = 1;
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
		CodecReg.mute.reg_num, CodecReg.mute.mute_u.reg_val);

	/*
	  REG 20H : ADC Volume Control Settings

	  Codec default
	  Right Channel : 0 DB (right = 0, useless to set)
	  Left Channel : 0 DB (left = 0, useless to set)
	*/

	/*
	  REG 21H : PGA settings and mute

	  Linear mute for decimator : No muting (0, useless to set)
	  PGA Right channel Gain : 0 DB (gain_r = 0, useless to set)
	  PGA Left channel Gain : 0 DB (gain_l = 0, useless to set)
	*/
	// Have to set Codec, No muting is not Codec default value
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
		CodecReg.pga.reg_num, CodecReg.pga.pga_u.reg_val);

	/*
	  REG 22H : ADC settings

	  Polarity control : non-inverting (adcpol_inv = 0, useless to set)
	  LNA Gain (VGA_CTRL) : +18 DB
	  Input Select (SEL_LNA) : LNA for left ADC
	  Channel Select (SEL_MIC) : Left channel ADC
	  DC filter before dec. filter : Enable (hp_skip_gain = 0)
	    (MUST be 0 if AGC is enabled - REG 23H)
	  DC filter at output of dec. filter : Disable (hp_en_dec = 0)
	*/
	CodecReg.adc.adc_u.adc_s.vga_ctrl = 0x05;
	CodecReg.adc.adc_u.adc_s.sel_lna = 1;
	CodecReg.adc.adc_u.adc_s.sel_mic = 1;
	Write_I2C_WORD(W1380_CODEC_ADDRESS,
		CodecReg.adc.reg_num, CodecReg.adc.adc_u.reg_val);

	/*
	  REG 23H : AGC settings

	  Codec default (AGC disable)
	*/

	LEAVE(TRACE_ON, "Leave : Cotulla_FullDuplex_Codec_Init()");
}

static inline void Cotulla_I2S_Init(void)
{
    
    ENTER(TRACE_ON,"Cotulla_I2S_Init");
    
    GPDR(0) &= ~0xF0000000;
    GPDR(1) &= ~0x00000001;
    
    GPDR(0) |= 0xD0000000;
    GPDR(1) |= 0x00000001;
    GPAFR0_U &= ~0xff000000;
    GPAFR0_U |= 0x59000000;
    GPAFR1_L &= ~0x01;
    GPAFR1_L |= 0x01;
    
    I2S_SACR0 |= 0x0008; // reset
    I2S_SACR0 &= ~0x0001;
    
    I2S_SACR0 &= ~0x0008; // no reset
    
    I2S_SACR0 |= 0x0004;  // bckd on

    I2S_SACR0 &= ~0x0010;//efwr off
    I2S_SACR0 &= ~0x0020;//strf off
    
    I2S_SADR = 0x0000;
    I2S_SADIV = 0x00000000D;
    
    I2S_SACR0 |= 0x7700;
    I2S_SACR0 |= 0x0001;    // i2s clk enable
    
    I2S_SACR1 &= ~0x0001; //amsl off
    I2S_SACR1 &= ~0x0008; //drec off
    I2S_SACR1 &= ~0x0008; //drec off
    I2S_SACR1 &= ~0x0010; //drpl off
    I2S_SACR1 &= ~0x0020; //enlbd off
        
    LEAVE(TRACE_ON,"Cotulla_I2S_Init");
}

static inline void Cotulla_CODEC_reset(void)
{
	ENTER(TRACE_ON,"Cotulla_CODEC_reset");

   	ASIC3_GPIO_DIR_B |= CODEC_RST;	// set pin output
	ASIC3_GPIO_PIOD_B &= ~CODEC_RST;
	ASIC3_GPIO_PIOD_B |= CODEC_RST;
	udelay(1000);
	ASIC3_GPIO_PIOD_B &= ~CODEC_RST;
#ifdef ASIC1    	
	clr_discovery_egpio2(EGPIO2_DISCOVERY_CODEC_RST);
	udelay(10000);
	set_discovery_egpio2(EGPIO2_DISCOVERY_CODEC_RST);
	clr_discovery_egpio2(EGPIO2_DISCOVERY_CODEC_RST);
	udelay(10000);
#endif    
    
	LEAVE(TRACE_ON,"Cotulla_CODEC_reset");
}   

/*
  Cotulla_audio_power_on() created by Alamy (suggested by Eric)
  Take replace of
    Cotulla_audio_play_power_on()
    Cotulla_audio_record_power_on()
    Cotulla_audio_fullduplex_power_on()
*/
void Cotulla_audio_power_on( access_type_t type )
{
	ENTER(TRACE_ON, "Enter : Cotulla_audio_power_on()");

	switch (type)
	{
	case RECORDING:
		Cotulla_I2C_Init(W1380_CODEC_ADDRESS);
		Cotulla_I2S_Init();
		Cotulla_Record_Codec_Init();
		Cotulla_MIC_on();
		udelay(1000);
		sound.hard = sound.soft;
		break;

	case PLAYBACK:
		CKEN |= 0x4100;

		Cotulla_I2C_Init(W1380_CODEC_ADDRESS);
		Cotulla_I2S_Init();
		Cotulla_MIC_off();
		Cotulla_Play_Codec_Init();	// 2002.5.18.13:17
		udelay(1000);
		sound.hard = sound.soft;
		break;

	case FULLDUPLEX:

		Cotulla_I2C_Init( W1380_CODEC_ADDRESS );
		Cotulla_I2S_Init();

		// HP_MIC is out side circuit of Audio Codec
		Cotulla_MIC_on();

		Cotulla_FullDuplex_Codec_Init();
		udelay(1000);

		// What does this for ?  Just copy from other xxx_power_on()
		sound.hard = sound.soft;
		break;
	}

	LEAVE(TRACE_ON, "Leave : Cotulla_audio_power_on()");
}


static void CotullaInit(void)
{
	ENTER(TRACE_ON,"CotullaInit");
	sound.hard = sound.soft;
	LEAVE(TRACE_ON,"CotullaInit");
}

static void Cotulla_sq_interrupt(void* id, int size)
{
    audio_buf_t *b = (audio_buf_t *) id;
    ENTER(TRACE_INTERRUPT,"Cotulla_sq_interrupt");

	if( -- b->dma_count > 0 ) return;

    /*
     * Current buffer is sent: wake up any process waiting for it.
     */
    ENTER(TRACE_SEM,"up sem");
    up(&b->sem);
    LEAVE(TRACE_SEM,"up sem");

    /* And any process polling on write. */
    ENTER(TRACE_SEM,"up wait");
    wake_up(&b->sem.wait);
    LEAVE(TRACE_SEM,"up wait");

    DPRINTK("Cotulla_sq_interrupt \n");
    LEAVE(TRACE_INTERRUPT,"Cotulla_sq_interrupt");
}

void CotullaDmaInit(void)
{
	int err;
	
    err = cotulla_request_dma(&cotulla_dmasound_irq,"dmasound");
    if (err) {
        LEAVE(TRACE_ON,"CotullaIrqInit_request");
        return;
    }
    
    err = cotulla_request_dma(&cotulla_dmarecording_irq,"dmarecording");
    if (err) {
        LEAVE(TRACE_ON,"CotullaIrqInit_request2");
        return;
    }
    
    
    err = cotulla_dma_set_device(cotulla_dmasound_irq,3,_I2S_SADR,32,4,0,0);
                 // SADR = 0x40400080, DCMD. width = 4, burst size = 32, recv = 0
    if (err) {
        LEAVE(TRACE_ON,"CotullaIrqInit_setdevice");
        return;
    }

    err = cotulla_dma_set_device(cotulla_dmarecording_irq,2,_I2S_SADR,32,4,0,1);
                 // SADR = 0x40400080, DCMD. width = 4, burst size = 32, recv = 1
    if (err) {
        LEAVE(TRACE_ON,"CotullaIrqInit_setdevice2");
        return;
    }

    err = cotulla_dma_set_callback(cotulla_dmasound_irq,
                   (dma_callback_t)Cotulla_sq_interrupt);

    if (err) {
        LEAVE(TRACE_ON,"CotullaIrqInit_callback");
        return;
    }
    
    err = cotulla_dma_set_callback(cotulla_dmarecording_irq,
                   (dma_callback_t)Cotulla_sq_interrupt);

    if (err) {
        LEAVE(TRACE_ON,"CotullaIrqInit_callback2");
        return;
    }

}

static int __init CotullaIrqInit(void)
{

    ENTER(TRACE_ON,"CotullaIrqInit");
    
	CotullaDmaInit();
	
    /* Disable sound & DMA */
    Cotulla_disable_sound();

    LEAVE(TRACE_ON,"CotullaIrqInit");
    return(1);

}

static int CotullaSetFormat(int format)
{
    int size;

    ENTER(TRACE_ON,"CotullaSetFormat");
    /* Falcon sound DMA supports 8bit and 16bit modes */
    audio_fmt = format;
    switch (format) {
    case AFMT_QUERY:
        LEAVE(TRACE_ON,"CotullaSetFormat");
        return(sound.soft.format);
    case AFMT_MU_LAW:
        size = 8;
        ct_func = sound.trans->ct_ulaw;
        ct_read_func = sound.trans_read->ct_ulaw;
        break;
    case AFMT_A_LAW:
        size = 8;
        ct_func = sound.trans->ct_alaw;
        ct_read_func = sound.trans_read->ct_alaw;
        break;
    case AFMT_S8:
        size = 8;
        ct_func = sound.trans->ct_s8;
        ct_read_func = sound.trans_read->ct_s8;
        break;
    case AFMT_U8:
        size = 8;
        ct_func = sound.trans->ct_u8;
        ct_read_func = sound.trans_read->ct_u8;
        break;
    case AFMT_S16_BE:
        size = 16;
        ct_func = sound.trans->ct_s16be;
        ct_read_func = sound.trans_read->ct_s16be;
        break;
    case AFMT_U16_BE:
        size = 16;
        ct_func = sound.trans->ct_u16be;
        ct_read_func = sound.trans_read->ct_u16be;
        break;
    case AFMT_S16_LE:
        size = 16;
        ct_func = sound.trans->ct_s16le;
        ct_read_func = sound.trans_read->ct_s16le;
        break;
    case AFMT_U16_LE:
        size = 16;
        ct_func = sound.trans->ct_u16le;
        ct_read_func = sound.trans_read->ct_u16le;
        break;
    default: /* :-) */
        size = 8;
        format = AFMT_S8;
        ct_func = sound.trans->ct_s8;
        ct_read_func = sound.trans_read->ct_s8;
    }

    sound.soft.format = format;
    sound.soft.size = size;
    if (sound.minDev == SND_DEV_DSP) {
        sound.dsp.format = format;
        sound.dsp.size = sound.soft.size;
    }

    LEAVE(TRACE_ON,"CotullaSetFormat");
    return(format);
}

/*** Machine definitions *****************************************************/

static MACHINE machCotulla = {
    DMASND_COTULLA,     // int type
    NULL,           // void *dma_alloc(uint, int)
    NULL,           // void  dma_free(void *, uint)
    CotullaIrqInit,     // void  irqinit(void)
#ifdef MODULE
    CotullaIrqCleanUp,  // void  irqcleanup(void)
#endif /* MODULE */
    CotullaInit,        // void  init(void)
    CotullaSilence,     // void  silence(void)
    CotullaSetFormat,   // int   setFormat(int)
    NULL,           // int   setVolume(int)
    NULL,           // int   setBass(int)
    NULL,           // int   setTreble(int)
    NULL,           // int   setGain(int)
    NULL            // void  play(void)
};


/*** Mid level stuff *********************************************************/


static void sound_silence(void)
{
    ENTER(TRACE_ON,"sound_silence");
    /* update hardware settings one more */
    //(*sound.mach.init)();
    (*sound.mach.silence)();
    LEAVE(TRACE_ON,"sound_silence");

}


static void sound_init(void)
{
    ENTER(TRACE_ON,"sound_init");
    (*sound.mach.init)();
    LEAVE(TRACE_ON,"sound_init");
}


static int sound_set_format(int format)
{
    ENTER(TRACE_ON,"sound_set_format");
    LEAVE(TRACE_ON,"sound_set_format");
    return(*sound.mach.setFormat)(format);
}


static int sound_set_speed(int speed)
{
    ENTER(TRACE_ON,"sound_set_speed");
    if (speed < 0) {
        LEAVE(TRACE_ON,"sound_set_speed");
        return(sound.soft.speed);
    }

    sound.soft.speed = speed;
    (*sound.mach.init)();
    if (sound.minDev == SND_DEV_DSP)
        sound.dsp.speed = sound.soft.speed;

    /* LoCoMo Audio clock */
    //Collie_audio_clock_off();
    Cotulla_set_audio_clock();
    
    LEAVE(TRACE_ON,"sound_set_speed");
    return(sound.soft.speed);
}

/*
	stereo : 0) mono, 1) stereo

	for SNDCTL_DSP_STEREO, should return mono/stereo (0 or 1),
	or Media Player won't work.
*/
static int sound_set_stereo(int stereo)
{
	ENTER(TRACE_ON, "Enter : sound_set_stereo()");

	/* stereo should be 0 or 1 */
	if ((stereo < 0) || (stereo > 1)) {
		LEAVE(TRACE_ON, "Leave : sound_set_stereo() - Out of range");
		return sound.soft.stereo;
	}

	sound.soft.stereo = stereo;
	if (sound.minDev == SND_DEV_DSP)
		sound.dsp.stereo = stereo;

	LEAVE(TRACE_ON, "Leave : sound_set_stereo()");
	return stereo;
}


/*
 * /dev/mixer abstraction
 */

static int mixer_open(struct inode *inode, struct file *file)
{
    ENTER(TRACE_ON,"mixer_open");
    MOD_INC_USE_COUNT;
    mixer.busy = 1;
    LEAVE(TRACE_ON,"mixer_open");
    return 0;
}


static int mixer_release(struct inode *inode, struct file *file)
{
    ENTER(TRACE_ON,"mixer_release");
    mixer.busy = 0;
    MOD_DEC_USE_COUNT;
    LEAVE(TRACE_ON,"mixer_release");
    return 0;
}


static int mixer_ioctl(struct inode *inode, struct file *file, u_int cmd,
               u_long arg)
{
    int data;

    ENTER(TRACE_ON, "Enter : mixer_ioctl()");
    
    switch (sound.mach.type) {
    case DMASND_COTULLA:
    {
        switch (cmd) {
            case SOUND_MIXER_READ_DEVMASK:
                LEAVE(TRACE_ON, "Leave : mixer_ioctl(SOUND_MIXER_READ_DEVMASK)");
                return IOCTL_OUT(arg, SOUND_MASK_VOLUME | SOUND_MASK_MIC );
            case SOUND_MIXER_READ_RECMASK:
                LEAVE(TRACE_ON, "Leave : mixer_ioctl(SOUND_MIXER_READ_RECMASK)");
                return IOCTL_OUT(arg, 0);
            case SOUND_MIXER_READ_STEREODEVS:
                LEAVE(TRACE_ON,"mixer_ioctl");
                return IOCTL_OUT(arg, SOUND_MASK_VOLUME);
            case SOUND_MIXER_READ_CAPS:
                LEAVE(TRACE_ON,"mixer_ioctl");
                return IOCTL_OUT(arg, 0);

            case SOUND_MIXER_WRITE_VOLUME:
                ENTER(TRACE_VOLUME, "Enter : mixer_ioctl(SOUND_MIXER_WRITE_VOLUME)");
                IOCTL_IN(arg, data);
                DPRINTK("<volume data>:%x\n", data);
                Cotulla_Set_Volume(data);
                LEAVE(TRACE_VOLUME, "Leave : mixer_ioctl(SOUND_MIXER_WRITE_VOLUME)");

            case SOUND_MIXER_READ_VOLUME:
                LEAVE(TRACE_VOLUME, "Leave : mixer_ioctl(SOUND_MIXER_READ_VOLUME)");
                return IOCTL_OUT(arg, Cotulla_Get_Volume());

            case SOUND_MIXER_READ_TREBLE:
                LEAVE(TRACE_ON,"mixer_ioctl");
                return IOCTL_OUT(arg, 0);
            case SOUND_MIXER_WRITE_TREBLE:
                LEAVE(TRACE_ON,"mixer_ioctl");
                return IOCTL_OUT(arg, 0);

            case SOUND_MIXER_WRITE_MIC:
			ENTER(TRACE_VOLUME, "Enter : mixer_ioctl(SOUND_MIXER_WRITE_MIC)");
                IOCTL_IN(arg, data);
                DPRINTK("<mic data>:%x\n",data);
			Cotulla_Set_Igain(data);
			ENTER(TRACE_VOLUME, "Leave : mixer_ioctl(SOUND_MIXER_WRITE_MIC)");
            case SOUND_MIXER_READ_MIC:
                LEAVE(TRACE_ON,"mixer_ioctl");
                return IOCTL_OUT(arg, Cotulla_Get_Igain());

#if 0                
	    case SOUND_MIXER_WRITE_IGAIN:
			IOCTL_IN(arg, data);
			Cotulla_Set_Igain(data);
	    case SOUND_MIXER_READ_IGAIN:
                LEAVE(TRACE_ON,"mixer_ioctl");
                return IOCTL_OUT(arg, Cotulla_Get_Igain());
#endif
            case SOUND_MIXER_READ_SPEAKER:
                LEAVE(TRACE_ON,"mixer_ioctl");
                return IOCTL_OUT(arg, 0);
            case SOUND_MIXER_WRITE_SPEAKER:
                LEAVE(TRACE_ON,"mixer_ioctl");
                return IOCTL_OUT(arg, 0);
        }
            break;
    }
    }
    LEAVE(TRACE_ON,"mixer_ioctl");
    return -EINVAL;

}


static struct file_operations mixer_fops =
{
    llseek:  sound_lseek,
    ioctl:   mixer_ioctl,
    open:    mixer_open,
    release: mixer_release,
};


static void __init mixer_init(void)
{
#ifndef MODULE
    int mixer_unit;
#endif
    ENTER(TRACE_ON,"mixer_init");
    mixer_unit = register_sound_mixer(&mixer_fops, -1);
    if (mixer_unit < 0) {
        LEAVE(TRACE_ON,"mixer_init");
        return;
    }

    mixer.busy = 0;
    sound.treble = 0;
    sound.bass = 0;
    switch (sound.mach.type) {
        case DMASND_COTULLA:
            sound.volume_left  = 80;
            sound.volume_right = 80;
            break;
    }

    LEAVE(TRACE_ON,"mixer_init");
}

/*
 * sq_allocate_buffers()
 * This function allocates the buffer structure array and buffer data space
 * according to the current number of fragments and fragment size.
 */

static int sq_allocate_buffers(audio_stream_t * s)
{
    int frag;
    int dmasize = 0;
    char *dmabuf = 0;
    dma_addr_t dmaphys = 0;

    ENTER(TRACE_ON, "Enter : sq_allocate_buffers()");
    DPRINTK("sq_allocate_buffers\n");
    if (s->buffers) {
        LEAVE(TRACE_ON, "Leave : sq_allocate_buffers() - EBUSY");
        return -EBUSY;
    }

    s->nbfrags = audio_nbfrags;
    s->fragsize = audio_fragsize;

    s->buffers = (audio_buf_t *)
        kmalloc(sizeof(audio_buf_t) * s->nbfrags, GFP_KERNEL/*GFP_ATOMIC*/);
    if (!s->buffers)
    goto err;
    memset(s->buffers, 0, sizeof(audio_buf_t) * s->nbfrags);

    for (frag = 0; frag < s->nbfrags; frag++) {
        audio_buf_t *b = &s->buffers[frag];

        /*
         * Let's allocate non-cached memory for DMA buffers.
         * We try to allocate all memory at once.
         * If this fails (a common reason is memory fragmentation),
         * then we allocate more smaller buffers.
         */
        if (!dmasize) {
            dmasize = (s->nbfrags - frag) * s->fragsize;
            do {
                dmabuf = consistent_alloc(GFP_KERNEL|GFP_DMA,
                              dmasize,
                              &dmaphys);
                if (!dmabuf)
                    dmasize -= s->fragsize;
            } while (!dmabuf && dmasize);
            if (!dmabuf)
                goto err;
			b->dma_size = dmasize;
        }

        b->start = dmabuf;
        b->dma_addr = dmaphys;
        sema_init(&b->sem, 1);
        DPRINTK("buf %d: start %p dma %p\n", frag, b->start,
            b->dma_addr);

        dmabuf += s->fragsize;
        dmaphys += s->fragsize;
        dmasize -= s->fragsize;
    }

    s->buf_idx = 0;
    s->buf = &s->buffers[0];

    LEAVE(TRACE_ON, "Leave : sq_allocate_buffers()");
    return 0;

err:
    printk("sound driver : unable to allocate audio memory\n ");
    sq_release_buffers(s);   
    LEAVE(TRACE_ON, "Leave : sq_allocate_buffers() - Error!");
    return -ENOMEM;
}

/*
 * This function frees all buffers
 */

static void sq_release_buffers(audio_stream_t * s)
{
    ENTER(TRACE_ON,"sq_release_buffers");
    DPRINTK("sq_release_buffers\n");
	
    /* ensure DMA won't run anymore */
	if( s == &input_stream ){
		if (audio_rd_refcount)
		  cotulla_dma_flush_all(COTULLA_RECORDING_DMA_CHANNEL);
	}else{
		if (audio_wr_refcount)
		  cotulla_dma_flush_all(COTULLA_SOUND_DMA_CHANNEL);
	}

    if (s->buffers) {
        int frag;
        for (frag = 0; frag < s->nbfrags; frag++) {
		if (!s->buffers[frag].dma_size)
			continue;
            consistent_free(s->buffers[frag].start,
                    s->buffers[frag].dma_size,
                    s->buffers[frag].dma_addr);
        }
        kfree(s->buffers);
        s->buffers = NULL;
    }

    s->buf_idx = 0;
    s->buf = NULL;

    LEAVE(TRACE_ON,"sq_release_buffers");
}

static int audio_recording(audio_stream_t * s)
{
	int i, is_start, idx;
	audio_buf_t *b;

	if (!s->buffers) {
		if (sq_allocate_buffers(s))
		{
			LEAVE(TRACE_READ,"sq_read2");
			return -ENOMEM;
		}
		is_start = 1;
	}else{
		is_start = 0;
	}

	if(( !is_start )&&( !cotulla_dma_is_busy(COTULLA_RECORDING_DMA_CHANNEL) )){
		is_start = 1;
		idx = s->buf_idx;
		for (i = 0; i < s->nbfrags; i++) {
			if (atomic_read(&(s->buffers[idx].sem.count)) > 0){
				is_start = 0;
				break;
			}
			idx = (idx+1)%(s->nbfrags);
		}
	}
	if( is_start ){
		for (i = 0; i < s->nbfrags; i++) {
			b = s->buf;
			down(&b->sem);
			set_dma_divided_buffer(s, b,
								   COTULLA_RECORDING_DMA_CHANNEL, s->fragsize);
			NEXT_BUF(s, buf);
		}
		
	}
	return 0;
}


static ssize_t sq_read(struct file *file, const char *src, size_t uLeft,
            loff_t *ppos)
{
	char *buffer0 = src;
	audio_stream_t *s = &input_stream;
	u_char *dest;
	ssize_t uUsed, bUsed, bLeft, ret = 0;

	ENTER(TRACE_READ, "Enter : sq_read()");

	Cotulla_OP_RDV_on();
	switch (file->f_flags & O_ACCMODE) {
	case O_RDONLY:
	case O_RDWR:
		break;
	case O_WRONLY:
		return -EFAULT;
	default:
		return -EPERM;
	}

	ret = audio_recording(s);
	if( ret != 0 ){
		return ret;
	}

	cotulla_do_reading = 1;
	if( cotulla_do_sync ){
		cotulla_do_reading = 0;
		return 0;
	}
	while (uLeft > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become full */
		if (file->f_flags & O_NONBLOCK) {
			DPRINTK("audio_read : down_trylock\n");
			ret = -EAGAIN;
			ENTER(TRACE_SEM, "Enter : down_trylock sem");
			if (down_trylock(&b->sem)) {
				LEAVE(TRACE_SEM, "Leave : down_trylock sem - Error");
				break;
			}
			LEAVE(TRACE_SEM, "Leave : down_trylock sem");
		} else {
			DPRINTK("audio_read : down_interruptible\n");
			ret = -ERESTARTSYS;
			ENTER(TRACE_SEM, "Enter : down_interruptible sem");
			if (down_interruptible(&b->sem)) {
				LEAVE(TRACE_SEM, "Leave : down_interruptible sem - Error");
				break;
			}
			LEAVE(TRACE_SEM, "Leave : down_interruptible sem");
		}

		dest = b->start + b->size;
		bUsed = 0;
		bLeft = s->fragsize - b->size;

		if (ct_read_func) {
		  uUsed = ct_read_func(src, uLeft, dest, &bUsed, bLeft);
		} else {
			cotulla_do_reading = 0;
			return -EFAULT;
		}

		if (uUsed < 0) {
			up(&b->sem);
			cotulla_do_reading = 0;
			return -EFAULT;
		}
		b->size += bUsed;
		src += uUsed;
		uLeft -= uUsed;

		if (b->size < s->fragsize) {
			up(&b->sem);
			break;
		}
		b->size = 0;

		set_dma_divided_buffer(s, b,
							   COTULLA_RECORDING_DMA_CHANNEL, s->fragsize);
		NEXT_BUF(s, buf);
	}
	cotulla_do_reading = 0;

	if ((src - buffer0))
		ret = src - buffer0;
	DPRINTK("audio_read: return=%d\n", ret);
	LEAVE(TRACE_READ, "Leave : sq_read()");

	return ret;
}

static ssize_t sq_write(struct file *file, const char *src, size_t uLeft,
            loff_t *ppos)
{
    const char *buffer0 = src;
    audio_stream_t *s = &output_stream;
    u_char *dest;
    ssize_t uUsed, bUsed, bLeft, ret = 0;

    ENTER(TRACE_WRITE,"Enter : sq_write()");
    DPRINTK("sq_write: uLeft=%d\n", uLeft);

    Cotulla_OP_PLV_on();
	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		break;
	case O_RDONLY:
		return -EFAULT;
	default: 
		LEAVE(TRACE_WRITE,"sq_write1");
		return -EPERM;
	} 

	if (!s->buffers && sq_allocate_buffers(s)) {
		LEAVE(TRACE_WRITE,"sq_write2");
		return -ENOMEM;
	}

	if (cotulla_resume == 1) {
		int	i;
		cotulla_resume = 0;
		for (i=0; i<1000*1000; i++) {
			udelay(1);
		}
	}

#ifdef CONFIG_PM
	/* Auto Power off cancel */
	autoPowerCancel = 0;
#endif

	cotulla_do_writing = 1;
	if( cotulla_do_sync ){
		return 0;
	}
	while (uLeft > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become free */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			ENTER(TRACE_SEM,"down_try sem");
			if (down_trylock(&b->sem)) {
				LEAVE(TRACE_SEM,"down_try1 sem");
				break;
			}
			LEAVE(TRACE_SEM,"down_try2 sem");
		} else {
			ret = -ERESTARTSYS;
			ENTER(TRACE_SEM,"down_int sem");
			if (down_interruptible(&b->sem)) {
				LEAVE(TRACE_SEM,"down_int1 sem");
				break;
			}
			LEAVE(TRACE_SEM,"down_int2 sem");
		}

		/* Feed the current buffer */
		dest = b->start + b->size;
		bUsed = 0;
		bLeft = s->fragsize - b->size;

		if (ct_func) {
		  uUsed = ct_func(src, uLeft, dest, &bUsed, bLeft);
		} else {
			up(&b->sem);
			cotulla_do_writing = 0;
			return -EFAULT;
		}

		if (uUsed < 0) {
			up(&b->sem);
			cotulla_do_writing = 0;
			return -EFAULT;
		}

		b->size += bUsed;
		src += uUsed;
		uLeft -= uUsed;

		if (b->size < s->fragsize) {
			up(&b->sem);
			break;
		}
		b->size = 0;
		set_dma_divided_buffer(s, b,
							   COTULLA_SOUND_DMA_CHANNEL, s->fragsize);
		NEXT_BUF(s, buf);
	}
	cotulla_do_writing = 0;

	if ((src - buffer0))
		ret = src - buffer0;

	LEAVE(TRACE_WRITE, "Leave : sq_write()");

	return ret;
}


static unsigned int sq_poll(struct file *file, 
                   struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    int i;
    int ret;

    ENTER(TRACE_ON,"sq_poll");
    DPRINTK("sq_poll(): mode=%s%s\n",
        (file->f_mode & FMODE_READ) ? "r" : "",
        (file->f_mode & FMODE_WRITE) ? "w" : "");

    if (file->f_mode & FMODE_WRITE) {
        if (!output_stream.buffers 
            && sq_allocate_buffers(&output_stream)) {
            LEAVE(TRACE_ON,"sq_poll");
            return -ENOMEM;
        }
        ENTER(TRACE_SEM,"poll_wait wait");
        poll_wait(file, &output_stream.buf->sem.wait, wait);
        LEAVE(TRACE_SEM,"poll_wait wait");
    }
    if (file->f_mode & FMODE_WRITE) {
        for (i = 0; i < output_stream.nbfrags; i++) {
			if (atomic_read(&output_stream.buffers[i].sem.count) > 0){
                mask |= POLLOUT | POLLWRNORM;
				break;
			}
        }
    }

	if (file->f_mode & FMODE_READ) {
		/* Start audio input if not already active */
		ret = audio_recording(&input_stream);
		if (ret < 0)
			return ret;

		poll_wait(file, &input_stream.buf->sem.wait, wait);
	}
	if (file->f_mode & FMODE_READ) {
		for (i = 0; i < input_stream.nbfrags; i++) {
			if (atomic_read(&input_stream.buffers[i].sem.count) > 0){
				mask |= POLLIN | POLLRDNORM;
				break;
			}
		}
	}

    DPRINTK("sq_poll() returned mask of %s%s\n",
        (mask & POLLIN) ? "r" : "",
        (mask & POLLOUT) ? "w" : "");

    LEAVE(TRACE_ON,"sq_poll");
    return mask;
}

static int sq_open(struct inode *inode, struct file *file)
{
    ENTER(TRACE_ON, "Enter : sq_open()");
    DPRINTK("sq_open\n");

	//eRic24! for R/W support
	if (audio_wr_refcount || audio_rd_refcount) {
		DPRINTK(" sq_open EBUSY\n");
		LEAVE(TRACE_ON,"sq_open");
		return -EBUSY;
	}
	if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		audio_wr_refcount++;
	}
	else if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		audio_rd_refcount++;
	}
	else if (((file->f_flags & O_ACCMODE) == O_RDWR)){
		audio_wr_refcount++;
		audio_rd_refcount++;
	}

	MOD_INC_USE_COUNT;

	while(cotulla_under_playing) {
		schedule();
	}

	if ((audio_rd_refcount == 1) && (audio_wr_refcount == 1)) {
	// Playback & Recording simultaneously
		audio_fragsize = AUDIO_FRAGSIZE_DEFAULT;
		audio_nbfrags = AUDIO_NBFRAGS_DEFAULT;
		audio_fmt = AUDIO_FMT_DEFAULT;
		sq_release_buffers( &input_stream );
		sq_release_buffers( &output_stream );
		sound.minDev = MINOR( inode->i_rdev ) & 0x0F;
		sound.soft = sound.dsp;
		sound.hard = sound.dsp;

		if ( (MINOR(inode->i_rdev) & 0x0F) == SND_DEV_AUDIO ) {
			sound_set_speed( AUDIO_RATE_DEFAULT );
			sound_set_stereo( 0 );
			sound_set_format( AUDIO_FMT_DEFAULT );
		}
		Cotulla_audio_power_on( FULLDUPLEX );
		CODEC_SetVolume( sound.volume_right, 0 );
		cotulla_under_recording = 1;
		cotulla_under_playing = 1;

	} else if (audio_rd_refcount == 1) {
	// Recording Only
		audio_fragsize = AUDIO_FRAGSIZE_DEFAULT;
		audio_nbfrags = AUDIO_NBFRAGS_DEFAULT;
		audio_fmt = AUDIO_FMT_DEFAULT; // 16bit
		sq_release_buffers(&input_stream);
		sound.minDev = MINOR(inode->i_rdev) & 0x0f;
		sound.soft = sound.dsp;
		sound.hard = sound.dsp;
        
		if ((MINOR(inode->i_rdev) & 0x0f) == SND_DEV_DSP) {
			sound_set_speed(8000);
			sound_set_stereo(0);
			sound_set_format(AFMT_S16_LE);
		}
		Cotulla_audio_power_on( RECORDING );
		cotulla_under_recording = 1;

	} else if (audio_wr_refcount == 1) {
	// Playback Only
		DPRINTK("cold\n");

		audio_fragsize = AUDIO_FRAGSIZE_DEFAULT;
		audio_nbfrags = AUDIO_NBFRAGS_DEFAULT;
		sq_release_buffers(&output_stream);
		sound.minDev = MINOR(inode->i_rdev) & 0x0f;
		sound.soft = sound.dsp;
		sound.hard = sound.dsp;
        
		if ((MINOR(inode->i_rdev) & 0x0f) == SND_DEV_AUDIO
		/*|| (MINOR(inode->i_rdev) & 0x0f) == SND_DEV_DSP*/ ) {
			sound_set_speed(8000);
			sound_set_stereo(0);
			sound_set_format(AFMT_S16_LE);
		}
		Cotulla_audio_power_on( PLAYBACK );
		CODEC_SetVolume(sound.volume_right, sound.volume_left);
		cotulla_under_playing = 1;
	} else {
	// Will this happened ?
		MOD_DEC_USE_COUNT;
		LEAVE(TRACE_ON, "Leave : sq_open() : EINVAL");
		return -EINVAL;
	}

	cotulla_do_writing = 0;
	cotulla_do_reading = 0;
	cotulla_do_sync = 0;

    LEAVE(TRACE_ON,"Leave : sq_open()");
    return 0;
}

/*
 * sq_fsync()
 */
static int sq_fsync(struct file *filp, struct dentry *dentry)
{
	audio_stream_t	*s, *outS = NULL;	// Output Stream
	audio_buf_t	*b, *outBuf = NULL;		// Output Buffer
	int		outRc = 0;					// Output Return Code
	int		i, tout, frg;

	ENTER(TRACE_ON, "Enter : sq_fsync()");

	cotulla_do_sync = 1;
	if (audio_rd_refcount == 1) {
		while( cotulla_do_reading ){
			schedule();
		}
		// Force-stop recording DMA
		cotulla_dma_flush_all(COTULLA_RECORDING_DMA_CHANNEL);
		s = &input_stream;
		if( s->buffers ){
			for (i = 0; i < s->nbfrags ; i++) {
				b = s->buf;
				sema_init(&b->sem, 1);
				NEXT_BUF(s, buf);
			}
		}
	}

	if (audio_wr_refcount == 1) {
		while( cotulla_do_writing ){
			schedule();
		}
		outS = &output_stream;
		outBuf = outS->buf;
	}

	// Flush half-full buffers
	if ((audio_wr_refcount == 1) && (outBuf != NULL) && (outBuf->size != 0)) {
#ifdef CONFIG_PM
		/* Auto Power off cancel */
		autoPowerCancel = 0;
#endif

		ENTER(TRACE_SEM, "Enter : Playback down sem");
		down(&outBuf->sem);
		LEAVE(TRACE_SEM, "Leave : Playback down sem");
		set_dma_divided_buffer(outS, outBuf,
							   COTULLA_SOUND_DMA_CHANNEL, outBuf->size);
		outBuf->size = 0;
		NEXT_BUF(outS, buf);
	}

	/*
	 * Let's wait for the last buffer we sent i.e. the one before the
	 * current buf_idx.  When we acquire the semaphore, this means either:
	 * - DMA on the buffer completed or
	 * - the buffer was already free thus nothing else to sync.
	*/
	if ((audio_wr_refcount == 1) && (outBuf != NULL)) {
		outBuf = outS->buffers +
		  ((outS->nbfrags + outS->buf_idx - 1) % outS->nbfrags);

		ENTER(TRACE_SEM, "Enter : Playback down-int sem");
#if 1	/* DMA bug(?) : Sometimes IRQ does not occur.
		 * Escape by timeout.
		 */
		printk( "sq_fsync\n" );
		frg = 1;
		for (i = 0; i < outS->nbfrags; i++) {
			if (atomic_read(&outS->buffers[i].sem.count) <= 0) {
				frg++;
			}
		}
		if( sound.soft.speed > 0 ){
			tout = (((frg * audio_fragsize)>>2)*100)/sound.soft.speed + 1;
		}else{
			tout = 0;
		}
		DPRINTK( "wait for %d msec (%d frags)\n", tout*10, frg );
		tout += jiffies;
		while( jiffies < tout ){
			if (!down_trylock(&(outBuf->sem))) {
				DPRINTK( "write sync success\n" );
				break;
			}
			schedule();
		}
#else
		if (down_interruptible(&outBuf->sem)) {
			outRc = -EINTR;
		}
#endif
		LEAVE(TRACE_SEM, "Leave : Playback down-int sem");

		ENTER(TRACE_SEM, "Enter : Playback up sem");
		up(&outBuf->sem);
		LEAVE(TRACE_SEM, "Leave : Playback up sem");
	}
	cotulla_do_sync = 0;

	LEAVE(TRACE_ON, "Leave : sq_fsync()");
	return outRc;
}


/*
  sq_release() rewrite by Alamy
  Reconstruct function
*/
static int sq_release(struct inode *inode, struct file *file)
{
	ENTER(TRACE_ON, "Enter : sq_release()");

	if (audio_rd_refcount == 1) {
		sq_fsync(file, file->f_dentry);
		sq_release_buffers(&input_stream);
		audio_rd_refcount = 0;
		cotulla_under_recording = 0;
		Cotulla_i2sc_record_power_saving();
	}
	if (audio_wr_refcount == 1) {
		sq_fsync(file, file->f_dentry);
		sq_release_buffers(&output_stream);
		audio_wr_refcount = 0;
		cotulla_under_playing = 0;
		Cotulla_i2sc_play_power_saving();
	}
	sound.soft = sound.dsp;
	sound.hard = sound.dsp;
	
	driver_waste &= ~VOLUME_MASK;

	sound_silence();
	MOD_DEC_USE_COUNT;

	LEAVE(TRACE_ON, "Leave : sq_release()");
	return 0;
}


/*
  Reference to ~include/linux/soundcard.h for ioctl request list
*/
static int sq_ioctl(struct inode *inode, struct file *file, u_int cmd,
            u_long arg)
{
    u_long fmt;
    int data;
    long val;

	/* for SNDCTL_DSP_GETISPACE & SNDCTL_DSP_GETOSPACE */
	int		i, ret;
	audio_buf_info	buf_info = { 0, };	
	audio_stream_t	*stream = NULL;

    ENTER(TRACE_ON,"sq_ioctl");
    switch (cmd) {
    case SNDCTL_DSP_RESET:
        switch (file->f_flags & O_ACCMODE) {
        case O_WRONLY:
		sq_release_buffers(&output_stream);
            break;
        case O_RDWR:
		sq_release_buffers(&output_stream);
		sq_release_buffers(&input_stream);
            break;
       case O_RDONLY:
		sq_release_buffers(&input_stream);
		break;
        }
        LEAVE(TRACE_ON,"sq_ioctl");
        return 0;
    case SNDCTL_DSP_POST:
    case SNDCTL_DSP_SYNC:
        LEAVE(TRACE_ON,"sq_ioctl");
        return sq_fsync(file, file->f_dentry);

        /* ++TeSche: before changing any of these it's
         * probably wise to wait until sound playing has
         * settled down. */
    case SNDCTL_DSP_SPEED:
        sq_fsync(file, file->f_dentry);
        IOCTL_IN(arg, data);
        LEAVE(TRACE_ON,"sq_ioctl");
        return IOCTL_OUT(arg, sound_set_speed(data));
    case SNDCTL_DSP_STEREO:
        sq_fsync(file, file->f_dentry);
        IOCTL_IN(arg, data);
        LEAVE(TRACE_ON,"sq_ioctl");
        return IOCTL_OUT(arg, sound_set_stereo(data));
    case SNDCTL_DSP_CHANNELS:
    /* same case value as SOUND_PCM_WRITE_CHANNELS */
		sq_fsync(file, file->f_dentry);
        IOCTL_IN(arg, data);
		data--;		// Convert from channels to stereo parameter
		LEAVE(TRACE_ON, "Leave : sq_ioctl(SNDCTL_DSP_CHANNELS)");
		if ((data < 0) || (data > 1))
		  return -EINVAL;
		return IOCTL_OUT(arg, sound_set_stereo(data)+1);
    case SNDCTL_DSP_SETFMT:
        sq_fsync(file, file->f_dentry);
        IOCTL_IN(arg, data);
        LEAVE(TRACE_ON,"sq_ioctl");
        return IOCTL_OUT(arg, sound_set_format(data));
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
        LEAVE(TRACE_ON,"sq_ioctl");
        return IOCTL_OUT(arg, fmt);
    case SNDCTL_DSP_GETBLKSIZE:
        LEAVE(TRACE_ON,"sq_ioctl");
        return IOCTL_OUT(arg, audio_fragsize);
    case SNDCTL_DSP_SUBDIVIDE:
        break;
    case SNDCTL_DSP_SETFRAGMENT:
        if (output_stream.buffers) {
            LEAVE(TRACE_ON,"sq_ioctl");
            return -EBUSY;
        }
        get_user(val, (long *) arg);
        audio_fragsize = 1 << (val & 0xFFFF);
        if (audio_fragsize < 16)
            audio_fragsize = 16;
        if (audio_fragsize > 16384)
            audio_fragsize = 16384;
        audio_nbfrags = (val >> 16) & 0x7FFF;
        if (audio_nbfrags < 2)
            audio_nbfrags = 2;
        if (audio_nbfrags * audio_fragsize > 128 * 1024)
            audio_nbfrags = 128 * 1024 / audio_fragsize;
        if (sq_allocate_buffers(&output_stream)) {
            LEAVE(TRACE_ON,"sq_ioctl");
            return -ENOMEM;
        }
        LEAVE(TRACE_ON,"sq_ioctl");
        return 0;

	case SNDCTL_DSP_GETISPACE:
		stream = &input_stream;
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		ret = audio_recording(stream);
		if( ret != 0 ){
			return ret;
		}
		Cotulla_OP_RDV_on();
		for (i = 0; i < stream->nbfrags; i++) {
			if (atomic_read(&stream->buffers[i].sem.count) > 0) {
				if (stream->buffers[i].size == 0){
					buf_info.fragments++;
					buf_info.bytes += stream->fragsize;
				}else{
					buf_info.bytes += stream->fragsize-stream->buffers[i].size;
				}
			}
		}
		buf_info.fragstotal = stream->nbfrags;
		buf_info.fragsize = stream->fragsize;
		return (copy_to_user((void *)arg, &buf_info, sizeof(buf_info)) ? -EFAULT : 0);

	case SNDCTL_DSP_GETOSPACE:
		stream = &output_stream;
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		if (!stream->buffers && sq_allocate_buffers(stream))
			return -ENOMEM;
		for (i = 0; i < stream->nbfrags; i++) {
			if (atomic_read(&stream->buffers[i].sem.count) > 0) {
				if (stream->buffers[i].size == 0)
					buf_info.fragments++;
				buf_info.bytes += stream->fragsize - stream->buffers[i].size;
			}
		}
		buf_info.fragstotal = stream->nbfrags;
		buf_info.fragsize = stream->fragsize;
		return (copy_to_user((void *)arg, &buf_info, sizeof(buf_info)) ? -EFAULT : 0);

    default:
        LEAVE(TRACE_ON,"sq_ioctl");
        return mixer_ioctl(inode, file, cmd, arg);
    }
    LEAVE(TRACE_ON,"sq_ioctl");
    return -EINVAL;
}



static struct file_operations sq_fops =
{
    llseek: sound_lseek,
    read: sq_read,
    write: sq_write,
    poll: sq_poll,
    ioctl: sq_ioctl,
    open: sq_open,
    release: sq_release,
};


static void __init sq_init(void)
{
#ifndef MODULE
    int sq_unit;
#endif
    ENTER(TRACE_ON,"sq_init");
    sq_unit = register_sound_dsp(&sq_fops, -1);
    if (sq_unit < 0) {
        LEAVE(TRACE_ON,"sq_init");
        return;
    }

    /* whatever you like as startup mode for /dev/dsp,
     * (/dev/audio hasn't got a startup mode). note that
     * once changed a new open() will *not* restore these!
     */
    sound.dsp.format = AFMT_S16_LE;

    sound.dsp.stereo = 0;
    sound.dsp.size = 16;

    /* set minimum rate possible without expanding */
    switch (sound.mach.type) {
        case DMASND_COTULLA:
            sound.dsp.speed = 8000;
            break;
    }

    /* before the first open to /dev/dsp this wouldn't be set */
    sound.soft = sound.dsp;
    sound.hard = sound.dsp;
    sound.trans = &transCotulla;
    sound.trans_read = &transReadCotulla;

    CotullaSetFormat(sound.dsp.format);
    sound_silence();
    LEAVE(TRACE_ON,"sq_init");
}

/*
 * /dev/sndstat
 */
/* state.buf should not overflow! */

static int state_open(struct inode *inode, struct file *file)
{
    char *buffer = state.buf;
    int len = 0;

    ENTER(TRACE_ON,"state_open");
    if (state.busy) {
        LEAVE(TRACE_ON,"state_open");
        return -EBUSY;
    }

    MOD_INC_USE_COUNT;
    state.ptr = 0;
    state.busy = 1;

    len += sprintf(buffer+len, "  COtulla DMA sound driver:\n");

    len += sprintf(buffer+len, "\tsound.format = 0x%x", sound.soft.format);
    switch (sound.soft.format) {
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
    len += sprintf(buffer+len, "\tsound.speed = %dHz (phys. %dHz)\n",
               sound.soft.speed, sound.hard.speed);
    len += sprintf(buffer+len, "\tsound.stereo = 0x%x (%s)\n",
               sound.soft.stereo,
               sound.soft.stereo ? "stereo" : "mono");
    state.len = len;
    LEAVE(TRACE_ON,"state_open");
    return 0;
}


static int state_release(struct inode *inode, struct file *file)
{
    ENTER(TRACE_ON,"state_release");
    state.busy = 0;
    MOD_DEC_USE_COUNT;
    LEAVE(TRACE_ON,"state_release");
    return 0;
}


static ssize_t state_read(struct file *file, char *buf, size_t count,
              loff_t *ppos)
{
    int n = state.len - state.ptr;
    ENTER(TRACE_ON,"state_read");
    if (n > count)
        n = count;
    if (n <= 0) {
        LEAVE(TRACE_ON,"state_read");
        return 0;
    }
    if (copy_to_user(buf, &state.buf[state.ptr], n)) {
        LEAVE(TRACE_ON,"state_read");
        return -EFAULT;
    }
    state.ptr += n;
    LEAVE(TRACE_ON,"state_read");
    return n;
}


static struct file_operations state_fops =
{
    llseek: sound_lseek,
    read: state_read,
    open: state_open,
    release: state_release,
};


static void __init state_init(void)
{
#ifndef MODULE
    int state_unit;
#endif
    ENTER(TRACE_ON,"state_init");
    state_unit = register_sound_special(&state_fops, SND_DEV_STATUS);
    if (state_unit < 0) {
        LEAVE(TRACE_ON,"state_init");
        return;
    }
    state.busy = 0;

    LEAVE(TRACE_ON,"state_init");

}

static void __init hp_irq_init(void)
{	
	ASIC3_GPIO_DIR_D &= ~ HEADPHONE_IN;	// set input
	ASIC3_GPIO_INTYP_D |= HEADPHONE_IN;	// edge trigger
	ASIC3_GPIO_INTSLP_D &= ~ HEADPHONE_IN; // enable interrupt
	detect_edge_and_set();
}



/*** Common stuff ********************************************************/

static long long sound_lseek(struct file *file, long long offset, int orig)
{
    ENTER(TRACE_ON,"sound_lseek");
    LEAVE(TRACE_ON,"sound_lseek");
    return -ESPIPE;
}

/*** Power Management ****************************************************/
#ifdef CONFIG_PM
static int cotulla_sound_pm_callback(struct pm_dev *pm_dev,
                    pm_request_t req, void *data)
{
// sample rate variable added by Alamy
/* Save sample rate dsp value at suspend, restore it at resume */
static unsigned int	sample_rate_div_value;

	UDA1380REGS CodecReg;
	//int original_aduio_clock;
	ENTER(TRACE_PM,"cotulla_sound_pm_callback");
	switch (req) {
	case PM_SUSPEND:
 		Cotulla_CODEC_REGS_Init(&CodecReg);
		CodecReg.power.power_u.reg_val = 0x0000;	// 0x02H, turn on only bias power
		Write_I2C_WORD(W1380_CODEC_ADDRESS,
			       CodecReg.power.reg_num, CodecReg.power.power_u.reg_val);
		Cotulla_AUD_off();
		///CKEN &= ~ 0x4100;
		//eRic24!
		//cotulla_dma_channel_deinit();
		disable_irq(IRQ_ASIC3_HEADPHONE_IN);
		clr_driver_waste_bits(AUDIO_WASTE);
		if (audio_wr_refcount == 1) {
			Cotulla_SPEAKER_off();
		}

		if (audio_rd_refcount == 1) {
			sample_rate_div_value = I2S_SADIV;
			Cotulla_MIC_off();
		}

        	break;
        	
	case PM_RESUME:
		enable_irq(IRQ_ASIC3_HEADPHONE_IN);
		CKEN |= 0x4100;

		Cotulla_AUD_on();
		udelay(5000);
		Cotulla_sound_hard_init();	// init I2s, I2c
		udelay(1000);
		Cotulla_CODEC_reset();  // codec 1380 reset
		udelay(1000);

		cotulla_dma_channel_init();
		CotullaDmaInit();

		Cotulla_disable_sound();

		Cotulla_i2sc_unit_power_saving();
		set_driver_waste_bits(AUDIO_WASTE);

		///////////////////////////////////////////////////
		// EricLai: for full-duplex 09/13/02
		if( audio_wr_refcount == 1 && audio_rd_refcount == 1 ) {
		    sq_release_buffers(&output_stream);
		    sq_release_buffers( &input_stream );

		    Cotulla_audio_power_on( FULLDUPLEX );

		    CODEC_SetVolume( sound.volume_right, 0 );
		    I2S_SADIV = Audio_Clock;

		    sound_set_stereo( sound.soft.stereo );
		    sound_set_format( sound.soft.format );
		    cotulla_under_recording = 1;

		    /* I2S_SADIV is changed in above code, reset again.
		       sound_set_speed() will work, but has to convert
		       value from BITS mode to USER mode
		    */
		    //I2S_SADIV = sample_rate_div_value;
		}
		else
	        if (audio_wr_refcount == 1) {
			sq_release_buffers(&output_stream);
			Cotulla_audio_power_on( PLAYBACK );
			CODEC_SetVolume( sound.volume_right, sound.volume_left);
			I2S_SADIV = Audio_Clock;
	        }
		else
		  // Resume Recording block added by Alamy
		  if (audio_rd_refcount == 1) {
			  sq_release_buffers( &input_stream );
			  Cotulla_audio_power_on( RECORDING );

			  sound_set_stereo( sound.soft.stereo );
			  sound_set_format( sound.soft.format );
			  cotulla_under_recording = 1;

			  /* I2S_SADIV is changed in above code, reset again.
			   sound_set_speed() will work, but has to convert
			   value from BITS mode to USER mode
			   */
			  I2S_SADIV = sample_rate_div_value;
		  }

		// Cotulla_audio_power_on() will initial codec, restore them
		if (audio_rd_refcount == 1) {
			// audio_igain & cotulla_igain should have same value
			CODEC_SetIgain( audio_igain  );	// MIC Gain
		}
		if (audio_wr_refcount == 1) {
			CODEC_SetVolume( sound.volume_right, sound.volume_left);
		}
		break;

	case PM_BATTERY_CRITICAL_LOW:
		cotulla_battery_critical_low = 1;
		if (sound.volume_left > 40)
			sound.volume_left = 40;
		if (sound.volume_right > 40)
			sound.volume_right = 40;
		break;
	}
	
	LEAVE(TRACE_PM,"collie_sound_pm_callback");
	return 0;
}
#endif

void detect_edge_and_set(void)
{
	if( ! (ASIC3_GPIO_PSTS_D & HEADPHONE_IN) )
	{
		ASIC3_GPIO_ETSEL_D |= HEADPHONE_IN; /* falling -> rising */
	}
	else
	{
		ASIC3_GPIO_ETSEL_D &= ~HEADPHONE_IN; /* rising -> falling */
	}
	
}

static void Cotulla_hp_interrupt(int irq, void *dummy, struct pt_regs *fp)

{
	ENTER(TRACE_ON,"Cotulla_hp_interrupt");

	detect_edge_and_set();
	
	if ( ASIC3_GPIO_PSTS_D & HEADPHONE_IN)
	{
		if( audio_wr_refcount ){
			Cotulla_SPEAKER_off();
			if( !audio_rd_refcount )
			  Cotulla_HP_on();
		}
	}
	else
	{
		if( audio_wr_refcount ){
			Cotulla_SPEAKER_on();
			if( !audio_rd_refcount )
			  Cotulla_HP_off();
		}
	}

	LEAVE(TRACE_ON,"Cotulla_hp_interrupt");

}

/*** Config & Setup ******************************************************/

int __init Cotulla_sound_init(void)
{
    int has_sound = 0;

    ENTER(TRACE_ON,"Cotulla_sound_init");
    has_sound = 1;
    sound.mach = machCotulla;

    if (!has_sound) {
        LEAVE(TRACE_ON,"Cotulla_sound_init");
        return -1;
    }

    CKEN &= ~0x4100;	//make sure to turn off i2s, i2c clock
    Cotulla_AUD_on();
    //wait_ms(10);// wait 10 ms
    udelay(5000);
    CKEN |= 0x4100;	// turn on the i2c clock enable
    udelay(1000);
	Cotulla_sound_hard_init();
    udelay(1000);
    Cotulla_CODEC_reset();  // codec 1380 reset
    //GPAFR1_L |= 0x01;     // turn on the I2S system clock
    //wait_ms(10);            //  wait 10 ms
    udelay(1000); 

    Cotulla_i2sc_unit_power_saving();
    
    Cotulla_MIC_off();	// set HP_MIC high (stereo) , 2002.5.18 17:48
    if (!sound.mach.irqinit()) {
        printk("Sound driver: Interrupt initialization failed\n");
        LEAVE(TRACE_ON,"Cotlla_sound_init");
        return -1;
    }

    /* Set up sound queue, /dev/audio and /dev/dsp. */

    /* Set default settings. */
    sq_init();

    /* Set up /dev/sndstat. */
    state_init();

    /* Set up /dev/mixer. */
    mixer_init();

    /* head-phone interrupt init. */
    hp_irq_init();
	set_driver_waste_bits(AUDIO_WASTE);

#if (defined DEBUG) && (defined CONFIG_PROC_FS)
	discovery_audio_proc_register();
#endif

#ifdef MODULE
    irq_installed = 1;
#endif

    printk("Cotulla Sound Driver Installed\n");

#ifdef CONFIG_PM
    cotulla_sound_pm_dev = pm_register(PM_COTULLA_DEV, 0,
                        cotulla_sound_pm_callback);
#endif

    if ( request_irq(IRQ_ASIC3_HEADPHONE_IN, Cotulla_hp_interrupt,
                   SA_INTERRUPT, "INSERT-HEADPHONE", Cotulla_hp_interrupt))  {
        printk("%s: request_irq(%d) failed.\n",
            __FUNCTION__, IRQ_ASIC3_HEADPHONE_IN);
    }

    LEAVE(TRACE_ON,"Cotulla_sound_init");
    return 0;
}

module_init(Cotulla_sound_init);

#ifdef MODULE

int init_module(void)
{
    ENTER(TRACE_ON,"init_module");
    Collie_sound_init();
	set_driver_waste_bits(AUDIO_WASTE);
    LEAVE(TRACE_ON,"init_module");
    return 0;
}

void cleanup_module(void)
{
    ENTER(TRACE_ON,"cleanup_module");
    if (irq_installed) {
        sound_silence();
        sound.mach.irqcleanup();
    }

	sq_release_buffers( &input_stream );
	sq_release_buffers( &output_stream );
	clr_driver_waste_bits(AUDIO_WASTE);
#if (defined DEBUG) && (defined CONFIG_PROC_FS)
	discovery_audio_proc_unregister();
#endif

    if (mixer_unit >= 0)
        unregister_sound_mixer(mixer_unit);
    if (state_unit >= 0)
        unregister_sound_special(state_unit);
    if (sq_unit >= 0)
        unregister_sound_dsp(sq_unit);
    LEAVE(TRACE_ON,"cleanup_module");
}

#endif /* MODULE */

#if (defined DEBUG) && (defined CONFIG_PROC_FS)
struct audio_entry {
	char *name;
	int (*fn)(char*, char**, off_t, int);
};

struct proc_dir_entry *proc_audio;
 
static struct audio_entry dir[] = {
	{"status",	audio_stat_proc_read},
};

#define AUDIO_ENTRIES_NUM (sizeof(dir)/sizeof(dir[0]))

static void discovery_audio_proc_register(void)
{
	int i;

	proc_audio = proc_mkdir("driver/audio", NULL);
	if (proc_audio == NULL)
		return;
	proc_audio->owner = THIS_MODULE;

	for (i=0;i<AUDIO_ENTRIES_NUM;i++)
		create_proc_info_entry(dir[i].name,0,proc_audio,dir[i].fn);
}

static void discovery_audio_proc_unregister(void)
{
	int i;

	if (proc_audio) {
		for (i=0;i<AUDIO_ENTRIES_NUM;i++)
		  remove_proc_entry(dir[i].name, proc_audio);

		remove_proc_entry("driver/audio", NULL);
		proc_audio = NULL;
	}
}

static int audio_stat_proc_read(char *buf, char **start, off_t offset, int len)
{
	audio_stream_t	*stream;
	audio_buf_t *b;
	unsigned long flags;
	int	i, frg, bsize;

	save_flags(flags);
	cli();

	len = 0;

	len += sprintf(buf+len, "STATUS: " );
	if( !audio_rd_refcount && !audio_wr_refcount ){
		len += sprintf(buf+len, "not opened\n" );
	}else{
		len += sprintf(buf+len, "open mode:%s%s\n",
					   (audio_rd_refcount)? "R":"",
					   (audio_wr_refcount)? "W":"");
		len += sprintf(buf+len, "buffer size: %d bytes x %d\n",
					   audio_fragsize, audio_nbfrags);
		len += sprintf(buf+len, "access: R:%d W:%d S:%d\n",
					   cotulla_do_reading,
					   cotulla_do_writing,
					   cotulla_do_sync);

		stream = &output_stream;
		if ( stream->buffers ){
			frg = 0;
			bsize = 0;
			for (i = 0; i < stream->nbfrags; i++) {
				if (atomic_read(&stream->buffers[i].sem.count) > 0) {
					if (stream->buffers[i].size == 0)
					  frg++;
					bsize += stream->fragsize - stream->buffers[i].size;
				}
			}
			len += sprintf(buf+len, "buffer : remains %d frags(%d bytes)\n",
						   frg, bsize);

			len += sprintf(buf+len, "current buffer: %d\n", stream->buf_idx );
			len += sprintf(buf+len, "buf  dma size\n" );
			b = stream->buffers;
			for (i = 0; i < stream->nbfrags; i++) {
				len += sprintf(buf+len, " %3d: %3d: %d\n",
							   i, b->dma_count, b->size);
				b++;
			}
		}
	}
	restore_flags(flags);

	return len;
}
#endif	/* (defined DEBUG) && (defined CONFIG_PROC_FS) */
