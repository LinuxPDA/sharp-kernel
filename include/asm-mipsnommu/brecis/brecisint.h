/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * Defines for the Atlas interrupt controller.
 *
 */
#ifndef _BRECISINT_H
#define _BRECISINT_H

/* Number of IRQ supported on hw interrupt 0. */
#define BRECISINT_UART      0
#define BRECISINT_END      32

/* 
 * Brecis registers are memory mapped on 64-bit aligned boundaries and 
 * only word access are allowed.
 */
struct brecis_ictrl_regs {
        volatile unsigned long intstatus;
        volatile unsigned long intenable;
};

extern void brecisint_init(void);

#endif /* !(_BRECISINT_H) */
