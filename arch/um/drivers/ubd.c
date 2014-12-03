/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "ubd_user.h"
#define MAJOR_NR UBD_MAJOR
#include "linux/config.h"
#include "linux/blk.h"
#include "linux/blkdev.h"
#include "linux/hdreg.h"
#include "linux/init.h"
#include "linux/devfs_fs_kernel.h"
#include "linux/cdrom.h"
#include "linux/proc_fs.h"
#include "linux/ctype.h"
#include "linux/capability.h"
#include "linux/mm.h"
#include "linux/vmalloc.h"
#include "asm/segment.h"
#include "asm/uaccess.h"
#include "asm/irq.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"
#include "mconsole_kern.h"

static int ubd_open(struct inode * inode, struct file * filp);
static int ubd_release(struct inode * inode, struct file * file);
static int ubd_ioctl(struct inode * inode, struct file * file,
		     unsigned int cmd, unsigned long arg);

#define MAX_DEV (8)

static int blk_sizes[MAX_DEV] = { [ 0 ... MAX_DEV - 1 ] = BLOCK_SIZE };

static int hardsect_sizes[MAX_DEV] = { [ 0 ... MAX_DEV - 1 ] = BLOCK_SIZE };

static int sizes[MAX_DEV] = { [ 0 ... MAX_DEV - 1 ] = 0 };

static struct block_device_operations ubd_blops = {
        open:		ubd_open,
        release:	ubd_release,
        ioctl:		ubd_ioctl,
};

static struct hd_struct	ubd_part[MAX_DEV] = 
{ [ 0 ... MAX_DEV - 1 ] = { 0, 0, 0 } };

static int fake_major = 0;

static struct gendisk ubd_gendisk = {
	MAJOR_NR,		/* Major number */	
	"ubd",			/* Major name */
	0,			/* Bits to shift to get real from partition */
	1,			/* Number of partitions per real */
	ubd_part,		/* hd struct */
	sizes,   		/* block sizes */
	MAX_DEV,		/* number */
	NULL,           	/* internal */
	NULL,			/* next */
	&ubd_blops,		/* file operations */
};

static struct gendisk fake_gendisk = {
	0,   			/* Major number */	
	"ubd",			/* Major name */
	0,			/* Bits to shift to get real from partition */
	1,			/* Number of partitions per real */
	ubd_part,		/* hd struct */
	sizes,   		/* block sizes */
	MAX_DEV,		/* number */
	NULL,           	/* internal */
	NULL,			/* next */
	&ubd_blops,		/* file operations */
};

#ifdef CONFIG_BLK_DEV_UBD_SYNC
#define OPEN_FLAGS O_RDWR | O_SYNC
#else
#define OPEN_FLAGS O_RDWR
#endif

struct cow {
	char *file;
	int fd;
	unsigned long *bitmap;
	int bitmap_offset;
        int data_offset;
};

struct ubd {
	char *file;
	int is_dir;
	int count;
	int fd;
	__u64 size;
	int boot_openflags;
	int openflags;
	devfs_handle_t real;
	devfs_handle_t fake;
	struct cow cow;
};

#define DEFAULT_COW { \
	file:			NULL, \
        fd:			-1, \
        bitmap:			NULL, \
	bitmap_offset:		0, \
        data_offset:		0, \
}

#define DEFAULT_UBD { \
	file: 			NULL, \
	is_dir:			0, \
	count:			0, \
	fd:			-1, \
	size:			-1, \
	boot_openflags:		OPEN_FLAGS, \
	openflags:		OPEN_FLAGS, \
	real:			NULL, \
	fake:			NULL, \
        cow:			DEFAULT_COW, \
}

struct ubd ubd_dev[MAX_DEV] = { 
{ 
	file: 			"root_fs",
	is_dir:			0, 
	count:			0, 
	fd:			-1, 
	size:			0, 
	boot_openflags:		OPEN_FLAGS, 
	openflags:		OPEN_FLAGS,
	real:			NULL,
	fake:			NULL,
	cow:			DEFAULT_COW,
}, 
[ 1 ... MAX_DEV - 1 ] = DEFAULT_UBD
};

static struct hd_driveid ubd_id = {
        cyls:		0,
	heads:		128,
	sectors:	32,
};

static int fake_ide = 0;
static struct proc_dir_entry *proc_ide_root = NULL;
static struct proc_dir_entry *proc_ide = NULL;

static void make_proc_ide(void)
{
	proc_ide_root = proc_mkdir("ide", 0);
	proc_ide = proc_mkdir("ide0", proc_ide_root);
}

static int proc_ide_read_media(char *page, char **start, off_t off, int count,
			       int *eof, void *data)
{
	int len;

	strcpy(page, "disk\n");
	len = strlen("disk\n");
	len -= off;
	if (len < count){
		*eof = 1;
		if (len <= 0) return 0;
	}
	else len = count;
	*start = page + off;
	return len;
	
}

static void make_ide_entries(char *dev_name)
{
	struct proc_dir_entry *dir, *ent;
	char name[64];

	if(!fake_ide) return;
	if(proc_ide_root == NULL) make_proc_ide();
	dir = proc_mkdir(dev_name, proc_ide);
	ent = create_proc_entry("media", S_IFREG|S_IRUGO, dir);
	if(!ent) return;
	ent->nlink = 1;
	ent->data = NULL;
	ent->read_proc = proc_ide_read_media;
	ent->write_proc = NULL;
	sprintf(name,"ide0/%s", dev_name);
	proc_symlink(dev_name, proc_ide_root, name);
}

static int fake_ide_setup(char *str)
{
	fake_ide = 1;
	return(1);
}

__setup("fake_ide", fake_ide_setup);

static int ubd_setup_common(char *str, int *index_out)
{
	char *backing_file;
	int n, sync, perm = O_RDWR;

	if(index_out) *index_out = -1;
	n = *str++;
	if(n == '='){
		char *end;
		int major;

		if(!strcmp(str, "sync")){
			sync = 1;
			return(0);
		}
		major = simple_strtoul(str, &end, 0);
		if(*end != '\0'){
			printk("ubd_setup : didn't parse major number\n");
			return(1);
		}
		fake_gendisk.major = major;
		fake_major = major;
		printk("Setting extra ubd major number to %d\n", major);
 		return(0);
	}
	if(n < '0'){
		printk("ubd_setup : index out of range\n");
		return(1);
	}
	n -= '0';
	if(n >= MAX_DEV){
		printk("ubd_setup : index out of range\n");
		return(1);
	}
	if(index_out) *index_out = n;
	sync = ubd_dev[n].boot_openflags & O_SYNC;
	if (*str == 'r') {
		perm = O_RDONLY;
		str++;
	}
	if (*str == 's') {
		sync = O_SYNC;
		str++;
	}
	if(*str++ != '='){
		printk("ubd_setup : Expected '='\n");
		return(1);
	}
	backing_file = strchr(str, ',');
	if(backing_file){
		*backing_file = '\0';
		backing_file++;
	}
	ubd_dev[n].file = str;
	ubd_dev[n].cow.file = backing_file;
	ubd_dev[n].boot_openflags = perm | sync;
	return(0);
}

static int ubd_setup(char *str)
{
	ubd_setup_common(str, NULL);
	return(1);
}

__setup("ubd", ubd_setup);

static int fakehd(char *str)
{
	printk("fakehd : Changing ubd_gendisk.major_name to \"hd\".\n");
	ubd_gendisk.major_name = "hd";
	return(1);
}

__setup("fakehd", fakehd);

static void do_ubd_request(request_queue_t * q);

int thread_fds[2] = { -1, -1 };

int intr_count = 0;

extern int errno;

#ifdef CONFIG_SMP
#error end_request needs some locking
#endif

static void ubd_finish(int error)
{
	int nsect;

	if(error){
		end_request(0);
		return;
	}
	nsect = CURRENT->current_nr_sectors;
	CURRENT->sector += nsect;
	CURRENT->buffer += nsect << 9;
	CURRENT->errors = 0;
	CURRENT->nr_sectors -= nsect;
	CURRENT->current_nr_sectors = 0;
	end_request(1);
}

static void ubd_handler(void)
{
	struct io_thread_req req;

	DEVICE_INTR = NULL;
	intr_count++;
	if(read_ubd_fs(thread_fds[0], &req, sizeof(req)) != sizeof(req)){
		printk("Pid %d - spurious interrupt in ubd_handler, "
		       "errno = %d\n", getpid(), errno);
		end_request(0);
		return;
	}
        
        if((req.offset != ((__u64) (CURRENT->sector)) << 9) ||
	   (req.length != (CURRENT->current_nr_sectors) << 9))
		panic("I/O op mismatch");
	
	ubd_finish(req.error);
	reactivate_fd(thread_fds[0]);	
	if (!QUEUE_EMPTY) do_ubd_request(NULL);
}

static void ubd_intr(int irq, void *dev, struct pt_regs *unused)
{
	ubd_handler();
}

static int io_pid = -1;

void kill_io_thread(void)
{
	if(io_pid != -1) kill(io_pid, SIGKILL);
}

__exitcall(kill_io_thread);

int sync = 0;

devfs_handle_t ubd_dir_handle;

static int ubd_add(int n)
{
	char name[sizeof("nnnnnn\0")], dev_name[sizeof("ubd0x")];

	if(ubd_dev[n].file == NULL) return(-1);
	sprintf(name, "%d", n);
	ubd_dev[n].real = devfs_register (ubd_dir_handle, name, 
					  DEVFS_FL_DEFAULT, MAJOR_NR, n,
					  S_IFBLK | S_IRUSR | S_IWUSR | 
					  S_IRGRP |S_IWGRP,
					  &ubd_blops, NULL);
	if(fake_major != 0){
		ubd_dev[n].fake = devfs_register (ubd_dir_handle, name, 
						  DEVFS_FL_DEFAULT, fake_major,
						  n, S_IFBLK | S_IRUSR | 
						  S_IWUSR | S_IRGRP | S_IWGRP,
						  &ubd_blops, NULL);
	}
	if(!strcmp(ubd_gendisk.major_name, "ubd")){
		sprintf(dev_name, "%s%d", ubd_gendisk.major_name, n);
	}
	else sprintf(dev_name, "%s%c", ubd_gendisk.major_name, 
		     n + 'a');
	make_ide_entries(dev_name);
	return(0);
}

static int ubd_config(char *str)
{
	int n, err;

	str = uml_strdup(str);
	if(str == NULL){
		printk(KERN_ERR "ubd_config failed to strdup string\n");
		return(1);
	}
	err = ubd_setup_common(str, &n);
	if(err){
		kfree(str);
		return(-1);
	}
	if(n != -1) ubd_add(n);
	return(0);
}

static int ubd_remove(char *str)
{
	int n;

	if(!isdigit(*str)) return(-1);
	n = *str - '0';
	if(ubd_dev[n].file == NULL) return(0);
	if(ubd_dev[n].count > 0) return(-1);
	if(ubd_dev[n].real != NULL) devfs_unregister(ubd_dev[n].real);
	if(ubd_dev[n].fake != NULL) devfs_unregister(ubd_dev[n].fake);
	ubd_dev[n] = ((struct ubd) DEFAULT_UBD);
	return(0);
}

static struct mc_device ubd_mc = {
	name:		"ubd",
	config:		ubd_config,
	remove:		ubd_remove,
};

int ubd_mc_init(void)
{
	mconsole_register_dev(&ubd_mc);
	return(0);
}

__initcall(ubd_mc_init);

int ubd_init(void)
{
	unsigned long stack;
        int i, err;
	request_queue_t *q;

	ubd_dir_handle = devfs_mk_dir (NULL, "ubd", NULL);
	if (devfs_register_blkdev(MAJOR_NR, "ubd", &ubd_blops)) {
		printk("ubd: unable to get major %d\n", MAJOR_NR);
		return -1;
	}
	q = BLK_DEFAULT_QUEUE(MAJOR_NR);
	blk_init_queue(q, DEVICE_REQUEST);
	elevator_init(&q->elevator, ELEVATOR_NOOP);
	read_ahead[MAJOR_NR] = 8;		/* 8 sector (4kB) read-ahead */
	blksize_size[MAJOR_NR] = blk_sizes;
	blk_size[MAJOR_NR] = sizes;
	hardsect_size[MAJOR_NR] = hardsect_sizes;

	add_gendisk(&ubd_gendisk);

	if (fake_major != 0){
		if(devfs_register_blkdev(fake_major, "ubd", &ubd_blops)) {
			printk("ubd: unable to get major %d\n", fake_major);
			return -1;
		}
		blk_init_queue(BLK_DEFAULT_QUEUE(fake_major), DEVICE_REQUEST);
		read_ahead[fake_major] = 8;	/* 8 sector (4kB) read-ahead */
		blksize_size[fake_major] = blk_sizes;
		hardsect_size[fake_major] = hardsect_sizes;

		add_gendisk(&fake_gendisk);
	}
	for(i=0;i<MAX_DEV;i++) ubd_add(i);
	if(sync){
		printk("ubd : Synchronous mode\n");
		return(0);
	}
	stack = alloc_stack();
	io_pid = start_io_thread(stack + PAGE_SIZE - sizeof(void *), 
				 thread_fds);
	if(io_pid < 0){
		printk("ubd : Failed to start I/O thread (errno = %d) - "
		       "falling back to synchronous I/O\n", -io_pid);
		return(0);
	}
	err = um_request_irq(UBD_IRQ, thread_fds[0], ubd_intr, SA_INTERRUPT, 
			     "ubd", ubd_dev);
	if(err != 0) printk("um_request_irq failed - errno = %d\n", -err);
	return(err);
}

__initcall(ubd_init);

static void ubd_close(struct ubd *dev)
{
	close_fd(dev->fd);
	if(dev->cow.file != NULL) {
		close_fd(dev->cow.fd);
		vfree(dev->cow.bitmap);
		dev->cow.bitmap = NULL;
	}
}

static int ubd_open_dev(struct ubd *dev)
{
	char *backing_file;
	int err, flags, n, bitmap_len;

	dev->fd = open_ubd_file(dev->file, &dev->openflags, &backing_file,
				&dev->cow.bitmap_offset, &bitmap_len, 
				&dev->cow.data_offset);

	if((dev->fd == -ENOENT) && (dev->cow.file != NULL)){
		printk(KERN_INFO "Creating \"%s\" as COW file for \"%s\"\n",
		       dev->file, dev->cow.file);
		n = dev - ubd_dev;
		dev->fd = create_cow_file(dev->file, dev->cow.file, 1 << 9,
					  &dev->cow.bitmap_offset, &bitmap_len,
					  &dev->cow.data_offset);
		if(dev->fd < 0){
			printk(KERN_ERR "Creation of COW file \"%s\" failed, "
			       "errno = %d\n", dev->file, -dev->fd);
		}
	}

	if(dev->fd < 0) return(dev->fd);

	if(dev->cow.file == NULL) dev->cow.file = backing_file;
	if((backing_file != NULL) && strcmp(dev->cow.file, backing_file)){
		printk(KERN_WARNING "Backing file mismatch - \"%s\" "
		       "requested, \"%s\" specified in COW header of \"%s\"\n",
		       dev->cow.file, backing_file, dev->file);
		printk(KERN_WARNING "Using \"%s\"\n", backing_file);
		dev->cow.file = backing_file;
	}

	if(dev->cow.file != NULL){
		err = -ENOMEM;
		dev->cow.bitmap = (void *) vmalloc(bitmap_len);
		flush_tlb_kernel_vm();
		if(dev->cow.bitmap == NULL) goto error;

		err = read_cow_bitmap(dev->fd, dev->cow.bitmap, 
				      dev->cow.bitmap_offset, bitmap_len);
		if(err) goto error;

		flags = O_RDONLY;
		err = open_ubd_file(dev->cow.file, &flags, NULL, NULL, NULL, 
				    NULL);
		if(err < 0) goto error;
		dev->cow.fd = err;
	}
	return(0);
 error:
	close_fd(dev->fd);
	return(err);
}

static int ubd_open(struct inode * inode, struct file * filp)
{
	char *file;
	int n;

	n = DEVICE_NR(inode->i_rdev);
	if(n > MAX_DEV)
		return -ENODEV;
	if(ubd_is_dir(ubd_dev[n].file)){
		ubd_dev[n].is_dir = 1;
		return(0);
	}
	if(ubd_dev[n].count == 0){
		ubd_dev[n].openflags = ubd_dev[n].boot_openflags;
		/* XXX This error is wrong when errno isn't stored in
		 * ubd_dev[n].fd
		 */
		if(ubd_open_dev(&ubd_dev[n]) < 0){
			printk(KERN_ERR "ubd%d: Can't open \"%s\": "
			       "errno = %d\n", n, ubd_dev[n].file, 
			       -ubd_dev[n].fd);
		}
		if(ubd_dev[n].fd < 0)
			return -ENODEV;
		file = ubd_dev[n].cow.file ? ubd_dev[n].cow.file : 
			ubd_dev[n].file;
		ubd_dev[n].size = file_size(file);
		if(ubd_dev[n].size < 0) return(ubd_dev[n].size);
		ubd_part[n].start_sect = 0;
		ubd_part[n].nr_sects = ubd_dev[n].size / blk_sizes[n];
		sizes[n] = ubd_dev[n].size / BLOCK_SIZE;
	}
	ubd_dev[n].count++;
	if ((filp->f_mode & FMODE_WRITE) && 
	    ((ubd_dev[n].openflags & ~O_SYNC) == O_RDONLY)){
	        if(--ubd_dev[n].count == 0) ubd_close(&ubd_dev[n]);
	        return -EROFS;
	}
	return(0);
}

static int ubd_release(struct inode * inode, struct file * file)
{
        int n;

	n =  DEVICE_NR(inode->i_rdev);
	if(n > MAX_DEV)
		return -ENODEV;
	if(--ubd_dev[n].count == 0) ubd_close(&ubd_dev[n]);
	return(0);
}

int cow_read = 0;
int cow_write = 0;

void cowify_req(struct io_thread_req *req, struct ubd *dev)
{
        int i, update_bitmap, sector = req->offset >> 9;

	if(req->length > (sizeof(req->sector_mask) * 8) << 9)
		panic("Operation too long");
	if(req->read) {
		for(i = 0; i < req->length >> 9; i++){
			if(ubd_test_bit(sector + i, dev->cow.bitmap)){
				ubd_set_bit(i, &req->sector_mask);
				cow_read++;
			}
                }
        } 
        else {
		update_bitmap = 0;
		for(i = 0; i < req->length >> 9; i++){
			cow_write++;
			ubd_set_bit(i, &req->sector_mask);
			if(!ubd_test_bit(sector + i, dev->cow.bitmap))
				update_bitmap = 1;
			ubd_set_bit(sector + i, dev->cow.bitmap);
		}
		if(update_bitmap){
			req->bitmap_start = sector / (sizeof(long) * 8);
			req->bitmap_end = (sector + i) / (sizeof(long) * 8);
			req->bitmap_end++;
			req->bitmap_offset = dev->cow.bitmap_offset;
		}
	}
}

static void do_one_request(void)
{
	struct io_thread_req req;
	int block, nsect, dev, n, err;

	if(CURRENT->rq_status == RQ_INACTIVE) return;
	if (DEVICE_INTR) return;
	INIT_REQUEST;
	block = CURRENT->sector;
	nsect = CURRENT->current_nr_sectors;
	dev = MINOR(CURRENT->rq_dev);
	if(ubd_dev[dev].is_dir){
		strcpy(CURRENT->buffer, "HOSTFS:");
		strcat(CURRENT->buffer, ubd_dev[dev].file);
		end_request(1);
		return;
	}
	if((CURRENT->cmd == WRITE) && 
	   ((ubd_dev[dev].openflags & O_ACCMODE) == O_RDONLY)){
		printk("Write attempted on readonly ubd device %d\n", 
		       dev);
		end_request(0);
		return;
	}

	req.read = (CURRENT->cmd == READ);
	req.fds[0] = (ubd_dev[dev].cow.file != NULL) ? 
		ubd_dev[dev].cow.fd : ubd_dev[dev].fd;
	req.fds[1] = ubd_dev[dev].fd;
	req.offsets[0] = 0;
	req.offsets[1] = ubd_dev[dev].cow.data_offset;
	req.offset = ((__u64) block) << 9;
	req.length = nsect << 9;
	req.buffer = CURRENT->buffer;
	req.sectorsize = 1 << 9;
	req.sector_mask = 0;
	req.bitmap = ubd_dev[dev].cow.bitmap;
	req.bitmap_offset = -1;
	req.bitmap_start = -1;
	req.bitmap_end = -1;
	req.error = 0;

        if(ubd_dev[dev].cow.file != NULL) cowify_req(&req, &ubd_dev[dev]);

	if(thread_fds[0] == -1){
		err = do_io(&req);
		ubd_finish(err);
	}
	else {
		SET_INTR(ubd_handler);
		n = write_ubd_fs(thread_fds[1], (char *) &req, sizeof(req));
		if(n != sizeof(req))
			printk("write to io thread failed - returned %d, "
			       "errno %d\n", n, errno);
	}
}

static void do_ubd_request(request_queue_t * q)
{
	if(thread_fds[0] == -1){
		while(!QUEUE_EMPTY) do_one_request();
	}
	else do_one_request();
}

static int ubd_ioctl(struct inode * inode, struct file * file,
		     unsigned int cmd, unsigned long arg)
{
	struct hd_geometry *loc = (struct hd_geometry *) arg;
	int dev, err;

	if ((!inode) || !(inode->i_rdev))
		return -EINVAL;
	dev = DEVICE_NR(inode->i_rdev);
	if (dev > MAX_DEV)
		return -EINVAL;
	switch (cmd) {
	        struct hd_geometry g;
		struct cdrom_volctrl volume;
	case HDIO_GETGEO:
		if (!loc)  return -EINVAL;
		g.heads = 128;
		g.sectors = 32;
		g.cylinders = ubd_dev[dev].size / (128 * 32);
		g.start = 2;
		return copy_to_user(loc, &g, sizeof g) ? -EFAULT : 0; 
	case BLKRASET:
		if(!capable(CAP_SYS_ADMIN))  return -EACCES;
		if(arg > 0xff) return -EINVAL;
		read_ahead[MAJOR(inode->i_rdev)] = arg;
		return 0;
	case BLKRAGET:
		if (!arg)  return -EINVAL;
		err = verify_area(VERIFY_WRITE, (long *) arg, sizeof(long));
		if (err)
			return err;
		return 0;
	case BLKGETSIZE:   /* Return device size */
		if (!arg)  return -EINVAL;
		err = verify_area(VERIFY_WRITE, (long *) arg, sizeof(long));
		if (err)
			return err;
		put_user(ubd_dev[dev].size >> 9, (long *) arg);
		return 0;
	case BLKFLSBUF:
		if(!capable(CAP_SYS_ADMIN))  return -EACCES;
		return 0;

	case BLKRRPART: /* Re-read partition tables */
		return 0; /* revalidate_hddisk(inode->i_rdev, 1); */

	case HDIO_SET_UNMASKINTR:
		if (!capable(CAP_SYS_ADMIN)) return -EACCES;
		if ((arg > 1) || (MINOR(inode->i_rdev) & 0x3F))
			return -EINVAL;
		return 0;

	case HDIO_GET_UNMASKINTR:
		if (!arg)  return -EINVAL;
		err = verify_area(VERIFY_WRITE, (long *) arg, sizeof(long));
		if (err)
			return err;
		return 0;

	case HDIO_GET_MULTCOUNT:
		if (!arg)  return -EINVAL;
		err = verify_area(VERIFY_WRITE, (long *) arg, sizeof(long));
		if (err)
			return err;
		return 0;

	case HDIO_SET_MULTCOUNT:
		if (!capable(CAP_SYS_ADMIN)) return -EACCES;
		if (MINOR(inode->i_rdev) & 0x3F) return -EINVAL;
		return 0;

	case HDIO_GET_IDENTITY:
		ubd_id.cyls = ubd_dev[dev].size / (128 * 32);
		if (copy_to_user((char *) arg, (char *) &ubd_id, 
				 sizeof(ubd_id)))
			return -EFAULT;
		return 0;
		
	case CDROMVOLREAD:
		if(copy_from_user(&volume, (char *) arg, sizeof(volume)))
			return -EFAULT;
		volume.channel0 = 255;
		volume.channel1 = 255;
		volume.channel2 = 255;
		volume.channel3 = 255;
		if(copy_from_user((char *) arg, &volume, sizeof(volume)))
			return -EFAULT;		
		return 0;

	default:
		return -EINVAL;
	}
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
