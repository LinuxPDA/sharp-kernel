/*
 * linux/include/asm-arm/arch-pxa/tosa_ac97_codec.h
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _TOSA_AC97_CODEC_H_
#define _TOSA_AC97_CODEC_H_

//#define USE_AC97_INTERRUPT

extern int pxa_ac97_get(struct ac97_codec **, unsigned char *);
extern void pxa_ac97_put(unsigned char *);
extern unsigned int ac97_set_dac_rate(struct ac97_codec *, unsigned int);
extern unsigned int ac97_set_adc_rate(struct ac97_codec *, unsigned int);

extern struct ac97_codec *codec;
#define ac97_read(r)		codec->codec_read(codec, r)
#define ac97_write(r, v)	codec->codec_write(codec, r, v);
#define ac97_bit_clear(r, v)	codec->codec_bit_clear(codec, r, v)
#define ac97_bit_set(r, v)	codec->codec_bit_set(codec, r, v)

/* Function1 */
#define AC97_JIEN	(1 << 12)
#define AC97_FRC	(1 << 11)
/* Function2 */
#define AC97_AMUTE	(1 << 15)	//DAC Auto-Mute Enable(read-only)
#define AC97_AMEN	(1 <<  7)	//DAC Auto-Mute Enable

#endif	/* _TOSA_AC97_CODEC_H_ */
