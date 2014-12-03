/* md5.h - header file for md5c.c
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD5 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD5 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.
   
   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is" without
   express or implied warranty of any kind.
   
   These notices must be retained in any copies of any part of this
   documentation and/or software.
*/


#ifndef _CRYPTO_MD5_H
#define _CRYPTO_MD5_H

/* MD5 context. */
struct MD5_CTX {
	u32 state[4];                                   /* state (ABCD) */
	u32 count[2];        /* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64];                       /* input buffer */
};

typedef struct MD5_CTX MD5_CTX;

static void MD5Init (struct MD5_CTX *);
static void MD5Update (struct MD5_CTX *, const unsigned char *, unsigned int);
static void MD5Final (unsigned char x[16], struct MD5_CTX *);

#endif /* _CRYPTO_MD5_H */













