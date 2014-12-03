/*
 * linux/drivers/video/sharpsl_pxafb.h
 *
 *  Copyright (C) 2004 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/drivers/video/cotulla_fb.h
 *
 * Copyright (C) 2001-2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#if !defined (_SHARPSL_PXAFB_H)
#define _SHARPSL_PXAFB_H

#define MAX_XRES          480
#define MAX_YRES          640
#define BITS_PER_PIXEL    16

#define MAX_FRAMEBUFFER_MEM_SIZE \
	((MAX_XRES * MAX_YRES * BITS_PER_PIXEL) / 8)
#define ALLOCATED_FB_MEM_SIZE \
	(PAGE_ALIGN(MAX_FRAMEBUFFER_MEM_SIZE + PAGE_SIZE))

/* LCD Control Register 0 Bits */
#define LCD0_V_LDDALT	26
#define LCD0_V_OUC	25
#define LCD0_V_CMDIM	24
#define LCD0_V_RDSTM	23
#define LCD0_V_LCDT	22
#define LCD0_V_OUM	21
#define LCD0_V_BSM0	20
#define LCD0_V_PDD	12
#define LCD0_V_QDM	11
#define LCD0_V_DIS	10
#define LCD0_V_DPD	9
#define LCD0_V_PAS	7
#define LCD0_V_EOFM0	6
#define LCD0_V_IUM	5
#define LCD0_V_SOFM0	4
#define LCD0_V_LDM	3
#define LCD0_V_SDS	2
#define LCD0_V_CMS	1
#define LCD0_V_ENB	0

/* LCD control Register 1 Bits */
#define LCD1_M_PPL	0x000003FF
#define LCD1_M_HSW	0x0000FC00
#define LCD1_M_ELW	0x00FF0000
#define LCD1_M_BLW	0xFF000000
#define LCD1_V_PPL	0
#define LCD1_V_HSW	10
#define LCD1_V_ELW	16
#define LCD1_V_BLW	24

/* LCD control Register 2 Bits */
#define LCD2_M_LPP	0x000003FF
#define LCD2_M_VSW	0x0000FC00
#define LCD2_M_EFW	0x00FF0000
#define LCD2_M_BFW	0xFF000000
#define LCD2_V_LPP	0
#define LCD2_V_VSW	10
#define LCD2_V_EFW	16
#define LCD2_V_BFW	24

/* LCD control Register 3 Bits */
#define LCD3_M_PCD	0x000000FF
#define LCD3_M_ACB	0x0000FF00
#define LCD3_M_API	0x000F0000
#define LCD3_M_VSP	0x00100000
#define LCD3_M_HSP	0x00200000
#define LCD3_M_PCP	0x00400000
#define LCD3_M_OEP	0x00800000
#define LCD3_M_PDFOR	0xC0000000
#define LCD3_V_PCD	0
#define LCD3_V_ACB	8
#define LCD3_V_API	16
#define LCD3_V_VSP	20
#define LCD3_V_HSP	21
#define LCD3_V_PCP	22
#define LCD3_V_OEP	23
#define LCD3_V_BPP	24
#define LCD3_V_DPC	27
#define LCD3_V_BPP3	29
#define LCD3_V_PDFOR	30

/* LCD Status Register */
#define LCSR_V_SINT 10
#define LCSR_V_BS   9
#define LCSR_V_EOF  8
#define LCSR_V_QD   7
#define LCSR_V_OU   6
#define LCSR_V_IUU  5
#define LCSR_V_IUL  4
#define LCSR_V_ABC  3
#define LCSR_V_BER  2
#define LCSR_V_SOF  1
#define LCSR_V_LDD  0


typedef struct _LCD_DescriptorS {
	unsigned long	FDADR;	/* frame descriptor address	     */
	unsigned long	FSADR;	/* frame start address		     */
	unsigned long	FIDR;	/* frame id reg			     */
	unsigned long	LDCMD;	/* LCD command reg		     */
} LCD_DescriptorT;

typedef struct FrameBuffer_16bitS {
	unsigned short	pixel[MAX_XRES * MAX_YRES];
} FrameBuffer_16bitT;

#endif

