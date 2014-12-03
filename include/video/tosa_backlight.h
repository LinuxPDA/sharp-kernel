/*
 * linux/include/video/tosa_backlight.h
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 * linux/include/video/corgi_backlight.h
 * 
 * (C) Copyright 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on include/video/sa1100_frontlight.h
 *
 * ChangeLog:
 *    06-Nov-2002 SHARP for SL-B500/5600
 */


#ifndef __TOSA_FRONTLIGHT_H
#define __TOSA_FRONTLIGHT_H

#define TOSA_BL_IOCTL_ON		1
#define TOSA_BL_IOCTL_OFF		2
#define TOSA_BL_IOCTL_STEP_CONTRAST	  100
#define TOSA_BL_IOCTL_GET_STEP_CONTRAST 101
#define TOSA_BL_IOCTL_GET_STEP          102

#define TOSA_BL_RESET_CONTRAST		(-1)
#define TOSA_BL_CAUTION_CONTRAST	(1)

#define BL_MAJOR  254
#define BL_NAME   "tosa-bl"

#ifdef CONFIG_PM
void tosa_bl_blank(int);
int tosa_bl_pm_callback(struct pm_dev*, pm_request_t, void*);
#endif
void tosa_bl_temporary_contrast_set(void);
void tosa_bl_temporary_contrast_reset(void);
void tosa_bl_set_limit_contrast(int);

#endif /*  __TOSA_FRONTLIGHT_H  */
