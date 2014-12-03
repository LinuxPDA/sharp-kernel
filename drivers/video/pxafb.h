/*
 * linux/drivers/video/pxafb.h
 *    -- Intel PXA250/210 LCD Controller Frame Buffer Device
 *
 *  Copyright (C) 1999 Eric A. Thomas
 *   Based on acornfb.c Copyright (C) Russell King.
 *
 *  2001-08-03: Cliff Brake <cbrake@acclent.com>
 *	 - ported SA1100 code to PXA
 *  
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/*
 * These are the bitfields for each
 * display depth that we support.
 */
struct pxafb_rgb {
	struct fb_bitfield	red;
	struct fb_bitfield	green;
	struct fb_bitfield	blue;
	struct fb_bitfield	transp;
};

/*
 * This structure describes the machine which we are running on.
 */
struct pxafb_mach_info {
	u_long		pixclock;

	u_short		xres;
	u_short		yres;

	u_char		bpp;
	u_char		hsync_len;
	u_char		left_margin;
	u_char		right_margin;

	u_char		vsync_len;
	u_char		upper_margin;
	u_char		lower_margin;
	u_char		sync;

	u_int		cmap_greyscale:1,
			cmap_inverse:1,
			cmap_static:1,
			unused:29;

	u_int		lccr0;
	u_int		lccr3;
};

/* Shadows for LCD controller registers */
struct pxafb_lcd_reg {
	unsigned int lccr0;
	unsigned int lccr1;
	unsigned int lccr2;
	unsigned int lccr3;
};

/* PXA LCD DMA descriptor */
struct pxafb_dma_descriptor {
	unsigned int fdadr;
	unsigned int fsadr;
	unsigned int fidr;
	unsigned int ldcmd;
};

#define RGB_8	(0)
#define RGB_16	(1)
#define NR_RGB	2

struct pxafb_info {
	struct fb_info		fb;
	signed int		currcon;

	struct pxafb_rgb	*rgb[NR_RGB];

	u_int			max_bpp;
	u_int			max_xres;
	u_int			max_yres;

	/*
	 * These are the addresses we mapped
	 * the framebuffer memory region to.
	 */

	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char *		map_cpu;	/* virtual */
	u_int			map_size;

	/* addresses of pieces placed in raw buffer */
	u_char *		screen_cpu;	/* virtual address of frame buffer */
	dma_addr_t		screen_dma;	/* physical address of frame buffer */
	u16 *			palette_cpu;	/* virtual address of palette memory */
	dma_addr_t		palette_dma;	/* physical address of palette memory */
	u_int			palette_size;

	/* DMA descriptors */
	struct pxafb_dma_descriptor * 	dmadesc_fblow_cpu;
	dma_addr_t				dmadesc_fblow_dma;
	struct pxafb_dma_descriptor * 	dmadesc_fbhigh_cpu;
	dma_addr_t				dmadesc_fbhigh_dma;
	struct pxafb_dma_descriptor *	dmadesc_palette_cpu;
	dma_addr_t				dmadesc_palette_dma;

	dma_addr_t		fdadr0;
	dma_addr_t		fdadr1;

	u_int			lccr0;
	u_int			lccr3;
	u_int			cmap_inverse:1,
				cmap_static:1,
				unused:30;

	u_int			reg_lccr0;
	u_int			reg_lccr1;
	u_int			reg_lccr2;
	u_int			reg_lccr3;

	volatile u_char		state;
	volatile u_char		task_state;
	struct semaphore	ctrlr_sem;
	wait_queue_head_t	ctrlr_wait;
	struct tq_struct	task;

#ifdef CONFIG_PM
	struct pm_dev		*pm;
#endif
#ifdef CONFIG_CPU_FREQ
	struct notifier_block	clockchg;
#endif
};

#define __type_entry(ptr,type,member) ((type *)((char *)(ptr)-offsetof(type,member)))

#define TO_INF(ptr,member)	__type_entry(ptr,struct pxafb_info,member)

/*
 * These are the actions for set_ctrlr_state
 */
#define C_DISABLE		(0)
#define C_ENABLE		(1)
#define C_DISABLE_CLKCHANGE	(2)
#define C_ENABLE_CLKCHANGE	(3)
#define C_REENABLE		(4)

#define PXA_NAME	"PXA"

/*
 *  Debug macros 
 */
#if DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

/*
 * Minimum X and Y resolutions
 */
#define MIN_XRES	64
#define MIN_YRES	64

/*
 * Are we configured for 8 or 16 bits per pixel?
 */
#ifdef CONFIG_FB_PXA_8BPP
#  define PXAFB_BPP		8
#  define PXAFB_BPP_BITS	0x03
#elif CONFIG_FB_PXA_16BPP
#  define PXAFB_BPP		16
#  define PXAFB_BPP_BITS	0x04
#endif

#if defined(CONFIG_ARCH_LUBBOCK)
#define LCD_PIXCLOCK			150000
#define LCD_BPP				PXAFB_BPP
#ifdef CONFIG_FB_PXA_QVGA
#define LCD_XRES			320
#define LCD_YRES			240
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	51
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	1
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	1
#define LCD_BEGIN_FRAME_WAIT_COUNT 	8
#define LCD_END_OF_LINE_WAIT_COUNT	1
#define LCD_END_OF_FRAME_WAIT_COUNT	1
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			0x003008F8
#define LCD_LCCR3 			(0x0040FF0C | (PXAFB_BPP_BITS << 24))
#else
#define LCD_XRES			640
#define LCD_YRES			480
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	1
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	1
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	3
#define LCD_BEGIN_FRAME_WAIT_COUNT 	0
#define LCD_END_OF_LINE_WAIT_COUNT	3
#define LCD_END_OF_FRAME_WAIT_COUNT	0
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			0x0030087C
#define LCD_LCCR3 			(0x0040FF0C | (PXAFB_BPP_BITS << 24))
#endif

#elif defined (CONFIG_ARCH_PXA_IDP)
#define LCD_PIXCLOCK			150000
#define LCD_BPP				PXAFB_BPP
#define LCD_XRES			640
#define LCD_YRES			480
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	1
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	1
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	3
#define LCD_BEGIN_FRAME_WAIT_COUNT 	0
#define LCD_END_OF_LINE_WAIT_COUNT	3
#define LCD_END_OF_FRAME_WAIT_COUNT	0
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			0x0030087C
#define LCD_LCCR3 			(0x0040FF0C | (PXAFB_BPP_BITS << 24))

#elif defined CONFIG_PXA_CERF_PDA
#define LCD_PIXCLOCK			171521
#define LCD_BPP				PXAFB_BPP
#define LCD_XRES			240
#define LCD_YRES			320
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	7
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	2
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	17
#define LCD_BEGIN_FRAME_WAIT_COUNT 	0
#define LCD_END_OF_LINE_WAIT_COUNT	17
#define LCD_END_OF_FRAME_WAIT_COUNT	0
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			(LCCR0_LDM | LCCR0_SFM | LCCR0_IUM | LCCR0_EFM | LCCR0_QDM | LCCR0_BM  | LCCR0_OUM)
#define LCD_LCCR3 			(LCCR3_PCP | LCCR3_PixClkDiv(0x12) | LCCR3_Bpp(PXAFB_BPP_BITS) | LCCR3_Acb(0x18))

#endif
