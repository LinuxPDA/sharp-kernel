/*
 * linux/drivers/sound/collie-ssp.c
 *
 * SA1100 16bit DAC sound driver for COLLIE (SHARP)
 *
 * Based on dmasound_iris.c (IRIS)
 *
 *		DAC : PCM1741 (BURR-BROWN) or PCM1717 (BURR-BROWN)
 *		I/F : Synchronous serial port (SSP) TI mode
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
 *	23-Oct-2001 SHARP
 *		tune hardware control method
 *	12-Nov-2001 Lineo Japan, Inc.
 *	14-Feb-2003 Sharp Corporation 8bit , GETOSPACE
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

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/ucb1200.h>

#include <linux/soundcard.h>
#include <asm/proc/cache.h>

#include <asm/arch/gpio.h>
#include <asm/arch/m62332.h>
#include <asm/arch/tc35143.h>

#undef DEBUG
//#define DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif

#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0) || \
    defined(CONFIG_COLLIE_TR1) || defined(CONFIG_COLLIE_DEV)
#define COLLIE_TRY_ONE
#else
#undef	COLLIE_TRY_ONE
#endif

#ifdef COLLIE_TRY_ONE
#ifndef	GPIO_REMOCON_ADC_SW
#define	GPIO_REMOCON_ADC_SW	GPIO_GPIO(18)
#endif

static DECLARE_WAIT_QUEUE_HEAD(audio_on);


static inline void collie_rc_set_int_mode(void)
{
	GPSR = GPIO_REMOCON_ADC_SW;
}

static inline void collie_rc_set_adc_mode(void)
{
	GPCR = GPIO_REMOCON_ADC_SW;
}
#endif


int collie_dmasound_irq = -1;
#define	COLLIE_SOUND_DMA_CHANNEL	(collie_dmasound_irq)

#define SND_NDEVS	256	/* Number of supported devices */
#define SND_DEV_CTL	0	/* Control port /dev/mixer */
#define SND_DEV_SEQ	1	/* Sequencer output /dev/sequencer (FM
				   synthesizer and MIDI output) */
#define SND_DEV_MIDIN	2	/* Raw midi access */
#define SND_DEV_DSP	3	/* Digitized voice /dev/dsp */
#define SND_DEV_AUDIO	4	/* Sparc compatible /dev/audio */
#define SND_DEV_DSP16	5	/* Like /dev/dsp but 16 bits/sample */
#define SND_DEV_STATUS	6	/* /dev/sndstat */
/* #7 not in use now. Was in 2.4. Free for use after v3.0. */
#define SND_DEV_SEQ2	8	/* /dev/sequencer, level 2 interface */
#define SND_DEV_SNDPROC 9	/* /dev/sndproc for programmable devices */
#define SND_DEV_PSS	SND_DEV_SNDPROC

#ifdef MODULE
static int sq_unit = -1;
static int mixer_unit = -1;
static int state_unit = -1;
static int irq_installed = 0;
#endif /* MODULE */

#ifdef CONFIG_PM
static struct pm_dev* collie_sound_pm_dev;
#endif

/*** Some declarations ***********************************************/
#define DMASND_TT	1
#define DMASND_FALCON	2
#define DMASND_AMIGA	3
#define DMASND_AWACS	4
#define DMASND_IRIS	5
#define DMASND_COLLIE	6

#define COLLIE_WAIT_AMP_ON	1	/* 10ms */
#define COLLIE_WAIT_LCM_ACC_XEN	50	/* 500ms */
#ifdef MODULE
static int catchRadius = 0;
#endif
static int collie_amp_init = 0;
static int collie_dac_init = 0;
static int collie_op_shdn_on = 0;
static int collie_resume = 0;
static int collie_hard_mute = 1;
static int collie_soft_mute = 1;
static int collie_volume = 0;

int collie_buzzer_volume = 0;

#if 1
static DECLARE_WAIT_QUEUE_HEAD(open_queue);

#define SIGNAL_RECEIVED	(signal_pending(current))
#define ONE_SECOND	HZ	/* in jiffies (100ths of a second) */
#define SLEEP(queue, time_limit) \
	interruptible_sleep_on_timeout((wait_queue_head_t*)&queue, (time_limit));
#define WAKE_UP(queue)	(wake_up_interruptible((wait_queue_head_t*)&queue))
#endif

#define AUDIO_NBFRAGS_DEFAULT   8
#define AUDIO_FRAGSIZE_DEFAULT  8192


#define TRACE 0
#if TRACE
#define TRACE_ON	1
#define TRACE_SEM	0
#define	TRACE_SENDDATA	0
#define	TRACE_PM	1
#define TRACE_AMP	1
#define TRACE_DAC	1
#define TRACE_OP_SHDN	1
#define TRACE_WRITE	1
#define TRACE_MUTE	1
#define TRACE_CLOCK	1
#define TRACE_PAIF	1
#define TRACE_SSP	1
#define TRACE_VOLUME	1
#define TRACE_MIC	1
#define TRACE_INTERRUPT	0
int	cLevel = 0;
char	*pLevel[16] = {
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
  int	i;
  return (level < 16 ) ? pLevel[level] : pLevel[15];
}

#define	P_ID	(current->tgid)
#define	ENTER(f,fn)	{if(f)printk("%d:%s+[%d]%s\n",jiffies,indent(cLevel),P_ID,(fn));cLevel++;}
#define LEAVE(f,fn)	{cLevel--;if(f>1)printk("%d:%s-[%d]%s\n",jiffies,indent(cLevel),P_ID,(fn));}
#else	/* ! TRACE */
#define	ENTER(f,fn)
#define LEAVE(f,fn)
#endif	/* end TRACE */

/*
 * DAC power management
 */
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0) || \
    defined(CONFIG_COLLIE_TR1) || defined(CONFIG_COLLIE_DEV)
#define	DAC_OFF_WITH_DEVICE_OFF		1
#undef	HARD_MUTE_CTRL_DISABLE		
#else
#undef	DAC_OFF_WITH_DEVICE_OFF
#undef	HARD_MUTE_CTRL_DISABLE		
#endif


#define TRY_DELAY_OFF
#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.19 */
static DECLARE_WAIT_QUEUE_HEAD(delay_off);
struct semaphore df_sem;
/*
 * delay execution
 */
static unsigned int	DelayedFlag = 0;
#define DELAY_DAC_OFF		0x1
#define DELAY_HARD_MUTE_ON	0x2

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
	int master;             /* master owner for buffer allocation */
} audio_buf_t;

typedef struct {
	audio_buf_t *buffers;   /* pointer to audio buffer structures */
	audio_buf_t *buf;       /* current buffer used by read/write */
	u_int buf_idx;          /* index for the pointer above... */
	u_int fragsize;         /* fragment i.e. buffer size */
	u_int nbfrags;          /* nbr of fragments i.e. buffers */
} audio_stream_t;

static audio_stream_t output_stream;

#define NEXT_BUF(_s_,_b_) { \
	(_s_)->_b_##_idx++; \
	(_s_)->_b_##_idx %= (_s_)->nbfrags; \
	(_s_)->_b_ = (_s_)->buffers + (_s_)->_b_##_idx; }
					
/* Current specs for incoming audio data */
static u_int audio_fragsize;
static u_int audio_nbfrags;

static volatile int audio_wr_refcount;	/* nbr of concurrent open() for playback */

static ssize_t (*ct_func)(const u_char *, size_t, u_char *, ssize_t *, ssize_t) = NULL;

#ifdef MODULE
MODULE_PARM(catchRadius, "i");
#endif
MODULE_PARM(numBufs, "i");
MODULE_PARM(bufSize, "i");

#define min(x, y)	((x) < (y) ? (x) : (y))

#define IOCTL_IN(arg, ret) \
	do { int error = get_user(ret, (int *)(arg)); \
		if (error) return error; \
	} while (0)
#define IOCTL_OUT(arg, ret)	ioctl_return((int *)(arg), ret)

/*
 * SA1110 GPIO (17)
 */
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
#define COLLIE_GPIO_OPSHDN	GPIO_GPIO (17)	/* AMP contorol        */
#else	/* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
#define COLLIE_GPIO_MIC		GPIO_GPIO (17)	/* MIC contorol        */
#endif	/* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */

/*
 * DAC setup data
 */
#if defined(CONFIG_COLLIE_PCM1741)
#define DAC_REG16_SetupData	0x10ff	/*  control register 16 */
#define DAC_REG17_SetupData	0x11ff	/*  control register 17 */
#define DAC_REG18_SetupData	0x1203	/*  control register 18 */
#define DAC_REG18_MuteOn	0x1203	/*  control register 18 */
#define DAC_REG18_MuteOff	0x1200	/*  control register 18 */
#define DAC_REG19_SetupData	0x1303	/*  control register 19 */
#define DAC_REG19_DACOff	0x1303	/*  control register 19 */
#define DAC_REG19_DACOn		0x1300	/*  control register 19 */
#define DAC_REG20_SetupData	0x1403	/*  control register 20 */
#define DAC_REG21_SetupData	0x0000	/*  control register 21 */
#define DAC_REG22_SetupData	0x1600	/*  control register 22 */
#elif defined(CONFIG_COLLIE_PCM1717)
#define DAC_MODE0_SetupData	0x01ff	/*  control register MODE0 */
#define DAC_MODE1_SetupData	0x03ff	/*  control register MODE1 */
#define DAC_MODE2_SetupData	0x0410	/*  control register MODE2 */
#define DAC_MODE3_SetupData	0x0690	/*  control register MODE3 */
#endif


/*** Some low level helpers **************************************************/

int	clock_set_data[8] = {	
		( LCM_ACC_XSEL0 | LCM_ACC_CLKSEL000 ) ,
		( LCM_ACC_XSEL0 | LCM_ACC_CLKSEL010 ) ,
		( LCM_ACC_XSEL0 | LCM_ACC_CLKSEL100 ) ,
		( LCM_ACC_XSEL1 | LCM_ACC_CLKSEL000 ) ,
		( LCM_ACC_XSEL1 | LCM_ACC_CLKSEL001 ) ,
		( LCM_ACC_XSEL1 | LCM_ACC_CLKSEL010 ) ,
		( LCM_ACC_XSEL1 | LCM_ACC_CLKSEL011 ) ,
		( LCM_ACC_XSEL1 | LCM_ACC_CLKSEL101 ) ,
};


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
	int format;		/* AFMT_* */
	int stereo;		/* 0 = mono, 1 = stereo */
	int size;		/* 8/16 bit*/
	int speed;		/* speed */
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
	MACHINE mach;		/* machine dependent things */
	SETTINGS hard;		/* hardware settings */
	SETTINGS soft;		/* software settings */
	SETTINGS dsp;		/* /dev/dsp default settings */
	TRANS *trans;		/* supported translations */
	int volume_left;	/* volume (range is machine dependent) */
	int volume_right;
	int bass;		/* tone (range is machine dependent) */
	int treble;
	int gain;
	int minDev;		/* minor device number currently open */
};

static struct sound_settings sound;

#ifdef CONFIG_PM
extern int autoPowerCancel;
#endif
int collie_main_volume;
extern int collie_under_recording;      /* collie_buzzer.c */
#define COLLE_RECORDING	(collie_under_recording)

static void Collie_Set_Volume(int volume);
static int Collie_Get_Volume(void);
static void Collie_disable_sound(void);
#ifdef CONFIG_PM
#if 0
static void Collie_clock_stop(void);
static void Collie_FS8KLPF_stop(void);
#endif
#endif
static int CollieIrqInit(void);
static int CollieGetSamp(void);
static void Collie_OP_SHDN_on(void);
static void Collie_OP_SHDN_off(void);
static void Collie_FS8KLPF_start(void);
#ifdef MODULE
static void CollieIrqCleanUp(void);
#endif /* MODULE */
static void CollieSilence(void);
static void Collie_DAC_sendword(int);
static void CollieInit(void);
static int CollieSetFormat(int format);
static void Collie_sq_interrupt(void*, int);
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

	ENTER(TRACE_ON,"collie_ct_law");
	frameLeft >>= 2;
	if (stereo)
		userCount >>= 1;
	used = count = min(userCount, frameLeft);
	if (!COLLE_RECORDING) {
		while (count > 0) {
			u_char data;
			if (get_user(data, userPtr++)) {
				LEAVE(TRACE_ON,"collie_ct_law");
				return -EFAULT;
			}
			val = table[data];
			*p++ = val;		/* Left Ch. */
			if (stereo) {
				if (get_user(data, userPtr++)) {
				LEAVE(TRACE_ON,"collie_ct_law");
				return -EFAULT;
				}
				val = table[data];
			}
			*p++ = val;		/* Right Ch. */
			count--;
		}
	} else {
		while (count > 0) {
			u_char data;
			int ave;
			if (get_user(data, userPtr++)) {
				LEAVE(TRACE_ON,"collie_ct_law");
				return -EFAULT;
			}
			val = table[data];
			ave = val;		/* Left Ch. */
			if (stereo) {
				if (get_user(data, userPtr++)) {
					LEAVE(TRACE_ON,"collie_ct_law");
					return -EFAULT;
				}
				val = table[data];
			}
			ave += val;		/* Right Ch. */
			ave >>= 1;
			*p++ = 0;		/* Left Ch. */
			*p++ = ave;		/* Right Ch. */
			count--;
		}
	}
	*frameUsed += used * 4;
	LEAVE(TRACE_ON,"collie_ct_law");
	return stereo? used * 2: used;
}


static ssize_t collie_ct_s8(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int stereo = sound.soft.stereo;
	short val;

	ENTER(TRACE_ON,"collie_ct_s8");
	frameLeft >>= 2;
	if (stereo)
		userCount >>= 1;
	used = count = min(userCount, frameLeft);
	if (!COLLE_RECORDING) {
		while (count > 0) {
			u_char data;

			if (get_user(data, userPtr++)) {
				LEAVE(TRACE_ON,"collie_ct_s8");
				return -EFAULT;
			}
			val = ( data - 0x80 ) << 8;
			*p++ = val;		/* Left Ch. */
			if ( stereo ) {
				if ( get_user(data, userPtr++)) {
					LEAVE(TRACE_ON,"collie_ct_s8");
					return -EFAULT;
				}
				val = ( data - 0x80 ) << 8;
			}
			*p++ = val;		/* Right Ch. */
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
			val = ( data - 0x80 ) << 8;
			ave = val;		/* Left Ch. */
			if ( stereo ) {
				if ( get_user(data, userPtr++)) {
					LEAVE(TRACE_ON,"collie_ct_s8");
					return -EFAULT;
				}
				val = ( data - 0x80 ) << 8;
			}
			ave += val;		/* Right Ch. */
			ave >>= 1;
			*p++ = 0;		/* Left Ch. */
			*p++ = ave;		/* Right Ch. */
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
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int stereo = sound.soft.stereo;
	short val;

	ENTER(TRACE_ON,"collie_ct_u8");
	frameLeft >>= 2;
	if (stereo)
		userCount >>= 1;
	used = count = min(userCount, frameLeft);
	if (!COLLE_RECORDING) {
		while (count > 0) {
			u_char data;

			if (get_user(data, userPtr++)) {
				LEAVE(TRACE_ON,"collie_ct_u8");
				return -EFAULT;
			}
			val = data;
			*p++ = (val ^ 0x80) << 8;		/* Left Ch. */
			if ( stereo ) {
				if ( get_user(data, userPtr++)) {
					LEAVE(TRACE_ON,"collie_ct_u8");
					return -EFAULT;
				}
				val = data;
			}
			*p++ = (val ^ 0x80) << 8;		/* Right Ch. */
			count--;
		}
	} else {
		while (count > 0) {
			u_char data;
			int ave;

			if (get_user(data, userPtr++)) {
				LEAVE(TRACE_ON,"collie_ct_u8");
				return -EFAULT;
			}
			val = data;
			ave = (val ^ 0x80) << 8;		/* Left Ch. */
			if ( stereo ) {
				if ( get_user(data, userPtr++)) {
					LEAVE(TRACE_ON,"collie_ct_u8");
					return -EFAULT;
				}
				val = data;
			}
			ave += (val ^ 0x80) << 8;		/* Right Ch. */
			ave >>= 1;
			*p++ = 0;			/* Left Ch. */
			*p++ = ave;			/* Right Ch. */
			count--;
		}
	}
	*frameUsed += used * 4;
	LEAVE(TRACE_ON,"collie_ct_u8");
	return stereo? used * 2: used;
}


static ssize_t collie_ct_s16(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
	ssize_t count, used;
	int stereo = sound.soft.stereo;
	short *fp = (short *) &frame[*frameUsed];

	ENTER(TRACE_ON,"collie_ct_s16");
	frameLeft >>= 2;
	userCount >>= (stereo? 2: 1);
	used = count = min(userCount, frameLeft);
	if (!stereo) {
		short *up = (short *) userPtr;
		while (count > 0) {
			short data;
			if (get_user(data, up++)) {
				LEAVE(TRACE_ON,"collie_ct_s16");
				return -EFAULT;
			}
			*fp++ = (!COLLE_RECORDING) ? data : 0;	/* Left Ch. */
			*fp++ = data;
			count--;
		}
	} else {
		short *up = (short *) userPtr;
		while (count > 0) {
			short data;
			short temp;
			if (get_user(data, up++)) {
				LEAVE(TRACE_ON,"collie_ct_s16");
				return -EFAULT;
			}
			if (get_user(temp, up++)) {
				LEAVE(TRACE_ON,"collie_ct_s16");
				return -EFAULT;
			}
			if (!COLLE_RECORDING) {
				*fp++ = data;		/* Left Ch. */
				*fp++ = temp;		/* Right Ch. */
			} else {
				data >>= 1;
				data += (temp >> 1);
				*fp++ = 0;		/* Left Ch. */
				*fp++ = data;		/* Right Ch. */
			}
			count--;
		}
	}
	*frameUsed += used * 4;
	LEAVE(TRACE_ON,"collie_ct_s16");
	return stereo? used * 4: used * 2;
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
	if (!COLLE_RECORDING) {
		while (count > 0) {
			int data;
			int temp;
			if (get_user(data, up++)) {
				LEAVE(TRACE_ON,"collie_ct_u16");
				return -EFAULT;
			}
			data ^= mask;
			*fp++ = data;			/* Left Ch. */
			if (stereo) {
				if (get_user(temp, up++)) {
					LEAVE(TRACE_ON,"collie_ct_u16");
					return -EFAULT;
				}
				temp ^= mask;
				data  = temp;
				data ^= mask;
			}
			*fp++ = data;			/* Right Ch. */
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
			ave = data;			/* Left Ch. */
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
			*fp++ = 0;			/* Left Ch. */
			*fp++ = ave;			/* Right Ch. */
			count--;
		}
	}
	*frameUsed += used * 4;
	LEAVE(TRACE_ON,"collie_ct_u16");
	return stereo? used * 4: used * 2;
}

static TRANS transCollie = {
	collie_ct_law, collie_ct_law, collie_ct_s8, collie_ct_u8,
	collie_ct_s16, collie_ct_u16, collie_ct_s16, collie_ct_u16
};

/*** Low level stuff *********************************************************/

static void Collie_Set_Volume(int volume)
{
	ENTER(TRACE_ON,"Collie_Set_Volume");

	sound.volume_left = volume & 0xff;
	if ( sound.volume_left > 100 ) sound.volume_left = 100;

	collie_main_volume = sound.volume_left;

	sound.volume_right = ( volume & 0xff00 >> 8);
	if ( sound.volume_right > 100 ) sound.volume_right = 100;
	LEAVE(TRACE_ON,"Collie_Set_Volume");

	collie_buzzer_volume = sound.volume_right;

}


static int Collie_Get_Volume(void)
{
	ENTER(TRACE_ON,"Collie_Get_Volume");
	LEAVE(TRACE_ON,"Collie_Get_Volume");
	return ( sound.volume_right << 8 | sound.volume_left );
}

static void wait_ms(int ten_ms)
{
	ENTER(TRACE_ON,"wait_ms");
	LEAVE(TRACE_ON,"wait_ms");
	schedule_timeout(ten_ms);
}

static inline void Collie_AMP_off(void)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_AMP,"Collie_AMP_off");
#if 0	/* OBSOLETED: power controled by only OP_SHDN */
	/* Set up TC35143 GPIO I/O Direction (GPIO4 output mode) */
	ucb1200_set_io_direction(TC35143_GPIO_AMP_ON,
				 TC35143_IODIR_OUTPUT);
 	/* AMP OFF */
	ucb1200_set_io(TC35143_GPIO_AMP_ON,
		       TC35143_IODAT_LOW);
	collie_amp_init = 0;
#endif	/* 0 */
	LEAVE(TRACE_AMP,"Collie_AMP_off");
#endif	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

static inline void Collie_AMP_on(void)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_AMP,"Collie_AMP_on");
//	if (!collie_amp_init) {
		/* Set up TC35143 GPIO I/O Direction (GPIO4 output mode) */
		ucb1200_set_io_direction(TC35143_GPIO_AMP_ON,
					 TC35143_IODIR_OUTPUT);
		/* AMP ON */
		ucb1200_set_io(TC35143_GPIO_AMP_ON, TC35143_IODAT_HIGH);
		SCP_REG_GPWR |= SCP_AMP_ON;
		wait_ms(COLLIE_WAIT_AMP_ON);
		collie_amp_init = 1;
//	}
	LEAVE(TRACE_AMP,"Collie_AMP_on");
#endif	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

static inline void Collie_AMP_init(void)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_AMP,"Collie_AMP_init");
  	Collie_AMP_off();
	LEAVE(TRACE_AMP,"Collie_AMP_init");
#endif	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

static inline void Collie_DAC_off(void)
{
	ENTER(TRACE_DAC,"Collie_DAC_off");
	if (!COLLE_RECORDING) {
		/* DAC OFF */
		LCM_GPD &= ~(LCM_GPIO_DAC_ON);	/* set up output */
		LCM_GPO &= ~(LCM_GPIO_DAC_ON);

		/* LoCoMo GPIO disable */
		LCM_GPE &= ~(LCM_GPIO_DAC_ON);
		collie_dac_init = 0;
	}
	LEAVE(TRACE_DAC,"Collie_DAC_off");
}

static inline void Collie_DAC_on(void)
{
	ENTER(TRACE_DAC,"Collie_DAC_on");
//	if (!collie_dac_init) {
	if (!(LCM_GPO & LCM_GPIO_DAC_ON)) {
		/* LoCoMo GPIO enable */
		LCM_GPE &= ~LCM_GPIO_DAC_ON;
	
		/* DAC ON */
		LCM_GPD &= ~(LCM_GPIO_DAC_ON);	/* set up output */
		LCM_GPO |= LCM_GPIO_DAC_ON;	
		collie_dac_init = 1;
		{
		  /* wait for DAC power */
		  int time = jiffies + 20;
		  while (jiffies <= time)
		    schedule();
		}
	}
//	}
	LEAVE(TRACE_DAC,"Collie_DAC_on");
}

#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP */
static inline void Collie_DAC_delay_off(void)
{
	ENTER(TRACE_DAC,"Collie_DAC_deleay_off");
	down(&df_sem);
	SetDelay(DELAY_DAC_OFF);
	up(&df_sem);
	LEAVE(TRACE_DAC,"Collie_DAC_delay_off");
}
#endif

static inline void Collie_DAC_init(void)
{
	ENTER(TRACE_DAC,"Collie_DAC_init");
	/* LoCoMo GPIO enable */
	LCM_GPE &=
	  ~(LCM_GPIO_DAC_SDATA | LCM_GPIO_DAC_SCK | LCM_GPIO_DAC_SLOAD);
	/* DAC Setting */
	LCM_GPD &=
	  ~(LCM_GPIO_DAC_SDATA | LCM_GPIO_DAC_SCK | LCM_GPIO_DAC_SLOAD);

#if defined(CONFIG_COLLIE_PCM1741)

	Collie_DAC_sendword(DAC_REG16_SetupData);	/* register16 */
	Collie_DAC_sendword(DAC_REG17_SetupData);	/* register17 */
	Collie_DAC_sendword(DAC_REG18_SetupData);	/* register18 */
	Collie_DAC_sendword(DAC_REG19_SetupData);	/* register19 */
	Collie_DAC_sendword(DAC_REG20_SetupData);	/* register20 */
////	Collie_DAC_sendword(DAC_REG21_SetupData);	/* register21 */
	Collie_DAC_sendword(DAC_REG22_SetupData);	/* register22 */

#elif defined(CONFIG_COLLIE_PCM1717)

	Collie_DAC_sendword(DAC_MODE0_SetupData);	/* MODE0 */
	Collie_DAC_sendword(DAC_MODE1_SetupData);	/* MODE1 */
	Collie_DAC_sendword(DAC_MODE2_SetupData);	/* MODE2 */
	Collie_DAC_sendword(DAC_MODE3_SetupData);	/* MODE3 */

#endif

	/* LoCoMo GPIO disable */
	LCM_GPE &=
	  ~(LCM_GPIO_DAC_SDATA | LCM_GPIO_DAC_SCK | LCM_GPIO_DAC_SLOAD);
	LEAVE(TRACE_DAC,"Collie_DAC_init");
}

static inline void Collie_soft_DAC_on(void)
{
#if defined(CONFIG_COLLIE_PCM1741)
	ENTER(TRACE_DAC, "Collie_soft_DAC_on");
	Collie_DAC_sendword(DAC_REG19_DACOn);	/* register19 */
	LEAVE(TRACE_DAC, "Collie_soft_DAC_on");
#endif	/* CONFIG_COLLIE_PCM1741 */
}

static inline void Collie_soft_DAC_off(void)
{
#if defined(CONFIG_COLLIE_PCM1741)
	ENTER(TRACE_DAC, "Collie_soft_DAC_off");
       	Collie_DAC_sendword(DAC_REG19_DACOff);	/* register19 */
	LEAVE(TRACE_DAC, "Collie_soft_DAC_off");
#endif	/* CONFIG_COLLIE_PCM1741 */
}

static inline void Collie_soft_mute_init(void)
{
#if defined(CONFIG_COLLIE_PCM1741)
	ENTER(TRACE_MUTE, "Collie_soft_mute_init");
	Collie_DAC_sendword(DAC_REG18_MuteOn);	/* register18 */
	collie_soft_mute = 1;
	LEAVE(TRACE_MUTE, "Collie_soft_mute_init");
#endif	/* CONFIG_COLLIE_PCM1741 */
}

static inline void Collie_soft_mute_on(void)
{
#if defined(CONFIG_COLLIE_PCM1741)
	ENTER(TRACE_MUTE, "Collie_soft_mute_on");
	if (!collie_soft_mute) {
		Collie_DAC_sendword(DAC_REG18_MuteOn);	/* register18 */
		collie_soft_mute = 1;
	}
	LEAVE(TRACE_MUTE, "Collie_soft_mute_on");
#endif	/* CONFIG_COLLIE_PCM1741 */
}

static inline void Collie_soft_mute_off(void)
{
#if defined(CONFIG_COLLIE_PCM1741)
	ENTER(TRACE_MUTE, "Collie_soft_mute_off");
	if (collie_soft_mute) {
		Collie_DAC_sendword(DAC_REG18_MuteOff);	/* register18 */
		collie_soft_mute = 0;
	}
	LEAVE(TRACE_MUTE, "Collie_soft_mute_off");
#endif	/* CONFIG_COLLIE_PCM1741 */
}

static inline void Collie_hard_mute_init(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_MUTE, "Collie_hard_mute_init");
	SCP_REG_GPCR |= (SCP_GPCR_PA14 | SCP_GPCR_PA15);
#ifdef HARD_MUTE_CTRL_DISABLE
	SCP_REG_GPWR |= (SCP_GPCR_PA14 | SCP_GPCR_PA15);
#else
	SCP_REG_GPWR &= ~(SCP_GPCR_PA14 | SCP_GPCR_PA15);
#endif	/* HARD_MUTE_CTRL_DISABLE */
	collie_hard_mute = 1;
#if !defined(CONFIG_COLLIE_TR1) && !defined(CONFIG_COLLIE_DEV)
	{
		int time = jiffies + 5;
		while (jiffies <= time)
			schedule();
	}
#endif
	LEAVE(TRACE_MUTE, "Collie_hard_mute_init");
#endif	/* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_hard_mute_on(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
#ifndef HARD_MUTE_CTRL_DISABLE
	ENTER(TRACE_MUTE, "Collie_hard_mute_on");
	if (!collie_hard_mute) {
		SCP_REG_GPWR &= ~(SCP_GPCR_PA14 | SCP_GPCR_PA15);
		collie_hard_mute = 1;
#if !defined(CONFIG_COLLIE_TR1) && !defined(CONFIG_COLLIE_DEV)
		{
			int time = jiffies + 5;
			while (jiffies <= time)
				schedule();
		}
#endif
	}
	LEAVE(TRACE_MUTE, "Collie_hard_mute_on");
#endif	/* HARD_MUTE_CTRL_DISABLE */
#endif	/* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_hard_mute_off(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
#ifndef HARD_MUTE_CTRL_DISABLE
	ENTER(TRACE_MUTE, "Collie_hard_mute_off");
	if (collie_hard_mute) {
		if (!COLLE_RECORDING)
			SCP_REG_GPWR |= (SCP_GPCR_PA14 | SCP_GPCR_PA15);
		else
			SCP_REG_GPWR |= (SCP_GPCR_PA15);
		collie_hard_mute = 0;
#if !defined(CONFIG_COLLIE_TR1) && !defined(CONFIG_COLLIE_DEV)
		{
			int i;
			for (i=0; i<=1000; i++) {
				udelay(1);
			}
		}
#endif
	}
	LEAVE(TRACE_MUTE, "Collie_hard_mute_off");
#endif	/* HARD_MUTE_CTRL_DISABLE */
#endif	/* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.19 */
static inline void Collie_hard_mute_delay_on(void)
{
	ENTER(TRACE_MUTE, "Collie_hard_mute_delay_on");
	down(&df_sem);
	SetDelay(DELAY_HARD_MUTE_ON);
	up(&df_sem);
	LEAVE(TRACE_MUTE, "Collie_hard_mute_delay_on");
}
#endif

static inline void Collie_audio_clock_init(void)
{
	ENTER(TRACE_CLOCK, "Collie_audio_clock_init");
	LCM_ACC = 0;
	LEAVE(TRACE_CLOCK, "Collie_audio_clock_init");
}

static inline void Collie_audio_clock_on(void)
{
	ENTER(TRACE_CLOCK, "Collie_audio_clock_on");
	/* LoCoMo Audio clock on */
	LCM_ACC  = CollieGetSamp();
	barrier();
	LCM_ACC |= LCM_ACC_XON;
	wait_ms(COLLIE_WAIT_LCM_ACC_XEN);	/* wait 1msec */
	LCM_ACC |= LCM_ACC_XEN;
	barrier();
	LCM_ACC |= (LCM_ACC_MCLKEN | LCM_ACC_64FSEN);
	LEAVE(TRACE_CLOCK, "Collie_audio_clock_on");
}

static inline void Collie_audio_clock_off(void)
{
	ENTER(TRACE_CLOCK, "Collie_audio_clock_off");
	/* LoCoMo Audio clock off */
	LCM_ACC &= ~(LCM_ACC_XEN | LCM_ACC_MCLKEN | LCM_ACC_64FSEN);
	barrier();
	LCM_ACC &= ~(LCM_ACC_XON);
	LEAVE(TRACE_CLOCK, "Collie_audio_clock_off");
}

static inline void Collie_paif_init(void)
{
	ENTER(TRACE_PAIF, "Collie_paif_init");
	//	LCM_PAIF  = (LCM_PAIF_SCINV | LCM_PAIF_LRCRST);
	LCM_PAIF  = (LCM_PAIF_LRCRST);
	LEAVE(TRACE_PAIF, "Collie_paif_init");
}

static inline void Collie_paif_on(void)
{
	ENTER(TRACE_PAIF, "Collie_paif_on");
	LCM_PAIF  = (LCM_PAIF_SCINV | LCM_PAIF_LRCRST);
	LCM_PAIF &= ~(LCM_PAIF_LRCRST);
	LCM_PAIF |= (LCM_PAIF_SCEN | LCM_PAIF_LRCEN);
	LEAVE(TRACE_PAIF, "Collie_paif_on");
}

static inline void Collie_paif_off(void)
{
	ENTER(TRACE_PAIF, "Collie_paif_off");
	//	LCM_PAIF  = (LCM_PAIF_SCINV | LCM_PAIF_LRCRST);
	LCM_PAIF  = (LCM_PAIF_LRCRST);
	LEAVE(TRACE_PAIF, "Collie_paif_off");
}

static inline void Collie_MIC_init(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_MIC, "Collie_MIC_init");
	/* MIC to GPIO 17 */
	/* alternate functions for the GPIOs */
	GAFR &= ~( COLLIE_GPIO_MIC );
	
	/* Set the direction: 17 output */
	GPDR |= ( COLLIE_GPIO_MIC );
	
#if defined(CONFIG_COLLIE_TR1)
	/* Set pin level (Low) */
       	GPCR = ( COLLIE_GPIO_MIC );
#else
	/* Set pin level (High) */
       	GPSR = ( COLLIE_GPIO_MIC );
#endif
	LEAVE(TRACE_MIC, "Collie_MIC_init");
#endif	/* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_MIC_on(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_MIC, "Collie_MIC_on");
#if defined(CONFIG_COLLIE_TR1)
	GPSR = ( COLLIE_GPIO_MIC );
#else
	GPCR = ( COLLIE_GPIO_MIC );
#endif
	LEAVE(TRACE_MIC, "Collie_MIC_on");
#endif	/* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_MIC_off(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_MIC, "Collie_MIC_off");
	if (!COLLE_RECORDING) {
#if defined(CONFIG_COLLIE_TR1)
	       	GPCR = ( COLLIE_GPIO_MIC );
#else
       		GPSR = ( COLLIE_GPIO_MIC );
#endif
	}
	LEAVE(TRACE_MIC, "Collie_MIC_off");
#endif	/* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_volume_init(void)
{
	ENTER(TRACE_VOLUME, "Collie_volume_init");
	m62332_senddata(0, M62332_EVR_CH);
	collie_volume = 0;
	LEAVE(TRACE_VOLUME, "Collie_volume_init");
}

static inline void Collie_volume_on(void)
{
	ENTER(TRACE_VOLUME, "Collie_volume_on");
	if (collie_volume != sound.volume_left) {
		//Collie_hard_mute_on();
		m62332_senddata(0xff * sound.volume_left / 100, M62332_EVR_CH);
		//Collie_hard_mute_off();
		collie_volume = sound.volume_left;
	}
	LEAVE(TRACE_VOLUME, "Collie_volume_on");
}

static inline void Collie_volume_off(void)
{
	ENTER(TRACE_VOLUME, "Collie_volume_off");
	if (collie_volume) {
		//Collie_hard_mute_on();
		m62332_senddata(0, M62332_EVR_CH);
		//Collie_hard_mute_off();
		collie_volume = 0;
	}
	LEAVE(TRACE_VOLUME, "Collie_volume_off");
}

#define	VOL_THRES	40
static void Collie_volume_half_adjust(void)
{
	int	volume = collie_volume;
	ENTER(TRACE_VOLUME, "Collie_volume_half_adjust");
	if (collie_volume > sound.volume_left) {
		/* volume down */
		if (collie_volume > VOL_THRES) {
			if (sound.volume_left > VOL_THRES) {
				volume = (collie_volume + sound.volume_left)/2;
				if (volume == collie_volume) {
					volume = sound.volume_left;
				}
			} else {
				volume = (collie_volume + VOL_THRES)/2;
				if (volume == collie_volume) {
					volume = VOL_THRES;
				}
			}
		} else {
			/* we can pull down without noise */
			volume = sound.volume_left;
		}
	} else if (collie_volume < sound.volume_left) {
		/* volume up */
		if (sound.volume_left > VOL_THRES) {
			if (collie_volume < VOL_THRES) {
				/* we can pull up to VOL_THRES without noise */
				volume = VOL_THRES;;
			} else {
				volume = (collie_volume + sound.volume_left)/2;
				if (volume == collie_volume) {
					volume = sound.volume_left;
				}
			}
		} else {
			/* we can pull up without noise */
			volume = sound.volume_left;
		}
	}
	if (collie_volume != volume) {
		m62332_senddata(0xff * volume / 100, M62332_EVR_CH);
		collie_volume = volume;
	}
	LEAVE(TRACE_VOLUME, "Collie_volume_half_adjust");
}

static void Collie_volume_half_off(void)
{
	int volume;
	int delta = 1;
	ENTER(TRACE_VOLUME, "Collie_volume_half_off");
	while (0 < collie_volume) {
		if (collie_volume <= VOL_THRES) {
			volume = 0;
		} else {
			if (collie_volume > delta) {
				volume = collie_volume - delta;
			} else {
				volume = 0;
			}
			if (volume && volume < VOL_THRES) {
				volume = VOL_THRES;
			}
			delta <<= 1;
		}
		m62332_senddata(0xff * volume / 100, M62332_EVR_CH);
		collie_volume = volume;
		udelay(100);
	}
	LEAVE(TRACE_VOLUME, "Collie_volume_half_off");
}

static void Collie_OP_SHDN_on(void)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_OP_SHDN,"Collie_OP_SHDN_on");
	if (!collie_op_shdn_on) {
		/* set volume */
		Collie_volume_off();

		/* OP_SHDN to GPIO 17 */
		/* alternate functions for the GPIOs */
		GAFR &= ~( COLLIE_GPIO_OPSHDN );
	
		/* Set the direction: 17 output */
		GPDR |= ( COLLIE_GPIO_OPSHDN );
	
		/* Set pin level (high) */
		GPSR |= ( COLLIE_GPIO_OPSHDN );
		
		/* set volume */
		Collie_volume_on();

		collie_op_shdn_on = 1;
	}
	LEAVE(TRACE_OP_SHDN,"Collie_OP_SHDN_on");
#endif	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

static void Collie_OP_SHDN_off(void)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_OP_SHDN,"Collie_OP_SHDN_off");
	/* OP_SHDN to GPIO 17 */
	/* alternate functions for the GPIOs */
	GAFR &= ~( COLLIE_GPIO_OPSHDN );
	
	/* Set the direction: 17 output */
	GPDR |= ( COLLIE_GPIO_OPSHDN );
	
	/* Clear pin level (low) */
	GPCR |= ( COLLIE_GPIO_OPSHDN );

	collie_op_shdn_on = 0;
	LEAVE(TRACE_OP_SHDN,"Collie_OP_SHDN_off");
#endif	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

static inline void Collie_ssp_init(void)
{
	ENTER(TRACE_SSP, "Collie_ssp_init");
	/* alternate functions for the GPIOs */
	/* SSP port to GPIO 10,12,13, 19 */
	GAFR |= ( GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM | GPIO_SSP_CLK );
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	/* SSP port to GPIO 11 */
	GAFR |= GPIO_SSP_RXD;
#endif	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */

	/* Set the direction: 10, 12, 13 output; 19 input */
	GPDR |= ( GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM );
	GPDR &= ~( GPIO_SSP_CLK );
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	/* Set the direction: 11 input */
	GPDR &= ~( GPIO_SSP_RXD );
#endif	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */

	/* enable SSP pin swap */
	PPAR |= PPAR_SPR;

	/* init SSP port */
	/* FIXME: disable MCP not necessary! */
	Ser4MCCR0 &= ~MCCR0_MCE;

	/* the SSP setting */
	Ser4SSCR0 = (SSCR0_DataSize(16) | SSCR0_TI | SSCR0_SerClkDiv(2));
	Ser4SSCR1 = (SSCR1_SClkIactL | SSCR1_SClk1P | SSCR1_ExtClk);

	LEAVE(TRACE_SSP, "Collie_ssp_init");
}

static inline void Collie_ssp_on(void)
{
	ENTER(TRACE_SSP, "Collie_ssp_on");
	/* turn on the SSP */
	Ser4SSCR0 |= ( SSCR0_SSE );
	LEAVE(TRACE_SSP, "Collie_ssp_on");
}

static inline void Collie_ssp_off(void)
{
	ENTER(TRACE_SSP, "Collie_ssp_off");
	/* turn off the SSP */
	Ser4SSCR0 &= ~( SSCR0_SSE );
	LEAVE(TRACE_SSP, "Collie_ssp_off");
}

static inline void Collie_sound_hard_init(void)
{
	ENTER(TRACE_ON, "Collie_sound_hard_init");
	Collie_hard_mute_init();
	Collie_audio_clock_init();
	Collie_paif_init();
	Collie_volume_init();
	Collie_ssp_init();
	Collie_MIC_init();

	Collie_FS8KLPF_start();
	LEAVE(TRACE_ON, "Collie_sound_hard_init");
}

static inline void Collie_sound_hard_term(void)
{
	ENTER(TRACE_ON, "Collie_sound_hard_term");
#ifdef DAC_OFF_WITH_DEVICE_OFF
	/* DAC Off */
	Collie_DAC_off();
#endif
	LEAVE(TRACE_ON, "Collie_sound_hard_term");
}

static void Collie_FS8KLPF_start(void)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	ENTER(TRACE_ON,"Collie_FS8KLPF_start");
	/* Set up TC35143 GPIO I/O Direction (GPIO5 output mode) */
	ucb1200_set_io_direction(TC35143_GPIO_FS8KLPF,
				 TC35143_IODIR_OUTPUT);
	/* Set up TC35143 GPIO 5 (set LOW) */
	ucb1200_set_io(TC35143_GPIO_FS8KLPF, TC35143_IODAT_LOW);
	LEAVE(TRACE_ON,"Collie_FS8KLPF_start");
#endif	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

#ifdef CONFIG_PM
#if 0
static void Collie_FS8KLPF_stop(void)
{
	/* Set up TC35143 GPIO I/O Direction (GPIO5 output mode) */
	ucb1200_set_io_direction(TC35143_GPIO_FS8KLPF,
				 TC35143_IODIR_OUTPUT);
	/* Set up TC35143 GPIO 5 (set LOW) */
	ucb1200_set_io(TC35143_GPIO_FS8KLPF, TC35143_IODAT_HIGH);
}

static void Collie_clock_stop(void)
{
	/* LoCoMo PCM audio interface */
	Collie_paif_off();

	/* LoCoMo audio clock off */
	Collie_audio_clock_off();
}
#endif
#endif

#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.19 */
static unsigned long in_timehandle = 0;
static struct timer_list timer;

static void collieDoDelayedSilence(void)
{
	ENTER(TRACE_ON,"collieDoDelayedSilence");
	down(&df_sem);
	if(isDelayed(DELAY_HARD_MUTE_ON)) {
		Collie_hard_mute_on();
	}
	if(isDelayed(DELAY_DAC_OFF)) {
		Collie_DAC_off();
	}
	ResetDelayAll();
	up(&df_sem);
	LEAVE(TRACE_ON,"colliDoDelayedSilence");
}

static void collieDelayedSilence(void)
{
	ENTER(TRACE_ON,"collieDelayedSilence");
	while (1) {
		sleep_on(&delay_off);
		collieDoDelayedSilence();
	}
	LEAVE(TRACE_ON,"collieDelayedSilence");
}

static void collieStartDelayedSilence(unsigned long data)
{
	ENTER(TRACE_ON,"collieStartDelayedSilence");
	in_timehandle = 0;
	wake_up(&delay_off);
	LEAVE(TRACE_ON,"collieStartDelayedSilence");
}

static void collieTriggerDelayedSilence(void)
{
	ENTER(TRACE_ON,"collieTriggerDelayedSilence");
	in_timehandle = 1;
	init_timer(&timer);
	timer.function = collieStartDelayedSilence;
	timer.expires = jiffies + 5*100;
	add_timer(&timer);
	LEAVE(TRACE_ON,"collieTriggerDelayedSilence");
}

static void collieCancelDelayedSilence(void)
{
	ENTER(TRACE_ON,"collieCancelDelayedSilence");
	down(&df_sem);
	ResetDelayAll();;
	up(&df_sem);
	if (in_timehandle) {
		del_timer(&timer);
		in_timehandle = 0;
	}
	LEAVE(TRACE_ON,"collieCancelDelayedSilence");
}
#endif

static void Collie_disable_sound(void)
{
	ENTER(TRACE_ON,"Collie_disable_sound");
	sa1100_dma_stop(COLLIE_SOUND_DMA_CHANNEL);
	sa1100_dma_flush_all(COLLIE_SOUND_DMA_CHANNEL);
#ifndef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.18 */
	Collie_volume_half_off();
	Collie_hard_mute_on();
	Collie_soft_mute_on();

	Collie_ssp_off();

	/* Collie_clock_stop(); */
#endif
	LEAVE(TRACE_ON,"Collie_disable_sound");
}

static void CollieSilence(void)
{
	ENTER(TRACE_ON,"CollieSilence");
	/* Disable sound & DMA */
	Collie_disable_sound();

#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.18 */
	Collie_volume_half_off();
	Collie_hard_mute_delay_on();
	Collie_soft_mute_on();

	Collie_ssp_off();

	/* Collie_clock_stop(); */
#endif

#if 0	/* H.Hayami SHARP 2001.12.18 */
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0) && \
    !defined(CONFIG_COLLIE_TR1) && !defined(CONFIG_COLLIE_DEV)
	Collie_volume_off();
#endif
#endif
	Collie_OP_SHDN_off();
	Collie_soft_DAC_off();
	Collie_paif_off();
	Collie_audio_clock_off();

	//Collie_MIC_on();

#ifndef DAC_OFF_WITH_DEVICE_OFF
	/* DAC Off */
#ifdef TRY_DELAY_OFF	/* H.Hayami 2001.12.15 */
	Collie_DAC_delay_off();
#else
	Collie_DAC_off();
#endif
#endif	/* end DAC_OFF_WITH_DEVICE_OFF */

	/* AMP Off */
	Collie_AMP_off();

#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.18 */
	collieTriggerDelayedSilence();
#endif
	LEAVE(TRACE_ON,"CollieSilence");
}

static int CollieGetSamp(void)
{
	ENTER(TRACE_ON,"CollieGetSamp");
	switch (sound.soft.speed) {
	case 8000:
		LEAVE(TRACE_ON,"CollieGetSamp");
	  	return clock_set_data[7];
	case 44100:
		LEAVE(TRACE_ON,"CollieGetSamp");
	  	return clock_set_data[0];
	case 22050:
		LEAVE(TRACE_ON,"CollieGetSamp");
	  	return clock_set_data[1];
	case 11025:
		LEAVE(TRACE_ON,"CollieGetSamp");
	  	return clock_set_data[2];
	case 48000:
		LEAVE(TRACE_ON,"CollieGetSamp");
	  	return clock_set_data[3];
	case 32000:
		LEAVE(TRACE_ON,"CollieGetSamp");
	  	return clock_set_data[4];
	case 24000:
		LEAVE(TRACE_ON,"CollieGetSamp");
	  	return clock_set_data[5];
	case 16000:
		LEAVE(TRACE_ON,"CollieGetSamp");
	  	return clock_set_data[6];
	default:
		printk("Collie sound: Illegal sound rate %d\n", sound.soft.speed);
		LEAVE(TRACE_ON,"CollieGetSamp");
		return clock_set_data[7];
	}
}

static inline void Collie_DAC_sendbit(int bit_data)
{
	ENTER(TRACE_SENDDATA,"Collie_DAC_sendbit");
	if (bit_data & 1) {
		LCM_GPO |=  (LCM_GPIO_DAC_SDATA);
	} else {
		LCM_GPO &= ~(LCM_GPIO_DAC_SDATA);
	}

	udelay(1);
	LCM_GPO |=  (LCM_GPIO_DAC_SCK);

	udelay(1);
	LCM_GPO &= ~(LCM_GPIO_DAC_SCK);
	udelay(1);
	LCM_GPO &= ~(LCM_GPIO_DAC_SDATA);
	udelay(1);
	LEAVE(TRACE_SENDDATA,"Collie_DAC_sendbit");
}

static void Collie_DAC_sendword(int data)
{
	int i;

	ENTER(TRACE_SENDDATA,"Collie_DAC_sendword");
#if defined(CONFIG_COLLIE_PCM1741)
	
	LCM_GPO &= ~(LCM_GPIO_DAC_SCK);
	udelay(1);
	LCM_GPO |=  (LCM_GPIO_DAC_SLOAD);
	udelay(1);

	for (i = 0; i < 16; i++)
		Collie_DAC_sendbit(data >> (15 - i));

	LCM_GPO &= ~(LCM_GPIO_DAC_SLOAD);
	udelay(2);

#elif defined(CONFIG_COLLIE_PCM1717)
	
	LCM_GPO &= ~(LCM_GPIO_DAC_SLOAD);
	udelay(1000);
	LCM_GPO |=  (LCM_GPIO_DAC_SLOAD);
	udelay(1000);
	LCM_GPO &= ~(LCM_GPIO_DAC_SCK);
	udelay(1000);

	for (i = 0; i < 16; i++)
		Collie_DAC_sendbit(data >> (15 - i));

	LCM_GPO &= ~(LCM_GPIO_DAC_SLOAD);
	udelay(1000);
	LCM_GPO |=  (LCM_GPIO_DAC_SLOAD);
	udelay(1000);
	LCM_GPO &= ~(LCM_GPIO_DAC_SLOAD);
	udelay(1000);
	
#endif
	LEAVE(TRACE_SENDDATA,"Collie_DAC_sendword");
}

void
Collie_audio_power_on(void)
{
	int send_data;

	ENTER(TRACE_ON,"Collie_audio_power_on");

#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.19 */
	collieCancelDelayedSilence();
#endif

	collie_amp_init = 0;

	/* OP_SHDN off */
	Collie_OP_SHDN_off();

	/* AMP on */
	Collie_AMP_on();

	/* DAC ON */
	Collie_DAC_on();

	Collie_MIC_off();
	Collie_ssp_on();
	
	/* LoCoMo Audio clock */
	Collie_audio_clock_off();
	Collie_audio_clock_on();

	/* LoCoMo PCM audio interface */
	Collie_paif_on();

	udelay(1000);

	/* DAC Setting */
	Collie_DAC_init();

	Collie_soft_DAC_on();
	Collie_soft_mute_init();

	sound.hard = sound.soft;

#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0) && \
    !defined(CONFIG_COLLIE_TR1) && !defined(CONFIG_COLLIE_DEV)
	/* Volume set */
	Collie_volume_half_adjust();
	{
		int i;
		for (i=0; i<10*1000; i++) {
			udelay(1);
		}
	}
#endif

	Collie_soft_mute_off();
	Collie_hard_mute_off();

	LEAVE(TRACE_ON,"Collie_audio_power_on");
}

static void CollieInit(void)
{
	ENTER(TRACE_ON,"CollieInit");
	sound.hard = sound.soft;
	LEAVE(TRACE_ON,"CollieInit");
}

static void Collie_sq_interrupt(void* id, int size)
{
	audio_buf_t *b = (audio_buf_t *) id;
	ENTER(TRACE_INTERRUPT,"Collie_sq_interrupt");
/***** DEBUG *****
printk("Collie_sq_interrupt: Start\n");
*****************/
	/*
	 * Current buffer is sent: wake up any process waiting for it.
	 */
	ENTER(TRACE_SEM,"up sem");
	up(&b->sem);
	LEAVE(TRACE_SEM,"up sem");
/***** DEBUG *****
printk("Collie_sq_interrupt: up End\n");
*****************/
	/* And any process polling on write. */
	ENTER(TRACE_SEM,"up wait");
	wake_up(&b->sem.wait);
	LEAVE(TRACE_SEM,"up wait");
/***** DEBUG *****
printk("Collie_sq_interrupt: wake_up End\n");
*****************/

	DPRINTK("Collie_sq_interrupt \n");
	LEAVE(TRACE_INTERRUPT,"Collie_sq_interrupt");
}


static int __init CollieIrqInit(void)
{
	int err;

	ENTER(TRACE_ON,"CollieIrqInit");
	err = sa1100_request_dma(&collie_dmasound_irq, "dmasound",
				 DMA_Ser4SSPWr);
	if (err) {
		LEAVE(TRACE_ON,"CollieIrqInit");
		return 0;
	}
	/* printk("collie_dmasound_irq=%d\n", collie_dmasound_irq); */

	sa1100_dma_set_callback(collie_dmasound_irq,
			       (dma_callback_t)Collie_sq_interrupt);


	/* Disable sound & DMA */
	Collie_disable_sound();

	LEAVE(TRACE_ON,"CollieIrqInit");
	return(1);

}

#ifdef MODULE
static void CollieIrqCleanUp(void)
{
	ENTER(TRACE_ON,"CollieIrqCleanUp");
	/* Disable sound & DMA */
	Collie_disable_sound();

	/* release the interrupt */
	free_irq(IRQ_DMA, Collie_sq_interrupt);
	LEAVE(TRACE_ON,"CollieIrqCleanUp");
}
#endif /* MODULE */


static int CollieSetFormat(int format)
{
	int size;

	ENTER(TRACE_ON,"CollieSetFormat");
	/* Falcon sound DMA supports 8bit and 16bit modes */

	switch (format) {
	case AFMT_QUERY:
		LEAVE(TRACE_ON,"CollieSetFormat");
		return(sound.soft.format);
	case AFMT_MU_LAW:
		size = 8;
		ct_func = sound.trans->ct_ulaw;
		break;
	case AFMT_A_LAW:
		size = 8;
		ct_func = sound.trans->ct_alaw;
		break;
	case AFMT_S8:
		size = 8;
		ct_func = sound.trans->ct_s8;
		break;
	case AFMT_U8:
		size = 8;
		ct_func = sound.trans->ct_u8;
		break;
	case AFMT_S16_BE:
		size = 16;
		ct_func = sound.trans->ct_s16be;
		break;
	case AFMT_U16_BE:
		size = 16;
		ct_func = sound.trans->ct_u16be;
		break;
	case AFMT_S16_LE:
		size = 16;
		ct_func = sound.trans->ct_s16le;
		break;
	case AFMT_U16_LE:
		size = 16;
		ct_func = sound.trans->ct_u16le;
		break;
	default: /* :-) */
		size = 8;
		format = AFMT_S8;
	}

	sound.soft.format = format;
	sound.soft.size = size;
	if (sound.minDev == SND_DEV_DSP) {
		sound.dsp.format = format;
		sound.dsp.size = sound.soft.size;
	}

	LEAVE(TRACE_ON,"CollieSetFormat");
	return(format);
}

/*** Machine definitions *****************************************************/

static MACHINE machCollie = {
	DMASND_COLLIE,		// int type
	NULL,			// void *dma_alloc(uint, int)
	NULL,			// void  dma_free(void *, uint)
	CollieIrqInit,		// void  irqinit(void)
#ifdef MODULE
	CollieIrqCleanUp,	// void  irqcleanup(void)
#endif /* MODULE */
	CollieInit,		// void  init(void)
	CollieSilence,		// void  silence(void)
	CollieSetFormat,	// int   setFormat(int)
	NULL,			// int   setVolume(int)
	NULL,			// int   setBass(int)
	NULL,			// int   setTreble(int)
	NULL,			// int   setGain(int)
	NULL			// void  play(void)
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
	Collie_audio_clock_off();
	Collie_audio_clock_on();

	LEAVE(TRACE_ON,"sound_set_speed");
	return(sound.soft.speed);
}


static int sound_set_stereo(int stereo)
{
	ENTER(TRACE_ON,"sound_set_stereo");
	if (stereo < 0) {
		LEAVE(TRACE_ON,"sound_set_stereo");
		return(sound.soft.stereo);
	}

	stereo = !!stereo;    /* should be 0 or 1 now */

	sound.soft.stereo = stereo;
	if (sound.minDev == SND_DEV_DSP)
		sound.dsp.stereo = stereo;
	//(*sound.mach.init)();

	LEAVE(TRACE_ON,"sound_set_stereo");
	return(stereo);
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

	ENTER(TRACE_ON,"mixer_ioctl");
	switch (sound.mach.type) {
	case DMASND_COLLIE:
	{
		switch (cmd) {
			case SOUND_MIXER_READ_DEVMASK:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, SOUND_MASK_VOLUME );
			case SOUND_MIXER_READ_RECMASK:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, 0);
			case SOUND_MIXER_READ_STEREODEVS:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, SOUND_MASK_VOLUME);
			case SOUND_MIXER_READ_CAPS:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, 0);

			case SOUND_MIXER_WRITE_VOLUME:
				IOCTL_IN(arg, data);
				Collie_Set_Volume(data);
			case SOUND_MIXER_READ_VOLUME:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, Collie_Get_Volume());

			case SOUND_MIXER_READ_TREBLE:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, 0);
			case SOUND_MIXER_WRITE_TREBLE:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, 0);

			case SOUND_MIXER_WRITE_MIC:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, 0);
			case SOUND_MIXER_READ_MIC:
				LEAVE(TRACE_ON,"mixer_ioctl");
				return IOCTL_OUT(arg, 0);

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
		case DMASND_COLLIE:
		  //			sound.volume_left  = 0x3c;
		  //			sound.volume_right = 0x3c;
		  	sound.volume_left  = 80;
		  	sound.volume_right = 80;
			collie_main_volume = sound.volume_left;
			break;
	}

	//	printk("mixer_init : ret \n");

	LEAVE(TRACE_ON,"mixer_init");
}

/* This function allocates the buffer structure array and buffer data space
 * according to the current number of fragments and fragment size.
 */

static int sq_allocate_buffers(audio_stream_t * s)
{
	int frag;
	int dmasize = 0;
	char *dmabuf = 0;
	dma_addr_t dmaphys = 0;

	ENTER(TRACE_ON,"sq_allocate_buffers");
	DPRINTK("sq_allocate_buffers\n");

	if (s->buffers) {
		LEAVE(TRACE_ON,"sq_allocate_buffers");
		return -EBUSY;
	}

	s->nbfrags = audio_nbfrags;
	s->fragsize = audio_fragsize;

	s->buffers = (audio_buf_t *)
		kmalloc(sizeof(audio_buf_t) * s->nbfrags, GFP_KERNEL);
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
			b->master = dmasize;
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

	LEAVE(TRACE_ON,"sq_allocate_buffers");
	return 0;

err:
	printk("sound driver : unable to allocate audio memory\n ");
	sq_release_buffers(s);
	LEAVE(TRACE_ON,"sq_allocate_buffers");
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
	sa1100_dma_flush_all(COLLIE_SOUND_DMA_CHANNEL);

	if (s->buffers) {
		int frag;
		for (frag = 0; frag < s->nbfrags; frag++) {
			if (!s->buffers[frag].master)
				continue;
			consistent_free(s->buffers[frag].start,
					s->buffers[frag].master,
					s->buffers[frag].dma_addr);
		}
		kfree(s->buffers);
		s->buffers = NULL;
	}

	s->buf_idx = 0;
	s->buf = NULL;
	LEAVE(TRACE_ON,"sq_release_buffers");
}

static ssize_t sq_write(struct file *file, const char *src, size_t uLeft,
			loff_t *ppos)
{
	const char *buffer0 = src;
	audio_stream_t *s = &output_stream;
	u_char *dest;
	ssize_t uUsed, bUsed, bLeft, ret = 0;

	ENTER(TRACE_WRITE,"sq_write");
	DPRINTK("sq_write: uLeft=%d\n", uLeft);

	/* OP_SHDN on */
	Collie_OP_SHDN_on();

	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		break;
	default: 
		LEAVE(TRACE_WRITE,"sq_write1");
		return -EPERM;
	} 

	if (!s->buffers && sq_allocate_buffers(s)) {
		LEAVE(TRACE_WRITE,"sq_write2");
		return -ENOMEM;
	}

#if 1
	if (collie_resume == 1) {
		int	i;
		collie_resume = 0;
		for (i=0; i<1000*1000; i++) {
			udelay(1);
		}
	}
#endif
#ifdef CONFIG_PM
	/* Auto Power off cancel */
	autoPowerCancel = 0;
#endif

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
			//printk("### 0x%08x:%d\n", &b->sem.count, atomic_read(&b->sem.count));
			if (down_interruptible(&b->sem)) {
				LEAVE(TRACE_SEM,"down_int1 sem");
				break;
			}
			LEAVE(TRACE_SEM,"down_int2 sem");
		}

		dest = b->start + b->size;
		bUsed = 0;
		bLeft = s->fragsize - b->size;

		if (ct_func) {
			uUsed = ct_func(src, uLeft, dest, &bUsed, bLeft);
			cpu_cache_clean_invalidate_range((unsigned long)dest,
				(unsigned long)(dest+(audio_fragsize)), 0);
		} else {
			LEAVE(TRACE_WRITE,"sq_write3");
			return -EFAULT;
		}

		if (uUsed < 0) {
			ENTER(TRACE_SEM,"up sem");
			up(&b->sem);
			LEAVE(TRACE_SEM,"up sem");
			LEAVE(TRACE_WRITE,"sq_write4");
			return -EFAULT;
		}
		src += uUsed;
		uLeft -= uUsed;
		b->size += bUsed;

		if (b->size < s->fragsize) {
			ENTER(TRACE_SEM,"up sem");
			up(&b->sem);
			LEAVE(TRACE_SEM,"up sem");
			break;
		}

		/* Send current buffer to dma */
		sa1100_dma_queue_buffer(COLLIE_SOUND_DMA_CHANNEL,
		 			(void *) b, b->dma_addr, b->size);

		Collie_volume_half_adjust();

		b->size = 0;    /* indicate that the buffer has been sent */
		NEXT_BUF(s, buf);
	}

	if ((src - buffer0))
		ret = src - buffer0;
	DPRINTK("sq_write: return=%d\n", ret);
	LEAVE(TRACE_WRITE,"sq_write0");
	return ret;
}

static unsigned int sq_poll(struct file *file, 
			       struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	int i;

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
			if (atomic_read(&output_stream.buffers[i].sem.count) > 0)
				mask |= POLLOUT | POLLWRNORM;
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
	ENTER(TRACE_ON,"sq_open");
	DPRINTK("sq_open\n");

	if (((file->f_flags & O_ACCMODE) == O_WRONLY)
	 || ((file->f_flags & O_ACCMODE) == O_RDWR)) {
#if 0
		MOD_INC_USE_COUNT;
		while(audio_wr_refcount) {
			SLEEP(open_queue, ONE_SECOND);
			if (SIGNAL_RECEIVED) {
				MOD_DEC_USE_COUNT;
				LEAVE(TRACE_ON,"sq_open");
				return -EINTR;
			}
		}
#else
		if (audio_wr_refcount) {
			DPRINTK(" sq_open        EBUSY\n");
			LEAVE(TRACE_ON,"sq_open");
			return -EBUSY;
		}
		MOD_INC_USE_COUNT;
#endif
		audio_wr_refcount++;
	} else {
		DPRINTK(" sq_open        EINVAL\n");
		LEAVE(TRACE_ON,"sq_open");
		return -EINVAL;
	}

	if (audio_wr_refcount == 1) {
		DPRINTK("cold\n");

		audio_fragsize = AUDIO_FRAGSIZE_DEFAULT;
		audio_nbfrags = AUDIO_NBFRAGS_DEFAULT;
		sq_release_buffers(&output_stream);
		sound.minDev = MINOR(inode->i_rdev) & 0x0f;
		sound.soft = sound.dsp;
		sound.hard = sound.dsp;

		if ((MINOR(inode->i_rdev) & 0x0f) == SND_DEV_AUDIO) {
			sound_set_speed(8000);
			sound_set_stereo(0);
			sound_set_format(AFMT_MU_LAW);
		}
		Collie_audio_power_on();
	}

#if 0
	MOD_INC_USE_COUNT;
#endif
	LEAVE(TRACE_ON,"sq_open");
	return 0;
}

static int sq_fsync(struct file *filp, struct dentry *dentry)
{
	audio_stream_t *s = &output_stream;
	audio_buf_t *b = s->buf;

	ENTER(TRACE_ON,"sq_fsync");
	DPRINTK("sq_fsync\n");

	if (!s->buffers) {
		LEAVE(TRACE_ON,"sq_fsync");
		return 0;
	}

	/* Send half-full buffers */
	if (b->size != 0) {
		DPRINTK("half-full_buf\n");

#ifdef CONFIG_PM
		/* Auto Power off cancel */
		autoPowerCancel = 0;
#endif

		ENTER(TRACE_SEM,"down sem");
		down(&b->sem);
		LEAVE(TRACE_SEM,"down sem");
		sa1100_dma_queue_buffer(COLLIE_SOUND_DMA_CHANNEL,
					(void *) b, b->dma_addr, b->size);
		b->size = 0;
		NEXT_BUF(s, buf);
	}

	/*
	 * Let's wait for the last buffer we sent i.e. the one before the
	 * current buf_idx.  When we acquire the semaphore, this means either:
	 * - DMA on the buffer completed or
	 * - the buffer was already free thus nothing else to sync.
	 */
	b = s->buffers + ((s->nbfrags + s->buf_idx - 1) % s->nbfrags);
	ENTER(TRACE_SEM,"down-int sem");
	if (down_interruptible(&b->sem)) {
		LEAVE(TRACE_SEM,"down-int sem");
		LEAVE(TRACE_ON,"sq_fsync");
		return -EINTR;
	}
	LEAVE(TRACE_SEM,"down-int sem");
	ENTER(TRACE_SEM,"up sem");
	up(&b->sem);
	LEAVE(TRACE_SEM,"up sem");
	LEAVE(TRACE_ON,"sq_fsync");
	return 0;
}

static int sq_release(struct inode *inode, struct file *file)
{
	ENTER(TRACE_ON,"sq_release");
	DPRINTK("sq_release\n");

	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		if (audio_wr_refcount == 1) {
			sq_fsync(file, file->f_dentry);
			sq_release_buffers(&output_stream);
			audio_wr_refcount = 0;
		}
	}       

	if (!audio_wr_refcount) {
		sound.soft = sound.dsp;
		sound.hard = sound.dsp;
		sound_silence();
		Collie_OP_SHDN_off();
	} 

#if 0
	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		if (!audio_wr_refcount) {
			WAKE_UP(open_queue);
		}
	}
#endif

	MOD_DEC_USE_COUNT;
	LEAVE(TRACE_ON,"sq_release");
	return 0;
}


static int sq_ioctl(struct inode *inode, struct file *file, u_int cmd,
		    u_long arg)
{
	u_long fmt;
	int data;
	long val;

	ENTER(TRACE_ON,"sq_ioctl");
	switch (cmd) {
	case SNDCTL_DSP_RESET:
		switch (file->f_flags & O_ACCMODE) {
		case O_WRONLY:
		case O_RDWR:
			sq_release_buffers(&output_stream);
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
	case SOUND_PCM_WRITE_CHANNELS:
		sq_fsync(file, file->f_dentry);
		IOCTL_IN(arg, data);
		LEAVE(TRACE_ON,"sq_ioctl");
		return IOCTL_OUT(arg, sound_set_stereo(data-1)+1);
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

#if 1 // 2003.2.14
	case SNDCTL_DSP_GETOSPACE:
	    {
		audio_buf_info inf = { 0, };
		int i;

		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		if (!output_stream.buffers && sq_allocate_buffers(&output_stream))
			return -ENOMEM;
		for (i = 0; i < output_stream.nbfrags; i++) {
			if (atomic_read(&output_stream.buffers[i].sem.count) > 0) {
				if (output_stream.buffers[i].size == 0)
					inf.fragments++;
				inf.bytes += output_stream.fragsize - output_stream.buffers[i].size;
			}
		}
		inf.fragstotal = output_stream.nbfrags;
		inf.fragsize = output_stream.fragsize;
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }
#endif


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
		case DMASND_COLLIE:
		  	sound.dsp.speed = 8000;
			break;
	}

	/* before the first open to /dev/dsp this wouldn't be set */
	sound.soft = sound.dsp;
	sound.hard = sound.dsp;
	sound.trans = &transCollie;

	CollieSetFormat(sound.dsp.format);
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

	len += sprintf(buffer+len, "  COLLIE DMA sound driver:\n");

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
	ENTER(TRACE_ON,"state_unit");
	state_unit = register_sound_special(&state_fops, SND_DEV_STATUS);
	if (state_unit < 0) {
		LEAVE(TRACE_ON,"state_unit");
		return;
	}
	state.busy = 0;

	//	printk("state_init : ret \n");
	LEAVE(TRACE_ON,"state_unit");

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
static int collie_sound_pm_callback(struct pm_dev *pm_dev,
				    pm_request_t req, void *data)
{
	ENTER(TRACE_PM,"collie_sound_pm_callback");
	switch (req) {
	case PM_SUSPEND:
#ifdef COLLIE_TRY_ONE
		disable_irq(IRQ_GPIO_nREMOCON_INT);
#endif
#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.19 */
		collieDoDelayedSilence();
		collieCancelDelayedSilence();
#endif
		if (audio_wr_refcount == 1) {
			Collie_volume_off();
			Collie_hard_mute_on();
			Collie_soft_mute_on();
			sa1100_dma_sleep((dmach_t)COLLIE_SOUND_DMA_CHANNEL);

#if 0	/* H.Hayami SHARP 2001.12.18 */
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0) && \
    !defined(CONFIG_COLLIE_TR1) && !defined(CONFIG_COLLIE_DEV)
			Collie_volume_off();
#endif
#endif
			Collie_OP_SHDN_off();
			Collie_soft_DAC_off();
			Collie_paif_off();
			Collie_audio_clock_off();
			//Collie_MIC_on();
			Collie_DAC_off();
			Collie_AMP_off();
		} else {
			sa1100_dma_sleep((dmach_t)COLLIE_SOUND_DMA_CHANNEL);
		}
		Collie_sound_hard_term();
		break;
	case PM_RESUME:
/***** DEBUG *****g
printk("collie_sound_pm_callback: audio_wr_refcount=%d\n", audio_wr_refcount);
*****************/
#ifdef COLLIE_TRY_ONE
		enable_irq(IRQ_GPIO_nREMOCON_INT);
#endif

		Collie_sound_hard_init();

		if (audio_wr_refcount == 1) {
			collie_resume = 1;

			Collie_audio_power_on();

			sa1100_dma_wakeup((dmach_t)COLLIE_SOUND_DMA_CHANNEL);
#if 0	/* H.Hayami SHARP 2001.12.18 */
			Collie_soft_mute_off();
			Collie_hard_mute_off();
#endif
		} else {
#ifdef COLLIE_TRY_ONE
		  if ( !( GPLR & GPIO_nREMOCON_INT ) )
		    Collie_DAC_on();
#endif
			sa1100_dma_wakeup((dmach_t)COLLIE_SOUND_DMA_CHANNEL);
		}


		break;

	}
	LEAVE(TRACE_PM,"collie_sound_pm_callback");
	return 0;
}
#endif


#ifdef COLLIE_TRY_ONE

static void collie_audio_on(void)
{
    while(1) {
      sleep_on(&audio_on);

      /* DAC ON */
      Collie_DAC_on();
      //      printk("call audio on \n");

    }
}

void Collie_rc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  //  printk("int !\n");
  wake_up(&audio_on);
}
#endif

/*** Config & Setup ******************************************************/

int __init Collie_sound_init(void)
{
	int has_sound = 0;


	ENTER(TRACE_ON,"Collie_sound_init");
	has_sound = 1;
	sound.mach = machCollie;

	if (!has_sound) {
		LEAVE(TRACE_ON,"Collie_sound_init");
		return -1;
	}

#ifdef TRY_DELAY_OFF	/* H.Hayami SHARP 2001.12.19 */
	sema_init(&df_sem, 1);
	kernel_thread(collieDelayedSilence, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
#endif

	Collie_sound_hard_init();

	if (!sound.mach.irqinit()) {
		printk("Sound driver: Interrupt initialization failed\n");
		LEAVE(TRACE_ON,"Collie_sound_init");
		return -1;
	}

	/* Set up sound queue, /dev/audio and /dev/dsp. */

	/* Set default settings. */
	sq_init();

	/* Set up /dev/sndstat. */
	state_init();

	/* Set up /dev/mixer. */
	mixer_init();

#ifdef MODULE
	irq_installed = 1;
#endif

	printk("Collie Sound Driver Installed\n");

#ifdef CONFIG_PM
	collie_sound_pm_dev = pm_register(PM_SYS_DEV, 0,
						collie_sound_pm_callback);
#endif


#ifdef COLLIE_TRY_ONE
	/* enable int sw */
	collie_rc_set_int_mode();

	/* GPIO15(int):IN, GPIO18(sw):OUT */
	GPDR = ((GPDR)&~GPIO_nREMOCON_INT)|GPIO_REMOCON_ADC_SW;

	/* GPIO15,18:not Alternate */
	GAFR &= ~(GPIO_nREMOCON_INT|GPIO_REMOCON_ADC_SW);

	/* GPIO15:Falling Edge */
	set_GPIO_IRQ_edge(GPIO_nREMOCON_INT, GPIO_FALLING_EDGE);

	/* Register interrupt handler */
	if ( request_irq(IRQ_GPIO_nREMOCON_INT, Collie_rc_interrupt,
			       SA_INTERRUPT, "INSERT-HEADPHONE", Collie_rc_interrupt))	{
		printk("%s: request_irq(%d) failed.\n",
			__FUNCTION__, IRQ_GPIO_nREMOCON_INT);
	}

	/* Make threads */
	kernel_thread(collie_audio_on,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
#endif


	LEAVE(TRACE_ON,"Collie_sound_init");
	return 0;
}

module_init(Collie_sound_init);


#ifdef MODULE

int init_module(void)
{
	ENTER(TRACE_ON,"init_module");
	Collie_sound_init();
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

	sq_release_buffers();

	if (mixer_unit >= 0)
		unregister_sound_mixer(mixer_unit);
	if (state_unit >= 0)
		unregister_sound_special(state_unit);
	if (sq_unit >= 0)
		unregister_sound_dsp(sq_unit);
	LEAVE(TRACE_ON,"cleanup_module");
}

#endif /* MODULE */
