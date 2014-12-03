/*
 * linux/drivers/usbd/net_fd/crc8.h
 *
 * Copyright (c) 2000, 2001 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *      Bruce Balden <balden@lineo.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

extern __u8 crc8_table[256];

#define CRC8_INITFCS     0xff  // Initial FCS value 
#define CRC8_GOODFCS     0x35  // Good final FCS value 

//#define CRC8_FCS(fcs, c) (((fcs) >> 8) ^ crc8_table[((fcs) ^ (c)) & 0xff])

//#define CRC8_FCS(fcs, c) ((fcs) ^ crc8_table[((fcs) ^ (c))])

#define CRC8_FCS(fcs, c) (crc8_table[((fcs) ^ (c))])

/**
 * fcs_memcpy8 - memcpy and calculate fcs
 * @dp:
 * @sp:
 * @len:
 * @fcs
 *
 * Perform a memcpy and calculate fcs using ppp 8bit CRC algorithm.
 */
static __u8 __inline__ fcs_memcpy8(unsigned char *dp, unsigned char *sp, int len, __u8 fcs)
{
    for (;len-->0; fcs = CRC8_FCS(fcs, *dp++ = *sp++));
    return fcs;
}

/**
 * fcs_pad8 - pad and calculate fcs
 * @dp:
 * @sp:
 * @len:
 * @fcs
 *
 * Pad and calculate fcs using ppp 8bit CRC algorithm.
 */
static __u8 __inline__ fcs_pad8(unsigned char *dp, int len, __u8 fcs)
{
    for (;len-->0; fcs = CRC8_FCS(fcs, *dp++ = '\0'));
    return fcs;
}

/**     
 * fcs_compute8 - memcpy and calculate fcs
 * @dp:
 * @sp:
 * @len:
 * @fcs
 *
 * Perform a memcpy and calculate fcs using ppp 8bit CRC algorithm.
 */
static __u8 __inline__ fcs_compute8(unsigned char *sp, int len, __u8 fcs)
{
    for (;len-->0; fcs = CRC8_FCS(fcs, *sp++));
    return fcs;
}

