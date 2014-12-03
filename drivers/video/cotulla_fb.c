/*
 * linux/drivers/video/cotulla_fb.c
 * 
 * Color LCD Frame Buffer Device for COTULLA
 * 
 * Copyright (C) 2001-2002 Lineo Japan, Inc.
 *   Author: Ozawa YOSHIHISA <ozawa@lineo.co.jp>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *  drivers/video/l7200fb.c
 *
 * ChangeLog:
 *   06-08-2002 Richard Rau
 *      add LCD power sequence for discovery
 *   06-26-2002 SHARP
 *      display the SHARP logo while booting.
 *
 */

/*
 * Function List
 * 	cotulla_fb_get_fix			(cotulla_fb_ops)
 * 	cotulla_fb_get_var			(cotulla_fb_ops)
 * 	cotulla_fb_set_var			(cotulla_fb_ops)
 * 	cotulla_fb_getcolreg
 * 	cotulla_fb_setcolreg
 * 	cotulla_fb_get_cmap			(cotulla_fb_ops)
 * 	cotulla_fb_set_cmap			(cotulla_fb_ops)
 * 	cotulla_fb_ioctl			(cotulla_fb_ops)
 * 	cotulla_fb_set_disp
 * 	cotulla_fb_disable_lcd_controller
 * 	cotulla_fb_enable_lcd_controller
 * 	cotulla_fb_updatevar
 * 	cotulla_fb_blank
 * 	cotulla_fb_switch
 * 	cotulla_fb_pm_callback
 * 	cotulla_fb_init				(__init)
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
#include <linux/apm_bios.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/proc/pgtable.h>

#include <asm/arch/cotulla.h>
#ifdef CONFIG_SABINAL_DISCOVERY
#include <asm/arch/discovery_asic3.h>
#endif
#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>

#include "cotulla_fb.h"

//#define DYNAMIC_ALLOCATE 1

/* frame buffer operations */

static int  cotulla_fb_get_fix(struct fb_fix_screeninfo*, int, struct fb_info*);
static int  cotulla_fb_get_var(struct fb_var_screeninfo*, int, struct fb_info*);
static int  cotulla_fb_set_var(struct fb_var_screeninfo*, int, struct fb_info*);
static int  cotulla_fb_get_cmap(struct fb_cmap*, int, int, struct fb_info*);
static int  cotulla_fb_set_cmap(struct fb_cmap*, int, int, struct fb_info*);
static int  cotulla_fb_ioctl(struct inode*, struct file*,
			  unsigned int, unsigned long, int, struct fb_info*);

static void cotulla_fb_set_disp(int, struct fb_info*);

#ifdef CONFIG_PM
static struct pm_dev *fb_pm_dev;
#endif

static struct display disp[MAX_NR_CONSOLES];
static struct fb_info fb_info;
static struct fb_ops  cotulla_fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	cotulla_fb_get_fix,
	fb_get_var:	cotulla_fb_get_var,
	fb_set_var:	cotulla_fb_set_var,
	fb_get_cmap:	cotulla_fb_get_cmap,
	fb_set_cmap:	cotulla_fb_set_cmap,
	fb_ioctl:	cotulla_fb_ioctl,
};

static u_char		*vram_base		= (u_char*)0;
static u_char		*vram_base_phys		= (u_char*)0;
static u_char		*discriptor		= (u_char*)0;
static u_char		*discriptor_phys	= (u_char*)0;

static u_char		*sup_disp 		= NULL;
static u_char		*sup_disp__phys		= NULL;

static LCD_DescriptorT	*LCD_Descriptor_HI;
static unsigned long	pa_LCD_Descriptor_HI;

static u16 fbcon_cmap_cfb16[16];

static int currcon = 0;

static int fb_flag;
static int discovery_fb_blank = 0;

static char cotulla_fb_name[] = "cotulla_fb";

static int cotulla_fb_get_fix(struct fb_fix_screeninfo* fix, int con,
                           struct fb_info* info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	strcpy(fix->id, "cotulla_fb");

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
        
static int cotulla_fb_get_var(struct fb_var_screeninfo* var, int con,
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

static int cotulla_fb_set_var(struct fb_var_screeninfo* var, int con,
			   struct fb_info* info)
{
	if (var->xres		!= XRES			 )	goto ERR;
	if (var->yres		!= YRES			 )	goto ERR;
	if (var->xres_virtual	!= XRES			 )	goto ERR;
	if (var->yres_virtual	!= YRES			 )	goto ERR;
	if (var->xoffset	!= 0			 )	goto ERR;
	if (var->yoffset	!= 0			 )	goto ERR;
	if (var->bits_per_pixel	!= BPP			 )	goto ERR;
	if (var->grayscale	!= 0			 )	goto ERR;
	if (var->nonstd		!= 0			 )	goto ERR;
	if (var->activate	!= FB_ACTIVATE_NOW	 )	goto ERR;
	if (var->pixclock	!= 0			 )	goto ERR;
	if (var->left_margin	!= 0			 )	goto ERR;
	if (var->right_margin	!= 0			 )	goto ERR;
	if (var->hsync_len	!= 0			 )	goto ERR;
	if (var->vsync_len	!= 0			 )	goto ERR;
	if (var->sync		!= 0			 )	goto ERR;
	if (var->vmode		!= FB_VMODE_NONINTERLACED)	goto ERR;

	return 0;

ERR:
	return -EINVAL;
}

static int cotulla_fb_getcolreg(unsigned regno, unsigned* red, unsigned* green,
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

static int cotulla_fb_setcolreg(unsigned regno, unsigned red, unsigned green,
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

static int cotulla_fb_get_cmap(struct fb_cmap* cmap, int kspc, int con,
			    struct fb_info* info)
{
	if (con == currcon)
		return fb_get_cmap(cmap, kspc, cotulla_fb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(
		1 << fb_display[con].var.bits_per_pixel), cmap, kspc ? 0 : 2);

	return 0;
}

static int cotulla_fb_set_cmap(struct fb_cmap* cmap, int kspc, int con,
			 struct fb_info* info)
{
	int err;

	if (!fb_display[con].cmap.len)
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
				 1 << fb_display[con].var.bits_per_pixel, 0)))
			return err;

	if (con == currcon)
	        return fb_set_cmap(cmap, kspc, cotulla_fb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

	return 0;
}

static int cotulla_fb_ioctl(struct inode* inode, struct file* file,
			 unsigned int cmd, unsigned long arg, int con,
			 struct fb_info* info)
{
	return -EINVAL;
}

static void cotulla_fb_set_disp(int con, struct fb_info* info)
{
	struct fb_fix_screeninfo fix;
	struct display* display;

	cotulla_fb_get_fix(&fix, con, info);

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
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
	display->xpanstep       = fix.xpanstep;
	display->xwrapstep      = fix.xwrapstep;
#else
	display->ypanstep       = fix.ypanstep;
	display->ywrapstep      = fix.ywrapstep;
#endif
	display->can_soft_blank = 1;
	display->inverse        = 0;
	display->line_length    = fix.line_length;

#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
	display->scrollmode     = SCROLL_XREDRAW;
#else
	display->scrollmode     = SCROLL_YREDRAW;
#endif

#ifdef FBCON_HAS_CFB16
	display->dispsw         = &fbcon_cfb16;
	disp->dispsw_data       = fbcon_cmap_cfb16;
#else
	display->dispsw         = &fbcon_dummy;
#endif
}
  
static void cotulla_fb_disable_lcd_controller(void)
{
#ifdef CONFIG_SABINAL_DISCOVERY
 		ASIC3_GPIO_PIOD_B |= XDOFF_ON;
	//	mdelay(1);
#endif

	LCCR(0) |= (1 << LCD0_V_DIS);
	
#ifdef CONFIG_SABINAL_DISCOVERY
		LCCR(0) &= ~(1 << LCD0_V_ENB);
	//	mdelay(1);
		ASIC3_GPIO_PIOD_B &= ~LCD_AVDD_DVCC_ON;
	//	mdelay(1);
		ASIC3_GPIO_PIOD_B &= ~VDD_5V_EN;
		mdelay(105);
		ASIC3_GPIO_PIOD_B &= ~XDOFF_ON;
		mdelay(5);
		ASIC3_GPIO_PIOD_B &= ~LCD_VCC_ON;
		mdelay(150);
		ASIC3_GPIO_PIOD_B &= ~VGL_ON;
#endif		
}

static void cotulla_fb_enable_lcd_controller(void)
{
	volatile FrameBuffer_16bitT	*fbP;
	
	if (LCCR(0) == 0x00000081)		// Richard 0703
		return;

#if CONFIG_SABINAL_DISCOVERY
		ASIC3_GPIO_PIOD_B &= 0xffe0;
		mdelay(40);
		ASIC3_GPIO_PIOD_B |= XDOFF_ON;
		ASIC3_GPIO_PIOD_B |= LCD_AVDD_DVCC_ON;
		ASIC3_GPIO_PIOD_B |= LCD_VCC_ON;
		ASIC3_GPIO_PIOD_B |= VDD_5V_EN;
		ASIC3_GPIO_PIOD_B |= VGL_ON;
		mdelay(40);
#endif

	if (fb_flag)
		CKEN		|= LCD_CE;

	LCCR(0)			 = 0;

	LCD_Descriptor_HI	= (LCD_DescriptorT*)discriptor;
	pa_LCD_Descriptor_HI	= (unsigned long)discriptor_phys;

	fbP = (FrameBuffer_16bitT *)vram_base_phys;

	LCD_Descriptor_HI->FDADR	= pa_LCD_Descriptor_HI;
	LCD_Descriptor_HI->FSADR	= (unsigned long)&fbP->pixel[0];
	LCD_Descriptor_HI->FIDR		= 0;
	LCD_Descriptor_HI->LDCMD	= MAX_FRAMEBUFFER_MEM_SIZE;

/*	LCCR(1) =	(0x0B  << LCD1_V_BLW) |
			(0x0B  << LCD1_V_ELW) |
			(0x03  << LCD1_V_HSW) |
			((XRES - 1) << LCD1_V_PPL);

	LCCR(2) =	(0x02  << LCD2_V_BFW) |
			(0x03  << LCD2_V_EFW) |
			(0x02  << LCD2_V_VSW) |
			((YRES - 1) << LCD2_V_LPP);

	LCCR(3) =	(0     << LCD3_V_DPC) |
			(0x04  << LCD3_V_BPP) |
			(0     << LCD3_V_OEP) |
			(1     << LCD3_V_PCP) |
			(0     << LCD3_V_HSP) |
			(0     << LCD3_V_VSP) |
			(0x00  << LCD3_V_API) |
			(0x00  << LCD3_V_ACB) |
			(0x08  << LCD3_V_PCD);
*/
	LCCR(1) = 0x070B0CEF;
	LCCR(2) = 0x121B053F;
	LCCR(3) = 0x0730000A;

	FDADR0	= pa_LCD_Descriptor_HI;

/*	LCCR(0) =	(0   << LCD0_V_OUM)   |
			(1   << LCD0_V_BM)    |
			(0x0 << LCD0_V_PDD)   |
			(0   << LCD0_V_QDM)   |
			(0   << LCD0_V_DIS)   |
			(0   << LCD0_V_DPD)   |
			(0   << LCD0_V_BLE)   |
			(1   << LCD0_V_PAS)   |
			(1   << LCD0_V_EFM)   |
			(1   << LCD0_V_IUM)   |
			(1   << LCD0_V_SFM)   |
			(1   << LCD0_V_LDM)   |
			(0   << LCD0_V_SDS)   |
			(0   << LCD0_V_CMS)   |
			(1   << LCD0_V_ENB);
*/
	LCCR(0) = 0x00000081;
	mdelay(5);
	
#ifdef CONFIG_SABINAL_DISCOVERY
		mdelay(60);
		ASIC3_GPIO_PIOD_B &= ~XDOFF_ON;
#endif
}

static int cotulla_fb_updatevar(int con, struct fb_info* info)
{
	return 0;
}

static void cotulla_fb_blank(int blank, struct fb_info* info)
{
	if (blank) {
		cotulla_fb_disable_lcd_controller();
//	  cotulla_fb_pm_callback(NULL,PM_SUSPEND,NULL);
	}
	else {
		cotulla_fb_enable_lcd_controller();
//	  cotulla_fb_pm_callback(NULL,PM_RESUME,NULL);
	}
}

static int cotulla_fb_switch(int con, struct fb_info* info)
{ 
	currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;
	cotulla_fb_set_var(&fb_display[con].var, con, info);
	return 0;
}

static int screen_backup(void)
{
	if ( sup_disp != NULL ) {
	  	memcpy(sup_disp , vram_base  ,ALLOCATED_FB_MEM_SIZE);
		memset(vram_base,0xff,ALLOCATED_FB_MEM_SIZE);
		mdelay(100);
		printk("screen backup!\n");
	} else {
		printk("backup fail!\n");
	}

	return 0;

}

static int screen_restore(void)
{
	if ( sup_disp != NULL ) {
		memcpy(vram_base, sup_disp , ALLOCATED_FB_MEM_SIZE);
		printk("screen restore!\n");
	}

	return 0;
}

#ifdef CONFIG_PM
static int cotulla_fb_pm_callback(struct pm_dev* pm_dev,
			       pm_request_t req, void* data)
{
	volatile FrameBuffer_16bitT	*fbP;
	switch (req) {
	case PM_SUSPEND:
	  screen_backup();
	  if  ( discovery_fb_blank == 1 ) {
		cotulla_fb_disable_lcd_controller();	
		discovery_fb_blank = 0;
	  }
		break;
	case PM_RESUME:
          screen_restore();
	  if ( discovery_fb_blank == 0 ) {
		fb_flag = 0;
		cotulla_fb_enable_lcd_controller();
		fb_flag = 1;
		discovery_fb_blank = 1;
	  }
		break;
	}
	return 0;
}
#endif

#ifdef CONFIG_COTULLA_LOGO_SCREEN
#include "LogoScreen.c"
static void __init draw_logo_screen(void)
{
	int		x,y;
	unsigned short	*p;
	int		sx, sy;
	int		i;

	p  = (unsigned short *)(vram_base);
	for (y=0; y<YRES-16; y++) {
		for (x=0; x<XRES; x++) {
#if 0
			*p++ = 0x7bef;
#else
			*p++ = 0xFFFF;
#endif
		}
	}

	p  = (unsigned short *)(vram_base);
	sx = (XRES - logo_screen_height) / 2;
	sy = (YRES - logo_screen_width) / 2;
	p += sy*XRES+sx;
	i = 0;
	for (y=0; y<logo_screen_width; y++) {
	  for (x=0; x<logo_screen_height; x++) {
	    unsigned short col = logo_screen_data[(logo_screen_width-1-y)+x*logo_screen_width];
#if 1
	    if (col==0x7bef) col=0xFFFF;
#endif
	    *p++ = col;
	  }
	  p += XRES - logo_screen_height;
	}

}
#endif

int __init cotulla_fb_init(void)
{
	unsigned long	wk1,wk2;

					/* Initialize GPIO - GPAFR	     */
	wk1		 = GPAFR1_U;
	wk2		 = GPAFR2_L;
	wk1		&= 0x000FFFFF;
	wk2		&= 0xF0000000;
	wk1		|= 0xAAA00000;
	wk2		|= 0x0AAAAAAA;
	GPAFR1_U	 = wk1;
	GPAFR2_L	 = wk2;

	fb_flag		 = 1;

	fb_info.changevar   = NULL;
	strcpy(&fb_info.modename[0], cotulla_fb_name);
	fb_info.fontname[0] = 0;
	fb_info.disp        = disp;
	fb_info.switch_con  = &cotulla_fb_switch;
	fb_info.updatevar   = &cotulla_fb_updatevar;
	fb_info.blank       = &cotulla_fb_blank;	
	fb_info.node        = -1;
	fb_info.fbops       = &cotulla_fb_ops;
	fb_info.flags       = FBINFO_FLAG_DEFAULT;

	vram_base	= consistent_alloc(GFP_KERNEL | GFP_DMA,
			ALLOCATED_FB_MEM_SIZE, (dma_addr_t *)&vram_base_phys);
	if (NULL == vram_base)
		return -EINVAL;

        sup_disp	= consistent_alloc(GFP_KERNEL | GFP_DMA,
        		ALLOCATED_FB_MEM_SIZE, (dma_addr_t *)&sup_disp__phys);
	if (NULL == sup_disp)
		return -EINVAL;

	discriptor	= consistent_alloc(GFP_KERNEL | GFP_DMA,
		sizeof(LCD_DescriptorT)	, (dma_addr_t *)&discriptor_phys);
	if (NULL == discriptor)
		return -EINVAL;	
	
        cotulla_fb_get_var(&disp[0].var, 0, &fb_info);
	cotulla_fb_set_disp(-1, &fb_info);

	if (register_framebuffer(&fb_info) < 0) {
		printk(KERN_ERR
		       "unable to register Cotulla frame buffer\n");
		return -EINVAL;
	}
#ifdef CONFIG_PM
	fb_pm_dev = pm_register(PM_COTULLA_DEV, 0, cotulla_fb_pm_callback);
#endif

	LCCR(0) = 0x0;
	cotulla_fb_enable_lcd_controller();
	discovery_fb_blank = 1;	

#ifdef CONFIG_COTULLA_LOGO_SCREEN
	draw_logo_screen();
#endif
	return 0;
}
