/*
 * drivers/mtd/nand/katana.c
 *
 * Copyright (C) 2003 Lineo uSolutions, Inc.
 *
 * $Id: katana.c,v 1.1.2.6 2003/04/02 06:20:30 soka Exp $
 *
 * Derived from drivers/mtd/nand/nand.c
 *   Copyright (C) 2000 Steven J. Hill (sjhill@cotw.com)
 *		   2002 Thomas Gleixner (tglx@linutronix.de)
 */

#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <asm/io.h>
#include <asm/arch/irqs.h>


struct katana_nand_info {
    /* DMA */
    u_char* dma_buf;
    dma_addr_t dma_buf_handle;

    /* lock */
    nand_state_t state;
    wait_queue_head_t wq;
    spinlock_t chip_lock;

    unsigned cmd_status;
    int cache_page_nr;
    u_char* data_cache;
};

#define MAX_ECC_RETRY	3

#define ERASE_SIZE	0x4000
#define OOB_BLOCK_SIZE	512
#define OOB_SIZE	16
#define PAGES_PER_BLOCK	(ERASE_SIZE / OOB_BLOCK_SIZE)
#define NAND_PAGE_SHIFT 9
#define NAND_PAGE_SIZE	(OOB_BLOCK_SIZE + OOB_SIZE)
#define DMA_BUF_SIZE	(NAND_PAGE_SIZE * 33)
#define ECC_START_POS	4
#define ECC_SIZE	(OOB_SIZE - ECC_START_POS)

#define READ_REG(reg)	    readl((MQ9000_REGS_VBASE + MQ_REG_PHYS_OFFSET(SBI_BASE)) + (reg))
#define WRITE_REG(val, reg) writel((val), (MQ9000_REGS_VBASE + MQ_REG_PHYS_OFFSET(SBI_BASE)) + (reg))

#define SB14R_VALUE (((NAND_PAGE_SIZE - 1) << 2) | 0) /* DMA src. is SDRAM */
#define SB16R_VALUE (0x191be813) /* HAM is used */

#define SB00R (0x00 * 4)	/* external memory interface enable register */
#define SB14R (0x14 * 4)	/* NAND-flash DMA interface register 1 */
#define SB15R (0x15 * 4)	/* NAND-flash DMA interface register 2 */
#define SB16R (0x16 * 4)	/* NAND-flash control register */
#define SB17R (0x17 * 4)	/* SBI NAND-flash page start address register */
#define SB18R (0x18 * 4)	/* SBI NAND-flash command register */
#define SB19R (0x19 * 4)	/* SBI NAND-flash status register */
#define SB1AR (0x1a * 4)	/* SBI NAND-flash ECC syndrome lower bits register */
#define SB1BR (0x1b * 4)	/* SBI NAND-flash ECC syndrome upper bits register */


/**********************************************************************
 * static variables
 */

static struct mtd_info* katana_nand_mtd = NULL;
static struct mtd_partition* katana_nand_partition_info;
static int katana_nand_nr_partitions;
static struct mtd_info** katana_nand_part_mtdp;

#define DEFAULT_NR_PARTITIONS 2
static struct mtd_partition katana_nand_default_partition_info[] = {
    {
	.name = "NAND flash partition 0",
	.offset = 0,
	.size = 32 * 1024 * 1024,
    },
    {
	.name = "NAND flash partition 1",
	.offset = 32 * 1024 * 1024,
	.size = 32 * 1024 * 1024,
    },
};



/**********************************************************************
 * functions
 */

/*
 * return true if hamming distance between a and b is <= 1.
 *
 * [NOTE]
 *	assume that higher 3 bytes are zero
 */
static inline int
katana_nand_hamming_distance(unsigned int a,
			     unsigned int b)
{
#ifdef __ARM_ARCH_5TE__
    unsigned int n;
    a ^= b;
    asm (
	"clz	%0, %1\n"
	: "=r" (n)
	: "r" (a)
    );
    return (a << (n + 1)) == 0;
#else
    unsigned int n;
    a ^= b;
    for (n = 0; n < 8; n++)
	if (! (a & ~(1 << n)))
	    return 1;
    return 0;
#endif
}


/****
 * This function must call with spin_lock.
 */
static inline void
katana_nand_wait(struct katana_nand_info* this,
		 unsigned long* flags)
{
    DECLARE_WAITQUEUE (wait, current);

    this->cmd_status = 0;
    set_current_state(TASK_UNINTERRUPTIBLE);
    add_wait_queue(&this->wq, &wait);
    spin_unlock_irqrestore(&this->chip_lock, *flags);
    schedule();
    remove_wait_queue(&this->wq, &wait);
}


/****
 * helper function for katana_nand_cmmand()
 */
static void
katana_nand_register_setup(struct katana_nand_info* this,
			   int page_nr,
			   int count,
			   int ecc_enable)
{
    WRITE_REG(~0, SB19R);
    if (count < 0)
	WRITE_REG(SB14R_VALUE, SB14R);
    else
	WRITE_REG((SB14R_VALUE & 0x03) | ((count * NAND_PAGE_SIZE - 1) << 2), SB14R);
    WRITE_REG(this->dma_buf_handle & 0x03ffffff, SB15R);
    if (count < 0)
	WRITE_REG(SB16R_VALUE, SB16R);
    else {
	unsigned val = (SB16R_VALUE & ~0x07) | (ecc_enable ? 3 : 5);
	WRITE_REG((val & ~((PAGES_PER_BLOCK - 1) << 5)) |
		  (((count - 1) & (PAGES_PER_BLOCK - 1)) << 5), SB16R);
    }
    WRITE_REG(page_nr << NAND_PAGE_SHIFT, SB17R);
}


/****
 * if requested command is read, write, or erase, then you must call this
 * function with spin_lock.
 */
static void
katana_nand_command(struct katana_nand_info* this,
		    unsigned command,
		    int page_nr,
		    int count,
		    int ecc_enable)
{
    switch (command) {
    case NAND_CMD_RESET:
	WRITE_REG(READ_REG(SB00R) & ~(1 << 4), SB00R);
	WRITE_REG(READ_REG(SB00R) | (1 << 4), SB00R);
	break;

    case NAND_CMD_ERASE1:
	katana_nand_register_setup(this, page_nr, count, 1);
	WRITE_REG(0x04, SB18R);
	break;

    case NAND_CMD_READ0:
	katana_nand_register_setup(this, page_nr, count, ecc_enable);
	WRITE_REG(0x01, SB18R);
	break;

    case NAND_CMD_PAGEPROG:
	katana_nand_register_setup(this, page_nr, -1, ecc_enable);
	WRITE_REG(0x02, SB18R);
	break;
    }
}


/****
 * Get chip for selected access
 */
static inline void
katana_nand_get_chip(struct katana_nand_info* this,
		     int new_state)
{
    unsigned long flags;
    DECLARE_WAITQUEUE(wait, current);

  retry:
    /* 
     * Grab the lock and see if the device is available For erasing, we keep
     * the spinlock until the erase command is written.
     */
    spin_lock_irqsave(&this->chip_lock, flags);

    if (this->state == FL_READY) {
	this->state = new_state;
	spin_unlock_irqrestore(&this->chip_lock, flags);
	return;
    }

    set_current_state(TASK_UNINTERRUPTIBLE);
    add_wait_queue(&this->wq, &wait);
    spin_unlock_irqrestore(&this->chip_lock, flags);
    schedule();
    remove_wait_queue(&this->wq, &wait);
    goto retry;
}


/****
 * same as katana_nand_read_page(), except that ECC is disable.
 */
static int
katana_nand_read_page_without_ecc(struct katana_nand_info* this,
				  int start_page_nr,
				  int count)
{
    unsigned long flags;

    spin_lock_irqsave(&this->chip_lock, flags);
    katana_nand_command(this, NAND_CMD_READ0, start_page_nr, count, 0);
    katana_nand_wait(this, &flags); /* returned after spin_lock released */

    return count;
}


/****
 * read count pages.
 *
 * ASSERT(start_page_nr % PAGES_PER_BLOCK + count <= PAGES_PER_BLOCK);
 */
static int
katana_nand_read_page(struct katana_nand_info* this,
		      int start_page_nr,
		      int count)
{
    unsigned long flags;
    unsigned long ecc;
    int read_count;

    spin_lock_irqsave(&this->chip_lock, flags);
    katana_nand_command(this, NAND_CMD_READ0, start_page_nr, count, 1);
    katana_nand_wait(this, &flags); /* returned after spin_lock released */

    spin_lock_irqsave(&this->chip_lock, flags);
    ecc = READ_REG(SB1AR) | READ_REG(SB1BR);
    if (count == 1)
	read_count = (ecc == 0) ? 1 : 0;
    else
	read_count = ((this->cmd_status >> 3) % PAGES_PER_BLOCK) + 1;
    spin_unlock_irqrestore(&this->chip_lock, flags);

    return read_count;
}


static int
katana_nand_write_page(struct katana_nand_info* this,
		       int page_nr,
		       int col,
		       int last,
		       u_char* oob_buf,
		       int oobsel)
{
    unsigned long flags;
    int status;
    int ecc_enable = (oobsel != NAND_NONE_OOB);

    if (this->cache_page_nr == page_nr)
	this->cache_page_nr = -1;

    /* pad oob area, if we have no oob buffer from fs-driver */
    if (! oob_buf)
	memset(&this->dma_buf[OOB_BLOCK_SIZE], 0xff, OOB_SIZE);
    else 
	memcpy(&this->dma_buf[OOB_BLOCK_SIZE], oob_buf, OOB_SIZE);

    if (ecc_enable && (col || last != OOB_BLOCK_SIZE))
	ecc_enable = 0;

    /* Prepad & postpad for partial page programming !!! */
    memset(this->dma_buf, 0xff, col);
    memset(&this->dma_buf[last], 0xff, OOB_BLOCK_SIZE - last);

    spin_lock_irqsave(&this->chip_lock, flags);
    katana_nand_command(this, NAND_CMD_PAGEPROG, page_nr, -1, ecc_enable);
    katana_nand_wait(this, &flags); /* returned after spin_lock released */

    spin_lock_irqsave(&this->chip_lock, flags);
    status = this->cmd_status;
    spin_unlock_irqrestore(&this->chip_lock, flags);

    /* See if device thinks it succeeded */
    if ((status & 3) != 1) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: Failed write, page 0x%08x\n",
	      __func__, page_nr);
	return -EIO;
    }
    else
	return 0;
}


/****
 * NAND write out-of-band
 */
static int
katana_nand_write_oob(struct mtd_info* mtd,
		      loff_t to,
		      size_t len,
		      size_t* retlen,
		      const u_char* buf)
{
    struct katana_nand_info* this = mtd->priv;
    unsigned long flags;
    u_char* oob_buf;
    int page_nr, col, ret;

    DEBUG(MTD_DEBUG_LEVEL3, "%s: to = 0x%08x, len = %i\n",
	  __func__, (unsigned)to, (int)len);


    *retlen = 0;
    page_nr = to / OOB_BLOCK_SIZE;
    col = to % OOB_SIZE;

    /* Do not allow write past end of page */
    if (col + len > OOB_SIZE) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: Attempt to write past end of page\n", __func__);
	return -EINVAL;
    }

    oob_buf = kmalloc(OOB_SIZE, GFP_KERNEL);
    if (! oob_buf) {
	printk(KERN_WARNING "%s: Unable to allocate oob_buf space\n", __func__);
	return -ENOMEM;
    }
    memset(oob_buf, 0xff, OOB_SIZE);
    memcpy(&oob_buf[col], buf, len);

    katana_nand_get_chip(this, FL_WRITING);

    ret = katana_nand_write_page(this, page_nr, OOB_BLOCK_SIZE, OOB_BLOCK_SIZE, oob_buf,
				 NAND_NONE_OOB);
    if (ret) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: Failed write, page 0x%08x\n", __func__, page_nr);
	ret = -EIO;
	goto out;
    }

    *retlen = len;

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
    {
	int i;

	for (i = 0; i < MAX_ECC_RETRY; i++)
	    if (katana_nand_read_page(this, page_nr, 1) == 1)
		break;
	if (i >= MAX_ECC_RETRY) {
	    printk(KERN_WARNING "%s: Failed ECC read, page 0x%08x\n", __func__, page_nr);
	    ret = -EIO;
	    goto out;
	}
	if (memcmp(&oob_buf[col], &this->dma_buf[OOB_BLOCK_SIZE + col], len)) {
	    DEBUG(MTD_DEBUG_LEVEL0, "%s: Failed write verify, page 0x%08x\n",
		  __func__, page_nr);
	    ret = -EIO;
	    goto out;
	}
    }
#endif

  out:
    if (ret == -EIO)
	katana_nand_command(this, NAND_CMD_RESET, -1, -1, 0);
    kfree(oob_buf);

    /* Wake up anyone waiting on the device */
    spin_lock_irqsave(&this->chip_lock, flags);
    this->state = FL_READY;
    wake_up(&this->wq);
    spin_unlock_irqrestore(&this->chip_lock, flags);

    return ret;
}


static int
katana_nand_write_ecc(struct mtd_info* mtd,
		      loff_t to,
		      size_t len,
		      size_t* retlen,
		      const u_char* buf,
		      u_char* eccbuf,
		      int oobsel)
{
    struct katana_nand_info* this = mtd->priv;
    unsigned long flags;
    int page_nr, end_page_nr, col;
    int written = 0, ret = 0;
    int (*read_page)(struct katana_nand_info*, int, int) = (oobsel == NAND_NONE_OOB)
	? katana_nand_read_page_without_ecc : katana_nand_read_page;

    DEBUG(MTD_DEBUG_LEVEL3, "%s: to = 0x%08x, len = %i\n",
	  __func__, (unsigned)to, (int)len);

    if (oobsel != NAND_NONE_OOB && oobsel != NAND_JFFS2_OOB) {
	printk("%s: oobsel == %d is not supported.\n", __func__, oobsel);
	*retlen = 0;
	return -EINVAL;
    }

    /* Do not allow write past end of device */
    if (to + len > mtd->size) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: Attempt to write past end of page\n", __func__);
	*retlen = 0;
	return -EINVAL;
    }

    page_nr = to / OOB_BLOCK_SIZE;
    end_page_nr = (to + len + OOB_BLOCK_SIZE - 1) / OOB_BLOCK_SIZE;
    col = to % OOB_BLOCK_SIZE;

    katana_nand_get_chip(this, FL_WRITING);

    while (page_nr < end_page_nr) {
	int i;
	int bytes = min_t(int, OOB_BLOCK_SIZE - col, len - written);

	memcpy(&this->dma_buf[col], buf, bytes);
	if (eccbuf)
	    ret = katana_nand_write_page(this, page_nr, col, col + bytes, eccbuf,
					 oobsel);
	else
	    ret = katana_nand_write_page(this, page_nr, col, col + bytes, NULL, oobsel);
	if (ret)
	    goto out;

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	for (i = 0; i < MAX_ECC_RETRY; i++)
	    if (read_page(this, page_nr, 1) == 1)
		break;
	if (i >= MAX_ECC_RETRY) {
	    printk(KERN_WARNING "%s: Failed ECC read, page 0x%08x\n", __func__, page_nr);
	    ret = -EIO;
	    goto out;
	}

	if (memcmp(&this->dma_buf[col], buf, bytes)) {
	    printk(KERN_WARNING "%s: Failed write verify, at page 0x%08x\n",
		   __func__, page_nr);
	    ret = -EIO;
	    goto out;
	}

	/* copy ecc values into eccbuf */
	if (eccbuf && oobsel != NAND_NONE_OOB)
	    memcpy(&eccbuf[ECC_START_POS],
		   &this->dma_buf[OOB_BLOCK_SIZE + ECC_START_POS], ECC_SIZE);

	if (eccbuf && memcmp(&this->dma_buf[OOB_BLOCK_SIZE], eccbuf, OOB_SIZE)) {
	    printk(KERN_WARNING "%s: Failed write verify, oob of page 0x%08x\n",
		   __func__, page_nr);
	    ret = -EIO;
	    goto out;
	}
#else
	/* copy ecc values into eccbuf */
	if (eccbuf && oobsel != NAND_NONE_OOB) {
	    for (i = 0; i < MAX_ECC_RETRY; i++)
		if (read_page(this, page_nr, 1) == 1)
		    break;
	    if (i >= MAX_ECC_RETRY) {
		printk(KERN_WARNING "%s: Failed ECC read, page 0x%08x\n",
		       __func__, page_nr);
		ret = -EIO;
		goto out;
	    }
	    memcpy(&eccbuf[ECC_START_POS],
		   &this->dma_buf[OOB_BLOCK_SIZE + ECC_START_POS], ECC_SIZE);
	}
#endif

	if (eccbuf)
	    eccbuf += OOB_SIZE;
	buf += bytes;
	written += bytes;
	col = 0;
	page_nr++;
    }

  out:
    if (ret == -EIO)
	katana_nand_command(this, NAND_CMD_RESET, -1, -1, 0);

    /* Wake up anyone waiting on the device */
    spin_lock_irqsave(&this->chip_lock, flags);
    this->state = FL_READY;
    wake_up(&this->wq);
    spin_unlock_irqrestore(&this->chip_lock, flags);

    *retlen = written;
    return ret;
}


static int
katana_nand_write(struct mtd_info* mtd,
		  loff_t to,
		  size_t len,
		  size_t* retlen,
		  const u_char* buf)
{
    return katana_nand_write_ecc(mtd, to, len, retlen, buf, NULL, NAND_JFFS2_OOB);
}


static int
katana_nand_writev_ecc(struct mtd_info* mtd,
		       const struct iovec* vecs,
		       unsigned long count,
		       loff_t to,
		       size_t* retlen,
		       u_char* eccbuf,
		       int oobsel)
{
    struct katana_nand_info* this = mtd->priv;
    unsigned long flags;
    int total_len, page_nr, end_page_nr, col, iov_rest_len;
    u_char* iov_p;
    u_char* dma_p;
    int i;
    int ret = 0, written = 0;
    int (*read_page)(struct katana_nand_info*, int, int) = (oobsel == NAND_NONE_OOB)
	? katana_nand_read_page_without_ecc : katana_nand_read_page;

    /* Calculate total length of data */
    for (i = total_len = 0; i < count; i++)
	total_len += vecs[i].iov_len;

    DEBUG(MTD_DEBUG_LEVEL3, "%s: to = 0x%08x, len = %i, count = %ld\n",
	  __func__, (unsigned)to, (unsigned)total_len, count);

    /* Do not allow write past end of page */
    if (to + total_len > mtd->size) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: Attempted write past end of device\n", __func__);
	return -EINVAL;
    }

    page_nr = to / OOB_BLOCK_SIZE;
    end_page_nr = (to + total_len + OOB_BLOCK_SIZE - 1) / OOB_BLOCK_SIZE;
    col = to % OOB_BLOCK_SIZE;
    dma_p = this->dma_buf + col;
    iov_rest_len = vecs->iov_len;
    iov_p = vecs->iov_base;

    katana_nand_get_chip(this, FL_WRITING);

    while (page_nr < end_page_nr) {
	int last_col = col;
	int bytes;

	while (iov_rest_len <= OOB_BLOCK_SIZE - last_col) {
	    memcpy(dma_p, iov_p, iov_rest_len);
	    last_col += iov_rest_len;
	    dma_p += iov_rest_len;
	    if (--count == 0) {
		bytes = 0;
		goto begin_write;
	    }
	    else {
		vecs++;
		iov_p = vecs->iov_base;
		iov_rest_len = vecs->iov_len;
	    }
	}
	bytes = OOB_BLOCK_SIZE - last_col;
	last_col = OOB_BLOCK_SIZE;
	memcpy(dma_p, iov_p, bytes);

      begin_write:
	if (eccbuf) {
	    ret = katana_nand_write_page(this, page_nr, col, last_col, eccbuf, oobsel);
	    eccbuf += OOB_SIZE;
	}
	else
	    ret = katana_nand_write_page(this, page_nr, col, last_col, NULL, oobsel);
	if (ret)
	    goto out;

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	memcpy(this->data_cache, this->dma_buf, OOB_BLOCK_SIZE);
	for (i = 0; i < MAX_ECC_RETRY; i++)
	    if (read_page(this, page_nr, 1) == 1)
		break;
	if (i >= MAX_ECC_RETRY) {
	    printk(KERN_WARNING "%s: Failed ECC read, page 0x%08x\n", __func__, page_nr);
	    ret = -EIO;
	    goto out;
	}

	if (memcmp(&this->dma_buf[col], this->data_cache, last_col - col)) {
	    printk(KERN_WARNING "%s: Failed write verify, at page 0x%08x\n",
		   __func__, page_nr);
	    ret = -EIO;
	    goto out;
	}

	/* copy ecc values into eccbuf */
	if (eccbuf && oobsel != NAND_NONE_OOB)
	    memcpy(&eccbuf[ECC_START_POS],
		   &this->dma_buf[OOB_BLOCK_SIZE + ECC_START_POS], ECC_SIZE);

	if (eccbuf && memcmp(&this->dma_buf[OOB_BLOCK_SIZE], eccbuf, OOB_SIZE)) {
	    printk(KERN_WARNING "%s: Failed write verify, oob of page 0x%08x\n",
		   __func__, page_nr);
	    ret = -EIO;
	    goto out;
	}
#else
	/* copy ecc values into eccbuf */
	if (eccbuf && oobsel != NAND_NONE_OOB) {
	    for (i = 0; i < MAX_ECC_RETRY; i++)
		if (read_page(this, page_nr, 1) == 1)
		    break;
	    if (i >= MAX_ECC_RETRY) {
		printk(KERN_WARNING "%s: Failed ECC read, page 0x%08x\n",
		       __func__, page_nr);
		ret = -EIO;
		goto out;
	    }
	    memcpy(&eccbuf[ECC_START_POS],
		   &this->dma_buf[OOB_BLOCK_SIZE + ECC_START_POS], ECC_SIZE);
	}
#endif

	written += last_col - col;
	iov_p += bytes;
	iov_rest_len -= bytes;
	dma_p = this->dma_buf;
	col = 0;
	page_nr++;
    }

  out:
    if (ret == -EIO)
	katana_nand_command(this, NAND_CMD_RESET, -1, -1, 0);

    /* Wake up anyone waiting on the device */
    spin_lock_irqsave(&this->chip_lock, flags);
    this->state = FL_READY;
    wake_up(&this->wq);
    spin_unlock_irqrestore(&this->chip_lock, flags);

    *retlen = written;
    return ret;
}


static int
katana_nand_writev(struct mtd_info* mtd,
		   const struct iovec* vecs,
		   unsigned long count,
		   loff_t to,
		   size_t* retlen)
{
    return katana_nand_writev_ecc(mtd, vecs, count, to, retlen, NULL, NAND_JFFS2_OOB);
}


/****
 * NAND read out-of-band
 */
static int
katana_nand_read_oob(struct mtd_info* mtd,
		     loff_t from,
		     size_t len,
		     size_t* retlen,
		     u_char* buf)
{
    struct katana_nand_info* this = mtd->priv;
    unsigned long flags;
    int page_nr, end_page_nr, count, col;
    int read = 0;

    DEBUG(MTD_DEBUG_LEVEL3, "%s: from = 0x%08x, len = %i\n",
	  __func__, (unsigned)from, (int)len);

    /* Do not allow reads past end of device */
    if (from + len > mtd->size) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: Attempt read beyond end of device\n", __func__);
	*retlen = 0;
	return -EINVAL;
    }

    page_nr = from / OOB_BLOCK_SIZE;
    col = from % OOB_SIZE;
    end_page_nr = page_nr + (col + len + OOB_SIZE - 1) / OOB_SIZE;
    count = min(PAGES_PER_BLOCK - page_nr % PAGES_PER_BLOCK, end_page_nr - page_nr);

    katana_nand_get_chip(this, FL_READING);

    while (count > 0) {
	u_char* dma_p;
	int i;

	katana_nand_read_page_without_ecc(this, page_nr, count);
	dma_p = this->dma_buf;
	for (i = 0; i < count; i++) {
	    int bytes = min_t(int, OOB_SIZE - col, len - read);
	    memcpy(buf, dma_p + OOB_BLOCK_SIZE + col, bytes);
	    buf += bytes;
	    read += bytes;
	    dma_p += NAND_PAGE_SIZE;
	    col = 0;
	}

	page_nr += count;
	count = min(PAGES_PER_BLOCK - page_nr % PAGES_PER_BLOCK, end_page_nr - page_nr);
    }

    /* Wake up anyone waiting on the device */
    spin_lock_irqsave(&this->chip_lock, flags);
    this->state = FL_READY;
    wake_up(&this->wq);
    spin_unlock_irqrestore(&this->chip_lock, flags);

    *retlen = read;
    return 0;
}


/****
 * NAND read with ECC
 */
static int
katana_nand_read_ecc(struct mtd_info* mtd,
		     loff_t from,
		     size_t len,
		     size_t* retlen,
		     u_char* buf,
		     u_char* oob_buf,
		     int oobsel)
{
    struct katana_nand_info* this = mtd->priv;
    int (*read_page)(struct katana_nand_info*, int, int);
    unsigned long flags;
    int col, count, page_nr, end_page_nr;
    int read = 0;
    int ecc_failed = 0;
    int retry_count = 0;

    DEBUG(MTD_DEBUG_LEVEL3, "%s: from = 0x%08x, len = %i\n",
	  __func__, (unsigned int)from, (int)len);

    switch (oobsel) {
    case NAND_NONE_OOB:
	read_page = katana_nand_read_page_without_ecc;
	break;

    case NAND_JFFS2_OOB:
	read_page = katana_nand_read_page;
	break;

    default:
	printk("%s: oobsel == %d is not supported.\n", __func__, oobsel);
	*retlen = 0;
	return -EINVAL;
    }

    /* Do not allow reads past end of device */
    if (from + len > mtd->size) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: Attempt read beyond end of device\n", __func__);
	*retlen = 0;
	return -EINVAL;
    }

    katana_nand_get_chip(this, FL_READING);

    page_nr = from / OOB_BLOCK_SIZE;
    end_page_nr = (from + len + OOB_BLOCK_SIZE - 1) / OOB_BLOCK_SIZE;
    count = min(PAGES_PER_BLOCK - page_nr % PAGES_PER_BLOCK, end_page_nr - page_nr);
    col = from % OOB_BLOCK_SIZE;

    while (count > 0) {
	int read_count;
	int i;
	u_char* dma_p;

	read_count = read_page(this, page_nr, count);

      ecc_retry:
	dma_p = this->dma_buf;
	for (i = 0; i < read_count; i++) {
	    int bytes =  min_t(int, OOB_BLOCK_SIZE - col, len - read);
	    memcpy(buf, dma_p + col, bytes);
	    if (oob_buf) {
		memcpy(oob_buf, dma_p + OOB_BLOCK_SIZE, OOB_SIZE);
		oob_buf += OOB_SIZE;
	    }

	    buf += bytes;
	    read += bytes;
	    dma_p += NAND_PAGE_SIZE;
	    col = 0;
	}

	page_nr += read_count;
	if (read_count == count) {
	    retry_count = 0;
	    count = min(PAGES_PER_BLOCK - page_nr % PAGES_PER_BLOCK,
			end_page_nr - page_nr);
	}
	else {
	    printk(KERN_WARNING "%s: Failed ECC read, page 0x%08x\n", __func__, page_nr);
	    ecc_failed++;
	    count -= read_count;
	    if (++retry_count >= MAX_ECC_RETRY) {
		read_count = katana_nand_read_page_without_ecc(this, page_nr, count);
		retry_count = 0;
		goto ecc_retry;
	    }
	}
    }

    if (ecc_failed)
	katana_nand_command(this, NAND_CMD_RESET, -1, -1, 0);

    /* Wake up anyone waiting on the device */
    spin_lock_irqsave(&this->chip_lock, flags);
    this->state = FL_READY;
    wake_up(&this->wq);
    spin_unlock_irqrestore(&this->chip_lock, flags);

    /*
     * Return success, if no ECC failures, else -EIO fs driver will take
     * care of that, because retlen == desired len and result == -EIO
     */
    *retlen = read;
    return ecc_failed ? -EIO : 0;
}


static int
katana_nand_read(struct mtd_info* mtd,
		 loff_t from,
		 size_t len,
		 size_t* retlen,
		 u_char* buf)
{
    return katana_nand_read_ecc(mtd, from, len, retlen, buf, NULL, NAND_JFFS2_OOB);
}


static int
katana_nand_erase(struct mtd_info* mtd,
		  struct erase_info* instr)
{
    struct katana_nand_info* this = mtd->priv;
    unsigned long flags;
    int page_nr;
    int len;
    int ret;

    /* Start address must align on block boundary */
    if (instr->addr & (mtd->erasesize - 1)) {
	DEBUG(MTD_DEBUG_LEVEL0, "nand_erase: Unaligned address\n");
	return -EINVAL;
    }

    /* Length must align on block boundary */
    if (instr->len & (mtd->erasesize - 1)) {
	DEBUG(MTD_DEBUG_LEVEL0, "nand_erase: Length not block aligned\n");
	return -EINVAL;
    }

    /* Do not allow erase past end of device */
    if ((instr->len + instr->addr) > mtd->size) {
	DEBUG (MTD_DEBUG_LEVEL0, "nand_erase: Erase past end of device\n");
	return -EINVAL;
    }

    katana_nand_get_chip(this, FL_ERASING);

    page_nr = instr->addr >> NAND_PAGE_SHIFT;
    len = instr->len;
    instr->state = MTD_ERASING;

    while (len) {
#ifdef CONFIG_MTD_NAND_ERASE_BY_FORCE
	if (instr->by_force) {
	    spin_lock_irqsave(&this->chip_lock, flags);
	    goto skip_badblock_check;
	}
#endif

	katana_nand_read_page_without_ecc(this, page_nr, 1);

	spin_lock_irqsave(&this->chip_lock, flags);
	if (! katana_nand_hamming_distance(this->dma_buf[OOB_BLOCK_SIZE +
							 NAND_BADBLOCK_POS], 0xff)) {
	    printk(KERN_WARNING "%s: attempt to erase a bad block at addr 0x%08x\n",
		   __func__, page_nr << NAND_PAGE_SHIFT);
	    instr->state = MTD_ERASE_FAILED;
	    goto erase_exit;
	}
#ifdef CONFIG_MTD_NAND_POST_BADBLOCK
	if (! katana_nand_hamming_distance(this->dma_buf[OOB_BLOCK_SIZE +
							 NAND_POSTBADBLOCK_POS], 0xff)) {
	    printk(KERN_WARNING "%s: attempt to erase a post bad block at addr 0x%08x\n",
		   __func__, page_nr << NAND_PAGE_SHIFT);
	    instr->state = MTD_ERASE_FAILED;
	    goto erase_exit;
	}
#endif

#ifdef CONFIG_MTD_NAND_ERASE_BY_FORCE
      skip_badblock_check:
#endif

	katana_nand_command(this, NAND_CMD_ERASE1, page_nr, -1, 0);
	katana_nand_wait(this, &flags);	/* returned after spin_lock released */

	spin_lock_irqsave(&this->chip_lock, flags);
	if ((this->cmd_status & 0x03) != 1) {
	    DEBUG(MTD_DEBUG_LEVEL0, "%s: Failed erase, page 0x%08x\n",
		  __func__, page_nr);
	    instr->state = MTD_ERASE_FAILED;
	    goto erase_exit;
	}

	/* Invalidate cache, if last_page is inside erase-block */
	if (this->cache_page_nr >= page_nr &&
	    this->cache_page_nr < page_nr + PAGES_PER_BLOCK)
	    this->cache_page_nr = -1;

	spin_unlock_irqrestore(&this->chip_lock, flags);

	len -= ERASE_SIZE;
	page_nr += PAGES_PER_BLOCK;
    }
    instr->state = MTD_ERASE_DONE;
    spin_lock_irqsave(&this->chip_lock, flags);

  erase_exit:
    ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;
    if (ret == -EIO)
	katana_nand_command(this, NAND_CMD_RESET, -1, -1, 0);

    spin_unlock_irqrestore(&this->chip_lock, flags);

    /* Do call back function */
    if (! ret && instr->callback)
	instr->callback (instr);

    /* The device is ready */
    spin_lock_irqsave(&this->chip_lock, flags);
    this->state = FL_READY;
    wake_up(&this->wq);
    spin_unlock_irqrestore(&this->chip_lock, flags);

    return ret;
}


static void
katana_nand_sync(struct mtd_info* mtd)
{
    struct katana_nand_info* this = mtd->priv;
    unsigned long flags;
    DECLARE_WAITQUEUE(wait, current);

  retry:
    spin_lock_irqsave(&this->chip_lock, flags);

    switch (this->state) {
    case FL_READY:
    case FL_SYNCING:
	this->state = FL_SYNCING;
	spin_unlock_irqrestore(&this->chip_lock, flags);
	break;

    default:
	/* Not an idle state */
	add_wait_queue(&this->wq, &wait);
	spin_unlock_irqrestore(&this->chip_lock, flags);
	schedule();

	remove_wait_queue (&this->wq, &wait);
	goto retry;
    }

    spin_lock_irqsave(&this->chip_lock, flags);

    if (this->state == FL_SYNCING) {
	this->state = FL_READY;
	wake_up(&this->wq);
    }

    spin_unlock_irqrestore(&this->chip_lock, flags);
}


static void
katana_nand_interrupt(int irq,
		      void* id,
		      struct pt_regs* regs)
{
    struct katana_nand_info* this = id;

    spin_lock(&this->chip_lock);

    this->cmd_status = READ_REG(SB19R);
    wake_up(&this->wq);

    spin_unlock(&this->chip_lock);
}


static void
katana_nand_mtd_info_init(struct mtd_info* mtd)
{
    mtd->name = "NAND 64MB 3,3V";
    mtd->erasesize = ERASE_SIZE;
    mtd->size = 1 << 26;
    mtd->eccsize = OOB_BLOCK_SIZE;
    mtd->oobblock = OOB_BLOCK_SIZE;
    mtd->oobsize = OOB_SIZE;

    mtd->type = MTD_NANDFLASH;
    mtd->flags = MTD_CAP_NANDFLASH | MTD_ECC;
    mtd->module = THIS_MODULE;
    mtd->ecctype = MTD_ECC_SW;
    mtd->erase = katana_nand_erase;
    mtd->point = NULL;
    mtd->unpoint = NULL;
    mtd->read = katana_nand_read;
    mtd->write = katana_nand_write;
    mtd->read_ecc = katana_nand_read_ecc;
    mtd->write_ecc = katana_nand_write_ecc;
    mtd->read_oob = katana_nand_read_oob;
    mtd->write_oob = katana_nand_write_oob;
    mtd->readv = NULL;
    mtd->writev = katana_nand_writev;
    mtd->writev_ecc = katana_nand_writev_ecc;
    mtd->sync = katana_nand_sync;
    mtd->lock = NULL;
    mtd->suspend = NULL;
    mtd->resume = NULL;
}


static void
katana_nand_info_init(struct katana_nand_info* this)
{
    this->state = FL_READY;
    init_waitqueue_head(&this->wq);
    spin_lock_init(&this->chip_lock);
    this->cmd_status = 0;
    this->cache_page_nr = -1;
}


/*
 * Main initialization routine
 */
static int __init
katana_nand_init(void)
{
    extern int parse_cmdline_partitions(struct mtd_info *master,
					struct mtd_partition **pparts,
					const char *mtd_id);
    struct katana_nand_info* this;
    int i;

    katana_nand_mtd =
	kmalloc(sizeof (struct mtd_info) + sizeof (struct katana_nand_info), GFP_KERNEL);
    if (! katana_nand_mtd) {
	printk("Unable to allocate Katana NAND MTD device structure.\n");
	return -ENOMEM;
    }
    this = (struct katana_nand_info*)&katana_nand_mtd[1];

    memset(katana_nand_mtd, 0, sizeof (struct mtd_info));
    memset(this, 0, sizeof (struct katana_nand_info));
    katana_nand_mtd->priv = this;

    katana_nand_mtd_info_init(katana_nand_mtd);
    katana_nand_info_init(this);

    i = request_irq(IRQNO_NAND, katana_nand_interrupt, 0, "NAND DMA", this);
    if (i) {
	printk("Unable to request irq for Katana MTD/NAND driver.\n");
	kfree(katana_nand_mtd);
	return -i;
    }

    this->dma_buf = consistent_alloc(GFP_KERNEL | GFP_DMA, DMA_BUF_SIZE,
				     &this->dma_buf_handle);
    if (! this->dma_buf) {
	printk("Unable to allocate NAND-DMA buffer for Katana MTD/NAND driver.\n");
	kfree(katana_nand_mtd);
	return -ENOMEM;
    }

    this->data_cache = kmalloc(OOB_BLOCK_SIZE, GFP_KERNEL);
    if (! this->data_cache) {
	printk("Unable to allocate NAND cache for Katana MTD/NAND driver.\n");
	consistent_free(this->dma_buf, DMA_BUF_SIZE, this->dma_buf_handle);
	kfree(katana_nand_mtd);
	return -ENOMEM;
    }

    /* Register the partitions */
    katana_nand_nr_partitions =
	parse_cmdline_partitions(katana_nand_mtd, &katana_nand_partition_info,
				 "katana-nand");
    if (katana_nand_nr_partitions <= 0) {
	katana_nand_nr_partitions = DEFAULT_NR_PARTITIONS;
	katana_nand_partition_info = katana_nand_default_partition_info;
    }

    katana_nand_part_mtdp =
	kmalloc(sizeof (struct mtd_info*) * katana_nand_nr_partitions, GFP_KERNEL);
    if (! katana_nand_part_mtdp) {
	printk("Unable to allocate \"katana_nand_part_mtdp\" "
	       "for Katana MTD/NAND driver.\n");
	kfree(this->data_cache);
	consistent_free(this->dma_buf, DMA_BUF_SIZE, this->dma_buf_handle);
	kfree(katana_nand_mtd);
	return -ENOMEM;
    }

    for (i = 0; i < katana_nand_nr_partitions; i++)
	katana_nand_partition_info[i].mtdp = &katana_nand_part_mtdp[i];
    add_mtd_partitions(katana_nand_mtd, katana_nand_partition_info,
		       katana_nand_nr_partitions);
    for (i = 0; i < katana_nand_nr_partitions; i++)
	add_mtd_device(katana_nand_part_mtdp[i]);

    katana_nand_command(this, NAND_CMD_RESET, -1, -1, 0);

    return 0;
}
module_init(katana_nand_init);


/*
 * Clean up routine
 */
static void __exit
katana_nand_cleanup(void)
{
    struct katana_nand_info* this = katana_nand_mtd->priv;
    int i;

    for (i = katana_nand_nr_partitions - 1; i >= 0; i--)
	del_mtd_device(katana_nand_part_mtdp[i]);

    kfree(katana_nand_part_mtdp);
    kfree(this->data_cache);
    consistent_free(this->dma_buf, DMA_BUF_SIZE, this->dma_buf_handle);
    kfree(katana_nand_mtd);
}
module_exit(katana_nand_cleanup);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sachio OKAMOTO <soka@lineo.co.jp");
MODULE_DESCRIPTION("glue layer for NAND flash on Katana SoC");
