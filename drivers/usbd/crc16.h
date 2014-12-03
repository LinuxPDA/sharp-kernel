/*
 * linux/drivers/usbd/net_fd/crc16.h
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

#ifdef CONFIG_USBD_NET_CRC16
extern __u16 crc16_table[256];

#define CRC16_INITFCS     0xffff  // Initial FCS value 
#define CRC16_GOODFCS     0xf0b8  // Good final FCS value 

#define CRC16_FCS(fcs, c) (((fcs) >> 8) ^ crc16_table[((fcs) ^ (c)) & 0xff])

/**
 * fcs_memcpy16 - memcpy and calculate fcs
 * @dp:
 * @sp:
 * @len:
 * @fcs
 *
 * Perform a memcpy and calculate fcs using ppp 16bit CRC algorithm.
 */
static __u16 __inline__ fcs_memcpy16(unsigned char *dp, unsigned char *sp, int len, __u16 fcs)
{
    for (;len-->0; fcs = CRC16_FCS(fcs, *dp++ = *sp++));
    return fcs;
}

/**
 * fcs_pad16 - pad and calculate fcs
 * @dp:
 * @sp:
 * @len:
 * @fcs
 *
 * Pad and calculate fcs using ppp 16bit CRC algorithm.
 */
static __u16 __inline__ fcs_pad16(unsigned char *dp, int len, __u16 fcs)
{
    for (;len-->0; fcs = CRC16_FCS(fcs, *dp++ = '\0'));
    return fcs;
}

/**     
 * fcs_compute16 - memcpy and calculate fcs
 * @dp:
 * @sp:
 * @len:
 * @fcs
 *
 * Perform a memcpy and calculate fcs using ppp 16bit CRC algorithm.
 */
static __u16 __inline__ fcs_compute16(unsigned char *sp, int len, __u16 fcs)
{
    for (;len-->0; fcs = CRC16_FCS(fcs, *sp++));
    return fcs;
}
#endif

