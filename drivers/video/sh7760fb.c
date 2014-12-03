/*
 *  linux/drivers/video/sh7760fb.c
 *
 *  Copyright (c) 2002 Lineo Japan, Inc.
 *  Copyright (c) 2002 clarion,co,LTD.
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 *  LCD Frame Buffer Device Driver for SH7760
 *  This source file was created on the base of SH7727 Frame Buffer
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/wrapper.h>
#include <linux/pm.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
#include <asm/ms7760cp.h>
#else
#error no supported processor type and features
#endif
#include "sh7760fb.h"

#if defined(CONFIG_SH_7760_BISCAYNE)
#if 1
/* for 6.5'CRT Display */
#define XRES 480
#define YRES 234
#define BPP  16
#else
/* for 5.8'CRT Display */
#define XRES 480
#define YRES 234
#define BPP  16
#endif
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)
#define XRES 240
#define YRES 320
#define BPP  16
#else
#error no supported processor type and features
#endif

#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>

/* frame buffer operations */

static int  sh7760fb_get_fix(struct fb_fix_screeninfo*, int, struct fb_info*);
static int  sh7760fb_get_var(struct fb_var_screeninfo*, int, struct fb_info*);
static int  sh7760fb_set_var(struct fb_var_screeninfo*, int, struct fb_info*);
static int  sh7760fb_get_cmap(struct fb_cmap*, int, int, struct fb_info*);
static int  sh7760fb_set_cmap(struct fb_cmap*, int, int, struct fb_info*);
static int  sh7760fb_ioctl(struct inode*, struct file*,
			   unsigned int, unsigned long, int, struct fb_info*);

static void sh7760fb_set_disp(int, struct fb_info*);

#ifdef CONFIG_PM
static struct pm_dev* fb_pm_dev;
#endif

static struct display disp[MAX_NR_CONSOLES];
static struct fb_info fb_info;
static struct fb_ops  sh7760fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	sh7760fb_get_fix,
	fb_get_var:	sh7760fb_get_var,
	fb_set_var:	sh7760fb_set_var,
	fb_get_cmap:	sh7760fb_get_cmap,
	fb_set_cmap:	sh7760fb_set_cmap,
	fb_ioctl:	sh7760fb_ioctl,
};

#define MAX_PIXEL_MEM_SIZE ((XRES * YRES * BPP) / 8)
#define LINE_LENGTH        ((XRES * BPP) / 8)
#define MAX_FRAMEBUFFER_MEM_SIZE (MAX_PIXEL_MEM_SIZE)
#define ALLOCATED_FB_MEM_SIZE \
	(PAGE_ALIGN(MAX_FRAMEBUFFER_MEM_SIZE + PAGE_SIZE))

static u_char* vram_base      = (u_char*)0;
static u_char* vram_base_phys = (u_char*)0;

static u16 fbcon_cmap_cfb16[16];

static int currcon = 0;

static char sh7760fb_name[] = "sh7760fb";

static int sh7760fb_contrast_value  = 0x80;
static int sh7760fb_backlight_value = 0x30;

static int sh7760fb_get_fix(struct fb_fix_screeninfo* fix, int con,
			    struct fb_info* info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	strcpy(fix->id, "sh7760fb");

	fix->smem_start  = (unsigned long)vram_base_phys;
	fix->smem_len    = MAX_PIXEL_MEM_SIZE;
	fix->type        = FB_TYPE_PACKED_PIXELS;
	fix->type_aux    = 0;
	fix->visual      = FB_VISUAL_TRUECOLOR;
	fix->xpanstep    = 0;
	fix->ypanstep    = 0;
	fix->ywrapstep   = 0;
        fix->line_length = LINE_LENGTH;

	return 0;
}
        
static int sh7760fb_get_var(struct fb_var_screeninfo* var, int con,
                           struct fb_info* info)
{
	memset(var, 0, sizeof(struct fb_var_screeninfo));

	var->xres           = XRES;
	var->yres           = YRES;
	var->xres_virtual   = XRES;
	var->yres_virtual   = YRES;
	var->bits_per_pixel = BPP;
	var->xoffset        = 0;
	var->yoffset        = 0;
	var->grayscale      = 0;
	var->nonstd         = 0;
	var->activate       = FB_ACTIVATE_NOW;
	var->height         = YRES;
	var->width          = XRES;
        var->red.length     = 5;
	var->blue.length    = 5;
	var->green.length   = 6;
	var->transp.length  = 0;
	var->red.offset     = 11;
	var->green.offset   = 5;
	var->blue.offset    = 0;
	var->transp.offset  = 0;
	var->pixclock       = 0;
	var->left_margin    = 0;
	var->right_margin   = 0;
	var->hsync_len      = 0;
	var->vsync_len      = 0;
	var->sync           = 0;
	var->vmode          = FB_VMODE_NONINTERLACED;

	return 0;

}

static int sh7760fb_set_var(struct fb_var_screeninfo* var, int con,
			    struct fb_info* info)
{
	if (var->xres != XRES) 
		return -EINVAL;
	if (var->yres != YRES)
		return -EINVAL;
	if (var->xres_virtual != XRES)
		return -EINVAL;
	if (var->yres_virtual != YRES)
		return -EINVAL;
	if (var->xoffset != 0)
		return -EINVAL;
	if (var->yoffset != 0)
		return -EINVAL;
	if (var->bits_per_pixel != BPP)
		return -EINVAL;
	if (var->grayscale != 0)
		return -EINVAL;
	if (var->nonstd != 0)
		return -EINVAL;
	if (var->activate != FB_ACTIVATE_NOW)
		return -EINVAL;
	if (var->pixclock != 0)
		return -EINVAL;
	if (var->left_margin != 0)
		return -EINVAL;
	if (var->right_margin != 0)
		return -EINVAL;
	if (var->hsync_len != 0)
		return -EINVAL;
	if (var->vsync_len != 0)
		return -EINVAL;
	if (var->sync != 0)
		return -EINVAL;
	if (var->vmode != FB_VMODE_NONINTERLACED)
		return -EINVAL;

	return 0;

}

static int sh7760fb_getcolreg(unsigned regno, unsigned* red, unsigned* green,
			      unsigned* blue, unsigned* transp,
			      struct fb_info* info)
{
	if (regno >= 16) return 1;

	*transp = 0;
	*green  = ((fbcon_cmap_cfb16[regno] >> 11) & 31) << 11;
	*red    = ((fbcon_cmap_cfb16[regno] >> 6)  & 31) << 11;
	*blue   = ((fbcon_cmap_cfb16[regno])       & 63) << 10;

	return 0;
}

static int sh7760fb_setcolreg(unsigned regno, unsigned red, unsigned green,
			     unsigned blue, unsigned transp,
			     struct fb_info* info)
{
	red   >>= 11;
	green >>= 11;
	blue  >>= 10;

	if (regno < 16)
		fbcon_cmap_cfb16[regno] = ((red & 31) << 6) |
		                          ((green & 31) << 11) |
					  ((blue & 63));

	return 0;
}

static int sh7760fb_get_cmap(struct fb_cmap* cmap, int kspc, int con,
			    struct fb_info* info)
{
	if (con == currcon)
		return fb_get_cmap(cmap, kspc, sh7760fb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(
		1 << fb_display[con].var.bits_per_pixel), cmap, kspc ? 0 : 2);

	return 0;
}

static int sh7760fb_set_cmap(struct fb_cmap* cmap, int kspc, int con,
			 struct fb_info* info)
{
	int err;

	if (!fb_display[con].cmap.len) {
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
				 1 << fb_display[con].var.bits_per_pixel, 0)))
			return err;
	}

	if (con == currcon) {
	        return fb_set_cmap(cmap, kspc, sh7760fb_setcolreg, info);
	}
	else {
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	}

	return 0;
}

static int sh7760fb_ioctl(struct inode* inode, struct file* file,
			 unsigned int cmd, unsigned long arg, int con,
			 struct fb_info* info)
{
	/* 2002/9/20 I'm going to put in the change routine of a display here.  */
	/* 5.8' LCD <-> 6.5' LCD (CRT;640*480) */
	return -EINVAL;
}

static int __init sh7760fb_map_video_memory(void)
{
	unsigned int required_pages;
	unsigned int extra_pages;
	unsigned int order;
        struct page* page;
        char* allocated_region;

	if (vram_base != NULL)
		return -EINVAL;

	required_pages = ALLOCATED_FB_MEM_SIZE >> PAGE_SHIFT;
        for (order = 0 ; required_pages >> order ; order++) {;}
        extra_pages = (1 << order) - required_pages;

        if ((allocated_region = 
             (char *)__get_free_pages(GFP_KERNEL | GFP_DMA, order)) == NULL) {
		return -ENOMEM;
	}

        vram_base = (u_char *)allocated_region + (extra_pages << PAGE_SHIFT); 
        vram_base_phys = (u_char *)virt_to_phys((void *)vram_base);

	for (;extra_pages;extra_pages--) {
        	free_page((unsigned int)allocated_region +
			  ((extra_pages-1) << PAGE_SHIFT));
	}

	for (page = virt_to_page(vram_base); 
	     page < virt_to_page(vram_base + ALLOCATED_FB_MEM_SIZE); page++) {
		mem_map_reserve(page);
	}

	vram_base = (u_char*)ioremap((u_long)vram_base_phys,
				     ALLOCATED_FB_MEM_SIZE);

	return (vram_base == NULL ? -EINVAL : 0);
}

static void sh7760fb_set_disp(int con, struct fb_info* info)
{
	struct fb_fix_screeninfo fix;
	struct display* display;

	sh7760fb_get_fix(&fix, con, info);

	if (con >= 0) {
		display = &fb_display[con];
	}
	else {
		display = &disp[0];
	}

	if (con < 0) { con = 0; }

	display->screen_base    = (u_char*)vram_base;
	display->visual         = fix.visual;
	display->type           = fix.type;
	display->type_aux       = fix.type_aux;
	display->ypanstep       = fix.ypanstep;
	display->ywrapstep      = fix.ywrapstep;
	display->can_soft_blank = 0;
	display->inverse        = 0;
	display->line_length    = fix.line_length;
	display->scrollmode     = SCROLL_YREDRAW;

#if defined(FBCON_HAS_CFB16)
	display->dispsw         = &fbcon_cfb16;
	disp->dispsw_data       = fbcon_cmap_cfb16;
#else
#error NO FBCON_HAS_CFB16
	display->dispsw         = &fbcon_dummy;
#endif
}
  
static void sh7760fb_disable_lcd_controller(void)
{
	ctrl_outw(0, LDCNTR); /* Display OFF */
}

static void sh7760fb_lcd_enabler(void)
{
	unsigned int fbaddr; 
#if 0
	unsigned short clock;	  /* input clock reg value          */
	unsigned short module;	  /* LCD module type reg value      */
	unsigned short datform;	  /* data format reg value          */
	unsigned short scanmod;	  /* scan mode reg value            */
	unsigned short addroff;	  /* data addr off reg value        */
	unsigned short hchar;	  /* char num of hori reg value     */
	unsigned short hsync;	  /* hori sync timing               */
	unsigned short vline;	  /* LCD vertical line num          */
	unsigned short vlall;	  /* LCD vertical all line num      */
	unsigned short vsync;	  /* LCD VSYNC timing               */
	unsigned short intctrl;	  /* LCD interrupts control         */
	int            divnum;    /* transmit size caliculate num   */
	int            wk;
#endif
	unsigned int   count;	  /* DMA transfer count             */
	unsigned int   dmaope;    /* DMA operation                  */
	unsigned int   reqselect; /* DMA request source select(A)   */
	unsigned int   reqctrl;   /* DMA request control            */
	unsigned int   chctrl;    /* DMA channel control            */

	fbaddr  = (unsigned long)vram_base_phys;
	fbaddr &= 0x1FFFFFFF;

	/* *** ASIC Setting *** */
//	ctrl_outw(0x0003, GRDCLK);

	/* *** DMAC Setting *** */

	count     = DMATCR_CNT; // org
	count     = ALLOCATED_FB_MEM_SIZE / 16;
	chctrl    = 0x00000000;
	reqctrl   = DMARCR_RPR_MODE01;
	reqselect = DMARSRA_CH0;
	dmaope    = DMAOR_DMS_DMABRG | DMAOR_PRI_ROUND;

	ctrl_outl(dmaope,   DMAOR);    // operation req
#if 0
	ctrl_outl(count,    DMATCR0);  // Trans count 
	ctrl_outl(chctrl,   CHCR0);    // CH control
#endif
	ctrl_outl(dmaope | DMAOR_MASTER_EN,   DMAOR); // DMA Enable(all CH) 
	ctrl_outl(reqctrl,  DMARCR);   // DMA req control
	ctrl_outl(reqselect,DMARSRA);  // req source select


#if defined(CONFIG_SH_7760_BISCAYNE)
#if 0
	/* *** LCDC Setting *** */
	/* for VGA (640 * 480) setting */
	clock 	= LDICKR_P_CLK | LDICKR_DCDR1; // org
	module	= LDMTR_FLMPOL | LDMTR_CL1POL | 
		  LDMTR_MCNT   | LDMTR_TFT_COLOR_16;
	datform = LDDFR_COLOR_64K;
	scanmod = 0;
	addroff = 0;	// XRES
	hchar	= 0x4f00 | 0x0063; // XRES / 8 - 1 | XRES / 8 - 1 + 20
	hsync	= 0x1000 | 0x0051;
	vline	= 0x01df; // YRES - 1 org
	vlall	= 0x0276; // 630
	vsync	= 0xf000 | 0x01e9; // width 1 | position 489
	intctrl = LDINTR_VINTE; // vsync interrupt enable

	ctrl_outl(fbaddr,	LDSARU);
#endif


#if 1
	// ********* 6.5'LCD(480*234) setting *************
	// ****** notes 2002-10-24 ********
	// This Setup for 6.5inch LCD(W:480dot * H:234dot)
	// for Toshiba Matsushita TFD65W51
	// ExtClk : 19.19616MHz   DotClk : 9.59808MHz

	ctrl_outw(0x2100 | 0x0002,	LDICKR); // Ext Clk , n/2
	ctrl_outw(0xc42b,	LDMTR); // org
	ctrl_outw(0x002d,	LDDFR);
	ctrl_outl(fbaddr,	LDSARU);
	//for H-SYNC
	ctrl_outw(XRES * BPP / 8 ,	LDLAOR); // (XRES) * bpp / 8
	ctrl_outw(((XRES / 8 - 1) << 8) | (XRES / 8 + 16),LDHCNR);  // XRES - 1 | XRES + 3 
	ctrl_outw((5 -1 << 12) | (XRES / 8 + 2 ),	LDHSYNR); // 9 | XRES / 8    
	//for V-SYNC
	ctrl_outw(YRES -1,	LDVDLNR); // YRES - 1 display OK
	ctrl_outw(YRES + 28,	LDVTLNR); // YRES - 1
	ctrl_outw((04 -1 <<12) | (YRES + 7),	LDVSYNR); // 0 | YRES - 1 
		
	ctrl_outw(0x0000,	LDACLNR);
	ctrl_outw(0x0000,	LDPMMR);
	ctrl_outw(0xffff,	LDPSPR); // org
	// ********* 6.5'LCD(480*234) setting *************
#else
	// ********* 5.8'LCD(400*234) setting *************
	ctrl_outw(0x2100 | 0x0002,	LDICKR); // Ext Clk , n/2
	ctrl_outw(0xc42b,	LDMTR); // org
	ctrl_outw(0x002d,	LDDFR);
	ctrl_outl(fbaddr,	LDSARU);
	
	ctrl_outw(XRES * BPP / 8 + 2,	LDLAOR); // (XRES) * bpp / 8
	ctrl_outw((XRES / 8 + 10) << 8 | XRES / 8 + 12,	LDHCNR);  // XRES - 1 | XRES + 3 
	ctrl_outw(0x00 << 8 | XRES / 8 + 2,	LDHSYNR); // 9 | XRES / 8: org
	
	ctrl_outw(YRES + 9,	LDVDLNR); // YRES - 1 display OK
	ctrl_outw(YRES + 20,	LDVTLNR); // YRES - 1
	ctrl_outw(0x0000 | YRES - 1,	LDVSYNR); // 0 | YRES - 1 
	
	ctrl_outw(0x0000,	LDACLNR);
	ctrl_outw(0x0000,	LDPMMR);
	ctrl_outw(0xffff,	LDPSPR); // org
#endif
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	// ********* QVGA LCD(240*320) setting *************
	ctrl_outw(0x0000,	LDCNTR);

#if 0
	ctrl_outw(0x1100 | 0x0010,	LDICKR);
#else
#if 1
	ctrl_outw(0x1100 | 0x0004,	LDICKR); // Pclk, Pclk/4 = 8.333MHz
#else
	ctrl_outw(0x1100 | 0x0008,	LDICKR); // Pclk, Pclk/8 = 4.125MHz
#endif
#endif
	ctrl_outw(0xc02b,	LDMTR); // TFT color 16bits
	ctrl_outw(0x002d,	LDDFR);
	ctrl_outl(fbaddr,	LDSARU);
	
	ctrl_outw(XRES * BPP / 8 + 2,	LDLAOR); // (XRES) * bpp / 8
	ctrl_outw((XRES / 8 + 10) << 8 | XRES / 8 + 12,	LDHCNR);  // XRES - 1 | XRES + 3 
	ctrl_outw(0x00 << 8 | XRES / 8 + 2,	LDHSYNR); // 9 | XRES / 8: org
	
	ctrl_outw(YRES + 9,	LDVDLNR); // YRES - 1 display OK
	ctrl_outw(YRES + 20,	LDVTLNR); // YRES - 1
	ctrl_outw(0x0000 | YRES - 1,	LDVSYNR); // 0 | YRES - 1 
	
	ctrl_outw(0x000c,	LDACLNR);
	ctrl_outw(0xff70,	LDPMMR);
	ctrl_outw(0x0500,	LDPSPR); // org
	// ********* 5.8'LCD(400*234) setting *************
#else
#error no supported processor type and features
#endif
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	ctrl_outw(0x0001,	LDCNTR);
#else
	ctrl_outw(0x0011,	LDCNTR);
#endif

#if 0
	while ((ctrl_inw(LDPMMR) & (LDPMMR_LPS1+LDPMMR_LPS0))
					!= (LDPMMR_LPS1+LDPMMR_LPS0)){}
#endif
}

static void sh7760fb_enable_lcd_controller(void)
{
	ctrl_outw(0x0000, PCCR); // Port C
	ctrl_outw(0x0000, PDCR); // Port D
	ctrl_outw(0x0000, PECR); // Port E

#if defined(CONFIG_SH_7760_BISCAYNE)
	ctrl_outw(0x0000, IPSELR); // module select SPI:HCAN:FLCTL:I2S:LCDC:(HV:Int :DCLK:Int)
//	ctrl_outw(0x0003, IPSELR); // module select SPI:HCAN:FLCTL:I2S:LCDC:(HV:Ext :DCLK:Ext)
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	ctrl_outw(0xe000, IPSELR);	// LCDC Mode 1 */
#else
#error no supported processor type and features
#endif
	ctrl_outb(0x00, MODSELR); // mode selct

	return sh7760fb_lcd_enabler();
}

int sh7760fb_get_contrast_value(void)
{
	return sh7760fb_contrast_value;
}

int sh7760fb_get_backlight_value(void)
{
	return sh7760fb_backlight_value;
}

void sh7760fb_set_contrast(int value)
{
	sh7760fb_contrast_value += value;
}

void sh7760fb_set_backlight(int value)
{
	sh7760fb_backlight_value = value;
}

static int sh7760fb_updatevar(int con, struct fb_info* info)
{
	return 0;
}

static void sh7760fb_blank(int blank, struct fb_info* info)
{
	if (blank) {
		sh7760fb_disable_lcd_controller();
	}
	else {
		sh7760fb_enable_lcd_controller();
	}
}

static int sh7760fb_switch(int con, struct fb_info* info)
{ 
	currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;
	sh7760fb_set_var(&fb_display[con].var, con, info);
	return 0;
}

#ifdef CONFIG_PM
static int sh7760fb_pm_callback(struct pm_dev* pm_dev,
				pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
		sh7760fb_disable_lcd_controller();
		break;
	case PM_RESUME:
		sh7760fb_enable_lcd_controller();
		break;
	}

	return 0;
}
#endif

int __init sh7760fb_init(void)
{
	fb_info.changevar   = NULL;
	strcpy(&fb_info.modename[0], sh7760fb_name);
	fb_info.fontname[0] = 0;
	fb_info.disp        = disp;
	fb_info.switch_con  = &sh7760fb_switch;
	fb_info.updatevar   = &sh7760fb_updatevar;
	fb_info.blank       = &sh7760fb_blank;	
	fb_info.node        = -1;
	fb_info.fbops       = &sh7760fb_ops;
	fb_info.flags       = FBINFO_FLAG_DEFAULT;
	
	if (sh7760fb_map_video_memory() < 0)
		return -EINVAL;

        sh7760fb_get_var(&disp[0].var, 0, &fb_info);
	sh7760fb_set_disp(-1, &fb_info);

	if (register_framebuffer(&fb_info) < 0) {
#ifdef DEBUG
		printk(KERN_ERR
		       "unable to register SH7760 frame buffer\n");
#endif
		return -EINVAL;
	}

#ifdef CONFIG_PM
	fb_pm_dev = pm_register(PM_SYS_DEV, 0, sh7760fb_pm_callback);
#endif
	sh7760fb_enable_lcd_controller();

	printk("SH7760 LCD frame buffer initialized.\n");

	return 0;
}	

