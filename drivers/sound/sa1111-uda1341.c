/*
 * Glue audio driver for the SA1111 compagnon chip & Philips UDA1341 codec.
 *
 * Copyright (c) 2000 John Dorsey
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2000-09-04	John Dorsey	SA-1111 Serial Audio Controller support
 * 				was initially added to the sa1100-uda1341.c
 * 				driver.
 * 
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/pm.h>

#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/dma.h>

#include "uda1341.h"
#include "sa1100-audio.h"


#undef DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


/* 
 * Definitions 
 */

#define AUDIO_NAME		"SA1111/UDA1341"

#define AUDIO_FMT_MASK		(AFMT_S16_LE)
#define AUDIO_FMT_DEFAULT	(AFMT_S16_LE)
#define AUDIO_CHANNELS_DEFAULT	2
#define AUDIO_RATE_DEFAULT	22050

#define AUDIO_CLK_BASE		561600


/*
 * Write data to a device on the L3 bus. The address is passed as well as
 * the data and length. The length written is returned. The register space
 * is encoded in the address (low two bits are set and device address is
 * in the upper 6 bits).
 */
static int L3_write(char addr, char *data, int len)
{
	int bytes = len;

	DPRINTK("%s(0x%x, %d)\n", __FUNCTION__, addr, len);

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

	DPRINTK("%s(0x%x, %d)\n", __FUNCTION__, addr, len);

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

	return bytes;
}


/*
 * Mixer (UDA1341) interface
 */

static UDA1341_regs_t UDA1341_regs = UDA1341_REGS_DFLT;

static UDA1341_state_t uda1341_state = {
	regs:		&UDA1341_regs,
	L3_write:	L3_write,
	L3_read:	L3_read,
};

static int mixer_ioctl (struct inode *inode, struct file *file,
			uint cmd, ulong arg)
{
	return uda1341_mixer_ioctl(&uda1341_state, cmd, arg);
}

static struct file_operations uda1341_mixer_fops = {
	ioctl:		mixer_ioctl,
	owner:		THIS_MODULE
};


/*
 * Audio interface
 */

static int audio_clk_div = AUDIO_CLK_BASE/AUDIO_RATE_DEFAULT - 1;

static void sa1111_audio_init(void)
{
#ifdef CONFIG_ASSABET_NEPONSET
	if (machine_is_assabet()) {
		/* Select I2S audio (instead of AC-Link) */
		AUD_CTL = AUD_SEL_1341;
	}
#endif
#ifdef CONFIG_SA1100_JORNADA720
	if (machine_is_jornada720()) {
		/* LDD4 is speaker, LDD3 is microphone */
		PPSR &= ~(PPC_LDD3 | PPC_LDD4);
		PPDR |= PPC_LDD3 | PPC_LDD4;
		PPSR |= PPC_LDD4; /* enable speaker */
		PPSR |= PPC_LDD3; /* enable microphone */
	}
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
	SKAUD = audio_clk_div;

	/* Initialize the UDA1341 internal state */
	uda1341_state.active = 1;
	uda1341_reset(&uda1341_state);
}

static void sa1111_audio_shutdown(void)
{
	uda1341_state.active = 0;
	SACR0 &= ~SACR0_ENB;
}

static int sa1111_audio_ioctl( struct inode *inode, struct file *file,
				uint cmd, ulong arg)
{
	long val;
	int ret = 0;
	
	switch (cmd) {
	case SNDCTL_DSP_SETFMT:
		ret = get_user(val, (long *) arg);
		if (ret) break;
		if (val & AUDIO_FMT_MASK) {
			break;
		} else
			return -EINVAL;

	case SNDCTL_DSP_CHANNELS:
	case SNDCTL_DSP_STEREO:
		ret = get_user(val, (long *) arg);
		if (ret) break;
		if (cmd == SNDCTL_DSP_STEREO)
			val = val ? 2 : 1;
		/* the UDA1341 is stereo only */
		if (val != 2)
			return -EINVAL;
		break;
													
	case SOUND_PCM_READ_CHANNELS:
		return put_user(2, (long *) arg);

	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (long *) arg);
		if (ret) break;
		if (val < 8000) val = 8000;
		if (val > 48000) val = 48000;
		SKAUD = audio_clk_div = AUDIO_CLK_BASE/val - 1;
		/* fall through */

	case SOUND_PCM_READ_RATE:
		return put_user(AUDIO_CLK_BASE/(audio_clk_div+1), (long *) arg);

	case SNDCTL_DSP_GETFMTS:
		return put_user(AUDIO_FMT_MASK, (long *) arg);

	default:
		/* Maybe this is meant for the mixer (as per OSS Docs) */
		return uda1341_mixer_ioctl(&uda1341_state, cmd, arg);
	}

	return ret;
}

static audio_stream_t output_stream, input_stream;

static audio_state_t audio_state = {
	output_stream:	&output_stream,
	input_stream:	&input_stream,
	hw_init:	sa1111_audio_init,
	hw_shutdown:	sa1111_audio_shutdown,
	client_ioctl:	sa1111_audio_ioctl,
};

static int sa1111_audio_open(struct inode *inode, struct file *file)
{
	        return sa1100_audio_instance(inode, file, &audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to sa1100_audio_instance()
 */
static struct file_operations sa1111_audio_fops = {
	open:		sa1111_audio_open,
	owner:		THIS_MODULE
};


static int audio_dev_id, mixer_dev_id;

static int __init sa1111_uda1341_init(void)
{
	if ( !(	(machine_is_assabet() && machine_has_neponset()) ||
		machine_is_jornada720() ))
		return -ENODEV;
	
	/* Acquire and initialize DMA */
	if (sa1111_sac_request_dma(&output_stream.dma_ch, "UDA1341 out", SA1111_SAC_XMT_CHANNEL) < 0 ||
	    sa1111_sac_request_dma(&input_stream.dma_ch, "UDA1341 in", SA1111_SAC_RCV_CHANNEL) < 0) {
		sa1100_free_dma(output_stream.dma_ch);
		printk( KERN_ERR AUDIO_NAME ": unable to get DMA channels\n" );
		return -EBUSY;
	}

	/* Settings which differ from the default initialisation */
	UDA1341_regs.status_0.input_fmt = UDA_STATUS0_IF_I2S;
	UDA1341_regs.status_0.system_clk = UDA_STATUS0_SC_256FS;

	/* register devices */
	audio_dev_id = register_sound_dsp(&sa1111_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&uda1341_mixer_fops, -1);

	printk( KERN_INFO AUDIO_NAME " initialized\n" );
	return 0;
}

static void __exit sa1111_uda1341_exit(void)
{
	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
	sa1100_free_dma(output_stream.dma_ch);
	sa1100_free_dma(input_stream.dma_ch);
}

module_init(sa1111_uda1341_init);
module_exit(sa1111_uda1341_exit);

MODULE_AUTHOR("John Dorsey, Nicolas Pitre");
MODULE_DESCRIPTION("Glue audio driver for the SA1111 compagnon chip & Philips UDA1341 codec.");

EXPORT_NO_SYMBOLS;
