/*
 *  linux/drivers/video/katanafb.c
 *
 *  Copyright (c) 2002 by MediaQ, Incorporated.
 *
 *      based on linux/drivers/video/sa1100fb.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/arch/platform.h>
#include <asm/arch/uncompress.h>	//for puts

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>

//HSU $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#define	PORTRAIT_PANEL

/*
 * debugging?
 */
#if 0	//HSU
#define DEBUG 1		//HSU
#define	DPRINTK	printk
#else
#define DEBUG 0
#endif

#include "katanafb.h"

/*
 * IMHO this looks wrong.  In 8BPP, length should be 8.
 */
#if 1
static struct katanafb_rgb rgb_8 = {
	red:	{ offset: 0,  length: 8, },
	green:	{ offset: 0,  length: 8, },
	blue:	{ offset: 0,  length: 8, },
	transp:	{ offset: 0,  length: 0, },
};
#else
static struct katanafb_rgb rgb_8 = {
	red:	{ offset: 0,  length: 4, },
	green:	{ offset: 0,  length: 4, },
	blue:	{ offset: 0,  length: 4, },
	transp:	{ offset: 0,  length: 0, },
};
#endif

static struct katanafb_rgb def_rgb_16 = {
	red:	{ offset: 11, length: 5, },
	green:	{ offset: 5,  length: 6, },
	blue:	{ offset: 0,  length: 5, },
	transp:	{ offset: 0,  length: 0, },
};


#if 1	//HSU
static struct katanafb_mach_info panel4_info __initdata = {
	pixclock:	171521,		bpp:		16,
#ifdef PORTRAIT_PANEL
	xres:		240,		yres:		320,
#else
	xres:		320,		yres:		240,
#endif

	hsync_len:	5,		vsync_len:	1,
	left_margin:	61,		upper_margin:	3,
	right_margin:	9,		lower_margin:	0,

	sync:		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

//	lccr0:		LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
//	lccr3:		LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};

#else	//HSU

/*
 * The assabet uses a sharp LQ039Q2DS54 LCD module.  It is actually
 * takes an RGB666 signal, but we provide it with an RGB565 signal
 * instead (def_rgb_16).
 */
static struct sa1100fb_mach_info lq039q2ds54_info __initdata = {
	pixclock:	171521,		bpp:		16,
	xres:		320,		yres:		240,

	hsync_len:	5,		vsync_len:	1,
	left_margin:	61,		upper_margin:	3,
	right_margin:	9,		lower_margin:	0,

	sync:		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	lccr0:		LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	lccr3:		LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};

//static struct sa1100fb_rgb h3600_rgb_16 = {
//	red:	{ offset: 12, length: 4, },
//	green:	{ offset: 7,  length: 4, },
//	blue:	{ offset: 1,  length: 4, },
//	transp: { offset: 0,  length: 0, },
//};
#endif	//HSU


static struct katanafb_mach_info * __init
katanafb_get_machine_info(struct katanafb_info *fbi)
{
	struct katanafb_mach_info *inf = NULL;

	/*
	 *            R        G       B       T
	 * default  {11,5}, { 5,6}, { 0,5}, { 0,0}
	 * h3600    {12,4}, { 7,4}, { 1,4}, { 0,0}
	 * freebird { 8,4}, { 4,4}, { 0,4}, {12,4}
	 */
	if ( machine_is_katana() )
		inf = &panel4_info;

	return inf;
}

static inline void katanafb_schedule_task(struct katanafb_info *fbi, u_int state)
{
	unsigned long flags;

	local_irq_save(flags);
	/*
	 * We need to handle two requests being made at the same time.
	 * There are two important cases:
	 *  1. When we are changing VT (C_REENABLE) while unblanking (C_ENABLE)
	 *     We must perform the unblanking, which will do our REENABLE for us.
	 *  2. When we are blanking, but immediately unblank before we have
	 *     blanked.  We do the "REENABLE" thing here as well, just to be sure.
	 */
	if (fbi->task_state == C_ENABLE && state == C_REENABLE)
		state = (u_int) -1;
	if (fbi->task_state == C_DISABLE && state == C_ENABLE)
		state = C_REENABLE;

	if (state != (u_int)-1) {
		fbi->task_state = state;
		schedule_task(&fbi->task);
	}
	local_irq_restore(flags);
}

/*
 * Get the VAR structure pointer for the specified console
 */
static inline struct fb_var_screeninfo *get_con_var(struct fb_info *info, int con)
{
	struct katanafb_info *fbi = (struct katanafb_info *)info;
	return (con == fbi->currcon || con == -1) ?
			 &fbi->fb.var : &fb_display[con].var;
}

/*
 * Get the DISPLAY structure pointer for the specified console
 */
static inline struct display *get_con_display(struct fb_info *info, int con)
{
	struct katanafb_info *fbi = (struct katanafb_info *)info;
	return (con < 0) ? fbi->fb.disp : &fb_display[con];
}

/*
 * Get the CMAP pointer for the specified console
 */
static inline struct fb_cmap *get_con_cmap(struct fb_info *info, int con)
{
	struct katanafb_info *fbi = (struct katanafb_info *)info;
	return (con == fbi->currcon || con == -1) ? &fbi->fb.cmap : &fb_display[con].cmap;
}

static inline u_int
chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

/*
 * Convert bits-per-pixel to a hardware palette PBS value.
 */
static inline u_int
palette_pbs(struct fb_var_screeninfo *var)
{
	int ret = 0;
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:  ret = 0 << 12;	break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:  ret = 1 << 12; break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16: ret = 2 << 12; break;
#endif
	}
	return ret;
}

static int
katanafb_setpalettereg(u_int regno, u_int red, u_int green, u_int blue,
		       u_int trans, struct fb_info *info)
{
	struct katanafb_info *fbi = (struct katanafb_info *)info;
	u_int val, ret = 1;

	if (regno < fbi->palette_size) {
		val = ((red >> 4) & 0xf00);
		val |= ((green >> 8) & 0x0f0);
		val |= ((blue >> 12) & 0x00f);

		if (regno == 0)
			val |= palette_pbs(&fbi->fb.var);

		fbi->palette_cpu[regno] = val;
		ret = 0;
	}
	return ret;
}

static int
katanafb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		   u_int trans, struct fb_info *info)
{
	struct katanafb_info *fbi = (struct katanafb_info *)info;
	struct display *disp = get_con_display(info, fbi->currcon);
	u_int val;
	int ret = 1;

	/*
	 * If inverse mode was selected, invert all the colours
	 * rather than the register number.  The register number
	 * is what you poke into the framebuffer to produce the
	 * colour you requested.
	 */
	if (disp->inverse) {
		red   = 0xffff - red;
		green = 0xffff - green;
		blue  = 0xffff - blue;
	}

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no mater what visual we are using.
	 */
	if (fbi->fb.var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
					7471 * blue) >> 16;

	switch (fbi->fb.disp->visual) {
	case FB_VISUAL_TRUECOLOR:
		/*
		 * 12 or 16-bit True Colour.  We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno < 16) {
			u16 *pal = fbi->fb.pseudo_palette;

			val  = chan_to_field(red, &fbi->fb.var.red);
			val |= chan_to_field(green, &fbi->fb.var.green);
			val |= chan_to_field(blue, &fbi->fb.var.blue);

			pal[regno] = val;
			ret = 0;
		}
		break;

	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		ret = katanafb_setpalettereg(regno, red, green, blue, trans, info);
		break;
	}

	return ret;
}

/*
 *  katanafb_decode_var():
 *    Get the video params out of 'var'. If a value doesn't fit, round it up,
 *    if it's too big, return -EINVAL.
 *
 *    Suggestion: Round up in the following order: bits_per_pixel, xres,
 *    yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *    bitfields, horizontal timing, vertical timing.
 */
static int
katanafb_validate_var(struct fb_var_screeninfo *var,
		      struct katanafb_info *fbi)
{
	int ret = -EINVAL;

	if (var->xres < MIN_XRES)
		var->xres = MIN_XRES;
	if (var->yres < MIN_YRES)
		var->yres = MIN_YRES;
	if (var->xres > fbi->max_xres)
		var->xres = fbi->max_xres;
	if (var->yres > fbi->max_yres)
		var->yres = fbi->max_yres;
	var->xres_virtual =
	    var->xres_virtual < var->xres ? var->xres : var->xres_virtual;
	var->yres_virtual =
	    var->yres_virtual < var->yres ? var->yres : var->yres_virtual;

	DPRINTK("var->bits_per_pixel=%d\n", var->bits_per_pixel);
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:		ret = 0; break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:		ret = 0; break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:	ret = 0; break;
#endif
	default:
		break;
	}

#ifdef CONFIG_CPU_FREQ
	printk(KERN_DEBUG "dma period = %d ps, clock = %d kHz\n",
		sa1100fb_display_dma_period(var),
		cpufreq_get(smp_processor_id()));
#endif

	return ret;
}

#define NO_CURSOR
#ifdef NO_CURSOR
static void fbcon_cfb16_cursor(struct display *p, int mode, int x, int y)
{
}
#endif

/*
 * katanafb_set_var():
 *	Set the user defined part of the display for the specified console
 */
static int
katanafb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct katanafb_info *fbi = (struct katanafb_info *)info;
	struct fb_var_screeninfo *dvar = get_con_var(&fbi->fb, con);
	struct display *display = get_con_display(&fbi->fb, con);
	int err, chgvar = 0, rgbidx;

	DPRINTK("set_var\n");

	/*
	 * Decode var contents into a par structure, adjusting any
	 * out of range values.
	 */
	err = katanafb_validate_var(var, fbi);
	if (err)
		return err;

	if (var->activate & FB_ACTIVATE_TEST)
		return 0;

	if ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW)
		return -EINVAL;

	if (dvar->xres != var->xres)
		chgvar = 1;
	if (dvar->yres != var->yres)
		chgvar = 1;
	if (dvar->xres_virtual != var->xres_virtual)
		chgvar = 1;
	if (dvar->yres_virtual != var->yres_virtual)
		chgvar = 1;
	if (dvar->bits_per_pixel != var->bits_per_pixel)
		chgvar = 1;
	if (con < 0)
		chgvar = 0;

	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:
		if (fbi->cmap_static)
			display->visual	= FB_VISUAL_STATIC_PSEUDOCOLOR;
		else
			display->visual	= FB_VISUAL_PSEUDOCOLOR;
		display->line_length	= var->xres / 2;
		display->dispsw		= &fbcon_cfb4;
		rgbidx			= RGB_8;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		if (fbi->cmap_static)
			display->visual	= FB_VISUAL_STATIC_PSEUDOCOLOR;
		else
			display->visual	= FB_VISUAL_PSEUDOCOLOR;
		display->line_length	= var->xres;
		display->dispsw		= &fbcon_cfb8;
		rgbidx			= RGB_8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
#if 1
	//****TEMP **** Program GE0AR to 640 bytes with 16bpp ****
	{
		MQ9000REGS	*pRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
#ifdef PORTRAIT_PANEL
		pRegs->ge.ge0A.DstStride = 0x400001E0;
		pRegs->ge.ge0B.BaseAddr = 0x00000000;
#else
		pRegs->ge.ge0A.DstStride = 0x40000280;
		pRegs->ge.ge0B.BaseAddr = 0x00000000;
#endif
	}
#endif

		display->visual	= FB_VISUAL_TRUECOLOR;
		//display->visual		= FB_VISUAL_DIRECTCOLOR;
		display->line_length	= var->xres * 2;
		display->dispsw		= &fbcon_cfb16;
#ifdef NO_CURSOR
		display->dispsw->cursor	= fbcon_cfb16_cursor;
#endif
		display->dispsw_data	= fbi->fb.pseudo_palette;
		rgbidx			= RGB_16;
		break;
#endif
	default:
		rgbidx = 0;
		display->dispsw = &fbcon_dummy;
		break;
	}

	//display->screen_base	= fbi->screen_cpu;
	display->screen_base	= (char*)MQ9000_DISPLAY_VBASE;
	display->next_line	= display->line_length;	//watch out rotation??
	display->type		= fbi->fb.fix.type;
	display->type_aux	= fbi->fb.fix.type_aux;
	display->ypanstep	= fbi->fb.fix.ypanstep;
	display->ywrapstep	= fbi->fb.fix.ywrapstep;
	display->can_soft_blank	= 1;
	//display->inverse	= fbi->cmap_inverse;
	display->inverse	= 0;

	*dvar			= *var;
	dvar->activate		&= ~FB_ACTIVATE_ALL;

	/*
	 * Copy the RGB parameters for this display
	 * from the machine specific parameters.
	 */
	dvar->red		= fbi->rgb[rgbidx]->red;
	dvar->green		= fbi->rgb[rgbidx]->green;
	dvar->blue		= fbi->rgb[rgbidx]->blue;
	dvar->transp		= fbi->rgb[rgbidx]->transp;

	DPRINTK("RGBT length = %d:%d:%d:%d\n",
		dvar->red.length, dvar->green.length, dvar->blue.length,
		dvar->transp.length);

	DPRINTK("RGBT offset = %d:%d:%d:%d\n",
		dvar->red.offset, dvar->green.offset, dvar->blue.offset,
		dvar->transp.offset);

	/*
	 * Update the old var.  The fbcon drivers still use this.
	 * Once they are using fbi->fb.var, this can be dropped.
	 */
	display->var = *dvar;

	/*
	 * If we are setting all the virtual consoles, also set the
	 * defaults used to create new consoles.
	 */
	if (var->activate & FB_ACTIVATE_ALL)
		fbi->fb.disp->var = *dvar;

	/*
	 * If the console has changed and the console has defined
	 * a changevar function, call that function.
	 */
	if (chgvar && info && fbi->fb.changevar)
		fbi->fb.changevar(con);

	/* If the current console is selected, activate the new var. */
	if (con != fbi->currcon)
		return 0;

	fb_set_cmap(&fbi->fb.cmap, 1, katanafb_setcolreg, &fbi->fb);

	return 0;
}

static int
__do_set_cmap(struct fb_cmap *cmap, int kspc, int con,
	      struct fb_info *info)
{
	struct katanafb_info *fbi = (struct katanafb_info *)info;
	struct fb_cmap *dcmap = get_con_cmap(info, con);
	int err = 0;

	if (con == -1)
		con = fbi->currcon;

	/* no colormap allocated? (we always have "this" colour map allocated) */
	if (con >= 0)
		err = fb_alloc_cmap(&fb_display[con].cmap, fbi->palette_size, 0);

	if (!err && con == fbi->currcon)
		err = fb_set_cmap(cmap, kspc, katanafb_setcolreg, info);

	if (!err)
		fb_copy_cmap(cmap, dcmap, kspc ? 0 : 1);

	return err;
}

static int
katanafb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		  struct fb_info *info)
{
	struct display *disp = get_con_display(info, con);

	if (disp->visual == FB_VISUAL_TRUECOLOR ||
	    disp->visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
		return -EINVAL;

	return __do_set_cmap(cmap, kspc, con, info);
}

static int
katanafb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct display *display = get_con_display(info, con);

	*fix = info->fix;
	fix->line_length = display->line_length;
	fix->visual	 = display->visual;

	//memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	/*
	fix->smem_start = MQ9000_DISPLAY_BASE;
	fix->smem_len = SZ_256K;
	fix->type = FB_TYPE_PACKED_PIXELS;
	*/
	//fix->line_length = 320 * 2;	//$$$$$$$$$$$$$$$$$

	return 0;
}

static int
katanafb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	*var = *get_con_var(info, con);

	return 0;
}

static int
katanafb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	struct fb_cmap *dcmap = get_con_cmap(info, con);
	fb_copy_cmap(dcmap, cmap, kspc ? 0 : 2);
	return 0;
}

static struct fb_ops katanafb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	katanafb_get_fix,
	fb_get_var:	katanafb_get_var,
	fb_set_var:	katanafb_set_var,
	fb_get_cmap:	katanafb_get_cmap,
	fb_set_cmap:	katanafb_set_cmap,
};

/*
 *  katanafb_switch():       
 *	Change to the specified console.  Palette and video mode
 *      are changed to the console's stored parameters.
 *
 *	Uh oh, this can be called from a tasklet (IRQ)
 */
static int katanafb_switch(int con, struct fb_info *info)
{
	struct katanafb_info *fbi = (struct katanafb_info *)info;
	struct display *disp;
	struct fb_cmap *cmap;

	DPRINTK("con=%d info->modename=%s\n", con, fbi->fb.modename);

	if (con == fbi->currcon)
		return 0;

	if (fbi->currcon >= 0) {
		disp = fb_display + fbi->currcon;

		/*
		 * Save the old colormap and video mode.
		 */
		disp->var = fbi->fb.var;

	
	//HSU	if (disp->cmap.len)
	//HSU		fb_copy_cmap(&fbi->fb.cmap, &disp->cmap, 0);
	}

	fbi->currcon = con;
	disp = fb_display + con;

	/*
	 * Make sure that our colourmap contains 256 entries.
	 */
	/* ??? HSU Don't copy any color map for now ??? */
	fb_alloc_cmap(&fbi->fb.cmap, 256, 0);

	if (disp->cmap.len)
		cmap = &disp->cmap;
	else
		cmap = fb_default_cmap(1 << disp->var.bits_per_pixel);

	fb_copy_cmap(cmap, &fbi->fb.cmap, 0);

	fbi->fb.var = disp->var;
	fbi->fb.var.activate = FB_ACTIVATE_NOW;

	katanafb_set_var(&fbi->fb.var, con, info);

	return 0;
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
 * sa1100fb_blank():
 *	Blank the display by setting all palette values to zero.  Note, the 
 * 	12 and 16 bpp modes don't really use the palette, so this will not
 *      blank the display in all modes.  
 */
static void katanafb_blank(int blank, struct fb_info *info)
{
}

static int katanafb_updatevar(int con, struct fb_info *info)
{
	DPRINTK("entered\n");
	return 0;
}

#ifdef CONFIG_PM
/*
 * Power management hook.  Note that we won't be called from IRQ context,
 * unlike the blank functions above, so we may sleep.
 */
static int
katanafb_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	return 0;
}
#endif

/*
 * sa1100fb_map_video_memory():
 *      Allocates the DRAM memory for the frame buffer.  This buffer is  
 *	remapped into a non-cached, non-buffered, memory region to  
 *      allow palette and pixel writes to occur without flushing the 
 *      cache.  Once this area is remapped, all virtual memory
 *      access to the video memory should occur at the new region.
 */
static int __init katanafb_map_video_memory(struct katanafb_info *fbi)
{
	fbi->fb.fix.smem_start = MQ9000_DISPLAY_BASE;
	fbi->fb.fix.mmio_start = MQ9000_REGS_BASE;
	fbi->fb.fix.mmio_len = SZ_1M;
	return 0;
}

/* Fake monspecs to fill in fbinfo structure */
static struct fb_monspecs monspecs __initdata = {
	30000, 70000, 50, 65, 0	/* Generic */
};


static struct katanafb_info * __init katanafb_init_fbinfo(void)
{
	struct katanafb_mach_info *inf;
	struct katanafb_info *fbi;

	fbi = kmalloc(sizeof(struct katanafb_info) + sizeof(struct display) +
		      sizeof(u16) * 16, GFP_KERNEL);
	if (!fbi)
		return NULL;

	memset(fbi, 0, sizeof(struct katanafb_info) + sizeof(struct display));

	fbi->currcon		= -1;

	strcpy(fbi->fb.fix.id, KATANA_NAME);

	fbi->fb.fix.type	= FB_TYPE_PACKED_PIXELS;
	fbi->fb.fix.type_aux	= 0;
	fbi->fb.fix.xpanstep	= 0;
	fbi->fb.fix.ypanstep	= 0;
	fbi->fb.fix.ywrapstep	= 0;
	fbi->fb.fix.accel	= FB_ACCEL_NONE;

	fbi->fb.var.nonstd	= 0;
	fbi->fb.var.activate	= FB_ACTIVATE_NOW;
	fbi->fb.var.height	= -1;
	fbi->fb.var.width	= -1;
	fbi->fb.var.accel_flags	= 0;
	fbi->fb.var.vmode	= FB_VMODE_NONINTERLACED;

	strcpy(fbi->fb.modename, KATANA_NAME);
	strcpy(fbi->fb.fontname, "Acorn8x8");
	//strcpy(fbi->fb.fontname, "VGA8x8");

	fbi->fb.fbops		= &katanafb_ops;
	fbi->fb.changevar	= NULL;
	fbi->fb.switch_con	= katanafb_switch;
	fbi->fb.updatevar	= katanafb_updatevar;
	fbi->fb.blank		= katanafb_blank;
	fbi->fb.flags		= FBINFO_FLAG_DEFAULT;
	fbi->fb.node		= -1;
	fbi->fb.monspecs	= monspecs;
	fbi->fb.disp		= (struct display *)(fbi + 1);
	fbi->fb.pseudo_palette	= (void *)(fbi->fb.disp + 1);

	fbi->rgb[RGB_8]		= &rgb_8;
	fbi->rgb[RGB_16]	= &def_rgb_16;

	inf = katanafb_get_machine_info(fbi);

	fbi->max_xres			= inf->xres;
	fbi->fb.var.xres		= inf->xres;
	fbi->fb.var.xres_virtual	= inf->xres;
	fbi->max_yres			= inf->yres;
	fbi->fb.var.yres		= inf->yres;
	fbi->fb.var.yres_virtual	= inf->yres;
	fbi->max_bpp			= inf->bpp;
	fbi->fb.var.bits_per_pixel	= inf->bpp;
	fbi->fb.var.pixclock		= inf->pixclock;
	fbi->fb.var.hsync_len		= inf->hsync_len;
	fbi->fb.var.left_margin		= inf->left_margin;
	fbi->fb.var.right_margin	= inf->right_margin;
	fbi->fb.var.vsync_len		= inf->vsync_len;
	fbi->fb.var.upper_margin	= inf->upper_margin;
	fbi->fb.var.lower_margin	= inf->lower_margin;
	fbi->fb.var.sync		= inf->sync;
	//fbi->fb.var.grayscale		= inf->cmap_greyscale;
	fbi->fb.var.grayscale		= 0;	//HSU: 0???
	//fbi->cmap_inverse		= inf->cmap_inverse;
	//fbi->cmap_static		= inf->cmap_static;
	//fbi->lccr0			= inf->lccr0;
	//fbi->lccr3			= inf->lccr3;
	//fbi->state			= C_DISABLE;
	//fbi->task_state			= (u_char)-1;
#if 0	//HSU ****** hardcoded full 320K for frame buffer ********
	fbi->fb.fix.smem_len		= (320 * 1024);
#else
	fbi->fb.fix.smem_len		= fbi->max_xres * fbi->max_yres *
					  fbi->max_bpp / 8;
#endif

	//init_waitqueue_head(&fbi->ctrlr_wait);
	//INIT_TQUEUE(&fbi->task, katanafb_task, fbi);
	//init_MUTEX(&fbi->ctrlr_sem);

	return fbi;
}


#define GxRCLK_PIXEL_CLK	0x00020000UL // G1RCLK source is Pixel Clock PLL2
#define IM_ENABLE	0x00000008UL // Image Window Enable
#define GC_8BPP		0x00000030UL // with color palette
#define FD_15		0x00100000UL // FD = 1.5
#define SD_5		0x05000000UL // SD = 5

#define GC_16BPP_BP	0x000000C0UL // with color palette bypassed
#define FP_MONO		0x00000010UL // Mono flat panel
#define GC_BPP_MASK	0x000000F0UL // Bpp Mas

#define GPIO_05_MASK	0x00e00000UL // GPIO 05 MASK
#define GPO_05_ENABLE	0x00200000UL // Enable GPIO 05 as Output	
#define GPO_05_DATA_H	0x00800000UL // GPO 05 Data High

static void init_panel(void)
{
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	unsigned long reg, val;

	pMQRegs->vi.vi1E.Clock = 0x00000001; // clock source PLL1

	pMQRegs->gc.gc0C.StartAddr = 0x00000000; // Window start
	pMQRegs->gc.gc0E.Stride = 240 * 16 / 8; // Window stride
	pMQRegs->gc.gc08.HWindow = (240 - 1) << 16;
	pMQRegs->gc.gc09.VWindow = (320 - 1) << 16;
	pMQRegs->gc.gc02.HTotalEnd = (317 - 2) | (248L << 16);
	pMQRegs->gc.gc03.VTotalEnd = 0x013f0150;
	pMQRegs->gc.gc04.HSync = 306 | (311L << 16);
	pMQRegs->gc.gc05.VSync = 336 | (337L << 16);

	pMQRegs->gc.gc06.HCounter = 309L << 16;
	pMQRegs->gc.gc07.VCounter = 311L << 16;
	pMQRegs->gc.gc00.Control = GxRCLK_PIXEL_CLK | IM_ENABLE | GC_8BPP | FD_15 | SD_5;
	pMQRegs->gc.gc01.PwrSeqControl = 0x00000200;
	pMQRegs->gc.gc0A.LineClock = 0x00f80000;
	pMQRegs->gc.gc0B.LineClockA = 0x006e8037;
	pMQRegs->gc.gc19.FrameClock = 0;
	pMQRegs->fp.fp00.Control = 0x00006020;
	pMQRegs->fp.fp01.PinControl = 0x000cd008;
	pMQRegs->fp.fp02.PinOutCtrl = 0xdffffcff;
	pMQRegs->fp.fp03.PinInCtrl = 0;
	pMQRegs->fp.fp04.STNPanelCtrl = 0x00bd0001;
	pMQRegs->fp.fp05.PinPolarityCtrl = 0x01000000;
	pMQRegs->fp.fp06.PinOutSelect0 = 0x80000000;
	pMQRegs->fp.fp07.PinOutSelect1 = 0x00020000;
	pMQRegs->fp.fp0B.AddinalPinOutSel = 0;
	pMQRegs->fp.fp0A.PinWeakPullDown = 0;
	pMQRegs->fp.fp0C.ModClockCtrl = 0x00000003;
	pMQRegs->fp.fp0F.PWMControl = 0xfff0;

	val = GC_16BPP_BP;
	reg = pMQRegs->fp.fp00.Control;
	if ((reg & FP_MONO) == FP_MONO)
		val |= 0x80;		// Monochrome mode
	reg = pMQRegs->gc.gc00.Control;
	reg &= ~GC_BPP_MASK;
	reg |= val;
	pMQRegs->gc.gc00.Control = reg;

	// Enable GE in PMU block
	pMQRegs->pm.pm00.ActiveCtrl |= 0x10UL;
	// Program GE clock to PLL1 / 2 = 67.5 MHz
	reg = pMQRegs->pm.pm08.GEConfig;
	reg &= ~0x0038UL;
	reg |= 0x4UL;
	pMQRegs->pm.pm08.GEConfig = reg;
	pMQRegs->ge.ge0A.DstStride = pMQRegs->gc.gc0E.Stride | 0x40000000;
	pMQRegs->ge.ge0B.BaseAddr = pMQRegs->gc.gc0C.StartAddr;

	// Enable display
	mdelay(40);
	pMQRegs->gc.gc00.Control |= 0x01UL;
	mdelay(40);
	reg = pMQRegs->cc.Reserved[0] & ~GPIO_05_MASK;
	pMQRegs->cc.Reserved[0] = reg | GPO_05_ENABLE | GPO_05_DATA_H;
	mdelay(40);
	mdelay(5);

	// Hardware cursor
	pMQRegs->gc.gc11.CursorStart = (((240 * 320 * 16 / 8) + 0x3ff) & 0xfffffc00) >> 10;
}

int __init katanafb_init(void)
{
	struct katanafb_info *fbi;
	int ret;

	// initialize panel
	init_panel();

	fbi = katanafb_init_fbinfo();
	ret = -ENOMEM;
	if (!fbi)
		goto failed;

	/* Initialize video memory */
	ret = katanafb_map_video_memory(fbi);
	if (ret)
		goto failed;

	katanafb_set_var(&fbi->fb.var, -1, &fbi->fb);

	ret = register_framebuffer(&fbi->fb);
	if (ret < 0)
		goto failed;

#ifdef CONFIG_PM
	/*
	 * Note that the console registers this as well, but we want to
	 * power down the display prior to sleeping.
	 */
	fbi->pm = pm_register(PM_SYS_DEV, PM_SYS_VGA, katanafb_pm_callback);
	if (fbi->pm)
		fbi->pm->data = fbi;
#endif
#ifdef CONFIG_CPU_FREQ
	fbi->clockchg.notifier_call = sa1100fb_clkchg_notifier;
	cpufreq_register_notifier(&fbi->clockchg);
#endif

	/* This driver cannot be unloaded at the moment */
	MOD_INC_USE_COUNT;

	return 0;

failed:
	if (fbi)
		kfree(fbi);
	printk("katana frame buffer driver initialize failed.\n");
	return ret;
}

int __init katanafb_setup(char *options)
{
	return 0;
}

MODULE_DESCRIPTION("KATANA (MQ9000) framebuffer driver");
MODULE_LICENSE("GPL");
