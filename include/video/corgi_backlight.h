/*
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

#ifndef __CORGI_FRONTLIGHT_H
#define __CORGI_FRONTLIGHT_H

#define CORGI_BL_IOCTL_ON		1
#define CORGI_BL_IOCTL_OFF		2
#define CORGI_BL_IOCTL_STEP_CONTRAST	  100
#define CORGI_BL_IOCTL_GET_STEP_CONTRAST 101
#define CORGI_BL_IOCTL_GET_STEP          102

#define CORGI_BL_RESET_CONTRAST		(-1)
#define CORGI_BL_CAUTION_CONTRAST	(3)

#define BL_MAJOR  254
#define BL_NAME   "corgi-bl"

void corgibl_blank(int blank);
int corgibl_pm_callback(struct pm_dev* pm_dev, pm_request_t req, void *data);
void corgibl_temporary_contrast_set(void);
void corgibl_temporary_contrast_reset(void);
void corgibl_set_limit_contrast(int val);

#endif /*  __CORGI_FRONTLIGHT_H  */
