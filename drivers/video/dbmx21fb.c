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
*******************************************************************************/
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
#include <linux/selection.h>
#include <linux/console.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>

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

#include <asm/arch/hardware.h>
#include <asm/arch/platform.h>
#include <asm/arch/memory.h>

#include <asm/arch/mx2.h>
#include "dbmx21fb.h"

#undef SUP_TTY0

//#define HARDWARE_CURSOR
 #undef HARDWARE_CURSOR
#undef DEBUG

/********************************************************************************/
#ifdef DEBUG
#  define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#define FUNC_START	DPRINTK(KERN_ERR"start of %s\n", __FUNCTION__);
#define FUNC_END	DPRINTK(KERN_ERR"end of %s\n", __FUNCTION__);
#else
#  define DPRINTK(fmt, args...)
#define FUNC_START
#define FUNC_END
#endif


#define FONT_DATA ((unsigned char *)font->data)
struct fbcon_font_desc *font;

/* Local LCD controller parameters */
struct dbmx21fb_par{
	u_char          *screen_start_address;	/* Screen Start Address */
	u_char          *v_screen_start_address;/* Virtul Screen Start Address */ 
	unsigned long   screen_memory_size;	/* screen memory size */
	unsigned int    palette_size;			
	unsigned int    max_xres;
	unsigned int    max_yres;
	unsigned int    xres;
	unsigned int    yres;
	unsigned int    xres_virtual;
	unsigned int    yres_virtual;
	unsigned int    max_bpp;
	unsigned int    bits_per_pixel;
	unsigned int    currcon;
	unsigned int    visual;
	unsigned int 	TFT :1;
	unsigned int    color :1 ;
	unsigned int	sharp :1 ;
};

static u_char*	p_framebuffer_memory_address;
static u_char*	v_framebuffer_memory_address;

/* Fake monspecs to fill in fbinfo structure */
static struct fb_monspecs monspecs __initdata = {
	 30000, 70000, 50, 65, 0 	/* Generic */
};

/* color map initial */
static unsigned short __attribute__((unused)) color4map[16] = {
	0x0000, 0x000f,	0x00f0,	0x0f2a,	0x0f00,	0x0f0f,	0x0f88,	0x0ccc,
	0x0888,	0x00ff,	0x00f8,	0x0f44,	0x0fa6,	0x0f22,	0x0ff0,	0x0fff
};

static unsigned short gray4map[16] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
	0x0008, 0x0009, 0x000a, 0x000b, 0x000c,	0x000d,	0x000e,	0x000f
};

static struct display global_disp;      /* Initial (default) Display Settings */
static struct fb_info fb_info;
static struct fb_var_screeninfo init_var = {};
static struct dbmx21fb_par current_par={ };

/* Frame buffer device API */
static int  dbmx21fb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info);
static int  dbmx21fb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info);
static int  dbmx21fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info);
static int  dbmx21fb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info);
static int  dbmx21fb_set_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info);
		
/* Interface to the low level driver  */ 
static int  dbmx21fb_switch(int con, struct fb_info *info);
static void dbmx21fb_blank(int blank, struct fb_info *info);
static int dbmx21fb_updatevar(int con, struct fb_info *info);

/*  Internal routines  */
static int  _reserve_fb_memory(void);
static void _install_cmap(int con, struct fb_info *info);
static void _enable_lcd_controller(void);
static void _disable_lcd_controller(void);
static int  _encode_var(struct fb_var_screeninfo *var, struct dbmx21fb_par *par);
static int  _decode_var(struct fb_var_screeninfo *var, struct dbmx21fb_par *par);

/* initialization routines */
static void __init _init_lcd_system(void);
static int  __init _init_lcd(void);
static void __init _init_fbinfo(void);
static int  __init _reserve_fb_memory(void);

/* frame buffer ops */
static struct fb_ops dbmx21fb_ops = {
	owner:          THIS_MODULE,
	fb_get_fix:     dbmx21fb_get_fix,
	fb_get_var:     dbmx21fb_get_var,
	fb_set_var:     dbmx21fb_set_var,
	fb_get_cmap:    dbmx21fb_get_cmap,
	fb_set_cmap:    dbmx21fb_set_cmap,
};

#ifdef CONFIG_FB_DBMX21_HWCURSOR
struct display_switch dbmx21fb_dispsw;
#endif


/*****************************************************************************
 * Function Name: dbmx21fb_getcolreg()
 * 
 * Input: 	regno	: Color register ID
 *		red	: Color map red[]
 *		green	: Color map green[]
 *		blue	: Color map blue[]
 *	transparent	: Flag
 *		info	: Fb_info database
 * 
 * Value Returned: int	: Return status.If no error, return 0.
 *	 
 * Description: Transfer to fb_xxx_cmap handlers as parameters to 
 *		control color registers
 *
******************************************************************************/
#define RED	0xf800
#define GREEN	0x07e0
#define BLUE	0x001f

#if defined(FBCON_HAS_CFB16)
static u16 colreg[16];
#endif

static int dbmx21fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue, u_int *trans, struct fb_info *info)
{
	unsigned int val;
	
	FUNC_START;
	
	if(regno >= current_par.palette_size)
		return 1;
	
#if defined(FBCON_HAS_CFB16)
	val = colreg[regno];
#else
	val = _reg_LCDC_BPLUT_BASE(regno);
#endif
	
	if((current_par.bits_per_pixel == 4)&&(!current_par.color))
	{
		*red = *green = *blue = (val & BLUE) << 4;//TODO:
		*trans = 0;
	}
	else
	{
		*red = (val & RED) << 0;
		*green = (val & GREEN) << 5;
		*blue = (val & BLUE) << 11;
		*trans = 0;
	}

	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: dbmx21fb_setcolreg()
 * 
 * Input: 	regno	: Color register ID
 *		red	: Color map red[]
 *		green	: Color map green[]
 *		blue	: Color map blue[]
 *	transparent	: Flag
 *		info	: Fb_info database
 * 
 * Value Returned: int 	: Return status.If no error, return 0.
 *	 
 * Description: Transfer to fb_xxx_cmap handlers as parameters to 
 *		control color registers 
 *
 **********************************************************F*******************/
static int
dbmx21fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue, 
		u_int trans, struct fb_info *info)
{
	unsigned int val = 0;
	FUNC_START
	if(regno >= current_par.palette_size)
		return 1;

	if((current_par.bits_per_pixel == 4)&&(!current_par.color))
		val = (blue & 0x00f) << 12;//TODO:
	else
	{
		val = (blue >> 11 ) & BLUE;
		val |= (green >> 5) & GREEN;
		val |= (red >> 0) & RED;
	}

#if defined(FBCON_HAS_CFB16)
	colreg[regno] = val;
#else
	_reg_LCDC_BPLUT_BASE(regno) = val;
#endif
	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: dbmx21fb_get_cmap()
 * 
 * Input: 	cmap	: Ouput data pointer
 *		kspc   	: Kernel space flag
 *		con    	: Console ID
 *		info	: Frame buffer information 
 * 
 * Value Returned: int	: Return status.If no error, return 0.
 *	 
 * Description: Data is copied from hardware or local or system DISPAY,  
 *		and copied to cmap. 
 *
******************************************************************************/
static int
dbmx21fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
		                 struct fb_info *info)
{
	int err = 0;

	FUNC_START;
	DPRINTK("current_par.visual=%d\n", current_par.visual);
	if (con == current_par.currcon)
		err = fb_get_cmap(cmap, kspc, dbmx21fb_getcolreg, info);
	else if (fb_display[con].cmap.len) 
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(current_par.palette_size), cmap, kspc ? 0 : 2);
	FUNC_END;
	return err;
}

/*****************************************************************************
 * Function Name: dbmx21fb_set_cmap()
 * 
 * Input: 	cmap	: Ouput data pointer
 *		kspc   	: Kernel space flag
 *		con    	: Console ID
 *		info	: Frame buffer information 
 * 
 * Value Returned: int	: Return status.If no error, return 0.
 *	 
 * Description: Copy data from cmap and copy to DISPLAY. If DISPLAy has no cmap, 
 * 		allocate memory for it. If DISPLAY is current console and visible, 
 * 		then hardware color map shall be set.
 *
******************************************************************************/
static int
dbmx21fb_set_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	int err = 0;

	FUNC_START;
	DPRINTK("current_par.visual=%d\n", current_par.visual);
	if (!fb_display[con].cmap.len)
		err = fb_alloc_cmap(&fb_display[con].cmap, current_par.palette_size, 0);
	
	if (!err && con == current_par.currcon)
		err = fb_set_cmap(cmap, kspc, dbmx21fb_setcolreg, info);

	if (!err)
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

	FUNC_END;
	return err;
}
/*****************************************************************************
 * Function Name: dbmx21fb_get_var()
 * 
 * Input: 	var	: Iuput data pointer
 *		con	: Console ID
 *		info	: Frame buffer information 
 * 
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: 	_encode_var()
 *	 
 * Description: Get color map from current, or global display[console] 
 * 		used by ioctl
 *
******************************************************************************/
static int
dbmx21fb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{               
	if (con == -1)
	{
		_encode_var(var, &current_par);
	}
	else
		*var = fb_display[con].var;
	
	return 0;
}   


/*****************************************************************************
 * Function Name: dbmx21fb_updatevar()
 * 
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *	 
 * Description: Fill in display switch with LCD information, 
 *
******************************************************************************/
static int dbmx21fb_updatevar(int con, struct fb_info *info)
{
	DPRINTK("entered\n");
	return 0;
}

#ifdef CONFIG_FB_DBMX21_HWCURSOR
/*****************************************************************************
 * Function Name: dbmx21fb_cursor()
 * 
 * Input: 	display		: Iuput data pointer
 * 
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *	 
 * Description: display cursor
 *
******************************************************************************/
static void dbmx21fb_cursor(struct display *disp, int mode, int x, int y)
{
    if (mode == CM_ERASE) {
	_reg_LCDC_LCPR = 0x00000000;
	_reg_LCDC_LCWHBR = 0x00000000;
	_reg_LCDC_LCCMR = 0x00000000;
    } else {
	_reg_LCDC_LCCMR = 0x0003ffff;
	_reg_LCDC_LCWHBR = 0x80000010 |
	    (fontwidth(disp) << 24) | (fontheight(disp) << 16);
	_reg_LCDC_LCPR = 0x90000000 |
	    (fontwidth(disp) * x << 16) | fontheight(disp) * y;
    }
}
#endif

/*****************************************************************************
 * Function Name: dbmx21fb_set_dispsw()
 * 
 * Input: 	display		: Iuput data pointer
 *		dbmx21fb_info   	: Frame buffer of LCD information 
 * 
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *	 
 * Description: Fill in display switch with LCD information, 
 *
******************************************************************************/
static void dbmx21fb_set_dispsw(struct display *disp)
{
	FUNC_START;
	switch (disp->var.bits_per_pixel)
	{
		/* first step disable the hardware cursor */
#ifdef FBCON_HAS_CFB4
		case 4:
		    disp->dispsw = &fbcon_cfb4;
		    break;
#endif
#ifdef FBCON_HAS_CFB8
		case 8:
		    disp->dispsw = &fbcon_cfb8;
		    break;
#endif
#ifdef FBCON_HAS_CFB16
		case 12:
		case 16:
#ifdef CONFIG_FB_DBMX21_HWCURSOR
		    dbmx21fb_dispsw = fbcon_cfb16;
		    dbmx21fb_dispsw.cursor = dbmx21fb_cursor;
		    disp->dispsw = &dbmx21fb_dispsw;
#else
		    disp->dispsw = &fbcon_cfb16;
#endif
		    disp->dispsw_data = colreg;
		    break;
#endif
		    
		default:
		    disp->dispsw = &fbcon_dummy;
	}

	FUNC_END;
}

/*****************************************************************************
 * Function Name: dbmx21fb_set_var()
 * 
 * Input: 	var	: Iuput data pointer
 *		con	: Console ID
 *		info	: Frame buffer information 
 * 
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: 	dbmx21fb_decode_var()
 * 			dbmx21fb_encode_var()
 *  			dbmx21fb_set_dispsw()
 *
 * Description: set current_par by var, also set display data, specially the console 
 * 		related fileops, then enable the lcd controller, and set cmap to
 * 		hardware.
 *
******************************************************************************/
static int
dbmx21fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{       
	struct display *display;
	int err, chgvar = 0;
	struct dbmx21fb_par par;

	FUNC_START;
	if (con >= 0)
		display = &fb_display[con]; /* Display settings for console */
	else
		display = &global_disp;     /* Default display settings */

    /* Decode var contents into a par structure, adjusting any */
    /* out of range values. */
	if ((err = _decode_var(var, &par)))
   	{
		DPRINTK("decode var error!");
		return err;
	}

	// Store adjusted par values into var structure
	_encode_var(var, &par);
	
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
	
	display->var 		= *var;
	display->screen_base    = par.v_screen_start_address;
	display->visual         = par.visual;
	display->type           = FB_TYPE_PACKED_PIXELS;
	display->type_aux       = 0;
	display->ypanstep       = 0;
	display->ywrapstep      = 0;
	display->line_length    =
	display->next_line      = (var->xres * 16) / 8;
	
	display->can_soft_blank = 1;
	display->inverse        = 0;
	
	dbmx21fb_set_dispsw(display);
	
	/* If the console has changed and the console has defined */
	/* a changevar function, call that function. */
	if (chgvar && info && info->changevar)
		info->changevar(con);	// TODO:
	
	/* If the current console is selected and it's not truecolor, 
	*  update the palette 
	*/
	if ((con == current_par.currcon) &&
			(current_par.visual != FB_VISUAL_TRUECOLOR)) {
		struct fb_cmap *cmap;
		
		current_par = par;	// TODO ?
		if (display->cmap.len)
			cmap = &display->cmap;
		else
			cmap = fb_default_cmap(current_par.palette_size);
		
		fb_set_cmap(cmap, 1, dbmx21fb_setcolreg, info);
	}
	
	/* If the current console is selected, activate the new var. */
	if (con == current_par.currcon){
		init_var = *var;	// TODO:gcc support structure copy?
		_enable_lcd_controller();
	}
	
	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: dbmx21fb_get_fix()
 * 
 * Input: 	fix	: Ouput data pointer
 *		con	: Console ID
 *		info	: Frame buffer information 
 * 
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: VOID
 *	 
 * Description: get fix from display data, current_par data
 * 		used by ioctl
 * 				
******************************************************************************/
static int
dbmx21fb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
        struct display *display;

	FUNC_START;
        memset(fix, 0, sizeof(struct fb_fix_screeninfo));
        strcpy(fix->id, DBMX21_NAME);

        if (con >= 0)
        {
                DPRINTK("Using console specific display for con=%d\n",con);
                display = &fb_display[con];  /* Display settings for console */
        }
        else
                display = &global_disp;      /* Default display settings */

        fix->smem_start  = (unsigned long)current_par.screen_start_address;
        fix->smem_len    = current_par.screen_memory_size;
//printk("dbmx21fb_get_fix, pointer fix: 0x%08x, smem_len: 0x%08x\n",fix,fix->smem_len);
        fix->type        = display->type;
        fix->type_aux    = display->type_aux;
        fix->xpanstep    = 0;
        fix->ypanstep    = display->ypanstep;
        fix->ywrapstep   = display->ywrapstep;
        fix->visual      = display->visual;
        fix->line_length = display->line_length;
        fix->accel       = FB_ACCEL_NONE;

	FUNC_END;
        return 0;
}

/*****************************************************************************
 * Function Name: dbmx21fb_inter_handler()
 *
 * Input:
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: Interrupt handler
 *
******************************************************************************/
static void dbmx21fb_inter_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int intsr;
	FUNC_START;
	intsr = _reg_LCDC_LISR;
	printk(KERN_ERR"lcd interrupt!\n");
	FUNC_END;
	// handle
}

/*****************************************************************************
 * Function Name: dbmx21fb_blank()
 *
 * Input: 	blank	: Blank flag
 *		info	: Frame buffer database
 *
 * Value Returned: VOID
 *
 * Functions Called: 	_enable_lcd_controller()
 * 			_disable_lcd_controller()		
 *	 
 * Description: blank the screen, if blank, disable lcd controller, while if no blank
 * 		set cmap and enable lcd controller
 *
******************************************************************************/
static void
dbmx21fb_blank(int blank, struct fb_info *info)
{
#ifdef SUP_TTY0
        int i;

	FUNC_START;
        DPRINTK("blank=%d info->modename=%s\n", blank, info->modename);
        if (blank) {
                if (current_par.visual != FB_VISUAL_TRUECOLOR)
  	              for (i = 0; i < current_par.palette_size; i++)
			      ; // TODO
//printk("Disable LCD\n");
					 _disable_lcd_controller();
        }
        else {
                if (current_par.visual != FB_VISUAL_TRUECOLOR)
                	dbmx21fb_set_cmap(&fb_display[current_par.currcon].cmap,
					1,
					current_par.currcon, info);
//printk("Enable LCD\n");
                _enable_lcd_controller();
        }
	FUNC_END;
#endif //SUP_TTY0
}

/*****************************************************************************
 * Function Name: dbmx21fb_switch()
 * 
 * Input:  	info	: Frame buffer database
 * 
 * Value Returned: VOID
 *
 * Functions Called:
 *	 
 * Description: Switch to another console
 *
******************************************************************************/
static int dbmx21fb_switch(int con, struct fb_info *info)
{
	FUNC_START;
	if (current_par.visual != FB_VISUAL_TRUECOLOR) {
		struct fb_cmap *cmap;
	        if (current_par.currcon >= 0) {
	        	// Get the colormap for the selected console 
			cmap = &fb_display[current_par.currcon].cmap;
	
		if (cmap->len)
			fb_get_cmap(cmap, 1, dbmx21fb_getcolreg, info);
		}
	}
	
	current_par.currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;
	dbmx21fb_set_var(&fb_display[con].var, con, info);
	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: _encode_par()
 *
 * Input:  	var	: Input var data
 *		par	: LCD controller parameters
 *
 * Value Returned: VOID
 *
 * Functions Called:
 *
 * Description: use current_par to set a var structure
 *
******************************************************************************/
static int _encode_var(struct fb_var_screeninfo *var,
		struct dbmx21fb_par *par)
{
	FUNC_START
	// Don't know if really want to zero var on entry.
	// Look at set_var to see.  If so, may need to add extra params to par  
	
	// memset(var, 0, sizeof(struct fb_var_screeninfo));
	
	var->xres = par->xres;
	DPRINTK("var->xress=%d\n", var->xres);
	var->yres = par->yres;
	DPRINTK("var->yres=%d\n", var->yres);
	var->xres_virtual = par->xres_virtual;
	DPRINTK("var->xres_virtual=%d\n", var->xres_virtual);
	var->yres_virtual = par->yres_virtual;
	DPRINTK("var->yres_virtual=%d\n", var->yres_virtual);
	
	var->bits_per_pixel = par->bits_per_pixel;
	DPRINTK("var->bits_per_pixel=%d\n", var->bits_per_pixel);
	
	switch(var->bits_per_pixel)
	{
	case 2:
	case 4:
	case 8:
		var->red.length    = 4;
		var->green         = var->red;
		var->blue          = var->red;
		var->transp.length = 0;
		break;
	case 12:          // This case should differ for Active/Passive mode
	case 16:
		DPRINTK("16->a\n");
		var->red.length    = 5;
		var->blue.length   = 6;
		var->green.length  = 5;
		var->transp.length = 0;
#ifdef __LITTLE_ENDIAN
		DPRINTK("16->b\n");
		var->red.offset    = 11;
		var->green.offset  = 5;
		var->blue.offset   = 0;
		var->transp.offset = 0;
#endif // __LITTLE_ENDIAN
		break;
	}

	FUNC_END
	return 0;
}

/*****************************************************************************
 * Function Name: _decode_var
 * 
 * Input:  	var	: Input var data
 *		par	: LCD controller parameters
 *  
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *	 
 * Description: Get the video params out of 'var'. If a value doesn't fit, 
 * 		round it up,if it's too big, return -EINVAL.
 *
 * Cautions: Round up in the following order: bits_per_pixel, xres,
 * 	yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 * 	bitfields, horizontal timing, vertical timing. 			
 *
******************************************************************************/
static int _decode_var(struct fb_var_screeninfo *var, struct dbmx21fb_par *par)
{
	FUNC_START
	
	*par = current_par;

	if ((par->xres = var->xres) < MIN_XRES)
		par->xres = MIN_XRES; 
	if ((par->yres = var->yres) < MIN_YRES)
		par->yres = MIN_YRES;
	if (par->xres > current_par.max_xres)
		par->xres = current_par.max_xres;
	if (par->yres > current_par.max_yres)
		par->yres = current_par.max_yres; 
	par->xres_virtual = var->xres_virtual < par->xres ? par->xres : var->xres_virtual;
	par->yres_virtual = var->yres_virtual < par->yres ? par->yres : var->yres_virtual;
	par->bits_per_pixel = var->bits_per_pixel;

	switch (par->bits_per_pixel)
	{
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
	case 12: 	// RGB 444
	case 16:  	/* RGB 565 */
		par->visual = FB_VISUAL_TRUECOLOR;
		par->palette_size = 16; 
		break;
#endif
	default:
		return -EINVAL;
	}

	par->screen_start_address  =(u_char*)((u_long)p_framebuffer_memory_address+PAGE_SIZE);
	par->v_screen_start_address =(u_char*)((u_long)v_framebuffer_memory_address+PAGE_SIZE);

	FUNC_END
	return 0;
}


/*****************************************************************************
 * Function Name: _reserve_fb_memory()
 * 
 * Input: VOID
 *  
 * Value Returned: VOID
 *
 * Functions Called: 	
 *
 * Description: get data out of var structure and set related LCD controller registers
 *
******************************************************************************/
static int __init _reserve_fb_memory(void)
{
	struct page *page;

	FUNC_START
	
	DPRINTK("frame buffer memory size = %x\n", (unsigned int)ALLOCATED_FB_MEM_SIZE);
	if (v_framebuffer_memory_address != NULL)
		return -EINVAL;

	if ((v_framebuffer_memory_address =
	     consistent_alloc(GFP_KERNEL | GFP_DMA, ALLOCATED_FB_MEM_SIZE,
			      &p_framebuffer_memory_address)) == NULL)
	{
		DPRINTK("can not allocated memory\n");
		return -ENOMEM;
	}
        
	/* Set reserved flag for fb memory to allow it to be remapped into */
	/* user space by the common fbmem driver using remap_page_range(). */
	for(page = virt_to_page(v_framebuffer_memory_address);
		page < virt_to_page(v_framebuffer_memory_address + ALLOCATED_FB_MEM_SIZE); 
		page++)
	{
		mem_map_reserve(page);
	}
	current_par.screen_start_address  =(u_char*)((u_long)p_framebuffer_memory_address+PAGE_SIZE);
	current_par.v_screen_start_address =(u_char*)((u_long)v_framebuffer_memory_address+PAGE_SIZE);

	DPRINTK("physical screen start addres: %x\n", (u_long)p_framebuffer_memory_address+PAGE_SIZE);

	FUNC_END

	return (v_framebuffer_memory_address == NULL ? -EINVAL : 0);
}

/*****************************************************************************
 * Function Name: _enable_lcd_controller()
 * 
 * Input: VOID
 *  
 * Value Returned: VOID
 *
 * Functions Called: 	
 *	 
 * Description: enable Lcd controller, setup registers, 
 *		base on current_par value
 *
******************************************************************************/
static void _enable_lcd_controller(void)
{
	unsigned int i;
	unsigned short *pscr;
	
	FUNC_START
	
	_reg_CRM_PCCR0 |= 0x04040000;
	
	// 0xE3800000 is the virtual address for 0xCC800000)
	*((volatile uint16_t *)0xE3800000) |= 0x0200;
		
	FUNC_END;
}

/*****************************************************************************
 * Function Name: _disable_lcd_controller()
 * 
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *	 
 * Description: just disable the LCD controller
 * 		disable lcd interrupt. others, i have no ideas
 *
******************************************************************************/
static void _disable_lcd_controller(void)
{
	FUNC_START
	_reg_CRM_PCCR0 &= ~0x04040000;
	
	//setmem 0xCC800000 0x0200 16 (0xE3000000 is the virtual address for 0xCC000000)
	*((volatile uint16_t *)0xE3800000)&= ~0x0200;
	FUNC_END
}

/*****************************************************************************
 * Function Name: _install_cmap
 * 
 * Input: VOID
 *  
 * Value Returned: VOID
 *
 * Functions Called: 	
 *	 
 * Description: set color map to hardware
 *
******************************************************************************/
static void _install_cmap(int con, struct fb_info *info) 
{
	FUNC_START
	if (con != current_par.currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, dbmx21fb_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), 1, dbmx21fb_setcolreg, info);
	
	FUNC_END	
	return;
}

/*****************************************************************************
 * Function Name: _init_lcd_system
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called:
 *
 * Description: initialise GPIO
 *
******************************************************************************/
static void __init _init_lcd_system(void)
{
	FUNC_START

#ifdef CONFIG_ARCH_MX2ADS
	// Port A PTA_GIUS
//printk("***default _reg_GPIO_GIUS(GPIOA)=%08x\n", _reg_GPIO_GIUS(GPIOA));
	_reg_GPIO_GIUS(GPIOA) = 0x40000000;
	//printk("1\n");
	// Port A PTA_GPR
//printk("***default _reg_GPIO_GPR(GPIOA)=%08x\n", _reg_GPIO_GPR(GPIOA));
	_reg_GPIO_GPR(GPIOA) = 0x00000000;
	//printk("2\n");
#endif
#if 0 //ORIG
	//setmem 0x10015520 0x00000000 32
	//_reg_GPIO_GIUS(GPIOF) = 0x00000000; //it affact NAND FLASH Frank Li
	//printk("3\n");
	//setmem 0x10015538 0x00000000 32
	//_reg_GPIO_GPR(GPIOF) = 0x00000000;
#else
	//setmem 0x10015520 0x00000000 32
	_reg_GPIO_GIUS(GPIOF) = 0x00000000; //it affact NAND FLASH Frank Li
	//printk("3\n");
	//setmem 0x10015538 0x00000000 32
	_reg_GPIO_GPR(GPIOF) = 0x00000000;
#endif

	FUNC_END
	//setmem 0xDF001008 0x00002000 32
//	_reg_WEIM_CSU(WEIM_CS1) = 0x00002000;
	//setmem 0xDF00100C 0x11118501 32
//	_reg_WEIM_CSL(WEIM_CS1) = 0x11118501;
	
}

/*****************************************************************************
 * Function Name: _init_lcd
 *
 * Input: VOID
 *
 * Value Returned: VOID
 *
 * Functions Called: _decode_var()
 *
 * Description: initialize the LCD controller, use current_par for 12bpp
 *
******************************************************************************/
static int __init _init_lcd()
{
	unsigned int val;

	FUNC_START
	_decode_var(&init_var, &current_par);

	// LCD regs
	DPRINTK(KERN_ERR"Writing LSSAR with %x\n", (unsigned int)current_par.screen_start_address);
	_reg_LCDC_LSSAR = (unsigned int)current_par.screen_start_address;
	DPRINTK(KERN_ERR"LSSAR=%x\n", _reg_LCDC_LSSAR);
	
	val = 0;
	val = current_par.xres/16;
	val = val<<20;
	val += current_par.yres;
	DPRINTK(KERN_ERR"par.x=%x, y=%x\n", current_par.xres, current_par.yres);
	_reg_LCDC_LSR = val;
	DPRINTK(KERN_ERR"LSR=%x\n", _reg_LCDC_LSR);
	
	val= 0;
	val=current_par.xres_virtual/2;
	_reg_LCDC_LVPWR = val;
	DPRINTK(KERN_ERR"LVPWR=%x\n", _reg_LCDC_LVPWR);
	
	_reg_LCDC_LPCR = PANELCFG_VAL_12;
	DPRINTK(KERN_ERR"LPCR=%x\n", _reg_LCDC_LPCR);
			
	// Sharp Configuration Register
	_reg_LCDC_LSCR = 0x00120300;
	DPRINTK(KERN_ERR"LSCR=%x\n", _reg_LCDC_LSCR);

	_reg_LCDC_LHCR = HCFG_VAL_12;
	DPRINTK(KERN_ERR"LHCR=%x\n", _reg_LCDC_LHCR);

	_reg_LCDC_LVCR = VCFG_VAL_12;
	DPRINTK(KERN_ERR"LVCR=%x\n", _reg_LCDC_LVCR);

	_reg_LCDC_LPCCR = 0x00A903FF;
	DPRINTK(KERN_ERR"LPCCR=%x\n", _reg_LCDC_LPCCR);

	_reg_LCDC_LRMCR = 0x00000000;
	DPRINTK(KERN_ERR"LRMCR=%x\n", _reg_LCDC_LRMCR);

	_reg_LCDC_LDCR = LDCR_VAL;
	DPRINTK(KERN_ERR"LDCR=%x\n", _reg_LCDC_LDCR);


	DPRINTK(KERN_ERR"_init_lcd end \n");

	FUNC_END
	return 0;
}


/*****************************************************************************
 * Function Name: dbmx21fb_init()
 *
 * Input: VOID
 *
 * Value Returned: int 		: Return status.If no error, return 0.
 *
 * Functions Called: 	_init_fbinfo()
 *			disable_irq()
 *	 		enable_irq()
 *			_init_lcd()
 *			dbmx21fb_init_cursor()
 *
 * Description: initialization module, all of init routine's entry point
 * 		initialize fb_info, init_var, current_par
 * 		and setup interrupt, memory, lcd controller
 *
******************************************************************************/
int __init dbmx21fb_init(void)
{
	int ret;
	FUNC_START
	
	_init_lcd_system();

	_init_fbinfo();

	if ((ret = _reserve_fb_memory()) != 0){
		printk(KERN_ERR"failed for reserved DBMX frame buffer memory\n");
		return ret;
	}

        if (dbmx21fb_set_var(&init_var, -1, &fb_info))
		; //current_par.allow_modeset = 0;

	_init_lcd();
	_enable_lcd_controller();
	
	register_framebuffer(&fb_info);

	/* This driver cannot be unloaded at the moment */
	MOD_INC_USE_COUNT;

	printk(KERN_ERR"DBM21X frame buffer initialized.\n");
	
	FUNC_END
	return 0;
}

/*****************************************************************************
 * Function Name: dbmx21fb_setup()
 * 
 * Input: info	: VOID
 *
 * Value Returned: int	: Return status.If no error, return 0.
 *
 * Functions Called: VOID		
 *	 
 * Description: basically, this routine used to parse command line parameters, which 
 * 		is initialization parameters for lcd controller, such as freq, xres, 
 * 		yres, and so on
 *
******************************************************************************/
int __init dbmx21fb_setup(char *options)
{
	FUNC_START;
	FUNC_END;
	return 0;
}

/*****************************************************************************
 * Function Name: _init_fbinfo()
 * 
 * Input: VOID
 * 
 * Value Returned: VOID
 *
 * Functions Called: VOID
 *
 * Description: while 16bpp is used to store a 12 bits pixels packet, but
 * 		it is not a really 16bpp system. maybe in-compatiable with
 * 		other system or GUI.There are some field in var which specify
 *		the red/green/blue offset in a 16bit word, just little endian is
 * 		concerned
 *
******************************************************************************/
static void __init _init_fbinfo(void)
{
	FUNC_START;
	strcpy(fb_info.modename, DBMX21_NAME);
	strcpy(fb_info.fontname, "Acorn8x8");

	fb_info.node		= -1;
	fb_info.flags		= 0;	// Low-level driver is not a module
	fb_info.fbops		= &dbmx21fb_ops;
	fb_info.monspecs	= monspecs;
	fb_info.disp		= &global_disp;
	fb_info.changevar	= NULL;
	fb_info.switch_con	= dbmx21fb_switch;
	fb_info.updatevar	= dbmx21fb_updatevar;
	fb_info.blank		= dbmx21fb_blank;

/*
 * * setup initial parameters
 * */
	memset(&init_var, 0, sizeof(init_var));

	init_var.transp.length	= 0;
	init_var.nonstd		= 0;
	init_var.activate	= FB_ACTIVATE_NOW;
	init_var.xoffset	= 0;
	init_var.yoffset	= 0;
	init_var.height		= -1;
	init_var.width		= -1;
	init_var.vmode		= FB_VMODE_NONINTERLACED;

	current_par.max_xres	= LCD_MAXX;
	current_par.max_yres	= LCD_MAXY;
	current_par.max_bpp	= LCD_MAX_BPP;		// 16
	init_var.red.length	= 5;  // 5;
	init_var.green.length	= 6;  // 6;
	init_var.blue.length	= 5;  // 5;
#ifdef __LITTLE_ENDIAN
	init_var.red.offset	= 11;
	init_var.green.offset	= 5;
	init_var.blue.offset	= 0;
#endif //__LITTLE_ENDIAN
	init_var.grayscale	= 16;	// i suppose, TODO
	init_var.sync		= 0;
	init_var.pixclock	= 171521;	// TODO

	current_par.screen_start_address	= NULL;
	current_par.v_screen_start_address	= NULL;
	current_par.screen_memory_size		= MAX_PIXEL_MEM_SIZE;
	current_par.currcon			= -1;	// TODO

	init_var.xres		= current_par.max_xres;
	init_var.yres		= current_par.max_yres;
	init_var.xres_virtual	= init_var.xres;
	init_var.yres_virtual	= init_var.yres;
	init_var.bits_per_pixel	= current_par.max_bpp;

	FUNC_END;
}



/* end of file */

