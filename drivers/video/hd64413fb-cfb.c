/*
 *  linux/drivers/video/cfb8.c -- Low level frame buffer operations for 8 bpp
 *				  packed pixels
 *
 *	Created 5 Apr 1997 by Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/fb.h>

#include <video/fbcon.h>

#include "hd64413fb.h"

    /*
     *  8 bpp packed pixels
     */

static u32 tab_cfb8[] = {
#if defined(__BIG_ENDIAN)
    0x00000000,0x000000ff,0x0000ff00,0x0000ffff,
    0x00ff0000,0x00ff00ff,0x00ffff00,0x00ffffff,
    0xff000000,0xff0000ff,0xff00ff00,0xff00ffff,
    0xffff0000,0xffff00ff,0xffffff00,0xffffffff
#elif defined(__LITTLE_ENDIAN)
    0x00000000,0xff000000,0x00ff0000,0xffff0000,
    0x0000ff00,0xff00ff00,0x00ffff00,0xffffff00,
    0x000000ff,0xff0000ff,0x00ff00ff,0xffff00ff,
    0x0000ffff,0xff00ffff,0x00ffffff,0xffffffff
#else
#error FIXME: No endianness??
#endif
};

static u32 tab_cfb16[] = {
#if defined(__BIG_ENDIAN)
    0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff
#elif defined(__LITTLE_ENDIAN)
    0x00000000, 0xffff0000, 0x0000ffff, 0xffffffff
#else
#error FIXME: No endianness??
#endif
};

void hd64413fb_cfb_setup(struct display *p)
{
DPRINTK("hd64413fb_cfb_setup\n");
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual;
    p->next_plane = 0;
}

void hd64413fb_cfb_bmove(struct display *p, int sy, int sx, int dy, int dx,
		      int height, int width)
{
DPRINTK("hd64413fb_cfb_bmove:%d, %d, %d, %d, %d, %d\n", sy, sx, dy, dx, height, width);
/*
    int bytes = p->next_line, linesize = bytes * fontheight(p), rows;
    u8 *src,*dst;

    if (sx == 0 && dx == 0 && width * fontwidth(p) == bytes) {
	fb_memmove(p->screen_base + dy * linesize,
		  p->screen_base + sy * linesize,
		  height * linesize);
	return;
    }
    if (fontwidthlog(p)) {
    	sx <<= fontwidthlog(p); dx <<= fontwidthlog(p); width <<= fontwidthlog(p);
    } else {
    	sx *= fontwidth(p); dx *= fontwidth(p); width *= fontwidth(p);
    }
    if (dy < sy || (dy == sy && dx < sx)) {
	src = p->screen_base + sy * linesize + sx;
	dst = p->screen_base + dy * linesize + dx;
	for (rows = height * fontheight(p) ; rows-- ;) {
	    fb_memmove(dst, src, width);
	    src += bytes;
	    dst += bytes;
	}
    } else {
	src = p->screen_base + (sy+height) * linesize + sx - bytes;
	dst = p->screen_base + (dy+height) * linesize + dx - bytes;
	for (rows = height * fontheight(p) ; rows-- ;) {
	    fb_memmove(dst, src, width);
	    src -= bytes;
	    dst -= bytes;
	}
    }
*/
}

static inline void rectfill(u8 *dest, int width, int height, u8 data,
			    int linesize)
{
/*
    while (height-- > 0) {
	fb_memset(dest, data, width);
	dest += linesize;
    }
*/
}

void hd64413fb_cfb_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		      int height, int width)
{

    int x, y, c;

    DPRINTK("hd64413fb_cfb_clear:%d, %d, %d, %d\n", sy, sx, height, width);

    c = ' ' | ((p->fgcol & 0x0f) << p->fgshift) |
	((p->bgcol & 0x0f) << p->bgshift);
    for (y = sy; y < sy + height; y++) {
	for (x = sx; x < sx + width; x++) {
	    hd64413fb_cfb_putc(conp, p, c, y, x);
	}
    }
/*
    u8 *dest;
    int bytes=p->next_line,lines=height * fontheight(p);
    u8 bgx;

    dest = p->screen_base + sy * fontheight(p) * bytes + sx * fontwidth(p);

    bgx=attr_bgcol_ec(p,conp);

    width *= fontwidth(p);
    if (width == bytes)
	rectfill(dest, lines * width, 1, bgx, bytes);
    else
	rectfill(dest, width, lines, bgx, bytes);
*/
}

void hd64413fb_cfb_putc(struct vc_data *conp, struct display *p, int c, int yy,
			int xx)
{
    u8 *dest,*cdat,mask,data,bits;
    int bytes=p->next_line,rows,i,j,blocksize;
    u32 eorx,fgx,bgx;
    struct hd64413fb_par *par = (struct hd64413fb_par *)p->dispsw_data;

#if 1
    dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p) * (par->bpp / 8);
#else
    bytes = par->block_width * par->bpp / 8;
    blocksize = bytes * par->block_height;
    dest = p->screen_base;
    dest += yy * par->wrap_offset * par->block_height * par->bpp / 8;
    i = par->block_width / fontwidth(p);
    dest += ((xx / i) * blocksize +
	     (xx & (i - 1)) * fontwidth(p) * par->bpp / 8);
#endif
    if (fontwidth(p) <= 8)
	cdat = p->fontdata + (c & p->charmask) * fontheight(p);
    else
	cdat = p->fontdata + ((c & p->charmask) * fontheight(p) << 1);

    fgx=attr_fgcol(p,c);
    bgx=attr_bgcol(p,c);
    if (par->bpp == 8) {
	fgx |= (fgx << 8);
	bgx |= (bgx << 8);
	fgx |= (fgx << 16);
	bgx |= (bgx << 16); 
    } else if (par->bpp == 16) {
	fgx = ((u16 *)par->cmap)[fgx];
	bgx = ((u16 *)par->cmap)[bgx];
	fgx |= (fgx << 16);
	bgx |= (bgx << 16); 
    }
    eorx = fgx ^ bgx;

/*
    DPRINTK("hd64413fb_cfb_putc:%c %08x %d %d %d %d %08x %08x %08x %d %d %d %d %d %d\n",
	   c & p->charmask, c, xx, yy, attr_fgcol(p,c), attr_bgcol(p,c),
	   fgx, bgx, eorx,
	   fontwidth(p), fontheight(p),
	   par->bpp, par->block_width, par->block_height, par->wrap_offset);
*/

    switch (fontwidth(p)) {
/*
    case 4:
	for (rows = fontheight(p) ; rows-- ; dest += bytes)
	    fb_writel((nibbletab_cfb8[*cdat++ >> 4] & eorx) ^ bgx, dest);
        break;
*/
    case 8:
/*
	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
	    fb_writel((nibbletab_cfb8[*cdat >> 4] & eorx) ^ bgx, dest);
	    fb_writel((nibbletab_cfb8[*cdat++ & 0xf] & eorx) ^ bgx, dest+4);
        }
*/
	if (par->bpp == 8) {
	    for (rows = fontheight(p) ; rows-- ; dest += bytes) {
		bits = *cdat++;
		fb_writel((tab_cfb8[bits >> 4] & eorx) ^ bgx, dest);
		fb_writel((tab_cfb8[bits & 0xf] & eorx) ^ bgx, dest+4);
	    }
	} else if (par->bpp == 16) {
	    for (rows = fontheight(p); rows--; dest += bytes) {
		bits = *cdat++;
		fb_writel((tab_cfb16[bits >> 6] & eorx) ^ bgx, dest);
		fb_writel((tab_cfb16[bits >> 4 & 3] & eorx) ^ bgx, dest+4);
		if (fontwidth(p) == 8) {
		    fb_writel((tab_cfb16[bits >> 2 & 3] & eorx) ^ bgx, dest+8);
		    fb_writel((tab_cfb16[bits & 3] & eorx) ^ bgx, dest+12);
		}
	    }
        }
        break;
/*
    case 12:
    case 16:
	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
	    fb_writel((nibbletab_cfb8[*cdat >> 4] & eorx) ^ bgx, dest);
	    fb_writel((nibbletab_cfb8[*cdat++ & 0xf] & eorx) ^ bgx, dest+4);
	    fb_writel((nibbletab_cfb8[(*cdat >> 4) & 0xf] & eorx) ^ bgx, dest+8);
	    if (fontwidth(p) == 16)
		fb_writel((nibbletab_cfb8[*cdat & 0xf] & eorx) ^ bgx, dest+12);
	    cdat++;
        }
        break;
*/
    }
}

void hd64413fb_cfb_putcs(struct vc_data *conp, struct display *p, 
		      const unsigned short *s, int count, int yy, int xx)
{
    u8 *cdat, *dest, *dest0;
    u16 c;
    int rows,bytes=p->next_line;
    u32 eorx, fgx, bgx;

    //DPRINTK("hd64413fb_cfb_putcs: %08x %04x\n", s, *s);

    while (count--) {
	hd64413fb_cfb_putc(conp, p, scr_readw(s++), yy, xx);
	//hd64413fb_cfb_putc(conp, p, *s++, yy, xx);
	xx++;
    }
/*
    dest0 = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p);
    fgx=attr_fgcol(p,scr_readw(s));
    bgx=attr_bgcol(p,scr_readw(s));
    fgx |= (fgx << 8);
    fgx |= (fgx << 16);
    bgx |= (bgx << 8);
    bgx |= (bgx << 16);
    eorx = fgx ^ bgx;
    switch (fontwidth(p)) {
    case 4:
	while (count--) {
	    c = scr_readw(s++) & p->charmask;
	    cdat = p->fontdata + c * fontheight(p);

	    for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes)
		fb_writel((nibbletab_cfb8[*cdat++ >> 4] & eorx) ^ bgx, dest);
	    dest0+=4;
        }
        break;
    case 8:
	while (count--) {
	    c = scr_readw(s++) & p->charmask;
	    cdat = p->fontdata + c * fontheight(p);

	    for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes) {
		fb_writel((nibbletab_cfb8[*cdat >> 4] & eorx) ^ bgx, dest);
		fb_writel((nibbletab_cfb8[*cdat++ & 0xf] & eorx) ^ bgx, dest+4);
	    }
	    dest0+=8;
        }
        break;
    case 12:
    case 16:
	while (count--) {
	    c = scr_readw(s++) & p->charmask;
	    cdat = p->fontdata + (c * fontheight(p) << 1);

	    for (rows = fontheight(p), dest = dest0; rows-- ; dest += bytes) {
		fb_writel((nibbletab_cfb8[*cdat >> 4] & eorx) ^ bgx, dest);
		fb_writel((nibbletab_cfb8[*cdat++ & 0xf] & eorx) ^ bgx, dest+4);
		fb_writel((nibbletab_cfb8[(*cdat >> 4) & 0xf] & eorx) ^ bgx, dest+8);
		if (fontwidth(p) == 16)
		   fb_writel((nibbletab_cfb8[*cdat & 0xf] & eorx) ^ bgx, dest+12);
		cdat++;
	    }
	    dest0+=fontwidth(p);
        }
        break;
    }
*/
}

void hd64413fb_cfb_revc(struct display *p, int xx, int yy)
{
//DPRINTK("hd64413fb_cfb_revc:%d, %d\n", xx, yy);
    u8 *dest;
    int bytes=p->next_line, rows, i, blocksize;
    struct hd64413fb_par *par = (struct hd64413fb_par *)p->dispsw_data;

    dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p);
    //bytes = par->block_width * par->bpp / 8;
    //blocksize = bytes * par->block_height;
    //dest = p->screen_base;
    //dest += yy * par->wrap_offset * par->block_height * par->bpp / 8;
    //i = par->block_width / fontwidth(p);
    //dest += ((xx / i) * blocksize + (xx & (i - 1)) * fontwidth(p) * par->bpp / 8);

    if (par->bpp == 8) {
#if 1
	for (rows = fontheight(p) ; rows-- ; dest += bytes) {
	    switch (fontwidth(p)) {
	    case 16:
		fb_writel(fb_readl(dest+12) ^ 0x0f0f0f0f, dest+12);
		/* fall thru */
	    case 12:
		fb_writel(fb_readl(dest+8) ^ 0x0f0f0f0f, dest+8);
		/* fall thru */
	    case 8:
		fb_writel(fb_readl(dest+4) ^ 0x0f0f0f0f, dest+4);
		/* fall thru */
	    case 4:
		fb_writel(fb_readl(dest) ^ 0x0f0f0f0f, dest);
	    }
    	}
#else
	int j, k;
	u16 data[20];
	for (rows = fontheight(p); rows--; dest += bytes) {
	    for (j = 0; j < fontwidth(p) / 2; j++) {
		data[0] = fb_readw(dest + j * 2);
		data[1] = fb_readw(dest + j * 2);
		k = 1;
		do {
		    k++;
		    data[k] = fb_readw(dest + j * 2);
		    if (data[k] == data[k - 1] && data[k] == data[k - 2])
			break;
		} while (k < 20 - 1);
		fb_writew(data[k] ^ 0x0f0f, dest + j * 2);
	    }
	}
#endif
    } else if (par->bpp == 16) {
	dest = p->screen_base + yy * fontheight(p) * bytes + xx * fontwidth(p) * 2;
#if 1
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
#else
	int j, k;
	u16 data[20];
	for (rows = fontheight(p); rows--; dest += bytes) {
	    for (j = 0; j < fontwidth(p); j++) {
		data[0] = fb_readw(dest + j * 2);
		data[1] = fb_readw(dest + j * 2);
		k = 1;
		do {
		    k++;
		    data[k] = fb_readw(dest + j * 2);
		    if (data[k] == data[k - 1] && data[k] == data[k - 2])
			break;
		} while (k < 20 - 1);
		fb_writew(data[k] ^ 0xffff, dest + j * 2);
	    }
	}
#endif
    }
}

void hd64413fb_cfb_clear_margins(struct vc_data *conp, struct display *p,
			      int bottom_only)
{
DPRINTK("hd64413fb_cfb_clear_margins:%d\n", bottom_only);
/*
    int bytes=p->next_line;
    u8 bgx;

    unsigned int right_start = conp->vc_cols*fontwidth(p);
    unsigned int bottom_start = conp->vc_rows*fontheight(p);
    unsigned int right_width, bottom_width;

    bgx=attr_bgcol_ec(p,conp);

    if (!bottom_only && (right_width = p->var.xres-right_start))
	rectfill(p->screen_base+right_start, right_width, p->var.yres_virtual,
		 bgx, bytes);
    if ((bottom_width = p->var.yres-bottom_start))
	rectfill(p->screen_base+(p->var.yoffset+bottom_start)*bytes,
		 right_start, bottom_width, bgx, bytes);
*/
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch hd64413fb_cfb = {
    setup:		hd64413fb_cfb_setup,
    bmove:		hd64413fb_cfb_bmove,
    clear:		hd64413fb_cfb_clear,
    putc:		hd64413fb_cfb_putc,
    putcs:		hd64413fb_cfb_putcs,
    revc:		hd64413fb_cfb_revc,
    clear_margins:	hd64413fb_cfb_clear_margins,
    fontwidthmask:	FONTWIDTH(8)
};


#ifdef MODULE
int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{}
#endif /* MODULE */

#if 0
    /*
     *  Visible symbols for modules
     */

EXPORT_SYMBOL(hd64413fb_cfb);
EXPORT_SYMBOL(hd64413fb_cfb_setup);
EXPORT_SYMBOL(hd64413fb_cfb_bmove);
EXPORT_SYMBOL(hd64413fb_cfb_clear);
EXPORT_SYMBOL(hd64413fb_cfb_putc);
EXPORT_SYMBOL(hd64413fb_cfb_putcs);
EXPORT_SYMBOL(hd64413fb_cfb_revc);
EXPORT_SYMBOL(hd64413fb_cfb_clear_margins);
#endif
