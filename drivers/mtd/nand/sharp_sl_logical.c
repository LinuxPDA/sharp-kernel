/*
 *  drivers/mtd/nand/sharp_sl_logical.c
 *
 * Copyright (C) 2002  SHARP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Change Log
 *
 *  28-Feb-2005 Sharp Corporation for Akita
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <asm-arm/sharp_nand_logical.h>

/////////////////////////////////////////////////////////////////////
// oob structure
/////////////////////////////////////////////////////////////////////

#define NAND_NOOB_LOGADDR_00		8
#define NAND_NOOB_LOGADDR_01		9
#define NAND_NOOB_LOGADDR_10		10
#define NAND_NOOB_LOGADDR_11		11
#define NAND_NOOB_LOGADDR_20		12
#define NAND_NOOB_LOGADDR_21		13

/////////////////////////////////////////////////////////////////////
// Logical Table
/////////////////////////////////////////////////////////////////////

#define WRITTEN_DUST		0
#define WRITTEN_LOGICAL		1
#define WRITTEN_JFFS2		2

struct block_usage {		/* the usage of block */
    u_char block_status;	/* 0xff : normal block     */
				/* 0xf0 : POST_BADBLOCK    */
				/* 0x00 : INITIAL_BADBLOCK */
    u_char block_written;	/* WRITTEN_DUST    : the DUST block is written */
				/* WRITTEN_LOGICAL : the LOGICAL block is written */
				/* WRITTEN_JFFS2   : the JFFS2 block is written */
};

struct mtd_logical {
    u_int32_t size;		/* partition size of the handled partition */
    int index;			/* mtd->index */

    u_int phymax;		/* # of usage */
    u_int logmax;		/* # of log2phy */

    u_int phynext;		/* ring pointer by phy-no for wear-leveling */

    u_char dirty;		/* the flag of dirty */

    struct block_usage *usage;	/* the usage of block */
    u_int *log2phy;		/* the logical-to-physical table */
};

static struct mtd_logical *sharp_sl_mtd_logical = NULL;

/////////////////////////////////////////////////////////////////////
// utility
/////////////////////////////////////////////////////////////////////

static unsigned char sharp_sl_nand_round_block_status(unsigned char status)
{
    unsigned char bit;
    int setbits = 0;

    if(status == 0xff){
	return 0xff;	// quick return
    }

    for(bit = 0x01; bit != 0; bit <<= 1){
	if((status & bit) != 0){
	    setbits++;
	}
    }

    if(setbits >= 7){
	return 0xff;		// Normal Block
    }else if(setbits <= 1){
	return 0x00;		// Bad Block
    }else{
	return 0x00;		// Bad Block
    }
}

static unsigned char sharp_sl_nand_get_block_status(unsigned char oob[])
{
    unsigned char status;

    status = sharp_sl_nand_round_block_status(oob[NAND_BADBLOCK_POS]);
    if(status != 0xff){
	return 0x00;
    }

    status = sharp_sl_nand_round_block_status(oob[NAND_POSTBADBLOCK_POS]);
    if(status != 0xff){
	return 0xf0;
    }

    return 0xff;
}

static void sharp_sl_nand_set_block_status(unsigned char status, unsigned char oob[])
{
	switch(status){
	case 0x00:
		oob[NAND_BADBLOCK_POS]     = 0x00;
		oob[NAND_POSTBADBLOCK_POS] = 0x00;
		break;
	case 0xf0:
		oob[NAND_BADBLOCK_POS]     = 0xff;
		oob[NAND_POSTBADBLOCK_POS] = 0x00;
		break;
	default:
		oob[NAND_BADBLOCK_POS]     = 0xff;
		oob[NAND_POSTBADBLOCK_POS] = 0xff;
		break;
	}
	return;
}

static u_int sharp_sl_nand_get_logical_no(unsigned char *oob)
{
    unsigned short us,bit;
    int par;
    int good0, good1;

    if(oob[NAND_NOOB_LOGADDR_00] == oob[NAND_NOOB_LOGADDR_10] &&
       oob[NAND_NOOB_LOGADDR_01] == oob[NAND_NOOB_LOGADDR_11]){
	good0 = NAND_NOOB_LOGADDR_00;
	good1 = NAND_NOOB_LOGADDR_01;
    }else
    if(oob[NAND_NOOB_LOGADDR_10] == oob[NAND_NOOB_LOGADDR_20] &&
       oob[NAND_NOOB_LOGADDR_11] == oob[NAND_NOOB_LOGADDR_21]){
	good0 = NAND_NOOB_LOGADDR_10;
	good1 = NAND_NOOB_LOGADDR_11;
    }else
    if(oob[NAND_NOOB_LOGADDR_20] == oob[NAND_NOOB_LOGADDR_00] &&
       oob[NAND_NOOB_LOGADDR_21] == oob[NAND_NOOB_LOGADDR_01]){
	good0 = NAND_NOOB_LOGADDR_20;
	good1 = NAND_NOOB_LOGADDR_21;
    }else{
	return (u_int)-1;
    }

    us = (((unsigned short)(oob[good0]) & 0x00ff) << 0) |
	 (((unsigned short)(oob[good1]) & 0x00ff) << 8);

    par = 0;
    for(bit = 0x0001; bit != 0; bit <<= 1){
	if(us & bit){
	    par++;
	}
    }
    if(par & 1){
	return (u_int)-2;
    }

    if(us == 0xffff){
	return 0xffff;
    }else{
	return ((us & 0x07fe) >> 1);
    }
}

static void sharp_sl_nand_set_logical_no(u_int log_no, unsigned char *oob)
{
    unsigned short us,bit;
    int par;

    us = (((log_no & 0x03ff) << 1) | 0x1000);

    par = 0;
    for(bit = 0x0001; bit != 0; bit <<= 1){
	if(us & bit){
	    par++;
	}
    }
    if(par & 1){
	us |= 0x0001;
    }

    oob[NAND_NOOB_LOGADDR_00] = (unsigned char)((us & 0x00ff) >> 0);
    oob[NAND_NOOB_LOGADDR_01] = (unsigned char)((us & 0xff00) >> 8);
    oob[NAND_NOOB_LOGADDR_10] = oob[NAND_NOOB_LOGADDR_00];
    oob[NAND_NOOB_LOGADDR_11] = oob[NAND_NOOB_LOGADDR_01];
    oob[NAND_NOOB_LOGADDR_20] = oob[NAND_NOOB_LOGADDR_00];
    oob[NAND_NOOB_LOGADDR_21] = oob[NAND_NOOB_LOGADDR_01];

    return;
}

static u_char sharp_sl_nand_is_ecc_ff(unsigned char *oob)
{
#if defined (CONFIG_ARCH_PXA_AKITA)
    unsigned char ecc[24];
    int i;
#else
    unsigned char ecc0, ecc1, ecc2, ecc3, ecc6, ecc7;
#endif

#if defined (CONFIG_ARCH_PXA_AKITA)
    ecc[0] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS0]);
    ecc[1] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS1]);
    ecc[2] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS2]);
    ecc[3] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS3]);
    ecc[4] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS4]);
    ecc[5] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS5]);
    ecc[6] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS6]);
    ecc[7] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS7]);
    ecc[8] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS8]);
    ecc[9] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS9]);
    ecc[10] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS10]);
    ecc[11] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS11]);
    ecc[12] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS12]);
    ecc[13] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS13]);
    ecc[14] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS14]);
    ecc[15] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS15]);
    ecc[16] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS16]);
    ecc[17] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS17]);
    ecc[18] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS18]);
    ecc[19] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS19]);
    ecc[20] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS20]);
    ecc[21] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS21]);
    ecc[22] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS22]);
    ecc[23] = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS23]);

    for (i=0; i <24 ; i++) 
        if (ecc[i] != 0xff) return 0;
    
    return 1;

#else
    ecc0 = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS0]);
    ecc1 = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS1]);
    ecc2 = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS2]);
    ecc3 = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS3]);
    ecc6 = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS4]);
    ecc7 = sharp_sl_nand_round_block_status(oob[NAND_NOOB_ECCPOS5]);


    if(ecc0 == 0xff &&
       ecc1 == 0xff &&
       ecc2 == 0xff &&
       ecc3 == 0xff &&
       ecc6 == 0xff &&
       ecc7 == 0xff){
	return 1;
    }else{
	return 0;
    }
#endif
}

static u_int sharp_sl_nand_search_free_block(void)
{
    struct mtd_logical *logical = sharp_sl_mtd_logical;
    u_int block_no;

    if(logical == NULL){
	return (u_int)-1;
    }

    for(block_no = logical->phynext; block_no < logical->phymax; block_no++){
	if(logical->usage[block_no].block_status == 0xff &&
	   logical->usage[block_no].block_written == WRITTEN_DUST){
	    logical->phynext = block_no + 1;
	    return block_no;
	}
    }

    for(block_no = 0; block_no < logical->phynext; block_no++){
	if(logical->usage[block_no].block_status == 0xff &&
	   logical->usage[block_no].block_written == WRITTEN_DUST){
	    logical->phynext = block_no + 1;
	    return block_no;
	}
    }

    return (u_int)-1;
}

// cleanup logical management.
static void sharp_sl_nand_cleanup_logical(void)
{
    struct mtd_logical *logical = sharp_sl_mtd_logical;

    sharp_sl_mtd_logical = NULL;

    if(logical != NULL){
	if(logical->log2phy != NULL){
	    kfree(logical->log2phy);
	    logical->log2phy = NULL;
	}
	if(logical->usage != NULL){
	    kfree(logical->usage);
	    logical->usage = NULL;
	}
	kfree(logical);
	logical = NULL;
    }

    return;
}

// initialize logical management.
static int sharp_sl_nand_init_logical(struct mtd_info *mtd, u_int32_t partition_size)
{
    struct mtd_logical *logical = NULL;
    u_int block_no;
    loff_t block_adr;
    u_int log_no;
    int unusable = 0;
#if defined (CONFIG_ARCH_PXA_AKITA)
    unsigned char oob[64];
#else
    unsigned char oob[16];
#endif
    int readretry;
    int ret;  
    int i;
    size_t retlen;

    // check argument
    if((partition_size % mtd->erasesize) != 0){
	printk("Illegal partition size. (%x)\n", partition_size);
	return -EINVAL;
    }
#if defined (CONFIG_ARCH_PXA_AKITA)
    if(mtd->oobblock != 2048 || mtd->oobsize != 64){
	printk("Unkknown oobblock/oobsize in sharp_sl_nand_logical_init()\n");
	return -EINVAL;
    }
#else
    if(mtd->oobblock != 512 || mtd->oobsize != 16){
	printk("Unknown oobblock/oobsize in sharp_sl_nand_logical_init()\n");
	return -EINVAL;
    }
#endif

    // Allocate memory for Logical Address Management
    logical = kmalloc(sizeof(struct mtd_logical), GFP_KERNEL);
    if(logical == NULL){
	printk("Unable to allocate NAND MTD Logical Address Management structure.\n");
	return -ENOMEM;
    }

    // initialize management structure
    memset((char*)logical, 0x00, sizeof(struct mtd_logical));
    logical->size = partition_size;
    logical->index = mtd->index;
    logical->phymax = (partition_size / mtd->erasesize);

#if defined (CONFIG_ARCH_PXA_AKITA)
    logical->logmax = (logical->phymax > 4) ? (logical->phymax - 4) : 1;
#else
    logical->logmax = (logical->phymax > 24) ? (logical->phymax - 24) : 1;
#endif

#if 1	// wear-leveling
    logical->phynext = (jiffies % logical->phymax);
#else
    logical->phynext = 0;
#endif
    logical->dirty = 0;
    logical->usage = NULL;
    logical->log2phy = NULL;

    // Allocate memory for logical->usage, logical->log2phy
    logical->usage = kmalloc(sizeof(struct block_usage) * logical->phymax, GFP_KERNEL);
    if(logical->usage == NULL){
	printk("Unable to allocate NAND MTD Logical Address Management structure.\n");
	kfree(logical);
	return -ENOMEM;
    }
    logical->log2phy = kmalloc(sizeof(u_int) * logical->logmax, GFP_KERNEL);
    if(logical->log2phy == NULL){
	printk("Unable to allocate NAND MTD Logical Address Management structure.\n");
	kfree(logical->usage);
	kfree(logical);
	return -ENOMEM;
    }

    // initialize logical->usage, logical->log2phy
    memset((char*)logical->usage, 0x00, sizeof(struct block_usage) * logical->phymax);
    memset((char*)logical->log2phy, 0x00, sizeof(u_int) * logical->logmax);
    for(i=0; i < logical->logmax; i++){
	logical->log2phy[i] = (u_int)-1;
    }

    // make physical-logical table
    for(block_no = 0; block_no < logical->phymax; block_no++){
	block_adr = block_no * mtd->erasesize;

	readretry = 3;
    ReadRetry:
	ret = (mtd->read_oob)(mtd, block_adr, mtd->oobsize, &retlen, oob);
	if(ret != 0){
	    printk("Failed in read_oob, skip this block.\n");
	    logical->usage[block_no].block_status = 0xf0;
	    logical->usage[block_no].block_written = WRITTEN_DUST;
	    unusable++;
	    continue;
	}
	if(mtd->oobsize != retlen){
	    printk("mtd->read_oob cannot read full-size.\n");
	    logical->usage[block_no].block_status = 0xf0;
	    logical->usage[block_no].block_written = WRITTEN_DUST;
	    unusable++;
	    continue;
	}

	// block status
	logical->usage[block_no].block_status = sharp_sl_nand_get_block_status(oob);
	if(logical->usage[block_no].block_status != 0xff){
	    logical->usage[block_no].block_written = WRITTEN_DUST;
	    unusable++;
	    continue;
	}

	// logical address
	log_no = sharp_sl_nand_get_logical_no(oob);
	if((int)log_no < 0){
	    printk("Bad logical no found.\n");
	    readretry--;
	    if(readretry > 0){
		goto ReadRetry;
	    }
	    logical->usage[block_no].block_written = WRITTEN_JFFS2;
	    unusable++;
	    continue;
	}

	// logical address available ?
	if(log_no == 0xffff){
	    if(sharp_sl_nand_is_ecc_ff(oob)){
		logical->usage[block_no].block_written = WRITTEN_DUST;
	    }else{
#if 1	// avoid check pattern
		logical->usage[block_no].block_written = WRITTEN_DUST;
#else
@		//printk(KERN_WARNING "JFFS2(?) block is found.\n");
@		logical->usage[block_no].block_written = WRITTEN_JFFS2;
@		unusable++;
#endif
	    }
	    continue;
	}
	if(log_no >= logical->logmax){
	    printk("The logical no is too big. (%d)\n", log_no);
	    printk("***patition(%d) erasesize(%d) logmax(%d) \n", partition_size, mtd->erasesize, logical->logmax);
	    logical->usage[block_no].block_written = WRITTEN_JFFS2;
	    unusable++;
	    continue;
	}

	// duplicate or not ?
	if(logical->log2phy[log_no] == (u_int)(-1)){
	    logical->usage[block_no].block_written = WRITTEN_LOGICAL;
	    logical->log2phy[log_no] = block_no;
	}else{
	    printk("The logical no is duplicated. (%d)\n", log_no);

	    // erase after...
	    logical->usage[block_no].block_written = WRITTEN_DUST;
	    continue;
	}
    }
#if defined (CONFIG_ARCH_PXA_AKITA)
    if(unusable > 4){
#else
    if(unusable > 24){
#endif
	printk("The unusable block is too much(%d). Your partition supports LOGICAL?\n", unusable);
	sharp_sl_mtd_logical = logical;
	sharp_sl_nand_cleanup_logical();
	return -EINVAL;
    }

    sharp_sl_mtd_logical = logical;
    return 0;
}

// rebuild management?
static int sharp_sl_nand_rebuild_logical(struct mtd_info *mtd)
{
    struct mtd_logical *logical;
    int ret;

    if(sharp_sl_mtd_logical){
	logical = sharp_sl_mtd_logical;

	if(logical->index == mtd->index &&
	   logical->dirty == 0){
	    return 0;
	}

	sharp_sl_nand_cleanup_logical();
    }

    // TODO-TMP
    // . partition size
    ret = sharp_sl_nand_init_logical(mtd, 7*1024*1024);

    return ret;
}

/////////////////////////////////////////////////////////////////////
// MTD METHOD
/////////////////////////////////////////////////////////////////////

int
sharp_sl_nand_cleanup_laddr(struct mtd_info* mtd)	// mtd is slave.
{
    sharp_sl_nand_cleanup_logical();
    return 0;
}

int
sharp_sl_nand_read_laddr(struct mtd_info* mtd,	// mtd is slave.
			 loff_t from,
			 size_t len,
			 u_char* buf)
{
    struct mtd_logical *logical;
    u_int log_no;
    u_int block_no;
    loff_t block_adr;
    loff_t block_ofs;
    size_t retlen;
    int ret;


    // support only for one block.
    if(len <= 0){
	printk("Illegal argument in sharp_sl_nand_read_laddr(). len=%x\n", len);
	return -EINVAL;
    }
    if((((u_int32_t)from) / mtd->erasesize) != ((((u_int32_t)from) + len - 1) / mtd->erasesize)){
	printk("Illegal argument in sharp_sl_nand_read_laddr(). from=%x len=%x\n", (u_int32_t)from, len);
	return -EINVAL;
    }

    // rebuild management?
    ret = sharp_sl_nand_rebuild_logical(mtd);
    if(ret != 0){
	printk("Rebuilding logical management is failed.\n");
	return -EINVAL;
    }
    logical = sharp_sl_mtd_logical;

    log_no = (((u_int32_t)from) / mtd->erasesize);
    if(log_no >= logical->logmax){
	printk("Illegal argument in sharp_sl_nand_read_laddr(). from=%x\n", (u_int32_t)from);
	return -EINVAL;
    }

    // Is log_no used ?
    if(logical->log2phy[log_no] == (u_int)-1){
	//printk(KERN_WARNING "There is not the LOGICAL block. from=%x\n", (u_int32_t)from);
	return -EINVAL;
    }

    // physical address
    block_no = logical->log2phy[log_no];
    block_adr = block_no * mtd->erasesize;
    block_ofs = (((u_int32_t)from) % mtd->erasesize);

    // read
    ret = (mtd->read)(mtd, block_adr + block_ofs, len, &retlen, buf);
    if(ret != 0){
	printk("mtd->read failed in sharp_sl_nand_read_laddr()\n");
	return ret;
    }
    if(len != retlen){
	printk("mtd->read cannot read full-size.\n");
	return -EINVAL;
    }

    return 0;
}

int
sharp_sl_nand_write_laddr(struct mtd_info* mtd,	// mtd is slave.
			  loff_t to,
			  size_t len,
			  u_char* buf,
			  int (*eraseproc)(struct mtd_info *mtd, u_int32_t addr))
{
    struct mtd_logical *logical;
    u_char is_all;
    int page_num, page_idx;
    u_int log_no;
    u_int block_no, block_no_read;
    loff_t block_adr, block_adr_read;
    loff_t block_ofs;
    size_t retlen;
    u_char *block, *oobs;
    u_char *blockv, *oobv;
    int ret;
#if defined (CONFIG_ARCH_PXA_AKITA)
    dma_addr_t	oobs_phys, oobv_phys;   
#endif

    // support only for one block.
    if(len <= 0){
	printk("Illegal argument in sharp_sl_nand_write_laddr(). len=%x\n", len);
	return -EINVAL;
    }
    if((((u_int32_t)to) / mtd->erasesize) != ((((u_int32_t)to) + len - 1) / mtd->erasesize)){
	printk("Illegal argument in sharp_sl_nand_write_laddr(). to=%x len=%x\n", (u_int32_t)to, len);
	return -EINVAL;
    }

    block_ofs = ((u_int32_t)to % mtd->erasesize);
    if(block_ofs == 0 && len == mtd->erasesize){
	is_all = 1;
    }else{
	is_all = 0;
    }
    page_num = mtd->erasesize / mtd->oobblock;

    // rebuild management?
    ret = sharp_sl_nand_rebuild_logical(mtd);
    if(ret != 0){
	printk("Rebuilding logical management is failed.\n");
	return -EINVAL;
    }
    logical = sharp_sl_mtd_logical;

    log_no = (((u_int32_t)to) / mtd->erasesize);
    if(log_no >= logical->logmax){
	printk("Illegal argument in sharp_sl_nand_write_laddr(). to=%x\n", (u_int32_t)to);
	return -EINVAL;
    }

    // alloc buffer
    if(is_all){
	oobs = kmalloc(mtd->oobsize * page_num, GFP_KERNEL);
	if(oobs == NULL){
	    printk("Unable to allocate for oobs.\n");
	    return -ENOMEM;
	}
	block = buf;
    }
    else{
#if defined (CONFIG_ARCH_PXA_AKITA)
	oobs = consistent_alloc(GFP_KERNEL, mtd->oobsize * page_num + mtd->erasesize,&oobs_phys );
#else
	oobs = kmalloc(mtd->oobsize * page_num + mtd->erasesize, GFP_KERNEL);
#endif

	if(oobs == NULL){
	    printk("Unable to allocate for oobs.\n");
	    return -ENOMEM;
	}
	block = oobs + mtd->oobsize * page_num;
    }

    // alloc buffer for verifying
#if defined (CONFIG_ARCH_PXA_AKITA)
    oobv = consistent_alloc(GFP_KERNEL, mtd->oobsize + mtd->erasesize, &oobv_phys );
#else
    oobv = kmalloc(mtd->oobsize + mtd->erasesize, GFP_KERNEL);
#endif
    if(oobv == NULL){
	printk("Unable to allocate for oobv.\n");
#if defined (CONFIG_ARCH_PXA_AKITA)
	if (is_all) kfree(oobs);
	else consistent_free(oobs, mtd->oobsize * page_num + mtd->erasesize, oobs_phys );
#else
	kfree(oobs);
#endif
	return -ENOMEM;
    }
    blockv = oobv + mtd->oobsize;
    // old block
    block_no_read = logical->log2phy[log_no];
    block_adr_read = block_no_read * mtd->erasesize;

    // make block data
    if(!is_all){
	if(block_no_read == (u_int)-1){
	    // instead of reading
	    memset(block, 0xff, mtd->erasesize);
	}else{
	    // reading
	    ret = (mtd->read)(mtd, block_adr_read, mtd->erasesize, &retlen, block);
	    if(ret != 0){
		printk("mtd->read failed in sharp_sl_nand_write_laddr()\n");
#if defined (CONFIG_ARCH_PXA_AKITA)
		consistent_free(oobv, mtd->oobsize + mtd->erasesize, oobv_phys );
		if (is_all) kfree(oobs);
		else consistent_free(oobs, mtd->oobsize * page_num + mtd->erasesize, oobs_phys );
#else
		kfree(oobv);
		kfree(oobs);
#endif
		return ret;
	    }
	    if(mtd->erasesize != retlen){
		printk("mtd->read cannot read full-size.\n");
#if defined (CONFIG_ARCH_PXA_AKITA)
		consistent_free(oobv, mtd->oobsize + mtd->erasesize, oobv_phys );
		if (is_all) kfree(oobs);
		else consistent_free(oobs, mtd->oobsize * page_num + mtd->erasesize,oobs_phys );
#else
		kfree(oobv);
		kfree(oobs);
#endif
		return -EINVAL;
	    }
	}

	memcpy(block + block_ofs, buf, len);


    }
    // make oob data
    memset(oobs, 0xff, mtd->oobsize * page_num);
    for(page_idx = 0; page_idx < page_num; page_idx++){
	sharp_sl_nand_set_block_status(0xff, oobs + mtd->oobsize * page_idx);
	sharp_sl_nand_set_logical_no(log_no, oobs + mtd->oobsize * page_idx);
    }

    while(1){
	// search free block
	block_no = sharp_sl_nand_search_free_block();
	block_adr = block_no * mtd->erasesize;
	if(block_no == (u_int)-1){
	    printk("No usable block in sharp_sl_nand_write_laddr().\n");
#if defined (CONFIG_ARCH_PXA_AKITA)
	    consistent_free(oobv, mtd->oobsize + mtd->erasesize, oobv_phys );
	    if (is_all) kfree(oobs);
	    else consistent_free(oobs, mtd->oobsize * page_num + mtd->erasesize, oobs_phys );
#else
	    kfree(oobv);
	    kfree(oobs);
#endif
	    return -EIO;
	}

	// erase
	ret = eraseproc(mtd, block_adr);
	if(ret != 0){
	    printk("Can not erase the block. adr=(%x)\n", (u_int32_t)block_adr);
	    logical->usage[block_no].block_status = 0xf0;
	    logical->usage[block_no].block_written = WRITTEN_DUST;
	    continue;
	}

	// write
	ret = (mtd->write_ecc)(mtd, block_adr, mtd->erasesize, &retlen, block, oobs, NAND_JFFS2_OOB);
	if(ret != 0){
	    printk("Can not write_ecc the block. adr=(%x)\n", (u_int32_t)block_adr);
	    goto EraseAndNext;
	}
	if(mtd->erasesize != retlen){
	    printk("Can not write_ecc full-size. adr=(%x)\n", (u_int32_t)block_adr);
	    goto EraseAndNext;
	}

	// read oob
	ret = (mtd->read_oob)(mtd, block_adr, mtd->oobsize, &retlen, oobv);
	if(ret != 0){
	    printk("Failed in read_oob. adr=(%x)\n", (u_int32_t)block_adr);
	    goto EraseAndNext;
	}
	if(mtd->oobsize != retlen){
	    printk("mtd->read_oob cannot read full-size. adr=(%x)\n", (u_int32_t)block_adr);
	    goto EraseAndNext;
	}

	// verify oob (only first page)
	if(memcmp(oobs, oobv, mtd->oobsize) != 0){
	    printk("Verifying oob error. adr=(%x) oob=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		   (u_int32_t)block_adr,
		   oobs[0], oobs[1],  oobs[2],  oobs[3],  oobs[4],  oobs[5],  oobs[6],  oobs[7],
		   oobs[8], oobs[9], oobs[10], oobs[11], oobs[12], oobs[13], oobs[14], oobs[15],
		   oobv[0], oobv[1],  oobv[2],  oobv[3],  oobv[4],  oobv[5],  oobv[6],  oobv[7],
		   oobv[8], oobv[9], oobv[10], oobv[11], oobv[12], oobv[13], oobv[14], oobv[15]);
	    goto EraseAndNext;
	}

	// read block
	ret = (mtd->read)(mtd, block_adr, mtd->erasesize, &retlen, blockv);
	if(ret != 0){
	    printk("Can not read the writing block. adr=(%x)\n", (u_int32_t)block_adr);
	    goto EraseAndNext;
	}
	if(mtd->erasesize != retlen){
	    printk("Can not read the writing block full-size. adr=(%x)\n", (u_int32_t)block_adr);
	    goto EraseAndNext;
	}

	// verify block
	if(memcmp(block, blockv, mtd->erasesize) != 0){
	    printk("Verifying block error. adr=(%x)\n", (u_int32_t)block_adr);
	    goto EraseAndNext;
	}

	// I WROTE!
	break;

    EraseAndNext:
	logical->usage[block_no].block_status = 0xf0;
	logical->usage[block_no].block_written = WRITTEN_DUST;

	ret = eraseproc(mtd, block_adr);
	if(ret != 0){
	    printk("Can not erase the block. adr=(%x)\n", (u_int32_t)block_adr);
	}
    }
    // chain new block
    logical->log2phy[log_no] = block_no;
    logical->usage[block_no].block_written = WRITTEN_LOGICAL;

    // erase old block
    if(block_no_read != (u_int)-1){
	logical->usage[block_no_read].block_written = WRITTEN_DUST;

	ret = eraseproc(mtd, block_adr_read);
	if(ret != 0){
	    printk("Can not erase the old block. adr=(%x)\n", (u_int32_t)block_adr_read);
#if defined (CONFIG_ARCH_PXA_AKITA)
	    consistent_free(oobv, mtd->oobsize + mtd->erasesize, oobv_phys );
	    if (is_all) kfree(oobs);
	    else consistent_free(oobs, mtd->oobsize * page_num + mtd->erasesize, oobs_phys );
#else
	    kfree(oobv);
	    kfree(oobs);
#endif
	    return ret;
	}
    }
#if defined (CONFIG_ARCH_PXA_AKITA)
    consistent_free(oobv, mtd->oobsize + mtd->erasesize, oobv_phys );
    if (is_all) kfree(oobs);
    else consistent_free(oobs, mtd->oobsize * page_num + mtd->erasesize,oobs_phys );
#else
    kfree(oobv);
    kfree(oobs);
#endif
    return 0;
}

//////////////////////////////////////////////////////////
// TODO   dirty if NOT LOGICAL access.
//////////////////////////////////////////////////////////

