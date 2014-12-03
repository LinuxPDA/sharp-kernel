/*
 *  linux/drivers/sound/dmasound/dmasound_ms7720rp.c
 *
 *  Copyright (c) 2004 Lineo uSolutions, Inc.
 *
 *  See linux/drivers/sound/dmasound/dmasound_core.c
 *      for copyright and credits
 *
 *  MS7720RP01 dmasound support.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/soundcard.h>
#include <linux/delay.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/sh7720.h>

#include "dmasound.h"

#define DMASOUND_MS7720RP_REVISION	0
#define DMASOUND_MS7720RP_EDITION	1

#undef DEBUG
#undef DEBUG2

#if DEBUG
# define SNDDBG(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
# define SNDDBG(fmt, args...)
#endif

#if DEBUG2
# define SNDDBG2(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
# define SNDDBG2(fmt, args...)
#endif

/* translation tables */

/* 16 bit mu-law */
short dmasound_ulaw2dma16[] = {
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
short dmasound_alaw2dma16[] = {
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

static SETTINGS def_hard = {
	format:	AFMT_S16_BE,
	stereo:	1,
	size:	16,
	speed:	44100
};
static SETTINGS def_soft = { 
	format:	AFMT_S16_BE,
	stereo:	1,
	size:	16, 
	speed:	44100
};

#define MS7720RP_SOUND_DMA_CHANNEL (ms7720rp_dmasound_play_irq)
static int   ms7720rp_dmasound_play_irq = -1;

#if defined(HAS_RECORD)
#define MS7720RP_SOUND_DMA_REC_CHANNEL (ms7720rp_dmasound_rec_irq)
static int   ms7720rp_dmasound_rec_irq = -1;
#endif

//#define SOFT_VOLUME

#ifdef SOFT_VOLUME
#define MS7720RP_SND_VOL (ms7720rp_dmasound_vol)
#define VOL_MAX		8
static int   ms7720rp_dmasound_vol = VOL_MAX;
#else
static int master_volume = 0;
#endif

/* Prototype top */
static void print_reg(int mode);

static void ms7720rp_sound_play(void);
static void ms7720rp_sound_silence(void);

#if defined(HAS_RECORD)
static void ms7720rp_sound_record(void);
#endif
/* Prototype bottom */

/* Translate Function top */
static ssize_t ms7720rp_ct_law(const u_char* userPtr, size_t userCount,
			     u_char frame[], ssize_t* frameUsed,
			     ssize_t frameLeft)
{
	short*  table = (dmasound.soft.format == AFMT_MU_LAW) ?
		dmasound_ulaw2dma16: dmasound_alaw2dma16;
	int     stereo = dmasound.soft.stereo;
	short*  p = (short*) &frame[*frameUsed];
	ssize_t count, used;

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	if (stereo) {
		userCount >>= 1;
	}
	used = count = min((int)userCount, (int)frameLeft);
	while (count > 0) {
		u_char data; int val;

		if (get_user(data, userPtr++))
			return -EFAULT;
#ifdef SOFT_VOLUME
		val = ((int)table[data] * MS7720RP_SND_VOL / VOL_MAX);
#else
		val = (int)table[data];
#endif
		*p++ = val; /* Left */
		if (stereo) {
			if (get_user(data, userPtr++))
				return -EFAULT;
#ifdef SOFT_VOLUME
			val = ((int)table[data] * MS7720RP_SND_VOL / VOL_MAX);
#else
			val = (int)table[data];
#endif
		}
		*p++ = val; /* Right */
		count--;
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 2 : used;
}

static ssize_t ms7720rp_ct_s8(const u_char* userPtr, size_t userCount,
			    u_char frame[], ssize_t* frameUsed,
			    ssize_t frameLeft)
{
	int     stereo = dmasound.soft.stereo;
	short*  p = (short*) &frame[*frameUsed];
	ssize_t count, used;

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	if (stereo) {
		userCount >>= 1;
	}
	used = count = min((int)userCount, (int)frameLeft);
	while (count > 0) {
		u_char data; u_long val;

		if (get_user(data, userPtr++))
			return -EFAULT;
		val = data;
#ifdef SOFT_VOLUME
		val = (((val - 0x80) << 8) * MS7720RP_SND_VOL / VOL_MAX);
#else
		val = ((val - 0x80) << 8);
#endif
		*p++ = val; /* Left */
		if (stereo) {
			if (get_user(data, userPtr++))
				return -EFAULT;
			val = data;
#ifdef SOFT_VOLUME
			val = (((val - 0x80) << 8) * MS7720RP_SND_VOL/VOL_MAX);
#else
			val = ((val - 0x80) << 8);
#endif
		}
		*p++ = val; /* Right */
		count--;
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 2 : used;
}

static ssize_t ms7720rp_ct_u8(const u_char* userPtr, size_t userCount,
			    u_char frame[], ssize_t* frameUsed,
			    ssize_t frameLeft)
{
	int     stereo = dmasound.soft.stereo;
	short*  p = (short*) &frame[*frameUsed];
	ssize_t count, used;

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	if (stereo) {
		userCount >>= 1;
	}
	used = count = min((int)userCount, (int)frameLeft);
	while (count > 0) {
		u_char data; int val;

		if (get_user(data, userPtr++))
			return -EFAULT;
		val = data;
#ifdef SOFT_VOLUME
		val = (((val ^ 0x80) << 8) * MS7720RP_SND_VOL / VOL_MAX);
#else
		val = ((val ^ 0x80) << 8);
#endif
		*p++ = val; /* Left */
		if (stereo) {
			if (get_user(data, userPtr++))
				return -EFAULT;
			val = data;
#ifdef SOFT_VOLUME
			val = (((val ^ 0x80) << 8) * MS7720RP_SND_VOL/VOL_MAX);
#else
			val = ((val ^ 0x80) << 8);
#endif
		}
		*p++ = val; /* Right */
		count--;
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 2: used;
}

static ssize_t ms7720rp_ct_s16(const u_char* userPtr, size_t userCount,
			     u_char frame[], ssize_t* frameUsed,
			     ssize_t frameLeft)
{
	int     stereo = dmasound.soft.stereo;
	short*  fp = (short*) &frame[*frameUsed];
	ssize_t count, used;

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	userCount >>= ((stereo) ? 2 : 1);
	used = count = min((int)userCount, (int)frameLeft);
	if (!stereo) {
		short* up = (short*)userPtr;
		int val;
		while (count > 0) {
			short data;
			if (get_user(data, up++))
				return -EFAULT;
			val = data;
#ifdef SOFT_VOLUME
			*fp++ = (val * MS7720RP_SND_VOL / VOL_MAX); /* Left */
			*fp++ = (val * MS7720RP_SND_VOL / VOL_MAX); /* Right */
#else
			*fp++ = val; /* Left */
			*fp++ = val; /* Right */
#endif
			count--;
		}
	}
	else {
		short* up = (short*)userPtr;
		while (count > 0) {
			short data;
			short temp;
			int val1, val2;
			if (get_user(data, up++))
				return -EFAULT;
			if (get_user(temp, up++))
				return -EFAULT;
			val1 = data;
			val2 = temp;
#ifdef SOFT_VOLUME
			*fp++ = (val1 * MS7720RP_SND_VOL / VOL_MAX); /* Left */
			*fp++ = (val2 * MS7720RP_SND_VOL / VOL_MAX); /* Right */
#else
			*fp++ = val1; /* Left */
			*fp++ = val2; /* Right */
#endif
			count--;
		}
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 4 : used * 2;
}

static ssize_t ms7720rp_ct_u16(const u_char* userPtr, size_t userCount,
			     u_char frame[], ssize_t* frameUsed,
			     ssize_t frameLeft)
{
	int     stereo = dmasound.soft.stereo;
	int     mask   = ((dmasound.soft.format == AFMT_U16_LE) ?
			  0x0080: 0x8000);
	short*  fp  = (short*) &frame[*frameUsed];
	short*  up  = (short*) userPtr;
	ssize_t count, used;

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	userCount >>= ((stereo) ? 2 : 1);
	used = count = min((int)userCount, (int)frameLeft);
	while (count > 0) {
		int data;
		int temp;
		if (get_user(data, up++))
			return -EFAULT;
		data ^= mask;
#ifdef SOFT_VOLUME
		*fp++ = (data * MS7720RP_SND_VOL / VOL_MAX); /* Left */
#else
		*fp++ = data; /* Left */
#endif
		if (stereo) {

			if (get_user(temp, up++))
				return -EFAULT;
			temp ^= mask;
			data  = temp;
			data ^= mask;
		}
#ifdef SOFT_VOLUME
		*fp++ = (data * MS7720RP_SND_VOL / VOL_MAX); /* Right */
#else
		*fp++ = data; /* Right */
#endif
		count--;
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 4 : used * 2;
}

#if defined(HAS_RECORD)
static ssize_t ms7720rp_ct_s8_read(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int val, stereo = dmasound.soft.stereo;

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	if (stereo)
		userCount >>= 1;
	used = count = min_t(unsigned long, userCount, frameLeft);
	while (count > 0) {
		u_char data;

		val = *p++;
		data = val >> 8;
		if (put_user(data, (u_char *)userPtr++))
			return -EFAULT;
		if (stereo) {
			val = *p;
			data = val >> 8;
			if (put_user(data, (u_char *)userPtr++))
				return -EFAULT;
		}
		p++;
		count--;
	}
	*frameUsed += used * 4;
	return stereo? used * 2: used;
}

static ssize_t ms7720rp_ct_u8_read(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int val, stereo = dmasound.soft.stereo;

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	if (stereo)
		userCount >>= 1;
	used = count = min_t(unsigned long, userCount, frameLeft);
	while (count > 0) {
		u_char data;

		val = *p++;
		data = (val >> 8) ^ 0x80;
		if (put_user(data, (u_char *)userPtr++))
			return -EFAULT;
		if (stereo) {
			val = *p;
			data = (val >> 8) ^ 0x80;
			if (put_user(data, (u_char *)userPtr++))
				return -EFAULT;
		}
		p++;
		count--;
	}
	*frameUsed += used * 4;
	return stereo? used * 2: used;
}

static ssize_t ms7720rp_ct_s16_read(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
	ssize_t count, used;
	int stereo = dmasound.soft.stereo;
	short *fp = (short *) &frame[*frameUsed];

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	userCount >>= (stereo? 2: 1);
	used = count = min_t(unsigned long, userCount, frameLeft);
	if (!stereo) {
		short *up = (short *) userPtr;
		while (count > 0) {
			short data;
			data = *fp;
			if (put_user(data, up++))
				return -EFAULT;
			fp+=2;
			count--;
		}
	} else {
		if (copy_to_user((u_char *)userPtr, fp, count * 4))
			return -EFAULT;
	}
	*frameUsed += used * 4;
	return stereo? used * 4: used * 2;
}

static ssize_t ms7720rp_ct_u16_read(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
	ssize_t count, used;
	int mask = (dmasound.soft.format == AFMT_U16_LE? 0x0080: 0x8000);
	int stereo = dmasound.soft.stereo;
	short *fp = (short *) &frame[*frameUsed];
	short *up = (short *) userPtr;

	SNDDBG2("stereo = %d\n",stereo);

	frameLeft >>= 2;
	userCount >>= (stereo? 2: 1);
	used = count = min_t(unsigned long, userCount, frameLeft);
	while (count > 0) {
		int data;

		data = *fp++;
		data ^= mask;
		if (put_user(data, up++))
			return -EFAULT;
		if (stereo) {
			data = *fp;
			data ^= mask;
			if (put_user(data, up++))
				return -EFAULT;
		}
		fp++;
		count--;
	}
	*frameUsed += used * 4;
	return stereo? used * 4: used * 2;
}
#endif

static TRANS transMS7720RP = {
	ms7720rp_ct_law,
	ms7720rp_ct_law,
	ms7720rp_ct_s8,
	ms7720rp_ct_u8,
	ms7720rp_ct_s16,
	ms7720rp_ct_u16,
	ms7720rp_ct_s16,
	ms7720rp_ct_u16
};

#if defined(HAS_RECORD)
static TRANS transMS7720RPRead = {
    ct_ulaw:     ms7720rp_ct_s16_read,
    ct_alaw:     ms7720rp_ct_s16_read,
    ct_s8:       ms7720rp_ct_s8_read,
    ct_u8:       ms7720rp_ct_u8_read,
    ct_s16be:    ms7720rp_ct_s16_read,
    ct_u16be:    ms7720rp_ct_u16_read,
    ct_s16le:    ms7720rp_ct_s16_read,
    ct_u16le:    ms7720rp_ct_u16_read,
};
#endif
/* Translate Function bottom */

static void ms7720rp_sound_playNextFrame(int index)
{
	u_char* start;
	u_long  size;

	start = write_sq.buffers[write_sq.front];
	size  = ((write_sq.count == index) ?
			write_sq.rear_size : write_sq.block_size);

	sh7720_dma_queue_buffer(MS7720RP_SOUND_DMA_CHANNEL,
				NULL,
				(unsigned long)start,
				size / 4);
	
	write_sq.front = (write_sq.front+1) % write_sq.max_count;
	write_sq.active++;
}

#if defined(HAS_RECORD)
static void ms7720rp_sound_recNextFrame(int index)
{
    u_char * start;
    u_long size;

    start = read_sq.buffers[read_sq.rear];
    size = read_sq.block_size;

    SNDDBG("start = %lx,size = %d\n",start,size);
    sh7720_dma_queue_buffer(MS7720RP_SOUND_DMA_REC_CHANNEL,
			    NULL,
			    (unsigned long)start,
			    size / 4);
    SNDDBG("rear = %x,front = %x\n",read_sq.rear,read_sq.front);
    SNDDBG("SIOF_SISCR0=0x%04x\n",ctrl_inw(SIOF_SISCR0));

    read_sq.active++;
}
#endif

/* Interrupt Function top */
static void ms7720rp_sound_interrupt(void* id, int size)
{
        if (!write_sq.active) {
	  	WAKE_UP(write_sq.sync_queue);
	}
	else {
		write_sq.active = 0;
	}

	write_sq.count--;

	ms7720rp_sound_play();

	WAKE_UP(write_sq.action_queue);
}

#if defined(HAS_RECORD)
static void ms7720rp_sound_rec_interrupt(void *id, int size)
{
    unsigned short sistr;
    SNDDBG("record Interrupt.\n");
    
/* DEBUG Start */
#if 0
    {
	SNDDBG("Data Dump\n");
	unsigned short *start;
	u_long  size;

	start = (unsigned short *)read_sq.buffers[read_sq.rear];
	size  = read_sq.block_size;
	int i;
	for ( i = 0; i < size/4;i++) {
	    printk("%04x:",*(start+i));
	    if (!(i%4) && i)
		printk("\n");
	}
    }
#endif
/* DEBUG END */
    read_sq.rear = (read_sq.rear+1) % read_sq.max_active;
#if 0
    if (!read_sq.active) {
	WAKE_UP(read_sq.sync_queue);
    }else{
	read_sq.active = 0;
    }
#endif
    if (read_sq.active)
	read_sq.active = 0;
    ms7720rp_sound_record();
    sistr = ctrl_inw(SIOF_SISTR0);
#if 0
    if ( sistr & 0x0020 )
	printk("SIOF:RFFUL\n");
    if ( sistr & 0x0002 )
	printk("SIOF:RFUDR\n");
    if ( sistr & 0x0001 )
	printk("SIOF:RFOVR\n");
#endif
    ctrl_outw(sistr & 0x0003,SIOF_SISTR0);
    SNDDBG("SIOF_SISTR0=%x\n",sistr);
    WAKE_UP(read_sq.action_queue);
}
#endif
/* Interrupt Function bottom */

/* I2C Function top */
static void ms7720rp_i2c_tx(unsigned short *data,
				unsigned char address,unsigned int num)
{
	unsigned int	i;
	unsigned char	msd,lsd;

	ctrl_outb(0xb0,ICCR1);		// I2C Enable, Master, Transferr
	ctrl_outb(0x38,ICMR);		// MSB first, WAIT disable
	ctrl_outb(0x1f,ICCKS);		// clock = /1024

	while (ctrl_inb(ICCR2) & 0x80);	// Bus Released ?

	ctrl_outb(0xbd,ICCR2);		// BBSY = 1, SCP = 0

					// Transmit Data Empty ?
	while (!(ctrl_inb(ICSR) & 0x80));

	// Slave Address & R/W send
	do {
		ctrl_outb(0x34,ICDRT);
					// Transmit Data Empty ?
		while (!(ctrl_inb(ICSR) & 0x80));
					// Transmit End ?
		while (!(ctrl_inb(ICSR) & 0x40));
	} while (ctrl_inb(ICIER) & 0x02);

	// address transmit
	do {
		ctrl_outb(address,ICDRT);
					// Transmit Data Empty ?
		while (!(ctrl_inb(ICSR) & 0x80));
					// Transmit End ?
		while (!(ctrl_inb(ICSR) & 0x40));
	} while (ctrl_inb(ICIER) & 0x02);

	// Data Transmit
	for (i = 0;i < num;i++){
		msd = (unsigned char)(data[i] >> 8);
		lsd = (unsigned char)(data[i] & 0x00ff);

		do {
			ctrl_outb(msd,ICDRT);
					// Transmit Data Empty ?
			while (!(ctrl_inb(ICSR) & 0x80));
					// Transmit End ?
			while (!(ctrl_inb(ICSR) & 0x40));
		} while (ctrl_inb(ICIER) & 0x02);

		do{
			ctrl_outb(lsd,ICDRT);
					// Transmit Data Empty ?
			while (!(ctrl_inb(ICSR) & 0x80));
					// Transmit End ?
			while (!(ctrl_inb(ICSR) & 0x40));
		} while (ctrl_inb(ICIER) & 0x02);
	}

					// TEND clear
	ctrl_outb(ctrl_inb(ICSR) & 0xbf,ICSR);
					// STOP clear
	ctrl_outb(ctrl_inb(ICSR) & 0xf7,ICSR);

	ctrl_outb(0x3d,ICCR2);		// BBSY = 0, SCP = 0

					// STOP = 1 ?
	while (!(ctrl_inb(ICSR) & 0x08));

	ctrl_outb(0x80,ICCR1);		// Slave, Receive mode

					// TDRE clear
	ctrl_outb(ctrl_inb(ICSR) & 0x7f,ICSR);
}

static void ms7720rp_i2c_rx(unsigned short *data,
				unsigned char address,unsigned int num)
{
	unsigned int	i;
	unsigned char	msd,lsd;

	ctrl_outb(0xb0,ICCR1);		// I2C Enable, Master, Transferr
	ctrl_outb(0x38,ICMR);		// MSB first, WAIT disable
	ctrl_outb(0x1f,ICCKS);		// clock = /1024

	while (ctrl_inb(ICCR2) & 0x80); // Bus Released ?

	ctrl_outb(0xbd,ICCR2);		// BBSY = 1, SCP = 0

					// Transmit Data Empty ?
	while (!(ctrl_inb(ICSR) & 0x80));

	// Slave Address & R/W send
	do{
		ctrl_outb(0x34,ICDRT);
					// Transmit Data Empty ?
		while (!(ctrl_inb(ICSR) & 0x80));
					// Transmit End ?
		while (!(ctrl_inb(ICSR) & 0x40));
	} while (ctrl_inb(ICIER) & 0x02);

	// address transmit
	do{
		ctrl_outb(address,ICDRT);
					// Transmit Data Empty ?
		while (!(ctrl_inb(ICSR) & 0x80));
					// Transmit End ?
		while (!(ctrl_inb(ICSR) & 0x40));
	} while (ctrl_inb(ICIER) & 0x02);

					// TEND clear
	ctrl_outb(ctrl_inb(ICSR) & 0xbf,ICSR);
					// STOP clear
	ctrl_outb(ctrl_inb(ICSR) & 0xf7,ICSR);

	ctrl_outb(0x3d,ICCR2);		// BBSY = 0, SCP = 0

					// STOP = 1 ?
	while (!(ctrl_inb(ICSR) & 0x08));

	while (ctrl_inb(ICCR2) & 0x80);	// Bus Released ?

	ctrl_outb(0xbd,ICCR2);		// BBSY = 1, SCP = 0

					// Transmit Data Empty ?
	while (!(ctrl_inb(ICSR) & 0x80));
 
	// Slave Address & R/W send
	do {
		ctrl_outb(0x35,ICDRT);
					// Transmit Data Empty ?
		while (!(ctrl_inb(ICSR) & 0x80));
					// Transmit End ?
		while (!(ctrl_inb(ICSR) & 0x40));
	} while (ctrl_inb(ICIER) & 0x02);

					// TEND clear
	ctrl_outb(ctrl_inb(ICSR) & 0xbf,ICSR);

	ctrl_outb(0xa0,ICCR1);		// I2C Enable, Master, Receive

					// TDRE clear
	ctrl_outb(ctrl_inb(ICSR) & 0x7f,ICSR);

					// ACKBT = 0
	ctrl_outb(ctrl_inb(ICIER) & 0xfe,ICIER);

	if (ctrl_inb(ICDRR));		// Dummy read

	for (i = 0;i < num - 1;i++){
					// Receive Data Full ?
		while (!(ctrl_inb(ICSR) & 0x20));

		msd = ctrl_inb(ICDRR);

					// Receive Data Full ?
		while (!(ctrl_inb(ICSR) & 0x20));

		lsd = ctrl_inb(ICDRR);

		data[i] = ((unsigned short)msd << 8) | (unsigned short)lsd;
	}

					// Receive Data Full ?
	while (!(ctrl_inb(ICSR) & 0x20));

					// ACKBT = 1
	ctrl_outb(ctrl_inb(ICIER) | 0x01,ICIER);

	ctrl_outb(0xe0,ICCR1);		// Receive disable

	msd = ctrl_inb(ICDRR);

					// Receive Data Full ?
	while (!(ctrl_inb(ICSR) & 0x20));

					// STOP clear
	ctrl_outb(ctrl_inb(ICSR) & 0xf7,ICSR);

	ctrl_outb(0x3d,ICCR2);		// BBSY = 0, SCP = 0

					// STOP = 1 ?
	while (!(ctrl_inb(ICSR) & 0x08));

	lsd = ctrl_inb(ICDRR);

	data[i] = ((unsigned short)msd << 8) | (unsigned short)lsd;

	ctrl_outb(0xa0,ICCR1);		// RCVD = 0
	ctrl_outb(0x80,ICCR1);		// MST = 0
}
/* I2C Function bottom */

/* DEBUG Function top */
static void print_reg(int mode)
{
	unsigned short data[2];

	if (mode == 0){
		printk("SIOF_SIMDR0  = 0x%04x\n",ctrl_inw(SIOF_SIMDR0));
		printk("SIOF_SISCR0  = 0x%04x\n",ctrl_inw(SIOF_SISCR0));
		printk("SIOF_SITDAR0 = 0x%04x\n",ctrl_inw(SIOF_SITDAR0));
		printk("SIOF_SIRDAR0 = 0x%04x\n",ctrl_inw(SIOF_SIRDAR0));
		printk("SIOF_SICDAR0 = 0x%04x\n",ctrl_inw(SIOF_SICDAR0));
		printk("SIOF_SICTR0  = 0x%04x\n",ctrl_inw(SIOF_SICTR0));
		printk("SIOF_SIFCTR0 = 0x%04x\n",ctrl_inw(SIOF_SIFCTR0));
		printk("SIOF_SISTR0  = 0x%04x\n",ctrl_inw(SIOF_SISTR0));
		printk("SIOF_SIIER0  = 0x%04x\n",ctrl_inw(SIOF_SIIER0));
		printk("SIOF_SITDR0  = 0x%08x\n",ctrl_inl(SIOF_SITDR0));
		printk("SIOF_SIRDR0  = 0x%08x\n",ctrl_inl(SIOF_SIRDR0));
		printk("SIOF_SITCR0  = 0x%08x\n",ctrl_inl(SIOF_SITCR0));
		printk("SIOF_SIRCR0  = 0x%08x\n",ctrl_inl(SIOF_SIRCR0));
		printk("SIOF_SPICR0  = 0x%04x\n",ctrl_inw(SIOF_SPICR0));
	}else{
		printk("SIOF_SIMDR1  = 0x%04x\n",ctrl_inw(SIOF_SIMDR1));
		printk("SIOF_SISCR1  = 0x%04x\n",ctrl_inw(SIOF_SISCR1));
		printk("SIOF_SITDAR1 = 0x%04x\n",ctrl_inw(SIOF_SITDAR1));
		printk("SIOF_SIRDAR1 = 0x%04x\n",ctrl_inw(SIOF_SIRDAR1));
		printk("SIOF_SICDAR1 = 0x%04x\n",ctrl_inw(SIOF_SICDAR1));
		printk("SIOF_SICTR1  = 0x%04x\n",ctrl_inw(SIOF_SICTR1));
		printk("SIOF_SIFCTR1 = 0x%04x\n",ctrl_inw(SIOF_SIFCTR1));
		printk("SIOF_SISTR1  = 0x%04x\n",ctrl_inw(SIOF_SISTR1));
		printk("SIOF_SIIER1  = 0x%04x\n",ctrl_inw(SIOF_SIIER1));
		printk("SIOF_SITDR1  = 0x%08x\n",ctrl_inl(SIOF_SITDR1));
		printk("SIOF_SIRDR1  = 0x%08x\n",ctrl_inl(SIOF_SIRDR1));
		printk("SIOF_SITCR1  = 0x%08x\n",ctrl_inl(SIOF_SITCR1));
		printk("SIOF_SIRCR1  = 0x%08x\n",ctrl_inl(SIOF_SIRCR1));
		printk("SIOF_SPICR1  = 0x%04x\n",ctrl_inw(SIOF_SPICR1));
	}

	data[0] = data[1] = 0; ms7720rp_i2c_rx(data,0x00,1);
	printk("UDA1342TS Register [00] = 0x%04x\n",data[0]);
	data[0] = data[1] = 0; ms7720rp_i2c_rx(data,0x01,1);
	printk("UDA1342TS Register [01] = 0x%04x\n",data[0]);
	data[0] = data[1] = 0; ms7720rp_i2c_rx(data,0x10,1);
	printk("UDA1342TS Register [10] = 0x%04x\n",data[0]);
	data[0] = data[1] = 0; ms7720rp_i2c_rx(data,0x11,1);
	printk("UDA1342TS Register [11] = 0x%04x\n",data[0]);
	data[0] = data[1] = 0; ms7720rp_i2c_rx(data,0x12,1);
	printk("UDA1342TS Register [12] = 0x%04x\n",data[0]);
	data[0] = data[1] = 0; ms7720rp_i2c_rx(data,0x20,1);
	printk("UDA1342TS Register [20] = 0x%04x\n",data[0]);
	data[0] = data[1] = 0; ms7720rp_i2c_rx(data,0x21,1);
	printk("UDA1342TS Register [21] = 0x%04x\n",data[0]);
	data[0] = data[1] = 0; ms7720rp_i2c_rx(data,0x30,1);
	printk("UDA1342TS Register [30] = 0x%04x\n",data[0]);
}
/* DEBUG Function bottom */

/* Initialize Function top */
#ifndef SOFT_VOLUME
static void ms7720rp_sound_set_dac_volume(void)
{
	unsigned short data[2];

	data[0] = master_volume;
	ms7720rp_i2c_tx(data,0x11,1);	// VL & VR
}
#endif

static void ms7720rp_sound_codec_init(void)
{
	unsigned short data[2];

	data[0] = 0x8000;		// Reset
	ms7720rp_i2c_tx(data,0x00,1);

					// IF : MSB-justified
					// DP : DAC Power-on
	data[0] = 0x0012;

#if defined(HAS_RECORD)
					// AM : input 1 select (input 2 off)
					// IF : MSB-justified
	data[0] |= 0x0210;
#endif

	ms7720rp_i2c_tx(data,0x00,1);

#ifndef SOFT_VOLUME
	ms7720rp_sound_set_dac_volume();
#endif

	SNDDBG("CODEC init done.\n");
}

static void ms7720rp_sound_dma_init(void)
{
	dma_device_t dev;
#if defined(HAS_RECORD)
	dma_device_t recdev;
#endif

	dev.sar    = 0;
	dev.dar    = SIOF_SITDR0;
	dev.dmatcr = 0;
	dev.chcr   = 0x1810;
	dev.way    = 0;
	dev.module = DMA_MODULE_SIOF0_TRANSMIT;

	sh7720_dma_set_device(MS7720RP_SOUND_DMA_CHANNEL, dev);

#if defined(HAS_RECORD)
	recdev.sar    = SIOF_SIRDR0;
	recdev.dar    = 0;
	recdev.dmatcr = 0;
	recdev.chcr   = 0x4810;
	recdev.way    = 1;
	recdev.module = DMA_MODULE_SIOF0_RECEIVE;

	sh7720_dma_set_device(MS7720RP_SOUND_DMA_REC_CHANNEL, recdev);
#endif

	SNDDBG("DMA init done.\n");
}

static void ms7720rp_sound_siof_init(void)
{
	unsigned short wk;

					// Reset TX & RX
	wk = ctrl_inw(SIOF_SICTR0);
	wk |= 0x3;
	ctrl_outw(wk,SIOF_SICTR0);
	ctrl_outw(wk & 0xFFFC,SIOF_SICTR0);

					// Clock = SIOFMCLK
	ctrl_outw(0x0002,SIOF_SISCR0);
					// Control Data disable
	ctrl_outw(0x0000,SIOF_SICDAR0);
					// Master mode 2
					// Data Length = 16
					// Frame Length = 32
	ctrl_outw(0xcc00,SIOF_SIMDR0);

					// Output Left  ch. enable
					// Left  ch. data assign = 0
					// Output Right ch. enable
					// Right ch. data assign = 1
	ctrl_outw(0x8081,SIOF_SITDAR0);
					// TFWM = 111
	ctrl_outw(0xe000,SIOF_SIFCTR0);
					// TDMAE = 1, TDREQE = 1,
	ctrl_outw(0x9000,SIOF_SIIER0);

#if defined(HAS_RECORD)
					// Sampling Edge
	ctrl_outw(ctrl_inw(SIOF_SIMDR0) | 0x1000,SIOF_SIMDR0);
					// Input Left  ch. enable
					// Left  ch. data assign = 0
					// Input Right ch. enable
					// Right ch. data assign = 1
	ctrl_outw(0x8081,SIOF_SIRDAR0);
					// RFWM = 000
	ctrl_outw(ctrl_inw(SIOF_SIFCTR0) | 0x0000,SIOF_SIFCTR0);
					// RDMAE = 1, RDREQE = 1
	ctrl_outw(ctrl_inw(SIOF_SIIER0) | 0x0900,SIOF_SIIER0);
#endif

					// SCKE = 1
	ctrl_outw(0x8000,SIOF_SICTR0);
					// FSE = 1, TXE = 1
	ctrl_outw(0xc200,SIOF_SICTR0);

#if defined(HAS_RECORD)
					// FSE = 1, RXE = 1
	ctrl_outw(ctrl_inw(SIOF_SICTR0) | 0x0100,SIOF_SICTR0);
#endif

	SNDDBG("SIOF init done.\n");
}

static void ms7720rp_sound_hw_init(void)
{
	ms7720rp_sound_dma_init();
	ms7720rp_sound_siof_init();
}
/* Initialize Function bottom */

/* MACHINE Function top */
static void ms7720rp_sound_open(void)
{
	MOD_INC_USE_COUNT;
	ms7720rp_sound_hw_init();
}

static void ms7720rp_sound_release(void)
{
	MOD_DEC_USE_COUNT;
}

static void* ms7720rp_sound_alloc(unsigned int size, int flags)
{
	unsigned long pa = virt_to_phys(kmalloc(size, flags)) | 0xa0000000;
        return (void*)pa;
}

static void ms7720rp_sound_free(void* ptr, unsigned int size)
{
	unsigned long va = (unsigned long)phys_to_virt((unsigned long)ptr);
	kfree((void*)va);
}

static int __init ms7720rp_sound_irq_init(void)
{
	int err;

	SNDDBG("request\n");

	err = sh7720_request_dma(&ms7720rp_dmasound_play_irq, "dmasound");
	if (err) {
		return 0;
	}

	sh7720_dma_set_callback(ms7720rp_dmasound_play_irq,
				(dma_callback_t)ms7720rp_sound_interrupt);

#if defined(HAS_RECORD)
	err = sh7720_request_dma(&ms7720rp_dmasound_rec_irq, "dmasoundrec");
	if (err) {
	    sh7720_free_dma(ms7720rp_dmasound_play_irq);
	    return 0;
	}

	sh7720_dma_set_callback(ms7720rp_dmasound_rec_irq,
				(dma_callback_t)ms7720rp_sound_rec_interrupt);
#endif

	return 1;
}

#ifdef MODULE
static void ms7720rp_sound_irq_cleanup(void)
{
	ms7720rp_free_dma(MS7720RP_SOUND_DMA_CHANNEL);

#if defined(HAS_RECORD)
	ms7720rp_free_dma(MS7720RP_SOUND_DMA_REC_CHANNEL);
#endif
}
#endif

static void ms7720rp_sound_init(void)
{
	unsigned short speed = 0;

	dmasound.hard = dmasound.soft;
	dmasound.trans_write = &transMS7720RP;

#if defined(HAS_RECORD)
	dmasound.trans_read = &transMS7720RPRead;
#endif

	ms7720rp_sound_silence();

	if (!dmasound.soft.speed)
		return;

	switch (dmasound.soft.speed) {
	case 44100:
		speed = 0x0002;
		break;
	default:
		printk("Not support sampling rate (%d)\n",dmasound.soft.speed);
		speed = 0x0002;
		break;
	}

	SNDDBG("dmasound.soft.speed=%d speed=%04X\n",
	       dmasound.soft.speed, speed);
	
	ctrl_outw(speed, SIOF_SISCR0);
}

static void ms7720rp_sound_silence(void)
{
	sh7720_dma_flush_all(MS7720RP_SOUND_DMA_CHANNEL);
	sh7720_dma_stop(MS7720RP_SOUND_DMA_CHANNEL);

#if defined(HAS_RECORD)
//	sh7720_dma_flush_all(MS7720RP_SOUND_DMA_REC_CHANNEL);
//	sh7720_dma_stop(MS7720RP_SOUND_DMA_REC_CHANNEL)
#endif

	SNDDBG("Stop Play\n");
}

static int ms7720rp_sound_set_format(int format)
{
	int size;

	switch (format) {
	case AFMT_QUERY:
		return dmasound.soft.format;
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
	default:
		format = AFMT_S8;
		size = 8;
	}

	dmasound.soft.format = format;
	dmasound.soft.size   = size;
	if (dmasound.minDev == SND_DEV_DSP) {
		dmasound.dsp.format = format;
		dmasound.dsp.size   = dmasound.soft.size;
	}

	ms7720rp_sound_init();

	return format;
}

#ifdef SOFT_VOLUME
static int ms7720rp_sound_set_volume(int volume)
{
	dmasound.volume_left  = ( volume & 0xff);
	dmasound.volume_right = ((volume & 0xff00) >> 8);

	SNDDBG("set_volume: left=%d, right=%d\n",
				dmasound.volume_left,dmasound.volume_right);
	
	ms7720rp_dmasound_vol = (dmasound.volume_left+dmasound.volume_right)/2;
	if (ms7720rp_dmasound_vol < 0)
		ms7720rp_dmasound_vol = 0;
	if (VOL_MAX < ms7720rp_dmasound_vol)
		ms7720rp_dmasound_vol = VOL_MAX;

	return (dmasound.volume_left | (dmasound.volume_right << 8));
	return 0;
}
#else
static int ms7720rp_sound_set_volume(int volume)
{
	unsigned int wk;

	dmasound.volume_left  = ( volume & 0xff);
	dmasound.volume_right = ((volume & 0xff00) >> 8);

	if (dmasound.volume_left > 100)
		dmasound.volume_left = 100;

	if (dmasound.volume_right > 100)
		dmasound.volume_right = 100;

	SNDDBG("set_volume: left=%d, right=%d\n",
				dmasound.volume_left,dmasound.volume_right);

	wk = (int)(((100 - dmasound.volume_left)  * 0xff) / 100);
	master_volume  = wk << 8;
	wk = (int)(((100 - dmasound.volume_right) * 0xff) / 100);
	master_volume |= wk;
	ms7720rp_sound_set_dac_volume();

	return (dmasound.volume_left | (dmasound.volume_right << 8));
}
#endif

static void ms7720rp_sound_play(void)
{
	if ((write_sq.active) || (write_sq.count <= 0))
		return;

	if ((write_sq.count <= 1) &&
	    (write_sq.rear_size < write_sq.block_size) &&
	    (!write_sq.syncing))
	         return;

	ms7720rp_sound_playNextFrame(1);
}

#if defined(HAS_RECORD)
static void ms7720rp_sound_record(void)
{
    SNDDBG("read_sq.active = %d\n",read_sq.active);

    if (read_sq.active)
	return;
    ms7720rp_sound_recNextFrame(1);
}
#endif

static void __init ms7720rp_mixer_init(void)
{
	SNDDBG("MIXER_INIT\n");
}

static int ms7720rp_mixer_ioctl(u_int cmd, u_long arg)
{
	int	data;

	SNDDBG("MIXER_IOCTL=%x,%lx\n",cmd,arg);

	switch(cmd){
	case SOUND_MIXER_READ_VOLUME:
		return IOCTL_OUT(arg,(dmasound.volume_left |
				     (dmasound.volume_right << 8)));
	case SOUND_MIXER_WRITE_VOLUME:
		IOCTL_IN(arg,data);
		return IOCTL_OUT(arg,dmasound_set_volume(data));
		break;
	default:
		break;
	}

	return -EINVAL;
}

#if defined(HAS_RECORD)
static void ms7720rp_sound_abort_read( void )
{
//    sh7720_dma_flush_all(MS7720RP_SOUND_DMA_CHANNEL);
//    sh7720_dma_stop(MS7720RP_SOUND_DMA_CHANNEL);

    sh7720_dma_flush_all(MS7720RP_SOUND_DMA_REC_CHANNEL);
    sh7720_dma_stop(MS7720RP_SOUND_DMA_REC_CHANNEL);

    SNDDBG("Rec Stop(CH=%d)\n",MS7720RP_SOUND_DMA_REC_CHANNEL);
}
#endif

static MACHINE machMS7720RP = {
	name:		"ms7720rp_sound",
	name2:		"ms7720rp_sound",
	open:		ms7720rp_sound_open,
	release:	ms7720rp_sound_release,
	dma_alloc:	ms7720rp_sound_alloc,
	dma_free:	ms7720rp_sound_free,
	irqinit:	ms7720rp_sound_irq_init,
#ifdef MODULE
	irqcleanup:	ms7720rp_sound_irq_cleanup,
#endif
	init:		ms7720rp_sound_init,
	silence:	ms7720rp_sound_silence, 
	setFormat:	ms7720rp_sound_set_format, 
	setVolume:	ms7720rp_sound_set_volume,
//	setBass:
//	setTreble:
//	setGain:
	play:		ms7720rp_sound_play,
#if defined(HAS_RECORD)
	record:         ms7720rp_sound_record,
#endif
	mixer_init:	ms7720rp_mixer_init,
	mixer_ioctl:	ms7720rp_mixer_ioctl,
//	write_sq_setup:
//	read_sq_setup:
//	sq_open:
//	state_info:
#if defined(HAS_RECORD)
	abort_read:     ms7720rp_sound_abort_read,
#endif
	min_dsp_speed:	8000,
//	max_dsp_speed:
	version:	((DMASOUND_MS7720RP_REVISION << 8) | \
						 DMASOUND_MS7720RP_EDITION),
	hardware_afmts:	(AFMT_MU_LAW | AFMT_A_LAW | AFMT_S8 | AFMT_U8 | AFMT_S16_BE | AFMT_U16_BE | AFMT_S16_LE | AFMT_U16_LE)
//	capabilities:
//	default_hard:
//	default_soft:
};
/* MACHINE Function bottom */

int __init dmasound_ms7720rp_init(void)
{
	ms7720rp_sound_codec_init();

	dmasound.mach = machMS7720RP;

	dmasound.mach.default_hard = def_hard;
	dmasound.mach.default_soft = def_soft;

	return dmasound_init();
}

#ifdef MODULE
static void __exit dmasound_ms7720rp_cleanup(void)
{
	dmasound_deinit();
}
#endif

module_init(dmasound_ms7720rp_init);
#ifdef MODULE
module_exit(dmasound_ms7720rp_cleanup);
#endif
