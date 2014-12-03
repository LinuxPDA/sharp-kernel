/*
 * linux/drivers/video/l7200fb.c
 * 
 * Color LCD Frame Buffer Device for LinkUp 7200
 * 
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *  drivers/video/q40fb.c
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
#include <video/fbcon-cfb16.h>

/* frame buffer operations */

static int  l7200fb_get_fix(struct fb_fix_screeninfo*, int, struct fb_info*);
static int  l7200fb_get_var(struct fb_var_screeninfo*, int, struct fb_info*);
static int  l7200fb_set_var(struct fb_var_screeninfo*, int, struct fb_info*);
static int  l7200fb_get_cmap(struct fb_cmap*, int, int, struct fb_info*);
static int  l7200fb_set_cmap(struct fb_cmap*, int, int, struct fb_info*);
static int  l7200fb_ioctl(struct inode*, struct file*,
			  unsigned int, unsigned long, int, struct fb_info*);

static void l7200fb_set_disp(int, struct fb_info*);

#ifdef CONFIG_PM
static struct pm_dev *fb_pm_dev;
#endif

static struct display disp[MAX_NR_CONSOLES];
static struct fb_info fb_info;
static struct fb_ops  l7200fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	l7200fb_get_fix,
	fb_get_var:	l7200fb_get_var,
	fb_set_var:	l7200fb_set_var,
	fb_get_cmap:	l7200fb_get_cmap,
	fb_set_cmap:	l7200fb_set_cmap,
	fb_ioctl:	l7200fb_ioctl,
};

#if defined(CONFIG_L7205SDB)

#ifdef CONFIG_FB_L7200_VGA_DSTN

#define XRES 640
#define YRES 480
#define BPP  16

#else

#define XRES 240
#define YRES 320
#define BPP  16

#endif

#elif defined(CONFIG_LYRA1)

#define XRES 240
#define YRES 320
#define BPP  16

#endif

#define MAX_PIXEL_MEM_SIZE ((XRES * YRES * BPP) / 8)
#define LINE_LENGTH        ((XRES * BPP) / 8)
#define MAX_FRAMEBUFFER_MEM_SIZE (MAX_PIXEL_MEM_SIZE)
#define ALLOCATED_FB_MEM_SIZE \
	(PAGE_ALIGN(MAX_FRAMEBUFFER_MEM_SIZE + PAGE_SIZE))

static u_char* vram_base      = (u_char*)0;
static u_char* vram_base_phys = (u_char*)0;

static u16 fbcon_cmap_cfb16[16];

static int currcon = 0;

static char l7200fb_name[] = "l7200fb";

static int l7200fb_get_fix(struct fb_fix_screeninfo* fix, int con,
                           struct fb_info* info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	strcpy(fix->id, "l7200fb");

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
        
static int l7200fb_get_var(struct fb_var_screeninfo* var, int con,
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

static int l7200fb_set_var(struct fb_var_screeninfo* var, int con,
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

static int l7200fb_getcolreg(unsigned regno, unsigned* red, unsigned* green,
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

static int l7200fb_setcolreg(unsigned regno, unsigned red, unsigned green,
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

static int l7200fb_get_cmap(struct fb_cmap* cmap, int kspc, int con,
			    struct fb_info* info)
{
	if (con == currcon)
		return fb_get_cmap(cmap, kspc, l7200fb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(
		1 << fb_display[con].var.bits_per_pixel), cmap, kspc ? 0 : 2);

	return 0;
}

static int l7200fb_set_cmap(struct fb_cmap* cmap, int kspc, int con,
			 struct fb_info* info)
{
	int err;

	if (!fb_display[con].cmap.len)
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
				 1 << fb_display[con].var.bits_per_pixel, 0)))
			return err;

	if (con == currcon)
	        return fb_set_cmap(cmap, kspc, l7200fb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

	return 0;
}

static int l7200fb_ioctl(struct inode* inode, struct file* file,
			 unsigned int cmd, unsigned long arg, int con,
			 struct fb_info* info)
{
	return -EINVAL;
}

static int __init l7200fb_map_video_memory(void)
{
	u_int required_pages;
	u_int extra_pages;
	u_int order;
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
					ALLOCATED_FB_MEM_SIZE,
					L_PTE_PRESENT  |
					L_PTE_YOUNG    |
					L_PTE_DIRTY    |
					L_PTE_WRITE);

	return (vram_base == NULL ? -EINVAL : 0);
}

static void l7200fb_set_disp(int con, struct fb_info* info)
{
	struct fb_fix_screeninfo fix;
	struct display* display;

	l7200fb_get_fix(&fix, con, info);

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

#ifdef FBCON_HAS_CFB16
	display->dispsw         = &fbcon_cfb16;
	disp->dispsw_data       = fbcon_cmap_cfb16;
#else
	display->dispsw         = &fbcon_dummy;
#endif
}
  
static void l7200fb_disable_lcd_controller(void)
{
	IO_CLCDCON &= ~CLCDPWR;
}

void l7200fb_enable_lcd_controller(void)
{
	/* Turn off Color LCD power before config */
	IO_CLCDCON &= ~CLCDPWR;

	/* Delay 20ms */
	mdelay(20);

#if defined(CONFIG_L7205SDB)
	/* Turn on CLCD contrast voltage */
	IO_PEDDR |= 0xff;
	IO_PEDR &= ~0x04;
#elif defined(CONFIG_LYRA1)
	/* Turn on CLCD contrast voltage */
	IO_PEDDR |= 0x02;
	IO_PEDR &= ~0x02;
	/* Turn on Bright voltage */
	IO_PADDR |= 0x08;
	IO_PADR &= ~0x08;
#endif

	/* Delay 20ms */
	mdelay(20);

	/* Disable Color LCD before config */
	IO_CLCDCON &= ~CLCDEN;

	/* set control register */
	IO_CLCDCON &= (CLCDEN | CLCDPWR);
	IO_CLCDCON |= (0x03 << 1);  /* 16 bpp */

	IO_CLCDBAR0 = vram_base_phys;
#if defined(CONFIG_L7205SDB)
	IO_CLCDBAR1 = vram_base_phys + (MAX_PIXEL_MEM_SIZE / 2);
#endif
	
	/* set interrupt mask */
	IO_CLCDMASK = 0;

#if defined(CONFIG_L7205SDB)

#ifdef CONFIG_FB_L7200_VGA_DSTN

#define	ACB	12 
#define	LED	15 
#define	HSW	7  
#define	HFP	15 
#define	HBP	31 
#define	VSW	0  
#define	VFP	0  
#define	VBP	0  
#define PCD	10 
#define CPL	(YRES / 2  - 1)
#define PPL	(XRES / 16 - 1)
#define LPS	(YRES / 2  - 1)  

	/* set clock divisor and clocks per line */
	IO_CLCDTIMING0 =
	  (PPL << 2) | (HSW << 8) | (HFP << 16) | (HBP << 24);
	IO_CLCDTIMING1 = LPS | (VSW <<10) | (VFP << 16) | (VBP << 24);
	IO_CLCDTIMING2 = PCD | (ACB << 6) | (CPL << 16);
	IO_CLCDTIMING3 = LED | (1 << 16);
	
#else

#define	ACB	12
/////#define	LED	0
#define	LED	15
#define	LEE	0
#define	HSW	29
#define	HFP	6
#define	HBP	46
#define	VSW	0
#define	VFP	0  
#define	VBP	0  
////#define	VSW	2
////#define	VFP	1  
////#define	VBP	18  
////#define PCD	10 
#define PCD	5
#define IVS	0 
#define IHS	0 
#define IPC	0 
#define CPL	(HSW + HBP + XRES + HFP - 1)
#define PPL	(XRES / 16 - 1)
#define LPS	(YRES - 1)  

	/* set clock divisor and clocks per line */
	IO_CLCDTIMING0 =
	  (PPL << 2) | (HSW << 8) | (HFP << 16) | (HBP << 24);
	IO_CLCDTIMING1 = LPS | (VSW <<10)  | (VFP << 16) | (VBP << 24);
	IO_CLCDTIMING2 = PCD | (ACB << 6)  | (IVS << 11) | (IHS << 12)
	                     | (IPC << 13) | (CPL << 16);
	IO_CLCDTIMING3 = LED | (LEE << 16);

#endif

#elif defined(CONFIG_LYRA1)

#define	ACB	12
/////#define	LED	0
#define	LED	15
#define	LEE	0
#define	HSW	29
#define	HFP	6
#define	HBP	46
#define	VSW	0
#define	VFP	0  
#define	VBP	0  
////#define	VSW	2
////#define	VFP	1  
////#define	VBP	18  
////#define PCD	10 
#define PCD	5
#define IVS	0 
#define IHS	0 
#define IPC	0 
#define CPL	(HSW + HBP + XRES + HFP - 1)
#define PPL	(XRES / 16 - 1)
#define LPS	(YRES - 1)  

	/* set clock divisor and clocks per line */
	IO_CLCDTIMING0 =
	  (PPL << 2) | (HSW << 8) | (HFP << 16) | (HBP << 24);
	IO_CLCDTIMING1 = LPS | (VSW <<10)  | (VFP << 16) | (VBP << 24);
	IO_CLCDTIMING2 = PCD | (ACB << 6)  | (IVS << 11) | (IHS << 12)
	                     | (IPC << 13) | (CPL << 16);
	IO_CLCDTIMING3 = LED | (LEE << 16);
	
#endif

#if defined(CONFIG_L7205SDB)

	/* Enable Color LCD */
#ifdef CONFIG_FB_L7200_VGA_DSTN
	IO_CLCDCON |= (CLCDEN | 0x800040);
#else
	IO_CLCDCON |= CLCDEN;
#endif

#elif defined(CONFIG_LYRA1)
	/* Enable Color STN LCD */
/////	IO_CLCDCON |= (CLCDEN | 0x800000);
/////	IO_CLCDCON |= (CLCDEN | 0x800040);
	IO_CLCDCON |= CLCDEN;
#endif

	/* Delay 20ms */
	mdelay(20);

#if defined(CONFIG_L7205SDB)
	/* Turn on CLCD contrast voltage */
	IO_PEDDR |= 0xff;
	IO_PEDR  |= 0x04;
#elif defined(CONFIG_LYRA1)
	/* Turn on CLCD contrast voltage */
	IO_PEDDR |= 0x02;
	IO_PEDR  |= 0x02;
	/* Turn on Bright voltage */
////	IO_PADDR |= 0x08;
////	IO_PADR  |= 0x08;
#endif

	/* Delay 20ms */
	mdelay(20);

	/* Turn on Color LCD power */
	IO_CLCDCON |= CLCDPWR;
}

static int l7200fb_updatevar(int con, struct fb_info* info)
{
	return 0;
}

static void l7200fb_blank(int blank, struct fb_info* info)
{
	if (blank) {
		l7200fb_disable_lcd_controller();
	}
	else {
		l7200fb_enable_lcd_controller();
	}
}

static int l7200fb_switch(int con, struct fb_info* info)
{ 
	currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;
	l7200fb_set_var(&fb_display[con].var, con, info);
	return 0;
}

#ifdef CONFIG_PM
static int l7200fb_pm_callback(struct pm_dev* pm_dev,
			       pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
		l7200fb_disable_lcd_controller();
		break;
	case PM_RESUME:
		l7200fb_enable_lcd_controller();
		break;
	}

	return 0;
}
#endif

int __init l7200fb_init(void)
{
	fb_info.changevar   = NULL;
	strcpy(&fb_info.modename[0], l7200fb_name);
	fb_info.fontname[0] = 0;
	fb_info.disp        = disp;
	fb_info.switch_con  = &l7200fb_switch;
	fb_info.updatevar   = &l7200fb_updatevar;
	fb_info.blank       = &l7200fb_blank;	
	fb_info.node        = -1;
	fb_info.fbops       = &l7200fb_ops;
	fb_info.flags       = FBINFO_FLAG_DEFAULT;
	
	if (l7200fb_map_video_memory() < 0)
		return -EINVAL;

        l7200fb_get_var(&disp[0].var, 0, &fb_info);
	l7200fb_set_disp(-1, &fb_info);

	if (register_framebuffer(&fb_info) < 0) {
		printk(KERN_ERR
		       "unable to register LinkUp 7200 frame buffer\n");
		return -EINVAL;
	}
#ifdef CONFIG_PM
	fb_pm_dev = pm_register(PM_SYS_DEV, 0, l7200fb_pm_callback);
#endif
	l7200fb_enable_lcd_controller();

	return 0;
}	
