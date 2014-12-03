/*
 *  linux/drivers/video/cfb16.c -- Low level frame buffer operations for 16 bpp
 *				   truecolor packed pixels
 *
 *	Created 5 Apr 1997 by Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 *
 * Change Log
 *	12-Nov-2001 Lineo Japan, Inc.
 *	30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 *      31-Oct-2002 SHARP
 *         added support for rotation logo screen on SHARP SL-C700
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>


    /*
     *  16 bpp packed pixels
     */

static u32 tab_cfb16[] = {
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
    0x00000000, 0x0000ffff
#else
#if defined(__BIG_ENDIAN)
    0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff
#elif defined(__LITTLE_ENDIAN)
    0x00000000, 0xffff0000, 0x0000ffff, 0xffffffff
#else
#error FIXME: No endianness??
#endif
#endif
};

void fbcon_cfb16_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual<<1;
    p->next_plane = 0;
}

void fbcon_cfb16_bmove(struct display *p, int sy, int sx, int dy, int dx,
		       int height, int width)
{
    int bytes = p->next_line, linesize = bytes * fontheight(p), rows;
    u8 *src, *dst;

#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
    if (fontwidthlog(p)) {
	sx <<= fontwidthlog(p);
	dx <<= fontwidthlog(p);
	width <<= fontwidthlog(p);
    } else {
	sx *= fontwidth(p);
	dx *= fontwidth(p);
	width *= fontwidth(p);
    }
    sy *= fontheight(p);
    dy *= fontheight(p);
    height *= fontheight(p);
#if defined(CONFIG_FBCON_ROTATE_R)
    {
	int tmp;
	tmp = sy;
	sy = sx;
	sx = p->var.xres - (tmp + height);
	tmp = dy;
	dy = dx;
	dx = p->var.xres - (tmp + height);
	tmp = height;
	height = width;
	width = tmp;
    }
#else
    {
	int tmp;
	tmp = sy;
	sy = p->var.yres - (sx + width);
	sx = tmp;
	tmp = dy;
	dy = p->var.yres - (dx + width);
	dx = tmp;
	tmp = height;
	height = width;
	width = tmp;
    }
#endif
    if (dy < sy || (dy == sy && dx < sx)) {
	src = p->screen_base + sy * bytes + sx * 2;
	dst = p->screen_base + dy * bytes + dx * 2;
	for (rows = height; rows--;) {
	    fb_memmove(dst, src, width * 2);
	    src += bytes;
	    dst += bytes;
	}
    } else {
	src = p->screen_base + (sy+height-1) * bytes + sx * 2;
	dst = p->screen_base + (dy+height-1) * bytes + dx * 2;
	for (rows = height; rows--;) {
	    fb_memmove(dst, src, width*2);
	    src -= bytes;
	    dst -= bytes;
	}
    }
#else
    if (sx == 0 && dx == 0 && width * fontwidth(p) * 2 == bytes) {
	fb_memmove(p->screen_base + dy * linesize,
		  p->screen_base + sy * linesize,
		  height * linesize);
	return;
    }
    if (fontwidthlog(p)) {
	sx <<= fontwidthlog(p)+1;
	dx <<= fontwidthlog(p)+1;
	width <<= fontwidthlog(p)+1;
    } else {
	sx *= fontwidth(p)*2;
	dx *= fontwidth(p)*2;
	width *= fontwidth(p)*2;
    }
    if (dy < sy || (dy == sy && dx < sx)) {
	src = p->screen_base + sy * linesize + sx;
	dst = p->screen_base + dy * linesize + dx;
	for (rows = height * fontheight(p); rows--;) {
	    fb_memmove(dst, src, width);
	    src += bytes;
	    dst += bytes;
	}
    } else {
	src = p->screen_base + (sy+height) * linesize + sx - bytes;
	dst = p->screen_base + (dy+height) * linesize + dx - bytes;
	for (rows = height * fontheight(p); rows--;) {
	    fb_memmove(dst, src, width);
	    src -= bytes;
	    dst -= bytes;
	}
    }
#endif
}

#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
static inline void rectfill(u8 *dest, int height, int width, u32 data,
			    int linesize)
#else
static inline void rectfill(u8 *dest, int width, int height, u32 data,
			    int linesize)
#endif
{
    int i;

#if defined(CONFIG_FBCON_ROTATE_R)
    while (height-- > 0) {
	for (i = 0; i < width; i++) {
	    fb_writew(data, dest + i * linesize);
	}
	dest -= 2;
    }
#elif defined(CONFIG_FBCON_ROTATE_L)
    while (height-- > 0) {
	for (i = 0; i < width; i++) {
	    fb_writew(data, dest - i * linesize);
	}
	dest += 2;
    }
#else
    data |= data<<16;

    while (height-- > 0) {
	u32 *p = (u32 *)dest;
	for (i = 0; i < width/4; i++) {
	    fb_writel(data, p++);
	    fb_writel(data, p++);
	}
	if (width & 2)
	    fb_writel(data, p++);
	if (width & 1)
	    fb_writew(data, (u16*)p);
	dest += linesize;
    }
#endif
}

#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
void fbcon_cfb16_clear(struct vc_data *conp, struct display *p, int sx, int sy,
		       int height, int width)
#else
void fbcon_cfb16_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		       int height, int width)
#endif
{
    u8 *dest;
    int bytes = p->next_line, lines = height * fontheight(p);
    u32 bgx;

#if defined(CONFIG_FBCON_ROTATE_R)
    dest = p->screen_base +
      sy * fontwidth(p) * bytes + (p->var.xres - sx * fontheight(p) - 1) * 2;
#elif defined(CONFIG_FBCON_ROTATE_L)
    dest = p->screen_base +
      (p->var.yres - sy * fontwidth(p) - 1) * bytes + sx * fontheight(p) * 2;
#else
    dest = p->screen_base + sy * fontheight(p) * bytes + sx * fontwidth(p) * 2;
#endif

    bgx = ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];

    width *= fontwidth(p)/4;
#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
    if (width * 8 == bytes)
	rectfill(dest, 1, lines * width * 4, bgx, bytes);
    else
	rectfill(dest, lines, width * 4, bgx, bytes);
#else
    if (width * 8 == bytes)
	rectfill(dest, lines * width * 4, 1, bgx, bytes);
    else
	rectfill(dest, width * 4, lines, bgx, bytes);
#endif
}

#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
void fbcon_cfb16_putc(struct vc_data *conp, struct display *p, int c, int xx,
		      int yy)
#else
void fbcon_cfb16_putc(struct vc_data *conp, struct display *p, int c, int yy,
		      int xx)
#endif
{
    u8 *dest, *cdat, bits;
    int bytes = p->next_line, rows;
    u32 eorx, fgx, bgx;

#if defined(CONFIG_FBCON_ROTATE_R)
    dest = p->screen_base +
      yy * fontwidth(p) * bytes + (p->var.xres - xx * fontheight(p) - 1) * 2;
#elif defined(CONFIG_FBCON_ROTATE_L)
    dest = p->screen_base +
      (p->var.yres - yy * fontwidth(p) - 1) * bytes + xx * fontheight(p) * 2;
#else
    dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p) * 2;
#endif

    fgx = ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
    bgx = ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
    fgx |= (fgx << 16);
    bgx |= (bgx << 16);
    eorx = fgx ^ bgx;

    switch (fontwidth(p)) {
    case 4:
    case 8:
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);
#if defined(CONFIG_FBCON_ROTATE_R)
	for (rows = fontheight(p); rows--; dest += 2) {
	    bits = *cdat++;
	    fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
		      dest);
	    fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
		      dest+1*bytes);
	    fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
		      dest+2*bytes);
	    fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
		      dest+3*bytes);
	    if (fontwidth(p) == 8) {
	        fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
			  dest+4*bytes);
		fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
			  dest+5*bytes);
		fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
			  dest+6*bytes);
		fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
			  dest+7*bytes);
	    }
	}
#elif defined(CONFIG_FBCON_ROTATE_L)
	for (rows = fontheight(p); rows--; dest += 2) {
	    bits = *cdat++;
	    fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
		      dest);
	    fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
		      dest-1*bytes);
	    fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
		      dest-2*bytes);
	    fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
		      dest-3*bytes);
	    if (fontwidth(p) == 8) {
	        fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
			  dest-4*bytes);
		fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
			  dest-5*bytes);
		fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
			  dest-6*bytes);
		fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
			  dest-7*bytes);
	    }
	}
#else
	for (rows = fontheight(p); rows--; dest += bytes) {
	    bits = *cdat++;
	    fb_writel((tab_cfb16[bits >> 6] & eorx) ^ bgx, dest);
	    fb_writel((tab_cfb16[bits >> 4 & 3] & eorx) ^ bgx, dest+4);
	    if (fontwidth(p) == 8) {
		fb_writel((tab_cfb16[bits >> 2 & 3] & eorx) ^ bgx, dest+8);
		fb_writel((tab_cfb16[bits & 3] & eorx) ^ bgx, dest+12);
	    }
	}
#endif
	break;
    case 12:
    case 16:
	cdat = p->fontdata + ((c & p->charmask) * fontheight(p) << 1);
#if defined(CONFIG_FBCON_ROTATE_R)
	for (rows = fontheight(p); rows--; dest -= 2) {
	    bits = *cdat++;
	    fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
		      dest);
	    fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
		      dest+bytes);
	    fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
		      dest+2*bytes);
	    fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
		      dest+3*bytes);
	    fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
		      dest+4*bytes);
	    fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
		      dest+5*bytes);
	    fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
		      dest+6*bytes);
	    fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
		      dest+7*bytes);
	    bits = *cdat++;
	    fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
		      dest+8*bytes);
	    fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
		      dest+9*bytes);
	    fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
		      dest+10*bytes);
	    fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
		      dest+11*bytes);
	    if (fontwidth(p) == 16) {
		fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
			  dest+12*bytes);
		fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
			  dest+13*bytes);
		fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
			  dest+14*bytes);
		fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
			  dest+15*bytes);
	    }
	}
#elif defined(CONFIG_FBCON_ROTATE_L)
	for (rows = fontheight(p); rows--; dest += 2) {
	    bits = *cdat++;
	    fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
		      dest);
	    fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
		      dest-bytes);
	    fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
		      dest-2*bytes);
	    fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
		      dest-3*bytes);
	    fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
		      dest-4*bytes);
	    fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
		      dest-5*bytes);
	    fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
		      dest-6*bytes);
	    fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
		      dest-7*bytes);
	    bits = *cdat++;
	    fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
		      dest-8*bytes);
	    fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
		      dest-9*bytes);
	    fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
		      dest-10*bytes);
	    fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
		      dest-11*bytes);
	    if (fontwidth(p) == 16) {
		fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
			  dest-12*bytes);
		fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
			  dest-13*bytes);
		fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
			  dest-14*bytes);
		fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
			  dest-15*bytes);
	    }
	}
#else
	for (rows = fontheight(p); rows--; dest += bytes) {
	    bits = *cdat++;
	    fb_writel((tab_cfb16[bits >> 6] & eorx) ^ bgx, dest);
	    fb_writel((tab_cfb16[bits >> 4 & 3] & eorx) ^ bgx, dest+4);
	    fb_writel((tab_cfb16[bits >> 2 & 3] & eorx) ^ bgx, dest+8);
	    fb_writel((tab_cfb16[bits & 3] & eorx) ^ bgx, dest+12);
	    bits = *cdat++;
	    fb_writel((tab_cfb16[bits >> 6] & eorx) ^ bgx, dest+16);
	    fb_writel((tab_cfb16[bits >> 4 & 3] & eorx) ^ bgx, dest+20);
	    if (fontwidth(p) == 16) {
		fb_writel((tab_cfb16[bits >> 2 & 3] & eorx) ^ bgx, dest+24);
		fb_writel((tab_cfb16[bits & 3] & eorx) ^ bgx, dest+28);
	    }
	}
#endif
	break;
    }
}

#if defined(CONFIG_FBCON_ROTATE_R) || defined(CONFIG_FBCON_ROTATE_L)
void fbcon_cfb16_putcs(struct vc_data *conp, struct display *p,
		       const unsigned short *s, int count, int xx, int yy)
#else
void fbcon_cfb16_putcs(struct vc_data *conp, struct display *p,
		       const unsigned short *s, int count, int yy, int xx)
#endif
{
    u8 *cdat, *dest, *dest0;
    u16 c;
    int rows, bytes = p->next_line;
    u32 eorx, fgx, bgx;

#if defined(CONFIG_FBCON_ROTATE_R)
    dest0 = p->screen_base +
      yy * fontwidth(p) * bytes + (p->var.xres - xx * fontheight(p) - 1) * 2;
#elif defined(CONFIG_FBCON_ROTATE_L)
    dest0 = p->screen_base +
      (p->var.yres - yy * fontwidth(p) - 1) * bytes + xx * fontheight(p) * 2;
#else
    dest0 = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p) * 2;
#endif
    c = scr_readw(s);
    fgx = ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
    bgx = ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
    fgx |= (fgx << 16);
    bgx |= (bgx << 16);
    eorx = fgx ^ bgx;

    switch (fontwidth(p)) {
    case 4:
    case 8:
	while (count--) {
	    c = scr_readw(s++) & p->charmask;
	    cdat = p->fontdata + c * fontheight(p);
#if defined(CONFIG_FBCON_ROTATE_R)
	    for (rows = fontheight(p), dest = dest0; rows--; dest -= 2) {
		u8 bits = *cdat++;
	        fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
			  dest);
	        fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
			  dest+bytes);
	        fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
			  dest+2*bytes);
	        fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
			  dest+3*bytes);
		if (fontwidth(p) == 8) {
			fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
				  dest+4*bytes);
			fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
				  dest+5*bytes);
			fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
				  dest+6*bytes);
			fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
				  dest+7*bytes);
		}
	    }
	    dest0 += fontwidth(p) * bytes;
#elif defined(CONFIG_FBCON_ROTATE_L)
	    for (rows = fontheight(p), dest = dest0; rows--; dest += 2) {
		u8 bits = *cdat++;
	        fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
			  dest);
	        fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
			  dest-bytes);
	        fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
			  dest-2*bytes);
	        fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
			  dest-3*bytes);
		if (fontwidth(p) == 8) {
			fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
				  dest-4*bytes);
			fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
				  dest-5*bytes);
			fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
				  dest-6*bytes);
			fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
				  dest-7*bytes);
		}
	    }
	    dest0 -= fontwidth(p) * bytes;
#else
	    for (rows = fontheight(p), dest = dest0; rows--; dest += bytes) {
		u8 bits = *cdat++;
	        fb_writel((tab_cfb16[bits >> 6] & eorx) ^ bgx, dest);
	        fb_writel((tab_cfb16[bits >> 4 & 3] & eorx) ^ bgx, dest+4);
		if (fontwidth(p) == 8) {
		    fb_writel((tab_cfb16[bits >> 2 & 3] & eorx) ^ bgx, dest+8);
		    fb_writel((tab_cfb16[bits & 3] & eorx) ^ bgx, dest+12);
		}
	    }
	    dest0 += fontwidth(p)*2;;
#endif
	}
	break;
    case 12:
    case 16:
	while (count--) {
	    c = scr_readw(s++) & p->charmask;
	    cdat = p->fontdata + (c * fontheight(p) << 1);
#if defined(CONFIG_FBCON_ROTATE_R)
	    for (rows = fontheight(p), dest = dest0; rows--; dest -= 2) {
		u8 bits = *cdat++;
	        fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
			  dest);
	        fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
			  dest+bytes);
	        fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
			  dest+2*bytes);
	        fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
			  dest+3*bytes);
	        fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
			  dest+4*bytes);
	        fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
			  dest+5*bytes);
	        fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
			  dest+6*bytes);
	        fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
			  dest+7*bytes);
		bits = *cdat++;
	        fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
			  dest+8*bytes);
	        fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
			  dest+9*bytes);
	        fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
			  dest+10*bytes);
	        fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
			  dest+11*bytes);
		if (fontwidth(p) == 16) {
 	 		fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
				  dest+12*bytes);
			fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
				  dest+13*bytes);
			fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
				  dest+14*bytes);
			fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
				  dest+15*bytes);
		}
	    }
	    dest0 += fontwidth(p) * bytes;
#elif defined(CONFIG_FBCON_ROTATE_L)
	    for (rows = fontheight(p), dest = dest0; rows--; dest += 2) {
		u8 bits = *cdat++;
	        fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
			  dest);
	        fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
			  dest-bytes);
	        fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
			  dest-2*bytes);
	        fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
			  dest-3*bytes);
	        fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
			  dest-4*bytes);
	        fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
			  dest-5*bytes);
	        fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
			  dest-6*bytes);
	        fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
			  dest-7*bytes);
		bits = *cdat++;
	        fb_writew((tab_cfb16[bits >> 7 & 1] & eorx) ^ bgx,
			  dest-8*bytes);
	        fb_writew((tab_cfb16[bits >> 6 & 1] & eorx) ^ bgx,
			  dest-9*bytes);
	        fb_writew((tab_cfb16[bits >> 5 & 1] & eorx) ^ bgx,
			  dest-10*bytes);
	        fb_writew((tab_cfb16[bits >> 4 & 1] & eorx) ^ bgx,
			  dest-11*bytes);
		if (fontwidth(p) == 16) {
 	 		fb_writew((tab_cfb16[bits >> 3 & 1] & eorx) ^ bgx,
				  dest-12*bytes);
			fb_writew((tab_cfb16[bits >> 2 & 1] & eorx) ^ bgx,
				  dest-13*bytes);
			fb_writew((tab_cfb16[bits >> 1 & 1] & eorx) ^ bgx,
				  dest-14*bytes);
			fb_writew((tab_cfb16[bits & 1] & eorx) ^ bgx,
				  dest-15*bytes);
		}
	    }
	    dest0 -= fontwidth(p) * bytes;
#else
	    for (rows = fontheight(p), dest = dest0; rows--; dest += bytes) {
		u8 bits = *cdat++;
	        fb_writel((tab_cfb16[bits >> 6] & eorx) ^ bgx, dest);
	        fb_writel((tab_cfb16[bits >> 4 & 3] & eorx) ^ bgx, dest+4);
	        fb_writel((tab_cfb16[bits >> 2 & 3] & eorx) ^ bgx, dest+8);
	        fb_writel((tab_cfb16[bits & 3] & eorx) ^ bgx, dest+12);
		bits = *cdat++;
	        fb_writel((tab_cfb16[bits >> 6] & eorx) ^ bgx, dest+16);
	        fb_writel((tab_cfb16[bits >> 4 & 3] & eorx) ^ bgx, dest+20);
		if (fontwidth(p) == 16) {
		    fb_writel((tab_cfb16[bits >> 2 & 3] & eorx) ^ bgx, dest+24);
		    fb_writel((tab_cfb16[bits & 3] & eorx) ^ bgx, dest+28);
		}
	    }
	    dest0 += fontwidth(p)*2;
#endif
	}
	break;
    }
}

void fbcon_cfb16_revc(struct display *p, int xx, int yy)
{
    u8 *dest;
    int bytes = p->next_line, rows;

#if defined(CONFIG_FBCON_ROTATE_R)
    dest = p->screen_base +
      yy * fontwidth(p) * bytes + (p->var.xres - xx * fontheight(p) - 1) * 2;
#elif defined(CONFIG_FBCON_ROTATE_L)
    dest = p->screen_base +
      (p->var.yres - yy * fontwidth(p) - 1) * bytes + xx * fontheight(p) * 2;
#else
    dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p)*2;
#endif
#if defined(CONFIG_FBCON_ROTATE_R)
    for (rows = fontheight(p); rows--; dest -= 2) {
	switch (fontwidth(p)) {
	case 16:
	    fb_writew(fb_readw(dest+15*bytes) ^ 0xffff, dest+15*bytes);
	    fb_writew(fb_readw(dest+14*bytes) ^ 0xffff, dest+14*bytes);
	    fb_writew(fb_readw(dest+13*bytes) ^ 0xffff, dest+13*bytes);
	    fb_writew(fb_readw(dest+12*bytes) ^ 0xffff, dest+12*bytes);
	    /* FALL THROUGH */
	case 12:
	    fb_writew(fb_readw(dest+11*bytes) ^ 0xffff, dest+11*bytes);
	    fb_writew(fb_readw(dest+10*bytes) ^ 0xffff, dest+10*bytes);
	    fb_writew(fb_readw(dest+ 9*bytes) ^ 0xffff, dest+ 9*bytes);
	    fb_writew(fb_readw(dest+ 8*bytes) ^ 0xffff, dest+ 8*bytes);
	    /* FALL THROUGH */
	case 8:
	    fb_writew(fb_readw(dest+ 7*bytes) ^ 0xffff, dest+ 7*bytes);
	    fb_writew(fb_readw(dest+ 6*bytes) ^ 0xffff, dest+ 6*bytes);
	    fb_writew(fb_readw(dest+ 5*bytes) ^ 0xffff, dest+ 5*bytes);
	    fb_writew(fb_readw(dest+ 4*bytes) ^ 0xffff, dest+ 4*bytes);
	    /* FALL THROUGH */
	case 4:
	    fb_writew(fb_readw(dest+ 3*bytes) ^ 0xffff, dest+ 3*bytes);
	    fb_writew(fb_readw(dest+ 2*bytes) ^ 0xffff, dest+ 2*bytes);
	    fb_writew(fb_readw(dest+ 1*bytes) ^ 0xffff, dest+ 1*bytes);
	    fb_writew(fb_readw(dest         ) ^ 0xffff, dest);
	}
    }
#elif defined(CONFIG_FBCON_ROTATE_L)
    for (rows = fontheight(p); rows--; dest += 2) {
	switch (fontwidth(p)) {
	case 16:
	    fb_writew(fb_readw(dest-15*bytes) ^ 0xffff, dest-15*bytes);
	    fb_writew(fb_readw(dest-14*bytes) ^ 0xffff, dest-14*bytes);
	    fb_writew(fb_readw(dest-13*bytes) ^ 0xffff, dest-13*bytes);
	    fb_writew(fb_readw(dest-12*bytes) ^ 0xffff, dest-12*bytes);
	    /* FALL THROUGH */
	case 12:
	    fb_writew(fb_readw(dest-11*bytes) ^ 0xffff, dest-11*bytes);
	    fb_writew(fb_readw(dest-10*bytes) ^ 0xffff, dest-10*bytes);
	    fb_writew(fb_readw(dest- 9*bytes) ^ 0xffff, dest- 9*bytes);
	    fb_writew(fb_readw(dest- 8*bytes) ^ 0xffff, dest- 8*bytes);
	    /* FALL THROUGH */
	case 8:
	    fb_writew(fb_readw(dest- 7*bytes) ^ 0xffff, dest- 7*bytes);
	    fb_writew(fb_readw(dest- 6*bytes) ^ 0xffff, dest- 6*bytes);
	    fb_writew(fb_readw(dest- 5*bytes) ^ 0xffff, dest- 5*bytes);
	    fb_writew(fb_readw(dest- 4*bytes) ^ 0xffff, dest- 4*bytes);
	    /* FALL THROUGH */
	case 4:
	    fb_writew(fb_readw(dest- 3*bytes) ^ 0xffff, dest- 3*bytes);
	    fb_writew(fb_readw(dest- 2*bytes) ^ 0xffff, dest- 2*bytes);
	    fb_writew(fb_readw(dest- 1*bytes) ^ 0xffff, dest- 1*bytes);
	    fb_writew(fb_readw(dest         ) ^ 0xffff, dest);
	}
    }
#else
    for (rows = fontheight(p); rows--; dest += bytes) {
	switch (fontwidth(p)) {
	case 16:
	    fb_writel(fb_readl(dest+24) ^ 0xffffffff, dest+24);
	    fb_writel(fb_readl(dest+28) ^ 0xffffffff, dest+28);
	    /* FALL THROUGH */
	case 12:
	    fb_writel(fb_readl(dest+16) ^ 0xffffffff, dest+16);
	    fb_writel(fb_readl(dest+20) ^ 0xffffffff, dest+20);
	    /* FALL THROUGH */
	case 8:
	    fb_writel(fb_readl(dest+8) ^ 0xffffffff, dest+8);
	    fb_writel(fb_readl(dest+12) ^ 0xffffffff, dest+12);
	    /* FALL THROUGH */
	case 4:
	    fb_writel(fb_readl(dest+0) ^ 0xffffffff, dest+0);
	    fb_writel(fb_readl(dest+4) ^ 0xffffffff, dest+4);
	}
    }
#endif
}

void fbcon_cfb16_clear_margins(struct vc_data *conp, struct display *p,
			       int bottom_only)
{
    int bytes = p->next_line;
    u32 bgx;

    unsigned int right_start = conp->vc_cols*fontwidth(p);
    unsigned int bottom_start = conp->vc_rows*fontheight(p);
    unsigned int right_width, bottom_width;

    bgx = ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];

#if defined(CONFIG_FBCON_ROTATE_R)
#ifdef CONFIG_FB_CORGI
    if (!bottom_only && (right_width = p->var.yres - right_start))
	rectfill(p->screen_base + right_start * bytes + (p->var.xres - 1) * 2,
		 right_width, p->var.xres_virtual, bgx, bytes);
    if ((bottom_width = p->var.xres - bottom_start))
	rectfill(p->screen_base +
		 (p->var.xres - (p->var.xoffset + bottom_start) - 1) * 2,
		 right_start, bottom_width, bgx, bytes);
#else // general R
    if (!bottom_only && (right_width = p->var.yres - right_start))
	rectfill(p->screen_base + right_start * bytes + (p->var.xres - 1) * 2,
		 right_width, p->var.xres_virtual, bgx, bytes);
    if ((bottom_width = p->var.yres - bottom_start))
	rectfill(p->screen_base +
		 (p->var.xres - (p->var.xoffset + bottom_start) - 1) * 2,
		 right_start, bottom_width, bgx, bytes);
#endif
#elif defined(CONFIG_FBCON_ROTATE_L)
#ifdef CONFIG_FB_CORGI
    if (!bottom_only && (right_width = p->var.yres - right_start))
	rectfill(p->screen_base + (p->var.yres - right_start - 1) * bytes,
		 right_width, p->var.xres_virtual, bgx, bytes);
    if ((bottom_width = p->var.xres - bottom_start))
	rectfill(p->screen_base + (p->var.yres - 1) * bytes +
		 (p->var.xoffset + bottom_start) * 2,
		 right_start, bottom_width, bgx, bytes);
#else // general L
    if (!bottom_only && (right_width = p->var.yres - right_start))
	rectfill(p->screen_base + (p->var.yres - right_start - 1) * bytes,
		 right_width, p->var.xres_virtual, bgx, bytes);
    if ((bottom_width = p->var.yres - bottom_start))
	rectfill(p->screen_base + (p->var.yres - 1) * bytes +
		 (p->var.xoffset + bottom_start) * 2,
		 right_start, bottom_width, bgx, bytes);
#endif
#else
    if (!bottom_only && (right_width = p->var.xres-right_start))
	rectfill(p->screen_base+right_start*2, right_width,
		 p->var.yres_virtual, bgx, bytes);
    if ((bottom_width = p->var.yres-bottom_start))
	rectfill(p->screen_base+(p->var.yoffset+bottom_start)*bytes,
		 right_start, bottom_width, bgx, bytes);
#endif
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_cfb16 = {
    setup:		fbcon_cfb16_setup,
    bmove:		fbcon_cfb16_bmove,
    clear:		fbcon_cfb16_clear,
    putc:		fbcon_cfb16_putc,
    putcs:		fbcon_cfb16_putcs,
    revc:		fbcon_cfb16_revc,
    clear_margins:	fbcon_cfb16_clear_margins,
    fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16)
};


#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{}
#endif /* MODULE */


    /*
     *  Visible symbols for modules
     */

EXPORT_SYMBOL(fbcon_cfb16);
EXPORT_SYMBOL(fbcon_cfb16_setup);
EXPORT_SYMBOL(fbcon_cfb16_bmove);
EXPORT_SYMBOL(fbcon_cfb16_clear);
EXPORT_SYMBOL(fbcon_cfb16_putc);
EXPORT_SYMBOL(fbcon_cfb16_putcs);
EXPORT_SYMBOL(fbcon_cfb16_revc);
EXPORT_SYMBOL(fbcon_cfb16_clear_margins);
