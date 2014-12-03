/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
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
 * Defines of the Atlas board specific address-MAP, registers, etc.
 *
 */
#ifndef _MIPS_BRECIS_H
#define _MIPS_BRECIS_H

#include <asm/addrspace.h>


/*
 * Brecis interrupt controller register base.
 */
#define BRECIS_ICTRL_REGS_BASE   (KSEG1ADDR(0x1f000000))

/*
 * Brecis UART register base.
 */
#define BRECIS_UART_BASE	(0x1c000100 + KSEG1)
#define BRECIS_UART_REGS_BASE    (volatile unsigned int *) (BRECIS_UART_BASE)
    // ...MaTed--- wish the value was 24.576 instead 
#define BRECIS_BASE_BAUD ( 25000000 / 16 ) 

#endif /* !(_MIPS_BRECIS_H) */
