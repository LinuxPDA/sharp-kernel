/*
 *  linux/indlude/asm-arm/arch-pxa/i2sc.h
 *
 *  Driver for I2c interface on Xscale
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
 */

#define I2C_ISR_RWM	0x0001
#define I2C_ISR_UB	0x0004
#define I2C_ISR_ITE	0x0040
#define I2C_ISR_IRF	0x0080
#define I2C_ISR_ACK	0x0002
#define I2C_ISR_IBB	0x0008
//#define I2C_ISR_

#define I2C_ICR_ALDIE	0x1000
#define I2C_ICR_SADIE	0x2000
#define I2C_ICR_SSDIE	0x0800
#define I2C_ICR_BEIE	0x0400
#define I2C_ICR_IRFIE	0x0200
#define I2C_ICR_ITEIE	0x0100
#define I2C_ICR_IUE		0x0040
#define I2C_ICR_SCLE	0x0020
#define I2C_ICR_TB		0x0008
#define I2C_ICR_STOP	0x0002
#define I2C_ICR_START	0x0001
#define I2C_ICR_MA		0x0010
#define I2C_ICR_IRF		0x0200
#define I2C_ICR_ACK		0x0004
#define I2C_ICR_UR		0x4000
#define I2C_ICR_FM		0x8000
