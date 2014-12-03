/*
 *  linux/include/linux/mmc/types.h
 *
 *  Author:	Vladimir Shebordaev	
 *  Copyright:	MontaVista Software Inc.
 *
 *	$Id: types.h,v 0.2 2002/07/11 16:28:21 ted Exp ted $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef __MMC_TYPES_H__
#define __MMC_TYPES_H__

/* MMC card */
typedef enum _mmc_type mmc_type_t;
typedef enum _mmc_state mmc_state_t;
typedef enum _mmc_transfer_mode mmc_transfer_mode_t;

typedef struct _mmc_card_csd_rec mmc_card_csd_rec_t;
typedef struct _mmc_card_cid_rec mmc_card_cid_rec_t;

typedef struct _mmc_card_info_rec mmc_card_info_rec_t;
typedef struct _mmc_card_info_rec *mmc_card_info_t;

typedef enum _mmc_error mmc_error_t;

#endif /* __MMC_TYPES_H__ */
