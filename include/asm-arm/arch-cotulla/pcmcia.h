/*
 * linux/include/asm/arch-cotulla/cotulla.h
 * 
 * Copyright (c) 2001 Lineo, Inc. 
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *
 * linux/include/asm/arch/pcmcia.h
 *
 * Copyright (C) 2000 John G Dorsey <john+@cs.cmu.edu>
 *
 * This file contains definitions for the low-level SA-1100 kernel PCMCIA
 * interface. Please see linux/Documentation/arm/SA1100/PCMCIA for details.
 * 
 *  Change Log
 */

#ifndef _ASM_ARCH_PCMCIA
#define _ASM_ARCH_PCMCIA


/* Ideally, we'd support up to MAX_SOCK sockets, but the SA-1100 only
 * has support for two. This shows up in lots of hardwired ways, such
 * as the fact that MECR only has enough bits to configure two sockets.
 * Since it's so entrenched in the hardware, limiting the software
 * in this way doesn't seem too terrible.
 */
#define CPLD_PCMCIA_MAX_SOCK	2


#ifndef __ASSEMBLY__

struct pcmcia_init {
  void (*handler)(int irq, void *dev, struct pt_regs *regs);
};

struct pcmcia_state {
  unsigned detect: 1,
            ready: 1,
             bvd1: 1,
             bvd2: 1,
           wrprot: 1,
            vs_3v: 1,
            vs_Xv: 1,
             mode: 1;
};

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
          reset: 1;
	u_int	flags;
	u_int	masks;
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
};

//extern struct pcmcia_low_level *pcmcia_low_level;

/*
 * Personal Computer Memory Card International Association (PCMCIA) sockets
 */

//#define PCMCIAPrtSp	0x04000000	/* PCMCIA Partition Space [byte]   */
//#define PCMCIASp	(4*PCMCIAPrtSp)	/* PCMCIA Space [byte]             */
//#define PCMCIAIOSp	PCMCIAPrtSp	/* PCMCIA I/O Space [byte]         */
//#define PCMCIAAttrSp	PCMCIAPrtSp	/* PCMCIA Attribute Space [byte]   */
//#define PCMCIAMemSp	PCMCIAPrtSp	/* PCMCIA Memory Space [byte]      */

//#define PCMCIA0Sp	PCMCIASp	/* PCMCIA 0 Space [byte]           */
//#define PCMCIA0IOSp	PCMCIAIOSp	/* PCMCIA 0 I/O Space [byte]       */
//#define PCMCIA0AttrSp	PCMCIAAttrSp	/* PCMCIA 0 Attribute Space [byte] */
//#define PCMCIA0MemSp	PCMCIAMemSp	/* PCMCIA 0 Memory Space [byte]    */

//#define PCMCIA1Sp	PCMCIASp	/* PCMCIA 1 Space [byte]           */
//#define PCMCIA1IOSp	PCMCIAIOSp	/* PCMCIA 1 I/O Space [byte]       */
//#define PCMCIA1AttrSp	PCMCIAAttrSp	/* PCMCIA 1 Attribute Space [byte] */
//#define PCMCIA1MemSp	PCMCIAMemSp	/* PCMCIA 1 Memory Space [byte]    */

//#define _PCMCIA(Nb)	        	/* PCMCIA [0..1]                   */ \
//                	(0x20000000 + (Nb)*PCMCIASp)
//#define _PCMCIAIO(Nb)	_PCMCIA (Nb)	/* PCMCIA I/O [0..1]               */
//#define _PCMCIAAttr(Nb)	        	/* PCMCIA Attribute [0..1]         */ \
//                	(_PCMCIA (Nb) + 2*PCMCIAPrtSp)
//#define _PCMCIAMem(Nb)	        	/* PCMCIA Memory [0..1]            */ \
//                	(_PCMCIA (Nb) + 3*PCMCIAPrtSp)
//
//#define _PCMCIA0	_PCMCIA (0)	/* PCMCIA 0                        */
//#define _PCMCIA0IO	_PCMCIAIO (0)	/* PCMCIA 0 I/O                    */
//#define _PCMCIA0Attr	_PCMCIAAttr (0)	/* PCMCIA 0 Attribute              */
//#define _PCMCIA0Mem	_PCMCIAMem (0)	/* PCMCIA 0 Memory                 */

//#define _PCMCIA1	_PCMCIA (1)	/* PCMCIA 1                        */
//#define _PCMCIA1IO	_PCMCIAIO (1)	/* PCMCIA 1 I/O                    */
//#define _PCMCIA1Attr	_PCMCIAAttr (1)	/* PCMCIA 1 Attribute              */
//#define _PCMCIA1Mem	_PCMCIAMem (1)	/* PCMCIA 1 Memory                 */

//#define _PCMCIA(Nb)	        	/* PCMCIA [0..1]                   */ \
//                	(0x20000000 + (Nb)*PCMCIASp)
#define _PCMCIAIO(Nb)		0x11000000	/* PCMCIA I/O [0..1]               */
#define _PCMCIAAttr(Nb)	    0x10000000 	/* PCMCIA Attribute [0..1]         */ 
#define _PCMCIAMem(Nb)	    0x10800000 	/* PCMCIA Memory [0..1]            */ 

#define _PCMCIA0	_PCMCIA (0)	/* PCMCIA 0                        */
#define _PCMCIA0IO	_PCMCIAIO (0)	/* PCMCIA 0 I/O                    */
#define _PCMCIA0Attr	_PCMCIAAttr (0)	/* PCMCIA 0 Attribute              */
#define _PCMCIA0Mem	_PCMCIAMem (0)	/* PCMCIA 0 Memory                 */

//#define _PCMCIA1	_PCMCIA (1)	/* PCMCIA 1                        */
//#define _PCMCIA1IO	_PCMCIAIO (1)	/* PCMCIA 1 I/O                    */
//#define _PCMCIA1Attr	_PCMCIAAttr (1)	/* PCMCIA 1 Attribute              */
//#define _PCMCIA1Mem	_PCMCIAMem (1)	/* PCMCIA 1 Memory                 */

#endif  /* __ASSEMBLY__ */

#endif
