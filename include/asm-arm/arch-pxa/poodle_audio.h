/*
 * linux/drivers/sound/poodle_audio.h
 *
 * Sound Driver for WM8731 codec device header file
 *
 *      
 * Copyright (C) 2002  SHARP
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
 *
 */

#ifndef _SOUNDDRV_H_
#define _SOUNDDRV_H_

typedef struct GAIN_SETTINGS {
  int left;
  int right;
} GAIN_SETTINGS;

typedef struct SOUND_SETTINGS {
  unsigned short mode;
  GAIN_SETTINGS output;
  GAIN_SETTINGS input;
  int frequency;
} SOUND_SETTINGS;

// mode
#define SOUND_PLAY_MODE		(0x0001)
#define SOUND_REC_MODE		(0x0002)
#define SOUND_MODE_MASK		(0x0003)


// volume
enum {
	VOLUME_IGNORE = -2,
	VOLUME_MUTE = -1
};
#define MAX_VOLUME		MAX_HP_VOL
#define MIN_VOLUME		MIN_HP_VOL
#define MAX_INPUT_VOLUME	MAX_LINE_VOL
#define MIN_INPUT_VOLUME	MIN_LINE_VOL


#endif
