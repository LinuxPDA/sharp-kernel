/*
 * FILE NAME
 *	arch/mips/vr41xx/nec-eagle/init.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Initialisation code for the NEC Eagle/Hawk board.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - Added support for NEC Hawk.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC Eagle is supported.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <asm/bootinfo.h>

char arcs_cmdline[CL_SIZE];

const char *get_system_type(void)
{
	return "TOA DKK TCS-8000";
}

void __init bus_error_init(void)
{
}

void __init prom_init(int argc, char **argv, unsigned long magic, int *prom_vec)
{
	int i;

#if 1
	/*
	 * collect args and prepare cmd_line
	 */
	for (i = 1; i < argc; i++) {
		strcat(arcs_cmdline, argv[i]);
		if (i < (argc - 1))
			strcat(arcs_cmdline, " ");
	}
	strcat ( arcs_cmdline,
		 " mem=32M"
		 " ide0=noprobe ide1=noprobe ide2=noprobe ide3=noprobe"
		 " ide4=noprobe ide5=noprobe"
		);
#endif

#if 0
	strcpy ( arcs_cmdline,
		 "mem=32M"
		 " console=ttyS0,115200"
		 " root=/dev/nfs ip=:::::: nfsroot=10.0.0.1:/home/exports2"
		 //" root=/dev/mtdblock0"
		 " ide0=noprobe ide1=noprobe ide2=noprobe ide3=noprobe"
		 " ide4=noprobe ide5=noprobe"
		);
#endif
#if 0
	strcpy ( arcs_cmdline,
		 "mem=32M"
		 " init=/bin/sh"
		 " root=/dev/ram"
		 " console=ttyS0,115200" );
#endif
	mips_machgroup = MACH_GROUP_NEC_VR41XX;
	mips_machtype = MACH_TOADKK_TCS8000;
}

void __init prom_free_prom_memory (void)
{
}
