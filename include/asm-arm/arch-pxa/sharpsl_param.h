/*
 * linux/include/asm-arm/arch-pxa/sharpsl_param.h
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

#define FLASH_MEM_BASE	0xa0000a00
#define	FLASH_DATA(adr) (*(volatile unsigned int*)(FLASH_MEM_BASE+(adr)))
#define	FLASH_DATA_F(adr) (*(volatile float32 *)(FLASH_MEM_BASE+(adr)))
#define FLASH_MAGIC_CHG(a,b,c,d) ( ( d << 24 ) | ( c << 16 )  | ( b << 8 ) | a )

// COMADJ
#define FLASH_COMADJ_MAJIC	FLASH_MAGIC_CHG('C','M','A','D')
#define	FLASH_COMADJ_MAGIC_ADR	0x00
#define	FLASH_COMADJ_DATA_ADR	0x04

// UUID
#define FLASH_UUID_MAJIC	FLASH_MAGIC_CHG('U','U','I','D')
#define	FLASH_UUID_MAGIC_ADR	0x08
#define	FLASH_UUID_DATA_ADR	0x0C

// TOUCH PANEL
#define FLASH_TOUCH_MAJIC	FLASH_MAGIC_CHG('T','U','C','H')
#define	FLASH_TOUCH_MAGIC_ADR	0x1C
#define	FLASH_TOUCH_XP_DATA_ADR	0x20
#define	FLASH_TOUCH_YP_DATA_ADR	0x24
#define	FLASH_TOUCH_XD_DATA_ADR	0x28
#define	FLASH_TOUCH_YD_DATA_ADR	0x2C

// AD
#define FLASH_AD_MAJIC	FLASH_MAGIC_CHG('B','V','A','D')
#define	FLASH_AD_MAGIC_ADR	0x30
#define	FLASH_AD_DATA_ADR	0x34

// PHAD
#define FLASH_PHAD_MAJIC	FLASH_MAGIC_CHG('P','H','A','D')
#define	FLASH_PHAD_MAGIC_ADR	0x38
#define	FLASH_PHAD_DATA_ADR	0x3C


typedef struct sharpsl_flash_param_info {
  unsigned int comadj_keyword;
  unsigned int comadj;

  unsigned int uuid_keyword;
  unsigned char uuid[16];

  unsigned int touch_keyword;
  unsigned int touch1;
  unsigned int touch2;
  unsigned int touch3;
  unsigned int touch4;

  unsigned int adadj_keyword;
  unsigned int adadj;

  unsigned int phad_keyword;
  unsigned int phadadj;
} sharpsl_flash_param_info;


int sharpsl_get_comadj(void);
int sharpsl_get_phadadj(void);
