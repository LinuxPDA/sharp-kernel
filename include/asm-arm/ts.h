/*
 * Touchscreen Driver Header File
 * Copyright (c) 2001 Lineo Inc.
 *
 * Change Log
 *	12-Nov-2001 Lineo Japan, Inc.
 *
 */

#ifndef _ARM_TS_H
#define _ARM_TS_H

#define TS_IOC_MAGIC 'p'
#define TS_IOCGRESX _IOR(TS_IOC_MAGIC, 0xa0, int)
#define TS_IOCGRESY _IOR(TS_IOC_MAGIC, 0xa1, int)
#define TS_IOCSSMOOTH _IOW(TS_IOC_MAGIC, 0xa2, int)

#endif /* _ARM_TS_H */

