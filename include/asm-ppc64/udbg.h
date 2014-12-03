#ifndef __UDBG_HDR
#define __UDBG_HDR

/*
 * c 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

void udbg_init(void);
void udbg_putc(unsigned char c);
unsigned char udbg_getc(void);
void udbg_puts(const char *s);
void udbg_puthex(unsigned long val);
void udbg_printSP(const char *s);
void udbg_printf(const char *fmt, ...);
void udbg_ppcdbg(unsigned long flags, const char *fmt, ...);
unsigned long udbg_ifdebug(unsigned long flags);
#endif
