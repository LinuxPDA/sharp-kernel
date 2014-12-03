/*
 *  linux/drivers/video/irisfb.c
 * 
 *  Based on simple frame buffer by Mike Klar, which was
 *  Based on virtual frame buffer by Geert Uytterhoeven.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */
/*
 * Modifications for SHARP Iris
 *	2001.2.16	Namikosi	Modify for SHARP Iris 
 *	2001.2.19	H.Hayami	irisfb_encode_fix(): handle phys. adr.
 *	2001.3.02	Lineo Japan     Changed for Embedix
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
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/wrapper.h>
#include <linux/pm.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/proc/pgtable.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>

#include <asm/hardware.h>
#include <asm/arch/fpga.h>

#ifdef CONFIG_PM
static struct pm_dev *fb_pm_dev;
#endif

#define FB_X_RES	176
#define FB_Y_RES	220
#define FB_BPP		16
#define FB_IS_GREY	0
#define FB_INVERSE	0
#define VIDEORAM_SIZE   (FB_X_RES * FB_Y_RES * FB_BPP / 8)

#define CLEAR_BUFFER_BEFORE_LCDOFF

#define ALLOCATED_FB_MEM_SIZE (PAGE_ALIGN(VIDEORAM_SIZE + PAGE_SIZE))
static u_char* vram_base      = (u_char*)0;
static u_char* vram_base_phys = (u_char*)0;

volatile u_long videomemorysize;

static int currcon = 0;
static struct display disp;	
static struct fb_info irisfb_info;
#if 0
static struct { u_char red, green, blue, pad; } palette[256];
#endif

// cmap 16bpp
static union {
#ifdef FBCON_HAS_CFB16
  u16 cfb16[16];
#endif
} fbcon_cmap;

static char irisfb_name[16] = "Iris-Fb";

static struct fb_var_screeninfo irisfb_var_default = {
  // __u32 xres,yres	/* visible resolution */
  FB_X_RES, FB_Y_RES,
  //  __u32 xres_virtual,yres_virtual /* virtual resolution	*/
  FB_X_RES, FB_Y_RES,
  //  __u32 xoffset,yoffset  /* offset from virtual to visible resolution */
  0, 0,
  // __u32 bits_per_pixel;	/* guess what			*/
  FB_BPP,
  // __u32 grayscale;		/* != 0 Graylevels instead of colors */
  FB_IS_GREY,
  // struct fb_bitfield red,green,blue 
  // /* bitfield in fb mem if true color,else only length is significant */
  {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
  // struct fb_bitfield transp   /* transparency */
  {0, 0, 0},
  // __u32 nonstd;			/* != 0 Non standard pixel format */
  0,
  // __u32 activate;			/* see FB_ACTIVATE_*		*/
  FB_ACTIVATE_NOW,
  // __u32 height,width  /* height and width of picture in mm */
  -1, -1,
  // __u32 accel_flags;		/* acceleration flags (hints)	*/
  0,

  /* Timing: All values in pixclocks, except pixclock (of course) */
  // __u32 pixclock;	/* pixel clock in ps (pico seconds) */
  20000,
  // __u32 left_margin;		/* time from sync to picture	*/
  64,
  // __u32 right_margin;		/* time from picture to sync	*/
  64,
  // __u32 upper_margin,lower_margin;	/* time from sync to picture	*/
  32, 32,
  //  __u32 hsync_len;		/* length of horizontal sync	*/
  64,
  // __u32 vsync_len;		/* length of vertical sync	*/
  2,
  // __u32 sync;			/* see FB_SYNC_*		*/
  0,
  // __u32 vmode;			/* see FB_VMODE_*		*/
  FB_VMODE_NONINTERLACED,
  // __u32 reserved[6];		/* Reserved for future compatibility */
  {0,0,0,0,0,0}
};

// helper functions
static u_long get_line_length(int xres_virtual, int bpp);
static void irisfb_encode_fix(struct fb_fix_screeninfo *fix, 
			     struct fb_var_screeninfo *var);
static void set_color_bitfields(struct fb_var_screeninfo *var);
static int irisfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info);
static int irisfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);

// framebuffer functions ( for fb_ops )
static int irisfb_open(struct fb_info *info, int user);
static int irisfb_release(struct fb_info *info, int user);
static int irisfb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info);
static int irisfb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info);
static int irisfb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info);
static int irisfb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info);
static int irisfb_set_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info);
static int irisfb_pan_display(struct fb_var_screeninfo *var, 
				int con, struct fb_info *info);
static int irisfb_ioctl(struct inode *inode, struct file *file, u_int cmd, 
		       u_long arg, int con, struct fb_info *info);

static struct fb_ops irisfb_ops = {
  //    owner:          THIS_MODULE,  // for 2.4.0 lator?
    fb_open:        irisfb_open,
    fb_release:     irisfb_release,
    fb_get_fix:     irisfb_get_fix,
    fb_get_var:     irisfb_get_var,
    fb_set_var:     irisfb_set_var,
    fb_get_cmap:    irisfb_get_cmap,
    fb_set_cmap:    irisfb_set_cmap,
    fb_pan_display: irisfb_pan_display,
    fb_ioctl:       irisfb_ioctl,
};

static void do_welcome_screen(void);

// console support functions
static int irisfb_fbcon_switch(int con, struct fb_info *info);
static int irisfb_fbcon_updatevar(int con, struct fb_info *info);
static void irisfb_fbcon_blank(int blank, struct fb_info *info);

// init&cleanup functions
int irisfb_init(void);
int irisfb_setup(char *options);
void irisfb_cleanup(void);

// ===================================================================
// ===================================================================

// --------------------------------------------
// helper functions

static u_long get_line_length(int xres_virtual, int bpp)
{
    u_long length;
    
    length = (xres_virtual * bpp + 7) >> 3;
    return(length);
}

// fill in the fixed display params based on var
static void irisfb_encode_fix(struct fb_fix_screeninfo* fix, struct fb_var_screeninfo* var )
{
  //printk("fbfunc:irisfb_encode_fix\r\n");  
  memset(fix, 0, sizeof(struct fb_fix_screeninfo));
  strcpy(fix->id, irisfb_name);
  fix->smem_start = (unsigned long)vram_base_phys;
  fix->smem_len = videomemorysize;
  fix->type = FB_TYPE_PACKED_PIXELS;
  fix->type_aux = 0;
  switch (var->bits_per_pixel) {
  case 1:
    fix->visual = FB_VISUAL_MONO01;
    break;
  case 2:
  case 4:
  case 8:
    fix->visual = FB_VISUAL_PSEUDOCOLOR;
    break;
  case 16:
    fix->visual = FB_VISUAL_TRUECOLOR;
    break;
  }
  fix->ywrapstep = 1;
  fix->xpanstep = 1;
  fix->ypanstep = 1;
  fix->line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);

}

// fill in the color bitfield info
void set_color_bitfields(struct fb_var_screeninfo *var)
{
  //printk("fbfunc:set_color_bitfields(%d)\r\n",var->bits_per_pixel);  

  switch (var->bits_per_pixel) {
  case 1:
  case 2:
  case 4:
  case 8:
    var->red.offset = 0;
    var->red.length = var->bits_per_pixel;
    var->green.offset = 0;
    var->green.length = var->bits_per_pixel;
    var->blue.offset = 0;
    var->blue.length = var->bits_per_pixel;
    var->transp.offset = 0;
    var->transp.length = 0;
    break;

  case 16:  // RGB 565
    var->red.offset = 11;
    var->red.length = 5;
    var->green.offset = 5;
    var->green.length = 6;
    var->blue.offset = 0;
    var->blue.length = 5;
    var->transp.offset = 0;
    var->transp.length = 0;
    break;

  }
  var->red.msb_right = 0;
  var->green.msb_right = 0;
  var->blue.msb_right = 0;
  var->transp.msb_right = 0;
}

#if FB_IS_TRUECOLOR
#define MAX_REGNO 15
#else
#define MAX_REGNO ((1 << FB_BPP) - 1)
#endif

static int irisfb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info)
{
  // Read a single color register and split it into
  // colors/transparent. Return != 0 for invalid regno.
  //printk("fbfunc:irisfb_getcolreg\r\n");  

  if (regno > MAX_REGNO)
    return 1;

#if 1
  *green  = ((fbcon_cmap.cfb16[regno] >> 11) & 31) << 11;
  *red    = ((fbcon_cmap.cfb16[regno] >> 6) & 31) << 11;
  *blue   = ((fbcon_cmap.cfb16[regno]) & 63 ) << 10;
#else
  *red   = (palette[regno].red<<8) | palette[regno].red;
  *green = (palette[regno].green<<8) | palette[regno].green;
  *blue  = (palette[regno].blue<<8) | palette[regno].blue;
#endif
  *transp = 0;

  return 0;
}

static int irisfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
  // Set a single color register. The values supplied are already
  // rounded down to the hardware's capabilities (according to the
  // entries in the var structure). Return != 0 for invalid regno.
  
  // So it says anyway.
  // From what I can tell, drivers/video/fbcmap.c passes in
  // un-rounded-down 16-bit values.
  //printk("fbfunc:irisfb_setcolreg\r\n");  
	
  if (regno > MAX_REGNO)
    return 1;

#if 1
	red   >>= 11;
	green >>= 11;
	blue  >>= 10;

	if (regno < 16) {
		fbcon_cmap.cfb16[regno] = ((red & 31) << 6) |
					  ((green & 31) << 11) |
					  (blue & 63);
	}
#else

#ifdef FBCON_HAS_CFB8
  if( regno < 16 ) {
    // maintain index to rgb map for fbcon (accessed through dispsw_data)
    fbcon_cmap.cfb8[regno] =
      ((red & 0xe000) >> 8)
      | ((green & 0xe000) >> 11)
      | ((blue & 0xc000) >> 14);
  }
#endif 

  red >>= 8;
  green >>= 8;
  blue >>= 8;

  palette[regno].red = red;
  palette[regno].green = green;
  palette[regno].blue = blue;

#endif

  return 0;
}

// called by irisfb_set_var, irisfb_fbcon_switch 
static void do_install_cmap(int con, struct fb_info *info)
{
  //printk("fbfunc:do_install_cmap\r\n");  

  if (con != currcon) return;

  if (fb_display[con].cmap.len)
    fb_set_cmap(&fb_display[con].cmap, 1, irisfb_setcolreg, info);
  else
    fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), 1,
		irisfb_setcolreg, info);
}


// --------------------------------------------
// framebuffer functions ( for fb_ops )

// Open the frame buffer device
static int irisfb_open(struct fb_info *info, int user)
{
  // Nothing, only a usage count for the moment
  //  printk("fbfunc:irisfb_open\r\n");

  MOD_INC_USE_COUNT;
  return 0;
}

// Release the frame buffer device
static int irisfb_release(struct fb_info *info, int user)
{
  //  printk("fbfunc:irisfb_release\r\n");  
  MOD_DEC_USE_COUNT;
  return 0;
}

// Get the Fixed Part of the Display
static int irisfb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
  struct fb_var_screeninfo *var;
  // printk("fbfunc:irisfb_get_fix\r\n");  
  //printk("Frame Buffer Address[%08x],size[%08x]\r\n",(long)FB,(long)VIDEORAM_SIZE);

  if (con == -1)
    var = &irisfb_var_default;
  else
    var = &fb_display[con].var;

  // fill in the fixed display params based on var
  irisfb_encode_fix(fix, var);

  return 0;

}

// Get the User Defined Part of the Display
static int irisfb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
  //printk("fbfunc:irisfb_get_var\r\n");  

  if (con == -1)
    *var = irisfb_var_default;
  else
    *var = fb_display[con].var;

  // fill in the color bitfield info
  set_color_bitfields(var);

  return 0;
}

// Set the User Defined Part of the Display
static int irisfb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
    int err, activate = var->activate;
    int oldxres, oldyres, oldvxres, oldvyres, oldbpp;
    u_long line_length;
    struct display *display;

    // printk("fbfunc:irisfb_set_var\r\n");  

    if (con >= 0)
	display = &fb_display[con];
    else
	display = &disp;	/* used during initialization */

    /*
     *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
     *  as FB_VMODE_SMOOTH_XPAN is only used internally
     */

    if (var->vmode & FB_VMODE_CONUPDATE) {
	var->vmode |= FB_VMODE_YWRAP;
	var->xoffset = display->var.xoffset;
	var->yoffset = display->var.yoffset;
    }

    /*
     *  Some very basic checks
     */
    if (!var->xres)
	var->xres = 1;
    if (!var->yres)
	var->yres = 1;
    if (var->xres > var->xres_virtual)
	var->xres_virtual = var->xres;
    if (var->yres > var->yres_virtual)
	var->yres_virtual = var->yres;
    if (var->bits_per_pixel <= 1)
	var->bits_per_pixel = 1;
    else if (var->bits_per_pixel <= 2)
	var->bits_per_pixel = 2;
    else if (var->bits_per_pixel <= 4)
	var->bits_per_pixel = 4;
    else if (var->bits_per_pixel <= 8)
	var->bits_per_pixel = 8;
    else if (var->bits_per_pixel <= 16)
	var->bits_per_pixel = 16;
    else
	return -EINVAL;

    /*
     *  Memory limit
     */
    line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
    if (line_length*var->yres_virtual > videomemorysize)
	return -ENOMEM;

    set_color_bitfields(var);

    if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
	oldxres = display->var.xres;
	oldyres = display->var.yres;
	oldvxres = display->var.xres_virtual;
	oldvyres = display->var.yres_virtual;
	oldbpp = display->var.bits_per_pixel;
	display->var = *var;
	if (oldxres != var->xres || oldyres != var->yres ||
	    oldvxres != var->xres_virtual || oldvyres != var->yres_virtual ||
	    oldbpp != var->bits_per_pixel) {
	    struct fb_fix_screeninfo fix;

	    irisfb_encode_fix(&fix, var);
	    display->screen_base = (char *)vram_base;
	    display->visual = fix.visual;
	    display->type = fix.type;
	    display->type_aux = fix.type_aux;
	    display->ypanstep = fix.ypanstep;
	    display->ywrapstep = fix.ywrapstep;
	    display->line_length = fix.line_length;
	    display->can_soft_blank = 0;
	    //	    display->inverse = FB_IS_INVERSE;
	    display->inverse = 0;
	    display->scrollmode = SCROLL_YREDRAW;
	    switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_MFB
		case 1:
		    display->dispsw = &fbcon_mfb;
		    break;
#endif
#ifdef FBCON_HAS_CFB2
		case 2:
		    display->dispsw = &fbcon_cfb2;
		    break;
#endif
#ifdef FBCON_HAS_CFB4
		case 4:
		    display->dispsw = &fbcon_cfb4;
		    break;
#endif
#ifdef FBCON_HAS_CFB8
		case 8:
		    display->dispsw = &fbcon_cfb8;
		    break;
#endif
#ifdef FBCON_HAS_CFB16
		case 16:
		    display->dispsw = &fbcon_cfb16;
		    display->dispsw_data = fbcon_cmap.cfb16;
		    break;
#endif
		default:
		    display->dispsw = &fbcon_dummy;
		    break;
	    }
	    if (irisfb_info.changevar)
		(*irisfb_info.changevar)(con);
	}
	if (oldbpp != var->bits_per_pixel) {
	    if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
		return err;
	    do_install_cmap(con, info);
	}
    }
    return 0;
}

// Get the Colormap
static int irisfb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
  //printk("fbfunc:irisfb_get_cmap\r\n");  
  return 0; // dummy
}

// Set the Colormap
static int irisfb_set_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
  //printk("fbfunc:irisfb_set_cmap\r\n");  

  return 0; // dummy
}

// Pan or Wrap the Display
// This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
static int irisfb_pan_display(struct fb_var_screeninfo *var, 
				int con, struct fb_info *info)
{
  return -EINVAL;
}

#define FBIO_IRIS_DO_WELCOME   0x10
#define FBIO_IRIS_ENABLE_FBCURSOR   0x11
#define FBIO_IRIS_DISABLE_FBCURSOR  0x12

int iris_fb_cursor_enable = 0;

// Iris Framebuffer Specific ioctrl
static int irisfb_ioctl(struct inode *inode, struct file *file, u_int cmd, 
		       u_long arg, int con, struct fb_info *info)
{
  // printk("fbfunc:irisfb_ioctl\r\n");  
#if 1
  switch(cmd){
  case FBIO_IRIS_DO_WELCOME:
    do_welcome_screen();
    return 0;
  case FBIO_IRIS_ENABLE_FBCURSOR:
    iris_fb_cursor_enable = 1;
    return 0;
  case FBIO_IRIS_DISABLE_FBCURSOR:
    iris_fb_cursor_enable = 0;
    return 0;
  }
#endif
#if 0 // dummy sample
  int value;
  switch(cmd) {
  case FBIOGET_CONTRAST:
    value = GET_LCD_CONTRAST();
    put_user(value, (int*)arg);
    return 0;
  case FBIOSET_CONTRAST:
    SET_LCD_CONTRAST(arg);
    return 0;
  }
#endif // end dummy
  return -EINVAL;
}

#if 0
/* perform fb specific mmap */
static int irisfb_mmap(struct fb_info *info, struct file *file, struct vm_area_struct *vma)
{
  if (remap_page_range(vma->vm_start, vma->vm_offset,
		       vma->vm_end - vma->vm_start, vma->vm_page_prot))
    return -EAGAIN;
  vma->vm_file = file;
  file->f_count++;
  return 0;
}
#endif

// --------------------------------------------
// console support functions

static int irisfb_fbcon_switch(int con, struct fb_info *info)
{
  // printk("fbfunc:irisfb_fbcon_switch\r\n");  
  // Do we have to save the colormap?
  if (fb_display[currcon].cmap.len)
    fb_get_cmap(&fb_display[currcon].cmap, 1, irisfb_getcolreg, info);

  currcon = con;
  // Install new colormap
  do_install_cmap(con, info);
  return 0;
}

// Update the `var' structure (called by fbcon.c)
static int irisfb_fbcon_updatevar(int con, struct fb_info *info)
{
  return 0;
}

// Blank the display.
static void irisfb_fbcon_blank(int blank, struct fb_info *info)
{
}


// --------------------------------------------
// init&cleanup functions

/* #define __init __attribute__ ((__section__ (".text.init"))) */

#define L7210_INTERNAL_SRAM_SIZE (80*1024) /* 80KB */

extern unsigned int l72xx_read_sysid(void);
static int irisfb_using_internal_sram = 0;

static int __init irisfb_map_video_memory(void)
{
	u_int required_pages;
	u_int extra_pages;
	u_int order;
        struct page* page;
        char* allocated_region;

	if (vram_base != NULL)
		return -EINVAL;

#ifdef CLEAR_BUFFER_BEFORE_LCDOFF
	/* allocate VRAM just twice as large as real size */
	required_pages = ( ALLOCATED_FB_MEM_SIZE << 1 ) >> PAGE_SHIFT;
#else /* ! CLEAR_BUFFER_BEFORE_LCDOFF */
	required_pages = ALLOCATED_FB_MEM_SIZE >> PAGE_SHIFT;
#endif / * ! CLEAR_BUFFER_BEFORE_LCDOFF */
        for (order = 0 ; required_pages >> order ; order++) {;}
        extra_pages = (1 << order) - required_pages;

	if( l72xx_read_sysid() == 0x7210 ){
	  if( VIDEORAM_SIZE <= L7210_INTERNAL_SRAM_SIZE ){
	    printk("Irisfb : This Is L7210. Using Internal SRAM\n");
	    //IO_CLCDCON |= CLCDSRAM_EN; /* needs to be set after IO_CLCDBAR0 ? */
	    irisfb_using_internal_sram = 1;
	    vram_base_phys = (u_char*)SRAM_START;
	    vram_base = (u_char *)__ioremap((u_long)vram_base_phys,
					    ALLOCATED_FB_MEM_SIZE,
					    L_PTE_PRESENT  |
					    L_PTE_YOUNG    |
					    L_PTE_DIRTY    |
					    L_PTE_WRITE);
	    printk("Irisfb : vram_base 0x%p (phys=0x%p)\n",vram_base,vram_base_phys);
	    return 0;
	  }else{
	    printk("Irisfb : This Is L7210. But Internal SRAM Too Small\n");
	    irisfb_using_internal_sram = 0;
	  }
	}else{
	  printk("Irisfb : This is Not L7210 device (%x)\n",l72xx_read_sysid());
	  irisfb_using_internal_sram = 0;
	}

        if ((allocated_region = 
             (char *)__get_free_pages(GFP_KERNEL | GFP_DMA, order)) == NULL) {
		return -ENOMEM;
	}

        vram_base = (u_char *)allocated_region + (extra_pages << PAGE_SHIFT); 
        vram_base_phys = (u_char *)__virt_to_phys((u_long)vram_base);

	for (;extra_pages;extra_pages--) {
        	free_page((u_int)allocated_region +
			  ((extra_pages-1) << PAGE_SHIFT));
	}

	for (page = virt_to_page(vram_base); 
	     page < virt_to_page(vram_base + ALLOCATED_FB_MEM_SIZE); page++) {
		mem_map_reserve(page);
	}

	vram_base = (u_char *)__ioremap((u_long)vram_base_phys,
#ifdef CLEAR_BUFFER_BEFORE_LCDOFF
					( ALLOCATED_FB_MEM_SIZE << 1),
#else /* ! CLEAR_BUFFER_BEFORE_LCDOFF */
					ALLOCATED_FB_MEM_SIZE,
#endif / * ! CLEAR_BUFFER_BEFORE_LCDOFF */
					L_PTE_PRESENT  |
					L_PTE_YOUNG    |
					L_PTE_DIRTY    |
					L_PTE_WRITE);

#ifdef CLEAR_BUFFER_BEFORE_LCDOFF
	//memset(vram_base+ALLOCATED_FB_MEM_SIZE,0xff,ALLOCATED_FB_MEM_SIZE);
	memset(vram_base+ALLOCATED_FB_MEM_SIZE,0x0,ALLOCATED_FB_MEM_SIZE);
#endif / * ! CLEAR_BUFFER_BEFORE_LCDOFF */

	return (vram_base == NULL ? -EINVAL : 0);
}

#define LCDTFT		1

static void iris_fpga_on(void)
{
  /* VSHD (3.3V) Power Up (GPO4) and wait 5ms */
  FPGA_GPO |= (1<<4);
  mdelay(6); /* mdelay(5); */
	
  /* VSHA (5V) Power Up (GPO3) and wait 40ms */
  FPGA_GPO |= (1<<3);
  mdelay(50); /* mdelay(40); */

  /* Discharge Open (GPO5) */
  FPGA_GPO |= (1<<5);

	/* Charge Pump supply ON (GPO6) */
  FPGA_GPO |= (1<<6);

  /* LCD Controller Power ON */
  IO_CLCDCON |= (LCDTFT<<4);
  IO_CLCDCON |= CLCDEN;
  IO_CLCDCON |= CLCDPWR;

#if 0
  /* Wait 100ms+2*VSYNC=128 or more */
  mdelay(128);

  /* set FPGA_PANEL */
  FPGA_PANEL |= (0x03)<<16;
#endif

  /*  Panel signals Active (LCDEN of PANEL register) */
  FPGA_PANEL |= (1<<2)<<16;

  mdelay(50); /* mdelay(40); */

  /* MOD signal High (GPO7) */
  FPGA_GPO |= (1<<7);

}

static void iris_fpga_off(void)
{
  /* Charge Pump supply OFF (GPO6) */
  FPGA_GPO &= ~(1<<6);

  /* MOD signal Low (GPO7) */
  FPGA_GPO &= ~(1<<7);

  /* Wait 500ms */
  mdelay(500);

  /* VSHA (5V) Power Down (GPO3) */
  FPGA_GPO &= ~(1<<3);

  /* Discharge Close (GPO5) */
  FPGA_GPO &= ~(1<<5);

  /* Wait 200ms */
  mdelay(200);

  /*  Panel signals All-Low (LCDEN of PANEL register) */
  FPGA_PANEL &= ~((1<<2)<<16);

  /* LCD Controller Power ON */
  IO_CLCDCON &= ~CLCDPWR;
  IO_CLCDCON &= ~CLCDEN;
  IO_CLCDCON &= ~(LCDTFT<<4);

  /* VSHD (3.3V) Power Down (GPO4) */
  FPGA_GPO &= ~(1<<4);

}

#ifdef CONFIG_PM

static int lcd_controller_enabled = 0;

static void irisfb_disable_lcd_controller(void)
{
  if( ! lcd_controller_enabled ) return;

#ifdef CLEAR_BUFFER_BEFORE_LCDOFF
  memcpy(vram_base+ALLOCATED_FB_MEM_SIZE,vram_base,ALLOCATED_FB_MEM_SIZE);
  memset(vram_base,0xff,ALLOCATED_FB_MEM_SIZE);
  //IO_CLCDBAR0= vram_base_phys + ALLOCATED_FB_MEM_SIZE;
#endif / * ! CLEAR_BUFFER_BEFORE_LCDOFF */

	/* Turn off Color LCD power */
        //IO_CLCDCON &= ~CLCDPWR;   /* this is moved to fpga_off */

	/* Delay 20ms */
	mdelay(20);

	iris_fpga_off();

	/* Disable Color LCD */
	IO_CLCDCON &= ~CLCDEN;

#ifdef CLEAR_BUFFER_BEFORE_LCDOFF
	IO_CLCDBAR0= vram_base_phys;
#endif / * ! CLEAR_BUFFER_BEFORE_LCDOFF */

  lcd_controller_enabled = 0;

}
#endif

extern unsigned int l72xx_read_pll(void);

#define PLLMUX_3M6  0x00
#define PLLMUX_18M  0x05
#define PLLMUX_36M  0x0a
#define PLLMUX_73M  0x14
#define PLLMUX_129M 0x23
#define PLLMUX_140M 0x26
#define PLLMUX_147M 0x28

static struct {
  int pll;
  int hfp;
  int hsw;
  int hbp;
  int pcd;
} lcdc_settings[] = {
  {
    pll : PLLMUX_18M, /* PLL 18MHz */
    hfp : 109, /* 59, */
    hsw : 59, /* 109, */
    hbp : 59,
    pcd : 3 
  },
  {
    pll : PLLMUX_36M, /* PLL 36MHz */
    hfp : 219, /* 119, */
    hsw : 119, /* 219, */
    hbp : 119,
    pcd : 8
  },
  {
    pll : PLLMUX_129M,  /* PLL 129MHz */
    hfp : 251, /* 64, */
    hsw : 215, /* 251, */
    hbp : 215,
    pcd : 16
  },
  {
    pll : PLLMUX_140M,  /* PLL 140MHz */
    hfp : 239,
    hsw : 239,
    hbp : 239,
    pcd : 18
  },
  {
    pll : PLLMUX_147M,  /* PLL 147MHz */
    hfp : 251,
    hsw : 251,
    hbp : 251,
    pcd : 19
  },
  { -1 , 0 , 0 , 0 , 0 } /* terminator */
};


static void irisfb_enable_lcd_controller(void)
{
  int hfp;
  int hsw;
  int hbp;
  int pcd;

#ifdef CONFIG_PM
	if( lcd_controller_enabled ) return;
#endif /* CONFIG_PM */

	/* Turn off Color LCD power before config */
	IO_CLCDCON &= ~CLCDPWR;

	/* Delay 20ms */
	mdelay(20);

	iris_fpga_off();

	/* Disable Color LCD before config */
	IO_CLCDCON &= ~CLCDEN;

	/* set control register */
	IO_CLCDCON &= (CLCDEN | CLCDPWR);
	IO_CLCDCON |= (0x03 << 1); /* 16bpp */

	/* set DMA Base Address register */
	IO_CLCDBAR0= vram_base_phys;
	IO_CLCDBAR1 = 0;
	
	/* set interrupt mask */
	IO_CLCDMASK = 0;

#define	PIXELS_PER_LINE	176
#define	NUM_LINES	220	
#define LCDMUX		1
#define LCDLOWMUX	0
#define PPL		(PIXELS_PER_LINE / 16 -1)
#define HSW		251
#define HFP		251
#define HBP		251
#define LPS		(NUM_LINES - 1)  
#define VSW		0
#define VFP		0
#define VBP		11
#define PCD		19
#define ACB		0
#define IVS		0
#define IHS		0
#define IPC		0
#define IOE		0
#define CPL		(PIXELS_PER_LINE - 1)
#define BCD		0

	{
	  int which = 4;
	  switch( l72xx_read_pll() ){
	  case PLLMUX_18M:
	    printk("Irisfb: PLL is 18M\n");
	    which = 0;
	    break;
	  case PLLMUX_36M:
	    printk("Irisfb: PLL is 36M\n");
	    which = 1;
	    break;
	  case PLLMUX_129M:
	    printk("Irisfb: PLL is 129M\n");
	    which = 2;
	    break;
	  case PLLMUX_140M:
	    printk("Irisfb: PLL is 140M\n");
	    which = 3;
	    break;
	  case PLLMUX_147M:
	    printk("Irisfb: PLL is 147M\n");
	  default:
	    break;
	  }
	  hfp = lcdc_settings[which].hfp;
	  hsw = lcdc_settings[which].hsw;
	  hbp = lcdc_settings[which].hbp;
	  pcd = lcdc_settings[which].pcd;
	}

	/* set clock divisor and clocks per line */
	IO_CLCDTIMING0 = (PPL << 2) | (hsw << 8) | (hfp <<16) | (hbp <<24);
	IO_CLCDTIMING1 = LPS | (VSW <<10) | (VFP << 16) | (VBP << 24);
	IO_CLCDTIMING2 = pcd | (ACB << 6) | (IVS<<11) | (IHS<<12) |
	  (IPC<<13) | (IOE<<14) | (CPL << 16) | (BCD<<26) ;
	IO_CLCDTIMING3 = 0; // not use CLLE

	//IO_CLCDCON |= CLCDEN | (LCDTFT<<4) | (LCDMUX<<23) | (LCDLOWMUX<<24) ;
	IO_CLCDCON |= (LCDMUX<<23) | (LCDLOWMUX<<24) ;

	/* Delay 20ms */
	mdelay(20);

	/* Turn on Color LCD power before config */
	/* this is moved to iris_fpga_on */
	//IO_CLCDCON |= CLCDPWR;

	iris_fpga_on();


	if( irisfb_using_internal_sram ){
	  printk("Irisfb : Changing to SRAM\n");
	  IO_CLCDCON |= CLCDSRAM_EN; /* SRAM - VRAM */
	}else{
	  printk("Irisfb : Changing to SDRAM\n");
	  IO_CLCDCON &= ~(CLCDSRAM_EN); /* SDRAM - VRAM */
	}
	{
	  unsigned long bar;
	  bar = IO_CLCDBAR0;
	  printk("Irisfb : IO_CLCDBAR0 = 0x%lx\n",bar);
	}


#ifdef CONFIG_PM
	lcd_controller_enabled = 1;
#endif /* CONFIG_PM */

}

static void do_welcome_screen(void)
{
  //memset(vram_base,0xff,ALLOCATED_FB_MEM_SIZE);
  memcpy(vram_base,FLASH2_BASE+0x190,ALLOCATED_FB_MEM_SIZE);
}


#ifdef CONFIG_PM
static int irisfb_pm_callback(struct pm_dev* pm_dev,
			      pm_request_t req, void* data)
{
	switch (req) {
	case PM_BLANK:
	case PM_STANDBY:
	case PM_SUSPEND:
		irisfb_disable_lcd_controller();
		break;
	case PM_UNBLANK:
	case PM_RESUME:
		irisfb_enable_lcd_controller();
#ifdef CLEAR_BUFFER_BEFORE_LCDOFF
	memcpy(vram_base,vram_base+ALLOCATED_FB_MEM_SIZE,ALLOCATED_FB_MEM_SIZE);
#endif / * ! CLEAR_BUFFER_BEFORE_LCDOFF */
		break;
	}

	return 0;
}
#endif

// called by init_module()

int __init irisfb_init(void)
{
  int result;
  
  if (irisfb_map_video_memory() < 0)
    return -EINVAL;

  // set global variables
  videomemorysize = VIDEORAM_SIZE;

  //  printk("Frame Buffer Address[%08x],size[%08x]\r\n",FB,VIDEORAM_SIZE);

  strcpy(irisfb_info.modename, irisfb_name);
  irisfb_info.changevar = NULL;
  irisfb_info.node = -1;
  irisfb_info.fbops = &irisfb_ops;
  irisfb_info.disp = &disp;
  irisfb_info.switch_con = &irisfb_fbcon_switch;
  irisfb_info.updatevar = &irisfb_fbcon_updatevar;
  irisfb_info.blank = &irisfb_fbcon_blank;
  irisfb_info.flags = FBINFO_FLAG_DEFAULT;

  irisfb_set_var(&irisfb_var_default, -1, &irisfb_info);

  //  set_recommended_lut_values();

  //printk("registering framebuffer\n");

  result = register_framebuffer(&irisfb_info);
  if (result < 0) {
    printk(KERN_INFO "FAIL: result %d registering frame buffer\n", result);
    return -EINVAL;
  }

  //printk("registered framebuffer\n");

  do_welcome_screen();

#ifdef CONFIG_PM
	fb_pm_dev = pm_register(PM_SYS_DEV, 0, irisfb_pm_callback);
#endif
	irisfb_enable_lcd_controller();

  printk(KERN_INFO "fb%d: %s frame buffer device\n",
	 GET_FB_IDX(irisfb_info.node), irisfb_info.modename);

  printk(KERN_INFO "%d x %d, %d bpp, %ldK of video memory\n",
	 irisfb_var_default.xres, irisfb_var_default.yres, irisfb_var_default.bits_per_pixel,
	 videomemorysize>>10);

  return 0;
}

// called by register_framebuffer() (fbmem.c)
int __init irisfb_setup(char *options)
{
  char *this_opt;

  irisfb_info.fontname[0] = '\0';

  if (!options || !*options)
    return 0;

  // strtok: Tokenからfont名の切り出し、optionの内容は変更される
  for (this_opt = strtok(options, ","); this_opt; this_opt = strtok(NULL, ",")) {
    if (!strncmp(this_opt, "font:", 5))
      strcpy(irisfb_info.fontname, this_opt+5);
  }

  return 0;
}

// called by cleanup_module()
void irisfb_cleanup(void)
{
  unregister_framebuffer(&irisfb_info);
}

// --------------------------------------------

#ifdef MODULE
int init_module(void)
{
  return irisfb_init();
}

void cleanup_module(void)
{
  irisfb_cleanup();
}
#endif /* MODULE */
