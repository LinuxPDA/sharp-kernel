/*
 * Alchemy Semi PB1000 board specific pcmcia routines.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
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
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/tqueue.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/bus_ops.h>
#include "cs_internal.h"

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <asm/vr41xx/vr4181a_pcmcia.h>
#include <asm/vr41xx/toadkk-tcs8000.h>

#include "i82365.h"

#define PCMCIA_MAX_SOCK 1

extern struct pcmcia_x_table x_table;

#if 0 /*@@@@@*/
#define TRACE printk(KERN_ERR __FUNCTION__ ": %d\n", __LINE__ )
#define DEBUG(args...) printk(KERN_ERR __FUNCTION__ args )
#else
#define TRACE /**/
#define DEBUG(args...) /**/
#endif

/*
 * ECU operations
 */
static spinlock_t bus_lock = SPIN_LOCK_UNLOCKED;

static u_char i365_get(u_short sock, u_short reg)
{
	unsigned long flags;
	spin_lock_irqsave(&bus_lock,flags);
	{
		ioaddr_t port =
			( sock == 0 ) ? VR4181A_ECUINDX0 : VR4181A_ECUINDX1;
		u_char val;

		writeb(reg, port); val = readb(port+4);
		spin_unlock_irqrestore(&bus_lock,flags);
		return val;
	}
}

static void i365_set(u_short sock, u_short reg, u_char data)
{
	unsigned long flags;
	spin_lock_irqsave(&bus_lock,flags);
	{
		ioaddr_t port =
			( sock == 0 ) ? VR4181A_ECUINDX0 : VR4181A_ECUINDX1;
		writeb(reg, port); writeb(data, port+4);
		spin_unlock_irqrestore(&bus_lock,flags);
	}
}


static inline void tcs8000_map_init ( u_short sock )
{
	u_short start_io = 0x0000,
		stop_io = (u_short)(VR4181A_PCMCIA_WIN_SIZE_IO - 1);

	u_long start_attr = 0x00000000,
		stop_attr = VR4181A_PCMCIA_WIN_SIZE_ATTR - 1;
	
	u_long start_mem = 0x00000000,
		stop_mem = VR4181A_PCMCIA_WIN_SIZE_MEM - 1;

	TRACE;
	/* set I/O window */
	i365_set ( sock, I365_IO(0)+0, start_io & 0x00ff );
	i365_set ( sock, I365_IO(0)+1, start_io >> 8 );
	i365_set ( sock, I365_IO(0)+2, stop_io & 0x00ff );
	i365_set ( sock, I365_IO(0)+3, stop_io >> 8 );
	i365_set ( sock, I365_IOCTL, 0x0e );
	
	/* set attr window */
	i365_set ( sock, I365_MEM(0)+0, start_attr >> 12 );
	i365_set ( sock, I365_MEM(0)+1, 0x80|((start_attr>>20) & 0x3f) );
	i365_set ( sock, I365_MEM(0)+2, stop_attr >> 12 );
	i365_set ( sock, I365_MEM(0)+3, 0xf0|((stop_attr>>20) & 0x3f) );
	i365_set ( sock, I365_MEM(0)+5, 0x40 );

#if 0
	/* set mem window */
	i365_set ( sock, I365_MEM(1)+0, start_mem >> 12 );
	i365_set ( sock, I365_MEM(1)+1, 0x00|((start_mem>>20) & 0x3f) );
	i365_set ( sock, I365_MEM(1)+2, stop_attr >> 12 );
	i365_set ( sock, I365_MEM(1)+3, 0x00|((stop_mem>>20) & 0x3f) );
	i365_set ( sock, I365_MEM(1)+5, 0x00 );
#endif

	/* enable these windows */
	i365_set ( sock, I365_ADDRWIN, (1<<6)|(1<<0) );
}	

static inline void tcs8000_map_release ( u_short sock )
{
	/* disable all windows */
	i365_set ( sock, I365_ADDRWIN, 0 );
}




static int tcs8000_pcmcia_init(struct pcmcia_init *init)
{
	/* set pin function */
	writew ( 0x0000, INTCS(0x00b084) );
	writew ( 0x0000, VR4181A_GPMODE3 );
	writew ( 0x5400, VR4181A_GPMODE4 );
	writew ( 0x0000, VR4181A_ECUIDE0 );

	udelay ( 100 );

	i365_set ( 0, I365_CSCINT, 0 );
	i365_set ( 0, I365_GENCTL, 1<<1 );

	return 1;
}

static int tcs8000_pcmcia_shutdown(void)
{
	TRACE;
	tcs8000_map_release ( 0 );

	i365_set ( 0, I365_CSCINT, 0 );
	i365_set ( 0, I365_POWER, 0 ); /* power off */
	
	return 0;
}

#if 0 /*@@@@@*/
unsigned char tcs8000_ready ( void )
{
	printk ( KERN_ERR "ECUINT0 = %d, ECUINTMSK0 = %d\n",
		 readb ( VR4181A_ECUINT0 ),
		 readb ( VR4181A_ECUINTMSK0 ));
	printk ( KERN_ERR "SYSINT1REG = %04x\n",
		 readb ( INTCS(0x00b08a)));
	return (i365_get ( 0, I365_STATUS ) & (1<<5));
}
#endif /*@@@@@*/


static int 
tcs8000_pcmcia_socket_state(unsigned sock, struct pcmcia_state *state)
{
	u8 cd;

	cd = i365_get ( sock, I365_STATUS );

	state->ready = 0;
	state->vs_Xv = 0;
	state->vs_3v = 0;
	state->detect = 0;


	if ( (cd & (3<<2)) == (3<<2) ) {
		state->detect = 1;
		state->vs_3v = 1; /* 3.3V card only */
		state->ready = 1;
	}

	state->bvd1=1;
	state->bvd2=1;
	state->wrprot=0; 

	return 1;
}


static int tcs8000_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{

	if(info->sock > PCMCIA_MAX_SOCK) return -1;

	if(info->sock == 0)
		info->irq = VR4181A_ECU0_INT;
	else 
		info->irq = VR4181A_ECU1_INT;

	return 0;
}


static int 
tcs8000_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	u16 sock = configure->sock;

	if (sock > PCMCIA_MAX_SOCK) return -1;

	i365_set ( sock, I365_POWER, (1<<4) );
	udelay ( 300 );
	i365_set ( sock, I365_POWER, (1<<4)|(1<<7) );
	udelay ( 300 );
	i365_set ( sock, I365_INTCTL, (1<<5)|(1<<6) );
	i365_set ( sock, I365_GENCTL, 1<<1 );
	i365_set ( sock, I365_CSCINT, 1<<2 );

	tcs8000_map_init ( sock );

	writeb ( 0x0001, ( sock == 0 ) ? VR4181A_ECUINTMSK0 :
		 VR4181A_ECUINTMSK1 );

	return 0;
}

struct pcmcia_low_level tcs8000_pcmcia_ops = { 
	tcs8000_pcmcia_init,
	tcs8000_pcmcia_shutdown,
	tcs8000_pcmcia_socket_state,
	tcs8000_pcmcia_get_irq_info,
	tcs8000_pcmcia_configure_socket
};
