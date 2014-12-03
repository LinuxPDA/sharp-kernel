/*
 * linux/drivers/video/epson1356fb.c -- Epson 1356 LCD Controller Frame Buffer Device
 *
 *  Copyright (C) 2001 MIT
 *
 * Edited from sa1100fb.c
 *  Copyright (C) 1999 Eric A. Thomas
 *  
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
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
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/wrapper.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/proc/pgtable.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>

/*
 *  Debug macros 
 */
#define DEBUG 1
#ifdef DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

#define REGISTER_OFFSET	((unsigned char *) 0xf0000000) /* SED1356 registers */
#define DISP_MEM_OFFSET	((unsigned char *) 0xf1000000) /* display buffer */
#define DISP_MEM_OFFSET_PHYS ((unsigned char *)0x48200000) /* display buffer */

#define MAX_PALETTE_NUM_ENTRIES		256
#define ALLOCATED_FB_MEM_SIZE 0x80000 /* display memory size (512kb) */

#define EPSON1356_PALETTE_MODE_VAL(bpp)    (((bpp) & 0x018) << 9)

/* Minimum X and Y resolutions */
#define MIN_XRES	64
#define MIN_YRES	64

/* Possible controller_state modes */
#define LCD_MODE_DISABLED		0    // Controller is disabled and Disable Done received
#define LCD_MODE_DISABLE_BEFORE_ENABLE	1    // Re-enable after Disable Done IRQ is received
#define LCD_MODE_ENABLED		2    // Controller is enabled

#define EPSON1356_NAME	"EPSON1356"
#define NR_MONTYPES	1

static u_char *VideoMemRegion;

/* Local LCD controller parameters */
/* Several duplicates exist in the two structures. */
struct epson1356fb_par {
	u_short		*v_palette_base;
	unsigned int	palette_size;
	unsigned int	max_xres;
	unsigned int	max_yres;
	unsigned int	xres;
	unsigned int	yres;
	unsigned int	xres_virtual;
	unsigned int	yres_virtual;
	unsigned int	max_bpp;
	unsigned int	bits_per_pixel;
	unsigned int	currcon;
	unsigned int	visual;
	unsigned int	allow_modeset : 1;
	unsigned int	active_lcd : 1;
	volatile u_char	controller_state;
};

/* Fake monspecs to fill in fbinfo structure */
static struct fb_monspecs monspecs __initdata = {
	 30000, 70000, 50, 65, 0 	/* Generic */
};

static struct display global_disp;	/* Initial (default) Display Settings */
static struct fb_info fb_info;
static struct epson1356fb_par current_par;
static struct fb_var_screeninfo __initdata init_var = {};

static int  epson1356fb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info);
static int  epson1356fb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info);
static int  epson1356fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info);
static int  epson1356fb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info);
static int  epson1356fb_set_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info);
 
static int  epson1356fb_switch(int con, struct fb_info *info);
static void epson1356fb_blank(int blank, struct fb_info *info);
static int  epson1356fb_activate_var(struct fb_var_screeninfo *var);
static void epson1356fb_enable_lcd_controller(void);
static void epson1356fb_disable_lcd_controller(void);

static struct fb_ops epson1356fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	epson1356fb_get_fix,
	fb_get_var:	epson1356fb_get_var,
	fb_set_var:	epson1356fb_set_var,
	fb_get_cmap:	epson1356fb_get_cmap,
	fb_set_cmap:	epson1356fb_set_cmap,
};
/* ** The SED1356 lookup table used only the upper four bits.  */
unsigned char LUT8[256*3] = {
/* Primary and secondary colors */
0x00, 0x00, 0x00,  0x00, 0x00, 0xA0,  0x00, 0xA0, 0x00,  0x00, 0xA0, 0xA0,  
0xA0, 0x00, 0x00,  0xA0, 0x00, 0xA0,  0xA0, 0xA0, 0x00,  0xA0, 0xA0, 0xA0,  
0x50, 0x50, 0x50,  0x00, 0x00, 0xF0,  0x00, 0xF0, 0x00,  0x00, 0xF0, 0xF0,  
0xF0, 0x00, 0x00,  0xF0, 0x00, 0xF0,  0xF0, 0xF0, 0x00,  0xF0, 0xF0, 0xF0,  

/* Gray shades */
0x00, 0x00, 0x00,  0x10, 0x10, 0x10,  0x20, 0x20, 0x20,  0x30, 0x30, 0x30,  
0x40, 0x40, 0x40,  0x50, 0x50, 0x50,  0x60, 0x60, 0x60,  0x70, 0x70, 0x70,  
0x80, 0x80, 0x80,  0x90, 0x90, 0x90,  0xA0, 0xA0, 0xA0,  0xB0, 0xB0, 0xB0,  
0xC0, 0xC0, 0xC0,  0xD0, 0xD0, 0xD0,  0xE0, 0xE0, 0xE0,  0xF0, 0xF0, 0xF0,  

/* Black to red */
0x00, 0x00, 0x00,  0x10, 0x00, 0x00,  0x20, 0x00, 0x00,  0x30, 0x00, 0x00,  
0x40, 0x00, 0x00,  0x50, 0x00, 0x00,  0x60, 0x00, 0x00,  0x70, 0x00, 0x00,  
0x80, 0x00, 0x00,  0x90, 0x00, 0x00,  0xA0, 0x00, 0x00,  0xB0, 0x00, 0x00,  
0xC0, 0x00, 0x00,  0xD0, 0x00, 0x00,  0xE0, 0x00, 0x00,  0xF0, 0x00, 0x00,  

/* Black to green */
0x00, 0x00, 0x00,  0x00, 0x10, 0x00,  0x00, 0x20, 0x00,  0x00, 0x30, 0x00,  
0x00, 0x40, 0x00,  0x00, 0x50, 0x00,  0x00, 0x60, 0x00,  0x00, 0x70, 0x00,  
0x00, 0x80, 0x00,  0x00, 0x90, 0x00,  0x00, 0xA0, 0x00,  0x00, 0xB0, 0x00,  
0x00, 0xC0, 0x00,  0x00, 0xD0, 0x00,  0x00, 0xE0, 0x00,  0x00, 0xF0, 0x00,  

/* Black to blue */
0x00, 0x00, 0x00,  0x00, 0x00, 0x10,  0x00, 0x00, 0x20,  0x00, 0x00, 0x30,  
0x00, 0x00, 0x40,  0x00, 0x00, 0x50,  0x00, 0x00, 0x60,  0x00, 0x00, 0x70,  
0x00, 0x00, 0x80,  0x00, 0x00, 0x90,  0x00, 0x00, 0xA0,  0x00, 0x00, 0xB0,  
0x00, 0x00, 0xC0,  0x00, 0x00, 0xD0,  0x00, 0x00, 0xE0,  0x00, 0x00, 0xF0,

/* Blue to cyan (blue and green) */
0x00, 0x00, 0xF0,  0x00, 0x10, 0xF0,  0x00, 0x20, 0xF0,  0x00, 0x30, 0xF0,  
0x00, 0x40, 0xF0,  0x00, 0x50, 0xF0,  0x00, 0x60, 0xF0,  0x00, 0x70, 0xF0,  
0x00, 0x80, 0xF0,  0x00, 0x90, 0xF0,  0x00, 0xA0, 0xF0,  0x00, 0xB0, 0xF0,  
0x00, 0xC0, 0xF0,  0x00, 0xD0, 0xF0,  0x00, 0xE0, 0xF0,  0x00, 0xF0, 0xF0,

/* Cyan (blue and green) to green */
0x00, 0xF0, 0xF0,  0x00, 0xF0, 0xE0,  0x00, 0xF0, 0xD0,  0x00, 0xF0, 0xC0,  
0x00, 0xF0, 0xB0,  0x00, 0xF0, 0xA0,  0x00, 0xF0, 0x90,  0x00, 0xF0, 0x80,  
0x00, 0xF0, 0x70,  0x00, 0xF0, 0x60,  0x00, 0xF0, 0x50,  0x00, 0xF0, 0x40,  
0x00, 0xF0, 0x30,  0x00, 0xF0, 0x20,  0x00, 0xF0, 0x10,  0x00, 0xF0, 0x00,  

/* Green to yellow (red and green) */
0x00, 0xF0, 0x00,  0x10, 0xF0, 0x00,  0x20, 0xF0, 0x00,  0x30, 0xF0, 0x00,  
0x40, 0xF0, 0x00,  0x50, 0xF0, 0x00,  0x60, 0xF0, 0x00,  0x70, 0xF0, 0x00,  
0x80, 0xF0, 0x00,  0x90, 0xF0, 0x00,  0xA0, 0xF0, 0x00,  0xB0, 0xF0, 0x00,  
0xC0, 0xF0, 0x00,  0xD0, 0xF0, 0x00,  0xE0, 0xF0, 0x00,  0xF0, 0xF0, 0x00,  

/* Yellow (red and green) to red */
0xF0, 0xF0, 0x00,  0xF0, 0xE0, 0x00,  0xF0, 0xD0, 0x00,  0xF0, 0xC0, 0x00,  
0xF0, 0xB0, 0x00,  0xF0, 0xA0, 0x00,  0xF0, 0x90, 0x00,  0xF0, 0x80, 0x00,  
0xF0, 0x70, 0x00,  0xF0, 0x60, 0x00,  0xF0, 0x50, 0x00,  0xF0, 0x40, 0x00,  
0xF0, 0x30, 0x00,  0xF0, 0x20, 0x00,  0xF0, 0x10, 0x00,  0xF0, 0x00, 0x00,  

/* Red to magenta (blue and red) */
0xF0, 0x00, 0x00,  0xF0, 0x00, 0x10,  0xF0, 0x00, 0x20,  0xF0, 0x00, 0x30,  
0xF0, 0x00, 0x40,  0xF0, 0x00, 0x50,  0xF0, 0x00, 0x60,  0xF0, 0x00, 0x70,  
0xF0, 0x00, 0x80,  0xF0, 0x00, 0x90,  0xF0, 0x00, 0xA0,  0xF0, 0x00, 0xB0,  
0xF0, 0x00, 0xC0,  0xF0, 0x00, 0xD0,  0xF0, 0x00, 0xE0,  0xF0, 0x00, 0xF0,  

/* Magenta (blue and red) to blue */
0xF0, 0x00, 0xF0,  0xE0, 0x00, 0xF0,  0xD0, 0x00, 0xF0,  0xC0, 0x00, 0xF0,  
0xB0, 0x00, 0xF0,  0xA0, 0x00, 0xF0,  0x90, 0x00, 0xF0,  0x80, 0x00, 0xF0,  
0x70, 0x00, 0xF0,  0x60, 0x00, 0xF0,  0x50, 0x00, 0xF0,  0x40, 0x00, 0xF0,  
0x30, 0x00, 0xF0,  0x20, 0x00, 0xF0,  0x10, 0x00, 0xF0,  0x00, 0x00, 0xF0,

/* Black to magenta (blue and red) */
0x00, 0x00, 0x00,  0x10, 0x00, 0x10,  0x20, 0x00, 0x20,  0x30, 0x00, 0x30,  
0x40, 0x00, 0x40,  0x50, 0x00, 0x50,  0x60, 0x00, 0x60,  0x70, 0x00, 0x70,  
0x80, 0x00, 0x80,  0x90, 0x00, 0x90,  0xA0, 0x00, 0xA0,  0xB0, 0x00, 0xB0,  
0xC0, 0x00, 0xC0,  0xD0, 0x00, 0xD0,  0xE0, 0x00, 0xE0,  0xF0, 0x00, 0xF0,  

/* Black to cyan (blue and green) */
0x00, 0x00, 0x00,  0x00, 0x10, 0x10,  0x00, 0x20, 0x20,  0x00, 0x30, 0x30,  
0x00, 0x40, 0x40,  0x00, 0x50, 0x50,  0x00, 0x60, 0x60,  0x00, 0x70, 0x70,  
0x00, 0x80, 0x80,  0x00, 0x90, 0x90,  0x00, 0xA0, 0xA0,  0x00, 0xB0, 0xB0,  
0x00, 0xC0, 0xC0,  0x00, 0xD0, 0xD0,  0x00, 0xE0, 0xE0,  0x00, 0xF0, 0xF0,  

/* Red to white */
0xF0, 0x00, 0x00,  0xF0, 0x10, 0x10,  0xF0, 0x20, 0x20,  0xF0, 0x30, 0x30,  
0xF0, 0x40, 0x40,  0xF0, 0x50, 0x50,  0xF0, 0x60, 0x60,  0xF0, 0x70, 0x70,  
0xF0, 0x80, 0x80,  0xF0, 0x90, 0x90,  0xF0, 0xA0, 0xA0,  0xF0, 0xB0, 0xB0,  
0xF0, 0xC0, 0xC0,  0xF0, 0xD0, 0xD0,  0xF0, 0xE0, 0xE0,  0xF0, 0xF0, 0xF0,  

/* Green to white */
0x00, 0xF0, 0x00,  0x10, 0xF0, 0x10,  0x20, 0xF0, 0x20,  0x30, 0xF0, 0x30,  
0x40, 0xF0, 0x40,  0x50, 0xF0, 0x50,  0x60, 0xF0, 0x60,  0x70, 0xF0, 0x70,  
0x80, 0xF0, 0x80,  0x90, 0xF0, 0x90,  0xA0, 0xF0, 0xA0,  0xB0, 0xF0, 0xB0,  
0xC0, 0xF0, 0xC0,  0xD0, 0xF0, 0xD0,  0xE0, 0xF0, 0xE0,  0xF0, 0xF0, 0xF0,  

/* Blue to white */
0x00, 0x00, 0xF0,  0x10, 0x10, 0xF0,  0x20, 0x20, 0xF0,  0x30, 0x30, 0xF0,  
0x40, 0x40, 0xF0,  0x50, 0x50, 0xF0,  0x60, 0x60, 0xF0,  0x70, 0x70, 0xF0,  
0x80, 0x80, 0xF0,  0x90, 0x90, 0xF0,  0xA0, 0xA0, 0xF0,  0xB0, 0xB0, 0xF0,  
0xC0, 0xC0, 0xF0,  0xD0, 0xD0, 0xF0,  0xE0, 0xE0, 0xF0,  0xF0, 0xF0, 0xF0
};

long idummy;

/*
 * epson1356fb_palette_write:
 *    Write palette data to the LCD frame buffer's palette area
 */
static inline void
epson1356fb_palette_write(u_int regno, u_short pal)
{
	current_par.v_palette_base[regno] = (regno ? pal : pal | 
	     EPSON1356_PALETTE_MODE_VAL(current_par.bits_per_pixel));
}


static inline u_short
epson1356fb_palette_encode(u_int regno, u_int red, u_int green, u_int blue, u_int trans)
{
	u_int pal;

	if(current_par.bits_per_pixel == 4){
		/*
		 * RGB -> luminance is defined to be
		 * Y =  0.299 * R + 0.587 * G + 0.114 * B
		 */
		pal = (19595 * red + 38470 * green + 7471 * blue) >> 28;
	}
	else{
		pal   = ((red   >>  4) & 0xf00);
		pal  |= ((green >>  8) & 0x0f0);
		pal  |= ((blue  >> 12) & 0x00f);
	}
	return pal;
}
	    
static inline u_short
epson1356fb_palette_read(u_int regno)
{
	return (current_par.v_palette_base[regno] & 0x0FFF);
}

static void
epson1356fb_palette_decode(u_int regno, u_int *red, u_int *green, u_int *blue, u_int *trans)
{
	u_short pal;

	pal = epson1356fb_palette_read(regno);
	if( current_par.bits_per_pixel == 4){
		pal &= 0x000f;
		pal |= pal << 4;
		pal |= pal << 8;
		*blue = *green = *red = pal;
	}
	else{
		*blue   = (pal & 0x000f) << 12;
		*green  = (pal & 0x00f0) << 8;
		*red    = (pal & 0x0f00) << 4;
	}
    *trans  = 0;
}

static int
epson1356fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue, u_int *trans, struct fb_info *info)
{
	if (regno >= current_par.palette_size)
		return 1;
	epson1356fb_palette_decode(regno, red, green, blue, trans);
	return 0;
}

static int
epson1356fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue, u_int trans, struct fb_info *info)
{
	u_short pal;

	if (regno >= current_par.palette_size)
		return 1;
	pal = epson1356fb_palette_encode(regno, red, green, blue, trans);
	epson1356fb_palette_write(regno, pal);
	return 0;
}

static int
epson1356fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
		 struct fb_info *info)
{
	int err = 0;

        DPRINTK("current_par.visual=%d\n", current_par.visual);
	if (con == current_par.currcon)
		err = fb_get_cmap(cmap, kspc, epson1356fb_getcolreg, info);
	else if (fb_display[con].cmap.len)
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(current_par.palette_size),
			     cmap, kspc ? 0 : 2);
	return err;
}

static int
epson1356fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		  struct fb_info *info)
{
	int err = 0;

        DPRINTK("current_par.visual=%d\n", current_par.visual);
	if (!fb_display[con].cmap.len)
		err = fb_alloc_cmap(&fb_display[con].cmap,
				    current_par.palette_size, 0);
	if (!err) {
		if (con == current_par.currcon)
			err = fb_set_cmap(cmap, kspc, epson1356fb_setcolreg,
					  info);
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	}
	return err;
}

static void inline
epson1356fb_get_par(struct epson1356fb_par *par)
{
	*par = current_par;
}


/*
 * epson1356fb_encode_var():
 * 	Modify var structure using values in par
 */
static int 
epson1356fb_encode_var(struct fb_var_screeninfo *var,
                    struct epson1356fb_par *par)
{
        // Don't know if really want to zero var on entry.
        // Look at set_var to see.  If so, may need to add extra params to par     
//	memset(var, 0, sizeof(struct fb_var_screeninfo));
 
	var->xres = par->xres;
	var->yres = par->yres;
	var->xres_virtual = par->xres_virtual;
	var->yres_virtual = par->yres_virtual;

	var->bits_per_pixel = par->bits_per_pixel;

        DPRINTK("var->bits_per_pixel=%d\n", var->bits_per_pixel);
	switch(var->bits_per_pixel) {
	case 2:
	case 4:
	case 8:
		var->red.length	   = 4;
		var->green         = var->red;
		var->blue          = var->red;
		var->transp.length = 0;
		break;
	case 12:          // This case should differ for Active/Passive mode  
	case 16:
        var->red.length    = 5;
        var->blue.length   = 5;
        var->green.length  = 6;
        var->transp.length = 0;
        var->red.offset    = 11;
        var->green.offset  = 5;
        var->blue.offset   = 0;
        var->transp.offset = 0;
        break;
	}
	return 0;
}
 
/*
 *  epson1356fb_decode_var():
 *    Get the video params out of 'var'. If a value doesn't fit, round it up,
 *    if it's too big, return -EINVAL.
 *
 *    Suggestion: Round up in the following order: bits_per_pixel, xres,
 *    yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *    bitfields, horizontal timing, vertical timing.
 */
static int
epson1356fb_decode_var(struct fb_var_screeninfo *var, 
                    struct epson1356fb_par *par)
{
	*par = current_par;

	if ((par->xres = var->xres) < MIN_XRES)
		par->xres = MIN_XRES; 
	if ((par->yres = var->yres) < MIN_YRES)
		par->yres = MIN_YRES;
	if (par->xres > current_par.max_xres)
		par->xres = current_par.max_xres;
	if (par->yres > current_par.max_yres)
		par->yres = current_par.max_yres; 
	par->xres_virtual = 
		var->xres_virtual < par->xres ? par->xres : var->xres_virtual;
    par->yres_virtual = 
		var->yres_virtual < par->yres ? par->yres : var->yres_virtual;
    par->bits_per_pixel = var->bits_per_pixel;

    DPRINTK("par->bits_per_pixel=%d\n", par->bits_per_pixel);
	switch (par->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
        case 4:
		par->visual = FB_VISUAL_PSEUDOCOLOR;
		par->palette_size = 16; 
                break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		par->visual = FB_VISUAL_PSEUDOCOLOR;
		par->palette_size = 256; 
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:  /* RGB 565 */
		par->visual = FB_VISUAL_TRUECOLOR;
		par->palette_size = 0; 
		break;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}

static int
epson1356fb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct epson1356fb_par par;

        DPRINTK("con=%d\n", con);
	if (con == -1) {
		epson1356fb_get_par(&par);
		epson1356fb_encode_var(var, &par);
	} else
		*var = fb_display[con].var;
	return 0;
}

/*
 * epson1356fb_set_var():
 *	Set the user defined part of the display for the specified console
 */
static int
epson1356fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct display *display;
	int err, chgvar = 0;
	struct epson1356fb_par par;

	if (con >= 0)
		display = &fb_display[con]; /* Display settings for console */
	else
		display = &global_disp;     /* Default display settings */

	/* Decode var contents into a par structure, adjusting any */
	/* out of range values. */
	if ((err = epson1356fb_decode_var(var, &par)))
		return err;
	// Store adjusted par values into var structure
	epson1356fb_encode_var(var, &par);
       
	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_TEST)
		return 0;
	else if (((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW) && 
		 ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NXTOPEN))
		return -EINVAL;

	if (con >= 0) {
		if ((display->var.xres != var->xres) ||
		    (display->var.yres != var->yres) ||
		    (display->var.xres_virtual != var->xres_virtual) ||
		    (display->var.yres_virtual != var->yres_virtual) ||
		    (display->var.sync != var->sync)                 ||
                    (display->var.bits_per_pixel != var->bits_per_pixel) ||
		    (memcmp(&display->var.red, &var->red, sizeof(var->red))) ||
		    (memcmp(&display->var.green, &var->green, sizeof(var->green))) ||
		    (memcmp(&display->var.blue, &var->blue, sizeof(var->blue)))) 
			chgvar = 1;
	}

	display->var = *var;
	display->screen_base	= (u_char *)VideoMemRegion; 
	display->visual		= par.visual;
	display->type		= FB_TYPE_PACKED_PIXELS;
	display->type_aux	= 0;
	display->ypanstep	= 0;
	display->ywrapstep	= 0;
	display->line_length	= 
	display->next_line      = (var->xres * var->bits_per_pixel) / 8;

	display->can_soft_blank	= 1;
	display->inverse	= 0;

	switch (display->var.bits_per_pixel) {
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
		break;
#endif
	default:
		display->dispsw = &fbcon_dummy;
		break;
	}

	/* If the console has changed and the console has defined */
	/* a changevar function, call that function. */
	if (chgvar && info && info->changevar)
		info->changevar(con);

        /* If the current console is selected and it's not truecolor, 
	 *  update the palette 
	 */
	if ((con == current_par.currcon) &&
	    (current_par.visual != FB_VISUAL_TRUECOLOR)) {
		struct fb_cmap *cmap;
		
		current_par = par;
		if (display->cmap.len)
			cmap = &display->cmap;
		else
			cmap = fb_default_cmap(current_par.palette_size);

		fb_set_cmap(cmap, 1, epson1356fb_setcolreg, info);
	}

	/* If the current console is selected, activate the new var. */
	if (con == current_par.currcon)
		epson1356fb_activate_var(var);
	
	return 0;
}

static int
epson1356fb_updatevar(int con, struct fb_info *info)
{
	DPRINTK("entered\n");
	return 0;
}

static int
epson1356fb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct display *display;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, EPSON1356_NAME);

	if (con >= 0) {
		DPRINTK("Using console specific display for con=%d\n",con);
		display = &fb_display[con];  /* Display settings for console */
	}
	else
		display = &global_disp;      /* Default display settings */

	fix->smem_start	 = (unsigned long)DISP_MEM_OFFSET_PHYS; 
	fix->smem_len	 =
		(current_par.max_xres * current_par.max_yres * current_par.max_bpp)/8;
	fix->type	 = display->type;
	fix->type_aux	 = display->type_aux;
	fix->xpanstep	 = 0;
	fix->ypanstep	 = display->ypanstep;
	fix->ywrapstep	 = display->ywrapstep;
	fix->visual	 = display->visual;
	fix->line_length = display->line_length;
	fix->accel	 = FB_ACCEL_NONE;
	return 0;
}

/*
 * epson1356fb_activate_var():
 *	Configures LCD Controller based on entries in var parameter.  Settings are      
 *      only written to the controller if changes were made.  
 */
static int
epson1356fb_activate_var(struct fb_var_screeninfo *var)
{						       
unsigned char * pRegs = REGISTER_OFFSET;
unsigned char * pMem  = DISP_MEM_OFFSET;
unsigned char * pLUT  = LUT8;
long lCnt;
int rgb;
int i;
static int initialized = 0;

#if 0
void jornada_contrast(int arg_contrast);
#else
/* where is this function defined? */
#define jornada_contrast(x)
#endif

    DPRINTK("Configuring  Jornada LCD\n");

	DPRINTK("Configuring xres = %d, yres = %d\n",var->xres, var->yres);
	if (initialized)
  		return 0;
	initialized = 1;
	PPSR &= ~(PPC_LDD0 | PPC_LDD1 | PPC_LDD2);
	PPDR |= PPC_LDD0 | PPC_LDD1 | PPC_LDD2;
	LCCR3 = 0;
	LCCR2 = 0;
	LCCR1 = 0;
	LCCR0 = 0;
	DBAR1 = 0;
	DBAR2 = 0;

	/* Enable access to SED1356 by setting memory/register select bit to 0.  */
	*(pRegs + 0x001) = 0;

	/* Disable display outputs during initialization) */
	*(pRegs + 0x1FC) = 0;

	/* Set the  GPIOs to input. Should GPIO bits in register [004] get switched
	then the GPIO outputs, according to register [008], are driven low.  */
	*(pRegs + 0x004) = 0;
	*(pRegs + 0x008) = 0;

	/* Program the LCD panel type and panel timing registers.
	 *	The horizontal and vertical non-display times have been
	 *	calculated for a 78 Hz frame rated.
	 *						         LCD PCLK
	 *			FrameRate = -----------------------------
	 *						(HDP + HNDP) * (VDP/2 + VNDP)
	 *
	 *						         20,000,000
	 *			          = -----------------------------
	 *						(640 + 256) * (480 / 2 + 45)
	 *
	 *			          = 78 Hz */

    pRegs[0x10] = 0x1;   /* Memory Clock Configuration Register */
    pRegs[0x14] = 0x11;   /* LCD Pixel Clock Configuration Register */
    pRegs[0x18] = 0x1;   /* CRT/TV Pixel Clock Configuration Register */
    pRegs[0x1c] = 0x1;   /* MediaPlug Clock Configuration Register */
    pRegs[0x1e] = 0x1;   /* CPU To Memory Wait State Select Register */
    pRegs[0x20] = 0;   /* Memory Configuration Register */
    pRegs[0x21] = 0x45;   /* DRAM Refresh Rate Register */
    pRegs[0x2a] = 0x1;   /* DRAM Timings Control Register 0 */
    pRegs[0x2b] = 0x1;   /* DRAM Timings Control Register 1 */
    pRegs[0x30] = 0x1c;   /* Panel Type Register */
    pRegs[0x31] = 0;   /* MOD Rate Register */
    pRegs[0x32] = 0x4f;   /* LCD Horizontal Display Width Register */
    pRegs[0x34] = 0x7;   /* LCD Horizontal Non-Display Period Register */
    pRegs[0x35] = 0x1;   /* TFT FPLINE Start Position Register */
    pRegs[0x36] = 0xb;   /* TFT FPLINE Pulse Width Register */
    pRegs[0x38] = 0xef;   /* LCD Vertical Display Height Register 0 */
    pRegs[0x39] = 0;   /* LCD Vertical Display Height Register 1 */
    pRegs[0x3a] = 0x13;   /* LCD Vertical Non-Display Period Register */
    pRegs[0x3b] = 0xb;   /* TFT FPFRAME Start Position Register */
    pRegs[0x3c] = 0x1;   /* TFT FPFRAME Pulse Width Register */
    pRegs[0x40] = 0x5;   /* LCD Display Mode Register */
    pRegs[0x41] = 0;   /* LCD Miscellaneous Register */
    pRegs[0x42] = 0;   /* LCD Display Start Address Register 0 */
    pRegs[0x43] = 0;   /* LCD Display Start Address Register 1 */
    pRegs[0x44] = 0;   /* LCD Display Start Address Register 2 */
    pRegs[0x46] = 0x80;   /* LCD Memory Address Offset Register 0 */
    pRegs[0x47] = 0x2;   /* LCD Memory Address Offset Register 1 */
    pRegs[0x48] = 0;   /* LCD Pixel Panning Register */
    pRegs[0x4a] = 0;   /* LCD Display FIFO High Threshold Control Register */
    pRegs[0x4b] = 0;   /* LCD Display FIFO Low Threshold Control Register */
    pRegs[0x50] = 0x4f;   /* CRT/TV Horizontal Display Width Register */
    pRegs[0x52] = 0x13;   /* CRT/TV Horizontal Non-Display Period Register */
    pRegs[0x53] = 0x1;   /* CRT/TV HRTC Start Position Register */
    pRegs[0x54] = 0xb;   /* CRT/TV HRTC Pulse Width Register */
    pRegs[0x56] = 0xdf;   /* CRT/TV Vertical Display Height Register 0 */
    pRegs[0x57] = 0x1;   /* CRT/TV Vertical Display Height Register 1 */
    pRegs[0x58] = 0x2b;   /* CRT/TV Vertical Non-Display Period Register */
    pRegs[0x59] = 0x9;   /* CRT/TV VRTC Start Position Register */
    pRegs[0x5a] = 0x1;   /* CRT/TV VRTC Pulse Width Register */
    pRegs[0x5b] = 0x10;   /* TV Output Control Register */
    pRegs[0x60] = 0x3;   /* CRT/TV Display Mode Register */
    pRegs[0x62] = 0;   /* CRT/TV Display Start Address Register 0 */
    pRegs[0x63] = 0;   /* CRT/TV Display Start Address Register 1 */
    pRegs[0x64] = 0;   /* CRT/TV Display Start Address Register 2 */
    pRegs[0x66] = 0x40;   /* CRT/TV Memory Address Offset Register 0 */
    pRegs[0x67] = 0x1;   /* CRT/TV Memory Address Offset Register 1 */
    pRegs[0x68] = 0;   /* CRT/TV Pixel Panning Register */
    pRegs[0x6a] = 0;   /* CRT/TV Display FIFO High Threshold Control Register */
    pRegs[0x6b] = 0;   /* CRT/TV Display FIFO Low Threshold Control Register */
    pRegs[0x70] = 0;   /* LCD Ink/Cursor Control Register */
    pRegs[0x71] = 0x1;   /* LCD Ink/Cursor Start Address Register */
    pRegs[0x72] = 0;   /* LCD Cursor X Position Register 0 */
    pRegs[0x73] = 0;   /* LCD Cursor X Position Register 1 */
    pRegs[0x74] = 0;   /* LCD Cursor Y Position Register 0 */
    pRegs[0x75] = 0;   /* LCD Cursor Y Position Register 1 */
    pRegs[0x76] = 0;   /* LCD Ink/Cursor Blue Color 0 Register */
    pRegs[0x77] = 0;   /* LCD Ink/Cursor Green Color 0 Register */
    pRegs[0x78] = 0;   /* LCD Ink/Cursor Red Color 0 Register */
    pRegs[0x7a] = 0x1f;   /* LCD Ink/Cursor Blue Color 1 Register */
    pRegs[0x7b] = 0x3f;   /* LCD Ink/Cursor Green Color 1 Register */
    pRegs[0x7c] = 0x1f;   /* LCD Ink/Cursor Red Color 1 Register */
    pRegs[0x7e] = 0;   /* LCD Ink/Cursor FIFO Threshold Register */
    pRegs[0x80] = 0;   /* CRT/TV Ink/Cursor Control Register */
    pRegs[0x81] = 0x1;   /* CRT/TV Ink/Cursor Start Address Register */
    pRegs[0x82] = 0;   /* CRT/TV Cursor X Position Register 0 */
    pRegs[0x83] = 0;   /* CRT/TV Cursor X Position Register 1 */
    pRegs[0x84] = 0;   /* CRT/TV Cursor Y Position Register 0 */
    pRegs[0x85] = 0;   /* CRT/TV Cursor Y Position Register 1 */
    pRegs[0x86] = 0;   /* CRT/TV Ink/Cursor Blue Color 0 Register */
    pRegs[0x87] = 0;   /* CRT/TV Ink/Cursor Green Color 0 Register */
    pRegs[0x88] = 0;   /* CRT/TV Ink/Cursor Red Color 0 Register */
    pRegs[0x8a] = 0x1f;   /* CRT/TV Ink/Cursor Blue Color 1 Register */
    pRegs[0x8b] = 0x3f;   /* CRT/TV Ink/Cursor Green Color 1 Register */
    pRegs[0x8c] = 0x1f;   /* CRT/TV Ink/Cursor Red Color 1 Register */
    pRegs[0x8e] = 0;   /* CRT/TV Ink/Cursor FIFO Threshold Register */

	/* Set the 2D acceleration (BitBLT) registers to a known state */
	for (i = 0x100; i <= 0x119; i++)
		if (i != 0x107 && i != 0x10b && i != 0x10e && i != 0x10f && i != 0x117)
			*(pRegs + i) = 0x00;	/* 0000 0000 */
	/* Program the look-up table to a known state.  */
	*(pRegs + 0x1E0) = 0x01;	/* Enable the LCD LUT for read/write. */
	*(pRegs + 0x1E2) = 0;		/* Reset the LUT address. */
	for (i = 0; i < 256; i++)
		for (rgb = 0; rgb < 3; rgb++)
			*(pRegs + 0x1E4) = *pLUT++;
    pRegs[0x1e4] = 0;   /* Look-Up Table Data Register */
    pRegs[0x1f0] = 0;   /* Power Save Configuration Register */
    pRegs[0x1f1] = 0;   /* Power Save Status Register */
    pRegs[0x1f4] = 0;   /* CPU-to-Memory Access Watchdog Timer Register */

	for (lCnt = 0; lCnt < ALLOCATED_FB_MEM_SIZE; lCnt++)
		*pMem++ = 0;		/* Clear display memory */

	PPSR |= PPC_LDD0;
	for (idummy = 0; idummy < 1000000; idummy++)
		; /* Wait for 100ms */
	/* Turn off power save mode.  */
	*(pRegs + 0x1F0) = 0;		/* 000 000 - Disable Power Save Mode. */
	/* Disable the watchdog timer.  */
	*(pRegs + 0x1F4) = 0;		/* 0000 0000 */
	/* Enable the display.  */
	*(pRegs + 0x1FC) = 0x01;	/* 0000 0001 - Disable Power Save Mode. */

	jornada_contrast(0x36);
	PPSR |= PPC_LDD2;
	for (idummy = 0; idummy < 1000000; idummy++)
		; /* Wait for 100ms */
#if 0
	jornada_contrast(0x19);
	jornada_contrast(0x36);
	jornada_contrast(0x50);
#endif
	jornada_contrast(0x60);
	PPSR |= PPC_LDD1;
	epson1356fb_enable_lcd_controller();
  	return 0;
}


/*
 *  epson1356fb_disable_lcd_controller():
 *    	Disables LCD controller.
 */
static void epson1356fb_disable_lcd_controller(void)
{
        DPRINTK("Disabling LCD controller\n");

	/* Exit if already LCD disabled, or LDD IRQ unmasked */
	if ((current_par.controller_state == LCD_MODE_DISABLED) ||
	    (!(LCCR0 & LCCR0_LDM))) {
		DPRINTK("LCD already disabled\n");
		return;
	}
}

/*
 *  epson1356fb_enable_lcd_controller():
 *    	Enables LCD controller.  If the controller is already enabled, it is first disabled.
 *      This forces all changes to the LCD controller registers to be done when the 
 *      controller is disabled.  Platform specific hardware enabling is also included.
 */
static void epson1356fb_enable_lcd_controller(void)
{
        /* Disable controller before changing parameters */
	if (current_par.controller_state == LCD_MODE_ENABLED) 	{
		current_par.controller_state = LCD_MODE_DISABLE_BEFORE_ENABLE;
		epson1356fb_disable_lcd_controller();
	} else {
		DPRINTK("Enabling LCD controller\n"); 
		current_par.controller_state = LCD_MODE_ENABLED; 
	}
}

void epson1356fb_light_on(int arg_on)
{
printk("[%s:%d] %s()\n", __FILE__, __LINE__, __FUNCTION__);
}

/*
 * Formal definition of the VESA spec:
 *  On
 *  	This refers to the state of the display when it is in full operation
 *  Stand-By
 *  	This defines an optional operating state of minimal power reduction with
 *  	the shortest recovery time
 *  Suspend
 *  	This refers to a level of power management in which substantial power
 *  	reduction is achieved by the display.  The display can have a longer 
 *  	recovery time from this state than from the Stand-by state
 *  Off
 *  	This indicates that the display is consuming the lowest level of power
 *  	and is non-operational. Recovery from this state may optionally require
 *  	the user to manually power on the monitor
 *
 *  Now, the fbdev driver adds an additional state, (blank), where they
 *  turn off the video (maybe by colormap tricks), but don't mess with the
 *  video itself: think of it semantically between on and Stand-By.
 *
 *  So here's what we should do in our fbdev blank routine:
 *
 *  	VESA_NO_BLANKING (mode 0)	Video on,  front/back light on
 *  	VESA_VSYNC_SUSPEND (mode 1)  	Video on,  front/back light off
 *  	VESA_HSYNC_SUSPEND (mode 2)  	Video on,  front/back light off
 *  	VESA_POWERDOWN (mode 3)		Video off, front/back light off
 *
 *  This will match the matrox implementation.
 */
/*
 * epson1356fb_blank():
 *	Blank the display by setting all palette values to zero.  Note, the 
 * 	12 and 16 bpp modes don't really use the palette, so this will not
 *      blank the display in all modes.  
 */
static void
epson1356fb_blank(int blank, struct fb_info *info)
{
	int i;
	
        DPRINTK("epson1356fb_blank: blank=%d info->modename=%s\n", blank, info->modename);
        switch (blank) {
        case VESA_POWERDOWN:
                if (current_par.visual != FB_VISUAL_TRUECOLOR)
			for (i = 0; i < current_par.palette_size; i++)
				epson1356fb_palette_write(i, epson1356fb_palette_encode(i, 0, 0, 0, 0));
		epson1356fb_disable_lcd_controller();
                epson1356fb_light_on(0);
                break;
        case VESA_NO_BLANKING: 
                epson1356fb_light_on(1);
                goto enable_lcd;
        case VESA_VSYNC_SUSPEND:
        case VESA_HSYNC_SUSPEND:
                if (current_par.visual != FB_VISUAL_TRUECOLOR)
			for (i = 0; i < current_par.palette_size; i++)
				epson1356fb_palette_write(i, epson1356fb_palette_encode(i, 0, 0, 0, 0));
		epson1356fb_disable_lcd_controller();
                epson1356fb_light_on(0);
                break;
        enable_lcd:
                if (current_par.visual != FB_VISUAL_TRUECOLOR)
			epson1356fb_set_cmap(&fb_display[current_par.currcon].cmap, 1, 
					  current_par.currcon, info); 
		epson1356fb_enable_lcd_controller();
	}
}


/*
 *  epson1356fb_switch():       
 *	Change to the specified console.  Palette and video mode
 *      are changed to the console's stored parameters. 
 */
static int
epson1356fb_switch(int con, struct fb_info *info)
{

  	DPRINTK("con=%d info->modename=%s\n", con, info->modename);
        if (current_par.visual != FB_VISUAL_TRUECOLOR) {
                struct fb_cmap *cmap;
		if (current_par.currcon >= 0) {
			// Get the colormap for the selected console 
			cmap = &fb_display[current_par.currcon].cmap;
			
			if (cmap->len)
				fb_get_cmap(cmap, 1, epson1356fb_getcolreg, info);
		}
        }

	current_par.currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;
	DPRINTK("fb_display[%d].var.activate=%x\n", con, fb_display[con].var.activate);
	epson1356fb_set_var(&fb_display[con].var, con, info);
	return 0;
}

int __init epson1356fb_init(void)
{
    struct page *page;

	strcpy(fb_info.modename, EPSON1356_NAME);
	strcpy(fb_info.fontname, "Acorn8x8");

	fb_info.node		= -1;
	fb_info.flags		= FBINFO_FLAG_DEFAULT;
	fb_info.fbops		= &epson1356fb_ops;
    fb_info.monspecs	= monspecs;
	fb_info.disp		= &global_disp;
	fb_info.changevar	= NULL;
	fb_info.switch_con	= epson1356fb_switch;
	fb_info.updatevar	= epson1356fb_updatevar;
	fb_info.blank		= epson1356fb_blank;

	/*
	 * setup initial parameters
	 */
	memset(&init_var, 0, sizeof(init_var));

	init_var.transp.length	= 0;
	init_var.nonstd		= 0;
	init_var.activate	= FB_ACTIVATE_NOW;
	init_var.xoffset	= 0;
	init_var.yoffset	= 0;
	init_var.height		= -1;
	init_var.width		= -1;
	init_var.vmode		= FB_VMODE_NONINTERLACED;

	current_par.max_xres	= 640;
	current_par.max_yres	= 240;
	current_par.max_bpp	= 16;
	init_var.red.length	= 4;
	init_var.green.length	= 4;
	init_var.blue.length	= 4;
	init_var.red.offset	= 12;
	init_var.green.offset	= 7;
	init_var.blue.offset	= 1;
	init_var.grayscale	= 0;

    current_par.v_palette_base = (u_short *)LUT8;
	current_par.palette_size	= MAX_PALETTE_NUM_ENTRIES;
	current_par.currcon		= -1;
	current_par.allow_modeset	=  1;
	current_par.controller_state	= LCD_MODE_DISABLED;

	init_var.xres			= current_par.max_xres;
	init_var.yres			= current_par.max_yres;
	init_var.xres_virtual		= init_var.xres;
	init_var.yres_virtual		= init_var.yres;
	init_var.bits_per_pixel		= current_par.max_bpp;
			
	/* Remap the fb memory to a non-buffered, non-cached region */
	VideoMemRegion = (u_char *)__ioremap((u_long)DISP_MEM_OFFSET_PHYS,
		ALLOCATED_FB_MEM_SIZE, L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY | L_PTE_WRITE);
    /* Set reserved flag for fb memory to allow it to be remapped into */
    /* user space by the common fbmem driver using remap_page_range(). */
	for(page = virt_to_page(VideoMemRegion); 
	    page < virt_to_page(VideoMemRegion + ALLOCATED_FB_MEM_SIZE); page++)
	  mem_map_reserve(page);

	if (epson1356fb_set_var(&init_var, -1, &fb_info))
		current_par.allow_modeset = 0;
	epson1356fb_decode_var(&init_var, &current_par);
	register_framebuffer(&fb_info);
	/* This driver cannot be unloaded at the moment */
	MOD_INC_USE_COUNT;
	return 0;
}

int __init epson1356fb_setup(char *options)
{
	char *this_opt;

	if (!options || !*options)
		return 0;

	for (this_opt = strtok(options, ","); this_opt;
	     this_opt = strtok(NULL, ",")) {
		if (!strncmp(this_opt, "bpp:", 4))
			current_par.max_bpp = simple_strtoul(this_opt+4, NULL, 0);
	}
	return 0;
}

