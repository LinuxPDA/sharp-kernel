/*
 *  linux/include/asm-arm/sharp_nand_param.h
 *
 * Definitions of the NAND partition accessed via logical address (SHARP)
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
 */

#ifndef _SHARP_NAND_PARAM_H_INCLUDED
#define _SHARP_NAND_PARAM_H_INCLUDED

//
//  Parameter Area Map
//
#define PARAM_BLOCK_ADJVALUE		0x00040000
#define PARAM_BLOCK_BOOTFLAG		0x00044000
#define PARAM_BLOCK_VERSION		0x00048000
#define PARAM_BLOCK_CLOCK		0x0004C000
#define PARAM_BLOCK_ROMOUNT		0x00050000
#define PARAM_BLOCK_RWMOUNT		0x00054000
#define PARAM_BLOCK_RSV_58		0x00058000
#define PARAM_BLOCK_RSV_5C		0x0005C000
#define PARAM_BLOCK_PARTITIONINFO1	0x00060000
#define PARAM_BLOCK_PARTITIONINFO2	0x00064000
#define PARAM_BLOCK_MODEL		0x00068000
#define PARAM_BLOCK_RSV_6C		0x0006C000
#define PARAM_BLOCK_MVERSION		0x00070000
#define PARAM_BLOCK_RSV_74_		0x00074000	// can not use this block
#define PARAM_BLOCK_RSV_78		0x00078000	// can not use this block because of BUG

//
//  BOOTFLAG
//
#define COLLIE_BOOT_DIAG_REMOTE_FLAG	"DIAR"
#define COLLIE_BOOT_DIAG_NORMAL_FLAG	"DIAG"
#define COLLIE_BOOT_DIAG_SERVICE_FLAG	"DIAS"
#define COLLIE_BOOT_MNTE_NORMAL_FLAG	"MNTE"
#define COLLIE_BOOT_MNTE_UPDTUSB_FLAG	"MUSB"
#define COLLIE_BOOT_MNTE_UPDTCF_FLAG	"MNCF"
#define COLLIE_BOOT_MNTE_UPDTSD_FLAG	"MNSD"
#define COLLIE_BOOT_FLAG_LEN		(4)

//
//  VERSION
//
typedef struct PARAM_VERSION_ENTRY {
	unsigned long		magic;
	unsigned char		name[12];
	unsigned long		prgid;
	unsigned long		startadr;
	unsigned long		endadr;
	unsigned long 		lpflag;
	unsigned char		pex_str[6];
	unsigned char 		pex_ver[8];
	unsigned short		sum;
} PARAM_VERSION_ENTRY;

#define PARAM_VERSION_MAGIC		0xaa55aa55
#define PARAM_VERSION_LOG_FLAG		0x4c4c4c4c	//LLLL
#define PARAM_VERSION_PHY_FLAG		0x50505050	//!LLLL
#define PARAM_VERSION_EX_STR		"SHARP!"

#define PARAM_PROGRAM_ID_GENERAL	0
#define PARAM_PROGRAM_ID_MAINTE		1
#define PARAM_PROGRAM_ID_DIAG		2
#define PARAM_PROGRAM_ID_2ND_KERNEL	3
#define PARAM_PROGRAM_ID_2ND_FS		4
#define PARAM_PROGRAM_ID_1ST_KERNEL	5
#define PARAM_PROGRAM_ID_1ST_FSRO	6
#define PARAM_PROGRAM_ID_1ST_FSRW	7

#define PARAM_VERSION_ENTRY_MAX		24
#define PARAM_VERSION_VALID_OFS		0x1000

//
//  PARTITIONINFO1,2
//
typedef struct NAND_PARTITION_INFO {
	unsigned long startadr;
	unsigned long endadr;
	unsigned char magic[4];
	unsigned long reserve;
} NAND_PARTITION_INFO;

#define PARAM_PARTITIONINFO_BOOT	"BOOT"
#define PARAM_PARTITIONINFO_FSRO	"FSRO"
#define PARAM_PARTITIONINFO_FSRW	"FSRW"

#endif

