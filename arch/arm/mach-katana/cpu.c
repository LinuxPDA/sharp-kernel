/*
 *  linux/arch/arm/mach-katana/cpu.c
 *
 *  Copyright (C) 2003 by MediaQ, Incorporated.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  CPU support functions
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>

#if 0	//HSU Don't do anything on set clock yet!!!
#define CM_ID  	(IO_ADDRESS(INTEGRATOR_HDR_BASE)+INTEGRATOR_HDR_ID_OFFSET)
#define CM_OSC	(IO_ADDRESS(INTEGRATOR_HDR_BASE)+INTEGRATOR_HDR_OSC_OFFSET)
#define CM_STAT (IO_ADDRESS(INTEGRATOR_HDR_BASE)+INTEGRATOR_HDR_STAT_OFFSET)
#define CM_LOCK (IO_ADDRESS(INTEGRATOR_HDR_BASE)+INTEGRATOR_HDR_LOCK_OFFSET)

struct vco {
	unsigned char vdw;
	unsigned char od;
};

/*
 * Divisors for each OD setting.
 */
static unsigned char cc_divisor[8] = { 10, 2, 8, 4, 5, 7, 9, 6 };

static unsigned int vco_to_freq(struct vco vco, int factor)
{
	return 2000 * (vco.vdw + 8) / cc_divisor[vco.od] / factor;
}

#ifdef CONFIG_CPU_FREQ
/*
 * Divisor indexes for in ascending divisor order
 */
static unsigned char s2od[] = { 1, 3, 4, 7, 5, 2, 6, 0 };

static struct vco freq_to_vco(unsigned int freq_khz, int factor)
{
	struct vco vco = {0, 0};
	unsigned int i, f;

	freq_khz *= factor;

	for (i = 0; i < 8; i++) {
		f = freq_khz * cc_divisor[s2od[i]];
		/* f must be between 10MHz and 320MHz */
		if (f > 10000 && f <= 320000)
			break;
	}

	vco.od  = s2od[i];
	vco.vdw = f / 2000 - 8;

	return vco;
}

#endif	//HSU

/*
 * Validate the speed in khz.  If it is outside our
 * range, then return the lowest.
 */
unsigned int katana_validatespeed(unsigned int freq_khz)
{
#if 1
	return 130000;	//fixed at 130MHz for now
#else
	struct vco vco;

	if (freq_khz < 12000)
		freq_khz = 12000;
	if (freq_khz > 160000)
		freq_khz = 160000;

	vco = freq_to_vco(freq_khz, 1);

	if (vco.vdw < 4 || vco.vdw > 152)
		return -EINVAL;

	return vco_to_freq(vco, 1);
#endif
}

void katana_setspeed(unsigned int freq_khz)
{
#if 0
	struct vco vco = freq_to_vco(freq_khz, 1);
	u_int cm_osc;

	cm_osc = __raw_readl(CM_OSC);
	cm_osc &= 0xfffff800;
	cm_osc |= vco.vdw | vco.od << 8;

	__raw_writel(0xa05f, CM_LOCK);
	__raw_writel(cm_osc, CM_OSC);
	__raw_writel(0, CM_LOCK);
#endif
}
#endif

static int __init cpu_init(void)
{
#if 1	//HSU Don't do anything for now
	printk("Calling cpu_init: Do nothing...\n");
#else
	u_int cm_osc, cm_stat, cpu_freq_khz, mem_freq_khz;
	struct vco vco;

	cm_osc = __raw_readl(CM_OSC);

	vco.od  = (cm_osc >> 20) & 7;
	vco.vdw = (cm_osc >> 12) & 255;
	mem_freq_khz = vco_to_freq(vco, 2);

	printk(KERN_INFO "Memory clock = %d.%03d MHz\n",
		mem_freq_khz / 1000, mem_freq_khz % 1000);

	vco.od = (cm_osc >> 8) & 7;
	vco.vdw = cm_osc & 255;
	cpu_freq_khz = vco_to_freq(vco, 1);

#ifdef CONFIG_CPU_FREQ
	cpufreq_init(cpu_freq_khz, 1000, 0);
	cpufreq_setfunctions(integrator_validatespeed, integrator_setspeed);
#endif

	cm_stat = __raw_readl(CM_STAT);
	printk("Module id: %d\n", cm_stat & 255);
#endif //

	return 0;
}

__initcall(cpu_init);
