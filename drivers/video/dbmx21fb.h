/*
 * linux/drivers/video/dbmx2fb.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
********************************************************************************
 * File Name:	dbmx21fb.c
 *
 * Synopsis:
 *
 * Descirption: DBMX21 LCD controller Linux frame buffer driver 
 * 		This file is subject to the terms and conditions of the 
 * 		GNU General Public License.  See the file COPYING in the main 
 * 		directory of this archive for more details.
 * 
 * Modification History:
 * 13 Aug, 2003, initial version (incomplete in feature set)
 * 
*******************************************************************************/

#ifndef DBMX21FB_H
#define DBMX21FB_H


// used for N/A values
#define DONTCARE	1

#define DBMX21_NAME	"dbmx21"


// default value
#define SHARP_TFT_240x320

#if defined(SHARP_TFT_240x320)

#define PANELCFG_VAL_12	0xfb108bc7
#define HCFG_VAL_12	0x04000f06
#define VCFG_VAL_12	0x04000907
//#define	LDCR_VAL	0x800d0002
#define	LDCR_VAL	0x0004000f
#define PWMR_VAL	0x0000008a
#define LCD_MAXX	240
#define LCD_MAXY	320

#else

#define PANELCFG_VAL_12	0xf8088c6b
#define HCFG_VAL_12	0x04000f06
#define VCFG_VAL_12	0x04010c03
#define	LDCR_VAL	0x800d0002
#define PWMR_VAL	0x00000200
#define LCD_MAXX	320
#define LCD_MAXY	240

#endif

#define PANELCFG_VAL_4	0x20008c09
#define PANELCFG_VAL_4C	0x60008c09

#define HCFG_VAL_4	0x04000f07

#define VCFG_VAL_4	0x04010c03


#define REFMCR_VAL_4	0x00000003
#define REFMCR_VAL_12	0x00000003
#define DISABLELCD_VAL	0x00000000

#define DMACR_VAL_4	0x800c0003	// 12 & 3 TRIGGER
#define DMACR_VAL_12	0x00020008

#define INTCR_VAL_4	0x00000000
#define INTCR_VAL_12	0x00000000

#define INTSR_UDRERR	0x00000008
#define INTSR_ERRRESP	0x00000004
#define INTSR_EOF	0x00000002
#define INTSR_BOF	0x00000001

#define MIN_XRES        64
#define MIN_YRES        64

#define LCD_MAX_BPP	16

	
#define MAX_PALETTE_NUM_ENTRIES         256
#define MAX_PIXEL_MEM_SIZE \
        ((current_par.max_xres * current_par.max_yres * current_par.max_bpp)/8)
#define MAX_FRAMEBUFFER_MEM_SIZE \
	        (MAX_PIXEL_MEM_SIZE + 32)
#define ALLOCATED_FB_MEM_SIZE \
	        (PAGE_ALIGN(MAX_FRAMEBUFFER_MEM_SIZE + PAGE_SIZE * 2))

	// TODO:
#define FBCON_HAS_CFB4
#define FBCON_HAS_CFB8
#define FBCON_HAS_CFB16

#define MAX_PALETTE_NUM_ENTRIES		256
#define DEFAULT_CURSOR_BLINK_RATE	(20)
#define CURSOR_DRAW_DELAY		(2)
/*cursor status*/
#define LCD_CURSOR_OFF			0
#define LCD_CURSOR_ON			1

#ifdef FBCON_HAS_CFB4
	#define LCD_CURSOR_REVERSED	2
   	#define LCD_CURSOR_ON_WHITE	3
#elif defined(FBCON_HAS_CFB8) || defined(FBCON_HAS_CFB16)
   	#define LCD_CURSOR_INVERT_BGD	2
   	#define LCD_CURSOR_AND_BGD	3
	#define LCD_CURSOR_OR_BGD	4		
	#define LCD_CURSOR_XOR_BGD	5
#endif //FBCON_HAS_CFB4


#ifdef FBCON_HAS_CFB4
#define CURSOR_REVERSED_MASK		0x80000000
#define CURSOR_WHITE_MASK		0xC0000000
#else
#define CURSOR_INVERT_MASK		0x80000000
#define CURSOR_AND_BGD_MASK		0xC0000000
#define CURSOR_OR_BGD_MASK		0x50000000
#define CURSOR_XOR_BGD_MASK		0x90000000
#endif // FBCON_HAS_CFB4

#endif //LCDFB_H

