/*
 * linux/drivers/sound/dmasound/dmasound_ms7727rp.c
 *
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * Based on
 *  linux/drivers/sound/dmasound/dmasound_awacs.c
 *
 * See linux/drivers/sound/dmasound/dmasound_core.c for copyright and credits
 * prior to 28/01/2001
 *
 * Hitachi MS7727RP02 sound driver.
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/soundcard.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/sh7727.h>

#include "dmasound.h"

#define DMASOUND_MS7727RP_REVISION 0
#define DMASOUND_MS7727RP_EDITION 1

//#define DEBUG 1
#undef DEBUG

#define MS7727RP_SOUND_DMA_CHANNEL (ms7727rp_dmasound_play_irq)
#define MS7727RP_SOUND_DMA_REC_CHANNEL (ms7727rp_dmasound_rec_irq)
static int   ms7727rp_dmasound_play_irq = -1;
static int   ms7727rp_dmasound_rec_irq = -1;
#define MS7727RP_SOUND_VOL (ms7727rp_dmasound_vol)
static int   ms7727rp_dmasound_vol = 7;

static void  ms7727rp_sound_hw_init(void);
static void  ms7727rp_sound_open(void);
static void  ms7727rp_sound_release(void);
static void* ms7727rp_sound_alloc(unsigned int size, int flags);
static void  ms7727rp_sound_free(void* , unsigned int);
static int   ms7727rp_sound_irq_init(void);
#ifdef MODULE
static void  ms7727rp_sound_irq_cleanup(void);
#endif
static void  ms7727rp_sound_silence(void);
static void  ms7727rp_sound_init(void);
static int   ms7727rp_sound_set_format(int format);
static int   ms7727rp_sound_set_volumne(int volume);
static void  ms7727rp_sound_playNextFrame(int index);
static void  ms7727rp_sound_recNextFrame(int index);
static void  ms7727rp_sound_play(void);
static void  ms7727rp_sound_record(void);
static void  ms7727rp_sound_interrupt(void* dummy, int size);
static void  ms7727rp_sound_rec_interrupt(void *dummy, int size);


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

static ssize_t ms7727rp_ct_law(const u_char* userPtr, size_t userCount,
			      u_char frame[], ssize_t* frameUsed,
			      ssize_t frameLeft)
{
	short*  table = (dmasound.soft.format == AFMT_MU_LAW) ?
	  dmasound_ulaw2dma16: dmasound_alaw2dma16;
	int     stereo = dmasound.soft.stereo;
	short*  p = (short*) &frame[*frameUsed];
	ssize_t count, used;

	frameLeft >>= 2;
	if (stereo) {
		userCount >>= 1;
	}
	used = count = min((int)userCount, (int)frameLeft);
	while (count > 0) {
		u_char data; int val;

		if (get_user(data, userPtr++))
			return -EFAULT;
		val = ((int)table[data] * MS7727RP_SOUND_VOL / 8);
		*p++ = val; /* Left */
		if (stereo) {
			if (get_user(data, userPtr++))
				return -EFAULT;
			val = ((int)table[data] * MS7727RP_SOUND_VOL / 8);
		}
		*p++ = val; /* Right */
		count--;
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 2 : used;
}

static ssize_t ms7727rp_ct_s8(const u_char* userPtr, size_t userCount,
			    u_char frame[], ssize_t* frameUsed,
			    ssize_t frameLeft)
{
	int     stereo = dmasound.soft.stereo;
	short*  p = (short*) &frame[*frameUsed];
	ssize_t count, used;

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
		val = (((val - 0x80) << 8) * MS7727RP_SOUND_VOL / 8);
		*p++ = val; /* Left */
		if (stereo) {
			if (get_user(data, userPtr++))
				return -EFAULT;
			val = data;
			val = (((val - 0x80) << 8) * MS7727RP_SOUND_VOL / 8);
		}
		*p++ = val; /* Right */
		count--;
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 2 : used;
}

static ssize_t ms7727rp_ct_u8(const u_char* userPtr, size_t userCount,
			    u_char frame[], ssize_t* frameUsed,
			    ssize_t frameLeft)
{
	int     stereo = dmasound.soft.stereo;
	short*  p = (short*) &frame[*frameUsed];
	ssize_t count, used;

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
		val = (((val ^ 0x80) << 8) * MS7727RP_SOUND_VOL / 8);
		*p++ = val; /* Left */
		if (stereo) {
			if (get_user(data, userPtr++))
				return -EFAULT;
			val = data;
			val = (((val ^ 0x80) << 8) * MS7727RP_SOUND_VOL / 8);
		}
		*p++ = val; /* Right */
		count--;
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 2: used;
}

static ssize_t ms7727rp_ct_s16(const u_char* userPtr, size_t userCount,
			     u_char frame[], ssize_t* frameUsed,
			     ssize_t frameLeft)
{
	int     stereo = dmasound.soft.stereo;
	short*  fp = (short*) &frame[*frameUsed];
	ssize_t count, used;

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
			*fp++ = (val * MS7727RP_SOUND_VOL / 8); /* Left */
			*fp++ = (val * MS7727RP_SOUND_VOL / 8); /* Right */
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
			*fp++ = (val1 * MS7727RP_SOUND_VOL / 8); /* Left */
			*fp++ = (val2 * MS7727RP_SOUND_VOL / 8); /* Right */
			count--;
		}
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 4 : used * 2;
}

static ssize_t ms7727rp_ct_u16(const u_char* userPtr, size_t userCount,
			     u_char frame[], ssize_t* frameUsed,
			     ssize_t frameLeft)
{
	int     stereo = dmasound.soft.stereo;
	int     mask   = ((dmasound.soft.format == AFMT_U16_LE) ?
			  0x0080: 0x8000);
	short*  fp  = (short*) &frame[*frameUsed];
	short*  up  = (short*) userPtr;
	ssize_t count, used;

	frameLeft >>= 2;
	userCount >>= ((stereo) ? 2 : 1);
	used = count = min((int)userCount, (int)frameLeft);
	while (count > 0) {
		int data;
		int temp;
		if (get_user(data, up++))
			return -EFAULT;
		data ^= mask;
		*fp++ = (data * MS7727RP_SOUND_VOL / 8); /* Left */
		if (stereo) {
			if (get_user(temp, up++))
				return -EFAULT;
			temp ^= mask;
			data  = temp;
			data ^= mask;
		}
		*fp++ = (data * MS7727RP_SOUND_VOL / 8); /* Right */
		count--;
	}
	*frameUsed += used * 4;
	return (stereo) ? used * 4 : used * 2;
}

static ssize_t ms7727rp_ct_s8_read(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int val, stereo = dmasound.soft.stereo;

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


static ssize_t ms7727rp_ct_u8_read(const u_char *userPtr, size_t userCount,
			  u_char frame[], ssize_t *frameUsed,
			  ssize_t frameLeft)
{
	ssize_t count, used;
	short *p = (short *) &frame[*frameUsed];
	int val, stereo = dmasound.soft.stereo;

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


static ssize_t ms7727rp_ct_s16_read(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
	ssize_t count, used;
	int stereo = dmasound.soft.stereo;
	short *fp = (short *) &frame[*frameUsed];

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

static ssize_t ms7727rp_ct_u16_read(const u_char *userPtr, size_t userCount,
			   u_char frame[], ssize_t *frameUsed,
			   ssize_t frameLeft)
{
	ssize_t count, used;
	int mask = (dmasound.soft.format == AFMT_U16_LE? 0x0080: 0x8000);
	int stereo = dmasound.soft.stereo;
	short *fp = (short *) &frame[*frameUsed];
	short *up = (short *) userPtr;

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



static TRANS transMS7727RP = {
	ms7727rp_ct_law,
	ms7727rp_ct_law,
	ms7727rp_ct_s8,
	ms7727rp_ct_u8,
	ms7727rp_ct_s16,
	ms7727rp_ct_u16,
	ms7727rp_ct_s16,
	ms7727rp_ct_u16
};

static TRANS transMS7727RPRead = {
    ct_s8:       ms7727rp_ct_s8_read,
    ct_u8:       ms7727rp_ct_u8_read,
    ct_s16be:    ms7727rp_ct_s16_read,
    ct_u16be:    ms7727rp_ct_u16_read,
    ct_s16le:    ms7727rp_ct_s16_read,
    ct_u16le:    ms7727rp_ct_u16_read,
};

static void ms7727rp_sound_open(void)
{
	MOD_INC_USE_COUNT;
	ms7727rp_sound_hw_init();
}

static void ms7727rp_sound_release(void)
{
	MOD_DEC_USE_COUNT;
}

static void* ms7727rp_sound_alloc(unsigned int size, int flags)
{
	unsigned long pa = virt_to_phys(kmalloc(size, flags)) | 0xa0000000;
        return (void*)pa;
}

static void ms7727rp_sound_free(void* ptr, unsigned int size)
{
	unsigned long va = (unsigned long)phys_to_virt((unsigned long)ptr);
	kfree((void*)va);
}

static int __init ms7727rp_sound_irq_init(void)
{
	int err;

	err = sh7727_request_dma(&ms7727rp_dmasound_play_irq, "dmasound");
	if (err) {
		return 0;
	}

	sh7727_dma_set_callback(ms7727rp_dmasound_play_irq,
				(dma_callback_t)ms7727rp_sound_interrupt);

	err = sh7727_request_dma(&ms7727rp_dmasound_rec_irq, "dmasoundrec");
	if (err) {
	    sh7727_free_dma(ms7727rp_dmasound_play_irq);
	    return 0;
	}

	sh7727_dma_set_callback(ms7727rp_dmasound_rec_irq,
				(dma_callback_t)ms7727rp_sound_rec_interrupt);




	return 1;
}

#ifdef MODULE
static void ms7727rp_sound_irq_cleanup(void)
{
	sh7727_free_dma(MS7727RP_SOUND_DMA_CHANNEL);
	sh7727_free_dma(MS7727RP_SOUND_DMA_REC_CHANNEL);
}
#endif

static void ms7727rp_sound_silence(void)
{
	sh7727_dma_flush_all(MS7727RP_SOUND_DMA_CHANNEL);
	sh7727_dma_stop(MS7727RP_SOUND_DMA_CHANNEL);
}

static void ms7727rp_sound_playNextFrame(int index)
{
	u_char* start;
	u_long  size;

	start = write_sq.buffers[write_sq.front];
	size  = ((write_sq.count == index) ?
			write_sq.rear_size : write_sq.block_size);

	sh7727_dma_queue_buffer(MS7727RP_SOUND_DMA_CHANNEL,
				NULL,
				(unsigned long)start,
				size / 4);
	
	write_sq.front = (write_sq.front+1) % write_sq.max_count;
	write_sq.active++;
}

static void ms7727rp_sound_play(void)
{
	if ((write_sq.active) || (write_sq.count <= 0))
		return;

	if ((write_sq.count <= 1) &&
	    (write_sq.rear_size < write_sq.block_size) &&
	    (!write_sq.syncing))
	         return;

	ms7727rp_sound_playNextFrame(1);
}

static void ms7727rp_sound_recNextFrame(int index)
{
	u_char * start;
	u_long size;

	start = read_sq.buffers[read_sq.rear];
	size = read_sq.block_size;

	sh7727_dma_queue_buffer(MS7727RP_SOUND_DMA_REC_CHANNEL,
				 NULL,
				 (unsigned long)start,
				 size / 4);
	read_sq.active++;
}

static void ms7727rp_sound_record(void)
{
	if (read_sq.active)
		return;
	ms7727rp_sound_recNextFrame(1);
}

static void ms7727rp_sound_interrupt(void* id, int size)
{
        if (!write_sq.active) {
	  	WAKE_UP(write_sq.sync_queue);
	}
	else {
		write_sq.active = 0;
	}


	write_sq.count--;

	ms7727rp_sound_play();

	WAKE_UP(write_sq.action_queue);
}

static void ms7727rp_sound_rec_interrupt(void *id, int size)
{
	unsigned short sistr;
    
	read_sq.rear = (read_sq.rear+1) % read_sq.max_active;
	if (read_sq.active) {
		read_sq.active = 0;
	}
    
	ms7727rp_sound_record();
	sistr = ctrl_inw(SISTR);

	ctrl_outw(sistr & 0x0003,SISTR);

	WAKE_UP(read_sq.action_queue);
}


static void ms7727rp_sound_abort_read( void )
{
	sh7727_dma_flush_all(MS7727RP_SOUND_DMA_REC_CHANNEL);
	sh7727_dma_stop(MS7727RP_SOUND_DMA_REC_CHANNEL);
}

static void ms7727rp_sound_hw_init(void)
{
	dma_device_t dev;
	dma_device_t recdev;

	ctrl_outb(ctrl_inw(SCPDR) & 0x13, SCPDR);
	ctrl_outw(ctrl_inw(SCPCR) & 0x030f, SCPCR);

	ctrl_outb(ctrl_inw(PKDR) & 0x01, PKDR);
	ctrl_outw((ctrl_inw(PKCR) & 0xfffc) | 0x001, PKCR);

	dev.sar    = 0;
	dev.dar    = SITDR;
	dev.dmatcr = 0;
	dev.chcr   = 0x1814;
	dev.way    = 0;
	sh7727_dma_set_device(MS7727RP_SOUND_DMA_CHANNEL, dev);

#if 0
	recdev.sar    = SIRDR;
	recdev.dar    = 0;
	recdev.dmatcr = 0;
	recdev.chcr   = 0x4814;
	recdev.way    = 1;
	sh7727_dma_set_device(MS7727RP_SOUND_DMA_REC_CHANNEL, recdev);
#endif

	ctrl_outw(0x0001, SICTR);  udelay(200);
	ctrl_outw(0x000f, SISTR);  udelay(200);

	ctrl_outw(0x1100, SIIER);  udelay(200); /* ... */
	ctrl_outw(0xdc00, SIMDR);  udelay(200); /* */
	ctrl_outw(0x8081, SITDAR); udelay(200); /* */
	ctrl_outw(0x8080, SIRDAR); udelay(200); /* */
	ctrl_outw(0x0000, SICDAR); udelay(200); /* */
	ctrl_outw(0xe000, SIFCTR); udelay(200); /* */
	ctrl_outw(0xc000, SICTR);  udelay(200); /* */
	ctrl_outw(0xc003, SICTR);  udelay(200); /* */
	ctrl_outw(0xc200, SICTR);  udelay(200); /* ... */
}

static void ms7727rp_sound_init(void)
{
	unsigned short speed = 0;

	dmasound.hard = dmasound.soft;
	dmasound.trans_write = &transMS7727RP;
	dmasound.trans_read = &transMS7727RPRead;

	ms7727rp_sound_silence();

	if (!dmasound.soft.speed)
		return;

#if 1
	speed = 0x0102;
#else
	switch (dmasound.soft.speed) {
	case 44100: speed = 0x0102; break;
	case 22050: speed = 0x0204; break;
	case 11025: speed = 0x0408; break;
	case 8000:  speed = 0x058E; break;
	default:
		speed = (((8000 * 0x058e) / dmasound.soft.speed) & 0x1F00);
		break;
	}
#endif

	ctrl_outw(speed, SISCR); /* udelay(200); */
}

static int ms7727rp_sound_set_format(int format)
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

	ms7727rp_sound_init();

	return format;
}

static int ms7727rp_sound_set_volumne(int volume)
{
	dmasound.volume_left  = (volume & 0xff);
	dmasound.volume_right = ((volume & 0xff00) >> 8);

#if 1
	printk("set_volume: left=%d, right=%d\n",
	       dmasound.volume_left,
	       dmasound.volume_right);
#endif
	
	ms7727rp_dmasound_vol =
	  ((dmasound.volume_left + dmasound.volume_right) / 2) * 80 / 100;
	return (dmasound.volume_left | (dmasound.volume_right << 8));
	return 0;
}

static MACHINE machMS7727RP = {
	name:		"ms7727rp_sound",
	name2:		"ms7727rp_sound",
	open:		ms7727rp_sound_open,
	release:	ms7727rp_sound_release,
	dma_alloc:	ms7727rp_sound_alloc,
	dma_free:	ms7727rp_sound_free,
	irqinit:	ms7727rp_sound_irq_init,
#ifdef MODULE
	irqcleanup:	ms7727rp_sound_irq_cleanup,
#endif
	init:		ms7727rp_sound_init,
	silence:	ms7727rp_sound_silence, 
	setFormat:	ms7727rp_sound_set_format, 
	setVolume:	ms7727rp_sound_set_volumne,
	play:		ms7727rp_sound_play,
	record:         ms7727rp_sound_record,
	abort_read:     ms7727rp_sound_abort_read,

	min_dsp_speed:	8000,
	max_dsp_speed:	48000,
	version:	((DMASOUND_MS7727RP_REVISION<<8)|DMASOUND_MS7727RP_EDITION),
#if 0
	mixer_ioctl:
	write_sq_setup:
	read_sq_setup:
	min_dsp_speed:
#endif
};

int __init dmasound_ms7727rp_init(void)
{
	dmasound.mach = machMS7727RP;
	return dmasound_init();
}

#ifdef MODULE
static void __exit dmasound_ms7727rp_cleanup(void)
{
	dmasound_deinit();
}
#endif

module_init(dmasound_ms7727rp_init);
#ifdef MODULE
module_exit(dmasound_ms7727rp_cleanup);
#endif

MODULE_DESCRIPTION("Hitachi MS7727RP02 sound driver");
MODULE_LICENSE("GPL");
