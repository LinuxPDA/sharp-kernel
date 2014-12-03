/*
 *  linux/include/linux/mmc/mmc.h 
 *
 *  Author: Vladimir Shebordaev 
 *  Copyright:  MontaVista Software Inc.
 *
 *  $Id: mmc.h,v 0.2.1.2 2002/07/25 16:29:47 ted Exp ted $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef __MMC_H__
#define __MMC_H__

#include <linux/types.h>
#include <mmc/types.h>

/*
 * MMC card type
 */
enum _mmc_type {
    MMC_CARD_TYPE_RO = 1,
    MMC_CARD_TYPE_RW,
    MMC_CARD_TYPE_IO
};

/*
 * MMC card state
 */
enum _mmc_state {
    MMC_CARD_STATE_IDLE = 1,
    MMC_CARD_STATE_READY,
    MMC_CARD_STATE_IDENT,
    MMC_CARD_STATE_STNBY,
    MMC_CARD_STATE_TRAN,
    MMC_CARD_STATE_DATA,
    MMC_CARD_STATE_RCV,
    MMC_CARD_STATE_DIS,
    MMC_CARD_STATE_UNPLUGGED=0xff
};

/*
 * Data transfer mode
 */
enum _mmc_transfer_mode {
    MMC_TRANSFER_MODE_STREAM = 1,
    MMC_TRANSFER_MODE_BLOCK_SINGLE,
    MMC_TRANSFER_MODE_BLOCK_MULTIPLE,
    MMC_TRANSFER_MODE_UNDEFINED = -1
};

struct _mmc_card_csd_rec { /* CSD register contents */
/* FIXME: BYTE_ORDER */
	u8	ecc:2,
		file_format:2,
		tmp_write_protect:1,
		perm_write_protect:1,
		copy:1,
		file_format_grp:1;
	u64	content_prot_app:1,
		rsvd3:4,
		write_bl_partial:1,
		write_bl_len:4,
		r2w_factor:3,
		default_ecc:2,
		wp_grp_enable:1,
		wp_grp_size:5,
		erase_grp_mult:5,
		erase_grp_size:5,
		c_size_mult:3,
		vdd_w_curr_max:3,
		vdd_w_curr_min:3,
		vdd_r_curr_max:3,
		vdd_r_curr_min:3,
		c_size:12,
		rsvd2:2,
		dsr_imp:1,
		read_blk_misalign:1,
		write_blk_misalign:1,
		read_bl_partial:1;

	u16	read_bl_len:4,
		ccc:12;
	u8	tran_speed;
	u8	nsac;
	u8	taac;
	u8	rsvd1:2,
	  	spec_vers:4,
		csd_structure:2;
};

struct _mmc_card_cid_rec { /* CID register contents */
/* FIXME: BYTE_ORDER */
	u8	mdt_year:4,
		mdt_mon:4;
	u32	psn;
	u8	prv_minor:4,
		prv_major:4;
	u8	pnm[6];
	u16	oid;
	u8	mid;
};

/* 
 * Public card description
 */
struct _mmc_card_info_rec {
    mmc_type_t type;
    mmc_transfer_mode_t transfer_mode; /* current data transfer mode */
    __u16 rca;        /* card's RCA assigned during initialization */
    struct _mmc_card_csd_rec csd;
    struct _mmc_card_cid_rec cid;
    __u32 tran_speed; /* kbits */
    __u16 read_bl_len;
    __u16 write_bl_len;
    size_t capacity;    /* card's capacity in bytes */
};

/* 
 * Micsellaneous defines
 */
#ifndef SEEK_SET
#define SEEK_SET (0)
#endif

#ifndef SEEK_CUR
#define SEEK_CUR (1)
#endif

#ifndef SEEK_END
#define SEEK_END (2)
#endif

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#endif /* __MMC_H__ */
