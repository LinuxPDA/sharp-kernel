/*
 * linux/drivers/usbd/usbd-arch.h 
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


/*
 * Lineo specific 
 */

#define VENDOR_SPECIFIC_CLASS           0xff
#define VENDOR_SPECIFIC_SUBCLASS        0xff
#define VENDOR_SPECIFIC_PROTOCOL        0xff

#define LINEO_CLASS            		0xff

#define LINEO_SUBCLASS_SAFENET          0x01
#define LINEO_SUBCLASS_SAFESERIAL       0x02

#define	LINEO_SAFENET_CRC 		0x01
#define	LINEO_SAFENET_CRC_PADDED	0x02

#define	LINEO_SAFESERIAL_CRC 		0x01
#define	LINEO_SAFESERIAL_CRC_PADDED	0x02


/*
 * Architecture specific endpoint configurations
 */

#ifdef CONFIG_ARCH_SA1100
#warning
#warning SETTING DEFAULTS FOR SA1110
/*
 * The StrongArm SA-1110 has fixed endpoints and the bulk endpoints
 * are limited to 64 byte packets and does not support ZLP.
 */

        #define ABS_OUT_ADDR                            1
        #define ABS_IN_ADDR                             2
        #define ABS_INT_ADDR                            0

        #define MAX_OUT_PKTSIZE                         64
        #define MAX_IN_PKTSIZE                          64
        #undef MAX_INT_PKTSIZE

        #undef CONFIG_USBD_SERIAL_OUT_ENDPOINT
        #undef CONFIG_USBD_SERIAL_IN_ENDPOINT
        #undef CONFIG_USBD_SERIAL_INT_ENDPOINT

        #define CONFIG_USBD_NO_ZLP_SUPPORT

#endif

#ifdef CONFIG_ARCH_L7200
#warning
#warning SETTING DEFAULTS FOR L7200
/*
 * The Linkup L7205/L7210 has fixed endpoints and the bulk endpoints
 * are limited to 32 byte packets and does not support ZLP.
 */
        #define ABS_OUT_ADDR                            1
        #define ABS_IN_ADDR                             2
        #define ABS_INT_ADDR                            3

        #define MAX_OUT_PKTSIZE                         32
        #define MAX_IN_PKTSIZE                          32
        #define MAX_INT_PKTSIZE                         16

        #undef CONFIG_USBD_SERIAL_OUT_ENDPOINT
        #undef CONFIG_USBD_SERIAL_IN_ENDPOINT
        #undef CONFIG_USBD_SERIAL_INT_ENDPOINT

        #define CONFIG_USBD_NO_ZLP_SUPPORT

#endif

#ifdef CONFIG_USBD_SL11_BUS
#warning
#warning SETTING DEFAULTS FOR SL11
/*
 * The SL11 endpoints can be a mix of 1, 2, or 3. The SL11 endpoints have
 * fixed addresses but each can be used for Bulk IN, Bulk OUT and Interrupt.
 *
 * The default addresses can be overridden by using the kernel configuration
 * and setting the net-fd IN, OUT and INT addresses. See Config.in
 */

        #define DFL_OUT_ADDR                            1
        #define DFL_IN_ADDR                             2
        #define DFL_INT_ADDR                            3

        #define MAX_OUT_ADDR                            3
        #define MAX_IN_ADDR                             3
        #define MAX_INT_ADDR                            3

        #define MAX_OUT_PKTSIZE                         64
        #define MAX_IN_PKTSIZE                          64
        #define MAX_INT_PKTSIZE                         16

#endif


/*
 * known vendor and product ids
 */

/*
 * For itsy we will use Compaq iPaq vendor ID and 0xffff for product id
 * Serial and network vendor ids can be overidden with kernel config.
 */
#ifdef CONFIG_SA1100_ITSY
#warning -
#warning USING ITSY VENDOR ID AND PRODUCT ID
#undef CONFIG_USBD_VENDORID
#undef CONFIG_USBD_PRODUCTID
#define CONFIG_USBD_VENDORID		0x049f
#define CONFIG_USBD_PRODUCTID		0xffff
#endif

/*
 * Assigned vendor and product ids for HP Calypso
 * Serial and network vendor ids can be overidden with kernel config.
 */
#if defined(CONFIG_SA1100_CALYPSO) || defined(CONFIG_SA1110_CALYPSO)
#warning -
#warning USING CALYPSO VENDOR ID AND PRODUCT ID
#undef CONFIG_USBD_VENDORID
#undef CONFIG_USBD_PRODUCTID
#define CONFIG_USBD_VENDORID		0x03f0
#define CONFIG_USBD_PRODUCTID		0x2101
#endif

/*
 * Assigned vendor and serial/network product ids for Sharp Iris
 * Serial and network vendor ids can be overidden with kernel config.
 */
#ifdef CONFIG_SA1100_IRIS
#warning -
#warning USING IRIS VENDOR ID
#undef CONFIG_USBD_VENDORID
#define CONFIG_USBD_VENDORID		0x0fdd

#if !defined(CONFIG_USBD_SERIAL_PRODUCTID) && (CONFIG_USBD_SERIAL_PRODUCTID == 0)
#warning -
#warning USING IRIS SERIAL PRODUCT ID
#undef CONFIG_USBD_SERIAL_PRODUCTID
#define CONFIG_USBD_SERIAL_PRODUCTID	0x8001
#endif
#if !defined(CONFIG_USBD_NET_PRODUCTID) && (CONFIG_USBD_NET_PRODUCTID == 0)
#warning -
#warning USING IRIS NET PRODUCT ID
#undef CONFIG_USBD_NET_PRODUCTID
#define CONFIG_USBD_NET_PRODUCTID	0x8003
#endif
#endif


/*
 * Assigned vendor and serial/network product ids for Sharp Collie
 * Serial and network vendor ids can be overidden with kernel config.
 *
 */
#ifdef CONFIG_SA1100_COLLIE
#warning -
#warning USING COLLIE VENDOR ID
#undef CONFIG_USBD_VENDORID
#define CONFIG_USBD_VENDORID		0x04dd

#if !defined(CONFIG_USBD_SERIAL_PRODUCTID) && (CONFIG_USBD_SERIAL_PRODUCTID == 0)
#warning -
#warning USING COLLIE SERIAL PRODUCT ID
#undef CONFIG_USBD_SERIAL_PRODUCT ID
#define CONFIG_USBD_SERIAL_PRODUCT ID	0x8002
#endif
#if !defined(CONFIG_USBD_NET_PRODUCTID) || (CONFIG_USBD_NET_PRODUCTID == 0)
#warning -
#warning USING COLLIE NET PRODUCT ID
#undef CONFIG_USBD_NET_PRODUCTID
#define CONFIG_USBD_NET_PRODUCTID	0x8004
#endif
#endif

