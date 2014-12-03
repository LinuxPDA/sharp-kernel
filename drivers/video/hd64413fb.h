/*
 *  linux/drivers/video/hd64413fb.h -- Hitach SuperH HD64413 frame buffer device
 *
 *  	Copyright (C) 2000 Akira Kobayashi
 *  
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef HD64413FB_H
#define HD64413FB_H

#include <linux/version.h>

#if LINUX_VERSION_CODE >= 0x20400
#else
#define fb_readl(addr) (*(volatile u32 *) (addr))
#define fb_writel(b,addr) (*(volatile u32 *) (addr) = (b))
#define fb_readw(addr) (*(volatile u16 *) (addr))
#define fb_writew(b,addr) (*(volatile u16 *) (addr) = (b))
#define fb_readb(addr) (*(volatile u8 *) (addr))
#define fb_writeb(b,addr) (*(volatile u8 *) (addr) = (b))
#endif

/* debug output */
#if 0
//#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
//#define DPRINTK(fmt, args...) dbg_printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define HD64431_DEFAULT_SCREEN_H_SIZE	640	/* in pixel */
#define HD64431_DEFAULT_SCREEN_V_SIZE	480	/* in pixel */
#define HD64431_DEFAULT_BPP		8	/* 8 or 16 */
//#define HD64431_DEFAULT_WRAP		1024	/* in pixel */
#define HD64431_DEFAULT_WRAP		HD64431_DEFAULT_SCREEN_H_SIZE	/* in pixel */
#define HD64413_INTERVAL_TIME		HZ / 25	/* timer interval time */

struct hd64413fb_info {
    struct fb_info_gen gen;
    //struct fb_info gen;
};

struct hd64413fb_par
{
    int xres;		/* visible horizontal resolution */
    int yres;		/* visible vertical resolution */
    int bpp;		/* bit per pixel */
    int wrap_offset;	/* next raster offset (in pixcel) */
    
    /* Filled in by hd64413fb_set_par */
    int block_width;	/* UGM FB block width (in pixel) */
    int block_height;	/* UGM FB block height (in pixel) */
    void *cmap;		/* color map for 16bpp */
};


static inline unsigned int hd64413fb_get_offset(u16 x, u16 y, u16 bits_per_pixel)
{
    u16 wrap_length = HD64431_DEFAULT_WRAP;	/* in pixel */
    u16 bpp_log = bits_per_pixel / 8;
    u16 block_wrap = 32;			/* in byte */
    u16 block_width = block_wrap / bpp_log;	/* in pixel */
    u16 block_height = 16;			/* in pixel */
    u16 block_size = block_wrap * block_height;	/* in byte */
    u16 band_size = wrap_length * bpp_log * block_height;	/* in byte */

    return (      (y / block_height) * band_size
		+ (x / block_width)  * block_size
		+ (y % block_height) * block_wrap
		+ (x % block_width)  * bpp_log );
}

#endif /* HD64413FB_H */
