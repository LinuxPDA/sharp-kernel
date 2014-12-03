/*
 * drivers/sound/collie-tc35143.c
 *
 * Toshiba TC35143 Audio Device Driver for Collie (SHARP)
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 * 
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 * drivers/sound/sa1100-audio.c
 * Common audio handling for the SA11x0 processor
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 *
 * This module handles the generic buffering/DMA/mmap audio interface for
 * codecs connected to the SA1100 chip.  All features depending on specific
 * hardware implementations like supported audio formats or samplerates are
 * relegated to separate specific modules.
 *
 * Change Log:
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>

#include <linux/pm.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/semaphore.h>
#include <asm/dma.h>
#include <asm/ucb1200.h>

#include <asm/arch/tc35143.h>


#undef DEBUG
////#define DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


/* 
 * Definitions 
 */

#define AUDIO_NAME		"TC35143F"
#define AUDIO_NAME_VERBOSE	"TC35143F audio driver"
#define AUDIO_VERSION_STRING	"version 0.01"

#define AUDIO_FMT_MASK		(AFMT_S16_LE|AFMT_U8)
#define AUDIO_FMT_DEFAULT	(AFMT_S16_LE)
////#define AUDIO_CHANNELS_DEFAULT	1
#define AUDIO_CHANNELS_DEFAULT	2
////#define AUDIO_RATE_DEFAULT	11080
#define AUDIO_RATE_DEFAULT	22050
////#define AUDIO_IGAIN_DEFAULT	3
#define AUDIO_IGAIN_DEFAULT	3
////#define AUDIO_IAMP_DEFAULT	0xf
#define AUDIO_IAMP_DEFAULT	0x7
#define AUDIO_NBFRAGS_DEFAULT	8
#define AUDIO_FRAGSIZE_DEFAULT	8192

extern int collie_buzzer_dev_init(void);
extern void collie_buzzer_dev_release(void);

#undef HARD_MUTE_CTRL_DISABLE
#define COLLIE_AIDIO_PLAYING	(SCP_REG_GPWR & SCP_GPCR_PA15)


/*
 * SA1110 GPIO (17)
 */
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
						/* no hardware for audio in */
#elif defined(CONFIG_COLLIE_TR1)
#define COLLIE_GPIO_MIC         GPIO_GPIO (17)  /* MIC contorol        */
#else
#define COLLIE_GPIO_MIC         GPIO_GPIO (17)  /* MIC contorol        */
#endif  /* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */

/* 
 * Buffer Management
 */

typedef struct {
	int size;		/* buffer size */
	char *start;		/* points to actual buffer */
	dma_addr_t dma_addr;	/* physical buffer address */
	struct semaphore sem;	/* down before touching the buffer */
	int master;		/* owner for buffer allocation, contain size when true */
} audio_buf_t;

typedef struct {
	audio_buf_t *buffers;	/* pointer to audio buffer structures */
	audio_buf_t *buf;	/* current buffer used by read/write */
	u_int buf_idx;		/* index for the pointer above... */
	u_int fragsize;		/* fragment i.e. buffer size */
	u_int nbfrags;		/* nbr of fragments i.e. buffers */
	dmach_t dma_ch;		/* DMA channel ID */
} audio_stream_t;

#if 0
static audio_stream_t output_stream;
#endif
static audio_stream_t input_stream;
extern int collie_tc35143f_irq;		/* collie_buzzer.c */
extern int collie_under_recording;	/* collie_buzzer.c */
extern int collie_under_playing;	/* collie_buzzer.c */

#define NEXT_BUF(_s_,_b_) { \
	(_s_)->_b_##_idx++; \
	(_s_)->_b_##_idx %= (_s_)->nbfrags; \
	(_s_)->_b_ = (_s_)->buffers + (_s_)->_b_##_idx; }


/* Current specs for incoming audio data */
static u_int audio_rate;
static u_int audio_igain;
static u_int audio_iamp;
static int audio_channels;
static int audio_fmt;
static u_int audio_fragsize;
static u_int audio_nbfrags;

#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
static int collie_hard_mute = 1;
#endif  /* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */

static int audio_rd_refcount;	/* nbr of concurrent open() for recording */
#if 0
static int audio_wr_refcount;	/* nbr of concurrent open() for playback */
#endif
#if 0
#define audio_active		(audio_rd_refcount || audio_wr_refcount)
#else
#define audio_active		(audio_rd_refcount)
#endif

static int audio_dev_dsp;	/* registered ID for DSP device */
static int audio_dev_mixer;	/* registered ID for mixer device */
static int audio_mix_modcnt;	/* mixer mods count */

#ifdef CONFIG_PM
static struct pm_dev *audio_pm_dev; /* registered PM device */
#endif


/* 
 * This function frees all buffers 
 */

static void audio_clear_buf(audio_stream_t * s)
{
	DPRINTK("audio_clear_buf\n");

	/* ensure DMA won't run anymore */
/**** DEBUG *****
if (s == &output_stream)
	sa1100_dma_flush_all(collie_tc35143f_irq);
else
**** DEBUG *****/
	sa1100_dma_flush_all(s->dma_ch);

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
}


/* This function allocates the buffer structure array and buffer data space
 * according to the current number of fragments and fragment size.
 */

static int audio_setup_buf(audio_stream_t * s)
{
	int frag;
	int dmasize = 0;
	char *dmabuf = 0;
	dma_addr_t dmaphys = 0;

	if (s->buffers)
		return -EBUSY;

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
		DPRINTK("buf %d: start %p dma %p\n", frag, b->start, b->dma_addr);

		dmabuf += s->fragsize;
		dmaphys += s->fragsize;
		dmasize -= s->fragsize;
	}

	s->buf_idx = 0;
	s->buf = &s->buffers[0];
	DPRINTK("audio_setup_buf : return\n");

	return 0;

      err:
	printk(AUDIO_NAME ": unable to allocate audio memory\n ");
	audio_clear_buf(s);
	return -ENOMEM;
}


/*
 * DMA callback functions
 */

#if 0
static void audio_dmaout_done_callback(void *buf_id, int size)
{
	audio_buf_t *b = (audio_buf_t *) buf_id;
	/* 
	 * Current buffer is sent: wake up any process waiting for it.
	 */
	up(&b->sem);
	/* And any process polling on write. */
	wake_up(&b->sem.wait);
}
#endif

static void audio_dmain_done_callback(void *buf_id, int size)
{
	audio_buf_t *b = (audio_buf_t *) buf_id;
	/* 
	 * Current buffer is full: set its size and wake up any 
	 * process waiting for it.
	 */
	b->size = size;
	up(&b->sem);
	/* And any process polling on read. */
	wake_up(&b->sem.wait);
}


#if 0
static int audio_sync(struct file *file)
{
	audio_stream_t *s = &output_stream;
	audio_buf_t *b = s->buf;

	DPRINTK("audio_sync\n");

	if (!s->buffers)
		return 0;

	/* Send half-full buffers */
	if (b->size != 0) {
		down(&b->sem);
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, b->size);
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
	if (down_interruptible(&b->sem))
		return -EINTR;
	up(&b->sem);
	return 0;
}
#endif

#if 0
static int audio_write(struct file *file, const char *buffer,
		       size_t count, loff_t * ppos)
{
	const char *buffer0 = buffer;
	audio_stream_t *s = &output_stream;
	int chunksize, ret = 0;

	DPRINTK("audio_write: count=%d\n", count);

	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		break;
	default:
		return -EPERM;
	}

	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;

	/* be sure to have a full sample byte count */
	count &= ~0x03;

	while (count > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become free */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&b->sem))
				break;
		}

		/* Feed the current buffer */
		chunksize = s->fragsize - b->size;
		if (chunksize > count)
			chunksize = count;
		DPRINTK("write %d to %d\n", chunksize, s->buf_idx);
		if (copy_from_user(b->start + b->size, buffer, chunksize)) {
			up(&b->sem);
			return -EFAULT;
		}
		b->size += chunksize;

		buffer += chunksize;
		count -= chunksize;
		if (b->size < s->fragsize) {
			up(&b->sem);
			break;
		}

		/* Send current buffer to dma */
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, b->size);
		b->size = 0;	/* indicate that the buffer has been sent */
		NEXT_BUF(s, buf);
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	DPRINTK("audio_write: return=%d\n", ret);
	return ret;
}
#endif


static int audio_recording(audio_stream_t * s)
{
	int i;
	unsigned int reg_b;

	if (!s->buffers) {
		if (audio_setup_buf(s))
			return -ENOMEM;

		/*
		 * Set TC35143 Audio in enable
		 */
////		DPRINTK("audio_recording : audio in enable\n");
////		reg_b  = ucb1200_read_reg(TC35143_CONTROL_REG_B);
////		reg_b &= ~(TC35143_VADMUTE);
////		reg_b |=  (TC35143_VIN_EN | TC35143_VCLP_CLR);
////		ucb1200_write_reg(TC35143_CONTROL_REG_B, reg_b);

		/*
		 * We must ensure there is an output stream at any time while 
		 * recording since this is how the TC35143 gets its clock.
		 * So if there is no playback data to send, the output DMA will
		 * spin with all zeroes.
		 */
		DPRINTK("audio_recording : sa1100_dma_set_spin\n");
/***** DEBUG *****
printk("audio_recording : collie_tc35143f_irq = %x\n", collie_tc35143f_irq);
***** DEBUG *****/
#if 0
		sa1100_dma_set_spin(output_stream.dma_ch,
					    (dma_addr_t) FLUSH_BASE_PHYS, 2048);
#else
		sa1100_dma_set_spin(collie_tc35143f_irq,
					    (dma_addr_t) FLUSH_BASE_PHYS, 2048);
#endif

		/* 
		 * Since we just allocated all buffers, we must send them to
		 * the DMA code before receiving data.
		 */
		DPRINTK("audio_recording : for loop\n");
		for (i = 0; i < s->nbfrags; i++) {
			audio_buf_t *b = s->buf;
			down(&b->sem);
			sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
						b->dma_addr, s->fragsize);
			NEXT_BUF(s, buf);
		}
		DPRINTK("audio_recording : End for loop\n");

		/*
		 * Set TC35143 Audio in enable
		 */
		DPRINTK("audio_recording : audio in enable\n");
		reg_b  = ucb1200_read_reg(TC35143_CONTROL_REG_B);
		reg_b &= ~(TC35143_VADMUTE);
		reg_b |=  (TC35143_VIN_EN | TC35143_VCLP_CLR);
		ucb1200_write_reg(TC35143_CONTROL_REG_B, reg_b);
/***** DEBUG *****
printk("audio_recording : TC35143 Reg7 = %x\n", ucb1200_read_reg(TC35143_CONTROL_REG_A));
printk("audio_recording : TC35143 Reg8 = %x\n", ucb1200_read_reg(TC35143_CONTROL_REG_B));
printk("audio_recording : TC35143 Regd = %x\n", ucb1200_read_reg(TC35143_MODE_REG));
printk("audio_recording : SA1100 GPIO = %x\n", GPLR);
printk("audio_recording : SA1100 MCCR = %x\n", Ser4MCCR0);
printk("audio_recording : scoop GPIO = %x\n", SCP_REG_GPRR);
printk("audio_recording : LoCoMo GPIO = %x\n", LCM_GPO);
***** DEBUG *****/
	}
	return 0;
}


static int audio_read(struct file *file, char *buffer,
		      size_t count, loff_t * ppos)
{
	char *buffer0 = buffer;
	audio_stream_t *s = &input_stream;
	int chunksize, ret = 0;

	DPRINTK("audio_read: count=%d\n", count);

	switch (file->f_flags & O_ACCMODE) {
	case O_RDONLY:
	case O_RDWR:
		break;
	default:
		return -EPERM;
	}

	ret = audio_recording(s);
	if (ret)
		return ret;

	/* be sure to have a full sample byte count */
	count &= ~0x03;

	DPRINTK("audio_read : Start while loop\n");
	while (count > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become full */
		if (file->f_flags & O_NONBLOCK) {
			DPRINTK("audio_read : down_trylock\n");
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				break;
		} else {
			DPRINTK("audio_read : down_interruptible\n");
			ret = -ERESTARTSYS;
			if (down_interruptible(&b->sem))
				break;
		}

		DPRINTK("audio_read : if audio_channels\n");
		/* Grab data from the current buffer */
		chunksize = b->size;
		if (audio_fmt&AFMT_U8) { // 8bit
		  if (chunksize > count*2)
		    chunksize = count*2;
		} else { // 16bit
		  if (chunksize > count)
		    chunksize = count;
		}
		DPRINTK("read %d from %d\n", chunksize, s->buf_idx);
		if (audio_fmt&AFMT_U8) { // 8bit
		  int ct;
		  short *srcdata = (short*)(b->start + s->fragsize - b->size);
		  char *dstdata = buffer;
		  for (ct=0; ct<chunksize/2; ct++) {
		    put_user((char)(((*srcdata)>>8)+128),dstdata);
		    srcdata++; dstdata++;
		  }
		  b->size -= chunksize;
		  buffer += chunksize/2;
		  count -= chunksize/2;
		} else { // 16bit
		  if (copy_to_user(buffer,
				   b->start + s->fragsize - b->size,
				   chunksize)) {
		    up(&b->sem);
		    return -EFAULT;
		  }
		  b->size -= chunksize;
		  buffer += chunksize;
		  count -= chunksize;
		}
		if (b->size > 0) {
			up(&b->sem);
			break;
		}

		/* Make current buffer available for DMA again */
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, s->fragsize);
		NEXT_BUF(s, buf);
	}
	DPRINTK("audio_read : End while loop\n");

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	DPRINTK("audio_read: return=%d\n", ret);
	return ret;
}


static unsigned int audio_poll(struct file *file,
			       struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	int i;
	int ret;

	DPRINTK("audio_poll(): mode=%s%s\n",
		(file->f_mode & FMODE_READ) ? "r" : "",
		(file->f_mode & FMODE_WRITE) ? "w" : "");

	if (file->f_mode & FMODE_READ) {
		/* Start audio input if not already active */
		ret = audio_recording(&input_stream);
		if (ret < 0)
			return ret;
		poll_wait(file, &input_stream.buf->sem.wait, wait);
	}

#if 0
	if (file->f_mode & FMODE_WRITE) {
		if (!output_stream.buffers
		    && audio_setup_buf(&output_stream)) 
			return -ENOMEM;
		poll_wait(file, &output_stream.buf->sem.wait, wait);
	}
#endif

	if (file->f_mode & FMODE_READ) {
		for (i = 0; i < input_stream.nbfrags; i++) {
			if (atomic_read(&input_stream.buffers[i].sem.count) > 0)
				mask |= POLLIN | POLLRDNORM;
		}
	}
#if 0
	if (file->f_mode & FMODE_WRITE) {
		for (i = 0; i < output_stream.nbfrags; i++) {
			if (atomic_read(&output_stream.buffers[i].sem.count) > 0)
				mask |= POLLOUT | POLLWRNORM;
		}
	}
#endif

	DPRINTK("audio_poll() returned mask of %s%s\n",
		(mask & POLLIN) ? "r" : "", 
		(mask & POLLOUT) ? "w" : "");

	return mask;
}


static loff_t audio_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}


static int mixer_ioctl(struct inode *inode, struct file *file,
		       uint cmd, ulong arg)
{
	int ret;
	long val = 0;
	unsigned int data;

	/* 
	 * Dispatch based on command.
	 * Exit with break if modifications occured. 
	 */
	switch (cmd) {
	case SOUND_MIXER_INFO:
	    {
		mixer_info info;
		strncpy(info.id, "TC35143F", sizeof(info.id));
		strncpy(info.name, "Toshiba TC35143F", sizeof(info.name));
		info.modify_counter = audio_mix_modcnt;
		return copy_to_user((void *)arg, &info, sizeof(info));
	    }

	case SOUND_OLD_MIXER_INFO:
	    {
		_old_mixer_info info;
		strncpy(info.id, "TC35143F", sizeof(info.id));
		strncpy(info.name, "Toshiba TC35143F", sizeof(info.name));
		return copy_to_user((void *)arg, &info, sizeof(info));
	    }

	case SOUND_MIXER_READ_DEVMASK:
		val = (SOUND_MASK_MIC);
		return put_user(val, (long *) arg);

	case SOUND_MIXER_READ_RECMASK:
		val = (SOUND_MASK_MIC |
		       SOUND_MASK_IGAIN);
		return put_user(val, (long *) arg);

	case SOUND_MIXER_READ_STEREODEVS:
		return put_user(0, (long *) arg);

	case SOUND_MIXER_READ_CAPS:
//		val = SOUND_CAP_EXCL_INPUT;
//		return put_user(val, (long *) arg);
		return put_user(0, (long *) arg);


#if 0
	case SOUND_MIXER_WRITE_VOLUME:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		break;

	case SOUND_MIXER_READ_VOLUME:
		return put_user(val, (long *) arg);
#endif

	case SOUND_MIXER_WRITE_TREBLE:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		break;

	case SOUND_MIXER_READ_TREBLE:
		return put_user(0, (long *) arg);

	case SOUND_MIXER_WRITE_BASS:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		break;

	case SOUND_MIXER_READ_BASS:
		return put_user(0, (long *) arg);

	case SOUND_MIXER_WRITE_LINE1:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		break;

	case SOUND_MIXER_READ_LINE1:
		return put_user(0, (long *) arg);

	case SOUND_MIXER_WRITE_LINE2:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		break;

	case SOUND_MIXER_READ_LINE2:
		return put_user(0, (long *) arg);

	case SOUND_MIXER_WRITE_MIC:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;

		data  = ucb1200_read_reg(TC35143_CONTROL_REG_A);
		data &= ~(TC35143_VAMP_MASK);
		data |= ((val & 0xf) << 7);
		ucb1200_write_reg(TC35143_CONTROL_REG_A, data);

		audio_iamp = val;
		break;

	case SOUND_MIXER_READ_MIC:
////		data = ucb1200_read_reg(TC35143_CONTROL_REG_A);
////		data = ((data >> 7) & 0xf);
////		return put_user(data, (long *) arg);
		return put_user(audio_iamp, (long *) arg);

	case SOUND_MIXER_WRITE_IGAIN:
		ret = get_user(val, (long *) arg);
		if (ret)

			return ret;
		data  = ucb1200_read_reg(TC35143_CONTROL_REG_A);
		data &= ~(TC35143_VGAIN_MASK);
		data |= ((val & 0x3) << 11);
		ucb1200_write_reg(TC35143_CONTROL_REG_A, data);

		audio_igain = val;
		break;

	case SOUND_MIXER_READ_IGAIN:
////		data = ucb1200_read_reg(TC35143_CONTROL_REG_A);
////		data = ((data >> 11) & 0x3);
////		return put_user(data, (long *) arg);
		return put_user(audio_igain, (long *) arg);

	case SOUND_MIXER_WRITE_RECLEV:
	case SOUND_MIXER_READ_RECLEV:
	case SOUND_MIXER_AGC:
	case SOUND_MIXER_WRITE_RECSRC:
	case SOUND_MIXER_READ_RECSRC:
	default:
		return -ENOSYS;
	}

	audio_mix_modcnt++;
	return 0;
}

static void audio_set_dsp_speed(long val)
{
	unsigned int reg_a;
	unsigned int reg_b;
	unsigned int reg_mode;

	reg_b  = ucb1200_read_reg(TC35143_CONTROL_REG_B);
	reg_b &= ~(TC35143_VIN_EN);
	ucb1200_write_reg(TC35143_CONTROL_REG_B, reg_b);

	reg_a  = ucb1200_read_reg(TC35143_CONTROL_REG_A);
	reg_a &= ~(TC35143_VDIV_MASK);
	reg_mode  = ucb1200_read_reg(TC35143_MODE_REG);
	reg_mode &= ~(TC35143_VCOF_MASK);

	switch(val) {
	case 8000:
		reg_a |= TC35143_VDIV_8KHZ;
		reg_mode |= TC35143_VCOF_11_OR_8KHZ;
		break;
	case 16000:
		reg_a |= TC35143_VDIV_16KHZ;
		reg_mode |= TC35143_VCOF_22_OR_16KHZ;
		break;
	case 22150:
	case 22050:
		reg_a |= TC35143_VDIV_22KHZ;
		reg_mode |= TC35143_VCOF_22_OR_16KHZ;
		break;
	case 11080:
	case 11025:
	default:
		reg_a |= TC35143_VDIV_11KHZ;
		reg_mode |= TC35143_VCOF_11_OR_8KHZ;
		break;
	}
	ucb1200_write_reg(TC35143_CONTROL_REG_A, reg_a);
	ucb1200_write_reg(TC35143_MODE_REG, reg_mode);
	
        audio_rate = val;
}

static int audio_ioctl(struct inode *inode, struct file *file,
		       uint cmd, ulong arg)
{
	long val;

	/* dispatch based on command */
	switch (cmd) {
	case SNDCTL_DSP_SETFMT:
		get_user(val, (long *) arg);
		if (val & AUDIO_FMT_MASK) {
			audio_fmt = val;
			break;
		} else
			return -EINVAL;

	case SNDCTL_DSP_CHANNELS:
	case SNDCTL_DSP_STEREO:
		get_user(val, (long *) arg);
		if (cmd == SNDCTL_DSP_STEREO)
			val = val ? 2 : 1;
#if 0
		if (val != 1 && val != 2)
			return -EINVAL;
#else
		if (val != 1)
			return -EINVAL;
#endif
		audio_channels = val;
		break;

	case SOUND_PCM_READ_CHANNELS:
		/* return current number of channels */
		put_user(audio_channels, (long *) arg);
		break;

	case SNDCTL_DSP_SPEED:
		get_user(val, (long *) arg);
		audio_set_dsp_speed(val);
		put_user(val, (long *) arg);
		break;

	case SOUND_PCM_READ_RATE:
		put_user(audio_rate, (long *) arg);
		break;

	case SNDCTL_DSP_GETFMTS:
		put_user(AUDIO_FMT_MASK, (long *) arg);
		break;

	case SNDCTL_DSP_GETBLKSIZE:
		put_user(audio_fragsize, (long *) arg);
		break;

	case SNDCTL_DSP_SETFRAGMENT:
		if (input_stream.buffers)
			return -EBUSY;
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
		if (audio_setup_buf(&input_stream))
			return -ENOMEM;
		break;

	case SNDCTL_DSP_SYNC:
#if 0
		return audio_sync(file);
#else
		return -ENOSYS;
#endif

	case SNDCTL_DSP_GETOSPACE:
#if 0
	    {
		audio_stream_t *s = &output_stream;
		audio_buf_info *inf = (audio_buf_info *) arg;
		int err = verify_area(VERIFY_WRITE, inf, sizeof(*inf));
		int i;
		int frags = 0, bytes = 0;

		if (err)
			return err;
		for (i = 0; i < s->nbfrags; i++) {
			if (atomic_read(&s->buffers[i].sem.count) > 0) {
				if (s->buffers[i].size == 0) frags++;
				bytes += s->fragsize - s->buffers[i].size;
			}
		}
		put_user(frags, &inf->fragments);
		put_user(s->nbfrags, &inf->fragstotal);
		put_user(s->fragsize, &inf->fragsize);
		put_user(bytes, &inf->bytes);
		break;
	    }
#else
		return -ENOSYS;
#endif

	case SNDCTL_DSP_GETISPACE:
	    {
		audio_stream_t *s = &input_stream;
		audio_buf_info *inf = (audio_buf_info *) arg;
		int err = verify_area(VERIFY_WRITE, inf, sizeof(*inf));
		int i;
		int frags = 0, bytes = 0;

		if (err)
			return err;
		for (i = 0; i < s->nbfrags; i++) {
			if (atomic_read(&s->buffers[i].sem.count) > 0) {
				if (s->buffers[i].size == s->fragsize) frags++;
				bytes += s->buffers[i].size;
			}
		}
		put_user(frags, &inf->fragments);
		put_user(s->nbfrags, &inf->fragstotal);
		put_user(s->fragsize, &inf->fragsize);
		put_user(bytes, &inf->bytes);
		break;
	    }

	case SNDCTL_DSP_RESET:
		switch (file->f_flags & O_ACCMODE) {
		case O_RDONLY:
		case O_RDWR:
#if 0
			sa1100_dma_set_spin(output_stream.dma_ch, 0, 0);
#else
			sa1100_dma_set_spin(collie_tc35143f_irq, 0, 0);
#endif
			audio_clear_buf(&input_stream);
		}
#if 0
		switch (file->f_flags & O_ACCMODE) {
		case O_WRONLY:
		case O_RDWR:
			audio_clear_buf(&output_stream);
		}
#endif
		return 0;

	case SNDCTL_DSP_POST:
	case SNDCTL_DSP_SUBDIVIDE:
	case SNDCTL_DSP_NONBLOCK:
	case SNDCTL_DSP_GETCAPS:
	case SNDCTL_DSP_GETTRIGGER:
	case SNDCTL_DSP_SETTRIGGER:
	case SNDCTL_DSP_GETIPTR:
	case SNDCTL_DSP_GETOPTR:
	case SNDCTL_DSP_MAPINBUF:
	case SNDCTL_DSP_MAPOUTBUF:
	case SNDCTL_DSP_SETSYNCRO:
	case SNDCTL_DSP_SETDUPLEX:
		return -ENOSYS;
	default:
		/* Maybe this is meant for the mixer (As per OSS Docs) */
		return mixer_ioctl(inode, file, cmd, arg);
	}

	return 0;
}


#if 1
#if 0
static int __init audio_init_dma(audio_stream_t * s, char *desc)
#else
static int audio_init_dma(audio_stream_t * s, char *desc)
#endif
{
	int err;

#if 0
	if (s == &output_stream) {
#if 0
		err = sa1100_request_dma(&s->dma_ch, desc, DMA_Ser4MCP0Wr);
		err = sa1100_request_dma(&s->dma_ch, desc, DMA_Ser4SSPWr);

		if (err)
			return err;

		sa1100_dma_set_callback(s->dma_ch,
					audio_dmaout_done_callback);
////		sa1100_dma_set_device(s->dma_ch, DMA_Ser4MCP0Wr);
#endif
	} else {
#endif
		err = sa1100_request_dma(&s->dma_ch, desc, DMA_Ser4MCP0Rd);

		if (err)
			return err;

		sa1100_dma_set_callback(s->dma_ch,
					audio_dmain_done_callback);
////		sa1100_dma_set_device(s->dma_ch, DMA_Ser4MCP0Rd);
#if 0
	}
#endif
	return 0;
}
#endif

#if 1
static int audio_clear_dma(audio_stream_t * s)
{
/**** DEBUG *****
if (s == &output_stream)
	return 0;
else
**** DEBUG *****/
	sa1100_free_dma(s->dma_ch);
	return 0;
}
#endif

static void audio_power_on(void);

static int audio_open(struct inode *inode, struct file *file)
{
	int cold = !audio_active;

	DPRINTK("audio_open\n");

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		if (audio_rd_refcount)
			return -EBUSY;
		audio_rd_refcount++;
#if 0
	} else if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		if (audio_wr_refcount)
			return -EBUSY;
		audio_wr_refcount++;
#endif
	} else if ((file->f_flags & O_ACCMODE) == O_RDWR) {
#if 0
		if (audio_rd_refcount || audio_wr_refcount)
			return -EBUSY;
		audio_rd_refcount++;
		audio_wr_refcount++;
#else
		if (audio_rd_refcount)
			return -EBUSY;
		audio_rd_refcount++;
#endif
	} else
		return -EINVAL;

	while(collie_under_playing) {
		schedule();
	}

/***** DEBUG *****/
	collie_under_recording = 1;
/***** DEBUG *****/

	if (cold) {
		collie_buzzer_dev_release();

#if 1
		/* Acquire and initialize DMA */
#if 0
		if (audio_init_dma(&output_stream, "TC35143 out") ||
		    audio_init_dma(&input_stream, "TC35143 in")) {
			audio_clear_dma(&output_stream);
#else
		if (audio_init_dma(&input_stream, "TC35143 in")) {
#endif
			audio_clear_dma(&input_stream);
			printk( KERN_WARNING AUDIO_NAME_VERBOSE
					": unable to get DMA channels\n" );
			collie_under_recording = 0;
			return -EBUSY;
		}
#endif

		audio_channels = AUDIO_CHANNELS_DEFAULT;
		audio_fragsize = AUDIO_FRAGSIZE_DEFAULT;
		audio_nbfrags = AUDIO_NBFRAGS_DEFAULT;
		audio_fmt = AUDIO_FMT_DEFAULT; // 16bit
#if 0
		audio_clear_buf(&output_stream);
#endif
		audio_clear_buf(&input_stream);
		audio_power_on();
	}

	MOD_INC_USE_COUNT;
	return 0;
}


static int mixer_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;
	return 0;
}


static void audio_power_off(void);

static int audio_release(struct inode *inode, struct file *file)
{
#if 0
	int asd;
#endif

	DPRINTK("audio_release\n");

	switch (file->f_flags & O_ACCMODE) {
	case O_RDONLY:
	case O_RDWR:
		if (audio_rd_refcount == 1) {
#if 0
			sa1100_dma_set_spin(output_stream.dma_ch, 0, 0);
#else
			sa1100_dma_set_spin(collie_tc35143f_irq, 0, 0);
#endif
			audio_clear_buf(&input_stream);
			audio_rd_refcount = 0;
		}
	}

#if 0
	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		if (audio_wr_refcount == 1) {
			audio_sync(file);
			audio_clear_buf(&output_stream);
			audio_wr_refcount = 0;
		}
	}
#endif

	if (!audio_active)
		audio_power_off();

#if 0
	audio_rate = AUDIO_RATE_DEFAULT;
	audio_set_dsp_speed(audio_rate);

	asd = Collie_Sampling_value(audio_rate);
	Ser4MCCR0 &= ~( MCCR0_ARE | 0x7f );
	Ser4MCCR0 |= asd;
#else
////	Ser4MCCR0 &= ~( MCCR0_ARE | 0x7f );

	sa1100_free_dma(input_stream.dma_ch);
	collie_buzzer_dev_init();

	collie_under_recording = 0;
#endif

	MOD_DEC_USE_COUNT;
	return 0;
}


static int mixer_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}


static struct file_operations TC35143_dsp_fops = {
	llseek:		audio_llseek,
#if 0
	write:		audio_write,
#endif
	read:		audio_read,
	poll:		audio_poll,
	ioctl:		audio_ioctl,
	open:		audio_open,
	release:	audio_release
};

static struct file_operations TC35143_mixer_fops = {
	ioctl:		mixer_ioctl,
	open:		mixer_open,
	release:	mixer_release
};

static inline void Collie_DAC_off(void)
{
	if (!COLLIE_AIDIO_PLAYING) {
		/* DAC OFF */
		LCM_GPD &= ~(LCM_GPIO_DAC_ON);  /* set up output */
		LCM_GPO &= ~(LCM_GPIO_DAC_ON);
	
		/* LoCoMo GPIO disable */
		LCM_GPE &= ~(LCM_GPIO_DAC_ON);
	}
}

static inline void Collie_DAC_on(void)
{
	if (!(LCM_GPO & LCM_GPIO_DAC_ON)) {
		/* LoCoMo GPIO enable */
		LCM_GPE &= ~LCM_GPIO_DAC_ON;

		/* DAC ON */
		LCM_GPD &= ~(LCM_GPIO_DAC_ON);  /* set up output */
		LCM_GPO |= LCM_GPIO_DAC_ON;
		{
			/* wait for DAC power */
			int time = jiffies + 20;
			while (jiffies <= time)
				schedule();
		}
	}
}

static inline void Collie_hard_mute_init(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
	SCP_REG_GPCR |=  (SCP_GPCR_PA14);
#ifdef HARD_MUTE_CTRL_DISABLE
	SCP_REG_GPWR |= (SCP_GPCR_PA14);
#else
	SCP_REG_GPWR &= ~(SCP_GPCR_PA14);
#endif	/* HARD_MUTE_CTRL_DISABLE */
	collie_hard_mute = 1;
#if !defined(CONFIG_COLLIE_TR1) && !defined(CONFIG_COLLIE_DEV)
	{
		int time = jiffies + 5;
		while (jiffies <= time)
			schedule();
	}
#endif
#endif  /* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_hard_mute_on(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
#ifndef HARD_MUTE_CTRL_DISABLE
///	if (!collie_hard_mute) {
		SCP_REG_GPWR &= ~(SCP_GPCR_PA14);
		collie_hard_mute = 1;
#if !defined(CONFIG_COLLIE_TR1) && !defined(CONFIG_COLLIE_DEV)
		{
			int time = jiffies + 5;
			while (jiffies <= time)
				schedule();
		}
#endif
///	}
#endif	/* HARD_MUTE_CTRL_DISABLE */
#endif  /* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_hard_mute_off(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
#ifndef HARD_MUTE_CTRL_DISABLE
///	if (collie_hard_mute) {
	if(COLLIE_AIDIO_PLAYING) {
		SCP_REG_GPWR |= (SCP_GPCR_PA14);
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
///	}
#endif	/* HARD_MUTE_CTRL_DISABLE */
#endif  /* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_MIC_init(void)
{
#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
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
#endif  /* !CONFIG_COLLIE_TS && !CONFIG_COLLIE_TR0 */
}

static inline void Collie_MIC_on(void)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)

#elif defined(CONFIG_COLLIE_TR1)
	GPSR = ( COLLIE_GPIO_MIC );
#else
	GPCR = ( COLLIE_GPIO_MIC );
#endif  /* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

static inline void Collie_MIC_off(void)
{
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)

#elif defined(CONFIG_COLLIE_TR1)
       	GPCR = ( COLLIE_GPIO_MIC );
#else
	GPSR = ( COLLIE_GPIO_MIC );
#endif  /* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

///#define CODEC_ASD_NUMERATOR     (9216000 / ( 32 * 11025 ))
static int Collie_Sampling_value(int rate)
{
///	int asd = CODEC_ASD_NUMERATOR; 
	int asd;
	asd = (9216000 / ( 32 * rate )); 
////	asd = (12000000 / ( 32 * rate )); 
	DPRINTK("Collie_Sampling_value %x\n",asd);
	return asd;
}

static void audio_power_on()
{
	unsigned int reg_a;
	unsigned int reg_b;
	unsigned int reg_mode;
	int asd;

	DPRINTK("audio_power_on\n");
	
	reg_a = ucb1200_read_reg(TC35143_CONTROL_REG_A);
	reg_b = ucb1200_read_reg(TC35143_CONTROL_REG_B);
	reg_mode = ucb1200_read_reg(TC35143_MODE_REG);

	reg_a &= ~(TC35143_VINSEL_MASK | TC35143_VGAIN_MASK | TC35143_VAMP_MASK);
	reg_a |=  (TC35143_VINSEL_VBIN2 | ((audio_igain & 0x3) << 11)
					| ((audio_iamp  & 0xf) <<  7));
	reg_b &= ~(TC35143_VIN_EN | TC35143_VCLP_CLR);
	reg_b |=  (TC35143_VADMUTE);
	reg_mode |= (TC35143_VOFFCAN);

	ucb1200_write_reg(TC35143_CONTROL_REG_B, reg_b );
	ucb1200_write_reg(TC35143_MODE_REG     , reg_mode );
	ucb1200_write_reg(TC35143_CONTROL_REG_A, reg_a );

	audio_set_dsp_speed(audio_rate);

	asd = Collie_Sampling_value(audio_rate);
/***** DEBUG *****
printk("audio_power_on : audio_rate = %d, asd = %d\n", audio_rate, asd);
***** DEBUG *****/
	Ser4MCCR0 &= ~(MCCR0_MCE | 0x7f);
	Ser4MCCR0 |= ( MCCR0_ARE | MCCR0_ADM | MCCR0_ECS | asd);
	Ser4MCCR0 |= MCCR0_MCE; 
	Ser4MCSR   = 0xffffffff;

	Collie_hard_mute_on();
	Collie_DAC_on();
	Collie_MIC_on();
}

static void audio_power_off(void)
{
	unsigned int reg_a;
	unsigned int reg_b;
	unsigned int reg_mode;

	DPRINTK("audio_power_off\n");
	
	/* power down audio */
	reg_a = ucb1200_read_reg(TC35143_CONTROL_REG_A);
	reg_b = ucb1200_read_reg(TC35143_CONTROL_REG_B);
	reg_mode = ucb1200_read_reg(TC35143_MODE_REG);

	reg_a &= ~(TC35143_VDIV_MASK);
	reg_a |=   TC35143_VDIV_11KHZ;
	reg_b &= ~(TC35143_VIN_EN);
	reg_b |=   TC35143_VADMUTE;
	reg_mode &= ~(TC35143_VOFFCAN | TC35143_VCOF_MASK);
	reg_mode |=  (TC35143_VCOF_11_OR_8KHZ);

	ucb1200_write_reg(TC35143_CONTROL_REG_B, reg_b );
	ucb1200_write_reg(TC35143_MODE_REG     , reg_mode );
	ucb1200_write_reg(TC35143_CONTROL_REG_A, reg_a );

	Collie_MIC_off();
	Collie_DAC_off();
	Collie_hard_mute_off();
}


#ifdef CONFIG_PM
static int audio_tc35143_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	unsigned int reg_b;

	switch (req) {
	case PM_SUSPEND: /* enter D1-D3 */
		sa1100_dma_sleep(input_stream.dma_ch);
#if 0
		sa1100_dma_sleep(output_stream.dma_ch);
#endif

		if (audio_active) 
			audio_power_off();
		break;
	case PM_RESUME:  /* enter D0 */
		if (audio_active) {
			Collie_hard_mute_init();
			Collie_MIC_init();

			audio_power_on();

			reg_b  = ucb1200_read_reg(TC35143_CONTROL_REG_B);
			reg_b &= ~(TC35143_VADMUTE);
			reg_b |=  (TC35143_VIN_EN | TC35143_VCLP_CLR);
			ucb1200_write_reg(TC35143_CONTROL_REG_B, reg_b);
		}

		sa1100_dma_wakeup(input_stream.dma_ch);
#if 0
		sa1100_dma_wakeup(output_stream.dma_ch);
#endif
                break;
	}
	return 0;
}
#endif


#if 0
#if 0
static int __init audio_init_dma(audio_stream_t * s, char *desc)
#else
static int audio_init_dma(audio_stream_t * s, char *desc)
#endif
{
	int err;

	if (s == &output_stream) {
#if 0
		err = sa1100_request_dma(&s->dma_ch, desc, DMA_Ser4MCP0Wr);
		err = sa1100_request_dma(&s->dma_ch, desc, DMA_Ser4SSPWr);

		if (err)
			return err;

		sa1100_dma_set_callback(s->dma_ch,
					audio_dmaout_done_callback);
////		sa1100_dma_set_device(s->dma_ch, DMA_Ser4MCP0Wr);
#endif
	} else {
		err = sa1100_request_dma(&s->dma_ch, desc, DMA_Ser4MCP0Rd);

		if (err)
			return err;

		sa1100_dma_set_callback(s->dma_ch,
					audio_dmain_done_callback);
////		sa1100_dma_set_device(s->dma_ch, DMA_Ser4MCP0Rd);
	}
	return 0;
}
#endif

#if 0
static int audio_clear_dma(audio_stream_t * s)
{
/**** DEBUG *****/
if (s == &output_stream)
	return 0;
else
/**** DEBUG *****/
	sa1100_free_dma(s->dma_ch);
	return 0;
}
#endif


int __init audio_tc35143_init(void)
{
	collie_under_recording = 0;

#if 0
	/* Acquire and initialize DMA */
#if 1
	if (audio_init_dma(&output_stream, "TC35143 out") ||
	    audio_init_dma(&input_stream, "TC35143 in")) {
		audio_clear_dma(&output_stream);
#else
	if (audio_init_dma(&input_stream, "TC35143 in")) {
		output_stream.dma_ch = -1;
#endif
		audio_clear_dma(&input_stream);
		printk( KERN_WARNING AUDIO_NAME_VERBOSE
			": unable to get DMA channels\n" );
		return -EBUSY;
	}
#endif

	Collie_hard_mute_init();
	Collie_MIC_init();

	audio_rate  = AUDIO_RATE_DEFAULT;
	audio_igain = AUDIO_IGAIN_DEFAULT;
	audio_iamp  = AUDIO_IAMP_DEFAULT;

	/* register devices */
	audio_dev_dsp = register_sound_dsp(&TC35143_dsp_fops, 1);
	audio_dev_mixer = register_sound_mixer(&TC35143_mixer_fops, 1);

#ifdef CONFIG_PM
        audio_pm_dev = pm_register(PM_SYS_DEV, 0, audio_tc35143_pm_callback);
#endif

	printk(AUDIO_NAME_VERBOSE " initialized\n");

	return 0;
}

module_init(audio_tc35143_init);


void __exit audio_tc35143_exit(void)
{
#ifdef CONFIG_PM
	if (audio_pm_dev)
		pm_unregister(audio_pm_dev);
#endif
	/* unregister driver and IRQ */
	unregister_sound_dsp(audio_dev_dsp);
	unregister_sound_mixer(audio_dev_mixer);
#if 0
	audio_clear_dma(&output_stream);
#endif
	audio_clear_dma(&input_stream);
	printk(AUDIO_NAME_VERBOSE " unloaded\n");
}

module_exit(audio_tc35143_exit);
