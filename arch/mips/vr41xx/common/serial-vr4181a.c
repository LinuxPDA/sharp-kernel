/*
 * FILE NAME
 *	arch/mips/vr41xx/common/serial.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Serial Interface Unit routines for NEC VR4100 series.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
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
 *  - Added support for NEC VR4111 and VR4121.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC VR4122 and VR4131 are supported.
 */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/serial.h>

#include <asm/addrspace.h>
#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/vr41xx/vr41xx.h>
#include <asm/vr41xx/toadkk-tcs8000.h>

#define SIU_BASE_BAUD		1152000

static u8 *vr4181a_siu_port_base[] = { (u8 *)VR4181A_SIU0_BASE,
			       (u8 *)VR4181A_SIU1_BASE,
			       (u8 *)VR4181A_SIU2_BASE, };
static int vr4181a_siu_irq[] = { VR4181A_SIU0_INT,
				 VR4181A_SIU1_INT,
				 VR4181A_SIU2_INT, };

static inline void vr4181a_clock_supply ( int interface )
{
	u16 d = readw ( VR4181A_CMUCLKMSK0 );
	
	switch ( interface ) {
	case 0:
		d |= 1<<8;
		break;
	case 1:
		d |= 1<<9;
		break;
	case 2:
		d |= 1<<10;
		break;
	default:
		break;
	}
	writew ( d, VR4181A_CMUCLKMSK0 );
}

int vr41xx_serial_ports = 0;


void __init vr41xx_siu_init(int interface, int module )
{
	struct serial_struct s;

	if ( interface < 0 || interface > 2 )
		return;

	memset(&s, 0, sizeof(s));

	//s.line = vr41xx_serial_ports;
	s.line = interface;
	s.baud_base = SIU_BASE_BAUD;
	s.irq = vr4181a_siu_irq[interface];
	s.flags = ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST;
	s.iomem_base = (unsigned char *)vr4181a_siu_port_base[interface];
	s.iomem_reg_shift = 0;
	s.io_type = SERIAL_IO_MEM;
	if (early_serial_setup(&s) != 0)
		printk(KERN_ERR "SIU setup failed!\n");

	vr4181a_clock_supply(interface);

	vr41xx_serial_ports++;
}
