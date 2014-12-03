/*
 *  linux/include/linux/mmc/ioctl.h
 *
 *  Author:	Vladimir Shebordaev	
 *  Copyright:	MontaVista Software Inc.
 *
 *	$Id: ioctl.h,v 0.2 2002/07/11 16:28:21 ted Exp ted $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef __MMC_IOCTL_H__
#define __MMC_IOCTL_H__

#include <asm/ioctl.h>

/* IOCTL commands provided by MMC subsystem */
#define IOCMMCSTRNSMODE _IOW('I',0x0f01,int)
#define IOCMMCGTRNSMODE _IOR('I',0x0f02,int)
#define IOCMMCGCARDESC  _IOR('I',0x0f03,int) /* FIXME */
#define IOCMMCGBLKSZMAX _IOR('I',0x0f04,ssize_t)
#define IOCMMCGNOBMAX	_IOR('I',0x0f05,ssize_t)

#endif /* __MMC_IOCTL_H__ */
