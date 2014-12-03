/*
 * Philips UDA1341 Audio Device Driver for SA1100 Linux
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * Portions are Copyright (C) 2000 Lernout & Hauspie Speech Products, N.V.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial release.
 *
 * 2000-06-10	Erik Bunce	Add initial poll support.
 * 
 * 2000-07-??	George France	Bitsy support.
 *
 * 2000-08-19	Erik Bunce	More inline w/ OSS API and UDA1341 docs
 * 				including fixed AGC and audio source handling
 *
 * 2000-08-22	Nicolas Pitre	Removed all DMA stuff. Now using the 
 * 				generic SA1100 DMA interface.
 *
 * 2000-09-04	John Dorsey     SA-1111 Serial Audio Controller support.
 *
 * 2000-10-30	Richard Fan	Pangolin support.
 * 
 * 2000-11-30	Nicolas Pitre	- Validation of opened instances;
 * 				- Code for mono samples compatibility;
 * 				- Power handling at open/release time instead
 * 				  of driver load/unload;
 * 				- More mixer functionalities.
 *
 * 2000-12-1 	Chester Kuo	,Add Freebird-1.1 audio support
 *
 * 2000-12-13	Deborah Wallach	- Fixed power handling for iPAQ/bitsy
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


#undef DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


/* 
 * Definitions 
 */

#define AUDIO_NAME		"UDA1341"
#define AUDIO_NAME_VERBOSE	"UDA1341 audio driver"
#define AUDIO_VERSION_STRING	"version 0.90"

#define AUDIO_FMT_MASK		(AFMT_S16_LE)
#define AUDIO_FMT_DEFAULT	(AFMT_S16_LE)
#define AUDIO_CHANNELS_DEFAULT	2
#define AUDIO_RATE_DEFAULT	44100
#define AUDIO_NBFRAGS_DEFAULT	8
#define AUDIO_FRAGSIZE_DEFAULT	8192


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

static audio_stream_t output_stream;
static audio_stream_t input_stream;

#define NEXT_BUF(_s_,_b_) { \
	(_s_)->_b_##_idx++; \
	(_s_)->_b_##_idx %= (_s_)->nbfrags; \
	(_s_)->_b_ = (_s_)->buffers + (_s_)->_b_##_idx; }


/* Current specs for incoming audio data */
static u_int audio_rate;
static int audio_channels;
static int audio_fmt;
static u_int audio_fragsize;
static u_int audio_nbfrags;


static int audio_rd_refcount;	/* nbr of concurrent open() for recording */
static int audio_wr_refcount;	/* nbr of concurrent open() for playback */
#define audio_active		(audio_rd_refcount || audio_wr_refcount)

static int audio_dev_dsp;	/* registered ID for DSP device */
static int audio_dev_mixer;	/* registered ID for mixer device */
static int audio_mix_modcnt;	/* mixer mods count */

#ifdef CONFIG_PM
static struct pm_dev *audio_pm_dev; /* registered PM device */
#endif


/*
 * GPIO based L3 bus support.
 *
 * This provides control of Philips L3 type devices. 
 * GPIO lines are used for clock, data and mode pins.
 *
 * Note: On Assabet the L3 pins are shared with I2C devices. This should not 
 * present any problems as long as an I2C start sequence is not generated. 
 * This is defined as a 1->0 transition on the data lines when the clock is 
 * high. It is critical this code only allow data transitions when the clock
 * is low. This is always legal in L3.
 *
 * The IIC interface requires the clock and data pin to be LOW when idle. We
 * must make sure we leave them in this state.
 *
 * It appears the read data is generated on the falling edge of the clock
 * and should be held stable during the clock high time.
 */

/* 
 * L3 bus GPIO pin definitions
 */

/* FIXME: should be made of variables instead... */
#ifdef CONFIG_SA1100_ASSABET
#define L3_DataPin	GPIO_GPIO(15)
#define L3_ClockPin	GPIO_GPIO(18)
#define L3_ModePin	GPIO_GPIO(17)
#endif
#ifdef CONFIG_SA1100_BITSY
#define L3_DataPin	GPIO_BITSY_L3_DATA
#define L3_ClockPin	GPIO_BITSY_L3_CLOCK
#define L3_ModePin	GPIO_BITSY_L3_MODE
#endif
#ifdef CONFIG_SA1100_PANGOLIN
#define L3_DataPin		GPIO_GPIO(15)
#define L3_ClockPin		GPIO_GPIO(18)
#define L3_ModePin		GPIO_GPIO(17)
#endif
#ifdef CONFIG_SA1100_YOPY
#define L3_ModePin	GPIO_YOPY_L3_MODE
#define L3_ClockPin	GPIO_YOPY_L3_CLOCK
#define L3_DataPin	GPIO_YOPY_L3_DATA
#endif
#ifdef CONFIG_SA1100_FREEBIRD
#define L3_DataPin		GPIO_FREEBIRD_L3_DATA	
#define L3_ModePin		GPIO_FREEBIRD_L3_MODE	
#define L3_ClockPin	GPIO_FREEBIRD_L3_CLOCK	
#endif

#define USE_SA1111 ((machine_is_assabet() && machine_has_neponset()) \
	 || machine_is_jornada720())

/* 
 * L3 setup and hold times (expressed in us)
 */
#define L3_DataSetupTime 1	/* 190 ns */
#define L3_DataHoldTime  1	/*  30 ns */
#define L3_ModeSetupTime 1	/* 190 ns */
#define L3_ModeHoldTime  1	/* 190 ns */
#define L3_ClockHighTime 1	/* 250 ns (min is 64*fs, 35us @ 44.1 Khz) */
#define L3_ClockLowTime  1	/* 250 ns (min is 64*fs, 35us @ 44.1 Khz) */
#define L3_HaltTime      1	/* 190 ns */

/*
 * Grab control of the IIC/L3 shared pins
 */
static inline void L3_acquirepins(void)
{
	GPSR = (L3_ModePin | L3_ClockPin | L3_DataPin);
	GPDR |= (L3_ModePin | L3_ClockPin | L3_DataPin);
}

/*
 * Release control of the IIC/L3 shared pins
 * For bitsy, release control of the pins since they will drive the UDC high
 */
static inline void L3_releasepins(void)
{
	GPDR &= ~(L3_ModePin | L3_ClockPin | L3_DataPin);
}

/*
 * Initialize the interface
 */
static void L3_init(void)
{
	if(!machine_has_neponset()){
		GAFR &= ~(L3_DataPin | L3_ClockPin | L3_ModePin);
		L3_releasepins();
	}
}

/*
 * Get a bit. The clock is high on entry and on exit. Data is read after
 * the clock low time has expired.
 */
static inline int L3_getbit(void)
{
	int data;

	GPCR = L3_ClockPin;
	udelay(L3_ClockLowTime);

	data = (GPLR & L3_DataPin) ? 1 : 0;

	GPSR = L3_ClockPin;
	udelay(L3_ClockHighTime);

	return data;
}

/*
 * Send a bit. The clock is high on entry and on exit. Data is sent only
 * when the clock is low (I2C compatibility).
 */
static inline void L3_sendbit(int bit)
{
	GPCR = L3_ClockPin;

	if (bit & 1)
		GPSR = L3_DataPin;
	else
		GPCR = L3_DataPin;

	/* Assumes L3_DataSetupTime < L3_ClockLowTime */
	udelay(L3_ClockLowTime);

	GPSR = L3_ClockPin;
	udelay(L3_ClockHighTime);
}

/*
 * Send a byte. The mode line is set or pulsed based on the mode sequence
 * count. The mode line is high on entry and exit. The mod line is pulsed
 * before the second data byte and before ech byte thereafter.
 */
static void L3_sendbyte(char data, int mode)
{
	int i;

	switch (mode) {
	case 0:		/* Address mode */
		GPCR = L3_ModePin;
		break;
	case 1:		/* First data byte */
		break;
	default:		/* Subsequent bytes */
		GPCR = L3_ModePin;
		udelay(L3_HaltTime);
		GPSR = L3_ModePin;
		break;
	}

	udelay(L3_ModeSetupTime);

	for (i = 0; i < 8; i++)
		L3_sendbit(data >> i);

	if (mode == 0)		/* Address mode */
		GPSR = L3_ModePin;

	udelay(L3_ModeHoldTime);
}

/*
 * Get a byte. The mode line is set or pulsed based on the mode sequence
 * count. The mode line is high on entry and exit. The mod line is pulsed
 * before the second data byte and before each byte thereafter. This
 * function is never valid with mode == 0 (address cycle) as the address
 * is always sent on the bus, not read.
 * L3_DataPin must be set to an input before calling this function!
 */
static char L3_getbyte(int mode)
{
	char data = 0;
	int i;

	switch (mode) {
	case 0:		/* Address mode - never valid */
		break;
	case 1:		/* First data byte */
		break;
	default:		/* Subsequent bytes */
		GPCR = L3_ModePin;
		udelay(L3_HaltTime);
		GPSR = L3_ModePin;
		break;
	}

	udelay(L3_ModeSetupTime);

	for (i = 0; i < 8; i++)
		data |= (L3_getbit() << i);

	udelay(L3_ModeHoldTime);

	return data;
}

/*
 * Write data to a device on the L3 bus. The address is passed as well as
 * the data and length. The length written is returned. The register space
 * is encoded in the address (low two bits are set and device address is
 * in the upper 6 bits).
 */
static int L3_write(char addr, char *data, int len)
{
	int bytes = len;

	/* 
	 * We better not try using L3 if the chip isn't powered up.
	 * When it'll get power, the complete set will be sent over anyway.
	 */
	if (!audio_active)
		return 0;

	DPRINTK("%s(0x%x, %d)\n", __FUNCTION__, addr, len);

	if(USE_SA1111){  /* SA-1111 L3 Control Bus */
#ifdef CONFIG_SA1111
		if( len > 1 ){
		  	SACR1 |= SACR1_L3MB;
			while( (len--) > 1 ){
			  	L3_CAR = addr;
			  	L3_CDR = *data++;
			  	while((SASR0 & SASR0_L3WD) == 0)
				  	mdelay(1);
				SASCR = SASCR_DTS;
			}
		}
		SACR1 &= ~SACR1_L3MB;
	  	L3_CAR = addr;
		L3_CDR = *data;
		while((SASR0 & SASR0_L3WD) == 0)
		  	mdelay(1);
		SASCR = SASCR_DTS;
#endif
	} else {

	  	int mode = 0;

		L3_acquirepins();
		L3_sendbyte(addr, mode++);
		while(len--)
		  	L3_sendbyte(*data++, mode++);
		L3_releasepins();
	}

	return bytes;
}

/*
 * Read data from a device on the L3 bus. The address is passed as well as
 * the data and length. The length read is returned. The register space
 * is encoded in the address (low two bits are set and device address is
 * in the upper 6 bits).
 */
static int L3_read(char addr, char *data, int len)
{
	int bytes = len;

	/* 
	 * We better not try using L3 if the chip isn't powered up.
	 */
	if (!audio_active)
		return -EIO;

	DPRINTK("%s(0x%x, %d)\n", __FUNCTION__, addr, len);

	if(USE_SA1111){  /* SA-1111 L3 Control Bus */
#ifdef CONFIG_SA1111
		if( len > 1 ){
		  	SACR1 |= SACR1_L3MB;
			while( (len--) > 1 ){
			  	L3_CAR = addr;
			  	while((SASR0 & SASR0_L3RD) == 0)
				  	mdelay(1);
				*data++ = L3_CDR;
				SASCR = SASCR_RDD;
			}
		}
		SACR1 &= ~SACR1_L3MB;
		L3_CAR = addr;
		while((SASR0 & SASR0_L3RD) == 0)
		  	mdelay(1);
		*data = L3_CDR;
		SASCR = SASCR_RDD;
#endif
	} else {

		int mode = 0;
		
		L3_acquirepins();
		L3_sendbyte(addr, mode++);
		GPDR &= ~(L3_DataPin);
		while(len--)
		  	*data++ = L3_getbyte(mode++);
		L3_releasepins();
	}

	return bytes;
}


/*
 * UDA1341 L3 address and command types
 */
#define UDA1341_L3Addr	5
#define UDA1341_DATA0	0
#define UDA1341_DATA1	1
#define UDA1341_STATUS	2


/* 
 * UDA1341 internal state variables.  
 * Those are initialized according to what it should be after a reset.
 */

/* UDA1341 status settings */

#define UDA_STATUS0_IF_I2S      0
#define UDA_STATUS0_IF_LSB16    1
#define UDA_STATUS0_IF_LSB18    2
#define UDA_STATUS0_IF_LSB20    3
#define UDA_STATUS0_IF_MSB      4
#define UDA_STATUS0_IF_MSB16    5
#define UDA_STATUS0_IF_MSB18    6
#define UDA_STATUS0_IF_MSB20    7

#define UDA_STATUS0_SC_512FS   0
#define UDA_STATUS0_SC_384FS   1
#define UDA_STATUS0_SC_256FS   2

static struct {
	u_int DC_filter:1;	/* DC filter */
	u_int input_fmt:3;	/* data input format */
	u_int system_clk:2;	/* system clock frequency */
	u_int reset:1;		/* reset */
	const u_int select:1;	/* must be set to 0 */
} STATUS_0 = {
0, UDA_STATUS0_IF_LSB16, UDA_STATUS0_SC_256FS, 0, 0};

static struct {
	u_int DAC_on:1;		/* DAC powered */
	u_int ADC_on:1;		/* ADC powered */
	u_int double_speed:1;	/* double speed playback */
	u_int DAC_pol:1;	/* polarity of DAC */
	u_int ADC_pol:1;	/* polarity of ADC */
	u_int ADC_gain:1;	/* gain of ADC */
	u_int DAC_gain:1;	/* gain of DAC */
	const u_int select:1;	/* must be set to 1 */
} STATUS_1 = {
1, 1, 0, 0, 0, 0, 0, 1};

/* UDA1341 direct control settings */

static struct {
	u_int volume:6;		/* volume control */
	const u_int select:2;	/* must be set to 0 */
} DATA0_0 = {
0, 0};

static struct {
	u_int treble:2;
	u_int bass:4;
	const u_int select:2;	/* must be set to 1 */
} DATA0_1 = {
0, 0, 1};

static struct {
	u_int mode:2;		/* mode switch */
	u_int mute:1;
	u_int deemphasis:2;
	u_int peak_detect:1;
	const u_int select:2;	/* must be set to 2 */
} DATA0_2 = {
0, 0, 0, 1, 2};

/* DATA0 extended programming registers */

static struct {
	const u_int ext_addr:3;	/* must be set to 0 */
	const u_int select1:5;	/* must be set to 24 */
	u_int ch1_gain:5;	/* mixer gain channel 1 */
	const u_int select2:3;	/* must be set to 7 */
} DATA0_ext0 = {
0, 24, 4, 7};

static struct {
	const u_int ext_addr:3;	/* must be set to 1 */
	const u_int select1:5;	/* must be set to 24 */
	u_int ch2_gain:5;	/* mixer gain channel 2 */
	const u_int select2:3;	/* must be set to 7 */
} DATA0_ext1 = {
1, 24, 4, 7};

static struct {
	const u_int ext_addr:3;	/* must be set to 2 */
	const u_int select1:5;	/* must be set to 24 */
	u_int mixer_mode:2;
	u_int mic_level:3;	/* MIC sensitivity level */
	const u_int select2:3;	/* must be set to 7 */
} DATA0_ext2 = {
2, 24, 0, 1, 7};

static struct {
	const u_int ext_addr:3;	/* must be set to 4 */
	const u_int select1:5;	/* must be set to 24 */
	u_int ch2_igain_l:2;	/* input amplifier gain channel 2 (bits 1-0) */
	const u_int reserved:2;	/* must be set to 0 */
	u_int AGC_ctrl:1;	/* AGC control */
	const u_int select2:3;	/* must be set to 7 */
} DATA0_ext4 = {
4, 24, 0 & 3, 0, 0, 7};

static struct {
	const u_int ext_addr:3;	/* must be set to 5 */
	const u_int select1:5;	/* must be set to 24 */
	u_int ch2_igain_h:5;	/* input amplifier gain channel 2 (bits 6-2) */
	const u_int select2:3;	/* must be set to 7 */
} DATA0_ext5 = {
5, 24, 0 >> 2, 7};

static struct {
	const u_int ext_addr:3;	/* must be set to 6 */
	const u_int select1:5;	/* must be set to 24 */
	u_int AGC_level:2;	/* AGC output level */
	u_int AGC_const:3;	/* AGC time constant */
	const u_int select2:3;	/* must be set to 7 */
} DATA0_ext6 = {
6, 24, 0, 0, 7};

static struct {
	u_int peak:6;		/* peak level value */
} DATA1 = { 0 };


/* 
 * This function frees all buffers 
 */

static void audio_clear_buf(audio_stream_t * s)
{
	DPRINTK("audio_clear_buf\n");

	/* ensure DMA won't run anymore */
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
		DPRINTK("buf %d: start %p dma %p\n", frag, b->start,
			b->dma_addr);

		dmabuf += s->fragsize;
		dmaphys += s->fragsize;
		dmasize -= s->fragsize;
	}

	s->buf_idx = 0;
	s->buf = &s->buffers[0];

	return 0;

      err:
	printk(AUDIO_NAME ": unable to allocate audio memory\n ");
	audio_clear_buf(s);
	return -ENOMEM;
}


/*
 * DMA callback functions
 */

static void audio_dmaout_done_callback(void *buf_id, int size)
{
	audio_buf_t *b = (audio_buf_t *) buf_id;
	/* 
	 * Current buffer is sent: wake up any process waiting for it.
	 */
	up(&b->sem);
	/* And any process polling on write. */
	wake_up(&b->sem.wait);

#define CHROME 1 /* What else are you going to do with 32 LEDs? */
#ifdef CHROME
	if (machine_is_assabet() && machine_has_neponset()) {
#ifdef CONFIG_ASSABET_NEPONSET
	  	L3_read( (UDA1341_L3Addr<<2)|UDA1341_DATA1, 
			 (char*)&DATA1, 1 );
		LEDS = (1 << (DATA1.peak >> 1)) - 1;
#endif
	}
#endif
}

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


static inline int copy_from_user_mono_stereo (char *to, const char *from, int count)
{
	/*
	 * Copy samples from user space while converting from mono.
	 * To do so, we duplicate each samples.
	 */
	u_int *dst = (u_int *)to;
	const char *end = from + count;

	if (verify_area(VERIFY_READ, from, count))
		return -EFAULT;

	/* deal with half-word aligned buffers */
	if ((int)from & 0x2) {
		u_int v;
		__get_user(v, (const u_short *)from); from += 2;
		*dst++ = v | (v << 16);
	}

	while (from < end-2) {
		u_int v, x, y;
		__get_user (v, (const u_int *)from); from += 4;
		x = v << 16;
		x |= x >> 16;
		y = v >> 16;
		y |= y << 16;
		*dst++ = x;
		*dst++ = y;
	}

	if (from < end) {
		u_int v;
		__get_user(v, (const u_short *)from);
		*dst = v | (v << 16);
	}

	return 0;
}


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
		if (audio_channels == 2) {
			chunksize = s->fragsize - b->size;
			if (chunksize > count)
				chunksize = count;
			DPRINTK("write %d to %d\n", chunksize, s->buf_idx);
			if (copy_from_user(b->start + b->size, buffer, chunksize)) {
				up(&b->sem);
				return -EFAULT;
			}
			b->size += chunksize;
		} else {
			/* 
			 * This is to help clueless apps out there that will
			 * complain if mono isn't supported.  Since this
			 * hardware is stereo only we must duplicate all 
			 * samples.
			 */
			chunksize = (s->fragsize - b->size) >> 1;
			if (chunksize > count)
				chunksize = count;
			DPRINTK("write %d to %d\n", chunksize*2, s->buf_idx);
			if (copy_from_user_mono_stereo(b->start + b->size, 
						       buffer, chunksize)) {
				up(&b->sem);
				return -EFAULT;
			}
			b->size += chunksize*2;
		}

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


static inline int copy_to_user_stereo_mono (char *to, const char *from, int count)
{
	/*
	 * Copy samples to user space while converting to mono.
	 * To do so, we preserve only the left samples.
	 */
	const u_int *src = (u_int *)from;
	const char *end = to + count;

	if (verify_area(VERIFY_WRITE, to, count))
		return -EFAULT;

	/* deal with half-word aligned buffers */
	if ((int)to & 0x2) {
		u_int v = *src++;
		__put_user(v, (u_short *)to); to += 2;
	}

	while (to < end-2) {
		u_int x, y, v;
		x = *(const u_short *)src; src++;
		y = *(const u_short *)src; src++;
		v = x | (y << 16);
		__put_user (v, (u_int *)to); to += 4;
	}

	if (to < end) {
		u_int v = *src;
		__put_user(v, (u_short *)to);
	}

	return 0;
}


static int audio_recording(audio_stream_t * s)
{
	int i;

	if (!s->buffers) {
		if (audio_setup_buf(s))
			return -ENOMEM;

		/*
		 * We must ensure there is an output stream at any time while 
		 * recording since this is how the UDA1341 gets its clock.
		 * So if there is no playback data to send, the output DMA will
		 * spin with all zeroes.
		 */
		if (!USE_SA1111)
		  	sa1100_dma_set_spin(output_stream.dma_ch,
					    (dma_addr_t) FLUSH_BASE_PHYS, 2048);

		/* 
		 * Since we just allocated all buffers, we must send them to
		 * the DMA code before receiving data.
		 */
		for (i = 0; i < s->nbfrags; i++) {
			audio_buf_t *b = s->buf;
			down(&b->sem);
			sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
						b->dma_addr, s->fragsize);
			NEXT_BUF(s, buf);
		}
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

	while (count > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become full */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&b->sem))
				break;
		}

		/* Grab data from the current buffer */
		if (audio_channels == 2) {
			chunksize = b->size;
			if (chunksize > count)
				chunksize = count;
			DPRINTK("read %d from %d\n", chunksize, s->buf_idx);
			if (copy_to_user(buffer,
					 b->start + s->fragsize - b->size,
					 chunksize)) {
				up(&b->sem);
				return -EFAULT;
			}
			b->size -= chunksize;
		} else {
			chunksize = b->size>>1;
			if (chunksize > count)
				chunksize = count;
			DPRINTK("read %d from %d\n", chunksize*2, s->buf_idx);
			if (copy_to_user_stereo_mono(buffer,
				b->start + s->fragsize - b->size, chunksize)) {
				up(&b->sem);
				return -EFAULT;
			}
			b->size -= chunksize*2;
		}

		buffer += chunksize;
		count -= chunksize;
		if (b->size > 0) {
			up(&b->sem);
			break;
		}

		/* Make current buffer available for DMA again */
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, s->fragsize);
		NEXT_BUF(s, buf);
	}

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

	if (file->f_mode & FMODE_WRITE) {
		if (!output_stream.buffers
		    && audio_setup_buf(&output_stream)) 
			return -ENOMEM;
		poll_wait(file, &output_stream.buf->sem.wait, wait);
	}

	if (file->f_mode & FMODE_READ) {
		for (i = 0; i < input_stream.nbfrags; i++) {
			if (atomic_read(&input_stream.buffers[i].sem.count) > 0)
				mask |= POLLIN | POLLRDNORM;
		}
	}
	if (file->f_mode & FMODE_WRITE) {
		for (i = 0; i < output_stream.nbfrags; i++) {
			if (atomic_read(&output_stream.buffers[i].sem.count) > 0)
				mask |= POLLOUT | POLLWRNORM;
		}
	}

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

	/* 
	 * Dispatch based on command.
	 * Exit with break if modifications occured. 
	 */
	switch (cmd) {
	case SOUND_MIXER_INFO:
	    {
		mixer_info info;
		strncpy(info.id, "UDA1341", sizeof(info.id));
		strncpy(info.name, "Philips UDA1341", sizeof(info.name));
		info.modify_counter = audio_mix_modcnt;
		return copy_to_user((void *)arg, &info, sizeof(info));
	    }

	case SOUND_OLD_MIXER_INFO:
	    {
		_old_mixer_info info;
		strncpy(info.id, "UDA1341", sizeof(info.id));
		strncpy(info.name, "Philips UDA1341", sizeof(info.name));
		return copy_to_user((void *)arg, &info, sizeof(info));
	    }

	case SOUND_MIXER_READ_DEVMASK:
		val = (SOUND_MASK_VOLUME |
		       SOUND_MASK_TREBLE |
		       SOUND_MASK_BASS |
		       SOUND_MASK_LINE1 |
		       SOUND_MASK_LINE2 | 
		       SOUND_MASK_MIC);
		return put_user(val, (long *) arg);

	case SOUND_MIXER_READ_RECMASK:
		val = (SOUND_MASK_MIC |
		       SOUND_MASK_LINE1 |
		       SOUND_MASK_LINE2 | 
		       SOUND_MASK_LINE3);
		return put_user(val, (long *) arg);

	case SOUND_MIXER_READ_STEREODEVS:
		return put_user(0, (long *) arg);

	case SOUND_MIXER_READ_CAPS:
		val = SOUND_CAP_EXCL_INPUT;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_AGC:
		/* 
		 * (as found in sb_mixer.c)
		 * Use ioctl(fd, SOUND_MIXER_AGC, &mode) to turn AGC off (0) or on (1).
		 */
		DATA0_ext4.AGC_ctrl = val ? 1 : 0;
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_ext4, 2);
		ret = put_user(DATA0_ext4.AGC_ctrl, (long *) arg);
		if (ret)
			return ret;
		break;

	case SOUND_MIXER_WRITE_RECSRC:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		/* Recording source is selected by mixer_mode */
		switch (val) {
		case SOUND_MASK_LINE1:
			/* input channel 1 select */
			DATA0_ext2.mixer_mode = 1;
			break;
		case SOUND_MASK_LINE2:
			/* Double differential mode */
			DATA0_ext2.mixer_mode = 0;
			break;
		case SOUND_MASK_LINE3:
			/*
			 * digital mixer mode
			 * (input 1 x MA + input2 x MB)
			 */
			DATA0_ext2.mixer_mode = 3;
			break;
		case SOUND_MASK_MIC:
		default:
			/* Input channel 2 select */
			DATA0_ext2.mixer_mode = 2;
			break;
		}
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_ext2, 2);
		break;

	case SOUND_MIXER_READ_RECSRC:
		/* Recording source is specified by mixer_mode */
		switch (DATA0_ext2.mixer_mode) {
		case 0:
			/* Double differential mode */
			val = SOUND_MASK_LINE2;
			break;
		case 1:
			/* input channel 1 select */
			val = SOUND_MASK_LINE1;
			break;
		case 3:
			/*
			 * digital mixer mode
			 * (input 1 x MA + input2 x MB)
			 */
			val = SOUND_MASK_LINE3;
			break;
		case 2:
		default:
			/* Input channel 2 select */
			val = SOUND_MASK_MIC;
			break;
		}
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_VOLUME:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		DATA0_0.volume = 63 - (((val & 0xff) + 1) * 63) / 100;
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_0, 1);
		break;

	case SOUND_MIXER_READ_VOLUME:
		val = ((63 - DATA0_0.volume) * 100) / 63;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_TREBLE:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		DATA0_1.treble = (((val & 0xff) + 1) * 3) / 100;
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_1, 1);
		break;

	case SOUND_MIXER_READ_TREBLE:
		val = (DATA0_1.treble * 100) / 3;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_BASS:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		DATA0_1.bass = (((val & 0xff) + 1) * 15) / 100;
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_1, 1);
		break;

	case SOUND_MIXER_READ_BASS:
		val = (DATA0_1.bass * 100) / 15;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_LINE1:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		DATA0_ext0.ch1_gain = (((val & 0xff) + 1) * 31) / 100;
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_ext0, 2);
		break;

	case SOUND_MIXER_READ_LINE1:
		val = (DATA0_ext0.ch1_gain * 100) / 31;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_LINE2:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		DATA0_ext1.ch2_gain = (((val & 0xff) + 1) * 31) / 100;
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_ext1, 2);
		break;

	case SOUND_MIXER_READ_LINE2:
		val = (DATA0_ext1.ch2_gain * 100) / 31;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_MIC:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		/* Use different registers depending on AGC setting */
		if (DATA0_ext4.AGC_ctrl == 1) {
			/* AGC On, play with MIC sensitivity */
			/* even if 3 bits, value 7 is not used */
			DATA0_ext2.mic_level =
			    (((val & 0xff) + 1) * 6) / 100;
			L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
				 (char *) &DATA0_ext2, 2);
		} else {
			/* AGC Off, plain with Input channel 2 amplifer gain */
			val = ((val & 0xff) * 127) / 100;
			DATA0_ext4.ch2_igain_l = val & 3;
			DATA0_ext5.ch2_igain_h = val >> 2;
			L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
				 (char *) &DATA0_ext4, 2);
			L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
				 (char *) &DATA0_ext5, 2);
		}
		break;

	case SOUND_MIXER_READ_MIC:
		/* Use different registers depending on AGC setting */
		if (DATA0_ext4.AGC_ctrl == 1) {
			val = (DATA0_ext2.mic_level * 100) / 6;
			val |= val << 8;
		} else {
			val =
			    DATA0_ext4.ch2_igain_l +
			    (DATA0_ext5.ch2_igain_h << 2);
			val = (val * 100) / 127;
			val |= val << 8;
		}
		return put_user(val, (long *) arg);

#if 0	/* Experimental.  What those should produce is still not obvious to me. */
	case SOUND_MIXER_WRITE_OGAIN:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		DATA0_ext6.AGC_level = (((val & 0xff) + 1) * 3) / 100;
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_ext6, 2);
		break;

	case SOUND_MIXER_READ_OGAIN:
		val = (DATA0_ext6.AGC_level * 100) / 3;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_IMIX:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		DATA0_ext2.mixer_mode = val;
		L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0,
			 (char *) &DATA0_ext2, 2);
		break;

	case SOUND_MIXER_READ_IMIX:
		val = DATA0_ext2.mixer_mode;
		return put_user(val, (long *) arg);
#endif

	case SOUND_MIXER_WRITE_RECLEV:
	case SOUND_MIXER_READ_RECLEV:
	default:
		return -ENOSYS;
	}

	audio_mix_modcnt++;
	return 0;
}

static void audio_set_dsp_speed(long val) {
	if (USE_SA1111) {
#ifdef CONFIG_SA1111
		/* Note that the 32kHz and 44.1kHz sample
		* rates are listed in the July 2000 release
		* of the SA-1111 Developer's Manual.
		* (Thanks to Chester for pointing this
		* out! -jd)
		*/
		if (val > 0 && val < 9512) {
			val = 8000;  /* 8kHz                 */
			SKAUD = 69;
		} else if (val >= 9512 && val < 13512) {
			val = 11025; /* 11.025kHz (11.01kHz) */
			SKAUD = 50;
		} else if (val >= 13512 && val < 19025) {
			val = 16000; /* 16kHz (16.05kHz)     */
			SKAUD = 34;
		} else if (val >= 19025 && val < 27025){
			val = 22050; /* 22.05kHz (22.46kHz)  */
			SKAUD = 24;
		} else if (val >= 27025 && val < 38050){
			val = 32000; /* 32kHz (31.2kHz)      */
			SKAUD = 18;
		} else {
			val = 44100; /* 44.1kHz (43.2kHz)    */
			SKAUD = 12;
		}

		STATUS_0.system_clk = UDA_STATUS0_SC_256FS;
		L3_write((UDA1341_L3Addr << 2) | 
			UDA1341_STATUS,
			(char *) &STATUS_0, 1);
#endif
	} else if (machine_is_assabet()) {
		/* FIXME: we need to modify the clock rate for DRAM bank 2 
		* (our clock source) accordingly.
		*/
		val = 44100;

	} else if (machine_is_bitsy()) {
#ifdef CONFIG_SA1100_BITSY
        	/* set the external clock generator */
		switch (val) {
		case 32000:
		case 48000:
			/* 00:  */
			GPCR =
				GPIO_BITSY_CLK_SET0 |
				GPIO_BITSY_CLK_SET1;
			break;
		case 44100:
			/* 01: 11.2896 MHz */
			GPSR = GPIO_BITSY_CLK_SET0;
			GPCR = GPIO_BITSY_CLK_SET1;
			break;
		case 8000:
		case 16000:
			/* 10: 4.096 MHz */
			GPCR = GPIO_BITSY_CLK_SET0;
			GPSR = GPIO_BITSY_CLK_SET1;
			break;
		case 11025:
		case 22050:
			/* 11: 5.6245 MHz */
			GPSR =
				GPIO_BITSY_CLK_SET0 |
				GPIO_BITSY_CLK_SET1;
			break;
		default:
			return -EINVAL;
		}
		switch (val) {
		case 8000:
			STATUS_0.system_clk = UDA_STATUS0_SC_512FS;
			break;
		case 11025:
			STATUS_0.system_clk = UDA_STATUS0_SC_512FS;
			break;
		case 16000:
			STATUS_0.system_clk = UDA_STATUS0_SC_256FS;
			break;
		case 22050:
			STATUS_0.system_clk = UDA_STATUS0_SC_256FS;
			break;
		case 32000:
			STATUS_0.system_clk = UDA_STATUS0_SC_384FS;
			break;
		case 44100:
			STATUS_0.system_clk = UDA_STATUS0_SC_256FS;
			break;
		case 48000:
			STATUS_0.system_clk = UDA_STATUS0_SC_256FS;
			break;
		}
		L3_write((UDA1341_L3Addr << 2) | UDA1341_STATUS,
			(char *) &STATUS_0, 1);
#endif
	} else if (machine_is_pangolin()) {
		val = 44100;
        } else if (machine_is_freebird()) {
		val = 44100;
        }

        /* save audio rate */
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
		if (val != 1 && val != 2)
			return -EINVAL;
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
		if (output_stream.buffers)
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
		if (audio_setup_buf(&output_stream))
			return -ENOMEM;
		break;

	case SNDCTL_DSP_SYNC:
		return audio_sync(file);

	case SNDCTL_DSP_GETOSPACE:
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
			sa1100_dma_set_spin(output_stream.dma_ch, 0, 0);
			audio_clear_buf(&input_stream);
		}
		switch (file->f_flags & O_ACCMODE) {
		case O_WRONLY:
		case O_RDWR:
			audio_clear_buf(&output_stream);
		}
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


static void audio_power_on(void);

static int audio_open(struct inode *inode, struct file *file)
{
	int cold = !audio_active;

	DPRINTK("audio_open\n");

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		if (audio_rd_refcount)
			return -EBUSY;
		audio_rd_refcount++;
	} else if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		if (audio_wr_refcount)
			return -EBUSY;
		audio_wr_refcount++;
	} else if ((file->f_flags & O_ACCMODE) == O_RDWR) {
		if (audio_rd_refcount || audio_wr_refcount)
			return -EBUSY;
		audio_rd_refcount++;
		audio_wr_refcount++;
	} else
		return -EINVAL;

	if (cold) {
		audio_rate = AUDIO_RATE_DEFAULT;
		audio_channels = AUDIO_CHANNELS_DEFAULT;
		audio_fragsize = AUDIO_FRAGSIZE_DEFAULT;
		audio_nbfrags = AUDIO_NBFRAGS_DEFAULT;
		audio_clear_buf(&output_stream);
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
	DPRINTK("audio_release\n");

	switch (file->f_flags & O_ACCMODE) {
	case O_RDONLY:
	case O_RDWR:
		if (audio_rd_refcount == 1) {
			sa1100_dma_set_spin(output_stream.dma_ch, 0, 0);
			audio_clear_buf(&input_stream);
			audio_rd_refcount = 0;
		}
	}

	switch (file->f_flags & O_ACCMODE) {
	case O_WRONLY:
	case O_RDWR:
		if (audio_wr_refcount == 1) {
			audio_sync(file);
			audio_clear_buf(&output_stream);
			audio_wr_refcount = 0;
		}
	}

	if (!audio_active)
		audio_power_off();

	MOD_DEC_USE_COUNT;
	return 0;
}


static int mixer_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}


static struct file_operations UDA1341_dsp_fops = {
	llseek:		audio_llseek,
	write:		audio_write,
	read:		audio_read,
	poll:		audio_poll,
	ioctl:		audio_ioctl,
	open:		audio_open,
	release:	audio_release
};

static struct file_operations UDA1341_mixer_fops = {
	ioctl:		mixer_ioctl,
	open:		mixer_open,
	release:	mixer_release
};

static inline void audio_sa1111_init(void)
{
#ifdef CONFIG_SA1111
#ifdef CONFIG_SA1100_ASSABET
        /* Select I2S audio (instead of AC-Link) */
  	AUD_CTL = AUD_SEL_1341;
#endif
#ifdef CONFIG_SA1100_JORNADA720
	/* LDD4 is speaker, LDD3 is microphone */
	PPSR &= ~(PPC_LDD3 | PPC_LDD4);
	PPDR |= PPC_LDD3 | PPC_LDD4;
	PPSR |= PPC_LDD4; /* enable speaker */
	PPSR |= PPC_LDD3; /* enable microphone */
#endif
	SKCR &= ~SKCR_SELAC;
	
	/* Enable the I2S clock and L3 bus clock: */
	SKPCR |= (SKPCR_I2SCLKEN | SKPCR_L3CLKEN);
	
	/* Activate and reset the Serial Audio Controller */
	SACR0 |= (SACR0_ENB | SACR0_RST);
	mdelay(5);
	SACR0 &= ~SACR0_RST;
	
	/* For I2S, BIT_CLK is supplied internally. The "SA-1111
	 * Specification Update" mentions that the BCKD bit should
	 * be interpreted as "0 = output". Default clock divider
	 * is 22.05kHz.
	 *
	 * Select I2S, L3 bus. "Recording" and "Replaying" 
	 * (receive and transmit) are enabled.
	 */
	SACR1 = SACR1_L3EN;
#endif
}

static inline void audio_sa1111_shutdown(void)
{
#ifdef CONFIG_SA1111
	SACR0 &= ~SACR0_ENB;
#ifdef CONFIG_SA1100_ASSABET
#if defined (CHROME)
	LEDS = 0;
#endif
#endif
#endif
}

static inline void audio_assabet_init(void)
{
#ifdef CONFIG_SA1100_ASSABET
  	/* Setup the uarts */
  	GAFR |= (GPIO_SSP_TXD | GPIO_SSP_RXD | GPIO_SSP_SCLK |
		 GPIO_SSP_SFRM | GPIO_SSP_CLK);
	GPDR |= (GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM);
	GPDR &= ~(GPIO_SSP_RXD | GPIO_SSP_CLK);
	PPAR |= PPAR_SPR;
	Ser4SSCR0 = SSCR0_SSE + SSCR0_DataSize(16) + SSCR0_TI;
	Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_ExtClk;
	
	/* Enable the audio power */
	BCR_clear(BCR_STEREO_LB | BCR_QMUTE | BCR_SPK_OFF);
	BCR_set(BCR_CODEC_RST | BCR_AUDIO_ON);
	
	/* 
	 * The assabet board uses the SDRAM clock as the source clock for 
	 * audio. This is supplied to the SA11x0 from the CPLD on pin 19. 
	 * At 206Mhz we need to run the audio clock (SDRAM bank 2) 
	 * at half speed. This clock will scale with core frequency so 
	 * the audio sample rate will also scale. The CPLD on Assabet 
	 * will need to be programmed to match the core frequency.
	 */
	MDREFR |= (MDREFR_K2DB2 | MDREFR_K2RUN);
	/* Make sure EAPD and KAPD are clear to run the clocks at all times. */
	MDREFR &= ~(MDREFR_EAPD | MDREFR_KAPD);
	
	/* Wait for the UDA1341 to wake up */
	mdelay(100);
#endif
}

static inline void audio_assabet_shutdown(void)
{
#ifdef CONFIG_SA1100_ASSABET
	/* disable the audio power */
	BCR_set(BCR_STEREO_LB | BCR_QMUTE | BCR_SPK_OFF);
	BCR_clear(BCR_AUDIO_ON);
	/* 
	 * We can't clear BCR_CODEC_RST without knowing if
	 * the UCB1300 driver is still active...  A global 
	 * count would be required for this.
	 */
#endif
}

static inline void audio_bitsy_init(void)
{
#ifdef CONFIG_SA1100_BITSY
	/* Setup the uarts */
	GAFR &= ~(GPIO_SSP_TXD | GPIO_SSP_RXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM);
	GAFR |= (GPIO_SSP_CLK);
	GPDR &= ~(GPIO_SSP_CLK);
	Ser4SSCR0 = 0;
	Ser4SSCR0 = SSCR0_DataSize(16) + SSCR0_TI;
	Ser4SSCR0 |= SSCR0_SerClkDiv(8);
	Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_ExtClk;
	Ser4SSCR0 |= SSCR0_SSE;

	/* Enable the audio power */
	set_bitsy_egpio(EGPIO_BITSY_CODEC_NRESET | EGPIO_BITSY_AUD_AMP_ON |
			EGPIO_BITSY_AUD_PWR_ON);
	clr_bitsy_egpio(EGPIO_BITSY_QMUTE);
	
	/* external clock configured for 44100 samples/sec */
	GPDR |= (GPIO_BITSY_CLK_SET0 | GPIO_BITSY_CLK_SET1);
	GPSR = GPIO_BITSY_CLK_SET0;
	GPCR = GPIO_BITSY_CLK_SET1;

	/* Wait for the UDA1341 to wake up */
	mdelay(100);
#endif
}

static inline void audio_bitsy_shutdown(void)
{
#ifdef CONFIG_SA1100_BITSY
	/* disable the audio power and all signals leading to audio chip */
	clr_bitsy_egpio(EGPIO_BITSY_CODEC_NRESET | EGPIO_BITSY_AUD_AMP_ON |
			EGPIO_BITSY_AUD_PWR_ON | EGPIO_BITSY_QMUTE);
#endif
}

static inline void audio_freebird_init(void)
{
#ifdef CONFIG_SA1100_FREEBIRD
   /* Setup the uarts */

   GAFR |= (GPIO_SSP_TXD | GPIO_SSP_RXD | GPIO_SSP_SCLK |
   GPIO_SSP_SFRM | GPIO_SSP_CLK);
   GPDR |= (GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM);
   GPDR &= ~(GPIO_SSP_RXD | GPIO_SSP_CLK);
   PPAR |= PPAR_SPR;
   Ser4SSCR0 = SSCR0_SSE + SSCR0_DataSize(16) + SSCR0_TI;
   Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_ExtClk;  
   //Check
   BCR_clear(BCR_FREEBIRD_QMUTE | BCR_FREEBIRD_SPK_OFF);
   BCR_set(BCR_FREEBIRD_CODEC_RST | BCR_FREEBIRD_AUDIO_PWR);

   /* Wait for the UDA1341 to wake up */
   mdelay(100);
#endif
}

static inline void audio_freebird_shutdown(void)
{
#ifdef CONFIG_SA1100_FREEBIRD

   /* disable the audio power */
   //Check
   BCR_set(BCR_FREEBIRD_QMUTE | BCR_FREEBIRD_SPK_OFF);
   BCR_clear(BCR_FREEBIRD_CODEC_RST | BCR_FREEBIRD_AUDIO_PWR);
#endif
}

static inline void audio_pangolin_init(void)
{
#ifdef CONFIG_SA1100_PANGOLIN
	/* Setup the uarts */
	GAFR |= (GPIO_SSP_TXD | GPIO_SSP_RXD | GPIO_SSP_SCLK |
		 GPIO_SSP_SFRM | GPIO_SSP_CLK);
	GPDR |= (GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM);
	GPDR &= ~(GPIO_SSP_RXD | GPIO_SSP_CLK);
	PPAR |= PPAR_SPR;
	Ser4SSCR0 = SSCR0_SSE + SSCR0_DataSize(16) + SSCR0_TI;
	Ser4SSCR1 = SSCR1_SClkIactL + SSCR1_SClk1P + SSCR1_IntClk;

	/* Wait for the UDA1341 to wake up */
	mdelay(100);
#endif
}


static void audio_uda1341_reset(void)
{
	DPRINTK("audio_uda1341_reset\n");
	
	/* Reset the chip */
	STATUS_0.reset = 1;
	L3_write((UDA1341_L3Addr << 2) | UDA1341_STATUS, (char *) &STATUS_0, 1);

	if (machine_is_bitsy()) {
#ifdef CONFIG_BITSY
		clr_bitsy_egpio(EGPIO_BITSY_CODEC_NRESET);
		set_bitsy_egpio(EGPIO_BITSY_CODEC_NRESET);
#endif
	}

	if (USE_SA1111) {
	  	STATUS_0.input_fmt = UDA_STATUS0_IF_I2S;
		STATUS_0.system_clk = UDA_STATUS0_SC_256FS;
	}

	STATUS_0.reset = 0;
	L3_write((UDA1341_L3Addr << 2) | UDA1341_STATUS, (char *) &STATUS_0, 1);
}


static void audio_power_on()
{
	DPRINTK("audio_power_on\n");
	
	L3_init();

	if (USE_SA1111)
		audio_sa1111_init();
	else if (machine_is_assabet())
		audio_assabet_init();

	if (machine_is_bitsy())
		audio_bitsy_init();
	if (machine_is_pangolin())
		audio_pangolin_init();
	if (machine_is_freebird())
		audio_freebird_init();

	audio_uda1341_reset();

	/* Restore chip state, mixer values, etc... */
	L3_write((UDA1341_L3Addr << 2) | UDA1341_STATUS, (char *) &STATUS_0, 1);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_STATUS, (char *) &STATUS_1, 1);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_0, 1);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_1, 1);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_2, 1);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_ext0, 2);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_ext1, 2);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_ext2, 2);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_ext4, 2);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_ext5, 2);
	L3_write((UDA1341_L3Addr << 2) | UDA1341_DATA0, (char *) &DATA0_ext6, 2);
	audio_set_dsp_speed(audio_rate);
}

static void audio_power_off(void)
{		audio_uda1341_reset();
	DPRINTK("audio_power_off\n");
	
	/* power down audio */
	if (USE_SA1111)
		audio_sa1111_shutdown();
	else if (machine_is_assabet())
		audio_assabet_shutdown();

	if (machine_is_bitsy())
		audio_bitsy_shutdown();
	if (machine_is_freebird())
		audio_freebird_shutdown();
}


#ifdef CONFIG_PM
static int audio_uda1341_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND: /* enter D1-D3 */
		if (USE_SA1111) {
#ifdef CONFIG_SA1111
#endif
		} else {
#ifndef CONFIG_SA1111
			sa1100_dma_sleep(input_stream.dma_ch);
			sa1100_dma_sleep(output_stream.dma_ch);
#endif
		}
		if (audio_active) 
			audio_power_off();
		break;
	case PM_RESUME:  /* enter D0 */
		if (audio_active)
			audio_power_on();
                if (USE_SA1111) {
#ifdef CONFIG_SA1111
#endif
                } else {
#ifndef CONFIG_SA1111
                        sa1100_dma_wakeup(input_stream.dma_ch);
                        sa1100_dma_wakeup(output_stream.dma_ch);
#endif
                }
                break;
	}
	return 0;
}
#endif


static int __init audio_init_dma(audio_stream_t * s, char *desc)
{
	int err;

#ifdef CONFIG_SA1111
	if(USE_SA1111)
	  	err = sa1111_sac_request_dma(&s->dma_ch, desc,
					     (s == &output_stream)?\
					     SA1111_SAC_XMT_CHANNEL:\
					     SA1111_SAC_RCV_CHANNEL);
	else
#endif
	  	err = sa1100_request_dma(&s->dma_ch, desc);

	if (err)
		return err;
	if (s == &output_stream) {
		sa1100_dma_set_callback(s->dma_ch,
					audio_dmaout_done_callback);
		sa1100_dma_set_device(s->dma_ch, DMA_Ser4SSPWr);
	} else {
		sa1100_dma_set_callback(s->dma_ch,
					audio_dmain_done_callback);
		sa1100_dma_set_device(s->dma_ch, DMA_Ser4SSPRd);
	}
	return 0;
}

static int audio_clear_dma(audio_stream_t * s)
{
	sa1100_free_dma(s->dma_ch);
	return 0;
}


int __init audio_uda1341_init(void)
{
	if (!(machine_is_assabet() || machine_is_bitsy() ||
	      machine_is_pangolin() || machine_is_freebird() ||
	      machine_is_jornada720())) {
		printk( KERN_WARNING AUDIO_NAME_VERBOSE
			": not supported on this machine\n" );
		return -ENODEV;
	}

	/* Acquire and initialize DMA */
	if (audio_init_dma(&output_stream, "UDA1341 out") ||
	    audio_init_dma(&input_stream, "UDA1341 in")) {
		audio_clear_dma(&output_stream);
		audio_clear_dma(&input_stream);
		printk( KERN_WARNING AUDIO_NAME_VERBOSE
			": unable to get DMA channels\n" );
		return -EBUSY;
	}

	/* Set some default mixer values... */
	STATUS_1.DAC_gain = 1;
	STATUS_1.ADC_gain = 1;
	DATA0_0.volume = 15;
	DATA0_2.mode = 3;
	DATA0_ext2.mixer_mode = 2;
	DATA0_ext2.mic_level = 4;
	DATA0_ext4.AGC_ctrl = 1;
	DATA0_ext6.AGC_level = 3;

	/* register devices */
	audio_dev_dsp = register_sound_dsp(&UDA1341_dsp_fops, -1);
	audio_dev_mixer = register_sound_mixer(&UDA1341_mixer_fops, -1);

#ifdef CONFIG_PM
        audio_pm_dev = pm_register(PM_SYS_DEV, 0, audio_uda1341_pm_callback);
#endif

	printk(AUDIO_NAME_VERBOSE "%s initialized\n",
	       (USE_SA1111) ? " (SA-1111 L3 Control Bus)" : "" );

	return 0;
}

module_init(audio_uda1341_init);


void __exit audio_uda1341_exit(void)
{
#ifdef CONFIG_PM
	if (audio_pm_dev)
		pm_unregister(audio_pm_dev);
#endif
	/* unregister driver and IRQ */
	unregister_sound_dsp(audio_dev_dsp);
	unregister_sound_mixer(audio_dev_mixer);
	audio_clear_dma(&output_stream);
	audio_clear_dma(&input_stream);
	printk(AUDIO_NAME_VERBOSE " unloaded\n");
}

module_exit(audio_uda1341_exit);
