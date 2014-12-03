/*
 * linux/include/asm-sh/sh7751r.h
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi SH7751r support.
 */
#ifndef __ASM_SH_SH7751R_H
#define __ASM_SH_SH7751R_H 1

#define SCSMR2			0xa4000150
#define SCSCR2			0xa4000154
#define SCBRR2			0xa4000152
#define SCFCR2			0xa400015c 

#define STBCR           0xffffff82 /* Standby Control Register 1 */
#define STBCR2          0xffffff88 /* Standby Control Register 2 */
#define STBCR3          0xa4000230 /* Standby Control Register 3 */

#define TSTR			0xfffffe92


#endif /* __ASM_SH_SH7751R_H */
