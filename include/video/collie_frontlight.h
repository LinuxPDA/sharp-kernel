/*
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on include/video/sa1100_frontlight.h
 * 
 */

#ifndef __COLLIE_FRONTLIGHT_H
#define __COLLIE_FRONTLIGHT_H

#define COLLIE_FL_IOCTL_ON		1
#define COLLIE_FL_IOCTL_OFF		2
#define COLLIE_FL_IOCTL_STEP_CONTRAST	  100
#define COLLIE_FL_IOCTL_GET_STEP_CONTRAST 101
#define COLLIE_FL_IOCTL_GET_STEP          102


#define FL_MAJOR  254
#define FL_NAME   "collie-fl"

#endif /*  __COLLIE_FRONTLIGHT_H  */
