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
 *    27-Nov-2002 SHARP for SL-B500/5600
 */

#ifndef __POODLE_FRONTLIGHT_H
#define __POODLE_FRONTLIGHT_H

#define POODLE_FL_IOCTL_ON		1
#define POODLE_FL_IOCTL_OFF		2
#define POODLE_FL_IOCTL_STEP_CONTRAST	  100
#define POODLE_FL_IOCTL_GET_STEP_CONTRAST 101
#define POODLE_FL_IOCTL_GET_STEP          102

#define POODLE_FL_RESET_CONTRAST	(-1)
#define POODLE_FL_CAUTION_CONTRAST	(1)


#define FL_MAJOR  254
#define FL_NAME   "poodle-fl"

void poodlefl_set_limit_contrast(int val);

#endif /*  __POODLE_FRONTLIGHT_H  */
