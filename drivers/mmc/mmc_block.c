/*
 *  linux/drivers/mmc/mmc_block.c 
 *  	 driver for the block device on the MMC card
 *
 *  Author:	Vladimir Shebordaev	
 *  Copyright:	MontaVista Software Inc.
 *
 *	$Id: mmc_block.c,v 0.3.1.16 2002/09/27 17:36:09 ted Exp ted $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/hdreg.h>
#include <linux/blkpg.h>
#include <asm/uaccess.h>

#include <mmc/types.h>
#include <mmc/mmc.h>

#include "types.h"
#include "mmc.h"
#include "error.h"

#define MAJOR_NR MMC_BLOCK_MAJOR
#define DEVICE_NAME "mmc_block"
#define DEVICE_REQUEST mmc_block_request
#define DEVICE_NR(device) (device)
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define DEVICE_NO_RANDOM
#include <linux/blk.h>
/* for old kernels... */
#ifndef QUEUE_EMPTY
#define QUEUE_EMPTY  (!CURRENT)
#endif
#if LINUX_VERSION_CODE < 0x20300
#define QUEUE_PLUGGED (blk_dev[MAJOR_NR].plug_tq.sync)
#else
#define QUEUE_PLUGGED (blk_dev[MAJOR_NR].request_queue.plugged)
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,14)
#define BLK_INC_USE_COUNT MOD_INC_USE_COUNT
#define BLK_DEC_USE_COUNT MOD_DEC_USE_COUNT
#else
#define BLK_INC_USE_COUNT do {} while(0)
#define BLK_DEC_USE_COUNT do {} while(0)
#endif

#define MMC_BLOCK_RAW_DEVICE( device ) ((device>>MMC_BLOCK_PARTNBITS)<<MMC_BLOCK_PARTNBITS)
#define MMC_BLOCK_MKDEV( host, slot ) \
	MKDEV( MMC_BLOCK_MAJOR, \
	(host<<MMC_MINOR_HOST_SHIFT) \
	| (slot<<MMC_BLOCK_PARTNBITS) )

typedef struct _mmc_block_device mmc_block_device_rec_t;
typedef struct _mmc_block_device *mmc_block_device_t;

struct _mmc_block_device {
	mmc_card_t card;
	int host;
	int slot;
	kdev_t rdev;
	int usage;
	semaphore_t sem;
};

static int mmc_block_blk_sizes[1<<MINORBITS];
static int mmc_block_blk_blksizes[1<<MINORBITS];
static int mmc_block_hardsect_sizes[1<<MINORBITS];
static struct hd_struct mmc_block_partitions[1<<MINORBITS];

/* Accessed under device table lock */
static gendisk_rec_t mmc_block_gendisk = {
	major:		MMC_BLOCK_MAJOR,
	major_name:	"mmc",
	minor_shift:	MMC_BLOCK_PARTNBITS,
	max_p:		(1<<MMC_BLOCK_PARTNBITS),
	sizes:		mmc_block_blk_sizes,
	part:		mmc_block_partitions
};

static mmc_block_device_rec_t mmc_block_device[1<<MINORBITS];
static rwsemaphore_t mmc_block_device_sem;

static inline void __mmc_block_rdlock_devices( void )
{
	down_read( &mmc_block_device_sem );
}

static inline void __mmc_block_rdunlock_devices( void )
{
	up_read( &mmc_block_device_sem );
}

static inline void __mmc_block_wrlock_devices( void )
{
	down_write( &mmc_block_device_sem );
}

static inline void __mmc_block_wrunlock_devices( void )
{
	up_write( &mmc_block_device_sem );
}

static inline void __mmc_block_lock_device( kdev_t rdev )
{
	__mmc_block_rdlock_devices();
	down( &mmc_block_device[MINOR( rdev )].sem );
}

static inline void __mmc_block_unlock_device( kdev_t rdev )
{
	up( &mmc_block_device[MINOR( rdev )].sem );
	__mmc_block_rdunlock_devices();
}

static inline void __mmc_block_device_init( int minor )
{
	mmc_block_device_t dev = &mmc_block_device[minor];
	
	dev->usage = 0;
	dev->card = NULL;
	dev->host = minor >> MMC_MINOR_HOST_SHIFT;
	dev->slot = (minor & MMC_MINOR_CARD_MASK)>>MMC_BLOCK_PARTNBITS;
	dev->rdev = MKDEV( MMC_BLOCK_MAJOR, minor );
}

static inline int __mmc_block_validate_device( kdev_t rdev )
{
	int ret = -1;
	int minor = MINOR( rdev );
	
	if ( mmc_block_device[minor].card 
			&& (mmc_block_gendisk.part[minor].nr_sects > 0) )
		ret = 0;

	return ret;
}

static inline int __mmc_block_invalidate_card( mmc_card_t card, int invalidate )
{
	int ret = 0;
	kdev_t start;
	int minor;

	__ENTER( "card = 0x%p", card );
	
	if ( card && card->ctrlr ) {
		register int i;
		
		start = MMC_BLOCK_MKDEV( card->ctrlr->slot, card->slot );
		minor = MINOR( start );

		__mmc_block_wrlock_devices();
		for ( i = mmc_block_gendisk.max_p - 1; i >= 0; --i ) {
			if ( invalidate )
				invalidate_device( start + i, 0 );
			
			__mmc_block_device_init( minor + i );
			
			mmc_block_gendisk.part[minor + i].nr_sects = 0;
			mmc_block_gendisk.part[minor + i].start_sect = 0;
		}
		__mmc_block_wrunlock_devices();
	}
	
	__LEAVE( "ret=%d", ret );
	return ret;
}

static inline int mmc_block_invalidate_card( int host, int slot, int invalidate )
{
	int ret = 0;
	kdev_t start;
	int minor;

	__ENTER( "host=%d slot=%d", host, slot );
	
	if ( (host >= 0) && (slot >= 0) ) {
		register int i;
		mmc_card_t card = NULL;
		
		start = MMC_BLOCK_MKDEV( host, slot );
		minor = MINOR( start );

		__mmc_block_wrlock_devices();
		for ( i = mmc_block_gendisk.max_p - 1; i >= 0; --i ) {
			if ( !card ) 
				card = mmc_block_device[minor + i].card;
			
			if ( invalidate )
				invalidate_device( start + i, 0 );
			
			__mmc_block_device_init( minor + i );
			
			mmc_block_gendisk.part[minor + i].nr_sects = 0;
			mmc_block_gendisk.part[minor + i].start_sect = 0;
		}
		if ( card )
			mmc_put_card( card );
		__mmc_block_wrunlock_devices();
	}
	
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* Get device reference locked for writing */
static inline mmc_block_device_t __mmc_block_get_device( kdev_t rdev )
{
	mmc_block_device_t ret = NULL;
	u8 minor = MINOR( rdev );
	int host_no, card_no;

	__ENTER( "rdev=%x:%x", MAJOR( rdev ), MINOR( rdev ) );

	host_no = minor >> MMC_MINOR_HOST_SHIFT;
	if ( host_no >= MMC_CONTROLLERS_MAX )
		goto error;

	card_no = (minor & MMC_MINOR_CARD_MASK)>>MMC_BLOCK_PARTNBITS;
	if ( card_no >= MMC_CARDS_MAX )
		goto error;
	
	__mmc_block_lock_device( rdev );
	if ( __mmc_block_validate_device( rdev ) ) {
		__mmc_block_unlock_device( rdev );	
		goto error;
	}

	ret = &mmc_block_device[minor];
	MMC_DEBUG( MMC_DEBUG_LEVEL2, "(%x:%x) card=%p, dusage=%d\n",
			MAJOR( ret->rdev ), MINOR( ret->rdev ), 
			ret->card, ret->usage );
error:
	__LEAVE( "ret=0x%p", ret );
	return ret;
}

/* Unlocks the device */
static inline void __mmc_block_put_device( mmc_block_device_t dev )
{
	__ENTER0();
	
	if ( dev ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL2, "(%x:%x) card=%p, dusage=%d\n",
				MAJOR( dev->rdev ), MINOR( dev->rdev ), 
				dev->card, dev->usage );
		__mmc_block_unlock_device( dev->rdev );
	}
	
	__LEAVE0();
}

/* Atomically increases use count of the valid device */
static inline mmc_block_device_t mmc_block_get_device( kdev_t rdev )
{
	mmc_block_device_t ret = NULL;
	
	__ENTER0();
	
	ret = __mmc_block_get_device( rdev );
	if ( !ret )
		goto error;
	
	ret->usage++;
	__mmc_block_put_device( ret );
error:
	__LEAVE( "ret=0x%p dusage=%d card=0x%p cusage=%d",
			ret, ret ? ret->usage : -1, 
			ret ? ret->card : NULL, 
			ret ? (ret->card ? ret->card->usage : -1) : -1 );
	return ret;
}

/* Check is there references to the card */
static inline int __mmc_block_check_card( kdev_t rdev ) 
{
	int ret = TRUE;
	int start = MINOR( MMC_BLOCK_RAW_DEVICE( rdev ) );
	register int i;

	for ( i = 0; i < mmc_block_gendisk.max_p; i++ )
		if ( mmc_block_device[start + i].usage > 0 ) {
			ret = FALSE;
			break;
		}

	return ret;
}

/* Atomically decreases device use count */
static inline void mmc_block_put_device( mmc_block_device_t dev )
{
	__ENTER0();
	
	if ( dev ) {
		int invalidate = FALSE;
		
		__mmc_block_get_device( dev->rdev );
		if ( dev->usage > 0 )
			--dev->usage;
		
		if ( dev->usage ) {
			__mmc_block_put_device( dev );
			goto out;

		} else {
			int host, slot;
			mmc_card_t card = NULL;
			
			invalidate = __mmc_block_check_card( dev->rdev );	
			if ( invalidate ) {
				host = dev->card->ctrlr->slot;
				slot = dev->card->slot;

				if ( dev->card ) {
					card = dev->card;
					mmc_put_card( dev->card );
					dev->card = NULL;
				}
			}
			__mmc_block_put_device( dev );

			if ( invalidate )
				__mmc_block_invalidate_card( card, TRUE );
		}
		
	}
out:
	__LEAVE0();
}

static int mmc_block_open( struct inode *inode, struct file *file )
{
	int ret = -ENODEV;
	mmc_block_device_t dev = NULL;
	
	__ENTER0();

	if ( !inode || !file )
		goto error;
	
	BLK_INC_USE_COUNT;
	
	check_disk_change( inode->i_rdev );
	
	dev = mmc_block_get_device( inode->i_rdev );
	if ( !dev )
		goto error;
	
	dev = __mmc_block_get_device( inode->i_rdev );
	if ( !dev )
		goto error;
	
	if ( file->f_mode & FMODE_WRITE ) { /* FIXME */
		if ( dev->usage > 1 ) {
			ret = -EBUSY;
			__mmc_block_put_device( dev );
			mmc_block_put_device( dev );
			goto error;
		}
	}
	
	__mmc_block_put_device( dev );
	
	if ( file )
		file->private_data = dev;

	ret = 0;
	goto out;
error:
	BLK_DEC_USE_COUNT;
out:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static int mmc_block_release( struct inode *inode, struct file *file )
{
	int ret = -EINVAL;
	mmc_block_device_t dev = NULL;
	
	__ENTER( "inode=0x%p file=0x%p rdev=(%x:%x)", inode, file,
			inode ? MAJOR( inode->i_rdev ) : 0xff,
			inode ? MINOR( inode->i_rdev ) : 0xff );

	if ( !file && !inode )
		goto error;
	
	if ( file )
		dev = file->private_data;
	else
		dev = __mmc_block_get_device( inode->i_rdev );

	if ( !dev ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "invalid device\n" );
		goto error;
	}
	
	if ( file ) {
		mmc_block_put_device( dev );
		file->private_data = NULL;

	} else {
		int invalidate = FALSE;
		
		if ( dev->usage > 0 )
			--dev->usage;
		
		if ( dev->usage ) {
			__mmc_block_put_device( dev );
			goto out;
			
		} else {
			int host, slot;
			mmc_card_t card = NULL;
			
			invalidate = __mmc_block_check_card( dev->rdev );	
			if ( invalidate ) {
				host = dev->card->ctrlr->slot;
				slot = dev->card->slot;

				if ( dev->card ) {
					card = dev->card;
					mmc_put_card( dev->card );
					dev->card = NULL;
				}
			}
			__mmc_block_put_device( dev );

			if ( invalidate )
				__mmc_block_invalidate_card( card, TRUE );
	
		}
	}
	
out:
	BLK_DEC_USE_COUNT;
	ret = 0;
error:
	__LEAVE0();
	return ret;
}  

static int mmc_block_check_disk_change( kdev_t rdev )
{
	int ret = 0;
#if 0
	mmc_block_device_t dev = &mmc_block_device[MINOR( rdev )];
	
	__mmc_block_lock_device( rdev );
	if ( !dev->card )
		ret = 1;
	__mmc_block_unlock_device( rdev );
#else
	ret = 1;
#endif
	return ret;
}

static int mmc_block_revalidate( kdev_t rdev )
{
	int ret = 1;
	mmc_card_t card;
	mmc_block_device_t dev;
	kdev_t start = MMC_BLOCK_RAW_DEVICE( rdev );
	int minor = MINOR( start );
	int host, slot;
	int i;
	
	__ENTER0();

	(void)mmc_update_card_stack( MINOR( start )>>MMC_MINOR_HOST_SHIFT );

	__mmc_block_wrlock_devices();

	dev = &mmc_block_device[minor];
	host = dev->host;
	slot = dev->slot;

	if ( dev->card ) { /* card has not been changed actually */
		__mmc_block_wrunlock_devices();
		goto out;
		
	} else {
		card = mmc_get_card( host, slot );
		if ( !card ) {
			MMC_DEBUG( MMC_DEBUG_LEVEL2, "failed to get card: "
					"host=%d, slot=%d\n", host, slot );
			__mmc_block_wrunlock_devices();
			goto error;
		}
		dev->card = card;
	}
	__mmc_block_wrunlock_devices();
 	/* FIXME */
	__mmc_block_rdlock_devices(); /* handle the request for sector 0 */
	grok_partitions( &mmc_block_gendisk, MINOR( start ),
			 mmc_block_gendisk.max_p,
			 card->info.capacity>>9 /* sectors */
		       );
	__mmc_block_rdunlock_devices();
	/* FIXME */
	__mmc_block_wrlock_devices();
	for ( i = start + mmc_block_gendisk.max_p - 1; i >= 0; --i ) {
		int minor = MINOR( i );
		
		dev = &mmc_block_device[minor];
		if ( mmc_block_gendisk.part[minor].nr_sects > 0 )
			dev->card = card;
	}
	__mmc_block_wrunlock_devices();
out:
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static void mmc_block_handle_request( void )
{
	struct request *request;
	mmc_block_device_t dev;
	mmc_card_t card;
	char *buf;
	loff_t pos;
	unsigned int result = 0;

	for (;;) {
		int minor;
		
		INIT_REQUEST;
		request = CURRENT;
		spin_unlock_irq( &io_request_lock );
		
		minor = MINOR( request->rq_dev );
		dev = __mmc_block_get_device( request->rq_dev );
		if ( !dev ) {
			MMC_DEBUG( MMC_DEBUG_LEVEL2, "invalid device (%x:%x)\n",
					MAJOR( request->rq_dev ), minor );
				
			goto end_req;
		}
		
		card = dev->card;
		(void)__mmc_block_put_device( dev );
		
		MMC_DEBUG( MMC_DEBUG_LEVEL2, 
//		printk( KERN_INFO __FUNCTION__"(): "
				"request %p: cmd %i sec %li (nr. %li)\n", 
				CURRENT, CURRENT->cmd, CURRENT->sector, 
				CURRENT->current_nr_sectors );
		
		if ( request->current_nr_sectors >
				mmc_block_gendisk.part[minor].nr_sects )
			goto end_req;
		
		// Handle the request
		// TODO: handle clusterred requests in multiple block transfer mode 
		buf = request->buffer;
		pos = (mmc_block_gendisk.part[minor].start_sect +
			request->sector) * MMC_BLOCK_SECT_SIZE;
		
		switch ( request->cmd )
		{
			int i, ret;
			
			case READ:
#if 0
				ret = mmc_read( card, 
					(request->current_nr_sectors > 1) ?
					MMC_TRANSFER_MODE_BLOCK_MULTIPLE :
					MMC_TRANSFER_MODE_BLOCK_SINGLE,
					buf, 
					request->current_nr_sectors 
					     * MMC_BLOCK_SECT_SIZE, /* FIXME */
					&pos );
				if ( ret < 0 ) 
					goto end_req;
				
#else
				for ( i = 0;
				      i < request->current_nr_sectors;
				      i++ ) {
					ret = mmc_read( card,
						MMC_TRANSFER_MODE_BLOCK_SINGLE,
						buf,
						MMC_BLOCK_SECT_SIZE, /* FIXME */
						&pos );
					if ( ret < 0 )
						goto end_req;
					else
						buf += ret;
				}
#endif
				result = 1;
				break;

			case WRITE:
			// TODO: Read only device
#if 0
				ret = mmc_write( card, 
					(request->current_nr_sectors > 1) ?
					MMC_TRANSFER_MODE_BLOCK_MULTIPLE :
					MMC_TRANSFER_MODE_BLOCK_SINGLE,
					buf, 
					request->current_nr_sectors 
					    * MMC_BLOCK_SECT_SIZE, /* FIXME */
					&pos );
				if ( ret < 0 ) 
					goto end_req;
				
#else
				for ( i = 0;
				      i < request->current_nr_sectors;
				      i++ ) {
					ret = mmc_write( card,
						MMC_TRANSFER_MODE_BLOCK_SINGLE,
						buf,
						MMC_BLOCK_SECT_SIZE, /* FIXME */
						&pos ); 
					if ( ret < 0 )
						goto end_req;
					else
						buf += ret;
				}
#endif
				result = 1;
				break;
		}

end_req:
		__LEAVE( "result=%d", result );
		spin_lock_irq( &io_request_lock );
		end_request( result );
	}
}

static volatile int leaving = 0;
static DECLARE_MUTEX_LOCKED( thread_sem );
static DECLARE_WAIT_QUEUE_HEAD( thr_wq );
static pid_t thr_id = -1;

int mmc_block_thread( void *arg )
{
	struct task_struct *task = current;
	DECLARE_WAITQUEUE(wait, task);

	__ENTER0();
	
	task->session = 1;
	task->pgrp = 1;
	task->flags |= PF_MEMALLOC;
	strcpy( task->comm, "mmcblockd" );
	task->tty = NULL;
	spin_lock_irq( &task->sigmask_lock );
	sigfillset( &task->blocked );
	recalc_sigpending( task );
	spin_unlock_irq( &task->sigmask_lock );
	exit_mm( task );
	exit_files( task );
	exit_sighand( task );
	exit_fs( task );

	while ( !leaving ) {
		add_wait_queue( &thr_wq, &wait);
		set_current_state( TASK_INTERRUPTIBLE );
		spin_lock_irq( &io_request_lock );
		if ( QUEUE_EMPTY || QUEUE_PLUGGED ) {
			spin_unlock_irq( &io_request_lock );
			schedule();
			remove_wait_queue( &thr_wq, &wait ); 
		} else {
			remove_wait_queue( &thr_wq, &wait ); 
			set_current_state( TASK_RUNNING );
			mmc_block_handle_request(); /* handle the request */
			spin_unlock_irq( &io_request_lock );
		}
	}

	up( &thread_sem );
	
	__LEAVE0();
	return 0;
}

#if LINUX_VERSION_CODE < 0x20300
#define RQFUNC_ARG void
#else
#define RQFUNC_ARG request_queue_t *q
#endif

static void mmc_block_request( RQFUNC_ARG )
{
	wake_up( &thr_wq );
}

static int mmc_block_ioctl( struct inode * inode, struct file * file,
		      unsigned int cmd, unsigned long arg )
{
	int ret = -ENODEV;
	mmc_block_device_t dev;
	mmc_card_t card;
	int minor;
	__ENTER0();
	
	if ( !inode || !file ) {
		ret = -EINVAL;
		goto error;
	}
	minor = MINOR( inode->i_rdev );
	
	dev = __mmc_block_get_device( inode->i_rdev );
	if ( !dev ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "invalid device\n" );
		goto error;
	}
	
	card = dev->card; 
	__mmc_block_put_device( dev );
	
	switch ( cmd ) {
	case BLKGETSIZE:   /* Return device size */
		{
			unsigned long value;
			
			__mmc_block_rdlock_devices();
			value = mmc_block_gendisk.part[minor].nr_sects;
			__mmc_block_rdunlock_devices();
			
			if ( put_user( value, (unsigned long *) arg) ) {
				ret = -EFAULT;
				goto error;
			}
		}
		break;

#ifdef BLKGETSIZE64
	case BLKGETSIZE64:
		{
			unsigned long value;
			
			__mmc_block_rdlock_devices();
			value = mmc_block_gendisk.part[minor].nr_sects;
			__mmc_block_rdunlock_devices();
			
			if ( put_user( (u64)value, (u64 *) arg) ) {
				ret = -EFAULT;
				goto error;
			}
		}
		break;
#endif

	case HDIO_GETGEO:
		{
			struct hd_geometry geo;
			
			ret = !access_ok( VERIFY_WRITE, arg, sizeof( geo ) );
			if ( ret ) {
				ret = -EFAULT;
				goto error;
			}

			geo.heads = 1;
			geo.sectors = 1;

			__mmc_block_rdlock_devices();
			geo.cylinders = mmc_block_gendisk.part[minor].nr_sects;
			geo.start = mmc_block_gendisk.part[minor].start_sect;
			__mmc_block_rdunlock_devices();

			if ( copy_to_user( (int *)arg, &geo, sizeof( geo ) ) ) {
				ret = -EFAULT;
				goto error;
			}
		}
		break;
		
	case BLKRRPART:
		if ( !capable( CAP_SYS_ADMIN ) ) {
			ret = -EACCES;
			goto error;
		}
		(void)mmc_block_revalidate( inode->i_rdev );
		break;
		
	default:
		ret = blk_ioctl( inode->i_rdev, cmd, arg );
		goto out;
	}

	ret = 0;
error:	
out:
	__LEAVE( "ret=%d", ret );
	return ret;
}

#if LINUX_VERSION_CODE < 0x20326
static struct file_operations mmc_block_fops =
{
	open: 			mmc_block_open,
	ioctl:			mmc_block_ioctl,
	release:		mmc_block_release,
	check_media_change:	mmc_block_check_disk_change,
	revalidate:		mmc_block_revalidate,
	read:			block_read,
	write:			block_write
};
#else
static struct block_device_operations mmc_block_fops = 
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,14)
	owner:			THIS_MODULE,
#endif
	open:			mmc_block_open,
	release:		mmc_block_release,
	ioctl:			mmc_block_ioctl,
	check_media_change:	mmc_block_check_disk_change,
	revalidate:		mmc_block_revalidate
};
#endif

#if 0
static int mmc_block_notify_add( mmc_card_t card )
{
	int ret = -1;
	mmc_block_device_t dev;
	kdev_t start;
	int minor;

	__ENTER0();
	
	if ( !card || !card->ctrlr ) 
		goto error;
	
	start = MMC_BLOCK_MKDEV( card->ctrlr->slot, card->slot );
	dev = &mmc_block_device[MINOR( start )];
		
	__mmc_block_wrlock_devices();
	if ( !dev->card ) {
		dev->card = card;
		ret = 0;
	}
	__mmc_block_wrunlock_devices();

	if ( !ret ) {
		int i;

		/* allow to read partition table */
		__mmc_block_rdlock_devices(); 
		grok_partitions( &mmc_block_gendisk, MINOR( start ),
			 mmc_block_gendisk.max_p,
			 card->info.capacity>>9 /* sectors */
		);
		__mmc_block_rdunlock_devices();
		
		__mmc_block_wrlock_devices();
		for ( i = start + mmc_block_gendisk.max_p - 1; i >= 0; --i ) {
			minor = MINOR( i );
			dev = &mmc_block_device[minor];
			if ( mmc_block_gendisk.part[minor].nr_sects > 0 )
				dev->card = card;
		}
		__mmc_block_wrunlock_devices();
	}
error:	
	__LEAVE( "ret=%d", ret );
	return ret;
}
#endif

static int mmc_block_notify_remove( mmc_card_t card )
{
	int ret = -1;
	
	__ENTER( "card=0x%p", card );
	
	if ( card && card->ctrlr )
		ret = __mmc_block_invalidate_card( card, FALSE );
	
	__LEAVE( "ret=%d", ret );
	return ret;
}

static mmc_notifier_rec_t mmc_block_notifier = {
//	add: mmc_block_notify_add,
	remove: mmc_block_notify_remove
};

static int __init mmc_block_module_init( void )
{
	int ret = -ENODEV;
	int i;

	__ENTER0();
	
	init_rwsem( &mmc_block_device_sem );
	
	if ( register_blkdev( MAJOR_NR, DEVICE_NAME, &mmc_block_fops ) ) {
		MMC_ERROR( "Can't allocate major number %d for MMC block devices.\n", MMC_BLOCK_MAJOR );
		ret = -EAGAIN;
		goto error;
	}
	
	for ( i = 0; i < (1<<MINORBITS); i++ ) {
		__mmc_block_device_init( i );
		init_MUTEX( &mmc_block_device[i].sem );

		/* We fill it in at open() time. */
		mmc_block_blk_sizes[i] = 0;
		mmc_block_blk_blksizes[i] = BLOCK_SIZE;
		mmc_block_hardsect_sizes[i] = 0;
	}
	
	init_waitqueue_head( &thr_wq );
	/* Allow the block size to default to BLOCK_SIZE. */
	blksize_size[MAJOR_NR] = mmc_block_blk_blksizes;
	hardsect_size[MAJOR_NR] = mmc_block_hardsect_sizes;
	/* Gendisk stuff */
	memset( mmc_block_partitions, 0, sizeof( mmc_block_partitions ) );
	add_gendisk( &mmc_block_gendisk );

/* FIXME: per controller request queue, I/O and card stack update threads */	
	blk_init_queue( BLK_DEFAULT_QUEUE( MAJOR_NR ), &mmc_block_request );
	thr_id = kernel_thread( mmc_block_thread, NULL, 
			CLONE_FS|CLONE_FILES|CLONE_SIGHAND );

	if ( !mmc_register( MMC_REG_TYPE_USER, &mmc_block_notifier, 0 ) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "failed to register with MMC core\n" );
		goto error;
	}

	ret = 0;
	printk( KERN_INFO "MMC block device driver, $Revision: 0.3.1.16 $\n" );
	goto out;
error:
	if ( thr_id != -1 ) {
/* quit the thread */
		leaving = 1;
		wake_up(&thr_wq);
	
		down(&thread_sem);
	}
	blksize_size[MAJOR_NR] = NULL;
	blk_size[MAJOR_NR] = NULL;
	hardsect_size[MAJOR_NR] = NULL;
out:	
	__LEAVE0();
	return ret;
}

static void __exit mmc_block_module_cleanup( void )
{
/* quit the thread */
	leaving = 1;
	wake_up(&thr_wq);
	
	down(&thread_sem);
	
	mmc_unregister( MMC_REG_TYPE_USER, &mmc_block_notifier );
	del_gendisk( &mmc_block_gendisk );
	unregister_blkdev( MAJOR_NR, DEVICE_NAME );
	
	blk_cleanup_queue( BLK_DEFAULT_QUEUE( MAJOR_NR ) );
	blksize_size[MAJOR_NR] = NULL;
	blk_size[MAJOR_NR] = NULL;
	hardsect_size[MAJOR_NR] = NULL;
}

EXPORT_NO_SYMBOLS;

module_init( mmc_block_module_init );
module_exit( mmc_block_module_cleanup );


MODULE_LICENSE("GPL");
