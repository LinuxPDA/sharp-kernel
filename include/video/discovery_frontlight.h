/*
 * linux/include/video/discovery_frontlight.h : Discovery frontlight driver
 *
 * Copyright (C) 2002 Richard Rau (richard.rau@msa.hinet.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *  Based on:
 *  linux/include/video/collie_frontlight.h
 *    Created by Lineo Japan, Inc.
 */

#ifndef __DISCOVERY_FRONTLIGHT_H
#define __DISCOVERY_FRONTLIGHT_H

#define DISCOVERY_FL_IOCTL_ON		1
#define DISCOVERY_FL_IOCTL_OFF		2
#define DISCOVERY_FL_IOCTL_STEP_CONTRAST	100
#define DISCOVERY_FL_IOCTL_GET_STEP_CONTRAST 	101
#define DISCOVERY_FL_IOCTL_GET_STEP          	102


#define FL_MAJOR  254
#define FL_NAME   "discovery-fl"

#endif /*  __DISCOVERY_FRONTLIGHT_H  */
