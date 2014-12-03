/*
 *  linux/include/asm/sharp_mmcsd.h
 *
 * Header file for SD/MMC card driver (SHARP)
 *
 * Copyright (C) 2001  SHARP
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
 *	12-Nov-2001 Lineo Japan, Inc.
 */

#ifndef __ASM_SHARP_MMCSD_H_INCLUDED
#define __ASM_SHARP_MMCSD_H_INCLUDED

#include <linux/major.h>

#define MMC_DEBUG        0

#define MAX_MMC_SLOTS    1

typedef enum {
  MMCCard,
  SDCard
} mmcsd_card_type;

typedef enum {
  MMCClockNegotiate,
  MMCClockTransfer
} mmc_modeclockspeed;

typedef enum {
  SDClockNegotiate,
  SDClockTransfer
} sd_modeclockspeed;

typedef struct mmcsd_cid_struct {
  __u32 manufacture;
  __u16 application;		/* sd only */
  __u8  product_name[8];	/* mmc [8] / sd [6] */
  __u32 hw_revision;
  __u32 fw_revision;
  __u32 serial_number;
  __u32 month;
  __u32 year;
} mmcsd_cid_struct;

typedef struct mmcsd_csd_struct {
  __u8   csdstructure;
  __u8   mmcprot;		/* mmc only */
  __u8   taac;
  __u8   nsac;
  __u8   transpeed;
  __u16  ccc;
  __u8   readbllen;
  __u8   readblpartial;
  __u8   writeblkmisalign;
  __u8   readblkmisalign;
  __u8   dsrimp;
  __u16  csize;
  __u8   vddrcurrmin;
  __u8   vddrcurrmax;
  __u8   vddwcurrmin;
  __u8   vddwcurrmax;
  __u8   csizemult;
  __u8   sectorsize;
  __u8   erasegrpsize;		/* mmc only */
  __u8   erase_blk_en;		/* sd only */
  __u8   wpgrpsize;
  __u8   wpgrpenable;
  __u8   defaultecc;		/* mmc only */
  __u8   r2wfactor;
  __u8   writebllen;
  __u8   writeblpartial;
  __u8   fileformatgrp;		/* sd only */
  __u8   copy;
  __u8   permwriteprotect;
  __u8   tmpwriteprotect;
  __u8   fileformat;
  __u8   ecc;			/* mmc only */
  __u8   crc;			/* sd only */
} mmcsd_csd_struct;

typedef struct mmcsd_card_struct {
  __u32 readblksize;
  __u32 writeblksize;
  __u32 blocks;
  __u32 capacity;
  __u32 max_speed;
  mmcsd_cid_struct cid;
  mmcsd_csd_struct csd;
  mmcsd_card_type type;
} mmcsd_card_struct;

typedef struct mmcsd_response_format {
  __u32 w0; /* bits 31:0 */
  __u32 w1; /* bits 63:32 */
  __u32 w2; /* bits 95:64 */
  __u32 w3; /* bits 127:96 */
  __u32 w4; /* bits 133:128 */
} mmcsd_response_format;

typedef struct mmcsd_busctrl {
  void  (*open)(void);
  void  (*release)(void);
  void  (*reset)(void);
  void  (*setclock)(mmc_modeclockspeed,struct mmcsd_card_struct*);
  void  (*setlength)(__u32);
  void  (*setmultilength)(__u32,__u32);
  void  (*put_pushpull)(__u32,__u32);
  void  (*put_opendrain)(__u32,__u32);
  __u32 (*wait_response)(void);
  __u32 (*wait_id_response)(void);
  void  (*get_resp)(mmcsd_response_format* format);
  void  (*wait_complete)(void);
  __u32 (*receive)(void*,__u32);
  __u32 (*transmit)(void*,__u32);
  __u32 (*select)(__u32);
  void  (*deselect)(void);
  __u32 (*before_receive)(struct mmcsd_card_struct*,__u32,void*,int);
  __u32 (*after_receive)(struct mmcsd_card_struct*,__u32,void*,int);
  __u32 (*before_transmit)(struct mmcsd_card_struct*,__u32,void*,int);
  __u32 (*after_transmit)(struct mmcsd_card_struct*,__u32,void*,int);
  int   (*write_protected)(void);
} mmcsd_busctrl;

struct mmcsd_protoctrl;

typedef struct mmcsd_drive_struct {
  struct mmcsd_busctrl* bus;
  struct mmcsd_protoctrl* proto;
  __u8 using;
  int rca;
  mmcsd_card_struct card;
  int use_count;
} mmcsd_drive_struct;

typedef struct mmcsd_protoctrl {
  int (*read_block)(mmcsd_drive_struct*,__u32,void*,int);
  int (*write_block)(mmcsd_drive_struct*,__u32,void*,int);
  int (*sync)(mmcsd_drive_struct*);
} mmcsd_protoctrl;

typedef struct mmcsd_bus_info {
  char* name;
  struct mmcsd_busctrl* ctrl;
  int (*initbus)(struct mmcsd_busctrl*); /* return non-zero if fails */
  int (*resetbus)(struct mmcsd_busctrl*);
  int (*quitbus)(struct mmcsd_busctrl*);
  int (*detect_insert)(struct mmcsd_busctrl* bus); /* return number of inserted cards */
  int (*detect_removal)(struct mmcsd_busctrl* bus); /* return number of removed cards */
  int (*install_cardirq)(struct mmcsd_bus_info* info);
  int (*deinstall_cardirq)(struct mmcsd_bus_info* info);
  int (*enable_cardirq)(struct mmcsd_bus_info* info);
  int (*disable_cardirq)(struct mmcsd_bus_info* info);
  int (*suspend)(struct mmcsd_busctrl*);
  int (*resume)(struct mmcsd_busctrl*);
} mmcsd_bus_info;

/*
 * generic interfaces
 */

int mmc_resetbus(mmcsd_busctrl* bus);
int mmc_initbus(mmcsd_busctrl* bus);
int mmc_quitbus(mmcsd_busctrl* bus);
int mmc_detect_insert(mmcsd_busctrl* bus);
int mmc_detect_removal(mmcsd_busctrl* bus);

int sd_resetbus(mmcsd_busctrl* bus);
int sd_initbus(mmcsd_busctrl* bus);
int sd_quitbus(mmcsd_busctrl* bus);
int sd_suspend(mmcsd_busctrl* bus);
int sd_resume(mmcsd_busctrl* bus);
int sd_detect_insert(mmcsd_busctrl* bus);
int sd_detect_removal(mmcsd_busctrl* bus);

void mmcdsd_gencheckcard(mmcsd_bus_info* info);

/* -------------------------------------------------------
 *  internal error codes
 * ------------------------------------------------------- */

#define RESPONSE_ERR_RESPONSE_CRC      1
#define RESPONSE_ERR_RESPONSE_TIMEOUT  2
#define RESPONSE_ERR_CID_TIMEOUT       3
#define TRANSMIT_TOOMANY_LOOP          4


#endif /* __ASM_SHARP_MMCSD_H_INCLUDED */

