/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * Reset the BRECIS ICs.
 *
 */
#include <linux/config.h>

#include <asm/reboot.h>
#include <asm/brecis/BrecisSysRegs.h>
// #include <asm/brecis/generic.h>  // ...MaTed
#if defined(CONFIG_BRECIS)
#include <asm/brecis/brecis.h>
#endif

static void mips_machine_restart(char *command);
static void mips_machine_halt(void);

static void mips_machine_restart(char *command)
{
//        volatile unsigned int *softres_reg = (void *)RST_SET_REG;

	*RST_SET_REG = 2;	// just reset MIPS I know Bad Magic #
}

static void mips_machine_halt(void)
{
//        volatile unsigned int *softres_reg = (void *)SOFTRES_REG;

	*RST_SET_REG = 1;	// reset everything
}

void mips_reboot_setup(void)
{
	_machine_restart = mips_machine_restart;
	_machine_halt = mips_machine_halt;
	_machine_power_off = mips_machine_halt;
}
