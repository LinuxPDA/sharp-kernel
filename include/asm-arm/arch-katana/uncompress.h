/*
 *  linux/include/asm-arm/arch-integrator/uncompress.h
 *
 *  Copyright (C) 1999 ARM Limited
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
 */

#ifndef _KATANA_UNCOMPRESS_H
#define _KATANA_UNCOMPRESS_H

#include <asm/arch/platform.h>
#include <asm/arch/MQK_Uart.h>

#if 1
#define	puts(x)
#define	puts_hex(x, y)
#define	puts2(x, mode)
#define	puts_hex2(x, y, mode)
#else

static void puts_hex2(const char *s, unsigned long val, int mode)
{
	//use physical addresses
	//Actually, the default boot code uses UART1
	unsigned long *pUART;
        unsigned long j;
	int	i;
	pUART = (unsigned long *)( ( mode ) ?
		(MQ9000_REGS_VBASE + MQ_REG_PHYS_OFFSET(MQ9000_UART1_BASE))
 		: MQ9000_UART1_BASE );
	while (*s) {
		while ( !(*(pUART + 0x4) & 0x80) )
			barrier();
		*(pUART + 0x3) = *s;
		if ( *s == '\n' )
		{	//should not have newline here for puts_hex
			//since a number is following it
			while ( !(*(pUART + 0x4) & 0x80) )
				barrier();
			*(pUART + 0x3) = '\r';
		}
		s++;
	}
	i = 32;
	do
	{
		i -= 4;
		j = (val >> i) & 0xF;
		//val &= (0xF << i);
	}
	while ( (0==j) && i );
        do
        {
		while ( !(*(pUART + 0x4) & 0x80) ) barrier();
		if ( j < 10 )
			*(pUART + 0x3) = j + '0';
		else
			*(pUART + 0x3) = j - 10 + 'A';
		if ( i <= 0 )
			break;
		i -= 4;
		j = (val >> i) & 0xF;
      	} while( 1 );
	while ( !(*(pUART + 0x4) & 0x80) ) barrier();
	*(pUART + 0x3) = '\n';
	while ( !(*(pUART + 0x4) & 0x80) ) barrier();
	*(pUART + 0x3) = '\r';
}

static void puts2(const char *s, int mode)
{
	//use physical addresses
#if 1
	//Actually, the default boot code uses UART1
	unsigned long *pUART;
	pUART = (unsigned long *)( ( mode ) ?
		(MQ9000_REGS_VBASE + MQ_REG_PHYS_OFFSET(MQ9000_UART1_BASE))
 		: MQ9000_UART1_BASE );
	while (*s) {
		while ( !(*(pUART + 0x4) & 0x80) ) barrier();
		*(pUART + 0x3) = *s;
		if ( *s == '\n' )
		{
			while ( !(*(pUART + 0x4) & 0x80) ) barrier();
			*(pUART + 0x3) = '\r';
		}
		s++;
	}
#else
	MQKTUARTReg	*pUART;
	pUART = (MQKTUARTReg *)(MQ9000_UART2_BASE);

	while (*s) {
		while (!(pUART->inStat & MQ_TXFIFO_EMPTY))
			barrier();
		pUART->txBufSize = 1;
		if ( *s == '\n' )
			pUART->txData = '\r';
		else
			pUART->txData = *s;
		s++;
	}
#endif
}

static void puts(const char *s)
{ puts2(s, 0); }

static void puts_hex(const char *s, unsigned long val)
{ puts_hex2(s, val, 0); }

#endif

/*
 * nothing to do
 */
#define arch_decomp_setup()

#define arch_decomp_wdog()
#endif	// _KATANA_UNCOMPRESS_H

