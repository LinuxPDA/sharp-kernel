/*
 *  linux/include/asm-arm/sharp_nand_logical.h
 *
 * Header file for NAND accessing via logical address (SHARP)
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

#ifndef __SHARP_NAND_LOGICAL_H__
#define __SHARP_NAND_LOGICAL_H__

int
sharp_sl_nand_cleanup_laddr(struct mtd_info* mtd);
int
sharp_sl_nand_read_laddr(struct mtd_info* mtd,
			 loff_t from,
			 size_t len,
			 u_char* buf);
int
sharp_sl_nand_write_laddr(struct mtd_info* mtd,
			  loff_t to,
			  size_t len,
			  u_char* buf,
			  int (*eraseproc)(struct mtd_info *mtd, u_int32_t addr));

#endif
