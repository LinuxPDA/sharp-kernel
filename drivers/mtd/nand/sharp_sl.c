/*
 * Drivers/mtd/nand/sharp_sl.c
 *
 *  Copyright (C) 2002 Lineo Japan, Inc.
 *
 * $Id: sharp_sl.c,v 1.13.2.27 2002/12/10 06:21:15 soka Exp $
 *
 * Based on:
 *
 *  drivers/mtd/nand/spia.c
 *
 *  Copyright (C) 2000 Steven J. Hill (sjhill@cotw.com)
 *
 *
 *	10-29-2001 TG	change to support hardwarespecific access
 *			to controllines	(due to change in nand.c)
 *			page_cache added
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   SPIA board which utilizes the Toshiba TC58V64AFT part. This is
 *   a 64Mibit (8MiB x 8 bits) NAND flash device.
 *
 * ChangLog:
 *     23-Oct-2002 SHARP  add functions for CONFIG_MTD_NAND_LOGICAL_ADDRESS_ACCESS
 *     14-Mar-2003 Sharp wait for ready
 *     01-Jun-2004 Lineo Solutions, Inc.  for Spitz
 *     26-Aug-2004 Sharp cancel write-protect at resume
 *     28-Feb-2005 Sharp Corporation for Akita
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>
#include <asm/hardware.h>
#ifdef CONFIG_MTD_NAND_LOGICAL_ADDRESS_ACCESS
#include <asm-arm/sharp_nand_logical.h>
#endif

#undef DEBUG_PROC
#ifdef DEBUG_PROC
#include <linux/proc_fs.h>
#endif
#include "measure.h"

#define MAX_ECC_RETRY 3
#define MAX_WRITE_RETRIES 32
#define MAX_COPIES 100

#ifdef CONFIG_ARCH_SHARP_SL
#define FAILURECOUNTER_POS NAND_POSTBADBLOCK_POS
#else
#define FAILURECOUNTER_POS NAND_BADBLOCK_POS
#endif

#if defined (CONFIG_ARCH_PXA_AKITA)
#define FAST_READ  
#endif

/*
 * MTD structure for Poodle
 */
static struct mtd_info *sharp_sl_mtd = NULL;

/*
 * original nand_* functions pointer
 */
static int (*orig_read_ecc)(struct mtd_info*, loff_t, size_t, size_t*, u_char*, u_char*, int);
static int (*orig_write_ecc)(struct mtd_info*, loff_t, size_t, size_t*, const u_char*, u_char*, int);
static int (*orig_writev_ecc)(struct mtd_info*, const struct iovec*, unsigned long, loff_t, size_t*, u_char*, int);
static int (*orig_write_oob)(struct mtd_info*, loff_t, size_t, size_t*, const u_char*);


/*
 * out of band configuration for different filesystems
 */
#if defined (CONFIG_ARCH_PXA_AKITA)
static int oobconfigs[][24] = {
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{ NAND_JFFS2_OOB_ECCPOS0, NAND_JFFS2_OOB_ECCPOS1, NAND_JFFS2_OOB_ECCPOS2,
	NAND_JFFS2_OOB_ECCPOS3, NAND_JFFS2_OOB_ECCPOS4, NAND_JFFS2_OOB_ECCPOS5,
	NAND_JFFS2_OOB_ECCPOS6, NAND_JFFS2_OOB_ECCPOS7, NAND_JFFS2_OOB_ECCPOS8,
	NAND_JFFS2_OOB_ECCPOS9, NAND_JFFS2_OOB_ECCPOS10, NAND_JFFS2_OOB_ECCPOS11,
	NAND_JFFS2_OOB_ECCPOS12, NAND_JFFS2_OOB_ECCPOS13, NAND_JFFS2_OOB_ECCPOS14,
	NAND_JFFS2_OOB_ECCPOS15, NAND_JFFS2_OOB_ECCPOS16, NAND_JFFS2_OOB_ECCPOS17,
	NAND_JFFS2_OOB_ECCPOS18, NAND_JFFS2_OOB_ECCPOS19, NAND_JFFS2_OOB_ECCPOS20,
	NAND_JFFS2_OOB_ECCPOS21, NAND_JFFS2_OOB_ECCPOS22, NAND_JFFS2_OOB_ECCPOS23},
	{ NAND_YAFFS_OOB_ECCPOS0, NAND_YAFFS_OOB_ECCPOS1, NAND_YAFFS_OOB_ECCPOS2,
	NAND_YAFFS_OOB_ECCPOS3, NAND_YAFFS_OOB_ECCPOS4, NAND_YAFFS_OOB_ECCPOS5,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}	
};
#else
static int oobconfigs[][6] = {
	{ 0,0,0,0,0,0},

	{ NAND_JFFS2_OOB_ECCPOS0, NAND_JFFS2_OOB_ECCPOS1, NAND_JFFS2_OOB_ECCPOS2,
	NAND_JFFS2_OOB_ECCPOS3, NAND_JFFS2_OOB_ECCPOS4, NAND_JFFS2_OOB_ECCPOS5 },

	{ NAND_YAFFS_OOB_ECCPOS0, NAND_YAFFS_OOB_ECCPOS1, NAND_YAFFS_OOB_ECCPOS2,
	NAND_YAFFS_OOB_ECCPOS3, NAND_YAFFS_OOB_ECCPOS4, NAND_YAFFS_OOB_ECCPOS5 }	
};
#endif

/*
 * Macros for low-level register control
 */
#define nand_select()	this->hwcontrol(NAND_CTL_SETNCE);

#define nand_deselect() this->hwcontrol(NAND_CTL_CLRNCE);


/*
 * Define partitions for flash device
 */
static struct mtd_info** sharp_sl_nand_part_mtdp;
static int nr_partitions;
static struct mtd_partition sharp_sl_nand_default_partition_info[] = {
#if defined(CONFIG_ARCH_PXA_SPITZ)
    {
	.name = "smf",
	.offset = 0,
	.size = 7 * 1024 * 1024,
    },
    {
	.name = "root",
	.offset = 7 * 1024 * 1024,
	.size = 53 * 1024 * 1024,
    },
    {
	.name = "home",
	.offset = 60 * 1024 * 1024,
	.size = (128 - 60) * 1024 * 1024,
    },
#else
    {
	.name = "NAND flash partition 0",
	.offset = 0,
	.size = 7 * 1024 * 1024,
    },
    {
	.name = "NAND flash partition 1",
	.offset = 7 * 1024 * 1024,
	.size = 30 * 1024 * 1024,
    },
    {
	.name = "NAND flash partition 2",
	.offset = 37 * 1024 * 1024,
	.size = (64 - 37) * 1024 * 1024,
    },
#endif
};
static struct mtd_partition* sharp_sl_nand_partition_info;

#define DEFAULT_NUM_PARTITIONS (sizeof(sharp_sl_nand_default_partition_info) / sizeof(struct mtd_partition))


/* 
 *	hardware specific access to control-lines
 */
static void
sharp_sl_nand_hwcontrol(int cmd)
{
    switch (cmd) {
	case NAND_CTL_SETCLE: CPLD_REG(FLASHCTL) |= FLCLE; break;
	case NAND_CTL_CLRCLE: CPLD_REG(FLASHCTL) &= ~FLCLE; break;

	case NAND_CTL_SETALE: CPLD_REG(FLASHCTL) |= FLALE; break;
	case NAND_CTL_CLRALE: CPLD_REG(FLASHCTL) &= ~FLALE; break;

	case NAND_CTL_SETNCE: CPLD_REG(FLASHCTL) &= ~(FLCE0|FLCE1); break;
	case NAND_CTL_CLRNCE: CPLD_REG(FLASHCTL) |= (FLCE0|FLCE1); break;
    }
}


/*
 *
 */
static int
sharp_sl_nand_command_1(struct mtd_info* mtd,
			unsigned command,
			int column,
			int page_addr)
{
    register struct nand_chip *this = mtd->priv;
    register unsigned long NAND_IO_ADDR = this->IO_ADDR_W;
    int i;

#ifdef CONFIG_ARCH_SHARP_SL
	if (command != NAND_CMD_RESET &&
		command != NAND_CMD_STATUS) {
		for (i = 0; i < NAND_BUSY_TIMEOUT; i++)
			if (!sharp_sl_nand_flash_busy())
				break;
		if (i == NAND_BUSY_TIMEOUT)
			return -EIO;
	}
#endif

    /* Begin command latch cycle */
    this->hwcontrol (NAND_CTL_SETCLE);
    /*
     * Write out the command to the device.
     */
    if (command != NAND_CMD_SEQIN) {
#if defined (CONFIG_ARCH_PXA_AKITA)
      if ((command == NAND_CMD_READOOB) || (command == NAND_CMD_READ0)) {
        writeb (NAND_CMD_READ0, NAND_IO_ADDR);
      }
      else
	writeb (command, NAND_IO_ADDR);
#else
	writeb (command, NAND_IO_ADDR);
#endif
    }
    else {
#if defined (CONFIG_ARCH_PXA_AKITA)
        writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
#else
	if (mtd->oobblock == 256 && column >= 256) {
	    column -= 256;
	    writeb (NAND_CMD_READOOB, NAND_IO_ADDR);
	    writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
	} else if (mtd->oobblock == 512 && column >= 256) {
	    if (column < 512) {
		column -= 256;
		writeb (NAND_CMD_READ1, NAND_IO_ADDR);
		writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
	    } else {
		column -= 512;
		writeb (NAND_CMD_READOOB, NAND_IO_ADDR);
		writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
	    }
	} else {
	    writeb (NAND_CMD_READ0, NAND_IO_ADDR);
	    writeb (NAND_CMD_SEQIN, NAND_IO_ADDR);
	}
#endif
    }

    /* Set ALE and clear CLE to start address cycle */
    this->hwcontrol (NAND_CTL_CLRCLE);

    if (column != -1 || page_addr != -1) {
	this->hwcontrol (NAND_CTL_SETALE);

	/* Serially input address */
#if defined (CONFIG_ARCH_PXA_AKITA)
	if (command == NAND_CMD_READOOB) {
	    if (column >= 0 && column < 64) writeb (column, NAND_IO_ADDR);
	    else writeb (0, NAND_IO_ADDR);
	    writeb (0x08, NAND_IO_ADDR);
	}
	else if (column != -1) {
	    writeb ((unsigned char)(column & 0xff), NAND_IO_ADDR);
	    writeb ((unsigned char)(column >> 8), NAND_IO_ADDR);
	}
	if (page_addr != -1) {
	    writeb ((unsigned char) (page_addr & 0xff), NAND_IO_ADDR);
	    writeb ((unsigned char) ((page_addr >> 8) & 0xff), NAND_IO_ADDR);
	}
#else
	if (column != -1)
	    writeb (column, NAND_IO_ADDR);
	if (page_addr != -1) {
	    writeb ((unsigned char) (page_addr & 0xff), NAND_IO_ADDR);
	    writeb ((unsigned char) ((page_addr >> 8) & 0xff), NAND_IO_ADDR);
	    /* One more address cycle for higher density devices */
	    if (mtd->size & 0x0c000000) 
		writeb ((unsigned char) ((page_addr >> 16) & 0x0f), NAND_IO_ADDR);
	}
#endif
	/* Latch in address */
	this->hwcontrol (NAND_CTL_CLRALE);

#if defined (CONFIG_ARCH_PXA_AKITA)
	if ((command == NAND_CMD_READOOB) || (command == NAND_CMD_READ0)) {
	    /* Begin command latch cycle */
	    this->hwcontrol (NAND_CTL_SETCLE);
	    writeb (NAND_CMD_READ30, NAND_IO_ADDR);
	    /* Set ALE and clear CLE to start address cycle */
	    this->hwcontrol (NAND_CTL_CLRCLE);
	}
#endif


    }
	
    /* 
     * program and erase have their own busy handlers 
     * status and sequential in needs no delay
     */
    switch (command) {
    case NAND_CMD_PAGEPROG:	/* wait in this->waitfunc() */
    case NAND_CMD_ERASE1:
    case NAND_CMD_ERASE2:	/* wait in this->waitfunc() */
    case NAND_CMD_SEQIN:
    case NAND_CMD_STATUS:
	return 0;

    case NAND_CMD_RESET:
	break;

    }
	
    /* wait until command is processed */
    for (i = 0; i < NAND_BUSY_TIMEOUT; i++)
	if (this->dev_ready())
	    return 0;

    return -EIO;
}


/*
 *
 */
static int
sharp_sl_nand_command(struct mtd_info* mtd,
		      unsigned command,
		      int column,
		      int page_addr)
{
    if (command == NAND_CMD_SEQIN)
	/* ignore Ready/Busy error */
	sharp_sl_nand_command_1(mtd, NAND_CMD_READ0, column, page_addr);
    return sharp_sl_nand_command_1(mtd, command, column, page_addr);
}


/*
 *	Get chip for selected access
 *	(copy from nand.c)
 */
static inline void
nand_get_chip(struct nand_chip *this,
	      struct mtd_info *mtd,
	      int new_state)
{

    DECLARE_WAITQUEUE (wait, current);

    /* 
     * Grab the lock and see if the device is available 
     * For erasing, we keep the spinlock until the
     * erase command is written. 
     */
  retry:
    spin_lock_bh (&this->chip_lock);

    if (this->state == FL_READY) {
	this->state = new_state;
	if (new_state != FL_ERASING)
	    spin_unlock_bh (&this->chip_lock);
	return;
    }

#if 0
    if (this->state == FL_ERASING) {
	if (new_state != FL_ERASING) {
	    this->state = new_state;
	    spin_unlock_bh (&this->chip_lock);
	    nand_select ();	/* select in any case */
	    this->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
	    return;
	}
    }
#endif

    set_current_state (TASK_UNINTERRUPTIBLE);
    add_wait_queue (&this->wq, &wait);
    spin_unlock_bh (&this->chip_lock);
    schedule ();
    remove_wait_queue (&this->wq, &wait);
    goto retry;
}


static inline void
sharp_sl_nand_read_page(struct mtd_info* mtd,
			struct nand_chip* this,
			u_char* data_poi,
			u_char* oob_data,
			u_char* ecc_calc,
			int ecc,
			int end)
{
    int j = 0;

    START_MEASUREMENT(nand_read_page);
#if defined (CONFIG_ARCH_PXA_AKITA)
    {
        int i,j;
	for (i=0; i<8; i++) { 
	    this->enable_hwecc (NAND_ECC_READ);	
	    for (j=i*ecc; j < (i+1)*ecc; j++)
	        data_poi[j] = readb (this->IO_ADDR_R);
	    this->calculate_ecc (&data_poi[ecc*i], &ecc_calc[3*i]);	/* read from hardware */
	}
    }
#else
    this->enable_hwecc (NAND_ECC_READ);	
    while (j < ecc)
	data_poi[j++] = readb (this->IO_ADDR_R);
    this->calculate_ecc (&data_poi[0], &ecc_calc[0]);	/* read from hardware */

    this->enable_hwecc (NAND_ECC_READ);	
    while (j < end)
	data_poi[j++] = readb (this->IO_ADDR_R);
    this->calculate_ecc (&data_poi[256], &ecc_calc[3]); /* read from hardware */

#endif
    /* read oobdata */
    for (j = 0; j <  mtd->oobsize; j++) 
	oob_data[j] = readb (this->IO_ADDR_R);

    ACCUMULATE_ELAPSED_TIME(nand_read_page);
    COUNTER_INC(nand_read_nr_pages);
}

#ifdef FAST_READ
static inline int
sharp_sl_nand_read_page_fast(struct mtd_info* mtd,
			struct nand_chip* this,
			u_char* data_poi,
			u_char* oob_data,
			u_char* ecc_calc,
			int ecc,
			int from,
			int end,
			int page)
{
    int j = 0;

    START_MEASUREMENT(nand_read_page);
    {
        int len = end - from;
        int i,j;
	for (i =0 ; i < len/ecc; i++) { 
	    this->enable_hwecc (NAND_ECC_READ);	
	    for (j=i*ecc; j < (i+1)*ecc; j++)
	        data_poi[j] = readb (this->IO_ADDR_R);
	    this->calculate_ecc (&data_poi[ecc*i], &ecc_calc[3*i]);	/* read from hardware */
	}
    }

    for (j = 0; j < NAND_BUSY_TIMEOUT; j++)
      if (this->dev_ready()) break;
    if (j == NAND_BUSY_TIMEOUT) {
          printk("%s : time out \n",__func__);
	  return -EIO;
    }

    /* Send the read command */
    if (this->cmdfunc (mtd, NAND_CMD_READOOB, 0, page)) {
          printk(KERN_WARNING "%s: Failed in NAND_CMD_READOOB command, page 0x%08x\n",
		 __func__, page);
	  return -EIO;
    }

    /* read oobdata */
    for (j = 0; j <  mtd->oobsize; j++) 
	oob_data[j] = readb (this->IO_ADDR_R);

    ACCUMULATE_ELAPSED_TIME(nand_read_page);
    COUNTER_INC(nand_read_nr_pages);

    return 0;
}
/*
 * this function is registered only if eccmode == NAND_ECC_HW3_256 &&
 * oobblock == 2048
 */
static int
sharp_sl_nand_read_ecc_fast(struct mtd_info* mtd,
		       loff_t from,
		       size_t len,
		       size_t* retlen,
		       u_char* buf,
		       u_char* oob_buf,
		       int oobsel)
{
    int col, page, end, ecc;
    int read, ecc_failed, ret;
    struct nand_chip *this;
    u_char *oob_data;
    int *oob_config;
    u_char* data_poi = 0;

    DEBUG(MTD_DEBUG_LEVEL3, "%s: from = 0x%08x, len = %i\n",
	   __func__, (unsigned int) from, (int) len);

    /* Grab the lock and see if the device is available */
    this = mtd->priv;
    nand_get_chip (this, mtd ,FL_READING);

    /* Select the NAND device */
    nand_select ();

    oob_config = oobconfigs[oobsel];
    page = from >> this->page_shift;
    col = from & (mtd->oobblock - 1);
    end = col + len;
    ecc = mtd->eccsize;
    oob_data = &this->data_buf[mtd->oobblock];
    read = 0;
    ecc_failed = 0;
    ret = 0;

	
    /* Loop until all data read */
    {
	int j;
	int ecc_status;
	int ecc_retry_counter = 0;
	u_char ecc_calc[24];
	u_char ecc_code[24];
	int col256 = col/ecc*ecc;

	/* Send the read command */
	if (this->cmdfunc (mtd, NAND_CMD_READ0, col256, page)) {
	  printk(KERN_WARNING "%s: Failed in NAND_CMD_READ0 command, page 0x%08x\n",
		 __func__, page);
	  ret = -EIO;
	  goto nand_read_ecc_exit;
	}
	/* 
	 * If the read is not page aligned, we have to read into data buffer
	 * due to ecc, else we read into return buffer direct
	 */
	data_poi = this->data_buf;

      ecc_retry:
	ret = sharp_sl_nand_read_page_fast(mtd, this, &data_poi[col256], oob_data, &ecc_calc[col/ecc*3], ecc, col256,(end+ecc-1)/ecc*ecc, page);
	if (ret == -EIO) goto nand_read_ecc_exit;

	/* If we have consequent page reads, apply delay or wait for ready/busy pin */
	for (j = 0; j < NAND_BUSY_TIMEOUT; j++)
	    if (this->dev_ready())
		break;
	if (j == NAND_BUSY_TIMEOUT) {
	    ret = -EIO;
	    goto nand_read_ecc_exit;
	}

	/* Pick the ECC bytes out of the oob data */
	for (j = 0; j < 24; j++)
	    ecc_code[j] = oob_data[oob_config[j]];

	/* correct data, if neccecary */
	{
	    int i;
	    i = col/ecc;
	    for ( ; i< (end+ecc-1)/ecc; i++) {
	        ecc_status = this->correct_data (&data_poi[i*ecc], &ecc_code[i*3], &ecc_calc[i*3]);
		if (ecc_status == -1) {
		    if (ecc_retry_counter++ < MAX_ECC_RETRY) {
		        this->cmdfunc (mtd, NAND_CMD_RESET, -1, -1);
			if (this->cmdfunc (mtd, NAND_CMD_READ0, col256, page)) {
			    printk(KERN_WARNING
				   "%s: Failed in NAND_CMD_READ0 command, page 0x%08x\n",
			   __func__, page);
			    ret = -EIO;
			    goto nand_read_ecc_exit;
			}
			goto ecc_retry;
		    }
		    else {
		        printk (KERN_WARNING "%s: Failed ECC read, page 0x%08x i=%d \n",
			__func__, page,i);
			ecc_failed++;
		    }
		}
	    }
	}	
	
	
	for (j = col; j < end && read < len; j++)
		buf[read++] = data_poi[j];
    }

  nand_read_ecc_exit:
    ret = (ret == 0) ? (ecc_failed ? -EIO : 0) : ret;
    if (ret == -EIO)
	this->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);

    /* De-select the NAND device */
    nand_deselect ();

    /* Wake up anyone waiting on the device */
    spin_lock_bh (&this->chip_lock);
    this->state = FL_READY;
    wake_up (&this->wq);
    spin_unlock_bh (&this->chip_lock);

    /*
     * Return success, if no ECC failures, else -EIO
     * fs driver will take care of that, because
     * retlen == desired len and result == -EIO
     */
    *retlen = read;
    return ret;
}

#endif /* FAST_READ */

/*
 * this function is registered only if eccmode == NAND_ECC_HW3_256 &&
 * oobblock == 512
 */
static int
sharp_sl_nand_read_ecc(struct mtd_info* mtd,
		       loff_t from,
		       size_t len,
		       size_t* retlen,
		       u_char* buf,
		       u_char* oob_buf,
		       int oobsel)
{
    int col, page, end, ecc;
    int read, ecc_failed, ret;
    struct nand_chip *this;
    u_char *oob_data;
    int *oob_config;
    u_char* data_poi = 0;

    if (oob_buf || ! oobsel)
	return orig_read_ecc(mtd, from, len, retlen, buf, oob_buf, oobsel);
    DEBUG(MTD_DEBUG_LEVEL3, "%s: from = 0x%08x, len = %i\n",
	   __func__, (unsigned int) from, (int) len);
    /* Do not allow reads past end of device */
    if ((from + len) > mtd->size) {
	DEBUG (MTD_DEBUG_LEVEL0, "%s: Attempt read beyond end of device\n", __func__);
	*retlen = 0;
	return -EINVAL;
    }

#ifdef FAST_READ
    if (((from & (mtd->oobblock - 1))+ len) < mtd->oobblock)
	return sharp_sl_nand_read_ecc_fast(mtd, from, len, retlen, buf, oob_buf, oobsel);
#endif

    /* Grab the lock and see if the device is available */
    this = mtd->priv;
    nand_get_chip (this, mtd ,FL_READING);

    /* Select the NAND device */
    nand_select ();

    oob_config = oobconfigs[oobsel];
    page = from >> this->page_shift;
    col = from & (mtd->oobblock - 1);
    end = mtd->oobblock;
    ecc = mtd->eccsize;
    oob_data = &this->data_buf[end];
    read = 0;
    ecc_failed = 0;
    ret = 0;

#if !defined (CONFIG_ARCH_PXA_AKITA)
    /* Send the read command */
    if (this->cmdfunc (mtd, NAND_CMD_READ0, 0x00, page)) {
      	printk(KERN_WARNING "%s: Failed in NAND_CMD_READ0 command, page 0x%08x\n", __func__, page);
	ret = -EIO;
	goto nand_read_ecc_exit;
    }
#endif
	
    /* Loop until all data read */
    while (read < len) {
	int j;
	int ecc_status;
	int ecc_retry_counter = 0;
#if defined (CONFIG_ARCH_PXA_AKITA)
	u_char ecc_calc[24];
	u_char ecc_code[24];
#else
	u_char ecc_calc[6];
	u_char ecc_code[6];
#endif

#if defined (CONFIG_ARCH_PXA_AKITA)
	/* Send the read command */
	if (this->cmdfunc (mtd, NAND_CMD_READ0, 0x00, page)) {
	  printk(KERN_WARNING "%s: Failed in NAND_CMD_READ0 command, page 0x%08x\n",
		 __func__, page);
	  ret = -EIO;
	  goto nand_read_ecc_exit;
	}
#endif

	/* 
	 * If the read is not page aligned, we have to read into data buffer
	 * due to ecc, else we read into return buffer direct
	 */
	if (!col && (len - read) >= end)  
	    data_poi = &buf[read];
	else 
	    data_poi = this->data_buf;

#ifdef CONFIG_MTD_NAND_PAGE_CACHE
	if (page == this->cache_page) {
	    memcpy(data_poi, this->data_cache, end);

	    if (this->cmdfunc (mtd, NAND_CMD_READ0, 0x00, page + 1)) {
		printk(KERN_WARNING "%s: Failed in NAND_CMD_READ0 command,"
		       " page 0x%08x\n", __func__, page);
		ret = -EIO;
		goto nand_read_ecc_exit;
	    }

	    goto loop_next;
	}
#endif

      ecc_retry:
	sharp_sl_nand_read_page(mtd, this, data_poi, oob_data, ecc_calc, ecc, end);

#if defined (CONFIG_ARCH_PXA_AKITA)
	/* Pick the ECC bytes out of the oob data */
	for (j = 0; j < 24; j++)
	    ecc_code[j] = oob_data[oob_config[j]];
#else
	/* Pick the ECC bytes out of the oob data */
	for (j = 0; j < 6; j++)
	    ecc_code[j] = oob_data[oob_config[j]];
#endif

	/* If we have consequent page reads, apply delay or wait for ready/busy pin */
	for (j = 0; j < NAND_BUSY_TIMEOUT; j++)
	    if (this->dev_ready())
		break;
	if (j == NAND_BUSY_TIMEOUT) {
	    ret = -EIO;
	    goto nand_read_ecc_exit;
	}

#if defined (CONFIG_ARCH_PXA_AKITA)
	/* correct data, if neccecary */
	{
	    int i;
	    for (i=0; i<8; i++) {
	        ecc_status = this->correct_data (&data_poi[i*256], &ecc_code[i*3], &ecc_calc[i*3]);
		if (ecc_status == -1) {
		    if (ecc_retry_counter++ < MAX_ECC_RETRY) {
		        this->cmdfunc (mtd, NAND_CMD_RESET, -1, -1);
			if (this->cmdfunc (mtd, NAND_CMD_READ0, 0x00, page)) {
			    printk(KERN_WARNING
				   "%s: Failed in NAND_CMD_READ0 command, page 0x%08x\n",
			   __func__, page);
			    ret = -EIO;
			    goto nand_read_ecc_exit;
			}
			goto ecc_retry;
		    }
		    else {
		        printk (KERN_WARNING "%s: Failed ECC read, page 0x%08x i=%d \n",
			__func__, page,i);
			ecc_failed++;
		    }
		}
	    }
	}	
#else
	/* correct data, if neccecary */
	ecc_status = this->correct_data (&data_poi[0], &ecc_code[0], &ecc_calc[0]);
	if (ecc_status == -1) {
	    if (ecc_retry_counter++ < MAX_ECC_RETRY) {
		this->cmdfunc (mtd, NAND_CMD_RESET, -1, -1);
		if (this->cmdfunc (mtd, NAND_CMD_READ0, 0x00, page)) {
		    printk(KERN_WARNING
			   "%s: Failed in NAND_CMD_READ0 command, page 0x%08x\n",
			   __func__, page);
		    ret = -EIO;
		    goto nand_read_ecc_exit;
		}
		goto ecc_retry;
	    }
	    else {
		printk (KERN_WARNING "%s: Failed ECC read, page 0x%08x\n",
			__func__, page);
		ecc_failed++;
	    }
	}
		
	ecc_status = this->correct_data (&data_poi[256], &ecc_code[3], &ecc_calc[3]);
	if (ecc_status == -1) {
	    if (ecc_retry_counter++ < MAX_ECC_RETRY) {
		this->cmdfunc (mtd, NAND_CMD_RESET, -1, -1);
		if (this->cmdfunc (mtd, NAND_CMD_READ0, 0x00, page)) {
		    printk(KERN_WARNING
			   "%s: Failed in NAND_CMD_READ0 command, page 0x%08x\n",
			   __func__, page);
		    ret = -EIO;
		    goto nand_read_ecc_exit;
		}
		goto ecc_retry;
	    }
	    else {
		printk (KERN_WARNING "%s: Failed ECC read, page 0x%08x\n",
			__func__, page);
		ecc_failed++;
	    }
	}
#endif

#ifdef CONFIG_MTD_NAND_PAGE_CACHE
      loop_next:
#endif
	if (col || (len - read) < end) { 
	    for (j = col; j < end && read < len; j++)
		buf[read++] = data_poi[j];
	} else		
	    read += mtd->oobblock;
	/* For subsequent reads align to page boundary. */
	col = 0;
	/* Increment page address */
	page++;


    }

  nand_read_ecc_exit:
    ret = (ret == 0) ? (ecc_failed ? -EIO : 0) : ret;
    if (ret == -EIO)
	this->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
#ifdef CONFIG_MTD_NAND_PAGE_CACHE
    else if (page - 1 != this->cache_page && data_poi) {
	memcpy(this->data_cache, data_poi, end);
	this->cache_page = page - 1;
    }
#endif

    /* De-select the NAND device */
    nand_deselect ();

    /* Wake up anyone waiting on the device */
    spin_lock_bh (&this->chip_lock);
    this->state = FL_READY;
    wake_up (&this->wq);
    spin_unlock_bh (&this->chip_lock);

    /*
     * Return success, if no ECC failures, else -EIO
     * fs driver will take care of that, because
     * retlen == desired len and result == -EIO
     */
    *retlen = read;
    return ret;
}


static int
sharp_sl_nand_read(struct mtd_info* mtd,
		   loff_t from,
		   size_t len,
		   size_t* retlen,
		   u_char* buf)
{
    return sharp_sl_nand_read_ecc(mtd, from, len, retlen, buf, NULL, NAND_JFFS2_OOB);
}


static inline int jffs2_hamming_distance(unsigned int a, unsigned int b)
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


static inline void
jffs2_correct_badblock_val(u_char* oob)
{
#ifdef CONFIG_MTD_NAND_POST_BADBLOCK
    if (jffs2_hamming_distance(oob[NAND_POSTBADBLOCK_POS], 0xff))
	oob[NAND_POSTBADBLOCK_POS] = 0xff;
#endif
    if (jffs2_hamming_distance(oob[NAND_BADBLOCK_POS], 0xff))
	oob[NAND_BADBLOCK_POS] = 0xff;
}


static inline void
jffs2_correct_failedblock_val(u_char* oob)
{
    if (jffs2_hamming_distance(oob[FAILURECOUNTER_POS], 0xff))
	oob[FAILURECOUNTER_POS] = 0xff;
}


static inline void
jffs2_correct_cleanmarker(u_char* oob,
			  u_char* correct_data,
			  int len)
{
    int i;
    for (i = 0; i < len; i++)
	if (jffs2_hamming_distance(oob[i], correct_data[i]))
	    oob[i] = correct_data[i];
}


static int
sharp_sl_nand_prepare_rewrite(struct mtd_info* mtd,
			      loff_t to,
			      int oobsel)
{
    int i;
    loff_t block_addr = to & ~(mtd->erasesize - 1);
    size_t len = to & (mtd->erasesize - 1);
    int nr_oobpages = 2;	// JFFS2 only (JFFS2 uses only 2 oobpages)
    int fsdata_pos = (mtd->oobblock == 256)
	? NAND_JFFS2_OOB8_FSDAPOS : NAND_JFFS2_OOB16_FSDAPOS;
    int fsdata_len = (mtd->oobblock == 256)
	? NAND_JFFS2_OOB8_FSDALEN : NAND_JFFS2_OOB16_FSDALEN;
    u_char* block_buf;
    u_char* oob_buf;
    int ret;
    int retlen;
    struct erase_info instr;

    block_buf = kmalloc(len, GFP_KERNEL);
    if (! block_buf) {
	printk("Unable to allocate NAND block buffer (%u)\n", len);
	return -EIO;
    }
    oob_buf = kmalloc(mtd->oobsize * nr_oobpages, GFP_KERNEL);
    if (! oob_buf) {
	printk("Unable to allocate NAND oob buffer\n");
	kfree(block_buf);
	return -EIO;
    }

    ret = mtd->read_ecc(mtd, block_addr, len, &retlen, block_buf, NULL, NAND_JFFS2_OOB);
    if (ret || retlen != len) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: read_ecc failed (%d)\n", __func__, ret);
	kfree(block_buf);
	kfree(oob_buf);
	return ret ? ret : -EIO;
    }

    ret = mtd->read_oob(mtd, block_addr, mtd->oobsize * nr_oobpages, &retlen, oob_buf);
    if (ret || retlen != mtd->oobsize * nr_oobpages) {
	DEBUG(MTD_DEBUG_LEVEL0, "%s: read_oob failed (%d)\n", __func__, ret);
	kfree(block_buf);
	kfree(oob_buf);
	return ret ? ret : -EIO;
    }
#if 1			/* FIXME */
    {
	struct {
	    /* All start like this */
	    uint16_t magic;
	    uint16_t nodetype;
	    uint32_t totlen; /* So we can skip over nodes we don't grok */
	} __attribute__((packed)) n = {
	    .magic = 0x1985,
	    .nodetype = 0x2003,
	    .totlen = 8,
	};
	u_char* p;

	p = (u_char*)&n;

	jffs2_correct_badblock_val(oob_buf);
	jffs2_correct_failedblock_val(&oob_buf[mtd->oobsize]);
	jffs2_correct_cleanmarker(&oob_buf[fsdata_pos], p, fsdata_len);
    }
#endif

    for (i = 0; i < MAX_COPIES; i++) {
	int j;

	memset(&instr, 0, sizeof instr);
	instr.mtd = mtd;
	instr.addr = block_addr;
	instr.len = mtd->erasesize;
	ret = mtd->erase(mtd, &instr);
	if (ret) {
	    DEBUG(MTD_DEBUG_LEVEL0, "%s: erase failed (%d)\n", __func__, ret);
	    continue;
	}

	ret = orig_write_ecc(mtd, block_addr, len, &retlen, block_buf, NULL,
			     NAND_JFFS2_OOB);
	if (ret || retlen != len) {
	    DEBUG(MTD_DEBUG_LEVEL0, "%s: write_ecc failed (%d)\n", __func__, ret);
	    continue;
	}

	for (j = 0; j < nr_oobpages; j++) {
	    loff_t addr = block_addr + mtd->oobblock * j;

#ifdef CONFIG_MTD_NAND_POST_BADBLOCK
	    ret = orig_write_oob(mtd, addr + NAND_POSTBADBLOCK_POS, 1, &retlen,
				 oob_buf + mtd->oobsize * j + NAND_POSTBADBLOCK_POS);
	    if (ret || retlen != 1) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: write_oob post_badblock(%d) failed (%d)\n",
		      __func__, j, ret);
		break;
	    }
#endif

	    ret = orig_write_oob(mtd, addr + NAND_BADBLOCK_POS, 1, &retlen,
				 oob_buf + mtd->oobsize * j + NAND_BADBLOCK_POS);
	    if (ret || retlen != 1) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: write_oob badblock(%d) failed (%d)\n",
		      __func__, j, ret);
		break;
	    }

	    ret = orig_write_oob(mtd, addr + fsdata_pos, fsdata_len, &retlen,
				 oob_buf + mtd->oobsize * j + fsdata_pos);
	    if (ret || retlen != fsdata_len) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: write_oob fsdata(%d) failed (%d)\n",
		      __func__, j, ret);
		break;
	    }
	}
	if (j < nr_oobpages)
	    continue;

	break;
    }

    kfree(block_buf);
    kfree(oob_buf);
    DEBUG(MTD_DEBUG_LEVEL0,
	  "%s: %d\n", __func__, (i < MAX_COPIES) ? 0 : (ret ? ret : -EIO));
    return (i < MAX_COPIES) ? 0 : (ret ? ret : -EIO);
}
			      

static int
sharp_sl_nand_write_ecc(struct mtd_info* mtd,
			loff_t to,
			size_t len,
			size_t* retlen,
			const u_char* buf,
			u_char* eccbuf,
			int oobsel)
{
    int i;
    int ret;

    for (i = 0; i < MAX_WRITE_RETRIES; i++) {
	ret = orig_write_ecc(mtd, to, len, retlen, buf, eccbuf, oobsel);
	if (ret != -EIO)
	    return ret;

	ret = sharp_sl_nand_prepare_rewrite(mtd, to, oobsel);
	if (ret)
	    return ret;
    }

    return -EIO;
}


static int
sharp_sl_nand_write(struct mtd_info* mtd,
		    loff_t to,
		    size_t len,
		    size_t* retlen,
		    const u_char* buf)
{
    return sharp_sl_nand_write_ecc(mtd, to, len, retlen, buf, NULL, NAND_JFFS2_OOB);
}


static int
sharp_sl_nand_writev_ecc(struct mtd_info* mtd,
			 const struct iovec* vecs,
			 unsigned long count,
			 loff_t to,
			 size_t* retlen,
			 u_char* eccbuf,
			 int oobsel)
{
    int i;
    int ret;

    for (i = 0; i < MAX_WRITE_RETRIES; i++) {
	ret = orig_writev_ecc(mtd, vecs, count, to, retlen, eccbuf, oobsel);
	if (ret != -EIO)
	    return ret;

	ret = sharp_sl_nand_prepare_rewrite(mtd, to, oobsel);
	if (ret)
	    return ret;
    }

    return -EIO;
}


static int
sharp_sl_nand_writev(struct mtd_info* mtd,
		     const struct iovec* vecs,
		     unsigned long count,
		     loff_t to,
		     size_t* retlen)
{
    return sharp_sl_nand_writev_ecc(mtd, vecs, count, to, retlen, NULL, NAND_JFFS2_OOB);
}


static int
sharp_sl_nand_write_oob(struct mtd_info* mtd,
			loff_t to,
			size_t len,
			size_t* retlen,
			const u_char* buf)
{
    int i;
    int ret;

    for (i = 0; i < MAX_WRITE_RETRIES; i++) {
	ret = orig_write_oob(mtd, to, len, retlen, buf);
	if (ret != -EIO)
	    return ret;

	ret = sharp_sl_nand_prepare_rewrite(mtd, to & ~(mtd->erasesize - 1),	// JFFS2 only (JFFS2 calls write_oob only after erasing)
					    NAND_JFFS2_OOB);
	if (ret)
	    return ret;
    }
    return -EIO;
}


static int
sharp_sl_nand_flash_busy(void)
{
    return (CPLD_REG(FLASHCTL) & FLRYBY) == 0;
}


/*
 *
 */
static int
sharp_sl_nand_dev_ready(void)
{
    int i;
    for (i = 0; i < 5; i++)
	sharp_sl_nand_flash_busy();

    return ! sharp_sl_nand_flash_busy();
}


/*
 *
 */
static int
sharp_sl_nand_suspend(struct mtd_info* mtd)
{
    int i;
    printk("%s", __func__);
    for (i = 0; i < nr_partitions; i++) {
	invalidate_device(MKDEV(MTD_BLOCK_MAJOR, sharp_sl_nand_part_mtdp[i]->index), 1);
    }
    mtd->sync(mtd);

    printk("\n");
    return 0;
}


/*
 *
 */
static void
sharp_sl_nand_resume(struct mtd_info* mtd)
{
    printk("%s\n", __func__);

    CPLD_REG(FLASHCTL) |= FLWP;
}


#ifdef CONFIG_MTD_NAND_ECC
/*
 *
 */
static void
sharp_sl_nand_enable_hwecc(int mode)
{
    CPLD_REG(ECCCLRR) = 0;
}


/*
 *
 */
static int
sharp_sl_nand_calculate_ecc(const u_char* dat,
			    u_char* ecc_code)
{
    ecc_code[0] = ~CPLD_REG(ECCLPUB);
    ecc_code[1] = ~CPLD_REG(ECCLPLB);
    ecc_code[2] = (~CPLD_REG(ECCCP) << 2) | 0x03;
    return CPLD_REG(ECCCNTR) != 0;
}
#endif


#ifdef DEBUG_PROC
DEFINE_MEASUREMENT_VAR(nand_read_page);
DEFINE_COUNTER_VAR(nand_read_nr_pages);
DEFINE_MEASUREMENT_VAR(nand_write_page);
DEFINE_MEASUREMENT_VAR(nand_write_verify);
DEFINE_COUNTER_VAR(nand_write_nr_pages);
static int
sharp_sl_nand_read_proc(char* buf,
			char** start,
			off_t offset,
			int count,
			int* eof,
			void* data)
{
    int len = 0;
    PRINT_ELAPSED_TIME(nand_read_page, buf, len);
    len += sprintf(buf + len, "nand_read_nr_pages %d\n", nand_read_nr_pages);
    PRINT_ELAPSED_TIME(nand_write_page, buf, len);
    PRINT_ELAPSED_TIME(nand_write_verify, buf, len);
    len += sprintf(buf + len, "nand_write_nr_pages %d\n", nand_write_nr_pages);
    *eof = 1;
    return len;
}


static int sharp_sl_nand_write_proc(struct file* file,
				    const char* buffer,
				    unsigned long count,
				    void* data)
{
    CLEAR_TIME_VER(nand_read_page);
    CLEAR_TIME_VER(nand_write_page);
    CLEAR_TIME_VER(nand_write_verify);
    nand_read_nr_pages = 0;
    nand_write_nr_pages = 0;
    return count;
}
#endif


/*
 * Main initialization routine
 */
int __init
sharp_sl_nand_init (void)
{
    extern int parse_cmdline_partitions(struct mtd_info *, struct mtd_partition **,
					const char *);
    struct nand_chip *this;
    int i;

    /* Allocate memory for MTD device structure and private data */
    sharp_sl_mtd = kmalloc (sizeof(struct mtd_info) + sizeof (struct nand_chip),
			    GFP_KERNEL);
    if (!sharp_sl_mtd) {
	printk ("Unable to allocate Poodle NAND MTD device structure.\n");
	return -ENOMEM;
    }

    /* Get pointer to private data */
    this = (struct nand_chip *) (&sharp_sl_mtd[1]);

    /* Initialize structures */
    memset((char *) sharp_sl_mtd, 0, sizeof(struct mtd_info));
    memset((char *) this, 0, sizeof(struct nand_chip));

    /* Link the private data with the MTD structure */
    sharp_sl_mtd->priv = this;

    /*
     * PXA initialize
     */
    CPLD_REG(FLASHCTL) |= FLWP;

    /* Set address of NAND IO lines */
    this->IO_ADDR_R = NAND_FLASH_REG_BASE + FLASHIO;
    this->IO_ADDR_W = NAND_FLASH_REG_BASE + FLASHIO;
    /* Set address of hardware control function */
    this->hwcontrol = sharp_sl_nand_hwcontrol;
    this->dev_ready = sharp_sl_nand_dev_ready;
    this->cmdfunc = sharp_sl_nand_command;
    /* 15 us command delay time */
    this->chip_delay = 15;
    /* set eccmode using hardware ECC */
#ifdef CONFIG_MTD_NAND_ECC
    this->eccmode = NAND_ECC_HW3_256;
    this->enable_hwecc = sharp_sl_nand_enable_hwecc;
    this->calculate_ecc = sharp_sl_nand_calculate_ecc;
    this->correct_data = nand_correct_data;
#endif

    /* Scan to find existence of the device */
    if (nand_scan (sharp_sl_mtd)) {
	kfree (sharp_sl_mtd);
	return -ENXIO;
    }


#if defined (CONFIG_ARCH_PXA_AKITA)
    if (this->eccmode == NAND_ECC_HW3_256 && sharp_sl_mtd->oobblock == 2048) {
#else
    if (this->eccmode == NAND_ECC_HW3_256 && sharp_sl_mtd->oobblock == 512) {
#endif
	orig_read_ecc = sharp_sl_mtd->read_ecc;
	sharp_sl_mtd->read = sharp_sl_nand_read;
	sharp_sl_mtd->read_ecc = sharp_sl_nand_read_ecc;

	orig_write_ecc = sharp_sl_mtd->write_ecc;
	sharp_sl_mtd->write = sharp_sl_nand_write;
	sharp_sl_mtd->write_ecc = sharp_sl_nand_write_ecc;

	orig_writev_ecc = sharp_sl_mtd->writev_ecc;
	sharp_sl_mtd->writev = sharp_sl_nand_writev;
	sharp_sl_mtd->writev_ecc = sharp_sl_nand_writev_ecc;

	orig_write_oob = sharp_sl_mtd->write_oob;
	sharp_sl_mtd->write_oob = sharp_sl_nand_write_oob;
    }
    sharp_sl_mtd->suspend = sharp_sl_nand_suspend;
    sharp_sl_mtd->resume = sharp_sl_nand_resume;
#ifdef CONFIG_MTD_NAND_LOGICAL_ADDRESS_ACCESS
    sharp_sl_mtd->cleanup_laddr = sharp_sl_nand_cleanup_laddr;
    sharp_sl_mtd->read_laddr = sharp_sl_nand_read_laddr;
    sharp_sl_mtd->write_laddr = sharp_sl_nand_write_laddr;
#endif

    /* Allocate memory for internal data buffer */
    this->data_buf = kmalloc (sizeof(u_char) * (sharp_sl_mtd->oobblock + sharp_sl_mtd->oobsize), GFP_KERNEL);
    if (!this->data_buf) {
	printk ("Unable to allocate NAND data buffer for Poodle.\n");
	kfree (sharp_sl_mtd);
	return -ENOMEM;
    }

    /* Allocate memory for internal data buffer */
    this->data_cache = kmalloc (sizeof(u_char) * (sharp_sl_mtd->oobblock + sharp_sl_mtd->oobsize), GFP_KERNEL);
    if (!this->data_cache) {
	printk ("Unable to allocate NAND data cache for Poodle.\n");
	kfree (this->data_buf);
	kfree (sharp_sl_mtd);
	return -ENOMEM;
    }

    /* Register the partitions */
    nr_partitions = parse_cmdline_partitions(sharp_sl_mtd,
					     &sharp_sl_nand_partition_info,
					     "sharpsl-nand");
    if (nr_partitions <= 0) {
	nr_partitions = DEFAULT_NUM_PARTITIONS;
	sharp_sl_nand_partition_info = sharp_sl_nand_default_partition_info;
    }
    sharp_sl_nand_part_mtdp = kmalloc(sizeof (struct mtd_info*) * nr_partitions,
				      GFP_KERNEL);
    if (! sharp_sl_nand_part_mtdp) {
	printk("Unable to allocate memory for sharp_sl_nand_part_mtdp\n");
	kfree(this->data_buf);
	kfree(this->data_cache);
	kfree(sharp_sl_mtd);
	return -ENOMEM;
    }
    for (i = 0; i < nr_partitions; i++)
	sharp_sl_nand_partition_info[i].mtdp = &sharp_sl_nand_part_mtdp[i];
    add_mtd_partitions(sharp_sl_mtd, sharp_sl_nand_partition_info, nr_partitions);

    for (i = 0; i < nr_partitions; i++)
	add_mtd_device(sharp_sl_nand_part_mtdp[i]);

#ifdef DEBUG_PROC
    struct proc_dir_entry* res = create_proc_read_entry("mtd-debug", 0, NULL, sharp_sl_nand_read_proc, NULL);
    res->write_proc = sharp_sl_nand_write_proc;
#endif

    /* Return happy */
    return 0;
}
module_init(sharp_sl_nand_init);

/*
 * Clean up routine
 */
#ifdef MODULE
static void __exit sharp_sl_nand_cleanup (void)
{
	struct nand_chip *this = (struct nand_chip *) &sharp_sl_mtd[1];

	/* Unregister the device */
	del_mtd_partitions (sharp_sl_mtd);
	int i;
	for (i = 0; i < NUM_PARTITIONS)
	    del_mtd_device (sharp_sl_nand_part_mtdp[i]);

	/* Free internal data buffer */
	kfree (this->data_buf);
	kfree (this->data_cache);
	kfree (this->page_cache);

	/* Free the MTD device structure */
	kfree (sharp_sl_nand_part_mtdp);
	if (sharp_sl_nand_partition_info != sharp_sl_nand_default_partition_info)
	    kfree (sharp_sl_nand_partition_info);
	kfree (sharp_sl_mtd);
}
module_exit(sharp_sl_nand_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Japan, Inc.");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on Poodle");
