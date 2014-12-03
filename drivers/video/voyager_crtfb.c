/*
 *  linux/drivers/video/voyafb.c
 *
 *  Copyright (c) 2003 ATOM Create, Inc.
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 *  VOYAGER Frame Buffer Device Driver for SH4
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
#include <asm/voyagergx_reg.h>
#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>
#include "voyager.h"

/* frame buffer operations */

static int  voyafb_get_fix(struct fb_fix_screeninfo*, int, struct fb_info*);
static int  voyafb_get_var(struct fb_var_screeninfo*, int, struct fb_info*);
static int  voyafb_set_var(struct fb_var_screeninfo*, int, struct fb_info*);
static int  voyafb_get_cmap(struct fb_cmap*, int, int, struct fb_info*);
static int  voyafb_set_cmap(struct fb_cmap*, int, int, struct fb_info*);
static int  voyafb_ioctl(struct inode*, struct file*,
			   unsigned int, unsigned long, int, struct fb_info*);
static void voyafb_set_disp(int, struct fb_info*);

#ifdef CONFIG_PM
static struct pm_dev* fb_pm_dev;
#endif

static struct display disp[MAX_NR_CONSOLES];
static struct fb_info fb_info;
static struct fb_ops  voyafb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	voyafb_get_fix,
	fb_get_var:	voyafb_get_var,
	fb_set_var:	voyafb_set_var,
	fb_get_cmap:	voyafb_get_cmap,
	fb_set_cmap:	voyafb_set_cmap,
	fb_ioctl:	voyafb_ioctl,
};

static u_char* vram_base      = (u_char*)0;
static u_char* vram_base_phys = (u_char*)0;

static u16 fbcon_cmap_cfb16[16];

static int currcon = 0;

static char voyafb_name[] = "voyager_crt_fb";

static int voyafb_get_fix(struct fb_fix_screeninfo* fix, int con,
			    struct fb_info* info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	strcpy(fix->id, "voyager_crt_fb");

	fix->smem_start  = (unsigned long)vram_base_phys;
	fix->smem_len    = MAX_PIXEL_MEM_SIZE;
	fix->type        = FB_TYPE_PACKED_PIXELS;
	fix->type_aux    = 0;
	fix->visual      = FB_VISUAL_TRUECOLOR;
	fix->xpanstep    = 0;
	fix->ypanstep    = 0;
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
	fix->xwrapstep   = 0;
#else
	fix->ywrapstep   = 0;
#endif
        fix->line_length = LINE_LENGTH;

	return 0;
}
        
static int voyafb_get_var(struct fb_var_screeninfo* var, int con,
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

static int voyafb_set_var(struct fb_var_screeninfo* var, int con,
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

static int voyafb_getcolreg(unsigned regno, unsigned* red, unsigned* green,
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

static int voyafb_setcolreg(unsigned regno, unsigned red, unsigned green,
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

static int voyafb_get_cmap(struct fb_cmap* cmap, int kspc, int con,
			    struct fb_info* info)
{
	if (con == currcon)
		return fb_get_cmap(cmap, kspc, voyafb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(
		1 << fb_display[con].var.bits_per_pixel), cmap, kspc ? 0 : 2);

	return 0;
}

static int voyafb_set_cmap(struct fb_cmap* cmap, int kspc, int con,
			 struct fb_info* info)
{
	int err;

	if (!fb_display[con].cmap.len) {
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
				 1 << fb_display[con].var.bits_per_pixel, 0)))
			return err;
	}

	if (con == currcon) {
	        return fb_set_cmap(cmap, kspc, voyafb_setcolreg, info);
	}
	else {
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	}

	return 0;
}

static int voyafb_ioctl(struct inode* inode, struct file* file,
			 unsigned int cmd, unsigned long arg, int con,
			 struct fb_info* info)
{
	if(cmd == VOYAGER_IOCTL_ENABLE) {
		if(arg == 0) {
			*(volatile unsigned long *)(CRT_DISPLAY_CTRL) &= 0xfffffffb;
		}
		else {
			*(volatile unsigned long *)(CRT_DISPLAY_CTRL) |= 0x04;
		}
		return 0;
	}
	else if(cmd == VOYAGER_IOCTL_TYPE) {
		*(volatile unsigned long *)(CRT_DISPLAY_CTRL) &= 0xfffffffc;
		*(volatile unsigned long *)(CRT_DISPLAY_CTRL) |= (arg & 0x03);
		return 0;
	}
	else if(cmd == VOYAGER_IOCTL_SELECT) {
		if(arg == 0) {
			*(volatile unsigned long *)(CRT_DISPLAY_CTRL) &= 0xfffffdff;
		}
		else {
			*(volatile unsigned long *)(CRT_DISPLAY_CTRL) |= 0x00000200;
		}
		return 0;
	}
	return -EINVAL;
}


static int __init voyafb_map_video_memory(void)
{
	if (vram_base != NULL)
		return -EINVAL;

        vram_base_phys = (u_char *)VOY_VRAM_TOP6;
	vram_base = (u_char*)ioremap((u_long)vram_base_phys,
				     ALLOCATED_FB_MEM_SIZE);
	return (vram_base == NULL ? -EINVAL : 0);
}

static void voyafb_set_disp(int con, struct fb_info* info)
{
	struct fb_fix_screeninfo fix;
	struct display* display;

	voyafb_get_fix(&fix, con, info);

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
	display->can_soft_blank = 0;
	display->inverse        = 0;
	display->line_length    = fix.line_length;
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
	display->scrollmode     = SCROLL_XREDRAW;
#else
	display->scrollmode     = SCROLL_YREDRAW;
#endif

#if defined(FBCON_HAS_CFB16)
	display->dispsw         = &fbcon_cfb16;
	disp->dispsw_data       = fbcon_cmap_cfb16;
#else
#error NO FBCON_HAS_CFB16
	display->dispsw         = &fbcon_dummy;
#endif
}
  
static void voyafb_disable_voy_controller(void)
{
	*(volatile unsigned long *)(CRT_DISPLAY_CTRL) &= 0xfffffffb;
}

static void voyafb_enable_voy_controller(void)
{
int	i;

	*(volatile unsigned long *)(CRT_DISPLAY_CTRL) &= 0xfffffcf0;
	*(volatile unsigned long *)(CRT_FB_ADDRESS) = 0x80000000 + (VOY_VRAM_TOP6 & 0x00ffffff);
	*(volatile unsigned long *)(CRT_FB_WIDTH) = 0x5000500;
	*(volatile unsigned long *)(CRT_HORIZONTAL_TOTAL) = 0x033f027f;
	*(volatile unsigned long *)(CRT_HORIZONTAL_SYNC) = 0x4a028b;
	*(volatile unsigned long *)(CRT_VERTICAL_TOTAL) = 0x20c01df;
	*(volatile unsigned long *)(CRT_VERTICAL_SYNC) = 0x201e9;
	*(volatile unsigned long *)(CRT_DISPLAY_CTRL) |= 0x70000105;
	//palet initialize
	for(i=0;i<256;i++) {
		*(volatile unsigned long *)(CRT_PALETTE_RAM+(i*4)) = (i << 16)+(i << 8)+i;
	}
}

static int voyafb_updatevar(int con, struct fb_info* info)
{
	return 0;
}

static void voyafb_blank(int blank, struct fb_info* info)
{
	if (blank) {
		voyafb_disable_voy_controller();
	}
	else {
		voyafb_enable_voy_controller();
	}
}

static int voyafb_switch(int con, struct fb_info* info)
{ 
	currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;
	voyafb_set_var(&fb_display[con].var, con, info);
	return 0;
}

#ifdef CONFIG_PM
static int voyafb_pm_callback(struct pm_dev* pm_dev,
				pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
		voyafb_disable_voy_controller();
		break;
	case PM_RESUME:
		voyafb_enable_voy_controller();
		break;
	}

	return 0;
}
#endif

int __init voyafb_init6(void)
{
	fb_info.changevar   = NULL;
	strcpy(&fb_info.modename[0], voyafb_name);
	fb_info.fontname[0] = 0;
	fb_info.disp        = disp;
	fb_info.switch_con  = &voyafb_switch;
	fb_info.updatevar   = &voyafb_updatevar;
	fb_info.blank       = &voyafb_blank;	
	fb_info.node        = -1;
	fb_info.fbops       = &voyafb_ops;
	fb_info.flags       = FBINFO_FLAG_DEFAULT;
	
	if (voyafb_map_video_memory() < 0)
		return -EINVAL;
	memset(vram_base_phys,0,MAX_FRAMEBUFFER_MEM_SIZE);

        voyafb_get_var(&disp[0].var, 0, &fb_info);
	voyafb_set_disp(-1, &fb_info);

	if (register_framebuffer(&fb_info) < 0) {
#ifdef DEBUG
		printk(KERN_ERR
		       "unable to register VOYAGER frame buffer crt plane \n");
#endif
		return -EINVAL;
	}

#ifdef CONFIG_PM
	fb_pm_dev = pm_register(PM_SYS_DEV, 0, voyafb_pm_callback);
#endif
	voyafb_enable_voy_controller();

	printk("VOYAGER frame buffer crt plane initialized.\n");

	return 0;
}
