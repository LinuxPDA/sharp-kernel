/*
 *  linux/drivers/mmc/mmc_test.c 
 *
 *  Author:	Vladimir Shebordaev	
 *  Copyright:	MontaVista Software Inc.
 *
 *	$Id: mmc_test.c,v 0.4 2002/08/01 12:26:40 ted Exp ted $
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>

#include <linux/fs.h>
#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include <asm/uaccess.h>

#include <mmc/types.h>
#include <mmc/mmc.h>
#include <mmc/ioctl.h>

#include "types.h"
#include "mmc.h"

typedef struct _mmc_test_device_rec mmc_test_device_rec_t;
typedef struct _mmc_test_device_rec *mmc_test_device_t;

struct _mmc_test_device_rec {
	mmc_card_t card;
	mmc_transfer_mode_t transfer_mode;
	int usage;
#ifdef CONFIG_DEVFS_FS
	devfs_handle_t devfs_handle;
#endif
};

/* MMC device table */
static mmc_test_device_rec_t mmc_test_device[MMC_CONTROLLERS_MAX][MMC_CARDS_MAX];
static DECLARE_MUTEX(mmc_test_device_mutex);

static inline mmc_test_device_t __mmc_test_get_device( kdev_t rdev )
{
	mmc_test_device_t ret = NULL;
	u8 minor = MINOR( rdev );
	int host_no, card_no;

	host_no = minor >> MMC_MINOR_HOST_SHIFT;
	if ( host_no >= MMC_CONTROLLERS_MAX )
		goto error;
	
	card_no = minor & MMC_MINOR_CARD_MASK;
	if ( card_no >= MMC_CARDS_MAX )
		goto error;
	
	ret = &mmc_test_device[host_no][card_no];
	if ( !ret->card ) {
		ret->card = mmc_get_card( host_no, card_no );
		if ( !ret->card ) {
			MMC_DEBUG( MMC_DEBUG_LEVEL0, "failed to get card: host=%d, card=%d\n", host_no, card_no );
			ret = NULL;
			goto error;
		}
		
	}
	++ret->usage;
	
error:
	return ret;
}

static inline void __mmc_test_put_device( mmc_test_device_t dev )
{
	mmc_put_card( dev->card );
	--dev->usage;
}

static inline mmc_test_device_t mmc_test_get_device( kdev_t kdev )
{
	mmc_test_device_t ret = NULL;
	
	down( &mmc_test_device_mutex );
	ret = __mmc_test_get_device( kdev );
	up( &mmc_test_device_mutex );

	return ret;
}

static inline void mmc_test_put_device( mmc_test_device_t dev )
{
	if ( dev ) {
		down( &mmc_test_device_mutex );
		__mmc_test_put_device( dev );
		if ( !dev->usage ) {
			if ( dev->card ) {
				if ( dev->card->usage ) {
					MMC_DEBUG( MMC_DEBUG_LEVEL0, 
						"broken card reference\n" );
				}
				memset( dev, 0, sizeof( mmc_test_device_rec_t ) );
			}
		}
		up( &mmc_test_device_mutex );
	}
}

static inline int mmc_test_set_transfer_mode( mmc_test_device_t dev, mmc_transfer_mode_t mode )
{
	int ret = -1;
	
	if ( dev ) {
		down( &mmc_test_device_mutex );
		dev->transfer_mode = mode;
		ret = 0;
		up( &mmc_test_device_mutex );
	}
	return ret;
}

static inline mmc_transfer_mode_t mmc_test_get_transfer_mode( mmc_test_device_t dev )
{
	mmc_transfer_mode_t ret = MMC_TRANSFER_MODE_UNDEFINED;
	
	if ( dev ) {
		down( &mmc_test_device_mutex );
		ret = dev->transfer_mode;
		up( &mmc_test_device_mutex );
	}
	return ret;
}

static int mmc_test_open( struct inode *inode, struct file *file )
{
	int ret = -ENODEV;
	mmc_test_device_t dev = NULL;
	
	MOD_INC_USE_COUNT;
	
	__ENTER0( );	
	dev = mmc_test_get_device( inode->i_rdev );
	if ( !dev || !dev->card ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "failed to acquire device\n" );
		goto error;
	}
	
	if ( dev->card->usage > 1 ) {
		ret = -EBUSY;
		goto error;
	}
	
	dev->transfer_mode = MMC_TEST_TRANSFER_MODE_DEFAULT; /* FIXME: should check card CCC */
	file->private_data = dev;
	
	__LEAVE0( );
	return 0;
error:
	MOD_DEC_USE_COUNT;
	mmc_test_put_device( dev );
	__LEAVE( "ret=%d", ret );
	return ret;
}

static int mmc_test_release( struct inode *inode, struct file *file )
{
	int ret = -ENODEV;
	mmc_test_device_t dev = (mmc_test_device_t)file->private_data;
	
	if ( !dev ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "file->private_data == NULL\n" );
		goto error;
	}

	__ENTER( "host=%d, card=%d", dev->card->ctrlr->slot, dev->card->slot );
	
	file->private_data = NULL;
	
	mmc_test_put_device( dev );
	MOD_DEC_USE_COUNT;
	
	ret = 0;
error:	
	__LEAVE( "ret=%d", ret );
	return ret;
}

static ssize_t mmc_test_read( struct file *file, char *buf, size_t size, loff_t *ppos )
{
	ssize_t ret = -ENODEV;
	ssize_t retsize = 0;
	mmc_test_device_t dev = (mmc_test_device_t)file->private_data;

	__ENTER( "host=%d, card=%d, size=%d", dev->card->ctrlr->slot, dev->card->slot, size );
	
	if ( !dev ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "file->private_data == NULL\n" );
		goto error;
	}
	
	switch ( dev->transfer_mode ) {
		char *mbuf;
		
		case MMC_TRANSFER_MODE_BLOCK_SINGLE:
			mbuf = kmalloc( 512, GFP_ATOMIC ); /* FIXME: actual read_bl_len or ctrlr->block_size_max whichever is less ), GFP_KERNEL */
			if ( !mbuf ) { 
				ret = -ENOMEM; 
				goto error; 
			}
			
			while( size > 0 ) {
				int lsize = (size > 512) ? 512 : size; 
		
				MMC_DEBUG( MMC_DEBUG_LEVEL4, 
						"before mmc_read mbuf=0x%x "
						"lsize=%d ppos=0x%x *ppos=%d\n",
						mbuf, lsize, ppos, *ppos );
				ret = mmc_read( dev->card, 
						MMC_TRANSFER_MODE_BLOCK_SINGLE,
						mbuf, lsize, ppos );
				if ( ret <= 0 )
					break;
				
				/* Copy to user */
				if ( copy_to_user( buf, mbuf, ret ) ) {
					ret = -EFAULT;
					break;
				}
				retsize += ret;
				buf += ret;
				size -= ret;
			}
			
			if ( retsize > 0 )
				ret = retsize;
			kfree(mbuf);
			break;
			
		case MMC_TRANSFER_MODE_BLOCK_MULTIPLE:
			mbuf = kmalloc( 1024, GFP_ATOMIC ); /* FIXME */
			if ( !mbuf ) { 
				ret = -ENOMEM; 
				goto error; 
			}
			
			while( size > 0 ) {
				int lsize = (size > 1024) ? 1024 : size; 
		
				MMC_DEBUG( MMC_DEBUG_LEVEL4, 
						"before mmc_read mbuf=0x%x "
						"lsize=%d ppos=0x%x *ppos=%d\n",
						mbuf, lsize, ppos, *ppos );
				ret = mmc_read( dev->card, 
					MMC_TRANSFER_MODE_BLOCK_MULTIPLE,
					mbuf, lsize, ppos );
				if ( ret <= 0 )
					break;
				
				/* Copy to user */
				if ( copy_to_user( buf, mbuf, ret ) ) {
					ret = -EFAULT;
					break;
				}
				retsize += ret;
				buf += ret;
				size -= ret;
			}
			
			if ( retsize > 0 )
				ret = retsize;
			kfree(mbuf);
			break;
			
		case MMC_TRANSFER_MODE_STREAM:
			ret = mmc_read( dev->card, dev->transfer_mode,
					buf, size, ppos );
			break;
			
		default:
			MMC_DEBUG( MMC_DEBUG_LEVEL0, "invalid transfer mode\n" );
	}
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static ssize_t mmc_test_write( struct file *file, const char *buf, size_t size, loff_t *ppos )
{
	ssize_t ret = -ENODEV;
	mmc_test_device_t dev = (mmc_test_device_t)file->private_data;
	int retsize=0;

	__ENTER( "host=%d, card=%d, size=%d", dev->card->ctrlr->slot, dev->card->slot, size );
	
	if ( !dev ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "file->private_data == NULL\n" );
		goto error;
	}
	
	switch ( dev->transfer_mode ) {
		char *mbuf;
		
		case MMC_TRANSFER_MODE_BLOCK_SINGLE:
			mbuf = kmalloc( 512, GFP_ATOMIC ); /* FIXME: actual write_bl_len or ctrlr->block_size_max whichever is less, GFP_KERNEL */
			if ( !mbuf ) { 
				ret = -ENOMEM; 
				goto error; 
			}
	
			while ( size > 0 ) {
				int lsize = ( size > 512 ) ? 512 : size;
		
				/* Copy from user */
				if ( copy_from_user( mbuf, buf, lsize ) ) {
					ret = -EFAULT;
					break;
				}
				
				ret = mmc_write( dev->card,
					MMC_TRANSFER_MODE_BLOCK_SINGLE, 
					mbuf, lsize, ppos );
				if( ret <= 0 )
					break;
	
				retsize += ret;
				buf += ret;
				size -= ret;
			}
			
			if ( retsize > 0 )
				ret = retsize;
			
			kfree( mbuf );
			break; 
			
		case MMC_TRANSFER_MODE_BLOCK_MULTIPLE:
			mbuf = kmalloc( 1024, GFP_ATOMIC ); /* FIXME */
			if ( !mbuf ) { 
				ret = -ENOMEM; 
				goto error; 
			}
			
			while( size > 0 ) {
				int lsize = (size > 1024) ? 1024 : size; 
		
				MMC_DEBUG( MMC_DEBUG_LEVEL4, 
						"before mmc_read mbuf=0x%x "
						"lsize=%d ppos=0x%x *ppos=%d\n",
						mbuf, lsize, ppos, *ppos );
				ret = mmc_write( dev->card, 
					MMC_TRANSFER_MODE_BLOCK_MULTIPLE,
					mbuf, lsize, ppos );
				if ( ret <= 0 )
					break;
				
				/* Copy to user */
				if ( copy_to_user( (char *)buf, mbuf, ret ) ) {
					ret = -EFAULT;
					break;
				}
				retsize += ret;
				buf += ret;
				size -= ret;
			}
			
			if ( retsize > 0 )
				ret = retsize;
			kfree(mbuf);
			break;
		case MMC_TRANSFER_MODE_STREAM:
			ret = mmc_write( dev->card, dev->transfer_mode,
					buf, size, ppos );
			break;
			
		default:
			MMC_DEBUG( MMC_DEBUG_LEVEL0, "invalid transfer mode\n" );
	}
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static loff_t mmc_test_llseek( struct file *file, loff_t offset, int origin )
{
	loff_t ret = -ESPIPE;
	mmc_test_device_t dev = (mmc_test_device_t)file->private_data;
	mmc_card_t card;
	
	if ( !dev ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "file->private_data == NULL\n" );
		ret = -ENODEV;
		goto error;
	}
	
	__ENTER( "host=%d, card=%d, off=%ld, orig=%d", dev->card->ctrlr->slot, dev->card->slot, (long)offset, origin );
	
	card = dev->card;
	
	switch ( origin ) {
		case SEEK_CUR:
			file->f_pos += offset;
			break;
	
		case SEEK_END:
			file->f_pos = card->info.capacity + offset;
			break;
		
		case SEEK_SET:
			file->f_pos = offset;
			break;
			
		default:
			ret = -EINVAL;
			goto error;
	}
	
	ret = file->f_pos;
error:	
	__LEAVE( "ret=%ld", (long)ret );
	return ret;
}

static int mmc_test_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg )
{
	int ret = -ENODEV;
	mmc_test_device_t dev = (mmc_test_device_t)file->private_data;

	if ( !dev ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "file->private_data == NULL\n" );
		goto error;
	}
	
	switch ( cmd ) {
		case IOCMMCSTRNSMODE:
			if ( get_user( ret, (int *)arg ) ) {
				ret = -EFAULT;
				goto error;
			}
			ret = mmc_test_set_transfer_mode( dev, ret );
			break;
			
		case IOCMMCGTRNSMODE:
			ret = mmc_test_get_transfer_mode( dev );
			if ( put_user( ret, (int *)arg ) ) 
				ret = -EFAULT;
			break;
		
		default:
			ret = mmc_ioctl( dev->card, cmd, arg );
	}
	
error:
	return ret;
}

struct file_operations mmc_test_fops = {
	owner:		THIS_MODULE,
	open:		mmc_test_open,
	release:	mmc_test_release,
	read:		mmc_test_read,
	write:		mmc_test_write,
	ioctl:		mmc_test_ioctl,
	llseek:		mmc_test_llseek
};

#ifdef CONFIG_DEVFS_FS
static int mmc_test_add_card( mmc_card_t card ) /* TODO */
{
	int ret = -1;
	__ENTER( "host=%d, card=%d", card->ctrlr->slot, card->slot );
/* TODO: make kdev; register with devfs */
	__LEAVE0( );
	return ret;
}

static int mmc_test_remove_card( mmc_card_t card ) /* TODO */
{
	int ret = -1;
	__ENTER( "host=%d, card=%d", card->ctrlr->slot, card->slot );
/* TODO: make kdev; unregister with devfs */
	__LEAVE0( );
	return ret;
}

static mmc_notifier_rec_t mmc_test_notifier = {
	add:	mmc_test_add_card,
	remove: mmc_test_remove_card
};
#endif /* CONFIG_DEVFS_FS */

static int __init mmc_test_module_init( void )
{
	int ret = -ENODEV;

#ifdef CONFIG_DEVFS_FS
	if ( !mmc_register( MMC_REG_TYPE_USER, &mmc_test_notifier, 0 ) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, "failed to register with MMC core\n" );
		goto error;
	}
#else
	mmc_register( MMC_REG_TYPE_USER, NULL, 0 );
	if ( register_chrdev( MMC_TEST_MAJOR, "mmc_test", &mmc_test_fops ) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, 
				"failed to request device major number\n" );
		mmc_unregister( MMC_REG_TYPE_USER, NULL );
		goto error;
	}
#endif
	
	memset( mmc_test_device, 0, sizeof( mmc_test_device ) );
	
	ret = 0;
error:
	return ret;
}

static void __exit mmc_test_module_cleanup( void )
{
#ifdef CONFIG_DEVFS_FS
	mmc_unregister( MMC_REG_TYPE_USER, &mmc_test_notifier );
#else
	mmc_unregister( MMC_REG_TYPE_USER, NULL );
	unregister_chrdev( MMC_TEST_MAJOR, "mmc_test" );
#endif
}

EXPORT_NO_SYMBOLS;

module_init( mmc_test_module_init );
module_exit( mmc_test_module_cleanup );

MODULE_LICENSE("GPL");
