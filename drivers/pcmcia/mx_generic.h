/*
 * linux/include/asm/arch/pcmcia.h
 *
 * Copyright (C) 2000 John G Dorsey <john+@cs.cmu.edu>
 *
 */
//#ifndef _ASM_ARCH_PCMCIA
//#define _ASM_ARCH_PCMCIA
#ifndef __MX_GENERIC_H_
#define __MX_GENERIC_H_

#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include "cs_internal.h"


/* Ideally, we'd support up to MAX_SOCK sockets, but the SA-1100 only
 * has support for two. This shows up in lots of hardwired ways, such
 * as the fact that MECR only has enough bits to configure two sockets.
 * Since it's so entrenched in the hardware, limiting the software
 * in this way doesn't seem too terrible.
 */
//#define MAX_IO_WIN 2
//#define MAX_WIN 4

#define DBMX_PCMCIA_MAX_SOCK   1

struct pcmcia_init {
  void (*handler)(int irq, void *dev, struct pt_regs *regs);
};

#if 0
struct pcmcia_state {
  unsigned detect: 1,
            ready: 1,
             bvd1: 1,
             bvd2: 1,
           wrprot: 1,
            vs_3v: 1,
            vs_Xv: 1;
};
#else
struct pcmcia_state {
  unsigned poweron: 1,
             ready: 1,
              bvd2: 1,
              bvd1: 1,
            detect: 1,
            wrprot: 1,
             vs_3v: 1,
             vs_Xv: 1;
};
#endif
struct pcmcia_state_array {
  unsigned int size;
  struct pcmcia_state *state;
};

struct pcmcia_configure {
  unsigned sock: 8,
            vcc: 8,
            vpp: 8,
         output: 1,
        speaker: 1,
          reset: 1,
            irq: 1;
};

struct pcmcia_irq_info {
  unsigned int sock;
  unsigned int irq;
};

struct pcmcia_low_level {
  int (*init)(struct pcmcia_init *);
  int (*shutdown)(void);
  int (*socket_state)(struct pcmcia_state_array *);
  int (*get_irq_info)(struct pcmcia_irq_info *);
  int (*configure_socket)(const struct pcmcia_configure *);

  /*
   * Enable card status IRQs on (re-)initialisation.  This can
   * be called at initialisation, power management event, or
   * pcmcia event.
   */
  int (*socket_init)(int sock);

  /*
   * Disable card status IRQs and PCMCIA bus on suspend.
   */
  int (*socket_suspend)(int sock);

/* The two functions below are newly added. */
//  int (*set_speed)(int voltage);
//  int (*set_mem)(addr_t base, addr_t offset);  
};

extern struct pcmcia_low_level *pcmcia_low_level;

/* Access time definition */
#define DBMX_PCMCIA_IO_ACCESS      (165)
#define DBMX_PCMCIA_5V_MEM_ACCESS  (150)
#define DBMX_PCMCIA_3V_MEM_ACCESS  (300)


/* The socket driver actually works nicely in interrupt-driven form,
 * so the (relatively infrequent) polling is "just to be sure."
 */
#define DBMX_PCMCIA_POLL_PERIOD    (2*HZ)

struct dbmx_pcmcia_socket {
  /*
   * Core PCMCIA state
   */
  socket_state_t        cs_state;
  pccard_io_map         io_map[MAX_IO_WIN];
  pccard_mem_map        mem_map[MAX_WIN];
  void                  (*handler)(void *, unsigned int);
  void                  *handler_info;

  struct pcmcia_state   k_state;
  ioaddr_t              phys_attr, phys_mem;
  void			*virt_io;
  unsigned short        speed_io, speed_attr, speed_mem;

  /*
   * Info from low level handler
   */
  unsigned int          irq;
};
#endif
