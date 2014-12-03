/*
 *  linux/drivers/mmc/mmc.h
 *
 *  Author:	Vladimir Shebordaev	
 *  Copyright:	MontaVista Software Inc.
 *  
 *  $Id: mmc.h,v 0.3.1.8 2002/09/18 12:58:00 ted Exp ted $
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef __MMC_P_H__
#define __MMC_P_H__

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/slab.h>

#include <linux/spinlock.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <asm/semaphore.h>
#include <mmc/types.h>
#include <mmc/mmc.h>

#include "types.h"

#include "error.h"

#define MMC_CONTROLLERS_MAX (4)
#define MMC_CARDS_MAX (16)

/* test device */
#define MMC_TEST_MAJOR (240)
#define MMC_TEST_TRANSFER_MODE_DEFAULT MMC_TRANSFER_MODE_BLOCK_SINGLE

/* block device */
#define MMC_BLOCK_MAJOR (241) /* FIXME: MMC_MAJOR */
#define MMC_BLOCK_DEVICES_MAX (1<<MINORBITS) /* FIXME */
#define MMC_BLOCK_SECT_SIZE (512) /* FIXME */
#define MMC_BLOCK_PARTNBITS (3)

/* Device minor number encoding:
 *   [7:6] - host
 *   [5:3] - card slot number
 *   [2:0] - partition number 
 */
#define MMC_MINOR_HOST_SHIFT (6)
#define MMC_MINOR_CARD_MASK (0x07)

/*
 *  MMC controller abstraction
 */
enum _mmc_controller_state {
	MMC_CONTROLLER_ABSENT = 0,
	MMC_CONTROLLER_FOUND,
	MMC_CONTROLLER_INITIALIZED,
	MMC_CONTROLLER_UNPLUGGED
};

enum _mmc_dir {
	MMC_READ = 1,
	MMC_WRITE
};

enum _mmc_buftype {
	MMC_USER = 1,
	MMC_KERNEL
};

struct _mmc_data_transfer_req_rec {
	mmc_dir_t cmd; /* read or write operation requested */
	mmc_transfer_mode_t mode; /* requested data transfer mode */
	mmc_buftype_t type; /* whether supplied buffer resides in user or kernel space */
	char *buf; /* poiner to the caller's buffer */
	ssize_t cnt; /* number of bytes to transfer */
	loff_t addr; /* card address */
	ssize_t blksz; /* block size as for CSD[READ_BL_LEN] or CSD[WRITE_BL_LEN] */
	ssize_t nob; /* number of blocks to transfer */
};

struct _mmc_controller_tmpl_rec {
	struct module *owner;	/* driver module */
	char name[16];

	const ssize_t block_size_max; /* max acceptable block size */
	const ssize_t nob_max; /* max blocks per one data transfer */

	int (*probe)( mmc_controller_t ); /* hardware probe */
	int (*init)( mmc_controller_t ); /* initialize, e.g. request irq, DMA and allocate buffers */
	void (*remove)( mmc_controller_t ); /* free resources */
#if 0 /* CONFIG_HOTPLUG */
	void (*attach)( void ); /|* controller hotplug callbacks *|/
	void (*detach)( void );
#endif
#ifdef CONFIG_PM 
	int (*suspend)( mmc_controller_t ); /* power management callbacks */
	void (*resume)( mmc_controller_t );
#endif
	
/* MMC protocol macros, v3.4, p.120 */
	int (*init_card_stack)( mmc_controller_t );
	int (*update_acq)( mmc_controller_t ); /* update card stack management data */
	int (*single_card_acq)( mmc_controller_t );
	int (*check_card_stack)( mmc_controller_t );
	int (*setup_card)( mmc_controller_t, mmc_card_t );
	int (*stream_read)( mmc_controller_t, mmc_data_transfer_req_t );
	int (*read_block)( mmc_controller_t,  mmc_data_transfer_req_t ); 
	int (*read_mblock)( mmc_controller_t, mmc_data_transfer_req_t  );
	int (*stream_write)( mmc_controller_t, mmc_data_transfer_req_t  );
	int (*write_block)( mmc_controller_t, mmc_data_transfer_req_t );
	int (*write_mblock)( mmc_controller_t, mmc_data_transfer_req_t  );
/* TODO:
	int (*sg_io)( mmc_controller_t, sg_list_t );
*/
/* TODO: 
 *	1) erase group macros 
 *	   int (*erase_group)( mmc_controller_t, mmc_erase_group_info_t );
 * 	2) write protection macros;
 * 	   int (*set_write_prot)( mmc_controller_t, mmc_write_protection_info_t )
 * 	3) lock/password management macros;
 */
};

#ifndef MMC_CTRLR_BLKSZ_DEFAULT 
#define MMC_CTRLR_BLKSZ_DEFAULT (512)
#endif

#ifndef MMC_CTRLR_NOB_DEFAULT 
#define MMC_CTRLR_NOB_DEFAULT (1)
#endif

struct _mmc_card_rec {
/* public card interface */
	struct _mmc_card_info_rec info; /* see <linux/mmc/mmc.h> */

/* private kernel specific data */
	mmc_state_t state;  /* card's state as per last operation */
	mmc_card_t next;     /* link to the stack */
	mmc_controller_t ctrlr;		/* back reference to the controller */
	int usage;			/* reference count */
	int slot;			/* card's number for device reference */
/* TODO: async I/O queue */
#ifdef CONFIG_PROC_FS
	proc_dir_entry_t proc;
	char proc_name[16];
#endif
	unsigned long card_data[0]	/* card specific data */
	__attribute__((aligned (sizeof(unsigned long))));
};

struct _mmc_card_stack_rec {
	mmc_card_t first;	/* first card on the stack */
	mmc_card_t last;	/* last card on the stack */
	mmc_card_t selected;    /* currently selected card */
	int ncards;
};

struct _mmc_controller_rec {
	mmc_controller_state_t state; /* found, initialized, unplugged... */
	int usage;		     /* reference count */
	int slot;		     /* host's number for device reference */
	semaphore_t io_sem;	     /* I/O serialization */
	rwsemaphore_t update_sem;    /* card stack check/update serialization */

	mmc_controller_tmpl_t tmpl; /* methods provided by the driver */
	mmc_card_stack_rec_t stack; /* card stack management data */

	u32 rca_next;  /* next RCA to assign */
	int slot_next; /* next slot number to assign */
#ifdef CONFIG_PROC_FS
	char proc_name[16];
	proc_dir_entry_t proc;
#endif
	unsigned long host_data[0]  /* driver can request some extra space */
	__attribute__((aligned (sizeof(unsigned long))));
};

/*
 * MMC core interface
 */
enum _mmc_reg_type {
	MMC_REG_TYPE_USER = 1,
	MMC_REG_TYPE_HOST,
	MMC_REG_TYPE_CARD
};

struct _mmc_notifier_rec {
	struct _mmc_notifier_rec *next;
	mmc_notifier_fn_t add;
	mmc_notifier_fn_t remove;
};

enum _mmc_response {
	MMC_NORESPONSE = 1,
	MMC_R1,
	MMC_R2,
	MMC_R3,
	MMC_R4,
	MMC_R5
};

#undef EXTERN
#ifndef __MMC_CORE_IMPLEMENTATION__
#define EXTERN extern
#else
#define EXTERN /* empty */
#endif

EXTERN void *mmc_register( mmc_reg_type_t, void *, size_t ); 
EXTERN void mmc_unregister( mmc_reg_type_t, void * );
EXTERN int mmc_update_card_stack( int );

EXTERN mmc_card_t mmc_get_card( int, int );/* get reference to the card */
EXTERN void mmc_put_card( mmc_card_t );  /* release card reference */

EXTERN int mmc_notify_add( mmc_card_t ); /* user notification */
EXTERN int mmc_notify_remove( mmc_card_t );

EXTERN ssize_t mmc_read( mmc_card_t, mmc_transfer_mode_t, char *, size_t, loff_t * ); /* generic read */
EXTERN ssize_t mmc_write( mmc_card_t, mmc_transfer_mode_t, const char *, size_t, loff_t * ); /* generic write */
EXTERN int mmc_ioctl( mmc_card_t, unsigned int, unsigned long ); /* generic ioctl */ 
/*
 * TODO: [?m.b. ioctl()] to erase, lock and write protect
 *    1) mmc_erase
 *    2) mmc_write_prot
 *    3) mmc_lock
 */
#undef EXTERN

static inline mmc_card_t __mmc_card_alloc( size_t extra )
{
	mmc_card_t ret = kmalloc( sizeof( mmc_card_rec_t ) + extra, GFP_KERNEL );
	
	if ( ret ) {
		memset( ret, 0, sizeof( mmc_card_rec_t ) + extra );
	}
	
	return ret;
}

static inline void __mmc_card_free( mmc_card_t card )
{
	if ( card ) {
		kfree( card );
	}
}

static inline mmc_card_stack_t __mmc_card_stack_init( mmc_card_stack_t stack )
{
	mmc_card_stack_t ret = NULL;
	if ( stack ) {
		memset( stack, 0, sizeof( mmc_card_stack_rec_t ) );
		ret = stack;
	}
	return ret;
}

static inline mmc_card_stack_t __mmc_card_stack_add( mmc_card_stack_t stack, mmc_card_t card )
{
	mmc_card_stack_t ret = NULL;
	
	if ( stack && card ) {
		card->next = NULL;
		
		if ( stack->first ) {
			stack->last->next = card;
			stack->last = card;
		} else 
			stack->first = stack->last = card;
		
		++stack->ncards;
		ret = stack;
	}
	return ret;
}

static inline mmc_card_stack_t __mmc_card_stack_remove( mmc_card_stack_t stack, mmc_card_t card )
{
	mmc_card_stack_t ret = NULL;
	register mmc_card_t prev;
	int found = FALSE;

	if ( !stack || !card )
		goto error;
	
	if ( stack->ncards > 0 ) {
		if ( stack->first == card ) {
			stack->first = stack->first->next;
			if ( stack->last == card )
				stack->last = stack->last->next;
			found = TRUE;		
		} else {
			for ( prev = stack->first; prev; prev = prev->next )
				if ( prev->next == card ) {
					found = TRUE;
					break;
				}
			if ( found ) {
				if ( prev->next == stack->last )
					stack->last = prev->next;
				prev->next = prev->next->next;
			}
		}
		if ( found ) {
			--stack->ncards;
			ret = stack;
		}
	}
error:
	return ret;
}

static inline void __mmc_card_stack_free( mmc_card_stack_t stack )
{
	mmc_card_t card, next;
	
	if ( stack && (stack->ncards > 0) ) {
		card = stack->first;
		while ( card ) {
			next = card->next;
			kfree( card );
			card = next;
		}
		__mmc_card_stack_init( stack );
	}
}

static inline int __mmc_card_stack_foreach( mmc_card_stack_t stack, mmc_notifier_fn_t fn, int unplugged_also )
{
	int ret = 0;
	register mmc_card_t card = NULL;

	if ( stack && fn ) {
		for ( card = stack->first; card; card = card->next ) 
			if ( (card->state != MMC_CARD_STATE_UNPLUGGED) 
					|| unplugged_also )
				if ( fn( card ) ) {
					ret = -card->slot;
					break;
				}
	}
	
	return ret;	
}

/*
 * Debugging macros
 */
#ifdef CONFIG_MMC_DEBUG

#define MMC_DEBUG_LEVEL0 (0)     /* major */
#define MMC_DEBUG_LEVEL1 (1)
#define MMC_DEBUG_LEVEL2 (2)     /* device */
#define MMC_DEBUG_LEVEL3 (3)     /* protocol */
#define MMC_DEBUG_LEVEL4 (4)     /* everything */

#define MMC_DEBUG(n, args...) \
if (n <=  CONFIG_MMC_DEBUG_VERBOSE) { \
	printk(KERN_INFO __FUNCTION__ "(): " args); \
}
#define __ENTER0( )	MMC_DEBUG( MMC_DEBUG_LEVEL2, "entry\n" );
#define __LEAVE0( )	MMC_DEBUG( MMC_DEBUG_LEVEL2, "exit\n" );
#define __ENTER( format, args... )	MMC_DEBUG( MMC_DEBUG_LEVEL2, "entry: " format "\n", args );
#define __LEAVE( format, args... )	MMC_DEBUG( MMC_DEBUG_LEVEL2, "exit: " format "\n", args );

#define MMC_DUMP_CSD( card ) MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"CSD register:\n" \
"    csd_structure=%u\n" \
"    spec_vers=%u\n" \
"    taac=%x\n" \
"    nsac=%x\n" \
"    tran_speed=%x\n" \
"    ccc=%x\n" \
"    read_bl_len=%u\n" \
"    read_bl_partial=%u\n" \
"    write_blk_misalign=%u\n" \
"    read_blk_misalign=%u\n" \
"    dsr_imp=%u\n" \
"    c_size=%u\n" \
"    vdd_r_curr_min=%u\n" \
"    vdd_r_curr_max=%u\n" \
"    vdd_w_curr_min=%u\n" \
"    vdd_w_curr_max=%u\n" \
"    c_size_mult=%u\n" \
"    erase_grp_size=%u\n" \
"    erase_grp_mult=%u\n" \
"    wp_grp_size=%u\n" \
"    wp_grp_enable=%u\n" \
"    default_ecc=%u\n" \
"    r2w_factor=%u\n" \
"    write_bl_len=%u\n" \
"    write_bl_partial=%u\n" \
"    content_prot_app=%u\n" \
"    file_format_grp=%u\n" \
"    copy=%u\n" \
"    perm_write_protect=%d\n" \
"    tmp_write_protect=%d\n" \
"    file_format=%d\n" \
"    ecc=%d\n", \
card->info.csd.csd_structure, \
card->info.csd.spec_vers, \
card->info.csd.taac, \
card->info.csd.nsac, \
card->info.csd.tran_speed, \
card->info.csd.ccc, \
card->info.csd.read_bl_len, \
card->info.csd.read_bl_partial, \
card->info.csd.write_blk_misalign, \
card->info.csd.read_blk_misalign, \
card->info.csd.dsr_imp, \
card->info.csd.c_size, \
card->info.csd.vdd_r_curr_min, \
card->info.csd.vdd_r_curr_max, \
card->info.csd.vdd_w_curr_min, \
card->info.csd.vdd_w_curr_max, \
card->info.csd.c_size_mult, \
card->info.csd.erase_grp_size, \
card->info.csd.erase_grp_mult, \
card->info.csd.wp_grp_size, \
card->info.csd.wp_grp_enable, \
card->info.csd.default_ecc, \
card->info.csd.r2w_factor, \
card->info.csd.write_bl_len, \
card->info.csd.write_bl_partial, \
card->info.csd.content_prot_app, \
card->info.csd.file_format_grp, \
card->info.csd.copy, \
card->info.csd.perm_write_protect, \
card->info.csd.tmp_write_protect, \
card->info.csd.file_format, \
card->info.csd.ecc );

#else /* CONFIG_MMC_DEBUG */
#define MMC_DEBUG(n, args...)	/* empty */
#define __ENTER0( )	/* empty */
#define __LEAVE0( )	/* empty */
#define __ENTER( args... )	/* empty */
#define __LEAVE( args... )	/* empty */
#define MMC_DUMP_CSD( card ) /* empty */
#endif /* CONFIG_MMC_DEBUG */

/* 
 * Miscellaneous defines
 */
#ifndef MMC_DUMP_R1
#define MMC_DUMP_R1( ctrlr ) /* empty */
#endif
#ifndef MMC_DUMP_R2
#define MMC_DUMP_R2( ctrlr ) /* empty */
#endif
#ifndef MMC_DUMP_R3
#define MMC_DUMP_R3( ctrlr ) /* empty */
#endif

#endif /* __KERNEL__ */

#endif /* __MMC_P_H__ */
