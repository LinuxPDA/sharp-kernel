/*
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

#include "w100fb.h"
#include <linux/pm.h>

//#define _IMAGE_CACHE_SUPPORT // for image cache on video memory

#ifdef CONFIG_PM
#include <asm/arch/sharpsl_param.h>
static struct pm_dev *w100_pm_dev;
static int w100_pm_callback(struct pm_dev* pm_dev,pm_request_t req, void* data);
#endif //CONFIG_PM

static void w100_pm_suspend(int suspend_mode);
static void w100_pm_resume(int resume_mode);

#define W100_SUSPEND_EXTMEM 0
#define W100_SUSPEND_ALL    1 

static void w100_resume(void);
static void w100_suspend(u32 mode);
static void w100_init_qvga_rotation(u16 deg);
static void w100_soft_reset(void);
static void w100_clear_screen(u32 mode,void *fbuf);
static void w100_vsync(void);

static void lcdtg_hw_init(u32 mode);
static void lcdtg_lcd_change(u32 mode);
static void lcdtg_resume(void);
static void lcdtg_suspend(void);
static void lcdtg_i2c_start_sequence(u8 base_data);
static void lcdtg_i2c_stop_sequence(u8 base_data);
static void lcdtg_i2c_wait_ack(u8 base_data);

#define USE_XTAL_12_5     1    // if not defined, it is assumed to be 14.3MHz

// Some hardware related constants
// The name the kernel will know us by
#define W100_NAME         "W100FB"

// Physical addresses, offsets & lengths
#define REG_OFFSET        0x10000
#define FB_OFFSET         MEM_EXT_BASE_VALUE
#define W100_PHYS_ADDRESS 0x08000000
#define W100_REG_BASE     (W100_PHYS_ADDRESS+REG_OFFSET)
#define W100_FB_BASE      (W100_PHYS_ADDRESS+MEM_EXT_BASE_VALUE)
#ifdef _IMAGE_CACHE_SUPPORT
#define REMAPPED_FB_LEN   0x200000
#else //_IMAGE_CACHE_SUPPORT
#define REMAPPED_FB_LEN   0x15ffff
#endif //_IMAGE_CACHE_SUPPORT
#define REMAPPED_CFG_LEN  0x10
#define REMAPPED_MMR_LEN  0x2000
#define W100_PHYS_ADR_LEN 0x1000000
#define MAX_XRES          480
#define MAX_YRES          640
#define BITS_PER_PIXEL    16

// Pseudo palette size
#define MAX_PALETTES      16

// ioctls
#define W100FB_CONFIG          0x57415200 /* WAL\00 */
#define W100INIT_ITEM          0
#define W100INIT_ALL           1
#define W100INIT_ITEM_WITH_VAL 2

#define W100FB_POWERDOWN       0x57415201 /* WAL\01 */
#define W100FB_CONFIG_EX       0x57415202 /* WAL\02 */

// General frame buffer data structures
struct w100fb_info {
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

struct w100fb_par {
  u32 xres;
  u32 yres;
  u32 xres_virtual;
  u32 yres_virtual;

  u32 bits_per_pixel;

  u32 visual;
  u32 palette_size;
};

static struct w100fb_info fb_info;
static struct w100fb_par current_par;
static int current_par_valid = 0;
static struct display disp;
static struct fb_var_screeninfo default_var;

typedef struct _W100CONFIG_ARGS_
{
  int  arg_count;
  char config_filename[256];
  char section_name[64];
  char item_name[64];
  unsigned int supplied_value;
} W100CONFIG_ARGS;


// Remapped addresses for base cfg, memmapped regs and the frame buffer itself
static void *remapped_base;
static void *remapped_regs;
static void *remapped_fbuf;

static int isRemapped = 0;

// for resolution change
#define LCD_MODE_480    0
#define LCD_MODE_320    1
#define LCD_MODE_240    2
#define LCD_MODE_UNKOWN (-1)

int w100fb_lcdMode = LCD_MODE_UNKOWN; //default UNKOWN

static u16 *gSaveImagePtr[640] = {NULL};
#define SAVE_IMAGE_MAX_SIZE ((640 * 480 * BITS_PER_PIXEL) / 8)

#ifdef _IMAGE_CACHE_SUPPORT // for image cache on video memory

#define IMG_CACHE_MALLOC_SIZE 0x1000
#define IMG_CACHE_OFFSET_VGA (0x97008)
#define IMG_CACHE_TOTAL_SIZE_VGA (0x200000-0x4000-IMG_CACHE_OFFSET_VGA)
#define IMG_CACHE_SKIP_MARK (0xffffffff)

static u32 *save_img_cache_ptr=NULL;
static u32 save_img_alloc_num=0;
#define __PRINTK(arg...) //printk(arg)
#undef __SUM //for debug

static u32* save_image_cache(u32 *alloc_num);
static int restore_image_cache(u32 *img_src,u32 alloc_num);
static int cleanup_image_cache(u32 *img_src,u32 alloc_num);

// defalut don't skip
static int start_skip_save_image_no = (-1);
static int end_skip_save_image_no = (-1);
#endif //_IMAGE_CACHE_SUPPORT

int w100fb_isblank = 0;

static int fb_blank_normal = 0;
static int isSuspended_tg_only = 0;

#if defined(CONFIG_ARCH_SHARP_SL)
// checking in mm/omm_kill.c
int disable_signal_to_mm = 0;
#endif

int w100fb_init(void);
int w100fb_setup(char*);
static int w100_encode_var(struct fb_var_screeninfo *var,
			   const void *raw_par,
			   struct fb_info_gen *info);
static int w100fb_ioctl(struct inode *inode, 
			struct file *file, 
			u_int cmd,
			u_long arg,
			int con,
			struct fb_info *info);
void w100_init_from_hwtab(int mode, int fd, char *section, char *item, u32 val);

int isspace(int x);
int config_open(const char *config_filename);
int config_close(int config_fd);
int config_get_dword(int config_fd,
		     const char *section,
		     const char *id,
		     u32 *val,
		     u32 defval);
long my_strtol(const char *string, char **endPtr, int base);


#define LCD_SHARP_QVGA 0
#define LCD_SHARP_VGA  1
static void w100_init_sharp_lcd(u32 mode);
static void w100_PwmSetup(void);
static void w100_InitExtMem(u32 mode);

/* ------------------- chipset specific functions -------------------------- */

//
// Initialize gamma to the middle of the contrast range. Even though
// the ATI core lib eventually sets the gamma, this will allow things
// like X to work properly
//
void w100_gamma_init(void)
{
  video_ctrl_u   video_ctrl;
  gamma_slope_u  gamma_slope;
  gamma_value1_u gamma_value1;
  gamma_value2_u gamma_value2;

#if 1  // use default value
	return;
#endif

  // For RGB gamma correction
  gamma_slope.f.slope1  = 0;
  gamma_slope.f.slope2  = 0;
  gamma_slope.f.slope3  = 0;
  gamma_slope.f.slope4  = 0;
  gamma_slope.f.slope5  = 0;
  gamma_slope.f.slope6  = 0;
  gamma_slope.f.slope7  = 0;
  gamma_slope.f.slope8  = 0;
  gamma_value1.f.gamma1 = 0;
  gamma_value1.f.gamma2 = 41;
  gamma_value1.f.gamma3 = 82;
  gamma_value1.f.gamma4 = 123;
  gamma_value2.f.gamma5 = 156;
  gamma_value2.f.gamma6 = 181;
  gamma_value2.f.gamma7 = 206;
  gamma_value2.f.gamma8 = 231;

  // Correct graphic & video on RGB
  video_ctrl.f.rgb_gamma_sel = 3;

  // For Y gamma correction
  video_ctrl.f.gamma_sel = 0;

  writel((u32)(gamma_value1.val), remapped_regs+mmGAMMA_VALUE1);
  writel((u32)(gamma_value2.val), remapped_regs+mmGAMMA_VALUE2);
  writel((u32)(gamma_slope.val), remapped_regs+mmGAMMA_SLOPE);
  writel((u32)(video_ctrl.val), remapped_regs+mmVIDEO_CTRL);
}

//
// Initialization of critical w100 hardware
//
static void w100_hw_init(void)
{
  u32                   temp32;
  cif_cntl_u            cif_cntl;
  intf_cntl_u		intf_cntl;
  cfgreg_base_u         cfgreg_base;
  wrap_top_dir_u        wrap_top_dir;
  cif_read_dbg_u	cif_read_dbg;
  cpu_defaults_u	cpu_default;
  cif_write_dbg_u       cif_write_dbg;
  wrap_start_dir_u      wrap_start_dir;
  mc_ext_mem_location_u mc_ext_mem_loc;
  cif_io_u              cif_io;

  if(isRemapped == 0){
      // Request the memory region we want
      request_mem_region(W100_PHYS_ADDRESS, W100_PHYS_ADR_LEN, "W100_BASE"); 

      // remap the areas we're going to use
      remapped_base = ioremap_nocache(W100_PHYS_ADDRESS, REMAPPED_CFG_LEN);
      remapped_regs = ioremap_nocache(W100_REG_BASE, REMAPPED_MMR_LEN);
      remapped_fbuf = ioremap_nocache(W100_FB_BASE, REMAPPED_FB_LEN);

      isRemapped = 1;
  }

  w100_soft_reset();

  // This is what the fpga_init code does on reset. May be wrong
  // but there is little info available
  writel(0x31, remapped_regs+mmSCRATCH_UMSK);
  for (temp32 = 0; temp32 < 10000; temp32++)
    readl(remapped_regs+mmSCRATCH_UMSK);
  writel(0x30, remapped_regs+mmSCRATCH_UMSK);

  // Set up CIF
  cif_io.val = defCIF_IO;
  writel((u32)(cif_io.val), remapped_regs+mmCIF_IO);

  cif_write_dbg.val = readl(remapped_regs+mmCIF_WRITE_DBG);
  cif_write_dbg.f.dis_packer_ful_during_rbbm_timeout = 0;
  cif_write_dbg.f.en_dword_split_to_rbbm = 1;
  cif_write_dbg.f.dis_timeout_during_rbbm = 1;
  writel((u32)(cif_write_dbg.val), remapped_regs+mmCIF_WRITE_DBG);

  cif_read_dbg.val = readl(remapped_regs+mmCIF_READ_DBG);
  cif_read_dbg.f.dis_rd_same_byte_to_trig_fetch = 1;
  writel((u32)(cif_read_dbg.val), remapped_regs+mmCIF_READ_DBG);

  cif_cntl.val = readl(remapped_regs+mmCIF_CNTL);
  cif_cntl.f.dis_system_bits = 1;
  cif_cntl.f.dis_mr = 1;
  cif_cntl.f.en_wait_to_compensate_dq_prop_dly = 0;
  cif_cntl.f.intb_oe = 1;
  cif_cntl.f.interrupt_active_high = 1;
  writel((u32)(cif_cntl.val), remapped_regs+mmCIF_CNTL);

  // Setup cfgINTF_CNTL and cfgCPU defaults
  intf_cntl.val = defINTF_CNTL;
  intf_cntl.f.ad_inc_a = 1;
  intf_cntl.f.ad_inc_b = 1;
  intf_cntl.f.rd_data_rdy_a = 0;
  intf_cntl.f.rd_data_rdy_b = 0;
  writeb((u8)(intf_cntl.val), remapped_base+cfgINTF_CNTL);

  cpu_default.val = defCPU_DEFAULTS;
  cpu_default.f.access_ind_addr_a = 1;
  cpu_default.f.access_ind_addr_b = 1;
  cpu_default.f.access_scratch_reg = 1;
  cpu_default.f.transition_size = 0;
  writeb((u8)(cpu_default.val), remapped_base+cfgCPU_DEFAULTS);

  // set up the apertures
  writeb((u8)(REG_BASE_VALUE >> 16), remapped_base+cfgREG_BASE);

  cfgreg_base.val = defCFGREG_BASE;
  cfgreg_base.f.cfgreg_base = CFG_BASE_VALUE;
  writel((u32)(cfgreg_base.val), remapped_regs+mmCFGREG_BASE);

  // This location is relative to internal w100 addresses
  writel(0x15FF1000, remapped_regs+mmMC_FB_LOCATION);

  mc_ext_mem_loc.val = defMC_EXT_MEM_LOCATION;
  mc_ext_mem_loc.f.mc_ext_mem_start = MEM_EXT_BASE_VALUE>>8;
  mc_ext_mem_loc.f.mc_ext_mem_top = MEM_EXT_TOP_VALUE>>8;
  writel((u32)(mc_ext_mem_loc.val), remapped_regs+mmMC_EXT_MEM_LOCATION);

  if (w100fb_lcdMode == LCD_MODE_UNKOWN){
      w100_InitExtMem(LCD_SHARP_VGA);
  } else if(w100fb_lcdMode == LCD_MODE_480) {
      w100_InitExtMem(LCD_SHARP_VGA);
  } else {
      w100_InitExtMem(LCD_SHARP_QVGA);
  }

  wrap_start_dir.val = defWRAP_START_DIR;
  wrap_start_dir.f.start_addr = WRAP_BUF_BASE_VALUE>>1;
  writel((u32)(wrap_start_dir.val), remapped_regs+mmWRAP_START_DIR);

  wrap_top_dir.val = defWRAP_TOP_DIR;
  wrap_top_dir.f.top_addr = WRAP_BUF_TOP_VALUE>>1;
  writel((u32)(wrap_top_dir.val), remapped_regs+mmWRAP_TOP_DIR);

  writel((u32)0x2440, remapped_regs+mmRBBM_CNTL);
}

//
// W100 detection and initialization
//
static void w100_detect(void)
{

  w100_hw_init();
  w100_PwmSetup();

  w100_encode_var(&default_var, NULL, NULL);
}

//
// Fill the fix structure based on values in the par
//
static int w100_encode_fix(struct fb_fix_screeninfo *fix,
			    const void  *raw_par,
			    struct fb_info_gen *info)
{
  const struct w100fb_par *par = raw_par;

  if (!par)
    BUG();

  memset(fix, 0, sizeof *fix);

  strcpy(fix->id, W100_NAME);
	
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

  fix->smem_start = W100_FB_BASE;

  if(w100fb_lcdMode == LCD_MODE_UNKOWN || w100fb_lcdMode == LCD_MODE_480){

      fix->line_length = (480 * BITS_PER_PIXEL) / 8;
      fix->smem_len = 0x200000;

  } else if(w100fb_lcdMode == LCD_MODE_320){

      fix->line_length = (320 * BITS_PER_PIXEL) / 8;
      fix->smem_len = 0x60000;

  } else if(w100fb_lcdMode == LCD_MODE_240){

      fix->line_length = (240 * BITS_PER_PIXEL) / 8;
      fix->smem_len = 0x60000;

  }

  fix->mmio_start = W100_REG_BASE;
  fix->mmio_len = REMAPPED_MMR_LEN;

  fix->accel = FB_ACCEL_NONE;

  return 0;
}

//
// Get video parameters out of the par
//
static int w100_decode_var(const struct fb_var_screeninfo *var,
			    void *raw_par,
			    struct fb_info_gen *info)
{
  struct w100fb_par *par = raw_par;
  
  *par = current_par;
 
  if (!par)
    BUG();

  if((par->xres == 480 && par->yres == 640)||
     (par->xres == 320 && par->yres == 240)||
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
static int w100_encode_var(struct fb_var_screeninfo *var, 
			    const void *raw_par,
			    struct fb_info_gen *info)
{
  struct w100fb_par *par = (struct w100fb_par *)raw_par;

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
     (par->xres == 320 && par->yres == 240)||
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
static void w100_get_par(void *raw_par, struct fb_info_gen *info)
{
  struct w100fb_par *par = raw_par;

  if (current_par_valid)
    *par = current_par;
}

//
// Set the hardware according to the values in the par
//
static void w100_set_par(const void *raw_par, struct fb_info_gen *info)
{
  const struct w100fb_par *par = raw_par;

  current_par = *par;
  current_par_valid = 1;
}

//
// Split a color register into RGBT components. N/A on W100
//
static int w100_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			   unsigned *blue, unsigned *transp,
			   struct fb_info *info)
{
  struct w100fb_info *linfo = (struct w100fb_info *)info;  

  if (regno >= 16) return 1;

  *transp = 0;
  *red    = ((linfo->fbcon_cmap.cfb16[regno] >> 11) & 31) << 11;
  *green  = ((linfo->fbcon_cmap.cfb16[regno] >> 5 ) & 63) << 10;
  *blue   = ((linfo->fbcon_cmap.cfb16[regno])       & 31) << 11;

  return 0;
}

//
// Set a single color register. N/A on W100
//
static int w100_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
  struct w100fb_info *linfo = (struct w100fb_info *)info;

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
static int w100_pan_display(const struct fb_var_screeninfo *var,
			     struct fb_info_gen *info)
{

  return 0;
}

//
// Blank the display based on value in blank_mode
//
static int w100_blank(int blank_mode, struct fb_info_gen *info)
{
    if(!w100fb_isblank && !in_interrupt()){
	if(blank_mode && fb_blank_normal == 0){
	    w100_pm_suspend( 1 );
	    fb_blank_normal = blank_mode;
	}else if(!blank_mode && fb_blank_normal != 0){
	    w100_pm_resume( 1 );
	    fb_blank_normal = blank_mode;
	}
    }

    return 0;
}

#ifdef CONFIG_SHARP_LOGO_SCREEN
#include "vgaLogoScreen.c"
#define SHOW_WAIT_MSG
#ifdef SHOW_WAIT_MSG

#ifdef CONFIG_ARCH_PXA_SHEPHERD

// Shepherd
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

#else // CONFIG_ARCH_PXA_SHEPHERD

// Corgi
#ifdef CONFIG_ARCH_SHARP_SL_J // Japanese message
#include "corgiLogoMsgJ.c"
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
static int	logo_msg_xoff __initdata = 400;
static int	logo_msg_yoff __initdata = 100;
#else
static int	logo_msg_xoff __initdata = 120;
static int	logo_msg_yoff __initdata = 500;
#endif
#endif

#endif // CONFIG_ARCH_PXA_SHEPHERD

#endif
static void __init draw_img( int sx, int sy, int w, int h, unsigned short* data )
{
	int i, x, y;
	unsigned short	*p  = (unsigned short *)(remapped_fbuf);

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
		p = &(((unsigned short *)remapped_fbuf)[y*current_par.xres]);
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
		p = &(((unsigned short *)remapped_fbuf)[y*current_par.xres]);
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
	p  = (unsigned short *)(remapped_fbuf);
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
static void w100_set_disp(const void *unused, struct display *disp,
			   struct fb_info_gen *info)
{
  u32 temp32;
  struct w100fb_info *linfo = (struct w100fb_info *)info;
  int isInitTG = 0;

  // Set the hardware to 565
  temp32 = readl(remapped_regs+mmDISP_DEBUG2);
  temp32 &= 0xff7fffff;
  temp32 |= 0x00800000;
  writel(temp32, remapped_regs+mmDISP_DEBUG2);

#ifdef _IMAGE_CACHE_SUPPORT // FOR RESOLUTION CHANGE
  // VGA only
  if(LCD_MODE_480 == w100fb_lcdMode){
    __PRINTK("RESOLUTION CHANGE: save image cache ... ");

    if(save_img_cache_ptr || save_img_alloc_num){
      __PRINTK("ERROR!\n");
    }else{
      save_img_cache_ptr = save_image_cache(&save_img_alloc_num);

      if(save_img_cache_ptr && save_img_alloc_num){
	__PRINTK("SUCCESS!\n");
      }else{
	__PRINTK("ERROR!!\n");
      }
    }
  }
#endif //_IMAGE_CACHE_SUPPORT

  switch(w100fb_lcdMode){
  case LCD_MODE_480:
      if(current_par.xres == 320 && current_par.yres == 240){

	  printk("change resolution 480x640 => 320x240\n");

	  w100_PwmSetup();
	  w100_vsync();
	  w100_suspend(W100_SUSPEND_EXTMEM);
	  w100_init_sharp_lcd(LCD_SHARP_QVGA);
	  w100_init_qvga_rotation( (u16)270 );
	  w100_InitExtMem(LCD_SHARP_QVGA);
	  w100_clear_screen(LCD_SHARP_QVGA,NULL);
	  lcdtg_lcd_change(LCD_SHARP_QVGA);

	  w100fb_lcdMode = LCD_MODE_320;

      }else if(current_par.xres == 240 && current_par.yres == 320){

	  printk("change resolution 480x640 => 240x320\n");

	  w100_PwmSetup();
	  w100_vsync();
	  w100_suspend(W100_SUSPEND_EXTMEM);
	  w100_init_sharp_lcd(LCD_SHARP_QVGA);
	  w100_init_qvga_rotation( (u16)0 );
	  w100_InitExtMem(LCD_SHARP_QVGA);
  	  w100_clear_screen(LCD_SHARP_QVGA,NULL);
	  lcdtg_lcd_change(LCD_SHARP_QVGA);

	  w100fb_lcdMode = LCD_MODE_240;

      }
      break;
  case LCD_MODE_240:
      if(current_par.xres == 480 && current_par.yres == 640){

	  printk("change resolution 240x320 => 480x640\n");

	  w100_PwmSetup();
	  w100_clear_screen(LCD_SHARP_QVGA,NULL);
	  writel(0xBFFFA000, remapped_regs+mmMC_EXT_MEM_LOCATION);
	  w100_InitExtMem(LCD_SHARP_VGA);
	  w100_clear_screen(LCD_SHARP_VGA,(void*)0xF1A00000);
	  w100_vsync();
	  w100_init_sharp_lcd(LCD_SHARP_VGA);
	  lcdtg_lcd_change(LCD_SHARP_VGA);

	  w100fb_lcdMode = LCD_MODE_480;

      }else if(current_par.xres == 320 && current_par.yres == 240){

	  printk("change resolution 240x320 => 320x240\n");

  	  w100_clear_screen(LCD_SHARP_QVGA,NULL);
	  w100_init_qvga_rotation( (u16)270 );

	  w100fb_lcdMode = LCD_MODE_320;
      }
      break;
  case LCD_MODE_320:
      if(current_par.xres == 480 && current_par.yres == 640){

	  printk("change resolution 320x240 => 480x640\n");

	  w100_PwmSetup();
	  w100_clear_screen(LCD_SHARP_QVGA,NULL);
	  writel(0xBFFFA000, remapped_regs+mmMC_EXT_MEM_LOCATION);
	  w100_InitExtMem(LCD_SHARP_VGA);
	  w100_clear_screen(LCD_SHARP_VGA,(void*)0xF1A00000);
	  w100_vsync();
	  w100_init_sharp_lcd(LCD_SHARP_VGA);
	  lcdtg_lcd_change(LCD_SHARP_VGA);

	  w100fb_lcdMode = LCD_MODE_480;

      }else if(current_par.xres == 240 && current_par.yres == 320){

	  printk("change resolution 320x240 => 240x320\n");

  	  w100_clear_screen(LCD_SHARP_QVGA,NULL);
	  w100_init_qvga_rotation( (u16)0 );

	  w100fb_lcdMode = LCD_MODE_240;
      }
      break;
  case LCD_MODE_UNKOWN:
      printk("reset resolution unkown => 480x640\n");
      w100_init_sharp_lcd(LCD_SHARP_VGA);
      w100fb_lcdMode = LCD_MODE_480;
      isInitTG = 1;
      break;
  default:
      printk("resolution error !!! \n");
      break;
  }

#ifdef _IMAGE_CACHE_SUPPORT // FOR RESOLUTION CHANGE
  if(LCD_MODE_480 == w100fb_lcdMode){

    __PRINTK("RESOLUTION CHANGE: restore image cache ... ");
    if(save_img_cache_ptr != NULL &&
       save_img_alloc_num != 0){

      if(!restore_image_cache(save_img_cache_ptr,save_img_alloc_num)){
	save_img_cache_ptr = NULL;
	save_img_alloc_num = 0;
	__PRINTK("SUCCESS!\n");
      }else{
	__PRINTK("ERROR!!\n");
      }

    }else{
      __PRINTK("ERROR!\n");
    }
  }
#endif //_IMAGE_CACHE_SUPPORT

  // Set the initial contrast to something sane
  w100_gamma_init();

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

  switch(w100fb_lcdMode){
  case LCD_MODE_480:
      if(isInitTG != 0)
	  lcdtg_hw_init(LCD_SHARP_VGA);
      break;
  case LCD_MODE_240:
  case LCD_MODE_320:
      if(isInitTG != 0)
	  lcdtg_hw_init(LCD_SHARP_QVGA);
      break;
  default:
      printk("resolution error !!! \n");
      break;
  }
}

/* ------------ Interfaces to hardware functions ------------ */


struct fbgen_hwswitch w100_switch = 
{
  detect:	w100_detect,
  encode_fix:	w100_encode_fix,
  decode_var:	w100_decode_var,
  encode_var:	w100_encode_var,
  get_par:	w100_get_par,
  set_par:	w100_set_par,
  getcolreg:	w100_getcolreg,
  setcolreg:	w100_setcolreg,
  pan_display:	w100_pan_display,
  blank:	w100_blank,
  set_disp:	w100_set_disp,
};


/* ------------ Hardware Independent Functions ------------ */


static struct fb_ops w100fb_ops =
{
  owner:	  THIS_MODULE,
  fb_get_fix:	  fbgen_get_fix,
  fb_get_var:	  fbgen_get_var,
  fb_set_var:	  fbgen_set_var,
  fb_get_cmap:	  fbgen_get_cmap,
  fb_set_cmap:	  fbgen_set_cmap,
  fb_pan_display: fbgen_pan_display,
  fb_ioctl:       w100fb_ioctl,
};



int __init w100fb_setup(char *str)
{
  return 0;
}

int __init w100fb_init(void)
{

  w100fb_lcdMode = LCD_MODE_UNKOWN;

  fb_info.gen.fbhw = &w100_switch;
  fb_info.gen.fbhw->detect();
  strcpy(fb_info.gen.info.modename, W100_NAME);
  fb_info.gen.info.changevar = NULL;
  fb_info.gen.info.node = -1;
  fb_info.gen.info.fbops = &w100fb_ops;
  fb_info.gen.info.disp = &disp;
  fb_info.gen.parsize = sizeof(struct w100fb_par);
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
  w100_pm_dev = pm_register(PM_COTULLA_DEV, 0, w100_pm_callback);
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

void w100fb_cleanup(struct fb_info *info)
{

  unregister_framebuffer(info);

  iounmap(remapped_base);
  iounmap(remapped_regs);
  iounmap(remapped_fbuf);
  release_mem_region(W100_PHYS_ADDRESS, 0x1000000);

  if(gSaveImagePtr[0] != NULL){
	  int i;
	  for (i = 0; i < 640; i++) {
		  if (gSaveImagePtr[i] != NULL) {
			  kfree(gSaveImagePtr[i]);
			  gSaveImagePtr[i]=NULL;
		  }
	  }
  }
  isRemapped = 0;

#ifdef _IMAGE_CACHE_SUPPORT //FOR CLEANUP
  cleanup_image_cache(save_img_cache_ptr,
		      save_img_alloc_num);
  save_img_cache_ptr = NULL;
  save_img_alloc_num = 0;
#endif //_IMAGE_CACHE_SUPPORT
}

#ifdef MODULE
int __init init_module(void)
{
  return w100fb_init();
}

void cleanup_module(void)
{
  w100fb_cleanup(&fb_info.gen.info);
}
#endif

static int w100fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
		       u_long arg, int con, struct fb_info *info2)
{
  int fd;
  W100CONFIG_ARGS *args = (W100CONFIG_ARGS *)arg;

  switch (cmd)
  {
  case W100FB_CONFIG:
    fd = config_open((char *)args->config_filename);
    if (fd >= 0)
    {
      if (args->arg_count == 3)
      {
	w100_init_from_hwtab(W100INIT_ITEM,
			     fd,
			     (char *)args->section_name,
			     (char *)args->item_name, 0);
      }
      else if (args->arg_count == 4)
      {
	w100_init_from_hwtab(W100INIT_ITEM_WITH_VAL,
			     fd,
			     (char *)args->section_name,
			     (char *)args->item_name,
			     (u32)args->supplied_value);
      }
      else
      {
	w100_init_from_hwtab(W100INIT_ALL, fd, NULL, NULL, 0);
      }
      config_close(fd);
    }
    else
    {
      printk("w100fb_ioctl: could not open hwtbl. fd = %x\n", fd);
    }
    break;
  case W100FB_POWERDOWN:
  {
      int blank_mode = (int)arg;

      if (blank_mode && !w100fb_isblank) {
	  w100_pm_suspend( 0 );
	  fb_blank_normal = 0;
	  w100fb_isblank = blank_mode;
      }
      else if (!blank_mode && w100fb_isblank) {
	  w100_pm_resume( 0 );
	  fb_blank_normal = 0;
	  w100fb_isblank = blank_mode;
      }
  }
  break;

#if defined(CONFIG_ARCH_SHARP_SL)
  case W100FB_CONFIG_EX:
    {
      int *setup_arg = (int*)arg;
      int mode;

      // a)arg = NULL: check this ioctl.
      // b)arg[0] = 0: disable memory check (disable signal).
      // c)arg[0] = 1: enable memory check (enable signal).
      // d)arg[0] = 2: set image cache info.
      if(setup_arg == NULL)
	return 0;

      mode = setup_arg[0];
      switch(mode){
      case 0:
	if(disable_signal_to_mm == 0){

	  //printk("[w100fb] set the image cache info!\n");

	  // disable memory check
	  disable_signal_to_mm = 1;

	  return 0;
	}
	break;
      case 1:
	if(disable_signal_to_mm == 1){

	  //printk("[w100fb] reset the image cache info!\n");

	  // enable memory check
	  disable_signal_to_mm = 0;
	  return 0;

	}
	break;
#ifdef _IMAGE_CACHE_SUPPORT
      case 2:
	{
	  u32 start,end;
	  start = (u32)setup_arg[1];
	  end = (u32)setup_arg[2];

	  if(start < 0 || end > (REMAPPED_FB_LEN-1) || start > end){
	    // error ... don't skip.
	    start_skip_save_image_no = (-1);
	    end_skip_save_image_no = (-1);
	    return -EINVAL;
	  }

	  __PRINTK("[w100fb] start=%x, end=%x\n",start,end);

	  if(start != 0 || end != 0){
	    start = start/IMG_CACHE_MALLOC_SIZE + 1;
	    end = end/IMG_CACHE_MALLOC_SIZE - 1;

	    __PRINTK("[w100fb] start_no=%x, end_no=%x\n",start,end);

	    if(start <= end){
	      // do skip.
	      start_skip_save_image_no = start;
	      end_skip_save_image_no = end;
	      return 0;
	    }
	  }
	  // don't skip.
	  start_skip_save_image_no = (-1);
	  end_skip_save_image_no = (-1);
	  return 0;
	}
#endif //_IMAGE_CACHE_SUPPORT
      default:
	break;
      }
    }
    return -EINVAL;
    break;
#endif

  default:
    return -EINVAL;
  }

  return 0;
}

void w100_init_from_hwtab(int mode, int fd, char *section, char *item, u32 val)
{
  u32 temp32;
  disp_db_buf_cntl_wr_u disp_db_buf_wr_cntl;

  if (mode == W100INIT_ALL)
    printk("loading entire hwtab...\n");

  // Prevent display updates
  disp_db_buf_wr_cntl.f.db_buf_cntl = 0x1e; 
  disp_db_buf_wr_cntl.f.update_db_buf = 0;
  disp_db_buf_wr_cntl.f.en_db_buf = 0;
  writel((u32)(disp_db_buf_wr_cntl.val), remapped_regs+mmDISP_DB_BUF_CNTL);

  // Set up the display (NEC 240x320x16)

  // Clock and power management setup
  if (!strcmp(item, "ClkPinCntl") || mode == W100INIT_ALL)
  {
    printk("changing ClkPinCtrl: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x000401BF);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCLK_PIN_CNTL);
  }

  if (!strcmp(item, "PllRefFBDiv") || mode == W100INIT_ALL)
  {
    printk("changing PllRefFBDiv: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x50500D04);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmPLL_REF_FB_DIV);
  }

  if (!strcmp(item, "PllCntl") || mode == W100INIT_ALL)
  {
    printk("changing PllCntl: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x4B000200);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmPLL_CNTL);
  }

  if (!strcmp(item, "SClkCntl") || mode == W100INIT_ALL)
  {
    printk("changing SClkCntl: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00000B11);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmSCLK_CNTL);
  }

  if (!strcmp(item, "PClkCntl") || mode == W100INIT_ALL)
  {
    printk("changing PClkCntl: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00008041);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmPCLK_CNTL);
  }

  if (!strcmp(item, "ClkTestCntl") || mode == W100INIT_ALL)
  {
    printk("changing ClkTestCntl: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00000001);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCLK_TEST_CNTL);
  }

  if (!strcmp(item, "PwrMgtCntl") || mode == W100INIT_ALL)
  {
    printk("changing PwrMgtCntl: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0xFFFF11C5);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmPWRMGT_CNTL);
  }

  // LCD setup
  if (!strcmp(item, "LcdFormat") || mode == W100INIT_ALL)
  {
    printk("changing LcdFormat: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00008003);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmLCD_FORMAT);
  }

  if (!strcmp(item, "GraphicCtrl") || mode == W100INIT_ALL)
  {
    printk("changing GraphicCtrl: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00d41c06);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGRAPHIC_CTRL);
  }

  if (!strcmp(item, "GraphicOffset") || mode == W100INIT_ALL)
  {
    printk("changing GraphicOffset: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00100000);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGRAPHIC_OFFSET);
  }

  if (!strcmp(item, "GraphicPitch") || mode == W100INIT_ALL)
  {
    printk("changing GraphicPitch: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x000001e0);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGRAPHIC_PITCH);
  }

  if (!strcmp(item, "CrtcTotal") || mode == W100INIT_ALL)
  {
    printk("changing CrtcTotal: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x01510117);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_TOTAL); 
  }

  if (!strcmp(item, "ActiveHDisp") || mode == W100INIT_ALL)
  {
    printk("changing ActiveHDisp: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x01040014);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmACTIVE_H_DISP);
  }

  if (!strcmp(item, "ActiveVDisp") || mode == W100INIT_ALL)
  {
    printk("changing ActiveVDisp: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x01490009);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmACTIVE_V_DISP);
  }

  if (!strcmp(item, "GraphicHDisp") || mode == W100INIT_ALL)
  {
    printk("changing GraphicHDisp: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x01040014);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGRAPHIC_H_DISP);
  }

  if (!strcmp(item, "GraphicVDisp") || mode == W100INIT_ALL)
  {
    printk("changing GraphicVDisp: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x01490009);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGRAPHIC_V_DISP);
  }

  if (!strcmp(item, "CrtcSS") || mode == W100INIT_ALL)
  {
    printk("changing CrtcSS: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x80140013);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_SS);
  }

  if (!strcmp(item, "CrtcLS") || mode == W100INIT_ALL)
  {
    printk("chaning CrtsLS: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x800f000a);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_LS);
  }

  if (!strcmp(item, "CrtcGS") || mode == W100INIT_ALL)
  {
    printk("changing CrtcGS: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x80050005);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_GS);
  }

  if (!strcmp(item, "CrtcVPosGS") || mode == W100INIT_ALL)
  {
    printk("changing CrtcVPosGS: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x000a0009);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_VPOS_GS);
  }

  if (!strcmp(item, "CrtcGClk") || mode == W100INIT_ALL)
  {
    printk("changing CrtcGClk: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x8015010f);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_GCLK);
  }

  if (!strcmp(item, "CrtcGOE") || mode == W100INIT_ALL)
  {
    printk("changing CrtcGOE: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x80100110);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_GOE);
  }

  if (!strcmp(item, "CrtcRev") || mode == W100INIT_ALL)
  {
    printk("changing CrtcRev: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00400008);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_REV);
  }

  if (!strcmp(item, "CrtcDClk") || mode == W100INIT_ALL)
  {
    printk("changing CrtcDClk: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x2906000a);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_DCLK);
  }

  if (!strcmp(item, "CrtcDefaultCount") || mode == W100INIT_ALL)
  {
    printk("changing CrtcDefaultCount: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00000000);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_DEFAULT_COUNT);
  }

  if (!strcmp(item, "CrtcFrame") || mode == W100INIT_ALL)
  {
    printk("changing CrtcFrame: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00000000);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_FRAME);
  }

  if (!strcmp(item, "CrtcFrameVPos") || mode == W100INIT_ALL)
  {
    printk("changing CrtcFrameVPos: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00000000);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_FRAME_VPOS);
  }

  if (!strcmp(item, "LcddCntl2") || mode == W100INIT_ALL)
  {
    printk("changing LcddCntl2: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x0003ffff);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmLCDD_CNTL2);
  }

  if (!strcmp(item, "LcdBackGroundColor") || mode == W100INIT_ALL)
  {
    printk("changing LcdBackGroundColor: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x0000ff00);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmLCD_BACKGROUND_COLOR); 
  }

  if (!strcmp(item, "GpioCntl1") || mode == W100INIT_ALL)
  {
    printk("changing GpioCntl1: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00000000);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGPIO_CNTL1);
  }

  if (!strcmp(item, "GenLcdCntl3") || mode == W100INIT_ALL)
  {
    printk("changing GenLcdCntl3: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x000102aa);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGENLCD_CNTL3);
  }

  if (!strcmp(item, "GpioCntl2") || mode == W100INIT_ALL)
  {
    printk("changing GpioCntl2: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x03c000ff);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGPIO_CNTL2);
  }

  if (!strcmp(item, "LcddCntl1") || mode == W100INIT_ALL)
  {
    printk("changing LcddCntl1: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00000000);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmLCDD_CNTL1);
  }

  if (!strcmp(item, "GenLcdCntl1") || mode == W100INIT_ALL)
  {
    printk("changing GenLcdCntl1: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00fff003);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGENLCD_CNTL1);
  }

  if (!strcmp(item, "GenLcdCntl2") || mode == W100INIT_ALL)
  {
    printk("changing GenLcdCntl2: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x00000003);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmGENLCD_CNTL2);
  }

  if (!strcmp(item, "CrtcPS1Active") || mode == W100INIT_ALL)
  {
    printk("changing CrtcPS1Active: ");
    if (mode == W100INIT_ITEM_WITH_VAL)
    {
      temp32 = val;
      printk("supplied value is %x (%d)\n", val, val);
    }
    else
    {
      config_get_dword(fd, section, item, &temp32, 0x41060010);
      printk("config_get_dword got %x (%d)\n", temp32, temp32);
    }
    writel(temp32, remapped_regs+mmCRTC_PS1_ACTIVE);
  }

  // Re-enable display updates
  disp_db_buf_wr_cntl.f.db_buf_cntl = 0x1e;
  disp_db_buf_wr_cntl.f.update_db_buf = 1;
  disp_db_buf_wr_cntl.f.en_db_buf = 1;
  writel((u32)(disp_db_buf_wr_cntl.val), remapped_regs+mmDISP_DB_BUF_CNTL);
} 

//
// The code here supports the use of hardware tables
// to control the configuration of the driver. The
// open code attempts to open a file called atihwtbl.conf
// in /etc. If this file does not exist, calls to config_get_dword
// will return the default value provided to the call. See frame
// buffer documentation for a detailed description of hardware
// tables and how to use them.
//

//
// Open the configuration file for read
// 
int config_open(const char *config_filename)
{
  int fd;
  mm_segment_t old_fs = get_fs ();

  // Try opening the file name passed to us
  set_fs(KERNEL_DS);
  fd = open(config_filename, O_RDONLY, 0);
  set_fs(old_fs);

  return fd;
}

//
// Close the configuration file
//
int config_close(int config_fd)
{
  // If we have a good fd, let sys_close return it's value, otherwise return -1
  if (config_fd >= 0)
    return close(config_fd);
  else
  {
    errno = EBADF;
    return -1;
  }

  return 0;
}

//
// Get a dword from the configuration file. Find the item specified in id
// in the group specified by section. Return 0 on success, -1 on failure.
// On failure, the value returned in val is unspecified.
//
int config_get_dword(int config_fd, const char *section, const char *id, u32 *val, u32 defval)
{
  int x;
  char *local_sec, *local_id, *local_val;
  char line_buffer[256];
  long retval;
  mm_segment_t oldfs;

  // Sanity check
  if (config_fd < 0 || section == NULL || id == NULL || val == NULL)
  {
    errno = EINVAL;
    *val = defval;
    return -1;
  }

  local_sec = kmalloc(strlen(section)+3, GFP_KERNEL);
  strcpy(local_sec, "[");
  strcat(local_sec, section);
  strcat(local_sec, "]");

  local_id = kmalloc(strlen(id)+3, GFP_KERNEL);
  strcpy(local_id, "\"");
  strcat(local_id, id);
  strcat(local_id, "\"");

  // First, find the requested section
  while (1)
  {
    memset(line_buffer, '\0', 256);
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    for (x = 0; x < 256; x++)
    {
      if (!read(config_fd, &line_buffer[x], 1))
	goto done;
      if (line_buffer[x] == '\n' || line_buffer[x] == '\r')
	break;
    }
    set_fs(oldfs);

    if (!strncmp(line_buffer, "//", 2))
      continue;

    if (!strncmp(line_buffer, local_sec, strlen(local_sec)))
    {
      // Now find the id and extract the value
      while (1)
      {
	memset(line_buffer, '\0', 256);
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	for (x = 0; x < 256; x++)
	{
	  if (!read(config_fd, &line_buffer[x], 1))
	    goto done;
	  if (line_buffer[x] == '\n' || line_buffer[x] == '\r')
	    break;
	}
	set_fs(oldfs);

	// If the new line starts with a '[', it's probably a
	// section header so skip it
	if (line_buffer[0] == '[')
	  continue;
	if (!strncmp(line_buffer, local_id, strlen(local_id)))
	{
	  // We found the id, now extract the value
	  local_val = strpbrk(line_buffer, ":");
	  local_val++;
	  // If the value starts with an 0x it's hex otherwise, assume dec
	  if (!strncmp(local_val, "0x", 2))
	    retval = my_strtol(local_val, NULL, 16);
	  else
	    retval = my_strtol(local_val, NULL, 10);
	  *val = retval;
	  // free the local buffers
	  kfree(local_id);
	  kfree(local_sec);
	  return 0;
	}
      }  
    }
  }
 done:
  // Didn't find it, free local buffers and indicate the error
  kfree(local_id);
  kfree(local_sec);
  errno = EINVAL;
  *val = defval;
  return -1;
}

static char cvtIn[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,		  /* '0' - '9' */
  100, 100, 100, 100, 100, 100, 100,	  /* punctuation */
  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, /* 'A' - 'Z' */
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
  30, 31, 32, 33, 34, 35,
  100, 100, 100, 100, 100, 100,		  /* punctuation */
  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, /* 'a' - 'z' */
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
  30, 31, 32, 33, 34, 35};

int isspace(int x)
{
  return x == ' ' || (x >= '\b' && x <= '\r');
}

unsigned long my_strtoul(const char *string, char **endPtr, int base)
{
  register const char *p;
  register unsigned long int result = 0;
  register unsigned digit;
  int anyDigits = 0;
  int negative=0;
  int overflow=0;

  /*
   * Skip any leading blanks.
   */

  p = string;
  while (isspace((unsigned char)(*p)))
  {
    p += 1;
  }
  if (*p == '-')
  {
    negative = 1;
    p += 1;
  }
  else
  {
    if (*p == '+')
    {
      p += 1;
    }
  }

  /*
   * If no base was provided, pick one from the leading characters
   * of the string.
  */
  if (base == 0)
  {
    if (*p == '0')
    {
      p += 1;
      if ((*p == 'x') || (*p == 'X'))
      {
	p += 1;
	base = 16;
      }
      else
      {
	/*
	 * Must set anyDigits here, otherwise "0" produces a
	 * "no digits" error.
	 */

	anyDigits = 1;
	base = 8;
      }
    }
    else
      base = 10;
  } 
  else if (base == 16)
  {
    /*
     * Skip a leading "0x" from hex numbers.
     */

    if ((p[0] == '0') && ((p[1] == 'x') || (p[1] == 'X')))
    {
      p += 2;
    }
  }

  /*
   * Sorry this code is so messy, but speed seems important.  Do
   * different things for base 8, 10, 16, and other.
   */

  if (base == 8)
  {
    unsigned long maxres = ULONG_MAX >> 3;
    for ( ; ; p += 1)
    {
      digit = *p - '0';
      if (digit > 7)
      {
	break;
      }
      if (result > maxres)
      { 
	overflow = 1;
      }
      result = (result << 3);
      if (digit > (ULONG_MAX - result))
      {
	overflow = 1; 
      }
      result += digit;
      anyDigits = 1;
    }
  }
  else if (base == 10) 
  {
    unsigned long maxres = ULONG_MAX / 10;
    for ( ; ; p += 1)
    {
      digit = *p - '0';
      if (digit > 9)
      {
	break;
      }
      if (result > maxres)
      {
	overflow = 1;
      }
      result *= 10;
      if (digit > (ULONG_MAX - result))
      {
	overflow = 1;
      }
      result += digit;
      anyDigits = 1;
    }
  }
  else if (base == 16)
  {
    unsigned long maxres = ULONG_MAX >> 4;
    for ( ; ; p += 1)
    {
      digit = *p - '0';
      if (digit > ('z' - '0'))
      {
	break;
      }
      digit = cvtIn[digit];
      if (digit > 15)
      {
	break;
      }
      if (result > maxres)
      { 
	overflow = 1; 
      }
      result = (result << 4);
      if (digit > (ULONG_MAX - result)) 
      {
	overflow = 1; 
      }
      result += digit;
      anyDigits = 1;
    }
  }
  else if ( base >= 2 && base <= 36 )
  {
    unsigned long maxres = ULONG_MAX / base;
    for ( ; ; p += 1)
    {
      digit = *p - '0';
      if (digit > ('z' - '0'))
      {
	break;
      }
      digit = cvtIn[digit];
      if (digit >= ( (unsigned) base ))
      {
	break;
      }
      if (result > maxres)
      { 
	overflow = 1;
      }
      result *= base;
      if (digit > (ULONG_MAX - result))
      { 
	overflow = 1;
      }
      result += digit;
      anyDigits = 1;
    }
  }

  /*
   * See if there were any digits at all.
   */

  if (!anyDigits)
  {
    p = string;
  }

  if (endPtr != 0)
  {
    /* unsafe, but required by the strtoul prototype */
    *endPtr = (char *) p;
  }

  if (overflow)
  {
    errno = ERANGE;
    return ULONG_MAX;
  } 
  if (negative)
  {
    return -result;
  }
  return result;
}

long my_strtol(const char *string, char **endPtr, int base)
{
  register const char *p;
  long result;

  /*
   * Skip any leading blanks.
   */

  p = string;
  while (isspace((unsigned char)(*p)))
  {
    p += 1;
  }

  /*
   * Check for a sign.
   */

  if (*p == '-')
  {
    p += 1;
    result = -(my_strtoul(p, endPtr, base));
  }
  else
  {
    if (*p == '+')
    {
      p += 1;
    }
    result = my_strtoul(p, endPtr, base);
  }
  if ((result == 0) && (endPtr != 0) && (*endPtr == p))
  {
    *endPtr = (char *) string;
  }
  return result;
}


//
// Constants
//

#define SUCCESS 1
#define ERROR   0


//
// Types
//

typedef struct pll_parm
{
    u16 Freq;       // desired Fout for PLL
    u8  M;
    u8  N_int;
    u8  N_fac;
    u8  tfgoal;
    u8  lock_time;
} pll_parm_t;

typedef struct
{
    clk_pin_cntl_u    clk_pin_cntl;
    pll_ref_fb_div_u  pll_ref_fb_div;
    pll_cntl_u        pll_cntl;
    sclk_cntl_u       sclk_cntl;
    pclk_cntl_u       pclk_cntl;
    clk_test_cntl_u   clk_test_cntl;
    pwrmgt_cntl_u     pwrmgt_cntl;
    u32               Freq;           // Fout for PLL calibration
    u8                tf100;          // for pll calibration
    u8                tf80;           // for pll calibration
    u8                tf20;           // for pll calibration
    u8                M;              // for pll calibration
    u8                N_int;          // for pll calibration
    u8                N_fac;          // for pll calibration
    u8                lock_time;      // for pll calibration
    u8                tfgoal;         // for pll calibration
    u8                AutoMode;       // hardware auto switch?
    u8                PWMMode;        // 0 fast, 1 normal/slow
    u16               FastSclk;       // fast clk freq
    u16               NormSclk;       // slow clk freq
} power_state_t;


//
// Global state variables
//

static power_state_t gPowerState;

#ifdef USE_XTAL_12_5
// This table is specific for 12.5MHz ref crystal.
static pll_parm_t gPLLTable[] =
{
    //Freq     M   N_int    N_fac  tfgoal  lock_time
    { 50,      0,   1,       0,     0xE0,        56}, //  50.00 MHz
    { 75,      0,   5,       0,     0xDE,	 37}, //  75.00 MHz
    {100,      0,   7,       0,     0xE0,        28}, // 100.00 MHz 
    {  0,      0,   0,       0,        0,         0}  // Terminator
};
#else
// This table is specific for the 14.3MHz ref crystal found on a W100 board.
static pll_parm_t gPLLTable[] =
{
    //Freq     M   N_int    N_fac  tfgoal  lock_time
    { 50,      1,   6,       0,     0xE0,	 64}, //  50.05 MHz
    { 75,      0,   4,       3,     0xE0,	 43}, //  75.08 MHz
    {100,      0,   6,       0,     0xE0,        32}, // 100.10 MHz
    {  0,      0,   0,       0,        0,         0}  // Terminator
};
#endif


// making delay by reading our chipid;
// also known as CORE_DELAY_US.
static void w100_Delay(u32 Count)
{
    udelay(Count);
} // w100_Delay


static u8 w100_GetPLLTestCount( u8 testclk_sel)
{
  w100_Delay(5);		//delay 5 us

  gPowerState.clk_test_cntl.f.start_check_freq = 0x0;
  gPowerState.clk_test_cntl.f.testclk_sel      = testclk_sel;
  gPowerState.clk_test_cntl.f.tstcount_rst     = 0x1; //reset test count
  writel((u32)(gPowerState.clk_test_cntl.val), remapped_regs+mmCLK_TEST_CNTL);
  gPowerState.clk_test_cntl.f.tstcount_rst     = 0x0;
  writel((u32)(gPowerState.clk_test_cntl.val), remapped_regs+mmCLK_TEST_CNTL);

  gPowerState.clk_test_cntl.f.start_check_freq = 0x1;
  writel((u32)(gPowerState.clk_test_cntl.val), remapped_regs+mmCLK_TEST_CNTL);

  w100_Delay(20);		//delay 20 us

  gPowerState.clk_test_cntl.val = readl(remapped_regs+mmCLK_TEST_CNTL);
  gPowerState.clk_test_cntl.f.start_check_freq = 0x0;
  writel((u32)(gPowerState.clk_test_cntl.val), remapped_regs+mmCLK_TEST_CNTL);

  return(gPowerState.clk_test_cntl.f.test_count);
} // w100_GetPLLTestCount


static s8 w100_AdjustPLL(void)
{
  do
  {
    // Wai Ming 80 percent of VDD 1.3V gives 1.04V, minimum operating voltage is 1.08V
    // therefore, commented out the following lines
    // tf80 meant tf100
    // set VCO input = 0.8 * VDD
    gPowerState.pll_cntl.f.pll_dactal = 0xd;
    writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);

    gPowerState.tf80 = w100_GetPLLTestCount( 0x1); // PLLCLK
    if (gPowerState.tf80 >= (gPowerState.tfgoal))
    {
      // set VCO input = 0.2 * VDD
      gPowerState.pll_cntl.f.pll_dactal = 0x7;
      writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);

      gPowerState.tf20 = w100_GetPLLTestCount( 0x1); // PLLCLK
      if (gPowerState.tf20 <= (gPowerState.tfgoal))
        return( SUCCESS );

      if ((gPowerState.pll_cntl.f.pll_vcofr   == 0x0) &&
         ((gPowerState.pll_cntl.f.pll_pvg     == 0x7) ||
          (gPowerState.pll_cntl.f.pll_ioffset == 0x0)))
      {
        // slow VCO config
        gPowerState.pll_cntl.f.pll_vcofr   = 0x1;
        gPowerState.pll_cntl.f.pll_pvg     = 0x0;
        gPowerState.pll_cntl.f.pll_ioffset = 0x0;
        writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);
        continue;
      }
    }
    if ((gPowerState.pll_cntl.f.pll_ioffset) < 0x3)
    {
      gPowerState.pll_cntl.f.pll_ioffset += 0x1;
      writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);
      continue;
    }
    if ((gPowerState.pll_cntl.f.pll_pvg) < 0x7)
    {
      gPowerState.pll_cntl.f.pll_ioffset = 0x0;
      gPowerState.pll_cntl.f.pll_pvg    += 0x1;
      writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);
      continue;
    }
    return( ERROR);
  } while (1);
} // w100_AdjustPLL


//********************************************************************
// w100_PLL_Calibration
//                freq = target frequency of the PLL
//                (note: crystal = 14.3MHz)
//********************************************************************
static s8 w100_PLL_Calibration( u32 freq)
{
    s8 status = SUCCESS;

    // initial setting
    gPowerState.pll_cntl.f.pll_pwdn     = 0x0; // power down
    gPowerState.pll_cntl.f.pll_reset    = 0x0; // not reset
    gPowerState.pll_cntl.f.pll_tcpoff   = 0x1; // Hi-Z
    gPowerState.pll_cntl.f.pll_pvg      = 0x0; // VCO gain = 0
    gPowerState.pll_cntl.f.pll_vcofr    = 0x0; // VCO frequency range control = off
    gPowerState.pll_cntl.f.pll_ioffset  = 0x0; // current offset inside VCO = 0
    gPowerState.pll_cntl.f.pll_ring_off = 0x0; //
    writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);

    // check for (tf80 >= tfgoal) && (tf20 =< tfgoal)
    if ((gPowerState.tf80 < gPowerState.tfgoal) || (gPowerState.tf20 > gPowerState.tfgoal))
    {
        if (w100_AdjustPLL() == ERROR)
        {
            status = ERROR;
        }
    }

    // PLL Reset And Lock

    //set VCO input = 0.5 * VDD
    gPowerState.pll_cntl.f.pll_dactal = 0xa;
    writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);

    // reset time
    w100_Delay(1);                // delay 1 us

    // enable charge pump
    gPowerState.pll_cntl.f.pll_tcpoff = 0x0; // normal
    writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);

    // set VCO input = Hi-Z
    // disable DAC
    gPowerState.pll_cntl.f.pll_dactal = 0x0;
    writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);

    // lock time
    w100_Delay(400);                // delay 400 us

    // PLL locked

    gPowerState.sclk_cntl.f.sclk_src_sel = 0x1; // PLL clock
    writel((u32)(gPowerState.sclk_cntl.val), remapped_regs+mmSCLK_CNTL);

    gPowerState.tf100 = w100_GetPLLTestCount( 0x1); // PLLCLK

    return( status );
} // w100_PLL_Calibration


static s8 w100_SetPllClk(void)
{
    u8 status;

    if (gPowerState.AutoMode == 1) // auto mode
    {
        gPowerState.pwrmgt_cntl.f.pwm_fast_noml_hw_en = 0x0; // disable fast to normal
        gPowerState.pwrmgt_cntl.f.pwm_noml_fast_hw_en = 0x0; // disable normal to fast
        writel((u32)(gPowerState.pwrmgt_cntl.val), remapped_regs+mmPWRMGT_CNTL);
    }

    gPowerState.sclk_cntl.f.sclk_src_sel    = 0x0; // crystal clock
    writel((u32)(gPowerState.sclk_cntl.val), remapped_regs+mmSCLK_CNTL);

    gPowerState.pll_ref_fb_div.f.pll_ref_div     = gPowerState.M;
    gPowerState.pll_ref_fb_div.f.pll_fb_div_int  = gPowerState.N_int;
    gPowerState.pll_ref_fb_div.f.pll_fb_div_frac = gPowerState.N_fac;
    gPowerState.pll_ref_fb_div.f.pll_lock_time   = gPowerState.lock_time;
    writel((u32)(gPowerState.pll_ref_fb_div.val), remapped_regs+mmPLL_REF_FB_DIV);

    gPowerState.pwrmgt_cntl.f.pwm_mode_req = 0;
    writel((u32)(gPowerState.pwrmgt_cntl.val), remapped_regs+mmPWRMGT_CNTL);

    // w100_Delay(400);  //delay 400 us

    status = w100_PLL_Calibration( gPowerState.Freq);

    // gPowerState.sclk_cntl.f.sclk_src_sel    = 0x1; //PLL clock
    // writel((u32)(gPowerState.sclk_cntl.val), remapped_regs+mmSCLK_CNTL);

    if (gPowerState.AutoMode == 1) // auto mode
    {
        gPowerState.pwrmgt_cntl.f.pwm_fast_noml_hw_en = 0x1; // reenable fast to normal
        gPowerState.pwrmgt_cntl.f.pwm_noml_fast_hw_en = 0x1; // reenable normal to fast 
        writel((u32)(gPowerState.pwrmgt_cntl.val), remapped_regs+mmPWRMGT_CNTL);
    }
    return( status );
} // w100_SetPllClk


// assume reference crystal clk is 12.5MHz,
// and that doubling is not enabled.
//
// Freq = 12 == 12.5MHz.
static u16 w100_SetSlowSysClk(u16 Freq)
{
  if (gPowerState.NormSclk == Freq)
    return(Freq);

  if (gPowerState.AutoMode == 1) //auto mode
    return(0);

  if (Freq == 12)
  {
      gPowerState.NormSclk = Freq;
      gPowerState.sclk_cntl.f.sclk_post_div_slow    = 0x0; // Pslow = 1
      gPowerState.sclk_cntl.f.sclk_src_sel          = 0x0; // crystal src

      writel((u32)(gPowerState.sclk_cntl.val), remapped_regs+mmSCLK_CNTL);

      gPowerState.clk_pin_cntl.f.xtalin_pm_en       = 0x1;
      writel((u32)(gPowerState.clk_pin_cntl.val), remapped_regs+mmCLK_PIN_CNTL);

      //gPowerState.pll_cntl.f.pll_pm_en               = 0x0;
      //writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);

      gPowerState.pwrmgt_cntl.f.pwm_enable          = 0x1;
      gPowerState.pwrmgt_cntl.f.pwm_mode_req        = 0x1;
      writel((u32)(gPowerState.pwrmgt_cntl.val), remapped_regs+mmPWRMGT_CNTL);
      gPowerState.PWMMode  = 1;	//normal mode
      return(Freq);
  }
  else
  {
      return (0);
  }
} // w100_SetSlowSysClk


static u16 w100_SetFastSysClk(u16 Freq)
{
    u16        PLLFreq;
    int i;

    while (1)
    {
        PLLFreq = (u16)(Freq * (gPowerState.sclk_cntl.f.sclk_post_div_fast + 1));
        i = 0;
        do
        {
            if (PLLFreq == gPLLTable[i].Freq)
            {
                gPowerState.Freq      = gPLLTable[i].Freq * 1000000;
                gPowerState.M         = gPLLTable[i].M;
                gPowerState.N_int     = gPLLTable[i].N_int;
                gPowerState.N_fac     = gPLLTable[i].N_fac;
                gPowerState.tfgoal    = gPLLTable[i].tfgoal;
                gPowerState.lock_time = gPLLTable[i].lock_time;
                gPowerState.tf20      = 0xff;  // set highest
                gPowerState.tf80      = 0x00;  // set lowest

                w100_SetPllClk();
                gPowerState.PWMMode  = 0;      // fast mode
                gPowerState.FastSclk = Freq;
                return(Freq);
            }
            i++;
        } while (gPLLTable[i].Freq);

        if (gPowerState.AutoMode == 1) break;

        if (gPowerState.sclk_cntl.f.sclk_post_div_fast == 0) break;

        gPowerState.sclk_cntl.f.sclk_post_div_fast -= 1;
        writel((u32)(gPowerState.sclk_cntl.val), remapped_regs+mmSCLK_CNTL);
    }
    return(0);
} // w100_SetFastSysClk


// Set up an initial state.  Some values/fields set
// here will be overwritten.
static void w100_PwmSetup(void)
{
    gPowerState.clk_pin_cntl.f.osc_en             = 0x1;
    gPowerState.clk_pin_cntl.f.osc_gain           = 0x1f;
    gPowerState.clk_pin_cntl.f.dont_use_xtalin    = 0x0;
    gPowerState.clk_pin_cntl.f.xtalin_pm_en       = 0x0;
    gPowerState.clk_pin_cntl.f.xtalin_dbl_en      = 0x0; // no freq doubling
    gPowerState.clk_pin_cntl.f.cg_debug           = 0x0;
    writel((u32)(gPowerState.clk_pin_cntl.val), remapped_regs+mmCLK_PIN_CNTL);

    gPowerState.sclk_cntl.f.sclk_src_sel          = 0x0; // Crystal Clk
    gPowerState.sclk_cntl.f.sclk_post_div_fast    = 0x0; // Pfast = 1
    gPowerState.sclk_cntl.f.sclk_clkon_hys        = 0x3;
    gPowerState.sclk_cntl.f.sclk_post_div_slow    = 0x0; // Pslow = 1
    gPowerState.sclk_cntl.f.disp_cg_ok2switch_en  = 0x0;
    gPowerState.sclk_cntl.f.sclk_force_reg        = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.sclk_force_disp       = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.sclk_force_mc         = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.sclk_force_extmc      = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.sclk_force_cp         = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.sclk_force_e2         = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.sclk_force_e3         = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.sclk_force_idct       = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.sclk_force_bist       = 0x0; // Dynamic
    gPowerState.sclk_cntl.f.busy_extend_cp        = 0x0;
    gPowerState.sclk_cntl.f.busy_extend_e2        = 0x0;
    gPowerState.sclk_cntl.f.busy_extend_e3        = 0x0;
    gPowerState.sclk_cntl.f.busy_extend_idct      = 0x0;
    writel((u32)(gPowerState.sclk_cntl.val), remapped_regs+mmSCLK_CNTL);

    gPowerState.pclk_cntl.f.pclk_src_sel          = 0x0; // Crystal Clk
    gPowerState.pclk_cntl.f.pclk_post_div         = 0x1; // P = 2
    gPowerState.pclk_cntl.f.pclk_force_disp       = 0x0; // Dynamic
    writel((u32)(gPowerState.pclk_cntl.val), remapped_regs+mmPCLK_CNTL);

    gPowerState.pll_ref_fb_div.f.pll_ref_div      = 0x0; // M = 1
    gPowerState.pll_ref_fb_div.f.pll_fb_div_int   = 0x0; // N = 1.0
    gPowerState.pll_ref_fb_div.f.pll_fb_div_frac  = 0x0;
    gPowerState.pll_ref_fb_div.f.pll_reset_time   = 0x5;
    gPowerState.pll_ref_fb_div.f.pll_lock_time    = 0xff;
    writel((u32)(gPowerState.pll_ref_fb_div.val), remapped_regs+mmPLL_REF_FB_DIV);

    gPowerState.pll_cntl.f.pll_pwdn               = 0x1;
    gPowerState.pll_cntl.f.pll_reset              = 0x1;
    gPowerState.pll_cntl.f.pll_pm_en              = 0x0;
    gPowerState.pll_cntl.f.pll_mode               = 0x0; // uses VCO clock
    gPowerState.pll_cntl.f.pll_refclk_sel         = 0x0;
    gPowerState.pll_cntl.f.pll_fbclk_sel          = 0x0;
    gPowerState.pll_cntl.f.pll_tcpoff             = 0x0;
    gPowerState.pll_cntl.f.pll_pcp                = 0x4;
    gPowerState.pll_cntl.f.pll_pvg                = 0x0;
    gPowerState.pll_cntl.f.pll_vcofr              = 0x0;
    gPowerState.pll_cntl.f.pll_ioffset            = 0x0;
    gPowerState.pll_cntl.f.pll_pecc_mode          = 0x0;
    gPowerState.pll_cntl.f.pll_pecc_scon          = 0x0;
    gPowerState.pll_cntl.f.pll_dactal             = 0x0; // Hi-Z
    gPowerState.pll_cntl.f.pll_cp_clip            = 0x3;
    gPowerState.pll_cntl.f.pll_conf               = 0x2;
    gPowerState.pll_cntl.f.pll_mbctrl             = 0x2;
    gPowerState.pll_cntl.f.pll_ring_off           = 0x0;
    writel((u32)(gPowerState.pll_cntl.val), remapped_regs+mmPLL_CNTL);

    gPowerState.clk_test_cntl.f.testclk_sel       = 0x1; // PLLCLK (for testing)
    gPowerState.clk_test_cntl.f.start_check_freq  = 0x0;
    gPowerState.clk_test_cntl.f.tstcount_rst      = 0x0;
    writel((u32)(gPowerState.clk_test_cntl.val), remapped_regs+mmCLK_TEST_CNTL);

    gPowerState.pwrmgt_cntl.f.pwm_enable          = 0x0;
    gPowerState.pwrmgt_cntl.f.pwm_mode_req        = 0x1; // normal mode (0, 1, 3)
    gPowerState.pwrmgt_cntl.f.pwm_wakeup_cond     = 0x0;
    gPowerState.pwrmgt_cntl.f.pwm_fast_noml_hw_en = 0x0;
    gPowerState.pwrmgt_cntl.f.pwm_noml_fast_hw_en = 0x0;
    gPowerState.pwrmgt_cntl.f.pwm_fast_noml_cond  = 0x1; // PM4,ENG
    gPowerState.pwrmgt_cntl.f.pwm_noml_fast_cond  = 0x1; // PM4,ENG
    gPowerState.pwrmgt_cntl.f.pwm_idle_timer      = 0xFF;
    gPowerState.pwrmgt_cntl.f.pwm_busy_timer      = 0xFF;
    writel((u32)(gPowerState.pwrmgt_cntl.val), remapped_regs+mmPWRMGT_CNTL);

    gPowerState.AutoMode                          = 0;        // manual mode
    gPowerState.PWMMode                           = 1;        // normal mode (0, 1, 2)
    gPowerState.Freq                              = 50000000; // 50 MHz
    gPowerState.M                                 = 3;        // M = 4
    gPowerState.N_int                             = 6;        // N = 7.0
    gPowerState.N_fac                             = 0;        //
    gPowerState.tfgoal                            = 0xE0;
    gPowerState.lock_time                         = 56;
    gPowerState.tf20                              = 0xff;     // set highest
    gPowerState.tf80                              = 0x00;     // set lowest
    gPowerState.tf100                             = 0x00;     // set lowest
    gPowerState.FastSclk                          = 50;       // 50.0 MHz
    gPowerState.NormSclk                          = 12;       // 12.5 MHz
} // w100_PwmSetup


static void w100_init_sharp_lcd(u32 mode)
{
    u32 temp32;
    disp_db_buf_cntl_wr_u disp_db_buf_wr_cntl;

    // Prevent display updates
    disp_db_buf_wr_cntl.f.db_buf_cntl = 0x1e; 
    disp_db_buf_wr_cntl.f.update_db_buf = 0;
    disp_db_buf_wr_cntl.f.en_db_buf = 0;
    writel((u32)(disp_db_buf_wr_cntl.val), remapped_regs+mmDISP_DB_BUF_CNTL);

    switch (mode)
    {
        case LCD_SHARP_QVGA:
            w100_SetSlowSysClk(12);  // use crystal -- 12.5MHz
#if 0  	    // not use PLL
            w100_SetFastSysClk(50);  // use PLL     -- 50.0MHz
#endif
	    writel(0x7FFF8000, remapped_regs+mmMC_EXT_MEM_LOCATION);
	    writel(0x85FF8000, remapped_regs+mmMC_FB_LOCATION);
            writel(0x00000003, remapped_regs+mmLCD_FORMAT);
            writel(0x00CF1C06, remapped_regs+mmGRAPHIC_CTRL);
            writel(0x01410145, remapped_regs+mmCRTC_TOTAL); 
            writel(0x01170027, remapped_regs+mmACTIVE_H_DISP);
            writel(0x01410001, remapped_regs+mmACTIVE_V_DISP);
            writel(0x01170027, remapped_regs+mmGRAPHIC_H_DISP);
            writel(0x01410001, remapped_regs+mmGRAPHIC_V_DISP);
            writel(0x81170027, remapped_regs+mmCRTC_SS);
            writel(0xA0140000, remapped_regs+mmCRTC_LS);
            writel(0x00400008, remapped_regs+mmCRTC_REV);
	    writel(0xA0000000, remapped_regs+mmCRTC_DCLK);
	    writel(0xC0140014, remapped_regs+mmCRTC_GS);
            writel(0x00010141, remapped_regs+mmCRTC_VPOS_GS);
            writel(0x8015010F, remapped_regs+mmCRTC_GCLK);
            writel(0x80100110, remapped_regs+mmCRTC_GOE);
            writel(0x00000000, remapped_regs+mmCRTC_FRAME);
            writel(0x00000000, remapped_regs+mmCRTC_FRAME_VPOS);
            writel(0x01CC0000, remapped_regs+mmLCDD_CNTL1);
            writel(0x0003FFFF, remapped_regs+mmLCDD_CNTL2);
            writel(0x00FFFF0D, remapped_regs+mmGENLCD_CNTL1);
	    writel(0x003F3003, remapped_regs+mmGENLCD_CNTL2);
            writel(0x00000000, remapped_regs+mmCRTC_DEFAULT_COUNT);
            writel(0x0000FF00, remapped_regs+mmLCD_BACKGROUND_COLOR); 
            // hmm ...
            writel(0x000102aa, remapped_regs+mmGENLCD_CNTL3);
            // hmm ...
	    writel(0x00800000, remapped_regs+mmGRAPHIC_OFFSET);
            writel(0x000001e0, remapped_regs+mmGRAPHIC_PITCH);
            // hmm ...
            writel(0x000000bf, remapped_regs+mmGPIO_DATA);
	    writel(0x03c0feff, remapped_regs+mmGPIO_CNTL2);
            writel(0x00000000, remapped_regs+mmGPIO_CNTL1);
            writel(0x41060010, remapped_regs+mmCRTC_PS1_ACTIVE);
            break;
        case LCD_SHARP_VGA:
            w100_SetSlowSysClk(12);  // use crystal -- 12.5MHz
            w100_SetFastSysClk(75);  // use PLL     -- 75.0MHz
            gPowerState.pclk_cntl.f.pclk_src_sel  = 0x1;
            gPowerState.pclk_cntl.f.pclk_post_div = 0x2;
            writel((u32)(gPowerState.pclk_cntl.val), remapped_regs+mmPCLK_CNTL);
            writel(0x15FF1000, remapped_regs+mmMC_FB_LOCATION);
	    writel(0x9FFF8000, remapped_regs+mmMC_EXT_MEM_LOCATION);
            writel(0x00000003, remapped_regs+mmLCD_FORMAT);
	    writel(0x00DE1D66, remapped_regs+mmGRAPHIC_CTRL);

            writel(0x0283028B, remapped_regs+mmCRTC_TOTAL); 
            writel(0x02360056, remapped_regs+mmACTIVE_H_DISP);
            writel(0x02830003, remapped_regs+mmACTIVE_V_DISP);
            writel(0x02360056, remapped_regs+mmGRAPHIC_H_DISP);
            writel(0x02830003, remapped_regs+mmGRAPHIC_V_DISP);
            writel(0x82360056, remapped_regs+mmCRTC_SS);
            writel(0xA0280000, remapped_regs+mmCRTC_LS);
            writel(0x00400008, remapped_regs+mmCRTC_REV);
	    writel(0xA0000000, remapped_regs+mmCRTC_DCLK);
	    writel(0x80280028, remapped_regs+mmCRTC_GS);
            writel(0x02830002, remapped_regs+mmCRTC_VPOS_GS);
            writel(0x8015010F, remapped_regs+mmCRTC_GCLK);
            writel(0x80100110, remapped_regs+mmCRTC_GOE);
            writel(0x00000000, remapped_regs+mmCRTC_FRAME);
            writel(0x00000000, remapped_regs+mmCRTC_FRAME_VPOS);
            writel(0x01CC0000, remapped_regs+mmLCDD_CNTL1);
            writel(0x0003FFFF, remapped_regs+mmLCDD_CNTL2);
            writel(0x00FFFF0D, remapped_regs+mmGENLCD_CNTL1);
	    writel(0x003F3003, remapped_regs+mmGENLCD_CNTL2);
            writel(0x00000000, remapped_regs+mmCRTC_DEFAULT_COUNT);
            writel(0x0000FF00, remapped_regs+mmLCD_BACKGROUND_COLOR); 
            // hmm ...
            writel(0x000102aa, remapped_regs+mmGENLCD_CNTL3);
            // hmm ...
            writel(0x00800000, remapped_regs+mmGRAPHIC_OFFSET);
            writel(0x000003C0, remapped_regs+mmGRAPHIC_PITCH);
            // hmm ...
            writel(0x000000bf, remapped_regs+mmGPIO_DATA);
	    writel(0x03c0feff, remapped_regs+mmGPIO_CNTL2);
            writel(0x00000000, remapped_regs+mmGPIO_CNTL1);
            writel(0x41060010, remapped_regs+mmCRTC_PS1_ACTIVE);
            break;
        default:
            break;
    }

    // Hack for overlay in ext memory
    temp32 = readl(remapped_regs+mmDISP_DEBUG2);
    temp32 |= 0xc0000000;
    writel(temp32, remapped_regs+mmDISP_DEBUG2);

    // Re-enable display updates
    disp_db_buf_wr_cntl.f.db_buf_cntl = 0x1e;
    disp_db_buf_wr_cntl.f.update_db_buf = 1;
    disp_db_buf_wr_cntl.f.en_db_buf = 1;
    writel((u32)(disp_db_buf_wr_cntl.val), remapped_regs+mmDISP_DB_BUF_CNTL);
} // w100_init_sharp_lcd

static void w100_init_qvga_rotation(u16 deg)
{
    // for resolution change and rotation
    // GRAPHIC_CTRL
    // GRAPHIC_OFFSET
    // GRAPHIC_PITCH

    switch(deg){
    case 0:
	writel(0x00d41c06, remapped_regs+mmGRAPHIC_CTRL);
	writel(0x00800000, remapped_regs+mmGRAPHIC_OFFSET);
	writel(0x000001e0, remapped_regs+mmGRAPHIC_PITCH);
	break;
    case 270:
	writel(0x00d41c16, remapped_regs+mmGRAPHIC_CTRL);
	//writel(0x0080027e, remapped_regs+mmGRAPHIC_OFFSET);
	writel(0x0080027c, remapped_regs+mmGRAPHIC_OFFSET);
	writel(0x00000280, remapped_regs+mmGRAPHIC_PITCH);
	break;
    default:
	// not-support
	break;
    }
}

static void w100_suspend(u32 mode)
{
    u32 val;

    writel(0x7FFF8000, remapped_regs+mmMC_EXT_MEM_LOCATION);
    writel(0x00FF0000, remapped_regs+mmMC_PERF_MON_CNTL);

    val = readl(remapped_regs+mmMEM_EXT_TIMING_CNTL);
    val &= ~(0x00100000); // bit20=0
    val |= 0xFF000000;    // bit31:24=0xff
    writel(val, remapped_regs+mmMEM_EXT_TIMING_CNTL);

    val = readl(remapped_regs+mmMEM_EXT_CNTL);
    val &= ~(0x00040000); // bit18=0
    val |= 0x00080000;    // bit19=1
    writel(val, remapped_regs+mmMEM_EXT_CNTL);

    udelay(1); // wait 1us

    if(mode == W100_SUSPEND_EXTMEM){

	// CKE: Tri-State
	val = readl(remapped_regs+mmMEM_EXT_CNTL);
	val |= 0x40000000;    // bit30=1
	writel(val, remapped_regs+mmMEM_EXT_CNTL);

	// CLK: Stop
	val = readl(remapped_regs+mmMEM_EXT_CNTL);
	val &= ~(0x00000001);    // bit0=0
	writel(val, remapped_regs+mmMEM_EXT_CNTL);
    }else{

	writel(0x00000000, remapped_regs+mmSCLK_CNTL);
	writel(0x000000BF, remapped_regs+mmCLK_PIN_CNTL);
	writel(0x00000015, remapped_regs+mmPWRMGT_CNTL);

	udelay(5); // wait 5us

	val = readl(remapped_regs+mmPLL_CNTL);
	val |= 0x00000004; // bit2=1
	writel(val, remapped_regs+mmPLL_CNTL);
	writel(0x0000001d, remapped_regs+mmPWRMGT_CNTL);
    }
}


static void w100_soft_reset()
{
    u16 val = readw((u16*)remapped_base+6);
    writew(val|0x08, (u16*)remapped_base+6);
    udelay(100);
    writew(0x00, (u16*)remapped_base+6);
    udelay(100);
}

static void w100_resume()
{
    u32    temp32;

    w100_hw_init();
    w100_PwmSetup();

    temp32 = readl(remapped_regs+mmDISP_DEBUG2);
    temp32 &= 0xff7fffff;
    temp32 |= 0x00800000;
    writel(temp32, remapped_regs+mmDISP_DEBUG2);

	if (w100fb_lcdMode == LCD_MODE_480) {
		w100_init_sharp_lcd(LCD_SHARP_VGA);
	}
	else {
		w100_init_sharp_lcd(LCD_SHARP_QVGA);
		if (w100fb_lcdMode == LCD_MODE_320) {
			w100_init_qvga_rotation( (u16)270 );
		}
	}

    w100_gamma_init();

}

static void w100_clear_screen(u32 mode,void *pfbuf)
{
  u16 *pVram = (u16 *)remapped_fbuf;
  int i,numPix=0;

  if(pfbuf != NULL)
      pVram = pfbuf;

  if(mode == LCD_SHARP_VGA){
    numPix = 640*480;
  }else if(mode == LCD_SHARP_QVGA){
    numPix = 320*240;
  }
  for(i=0;i<numPix;i++)
    pVram[i] = 0xffff;
}

#define W100_VSYNC_TIMEOUT 30000 // timeout = 30[ms] > 16.8[ms]
//
static void w100_vsync()
{
    u32 tmp;
    int timeout = W100_VSYNC_TIMEOUT;

    tmp = readl(remapped_regs+mmACTIVE_V_DISP);

    // set vline pos 
    writel((tmp>>16)&0x3ff,remapped_regs+mmDISP_INT_CNTL);

    // disable vline irq
    tmp = readl(remapped_regs+mmGEN_INT_CNTL);

    tmp &= ~0x00000002;
    writel(tmp,remapped_regs+mmGEN_INT_CNTL);

    // clear vline irq status
    writel(0x00000002,remapped_regs+mmGEN_INT_STATUS);

    // enable vline irq
    writel((tmp|0x00000002),remapped_regs+mmGEN_INT_CNTL);

    // clear vline irq status
    writel(0x00000002,remapped_regs+mmGEN_INT_STATUS);

    while(timeout > 0)
    {
	if(readl(remapped_regs+mmGEN_INT_STATUS) & 0x00000002)
	    break;
	udelay(1);
	timeout--;
    }

    // disable vline irq
    writel(tmp,remapped_regs+mmGEN_INT_CNTL);

    // clear vline irq status
    writel(0x00000002,remapped_regs+mmGEN_INT_STATUS);
}

static void w100_InitExtMem(u32 mode)
{
    switch (mode)
    {
        case LCD_SHARP_QVGA:
            // QVGA doesn't use external memory
            // nothing to do, really.
            break;
        case LCD_SHARP_VGA:
            writel(0x00007800, remapped_regs+mmMC_BIST_CTRL);
            writel(0x00040003, remapped_regs+mmMEM_EXT_CNTL);			
            writel(0x00200021, remapped_regs+mmMEM_SDRAM_MODE_REG);
            w100_Delay(100);
            writel(0x80200021, remapped_regs+mmMEM_SDRAM_MODE_REG);
            w100_Delay(100);
            writel(0x00650021, remapped_regs+mmMEM_SDRAM_MODE_REG);
            w100_Delay(100);
	    writel(0x10002a4a, remapped_regs+mmMEM_EXT_TIMING_CNTL);
	    writel(0x7ff87012, remapped_regs+mmMEM_IO_CNTL);
            break;
        default:
            // assume our Wallaby board memory
            writel(0x00007800, remapped_regs+mmMC_BIST_CTRL);
            writel(0x00040007, remapped_regs+mmMEM_EXT_CNTL);
            writel(0x00200021, remapped_regs+mmMEM_SDRAM_MODE_REG);
            w100_Delay(100);
            writel(0x80200021, remapped_regs+mmMEM_SDRAM_MODE_REG);
            w100_Delay(100);
            writel(0x00650021, remapped_regs+mmMEM_SDRAM_MODE_REG);
            w100_Delay(100);
            writel(0x01002a5a, remapped_regs+mmMEM_EXT_TIMING_CNTL);
            writel(0x7ff07001, remapped_regs+mmMEM_IO_CNTL);
            break;
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
  if (w100fb_lcdMode == LCD_MODE_480) {
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
    u16 *pVram = (u16*)remapped_fbuf;

    for( i = 0; i < (current_par.xres * current_par.yres); i++ ) {
	*pVram = 0xffff; // white color
	pVram++;
    }

    // 60Hz x 2 frame = 16.7msec x 2 = 33.4 msec
    mdelay(34);

    // (1) backlight off
    corgibl_pm_callback(NULL, PM_SUSPEND, NULL);

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
//  W100SoftwareReset();

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

#ifdef _IMAGE_CACHE_SUPPORT //

// vga only
//#undef __PRINTK
//#define __PRINTK(arg...) printk(arg)
//#define __SUM
static u32* save_image_cache(u32 *alloc_num)
{
  u32 *img_src=NULL;
  u32 *img_dst=NULL;
  u32 img_size=0;
  u32 img_kmalloc_num=0;
  int i,j;
#ifdef __SUM
  u32 sum=0;
#endif 

#if defined(CONFIG_ARCH_SHARP_SL)
    // checking in mm/omm_kill.c
    //disable_signal_to_mm = 1; //@DEBUG
#endif

  __PRINTK("save image cache\n");
  img_src = (u32*)remapped_fbuf + IMG_CACHE_OFFSET_VGA/sizeof(u32);
  img_size = IMG_CACHE_TOTAL_SIZE_VGA;

  __PRINTK(" - remapped_fbuf = %x\n",(int)remapped_fbuf);
  __PRINTK(" - img_src = %x\n",(int)img_src);
  __PRINTK(" - img_size = %x\n",img_size);

  img_kmalloc_num = (img_size+IMG_CACHE_MALLOC_SIZE)/IMG_CACHE_MALLOC_SIZE;
  img_dst = kmalloc(img_kmalloc_num*sizeof(u32),GFP_KERNEL);
  if(img_dst == NULL)
    goto save_img_cache_err;

  __PRINTK(" - img_kmalloc_num = %d\n",img_kmalloc_num);
  __PRINTK(" - img_dst = %x\n",(int)img_dst);

  for(i=0;i<img_kmalloc_num;i++) img_dst[i] = (u32)NULL;

  for(i=0;i<img_kmalloc_num;i++){
#if 1 //skip image data
    if(i < start_skip_save_image_no || i > end_skip_save_image_no ||
       start_skip_save_image_no < 0 || end_skip_save_image_no < 0){
      img_dst[i] = (u32)kmalloc(IMG_CACHE_MALLOC_SIZE,GFP_KERNEL);
    }else{
      img_dst[i] = IMG_CACHE_SKIP_MARK;
    }
#else
    img_dst[i] = (u32)kmalloc(IMG_CACHE_MALLOC_SIZE,GFP_KERNEL);
#endif

    __PRINTK(" - malloc[%d] = %x\n",i,(int)img_dst[i]);

    if(img_dst[i] == (u32)NULL){
      img_kmalloc_num = i;
      for(i=0;i<img_kmalloc_num;i++){
#if 1 //skip image data
	if(img_dst[i] != IMG_CACHE_SKIP_MARK){
	  kfree((void*)img_dst[i]);
	}
#else
	kfree((void*)img_dst[i]);
#endif
	__PRINTK("-- error: free[%d]=%x,i\n",img_dst[i]);
      }
      goto save_img_cache_err;
    }
  }

  __PRINTK(" - img_src = %x\n",(int)img_src);

  for(i=0;i<img_kmalloc_num;i++){
    u32 *dst = (u32*)img_dst[i];
#if 1 //skip image data
    if((u32)dst == IMG_CACHE_SKIP_MARK){
      __PRINTK("...save_skip[%d] = %x\n",i,dst);
      for(j=0;j<(IMG_CACHE_MALLOC_SIZE/sizeof(u32));j++){
	img_src++;
	img_size -= sizeof(u32);
	if((int)img_size <= 0){
	  i=img_kmalloc_num; // for break!
	  break;
	}
      }
      if(i==img_kmalloc_num)
	break;
      continue;
    }
    __PRINTK("...save_exec[%d] = %x\n",i,dst);
#endif
    for(j=0;j<(IMG_CACHE_MALLOC_SIZE/sizeof(u32));j++){
      dst[j] = *img_src;
#ifdef __SUM
      sum+=*img_src;
#endif
      img_src++;
      img_size -= sizeof(u32);
      if((int)img_size <= 0){
	i=img_kmalloc_num; // for break!
	break;
      }
    }
  }
  __PRINTK(" - SUCCESS!!\n");
#ifdef __SUM
  __PRINTK(" - SUM=%x\n",(int)sum);
#endif

  *alloc_num = img_kmalloc_num;

#if defined(CONFIG_ARCH_SHARP_SL)
    // checking in mm/omm_kill.c
  //disable_signal_to_mm = 0; //@DEBUG
  //disable_signal_to_mm = 1; //@DEBUG
#endif

  return img_dst;

//
 save_img_cache_err:
  __PRINTK(" - ERROR!!\n");
  if(img_dst){
    kfree(img_dst);
  }
  *alloc_num = 0;

#if defined(CONFIG_ARCH_SHARP_SL)
    // checking in mm/omm_kill.c
  // disable_signal_to_mm = 0; //@DEBUG
  //  disable_signal_to_mm = 1; //@DEBUG
#endif

  return NULL;
}

//#undef __PRINTK
//#define __PRINTK(arg...) printk(arg)

static int restore_image_cache(u32 *img_src,u32 alloc_num)
{
  u32 *img_dst=NULL;
  u32 img_size=0;
  int i,j;
#ifdef __SUM
  u32 sum=0;
#endif

  if(img_src == NULL)
    return -1; //ERROR

  if(alloc_num == 0)
    return -2; //ERROR

  __PRINTK("restore image cache\n");
  img_dst = (u32*)remapped_fbuf + IMG_CACHE_OFFSET_VGA/sizeof(u32);
  img_size = IMG_CACHE_TOTAL_SIZE_VGA;

  __PRINTK(" - remapped_fbuf = %x\n",(int)remapped_fbuf);
  __PRINTK(" - img_src = %x\n",(int)img_src);
  __PRINTK(" - img_dst = %x\n",(int)img_dst);

  for(i=0;i<alloc_num;i++){
    u32 *src = (u32*)img_src[i];
#if 1 //skip image data
    if((u32)src == IMG_CACHE_SKIP_MARK){
      __PRINTK("...restore_skip[%d] = %x\n",i,src);
      for(j=0;j<(IMG_CACHE_MALLOC_SIZE/sizeof(u32));j++){
	img_dst++;
	img_size -= sizeof(u32);
	if((int)img_size <= 0){
	  i=alloc_num; // for break!
	  break;
	}
      }
      if(i==alloc_num)
	break;
      continue;
    }
    __PRINTK("...restore_exec[%d] = %x\n",i,src);
#else
    __PRINTK(" -- %d:[%x]\n",i,img_src[i]);
#endif

    for(j=0;j<(IMG_CACHE_MALLOC_SIZE/sizeof(u32));j++){
	*img_dst = src[j];
#ifdef __SUM
	sum+=*img_dst;
#endif
	img_dst++;
	img_size -= sizeof(u32);
	if((int)img_size <= 0){
	  i=alloc_num; // for break!
	  break;
	}
    }
    //__PRINTK(" - free[%d] = %x\n",i,src);
    //kfree(src);
  }

  __PRINTK(" - restore finished !!\n");

  for(i=0;i<alloc_num;i++){
    __PRINTK(" - free[%d] = %x\n",i,img_src[i]);
#if 1 //skip image data
    if(img_src[i] != IMG_CACHE_SKIP_MARK)
      kfree((void*)img_src[i]);
#else
    kfree((void*)img_src[i]);
#endif
  }

  __PRINTK(" - free[img_src] = %x\n",(int)img_src);
  kfree((void*)img_src);

  __PRINTK(" - SUCCESS!!\n");
#ifdef __SUM
  __PRINTK(" - SUM=%x\n",sum);
#endif

#if 1 //skip image data
  start_skip_save_image_no = (-1);
  end_skip_save_image_no = (-1);
#endif 
  return 0; //SUCCESS
}

static int cleanup_image_cache(u32 *img_src,u32 alloc_num)
{
  int i;

  if(img_src == NULL)
    return -1; //ERROR

  if(alloc_num == 0)
    return -2; //ERROR

  __PRINTK("cleanup image cache \n");
  __PRINTK(" - img_src = %x\n",(int)img_src);

  for(i=0;i<alloc_num;i++){
    __PRINTK(" -- %d:[%x]\n",i,img_src[i]);
#if 1 //skip image data
    if(img_src[i] != IMG_CACHE_SKIP_MARK)
      kfree((void*)img_src[i]);
#else
    kfree((void*)img_src[i]);
#endif
  }
  kfree((void*)img_src);

  return 0; //SUCCESS
}
#endif //_IMAGE_CACHE_SUPPORT

static void w100_pm_suspend(int suspend_mode)
{
    int i, j;
    u16 *pVram = (u16*)remapped_fbuf;

    if(suspend_mode == 1){
	// called from blank()
	isSuspended_tg_only = 1;
    }else{
	if(isSuspended_tg_only){
	    // suspend w100 only.
	    w100_suspend(W100_SUSPEND_ALL);
	    isSuspended_tg_only = 0;
	    return;
	}
	isSuspended_tg_only = 0;
    }

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

#ifdef _IMAGE_CACHE_SUPPORT // FOR SUSPEND
//#undef __PRINTK
//#define __PRINTK(arg...) //printk(arg)
	// VGA only
	if(w100fb_lcdMode == LCD_MODE_480){
	  __PRINTK("SUSPEND: save image cache ... ");
	  if(save_img_alloc_num != 0 || save_img_cache_ptr != NULL){
	    __PRINTK("ERROR!\n");
	  }else{
	    save_img_cache_ptr = save_image_cache(&save_img_alloc_num);

	    if(save_img_alloc_num == 0 || save_img_cache_ptr == NULL){
	      __PRINTK("ERROR!!\n");
	    }else{
	      __PRINTK("SUCCESS!\n");
	    }
	  }
	}
#endif //_IMAGE_CACHE_SUPPORT

    lcdtg_suspend();

    if(suspend_mode == 1){
	// w100 suspend skip ...
	return;
    }

    w100_suspend(W100_SUSPEND_ALL);
}

static void w100_pm_resume(int resume_mode)
{
    int i, j;
    u16 *pVram = (u16*)remapped_fbuf;

    if(resume_mode != 1 || !isSuspended_tg_only){
	w100_resume();
    }
    else{
    }
    isSuspended_tg_only = 0;

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

#ifdef _IMAGE_CACHE_SUPPORT // FOR RESUME
//#undef __PRINTK
//#define __PRINTK(arg...) //printk(arg)
    // VGA only
    if(w100fb_lcdMode == LCD_MODE_480){
      __PRINTK("RESUME: restore image cache ... ");
      if(save_img_alloc_num == 0 || save_img_cache_ptr == NULL){
	__PRINTK("ERROR!\n");
      }else{
	if(restore_image_cache(save_img_cache_ptr,
			     save_img_alloc_num)){
	  __PRINTK("ERROR!!\n");
	}else{
	  save_img_alloc_num=0;
	  save_img_cache_ptr=NULL;
	  __PRINTK("SUCCESS!\n");
	}
      }
    }
#endif //_IMAGE_CACHE_SUPPORT

    lcdtg_resume();

}

#ifdef CONFIG_PM
static int w100_pm_callback(struct pm_dev* pm_dev,
			    pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
		if (!w100fb_isblank) {
		    w100_pm_suspend( 0 );
		    fb_blank_normal = 0;

		}
		break;
		
	case PM_RESUME:
		w100_pm_resume( 0 );
		fb_blank_normal = 0;
		w100fb_isblank = 0;
		break;
		
	}

	return 0;
}

void w100_fatal_off(void)
{
  lcdtg_suspend();
  w100_resume();
  w100_suspend(W100_SUSPEND_ALL);
}
#endif //CONFIG_PM

