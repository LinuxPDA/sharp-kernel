/*
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
 * Copyright (C) 2003 Motorola Inc Ltd
 *
 */


#ifndef _DBMX_MW8731_H
#define _DBMX_MW8731_H


#define KTrue	(1 == 1)
#define KFalse	(1 == 0)


#define KBit0		(1 << 0)
#define KBit1		(1 << 1)
#define KBit2		(1 << 2)
#define KBit3		(1 << 3)
#define KBit4		(1 << 4)
#define KBit5		(1 << 5)
#define KBit6		(1 << 6)
#define KBit7		(1 << 7)
#define KBit8		(1 << 8)


// CODEC Registers
#define KR0		0x0
#define KR1		0x1
#define KR2		0x2
#define KR3		0x3
#define KR4		0x4
#define KR5		0x5
#define KR6		0x6
#define KR7		0x7 
#define KR8		0x8
#define KR9		0x9
#define KR15	0xF

#define KAddrShift	9
#define KLLIN		((KR0<<KAddrShift) | 0x97)
#define KRLIN		((KR1<<KAddrShift) | 0x97)
#define KLHPOUT		((KR2<<KAddrShift) | 0x79)
#define KRHPOUT		((KR3<<KAddrShift) | 0x79)
#define KAAPC		((KR4<<KAddrShift) | 0x0A)
#define KDAPC		((KR5<<KAddrShift) | 0x08)
#define KPDC		((KR6<<KAddrShift) | 0x9F)
#define KDAIF		((KR7<<KAddrShift) | 0x0A)
#define KSC			((KR8<<KAddrShift) | 0x00)
#define KAC			((KR9<<KAddrShift) | 0x00)
#define KRESET		((KR15<<KAddrShift) | 0x01)

// KR0 -- Left Line In
#define KLRINBOTH	KBit8
#define KLINMUTE	KBit7
#define KLINVOL		(KBit4|KBit3|KBit2|KBit1|KBit0)

// KR1 -- Right Line In
#define KRLINBOTH	KBit8
#define KRINMUTE	KBit7
#define KRINVOL		(KBit4|KBit3|KBit2|KBit1|KBit0)

#define KMaxLineInVolume	0x1F
#define KMinLineInVolume	0x00

// KR2 -- Left Headphone Out
#define KLRHPBOTH	KBit8
#define KLZCEN		KBit7
#define KLHPVOL		(KBit6|KBit5|KBit4|KBit3|KBit2|KBit1|KBit0)

// KR3 -- Right Headphone Out
#define KRLHPBOTH	KBit8
#define KRZCEN		KBit7
#define KRHPVOL		(KBit6|KBit5|KBit4|KBit3|KBit2|KBit1|KBit0)

#define KMaxOutVolume		0x7F
#define KMinOutVolume		0x30

// KR4 -- Analog Audio Path Control
#define KSIDEATT	(KBit7|KBit6)	// 11 = -15dB; 10 = -12dB; 01 = -9dB; 00 = -6dB
#define KSIDETONE	KBit5			// 1 = enable; 0 = disable
#define KDACSEL		KBit4			// 1 = select DAC; don't select DAC
#define KBYPASS		KBit3			// 1 = enable; 0 = disable
#define KINSEL		KBit2			// 1 = MIC; 0 = Line In
#define KMUTEMIC	KBit1			// 1 = enable; 0 = disable
#define KMICBOOST	KBit0			// 1 = enable; 0 = disable

// KR5 -- Digital Audio Path Control
#define KHPOR		KBit4
#define KDACMU		KBit3
#define	KDEEMP		(KBit2|KBit1)
#define KADCHPD		KBit0

// KR6 -- Power Down Control
#define KPOWEROFF		KBit7
#define KCLKOUTPD		KBit6
#define KOSCPD			KBit5
#define KOUTPD			KBit4
#define KDACPD			KBit3
#define KADCPD			KBit2
#define KMICPD			KBit1
#define KLINEINPD		KBit0
#define KAllPowerDown	0xFF

// KR7 -- Digital Audio Interface Format
#define KBCLKINV	KBit7
#define KMS			KBit6
#define KLRSWAP		KBit5
#define KLRP		KBit4
#define KIWL		(KBit3|KBit2)
#define KFORMAT		(KBit1|KBit0)

// KR8 -- Sampling Control
#define KCLKODIV2	KBit7
#define KCLKIDIV2	KBit6
#define KSR			(KBit5|KBit4|KBit3}KBit2)
#define KBOSR		KBit1
#define KMODE		KBit0

// KR9 -- Active Control
#define KACTIVE		KBit0


typedef enum
{
	KLeft,
	KRight,
	KBoth,
} leftRightDirection_t;

struct ssi_aud{
	int	 			open_mode;
	int  			ssi_port;
	int  			aud_port;
	int				dma_channel_r;	
	int				dma_channel_w;	
};

#endif
