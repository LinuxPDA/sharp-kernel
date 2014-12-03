/*
 *  drivers/mtd/dbmx2.c
 *
 *  Copyright (C) 2004 Lineo Solutions, Inc.
 *
 * Based on:
 *  drivers/mtd/nand/mx2_nand_map.c
 *  drivers/mtd/nand/spia.c
 *
 *  Copyright (C) 2000 Steven J. Hill (sjhill@cotw.com)
 *
 *
 *	10-29-2001 TG	change to support hardwarespecific access
 *			to controllines	(due to change in nand.c)
 *			page_cache added
 *
 * $Id: spia.c,v 1.16 2002/03/05 13:50:47 dwmw2 Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 */

///@file 	mx2_nand_map.c
///@brief 	define mx2 nand flash partition
///
///@author 	Frank Li(Zhili@motorola.com)
///@version	$Version$


#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

#include <asm/arch/mx2.h>
#include "mx2_nand.h"

/**
 * MTD structure for MX2ADS  board
 */
static struct mtd_info *mx2_mtd = NULL;

/**
 * Define partitions for flash device
 */

const static struct mtd_partition partition_info[] = {
	{ name: "ipl",
	  offset: 0,
	  size: 0x04000,
	},
	{ name: "spl",
	  offset: MTDPART_OFS_APPEND,
	  size:   0x100000-0x4000
	},
	{ name: "kernel",
	  offset: 0x100000,
	  size:   0x200000
	},
	{ name: "RootDisk",
	  offset: 0x300000,
	  size:  0x1d00000
	},
};

///Define Number of Partitions
#define NUM_PARTITIONS (sizeof(partition_info) / sizeof(struct mtd_partition))

int mx2_nand_probed;

///Call this function when operator MX2 NAND Controler
void mx2_nand_preset()
{
    // _reg_NFC_CONFIGURATION=0x10; //unlocked first 2 page buffer
                                    //We don't use first 2 page buffer
    _reg_NFC_CONFIGURATION=0x0002;
    _reg_NFC_ULOCK_START_BLK=0;
    _reg_NFC_ULOCK_END_BLK=0xffff;
    _reg_NFC_NF_WR_PROT=0x0004;
    _reg_NFC_NF_CONFIG1=0x0002;
    _reg_NFC_RAM_BUF_ADDR=0x3;          
                      
}

int mx2_nand_command(int command)
{
    _reg_NFC_NF_CONFIG2&=~NAND_FLASH_CONFIG2_INT;
    
    _reg_NFC_NAND_FLASH_CMD=command;
    _reg_NFC_NF_CONFIG2&=~NAND_FLASH_CONFIG2_FCMD;
    _reg_NFC_NF_CONFIG2|=NAND_FLASH_CONFIG2_FCMD;
    
    WAIT((!(_reg_NFC_NF_CONFIG2&NAND_FLASH_CONFIG2_INT)),100000);
    if(!(_reg_NFC_NF_CONFIG2&NAND_FLASH_CONFIG2_INT))
        return ERR_TIME_OUT;
    
    return 0;
}

int mx2_nand_address(int addr)
{
    _reg_NFC_NF_CONFIG2&=~NAND_FLASH_CONFIG2_INT;
    _reg_NFC_NAND_FLASH_ADDR=addr;
    
    _reg_NFC_NF_CONFIG2&=~NAND_FLASH_CONFIG2_FADD;
    _reg_NFC_NF_CONFIG2|=NAND_FLASH_CONFIG2_FADD;
    
    WAIT(!(_reg_NFC_NF_CONFIG2&NAND_FLASH_CONFIG2_INT),1000);
    if(!(_reg_NFC_NF_CONFIG2&NAND_FLASH_CONFIG2_INT))
	return ERR_TIME_OUT;
    return 0;
}

void mx2_nand_read(unsigned char *buf, int start, int end, unsigned io)
{
    int pos, end2;
    unsigned int data, *p1, *p2;

    pos = start;
    p1 = (unsigned int *)buf;
    p2 = (unsigned int *)(io + (pos & ~3));
    if (pos & 3) {
	data = *p2++ >> ((pos & 3) * 8);
	end2 = (pos + 3) & ~3;
	if (end2 > end)
	    end2 = end;
	for (; pos < end2; pos++) {
	    *((unsigned char *)p1)++ = data & 0xff;
	    data >>= 8;
	}
    }
    end2 = end & ~3;
    for (; pos < end2; pos += 4)
	*p1++ = *p2++;
    if (pos < end) {
	data = *p2;
	for (; pos < end; pos++) {
	    *((unsigned char *)p1)++ = data & 0xff;
	    data >>= 8;
	}
    }
}

void mx2_nand_write(char *databuf, char *oobbuf, struct mtd_info *mtd)
{
    int cnt;
    unsigned int *p1, *p2;

    p1 = (unsigned int *)databuf;
    p2 = (unsigned int *)NFC_MAB3_BASE;
    cnt = mtd->oobblock / 4;
    while (cnt-- > 0)
	*p2++ = *p1++;
    p1 = (unsigned int *)oobbuf;
    p2 = (unsigned int *)NFC_SAB3_BASE;
    cnt = mtd->oobsize / 4;
    while (cnt-- > 0)
	*p2++ = *p1++;
}

void mx2_nand_write_oob(char *oobbuf, struct mtd_info *mtd)
{
    int cnt;
    unsigned int *p1, *p2;

    p2 = (unsigned int *)NFC_MAB3_BASE;
    cnt = mtd->oobblock / 4;
    while (cnt-- > 0)
	*p2++ = 0xffffffff;
    p1 = (unsigned int *)oobbuf;
    p2 = (unsigned int *)NFC_SAB3_BASE;
    cnt = mtd->oobsize / 4;
    while (cnt-- > 0)
	*p2++ = *p1++;
}

/* 
 *	hardware specific access to control-lines
*/
void mx2_hwcontrol(int cmd)
{
	switch (cmd) {
	case NAND_CTL_SETCLE:
		break;
	case NAND_CTL_CLRCLE:
		break;
	case NAND_CTL_SETALE:
		break;
	case NAND_CTL_CLRALE:
		break;
	case NAND_CTL_SETNCE:
		mx2_nand_preset();
		break;
	case NAND_CTL_CLRNCE:
		break;
	}
}

/*
*	read device ready pin
*/
int mx2_device_ready(void)
{
	return (_reg_NFC_NF_CONFIG2 & NAND_FLASH_CONFIG2_INT) ? 1 : 0;
}

/**
 * Main initialization routine
 * 
 * This is first function called by linux system. It will init data and call nand_scan to
 * search system nand flash
 * 
 */
int __init mx2_init (void)
{
	struct nand_chip *this;

	/* Allocate memory for MTD device structure and private data */
	mx2_mtd = kmalloc (sizeof(struct mtd_info) + sizeof (struct nand_chip),
				GFP_KERNEL);
	if (!mx2_mtd) {
		printk ("Unable to allocate DBMX2 NAND MTD device structure.\n");
		return -ENOMEM;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *) (&mx2_mtd[1]);

	/* Initialize structures */
	memset((char *) mx2_mtd, 0, sizeof(struct mtd_info));
	memset((char *) this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	mx2_mtd->priv = this;

	/*
	 * Set GPIO Port E control register so that the pins are configured
	 * to be outputs for controlling the NAND flash.
	 */
	/* Set address of NAND IO lines */
	this->IO_ADDR_R = NFC_MAB3_BASE;
	this->IO_ADDR_W = NFC_MAB3_BASE;
	this->hwcontrol = mx2_hwcontrol;
	this->dev_ready = mx2_device_ready;
	/* 20 us command delay time */
	this->chip_delay = 20;		
	this->eccmode = NAND_ECC_SOFT;

	/* Set address of hardware control function */

	_reg_CRM_PCDR=(_reg_CRM_PCDR&0xFFFF0FFF)|0x7000;

	_reg_GPIO_GIUS(GPIOF) &= ~0xfb807fff;
	_reg_GPIO_GPR(GPIOF)  |=  0xfb800000;
	_reg_GPIO_GPR(GPIOF)  &= ~0x00007fff;

	/* Scan to find existence of the device */
	if (nand_scan (mx2_mtd)) {
		kfree (mx2_mtd);
		return -ENXIO;
	}

	/* Allocate memory for internal data buffer */
	this->data_buf = kmalloc (sizeof(u_char) * (mx2_mtd->oobblock + mx2_mtd->oobsize), GFP_KERNEL);
	if (!this->data_buf) {
		printk ("Unable to allocate NAND data buffer for Mx2.\n");
		kfree (mx2_mtd);
		return -ENOMEM;
	}

	/* Allocate memory for internal data buffer */
	this->data_cache = kmalloc (sizeof(u_char) * (mx2_mtd->oobblock + mx2_mtd->oobsize), GFP_KERNEL);
	if (!this->data_cache) 
	{
		printk ("Unable to allocate NAND data cache for Mx2.\n");
		kfree (this->data_buf);
		kfree (mx2_mtd);
		return  -ENOMEM;
	}
	this->cache_page = -1;

	/* Register the partitions */
	add_mtd_partitions(mx2_mtd, partition_info, NUM_PARTITIONS);

	mx2_nand_probed = 1;

	/* Return happy */
	return 0;
}
module_init(mx2_init);

/**
 * Clean up routine
 */
#ifdef MODULE
static void __exit mx2_cleanup (void)
{
	struct nand_chip *this = (struct nand_chip *) &mx2_mtd[1];

	/* Unregister the device */
	del_mtd_device (mx2_mtd);

	/* Free internal data buffer */
	kfree (this->data_buf);
	kfree (this->page_cache);

	/* Free the MTD device structure */
	kfree (mx2_mtd);
}
module_exit(mx2_cleanup);
#endif

MODULE_LICENSE("GPL");
