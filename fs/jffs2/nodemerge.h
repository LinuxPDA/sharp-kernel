/*
 * fs/jffs2/nodemerge.h
 *
 * Copyright (C) 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * $Id: nodemerge.h,v 1.3 2002/12/19 02:42:45 yamade Exp $
 *
 * ChangLog:
 *     05-Dec-2002 SHARP  nodemerge-thershold is changable
 */

#ifndef __JFFS2_NODEMERGE_H__
#define __JFFS2_NODEMERGE_H__

#ifdef CONFIG_JFFS2_NODEMERGE
void jffs2_merge_nodes(struct jffs2_sb_info*, struct jffs2_inode_info*, uint32_t, int);
#else
static inline void jffs2_merge_nodes(struct jffs2_sb_info* c, struct jffs2_inode_info* f, uint32_t size, int frags_threshold) { }
#endif

#endif
