/*
 * MassWorks ID-75 USB Driver based on
 *		USB Skeleton driver
 *
 * Copyright (c) 2001 MassWorks (contact@massworks.com)
 * Copyright (c) 2001 Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 *
 *
 * This driver is to used to open up the endpoints of the ID-75 to user
 * applications.  All the 'smarts' of the driver comes from streaming commands
 * to the ID-75 from user space.
 *
 * 
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/smp_lock.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/usb.h>

#ifdef CONFIG_USB_DEBUG
	static int debug = 1;
#else
	static int debug;
#endif

/* Use our own dbg macro */
#undef dbg
#define dbg(format, arg...) do { if (debug) printk(KERN_DEBUG __FILE__ ": " format "\n" , ## arg); } while (0)


/* Version Information */
#define DRIVER_VERSION	"v1.0"
#define DRIVER_AUTHOR	"MassWorks, contact@massworks.com"
#define DRIVER_DESC	"MassWorks ID-75 USB Driver"

/* 'ioctl' calls */
/* #define	IOCTL_ID75_RESETI		_IO('i', 0x01) */

/* Module paramaters */
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");


/* Define these values to match your device */
#define USB_ID75_VENDOR_ID	0x088b
#define USB_ID75_PRODUCT_ID	0x4944

/* table of devices that work with this driver */
static struct usb_device_id id75_table [] = {
	{ USB_DEVICE(USB_ID75_VENDOR_ID, USB_ID75_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id75_table);



/* Get a minor range for your devices from the usb maintainer */
#define USB_ID75_MINOR_BASE	65

/* we can have up to this number of device plugged in at once */
#define MAX_DEVICES		4

/* Structure to hold all of our device specific stuff */
struct usb_id75 {
	struct usb_device *	udev;			/* save off the usb device pointer */
	struct usb_interface *	interface;		/* the interface for this device */
	devfs_handle_t		devfs;			/* devfs device node */
	unsigned char		minor;			/* the starting minor number for this device */
	unsigned char		num_ports;		/* the number of ports this device has */
	char			num_interrupt_in;	/* number of interrupt in endpoints we have */
	char			num_bulk_in;		/* number of bulk in endpoints we have */
	char			num_bulk_out;		/* number of bulk out endpoints we have */

	unsigned char *		int_in_buffer;		/* the buffer to receive data */
	int			int_in_size;		/* the size of the receive buffer */
	struct urb *		read_urb;		/* the urb used to receive data */
	__u8			int_in_endpointAddr;	/* the address of the interrupt in endpoint */

	unsigned char *		bulk_out_buffer;	/* the buffer to send data */
	int			bulk_out_size;		/* the size of the send buffer */
	struct urb *		write_urb;		/* the urb used to send data */
	__u8			bulk_out_endpointAddr;	/* the address of the bulk out endpoint */

	struct tq_struct	tqueue;			/* task queue for line discipline waking up */
	int			open_count;		/* number of times this port has been opened */
	struct semaphore	sem;			/* locks this structure */

	unsigned char		irqbufr[8];		/* Last captured sample */
	int			nirqbufr;		/* Number of valid bytes in buffer */
};


/* the global usb devfs handle */
extern devfs_handle_t usb_devfs_handle;


/* local function prototypes */
static ssize_t id75_read	(struct file *file, char *buffer, size_t count, loff_t *ppos);
static ssize_t id75_write	(struct file *file, const char *buffer, size_t count, loff_t *ppos);
static int id75_ioctl		(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int id75_open		(struct inode *inode, struct file *file);
static int id75_release		(struct inode *inode, struct file *file);
	
static void * id75_probe	(struct usb_device *dev, unsigned int ifnum, const struct usb_device_id *id);
static void id75_disconnect	(struct usb_device *dev, void *ptr);

static void id75_write_bulk_callback	(struct urb *urb);


/* array of pointers to our devices that are currently connected */
static struct usb_id75		*minor_table[MAX_DEVICES];

/* lock to protect the minor_table structure */
static DECLARE_MUTEX (minor_table_mutex);

/* file operations needed when we register this driver */
static struct file_operations id75_fops = {
	owner:		THIS_MODULE,
	read:		id75_read,
	write:		id75_write,
	ioctl:		id75_ioctl,
	open:		id75_open,
	release:	id75_release,
};      


/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver id75_driver = {
	name:		"id75",
	probe:		id75_probe,
	disconnect:	id75_disconnect,
	fops:		&id75_fops,
	minor:		USB_ID75_MINOR_BASE,
	id_table:	id75_table,
};





/**
 *	usb_id75_debug_data
 */
static inline void usb_id75_debug_data (const char *function, int size, const unsigned char *data)
{
	int i;

	if (!debug)
		return;
	
	printk (KERN_DEBUG __FILE__": %s - length = %d, data = ", 
		function, size);
	for (i = 0; i < size; ++i) {
		printk ("%.2x ", data[i]);
	}
	printk ("\n");
}


/**
 *	id75_irq
 */
static void id75_irq (struct urb *urb)
{
	struct usb_id75 *dev = (urb->context);

	/* Process data */
	if (!urb->status) {
		/* Copy to buffer if available */
		if (!dev->nirqbufr) {
			dev->nirqbufr = urb->actual_length;
			memcpy (dev->irqbufr, dev->int_in_buffer, dev->nirqbufr);
		}
	} else {
		/* Error status ? */
		if (urb->status != -ENOENT) {
			/* On error, restart urb */
			err (__FUNCTION__":Error, status %d\n", urb->status );
			usb_submit_urb (dev->read_urb);
		}

	}
}


/**
 *	id75_open
 */
static int id75_open (struct inode *inode, struct file *file)
{
	struct usb_id75 *dev = NULL;
	int subminor;
	int retval = 0;
	
	dbg(__FUNCTION__);

	subminor = MINOR (inode->i_rdev) - USB_ID75_MINOR_BASE;
	if ((subminor < 0) ||
	    (subminor >= MAX_DEVICES)) {
		return -ENODEV;
	}

	/* increment our usage count for the module */
	MOD_INC_USE_COUNT;

	/* lock our minor table and get our local data for this minor */
	down (&minor_table_mutex);
	dev = minor_table[subminor];
	if (dev == NULL) {
		up (&minor_table_mutex);
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}

	/* lock this device */
	down (&dev->sem);

	/* unlock the minor table */
	up (&minor_table_mutex);

	/* increment our usage count for the driver */
	++dev->open_count;

	/* save our object in the file's private structure */
	file->private_data = dev;

	/* unlock this device */
	up (&dev->sem);

	return retval;
}


/**
 *	id75_release
 */
static int id75_release (struct inode *inode, struct file *file)
{
	struct usb_id75 *dev;
	int retval = 0;

	dev = (struct usb_id75 *)file->private_data;
	if (dev == NULL) {
		dbg (__FUNCTION__ " - object is NULL");
		return -ENODEV;
	}

	dbg(__FUNCTION__ " - minor %d", dev->minor);

	/* lock our minor table */
	down (&minor_table_mutex);

	/* lock our device */
	down (&dev->sem);

	if (dev->open_count <= 0) {
		dbg (__FUNCTION__ " - device not opened");
		retval = -ENODEV;
		goto exit_not_opened;
	}

	if (dev->udev == NULL) {
		/* the device was unplugged before the file was released */
		minor_table[dev->minor] = NULL;
		if (dev->int_in_buffer != NULL)
			kfree (dev->int_in_buffer);
		if (dev->bulk_out_buffer != NULL)
			kfree (dev->bulk_out_buffer);
		if (dev->write_urb != NULL)
			usb_free_urb (dev->write_urb);
		if (dev->read_urb != NULL)
			usb_free_urb (dev->read_urb);
		kfree (dev);
		goto exit;
	}

	/* decrement our usage count for the device */
	--dev->open_count;
	if (dev->open_count <= 0) {
		/* shutdown any transfers that might be going on */
		usb_unlink_urb (dev->write_urb);
		usb_unlink_urb (dev->read_urb);
		dev->open_count = 0;
	}

exit:
	/* decrement our usage count for the module */
	MOD_DEC_USE_COUNT;

exit_not_opened:
	up (&dev->sem);
	up (&minor_table_mutex);

	return retval;
}


/**
 *	id75_read
 */
static ssize_t id75_read (struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct usb_id75 *dev;
	int retval = 0;

	dev = (struct usb_id75 *)file->private_data;
	
	dbg(__FUNCTION__ " - minor %d, count = %d", dev->minor, count);

	/* lock this object */
	down (&dev->sem);

	/* verify that the device wasn't unplugged */
	if (dev->udev == NULL) {
		up (&dev->sem);
		return -ENODEV;
	}

	/* Any data to return ? */
	if (dev->nirqbufr) {
		if (copy_to_user (buffer, dev->irqbufr, dev->nirqbufr))
			retval = -EFAULT;
		else
			retval = dev->nirqbufr;
		dev->nirqbufr = 0;
	}
	
	/* unlock the device */
	up (&dev->sem);

	return retval;
}


/**
 *	id75_write
 */
static ssize_t id75_write (struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	struct usb_id75 *dev;
	ssize_t bytes_written = 0;
	int retval = 0;

	dev = (struct usb_id75 *)file->private_data;

	dbg(__FUNCTION__ " - minor %d, count = %d", dev->minor, count);

	/* lock this object */
	down (&dev->sem);

	/* verify that the device wasn't unplugged */
	if (dev->udev == NULL) {
		retval = -ENODEV;
		goto exit;
	}

	/* verify that we actually have some data to write */
	if (count == 0) {
		dbg(__FUNCTION__ " - write request of 0 bytes");
		goto exit;
	}

	/* see if we are already in the middle of a write */
	if (dev->write_urb->status == -EINPROGRESS) {
		dbg (__FUNCTION__ " - already writing");
		goto exit;
	}

	/* we can only write as much as 1 urb will hold */
	bytes_written = (count > dev->bulk_out_size) ? 
				dev->bulk_out_size : count;

	/* copy the data from userspace into our urb */
	if (copy_from_user(dev->write_urb->transfer_buffer, buffer, 
			   bytes_written)) {
		retval = -EFAULT;
		goto exit;
	}

	usb_id75_debug_data (__FUNCTION__, bytes_written, 
			     dev->write_urb->transfer_buffer);

	/* set up our urb */
	FILL_BULK_URB(dev->write_urb, dev->udev, 
		      usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
		      dev->write_urb->transfer_buffer, bytes_written,
		      id75_write_bulk_callback, dev);

	/* send the data out the bulk port */
	retval = usb_submit_urb(dev->write_urb);
	if (retval) {
		err(__FUNCTION__ " - failed submitting write urb, error %d",
		    retval);
	} else {
		retval = bytes_written;
	}

exit:
	/* unlock the device */
	up (&dev->sem);

	return retval;
}


/**
 *	id75_ioctl
 */
static int id75_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct usb_id75 *dev;
	int		ret = -ENOTTY;

	dev = (struct usb_id75 *)file->private_data;

	/* lock this object */
	down (&dev->sem);

	/* verify that the device wasn't unplugged */
	if (dev->udev == NULL) {
		up (&dev->sem);
		return -ENODEV;
	}

	dbg(__FUNCTION__ " - minor %d, cmd 0x%.4x, arg %ld", 
	    dev->minor, cmd, arg);

#if 0
	/* Process 'ioctl' request */
	switch (cmd) {
		/*
		 * TODO:  Until this reset works, developers may have to 
		 * power on/off the device to get it back into sync if 
		 * they confuse the internal processor.  It expects the 
		 * exact number of bytes for each command.
		 */

		/* Reset input ? */
		case IOCTL_ID75_RESETI :
			dbg (__FUNCTION__": IOCTL_ID75_RESETI");

			/* A input 'reset' consists of any packet on the INT(OUT) endpoint */
			FILL_INT_URB(dev->reset_urb, dev->udev,
				usb_sndintpipe(dev->udev,dev->int_out_endpointAddr),
			/*	NULL, 0, NULL, dev, -1); */
				dev->int_out_buffer, 1, id75_irq, dev, -1);

			/* Transmit */
			r = usb_submit_urb ( dev->reset_urb );
			dbg (__FUNCTION__":r %d\n", r);
			if (r) {
				err("Couldn't submit reset URB");
			}

			ret = 0;
			break;
		}
#endif

	/* fill in your device specific stuff here */
	
	/* unlock the device */
	up (&dev->sem);
	
	/* return that we did not understand this ioctl call */
	return ret;
}


/**
 *	id75_write_bulk_callback
 */
static void id75_write_bulk_callback (struct urb *urb)
{
	struct usb_id75 *dev = (struct usb_id75 *)urb->context;

	dbg(__FUNCTION__ " - minor %d", dev->minor);

	if ((urb->status != -ENOENT) && 
	    (urb->status != -ECONNRESET)) {
		dbg(__FUNCTION__ " - nonzero write bulk status received: %d",
		    urb->status);
		return;
	}

	return;
}


static void * id75_probe(struct usb_device *udev, unsigned int ifnum, const struct usb_device_id *id)
{
	struct usb_id75 *dev = NULL;
	struct usb_interface *interface;
	struct usb_interface_descriptor *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int minor;
	int buffer_size;
	int i;
	char name[10];

	
	/* See if the device offered us matches what we can accept */
	if ((udev->descriptor.idVendor != USB_ID75_VENDOR_ID) ||
	    (udev->descriptor.idProduct != USB_ID75_PRODUCT_ID)) {
		return NULL;
	}

	/* select a "subminor" number (part of a minor number) */
	down (&minor_table_mutex);
	for (minor = 0; minor < MAX_DEVICES; ++minor) {
		if (minor_table[minor] == NULL)
			break;
	}
	if (minor >= MAX_DEVICES) {
		info ("Too many devices plugged in, can not handle this device.");
		goto exit;
	}

	/* allocate memory for our device state and intialize it */
	dev = kmalloc (sizeof(struct usb_id75), GFP_KERNEL);
	if (dev == NULL) {
		err ("Out of memory");
		goto exit;
	}
	minor_table[minor] = dev;

	interface = &udev->actconfig->interface[ifnum];

	init_MUTEX (&dev->sem);
	dev->udev = udev;
	dev->interface = interface;
	dev->minor = minor;

	/* set up the endpoint information */
	/* check out the endpoints */
	iface_desc = &interface->altsetting[0];
	for (i = 0; i < iface_desc->bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i];

		/* Bulk output for device command */
		if (((endpoint->bEndpointAddress & 0x80) == 0x00) &&
		    ((endpoint->bmAttributes & 3) == 0x02)) {
			/* we found a bulk out endpoint */
			dev->write_urb = usb_alloc_urb(0);
			if (!dev->write_urb) {
				err("No free urbs available");
				goto error;
			}
			buffer_size = endpoint->wMaxPacketSize;
			dev->bulk_out_size = buffer_size;
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_out_buffer = kmalloc (buffer_size, GFP_KERNEL);
			if (!dev->bulk_out_buffer) {
				err("Couldn't allocate bulk_out_buffer");
				goto error;
			}
			FILL_BULK_URB(dev->write_urb, udev, 
				      usb_sndbulkpipe(udev, 
						      endpoint->bEndpointAddress),
				      dev->bulk_out_buffer, buffer_size,
				      id75_write_bulk_callback, dev);
		}

		/* Interrupt input for device data */
		if ((endpoint->bEndpointAddress & 0x80) &&
		    ((endpoint->bmAttributes & 3) == 0x03)) {
			/* we found an interrupt in endpoint */
			dev->read_urb = usb_alloc_urb(0);
			if (!dev->read_urb) {
				err("No free urbs available");
				goto error;
			}
			buffer_size = endpoint->wMaxPacketSize;
			dev->int_in_size = buffer_size;
			dev->int_in_endpointAddr = endpoint->bEndpointAddress;
			dev->int_in_buffer = kmalloc (buffer_size, GFP_KERNEL);
			if (!dev->int_in_buffer) {
				err("Couldn't allocate int_in_buffer");
				goto error;
			}
			FILL_INT_URB(dev->read_urb, dev->udev,
				usb_rcvintpipe(udev,endpoint->bEndpointAddress),
				dev->int_in_buffer, buffer_size,
				id75_irq, dev, 100);
			if (usb_submit_urb ( dev->read_urb )) {
				err("Couldn't submit INT URB");
				goto error;
			}

		}

	}

	/* initialize the devfs node for this device and register it */
	sprintf(name, "id75%d", dev->minor);
	
	dev->devfs = devfs_register (usb_devfs_handle, name,
				     DEVFS_FL_DEFAULT, USB_MAJOR,
				     USB_ID75_MINOR_BASE + dev->minor,
				     S_IFCHR | S_IRUSR | S_IWUSR | 
				     S_IRGRP | S_IWGRP | S_IROTH, 
				     &id75_fops, NULL);

	/* let the user know what node this device is now attached to */
	info ("MassWorks ID-75 USB device now attached to id75%d", dev->minor);
	goto exit;
	
error:
	minor_table [dev->minor] = NULL;
	if (dev->int_in_buffer != NULL)
		kfree (dev->int_in_buffer);
	if (dev->bulk_out_buffer != NULL)
		kfree (dev->bulk_out_buffer);
	if (dev->write_urb != NULL)
		usb_free_urb (dev->write_urb);
	kfree (dev);
	dev = NULL;

exit:
	up (&minor_table_mutex);
	return dev;
}


/**
 *	id75_disconnect
 */
static void id75_disconnect(struct usb_device *udev, void *ptr)
{
	struct usb_id75 *dev;

	int minor;

	dev = (struct usb_id75 *)ptr;
	
	down (&minor_table_mutex);
	down (&dev->sem);
		
	minor = dev->minor;

	/* Unlink the interrupt urb */
	if (dev->read_urb != NULL) {
		dbg ( __FUNCTION__": Unlinking INT urb\n" );
		usb_unlink_urb (dev->read_urb);
	}

	/* if the device is not opened, then we clean up right now */
	if (!dev->open_count) {
		minor_table[dev->minor] = NULL;
		if (dev->int_in_buffer != NULL)
			kfree (dev->int_in_buffer);
		if (dev->bulk_out_buffer != NULL)
			kfree (dev->bulk_out_buffer);
		if (dev->write_urb != NULL)
			usb_free_urb (dev->write_urb);
		kfree (dev);
	} else {
		dev->udev = NULL;
		up (&dev->sem);
	}

	/* remove our devfs node */
	devfs_unregister(dev->devfs);

	info("MassWorks ID-75 USB #%d now disconnected", minor);
	up (&minor_table_mutex);
}


/**
 *	usb_id75_init
 */
static int __init usb_id75_init(void)
{
	int result;

	/* register this driver with the USB subsystem */
	result = usb_register(&id75_driver);
	if (result < 0) {
		err("usb_register failed for the "__FILE__" driver. Error number %d",
		    result);
		return -1;
	}

	info(DRIVER_VERSION " " DRIVER_AUTHOR);
	info(DRIVER_DESC);
	return 0;
}


/**
 *	usb_id75_exit
 */
static void __exit usb_id75_exit(void)
{
	/* deregister this driver with the USB subsystem */
	usb_deregister(&id75_driver);
}


module_init (usb_id75_init);
module_exit (usb_id75_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

