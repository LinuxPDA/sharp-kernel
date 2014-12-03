/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/drivers/video/dbmx1fb.c
 *
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/pm.h>

#include <asm/io.h>

#include <video/fbcon.h>
#if defined(CONFIG_FBCON_CFB16)
#include <video/fbcon-cfb16.h>
#elif defined(CONFIG_FBCON_CFB8)
#include <video/fbcon-cfb8.h>
#endif

#define	VA_LCDC_BASE		IO_ADDRESS(DBMX1_LCDC_BASE)
#define	DBMX1_LCDC_SSA		(*(volatile u32 *)(VA_LCDC_BASE + 0x000))
#define	DBMX1_LCDC_SIZE		(*(volatile u32 *)(VA_LCDC_BASE + 0x004))
#define	DBMX1_LCDC_VPW		(*(volatile u32 *)(VA_LCDC_BASE + 0x008))
#define	DBMX1_LCDC_CPOS		(*(volatile u32 *)(VA_LCDC_BASE + 0x00c))
#define	DBMX1_LCDC_LCWHB	(*(volatile u32 *)(VA_LCDC_BASE + 0x010))
#define	DBMX1_LCDC_LCHCC	(*(volatile u32 *)(VA_LCDC_BASE + 0x014))
#define	DBMX1_LCDC_PCR		(*(volatile u32 *)(VA_LCDC_BASE + 0x018))
#define	DBMX1_LCDC_HCR		(*(volatile u32 *)(VA_LCDC_BASE + 0x01c))
#define	DBMX1_LCDC_VCR		(*(volatile u32 *)(VA_LCDC_BASE + 0x020))
#define	DBMX1_LCDC_POS		(*(volatile u32 *)(VA_LCDC_BASE + 0x024))
#define	DBMX1_LCDC_LGPMR	(*(volatile u32 *)(VA_LCDC_BASE + 0x028))
#define	DBMX1_LCDC_PWMR		(*(volatile u32 *)(VA_LCDC_BASE + 0x02c))
#define	DBMX1_LCDC_DMACR	(*(volatile u32 *)(VA_LCDC_BASE + 0x030))
#define	DBMX1_LCDC_RMCR		(*(volatile u32 *)(VA_LCDC_BASE + 0x034))
#define	DBMX1_LCDC_LCDICR	(*(volatile u32 *)(VA_LCDC_BASE + 0x038))
#define	DBMX1_LCDC_LCDISR	(*(volatile u32 *)(VA_LCDC_BASE + 0x040))
#define	DBMX1_LCDC_MAPRAM	( (volatile u32 *)(VA_LCDC_BASE + 0x800))

#if defined(FBCON_HAS_CFB16)
#define	BPP	16
#elif defined(FBCON_HAS_CFB8)
#define	BPP	8
#endif

#define	VRAM_SIZE	(240 * 320 * BPP / 8)

#if defined(FBCON_HAS_CFB16)
static u16 colreg[16];
#elif defined(FBCON_HAS_CFB8)
static u16 colreg[256];
#endif
static int currcon = 0;
static struct fb_info fb_info;
static struct display display;
static unsigned long vram_base;
static unsigned long vram_base_phys;

static int dbmx1fb_getcolreg(u_int regno, u_int *red, u_int *green,
			     u_int *blue, u_int *transp, struct fb_info *info)
{
#if defined(FBCON_HAS_CFB16)
    if (regno > 15)
	return 1;

    *red = colreg[regno] & 0xf800;
    *green = colreg[regno] & 0x7e0 << 5;
    *blue = colreg[regno] & 0x1f << 11;
    *transp = 0;
#elif defined(FBCON_HAS_CFB8)
    if (regno > 255)
	return 1;

    *red = colreg[regno] & 0x0f00 << 4;
    *green = colreg[regno] & 0x00f0 << 8;
    *blue = colreg[regno] & 0x000f << 12;
    *transp = 0;
#endif
    return 0;
}

static int dbmx1fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			     u_int transp, struct fb_info *info)
{
#if defined(FBCON_HAS_CFB16)
    if (regno > 15)
	return 1;

    colreg[regno] = (red & 0xf800) | (green & 0xfc00 >> 5) |
	(blue & 0xf800 >> 11);
#elif defined(FBCON_HAS_CFB8)
    if (regno > 255)
	return 1;

    colreg[regno] = (red & 0xf000 >> 4) | (green & 0xf000 >> 8) |
	(blue & 0xf000 >> 12);
    DBMX1_LCDC_MAPRAM[regno] = colreg[regno];
#endif
    return 0;
}

static int dbmx1fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			   struct fb_info *info)
{
    memset(fix, 0, sizeof(struct fb_fix_screeninfo));
    strcpy(fix->id, "DBMX1FB");
    fix->smem_start = vram_base_phys;
    fix->smem_len = VRAM_SIZE;
    fix->type = FB_TYPE_PACKED_PIXELS;
    fix->type_aux = 0;
    fix->visual = display.visual;
    fix->xpanstep = 0;
    fix->ypanstep = 0;
    fix->ywrapstep = 0;
    fix->line_length = 240 * BPP / 8;
    fix->accel = FB_ACCEL_NONE;
    return 0;
}

static int dbmx1fb_get_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info)
{
    memset(var, 0, sizeof(struct fb_var_screeninfo));
    var->xres = 240;
    var->yres = 320;
    var->xres_virtual = 240;
    var->yres_virtual = 320;
    var->xoffset = 0;
    var->yoffset = 0;
    var->bits_per_pixel = BPP;
    var->grayscale = 0;
#if defined(FBCON_HAS_CFB16)
    var->red.offset = 11;
    var->red.length = 5;
    var->green.offset = 5;
    var->green.length = 6;
    var->blue.offset = 0;
    var->blue.length = 5;
#elif defined(FBCON_HAS_CFB8)
    var->red.offset = 0;
    var->red.length = 4;
    var->green.offset = 0;
    var->green.length = 4;
    var->blue.offset = 0;
    var->blue.length = 4;
#endif
    var->transp.offset = 0;
    var->transp.length = 0;
    var->nonstd = 0;
    var->activate = FB_ACTIVATE_NOW;
    var->height = -1;
    var->width = -1;
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

static int dbmx1fb_set_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info)
{
    return -EINVAL;
}

static int dbmx1fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info)
{
    if (con == currcon)
	return fb_get_cmap(cmap, kspc, dbmx1fb_getcolreg, info);
    else if (fb_display[con].cmap.len)
	fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
    else
	fb_copy_cmap(fb_default_cmap(16), cmap, kspc ? 0 : 2);
    return 0;
}

static int dbmx1fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info)
{
    int err;

    if (!fb_display[con].cmap.len) {
	if ((err = fb_alloc_cmap(&fb_display[con].cmap, 16, 0)))
	    return err;
    }
    if (con == currcon)
	return fb_set_cmap(cmap, kspc, dbmx1fb_setcolreg, info);
    else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
    return 0;
}

static int dbmx1fb_switch_con(int con, struct fb_info *info)
{
    currcon = con;
    return 0;

}

static int dbmx1fb_updatevar(int con, struct fb_info *info)
{
    return 0;
}

static void dbmx1fb_blank(int blank, struct fb_info *info)
{
}

static void dbmx1fb_lcd_power_on(void)
{
#ifdef CONFIG_ARCH_APLAT
//    DBMX1_GPIO_DR_A  =  0x00000040;	// power off
    DBMX1_GPIO_DR_B &= ~0x00800000;	// LCD 3.3V on
    DBMX1_GPIO_DR_B &= ~0x00200000;	// LCD 5.0V on
    DBMX1_GPIO_DR_A |=  0x00000020;	// Controller power on
    DBMX1_GPIO_DR_A |=  0x00000010;	// LCD panel power on
    DBMX1_GPIO_DR_A &= ~0x00000040;	// Backlight power on
    DBMX1_GPIO_DR_A |=  0x00000008;	// Power on clear
#endif
}

static void dbmx1fb_lcd_power_off(void)
{
#ifdef CONFIG_ARCH_APLAT
    DBMX1_GPIO_DR_A |=  0x00000040;	// Backlight power off
    DBMX1_GPIO_DR_A &= ~0x00000010;	// LCD panel power off
    DBMX1_GPIO_DR_A &= ~0x00000020;	// Controller power off
    DBMX1_GPIO_DR_B |=  0x00200000;	// LCD 5.0V off
    DBMX1_GPIO_DR_B |=  0x00800000;	// LCD 3.3V off
#endif
}

#ifdef CONFIG_PM
static int dbmx1fb_pm_callback(struct pm_dev *pm_dev, pm_request_t req,
			       void *data)
{
    switch (req) {
    case PM_SUSPEND:
	dbmx1fb_lcd_power_off();
	break;
    case PM_RESUME:
	dbmx1fb_lcd_power_on();
	break;
    }
    return 0;
}
#endif

static struct fb_ops dbmx1fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	dbmx1fb_get_fix,
	fb_get_var:	dbmx1fb_get_var,
	fb_set_var:	dbmx1fb_set_var,
	fb_get_cmap:	dbmx1fb_get_cmap,
	fb_set_cmap:	dbmx1fb_set_cmap,
};

int __init dbmx1fb_init(void)
{
    int i, j;
    char *virt[10];
    dma_addr_t phys[10];

    memset(&fb_info, 0, sizeof(struct fb_info));
    strcpy(fb_info.modename, "DBMX1FB");
    fb_info.node = -1;
    fb_info.flags = FBINFO_FLAG_DEFAULT;
    fb_info.fbops = &dbmx1fb_ops;
    fb_info.disp = &display;
    strcpy(fb_info.fontname, "VGA8x16");
    fb_info.changevar = NULL;
    fb_info.switch_con = &dbmx1fb_switch_con;
    fb_info.updatevar = &dbmx1fb_updatevar;
    fb_info.blank = &dbmx1fb_blank;

    memset(&display, 0, sizeof(struct display));
    dbmx1fb_get_var(&display.var, 0, &fb_info);
#if defined(FBCON_HAS_CFB16)
    for (i = 0; i < 10; i++) {
	virt[i] = consistent_alloc(GFP_KERNEL | GFP_DMA, VRAM_SIZE, &phys[i]);
	// 4MB boundary check
	if ((phys[i] & 0xffc00000) == ((phys[i] + VRAM_SIZE - 1) & 0xffc00000))
	    break;
    }
    for (j = 0; j < i; j++)
	consistent_free(virt[i], VRAM_SIZE, phys[i]);
    if (i >= 10)
	return -EINVAL;
    vram_base_phys = phys[i];
    vram_base = (unsigned long)virt[i];
#elif defined(FBCON_HAS_CFB8)
    vram_base_phys = DBMX1_SRAM_BASE;
    vram_base = IO_ADDRESS(DBMX1_SRAM_BASE);
#endif

    display.screen_base = vram_base;
#if defined(FBCON_HAS_CFB16)
    display.visual = FB_VISUAL_TRUECOLOR;
#elif defined(FBCON_HAS_CFB8)
    display.visual = FB_VISUAL_PSEUDOCOLOR;
#endif
    display.type = FB_TYPE_PACKED_PIXELS;
    display.type_aux = 0;
    display.ypanstep = 0;
    display.ywrapstep = 0;
    display.line_length = 240 * BPP / 8;
    display.can_soft_blank = 1;
    display.inverse = 0;

#if defined(FBCON_HAS_CFB16)
    display.dispsw = &fbcon_cfb16;
    display.dispsw_data = colreg;
#elif defined(FBCON_HAS_CFB8)
    display.dispsw = &fbcon_cfb8;
#else
    display.dispsw = &fbcon_dummy;
#endif
//    DBMX1_GPIO_GIUS_D &= ~0x7ffffc40;	// LCDC enable
//    DBMX1_GPIO_GPR_D  &= ~0x7ffffc40;
#if 0
    DBMX1_SPI2_RESETREG2 = 1;
    DBMX1_SPI2_RESETREG2 = 0;
    DBMX1_SPI2_CONTROLREG2 = 0x00002607;
    DBMX1_GPIO_DR_B &= ~0x00800000;	// LCD 3.3V on
#endif
#if 0
    printk("CSCR : %08x\n", *(volatile u32 *)0xf021b000);
    printk("PCDR : %08x\n", *(volatile u32 *)0xf021b020);
    printk("SSA  : %08x\n", DBMX1_LCDC_SSA);
    printk("POS  : %08x\n", DBMX1_LCDC_POS);
    printk("PWMR : %08x\n", DBMX1_LCDC_PWMR);
    printk("RMCR : %08x\n", DBMX1_LCDC_RMCR);
    printk("DMACR: %08x\n", DBMX1_LCDC_DMACR);
#endif
    DBMX1_LCDC_SSA   = vram_base_phys;
    DBMX1_LCDC_SIZE  = 0x00f00140;	// 240x320
#if defined(FBCON_HAS_CFB16)
    DBMX1_LCDC_VPW   = 0x00000078;	// 240
    DBMX1_LCDC_PCR   = 0xc8280084;
#elif defined(FBCON_HAS_CFB8)
    DBMX1_LCDC_VPW   = 0x0000003c;	// 240
    DBMX1_LCDC_PCR   = 0xc62a0084;
#endif
    DBMX1_LCDC_HCR   = 0x00001615;
    DBMX1_LCDC_VCR   = 0x04000506;
    DBMX1_LCDC_DMACR = 0x00040008;
    DBMX1_LCDC_RMCR  = 0x00000002;

    dbmx1fb_lcd_power_on();

    if (register_framebuffer(&fb_info) < 0)
	return -EINVAL;

#ifdef CONFIG_PM
    pm_register(PM_SYS_DEV, PM_SYS_VGA, dbmx1fb_pm_callback);
#endif

    MOD_INC_USE_COUNT;
    return 0;
}
