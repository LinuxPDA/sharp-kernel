/*
 * framebuffer driver for VBE 2.0 compliant graphic boards
 *
 *  (c) 2000 Eric Peng <ericpeng@xlinux.com>
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>

#include <asm/io.h>
//#include <asm/mtrr.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "mq2hw.h"
/* the virtual MQ MEM/REG base address is defined in arch/arm/mm/mm-sa1100.c */
unsigned long mq_mem_base = 0xf2800000;
unsigned long mq_reg_base = 0xf2e00000;

#include "mq_utils.c"
#include "mqregs.c"


/* --------------------------------------------------------------------- */

/*
 * card parameters
 */

/* card */
unsigned long video_base; /* physical addr */
int   video_size;
char *video_vbase=0;        /* mapped */


/* mode */
int  video_bpp;
int  video_width;
int  video_height;
int  video_height_virtual;
int  video_type = FB_TYPE_PACKED_PIXELS;
int  video_visual;
int  video_linelength;
int  video_cmap_len;

/* --------------------------------------------------------------------- */

#ifdef CONFIG_SA1100_PANGOLIN
static struct fb_var_screeninfo mqfb_defined = {
	800,600,800,600,/* W,H, W, H (virtual) load xres,xres_virtual*/
	0,0,		/* virtual -> visible no offset */
	16,		/* depth -> load bits_per_pixel */
	0,		/* greyscale ? */
	{11,5,0},	/* R */
	{5,6,0},	/* G */
	{0,5,0},	/* B */
	{16,0,0},	/* transparency */
	0,		/* standard pixel format */
	FB_ACTIVATE_NOW,
	-1,-1,
	0,
	0L,0L,0L,0L,0L,
	0L,0L,0,	/* No sync info */
	FB_VMODE_NONINTERLACED,
	{0,0,0,0,0,0}
};
#else
static struct fb_var_screeninfo mqfb_defined = {
	0,0,0,0,	/* W,H, W, H (virtual) load xres,xres_virtual*/
	0,0,		/* virtual -> visible no offset */
	8,		/* depth -> load bits_per_pixel */
	0,		/* greyscale ? */
	{0,0,0},	/* R */
	{0,0,0},	/* G */
	{0,0,0},	/* B */
	{0,0,0},	/* transparency */
	0,		/* standard pixel format */
	FB_ACTIVATE_NOW,
	-1,-1,
	0,
	0L,0L,0L,0L,0L,
	0L,0L,0,	/* No sync info */
	FB_VMODE_NONINTERLACED,
	{0,0,0,0,0,0}
};
#endif

static struct display disp;
static struct fb_info fb_info;
static struct { u_short blue, green, red, pad; } palette[256];
static union {
#ifdef FBCON_HAS_CFB16
    u16 cfb16[16];
#endif
#ifdef FBCON_HAS_CFB24
    u32 cfb24[16];
#endif
#ifdef FBCON_HAS_CFB32
    u32 cfb32[16];
#endif
} fbcon_cmap;

static int             inverse   = 0;
static int             mtrr      = 0;
static int             currcon   = 0;

static int             ypan       = 0;  /* 0..nothing, 1..ypan, 2..ywrap */

static struct display_switch mqfb_sw;

/* --------------------------------------------------------------------- */

	/*
	 * Open/Release the frame buffer device
	 */

static int mqfb_open(struct fb_info *info, int user)
{
	/*
	 * Nothing, only a usage count for the moment
	 */
	MOD_INC_USE_COUNT;
	return(0);
}

static int mqfb_release(struct fb_info *info, int user)
{
	MOD_DEC_USE_COUNT;
	return(0);
}

static int mqfb_pan_display(struct fb_var_screeninfo *var, int con,
                              struct fb_info *info)
{
	int offset;

    /* we don't do accelerated PAN function now */
		return -EINVAL;
}

static int mqfb_update_var(int con, struct fb_info *info)
{
	if (con == currcon && ypan) {
		struct fb_var_screeninfo *var = &fb_display[currcon].var;
                return mqfb_pan_display(var,con,info);
	}
	return 0;
}

static int mqfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
        strcpy(fix->id,"MQ200 VGA");

        fix->smem_start=video_base;
        fix->smem_len=video_size;
	fix->type = video_type;
	fix->visual = video_visual;
	fix->xpanstep  = 0;
        fix->ypanstep  = 0 /*ypan     ? 1 : 0*/;
        fix->ywrapstep = 0 /*(ypan>1) ? 1 : 0*/;
	fix->line_length=video_linelength;
	return 0;
}

static int mqfb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	if(con==-1)
                memcpy(var, &mqfb_defined, sizeof(struct fb_var_screeninfo));
	else
		*var=fb_display[con].var;
	return 0;
}

static void mqfb_set_disp(int con)
{
	struct fb_fix_screeninfo fix;
	struct display *display;
	struct display_switch *sw;
	
	if (con >= 0)
		display = &fb_display[con];
	else
		display = &disp;	/* used during initialization */

        mqfb_get_fix(&fix, con, 0);

	memset(display, 0, sizeof(struct display));
	display->screen_base = video_vbase;
	display->visual = fix.visual;
	display->type = fix.type;
	display->type_aux = fix.type_aux;
	display->ypanstep = fix.ypanstep;
	display->ywrapstep = fix.ywrapstep;
	display->line_length = fix.line_length;
	display->next_line = fix.line_length;
	display->can_soft_blank = 0;
	display->inverse = inverse;
        mqfb_get_var(&display->var, -1, &fb_info);

	switch (video_bpp) {
#ifdef FBCON_HAS_CFB8
	case 8:
		sw = &fbcon_cfb8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
	case 16:
		sw = &fbcon_cfb16;
		display->dispsw_data = fbcon_cmap.cfb16;
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		sw = &fbcon_cfb24;
		display->dispsw_data = fbcon_cmap.cfb24;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		sw = &fbcon_cfb32;
		display->dispsw_data = fbcon_cmap.cfb32;
		break;
#endif
	default:
		sw = &fbcon_dummy;
		return;
	}
        memcpy(&mqfb_sw, sw, sizeof(*sw));
        display->dispsw = &mqfb_sw;
	if (!ypan) {
		display->scrollmode = SCROLL_YREDRAW;
                mqfb_sw.bmove = fbcon_redraw_bmove;
	}
}

static int mqfb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
	static int first = 1;

        if (var->xres           != mqfb_defined.xres           ||
            var->yres           != mqfb_defined.yres           ||
            var->xres_virtual   != mqfb_defined.xres_virtual   ||
	    var->yres_virtual   >  video_height_virtual          ||
	    var->yres_virtual   <  video_height                  ||
	    var->xoffset                                         ||
            var->bits_per_pixel != mqfb_defined.bits_per_pixel ||
	    var->nonstd) {
		if (first) {
                        printk(KERN_ERR "mqfb does not support changing the video mode\n");
			first = 0;
		}
		return -EINVAL;
	}

	if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_TEST)
		return 0;

	if (ypan) {
                if (mqfb_defined.yres_virtual != var->yres_virtual) {
                        mqfb_defined.yres_virtual = var->yres_virtual;
			if (con != -1) {
                                fb_display[con].var = mqfb_defined;
				info->changevar(con);
			}
		}

                if (var->yoffset != mqfb_defined.yoffset)
                        return mqfb_pan_display(var,con,info);
		return 0;
	}

	if (var->yoffset)
		return -EINVAL;
	return 0;
}

static int mq_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			  unsigned *blue, unsigned *transp,
			  struct fb_info *fb_info)
{
	/*
	 *  Read a single color register and split it into colors/transparent.
	 *  Return != 0 for invalid regno.
	 */

	if (regno >= video_cmap_len)
		return 1;

	*red   = palette[regno].red;
	*green = palette[regno].green;
	*blue  = palette[regno].blue;
	*transp = 0;
	return 0;
}

#ifdef FBCON_HAS_CFB8

static void mq_setpalette(int regno, unsigned red, unsigned green, unsigned blue)
{
  unsigned long DAC_value;

  DAC_value = red >> 8 | green | blue << 8;

  REG32_PAL(regno, DAC_value);
        
}

#endif

static int mq_setcolreg(unsigned regno, unsigned red, unsigned green,
			  unsigned blue, unsigned transp,
			  struct fb_info *fb_info)
{
	/*
	 *  Set a single color register. The values supplied are
	 *  already rounded down to the hardware's capabilities
	 *  (according to the entries in the `var' structure). Return
	 *  != 0 for invalid regno.
	 */
	
	if (regno >= video_cmap_len)
		return 1;

	palette[regno].red   = red;
	palette[regno].green = green;
	palette[regno].blue  = blue;
	
	switch (video_bpp) {
#ifdef FBCON_HAS_CFB8
	case 8:
                mq_setpalette(regno,red,green,blue);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 15:
	case 16:
                if (mqfb_defined.red.offset == 10) {
			/* 1:5:5:5 */
			fbcon_cmap.cfb16[regno] =
				((red   & 0xf800) >>  1) |
				((green & 0xf800) >>  6) |
				((blue  & 0xf800) >> 11);
		} else {
			/* 0:5:6:5 */
			fbcon_cmap.cfb16[regno] =
				((red   & 0xf800)      ) |
				((green & 0xfc00) >>  5) |
				((blue  & 0xf800) >> 11);
		}
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		red   >>= 8;
		green >>= 8;
		blue  >>= 8;
		fbcon_cmap.cfb24[regno] =
                        (red   << mqfb_defined.red.offset)   |
                        (green << mqfb_defined.green.offset) |
                        (blue  << mqfb_defined.blue.offset);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		red   >>= 8;
		green >>= 8;
		blue  >>= 8;
		fbcon_cmap.cfb32[regno] =
                        (red   << mqfb_defined.red.offset)   |
                        (green << mqfb_defined.green.offset) |
                        (blue  << mqfb_defined.blue.offset);
		break;
#endif
    }
    return 0;
}

static void do_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
                fb_set_cmap(&fb_display[con].cmap, 1, mq_setcolreg, info);
	else
                fb_set_cmap(fb_default_cmap(video_cmap_len), 1, mq_setcolreg,
			    info);
}

static int mqfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, mq_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(video_cmap_len),
		     cmap, kspc ? 0 : 2);
	return 0;
}

static int mqfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info)
{
	int err;

	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		err = fb_alloc_cmap(&fb_display[con].cmap,video_cmap_len,0);
		if (err)
			return err;
	}
	if (con == currcon)			/* current console? */
		return fb_set_cmap(cmap, kspc, mq_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	return 0;
}

static int mqfb_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg, int con,
		       struct fb_info *info)
{
	return -EINVAL;
}


static struct fb_ops mqfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	mqfb_get_fix,
	fb_get_var:	mqfb_get_var,
	fb_set_var:	mqfb_set_var,
	fb_get_cmap:	mqfb_get_cmap,
	fb_set_cmap:	mqfb_set_cmap,
	fb_pan_display:	mqfb_pan_display,
	fb_ioctl:	mqfb_ioctl,
};


int mqfb_setup(char *options)
{
	char *this_opt;

	/* this part should be modified for MQ200 need, but currently intact */	
	fb_info.fontname[0] = '\0';
	
	if (!options || !*options)
		return 0;
	
	for(this_opt=strtok(options,","); this_opt; this_opt=strtok(NULL,",")) {
		if (!*this_opt) continue;
		
		if (! strcmp(this_opt, "inverse"))
			inverse=1;
		else if (! strcmp(this_opt, "redraw"))
			ypan=0;
		else if (! strcmp(this_opt, "ypan"))
			ypan=1;
		else if (! strcmp(this_opt, "ywrap"))
			ypan=2;
		else if (!strncmp(this_opt, "font:", 5))
			strcpy(fb_info.fontname, this_opt+5);
	}
	return 0;
}

static int mqfb_switch(int con, struct fb_info *info)
{
	/* Do we have to save the colormap? */
	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, mq_getcolreg,
			    info);
	
	currcon = con;
	/* Install new colormap */
	do_install_cmap(con, info);
        mqfb_update_var(con,info);
	return 1;
}

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */

static void mqfb_blank(int blank, struct fb_info *info)
{
	/* Not supported */
}

int __init mqfb_init(void)
{
	int i,j;

  /* Disable CF bus: */
#ifdef CONFIG_SA1100_PANGOLIN
#else
	BCR_set(BCR_CF_BUS_OFF);
#endif
	{ volatile int i; for (i=0; i<100; i++); }

#ifdef CONFIG_SA1100_PANGOLIN
	TUCR = (TUCR & 0x1fffffff) | TUCR_32_768kHz;  /* set output 32.768kHz clock */
#else
	TUCR = (TUCR & 0x1fffffff) | TUCR_3_6864MHz;  /* set output 3.6864MHz clock */
#endif
	GPDR = GPDR | 0x08000000;    /* turn GPIO bit 27 for output use */
	GAFR = GAFR | 0x08000000;    /* turn GPIO bit 27 for alternative function */

#ifdef CONFIG_SA1100_PANGOLIN
	GAFR &= ~GPIO_GPIO(14);
	GPDR |= GPIO_GPIO(14);
	GPCR = GPIO_GPIO(14);
#else
	BCR_clear(BCR_GFX_RST);
#endif
	{ volatile int i; for (i=0; i<100; i++); }
#ifdef CONFIG_SA1100_PANGOLIN
	GPSR = GPIO_GPIO(14);
#else
	BCR_set(BCR_GFX_RST);
#endif

	printk("Vendor ID data %x\n", *((volatile Word *) (mq_reg_base+0x16000)));
	setmqmode();


        video_base          = MQ_MEM_BASE; /*0x4b800000*/ 
#ifdef CONFIG_SA1100_PANGOLIN
        video_bpp           = 16;
#else
        video_bpp           = 8;
#endif
	if (15 == video_bpp)
		video_bpp = 16;
#ifdef CONFIG_SA1100_PANGOLIN
        video_width         = 800;
        video_height        = 600;
        video_linelength    = 800 * 2; // 800 x 2 bytes/pixel        
#else
        video_width         = 640;
        video_height        = 480;
        video_linelength    = 640;
#endif
        video_size          = 32 * 65536;  /* 2MB in total */
	video_visual = (video_bpp == 8) ?
		FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;

        if (!request_mem_region(video_base, video_size, "mqfb")) 
	{
		printk(KERN_ERR
                       "mqfb: abort, cannot reserve video memory at 0x%lu\n",
			video_base);
		return -1;
	}

        video_vbase = ioremap(video_base, video_size);

        printk(KERN_INFO "mqfb: framebuffer at 0x%lx, mapped to 0x%p, size %dk\n",
	       video_base, video_vbase, video_size/1024);
        printk(KERN_INFO "mqfb: mode is %dx%dx%d, linelength=%d, pages=%d\n",
	       video_width, video_height, video_bpp, video_linelength, screen_info.pages);

        mqfb_defined.xres=video_width;
        mqfb_defined.yres=video_height;
        mqfb_defined.xres_virtual=video_width;
        mqfb_defined.yres_virtual=video_size / video_linelength;
        mqfb_defined.bits_per_pixel=video_bpp;

        if (ypan && mqfb_defined.yres_virtual > video_height) {
                printk(KERN_INFO "mqfb: scrolling: %s using protected mode interface, yres_virtual=%d\n",
                       (ypan > 1) ? "ywrap" : "ypan",mqfb_defined.yres_virtual);
	} else {
                printk(KERN_INFO "mqfb: scrolling: redraw\n");
                mqfb_defined.yres_virtual = video_height;
		ypan = 0;
	}
        video_height_virtual = mqfb_defined.yres_virtual;

	/* some dummy values for timing to make fbset happy */
#ifdef CONFIG_SA1100_PANGOLIN
        mqfb_defined.pixclock     = 25180; // 1/(1344*806*60Hz)*10^12 =15386
        				   // 1/(1056*625*60Hz)*10^12 =25252
        				   // 1/(1054*628*60Hz)*10^12 =25180
        				   // unit : ps (pico second)
        mqfb_defined.left_margin  = 87;    // 1344-1184=312 , 1054-967=87
        mqfb_defined.right_margin = 39;    // 1048-1024=8   , 839-800=39
        mqfb_defined.upper_margin = 23;    // 806-777=29   , 628-605=23
        mqfb_defined.lower_margin = 1;     // 771-768=3    , 601-600=1
        mqfb_defined.hsync_len    = 128;   // 1184-1048=144 , 967-839=128
        mqfb_defined.vsync_len    = 4;     // 777-771=6    , 605-601=4
#else
        mqfb_defined.pixclock     = 10000000 / video_width * 1000 / video_height;
        mqfb_defined.left_margin  = (video_width / 8) & 0xf8;
        mqfb_defined.right_margin = 32;
        mqfb_defined.upper_margin = 16;
        mqfb_defined.lower_margin = 4;
        mqfb_defined.hsync_len    = (video_width / 8) & 0xf8;
        mqfb_defined.vsync_len    = 4;
#endif

	if (video_bpp > 8) {
#ifdef CONFIG_SA1100_PANGOLIN
#else
                mqfb_defined.red.offset    = screen_info.red_pos;
                mqfb_defined.red.length    = screen_info.red_size;
                mqfb_defined.green.offset  = screen_info.green_pos;
                mqfb_defined.green.length  = screen_info.green_size;
                mqfb_defined.blue.offset   = screen_info.blue_pos;
                mqfb_defined.blue.length   = screen_info.blue_size;
                mqfb_defined.transp.offset = screen_info.rsvd_pos;
                mqfb_defined.transp.length = screen_info.rsvd_size;
                printk(KERN_INFO "mqfb: directcolor: "
		       "size=%d:%d:%d:%d, shift=%d:%d:%d:%d\n",
		       screen_info.rsvd_size,
		       screen_info.red_size,
		       screen_info.green_size,
		       screen_info.blue_size,
		       screen_info.rsvd_pos,
		       screen_info.red_pos,
		       screen_info.green_pos,
		       screen_info.blue_pos);
#endif
		video_cmap_len = 16;

	} else {
                mqfb_defined.red.length   = 8/*6*/;
                mqfb_defined.green.length = 8/*6*/;
                mqfb_defined.blue.length  = 8/*6*/;
		for(i = 0; i < 16; i++) {
			j = color_table[i];
			palette[i].red   = default_red[j];
			palette[i].green = default_grn[j];
			palette[i].blue  = default_blu[j];
		}
		video_cmap_len = 256;
	}

	strcpy(fb_info.modename, "mqfb");
	fb_info.changevar = NULL;
	fb_info.node = -1;
        fb_info.fbops = &mqfb_ops;
	fb_info.disp=&disp;
        fb_info.switch_con=&mqfb_switch;
        fb_info.updatevar=&mqfb_update_var;
        fb_info.blank=&mqfb_blank;
	fb_info.flags=FBINFO_FLAG_DEFAULT;
        mqfb_set_disp(-1);

	if (register_framebuffer(&fb_info)<0)
		return -EINVAL;

	printk(KERN_INFO "fb%d: %s frame buffer device\n",
	       GET_FB_IDX(fb_info.node), fb_info.modename);

	return 0;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
