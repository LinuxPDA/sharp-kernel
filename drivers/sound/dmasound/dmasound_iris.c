/*
 * linux/drivers/sound/dmasound/dmasound_iris.c
 *
 * L7205(L7210) DMA based 16bit DAC sound driver for IRIS (SHARP)
 *
 * Based on dmasound.c (AMIGA)
 *
 *		DAC : LC78817M (SANYO)
 *		I/F : Serial interface to codec (SIC) AIL mode
 *
 * (c) 2001 by K.Yamade (SHARP)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License Version2. See the file COPYING in the main directory of this archive
 * for more details.
 *
 * ChangeLog:
 *   03-06-2001 T.ASANO ...
 *   04-03-2001         dma workaround.
 *   04-16-2001         pm.
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/major.h>
#include <linux/config.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/sound.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>

#include <linux/soundcard.h>
#include <linux/pm.h>
#include <asm/dma.h>

#include <asm/arch/gpio.h>
#include <asm/arch/dmasound_iris.h>

#ifdef CONFIG_PM
static struct pm_dev *audio_pm_dev; /* registered PM device */
#endif

/* #undef CONFIG_DMA_L7200 */

#if defined(CONFIG_DMA_L7200)
int iris_dmasound_irq = -1;
#define	IRIS_SOUND_DMA_CHANNEL	(iris_dmasound_irq)
#else
#define	IRIS_DMA_CHANNEL0	0x00
#define	IRIS_SOUND_DMA_CHANNEL	IRIS_DMA_CHANNEL0
#endif

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
static char **sound_buffers = NULL;
static int **buf_flag = NULL;
static int **buf_size = NULL;

/*** Some declarations ***********************************************/
#define DMASND_TT	1
#define DMASND_FALCON	2
#define DMASND_AMIGA	3
#define DMASND_AWACS	4
#define DMASND_IRIS	5

#define MAX_CATCH_RADIUS	10
#define MIN_BUFFERS		4
#define MIN_BUFSIZE 		4
#define MAX_BUFSIZE		128	/* Limit for Amiga */

#define IRIS_WAIT_AMP_ON	1	/* 10ms */
#define BUFFER_EMPTY	((int*)0x0001)
#define BUFFER_FULL     ((int*)0x0002)
#ifdef MODULE
static int catchRadius = 0;
#endif
static int numBufs = 4, bufSize = 32;
static int iris_amp_init = 0;
static int iris_volume;
static DECLARE_WAIT_QUEUE_HEAD(write_queue);
static DECLARE_WAIT_QUEUE_HEAD(open_queue);
static DECLARE_WAIT_QUEUE_HEAD(sync_queue);

#ifdef MODULE
MODULE_PARM(catchRadius, "i");
#endif
MODULE_PARM(numBufs, "i");
MODULE_PARM(bufSize, "i");

#define arraysize(x)	(sizeof(x)/sizeof(*(x)))
#define min(x, y)	((x) < (y) ? (x) : (y))
#define le2be16(x)	(((x)<<8 & 0xff00) | ((x)>>8 & 0x00ff))
#define le2be16dbl(x)	(((x)<<8 & 0xff00ff00) | ((x)>>8 & 0x00ff00ff))

#define IOCTL_IN(arg, ret) \
	do { int error = get_user(ret, (int *)(arg)); \
		if (error) return error; \
	} while (0)
#define IOCTL_OUT(arg, ret)	ioctl_return((int *)(arg), ret)


/*** Some low level helpers **************************************************/

int	clock_set_data[10] = {	
	( SICSYN_SEL10 | SICDIV_SEL101 ) , ( SICSYN_SEL00 | SICDIV_SEL101 ) ,
	( SICSYN_SEL10 | SICDIV_SEL011 ) , ( SICSYN_SEL00 | SICDIV_SEL011 ) ,
	( SICSYN_SEL10 | SICDIV_SEL010 ) , ( SICSYN_SEL00 | SICDIV_SEL010 ) ,
	( SICSYN_SEL10 | SICDIV_SEL001 ) , ( SICSYN_SEL00 | SICDIV_SEL001 ) ,
	( SICSYN_SEL10 | SICDIV_SEL000 ) , ( SICSYN_SEL00 | SICDIV_SEL000 ) ,
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

static ssize_t iris_ct_law(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft);
static ssize_t iris_ct_s8(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft);
static ssize_t iris_ct_u8(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft);
static ssize_t iris_ct_s16(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft);
static ssize_t iris_ct_u16(const u_char *userPtr, size_t userCount,
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
#if defined(CONFIG_ATARI) || defined(CONFIG_PPC)
	int bal;		/* balance factor for expanding (not volume!) */
	u_long data;		/* data for expanding */
#endif /* CONFIG_ATARI */
};

static struct sound_settings sound;

static void Iris_Set_Volume(int volume);
static int Iris_Get_Volume(void);
#if !defined(CONFIG_DMA_L7200)
static void Iris_disable_dma(void);
static void Iris_enable_dma(void);
static void Iris_clear_irq(void);
static void Iris_disable_irq(void);
static void Iris_enable_irq(void);
#endif
static void Iris_disable_sound(void);
static void Iris_clock_stop(void);
static int IrisIrqInit(void);
static int IrisGetSamp(void);
#ifdef MODULE
static void IrisIrqCleanUp(void);
#endif /* MODULE */
static void IrisSilence(void);
static void IrisInit(void);
static int IrisSetFormat(int format);
static void Iris_sq_play_next_frame(void);
static void IrisPlay(void);
#if defined(CONFIG_DMA_L7200)
static void Iris_sq_interrupt(void* id, int size);
#else
static void Iris_sq_interrupt(int irq, void *dummy, struct pt_regs *fp);
#endif
static void *IrisMalloc(unsigned int size , int flags);
static void IrisFree(void *ptr , unsigned int size);

/*** Mid level stuff *********************************************************/
static void sound_silence(void);
static void sound_init(void);
static int sound_set_format(int format);
static int sound_set_speed(int speed);
static int sound_set_stereo(int stereo);

static ssize_t sound_copy_translate(const u_char *userPtr,
				    size_t userCount,
				    u_char frame[], ssize_t *frameUsed,
				    ssize_t frameLeft);


/*
 * /dev/mixer abstraction
 */

struct sound_mixer {
    int busy;
};

static struct sound_mixer mixer;

/*
 * Sound queue stuff, the heart of the driver
 */

struct sound_queue {
	int max_count, block_size;
	char **buffers;
	int max_active;

	/* it shouldn't be necessary to declare any of these volatile */
	int front, rear, count;
	int rear_size;
	int playing;
//	struct wait_queue *write_queue, *open_queue, *sync_queue;
	int open_mode;
	int busy, syncing;

};

static struct sound_queue sq;

#define sq_block_address(i)	(sq.buffers[i])
#define SIGNAL_RECEIVED	(signal_pending(current))
#define NON_BLOCKING(open_mode)	(open_mode & O_NONBLOCK)
#define ONE_SECOND	HZ	/* in jiffies (100ths of a second) */
#define NO_TIME_LIMIT	0xffffffff
#define SLEEP(queue, time_limit) \
	interruptible_sleep_on_timeout((wait_queue_head_t*)&queue, (time_limit));
#define WAKE_UP(queue)	(wake_up_interruptible((wait_queue_head_t*)&queue))

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
	if (value < 0)
		return(value);

	return put_user(value, addr)? -EFAULT: 0;
}


/*** Config & Setup **********************************************************/


void dmasound_init(void);
void dmasound_setup(char *str, int *ints);

#define IRIS_STEREO_AUDIO
#define USE_DAC_AS_BUZZER /* Buzzer and Earphone uses DAC commonly. */

#ifdef IRIS_STEREO_AUDIO
int iris_ctrl_buzzer_sw = 0;
#endif /* IRIS_STEREO_AUDIO */


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

static ssize_t iris_ct_law(const u_char *userPtr, size_t userCount,
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
		if (get_user(data, userPtr++))
			return -EFAULT;
#ifndef IRIS_STEREO_AUDIO
		val = table[data];
		if (stereo) {
			if (get_user(data, userPtr++))
				return -EFAULT;
			val += table[data];
			val >>= 1;	/* take average of R & L */
		}
		*p++ = ( val >> ( 8 - iris_volume ) );	/* Left Ch. */
		*p++ = 0;				/* Right Ch. */
#else /* ! IRIS_STEREO_AUDIO */
		val = table[data];
		*p++ = ( val >> ( 8 - iris_volume ) );	/* Left Ch. */
		if (stereo) {
			if (get_user(data, userPtr++))
				return -EFAULT;
			val = table[data];
		}


		if( iris_ctrl_buzzer_sw ){
		  *p++ = 0;	/* Right Ch. */
		}else{
		  *p++ = ( val >> ( 8 - iris_volume ) );	/* Right Ch. */
		}
#endif /* ! IRIS_STEREO_AUDIO */
		count--;
	}
	*frameUsed += used * 4;
	return stereo? used * 2: used;
}


static ssize_t iris_ct_s8(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int val, stereo = sound.soft.stereo;

	frameLeft >>= 2;
	if (stereo)
		userCount >>= 1;
	used = count = min(userCount, frameLeft);
	while (count > 0) {
		u_char data;

		if (get_user(data, userPtr++))
			return -EFAULT;
#ifndef IRIS_STEREO_AUDIO
		val = ( data - 0x80 );
		if ( stereo ) {
			if ( get_user(data, userPtr++))
				return -EFAULT;
			val += ( data - 0x80 );
			val >>= 1;	/* take average of R & L */
		}
		*p++ = ( val << iris_volume );	/* Left Ch. */
		*p++ = 0;			/* Right Ch. */
#else /* ! IRIS_STEREO_AUDIO */
		val = ( data - 0x80 );
		*p++ = ( val << iris_volume );	/* Left Ch. */
		if ( stereo ) {
			if ( get_user(data, userPtr++))
				return -EFAULT;
			val = ( data - 0x80 );
		}
		if( iris_ctrl_buzzer_sw ){
		  *p++ = 0;	/* Right Ch. */
		}else{
		  *p++ = ( val << iris_volume );	/* Right Ch. */
		}
#endif /* ! IRIS_STEREO_AUDIO */
		count--;
	}
	*frameUsed += used * 4;
	return stereo? used * 2: used;
}


static ssize_t iris_ct_u8(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int val, stereo = sound.soft.stereo;

	frameLeft >>= 2;
	if (stereo)
		userCount >>= 1;
	used = count = min(userCount, frameLeft);
	while (count > 0) {
		u_char data;

		if (get_user(data, userPtr++))
			return -EFAULT;
#ifndef IRIS_STEREO_AUDIO
		val = data;
		if ( stereo ) {
			if ( get_user(data, userPtr++))
				return -EFAULT;
			val += data;
			val >>= 1;			/* take average of R & L */
		}
		*p++ = ( (val ^ 0x80) << iris_volume );		/* Left Ch. */
		*p++ = 0;					/* Right Ch. */
#else /* ! IRIS_STEREO_AUDIO */
		val = data;
		*p++ = ( (val ^ 0x80) << iris_volume );		/* Left Ch. */
		if ( stereo ) {
			if ( get_user(data, userPtr++))
				return -EFAULT;
			val = data;
		}
		if( iris_ctrl_buzzer_sw ){
		  *p++ = 0;		/* Right Ch. */
		}else{
		  *p++ = ( (val ^ 0x80) << iris_volume );		/* Right Ch. */
		}
#endif /* ! IRIS_STEREO_AUDIO */
		count--;
	}
	*frameUsed += used * 4;
	return stereo? used * 2: used;
}


static ssize_t iris_ct_s16(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
	ssize_t count, used;
	int stereo = sound.soft.stereo;
	short *fp = (short *) &frame[*frameUsed];

	frameLeft >>= 2;
	userCount >>= (stereo? 2: 1);
	used = count = min(userCount, frameLeft);
	if (!stereo) {
		short *up = (short *) userPtr;
		while (count > 0) {
			short data;
			if (get_user(data, up++))
				return -EFAULT;
			*fp++ = ( data >> ( 8 - iris_volume ));		/* Left Ch. */


#ifndef IRIS_STEREO_AUDIO
			*fp++ = 0;					/* Right Ch. */
#else /* ! IRIS_STEREO_AUDIO */
			if( iris_ctrl_buzzer_sw ){
			  *fp++ = 0;		/* Right Ch. */
			}else{
			  *fp++ = ( data >> ( 8 - iris_volume ));		/* Right Ch. */
			}
#endif /* ! IRIS_STEREO_AUDIO */
			count--;
		}
	} else {
		short *up = (short *) userPtr;
		while (count > 0) {
			short data;
			short temp;
			if (get_user(data, up++))
				return -EFAULT;
			if (get_user(temp, up++))
				return -EFAULT;
#ifndef IRIS_STEREO_AUDIO
			data += temp;
			data >>= 1;			/* take average of R & L */
			*fp++ = ( data >> ( 8 - iris_volume ));		/* Left Ch. */
			*fp++ = 0;					/* Right Ch. */
#else /* ! IRIS_STEREO_AUDIO */
			*fp++ = ( data >> ( 8 - iris_volume ));		/* Left Ch. */
		if( iris_ctrl_buzzer_sw ){
			*fp++ = 0;	/* Left Ch. */
		}else{
			*fp++ = ( temp >> ( 8 - iris_volume ));		/* Right Ch. */
		}
#endif /* ! IRIS_STEREO_AUDIO */
			count--;
		}
	}
	*frameUsed += used * 4;
	return stereo? used * 4: used * 2;
}

static ssize_t iris_ct_u16(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
	ssize_t count, used;
	int mask = (sound.soft.format == AFMT_U16_LE? 0x0080: 0x8000);
	int stereo = sound.soft.stereo;
	short *fp = (short *) &frame[*frameUsed];
	short *up = (short *) userPtr;

	frameLeft >>= 2;
	userCount >>= (stereo? 2: 1);
	used = count = min(userCount, frameLeft);
	while (count > 0) {
		int data;
		int temp;
		if (get_user(data, up++))
			return -EFAULT;
#ifndef IRIS_STEREO_AUDIO
		data ^= mask;
		if (stereo) {
			if (get_user(temp, up++))
				return -EFAULT;
			temp ^= mask;
			data += temp;
			data >>= 1;			/* take average of R & L */
			data ^= mask;
		}
		*fp++ = ( data >> ( 8 - iris_volume ));		/* Left Ch. */
		*fp++ = 0;					/* Right Ch. */
#else /* ! IRIS_STEREO_AUDIO */
		data ^= mask;
		*fp++ = ( data >> ( 8 - iris_volume ));		/* Left Ch. */
		if (stereo) {
			if (get_user(temp, up++))
				return -EFAULT;
			temp ^= mask;
			data = temp;
			data ^= mask;
		}
		if( iris_ctrl_buzzer_sw ){
		  *fp++ = 0;	/* Right Ch. */
		}else{
		  *fp++ = ( data >> ( 8 - iris_volume ));		/* Right Ch. */
		}
#endif /* ! IRIS_STEREO_AUDIO */
		count--;
	}
	*frameUsed += used * 4;
	return stereo? used * 4: used * 2;
}

static TRANS transIris = {
	iris_ct_law, iris_ct_law, iris_ct_s8, iris_ct_u8,
	iris_ct_s16, iris_ct_u16, iris_ct_s16, iris_ct_u16
};

/*** Low level stuff *********************************************************/


#ifdef USE_DAC_AS_BUZZER

static int dac_is_used_as_buzzer_currently = 0;
static int interrupt_input_enabled = 0;
static int ref_count = 0;
static int this_is_major = 0;
static int buzzer_is_major = 0;

static int stacked_settings_depth = 0;
#define STACKED_SETTINGS_MAX  1

static struct {
  SETTINGS hard;
  SETTINGS soft;
  SETTINGS dsp;
  int volume_left;
  int volume_right;
  int iris_volume;
} stacked_settings[STACKED_SETTINGS_MAX];

static void sq_reset(void);
static void sq_setup(int numBufs, int bufSize, char **buffers);


#endif /* USE_DAC_AS_BUZZER */


static void Iris_Set_Volume(int volume)
{
	// convert 'general volume' to 'iris volume(pcm shift value)'
	iris_volume = ( ( volume & 0xff ) / 8 );
	if ( iris_volume > 8 ) iris_volume = 8;

	sound.volume_left = volume & 0xff;
	if ( sound.volume_left > 100 ) sound.volume_left = 100;

	sound.volume_right = ( volume & 0xff00 >> 8);
	if ( sound.volume_right > 100 ) sound.volume_right = 100;
}

static int Iris_Get_Volume(void)
{
	return ( sound.volume_right << 8 | sound.volume_left );
}

static void wait_ms(int ten_ms)
{
	schedule_timeout(ten_ms);
}

static int iris_amp_turned_on_externally = 0;
static int iris_amp_turned_on_internally = 0;

static void amp_off(void)
{
  //printk("amp off\n");
  SET_PEDR_LO(bitPE2);
  SET_PESBSR_LO(bitPE2);
  iris_amp_init = 0;
}

static void amp_on(void)
{
  //printk("amp on\n");
  if( ! iris_amp_init ){
    //printk("amp on init\n");
    SET_PEDR_HI(bitPE2);
    wait_ms(IRIS_WAIT_AMP_ON);
    iris_amp_init = 1;
  }
}

static void Iris_AMP_off(void)
{
  //  	SET_PEDR_LO(bitPE2);	/* AMP OFF */  /* for CEBIT IRIS yata */
  //	iris_amp_init = 0;
  //printk("AMP_off\n");
  iris_amp_turned_on_internally = 0;
  if( ! iris_amp_turned_on_externally ) amp_off();
}

void Iris_AMP_off_externally(void)
{
  //printk("AMP_off ext\n");
  iris_amp_turned_on_externally = 0;
  if( ! iris_amp_turned_on_internally ) amp_off();
}

static void Iris_AMP_on(void)
{
  //printk("AMP_on\n");
  iris_amp_turned_on_internally = 1;
  amp_on();
  //	if (!iris_amp_init) {
  //		SET_PEDR_HI(bitPE2);	/* AMP ON */
  //		wait_ms(IRIS_WAIT_AMP_ON);
  //		iris_amp_init = 1;
  //	}
}

void Iris_AMP_on_externally(void)
{
  //printk("AMP_on ext\n");
  iris_amp_turned_on_externally = 1;
  amp_on();
}

static inline void Iris_AMP_init(void)
{
  	DISABLE_PE_IRQ(bitPE2);	/* IRQ Disable */
  	SET_PE_OUT(bitPE2);	/* PE2 set to output */
	SET_PESBSR_LO(bitPE2);	/* set to AMP off on Suspend */ /* for CEBIT IRIS yata */
  	Iris_AMP_off();
}


#if !defined(CONFIG_DMA_L7200)

static void Iris_disable_dma(void)
{
  	IO_DMA_TERM |= ( 1 << IRIS_SOUND_DMA_CHANNEL );
	barrier();
}

static void Iris_enable_dma(void)
{
	IO_DMA_EN |= ( 1 << IRIS_SOUND_DMA_CHANNEL );
	barrier();
}

static void Iris_clear_irq(void)
{
	IO_DMA_ICR |= ( 1 << IRIS_SOUND_DMA_CHANNEL );
	barrier();
}

static void Iris_disable_irq(void)
{
	IO_DMA_IER &= ~( 1 << IRIS_SOUND_DMA_CHANNEL );
	barrier();
}

static void Iris_enable_irq(void)
{
	IO_DMA_IER |= ( 1 << IRIS_SOUND_DMA_CHANNEL );
	barrier();
}

#endif

static void Iris_clock_stop(void)
{
	IO_SYS_CLOCK_ENABLE &= ~0x8000;
	barrier();
	IO_SYS_CLOCK_ENABLE &= ~0x3000;
	barrier();
}

static void Iris_disable_sound(void)
{
#if defined(CONFIG_DMA_L7200)
	l7200_dma_stop(IRIS_SOUND_DMA_CHANNEL);

	IO_SIIDR |=  ( SIIDR_ATFS | SIIDR_ARFS );
	IO_SICR0 &= ~( SICR0_ENB );

	Iris_clock_stop();
#else
	while( IO_DMA_BZ & ( 1 << IRIS_SOUND_DMA_CHANNEL ) );
	//	  printk("dma busy -- wait\n");

	Iris_disable_dma();

	IO_SIIDR |=  ( SIIDR_ATFS | SIIDR_ARFS );
	IO_SICR0 &= ~( SICR0_ENB );

	Iris_clock_stop();
	Iris_clear_irq();
	Iris_disable_irq();
#endif
}

static void IrisSilence(void)
{
	/* Disable sound & DMA */
	Iris_disable_sound();

	/* reset SIC & FIFOs */
	IO_SICR0 |= SICR0_RST;

	/* AMP Off */
	//printk("Silence\n");
	Iris_AMP_off();
}

static int IrisGetSamp(void)
{
	switch (sound.soft.speed) {
	case 8000:
	  	return clock_set_data[1];
	case 44100:
	  	return clock_set_data[8];
	case 22050:
	  	return clock_set_data[6];
	case 11025:
	  	return clock_set_data[2];
	default:
		printk("DMA sound: Illegal sound rate %d\n", sound.soft.speed);
		return clock_set_data[1];
	}
}


static void IrisInit(void)
{
  //iris_amp_init = 0; /* this is not needed ... ? yata */


	/* AMP init */
	Iris_AMP_init();
	Iris_AMP_on();

	/* set volume */
	Iris_Set_Volume(sound.volume_left);

	//printk("IrisInit\n");
	//IrisSilence(); /* this is not needed ... ? yata */


	/*	for Multiplexing	*/
	IO_MCCR |= ( 1 << 26 );
	IO_MCCR &= ~( 1 << 16 );

	/* initialize SIC control registers enable AIL  */
	IO_SICR0  = (SICR0_SAIL);
	barrier();
	IO_SICR0 |= SICR0_BCKD;				// Set BIT_CLK direction to output
//	IO_SICR0 |= SICR0_SYNCD;			// SYNCD direction output:0
	barrier();

	/* 	initialize internel clock */
	IO_SYS_CLOCK_AUX    |= 0x34;
	IO_SYS_CLOCK_SELECT &= ~0x7e0000;
	IO_SYS_CLOCK_SELECT |= ( IrisGetSamp() | 0x2000000 );
	IO_SYS_CLOCK_ENABLE |= 0x3000;
	barrier();
	IO_SYS_CLOCK_ENABLE |= 0x8000;
	barrier();

	/* enable audio playback (tx)*/
	IO_SICR1 = (SICR1_ERPL + SICR1_MODELSB);

	/* enable AC-link */
	IO_SICR2 |= SICR2_ERPL;

	/* enable SIC in AIL mode	*/
	IO_SICR0 |= SICR0_ENB;

#if defined(CONFIG_DMA_L7200)
	l7200_dma_set_device(IRIS_SOUND_DMA_CHANNEL,
			     SIADR,
			     DMA_REQ_SIC_AUDIO_TRANSMIT,
			     DMA_CTRL_PW_32 | DMA_CTRL_BE);
#else
	/* DMA Setting */
	IO_DMA_RCM1 &= ~(0x000000ff << (IRIS_SOUND_DMA_CHANNEL*8));
	IO_DMA_RCM1 |= ( 0x18 << (IRIS_SOUND_DMA_CHANNEL*8)) ;
	IO_DMA_CxPA(IRIS_SOUND_DMA_CHANNEL) = SIADR;

	// fixme 2001.2.22   K.Yamade
	//IO_DMA_CxCTL(IRIS_SOUND_DMA_CHANNEL) = 0x71b;
	//IO_DMA_CxCTL(IRIS_SOUND_DMA_CHANNEL) = 0x70b;
	IO_DMA_CxCTL(IRIS_SOUND_DMA_CHANNEL) = 0x00b;
	//IO_DMA_CxCTL(IRIS_SOUND_DMA_CHANNEL) = 0x01b;
#endif

	sound.hard = sound.soft;
	sound.trans = &transIris;
}


static void Iris_sq_play_next_frame()
{
	char *start;
	ulong size;

	if ( ( buf_flag[sq.count] == BUFFER_EMPTY ) && ( sq.count == sq.front ) ) {
		sq.playing = 0;
#if 0
		WAKE_UP(sq.sync_queue);
#else
		WAKE_UP(sync_queue);
#endif
	}
	if ( buf_flag[sq.count] == BUFFER_EMPTY ){
	  //printk("play_next : EMPTY\n");
		return ;
	}
		
	start = __virt_to_phys(sq_block_address(sq.count));
	size = (ulong)buf_size[sq.count];

	// Setup DMA address
#if defined(CONFIG_DMA_L7200)
	l7200_dma_queue_buffer(IRIS_SOUND_DMA_CHANNEL,
			       NULL, (dma_addr_t)start, size);
#else
	IO_DMA_CxNBA(IRIS_SOUND_DMA_CHANNEL) = start;
	IO_DMA_CxNTC(IRIS_SOUND_DMA_CHANNEL) = size;
	barrier();

	Iris_clear_irq();
	Iris_enable_irq();
	Iris_enable_dma();
#endif
}

static void IrisPlay(void)
{
	sq.playing = 1;

#if defined(CONFIG_DMA_L7200)
	if ( l7200_dma_is_busy(IRIS_SOUND_DMA_CHANNEL) ){
	  //printk("IrisPlay:DMABUSY\n");
		return;
	}
#else
	/* DMA BUSY , so wait ! */ 
	if ( IO_DMA_BZ & ( 1 << IRIS_SOUND_DMA_CHANNEL ) )
		return;
#endif
	Iris_sq_play_next_frame();
}

#ifdef USE_DAC_AS_BUZZER
static void IrisTerminatePlay(void)
{
}
#endif /* USE_DAC_AS_BUZZER */


#if defined(CONFIG_DMA_L7200)
static void Iris_sq_interrupt(void* id, int size)
{
  //printk("sq_interrupt\n");
	buf_flag[sq.count] = BUFFER_EMPTY;
	buf_size[sq.count] = 0;
	sq.count = (sq.count+1) % sq.max_count;
	WAKE_UP(write_queue);
	IrisPlay();
}
#else
static void Iris_sq_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
	int data = IO_DMA_ISR & ( 1 << IRIS_SOUND_DMA_CHANNEL );
	if ( data ) {
		Iris_clear_irq();

		if ( IO_DMA_BZ & ( 1 << IRIS_SOUND_DMA_CHANNEL ) ) {
			return;
		}

		buf_flag[sq.count] = BUFFER_EMPTY;
		buf_size[sq.count] = 0;
		sq.count = (sq.count+1) % sq.max_count;
#if 0
		WAKE_UP(sq.write_queue);
#else
		WAKE_UP(write_queue);
#endif
		IrisPlay();

	}
}
#endif


static int __init IrisIrqInit(void)
{
	int err;

#if defined(CONFIG_DMA_L7200)
	err = l7200_request_dma(&iris_dmasound_irq, "dmasound");
	if (err) {
		return 0;
	}
	/* printk("iris_dmasound_irq=%d\n", iris_dmasound_irq); */

	l7200_dma_set_callback(iris_dmasound_irq,
			       (dma_callback_t)Iris_sq_interrupt);

	/* Disable sound & DMA */
	Iris_disable_sound();
#else
	/* Disable sound & DMA */
	Iris_disable_sound();

	/* Register interrupt handler. */
	if ((err = request_irq(IRQ_DMA, Iris_sq_interrupt, SA_SHIRQ,
			       "DMA sound", Iris_sq_interrupt))) {
		return(0);
	}
#endif

	return(1);

}

static void *IrisMalloc(unsigned int size , int flags )
{
	return kmalloc(size , flags );
}

static void IrisFree(void *ptr , unsigned int flags )
{
	kfree(ptr);
}


#ifdef MODULE
static void IrisIrqCleanUp(void)
{
	/* Disable sound & DMA */
	Iris_disable_sound();

	/* release the interrupt */
	free_irq(IRQ_DMA, Iris_sq_interrupt);
}
#endif /* MODULE */


static int IrisSetFormat(int format)
{
	int size;

	/* Falcon sound DMA supports 8bit and 16bit modes */

	switch (format) {
	case AFMT_QUERY:
		return(sound.soft.format);
	case AFMT_MU_LAW:
	case AFMT_A_LAW:
	case AFMT_U8:
	case AFMT_S8:
		size = 8;
		break;
	case AFMT_S16_BE:
	case AFMT_U16_BE:
	case AFMT_S16_LE:
	case AFMT_U16_LE:
		size = 16;
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
	//printk("IrisInit setformat\n");
	IrisInit();

	return(format);
}

#ifdef USE_DAC_AS_BUZZER

static int IrisSettingStack_Push(struct sound_settings* cur)
{
  int size;
  int format;
  if( ! cur ) return -EINVAL;
  size = cur->soft.size;
  format = cur->soft.format;
  if( stacked_settings_depth >= STACKED_SETTINGS_MAX ){
    printk("SndPushSetting: No More Stack\n");
    return -EINVAL;
  }

  memcpy(&(stacked_settings[stacked_settings_depth].hard),&(cur->hard),
	 sizeof(SETTINGS));
  memcpy(&(stacked_settings[stacked_settings_depth].soft),&(cur->soft),
	 sizeof(SETTINGS));
  memcpy(&(stacked_settings[stacked_settings_depth].dsp),&(cur->dsp),
	 sizeof(SETTINGS));

  stacked_settings[stacked_settings_depth].volume_left = cur->volume_left;
  stacked_settings[stacked_settings_depth].volume_right = cur->volume_right;
  stacked_settings[stacked_settings_depth].iris_volume = iris_volume;

  stacked_settings_depth++;

  return 0;
}

static int IrisSettingStack_Pop(struct sound_settings* cur)
{
  int size;
  int format;
  int stereo;
  int speed;
  if( ! cur ) return -EINVAL;
  size = cur->soft.size;
  format = cur->soft.format;
  stereo = cur->soft.stereo;
  speed = cur->soft.speed;
  if( stacked_settings_depth < 1 ){
    printk("SndPopSetting: No More Stack\n");
    return -EINVAL;
  }
  stacked_settings_depth--;

  memcpy(&(cur->hard),&(stacked_settings[stacked_settings_depth].hard),
	 sizeof(SETTINGS));
  memcpy(&(cur->soft),&(stacked_settings[stacked_settings_depth].soft),
	 sizeof(SETTINGS));
  memcpy(&(cur->dsp),&(stacked_settings[stacked_settings_depth].dsp),
	 sizeof(SETTINGS));

  cur->volume_left = stacked_settings[stacked_settings_depth].volume_left;
  cur->volume_right = stacked_settings[stacked_settings_depth].volume_right;
  iris_volume = stacked_settings[stacked_settings_depth].iris_volume;

  if( cur->soft.speed != speed || cur->soft.format != format || cur->soft.size != size || cur->soft.stereo != stereo ){
    sq_reset();
    l7200_dma_flush_all(IRIS_SOUND_DMA_CHANNEL);
    sq_setup(numBufs, bufSize << 10, sound_buffers);
    IrisInit();
  }

  return 0;
}

int IrisSoundCtl_Dsp_Buzzer_On(void)
{
  printk("IRIS_SNDCTL_DSP_BUZZER_ON\n");
  if( dac_is_used_as_buzzer_currently ){
    printk("!! Already Enabled !!\n");
    return 0;
  }
  if( ref_count > 1 ){
    printk("!! MORE THAN 1 FILES OPENED !!\n");
    return -EINVAL;
  }
  dac_is_used_as_buzzer_currently = 1;
  interrupt_input_enabled = 1;
  if( ! ref_count ){
    buzzer_is_major = 0;
  }else{
    IrisTerminatePlay();
  }
  
  IrisSettingStack_Push(&sound);
  
  printk("IRIS_SNDCTL_DSP_BUZZER_ON DONE\n");
  return 0;
}

int IrisSoundCtl_Dsp_Buzzer_Off(void)
{
  printk("IRIS_SNDCTL_DSP_BUZZER_OFF\n");
  if( ! dac_is_used_as_buzzer_currently ){
    printk("!! Already Disabled !!\n");
    return 0;
  }
  if( ref_count > 1 ){
    printk("!! MORE THAN 1 FILES OPENED !!\n");
    return -EINVAL;
  }
  if( ref_count ){
    IrisTerminatePlay();
  }
  dac_is_used_as_buzzer_currently = 0;
  interrupt_input_enabled = 0;
  
  IrisSettingStack_Pop(&sound);
  
  printk("IRIS_SNDCTL_DSP_BUZZER_OFF DONE\n");
  return 0;
}

#else /* USE_DAC_AS_BUZZER */

int IrisSoundCtl_Dsp_Buzzer_On(void)
{
  return -EINVAL;
}

int IrisSoundCtl_Dsp_Buzzer_Off(void)
{
  return -EINVAL;
}

#endif /* USE_DAC_AS_BUZZER */




/*** Machine definitions *****************************************************/


static MACHINE machIris = {
	DMASND_IRIS,		// int type
	IrisMalloc,		// void *dma_alloc(uint, int)
	IrisFree,		// void  dma_free(void *, uint)
	IrisIrqInit,		// void  irqinit(void)
#ifdef MODULE
	IrisIrqCleanUp,		// void  irqcleanup(void)
#endif /* MODULE */
	IrisInit,		// void  init(void)
	IrisSilence,		// void  silence(void)
	IrisSetFormat,		// int   setFormat(int)
	NULL,			// int   setVolume(int)
	NULL,			// int   setBass(int)
	NULL,			// int   setTreble(int)
	NULL,			// int   setGain(int)
	IrisPlay		// void  play(void)
};


/*** Mid level stuff *********************************************************/


static void sound_silence(void)
{
	/* update hardware settings one more */
  //printk("sound_silence\n");
	(*sound.mach.init)();
	(*sound.mach.silence)();

}


static void sound_init(void)
{
	(*sound.mach.init)();
}


static int sound_set_format(int format)
{
	return(*sound.mach.setFormat)(format);
}


static int sound_set_speed(int speed)
{
	if (speed < 0)
		return(sound.soft.speed);

	sound.soft.speed = speed;
	(*sound.mach.init)();
	if (sound.minDev == SND_DEV_DSP)
		sound.dsp.speed = sound.soft.speed;

	return(sound.soft.speed);
}


static int sound_set_stereo(int stereo)
{
	if (stereo < 0)
		return(sound.soft.stereo);

	stereo = !!stereo;    /* should be 0 or 1 now */

	sound.soft.stereo = stereo;
	if (sound.minDev == SND_DEV_DSP)
		sound.dsp.stereo = stereo;
	(*sound.mach.init)();

	return(stereo);
}



static ssize_t sound_copy_translate(const u_char *userPtr,
				    size_t userCount,
				    u_char frame[], ssize_t *frameUsed,
				    ssize_t frameLeft)
{
	ssize_t (*ct_func)(const u_char *, size_t, u_char *, ssize_t *, ssize_t) = NULL;
	ssize_t ret = 0; 

	switch (sound.soft.format) {
	case AFMT_MU_LAW:
		ct_func = sound.trans->ct_ulaw;
		break;
	case AFMT_A_LAW:
		ct_func = sound.trans->ct_alaw;
		break;
	case AFMT_S8:
		ct_func = sound.trans->ct_s8;
		break;
	case AFMT_U8:
		ct_func = sound.trans->ct_u8;
		break;
	case AFMT_S16_BE:
		ct_func = sound.trans->ct_s16be;
		break;
	case AFMT_U16_BE:
		ct_func = sound.trans->ct_u16be;
		break;
	case AFMT_S16_LE:
		ct_func = sound.trans->ct_s16le;
		break;
	case AFMT_U16_LE:
		ct_func = sound.trans->ct_u16le;
		break;
	}

	if (ct_func) {
		ret = ct_func(userPtr, userCount, frame, frameUsed, frameLeft);
#if 0
		processor.u.armv3v4._flush_cache_area((unsigned long)frame,(unsigned long)(frame+(bufSize<<10)),0);
#endif
		return ret;
	}	    
	else
		return 0;
}


/*
 * /dev/mixer abstraction
 */

static int mixer_open(struct inode *inode, struct file *file)
{
  //printk("mixer_open\n");
	MOD_INC_USE_COUNT;
	mixer.busy = 1;
	//printk("mixer_open: done\n");
	return 0;
}


static int mixer_release(struct inode *inode, struct file *file)
{
  //printk("mixer_release\n");
	mixer.busy = 0;
	MOD_DEC_USE_COUNT;
	//printk("mixer_release: done\n");
	return 0;
}


static int mixer_ioctl(struct inode *inode, struct file *file, u_int cmd,
		       u_long arg)
{
	int data;

	//printk("mixer_ioctl\n");

	switch (sound.mach.type) {
	case DMASND_IRIS:
	{
		switch (cmd) {
			case SOUND_MIXER_READ_DEVMASK:
				return IOCTL_OUT(arg, SOUND_MASK_VOLUME );
			case SOUND_MIXER_READ_RECMASK:
				return IOCTL_OUT(arg, 0);
			case SOUND_MIXER_READ_STEREODEVS:
				return IOCTL_OUT(arg, SOUND_MASK_VOLUME);
			case SOUND_MIXER_READ_CAPS:
				return IOCTL_OUT(arg, 0);

			case SOUND_MIXER_WRITE_VOLUME:
				IOCTL_IN(arg, data);
				Iris_Set_Volume(data);
			case SOUND_MIXER_READ_VOLUME:
				return IOCTL_OUT(arg, Iris_Get_Volume());

			case SOUND_MIXER_READ_TREBLE:
				return IOCTL_OUT(arg, 0);
			case SOUND_MIXER_WRITE_TREBLE:
				return IOCTL_OUT(arg, 0);

			case SOUND_MIXER_WRITE_MIC:
				return IOCTL_OUT(arg, 0);
			case SOUND_MIXER_READ_MIC:
				return IOCTL_OUT(arg, 0);

			case SOUND_MIXER_READ_SPEAKER:
				return IOCTL_OUT(arg, 0);
			case SOUND_MIXER_WRITE_SPEAKER:
				return IOCTL_OUT(arg, 0);
#ifdef USE_DAC_AS_BUZZER

#define IRIS_SNDCTL_DSP_BUZZER_ON   0x5680
#define IRIS_SNDCTL_DSP_BUZZER_OFF  0x5681
		case IRIS_SNDCTL_DSP_BUZZER_ON:
		  return IrisSoundCtl_Dsp_Buzzer_On();
		  break;
		case IRIS_SNDCTL_DSP_BUZZER_OFF:
		  return IrisSoundCtl_Dsp_Buzzer_Off();
		  break;
#endif /* USE_DAC_AS_BUZZER */
		  
		}
			break;
	}
	}
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
	mixer_unit = register_sound_mixer(&mixer_fops, -1);
	if (mixer_unit < 0)
		return;

	mixer.busy = 0;
	sound.treble = 0;
	sound.bass = 0;
	switch (sound.mach.type) {
		case DMASND_IRIS:				// // nothing to do ??
			sound.volume_left = 0x3c;
			sound.volume_right = 0x3c;
			break;
	}

	//	printk("mixer_init : ret \n");

}


/*
 * Sound queue stuff, the heart of the driver
 */



static int sq_allocate_buffers(void)
{
	int i;

	if (sound_buffers)
		return 0;
	sound_buffers = kmalloc (numBufs * sizeof(char *), GFP_KERNEL);
	buf_flag = kmalloc (numBufs * sizeof(int *), GFP_KERNEL);
	buf_size = kmalloc (numBufs * sizeof(int *), GFP_KERNEL);
	if (!sound_buffers || !buf_flag || !buf_size )
		return -ENOMEM;
	for (i = 0; i < numBufs; i++) {
	  		sound_buffers[i] = sound.mach.dma_alloc (bufSize << 10, GFP_DMA);
		if (!sound_buffers[i]) {
			while (i--)
				sound.mach.dma_free (sound_buffers[i], bufSize << 10);
			kfree (sound_buffers);
			kfree (buf_flag);
			kfree (buf_size);
			sound_buffers = NULL;
			buf_flag = NULL;
			buf_size = NULL;
			return -ENOMEM;
		}
	}

	return 0;
}


static void sq_release_buffers(void)
{
	int i;

	if (sound_buffers) {
		for (i = 0; i < numBufs; i++)
			sound.mach.dma_free (sound_buffers[i], bufSize << 10);
			kfree (sound_buffers);
			kfree (buf_flag);
			kfree (buf_size);
			sound_buffers = NULL;
			buf_flag = NULL;
			buf_size = NULL;
	}
}


static void sq_setup(int numBufs, int bufSize, char **buffers)
{
	int i;

	sq.max_count = numBufs;
	sq.max_active = numBufs;
	sq.block_size = bufSize;
	sq.buffers = buffers;

	sq.front = sq.count = 0;
	sq.rear = -1;
#if 0
	sq.write_queue = sq.open_queue = sq.sync_queue = 0;
#endif
	sq.syncing = 0;

	sq.playing = 0;

	if( buf_flag ){
	  for(i=0;i<numBufs;i++) {
	    buf_flag[i]=BUFFER_EMPTY;
	    buf_size[i]=0;
	  }
	}

}

static void sq_play(void)
{
  //printk("sq_play\n");
	(*sound.mach.play)();
}


#ifdef USE_DAC_AS_BUZZER

static int file_is_current_player(struct file* file)
{
  if( file->private_data != &this_is_major ) return 1;
  if( dac_is_used_as_buzzer_currently && buzzer_is_major ) return 1;
  if( dac_is_used_as_buzzer_currently ) return 0;
  return 1;
}
#endif /* USE_DAC_AS_BUZZER */


static ssize_t sq_write(struct file *file, const char *src, size_t uLeft,
			loff_t *ppos)
{
	ssize_t uWritten = 0;
	u_char *dest;
	ssize_t uUsed, bUsed, bLeft;
	int flag = 0;

	if (uLeft == 0)
		return 0;

	while(1) {
		/* buffer full !   must to wait buffer free */
		while (buf_flag[sq.front] == BUFFER_FULL) {

			sq_play();		/* play! maybe buffer free :) */
			if (NON_BLOCKING(sq.open_mode))
				return uWritten > 0 ? uWritten : -EAGAIN;
#if 0
			SLEEP(sq.write_queue, ONE_SECOND);
#else
			//printk("sq_write : wait\n");
			SLEEP(write_queue, ONE_SECOND);
			//printk("sq_write : esl\n");
#endif
			if (SIGNAL_RECEIVED)
				return uWritten > 0 ? uWritten : -EINTR;
		}

		/* to be fill buffers */
		while(1) {

			dest = sq_block_address(sq.front);
			bUsed = 0;
			bLeft = sq.block_size;
			uUsed = sound_copy_translate(src, uLeft, dest, &bUsed, bLeft);
			if (uUsed <= 0) {
				flag = 1;
				break;
			}
			src += uUsed;
			uWritten += uUsed;
			uLeft -= uUsed;
			if (bUsed) {
				buf_flag[sq.front] = BUFFER_FULL;
				buf_size[sq.front] = (int*)bUsed;
				sq.front = (sq.front+1) % sq.max_count;
			}
			if ( buf_flag[sq.front] == BUFFER_FULL ) {
				flag = 1;
				break;
			}
		}

		sq_play();
		if ( flag == 1 ) break;
	}

	return uWritten;
}


#ifdef USE_DAC_AS_BUZZER

static ssize_t sq_write_wrap(struct file *file, const char *src, size_t uLeft,
			loff_t *ppos)
{
  ssize_t retval;
  //printk("sq_write_wrap\n");
  if( file_is_current_player(file) ){
    retval = sq_write(file,src,uLeft,ppos);
    return retval;
  }
  if (NON_BLOCKING(sq.open_mode)) return uLeft;
  SLEEP(write_queue, ONE_SECOND);
  if (SIGNAL_RECEIVED) return uLeft;
  return uLeft;
}
#endif /* USE_DAC_AS_BUZZER */




static int sq_open(struct inode *inode, struct file *file)
{
	int rc = 0;


	MOD_INC_USE_COUNT;
	if (sq.busy) {
#ifdef USE_DAC_AS_BUZZER
	  if( interrupt_input_enabled ){
	    printk("sq_open: interrupt_input_enabled\n");
	    goto  allow_multiple_open;
	  }
#endif /* USE_DAC_AS_BUZZER */
		rc = -EBUSY;
		if (NON_BLOCKING(file->f_flags))
			goto err_out;
		rc = -EINTR;
		while (sq.busy) {
#if 0
			SLEEP(sq.open_queue, ONE_SECOND);
#else
			SLEEP(open_queue, ONE_SECOND);
#endif
			if (SIGNAL_RECEIVED)
				goto err_out;
		}
		rc = 0;
	}
	sq.busy = 1;
#ifdef USE_DAC_AS_BUZZER
	file->private_data = &this_is_major;
	if( dac_is_used_as_buzzer_currently ){
	  buzzer_is_major = 1;
	}else{
	}
	printk("sq_open: this is major\n");
 allow_multiple_open:
	if( file->private_data != &this_is_major ){
	  printk("sq_open: this is not major\n");
	  sq.busy = 1;
	}else{
	}
	if( ref_count++ ){
	  printk("sq_open: ref_count++\n");
	  sq_setup(numBufs, bufSize << 10, sound_buffers);
	  goto done_setup_initialize;
	}
#endif /* USE_DAC_AS_BUZZER */
	rc = sq_allocate_buffers();
	if (rc)
		goto err_out_nobusy;
	sq_setup(numBufs, bufSize << 10, sound_buffers);
	sq.open_mode = file->f_flags;
	sound.minDev = MINOR(inode->i_rdev) & 0x0f;
	sound.soft = sound.dsp;
	sound.hard = sound.dsp;
	sound_init();
	if ((MINOR(inode->i_rdev) & 0x0f) == SND_DEV_AUDIO) {
		sound_set_speed(8000);
		sound_set_stereo(0);
		sound_set_format(AFMT_MU_LAW);
	}
#ifdef USE_DAC_AS_BUZZER
 done_setup_initialize:
	printk("sq_open: done\n");
#endif /* USE_DAC_AS_BUZZER */

	return 0;
err_out_nobusy:
	sq.busy = 0;
#ifdef USE_DAC_AS_BUZZER
	file->private_data = NULL;
#endif /* USE_DAC_AS_BUZZER */
#if 0
	WAKE_UP(sq.open_queue);
#else
	WAKE_UP(open_queue);
#endif
err_out:
	MOD_DEC_USE_COUNT;
	return rc;
}


static void sq_reset(void)
{
  //printk("sq_reset\n");
	sound_silence();
	sq.playing = 0;
	sq.count = 0;
	sq.front = 0;
}


static int sq_fsync(struct file *filp, struct dentry *dentry)
{
	int rc = 0;

	sq.syncing = 1;

	printk("sq_fsync\n");


	while (sq.playing) {
			printk("sq_fsync: playing\n");
#if 0
		SLEEP(sq.sync_queue, ONE_SECOND);
#else
		SLEEP(sync_queue, ONE_SECOND);
#endif
		sq_play();	/* there may be an incomplete frame waiting */
		if (SIGNAL_RECEIVED) {
			/* While waiting for audio output to drain, an
			 * interrupt occurred.  Stop audio output immediately
			 * and clear the queue. */
		  printk("sq_fsync: signal\n");
			sq_reset();
			rc = -EINTR;
			break;
		}
	}

	sq.syncing = 0;
	return rc;
}

static int sq_release(struct inode *inode, struct file *file)
{
	int rc = 0;
#ifdef USE_DAC_AS_BUZZER
	if( --ref_count ){
	  printk("sq_close: ref_count--\n");
	  goto done_release;
	}
#endif /* USE_DAC_AS_BUZZER */
	if (sq.busy) {

	  printk("sq_close: sq.busy\n");

		rc = sq_fsync(file, file->f_dentry);
		sq.busy = 0;
#if 0
		WAKE_UP(sq.open_queue);
#else
		WAKE_UP(open_queue);
#endif
		/* Wake up a process waiting for the queue being released.
		 * Note: There may be several processes waiting for a call
		 * to open() returning. */
	}
	sound.soft = sound.dsp;
	sound.hard = sound.dsp;
	printk("sq_release\n");
	sound_silence();
	if (rc == 0) {
		sq_release_buffers();
		MOD_DEC_USE_COUNT;
	}
#ifdef USE_DAC_AS_BUZZER
 done_release:
	if( file->private_data ){
	  printk("sq_close: major rel.\n");
	  file->private_data = NULL;
	  if( buzzer_is_major ) buzzer_is_major = 0;
	}else{
	  printk("sq_close: not major rel.\n");
	}
#endif /* USE_DAC_AS_BUZZER */
	return rc;
}


static int sq_ioctl(struct inode *inode, struct file *file, u_int cmd,
		    u_long arg)
{
	u_long fmt;
	int data;
	int size, nbufs;

	printk("sq_ioctl\n");

	switch (cmd) {
	case SNDCTL_DSP_RESET:
	  printk("sq_ioctl: SNDCTL_DSP_RESET\n");
		sq_reset();
		return 0;
	case SNDCTL_DSP_POST:
	case SNDCTL_DSP_SYNC:
	  printk("sq_ioctl: SNDCTL_DSP_SYNC\n");
		return sq_fsync(file, file->f_dentry);

		/* ++TeSche: before changing any of these it's
		 * probably wise to wait until sound playing has
		 * settled down. */
	case SNDCTL_DSP_SPEED:
		sq_fsync(file, file->f_dentry);
		IOCTL_IN(arg, data);
		return IOCTL_OUT(arg, sound_set_speed(data));
	case SNDCTL_DSP_STEREO:
		sq_fsync(file, file->f_dentry);
		IOCTL_IN(arg, data);
		return IOCTL_OUT(arg, sound_set_stereo(data));
	case SOUND_PCM_WRITE_CHANNELS:
		sq_fsync(file, file->f_dentry);
		IOCTL_IN(arg, data);
		return IOCTL_OUT(arg, sound_set_stereo(data-1)+1);
	case SNDCTL_DSP_SETFMT:
		sq_fsync(file, file->f_dentry);
		IOCTL_IN(arg, data);
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
		return IOCTL_OUT(arg, fmt);
	case SNDCTL_DSP_GETBLKSIZE:
		size = sq.block_size
			* sound.soft.size * (sound.soft.stereo + 1)
			/ (sound.hard.size * (sound.hard.stereo + 1));
		return IOCTL_OUT(arg, size);
	case SNDCTL_DSP_SUBDIVIDE:
		break;
	case SNDCTL_DSP_SETFRAGMENT:
		if (sq.count || sq.playing || sq.syncing)
			return -EINVAL;
		IOCTL_IN(arg, size);
		nbufs = size >> 16;
		if (nbufs < 2 || nbufs > numBufs)
			nbufs = numBufs;
		size &= 0xffff;
		if (size >= 8 && size <= 30) {
			size = 1 << size;
			size *= sound.hard.size * (sound.hard.stereo + 1);
			size /= sound.soft.size * (sound.soft.stereo + 1);
			if (size > (bufSize << 10))
				size = bufSize << 10;
		} else
			size = bufSize << 10;
		sq_setup(numBufs, size, sound_buffers);
		sq.max_active = nbufs;
		return 0;
	default:
	  //printk("sq_ioctl: mixer\n");
		return mixer_ioctl(inode, file, cmd, arg);
	}
	return -EINVAL;
}



static struct file_operations sq_fops =
{
	llseek: sound_lseek,
#ifdef USE_DAC_AS_BUZZER
	write: sq_write_wrap,
#else /* ! USE_DAC_AS_BUZZER */
	write: sq_write,
#endif /* ! USE_DAC_AS_BUZZER */
	ioctl: sq_ioctl,
	open: sq_open,
	release: sq_release,
};


static void __init sq_init(void)
{
#ifndef MODULE
	int sq_unit;
#endif
	sq_unit = register_sound_dsp(&sq_fops, -1);
	if (sq_unit < 0)
		return;

	/* whatever you like as startup mode for /dev/dsp,
	 * (/dev/audio hasn't got a startup mode). note that
	 * once changed a new open() will *not* restore these!
	 */
	//	sound.dsp.format = AFMT_U8;
	//	sound.dsp.format = AFMT_S8;
	sound.dsp.format = AFMT_S16_LE;

	sound.dsp.stereo = 0;
	//	sound.dsp.size = 8;
	sound.dsp.size = 16;

	/* set minimum rate possible without expanding */
	switch (sound.mach.type) {
		case DMASND_IRIS:
		  	sound.dsp.speed = 8000;
			//			sound.dsp.speed = 22050;
			break;
	}

	/* before the first open to /dev/dsp this wouldn't be set */
	sound.soft = sound.dsp;
	sound.hard = sound.dsp;

	sound_silence();
}

/*
 * /dev/sndstat
 */


/* state.buf should not overflow! */

static int state_open(struct inode *inode, struct file *file)
{
	char *buffer = state.buf;
	int len = 0;

	if (state.busy)
		return -EBUSY;

	MOD_INC_USE_COUNT;
	state.ptr = 0;
	state.busy = 1;

	len += sprintf(buffer+len, "  IRIS DMA sound driver:\n");

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
		       sound.soft.stereo, sound.soft.stereo ? "stereo" : "mono");
	len += sprintf(buffer+len, "\tsq.block_size = %d sq.max_count = %d"
		       " sq.max_active = %d\n",
		       sq.block_size, sq.max_count, sq.max_active);
	len += sprintf(buffer+len, "\tsq.count = %d sq.rear_size = %d\n", sq.count,
		       sq.rear_size);
	len += sprintf(buffer+len, "\tsq.playing = %d sq.syncing = %d\n",
		       sq.playing, sq.syncing);
	state.len = len;
	return 0;
}


static int state_release(struct inode *inode, struct file *file)
{
	state.busy = 0;
	MOD_DEC_USE_COUNT;
	return 0;
}


static ssize_t state_read(struct file *file, char *buf, size_t count,
			  loff_t *ppos)
{
	int n = state.len - state.ptr;
	if (n > count)
		n = count;
	if (n <= 0)
		return 0;
	if (copy_to_user(buf, &state.buf[state.ptr], n))
		return -EFAULT;
	state.ptr += n;
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
	state_unit = register_sound_special(&state_fops, SND_DEV_STATUS);
	if (state_unit < 0)
		return;
	state.busy = 0;

	//	printk("state_init : ret \n");

}


/*** Common stuff ********************************************************/

static long long sound_lseek(struct file *file, long long offset, int orig)
{
	return -ESPIPE;
}

static void iris_sound_power_off(void)
{
  //Iris_AMP_off(); /* prev. version */
  if( ! iris_amp_turned_on_externally ) /* turn off only if externally off */
  amp_off(); /* force off */
}

static void iris_sound_power_on(void)
{
  //Iris_AMP_on(); /* prev. version */
  if( iris_amp_turned_on_internally || iris_amp_turned_on_externally ) amp_on(); /* on if needed */
}

#ifdef CONFIG_PM
static int iris_sound_pm_callback(struct pm_dev *pm_dev,
				  pm_request_t req, void *data)
{
  static int suspended = 0;
  static int sicr_enb = 0;
	switch (req) {
	case PM_STANDBY:
	case PM_BLANK:
	case PM_SUSPEND:
	  if( suspended ) break;
		iris_sound_power_off();
		if( req == PM_SUSPEND ){
		  SET_PESBSR_LO(bitPE2);
		}else{
		  if( GET_PEDR(bitPE2) ){
		    SET_PESBSR_HI(bitPE2);
		  }else{
		    SET_PESBSR_LO(bitPE2);
		  }
		}
		{ /* disable SIB */
		  if( IO_SICR0 & SICR0_ENB ){
		    sicr_enb = 1;
		  }else{
		    sicr_enb = 0;
		  }
		  IO_SICR0 &= ~( SICR0_ENB );
		}
	  suspended = 1;
		break;
	case PM_UNBLANK:
	case PM_RESUME:
	  if( ! suspended ) break;
	  { /* disable SIB */
	    if( sicr_enb ){
	      IO_SICR0 |= SICR0_ENB;
	    }else{
	      IO_SICR0 &= ~SICR0_ENB;
	    }
	  }
	  SET_PESBSR_LO(bitPE2);
		iris_sound_power_on();
	  suspended = 0;
                break;
	}
	return 0;
}
#endif

/*** Config & Setup **********************************************************/


int __init Iris_sound_init(void)
{
	int has_sound = 0;


	has_sound = 1;
	sound.mach = machIris;

	if (!has_sound)
		return -1;

	if (!sound.mach.irqinit()) {
		printk("DMA sound driver: Interrupt initialization failed\n");
		return -1;
	}

	/* Set up sound queue, /dev/audio and /dev/dsp. */
	/* Set default settings. */
	//	printk("sq_init\n");
	sq_init();

	/* Set up /dev/sndstat. */
	//	printk("state_init\n");
	state_init();

	/* Set up /dev/mixer. */
	//	printk("mixer_init\n");
	mixer_init();

#ifdef MODULE
	irq_installed = 1;
#endif

#ifdef CONFIG_PM
        audio_pm_dev = pm_register(PM_SYS_DEV, 0, iris_sound_pm_callback);
#endif

	init_waitqueue_head(&write_queue);
	init_waitqueue_head(&open_queue);
	init_waitqueue_head(&sync_queue);

	printk("DMA sound driver installed, using %d buffers of %dk.\n", numBufs,
	       bufSize);

	/* follow initial value */
	if( iris_amp_turned_on_externally ){
	  Iris_AMP_on_externally();
	}

#ifdef IRIS_STEREO_AUDIO
#define DAC_OUTPUT_SW     (bitPA7)

      SET_PA_OUT(DAC_OUTPUT_SW);
      SET_PADR_LO(DAC_OUTPUT_SW);
      iris_ctrl_buzzer_sw = 0;
#endif /* IRIS_STEREO_AUDIO */

#ifdef USE_DAC_AS_BUZZER
#endif /* USE_DAC_AS_BUZZER */



	return 0;
}

module_init(Iris_sound_init);


#ifdef MODULE

int init_module(void)
{
	Iris_sound_init();
	return 0;
}


void cleanup_module(void)
{
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
}

#endif /* MODULE */
