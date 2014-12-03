/*
 * linux/drivers/video/colliefb.c
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * Based on:
 *  drivers/video/collie.c
 *  Copyright (C) 1999 Eric A. Thomas
 *  
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * ChangeLog:
 *	14-Nov-2001 SHARP Corporation
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
#include <linux/vmalloc.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/proc/pgtable.h>
#include <asm/arch/m62332.h>
#include <asm/arch/tc35143.h>
#include <asm/ucb1200.h>

extern void ucb1200_init(void);

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>


/*
 *  Debug macros 
 */
// #define DEBUG 1
#ifdef DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif


#define MAX_PALETTE_NUM_ENTRIES	256
#define MAX_PALETTE_MEM_SIZE	(MAX_PALETTE_NUM_ENTRIES * 2)
#define MAX_PIXEL_MEM_SIZE \
	((current_par.max_xres * current_par.max_yres * current_par.max_bpp)/8)
#define MAX_FRAMEBUFFER_MEM_SIZE \
	(MAX_PIXEL_MEM_SIZE + MAX_PALETTE_MEM_SIZE + 32)
#define ALLOCATED_FB_MEM_SIZE \
	(PAGE_ALIGN(MAX_FRAMEBUFFER_MEM_SIZE + PAGE_SIZE * 2))

#define COLLIE_PALETTE_MEM_SIZE(bpp)	(((bpp)==8?256:16)*2)
#define COLLIE_PALETTE_MODE_VAL(bpp)    (((bpp) & 0x018) << 9)

#define MIN_XRES 64
#define MIN_YRES 64

/* Possible controller_state modes */
#define LCD_MODE_DISABLED		0
#define LCD_MODE_DISABLE_BEFORE_ENABLE	1
#define LCD_MODE_ENABLED		2

#define COLLIE_NAME	"colliefb"
#define NR_MONTYPES	1


#define CF_BUF_CTRL_BASE 0xF0800000
#define SCP_REG(adr) (*(volatile unsigned short*)(CF_BUF_CTRL_BASE+(adr)))


#ifdef CONFIG_PM
static struct pm_dev *fb_pm_dev;
#endif
static int collie_fb_blank = 0;
static int is_collie_fb_blank = 0;

static u_char *VideoMemRegion;
static u_char *VideoMemRegion_phys;

static u_char *BackUpVideoMemRegion;



/* Local LCD controller parameters */
/* These can be reduced by making better use of fb_var_screeninfo parameters.  */
/* Several duplicates exist in the two structures. */
struct colliefb_par {
	u_char*		p_screen_base;
	u_char*		v_screen_base;
	u_short*	p_palette_base;
	u_short*	v_palette_base;
	unsigned long	screen_size;
	unsigned int	palette_size;
	unsigned int	max_xres;
	unsigned int	max_yres;
	unsigned int	xres;
	unsigned int	yres;
	unsigned int	xres_virtual;
	unsigned int	yres_virtual;
	unsigned int	max_bpp;
	unsigned int	bits_per_pixel;
	  signed int	montype;
	unsigned int	currcon;
	unsigned int	visual;
	unsigned int	allow_modeset : 1;
	unsigned int	active_lcd : 1;
	unsigned int	inv_4bpp : 1;
	volatile u_char	controller_state;
};

/* Shadows for LCD controller registers */
struct colliefb_lcd_reg {
	Address	dbar1;
	Address dbar2;
	Word	lccr0;
	Word	lccr1;
	Word	lccr2;
	Word	lccr3;
};

/* Fake monspecs to fill in fbinfo structure */
static struct fb_monspecs monspecs __initdata = {
	 30000, 70000, 50, 65, 0 	/* Generic */
};

static struct display global_disp;
static struct fb_info fb_info;
static struct colliefb_par current_par;
static struct fb_var_screeninfo __initdata init_var = {};
static struct colliefb_lcd_reg lcd_shadow;


static int  colliefb_get_fix(struct fb_fix_screeninfo *fix,
			     int con, struct fb_info *info);
static int  colliefb_get_var(struct fb_var_screeninfo *var,
			     int con, struct fb_info *info);
static int  colliefb_set_var(struct fb_var_screeninfo *var,
			     int con, struct fb_info *info);
static int  colliefb_get_cmap(struct fb_cmap *cmap,
			      int kspc, int con, struct fb_info *info);
static int  colliefb_set_cmap(struct fb_cmap *cmap,
			      int kspc, int con, struct fb_info *info);
 
static int  colliefb_switch(int con, struct fb_info *info);
static void colliefb_blank(int blank, struct fb_info *info);
static int  colliefb_map_video_memory(void);
static int  colliefb_activate_var(struct fb_var_screeninfo *var);
static void colliefb_enable_lcd_controller(void);
static void colliefb_disable_lcd_controller(void);
void colliefb_disable_lcd_controller_emeregency(void);

static int screen_backup(void);
static int screen_restore(void);
static int colliefb_map_backup_video_memory(void);

static struct fb_ops colliefb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	colliefb_get_fix,
	fb_get_var:	colliefb_get_var,
	fb_set_var:	colliefb_set_var,
	fb_get_cmap:	colliefb_get_cmap,
	fb_set_cmap:	colliefb_set_cmap,
};

static inline void colliefb_palette_write(u_int regno, u_short pal)
{
	current_par.v_palette_base[regno] = 
		(regno ? pal : pal | 
		 COLLIE_PALETTE_MODE_VAL(current_par.bits_per_pixel));
}

static inline u_short colliefb_palette_encode(u_int regno,
					      u_int red,
					      u_int green,
					      u_int blue,
					      u_int trans)
{
	u_int pal;

	if (current_par.bits_per_pixel == 8) {
		pal   = ((red   >>  4) & 0xf00);
		pal  |= ((green >>  8) & 0x0f0);
		pal  |= ((blue  >> 12) & 0x00f);
	} else {
		pal =  (red   & 0xf800);
	       	pal |= ((green & 0xfc00) >>  5);
		pal |= ((blue  & 0xf800) >> 11);
	}

	return pal;
}
	    
static inline u_short colliefb_palette_read(u_int regno)
{
	if (current_par.bits_per_pixel == 8)
		return (current_par.v_palette_base[regno] & 0x0FFF);
	else
		return (current_par.v_palette_base[regno] & 0xFFFF);
}


static void colliefb_palette_decode(u_int regno,
				    u_int *red,
				    u_int *green,
				    u_int *blue,
				    u_int *trans)
{
	u_short pal;

	pal = colliefb_palette_read(regno);

	if (current_par.bits_per_pixel == 8) {
		*blue   = (pal & 0x000f) << 12;
		*green  = (pal & 0x00f0) << 8;
		*red    = (pal & 0x0f00) << 4;
	} else {
		*blue   = (pal & 0x001f) << 11;
		*green  = (pal & 0x07e0) << 5;
		*red    = (pal & 0xf800);
	}
        *trans  = 0;
}

static int colliefb_getcolreg(u_int regno,
			      u_int *red,
			      u_int *green,
			      u_int *blue,
			      u_int *trans,
			      struct fb_info *info)
{
	if (regno >= current_par.palette_size)
		return 1;
	
	colliefb_palette_decode(regno, red, green, blue, trans);

	return 0;
}


static int colliefb_setcolreg(u_int regno,
			      u_int red,
			      u_int green,
			      u_int blue,
			      u_int trans,
			      struct fb_info *info)
{
	u_short pal;

	if (regno >= current_par.palette_size)
		return 1;

	pal = colliefb_palette_encode(regno, red, green, blue, trans);

	colliefb_palette_write(regno, pal);

	return 0;
}

static int colliefb_get_cmap(struct fb_cmap *cmap,
			     int kspc,
			     int con,
			     struct fb_info *info)
{
	int err = 0;

	if (con == current_par.currcon) {
		err = fb_get_cmap(cmap, kspc, colliefb_getcolreg, info);
	}
	else if (fb_display[con].cmap.len) {
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	}
	else {
		fb_copy_cmap(fb_default_cmap(current_par.palette_size),
			     cmap, kspc ? 0 : 2);
	}
	return err;
}

static int colliefb_set_cmap(struct fb_cmap *cmap,
			     int kspc,
			     int con,
			     struct fb_info *info)
{
	int err = 0;

	if (!fb_display[con].cmap.len) {
		err = fb_alloc_cmap(&fb_display[con].cmap,
				    current_par.palette_size, 0);
	}
	if (!err) {
		if (con == current_par.currcon) {
			err = fb_set_cmap(cmap, kspc, colliefb_setcolreg,
					  info);
		}
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	}
	return err;
}

static void colliefb_install_cmap(int con, struct fb_info *info)
{
	if (con != current_par.currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, colliefb_setcolreg, info);
	else {
		int size = current_par.palette_size;
		current_par.palette_size 
			= COLLIE_PALETTE_MEM_SIZE(fb_display[con].var.bits_per_pixel) 
				/ 2;
		fb_set_cmap(fb_default_cmap(current_par.palette_size), 
				1, colliefb_setcolreg, info);
		current_par.palette_size = size;
	}
}

static void inline
colliefb_get_par(struct colliefb_par *par)
{
	*par = current_par;
}

static int colliefb_encode_var(struct fb_var_screeninfo *var,
			       struct colliefb_par *par)
{
	var->xres = par->xres;
	var->yres = par->yres;
	var->xres_virtual = par->xres_virtual;
	var->yres_virtual = par->yres_virtual;

	var->bits_per_pixel = par->bits_per_pixel;

	var->red.length    = 5;
	var->blue.length   = 5;
	var->green.length  = 6;
	var->transp.length = 0;
	var->red.offset    = 11;
	var->green.offset  = 5;
	var->blue.offset   = 0;
	var->transp.offset = 0;

	return 0;
}
 
static int colliefb_decode_var(struct fb_var_screeninfo *var, 
                    struct colliefb_par *par)
{
	u_long palette_mem_phys;
	u_long palette_mem_size;

	*par = current_par;

	if ((par->xres = var->xres) < MIN_XRES) {
		par->xres = MIN_XRES; 
	}
	if ((par->yres = var->yres) < MIN_YRES) {
		par->yres = MIN_YRES;
	}
	if (par->xres > current_par.max_xres) {
		par->xres = current_par.max_xres;
	}
	if (par->yres > current_par.max_yres) {
		par->yres = current_par.max_yres; 
	}
	par->xres_virtual = 
		var->xres_virtual < par->xres ? par->xres : var->xres_virtual;
        par->yres_virtual = 
		var->yres_virtual < par->yres ? par->yres : var->yres_virtual;
        par->bits_per_pixel = var->bits_per_pixel;

	par->visual = FB_VISUAL_TRUECOLOR;
	par->palette_size = 0; 

	palette_mem_size = COLLIE_PALETTE_MEM_SIZE(par->bits_per_pixel);
	palette_mem_phys = (u_long)VideoMemRegion_phys +
				PAGE_SIZE - palette_mem_size;
	par->p_palette_base = (u_short *)palette_mem_phys;
        par->v_palette_base = (u_short *)((u_long)VideoMemRegion +
					  PAGE_SIZE - palette_mem_size);
	par->p_screen_base  = (u_char *)((u_long)VideoMemRegion_phys +
					 PAGE_SIZE); 
	par->v_screen_base  = (u_char *)((u_long)VideoMemRegion +
					 PAGE_SIZE); 

	return 0;
}

static int colliefb_get_var(struct fb_var_screeninfo *var,
			    int con, struct fb_info *info)
{
	struct colliefb_par par;

	if (con == -1) {
		colliefb_get_par(&par);
		colliefb_encode_var(var, &par);
	}
	else {
		*var = fb_display[con].var;
	}

	return 0;
}

static int colliefb_set_var(struct fb_var_screeninfo *var,
			    int con, struct fb_info *info)
{
	struct display *display;
	int err, chgvar = 0;
	struct colliefb_par par;

	if (con >= 0) {
		display = &fb_display[con]; /* Display settings for console */
	}
	else {
		display = &global_disp;     /* Default display settings */
	}

	if ((err = colliefb_decode_var(var, &par)))
		return err;

	colliefb_encode_var(var, &par);
       
	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_TEST) {
		return 0;
	}
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
		    (memcmp(&display->var.red, &var->red,
			    sizeof(var->red))) ||
		    (memcmp(&display->var.green, &var->green,
			    sizeof(var->green))) ||
		    (memcmp(&display->var.blue, &var->blue,
			    sizeof(var->blue)))) 
			chgvar = 1;
	}

	display->var = *var;
	display->screen_base	= par.v_screen_base;
	display->visual		= par.visual;
	display->type		= FB_TYPE_PACKED_PIXELS;
	display->type_aux	= 0;
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
	display->xpanstep	= 0;
	display->xwrapstep	= 0;
#else
	display->ypanstep	= 0;
	display->ywrapstep	= 0;
#endif
	display->line_length	= 
	display->next_line      = (var->xres * var->bits_per_pixel) / 8;

	display->can_soft_blank	= 1;
	display->inverse	= 0;

	display->dispsw = &fbcon_cfb16;
	display->dispsw_data = par.v_palette_base;

	if (chgvar && info && info->changevar) {
		info->changevar(con);
	}

	if (con == current_par.currcon) {
		colliefb_activate_var(var);
	}
	
	return 0;
}

static int colliefb_updatevar(int con, struct fb_info *info)
{
	return 0;
}

static int colliefb_get_fix(struct fb_fix_screeninfo *fix,
			    int con, struct fb_info *info)
{
	struct display *display;

	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, COLLIE_NAME);

	if (con >= 0) {
		display = &fb_display[con];  /* Display settings for console */
	}
	else {
		display = &global_disp;      /* Default display settings */
	}

	fix->smem_start	 = (unsigned long)current_par.p_screen_base;
	fix->smem_len	 = current_par.screen_size;
	fix->type	 = display->type;
	fix->type_aux	 = display->type_aux;
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
	fix->xpanstep	 = display->xpanstep;
	fix->ypanstep	 = 0;
	fix->xwrapstep	 = display->xwrapstep;
#else
	fix->xpanstep	 = 0;
	fix->ypanstep	 = display->ypanstep;
	fix->ywrapstep	 = display->ywrapstep;
#endif
	fix->visual	 = display->visual;
	fix->line_length = display->line_length;
	fix->accel	 = FB_ACCEL_NONE;

	return 0;
}

static void colliefb_init_fbinfo(void)
{
	strcpy(fb_info.modename, COLLIE_NAME);
	strcpy(fb_info.fontname, "Acorn8x8");

	fb_info.node		= -1;
	fb_info.flags		= FBINFO_FLAG_DEFAULT;
	fb_info.fbops		= &colliefb_ops;
        fb_info.monspecs	= monspecs;
	fb_info.disp		= &global_disp;
	fb_info.changevar	= NULL;
	fb_info.switch_con	= colliefb_switch;
	fb_info.updatevar	= colliefb_updatevar;
	fb_info.blank		= colliefb_blank;

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

	current_par.max_xres	= 320;
	current_par.max_yres	= 240;
	current_par.max_bpp	= 16;
	init_var.red.length	= 5;				
	init_var.green.length	= 6;
	init_var.blue.length	= 5;
	init_var.grayscale	= 0;
	init_var.sync		= 0;
	init_var.pixclock	= 171521;

	current_par.p_palette_base	= NULL;
	current_par.v_palette_base	= NULL;
	current_par.p_screen_base	= NULL;
	current_par.v_screen_base	= NULL;
	current_par.palette_size	= MAX_PALETTE_NUM_ENTRIES;
	current_par.screen_size		= MAX_PIXEL_MEM_SIZE;
	current_par.montype		= -1;
	current_par.currcon		= -1;
	current_par.allow_modeset	=  1;
	current_par.controller_state	= LCD_MODE_DISABLED;

	init_var.xres			= current_par.max_xres;
	init_var.yres			= current_par.max_yres;
	init_var.xres_virtual		= init_var.xres;
	init_var.yres_virtual		= init_var.yres;
	init_var.bits_per_pixel		= current_par.max_bpp;
}

static int colliefb_map_video_memory(void)
{
	u_int  required_pages;
	u_int  extra_pages;
	u_int  order;
        struct page *page;
        char   *allocated_region;

	if (VideoMemRegion != NULL)
		return -EINVAL;

	required_pages = ALLOCATED_FB_MEM_SIZE >> PAGE_SHIFT;
        for (order = 0 ; required_pages >> order ; order++) {;}
        extra_pages = (1 << order) - required_pages;

        if (!(allocated_region = (char*)__get_free_pages(GFP_KERNEL, order))) {
		return -ENOMEM;
	}

        VideoMemRegion = (u_char *)allocated_region +
			(extra_pages << PAGE_SHIFT); 
        VideoMemRegion_phys = (u_char *)__virt_to_phys((u_long)VideoMemRegion);

	for (;extra_pages; extra_pages--) {
		free_page((u_int)allocated_region +
			  ((extra_pages-1) << PAGE_SHIFT));
	}

	for(page = virt_to_page(VideoMemRegion); 
	    page < virt_to_page(VideoMemRegion + ALLOCATED_FB_MEM_SIZE);
	    page++) {
		mem_map_reserve(page);
	}

	VideoMemRegion = (u_char *)__ioremap((u_long)VideoMemRegion_phys,
					     ALLOCATED_FB_MEM_SIZE,
					     L_PTE_PRESENT  |
					     L_PTE_YOUNG    |
					     L_PTE_DIRTY    |
					     L_PTE_WRITE);

	return (VideoMemRegion == NULL ? -EINVAL : 0);
}

static const int frequency[16] = {
	59000000,
        73700000,
        88500000,
        103200000,
        118000000,
        132700000,
        147500000,
        162200000,
        176900000,
        191700000,
        206400000, 
	230000000,
	245000000,
	260000000,
	275000000,
	290000000
};

static inline int get_pcd(unsigned int pixclock)
{
	unsigned int pcd = 0;

	pcd = frequency[PPCR & 0xf] / 1000;
	pcd *= pixclock / 1000;
	pcd = pcd / 1000000;
	pcd++;

	return pcd;
}


#define COLLIE_72HZ
//#define COLLIE_60HZ
static int colliefb_activate_var(struct fb_var_screeninfo *var)
{						       
#ifdef COLLIE_60HZ
  	int pcd = get_pcd(var->pixclock);
#endif
#ifdef COLLIE_72HZ
	int pcd = 32;
#endif
	u_long	flags;


	if (current_par.p_palette_base == NULL)
		return -EINVAL;

	local_irq_save(flags);

	/* Reset the LCD Controller's DMA address if it has changed */
  	lcd_shadow.dbar1 = (Address)current_par.p_palette_base;
	lcd_shadow.dbar2 = (Address)(current_par.p_screen_base +
				     (current_par.xres * current_par.yres *
				      current_par.bits_per_pixel / 8 / 2));

	lcd_shadow.lccr0 = LCCR0_LEN + LCCR0_Color + LCCR0_Sngl + 
			   LCCR0_LDM + LCCR0_BAM + LCCR0_ERM + LCCR0_Act +
	                   LCCR0_LtlEnd + LCCR0_DMADel(0);
	lcd_shadow.lccr1 = LCCR1_DisWdth(var->xres) + LCCR1_HorSnchWdth(6-1) + 
#ifdef COLLIE_72HZ
	                   LCCR1_BegLnDel(11) + LCCR1_EndLnDel(30);
#endif
#ifdef COLLIE_60HZ
	                   LCCR1_BegLnDel(8) + LCCR1_EndLnDel(30);
#endif

	lcd_shadow.lccr2 = LCCR2_DisHght(var->yres) + LCCR2_VrtSnchWdth(1) + 
	                   LCCR2_BegFrmDel(2) + LCCR2_EndFrmDel(0);
	lcd_shadow.lccr3 = LCCR3_OutEnH + LCCR3_PixRsEdg + LCCR3_VrtSnchH + 
	                   LCCR3_HorSnchH + LCCR3_ACBsCntOff + 
	                   LCCR3_ACBsDiv(2) + LCCR3_PixClkDiv(pcd);


	local_irq_restore(flags);

	if (( LCCR0 != lcd_shadow.lccr0 ) ||
	    ( LCCR1 != lcd_shadow.lccr1 ) ||
	    ( LCCR2 != lcd_shadow.lccr2 ) ||
            ( LCCR3 != lcd_shadow.lccr3 ) ||
	    ( DBAR1 != lcd_shadow.dbar1 ) ||
	    ( DBAR2 != lcd_shadow.dbar2 )) {
		colliefb_enable_lcd_controller();
	}

  	return 0;
}


#if 1
static void colliefb_inter_handler(int irq, void* dev_id, struct pt_regs* regs)
{
	if (LCSR & LCSR_LDD) {
		int controller_state = current_par.controller_state;
		/* Disable Done Flag is set */
		LCCR0 |= LCCR0_LDM;	/* Mask LCD Disable Done Interrupt */
		current_par.controller_state = LCD_MODE_DISABLED;
		if (controller_state == LCD_MODE_DISABLE_BEFORE_ENABLE) {
			colliefb_enable_lcd_controller();
		}
	}

	LCSR = 0;	      /* Clear LCD Status Register */
}
#endif


static void colliefb_disable_lcd_controller(void)
{


	if ((current_par.controller_state == LCD_MODE_DISABLED) ||
	    (!(LCCR0 & LCCR0_LDM))) {
		return;
	}

	current_par.controller_state = LCD_MODE_DISABLED;

	LCM_TC   = ( 0x06 ); /* TFTCRST=1 | CPSOUT=1 | CPSEN = 0 */

	udelay(1000); /* 1ms */

	LCM_GPO &= ~LCM_GPIO4;

	mdelay(110);

	LCM_GPO &= ~LCM_GPIO6;

	mdelay(700);


	/* AMP OFF */
	if (!collie_fb_blank) {
		ucb1200_set_io_direction(TC35143_GPIO_AMP_ON, TC35143_IODIR_OUTPUT);
		ucb1200_set_io(TC35143_GPIO_AMP_ON, TC35143_IODAT_LOW);
		SCP_REG_GPWR &= ~SCP_AMP_ON;
	}


        LCM_TC   = ( 0x00 ); /* TFTCRST=0 | CPSOUT=0 | CPSEN = 0 */

	LCM_GPO &= ~LCM_GPIO7;

	LCSR = 0;		/* Clear LCD Status Register */
	LCCR0 &= ~(LCCR0_LDM);	/* Enable LCD Disable Done Interrupt */
	enable_irq(IRQ_LCD);	/* Enable LCD IRQ */
	LCCR0 &= ~(LCCR0_LEN);	/* Disable LCD Controller */
	while(!(LCSR & LCSR_LDD));

	LCM_GPO &= ~LCM_GPIO5;		// LCD Controler Off

}

void colliefb_disable_lcd_controller_emergency(void)
{
	LCM_TC   = ( 0x06 ); /* TFTCRST=1 | CPSOUT=1 | CPSEN = 0 */

	udelay(1000); /* 1ms */

	LCM_GPO &= ~LCM_GPIO4;

	mdelay(110);

	LCM_GPO &= ~LCM_GPIO6;

	mdelay(200);

	/* AMP OFF */
	if (!collie_fb_blank) {
		ucb1200_set_io_direction(TC35143_GPIO_AMP_ON, TC35143_IODIR_OUTPUT);
		ucb1200_set_io(TC35143_GPIO_AMP_ON, TC35143_IODAT_LOW);
		SCP_REG_GPWR &= ~SCP_AMP_ON;
	}

        LCM_TC   = ( 0x00 ); /* TFTCRST=0 | CPSOUT=0 | CPSEN = 0 */

	LCM_GPO &= ~LCM_GPIO7;

	LCSR = 0;		/* Clear LCD Status Register */
	LCCR0 &= ~(LCCR0_LDM);	/* Enable LCD Disable Done Interrupt */
	enable_irq(IRQ_LCD);	/* Enable LCD IRQ */
	LCCR0 &= ~(LCCR0_LEN);	/* Disable LCD Controller */
	while(!(LCSR & LCSR_LDD));

	LCM_GPO &= ~LCM_GPIO5;		// LCD Controler Off
}



u_long colliefb_get_comadj()
{
  int comadj;

  if ( FLASH_DATA(FLASH_COMADJ_MAGIC_ADR) == FLASH_COMADJ_MAJIC ) {
    comadj = FLASH_DATA(FLASH_COMADJ_DATA_ADR);
  } else {
#ifdef CONFIG_COLLIE_TR0
    comadj = 100;
#else
    //    comadj = 155;
    comadj = 128;
#endif
  }

  printk("comadj = %d,%x,%x\n",comadj,FLASH_DATA(FLASH_COMADJ_MAGIC_ADR),FLASH_COMADJ_MAJIC);

  return comadj;
}



static void colliefb_enable_lcd_controller(void)
{
	u_long	flags;
	u_long comadj;

	local_irq_save(flags);		

	if (current_par.controller_state == LCD_MODE_ENABLED) 	{
	  // current_par.controller_state = LCD_MODE_DISABLE_BEFORE_ENABLE;
	  // colliefb_disable_lcd_controller();
	  return;
	}


	current_par.controller_state = LCD_MODE_ENABLED;


	comadj = colliefb_get_comadj();

	LCM_GPD &= ~LCM_GPIO4;
	LCM_GPE &= ~LCM_GPIO4;
	LCM_GPO |= LCM_GPIO4;   /* LCD_VSHA_ON */

	udelay(2000); /* 2ms */

	LCM_GPD &= ~LCM_GPIO5;
	LCM_GPE &= ~LCM_GPIO5;
	LCM_GPO |= LCM_GPIO5;   /* LCD_VSHD_ON */

	udelay(2000); /* 2ms */
	
	/* AMP ON */
	ucb1200_set_io_direction(TC35143_GPIO_AMP_ON, TC35143_IODIR_OUTPUT);
	ucb1200_set_io(TC35143_GPIO_AMP_ON, TC35143_IODAT_HIGH);
	SCP_REG_GPWR |= SCP_AMP_ON;


	udelay(5000); /* 5ms */

	m62332_senddata(comadj, 0); /* COMADJ */

	udelay(5000); /* 5ms */

	LCM_GPD &= ~LCM_GPIO6;
	LCM_GPE &= ~LCM_GPIO6;
	LCM_GPO |= LCM_GPIO6;   /* LCD_VEE_ON */

	udelay(10000); /* 10ms */

	LCM_TC   = ( 0x01 ); /* TFTCRST | CPSOUT=0 | CPSEN */

	LCM_CPSD = 6;	/* Set CPSD */

	current_par.v_palette_base[0] &= 0x0FFF;
	current_par.v_palette_base[0] |= 
		COLLIE_PALETTE_MODE_VAL(current_par.bits_per_pixel);

	if(( ! (lcd_shadow.lccr0 & LCCR0_Mono)) && 
	   ((lcd_shadow.lccr0 & LCCR0_Dual) ||
	    (lcd_shadow.lccr0 & LCCR0_Act))) {
		GPDR |= 0x3fc;
		GAFR |= 0x3fc;
	}
 		
	LCCR3 = lcd_shadow.lccr3;
	LCCR2 = lcd_shadow.lccr2;
	LCCR1 = lcd_shadow.lccr1;
	LCCR0 = lcd_shadow.lccr0 & ~LCCR0_LEN;
	DBAR1 = lcd_shadow.dbar1;
	DBAR2 = lcd_shadow.dbar2;
	LCCR0 |= LCCR0_LEN;

	LCM_TC   = ( 0x04 | 0x01 ); /* TFTCRST | CPSOUT=0 | CPSEN */

	mdelay(10); /* 10ms */

	LCM_GPD &= ~LCM_GPIO7;
	LCM_GPE &= ~LCM_GPIO7;
	LCM_GPO |= LCM_GPIO7;   /* LCD_MODE */


	local_irq_restore(flags);
}

static void colliefb_blank(int blank, struct fb_info* info)
{
	collie_fb_blank = 1;

	if (blank) {
		colliefl_blank(1);
		if (!is_collie_fb_blank) {
			is_collie_fb_blank = 1;
			colliefb_disable_lcd_controller();
		}
	}
	else {
		if (is_collie_fb_blank) {
			colliefb_enable_lcd_controller();
			is_collie_fb_blank = 0;
		}
		colliefl_blank(0);
	}
	
	collie_fb_blank = 0;
}

static int colliefb_switch(int con, struct fb_info *info)
{
	current_par.currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;

	colliefb_set_var(&fb_display[con].var, con, info);
	colliefb_install_cmap(con, info);
	current_par.v_palette_base[0] = (current_par.v_palette_base[0] &
		0xcfff) | COLLIE_PALETTE_MODE_VAL(current_par.bits_per_pixel);

	return 0;
}

static int colliefb_map_backup_video_memory(void)
{
	u_int  required_pages;
	u_int  extra_pages;
	u_int  order;
        struct page *page;
        char   *allocated_region;


	if (BackUpVideoMemRegion != NULL)
		return -EINVAL;

	required_pages = ALLOCATED_FB_MEM_SIZE >> PAGE_SHIFT;
        for (order = 0 ; required_pages >> order ; order++) {;}
        extra_pages = (1 << order) - required_pages;

        if (!(allocated_region = (char*)__get_free_pages(GFP_KERNEL, order))) {
		return -ENOMEM;
	}

        BackUpVideoMemRegion = (u_char *)allocated_region + (extra_pages << PAGE_SHIFT); 

	for (;extra_pages; extra_pages--) {
		free_page((u_int)allocated_region +
			  ((extra_pages-1) << PAGE_SHIFT));
	}

	for(page = virt_to_page(BackUpVideoMemRegion); 
	    page < virt_to_page(BackUpVideoMemRegion + ALLOCATED_FB_MEM_SIZE);
	    page++) {
		mem_map_reserve(page);
	}


	return (BackUpVideoMemRegion == NULL ? -EINVAL : 0);
}


static int screen_backup(void)
{
	if ( BackUpVideoMemRegion != NULL ) {
	  	memcpy(BackUpVideoMemRegion , VideoMemRegion  ,ALLOCATED_FB_MEM_SIZE);
		memset(VideoMemRegion,0xff,ALLOCATED_FB_MEM_SIZE);
		mdelay(100);
		printk("screen backup!\n");
	} else {
		printk("backup fail!\n");
	}

	return 0;

}
static int screen_restore(void)
{

	if ( BackUpVideoMemRegion != NULL ) {
		memcpy(VideoMemRegion, BackUpVideoMemRegion , ALLOCATED_FB_MEM_SIZE);
		printk("screen restore!\n");
	}

	return 0;

}

#ifdef CONFIG_COLLIE_LOGO_SCREEN
#include "LogoScreen.c"
static void __init draw_logo_screen(void)
{
	int		x,y;
	unsigned short	*p = (unsigned short *)(VideoMemRegion+PAGE_SIZE);
	int		sx, sy;
	int		i;

	for (y=0; y<current_par.max_yres; y++) {
#if 1
		for (x=8; x<current_par.max_xres; x++) {
			p[x] = 0x7bef;
		}
#else
		for (x=0; x<current_par.max_xres; x++) {
			p[x] = ((15*x/current_par.max_xres)<<11)
			  + ((31*x/current_par.max_xres)<<5)
			  + (31*x/current_par.max_xres);
		}
#endif
		p += current_par.max_xres;
	}

	sx = (current_par.max_xres - logo_screen_width) / 2;
	sy = (current_par.max_yres - logo_screen_height) / 2;
	p = (unsigned short *)(VideoMemRegion+PAGE_SIZE);
	p += sy*current_par.max_xres;
	i = 0;
	for ( y=sy; y<sy+logo_screen_height; y++ ) {
		for ( x=sx; x<sx+logo_screen_width; x++ ) {
			unsigned short pix = logo_screen_data[i++];
#if 1
			p[x] = pix;
#else
			if (pix) {
				p[x] = pix;
			}
#endif
		}
		p += current_par.max_xres;
	}
}
#endif

#ifdef CONFIG_PM
static int colliefb_pm_callback(struct pm_dev* pm_dev,
				pm_request_t req, void *data)
{

	switch (req) {
#if 0
	case PM_BLANK:
		collie_fb_blank = 1;
	  	screen_backup();
		colliefb_disable_lcd_controller();
		collie_fb_blank = 0;
		break;
	case PM_UNBLANK:
		collie_fb_blank = 1;
		colliefb_enable_lcd_controller();
		screen_restore();
		collie_fb_blank = 0;
		break;
#endif
	case PM_SUSPEND:
		screen_backup();
		if (!is_collie_fb_blank) {
			colliefb_disable_lcd_controller();
			is_collie_fb_blank = 1;
		}
		while(current_par.controller_state != LCD_MODE_DISABLED);
		break;
	case PM_RESUME:
                screen_restore();
		if (is_collie_fb_blank) {
			colliefb_enable_lcd_controller();
			is_collie_fb_blank = 0;
		}
		break;
	}

	return 0;
}
#endif

int __init colliefb_init(void)
{
	int ret;



	colliefb_init_fbinfo();

	/* Initialize video memory */
	if ((ret = colliefb_map_video_memory()) != 0)
		return ret;

	if ((ret = colliefb_map_backup_video_memory()) != 0)
		return ret;



	if (current_par.montype < 0 || current_par.montype > NR_MONTYPES) {
		current_par.montype = 1;
	}

#if 0
	if (request_irq(IRQ_LCD, colliefb_inter_handler,
			SA_INTERRUPT, "Collie LCD", NULL) != 0) {
		printk(KERN_ERR "colliefb: failed in request_irq\n");
		return -EBUSY;
	}
#endif

	disable_irq(IRQ_LCD);

	if (colliefb_set_var(&init_var, -1, &fb_info)) {
		current_par.allow_modeset = 0;
	}
	colliefb_decode_var(&init_var, &current_par);

#ifdef CONFIG_COLLIE_TS
#else
	SCP_REG(0x00) |= 0x0100;
#endif

	register_framebuffer(&fb_info);
#ifdef CONFIG_COLLIE_LOGO_SCREEN
	draw_logo_screen();
#endif
#ifdef CONFIG_PM
	fb_pm_dev = pm_register(PM_SYS_DEV, 0, colliefb_pm_callback);
#endif
	/* This driver cannot be unloaded at the moment */
	MOD_INC_USE_COUNT;


 	printk("Collie frame buffer driver initialized.\n");

	return 0;
}
