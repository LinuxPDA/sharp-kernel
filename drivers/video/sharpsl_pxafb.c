/*
 * linux/drivers/video/sharpsl_pxafb.c
 *
 *  Copyright (C) 2004 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/drivers/video/w100fb.c
 *
 * Frame Buffer Device for ATI w100 (Wallaby)
 *
 * Copyright (C) 2002, ATI Corp.
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
 * Based on:
 *  drivers/video/skeletonfb.c
 *
 * ChangeLog:
 *  28-02-2003 SHARP supported VRAM image cache for ver.1.3
 *  19-03-2003 SHARP disabled VRAM image cache for ver.1.3
 *  16-04-2003 SHARP for Shepherd
 */

// define this here because unistd.h needs it
extern int errno;

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/ucb1200.h>
#include <asm/uaccess.h>
// unistd.h is included for the configuration ioctl stuff
#define __KERNEL_SYSCALLS__ 1
#include <asm/unistd.h>
#undef  __KERNEL_SYSCALLS__
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon.h>

#include "sharpsl_pxafb.h"
#include <linux/pm.h>

#ifdef CONFIG_PM
#include <asm/arch/sharpsl_param.h>
static struct pm_dev *sharpsl_pxafb_pm_dev;
static int sharpsl_pxafb_pm_callback(struct pm_dev* pm_dev,pm_request_t req,
				     void* data);
#endif //CONFIG_PM

static void sharpsl_pxafb_pm_suspend(void);
static void sharpsl_pxafb_pm_resume(void);

static void sharpsl_pxafb_resume(void);
static void sharpsl_pxafb_suspend(void);
static void sharpsl_pxafb_clear_screen(u32 mode,void *fbuf);
static void sharpsl_pxafb_vsync(void);

static void lcdtg_hw_init(u32 mode);
static void lcdtg_lcd_change(u32 mode);
static void lcdtg_resume(void);
static void lcdtg_suspend(void);
static void lcdtg_i2c_start_sequence(u8 base_data);
static void lcdtg_i2c_stop_sequence(u8 base_data);
static void lcdtg_i2c_wait_ack(u8 base_data);

// Some hardware related constants
// The name the kernel will know us by
#define SHARPSL_PXAFB_NAME         "SHARPSL_PXAFB"

// Physical addresses, offsets & lengths
#define	SRAM_BASE	0xfe100000;
#define	SRAM_BASE_PHYS	0x5c000000;

// Pseudo palette size
#define MAX_PALETTES      16

// General frame buffer data structures
struct sharpsl_pxafb_info {
  struct fb_info_gen gen;
  union {
#ifdef FBCON_HAS_CFB16
	u16 cfb16[ MAX_PALETTES];
#endif
#ifdef FBCON_HAS_CFB24
	u32 cfb24[ MAX_PALETTES];
#endif
#ifdef FBCON_HAS_CFB32
	u32 cfb32[ MAX_PALETTES];
#endif
    } fbcon_cmap;
};

struct sharpsl_pxafb_par {
  u32 xres;
  u32 yres;
  u32 xres_virtual;
  u32 yres_virtual;

  u32 bits_per_pixel;

  u32 visual;
  u32 palette_size;
};

static struct sharpsl_pxafb_info fb_info;
static struct sharpsl_pxafb_par current_par;
static int current_par_valid = 0;
static struct display disp;
static struct fb_var_screeninfo default_var;

typedef struct _SHARPSL_PXAFB_CONFIG_ARGS_
{
  int  arg_count;
  char config_filename[256];
  char section_name[64];
  char item_name[64];
  unsigned int supplied_value;
} SHARPSL_PXAFB_CONFIG_ARGS;


// for resolution change
#define LCD_MODE_480    0
#define LCD_MODE_320    1
#define LCD_MODE_240    2
#define LCD_MODE_UNKOWN (-1)

int sharpsl_pxafb_lcdMode = LCD_MODE_UNKOWN; //default UNKOWN

static u16 *gSaveImagePtr[640] = {NULL};
#define SAVE_IMAGE_MAX_SIZE ((640 * 480 * BITS_PER_PIXEL) / 8)

int sharpsl_pxafb_isblank = 0;

static u_char		*vram_base		= (u_char*)0;
static u_char		*vram_base_phys		= (u_char*)0;
static u_char		*vga_vram_base		= (u_char*)0;
static u_char		*vga_vram_base_phys	= (u_char*)0;
static u_char		*discriptor		= (u_char*)0;
static u_char		*discriptor_phys	= (u_char*)0;

int sharpsl_pxafb_init(void);
int sharpsl_pxafb_setup(char*);
static int sharpsl_pxafb_encode_var(struct fb_var_screeninfo *var,
				    const void *raw_par,
				    struct fb_info_gen *info);
static int sharpsl_pxafb_ioctl(struct inode *inode, 
			       struct file *file, 
			       u_int cmd,
			       u_long arg,
			       int con,
			       struct fb_info *info);

#define LCD_SHARP_QVGA 0
#define LCD_SHARP_VGA  1
static void sharpsl_pxafb_init_sharp_lcd(u32 mode);

#ifdef CONFIG_SHARPSL_PXA_CONSISTENT_ALLOC
static void *l_consistent_alloc2(int gfp, size_t size, dma_addr_t *dma_handle,
				 int pte);
static void *consistent_alloc(int gfp, size_t size, dma_addr_t *dma_handle);
static void consistent_free(void *vaddr, size_t size, dma_addr_t handle);
#endif

/* ------------------- chipset specific functions -------------------------- */

//
// Initialization of critical PXA LCDC hardware
//
static int sharpsl_pxafb_hw_init(void)
{
  vga_vram_base = consistent_alloc(GFP_KERNEL | GFP_DMA, ALLOCATED_FB_MEM_SIZE,
				   (dma_addr_t *)&vga_vram_base_phys);
  if (NULL == vga_vram_base)
    return -EINVAL;

  discriptor = consistent_alloc(GFP_KERNEL | GFP_DMA, sizeof(LCD_DescriptorT),
				(dma_addr_t *)&discriptor_phys);
  if (NULL == discriptor)
    return -EINVAL;

  printk("Sharp SL-Series PXA fb driver initialized.\n");

  return 0;
}

//
// PXA LCDC detection and initialization
//
static void sharpsl_pxafb_detect(void)
{

  if (sharpsl_pxafb_hw_init())
    return;

  sharpsl_pxafb_encode_var(&default_var, NULL, NULL);
}

//
// Fill the fix structure based on values in the par
//
static int sharpsl_pxafb_encode_fix(struct fb_fix_screeninfo *fix,
				    const void  *raw_par,
				    struct fb_info_gen *info)
{
  const struct sharpsl_pxafb_par *par = raw_par;

  if (!par)
    BUG();

  memset(fix, 0, sizeof *fix);

  strcpy(fix->id, SHARPSL_PXAFB_NAME);
	
  fix->type= FB_TYPE_PACKED_PIXELS;
  fix->type_aux = 0;

  if (par->bits_per_pixel == 1)
  {
    fix->visual = FB_VISUAL_MONO10;
  }
  else if (par->bits_per_pixel <= 8) 
  {
    fix->visual = FB_VISUAL_PSEUDOCOLOR;
  }
  else
  {
    fix->visual = FB_VISUAL_TRUECOLOR;
  }

#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
  fix->ypanstep = fix->xpanstep = fix->xwrapstep = 0;
#else
  fix->xpanstep = fix->ypanstep = fix->ywrapstep = 0;
#endif

  fix->smem_start = (unsigned long)vram_base_phys;

  if(sharpsl_pxafb_lcdMode == LCD_MODE_UNKOWN ||
     sharpsl_pxafb_lcdMode == LCD_MODE_480){

      fix->line_length = (480 * BITS_PER_PIXEL) / 8;
      fix->smem_len = 0x200000;

  } else if(sharpsl_pxafb_lcdMode == LCD_MODE_240){

      fix->line_length = (240 * BITS_PER_PIXEL) / 8;
      fix->smem_len = 0x60000;

  }

  fix->accel = FB_ACCEL_NONE;

  return 0;
}

//
// Get video parameters out of the par
//
static int sharpsl_pxafb_decode_var(const struct fb_var_screeninfo *var,
				    void *raw_par,
				    struct fb_info_gen *info)
{
  struct sharpsl_pxafb_par *par = raw_par;
  
  *par = current_par;
 
  if (!par)
    BUG();

  if((par->xres == 480 && par->yres == 640)||
     (par->xres == 240 && par->yres == 320)){
      par->xres = var->xres;
      par->yres = var->yres;
  }else{
      par->xres = MAX_XRES;
      par->yres = MAX_YRES;
  }

  par->xres_virtual =
    var->xres_virtual < par->xres ? par->xres : var->xres_virtual;
  par->yres_virtual =
    var->yres_virtual < par->yres ? par->yres : var->yres_virtual;

  par->bits_per_pixel = var->bits_per_pixel;

  par->visual = FB_VISUAL_TRUECOLOR;
  par->palette_size = 0;

  return 0;
}

//
// Fill the var structure with values in the par
//
static int sharpsl_pxafb_encode_var(struct fb_var_screeninfo *var, 
				    const void *raw_par,
				    struct fb_info_gen *info)
{
  struct sharpsl_pxafb_par *par = (struct sharpsl_pxafb_par *)raw_par;

  // set up var for 565
  var->bits_per_pixel = BITS_PER_PIXEL;
  var->red.offset = 11;
  var->red.length = 5;
  var->green.offset = 5;
  var->green.length = 6;
  var->blue.offset = 0;
  var->blue.length = 5;
  var->transp.offset = 0;
  var->transp.length = 0;

  var->red.msb_right = 0;
  var->green.msb_right = 0;
  var->blue.msb_right = 0;
  var->transp.msb_right = 0;

  var->nonstd = 0;

  var->grayscale = 0;

  // set up screen coordinates
  if((par->xres == 480 && par->yres == 640)||
     (par->xres == 240 && par->yres == 320)){
      var->xres = par->xres;
      var->yres = par->yres;
  }else{
      var->xres = MAX_XRES;
      var->yres = MAX_YRES;
  }

  var->xres_virtual = var->xres;
  var->yres_virtual = var->yres;
  var->xoffset = var->yoffset = 0;

  var->activate = FB_ACTIVATE_NOW;

  var->height = -1;
  var->width = -1;
  var->vmode = FB_VMODE_NONINTERLACED;

  var->sync = 0;
  var->pixclock = 0x04; // 171521;


  return 0;
}

//
// Fill the par structure with relevant hardware values
//
static void sharpsl_pxafb_get_par(void *raw_par, struct fb_info_gen *info)
{
  struct sharpsl_pxafb_par *par = raw_par;

  if (current_par_valid)
    *par = current_par;
}

//
// Set the hardware according to the values in the par
//
static void sharpsl_pxafb_set_par(const void *raw_par, struct fb_info_gen *info)
{
  const struct sharpsl_pxafb_par *par = raw_par;

  current_par = *par;
  current_par_valid = 1;
}

//
// Split a color register into RGBT components. N/A on PXA LCDC
//
static int sharpsl_pxafb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			   unsigned *blue, unsigned *transp,
			   struct fb_info *info)
{
  struct sharpsl_pxafb_info *linfo = (struct sharpsl_pxafb_info *)info;  

  if (regno >= 16) return 1;

  *transp = 0;
  *red    = ((linfo->fbcon_cmap.cfb16[regno] >> 11) & 31) << 11;
  *green  = ((linfo->fbcon_cmap.cfb16[regno] >> 5 ) & 63) << 10;
  *blue   = ((linfo->fbcon_cmap.cfb16[regno])       & 31) << 11;

  return 0;
}

//
// Set a single color register. N/A on PXA LCDC
//
static int sharpsl_pxafb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
  struct sharpsl_pxafb_info *linfo = (struct sharpsl_pxafb_info *)info;

  if (regno < MAX_PALETTES)
  {
    linfo->fbcon_cmap.cfb16[regno] =
      (red & 0xf800) | ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
  }

  return 0;
}

//
// Pan the display based on var->xoffset & var->yoffset values
//
static int sharpsl_pxafb_pan_display(const struct fb_var_screeninfo *var,
			     struct fb_info_gen *info)
{

  return 0;
}

//
// Blank the display based on value in blank_mode
//
static int sharpsl_pxafb_blank(int blank_mode, struct fb_info_gen *info)
{
  if(!in_interrupt()){
    if(blank_mode && !sharpsl_pxafb_isblank){
      sharpsl_pxafb_pm_suspend();
      sharpsl_pxafb_restore_vram();
      sharpsl_pxafb_isblank = 1;
    }else if(!blank_mode && sharpsl_pxafb_isblank){
      sharpsl_pxafb_pm_resume();
      sharpsl_pxafb_isblank = 0;
    }
  }

  return 0;
}

#ifdef CONFIG_SHARP_LOGO_SCREEN
#include "vgaLogoScreen.c"
#define SHOW_WAIT_MSG
#ifdef SHOW_WAIT_MSG

#ifdef CONFIG_ARCH_SHARP_SL_J // Japanese message
#include "shepherdLogoMsgJ.c"
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
static int	logo_msg_xoff __initdata = 400;
static int	logo_msg_yoff __initdata = 100;
#else
static int	logo_msg_xoff __initdata = 120;
static int	logo_msg_yoff __initdata = 500;
#endif
#endif

#ifdef CONFIG_ARCH_SHARP_SL_E // English message
#include "shepherdLogoMsg.c"
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
static int	logo_msg_xoff __initdata = 400;
static int	logo_msg_yoff __initdata = 100;
#else
static int	logo_msg_xoff __initdata = 120;
static int	logo_msg_yoff __initdata = 500;
#endif
#endif

#endif
static void __init draw_img( int sx, int sy, int w, int h, unsigned short* data )
{
	int i, x, y;
	unsigned short	*p  = (unsigned short *)(vram_base);

#if defined(CONFIG_FBCON_ROTATE_R)
	p += sy*current_par.xres+sx;
	i = 0;
	for (y=0; y<h; y++) {
	  for (x=0; x<w; x++) {
	    unsigned short col = data[x+y*w];
	    if (col==0x7bef) col=0xFFFF;
	    *p++ = col;
	  }
	  p += current_par.xres - w;
	}
#elif  defined(CONFIG_FBCON_ROTATE_L)
	p += sy*current_par.xres+sx;
	i = 0;
	for (y=0; y<h; y++) {
	  for (x=0; x<w; x++) {
	    unsigned short col = data[(w-1-x)+(h-1-y)*w];
	    if (col==0x7bef) col=0xFFFF;
	    *p++ = col;
	  }
	  p += current_par.xres - w;
	}
#else
	p += sy*current_par.xres+sx;
	i = 0;
	for (y=0; y<h; y++) {
	  for (x=0; x<w; x++) {
	    unsigned short col = data[(h-1-y)+x*h];
	    if (col==0x7bef) col=0xFFFF;
	    *p++ = col;
	  }
	  p += current_par.xres - w;
	}
#endif
}

static void __init draw_logo_screen(void)
{
	int		x,y;
	unsigned short	*p;

#if defined(CONFIG_FBCON_ROTATE_R)
	for (y=0; y<current_par.yres; y++) {
		p = &(((unsigned short *)vram_base)[y*current_par.xres]);
		p += 16;
		for (x=16; x<current_par.xres; x++) {
			*p++ = 0xFFFF;
		}
	}
	draw_img((current_par.xres - logo_screen_width) / 2,
		 (current_par.yres - logo_screen_height) / 2,
		 logo_screen_width,logo_screen_height, logo_screen_data);

#ifdef SHOW_WAIT_MSG
	draw_img(logo_msg_yoff,(current_par.yres - logo_msg_height)/2,
		 logo_msg_width,logo_msg_height,logo_msg_data);
#endif

#elif  defined(CONFIG_FBCON_ROTATE_L)
	for (y=0; y<current_par.yres; y++) {
		p = &(((unsigned short *)vram_base)[y*current_par.xres]);
		for (x=0; x<current_par.xres-16; x++) {
			*p++ = 0xFFFF;
		}
	}
	draw_img((current_par.xres - logo_screen_width) / 2,
		 (current_par.yres - logo_screen_height) / 2,
		 logo_screen_width,logo_screen_height, logo_screen_data);

#ifdef SHOW_WAIT_MSG
	draw_img(logo_msg_xoff,(current_par.yres - logo_msg_height)/2,
		 logo_msg_width,logo_msg_height,logo_msg_data);
#endif

#else
	p  = (unsigned short *)(vram_base);
	for (y=0; y<current_par.yres-16; y++) {
		for (x=0; x<current_par.xres; x++) {
			*p++ = 0xFFFF;
		}
	}
	draw_img((current_par.xres - logo_screen_height) / 2,
		 (current_par.yres - logo_screen_width) / 2,
		 logo_screen_height, logo_screen_width, logo_screen_data);

#ifdef SHOW_WAIT_MSG
	draw_img(logo_msg_xoff,logo_msg_yoff,
		 logo_msg_height,logo_msg_width,logo_msg_data);
#endif

#endif
}
#endif //CONFIG_SHARP_LOGO_SCREEN

//
// Set up the display for the fb subsystem
//
static void sharpsl_pxafb_set_disp(const void *unused, struct display *disp,
				   struct fb_info_gen *info)
{
  u32 temp32;
  struct sharpsl_pxafb_info *linfo = (struct sharpsl_pxafb_info *)info;
  int isInitTG = 0;

  switch(sharpsl_pxafb_lcdMode){
  case LCD_MODE_480:
      if(current_par.xres == 240 && current_par.yres == 320){

	  printk("change resolution 480x640 => 240x320\n");

	  sharpsl_pxafb_vsync();
	  sharpsl_pxafb_init_sharp_lcd(LCD_SHARP_QVGA);
  	  sharpsl_pxafb_clear_screen(LCD_SHARP_QVGA,NULL);
	  lcdtg_lcd_change(LCD_SHARP_QVGA);

	  sharpsl_pxafb_lcdMode = LCD_MODE_240;

      }
      break;
  case LCD_MODE_240:
      if(current_par.xres == 480 && current_par.yres == 640){

	  printk("change resolution 240x320 => 480x640\n");

	  sharpsl_pxafb_clear_screen(LCD_SHARP_QVGA,NULL);
	  sharpsl_pxafb_clear_screen(LCD_SHARP_VGA,NULL);
	  sharpsl_pxafb_vsync();
	  sharpsl_pxafb_init_sharp_lcd(LCD_SHARP_VGA);
	  lcdtg_lcd_change(LCD_SHARP_VGA);

	  sharpsl_pxafb_lcdMode = LCD_MODE_480;
      }
      break;
  case LCD_MODE_UNKOWN:
      printk("reset resolution unkown => 480x640\n");
      sharpsl_pxafb_init_sharp_lcd(LCD_SHARP_VGA);
      sharpsl_pxafb_lcdMode = LCD_MODE_480;
      isInitTG = 1;
      break;
  default:
      printk("resolution error !!! \n");
      break;
  }

  // Do the rest of the display initialization
  disp->screen_base = vram_base;

  // Set appropriate low level text console handler
  switch(disp->var.bits_per_pixel)
  {
#ifdef FBCON_HAS_MFB
  case 1:
    disp->dispsw = &fbcon_mfb;
    break;
#endif	       
#ifdef FBCON_HAS_CFB8
  case 8:
    disp->dispsw = &fbcon_cfb8;
    break;
#endif
#ifdef FBCON_HAS_CFB16
  case 16:
    disp->dispsw = &fbcon_cfb16;
    disp->dispsw_data = linfo->fbcon_cmap.cfb16;
    break;
#endif
  default:
    disp->dispsw = &fbcon_dummy;
    break;
  }

#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
	disp->xpanstep       = 0;
	disp->xwrapstep      = 0;
	disp->scrollmode     = 0; // for logoscreen scroll
#else
	disp->ypanstep       = 0;
	disp->ywrapstep      = 0;
#endif

  switch(sharpsl_pxafb_lcdMode){
  case LCD_MODE_480:
      if(isInitTG != 0)
	  lcdtg_hw_init(LCD_SHARP_VGA);
      break;
  case LCD_MODE_240:
      if(isInitTG != 0)
	  lcdtg_hw_init(LCD_SHARP_QVGA);
      break;
  default:
      printk("resolution error !!! \n");
      break;
  }
}

/* ------------ Interfaces to hardware functions ------------ */


struct fbgen_hwswitch sharpsl_pxafb_switch = 
{
  detect:	sharpsl_pxafb_detect,
  encode_fix:	sharpsl_pxafb_encode_fix,
  decode_var:	sharpsl_pxafb_decode_var,
  encode_var:	sharpsl_pxafb_encode_var,
  get_par:	sharpsl_pxafb_get_par,
  set_par:	sharpsl_pxafb_set_par,
  getcolreg:	sharpsl_pxafb_getcolreg,
  setcolreg:	sharpsl_pxafb_setcolreg,
  pan_display:	sharpsl_pxafb_pan_display,
  blank:	sharpsl_pxafb_blank,
  set_disp:	sharpsl_pxafb_set_disp,
};


/* ------------ Hardware Independent Functions ------------ */


static struct fb_ops sharpsl_pxafb_ops =
{
  owner:	  THIS_MODULE,
  fb_get_fix:	  fbgen_get_fix,
  fb_get_var:	  fbgen_get_var,
  fb_set_var:	  fbgen_set_var,
  fb_get_cmap:	  fbgen_get_cmap,
  fb_set_cmap:	  fbgen_set_cmap,
  fb_pan_display: fbgen_pan_display,
  fb_ioctl:       sharpsl_pxafb_ioctl,
};



int __init sharpsl_pxafb_setup(char *str)
{
  return 0;
}

int __init sharpsl_pxafb_init(void)
{

  sharpsl_pxafb_lcdMode = LCD_MODE_UNKOWN;

  fb_info.gen.fbhw = &sharpsl_pxafb_switch;
  fb_info.gen.fbhw->detect();
  strcpy(fb_info.gen.info.modename, SHARPSL_PXAFB_NAME);
  fb_info.gen.info.changevar = NULL;
  fb_info.gen.info.node = -1;
  fb_info.gen.info.fbops = &sharpsl_pxafb_ops;
  fb_info.gen.info.disp = &disp;
  fb_info.gen.parsize = sizeof(struct sharpsl_pxafb_par);
  fb_info.gen.info.switch_con = &fbgen_switch;
  fb_info.gen.info.updatevar = &fbgen_update_var;
  fb_info.gen.info.blank = &fbgen_blank;
  fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
  /* This should give a reasonable default video mode */
  fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
  fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
  fbgen_set_disp(-1, &fb_info.gen);

  if (disp.var.bits_per_pixel > 1) 
    fbgen_install_cmap(0, &fb_info.gen);

  if (register_framebuffer(&fb_info.gen.info) < 0)
    return -EINVAL;

#ifdef CONFIG_PM
  sharpsl_pxafb_pm_dev = pm_register(PM_COTULLA_DEV, 0, sharpsl_pxafb_pm_callback);
#endif //CONFIG_PM

  printk("fb%d: %s frame buffer device\n", 
	 GET_FB_IDX(fb_info.gen.info.node),
	 fb_info.gen.info.modename);

#if defined(CONFIG_SHARP_LOGO_SCREEN)
	draw_logo_screen();
	corgibl_temporary_contrast_set();
	corgibl_temporary_contrast_reset();
#endif

  return 0;
}

void sharpsl_pxafb_cleanup(struct fb_info *info)
{

  unregister_framebuffer(info);

  if(gSaveImagePtr[0] != NULL){
	  int i;
	  for (i = 0; i < 640; i++) {
		  if (gSaveImagePtr[i] != NULL) {
			  kfree(gSaveImagePtr[i]);
			  gSaveImagePtr[i]=NULL;
		  }
	  }
  }

}

#ifdef MODULE
int __init init_module(void)
{
  return sharpsl_pxafb_init();
}

void cleanup_module(void)
{
  sharpsl_pxafb_cleanup(&fb_info.gen.info);
}
#endif

static int sharpsl_pxafb_ioctl(struct inode *inode, struct file *file,
			       u_int cmd, u_long arg, int con,
			       struct fb_info *info2)
{
  //printk("ERROR: called %s function\n",__func__);
    return -EINVAL;
}

static void sharpsl_pxafb_init_sharp_lcd(u32 mode)
{
    LCD_DescriptorT	*LCD_Descriptor_HI;
    unsigned long	pa_LCD_Descriptor_HI;
    int			i;

    CKEN |= CKEN16_LCD;

    for (i = 0; i < 2; i++) {	/* loop for Errata E15 */
	LCCR(0) = 0;

	LCD_Descriptor_HI	= (LCD_DescriptorT*)discriptor;
	pa_LCD_Descriptor_HI	= (unsigned long)discriptor_phys;

	switch (mode) {
	case LCD_SHARP_QVGA:
	    CKEN |= CKEN20_INTMEM;
	    vram_base = SRAM_BASE;
	    vram_base_phys = SRAM_BASE_PHYS;
	    LCD_Descriptor_HI->FDADR = pa_LCD_Descriptor_HI;
	    LCD_Descriptor_HI->FSADR = vram_base_phys;
	    LCD_Descriptor_HI->FIDR  = 0;
	    LCD_Descriptor_HI->LDCMD = MAX_FRAMEBUFFER_MEM_SIZE / 4;

	    LCCR(0) =	(1   << LCD0_V_LDDALT) |
			(1   << LCD0_V_OUC)    |
			(1   << LCD0_V_CMDIM)  |
			(1   << LCD0_V_RDSTM)  |
			(1   << LCD0_V_OUM)    |
			(1   << LCD0_V_BSM0)   |
			(0x0 << LCD0_V_PDD)    |
			(1   << LCD0_V_QDM)    |
			(0   << LCD0_V_DIS)    |
			(0   << LCD0_V_DPD)    |
			(1   << LCD0_V_PAS)    |
			(1   << LCD0_V_EOFM0)  |
			(1   << LCD0_V_IUM)    |
			(1   << LCD0_V_SOFM0)  |
			(1   << LCD0_V_LDM)    |
			(0   << LCD0_V_SDS)    |
			(0   << LCD0_V_CMS)    |
			(0   << LCD0_V_ENB);

	    LCCR(1) =	( 19  << LCD1_V_BLW) |
			( 45  << LCD1_V_ELW) |
			( 19  << LCD1_V_HSW) |
			((240 - 1) << LCD1_V_PPL);

	    LCCR(2) =	( 1    << LCD2_V_BFW) |
			( 0    << LCD2_V_EFW) |
			( 1   << LCD2_V_VSW) |
			((320 - 1) << LCD2_V_LPP);

	    LCCR(3) =	(0     << LCD3_V_PDFOR) |
			(0     << LCD3_V_BPP3)  |
			(0     << LCD3_V_DPC)   |
			(4     << LCD3_V_BPP)   |
			(0     << LCD3_V_OEP)   |
			(0     << LCD3_V_PCP)   |
			(1     << LCD3_V_HSP)   |
			(1     << LCD3_V_VSP)   |
			(0     << LCD3_V_API)   |
			(0     << LCD3_V_ACB)   |
			(7     << LCD3_V_PCD);
	    break;
	case LCD_SHARP_VGA:
	    vram_base = vga_vram_base;
	    vram_base_phys = vga_vram_base_phys;
	    LCD_Descriptor_HI->FDADR = pa_LCD_Descriptor_HI;
	    LCD_Descriptor_HI->FSADR = vram_base_phys;
	    LCD_Descriptor_HI->FIDR  = 0;
	    LCD_Descriptor_HI->LDCMD = MAX_FRAMEBUFFER_MEM_SIZE;

	    LCCR(0) =	(1   << LCD0_V_LDDALT) |
			(1   << LCD0_V_OUC)    |
			(1   << LCD0_V_CMDIM)  |
			(1   << LCD0_V_RDSTM)  |
			(1   << LCD0_V_OUM)    |
			(1   << LCD0_V_BSM0)   |
			(0x0 << LCD0_V_PDD)    |
			(1   << LCD0_V_QDM)    |
			(0   << LCD0_V_DIS)    |
			(0   << LCD0_V_DPD)    |
			(1   << LCD0_V_PAS)    |
			(1   << LCD0_V_EOFM0)  |
			(1   << LCD0_V_IUM)    |
			(1   << LCD0_V_SOFM0)  |
			(1   << LCD0_V_LDM)    |
			(0   << LCD0_V_SDS)    |
			(0   << LCD0_V_CMS)    |
			(0   << LCD0_V_ENB);

	    LCCR(1) =	( 45  << LCD1_V_BLW) |
			( 124 << LCD1_V_ELW) |
			( 39  << LCD1_V_HSW) |
			((480 - 1) << LCD1_V_PPL);

	    LCCR(2) =	( 1    << LCD2_V_BFW) |
			( 0    << LCD2_V_EFW) |
			( 2    << LCD2_V_VSW) |
			((640 - 1) << LCD2_V_LPP);

	    LCCR(3) =	(0     << LCD3_V_PDFOR) |
			(0     << LCD3_V_BPP3)  |
			(0     << LCD3_V_DPC)   |
			(4     << LCD3_V_BPP)   |
			(0     << LCD3_V_OEP)   |
			(0     << LCD3_V_PCP)   |
			(1     << LCD3_V_HSP)   |
			(1     << LCD3_V_VSP)   |
			(0     << LCD3_V_API)   |
			(0     << LCD3_V_ACB)   |
			(1     << LCD3_V_PCD);
	    CKEN &= ~CKEN20_INTMEM;
	    break;
	}

	LCDBSCNTR = 5;

	FDADR0 = pa_LCD_Descriptor_HI;

	LCSR0 = 0x00001FFF;
	LCSR1 = 0x3E3F3F3F;

	LCCR0 |= (1 << LCD0_V_ENB);
    }
} // sharpsl_pxafb_init_sharp_lcd

static void sharpsl_pxafb_suspend()
{
  //printk("%s:LCCR0=%x\n",__func__,LCCR0);
  LCCR0 |= (1 << LCD0_V_DIS);
}

static void sharpsl_pxafb_resume()
{
	if (sharpsl_pxafb_lcdMode == LCD_MODE_480) {
		sharpsl_pxafb_init_sharp_lcd(LCD_SHARP_VGA);
	}
	else {
		sharpsl_pxafb_init_sharp_lcd(LCD_SHARP_QVGA);
	}
}

static void sharpsl_pxafb_clear_screen(u32 mode,void *pfbuf)
{
  u16 *pVram = (u16 *)vga_vram_base;
  int i,numPix=0;

  if (mode == LCD_SHARP_QVGA)
    pVram = SRAM_BASE;

  if(pfbuf != NULL)
      pVram = pfbuf;

  if(mode == LCD_SHARP_VGA){
    numPix = 640*480;
  }else if(mode == LCD_SHARP_QVGA){
    numPix = 320*240;
  }
#if 1
    memset(pVram,0x0000,numPix*2);
#else
  for(i=0;i<numPix;i++)
    pVram[i] = 0xffff;
#endif
}

#define SHARPSL_PXA_VSYNC_TIMEOUT 30000 // timeout = 30[ms] > 16.8[ms]
//
static void sharpsl_pxafb_vsync()
{
    int timeout = SHARPSL_PXA_VSYNC_TIMEOUT;

    while(timeout > 0)
    {
	if ((GPLR(GPIO74_LCD_FCLK) & GPIO_bit(GPIO74_LCD_FCLK)))
	    break;
	udelay(1);
	timeout--;
    }
    while(timeout > 0)
    {
	if (!(GPLR(GPIO74_LCD_FCLK) & GPIO_bit(GPIO74_LCD_FCLK)))
	    break;
	udelay(1);
	timeout--;
    }
}

void sharpsl_pxafb_set_clock(int clock)
{
    if (sharpsl_pxafb_lcdMode == LCD_MODE_480) {
	if (clock)
	    LCCR(3) = (LCCR(3) & ~LCD3_M_PCD) | 0;
	else
	    LCCR(3) = (LCCR(3) & ~LCD3_M_PCD) | 1;
    } else {
	if (clock)
	    LCCR(3) = (LCCR(3) & ~LCD3_M_PCD) | 3;
	else
	    LCCR(3) = (LCCR(3) & ~LCD3_M_PCD) | 7;
    }
}

#define RESCTL_ADRS     0x00
#define PHACTRL_ADRS	0x01
#define DUTYCTRL_ADRS	0x02
#define POWERREG0_ADRS	0x03
#define POWERREG1_ADRS	0x04
#define GPOR3_ADRS	0x05
#define PICTRL_ADRS     0x06
#define POLCTRL_ADRS    0x07

#define RESCTL_QVGA     0x01
#define RESCTL_VGA      0x00

#define POWER1_VW_ON	0x01	/* VW Supply FET ON */
#define POWER1_GVSS_ON	0x02	/* GVSS(-8V) Power Supply ON */
#define POWER1_VDD_ON	0x04	/* VDD(8V),SVSS(-4V) Power Supply ON */

#define POWER1_VW_OFF	0x00	/* VW Supply FET OFF */
#define POWER1_GVSS_OFF	0x00	/* GVSS(-8V) Power Supply OFF */
#define POWER1_VDD_OFF	0x00	/* VDD(8V),SVSS(-4V) Power Supply OFF */

#define POWER0_COM_DCLK	0x01	/* COM Voltage DC Bias DAC Serial Data Clock */
#define POWER0_COM_DOUT	0x02	/* COM Voltage DC Bias DAC Serial Data Out */
#define POWER0_DAC_ON	0x04	/* DAC Power Supply ON */
#define POWER0_COM_ON	0x08	/* COM Powewr Supply ON */
#define POWER0_VCC5_ON	0x10	/* VCC5 Power Supply ON */

#define POWER0_DAC_OFF	0x00	/* DAC Power Supply OFF */
#define POWER0_COM_OFF	0x00	/* COM Powewr Supply OFF */
#define POWER0_VCC5_OFF	0x00	/* VCC5 Power Supply OFF */


#define POWER0_I2C_DATA 	(POWER0_DAC_ON /* DAC ON */ | \
	POWER0_COM_OFF /* COM OFF */ | POWER0_VCC5_OFF /* VCC5 OFF */)

#define PICTRL_INIT_STATE	0x01
#define PICTRL_INIOFF		0x02
#define PICTRL_POWER_DOWN	0x04
#define PICTRL_COM_SIGNAL_OFF	0x08
#define PICTRL_DAC_SIGNAL_OFF	0x10

#define PICTRL_POWER_ACTIVE	(0)

#define POLCTRL_SYNC_POL_FALL	0x01
#define POLCTRL_EN_POL_FALL	0x02
#define POLCTRL_DATA_POL_FALL	0x04
#define POLCTRL_SYNC_ACT_H	0x08
#define POLCTRL_EN_ACT_L	0x10

#define POLCTRL_SYNC_POL_RISE	0x00
#define POLCTRL_EN_POL_RISE	0x00
#define POLCTRL_DATA_POL_RISE	0x00
#define POLCTRL_SYNC_ACT_L	0x00
#define POLCTRL_EN_ACT_H	0x00

#define PHACTRL_PHASE_MANUAL	0x01

#define LCDTG_I2C_WAIT (10) /* usec */

#define PHAD_QVGA_DEFAULT_VAL (9)
#define COMADJ_DEFAULT        (125)

static void lcdtg_ssp_send(u8 adrs, u8 data)
{
    ssp_put_dac_val(((adrs & 0x07) << 5) | (data & 0x1f), CS_LZ9JG18);
}

static void lcdtg_i2c_start_sequence(u8 base_data)
{
    u8 base = base_data;

    lcdtg_ssp_send(POWERREG0_ADRS, base|POWER0_COM_DCLK|POWER0_COM_DOUT);
    udelay(LCDTG_I2C_WAIT);
    lcdtg_ssp_send(POWERREG0_ADRS, base|POWER0_COM_DCLK);
    udelay(LCDTG_I2C_WAIT);
    lcdtg_ssp_send(POWERREG0_ADRS, base);
    udelay(LCDTG_I2C_WAIT);
}

static void lcdtg_i2c_stop_sequence(u8 base_data)
{
    u8 base = base_data;

    lcdtg_ssp_send(POWERREG0_ADRS, base);
    udelay(LCDTG_I2C_WAIT);
    lcdtg_ssp_send(POWERREG0_ADRS, base|POWER0_COM_DCLK);
    udelay(LCDTG_I2C_WAIT);
    lcdtg_ssp_send(POWERREG0_ADRS, base|POWER0_COM_DCLK|POWER0_COM_DOUT);
    udelay(LCDTG_I2C_WAIT);
}

static void lcdtg_i2c_send_byte(u8 base_data,u8 data)
{
    int i;

    u8 base = base_data;

    for( i = 0; i < 8; i++ ){
	if(data & 0x80){
	    lcdtg_ssp_send(POWERREG0_ADRS, base|POWER0_COM_DOUT);
	    udelay(LCDTG_I2C_WAIT);
	    lcdtg_ssp_send(POWERREG0_ADRS, 
			   base|POWER0_COM_DOUT|POWER0_COM_DCLK);
	    udelay(LCDTG_I2C_WAIT);
	    lcdtg_ssp_send(POWERREG0_ADRS, base|POWER0_COM_DOUT);
	}else{
	    lcdtg_ssp_send(POWERREG0_ADRS, base);
	    udelay(LCDTG_I2C_WAIT);
	    lcdtg_ssp_send(POWERREG0_ADRS, base|POWER0_COM_DCLK);
	    udelay(LCDTG_I2C_WAIT);
	    lcdtg_ssp_send(POWERREG0_ADRS, base);
	}
	udelay(LCDTG_I2C_WAIT);
	data <<= 1;
    }
}

static void lcdtg_i2c_wait_ack(u8 base_data)
{
    u8 base = base_data;

    lcdtg_ssp_send(POWERREG0_ADRS, base);
    udelay(LCDTG_I2C_WAIT);
    lcdtg_ssp_send(POWERREG0_ADRS, base|POWER0_COM_DCLK);
    udelay(LCDTG_I2C_WAIT);
    lcdtg_ssp_send(POWERREG0_ADRS, base);
    udelay(LCDTG_I2C_WAIT);
}

static void lcdtg_set_common_voltage(u8 base_data,u8 data)
{
    /* Set Common Voltage to M62232FP via I2C */
    lcdtg_i2c_start_sequence( base_data );
    lcdtg_i2c_send_byte( base_data, 0x9c );
    lcdtg_i2c_wait_ack( base_data );
    lcdtg_i2c_send_byte( base_data, 0x00 );
    lcdtg_i2c_wait_ack( base_data );
    lcdtg_i2c_send_byte( base_data, data );
    lcdtg_i2c_wait_ack( base_data );
    lcdtg_i2c_stop_sequence( base_data );
}

static struct lcdtg_register_setting {
    u8 adrs;
    u8 data;
    u32 wait;
} lcdtg_power_on_table[] = {

    /* Initialize Internal Logic & Port */
    { PICTRL_ADRS, 
      PICTRL_POWER_DOWN | PICTRL_INIOFF | PICTRL_INIT_STATE |
      PICTRL_COM_SIGNAL_OFF | PICTRL_DAC_SIGNAL_OFF,
      0 }, 

    { POWERREG0_ADRS,
      POWER0_COM_DCLK | POWER0_COM_DOUT | POWER0_DAC_OFF | POWER0_COM_OFF | 
      POWER0_VCC5_OFF,
      0 },

    { POWERREG1_ADRS,
      POWER1_VW_OFF | POWER1_GVSS_OFF | POWER1_VDD_OFF,
      0 },

    /* VDD(+8V),SVSS(-4V) ON */
    { POWERREG1_ADRS,
      POWER1_VW_OFF | POWER1_GVSS_OFF | POWER1_VDD_ON /* VDD ON */,
      3000 },

    /* DAC ON */
    { POWERREG0_ADRS,
      POWER0_COM_DCLK | POWER0_COM_DOUT | POWER0_DAC_ON /* DAC ON */ | 
      POWER0_COM_OFF | POWER0_VCC5_OFF,
      0 },

    /* INIB = H, INI = L  */
    { PICTRL_ADRS, 
      /* PICTL[0] = H , PICTL[1] = PICTL[2] = PICTL[4] = L */
      PICTRL_INIT_STATE | PICTRL_COM_SIGNAL_OFF,
      0 }, 

    /* Set Common Voltage */
    { 0xfe, 0, 0 },

    /* VCC5 ON */
    { POWERREG0_ADRS,
      POWER0_COM_DCLK | POWER0_COM_DOUT | POWER0_DAC_ON /* DAC ON */ | 
      POWER0_COM_OFF | POWER0_VCC5_ON /* VCC5 ON */,
      0 },

    /* GVSS(-8V) ON */
    { POWERREG1_ADRS,
      POWER1_VW_OFF | POWER1_GVSS_ON /* GVSS ON */ | 
      POWER1_VDD_ON /* VDD ON */,
      2000 },

    /* COM SIGNAL ON (PICTL[3] = L) */
    { PICTRL_ADRS, 
      PICTRL_INIT_STATE,
      0 },

    /* COM ON */
    { POWERREG0_ADRS,
      POWER0_COM_DCLK | POWER0_COM_DOUT | POWER0_DAC_ON /* DAC ON */ | 
      POWER0_COM_ON /* COM ON */ | POWER0_VCC5_ON /* VCC5_ON */,
      0 },

    /* VW ON */
    { POWERREG1_ADRS,
      POWER1_VW_ON /* VW ON */ | POWER1_GVSS_ON /* GVSS ON */ | 
      POWER1_VDD_ON /* VDD ON */,
      0 /* Wait 100ms */ },

    /* Signals output enable */
    { PICTRL_ADRS, 
      0 /* Signals output enable */,
      0 },

    { PHACTRL_ADRS,
      PHACTRL_PHASE_MANUAL,
      0 },

    /* Initialize for Input Signals from ATI */
    { POLCTRL_ADRS,
      POLCTRL_SYNC_POL_RISE | POLCTRL_EN_POL_RISE | POLCTRL_DATA_POL_RISE |
      POLCTRL_SYNC_ACT_L | POLCTRL_EN_ACT_H,
      1000 /*100000*/ /* Wait 100ms */ },

    /* end mark */
    { 0xff, 0, 0 }
};

static void lcdtg_resume()
{
  if (sharpsl_pxafb_lcdMode == LCD_MODE_480) {
	  lcdtg_hw_init(LCD_SHARP_VGA);
  }
  else {
	  lcdtg_hw_init(LCD_SHARP_QVGA);
  }
  corgibl_pm_callback(NULL, PM_RESUME, NULL);
}

static void lcdtg_suspend()
{
    int i;
    u16 *pVram = (u16*)vram_base;

    // (1) backlight off
    corgibl_pm_callback(NULL, PM_SUSPEND, NULL);

    for( i = 0; i < (current_par.xres * current_par.yres); i++ ) {
	*pVram = 0xffff; // white color
	pVram++;
    }

    // 60Hz x 2 frame = 16.7msec x 2 = 33.4 msec
    mdelay(34);


    // (2)VW OFF */  
    lcdtg_ssp_send( POWERREG1_ADRS, POWER1_VW_OFF | POWER1_GVSS_ON | POWER1_VDD_ON );	

    // (4)COM OFF */  
    lcdtg_ssp_send( PICTRL_ADRS, PICTRL_COM_SIGNAL_OFF  );
    lcdtg_ssp_send( POWERREG0_ADRS, POWER0_DAC_ON | POWER0_COM_OFF | POWER0_VCC5_ON );

    // (5)Set Common Voltage Bias 0V */
    lcdtg_set_common_voltage(POWER0_DAC_ON | POWER0_COM_OFF | POWER0_VCC5_ON ,0);

    // (6)GVSS OFF */
    lcdtg_ssp_send( POWERREG1_ADRS, POWER1_VW_OFF | POWER1_GVSS_OFF | POWER1_VDD_ON );

    // (7)VCC5 OFF */
    lcdtg_ssp_send( POWERREG0_ADRS, POWER0_DAC_ON | POWER0_COM_OFF | POWER0_VCC5_OFF );

    // (8)Set PDWN, INIOFF, DACOFF */
    lcdtg_ssp_send( PICTRL_ADRS, PICTRL_INIOFF | PICTRL_DAC_SIGNAL_OFF | 
		    PICTRL_POWER_DOWN | PICTRL_COM_SIGNAL_OFF  );

    /* (9)Stop LCDC */

    // (10)DAC OFF */
    lcdtg_ssp_send( POWERREG0_ADRS, POWER0_DAC_OFF | POWER0_COM_OFF | POWER0_VCC5_OFF );

    // (11)VDD OFF */
    lcdtg_ssp_send( POWERREG1_ADRS, POWER1_VW_OFF | POWER1_GVSS_OFF | POWER1_VDD_OFF );

}
static void lcdtg_set_phadadj(u32 mode)
{
    int adj;

    if(mode == LCD_SHARP_VGA){
	// Setting for VGA
	adj = -1;
	adj = sharpsl_get_phadadj();
	if ( adj < 0 ) {
	    adj = PHACTRL_PHASE_MANUAL;
	}else{
	    adj = ((adj & 0x0f)<< 1)| PHACTRL_PHASE_MANUAL;
	}
    }else{
	// Setting for QVGA
	adj = (PHAD_QVGA_DEFAULT_VAL << 1)| PHACTRL_PHASE_MANUAL;
    }
    lcdtg_ssp_send( PHACTRL_ADRS, adj );
}

static void lcdtg_hw_init(u32 mode)
{
    int i;
    int comadj;

    i = 0;
    while(lcdtg_power_on_table[i].adrs != 0xff){
	if (lcdtg_power_on_table[i].adrs == 0xfe) {
	    /* Set Common Voltage */
	    comadj = -1;
	    comadj = sharpsl_get_comadj();
	    if ( comadj < 0 ) {
		comadj = COMADJ_DEFAULT;
	    }
	    lcdtg_set_common_voltage( POWER0_I2C_DATA, comadj );
	}else if(lcdtg_power_on_table[i].adrs == PHACTRL_ADRS){
	    /* Set Phase Adjuct */
	    lcdtg_set_phadadj(mode);
	}else{
	    /* Other */
	    lcdtg_ssp_send( lcdtg_power_on_table[i].adrs,
			    lcdtg_power_on_table[i].data );
	}
	if(lcdtg_power_on_table[i].wait != 0)
	    udelay(lcdtg_power_on_table[i].wait);
	i++;
    }

    switch(mode){
    case LCD_SHARP_QVGA:
	/* Set Lcd Resolution (QVGA) */
	lcdtg_ssp_send( RESCTL_ADRS, RESCTL_QVGA );
	break;
    case LCD_SHARP_VGA:
	/* Set Lcd Resolution (VGA) */
	lcdtg_ssp_send( RESCTL_ADRS, RESCTL_VGA );
	break;
    default:
	break;
    }
}

static void lcdtg_lcd_change(u32 mode)
{
    /* Set Phase Adjuct */
    lcdtg_set_phadadj(mode);

    if(mode == LCD_SHARP_VGA)
	/* Set Lcd Resolution (VGA) */
	lcdtg_ssp_send( RESCTL_ADRS, RESCTL_VGA );
    else if(mode == LCD_SHARP_QVGA)
	/* Set Lcd Resolution (QVGA) */
	lcdtg_ssp_send( RESCTL_ADRS, RESCTL_QVGA );
}

static void sharpsl_pxafb_store_vram(void)
{
    int i, j;
    u16 *pVram = (u16*)vram_base;

    for (i = 0; i < current_par.yres; i++) {
      if (gSaveImagePtr[i] != NULL){
	kfree(gSaveImagePtr[i]);
	gSaveImagePtr[i] = NULL;
      }
      gSaveImagePtr[i] = kmalloc(current_par.xres * BITS_PER_PIXEL / 8, GFP_KERNEL);
      if (gSaveImagePtr[i] != NULL){
	for (j = 0; j < (current_par.xres); j++)
	  *(gSaveImagePtr[i] + j) = *(pVram++);
      }
      else {
	printk("can't alloc pre-off image buffer %d\n", i);
	for (j = 0; j < i; j++) {
	  if (gSaveImagePtr[i] != NULL) {
	    kfree(gSaveImagePtr[i]);
	    gSaveImagePtr[i] = NULL;
	  }
	}
	break;
      }
    }
    for (; i < 640; i++) {
      gSaveImagePtr[i] = NULL;
    }
}

static void sharpsl_pxafb_restore_vram(void)
{
    int i, j;
    u16 *pVram = (u16*)vram_base;

    if (gSaveImagePtr[0] != NULL){
      for (i = 0; i < (current_par.yres); i++) {
	  if (gSaveImagePtr[i] == NULL) {
	    printk("can't find pre-off image buffer %d\n", i);
	    continue;
	  }
	  for (j = 0; j < (current_par.xres); j++) {
	    *(pVram++) = *(gSaveImagePtr[i] + j);
	  }
	  kfree(gSaveImagePtr[i]);
	  gSaveImagePtr[i] = NULL;
      }
    }
}

static void sharpsl_pxafb_pm_suspend(void)
{
    sharpsl_pxafb_store_vram();
    lcdtg_suspend();
    sharpsl_pxafb_suspend();
}

static void sharpsl_pxafb_pm_resume(void)
{
    int i, j;
    u16 *pVram = (u16*)vram_base;

    sharpsl_pxafb_resume();
    sharpsl_pxafb_restore_vram();
    lcdtg_resume();
}

#ifdef CONFIG_PM
static int sharpsl_pxafb_pm_callback(struct pm_dev* pm_dev,
				     pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
		if (!sharpsl_pxafb_isblank) {
		  sharpsl_pxafb_pm_suspend();
		}else{
		  sharpsl_pxafb_store_vram();
		}
		sharpsl_pxafb_isblank = 0;
		break;
		
	case PM_RESUME:
		sharpsl_pxafb_pm_resume();
		sharpsl_pxafb_isblank = 0;
		break;
		
	}

	return 0;
}

void sharpsl_pxafb_fatal_off(void)
{
  lcdtg_suspend();
  sharpsl_pxafb_suspend();
}
#endif //CONFIG_PM

#ifdef CONFIG_SHARPSL_PXA_CONSISTENT_ALLOC
//////////////////////////////////////////////
#include <linux/interrupt.h>

/*
 * This allocates one page of cache-coherent memory space and returns
 * both the virtual and a "dma" address to that space.  It is not clear
 * whether this could be called from an interrupt context or not.  For
 * now, we expressly forbid it, especially as some of the stuff we do
 * here is not interrupt context safe.
 *
 * We should allow this function to be called from interrupt context.
 * However, we call ioremap, which needs to fiddle around with various
 * things (like the vmlist_lock, and allocating page tables).  These
 * things aren't interrupt safe (yet).
 *
 * Note that this does *not* zero the allocated area!
 */
static void *consistent_alloc(int gfp, size_t size, dma_addr_t *dma_handle)
{
    return l_consistent_alloc2(gfp, size, dma_handle, L_PTE_CACHEABLE);
}

static void *l_consistent_alloc2(int gfp, size_t size, dma_addr_t *dma_handle, int pte)
{
	struct page *page, *end, *free;
	unsigned long order;
	void *ret;

	/* FIXME */
	if (in_interrupt())
		BUG();

	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = alloc_pages(gfp, order);
	if (!page)
		goto no_page;

	*dma_handle = page_to_bus(page);
	//ret = __ioremap(page_to_pfn(page) << PAGE_SHIFT, size, 0);
	ret = __ioremap(page_to_pfn(page) << PAGE_SHIFT, size, pte);
	if (!ret)
		goto no_remap;

#if 0 /* ioremap_does_flush_cache_all */
	{
		void *virt = page_address(page);

		/*
		 * we need to ensure that there are no cachelines in use, or
		 * worse dirty in this area.  Really, we don't need to do
		 * this since __ioremap does a flush_cache_all() anyway. --rmk
		 */
		invalidate_dcache_range(virt, virt + size);
	}
#endif

	/*
	 * free wasted pages.  We skip the first page since we know
	 * that it will have count = 1 and won't require freeing.
	 * We also mark the pages in use as reserved so that
	 * remap_page_range works.
	 */
	free = page + (size >> PAGE_SHIFT);
	end  = page + (1 << order);

	for (; page < end; page++) {
		set_page_count(page, 1);
		if (page >= free)
			__free_page(page);
		else
			SetPageReserved(page);
	}
	return ret;

no_remap:
	__free_pages(page, order);
no_page:
	return NULL;
}

/*
 * free a page as defined by the above mapping.  We expressly forbid
 * calling this from interrupt context.
 */
static void consistent_free(void *vaddr, size_t size, dma_addr_t handle)
{
	struct page *page, *end;

	if (in_interrupt())
		BUG();

	/*
	 * More messing around with the MM internals.  This is
	 * sick, but then so is remap_page_range().
	 */
	size = PAGE_ALIGN(size);
	page = virt_to_page(bus_to_virt(handle));
	end = page + (size >> PAGE_SHIFT);

	for (; page < end; page++)
		ClearPageReserved(page);

	__iounmap(vaddr);
}
#endif
