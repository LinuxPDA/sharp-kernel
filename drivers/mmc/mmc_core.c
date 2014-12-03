/*
 *  linux/drivers/mmc/mmc_core.c 
 *  	MultiMediaCard subsystem core implementation
 *
 *  Author:	Vladimir Shebordaev	
 *  Copyright:	MontaVista Software Inc.
 *
 *	$Id: mmc_core.c,v 0.3.1.14 2002/09/27 17:36:09 ted Exp ted $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>

#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include <mmc/types.h>
#include <mmc/mmc.h>
#include <mmc/ioctl.h>

#include "types.h"

#define __MMC_CORE_IMPLEMENTATION__
#include "mmc.h"

/* MMC controllers registered in the system */ 
static mmc_controller_t mmc_controller[MMC_CONTROLLERS_MAX];
static int mmc_ncontrollers = 0;
static rwsemaphore_t mmc_controller_sem; /* controller table lock */
#ifdef CONFIG_PM
static struct pm_dev *mmc_pm_dev = NULL;
#endif

/* users' notification list */
static mmc_notifier_t mmc_notifier = NULL;
static rwsemaphore_t mmc_notifier_sem; /* notifiers' list lock */ 
#ifdef CONFIG_PROC_FS
static proc_dir_entry_t mmc_proc_dir = NULL;
#endif

/************************************************
 * service function prototypes and declarations *
 ************************************************/
static inline int mmc_acquire_io( mmc_controller_t ctrlr, mmc_card_t card )
{
	int ret = -EIO;

	__ENTER0();
	
	if ( !card || !ctrlr ) {
		ret = -EINVAL;
		goto error;
	}
#ifdef CONFIG_HOTPLUG
/* TODO: account for controller removal */
#endif
	down( &ctrlr->io_sem );
#if 0	
	down_read( &ctrlr->update_sem ); /* FIXME */
	if ( card->state != MMC_CARD_STATE_UNPLUGGED )
		ret = 0;
	up_read( &ctrlr->update_sem );
	
	if ( ret )
		up( &ctrlr->io_sem );
#else
	ret = 0;
#endif

error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static inline void mmc_release_io( mmc_controller_t ctrlr, mmc_card_t card )
{
	__ENTER0();
#ifdef CONFIG_HOTPLUG
/* TODO: account for controller removal */
#endif
	if ( !card && !ctrlr ) { /* FIXME */
		MMC_DEBUG( MMC_DEBUG_LEVEL2, "bad card reference\n" );
		goto error;
	}
	up( &ctrlr->io_sem );	
error:
	__LEAVE0();
}

/* TODO: there should be a separate context to be awaken 
 * by the card intertion interrupt; called under ctrlr->update_sem
 * held down by now */
static int __mmc_update_card_stack( mmc_controller_t ctrlr )
{
	int ret = -1;
	mmc_card_t card, prev;
	
	__ENTER0();
	
	if ( !ctrlr || !ctrlr->tmpl )
		goto error;
	
	/* check unplugged cards first... */
	if ( (ret = ctrlr->tmpl->check_card_stack( ctrlr )) )
		goto error;
	
	/* unregister unplugged cards and free 'em immediately */
	if ( ctrlr->stack.ncards > 0 ) {
		prev = ctrlr->stack.first;
		/* process the stack tail first */
		if ( prev->next ) {
			card = prev->next; 
			while ( card ) {
				if ( card->state == MMC_CARD_STATE_UNPLUGGED ) {
					if ( ctrlr->stack.selected == card )
						ctrlr->stack.selected = NULL;
#ifdef CONFIG_PROC_FS
					if ( card->proc ) {
						remove_proc_entry( card->proc_name, ctrlr->proc );
						card->proc = NULL;
					}
#endif
					ctrlr->slot_next = card->slot; /* FIXME */
					prev->next = card->next;
					if ( ctrlr->stack.last == card )
						ctrlr->stack.last = prev;
					/* FIXME: controller use count */
					mmc_notify_remove( card );
					--ctrlr->stack.ncards;
					if ( (ctrlr->usage > 0) && ctrlr->tmpl->owner ) {
						--ctrlr->usage;
						MMC_DEBUG( MMC_DEBUG_LEVEL2,
							"'%s' use count "
							"decreased (%d)\n",
							ctrlr->tmpl->name,
							ctrlr->usage );
						__MOD_DEC_USE_COUNT( 
							ctrlr->tmpl->owner );
					}
					__mmc_card_free( card );
				
					card = prev->next;
				}
			}
		}
		/* then the head */		
		card = ctrlr->stack.first;
		if ( card && (card->state == MMC_CARD_STATE_UNPLUGGED) ) {
			if ( ctrlr->stack.selected == card )
				ctrlr->stack.selected = NULL;
#ifdef CONFIG_PROC_FS
			if ( card->proc ) {
				remove_proc_entry( card->proc_name, ctrlr->proc );
				card->proc = NULL;
			}
#endif
			ctrlr->slot_next = card->slot; /* FIXME */
			mmc_notify_remove( card ); /* FIXME: should unregister here */
			ctrlr->stack.first = card->next;
			if ( ctrlr->stack.last == card )
				ctrlr->stack.last = NULL;
			/* FIXME: controller use count */
			--ctrlr->stack.ncards;
			if ( (ctrlr->usage > 0) && ctrlr->tmpl->owner ) {
				--ctrlr->usage;
				MMC_DEBUG( MMC_DEBUG_LEVEL2, "'%s' use count "
					"decreased (%d)\n", ctrlr->tmpl->name,
					ctrlr->usage );
				__MOD_DEC_USE_COUNT( ctrlr->tmpl->owner );
			}
			__mmc_card_free( card );
		}
	}
	MMC_DEBUG( MMC_DEBUG_LEVEL2, "after stack check: ncards=%d"
			" first=0x%x last=0x%x\n", ctrlr->stack.ncards,
			ctrlr->stack.first, ctrlr->stack.last );
	/* ...then add newly inserted ones */
	if ( (ret = ctrlr->tmpl->update_acq( ctrlr )) )
		goto error;
	/* ret = 0; */
error:			
	__LEAVE( "ret=%d", ret );
	return ret;
}

/*
 * 	1) check error code returned by controller; it's up to
 * 	   controller to detect error conditions reported by the card
 * 	   and to abort data transfer requests properly (e.g. send
 * 	   CMD12(STOP_TRANSMISSION) to abort ADDRESS_ERROR multiple
 * 	   block transfers)
 * 	2) arrange for card stack update when necessary
 *	   (all pending i/o requests must be held pending,
 *	   update procedure must start immediately after 
 *	   error has been detected)
 */
static inline int __mmc_check_error( mmc_card_t card, int err )
{
	int ret = -EIO;
	mmc_controller_t ctrlr;
	
	__ENTER0();

	if ( !card || !card->ctrlr )
		goto error;
	
	ctrlr = card->ctrlr;
	
	if ( err < 0 ) {
		switch ( err ) {
		/* bus error occurred */
		case MMC_ERROR_CRC_WRITE_ERROR:
		case MMC_ERROR_CRC_READ_ERROR:
		case MMC_ERROR_RES_CRC_ERROR:
		case MMC_ERROR_READ_TIME_OUT:
		case MMC_ERROR_TIME_OUT_RESPONSE:
			down_write( &ctrlr->update_sem ); /* FIXME */
			if ( !__mmc_update_card_stack( ctrlr ) )
				ret = -ENXIO;
			up_write( &ctrlr->update_sem );
			break;
		default:
			/* generic I/O error */
		}
	} else
		ret = err;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static inline void __mmc_free_controller( mmc_controller_t ctrlr )
{
	if ( ctrlr ) {
		if ( ctrlr->stack.ncards > 0 )
			__mmc_card_stack_free( &ctrlr->stack );
		kfree( ctrlr );
	}
}

#ifdef CONFIG_PROC_FS
static int mmc_proc_read_card_info( char *page, char **start, off_t off, int count, int *eof, void *data )
{
	int ret = -EINVAL;
	mmc_card_t card = (mmc_card_t)data;
	char *cp = page;
	
	if ( !card )
		goto error;
	
	down_read( &card->ctrlr->update_sem );
/* TODO: proc report
 * Type: RO, RW or IO (by CCC)
 * MID: 0x%02x card->info.cid.mid
 * OID: 0x%04x card->info.cid.oid
 * PNM: %s card->info.pnm
 * PRV: %s card->info.prv
 * PSN: 0x%08x card->info.cid.psn
 * MDT: %s card->info.mdt
 * Capacity: card->info.capacity (Bytes)
 */
#if 1
	cp += sprintf( cp, "Capacity: %dKb.\n\n", (card->info.capacity>>10) );
#else /* TODO */
	cp += sprintf( cp, "Type    : %s\n", card->info.type );
	cp += sprintf( cp, "MID     : 0x%02x\n", card->info.cid.mid );
	cp += sprintf( cp, "OID     : 0x%04x\n", card->info.cid.oid );
	cp += sprintf( cp, "PNM     : %s\n", card->info.pnm );
	cp += sprintf( cp, "PRV     : %s\n", card->info.prv );
	cp += sprintf( cp, "PSN     : 0x%08x\n", card->info.cid.psn );
	cp += sprintf( cp, "MDT     : %s\n", card->info.mdt );
	cp += sprintf( cp, "Capacity: %dKB\n", 
			(card->info.capacity>>10) );
#endif
	up_read( &card->ctrlr->update_sem );
	
	ret = cp - page;
error:
	return ret;
}
#endif

/*************************************
 * MMC core interface implementation *
 *************************************/
int mmc_notify_add( mmc_card_t card )
{
	int ret = 0;
	mmc_notifier_t notifier;
	
	__ENTER0();
	if ( card ) {
		for ( notifier = mmc_notifier; notifier;
		      notifier = notifier->next )
			if ( notifier->add )
				if ( (ret = notifier->add( card )) )
					break;
	}
	__LEAVE( "ret=%d", ret );
	return ret;
}
EXPORT_SYMBOL( mmc_notify_add );

int mmc_notify_remove( mmc_card_t card )
{
	int ret = 0;
	mmc_notifier_t notifier;
	
	__ENTER0();
	if ( card ) {
		for ( notifier = mmc_notifier; notifier; 
		      notifier = notifier->next )
			if ( notifier->remove )
				if ( (ret = notifier->remove( card )) )
					break;
	}
	__LEAVE( "ret=%d", ret );
	return ret;
}
EXPORT_SYMBOL( mmc_notify_remove );

int mmc_update_card_stack( int host )
{
	int ret = -EINVAL;
	mmc_controller_t ctrlr;

	__ENTER0();
	
	if ( (host < 0) || (host >= MMC_CONTROLLERS_MAX) )
		goto error;
	
	down_read( &mmc_controller_sem );
	if ( (ctrlr = mmc_controller[host]) ) {
		down_write( &ctrlr->update_sem );
		(void)__mmc_update_card_stack( ctrlr );
		up_write( &ctrlr->update_sem );
	}
	up_read( &mmc_controller_sem );
	ret = 0;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}
EXPORT_SYMBOL( mmc_update_card_stack );

ssize_t mmc_read( mmc_card_t card, mmc_transfer_mode_t mode, char *buf, size_t size, loff_t *paddr )
{
	ssize_t ret = -EIO;
	mmc_controller_t ctrlr;	
	mmc_data_transfer_req_rec_t transfer; 
		
	if ( !paddr ) {
		ret = -EINVAL;
		goto error;
	}
	
	if ( !card ) {
		ret = -ENODEV;
		goto error;
	}

	__ENTER( "card=%p usage=%d mode=%d buf=%p size=%d addr=%x",
		card, card->usage, mode, buf, size, *paddr );
	
	ctrlr = card->ctrlr;
	if ( (ret = mmc_acquire_io( ctrlr, card )) )
		goto error;
	
	memset( &transfer, 0, sizeof( mmc_data_transfer_req_rec_t ) );
	transfer.cmd = MMC_READ;
	transfer.mode = mode;
	transfer.type = MMC_USER; /* FIXME: buffer cache */
	transfer.buf = buf;
	transfer.addr = *paddr;
	transfer.cnt = size;

/* max block size defined by CSD[read_bl_len] */
	transfer.blksz = card->info.read_bl_len;
	transfer.nob = size / transfer.blksz;
	if ( (size - (transfer.nob * transfer.blksz)) > 0 )
		transfer.nob++;

/* TODO: controller may restrict maximum block size; set block size
 * and number of blocks that their accumulated length fit to
 * CSD[READ_BL_LEN] not to bother with block misalignment in multiple
 * block transfers */
	ctrlr = card->ctrlr;
	if ( transfer.blksz > ctrlr->tmpl->block_size_max ) { 
		ret = -EINVAL; /* FIXME */
		goto error;
	}
		
    	if ( ctrlr->stack.selected != card ) {
		if ( (ret = ctrlr->tmpl->setup_card( ctrlr, card )) )
			goto err_mmc;		
		ctrlr->stack.selected = card;
	}
	
	switch( mode ) {
		case MMC_TRANSFER_MODE_STREAM:
			if ( !ctrlr->tmpl->stream_read ) {
				ret = -ENXIO;
				goto err_down;
			}
/* TODO: The max clock frequency for stream read operation is given by
   the following formula:
   	max speed = min ( TRAN_SPEED, 8*2^(READ_BL_LEN) - NSAC/TAAC )

   If the card is not able to sustain data transfer it will set the
   UNDERRUN error bit in the status register, abort the transmission
   and wait in the Data state for a stop command 
 */
			ret = ctrlr->tmpl->stream_read( ctrlr, &transfer );
			break;

		case MMC_TRANSFER_MODE_BLOCK_SINGLE:
			if ( !ctrlr->tmpl->read_block ) {
				ret = -ENXIO;
				goto err_down;
			}
/* TODO: buffer size and data alignment (v3.4, p.29): 
	if CSD[READ_BL_PARTIAL] is set, smaller blocks whose starting
	and ending address are entirely contained within one physical
	block (as defined by CSD[READ_BL_LEN]) may also be transmitted
 */
			transfer.type = MMC_KERNEL; /* FIXME */
			ret = ctrlr->tmpl->read_block( ctrlr, &transfer );
			break;
			
		case MMC_TRANSFER_MODE_BLOCK_MULTIPLE:
			if ( !ctrlr->tmpl->read_mblock ) {
				ret = -ENXIO;
				goto err_down;
			}
			
			if ( transfer.nob > ctrlr->tmpl->nob_max ) {
				ret = -EINVAL;
				goto error;
			}
/* TODO: buffer size and data alignment (v3.4, p.29): 
	if the host uses patrial blocks whose accumulated length is
	not block aligned and block misalignment is not allowed, the
	card should detect a block misalignment error condition at the
	beginning of the first misaligned block
 */
			transfer.type = MMC_KERNEL; /* FIXME */
			ret = ctrlr->tmpl->read_mblock( card->ctrlr, &transfer );
			break;
		
		default:
			MMC_DEBUG( MMC_DEBUG_LEVEL0, "request for unknown transfer type\n" );
			ret = -EINVAL;
	}
err_mmc:
	ret = __mmc_check_error( card, ret );
	if ( ret >= 0 ) {
		ret = size - transfer.cnt;
		*paddr += ret;
	}
err_down:
	mmc_release_io( ctrlr, card );
error:
	__LEAVE("ret=%d", ret);
	return ret;
}
EXPORT_SYMBOL( mmc_read );

ssize_t mmc_write( mmc_card_t card, mmc_transfer_mode_t mode, const char *buf, size_t size, loff_t *paddr )
{
	ssize_t ret = -ESPIPE;
	mmc_controller_t ctrlr;
	mmc_data_transfer_req_rec_t transfer; 
		
	if ( !paddr ) {
		ret = -EINVAL;
		goto error;
	}
	
	if ( !card ) {
		ret = -ENODEV;
		goto error;
	}

	__ENTER( "card=%p usage=%d mode=%d buf=%p size=%d addr=%llx",
		card, card->usage, mode, buf, size, *paddr );

	ctrlr = card->ctrlr;	
	if ( (ret = mmc_acquire_io( ctrlr, card )) )
		goto error;
	
	memset( &transfer, 0, sizeof( mmc_data_transfer_req_rec_t ) );
	transfer.cmd = MMC_WRITE;
	transfer.mode = mode;
	transfer.type = MMC_USER; /* FIXME: buffer cache */
	transfer.buf = (char *)buf;
	transfer.addr = *paddr;
	transfer.cnt = size;

/* max block size defined by CSD[write_bl_len] */
	transfer.blksz = card->info.write_bl_len;
	transfer.nob = size / transfer.blksz;
	if ( (size - (transfer.nob * transfer.blksz)) > 0 )
		transfer.nob++;

/* TODO: controller may restrict maximum block size; set block size
 * and number of blocks that their accumulated length fit to
 * CSD[WRITE_BL_LEN] not to bother with block misalignment in multiple
 * block transfers */
	ctrlr = card->ctrlr;
	if ( transfer.blksz > ctrlr->tmpl->block_size_max ) { 
		ret = -EINVAL; /* FIXME */
		goto error;
	}
		
	if ( ctrlr->stack.selected != card ) {
		if ( (ret = ctrlr->tmpl->setup_card( ctrlr, card )) )
			goto err_mmc;		
		ctrlr->stack.selected = card;
	}
	
	transfer.cmd = MMC_WRITE;
	transfer.mode = mode;
	transfer.type = MMC_USER;
	switch( mode ) {
		case MMC_TRANSFER_MODE_STREAM:
			if ( !ctrlr->tmpl->stream_write ) {
				ret = -ENXIO;
				goto err_down;
			}
			ret = ctrlr->tmpl->stream_write( ctrlr, &transfer );
			break;

		case MMC_TRANSFER_MODE_BLOCK_SINGLE:
			if ( !ctrlr->tmpl->write_block ) {
				ret = -ENXIO;
				goto err_down;
			}
			transfer.type = MMC_KERNEL; /* FIXME */
			ret = ctrlr->tmpl->write_block( ctrlr, &transfer );
			break;
			
		case MMC_TRANSFER_MODE_BLOCK_MULTIPLE:
			if ( !ctrlr->tmpl->write_mblock ) {
				ret = -ENXIO;
				goto err_down;
			}
			transfer.type = MMC_KERNEL; /* FIXME */
			ret = ctrlr->tmpl->write_mblock( card->ctrlr, &transfer );
			break;
		
		default:
			MMC_DEBUG( MMC_DEBUG_LEVEL0, "request for unknown transfer type\n" )
	}

err_mmc:
	ret = __mmc_check_error( card, ret ); /* FIXME */
	if ( ret >= 0 ) {
		ret = size - transfer.cnt;
		*paddr += ret;
	}
err_down:
	mmc_release_io( ctrlr, card );
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}
EXPORT_SYMBOL( mmc_write );

int mmc_ioctl( mmc_card_t card, unsigned int cmd, unsigned long arg )
{
	int ret = -EINVAL;
	mmc_controller_t ctrlr;
	
	if ( !card ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "bad card reference\n" )
		goto error;
	}
		
	ctrlr = card->ctrlr;
	if ( mmc_acquire_io( ctrlr, card ) ) {  
		ret = -ENXIO;
		goto error;
	}

	switch ( cmd ) {
		case IOCMMCGCARDESC:
			if ( copy_to_user( (void *)arg, &card->info, sizeof( mmc_card_info_rec_t ) ) )
				ret = -EFAULT;
			break;
/*	
 * 1. TODO: erase region
 * 2. TODO: set/unset write protection, lock/password 
 */
		default:
			ret = -ENOIOCTLCMD;
	}
	
	mmc_release_io( ctrlr, card );
error:
	return ret;
}
EXPORT_SYMBOL( mmc_ioctl );

/* 
 * registry stuff 
 */
mmc_card_t mmc_get_card( int host, int slot )
{
	mmc_card_t ret = NULL;
	mmc_controller_t ctrlr = NULL;
	int found;

	__ENTER( "host=%d, card=%d", host, slot );
	
	if ( ((host < 0) || (host >= MMC_CONTROLLERS_MAX)) 
	     && ((slot < 0) || (slot >= MMC_CARDS_MAX)) )
		goto error;	

 	down_read( &mmc_controller_sem );
	
	if ( (ctrlr = mmc_controller[host]) ) {
		down_write( &ctrlr->update_sem );
		if ( ctrlr->stack.ncards > 0 ) {
			ret = ctrlr->stack.first;
			found = FALSE;
			while ( ret ) {
				if ( (ret->slot == slot) && (ret->state != 
				     MMC_CARD_STATE_UNPLUGGED) ) {
					found = TRUE;
					break;
				}
				ret = ret->next;
			}

			if ( found ) {
				if ( ctrlr->tmpl->owner ) {
					++ctrlr->usage;
					MMC_DEBUG( MMC_DEBUG_LEVEL2, 
					    "'%s' use count increased (%d)\n", 
					    ctrlr->tmpl->name, ctrlr->usage );
					__MOD_INC_USE_COUNT( ctrlr->tmpl->owner );
				}
				++ret->usage;
			} else
				ret = NULL;
		}
		up_write( &ctrlr->update_sem );	
	}
	up_read( &mmc_controller_sem );
error:
	__LEAVE("ret=0x%p usage=%d", ret, ret ? ret->usage : -1 );
	return ret;
}
EXPORT_SYMBOL( mmc_get_card );

void mmc_put_card( mmc_card_t card )
{
	mmc_card_t tmp = NULL;
	mmc_controller_t ctrlr;
	int found;
	
	__ENTER( "card=0x%p", card );

	if ( !card )
	     	goto error;	

	ctrlr = card->ctrlr;
	
 	down_read( &mmc_controller_sem );
	if ( !ctrlr || (ctrlr != mmc_controller[ctrlr->slot]) ) {
		MMC_ERROR( "bad controller reference: ctrlr=0x%p\n", ctrlr );
		goto err_down;
	}
	
	down_write( &ctrlr->update_sem );
	if ( ctrlr->stack.ncards > 0 ) {
		tmp = ctrlr->stack.first;
		found = FALSE;
		while ( tmp ) {
			if ( tmp == card ) {
				found = TRUE;
				break;
			}
			tmp = tmp->next;
		}

		if ( found ) {
			if ( tmp->usage > 0 ) {
				--tmp->usage;
				MMC_DEBUG( MMC_DEBUG_LEVEL2, "usage=%d"
					"owner=0x%p\n", tmp->usage, 
					ctrlr->tmpl->owner );
				if ( !tmp->usage && (ctrlr->usage > 0) 
	  			     && ctrlr->tmpl->owner ) {
					--ctrlr->usage;
					MMC_DEBUG( MMC_DEBUG_LEVEL2, 
						"'%s' use count "
						"decreased (%d)\n", 
						ctrlr->tmpl->name, 
						ctrlr->usage );
					__MOD_DEC_USE_COUNT( 
							ctrlr->tmpl->owner );
				}
			}
		} else
			MMC_DEBUG( MMC_DEBUG_LEVEL0, "bad card reference\n" );
			
	}
	up_write( &ctrlr->update_sem );	
err_down:
 	up_read( &mmc_controller_sem );
error:
	__LEAVE( "found=%d", found );
	return;
}
EXPORT_SYMBOL( mmc_put_card );

static inline void *mmc_register_user( mmc_notifier_t notifier )
{
	mmc_notifier_t ret = NULL, last = mmc_notifier;
	
	MOD_INC_USE_COUNT;
	if ( notifier ) {
		down_write( &mmc_notifier_sem );
		
		notifier->next = NULL;
		if ( !last ) {
			mmc_notifier = notifier;
			ret = notifier;	
		} else {
			while ( last->next ) {
				if ( last == notifier ) {
					MOD_DEC_USE_COUNT;
					break;
				}
				last = last->next;
			}
			if ( last != notifier ) {
				last->next = notifier;
				ret = notifier;
			}
		}
		up_write( &mmc_notifier_sem );
	}
/* notify new user about the cards present in the system */
	if ( ret && ret->add ) {
		int i;
		
		down_read( &mmc_controller_sem );
		for ( i = 0; i < mmc_ncontrollers; i++ ) {
			mmc_controller_t ctrlr = mmc_controller[i];
			
			down_read( &ctrlr->update_sem ); /* FIXME */
			__mmc_card_stack_foreach( &ctrlr->stack, 
					ret->add, FALSE );
			up_read( &ctrlr->update_sem ); /* FIXME */
		}
		up_read( &mmc_controller_sem );
	}
/* error: */
	__LEAVE( "mmc_notifier=0x%p, mmc_notifier->next=0x%p",
			mmc_notifier, mmc_notifier ? mmc_notifier->next : NULL );
	return ret;
}

static inline mmc_controller_t mmc_register_controller( mmc_controller_tmpl_t tmpl, size_t extra )
{
	mmc_controller_t ret = NULL;
	int found;
	int i;
	
	MOD_INC_USE_COUNT;
	
	down_write( &mmc_controller_sem );
	
	if ( mmc_ncontrollers >= MMC_CONTROLLERS_MAX ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "there're too many controllers\n" );
		goto error;
	}
	
	found = FALSE;
	for ( i = 0; i < MMC_CONTROLLERS_MAX; i++ )
		if ( !mmc_controller[i] ) {
			found = TRUE;
			break;
		}
	
	if ( !found ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "there're no empty slots\n" );
		goto error;
	}

	if ( !tmpl->init ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "host template lacks 'init()'\n" );
		goto error;
	}
	
	if ( !tmpl->probe ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "host template lacks 'probe()'\n" );
		goto error;
	}
	
	if ( !tmpl->init_card_stack ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "host template lacks 'init_card_stack()'\n" );
		goto error;
	}
	
	if ( !tmpl->update_acq ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "host template lacks 'update_acq()'\n" );
		goto error;
	}
	
	if ( !tmpl->check_card_stack ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "host template lacks 'check_card_stack()'\n" );
		goto error;
	}
	
	if ( !tmpl->setup_card ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "host template lacks 'setup_card()'\n" );
		goto error;
	}
	
	ret = kmalloc( sizeof( mmc_controller_rec_t ) + extra, GFP_ATOMIC ); /* FIXME: ISA + GFP_DMA */
	if ( !ret ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "out of memory\n" );
		goto error;
	}

	memset( ret, 0, sizeof( mmc_controller_rec_t ) + extra );
	
	if ( (tmpl->probe( ret ) != 1) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "controller probe failed\n" );
		goto err_free;
	}
	
	if ( tmpl->init( ret ) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "controller initialization failure\n" );
		goto err_free;
	}
	
	ret->state = MMC_CONTROLLER_FOUND;
	ret->slot = i;
	ret->tmpl = tmpl;
	init_MUTEX( &ret->io_sem );
	init_rwsem( &ret->update_sem );
#ifdef CONFIG_PROC_FS
	if ( mmc_proc_dir ) {
		snprintf( ret->proc_name, sizeof( ret->proc_name ),
				"host%d", ret->slot );
		ret->proc = proc_mkdir( ret->proc_name, mmc_proc_dir );
	}
#endif
	
/* initialize card stack */
	if ( ret->tmpl->init_card_stack( ret ) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "card stack initialization failure\n" );
		if ( ret->tmpl->remove )
			ret->tmpl->remove( ret ); /* FIXME */
		goto err_free;
	}
	
	mmc_controller[ret->slot] = ret;
	++mmc_ncontrollers;

/* notify users */
	if ( ret->stack.ncards > 0 ) {
		down_read( &mmc_notifier_sem );
		if ( (i = __mmc_card_stack_foreach( &ret->stack, mmc_notify_add, FALSE ) ) < 0 )
			MMC_ERROR( "device add notification failed at slot %d\n", -i );
		up_read( &mmc_notifier_sem );
	}
	goto out;
	
err_free:
#ifdef CONFIG_PROC_FS
	if ( ret->proc )
		remove_proc_entry( ret->proc_name, mmc_proc_dir );
#endif
	kfree( ret );
error:
	ret = NULL;
	MOD_DEC_USE_COUNT;
out:	
	up_write( &mmc_controller_sem );
	return ret;
}

static inline mmc_card_t mmc_register_card( mmc_card_t card )
{
	mmc_card_t ret = NULL;
	mmc_controller_t ctrlr;
	
	if ( !card || !card->ctrlr )
		goto error;
	
	ctrlr = card->ctrlr;
#ifdef CONFIG_PROC_FS
	if ( ctrlr->proc ) {
		snprintf( card->proc_name, sizeof( card->proc_name ),
					"card%d", card->slot );
		card->proc = create_proc_read_entry( card->proc_name,
					0444, ctrlr->proc,
					mmc_proc_read_card_info, card );
	}
#endif
	mmc_notify_add( card );
error:
	return ret;
}

void *mmc_register( mmc_reg_type_t reg_type, void *tmpl, size_t extra )
{
	void *ret = NULL;

	switch ( reg_type ) {
		case MMC_REG_TYPE_CARD:
			ret = mmc_register_card( (mmc_card_t)tmpl );
			break;

		case MMC_REG_TYPE_USER:
			ret = mmc_register_user( (mmc_notifier_t)tmpl );
			break;

		case MMC_REG_TYPE_HOST:
			ret = mmc_register_controller( (mmc_controller_tmpl_t)tmpl, extra );
			break;
		
		default:
			MMC_DEBUG( MMC_DEBUG_LEVEL0, "register request for unknown type\n" );
	}
	
	return ret;
}
EXPORT_SYMBOL( mmc_register );

static inline void mmc_unregister_user( mmc_notifier_t notifier )
{
	mmc_notifier_t prev = mmc_notifier;
	int found = FALSE;
	
	if ( notifier ) {
		down_write( &mmc_notifier_sem );
		
		if ( mmc_notifier == notifier) {
			mmc_notifier = prev->next;
			found = TRUE;
			
		} else if ( mmc_notifier ) {
			while( prev ) {
				if ( prev->next == notifier ) {
					found = TRUE;
					prev->next = prev->next->next;
					break;
				}
				prev = prev->next;
			}
		}
		
		if ( found ) {
			if ( notifier->remove ) {
				int i;
		
				down_read( &mmc_controller_sem );
				for ( i = 0; i < mmc_ncontrollers; i++ ) {
					mmc_controller_t ctrlr = 
						mmc_controller[i];
	
					down_read( &ctrlr->update_sem );
					__mmc_card_stack_foreach( &ctrlr->stack, notifier->remove, FALSE );
					up_read( &ctrlr->update_sem );
				}
				up_read( &mmc_controller_sem );
			}
		}
		
		up_write( &mmc_notifier_sem );
	}
	
	MOD_DEC_USE_COUNT;
}

static inline void mmc_unregister_controller( mmc_controller_t ctrlr )
{
	if ( !ctrlr || (mmc_controller[ctrlr->slot] != ctrlr ) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "bad unregister request\n" );
		goto error;
	}

	down_write( &mmc_controller_sem );

/* notify users */
	if ( ctrlr->stack.ncards > 0 ) {
		int slot;
		
		down_read( &mmc_notifier_sem );
		if ( (slot = __mmc_card_stack_foreach( &ctrlr->stack, mmc_notify_remove, FALSE ) ) )
			MMC_ERROR( "device remove notification failed at slot %d\n", -slot );
		up_read( &mmc_notifier_sem );
	}

#ifdef CONFIG_PROC_FS
	if ( ctrlr->proc ) 
		remove_proc_entry( ctrlr->proc_name, mmc_proc_dir );
#endif

	if ( ctrlr->tmpl && ctrlr->tmpl->remove )
			ctrlr->tmpl->remove( ctrlr );

	mmc_controller[ctrlr->slot] = NULL;
	--mmc_ncontrollers;

	__mmc_free_controller( ctrlr ); 

	up_write( &mmc_controller_sem );
	MOD_DEC_USE_COUNT;
error:
	return;
}

void mmc_unregister( mmc_reg_type_t reg_type, void *tmpl )
{
	switch ( reg_type ) {
		case MMC_REG_TYPE_USER:
			mmc_unregister_user( (mmc_notifier_t)tmpl );
			break;

		case MMC_REG_TYPE_HOST:
			mmc_unregister_controller( (mmc_controller_t)tmpl );
			break;
		
		default:
			MMC_DEBUG( MMC_DEBUG_LEVEL0, "unregister request for unknown type\n" );
	}
}
EXPORT_SYMBOL( mmc_unregister );

#ifdef CONFIG_PM
/* power management support */
static int mmc_pm_callback( struct pm_dev *pmdev, pm_request_t pmreq, void *pmdata )
{
	int ret = -EINVAL;
	mmc_controller_t ctrlr;
	int i;
	
	__ENTER( "pmreq=%d", pmreq );
	
	down_read( &mmc_controller_sem );
	
	switch ( pmreq ) {
	case PM_SUSPEND:
		for ( ret = 0, i = 0; !ret && (i < mmc_ncontrollers); i++ ) {
			ctrlr = mmc_controller[i];
			if ( ctrlr->tmpl->suspend )
				ret = ctrlr->tmpl->suspend( ctrlr );
		}
		if ( !ret )
			break;
		
	case PM_RESUME:
		for ( i = mmc_ncontrollers - 1; i >= 0; i-- ) {
			ctrlr = mmc_controller[i];
			if ( ctrlr->tmpl->resume )
				ctrlr->tmpl->resume( ctrlr );
		}
		ret = 0;
		break;

	default:
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "unsupported PM request %d\n", 
					pmreq );
	}

	up_read( &mmc_controller_sem );
/* error: */	
	__LEAVE( "ret=%d", ret );
	return ret;
}
#endif

/* kernel module stuff */
static int __init mmc_core_module_init( void )
{
	int ret = -ENODEV;
	
	memset( &mmc_controller, 0, sizeof( mmc_controller ) );
	
	init_rwsem( &mmc_controller_sem );
	init_rwsem( &mmc_notifier_sem );
#ifdef CONFIG_PM
	if ( !(mmc_pm_dev = pm_register( PM_UNKNOWN_DEV, 0, mmc_pm_callback )) ) 		MMC_DEBUG( MMC_DEBUG_LEVEL0, "failed to register PM callback\n" );
#endif
#ifdef CONFIG_PROC_FS
	mmc_proc_dir = proc_mkdir( "mmc", NULL );
#endif
	ret = 0;
	printk( KERN_INFO "MMC subsystem, $Revision: 0.3.1.14 $\n" );	
/* error: */
	return ret;
}

static void __exit mmc_core_module_cleanup( void )
{
#ifdef CONFIG_PROC_FS
	if ( mmc_proc_dir )
		remove_proc_entry( "mmc", NULL );
#endif
#ifdef CONFIG_PM
	pm_unregister( mmc_pm_dev );
#endif
}

module_init( mmc_core_module_init );
module_exit( mmc_core_module_cleanup );

MODULE_LICENSE( "GPL" );

