/* 
 * $Id: blkmtd.c,v 1.15 2002/09/11 14:10:14 spse Exp $
 *
 * blkmtd.c - use a block device as a fake MTD
 *
 * Author: Simon Evans <spse@secret.org.uk>
 *
 * Copyright (C) 2001 Simon Evans
 * 
 * Licence: GPL
 *
 * How it works:
 *       The driver uses raw/io to read/write the device and the page
 *       cache to cache access. Writes update the page cache with the
 *       new data but make a copy of the new page(s) and then a kernel
 *       thread writes pages out to the device in the background. This
 *       ensures that writes are order even if a page is updated twice.
 *       Also, since pages in the page cache are never marked as dirty,
 *       we dont have to worry about writepage() being called on some 
 *       random page which may not be in the write order.
 * 
 *       Erases are handled like writes, so the callback is called after
 *       the page cache has been updated. Sync()ing will wait until it is 
 *       all done.
 *
 *       It can be loaded Read-Only to prevent erases and writes to the 
 *       medium.
 *
 * Todo:
 *       Make the write queue size dynamic so this it is not too big on
 *       small memory systems and too small on large memory systems.
 * 
 *       Page cache usage may still be a bit wrong. Check we are doing
 *       everything properly.
 * 
 *       Somehow allow writes to dirty the page cache so we dont use too
 *       much memory making copies of outgoing pages. Need to handle case
 *       where page x is written to, then page y, then page x again before
 *       any of them have been committed to disk.
 * 
 *       Reading should read multiple pages at once rather than using 
 *       readpage() for each one. This is easy and will be fixed asap.
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/iobuf.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/list.h>
#include <linux/mtd/mtd.h>

#ifdef CONFIG_MTD_DEBUG
#ifdef CONFIG_PROC_FS
#  include <linux/proc_fs.h>
#  define BLKMTD_PROC_DEBUG
   static struct proc_dir_entry *blkmtd_proc;
#endif
#endif


#define err(format, arg...) printk(KERN_ERR "blkmtd: " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO "blkmtd: " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "blkmtd: " format "\n" , ## arg)
#define crit(format, arg...) printk(KERN_CRIT "blkmtd: " format "\n" , ## arg)


/* Default erase size in K, always make it a multiple of PAGE_SIZE */
#define CONFIG_MTD_BLKDEV_ERASESIZE (128 << 10)	/* 128K */
#define VERSION "1.9"

/* Info for the block device */
struct blkmtd_dev {
	struct list_head list;
	struct block_device *binding;
	int readonly;
	struct mtd_info mtd_info;
};

/* Info for each queue item in the write queue */
struct blkmtd_wq {
	struct blkmtd_dev *dev;
	struct page **pages;
	int pagenr;
	int pagecnt;
	int iserase;
};


/* Our erase page - always remains locked. */
static struct page *erase_page;

/* Static info about the MTD, used in cleanup_module */
static LIST_HEAD(blkmtd_device_list);

/* Write queue fixed size */
#define WRITE_QUEUE_SZ 512

/* Storage for the write queue */
static struct blkmtd_wq *write_queue;
static int write_queue_sz = WRITE_QUEUE_SZ;
static int volatile write_queue_head;
static int volatile write_queue_tail;
static int volatile write_queue_cnt;
static spinlock_t mbd_writeq_lock = SPIN_LOCK_UNLOCKED;

/* Tell the write thread to finish */
static volatile int write_task_finish;

/* ipc with the write thread */
static DECLARE_MUTEX_LOCKED(thread_sem);
static DECLARE_WAIT_QUEUE_HEAD(thr_wq);
static DECLARE_WAIT_QUEUE_HEAD(mtbd_sync_wq);

#define MAX_DEVICES 4

/* Module parameters passed by insmod/modprobe */
char *device[MAX_DEVICES];    /* the block device to use */
int erasesz[MAX_DEVICES];     /* optional default erase size */
int ro[MAX_DEVICES];          /* optional read only flag */
int wqs;         /* optionally set the write queue size */


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Evans <spse@secret.org.uk>");
MODULE_DESCRIPTION("Emulate an MTD using a block device");
MODULE_PARM(device, "1-4s");
MODULE_PARM_DESC(device, "block device to use");
MODULE_PARM(erasesz, "1-4i");
MODULE_PARM_DESC(erasesz, "optional erase size to use in KB. eg 4=4K.");
MODULE_PARM(ro, "1-4i");
MODULE_PARM_DESC(ro, "1=Read only, writes and erases cause errors");
MODULE_PARM(wqs, "i");


/* Page cache stuff */

/* readpage() - reads one page from the block device */                 
static int blkmtd_readpage(struct blkmtd_dev *dev, struct page *page)
{  
	int err;
	struct kiobuf *iobuf;
	kdev_t kdev;
	unsigned long *blocks;

	if(!dev) {
		err("readpage: PANIC dev == NULL");
		return -EIO;
	}
	kdev = to_kdev_t(dev->binding->bd_dev);

	DEBUG(2, "blkmtd: readpage called, dev = `%s' page = %p index = %ld\n",
	      bdevname(kdev), page, page->index);

	if(Page_Uptodate(page)) {
		DEBUG(2, "blkmtd: readpage page %ld is already upto date\n",
		      page->index);
		unlock_page(page);
		return 0;
	}

	ClearPageUptodate(page);
	ClearPageError(page);

	/* see if page is in the outgoing write queue */
	spin_lock(&mbd_writeq_lock);
	if(write_queue_cnt) {
		int i = write_queue_tail;
		while(i != write_queue_head) {
			struct blkmtd_wq *item = &write_queue[i];
			if((item->dev == dev)
			   && (page->index >= item->pagenr)
			   && (page->index < item->pagenr+item->pagecnt)) {
				/* yes it is */
				int index = page->index - item->pagenr;
		
				DEBUG(2, "blkmtd: readpage: found page %ld in outgoing write queue\n",
				      page->index);
				if(item->iserase) {
					memset(page_address(page), 0xff, PAGE_SIZE);
				} else {
					memcpy(page_address(page), page_address(item->pages[index]), PAGE_SIZE);
				}
				SetPageUptodate(page);
				flush_dcache_page(page);
				unlock_page(page);
				spin_unlock(&mbd_writeq_lock);
				return 0;
			}
			i++;
			i %= write_queue_sz;
		}
	}
	spin_unlock(&mbd_writeq_lock);


	DEBUG(3, "blkmtd: readpage: getting kiovec\n");
	err = alloc_kiovec(1, &iobuf);
	if (err) {
		crit("cant allocate kiobuf");
		SetPageError(page);
		return err;
	}

	/* Pre 2.4.4 doesn't have space for the block list in the kiobuf */ 
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
	blocks = kmalloc(KIO_MAX_SECTORS * sizeof(unsigned long), GFP_KERNEL);
	if(blocks == NULL) {
		crit("cant allocate iobuf blocks");
		free_kiovec(1, &iobuf);
		SetPageError(page);
		return -ENOMEM;
	}
#else 
	blocks = iobuf->blocks;
#endif

	iobuf->offset = 0;
	iobuf->nr_pages = 1;
	iobuf->length = PAGE_SIZE;
	iobuf->locked = 1;
	iobuf->maplist[0] = page;
	blocks[0] = page->index;

	DEBUG(3, "bklmtd: readpage: starting brw_kiovec\n");
	err = brw_kiovec(READ, 1, &iobuf, kdev, blocks, PAGE_SIZE);
	DEBUG(3, "blkmtd: readpage: finished, err = %d\n", err);
	iobuf->locked = 0;
	free_kiovec(1, &iobuf);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
	kfree(blocks);
#endif
	if(err != PAGE_SIZE) {
		err("readpage: error reading page %ld", page->index);
		memset(page_address(page), 0, PAGE_SIZE);
		SetPageError(page);
		err = -EIO;
	} else {
		DEBUG(3, "blkmtd: readpage: setting page upto date\n");
		SetPageUptodate(page);
		err = 0;
	}
	flush_dcache_page(page);
	unlock_page(page);
	DEBUG(2, "blkmtd: readpage: finished, err = %d\n", err);
	return 0;
}

                    
/* This is the kernel thread that empties the write queue to disk */
static int write_queue_task(void *data)
{
	int err;
	struct task_struct *tsk = current;
	struct kiobuf *iobuf;
	unsigned long *blocks;

	DECLARE_WAITQUEUE(wait, tsk);
	DEBUG(1, "blkmtd: writetask: starting (pid = %d)\n", tsk->pid);
	daemonize();
	strcpy(tsk->comm, "blkmtdd");
	tsk->tty = NULL;
	spin_lock_irq(&tsk->sigmask_lock);
	sigfillset(&tsk->blocked);
	recalc_sigpending();
	spin_unlock_irq(&tsk->sigmask_lock);
	exit_sighand(tsk);

	if(alloc_kiovec(1, &iobuf)) {
		crit("write_queue_task cant allocate kiobuf");
		return 0;
	}

	/* Pre 2.4.4 doesn't have space for the block list in the kiobuf */ 
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
	blocks = kmalloc(KIO_MAX_SECTORS * sizeof(unsigned long), GFP_KERNEL);
	if(blocks == NULL) {
		crit("write_queue_task cant allocate iobuf blocks");
		free_kiovec(1, &iobuf);
		return 0;
	}
#else 
	blocks = iobuf->blocks;
#endif

	DEBUG(2, "blkmtd: writetask: entering main loop\n");
	add_wait_queue(&thr_wq, &wait);

	while(1) {
		spin_lock(&mbd_writeq_lock);

		if(!write_queue_cnt) {
			/* If nothing in the queue, wake up anyone wanting 
			 * to know when there is space in the queue then
			 * sleep for 2*HZ
			 */
			spin_unlock(&mbd_writeq_lock);
			DEBUG(4, "blkmtd: writetask: queue empty\n");
			if(waitqueue_active(&mtbd_sync_wq))
				wake_up(&mtbd_sync_wq);
			interruptible_sleep_on_timeout(&thr_wq, 2*HZ);
			DEBUG(4, "blkmtd: writetask: woken up\n");
			if(write_task_finish)
				break;
		} else {
			/* we have stuff to write */
			struct blkmtd_wq *item = &write_queue[write_queue_tail];
			struct page **pages = item->pages;

			int i;
			int sectornr = item->pagenr;
			int sectorcnt = item->pagecnt;
			int max_sectors = KIO_STATIC_PAGES;
			kdev_t kdev = to_kdev_t(item->dev->binding->bd_dev);

			DEBUG(3, "blkmtd: writetask: got %d queue items\n",
			      write_queue_cnt);
			set_current_state(TASK_RUNNING);
			spin_unlock(&mbd_writeq_lock);

			DEBUG(2, "blkmtd: writetask: writing pagenr = %d pagecnt = %d\n", 
			      item->pagenr, item->pagecnt);

			iobuf->offset = 0;
			iobuf->locked = 1;

			/* Loop through all the pages to be written in the
			 * queue item, remembering
			 * we can only write KIO_MAX_SECTORS at a time
			 */
			while(sectorcnt) {
				int cursectors = (sectorcnt < max_sectors) ? sectorcnt : max_sectors;
				int cpagecnt = (cursectors << PAGE_SHIFT) + PAGE_SIZE-1;
				cpagecnt >>= PAGE_SHIFT;
	
				for(i = 0; i < cpagecnt; i++) {
					if(item->iserase) {
						iobuf->maplist[i] = erase_page;
					} else {
						iobuf->maplist[i] = *(pages++);
					}
				}
	
				for(i = 0; i < cursectors; i++) {
					blocks[i] = sectornr++;
				}
	
				iobuf->nr_pages = cpagecnt;
				iobuf->length = iobuf->nr_pages << PAGE_SHIFT;
				DEBUG(3, "blkmtd: write_task: about to kiovec\n");
				err = brw_kiovec(WRITE, 1, &iobuf, kdev, blocks, PAGE_SIZE);
				DEBUG(3, "bklmtd: write_task: done, err = %d\n", err);
				if(err != (cursectors << PAGE_SHIFT)) {
					/* if an error occured - set this to
					 * exit the loop
					 */
					sectorcnt = 0;
				} else {
					sectorcnt -= cursectors;
				}
			}

			/* free up the pages used in the write and
			 * list of pages used in the write
			 * queue item
			 */
			iobuf->locked = 0;
			spin_lock(&mbd_writeq_lock);
			write_queue_cnt--;
			write_queue_tail++;
			write_queue_tail %= write_queue_sz;
			if(!item->iserase) {
				for(i = 0 ; i < item->pagecnt; i++) {
					unlock_page(item->pages[i]);
					__free_pages(item->pages[i], 0);
				}
				kfree(item->pages);
			}
			item->pages = NULL;
			spin_unlock(&mbd_writeq_lock);
			/* Tell others there is some space in the write queue */
			if(waitqueue_active(&mtbd_sync_wq))
				wake_up(&mtbd_sync_wq);
		}
	}
	remove_wait_queue(&thr_wq, &wait);
	DEBUG(1, "blkmtd: writetask: exiting\n");
	free_kiovec(1, &iobuf);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)
	kfree(blocks);
#endif

	/* Tell people we have exitd */
	up(&thread_sem);
	return 0;
}


/* Add a range of pages into the outgoing write queue, making copies of them */
static int queue_page_write(struct blkmtd_dev *dev, struct page **pages,
			    int pagenr, int pagecnt, int iserase)
{
	struct page *outpage;
	struct page **new_pages = NULL;
	struct blkmtd_wq *item;
	int i;
	DECLARE_WAITQUEUE(wait, current);
	DEBUG(2, "blkmtd: queue_page_write: adding pagenr = %d pagecnt = %d\n",
	      pagenr, pagecnt);

	if(!pagecnt)
		return 0;

	if(pages == NULL && !iserase)
		return -EINVAL;

	/* create a array for the list of pages */
	if(!iserase) {
		new_pages = kmalloc(pagecnt * sizeof(struct page *), GFP_KERNEL);
		if(new_pages == NULL)
			return -ENOMEM;

		/* make copies of the pages in the page cache */
		for(i = 0; i < pagecnt; i++) {
			outpage = alloc_pages(GFP_KERNEL, 0);
			if(!outpage) {
				while(i--) {
					unlock_page(new_pages[i]);
					__free_pages(new_pages[i], 0);
				}
				kfree(new_pages);
				return -ENOMEM;
			}
			lock_page(outpage);
			memcpy(page_address(outpage), page_address(pages[i]), PAGE_SIZE);
			new_pages[i] = outpage;
		}
	}

	/* wait until there is some space in the write queue */
 test_lock:
	spin_lock(&mbd_writeq_lock);
	if(write_queue_cnt == write_queue_sz) {
		spin_unlock(&mbd_writeq_lock);
		DEBUG(3, "blkmtd: queue_page: Queue full\n");
		current->state = TASK_UNINTERRUPTIBLE;
		add_wait_queue(&mtbd_sync_wq, &wait);
		wake_up_interruptible(&thr_wq);
		schedule();
		current->state = TASK_RUNNING;
		remove_wait_queue(&mtbd_sync_wq, &wait);
		DEBUG(3, "blkmtd: queue_page_write: Queue has %d items in it\n",
		      write_queue_cnt);
		goto test_lock;
	}

	DEBUG(3, "blkmtd: queue_page_write: qhead: %d qtail: %d qcnt: %d\n", 
	      write_queue_head, write_queue_tail, write_queue_cnt);

	/* fix up the queue item */
	item = &write_queue[write_queue_head];
	item->pages = new_pages;
	item->pagenr = pagenr;
	item->pagecnt = pagecnt;
	item->dev = dev;
	item->iserase = iserase;

	write_queue_head++;
	write_queue_head %= write_queue_sz;
	write_queue_cnt++;
	DEBUG(3, "blkmtd: queue_page_write: qhead: %d qtail: %d qcnt: %d\n", 
	      write_queue_head, write_queue_tail, write_queue_cnt);
	spin_unlock(&mbd_writeq_lock);
	DEBUG(2, "blkmtd: queue_page_write: finished\n");
	return 0;
}


/* erase a specified part of the device */
static int blkmtd_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct blkmtd_dev *dev = mtd->priv;
	struct mtd_erase_region_info *einfo = mtd->eraseregions;
	int numregions = mtd->numeraseregions;
	size_t from;
	u_long len;
	int err = 0;

	/* check readonly */
	if(dev->readonly) {
		err("error: mtd%d trying to erase readonly device %s",
		    mtd->index, mtd->name);
		instr->state = MTD_ERASE_FAILED;
		goto erase_callback;
	}

	instr->state = MTD_ERASING;
	from = instr->addr;
	len = instr->len;

	/* check erase region has valid start and length */
	DEBUG(2, "blkmtd: erase: dev = `%s' from = 0x%x len = 0x%lx\n",
	      bdevname(dev->binding->bd_dev), from, len);
	while(numregions) {
		DEBUG(3, "blkmtd: checking erase region = 0x%08X size = 0x%X num = 0x%x\n",
		      einfo->offset, einfo->erasesize, einfo->numblocks);
		if(from >= einfo->offset 
		   && from < einfo->offset + (einfo->erasesize * einfo->numblocks)) {
			if(len == einfo->erasesize 
			   && ( (from - einfo->offset) % einfo->erasesize == 0))
				break;
		}
		numregions--;
		einfo++;
	}

	if(!numregions) {
		/* Not a valid erase block */
		err("erase: invalid erase request 0x%lX @ 0x%08X", len, from);
		instr->state = MTD_ERASE_FAILED;
		err = -EIO;
	}
  
	if(instr->state != MTD_ERASE_FAILED) {
		/* start the erase */
		int pagenr, pagecnt;
		struct page *page, **pages;
		int i = 0;

		/* Handle the last page of the device not being whole */
		if(len < PAGE_SIZE)
			len = PAGE_SIZE;

		pagenr = from >> PAGE_SHIFT;
		pagecnt = len >> PAGE_SHIFT;
		DEBUG(3, "blkmtd: erase: pagenr = %d pagecnt = %d\n",
		      pagenr, pagecnt);

		pages = kmalloc(pagecnt * sizeof(struct page *), GFP_KERNEL);
		if(pages == NULL) {
			err = -ENOMEM;
			instr->state = MTD_ERASE_FAILED;
			goto erase_out;
		}


		while(pagecnt) {
			/* get the page via the page cache */
			DEBUG(3, "blkmtd: erase: doing grab_cache_page() for page %d\n", pagenr);
			page = grab_cache_page(dev->binding->bd_inode->i_mapping, pagenr);
			if(!page) {
				DEBUG(3, "blkmtd: erase: grab_cache_page() failed for page %d\n",
				      pagenr);
				kfree(pages);
				err = -EIO;
				instr->state = MTD_ERASE_FAILED;
				goto erase_out;
			}
			memset(page_address(page), 0xff, PAGE_SIZE);
			pages[i] = page;
			pagecnt--;
			pagenr++;
			i++;
		}
		DEBUG(3, "blkmtd: erase: queuing page write\n");
		err = queue_page_write(dev, NULL, from >> PAGE_SHIFT,
				       len >> PAGE_SHIFT, 1);
		pagecnt = len >> PAGE_SHIFT;
		if(!err) {
			while(pagecnt--) {
				SetPageUptodate(pages[pagecnt]);
				unlock_page(pages[pagecnt]);
				page_cache_release(pages[pagecnt]);
				flush_dcache_page(pages[pagecnt]);
			}
			kfree(pages);
			instr->state = MTD_ERASE_DONE;
		} else {
			while(pagecnt--) {
				SetPageError(pages[pagecnt]);
				page_cache_release(pages[pagecnt]);
			}
			kfree(pages);
			instr->state = MTD_ERASE_FAILED;
		}
	}
 erase_out:
	DEBUG(3, "blkmtd: erase: checking callback\n");
 erase_callback:
	if (instr->callback) {
		(*(instr->callback))(instr);
	}
	DEBUG(2, "blkmtd: erase: finished (err = %d)\n", err);
	return err;
}


/* read a range of the data via the page cache */
static int blkmtd_read(struct mtd_info *mtd, loff_t from, size_t len,
		       size_t *retlen, u_char *buf)
{
	struct blkmtd_dev *dev = mtd->priv;
	int err = 0;
	int offset;
	int pagenr, pages;

	*retlen = 0;

	DEBUG(2, "blkmtd: read: dev = `%s' from = %ld len = %d buf = %p\n",
	      bdevname(dev->binding->bd_dev), (long int)from, len, buf);

	pagenr = from >> PAGE_SHIFT;
	offset = from - (pagenr << PAGE_SHIFT);
  
	pages = (offset+len+PAGE_SIZE-1) >> PAGE_SHIFT;
	DEBUG(3, "blkmtd: read: pagenr = %d offset = %d, pages = %d\n",
	      pagenr, offset, pages);

	/* just loop through each page, getting it via readpage() - slow but easy */
	while(pages) {
		struct page *page;
		int cpylen;
		DEBUG(3, "blkmtd: read: looking for page: %d\n", pagenr);
		page = read_cache_page(dev->binding->bd_inode->i_mapping, pagenr,
				       (filler_t *)blkmtd_readpage, dev);
		if(IS_ERR(page)) {
			return PTR_ERR(page);
		}
		wait_on_page(page);
		if(!Page_Uptodate(page)) {
			/* error reading page */
			err("read: page not uptodate");
			page_cache_release(page);
			return -EIO;
		}

		cpylen = (PAGE_SIZE > len) ? len : PAGE_SIZE;
		if(offset+cpylen > PAGE_SIZE)
			cpylen = PAGE_SIZE-offset;
    
		memcpy(buf + *retlen, page_address(page) + offset, cpylen);
		offset = 0;
		len -= cpylen;
		*retlen += cpylen;
		pagenr++;
		pages--;
		page_cache_release(page);
	}
  
	DEBUG(2, "blkmtd: end read: retlen = %d, err = %d\n", *retlen, err);
	return err;
}

    
/* write a range of the data via the page cache.
 *
 * Basic operation. break the write into three parts. 
 *
 * 1. From a page unaligned start up until the next page boundary
 * 2. Page sized, page aligned blocks
 * 3. From end of last aligned block to end of range
 *
 * 1,3 are read via the page cache and readpage() since these are partial
 * pages, 2 we just grab pages from the page cache, not caring if they are
 * already in memory or not since they will be completly overwritten.
 *
 */
 
static int blkmtd_write(struct mtd_info *mtd, loff_t to, size_t len,
			size_t *retlen, const u_char *buf)
{
	struct blkmtd_dev *dev = mtd->priv;
	int err = 0;
	int offset;
	int pagenr;
	size_t len1 = 0, len2 = 0, len3 = 0;
	struct page **pages;
	int pagecnt = 0;

	*retlen = 0;
	DEBUG(2, "blkmtd: write: dev = `%s' to = %ld len = %d buf = %p\n",
	      bdevname(dev->binding->bd_dev), (long int)to, len, buf);

	/* handle readonly and out of range numbers */

	if(dev->readonly) {
		err("error: trying to write to a readonly device %s", mtd->name);
		return -EROFS;
	}

	if(to >= mtd->size) {
		return -ENOSPC;
	}

	if(to + len > mtd->size) {
		len = (mtd->size - to);
	}

	pagenr = to >> PAGE_SHIFT;
	offset = to - (pagenr << PAGE_SHIFT);

	/* see if we have to do a partial write at the start */
	if(offset) {
		if((offset + len) > PAGE_SIZE) {
			len1 = PAGE_SIZE - offset;
			len -= len1;
		} else {
			len1 = len;
			len = 0;
		}
	}

	/* calculate the length of the other two regions */
	len3 = len & ~PAGE_MASK;
	len -= len3;
	len2 = len;


	if(len1)
		pagecnt++;
	if(len2)
		pagecnt += len2 >> PAGE_SHIFT;
	if(len3)
		pagecnt++;

	DEBUG(3, "blkmtd: write: len1 = %d len2 = %d len3 = %d pagecnt = %d\n",
	      len1, len2, len3, pagecnt);
  
	/* get space for list of pages */
	pages = kmalloc(pagecnt * sizeof(struct page *), GFP_KERNEL);
	if(pages == NULL) {
		return -ENOMEM;
	}
	pagecnt = 0;

	if(len1) {
		/* do partial start region */
		struct page *page;
    
		DEBUG(3, "blkmtd: write: doing partial start, page = %d len = %d offset = %d\n",
		      pagenr, len1, offset);
		page = read_cache_page(dev->binding->bd_inode->i_mapping, pagenr,
				       (filler_t *)blkmtd_readpage, dev);

		if(IS_ERR(page)) {
			kfree(pages);
			return PTR_ERR(page);
		}
		memcpy(page_address(page)+offset, buf, len1);
		pages[pagecnt++] = page;
		buf += len1;
		*retlen = len1;
		err = 0;
		pagenr++;
	}

	/* Now do the main loop to a page aligned, n page sized output */
	if(len2) {
		int pagesc = len2 >> PAGE_SHIFT;
		DEBUG(3, "blkmtd: write: whole pages start = %d, count = %d\n",
		      pagenr, pagesc);
		while(pagesc) {
			struct page *page;

			/* see if page is in the page cache */
			DEBUG(3, "blkmtd: write: grabbing page %d from page cache\n", pagenr);
			page = grab_cache_page(dev->binding->bd_inode->i_mapping, pagenr);
			DEBUG(3, "blkmtd: write: got page %d from page cache\n", pagenr);
			if(!page) {
				warn("write: cant grab cache page %d", pagenr);
				err = -EIO;
				goto write_err;
			}
			memcpy(page_address(page), buf, PAGE_SIZE);
			pages[pagecnt++] = page;
			unlock_page(page);
			SetPageUptodate(page);
			pagenr++;
			pagesc--;
			buf += PAGE_SIZE;
			*retlen += PAGE_SIZE;
		}
	}


	if(len3) {
		/* do the third region */
		struct page *page;
		DEBUG(3, "blkmtd: write: doing partial end, page = %d len = %d\n",
		      pagenr, len3);
		page = read_cache_page(dev->binding->bd_inode->i_mapping, pagenr,
				       (filler_t *)blkmtd_readpage, dev);
		if(IS_ERR(page)) {
			err = PTR_ERR(page);
			goto write_err;
		}
		memcpy(page_address(page), buf, len3);
		DEBUG(3, "blkmtd: write: writing out partial end\n");
		pages[pagecnt++] = page;
		*retlen += len3;
		err = 0;
	}
	DEBUG(2, "blkmtd: write: end, retlen = %d, err = %d\n", *retlen, err);
	/* submit it to the write task */
	err = queue_page_write(dev, pages, to >> PAGE_SHIFT, pagecnt, 0);
	if(!err) {
		while(pagecnt--) {
			SetPageUptodate(pages[pagecnt]);
			flush_dcache_page(pages[pagecnt]);
			page_cache_release(pages[pagecnt]);
		}
		kfree(pages);
		return 0;
	}

 write_err:
	while(--pagecnt) {
		SetPageError(pages[pagecnt]);
		page_cache_release(pages[pagecnt]);
	}
	kfree(pages);
	return err;
}


/* sync the device - wait until the write queue is empty */
static void blkmtd_sync(struct mtd_info *mtd)
{
	DECLARE_WAITQUEUE(wait, current);
	if(!write_queue)
		return;

	DEBUG(2, "blkmtd: sync: called\n");

 stuff_inq:
	spin_lock(&mbd_writeq_lock);
	if(write_queue_cnt) {
		spin_unlock(&mbd_writeq_lock);
		current->state = TASK_UNINTERRUPTIBLE;
		add_wait_queue(&mtbd_sync_wq, &wait);
		DEBUG(3, "blkmtd: sync: waking up task\n");
		wake_up_interruptible(&thr_wq);
		schedule();
		current->state = TASK_RUNNING;
		remove_wait_queue(&mtbd_sync_wq, &wait);
		DEBUG(3, "blkmtd: sync: waking up after write task\n");
		goto stuff_inq;
	}
	spin_unlock(&mbd_writeq_lock);

	DEBUG(2, "blkmtd: sync: finished\n");
}


#ifdef BLKMTD_PROC_DEBUG
/* procfs stuff */
static int blkmtd_proc_read(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
	int i, len, pages = 0, cnt;
	struct list_head *temp1, *temp2;


	MOD_INC_USE_COUNT;

	spin_lock(&mbd_writeq_lock);
	cnt = write_queue_cnt;
	i = write_queue_tail;
	while(cnt) {
		if(!write_queue[i].iserase)
			pages += write_queue[i].pagecnt;
		i++;
		i %= write_queue_sz;
		cnt--;
	}
	spin_unlock(&mbd_writeq_lock);

	len = sprintf(page, "Write queue head: %d\nWrite queue tail: %d\n"
		      "Write queue count: %d\nPages in queue: %d (%dK)\n",
		      write_queue_head, write_queue_tail, write_queue_cnt,
		      pages, pages << (PAGE_SHIFT-10));

	/* Count the size of the page lists */

	len += sprintf(page+len, "dev\tnrpages\tclean\tdirty\tlocked\tlru\n");
	list_for_each_safe(temp1, temp2, &blkmtd_device_list) {
		struct blkmtd_dev *dev = list_entry(temp1,  struct blkmtd_dev,
						    list);
		struct list_head *temp;
		struct page *pagei;

		int clean = 0, dirty = 0, locked = 0, lru = 0;
		/* Count the size of the page lists */
		list_for_each(temp, &dev->binding->bd_inode->i_mapping->clean_pages) {
			pagei = list_entry(temp, struct page, list);
			clean++;
			if(PageLocked(pagei))
				locked++;
			if(PageDirty(pagei))
				dirty++;
			if(PageLRU(pagei))
				lru++;
		}
		list_for_each(temp, &dev->binding->bd_inode->i_mapping->dirty_pages) {
			pagei = list_entry(temp, struct page, list);
			if(PageLocked(pagei))
				locked++;
			if(PageDirty(pagei))
				dirty++;
			if(PageLRU(pagei))
				lru++;
		}
		list_for_each(temp, &dev->binding->bd_inode->i_mapping->locked_pages) {
			pagei = list_entry(temp, struct page, list);
			if(PageLocked(pagei))
				locked++;
			if(PageDirty(pagei))
				dirty++;
			if(PageLRU(pagei))
				lru++;
		}

		len += sprintf(page+len, "mtd%d:\t%ld\t%d\t%d\t%d\t%d\n",
			       dev->mtd_info.index,
			       dev->binding->bd_inode->i_mapping->nrpages,
			       clean, dirty, locked, lru);
	}

	if(len <= count)
		*eof = 1;

	MOD_DEC_USE_COUNT;
	return len;
}
#endif


static void free_device(struct blkmtd_dev *dev)
{
	if(dev) {
		del_mtd_device(&dev->mtd_info);
		info("mtd%d: [%s] removed", dev->mtd_info.index,
		     dev->mtd_info.name + strlen("blkmtd: "));
		if(dev->mtd_info.eraseregions)
			kfree(dev->mtd_info.eraseregions);
		if(dev->mtd_info.name)
			kfree(dev->mtd_info.name);
		
		if(dev->binding) {
			kdev_t kdev = to_kdev_t(dev->binding->bd_dev);
			invalidate_inode_pages(dev->binding->bd_inode);
			set_blocksize(kdev, 1 << 10);
			blkdev_put(dev->binding, BDEV_RAW);
		}
		kfree(dev);
	}
}  


/* For a given size and initial erase size, calculate the number
 * and size of each erase region. Goes round the loop twice,
 * once to find out how many regions, then allocates space,
 * then round the loop again to fill it in.
 */
static struct mtd_erase_region_info *calc_erase_regions(
	size_t erase_size, size_t total_size, int *regions)
{
	struct mtd_erase_region_info *info = NULL;

	DEBUG(2, "calc_erase_regions, es = %d size = %d regions = %d\n",
	      erase_size, total_size, *regions);
	/* Make any user specified erasesize be a power of 2
	   and at least PAGE_SIZE */
	if(erase_size) {
		int es = erase_size;
		erase_size = 1;
		while(es != 1) {
			es >>= 1;
			erase_size <<= 1;
		}
		if(erase_size < PAGE_SIZE)
			erase_size = PAGE_SIZE;
	} else {
		erase_size = CONFIG_MTD_BLKDEV_ERASESIZE;
	}

	*regions = 0;

	do {
		int tot_size = total_size;
		int er_size = erase_size;
		int count = 0, offset = 0, regcnt = 0;

		while(tot_size) {
			count = tot_size / er_size;
			if(count) {
				tot_size = tot_size % er_size;
				if(info) {
					DEBUG(2, "adding to erase info off=%d er=%d cnt=%d\n",
					      offset, er_size, count);
					(info+regcnt)->offset = offset;
					(info+regcnt)->erasesize = er_size;
					(info+regcnt)->numblocks = count;
					(*regions)++;
				}
				regcnt++;
				offset += (count * er_size);
			}
			while(er_size > tot_size)
				er_size >>= 1;
		}
		if(info == NULL) {
			DEBUG(2, "allocating %d bytes\n",
			      regcnt * sizeof(struct mtd_erase_region_info));
			info = kmalloc(regcnt * sizeof(struct mtd_erase_region_info), GFP_KERNEL);
			DEBUG(2, "calc_erase info = %p regcnt = %d\n", info, regcnt);
			if(!info)
				break;
		}
	} while(!(*regions));
	DEBUG(2, "calc_erase_regions done, es = %d size = %d regions = %d\n",
	      erase_size, total_size, *regions);
	return info;
}


extern kdev_t name_to_kdev_t(char *line) __init;


static struct blkmtd_dev *add_device(char *devname, int readonly, int erase_size)
{
	int maj, min;
	kdev_t kdev;
	int mode;
	struct blkmtd_dev *dev;

#ifdef MODULE
	struct file *file = NULL;
	struct inode *inode;
#endif

	if(!devname)
		return NULL;

	/* Get a handle on the device */
	mode = (readonly) ? O_RDONLY : O_RDWR;

#ifdef MODULE

	file = filp_open(devname, mode, 0);
	if(IS_ERR(file)) {
		err("error: cant open device %s", devname);
		DEBUG(2, "blkmtd: filp_open returned %ld\n", PTR_ERR(file));
		return NULL;
	}
  
	/* determine is this is a block device and
	 * if so get its major and minor numbers
	 */
	inode = file->f_dentry->d_inode;
	if(!S_ISBLK(inode->i_mode)) {
		err("%s not a block device", devname);
		filp_close(file, NULL);
		return NULL;
	}
	kdev = inode->i_rdev;
	filp_close(file, NULL);
#else
	kdev = name_to_kdev_t(devname);
#endif	/* MODULE */

	if(!kdev) {
		err("bad block device: `%s'", devname);
		return NULL;
	}

	maj = MAJOR(kdev);
	min = MINOR(kdev);
	DEBUG(1, "blkmtd: found a block device major = %d, minor = %d\n",
	      maj, min);

	if(maj == MTD_BLOCK_MAJOR) {
		err("attempting to use an MTD device as a block device");
		return NULL;
	}

	DEBUG(1, "blkmtd: devname = %s\n", bdevname(kdev));

	dev = kmalloc(sizeof(struct blkmtd_dev), GFP_KERNEL);
	if(dev == NULL)
		return NULL;

	memset(dev, 0, sizeof(struct blkmtd_dev));
	/* get the block device */
	dev->binding = bdget(kdev_t_to_nr(MKDEV(maj, min)));
	if(blkdev_get(dev->binding, mode, 0, BDEV_RAW))
		goto devinit_err;

	if(set_blocksize(kdev, PAGE_SIZE)) {
		err("cant set block size to PAGE_SIZE on %s", bdevname(kdev));
		goto devinit_err;
	}

	dev->mtd_info.size = dev->binding->bd_inode->i_size & PAGE_MASK;
	DEBUG(1, "blkmtd: size = %ld\n", (long int)dev->mtd_info.size);
	dev->readonly = readonly;

	/* Setup the MTD structure */
	/* make the name contain the block device in */
	dev->mtd_info.name = kmalloc(sizeof("blkmtd: ") + strlen(devname), GFP_KERNEL);
	if(dev->mtd_info.name == NULL)
		goto devinit_err;

	sprintf(dev->mtd_info.name, "blkmtd: %s", devname);
	dev->mtd_info.eraseregions = calc_erase_regions(erase_size, dev->mtd_info.size,
							&dev->mtd_info.numeraseregions);
	if(dev->mtd_info.eraseregions == NULL)
		goto devinit_err;

	dev->mtd_info.erasesize = dev->mtd_info.eraseregions->erasesize;
	DEBUG(1, "blkmtd: init: found %d erase regions\n",
	      dev->mtd_info.numeraseregions);

	if(readonly) {
		dev->mtd_info.type = MTD_ROM;
		dev->mtd_info.flags = MTD_CAP_ROM;
	} else {
		dev->mtd_info.type = MTD_RAM;
		dev->mtd_info.flags = MTD_CAP_RAM;
	}
	dev->mtd_info.erase = blkmtd_erase;
	dev->mtd_info.read = blkmtd_read;
	dev->mtd_info.write = blkmtd_write;
	dev->mtd_info.sync = blkmtd_sync;
	dev->mtd_info.point = 0;
	dev->mtd_info.unpoint = 0;
	dev->mtd_info.priv = dev;
	dev->mtd_info.module = THIS_MODULE;

	list_add(&dev->list, &blkmtd_device_list);
	if (add_mtd_device(&dev->mtd_info)) {
		/* Device didnt get added, so free the entry */
		list_del(&dev->list);
		free_device(dev);
		return NULL;
	} else {
		info("mtd%d: [%s] erase_size = %dK %s",
		     dev->mtd_info.index, dev->mtd_info.name + strlen("blkmtd: "),
		     dev->mtd_info.erasesize >> 10,
		     (dev->readonly) ? "(read-only)" : "");
	}
	
	return dev;

 devinit_err:
	free_device(dev);
	return NULL;
}


/* Cleanup and exit - sync the device and kill of the kernel thread */
static void __devexit cleanup_blkmtd(void)
{
	struct list_head *temp1, *temp2;
#ifdef BLKMTD_PROC_DEBUG
	if(blkmtd_proc) {
		remove_proc_entry("blkmtd_debug", NULL);
	}
#endif

	if (write_queue) {
		/* sync the device */
		blkmtd_sync(NULL);
		write_task_finish = 1;
		wake_up_interruptible(&thr_wq);
		down(&thread_sem);
		kfree(write_queue);
	}


	/* Remove the MTD devices */
	list_for_each_safe(temp1, temp2, &blkmtd_device_list) {
		struct blkmtd_dev *dev = list_entry(temp1, struct blkmtd_dev,
						    list);
		free_device(dev);
	}

	if(erase_page) {
		unlock_page(erase_page);
		__free_pages(erase_page, 0);
	}
}

#ifndef MODULE

/* Handle kernel boot params */


static int __init param_blkmtd_device(char *str)
{
	int i;

	for(i = 0; i < MAX_DEVICES; i++) {
		device[i] = str;
		DEBUG(2, "blkmtd: device setup: %d = %s\n", i, device[i]);
		strsep(&str, ",");
	}
	return 1;
}


static int __init param_blkmtd_erasesz(char *str)
{
	int i;
	for(i = 0; i < MAX_DEVICES; i++) {
		char *val = strsep(&str, ",");
		if(val)
			erasesz[i] = simple_strtoul(val, NULL, 0);
		DEBUG(2, "blkmtd: erasesz setup: %d = %d\n", i, erasesz[i]);
	}

	return 1;
}


static int __init param_blkmtd_ro(char *str)
{
	int i;
	for(i = 0; i < MAX_DEVICES; i++) {
		char *val = strsep(&str, ",");
		if(val)
			ro[i] = simple_strtoul(val, NULL, 0);
		DEBUG(2, "blkmtd: ro setup: %d = %d\n", i, ro[i]);
	}

	return 1;
}


__setup("blkmtd_device=", param_blkmtd_device);
__setup("blkmtd_erasesz=", param_blkmtd_erasesz);
__setup("blkmtd_ro=", param_blkmtd_ro);

#endif


/* Startup */
static int __init init_blkmtd(void)
{
	int err = 0;
	struct blkmtd_dev *dev;
	int i;
	int global_ro = 1;

	/* Check args - device[0] is the bare minimum*/
	if(!device[0]) {
		err("error: missing `device' name\n");
		return -EINVAL;
	}

	if(wqs) {
		if(wqs < 16) 
			wqs = 16;
		if(wqs > 4*WRITE_QUEUE_SZ)
			wqs = 4*WRITE_QUEUE_SZ;
		write_queue_sz = wqs;
	}

	for(i = 0; i < MAX_DEVICES; i++) {
		dev = add_device(device[i], ro[i], erasesz[i] << 10);
		if(dev) {
			if(global_ro && !ro[i])
				global_ro = 0;
		}
	}

	if(list_empty(&blkmtd_device_list))
		goto init_err;

	if(!global_ro) {
		/* Allocate the write queue */
		write_queue = kmalloc(write_queue_sz * sizeof(struct blkmtd_wq),
				      GFP_KERNEL);
		if(!write_queue) {
			err = -ENOMEM;
			goto init_err;
		}
		/* Set up the erase page */
		erase_page = alloc_pages(GFP_KERNEL, 0);
		if(erase_page == NULL) {
			err = -ENOMEM;
			goto init_err;
		}
		memset(page_address(erase_page), 0xff, PAGE_SIZE);
		lock_page(erase_page);

		init_waitqueue_head(&thr_wq);
		init_waitqueue_head(&mtbd_sync_wq);
		DEBUG(3, "blkmtd: init: kernel task @ %p\n", write_queue_task);
		DEBUG(2, "blkmtd: init: starting kernel task\n");
		kernel_thread(write_queue_task, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	}
	info("version " VERSION);

#ifdef BLKMTD_PROC_DEBUG
	/* create proc entry */
	DEBUG(2, "Creating /proc/blkmtd_debug\n");
	blkmtd_proc = create_proc_read_entry("blkmtd_debug", 0444,
					     NULL, blkmtd_proc_read, NULL);
	if(blkmtd_proc == NULL) {
		err("Cant create /proc/blkmtd_debug");
	} else {
		blkmtd_proc->owner = THIS_MODULE;
	}
#endif

	if(!list_empty(&blkmtd_device_list))
		/* Everything is ok if we got here */
		return 0;

	/* There are no devices, so kill the thread and get quit */
	write_task_finish = 1;
	wake_up_interruptible(&thr_wq);
	down(&thread_sem);

 init_err:
	if(write_queue) {
		kfree(write_queue);
		write_queue = NULL;
	}

	if(erase_page) 
		__free_pages(erase_page, 0);
	return -EINVAL;
}

module_init(init_blkmtd);
module_exit(cleanup_blkmtd);
