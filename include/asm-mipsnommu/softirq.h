/* $Id: softirq.h,v 1.4 2001/08/15 20:48:22 mleslie Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997, 1998, 1999, 2000 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_SOFTIRQ_H
#define _ASM_SOFTIRQ_H

#include <asm/atomic.h>
#include <asm/hardirq.h>

#define cpu_bh_disable(cpu)	do { local_bh_count(cpu)++; barrier(); } while (0)
#define __cpu_bh_enable(cpu)	do { barrier(); local_bh_count(cpu)--; } while (0)

#define cpu_bh_enable __cpu_bh_enable

#define local_bh_disable()	cpu_bh_disable(smp_processor_id())
#define __local_bh_enable()	__cpu_bh_enable(smp_processor_id())

#define local_bh_enable __local_bh_enable

#define __cpu_raise_softirq(cpu,nr) set_bit((nr), &softirq_pending(cpu))

#define in_softirq() (local_bh_count(smp_processor_id()) != 0)

#endif /* _ASM_SOFTIRQ_H */
