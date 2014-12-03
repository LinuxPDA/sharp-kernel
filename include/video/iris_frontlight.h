/*
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#ifndef __IRIS_FRONTLIGHT_H
#define __IRIS_FRONTLIGHT_H

#define IRIS_FL_IOCTL_ON	      1
#define IRIS_FL_IOCTL_OFF	      2
#define IRIS_FL_IOCTL_INTENSITY	      3
#define IRIS_FL_IOCTL_BACKLIGHT       4
#define IRIS_FL_IOCTL_CONTRAST	      5
#define IRIS_FL_IOCTL_GET_BACKLIGHT   6
#define IRIS_FL_IOCTL_GET_CONTRAST    7

#define IRIS_FL_IOCTL_STEP_CONTRAST	100
#define IRIS_FL_IOCTL_GET_STEP_CONTRAST 101
#define IRIS_FL_IOCTL_GET_STEP          102

#define FL_MAJOR  254
#define FL_NAME   "iris-fl"

#endif /*  __IRIS_FRONTLIGHT_H  */
