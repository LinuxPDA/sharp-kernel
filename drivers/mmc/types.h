/*
 *  linux/drivers/mmc/types.h
 *
 *  Author:	Vladimir Shebordaev	
 *  Copyright:	MontaVista Software Inc.
 *
 *  $Id: types.h,v 0.5 2002/08/13 17:34:02 ted Exp ted $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef __MMC_TYPES_P_H__
#define __MMC_TYPES_P_H__

#ifdef __KERNEL__
#include <linux/kdev_t.h>

typedef enum _mmc_reg_type mmc_reg_type_t;
typedef enum _mmc_response mmc_response_fmt_t;

/* MMC card private description */
typedef struct _mmc_card_rec mmc_card_rec_t;
typedef struct _mmc_card_rec *mmc_card_t;
typedef enum _mmc_dir mmc_dir_t;
typedef enum _mmc_buftype mmc_buftype_t;

/* notifier declarations */
typedef struct _mmc_notifier_rec mmc_notifier_rec_t;
typedef struct _mmc_notifier_rec *mmc_notifier_t;

typedef int (*mmc_notifier_fn_t) ( mmc_card_t );

/* MMC card stack */
typedef struct _mmc_card_stack_rec mmc_card_stack_rec_t;
typedef struct _mmc_card_stack_rec *mmc_card_stack_t;

typedef struct _mmc_data_transfer_req_rec mmc_data_transfer_req_rec_t;
typedef struct _mmc_data_transfer_req_rec *mmc_data_transfer_req_t;

/* MMC controller */
typedef struct _mmc_controller_tmpl_rec mmc_controller_tmpl_rec_t;
typedef struct _mmc_controller_tmpl_rec *mmc_controller_tmpl_t;

typedef enum _mmc_controller_state mmc_controller_state_t;
typedef struct _mmc_controller_rec mmc_controller_rec_t;
typedef struct _mmc_controller_rec *mmc_controller_t;

/* various kernel types */
typedef struct semaphore semaphore_t;
typedef struct rw_semaphore rwsemaphore_t;
typedef struct proc_dir_entry proc_dir_entry_rec_t;
typedef struct proc_dir_entry *proc_dir_entry_t;
typedef struct gendisk gendisk_rec_t;
typedef struct gendisk *gendisk_t;
#endif /* __KERNEL__ */

#endif /* __MMC_TYPES_P_H__ */

