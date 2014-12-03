/*
 * linux/drivers/video/tc6393fb.h
 *
 * Frame Buffer Device for TOSHIBA tc6393
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#if !defined (_TC6393FB)
#define _TC6393FB

#define KH_COMMAND_REG	0x004
#define KH_BASEADDR_H	0x012
#define KH_BASEADDR_L	0x010
#define KH_CLKEN		0x040
#define KH_GCLKEN		0x042
#define KH_PSWCLR		0x050
#define KH_VRAMTC		0x060
#define KH_VRAMRC		0x061
#define KH_VRAMAC		0x062
#define KH_VRAMBC		0x064
#define KH_VMREQMD		0x06F

#define KH_CMDADR_L		0x00A
#define KH_CMDADR_H		0x00C
#define KH_CMDFIF		0x00E
#define KH_CMDFINT		0x010
#define KH_BINTMSK		0x012
#define KH_BINTST		0x014
#define KH_FIPT			0x016
#define KH_DMAST		0x018
#define KH_COMD_L		0x01C
#define KH_COMD_H		0x01E
#define KH_FIFOR		0x022

#define	PXAIO_DMAST_DMA		0x0001
#define	PXAIO_DMAST_BLT		0x0002

#define kurohyo_LCDCREG_NUM   18

#define PLMOD              0x164
#define PLCST              0x102
#define VHLIN              0x008
#define PLDSA_H            0x124
#define PLDSA_L            0x122
#define PLPIT_H            0x12c
#define PLPIT_L            0x12a
#define PLHT               0x140
#define PLHDS              0x142
#define PLHSS              0x144
#define PLHSE              0x146
#define PLHBS              0x148
#define PLHBE              0x14a
#define PLHPX              0x14c
#define PLVT               0x150
#define PLVDS              0x152
#define PLVSS              0x154
#define PLVSE              0x156
#define PLVBS              0x158
#define PLVBE              0x15a
#define PLGMD              0x12e
#define PLCST              0x102
#define PLMOD              0x164
#define MISC               0x166
#define PLCNT              0x100
#define PIFEN              0x18e
#define PLCLN		   0x160


#define CLKCTL_USB_MASK    0x0002
#define CLKCTL_CONFIG_DTA  0x1310

#define SCOOP_CF_MODE		0x0100
#define SCOOP21_OUT_MASK	0x0004
#define SCOOP22_OUT_MASK	0x036e

#define SCOOP21_RESET_IN	0x0004
#define SCOOP22_SUSPEND		0x0020
#define SCOOP22_L3VON		0x0040


#define PLCNT_SOFT_RESET	0x0001
#define PLCNT_STOP_CKP		0x0004
#define PLCNT_DEFAULT		0x0010



#define FUNC_SLCDEN 0x0004
#define PLL10_QVGA 0xf203
#define PLL11_QVGA 0x00e7
#define PLL10_VGA 0xdf00
#define PLL11_VGA 0x002c

typedef struct
{
    unsigned short Index;
    unsigned short Value;
} TC6393_REGS;

static TC6393_REGS kurohyoLcdcQVGA[] =
{
	{ VHLIN,	0x01E0 },  // VHLIN
	{ PLDSA_H,	0x0000 },  // PLDSA display start address high
	{ PLDSA_L,	0x0000 },  // PLDSA display start address low
	{ PLPIT_H,	0x0000 },  // PLPIT horizontal pixel high
	{ PLPIT_L,	0x01E0 },  // PLPIT horizontal pixel low
	{ PLHT,		0x0145 },  // PLHT  horizontal total
	{ PLHDS,	0x0026 },  // PLHDS horizontal display start
	{ PLHSS,	0x0000 },  // PLHSS H-sync start
	{ PLHSE,	0x0002 },  // PLHSE H-sync end
	{ PLHPX,	0x00f0 },  // PLHPX Horizontal number of pixel
	{ PLVT,		0x014F },  // PLVT  Vertical total
	{ PLVDS,	0x0002 },  // PLVDS Vertical display start
	{ PLVSS,	0x0000 },  // PLVSS V-sync start
	{ PLVSE,	0x0001 },  // PLVSE V-sync end
	{ MISC,		0x0003 },  // MISC  RGB565 mode
	{ PLGMD,	0x0001 },  // PLGMD vram access enable
	{ PLCST,	0x4007 },  // PLCST
	{ PLMOD,	0x0003 },  // PLMOD Sync polarity
};

static TC6393_REGS kurohyoLcdcVGA[] =
{
	{ VHLIN,	0x03c0 },  // VHLIN
	{ PLDSA_H,	0x0000 },  // PLDSA display start address high
	{ PLDSA_L,	0x0000 },  // PLDSA display start address low
	{ PLPIT_H,	0x0000 },  // PLPIT horizontal pixel high
	{ PLPIT_L,	0x03c0 },  // PLPIT horizontal pixel low
	{ PLHT,		0x0289 },  // PLHT  horizontal total
	{ PLHDS,	0x004e },  // PLHDS horizontal display start
	{ PLHSS,	0x0000 },  // PLHSS H-sync start
	{ PLHSE,	0x0002 },  // PLHSE H-sync end
	{ PLHPX,	0x01e0 },  // PLHPX Horizontal number of pixel
	{ PLVT,		0x028f },  // PLVT  Vertical total
	{ PLVDS,	0x0002 },  // PLVDS Vertical display start
	{ PLVSS,	0x0000 },  // PLVSS V-sync start
	{ PLVSE,	0x0001 },  // PLVSE V-sync end
	{ MISC,		0x0003 },  // MISC  RGB565 mode
	{ PLGMD,	0x0001 },  // PLGMD vram access enable
	{ PLCST,	0x4007 },  // PLCST
	{ PLMOD,	0x0003 },  // PLMOD Sync polarity
};

#define TG_REG0_VQV   0x0001
#define TG_REG0_COLOR 0x0002
#define TG_REG0_UD    0x0004
#define TG_REG0_LR    0x0008

#define CK_SSP	(0x1u << 3)

/*
 * Accelerator COMMAND
 */

// Set Command
#define	PXAIO_COMDI_CSADR		0x00000000
#define	PXAIO_COMDD_CSADR(x)		(x&0x001ffffe)
#define	PXAIO_COMDI_CHPIX		0x01000000
#define	PXAIO_COMDD_CHPIX(x)		(x&0x000003ff)
#define	PXAIO_COMDI_CVPIX		0x02000000
#define	PXAIO_COMDD_CVPIX(x)		(x&0x000003ff)
#define	PXAIO_COMDI_PSADR		0x03000000
#define	PXAIO_COMDD_PSADR(x)		(x&0x00fffffe)
#define	PXAIO_COMDI_PHPIX		0x04000000
#define	PXAIO_COMDD_PHPIX(x)		(x&0x000003ff)
#define	PXAIO_COMDI_PVPIX		0x05000000
#define	PXAIO_COMDD_PVPIX(x)		(x&0x000003ff)
#define	PXAIO_COMDI_PHOFS		0x06000000
#define	PXAIO_COMDD_PHOFS(x)		(x&0x000003ff)
#define	PXAIO_COMDI_PVOFS		0x07000000
#define	PXAIO_COMDD_PVOFS(x)		(x&0x000003ff)
#define	PXAIO_COMDI_POADR		0x08000000
#define	PXAIO_COMDD_POADR(x)		(x&0x00fffffe)
#define	PXAIO_COMDI_RSTR		0x09000000
#define	PXAIO_COMDD_RSTR(x)		(x&0x000000ff)
#define	PXAIO_COMDI_TCLOR		0x0A000000
#define	PXAIO_COMDD_TCLOR(x)		(x&0x0000ffff)
#define	PXAIO_COMDI_FILL		0x0B000000
#define	PXAIO_COMDD_FILL(x)		(x&0x0000ffff)
#define	PXAIO_COMDI_DSADR		0x0C000000
#define	PXAIO_COMDD_DSADR(x)		(x&0x00fffffe)
#define	PXAIO_COMDI_SSADR		0x0D000000
#define	PXAIO_COMDD_SSADR(x)		(x&0x00fffffe)
#define	PXAIO_COMDI_DHPIX		0x0E000000
#define	PXAIO_COMDD_DHPIX(x)		(x&0x000003ff)
#define	PXAIO_COMDI_DVPIX		0x0F000000
#define	PXAIO_COMDD_DVPIX(x)		(x&0x000003ff)
#define	PXAIO_COMDI_SHPIX		0x10000000
#define	PXAIO_COMDD_SHPIX(x)		(x&0x000003ff)
#define	PXAIO_COMDI_SVPIX		0x11000000
#define	PXAIO_COMDD_SVPIX(x)		(x&0x000003ff)
#define	PXAIO_COMDI_LBINI		0x12000000
#define	PXAIO_COMDD_LBINI(x)		(x&0x0000ffff)
#define	PXAIO_COMDI_LBK2		0x13000000
#define	PXAIO_COMDD_LBK2(x)		(x&0x0000ffff)
#define	PXAIO_COMDI_SHBINI		0x14000000
#define	PXAIO_COMDD_SHBINI(x)		(x&0x0000ffff)
#define	PXAIO_COMDI_SHBK2		0x15000000
#define	PXAIO_COMDD_SHBK2(x)		(x&0x0000ffff)
#define	PXAIO_COMDI_SVBINI		0x16000000
#define	PXAIO_COMDD_SVBINI(x)		(x&0x0000ffff)
#define	PXAIO_COMDI_SVBK2		0x17000000
#define	PXAIO_COMDD_SVBK2(x)		(x&0x0000ffff)

// Action Command
#define	PXAIO_COMDI_CMGO		0x20000000
#define	PXAIO_COMDD_CMGO_CEND		0x00000001
#define	PXAIO_COMDD_CMGO_INT		0x00000002
#define	PXAIO_COMDD_CMGO_CMOD		0x00000010
#define	PXAIO_COMDD_CMGO_CDVRV		0x00000020
#define	PXAIO_COMDD_CMGO_CDHRV		0x00000040
#define	PXAIO_COMDD_CMGO_RUND		0x00008000
#define	PXAIO_COMDI_SCGO		0x21000000
#define	PXAIO_COMDD_SCGO_CEND		0x00000001
#define	PXAIO_COMDD_SCGO_INT		0x00000002
#define	PXAIO_COMDD_SCGO_ROP3		0x00000004
#define	PXAIO_COMDD_SCGO_TRNS		0x00000008
#define	PXAIO_COMDD_SCGO_DVRV		0x00000010
#define	PXAIO_COMDD_SCGO_DHRV		0x00000020
#define	PXAIO_COMDD_SCGO_SVRV		0x00000040
#define	PXAIO_COMDD_SCGO_SHRV		0x00000080
// Specifications Document Rev.18 Add
#define	PXAIO_COMDD_SCGO_DSTXY	0x00008000
#define	PXAIO_COMDI_SBGO		0x22000000
#define	PXAIO_COMDD_SBGO_CEND		0x00000001
#define	PXAIO_COMDD_SBGO_INT		0x00000002
#define	PXAIO_COMDD_SBGO_DVRV		0x00000010
#define	PXAIO_COMDD_SBGO_DHRV		0x00000020
#define	PXAIO_COMDD_SBGO_SVRV		0x00000040
#define	PXAIO_COMDD_SBGO_SHRV		0x00000080
#define	PXAIO_COMDD_SBGO_SBMD		0x00000100
#define	PXAIO_COMDI_FLGO		0x23000000
#define	PXAIO_COMDD_FLGO_CEND		0x00000001
#define	PXAIO_COMDD_FLGO_INT		0x00000002
#define	PXAIO_COMDD_FLGO_ROP3		0x00000004
#define	PXAIO_COMDI_LDGO		0x24000000
#define	PXAIO_COMDD_LDGO_CEND		0x00000001
#define	PXAIO_COMDD_LDGO_INT		0x00000002
#define	PXAIO_COMDD_LDGO_ROP3		0x00000004
#define	PXAIO_COMDD_LDGO_ENDPX	0x00000008
#define	PXAIO_COMDD_LDGO_LVRV		0x00000010
#define	PXAIO_COMDD_LDGO_LHRV		0x00000020
#define	PXAIO_COMDD_LDGO_LDMOD	0x00000040

#endif
