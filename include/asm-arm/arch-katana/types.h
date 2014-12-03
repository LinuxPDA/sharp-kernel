/*
 *  Type defintions
 *
 *  Copyright (c) MediaQ Incorporated.  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __ARMTypes_h
#define __ARMTypes_h                       1

/* handy types */
#ifndef __ASSEMBLY__	//linux gcc does not like typedef

#ifndef UInt32
typedef unsigned long UInt32;
#endif

#ifndef UInt16
typedef unsigned short UInt16;
#endif

#ifndef UInt8
typedef unsigned char UInt8;
#endif

#ifndef DWORD
typedef unsigned long DWORD;
#endif

#ifndef WORD
typedef unsigned short WORD;
#endif

#ifndef ULONG
typedef unsigned long ULONG;
#endif

#ifndef USHORT
typedef unsigned short USHORT;
#endif

#ifndef UCHAR
typedef unsigned char UCHAR;
#endif

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef PVOID
#define PVOID void*
#endif

#ifndef PULONG
#define PULONG ULONG*
#endif

#ifndef PUSHORT
#define PUSHORT USHORT*
#endif

#ifndef UINT
typedef unsigned int  UINT;
#endif

#endif	//__ASSEMBLY__

#endif
