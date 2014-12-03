/*
 *
 * Alchemy Semi Au1000 pcmcia driver include file
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
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
 * 
 */
#ifndef __ASM_VR4181A_PCMCIA_H
#define __ASM_VR4181A_PCMCIA_H


#define VR4181A_PCMCIA_POLL_PERIOD    (2*HZ)
#define VR4181A_PCMCIA_IO_SPEED       (255)
#define VR4181A_PCMCIA_MEM_SPEED      (300)


struct pcmcia_state {
  unsigned detect: 1,
            ready: 1,
           wrprot: 1,
	     bvd1: 1,
	     bvd2: 1,
            vs_3v: 1,
            vs_Xv: 1;
};


struct pcmcia_configure {
  unsigned sock: 8,
            vcc: 8,
            vpp: 8,
         output: 1,
        speaker: 1,
          reset: 1;
};

struct pcmcia_irq_info {
	unsigned int sock;
	unsigned int irq;
};

typedef unsigned long memaddr_t;

struct vr4181a_pcmcia_socket {
	socket_state_t        cs_state;
	struct pcmcia_state   k_state;
	unsigned int          irq;
	void                  (*handler)(void *, unsigned int);
	void                  *handler_info;
	pccard_io_map         io_map[MAX_IO_WIN];
	pccard_mem_map        mem_map[MAX_WIN];
	u32                   virt_io;
	memaddr_t             phys_attr, phys_mem;
	unsigned short        speed_io, speed_attr, speed_mem;
};

struct pcmcia_init {
	void (*handler)(int irq, void *dev, struct pt_regs *regs);
};

struct pcmcia_low_level {
	int (*init)(struct pcmcia_init *);
	int (*shutdown)(void);
	int (*socket_state)(unsigned sock, struct pcmcia_state *);
	int (*get_irq_info)(struct pcmcia_irq_info *);
	int (*configure_socket)(const struct pcmcia_configure *);
};

#ifdef CONFIG_TOADKK_TCS8000
extern struct pcmcia_low_level tcs8000_pcmcia_ops;
#endif

#endif /* __ASM_VR4181A_PCMCIA_H */
