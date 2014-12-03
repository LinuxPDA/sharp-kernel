/*
 * linux/drivers/video/tc6393fb.c
 *
 * Frame Buffer Device for TOSHIBA tc6393
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 * 
 * Based on:
 * linux/drivers/video/w100fb.c
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

#include "tc6393fb.h"
#include <linux/pm.h>
#include <asm/arch/i2sc.h>
#include <asm/arch/tosa.h>

#include <video/tosa_backlight.h>

#ifdef CONFIG_PM
#include <asm/arch/sharpsl_param.h>
static struct pm_dev *tc6393_pm_dev;
static int tc6393_pm_callback(struct pm_dev* pm_dev,pm_request_t req, void* data);
#endif //CONFIG_PM

static void tc6393_pm_suspend(int suspend_mode);
static void tc6393_pm_resume(void);
static void tc6393_clear_screen(u32 mode);

// Some hardware related constants
// The name the kernel will know us by
#define TC6393_NAME         "TC6393FB"


#define KH_CMDREG_NUM		6

#define PIXEL_PER_LINE_QVGA 240
#define HORIZONTAL_LINE_QVGA 320
#define PIXEL_PER_LINE_VGA 480
#define HORIZONTAL_LINE_VGA 640

typedef struct FrameBuffer_QVGA {
  unsigned short pixel[HORIZONTAL_LINE_QVGA][PIXEL_PER_LINE_QVGA];
} FrameBuffer_QVGA;

typedef struct FrameBuffer_VGA {
  unsigned short pixel[HORIZONTAL_LINE_VGA][PIXEL_PER_LINE_VGA];
} FrameBuffer_VGA;

static TC6393_REGS kh_cmd[] = 
{
	{ KH_BASEADDR_H,	0x0000},         // base address high
	{ KH_BASEADDR_L,	TC6393_GC_INTERNAL_REG_BASE},  	// base address low
	{ KH_COMMAND_REG,	0x0002},         // base address enable
	{ KH_VRAMTC,		0x40a8},         // VRAMRC, VRAMTC
	{ KH_VRAMAC,		0x0018},         // VRAMSTS, VRAMAC
	{ KH_VRAMBC,		0x0002},
};

#define TC6393_lcdinner   (TC6393_SYS_BASE+TC6393_GC_INTERNAL_REG_BASE)

#define FB_OFFSET         0x00100000
#define TC6393_LCD_INNER_ADDRESS TC6393_lcdinner
#define TC6393_FB_BASE      TC6393_RAM1_BASE
#define REMAPPED_FB_LEN   0x100000

#define REMAPPED_LCD_BASE_LEN 0x200
#define REMAPPED_CFG_LEN  0x100
#define REMAPPED_MMR_LEN  0x200
#define TC6393_PHYS_ADR_LEN 0x2000000
#define MAX_XRES_QVGA		240
#define MAX_YRES_QVGA		320
#define MAX_XRES_VGA		480
#define MAX_YRES_VGA		640
#define BITS_PER_PIXEL    16

#define TC6393_SMEM_START (TOSA_LCDC_PHYS+FB_OFFSET)
#define TC6393_MMIO_START (TOSA_LCDC_PHYS+0x500)

#define COMADJ_DEFAULT 97

// Pseudo palette size
#define MAX_PALETTES      16

#define USE_ACCELERATOR
#ifdef USE_ACCELERATOR
// for Accelerator
#define FIFO_ADDR	(((int)remapped_fbuf)+(1024*1024)-(4*512))
static void tc6393_acc_init(void);
static void tc6393_acc_write( u32* cmd, int cmdnum );
static void tc6393_acc_sync(void);
#endif

// ioctls
#define TC6393FB_POWERDOWN       0x54433601 /* TC6\01 */
#ifdef USE_ACCELERATOR
#define TC6393FB_ACC_CMD_WRITE   0x54433602 /* TC6\02 */
#define TC6393FB_ACC_SYNC        0x54433603 /* TC6\03 */
#define TC6393FB_ACC_CMD_MAX_LEN   10
#endif

// General frame buffer data structures
struct tc6393fb_info {
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

struct tc6393fb_par {
  u32 xres;
  u32 yres;
  u32 xres_virtual;
  u32 yres_virtual;
  u32 bits_per_pixel;
  u32 visual;
  u32 palette_size;
};

static struct tc6393fb_info fb_info;
static struct tc6393fb_par current_par;
static int current_par_valid = 0;
static struct display disp;

static void *remapped_lcdbase;
static void *remapped_regs;
static void *remapped_inner;
static void *remapped_fbuf;

static FrameBuffer_QVGA *fb_QVGA;
static FrameBuffer_VGA *fb_VGA;

static void *remapped_scoop2_1;
static void *remapped_scoop2_2;

#define LCD_MODE_QVGA    0
#define LCD_MODE_VGA     1

// referenced by tosa_fb
int tc6393fb_lcdMode = LCD_MODE_VGA;
int tc6393fb_isblank = 0;

static int tg_ssp_reg0;

#if defined(CONFIG_ARCH_SHARP_SL)
// checking in mm/omm_kill.c
int disable_signal_to_mm = 0;
#endif

int tc6393fb_init(void);
static void tc6393fb_setlcdmode(int);
static void tc6393fb_initssp(void);
static void tc6393fb_lcdhwsetup(int);
static void tc6393_soft_reset(void);

static int tc6393_encode_var(struct fb_var_screeninfo *var,
			   const void *raw_par,
			   struct fb_info_gen *info);
static int tc6393fb_ioctl(struct inode *inode, 
			struct file *file, 
			u_int cmd,
			u_long arg,
			int con,
			struct fb_info *info);

void pxa_nssp_init(void);

int isspace(int x);
int config_open(const char *config_filename);
int config_close(int config_fd);
int config_get_dword(int config_fd,
		     const char *section,
		     const char *id,
		     u32 *val,
		     u32 defval);
long my_strtol(const char *string, char **endPtr, int base);

extern void pxa_nssp_output(unsigned char, unsigned char);

#define LINE_WAIT	20
#define PLCLN_MASK	0x03FF
static int wait_vsync(void)
{
  int ct=0;
  volatile u16 val1,val2;

  u16 line = (tc6393fb_lcdMode==LCD_MODE_VGA)?HORIZONTAL_LINE_VGA:HORIZONTAL_LINE_QVGA;
  do {
    val1 = readw(remapped_inner + PLCLN)&PLCLN_MASK;
    while(val1!=(val2=(readw(remapped_inner + PLCLN)&PLCLN_MASK))) {
      val1 = val2;
    }
    if (val1==line) break;
    if (++ct>1000*1000/LINE_WAIT) break;
    udelay(LINE_WAIT);
  } while(1);
  if (val1!=line) {
    printk("unable to wait vsync\n");
    return 1;
  }
  return 0;
}

// save & restore image
//#define STATIC_IMAGE_BUF
#ifdef STATIC_IMAGE_BUF
static u32 ImageBuf[MAX_XRES_VGA*MAX_YRES_VGA/2];
static void save_image(void)
{
  int i,size = current_par.xres*current_par.yres/2;
  u32 *src = (u32*)remapped_fbuf;
  u32 *dst = ImageBuf;

  for (i=0; i<size; i++) {
    *dst++ = *src++;
  }
}
static void load_image(void)
{
  int i,size = current_par.xres*current_par.yres/2;
  u32 *src = ImageBuf;
  u32 *dst = (u32*)remapped_fbuf;

  for (i=0; i<size; i++) {
    *dst++ = *src++;
  }
}
#else
#define SAVE_LINES	20
static u16 *gSaveImagePtr[HORIZONTAL_LINE_VGA/SAVE_LINES] = {NULL};
void clear_imagebuf()
{
  if(gSaveImagePtr[0] != NULL){
    int i;
    for (i = 0; i < HORIZONTAL_LINE_VGA/SAVE_LINES; i++) {
      if (gSaveImagePtr[i] != NULL) {
	kfree(gSaveImagePtr[i]);
	gSaveImagePtr[i]=NULL;
      }
    }
  }
}
#endif



/* ------------------- chipset specific functions -------------------------- */
//
// static void tc6393_hw_init(void)
//
static void tc6393_hw_init(void)
{
	remapped_regs = (void *)TC6393_SYS_BASE;
	remapped_lcdbase = (void *)TC6393_GC_BASE;
	remapped_inner = (void *)TC6393_LCD_INNER_ADDRESS;
	remapped_scoop2_1 = (void *)CF_BUF_CTRL_BASE;
	remapped_scoop2_2 = (void *)CF2_BUF_CTRL_BASE;
	remapped_fbuf = (void *)TC6393_FB_BASE;
	tc6393_soft_reset();
#ifdef USE_ACCELERATOR
	tc6393_acc_init();
#endif

}


//
// Fill the fix structure based on values in the par
//
static int tc6393_encode_fix(struct fb_fix_screeninfo *fix,
			    const void  *raw_par,
			    struct fb_info_gen *info)
{
  const struct tc6393fb_par *par = raw_par;

  if (!par)
    BUG();

  memset(fix, 0, sizeof *fix);

  strcpy(fix->id, TC6393_NAME);
	
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
  fix->smem_start = TC6393_SMEM_START;

  if(tc6393fb_lcdMode == LCD_MODE_VGA ){
      fix->line_length = (480 * BITS_PER_PIXEL) / 8;
      fix->smem_len = (480 * 640 * BITS_PER_PIXEL) / 8;
  } else {					// QVGA
      fix->line_length = (240 * BITS_PER_PIXEL) / 8;
      fix->smem_len = (240 * 320 * BITS_PER_PIXEL) / 8;
  }
  fix->mmio_start = TC6393_MMIO_START;
  fix->mmio_len = REMAPPED_CFG_LEN + REMAPPED_MMR_LEN; // LCD-COMMON+LCD-MMR

  fix->accel = FB_ACCEL_NONE;

  return 0;
}

//
// Get video parameters out of the par
//
static int tc6393_decode_var(const struct fb_var_screeninfo *var,
			    void *raw_par,
			    struct fb_info_gen *info)
{
  struct tc6393fb_par *par = raw_par;
  
  *par = current_par;
 
  if (!par)
    BUG();

  if((par->xres == 480 && par->yres == 640)||
     (par->xres == 240 && par->yres == 320)){
      par->xres = var->xres;
      par->yres = var->yres;
  }else{

    if(tc6393fb_lcdMode == LCD_MODE_VGA ){
	      par->xres = MAX_XRES_VGA;
    	  par->yres = MAX_YRES_VGA;
		}else{				// QVGA
	      par->xres = MAX_XRES_QVGA;
    	  par->yres = MAX_YRES_QVGA;
		}
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
static int tc6393_encode_var(struct fb_var_screeninfo *var, 
			    const void *raw_par,
			    struct fb_info_gen *info)
{
  struct tc6393fb_par *par = (struct tc6393fb_par *)raw_par;

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
    if(tc6393fb_lcdMode == LCD_MODE_VGA ){
			var->xres = MAX_XRES_VGA;
			var->yres = MAX_YRES_VGA;
		}else{					// QVGA
			var->xres = MAX_XRES_QVGA;
			var->yres = MAX_YRES_QVGA;
		}
  }

  var->xres_virtual = var->xres;
  var->yres_virtual = var->yres;
  var->xoffset = var->yoffset = 0;

  var->activate = FB_ACTIVATE_NOW;

  var->height = -1;
  var->width = -1;
  var->vmode = FB_VMODE_NONINTERLACED;

  var->sync = 0;
  var->pixclock = 0x04;

  return 0;
}

//
// Fill the par structure with relevant hardware values
//
static void tc6393_get_par(void *raw_par, struct fb_info_gen *info)
{
  struct tc6393fb_par *par = raw_par;

  if (current_par_valid)
    *par = current_par;
}

//
// Set the hardware according to the values in the par
//
static void tc6393_set_par(const void *raw_par, struct fb_info_gen *info)
{
  const struct tc6393fb_par *par = raw_par;

  current_par = *par;
  current_par_valid = 1;
}

//
// Split a color register into RGBT components. N/A on TC6393
//
static int tc6393_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			   unsigned *blue, unsigned *transp,
			   struct fb_info *info)
{
  struct tc6393fb_info *linfo = (struct tc6393fb_info *)info;  

  if (regno >= 16) return 1;

  *transp = 0;
  *red    = ((linfo->fbcon_cmap.cfb16[regno] >> 11) & 31) << 11;
  *green  = ((linfo->fbcon_cmap.cfb16[regno] >> 5 ) & 63) << 10;
  *blue   = ((linfo->fbcon_cmap.cfb16[regno])       & 31) << 11;

  return 0;
}

//
// Set a single color register. N/A on TC6393
//
static int tc6393_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
  struct tc6393fb_info *linfo = (struct tc6393fb_info *)info;

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
static int tc6393_pan_display(const struct fb_var_screeninfo *var,
			     struct fb_info_gen *info)
{

  return 0;
}

//
// Blank the display based on value in blank_mode
//
static int tc6393_blank(int blank_mode, struct fb_info_gen *info)
{
    if(!in_interrupt()){
      if (blank_mode && !tc6393fb_isblank) {
	    tc6393_pm_suspend(1);
	}else if(!blank_mode && tc6393fb_isblank){
	    tc6393_pm_resume();
	}
    }

    return 0;
}

#ifdef CONFIG_SHARP_LOGO_SCREEN
#include "vgaLogoScreen.c"
#define SHOW_WAIT_MSG
#ifdef SHOW_WAIT_MSG
#include "tosaLogoMsgJ.c"
#include "tosaLogoMsg.c"
#if 0
static int logo_lang = 0;
#else
int logo_lang = 0; // Battery Driver refers.
#endif
static int logo_msg_width __initdata = 0;
static int logo_msg_height __initdata = 0;
static unsigned short *logo_msg_data __initdata = NULL;
#ifndef MODULE
static int __init logolang_setup(char *str)
{
  logo_lang = simple_strtoul(str,NULL,0);
  return 0;
}
__setup("LOGOLANG=", logolang_setup);
#endif // MODULE
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
static int	logo_msg_yoff __initdata = 100;
#else
static int	logo_msg_yoff __initdata = 500;
#endif
#endif // end SHOW_WAIT_MSG

//
// static void __init draw_img()
//
static void __init draw_img( int sx, int sy, int w, int h, unsigned short* data )
{
	int x, y,ppl;
	unsigned short	*p;
								// LCD_MODE_VGA
	if (tc6393fb_lcdMode == LCD_MODE_VGA ){
		ppl = PIXEL_PER_LINE_VGA;
		p = &fb_VGA->pixel[sy][sx];
	}else{ 						//	QVGA
		ppl = PIXEL_PER_LINE_QVGA;
		p = &fb_QVGA->pixel[sy][sx];
	}

	for (y=0; y<h; y++) {
	  for (x=0; x<w; x++) {
	    unsigned short col = data[(h-1-y)+x*h];
#if 0 //
	    if (col==0x7bef) col=0xFFFF;
#endif
	    *p++ = col;
	  }
	  p += ppl - w;
	}
}

static void __init draw_logo_screen(void)
{
	switch(logo_lang) {
	case 1: // JP
	  logo_msg_width = logo_msg_width_jp;
	  logo_msg_height = logo_msg_height_jp;
	  logo_msg_data = logo_msg_data_jp;
	  break;
	default: // EN
	  logo_msg_width = logo_msg_width_en;
	  logo_msg_height = logo_msg_height_en;
	  logo_msg_data = logo_msg_data_en;
	  break;
	}

	if (tc6393fb_lcdMode == LCD_MODE_VGA){
		memset(remapped_fbuf,0xff,(HORIZONTAL_LINE_VGA-16)*(PIXEL_PER_LINE_VGA*2));
	}else{				// QVGA
		memset(remapped_fbuf,0xff,(HORIZONTAL_LINE_QVGA-16)*(PIXEL_PER_LINE_QVGA*2));
	}

	draw_img((current_par.xres - logo_screen_height) / 2,
		 (current_par.yres - logo_screen_width) / 2,
		 logo_screen_height, logo_screen_width, logo_screen_data);


#ifdef SHOW_WAIT_MSG
	draw_img((current_par.xres - logo_msg_height) / 2,logo_msg_yoff,
		 logo_msg_height,logo_msg_width,logo_msg_data);
#endif

}
#endif //CONFIG_SHARP_LOGO_SCREEN


//
// Set up the display for the fb subsystem
//
static void tc6393_set_disp(const void *unused, struct display *disp,
			   struct fb_info_gen *info)
{
	struct tc6393fb_info *linfo = (struct tc6393fb_info *)info;
	int newLcdMode = -1;

	if (current_par.xres == PIXEL_PER_LINE_QVGA && current_par.yres == HORIZONTAL_LINE_QVGA){
	  newLcdMode = LCD_MODE_QVGA;
	} else if (current_par.xres == PIXEL_PER_LINE_VGA && current_par.yres == HORIZONTAL_LINE_VGA){
	  newLcdMode = LCD_MODE_VGA;
	} else { // unknown
	  return;
	}

	if (newLcdMode!=tc6393fb_lcdMode) {
#if 0
	  printk("change resolution from %s to %s\n", tc6393fb_lcdMode?"VGA":"QVGA", newLcdMode?"VGA":"QVGA");
#endif
	  if (tc6393fb_isblank) {
	    printk("force resume from blank!!!\n");
	    clear_imagebuf();
	    tc6393_pm_resume();
	  }

	  wait_vsync();
	  tc6393_clear_screen(LCD_MODE_VGA);
	  wait_vsync();
	  tc6393fb_setlcdmode(newLcdMode);
	}
	
  // Do the rest of the display initialization
  disp->screen_base = remapped_fbuf;

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


}

/* ------------ Interfaces to hardware functions ------------ */

struct fbgen_hwswitch tc6393_switch =
{
  //X  detect:	tc6393_detect,
  encode_fix:	tc6393_encode_fix,
  decode_var:	tc6393_decode_var,
  encode_var:	tc6393_encode_var,
  get_par:	tc6393_get_par,
  set_par:	tc6393_set_par,
  getcolreg:	tc6393_getcolreg,
  setcolreg:	tc6393_setcolreg,
  pan_display:	tc6393_pan_display,
  blank:	tc6393_blank,
  set_disp:	tc6393_set_disp,
};

/* ------------ Hardware Independent Functions ------------ */


static struct fb_ops tc6393fb_ops =
{
  owner:	  THIS_MODULE,
  fb_get_fix:	  fbgen_get_fix,
  fb_get_var:	  fbgen_get_var,
  fb_set_var:	  fbgen_set_var,
  fb_get_cmap:	  fbgen_get_cmap,
  fb_set_cmap:	  fbgen_set_cmap,
  fb_pan_display: fbgen_pan_display,
  fb_ioctl:       tc6393fb_ioctl,
};

//
int __init tc6393fb_init(void)
{
#ifndef STATIC_IMAGE_BUF
  memset(gSaveImagePtr,0,sizeof(gSaveImagePtr));
#endif

  fb_info.gen.fbhw = &tc6393_switch;	// generic frame buffer device
  tc6393_hw_init();
  tc6393_clear_screen(LCD_MODE_VGA);
  tc6393fb_setlcdmode(tc6393fb_lcdMode);

  strcpy(fb_info.gen.info.modename, TC6393_NAME);
  fb_info.gen.info.changevar = NULL;
  fb_info.gen.info.node = -1;
  fb_info.gen.info.fbops = &tc6393fb_ops;
  fb_info.gen.info.disp = &disp;
  fb_info.gen.parsize = sizeof(struct tc6393fb_par);
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
  tc6393_pm_dev = pm_register(PM_COTULLA_DEV, 0, tc6393_pm_callback);
#endif //CONFIG_PM

  printk("fb%d: %s frame buffer device\n", 
	 GET_FB_IDX(fb_info.gen.info.node),
	 fb_info.gen.info.modename);


#if defined(CONFIG_SHARP_LOGO_SCREEN)
	draw_logo_screen();
	tosa_bl_temporary_contrast_set();
	tosa_bl_temporary_contrast_reset();
#endif

  return 0;
}

void tc6393fb_cleanup(struct fb_info *info)
{
	unregister_framebuffer(info);
#ifndef STATIC_IMAGE_BUF
	if(gSaveImagePtr[0] != NULL){
		int i;
		for (i = 0; i < HORIZONTAL_LINE_VGA/SAVE_LINES; i++) {
			if (gSaveImagePtr[i] != NULL) {
				kfree(gSaveImagePtr[i]);
				gSaveImagePtr[i]=NULL;
			}
		}
	}
#endif
}

#ifdef MODULE
int __init init_module(void)
{
  return tc6393fb_init();
}

void cleanup_module(void)
{
  tc6393fb_cleanup(&fb_info.gen.info);
}
#endif

static int tc6393fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
		       u_long arg, int con, struct fb_info *info2)
{
  switch (cmd){
  case TC6393FB_POWERDOWN:
    {
      int blank_mode = (int)arg;
      tc6393_blank(blank_mode,NULL);
    }
    break;
#ifdef USE_ACCELERATOR
  case TC6393FB_ACC_CMD_WRITE:
    {
      u32 cmdbuf[TC6393FB_ACC_CMD_MAX_LEN + 1];

      if (tc6393fb_isblank) {
	printk("force resume from blank!\n");
	clear_imagebuf();
	tc6393_pm_resume();
      }
      if (copy_from_user(cmdbuf, (void *)arg, 
			 (TC6393FB_ACC_CMD_MAX_LEN + 1) * sizeof(u32)))
	return -EFAULT;

      if (cmdbuf[0] == 0)
	return 0;

      if (cmdbuf[0] > TC6393FB_ACC_CMD_MAX_LEN)
	return -EFAULT;

      tc6393_acc_write( &cmdbuf[1], cmdbuf[0] );
    }
    break;
  case TC6393FB_ACC_SYNC:
    if (tc6393fb_isblank) {
      printk("force resume from blank!!\n");
      clear_imagebuf();
      tc6393_pm_resume();
    }
    tc6393_acc_sync();
    break;
#endif // end USE_ACCELERATOR
  default:
    return -EINVAL;
  }
  return 0;
}

//
// Constants
//

static void tc6393fb_initssp( void)
{
	pxa_nssp_init();
	tg_ssp_reg0 = TG_REG0_COLOR | TG_REG0_UD | TG_REG0_LR;
	pxa_nssp_output(1,0x00); //delaied 0clk TCTL signal for VGA
	pxa_nssp_output(3,0x02); // GPOS0=powercontrol, GPOS1=GPIO, GPOS2=TCTL
}

static void tc6393fb_setpnlctl(int dat)
{
	pxa_nssp_output(6,dat);
}

static void tc6393fb_lcdhwsetup(int mode)
{
	int i;
	
	// define the framebuffer base.
	fb_VGA = (FrameBuffer_VGA *) (remapped_fbuf);
	fb_QVGA = (FrameBuffer_QVGA *) (remapped_fbuf);

	if ( mode == LCD_MODE_QVGA ) { // change to QVGA
		writew((u16)(0x0000), remapped_inner + PLGMD);
		writew((u16)(readw(remapped_regs + TC6393_SYS_FER) & ~FUNC_SLCDEN),
		       remapped_regs + TC6393_SYS_FER);
		writew((u16)(PLCNT_DEFAULT), remapped_inner + PLCNT);
		writew((u16)(PLL10_QVGA), remapped_regs + TC6393_SYS_PLL1CR1);
		writew((u16)(PLL11_QVGA), remapped_regs + TC6393_SYS_PLL1CR2);
		writew((u16)(readw(remapped_regs + TC6393_SYS_FER) | FUNC_SLCDEN),
		       remapped_regs + TC6393_SYS_FER);

		for( i=0; i<kurohyo_LCDCREG_NUM-1; i++){
			writew((u16)(kurohyoLcdcQVGA[i].Value), remapped_inner + kurohyoLcdcQVGA[i].Index);
		}
	} else {	// change to VGA
		writew((u16)(0x0000), remapped_inner + PLGMD);
		writew((u16)(readw(remapped_regs + TC6393_SYS_FER) & ~FUNC_SLCDEN),
		       remapped_regs + TC6393_SYS_FER);
		writew((u16)(PLCNT_DEFAULT), remapped_inner + PLCNT);
		writew((u16)(PLL10_VGA), remapped_regs + TC6393_SYS_PLL1CR1);
		writew((u16)(PLL11_VGA), remapped_regs + TC6393_SYS_PLL1CR2);
		writew((u16)(readw(remapped_regs + TC6393_SYS_FER) | FUNC_SLCDEN),
		       remapped_regs + TC6393_SYS_FER);

		for( i=0; i<kurohyo_LCDCREG_NUM-1; i++){
			writew((u16)(kurohyoLcdcVGA[i].Value), remapped_inner + kurohyoLcdcVGA[i].Index);
		}
	}

	writew((u16)(PLCNT_DEFAULT), remapped_inner + PLCNT);
	mdelay(5);
	writew((u16)(PLCNT_DEFAULT | PLCNT_STOP_CKP), remapped_inner+PLCNT);
	mdelay(5);
	writew((u16)(PLCNT_DEFAULT | PLCNT_STOP_CKP | PLCNT_SOFT_RESET), remapped_inner + PLCNT);
	writew((u16)(0xfffa), remapped_inner + PIFEN);

	// TG LCD pannel power up
	tc6393fb_setpnlctl(0x4);
	mdelay(50);

	// TG LCD GVSS
	tc6393fb_setpnlctl(0x0);

	{
	  int comadj;
	  /* Set Common Voltage */
	  comadj = -1;
	  comadj = sharpsl_get_comadj();
	  if ( comadj < 0 ) {
	    comadj = COMADJ_DEFAULT;
	  }
	  mdelay(50);
	  i2c_init(1);
	  tosa_set_common_voltage( comadj );
	}

}

static void tc6393fb_setlcdmode(int mode)
{
	if( mode == LCD_MODE_VGA ){
		tg_ssp_reg0 |= TG_REG0_VQV;
		pxa_nssp_output(0, tg_ssp_reg0);	// data send to TG
		tc6393fb_lcdhwsetup(LCD_MODE_VGA);
		tc6393fb_lcdMode = LCD_MODE_VGA;
	}else{
		tg_ssp_reg0 &= ~TG_REG0_VQV;
		pxa_nssp_output(0, tg_ssp_reg0);	// data send to TG
		tc6393fb_lcdhwsetup(LCD_MODE_QVGA);
		tc6393fb_lcdMode = LCD_MODE_QVGA;
	}
}

static void tc6393_soft_reset()
{
	int i;
	PSPR=0;// Clear Core Clock Change status

	// L3V On
	writew((u16)(readw(remapped_scoop2_2 + SCP_GPWR) | SCOOP22_L3VON),remapped_scoop2_2 + SCP_GPWR);

	// TG On
	writew((u16)(readw(remapped_regs +  TC6393_SYS_GPODSR1) & ~0x0001),remapped_regs +  TC6393_SYS_GPODSR1);

	// Clock Control Register
	writew((u16)((readw(remapped_regs + TC6393_SYS_CCR) & CLKCTL_USB_MASK) | CLKCTL_CONFIG_DTA),
							 remapped_regs + TC6393_SYS_CCR);

	writew((u16)(0x0cc1), remapped_regs + TC6393_SYS_PLL2CR);

	if ( tc6393fb_lcdMode == LCD_MODE_VGA ) { // VGA
		writew((u16)(PLL10_VGA), remapped_regs + TC6393_SYS_PLL1CR1);
		writew((u16)(PLL11_VGA), remapped_regs + TC6393_SYS_PLL1CR2);
	} else { // QVGA
		writew((u16)(PLL10_QVGA), remapped_regs + TC6393_SYS_PLL1CR1);
		writew((u16)(PLL11_QVGA), remapped_regs + TC6393_SYS_PLL1CR2);
	}
	writew((u16)(0x003a), remapped_lcdbase + KH_CLKEN);
	writew((u16)(0x003a), remapped_lcdbase + KH_GCLKEN);
	writew((u16)(0x3f00), remapped_lcdbase + KH_PSWCLR);
	writew((u16)(readw(remapped_regs + TC6393_SYS_FER) | FUNC_SLCDEN), remapped_regs + TC6393_SYS_FER);
	mdelay(2);

	writew((u16)(0x0000), remapped_lcdbase + KH_PSWCLR);
	writew((u16)(0x3300), remapped_regs + TC6393_SYS_GPER);
	
	// initialize kurohyo configration register
	for( i=0; i <= KH_CMDREG_NUM; i++){
		writew((u16)(kh_cmd[i].Value), remapped_lcdbase + kh_cmd[i].Index);
	}
	mdelay(2);
	writew((u16)(0x000b), remapped_lcdbase + KH_VRAMBC);

	tc6393fb_initssp();
	tc6393fb_isblank = 0;
}

#ifdef USE_ACCELERATOR
static void tc6393_acc_init()
{
  writew((u16)((FIFO_ADDR>>16)&0x001f), remapped_inner + KH_CMDADR_H);
  writew((u16)(FIFO_ADDR&0x0fff8), remapped_inner + KH_CMDADR_L);
  writew(512-1, remapped_inner + KH_CMDFIF);
  
  writew(1, remapped_inner + KH_FIFOR);

  writew(0, remapped_inner + KH_BINTMSK);
  writew(0, remapped_inner + KH_CMDFINT);
}

static void tc6393_acc_write( u32* cmd, int cmdnum )
{
  volatile int i;
  int ct=0;

  if (readw(remapped_inner + KH_FIPT)>400) {
    while(readw(remapped_inner + KH_FIPT)>200) {
      mdelay(1);
      if (++ct>1000) {
	printk(__FUNCTION__ ": timeout\n");
	return; // timeout
      }
    }
  }
  
  for ( i = 0; i < cmdnum; i ++ ) {
    writew((u16)((cmd[i]>>16) & 0x0ffff), remapped_inner + KH_COMD_H);
    writew((u16)(cmd[i] & 0x0ffff), remapped_inner + KH_COMD_L);
  }
}

static void tc6393_acc_sync()
{
  int ct=0;
  while(readw(remapped_inner + KH_FIPT)>0) {
    mdelay(1);
    if (++ct>1000) {
      printk(__FUNCTION__ ": timeout\n");
      return; // timeout
    }
  }
  ct=0;
  while(readw(remapped_inner + KH_DMAST)&PXAIO_DMAST_BLT) {
    mdelay(1);
    if (++ct>1000) {
      printk(__FUNCTION__ ": timeout\n");
      return; // timeout
    }
  }
}
#endif // end USE_ACCELERATOR

static void tc6393_clear_screen(u32 mode)
{
#ifndef USE_ACCELERATOR
  if(mode == LCD_MODE_VGA){
    memset(remapped_fbuf,0xff,MAX_XRES_VGA*MAX_YRES_VGA*2);
  }else if(mode == LCD_MODE_QVGA){
    memset(remapped_fbuf,0xff,MAX_XRES_QVGA*MAX_YRES_QVGA*2);
  }
#else // Accelerator always clear VGA area
  static u32 cmd[6];
  u16 xres = (tc6393fb_lcdMode == LCD_MODE_VGA)?MAX_XRES_VGA:MAX_XRES_QVGA;
  u16 yres = (tc6393fb_lcdMode == LCD_MODE_VGA)?MAX_YRES_VGA:MAX_YRES_QVGA;

  cmd[0] = PXAIO_COMDI_DSADR|PXAIO_COMDD_DSADR(((int)remapped_fbuf));
  cmd[1] = PXAIO_COMDI_DHPIX|PXAIO_COMDD_DHPIX(xres - 1);
  cmd[2] = PXAIO_COMDI_DVPIX|PXAIO_COMDD_DVPIX(yres - 1);
  cmd[3] = PXAIO_COMDI_FILL|PXAIO_COMDD_FILL(0xffff);
  cmd[4] = PXAIO_COMDI_FLGO;

  tc6393_acc_write(cmd,5);
  tc6393_acc_sync();

#endif // end USE_ACCELERATOR
}

static DECLARE_WAIT_QUEUE_HEAD(blank_queue);

static void tc6393_pm_suspend(int suspend_mode)
{
#ifndef STATIC_IMAGE_BUF
    int i,j;
    u32 *pVram = (u32*)remapped_fbuf;
#endif

#ifdef USE_ACCELERATOR
      tc6393_acc_sync();
#endif

    if (!tc6393fb_isblank) {
#ifdef STATIC_IMAGE_BUF
      save_image();
#else
      for (i = 0; i < current_par.yres/SAVE_LINES; i++) {
	if (gSaveImagePtr[i] != NULL){
	  kfree(gSaveImagePtr[i]);
	  gSaveImagePtr[i] = NULL;
	}
	gSaveImagePtr[i] = kmalloc(current_par.xres * BITS_PER_PIXEL*SAVE_LINES / 8, GFP_KERNEL);
	if (gSaveImagePtr[i] != NULL){
	  memcpy(gSaveImagePtr[i],pVram,current_par.xres*SAVE_LINES*2);
	  pVram += current_par.xres*SAVE_LINES/2;
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
      for (; i < HORIZONTAL_LINE_VGA/SAVE_LINES; i++) {
	gSaveImagePtr[i] = NULL;
      }
#endif

#ifdef CONFIG_PM
      tosa_bl_pm_callback(NULL, PM_SUSPEND, NULL);
#endif

      tc6393fb_isblank = 1;

      wait_vsync();
      tc6393_clear_screen(LCD_MODE_VGA);
      wait_vsync();
      wait_vsync();

      // TG LCD VHSA off
      tc6393fb_setpnlctl(0x4);
      if (suspend_mode) interruptible_sleep_on_timeout((wait_queue_head_t*)&blank_queue, 50/2 );
      else mdelay(50);
      
      // TG LCD signal off
      tc6393fb_setpnlctl(0x6);
      wait_vsync();
      if (suspend_mode) interruptible_sleep_on_timeout((wait_queue_head_t*)&blank_queue, 50/2 );
      else mdelay(50);

      // TG Off
      writew((u16)(readw(remapped_regs +  TC6393_SYS_GPODSR1) | 0x0001),remapped_regs +  TC6393_SYS_GPODSR1);
      if (suspend_mode) interruptible_sleep_on_timeout((wait_queue_head_t*)&blank_queue, 120/2 );
      else mdelay(120);

      // CLK: Stop
      writew((u16)(0x0), remapped_lcdbase + KH_CLKEN);
      if (suspend_mode) interruptible_sleep_on_timeout((wait_queue_head_t*)&blank_queue, 100/2 );
      else mdelay(100);

      // L3V Off
      writew((u16)(readw(remapped_scoop2_2 + SCP_GPWR) & ~SCOOP22_L3VON),remapped_scoop2_2 + SCP_GPWR);

//X      tc6393fb_isblank = 1;
    }
}

static void tc6393_pm_resume()
{
#ifndef STATIC_IMAGE_BUF
    int i;
    u32 *pVram = (u32*)remapped_fbuf;
#endif

    tc6393_hw_init();

#ifdef USE_ACCELERATOR
  // erase whole VRAM
  if (tc6393fb_lcdMode == LCD_MODE_QVGA) {
    memset(remapped_fbuf,0xff,MAX_XRES_VGA*MAX_YRES_VGA*2);
  }
#endif

#ifdef STATIC_IMAGE_BUF
    load_image();
#else
    if (gSaveImagePtr[0] != NULL){
		for (i = 0; i < (current_par.yres)/SAVE_LINES; i++) {
			if (gSaveImagePtr[i] == NULL) {
				printk("can't find pre-off image buffer %d\n", i);
				continue;
			}
			memcpy(pVram,gSaveImagePtr[i],current_par.xres*SAVE_LINES*2);
			pVram += current_par.xres*SAVE_LINES/2;
			kfree(gSaveImagePtr[i]);
			gSaveImagePtr[i] = NULL;
		}
    }
#endif

    tc6393fb_setlcdmode(tc6393fb_lcdMode);

#ifdef CONFIG_PM
    tosa_bl_pm_callback(NULL, PM_RESUME, NULL);
#endif

}

#ifdef CONFIG_PM
static int tc6393_pm_callback(struct pm_dev* pm_dev,
			    pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
	  tc6393_pm_suspend(0);
	  break;
		
	case PM_RESUME:
	  tc6393_pm_resume();
	  break;
	}
	return 0;
}

void tc6393_fatal_off(void)
{
  tc6393_pm_suspend(0);
}
#endif //CONFIG_PM

// EOF
