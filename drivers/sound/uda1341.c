/*
 * Philips UDA1341 mixer device driver
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
 * 2000-08-19	Erik Bunce	More inline w/ OSS API and UDA1341 docs
 * 				including fixed AGC and audio source handling
 *
 * 2000-11-30	Nicolas Pitre	- More mixer functionalities.
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>

#include <asm/uaccess.h>

#include "uda1341.h"


#undef DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


#define SET(adr, reg, len)  \
	if (active) state->L3_write((UDA1341_L3Addr << 2)|(adr), (char*)&regs->reg, (len))

int uda1341_mixer_ioctl (UDA1341_state_t *state, uint cmd, ulong arg)
{
	UDA1341_regs_t *regs = state->regs;
	int active = state->active;
	int ret;
	long val = 0;

	/* 
	 * Dispatch based on command.
	 * Exit with break if modifications occurred. 
	 */
	switch (cmd) {
	case SOUND_MIXER_INFO:
	    {
		mixer_info info;
		strncpy(info.id, "UDA1341", sizeof(info.id));
		strncpy(info.name, "Philips UDA1341", sizeof(info.name));
		info.modify_counter = state->mix_modcnt;
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
		regs->data0_ext4.AGC_ctrl = val ? 1 : 0;
		SET (UDA1341_DATA0, data0_ext4, 2);
		ret = put_user(regs->data0_ext4.AGC_ctrl, (long *) arg);
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
			regs->data0_ext2.mixer_mode = 1;
			break;
		case SOUND_MASK_LINE2:
			/* Double differential mode */
			regs->data0_ext2.mixer_mode = 0;
			break;
		case SOUND_MASK_LINE3:
			/*
			 * digital mixer mode
			 * (input 1 x MA + input2 x MB)
			 */
			regs->data0_ext2.mixer_mode = 3;
			break;
		case SOUND_MASK_MIC:
		default:
			/* Input channel 2 select */
			regs->data0_ext2.mixer_mode = 2;
			break;
		}
		SET (UDA1341_DATA0, data0_ext2, 2);
		break;

	case SOUND_MIXER_READ_RECSRC:
		/* Recording source is specified by mixer_mode */
		switch (regs->data0_ext2.mixer_mode) {
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
		regs->data0_0.volume = 63 - (((val & 0xff) + 1) * 63) / 100;
		SET (UDA1341_DATA0, data0_0, 1);
		break;

	case SOUND_MIXER_READ_VOLUME:
		val = ((63 - regs->data0_0.volume) * 100) / 63;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_TREBLE:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		regs->data0_1.treble = (((val & 0xff) + 1) * 3) / 100;
		SET (UDA1341_DATA0, data0_1, 1);
		break;

	case SOUND_MIXER_READ_TREBLE:
		val = (regs->data0_1.treble * 100) / 3;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_BASS:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		regs->data0_1.bass = (((val & 0xff) + 1) * 15) / 100;
		SET (UDA1341_DATA0, data0_1, 1);
		break;

	case SOUND_MIXER_READ_BASS:
		val = (regs->data0_1.bass * 100) / 15;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_LINE1:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		regs->data0_ext0.ch1_gain = (((val & 0xff) + 1) * 31) / 100;
		SET (UDA1341_DATA0, data0_ext0, 2);
		break;

	case SOUND_MIXER_READ_LINE1:
		val = (regs->data0_ext0.ch1_gain * 100) / 31;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_LINE2:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		regs->data0_ext1.ch2_gain = (((val & 0xff) + 1) * 31) / 100;
		SET (UDA1341_DATA0, data0_ext1, 2);
		break;

	case SOUND_MIXER_READ_LINE2:
		val = (regs->data0_ext1.ch2_gain * 100) / 31;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_MIC:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		/* Use different registers depending on AGC setting */
		if (regs->data0_ext4.AGC_ctrl == 1) {
			/* AGC On, play with MIC sensitivity */
			/* even if 3 bits, value 7 is not used */
			regs->data0_ext2.mic_level =
				(((val & 0xff) + 1) * 6) / 100;
			SET (UDA1341_DATA0, data0_ext2, 2);
		} else {
			/* AGC Off, plain with Input channel 2 amplifier gain */
			val = ((val & 0xff) * 127) / 100;
			regs->data0_ext4.ch2_igain_l = val & 3;
			regs->data0_ext5.ch2_igain_h = val >> 2;
			SET (UDA1341_DATA0, data0_ext4, 2);
			SET (UDA1341_DATA0, data0_ext5, 2);
		}
		break;

	case SOUND_MIXER_READ_MIC:
		/* Use different registers depending on AGC setting */
		if (regs->data0_ext4.AGC_ctrl == 1) {
			val = (regs->data0_ext2.mic_level * 100) / 6;
			val |= val << 8;
		} else {
			val = regs->data0_ext4.ch2_igain_l +
			     (regs->data0_ext5.ch2_igain_h << 2);
			val = (val * 100) / 127;
			val |= val << 8;
		}
		return put_user(val, (long *) arg);

#if 0	/* Experimental.  What those should produce is still not obvious to me. */
	case SOUND_MIXER_WRITE_OGAIN:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		regs->data0_ext6.AGC_level = (((val & 0xff) + 1) * 3) / 100;
		SET (UDA1341_DATA0, data0_ext6, 2);
		break;

	case SOUND_MIXER_READ_OGAIN:
		val = (regs->data0_ext6.AGC_level * 100) / 3;
		val |= val << 8;
		return put_user(val, (long *) arg);

	case SOUND_MIXER_WRITE_IMIX:
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;
		regs->data0_ext2.mixer_mode = val;
		SET (UDA1341_DATA0, data0_ext2, 2);
		break;

	case SOUND_MIXER_READ_IMIX:
		val = regs->data0_ext2.mixer_mode;
		return put_user(val, (long *) arg);
#endif

	case SOUND_MIXER_WRITE_RECLEV:
	case SOUND_MIXER_READ_RECLEV:
	default:
		return -ENOSYS;
	}

	state->mix_modcnt++;
	return 0;
}

void uda1341_reset (UDA1341_state_t *state)
{
	UDA1341_regs_t *regs = state->regs;
	int active = 1;

	DPRINTK("uda1341_reset\n");
	
	/* Reset the chip */
	regs->status_0.reset = 1;
	SET (UDA1341_STATUS, status_0, 1);
	regs->status_0.reset = 0;

	/* Restore chip state, mixer values, etc... */
	SET (UDA1341_STATUS, status_0, 1);
	SET (UDA1341_STATUS, status_1, 1);
	SET (UDA1341_DATA0, data0_0, 1);
	SET (UDA1341_DATA0, data0_1, 1);
	SET (UDA1341_DATA0, data0_2, 1);
	SET (UDA1341_DATA0, data0_ext0, 2);
	SET (UDA1341_DATA0, data0_ext1, 2);
	SET (UDA1341_DATA0, data0_ext2, 2);
	SET (UDA1341_DATA0, data0_ext4, 2);
	SET (UDA1341_DATA0, data0_ext5, 2);
	SET (UDA1341_DATA0, data0_ext6, 2);
}

EXPORT_SYMBOL(uda1341_mixer_ioctl);
EXPORT_SYMBOL(uda1341_reset);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("Philips UDA1341 mixer device driver");
