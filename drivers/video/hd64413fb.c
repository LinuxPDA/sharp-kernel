/*
 *  linux/drivers/video/hd64413fb.c -- Hitach SuperH HD64413 frame buffer device
 *
 *  	Copyright (C) 2000 Akira Kobayashi
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
//#include <linux/malloc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/hd64413.h>

#include <linux/fb.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>

#include "hd64413fb.h"

extern struct display_switch hd64413fb_cfb;

#define HD64413FB_MODE_NAME "Hitachi HD64413"	/* max 15 characters */
#define HD64413FB_DEFAULT_FONTNAME "VGA8x16"	/* supported only 8*16 font */

static struct hd64413fb_info fb_info;
static struct hd64413fb_par current_par;
static int current_par_valid = 0;
static struct display disp;

static struct { u_char red, green, blue, pad; } palette[256];
#ifdef FBCON_HAS_CFB16
static u16 fbcon_cfb16_cmap[16];
#endif


static struct fb_var_screeninfo default_var;

static struct {
    const char *name;
    struct fb_var_screeninfo var;
} hd64413fb_screen_list[] __initdata = {
    {   "640x480-60Hz 8bpp", {
	HD64431_DEFAULT_SCREEN_H_SIZE, HD64431_DEFAULT_SCREEN_V_SIZE,
	HD64431_DEFAULT_SCREEN_H_SIZE, HD64431_DEFAULT_SCREEN_V_SIZE,
	0,0, 8, 0,
	{ 0,8,0 }, { 0,8,0 }, { 0,8,0 }, { 0,0,0 },
	0, 0, -1,-1, FB_ACCELF_TEXT, 0,0,0,0,0,0,0,
	0, FB_VMODE_NONINTERLACED
    }},
#ifdef FBCON_HAS_CFB16
    {   "640x480-60Hz 16bpp", {
	HD64431_DEFAULT_SCREEN_H_SIZE, HD64431_DEFAULT_SCREEN_V_SIZE,
	HD64431_DEFAULT_SCREEN_H_SIZE, HD64431_DEFAULT_SCREEN_V_SIZE,
	0,0, 16, 0,
	{ 11,5,0 }, { 5,6,0 }, { 0,5,0 }, { 0,0,0 },
	0, 0, -1,-1, FB_ACCELF_TEXT, 0,0,0,0,0,0,0,
	0, FB_VMODE_NONINTERLACED
    }}
#endif
};


//static int currcon = 0;
//static int inverse = 0;

static int timerflag = 1;	// timer interrupt enable/disable
				// 0:disable 1:enable(default)

int hd64413fb_init(void);
int hd64413fb_setup(char*);
static int hd64413fb_setcolreg(unsigned regno, 
				unsigned red, unsigned green, unsigned blue, 
				unsigned transp, struct fb_info *info);


/* ------------------- chipset specific functions -------------------------- */

static int sh_hd64413_init(void)
{

#if defined(__sh3__)
#define BCR2 *(volatile unsigned short *)0xFFFFFF62
#define WCR1 *(volatile unsigned short *)0xFFFFFF64
#define WCR2 *(volatile unsigned short *)0xFFFFFF66
#if defined(Q2SD_SH_AREA2)
    BCR2 = (BCR2 & 0x3FC0) | 0x0020;
    WCR1 = (WCR1 & 0x3FC3) | 0x0010;
    WCR2 = (WCR2 & 0xFFE7) | 0x0010;
#elif defined(Q2SD_SH_AREA5)
    BCR2 = (BCR2 & 0x33F0) | 0x0800;
    WCR1 = (WCR1 & 0x33F3) | 0x0400;
    WCR2 = (WCR2 & 0xE3FF) | 0x0400;
#else
#error NotYet SH3
#endif
#endif
#if defined(__SH4__)
#define	BCR2 *(volatile unsigned short *)0xFF800004
#define WCR1 *(volatile unsigned long  *)0xFF800008
#define WCR2 *(volatile unsigned long  *)0xFF80000C
#define WCR3 *(volatile unsigned long  *)0xFF800010
#if defined(Q2SD_SH_AREA2)
    BCR2 = (BCR2 & 0xFFCF) | 0x0020;
    WCR1 = (WCR1 & 0xFFFFF0FF) | 0x00000700;
    WCR2 = (WCR2 & 0xFFFFF1FF) | 0x00000E00;
    WCR3 = (WCR3 & 0xFFFFF0FF) | 0x00000700;
#elif defined(Q2SD_SH_AREA5)
    BCR2 = (BCR2 & 0xFFCF) | 0x0020;
    WCR1 = (WCR1 & 0xFF0FFFFF) | 0x00100000;
    WCR2 = (WCR2 & 0xFC7FFFFF) | 0x01000000;
    WCR3 = (WCR3 & 0xFF0FFFFF) | 0x00100000;
#else
#error NotYet SH4
#endif
#endif

    /* is HD64413 (Q2SD) presented */
    if ((ctrl_inw(Q2SR) & 0x000f) != 4)
	return -EINVAL;

    /* SRES бс0 / Stop display(DRES=1 DEN=0) / Manual display change mode (DBM=2) */
    ctrl_outw(0x4080, Q2SYSR);

    ctrl_outw(0xFE80, Q2SRCR);		/* Status Reg. clear */
    ctrl_outw(0x0000, Q2IER);		/* All Interrupt disable */
    ctrl_outw(0x0031, Q2MEMR);		/* 64M-SDRAM 32bit bus */

    /* set display mode */
    	/* FLT=0 / WRAP=0 / BG=0 / TVM=0 / SCM=0 / Refresh (REF=5) */
    ctrl_outw(0x0005, Q2DSMR);		
    	/* CDC=0 / cursor1>2>fg>video(RPI=00) / RGB(VWRY=1) / HDIS=0 / ODEV=0 / CSY=0 */
    	/* foreground enable (FBD=0) / cousor (CE2=0 / CE1=0) / video window (VWE=0) */
    ctrl_outw(0x0000, Q2DSMR2);

    /* */
    	/* RSAE=0 / UGB x max (MWX=1) / Graphic mode (GBM) */
    if (HD64431_DEFAULT_BPP == 8)
	ctrl_outw(0x0040, Q2REMR);	/* 8bpp */
    else
	ctrl_outw(0x0041, Q2REMR);	/* 16bpp */
    ctrl_outw(0x0000, Q2IEMR);		/* input data convert mode = normal */

    ctrl_outw(0x0000, Q2DSA0);		/* FB0 address A22-16 */
    ctrl_outw(0x0010, Q2DSA1);		/* FB1 address A22-16 */
    ctrl_outw(USE_DL >> 16, Q2DLSAH);
				/* display list address A22-5 (32k * 2) */
    ctrl_outw(USE_DL & 0xFFFF, Q2DLSAL);
    ctrl_outw(COLOR_WORK >> 16, Q2SSAR);/* color work  address A22-13 */
    ctrl_outw(0x001F, Q2WSAR);		/* work area address A22-13 */

    /* DMA not used */
    ctrl_outw(0x0000, Q2DMASH);
    ctrl_outw(0x0000, Q2DMASL);
    ctrl_outw(0x0000, Q2DMAWL);

    ctrl_outw(0x0000, Q2DOR);		/* set color of blank */
    ctrl_outw(0x0000, Q2DOG_DOB);
    ctrl_outw(0x00FC, Q2CDR);		/* CDE decode color */
    ctrl_outw(0xFCFC, Q2CDG_CDB);

    ctrl_outw(0x0000, Q2ISAH);
    ctrl_outw(0x0000, Q2ISAL);
    ctrl_outw(0x0000, Q2IDSX);
    ctrl_outw(0x0000, Q2IDSY);
    ctrl_outw(0x0000, Q2IDE);
    
    /* SRES бс0 / Start display (DRES=1 DEN=0) / Manual display change mode (DBM=1) */
    ctrl_outw(0x2080, Q2SYSR);

    return 0;
}


static void hd64413fb_detect(void)
{
    DPRINTK("\n");
}

/*
  for Q2SD Frame Buffer
  Execute polygon4a command
*/
void
hd64413fb_polygon4a(void)
{
	unsigned int xres = HD64431_DEFAULT_SCREEN_H_SIZE;
	unsigned int yres = HD64431_DEFAULT_SCREEN_V_SIZE;
	unsigned int srcwk;
	volatile unsigned short *dlist; // Display List addr

	// color work area address
	srcwk = Q2SD_UGMBASE + COLOR_WORK;

	// set Display List top Addr
	dlist = (unsigned short*)(Q2SD_UGMBASE + USE_DL);

	// synchronize frame change timing (VBKEM command)
	*dlist++ = 0xd000;
	*dlist++ = 0;
	*dlist++ = 0;

	// set System Clip Area (SCLIP command)
	*dlist++ = 0xb800;
	*dlist++ = xres; // x
	*dlist++ = yres; // y

	// set local offset (lcofs command)
	*dlist++ = 0x9000;
	*dlist++ = 0; // x
	*dlist++ = 0; // y

	// clear screen (POLYGON4C command)
	*dlist++ = 0x1000;
	*dlist++ = 0;
	*dlist++ = 0;
	*dlist++ = xres - 1;
	*dlist++ = 0;
	*dlist++ = xres - 1;
	*dlist++ = yres - 1;
	*dlist++ = 0;
	*dlist++ = yres - 1;
	*dlist++ = 0x00;


	// POLYGON4A command
	*dlist++ = 0x0000 | 0x0004;	// POLYGON4A command
					// res:0,TRNS:0,STYL:0,CLIP:0,REL:0,
					// NET:0,EOS:0,FST:0,LNi:1,COOF:0,WORK:0
	*dlist++ = (unsigned short)((srcwk >> 13 ) & 0x000003ff);
	*dlist++ = (unsigned short)(srcwk & 0x00001fff);
	*dlist++ = xres;		// dst width
	*dlist++ = yres;		// dst height
	*dlist++ = 0;		// fb x1 pos
	*dlist++ = 0;		// fb y1 pos
	*dlist++ = xres - 1;	// fb x2 pos
	*dlist++ = 0;		// fb y2 pos
	*dlist++ = xres - 1;	// fb x3 pos
	*dlist++ = yres - 1;	// fb y3 pos
	*dlist++ = 0;		// fb x4 pos
	*dlist++ = yres - 1;	// fb y4 pos
	*dlist++ = 0xf800;		// TRAP (command end)


	ctrl_outw(0x0400, Q2SR);	// clear TRA


	// renddering start
					// SRES:0,DRES:0,DEN:1,0,0,RBRK:0,DC:0,RS:1,
					// DBM1:1,DBM0:0,DMA1:0,DMA0:0,DAA1:0,DAA0:0,0,0
	ctrl_outw(0x2140, Q2SYSR);	// write SYSR reg

}

/*
   for re-write Frame Buffer
*/
static void fb_rewrite_timer_handler(unsigned long);

static struct timer_list fb_rewrite_timer = {
	function: fb_rewrite_timer_handler
};

static void fb_rewrite_timer_handler(unsigned long dev_addr)
{

	hd64413fb_polygon4a();

	if(timerflag == 1){ // interrupt enable
		fb_rewrite_timer.expires = jiffies + HD64413_INTERVAL_TIME;
		add_timer(&fb_rewrite_timer);
	}
}

static int hd64413fb_encode_fix(struct fb_fix_screeninfo *fix,
				    const void *fb_par,
				    struct fb_info_gen *info)
{
    struct hd64413fb_par *par = (struct hd64413fb_par *)fb_par;

    DPRINTK("xres=%d yres=%d bpp=%d wrap_offset=%d b_width=%d b_height=%d\n",
		    par->xres,par->yres,par->bpp,par->wrap_offset,
		    par->block_width,par->block_height);

    memset(fix, 0, sizeof(struct fb_fix_screeninfo));

    strcpy(fix->id, HD64413FB_MODE_NAME);
    // source addr
    fix->smem_start = (char*)(Q2SD_UGMBASE + COLOR_WORK);
    fix->smem_len = ((par->wrap_offset * par->bpp + 7) / 8) * par->yres;
    fix->type = FB_TYPE_PACKED_PIXELS;
    fix->type_aux = 0;
    if (par->bpp == 8) {
        fix->visual = FB_VISUAL_PSEUDOCOLOR;
    } else {
        fix->visual = FB_VISUAL_TRUECOLOR;
    }
    fix->xpanstep = 0;
    fix->ypanstep = 0;
    fix->ywrapstep = 0;

    fix->line_length = (par->wrap_offset * par->bpp + 7) / 8;
    //fix->line_length = (par->block_width * par->bpp + 7) / 8;

    fix->mmio_start = (char*)Q2SD_IOBASE;
    fix->mmio_len = 0x600;
    fix->accel = FB_ACCEL_HITACHI_HD64413;

    return 0;
}


/* check var / var -> par */
static int hd64413fb_decode_var(const struct fb_var_screeninfo *var,
				    void *fb_par,
				    struct fb_info_gen *info)
{
    struct hd64413fb_par *par = (struct hd64413fb_par *)fb_par;

    DPRINTK("\n");

    if (var->bits_per_pixel == 8 || var->bits_per_pixel == 16) {
        par->bpp = var->bits_per_pixel;
    }else {
	return -EINVAL;
    }

    if (var->xres == HD64431_DEFAULT_SCREEN_H_SIZE 
		    && var->yres == HD64431_DEFAULT_SCREEN_V_SIZE) {
        par->xres = var->xres;
        par->yres = var->yres;
    }else {
	return -EINVAL;
    }

    par->wrap_offset = HD64431_DEFAULT_WRAP;

    DPRINTK("xres=%d yres=%d bpp=%d wrap_offset=%d\n",
		    par->xres,par->yres,par->bpp,par->wrap_offset);
    return 0;
}


/* par => var */
static int hd64413fb_encode_var(struct fb_var_screeninfo *var, 
			     	const void *fb_par,
			     	struct fb_info_gen *info)
{
    struct hd64413fb_par *par = (struct hd64413fb_par *)fb_par;

    DPRINTK("xres=%d yres=%d bpp=%d wrap_offset=%d\n",
		    par->xres,par->yres,par->bpp,par->wrap_offset);

    memset(var, 0, sizeof(*var));

    var->xres = par->xres;
    var->yres = par->yres;
    var->xres_virtual = var->xres;
    var->yres_virtual = var->yres;
    var->xoffset = 0;
    var->yoffset = 0;
    var->bits_per_pixel = par->bpp;
    var->grayscale = 0;

    switch (var->bits_per_pixel) {
	case 8:
	    var->red.offset = 0;
	    var->red.length = 8;
	    var->green.offset = 0;
	    var->green.length = 8;
	    var->blue.offset = 0;
	    var->blue.length = 8;
	    var->transp.offset = 0;
	    var->transp.length = 0;
	    break;

	case 16:	/* RGB 565 */
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
    
    var->nonstd = 0;
    var->activate = 0;
    var->height = -1;
    var->width = -1;
    var->accel_flags = FB_ACCELF_TEXT;

    /* ??? create from xres and yres */
    var->pixclock = 0;
    var->left_margin = 0;
    var->right_margin = 0;
    var->upper_margin = 0;
    var->lower_margin = 0;
    var->hsync_len = 0;
    var->vsync_len = 0;
    var->sync = 0;
    var->vmode = FB_VMODE_NONINTERLACED;
    
    return 0;
}


static void hd64413fb_get_par(void *fb_par, struct fb_info_gen *info)
{
    struct hd64413fb_par *par = (struct hd64413fb_par *)fb_par;


    if (current_par_valid) {
        *par = current_par;
    } else {
	hd64413fb_decode_var(&default_var, par, info);
    }

    DPRINTK("xres=%d yres=%d bpp=%d wrap_offset=%d b_width=%d b_height=%d\n",
		    par->xres,par->yres,par->bpp,par->wrap_offset,
		    par->block_width,par->block_height);
}


static void hd64413fb_set_par(const void *fb_par, struct fb_info_gen *info)
{
    struct hd64413fb_par *par = (struct hd64413fb_par *)fb_par;

    /* Set the hardware according to 'par'. */
    current_par = *par;

    current_par.block_width = 32 / (par->bpp >> 3);
    current_par.block_height = 16;
#ifdef FBCON_HAS_CFB16
    current_par.cmap = (void *)fbcon_cfb16_cmap;
    if (par->bpp != 8) {
        int i;
        struct fb_cmap *def_cmap;
        def_cmap = fb_default_cmap(16);
	if (def_cmap) {
DPRINTK("start=%d len=%d\n",def_cmap->start,def_cmap->len);
            for (i = def_cmap->start; i < def_cmap->len; i++) {
	        hd64413fb_setcolreg(i, 
			def_cmap->red[i], def_cmap->green[i], def_cmap->blue[i],
			0, (struct fb_info *)info);
            }
	}
    }
#endif

    /* Set register */
    /* set bpp and UGM x width */
    {	
        u16 remr;
        remr = ctrl_inw(Q2REMR) & 0xffb8;
        if (par->bpp != 8)
	    remr |= 0x1;
	if (par->wrap_offset > 512)
	    remr |= 0x40;
        ctrl_outw(remr, Q2REMR);

	DPRINTK("remr=%04x\n", remr);
    }

    /* set resolution */
    {
	/* 640x480-60Hz */
        unsigned short hsw =  51;
        unsigned short xs  =  99;
        unsigned short xw  = par->xres;
        unsigned short hc  = 800;
        unsigned short vsw =   3;
        unsigned short ys  =  40;
        unsigned short yw  = par->yres;
        unsigned short vc  = 520;

        ctrl_outw(xw,          Q2DSX);	/* horizontal display size  (in dot) */
        ctrl_outw(yw,          Q2DSY);	/* vertical display size  (in dot) */

	ctrl_outw(hsw+xs-11,   Q2HDS);	/* horizontal display start (in dot clock) */
	ctrl_outw(hsw+xs-11+xw,Q2HDE);	/* horizontal display end (in dot clock) */
	ctrl_outw(ys-2,        Q2VDS);	/* vertical display start (in raster line) */
	ctrl_outw(ys-2+yw,     Q2VDE);	/* vertical display end (in raster line) */

        ctrl_outw(hsw-1,       Q2HSW);	/* hsync low pulse width (in dot clock) */
        ctrl_outw(hc-1,        Q2HC);	/* hsync cycle (in dot clock) */
        ctrl_outw(vc-vsw-1,    Q2VSP);	/* vsync start (in raster line) */
        ctrl_outw(vc-1,        Q2VC);	/* vsync cycle (in raster line) */
    }

    current_par_valid = 1;

    DPRINTK("xres=%d yres=%d bpp=%d wrap_offset=%d b_width=%d b_height=%d\n",
			current_par.xres,current_par.yres,
			current_par.bpp,current_par.wrap_offset,
			current_par.block_width,current_par.block_height);
}


static int hd64413fb_getcolreg(unsigned regno, 
				unsigned *red, unsigned *green, unsigned *blue,
                      		unsigned *transp, struct fb_info *info)
{
    if (regno > 255)
	return 1;	

#if 0
    {
    	u16 cpr_h, cpr_l;
        cpr_l = ctrl_inw(Q2SD_IOBASE+0x200+regno*4);
        cpr_h = ctrl_inw(Q2SD_IOBASE+0x200+regno*4+2);
	*red = (cpr_l & 0xfc) << 8;
	*green = cpr_h & 0xfc00;
	*blue = (cpr_h & 0xfc) << 8;
    }
#else
    *red = (palette[regno].red<<8);
    *green = (palette[regno].green<<8);
    *blue = (palette[regno].blue<<8);
#endif

    *transp = 0;

    DPRINTK("[%d] r:%04x g:%04x b:%04x\n", regno,*red,*green,*blue);
    
    return 0;
}


static int hd64413fb_setcolreg(unsigned regno, 
				unsigned red, unsigned green, unsigned blue, 
				unsigned transp, struct fb_info *info)
{
    u16 rgb = 0;

    if (regno > 255)
        return 1;

    palette[regno].red = red >> 8;
    palette[regno].green = green >> 8;
    palette[regno].blue = blue >> 8;

    if (current_par.bpp == 8) {
        ctrl_outw(palette[regno].red & 0x00FC, Q2SD_IOBASE+0x200+regno*4);
        ctrl_outw((green & 0xFC00) | (palette[regno].blue & 0x00FC), 
		    				Q2SD_IOBASE+0x200+regno*4+2);
    } else {
#ifdef FBCON_HAS_CFB16
        if (regno < 16) {
	    rgb =   ((red   & 0xf800)      ) |
			((green & 0xfc00) >>  5) |
			((blue  & 0xf800) >> 11);
	    fbcon_cfb16_cmap[regno] = rgb;
	}
#endif
    }

    DPRINTK("[%d] r:%04x g:%04x b:%04x cmap:%04x\n", 
			regno, red, green, blue, rgb);

    return 0;
}

static int hd64413fb_blank(int blank_mode, struct fb_info_gen *info)
{
    u16 sysr;

    DPRINTK("blank_mode=%d\n", blank_mode);
    
    sysr = ctrl_inw(Q2SYSR);
    switch (blank_mode) {
	case 0:		/* Unblanking */
	    if ((sysr & 0x6000) != 0x2000)
	        ctrl_outw((sysr & 0x9fff) | 0x2000, Q2SYSR);
	    break;
	case 1:		/* Normal blanking */
	    if ((sysr & 0x6000) != 0x0000)
	        ctrl_outw((sysr & 0x9fff) | 0x0000, Q2SYSR);
	    break;
	case 2:		/* vsync off */
	case 3:		/* hsync off */
	case 4:		/* poweroff */
	    if ((sysr & 0x6000) != 0x0000)
	        ctrl_outw((sysr & 0x9fff) | 0x0000, Q2SYSR);
/*
	    if ((sysr & 0x6000) != 0x4000)
	        ctrl_outw((sysr & 0x9fff) | 0x4000, Q2SYSR);
*/
	    break;
    }
    return 0;
}


static void hd64413fb_set_disp(const void *par, struct display *disp,
			   struct fb_info_gen *info)
{
    DPRINTK("\n");

    // source addr
    disp->screen_base = (void *)Q2SD_UGMBASE + COLOR_WORK;
    disp->dispsw = &hd64413fb_cfb;
    disp->dispsw_data = &current_par;
}

static int
hd64413fb_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	 unsigned long arg, int con ,struct fb_info *info)
{

	switch(cmd){
		case FBIO_CMD_EXEC_AUTO: // exec of timer interrupt
			if(timerflag == 0){ // timer interrpt disable
				timerflag = 1; // timer interrpt enable
				fb_rewrite_timer.expires = 
					jiffies + HD64413_INTERVAL_TIME;
				add_timer(&fb_rewrite_timer);
			}
			break;
		case FBIO_CMD_EXEC_MANUAL: // exec of fsync() system call
			if(timerflag == 1){ // timer interrpt enable
				timerflag = 0; // timer interrpt disable
			}
			break;
		default:
			break;
	}
	return 0;
}

/* ------------ Interfaces to hardware functions ------------ */

struct fbgen_hwswitch hd64413fb_switch = {
    hd64413fb_detect,
    hd64413fb_encode_fix,
    hd64413fb_decode_var,
    hd64413fb_encode_var,
    hd64413fb_get_par,
    hd64413fb_set_par,
    hd64413fb_getcolreg,
    hd64413fb_setcolreg,
    NULL,			/* hd64413fb_pan_display */
    hd64413fb_blank,
    hd64413fb_set_disp
};



/* ------------ Hardware Independent Functions ------------ */

#if LINUX_VERSION_CODE >= 0x20400
#else

static int hd64413fb_open(struct fb_info* info, int user) {
	MOD_INC_USE_COUNT;
	return 0;
}

static int hd64413fb_release(struct fb_info* info, int user) {
	MOD_DEC_USE_COUNT;
	return 0;
}

#endif


static struct fb_ops hd64413fb_ops = {
#if LINUX_VERSION_CODE >= 0x20400
	owner:		THIS_MODULE,
#else
	fb_open:	hd64413fb_open,
	fb_release:	hd64413fb_release,
#endif
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	fbgen_set_cmap,
	fb_ioctl:	hd64413fb_ioctl,
/*
	fb_pan_display:
	fb_mmap:
	fb_rasterimg:
*/
};



int __init hd64413fb_init(void)
{
    struct hd64413fb_par par;

    DPRINTK("\n");

    if (sh_hd64413_init() != 0)
	return -EINVAL;

#ifdef FBCON_HAS_CFB16
    default_var = hd64413fb_screen_list[1].var;
#else
    default_var = hd64413fb_screen_list[0].var;
#endif
    if (hd64413fb_decode_var(&default_var, &par, NULL) != 0)
	return -EINVAL;
    hd64413fb_set_par(&par, NULL);

    strcpy(fb_info.gen.info.modename, HD64413FB_MODE_NAME);
    fb_info.gen.info.node = -1;
    fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
    fb_info.gen.info.fbops = &hd64413fb_ops;
    fb_info.gen.info.disp = &disp;
    fb_info.gen.info.changevar = NULL;
    fb_info.gen.info.switch_con = &fbgen_switch;
    fb_info.gen.info.updatevar = &fbgen_update_var;
    fb_info.gen.info.blank = &fbgen_blank;
    fb_info.gen.parsize = sizeof(struct hd64413fb_par);
    fb_info.gen.fbhw = &hd64413fb_switch;
    fb_info.gen.fbhw->detect();

    strcpy(fb_info.gen.info.fontname, HD64413FB_DEFAULT_FONTNAME);
    
    fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
    disp.var.activate = FB_ACTIVATE_NOW;
    fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
    fbgen_set_disp(-1, &fb_info.gen);
    fbgen_install_cmap(0, &fb_info.gen);
    
    if(register_framebuffer(&fb_info.gen.info)<0)
	return -EINVAL;
    
    DPRINTK("fb%d: %s frame buffer device\n",
	   GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);
    
    fb_rewrite_timer.expires = jiffies + HD64413_INTERVAL_TIME;
    add_timer(&fb_rewrite_timer);

    return 0;
}


void hd64413fb_cleanup(struct fb_info *info)
{
    DPRINTK("\n");

    unregister_framebuffer(info);
}


int __init hd64413fb_setup(char *options)
{
    DPRINTK("\n");

    return 0;
}


/* ------------------------------------------------------------------------- */



#ifdef MODULE
int init_module(void)
{
    return hd64413fb_init();
}

void cleanup_module(void)
{
    hd64413fb_cleanup(void);
}
#endif
