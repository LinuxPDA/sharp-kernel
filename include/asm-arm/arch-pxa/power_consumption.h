/*  linux/asm-arm/arch-pxa/power_consumption.h
 *
 * Head file for power driver.
 *
 * Copyright (C) 2002 Steve Lin (stevelin168@hotmail.com)
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * ChangeLog:
 *	27-AUG-2002 Steve Lin for Discovery
 */

#define	VOLUME_VAL		(1<<0) /* bit 2,1,0 */
#define	BRIGHT_VAL		(1<<3) /* bit 5,4,3 */
#define	SD_WASTE		(1<<6)
#define	IRDA_WASTE		(1<<7)
#define	FLIGHT_WASTE		(1<<8)
#define	AUDIO_WASTE		(1<<9)
#define	BACKPACK_WASTE	(1<<10)
#define	NFLED_on_waste	(1<<11)
#define	NFLED_blink_waste	(1<<12)
#define	VOLUME_MASK		0x07		/* 0b00000111 */
#define	BRIGHT_MASK		0x38		/* 0b00111000 */
#define	NFLED_mask		0x1800		/* 0b0001100000000000; bit 12, 11 */
