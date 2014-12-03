/*
 * linux/include/asm/arch-cotulla/cotulla.h
 * 
 * Copyright (c) 2001 Lineo, Inc. 
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * ChangeLog:
 *  04-04-2002 Richard Rau : add IRQ def. of Discovery
 */

#ifndef _COTULLA_H_
#define _COTULLA_H_

//#define io_p2v(PhAdd)   (PhAdd)
//#define io_p2v(PhAdd)   (0xb0000000 + PhAdd)
#define io_p2v(PhAdd)   ((PhAdd >= 0x40000000) ? ((PhAdd >= 0x48000000) ? (0xb5000000 + PhAdd ) : (0xb0000000 + PhAdd)) : (0xd0000000 + PhAdd))

#define COTULLA_RAMBASE      0xa0000000

#ifndef __ASSEMBLY__
#include <asm/types.h>

#if 0
# define __REG(x)	(*((volatile u32 *)io_p2v(x)))
#else
/*
 * This __REG() version gives the same results as the one above,  except
 * that we are fooling gcc somehow so it generates far better and smaller
 * assembly code for access to contigous registers.  It's a shame that gcc
 * doesn't guess this by itself.
 */
typedef struct { volatile u32 offset[4096]; } __regbase;
# define __REGP(x)	((__regbase *)((x)&~4095))->offset[((x)&4095)>>2]
# define __REG(x)	__REGP(io_p2v(x))
#endif

/* Let's kick gcc's ass again... */
# define __REG2(x,y)	\
	( __builtin_constant_p(y) ? (__REG((x) + (y))) \
				  : (*(volatile u32 *)((u32)&__REG(x) + (y))) )

# define __PREG(x)	(io_v2p((u32)&(x)))

#else

# define __REG(x)	io_p2v(x)
# define __PREG(x)	io_v2p(x)

#endif

#ifndef __ASSEMBLY__
/* A bit of stuff pulled from 2.4.18 pxa dma.h, used in the USB driver */

typedef struct {
	volatile u32 ddadr;	/* Points to the next descriptor + flags */
	volatile u32 dsadr;	/* DSADR value for the current transfer */
	volatile u32 dtadr;	/* DTADR value for the current transfer */
	volatile u32 dcmd;	/* DCMD value for the current transfer */
} pxa_dma_desc;
#endif

/* 
 * Clock Manager Controle registers
 *
 * Registers
 *    CCCR	        Core Clock Config register.
 *    CKEN              Clock Enable register.
 *    OSCC              Osillator Config register.
 *
*/

#define _CCCR	        0x41300000    /* Core Clock Config register */
#define _CKEN           0x41300004    /* Clock Enable register */
#define _OSCC           0x41300008    /* Osillator Config register */

#define CCCR	        __REG(_CCCR)
#define CKEN            __REG(_CKEN)
#define OSCC            __REG(_OSCC)


/*
 * Power Manager (PM) control registers
 *
 * Registers
 *    PMCR              Power Manager (PM) Control Register (read/write).
 *    PSSR              Power Manager (PM) Sleep Status Register (read/write).
 *    PSPR              Power Manager (PM) Scratch-Pad Register (read/write).
 *    PWER              Power Manager (PM) Wake-up Enable Register
 *                      (read/write).
 *    PRER		Power Manager (PM) GPIO Rising-Edge Detect Enable  
 *			Register(read/write)
 *    PFER		Power Manager (PM) GPIO Falling-Edge Detect Enable  
 *			Register(read/write)
 *    PEDR		Power Manager (PM) GPIO Edge Detect Enable Register
 *			(read/write)
 *    PCFR		Power Manager (PM) General ConFiguration Register
 *                      (read/write).
 *    GGSR0		Power Manager (PM) General-Purpose Input/Output (GPIO)
 *                      Sleep state Register for GP[31-0](read/write).
 *    GGSR1		Power Manager (PM) General-Purpose Input/Output (GPIO)
 *                      Sleep state Register for GP[63-32](read/write).
 *    GGSR2		Power Manager (PM) General-Purpose Input/Output (GPIO)
 *                      Sleep state Register for GP[84-64](read/write).
 */

#define _PMCR           0x40f00000      /* PM Control Reg.                 */
#define _PSSR           0x40f00004      /* PM Sleep Status Reg.            */
#define _PSPR           0x40f00008      /* PM Scratch-Pad Reg.             */
#define _PWER           0x40f0000C      /* PM Wake-up Enable Reg.          */
#define _PRER           0x40f00010      /* PM GPIO Rising-Edge Enable Reg. */
#define _PFER           0x40f00014      /* PM GPIO Falling-Edge Enable Reg.*/
#define _PEDR           0x40f00018      /* PM GPIO Edge Detect Enable      */
#define _PCFR           0x40f0001C      /* PM general ConFiguration Reg.   */
#define _GGSR0          0x40f00020      /* PM GPIO Sleep State Reg. [31-0] */
#define _GGSR1          0x40f00024      /* PM GPIO Sleep State Reg. [63-32]*/
#define _GGSR2          0x40f00028      /* PM GPIO Sleep State Reg. [84-64]*/
#define _RCSR          0x40f00030      /* Reset control register */

#define PMCR            __REG(_PMCR)
#define PSSR            __REG(_PSSR)
#define PSPR            __REG(_PSPR)
#define PWER            __REG(_PWER)
#define PRER            __REG(_PRER)
#define PFER            __REG(_PFER)
#define PEDR            __REG(_PEDR)
#define PCFR            __REG(_PCFR)
#define GGSR0           __REG(_GGSR0)
#define GGSR1           __REG(_GGSR1)
#define GGSR2           __REG(_GGSR2)
#define RCSR           __REG(_RCSR)

/*
 * Operating System (OS) timer control registers
 *
 * Registers
 *    OSMR0             Operating System (OS) timer Match Register 0
 *                      (read/write).
 *    OSMR1             Operating System (OS) timer Match Register 1
 *                      (read/write).
 *    OSMR2             Operating System (OS) timer Match Register 2
 *                      (read/write).
 *    OSMR3             Operating System (OS) timer Match Register 3
 *                      (read/write).
 *    OSSR              Operating System (OS) timer Status Register
 *                      (read/write).
 *    OWER              Operating System (OS) timer Watch-dog Enable Register
 *                      (read/write).
 *    OIER              Operating System (OS) timer Interrupt Enable Register
 *                      (read/write).
 */

#define _OSMR(Nb)           (0x40a00000 + (Nb)*4)

#define _OSMR0          _OSMR (0)       /* OS timer Match Reg. 0           */
#define _OSMR1          _OSMR (1)       /* OS timer Match Reg. 1           */
#define _OSMR2          _OSMR (2)       /* OS timer Match Reg. 2           */
#define _OSMR3          _OSMR (3)       /* OS timer Match Reg. 3           */
#define _OSCR           0x40a00010      /* OS timer Counter Reg.           */
#define _OSSR           0x40a00014      /* OS timer Status Reg.            */
#define _OWER           0x40a00018      /* OS timer Watch-dog Enable Reg.  */
#define _OIER           0x40a0001C      /* OS timer Interrupt Enable Reg.  */

#define OSMR                ((volatile long *) io_p2v (_OSMR (0)))
#define OSMR0           __REG(_OSMR(0)) /* OS timer Match Reg. 0 */
#define OSMR1           __REG(_OSMR(1)) /* OS timer Match Reg. 1 */
#define OSMR2           __REG(_OSMR(2)) /* OS timer Match Reg. 2 */
#define OSMR3           __REG(_OSMR(3)) /* OS timer Match Reg. 3 */
#define OSCR            __REG(_OSCR)
#define OSSR            __REG(_OSSR)
#define OWER            __REG(_OWER)
#define OIER            __REG(_OIER)

#define OSSR_M(Nb)      (0x00000001 << (Nb))
#define OSSR_M0         OSSR_M (0)      /* Match detected 0                */
#define OSSR_M1         OSSR_M (1)      /* Match detected 1                */
#define OSSR_M2         OSSR_M (2)      /* Match detected 2                */
#define OSSR_M3         OSSR_M (3)      /* Match detected 3                */

#define OWER_WME        0x00000001      /* Watch-dog Match Enable          */
                                        /* (set only)                      */

#define OIER_E(Nb)      (0x00000001 << (Nb))
#define OIER_E0         OIER_E (0)      /* match interrupt Enable 0        */
#define OIER_E1         OIER_E (1)      /* match interrupt Enable 1        */
#define OIER_E2         OIER_E (2)      /* match interrupt Enable 2        */
#define OIER_E3         OIER_E (3)      /* match interrupt Enable 3        */


/*
 * General-Purpose Input/Output (GPIO) control registers
 *
 * Registers
 *    GPLR              General-Purpose Input/Output (GPIO) Pin Level
 *                      Register (read).
 *    GPDR              General-Purpose Input/Output (GPIO) Pin Direction
 *                      Register (read/write).
 *    GPSR              General-Purpose Input/Output (GPIO) Pin output Set
 *                      Register (write).
 *    GPCR              General-Purpose Input/Output (GPIO) Pin output Clear
 *                      Register (write).
 *    GRER              General-Purpose Input/Output (GPIO) Rising-Edge
 *                      detect Register (read/write).
 *    GFER              General-Purpose Input/Output (GPIO) Falling-Edge
 *                      detect Register (read/write).
 *    GEDR              General-Purpose Input/Output (GPIO) Edge Detect
 *                      status Register (read/write).
 *    GAFR              General-Purpose Input/Output (GPIO) Alternate
 *                      Function Register (read/write).
 *
 * Clock
 *    fcpu, Tcpu        Frequency, period of the CPU core clock (CCLK).
 */

#define _GPLR(Nb)           (0x40e00000 + (Nb)*4)
#define _GPLR0          _GPLR (0)       /* GPIO Pin Level Reg. [31-0]      */
#define _GPLR1          _GPLR (1)       /* GPIO Pin Level Reg. [63-32]     */
#define _GPLR2          _GPLR (2)       /* GPIO Pin Level Reg. [84-64]     */

#define _GPDR(Nb)           (0x40e0000c + (Nb)*4)
#define _GPDR0          _GPDR (0)       /* GPIO Pin Direction Reg. [31-0]  */
#define _GPDR1          _GPDR (1)       /* GPIO Pin Direction Reg. [63-32] */
#define _GPDR2          _GPDR (2)       /* GPIO Pin Direction Reg. [84-64] */

#define _GPSR(Nb)           (0x40e00018 + (Nb)*4)
#define _GPSR0          _GPSR (0)       /* GPIO Pin output Set Reg. [31-0] */
#define _GPSR1          _GPSR (1)       /* GPIO Pin output Set Reg. [63-32]*/
#define _GPSR2          _GPSR (2)       /* GPIO Pin output Set Reg. [84-64]*/

#define _GPCR(Nb)           (0x40e00024 + (Nb)*4)
#define _GPCR0          _GPCR (0)       /* GPIO Pin output Clear Reg. [31-0] */
#define _GPCR1          _GPCR (1)       /* GPIO Pin output Clear Reg. [63-32]*/
#define _GPCR2          _GPCR (2)       /* GPIO Pin output Clear Reg. [84-64]*/

#define _GRER(Nb)           (0x40e00030 + (Nb)*4)
#define _GRER0          _GRER (0)       /* GPIO Rising-Edge detect Reg.[31-0]*/
#define _GRER1          _GRER (1)       /*                            [63-32]*/
#define _GRER2          _GRER (2)       /*                            [84-64]*/

#define _GFER(Nb)           (0x40e0003c + (Nb)*4)
#define _GFER0          _GFER (0)       /*GPIO Falling-Edge detect Reg.[31-0]*/
#define _GFER1          _GFER (1)       /*                            [63-32]*/
#define _GFER2          _GFER (2)       /*                            [84-64]*/

#define _GEDR(Nb)           (0x40e00048 + (Nb)*4)
#define _GEDR0          _GEDR (0)       /* GPIO Edge detect status Reg.[31-0]*/
#define _GEDR1          _GEDR (1)       /*                            [63-32]*/
#define _GEDR2          _GEDR (2)       /*                            [84-64]*/

#define _GPAFR0_L       0x40e00054    /* GPIO Alternate Function Reg.[15-0]  */
#define _GPAFR0_U       0x40e00058    /* GPIO Alternate Function Reg.[31-16] */
#define _GPAFR1_L       0x40e0005c    /* GPIO Alternate Function Reg.[47-32] */
#define _GPAFR1_U       0x40e00060    /* GPIO Alternate Function Reg.[63-48] */
#define _GPAFR2_L       0x40e00064    /* GPIO Alternate Function Reg.[79-64] */
#define _GPAFR2_U       0x40e00068    /* GPIO Alternate Function Reg.[80]    */

#define GPLR(n)                __REG(_GPLR (n))

//#define GPDR                __REG(_GPDR (0))
#define GPDR(n)             __REG(_GPDR (n))

#define GPDR0           (GPDR [0])
#define GPDR1           (GPDR [1])
#define GPDR2           (GPDR [2])

#define GPSR(n)         __REG(_GPSR (n))

#define GPSR0           (GPSR [0])
#define GPSR1           (GPSR [1])
#define GPSR2           (GPSR [2])

#define GPCR(n)         __REG(_GPCR (n))

#define GPCR0           (GPCR [0])
#define GPCR1           (GPCR [1])
#define GPCR2           (GPCR [2])

#define GRER(n)                __REG(_GRER (n))

#define GFER(n)                __REG(_GFER (n))

#define GEDR(n)                __REG(_GEDR (n))

#define GPAFR0_L        __REG(_GPAFR0_L)
#define GPAFR0_U        __REG(_GPAFR0_U)
#define GPAFR1_L        __REG(_GPAFR1_L)
#define GPAFR1_U        __REG(_GPAFR1_U)
#define GPAFR2_L        __REG(_GPAFR2_L)
#define GPAFR2_U        __REG(_GPAFR2_U)

#define GPIO_MIN        (0)
#define GPIO_MAX        (27)

#define GPIO_GPIO(Nb)    /* GPIO [0..31]                                   */ \
                       (((Nb) < 32) ? (0x00000001 << (Nb)) : (\
                        /* GPIO [32..63]                                   */ \
                        ((Nb) < 64) ? (0x00000001 << (Nb - 32)) : \
                        /* GPIO [64..80]                                   */ \
                        (0x00000001 << (Nb - 64))))

#define GPIO_GPIO0      GPIO_GPIO (0)   /* GPIO  [0]                       */
#define GPIO_GPIO1      GPIO_GPIO (1)   /* GPIO  [1]                       */
#define GPIO_GPIO2      GPIO_GPIO (2)   /* GPIO  [2]                       */
#define GPIO_GPIO3      GPIO_GPIO (3)   /* GPIO  [3]                       */
#define GPIO_GPIO4      GPIO_GPIO (4)   /* GPIO  [4]                       */
#define GPIO_GPIO5      GPIO_GPIO (5)   /* GPIO  [5]                       */
#define GPIO_GPIO6      GPIO_GPIO (6)   /* GPIO  [6]                       */
#define GPIO_GPIO7      GPIO_GPIO (7)   /* GPIO  [7]                       */
#define GPIO_GPIO8      GPIO_GPIO (8)   /* GPIO  [8]                       */
#define GPIO_GPIO9      GPIO_GPIO (9)   /* GPIO  [9]                       */
#define GPIO_GPIO10     GPIO_GPIO (10)  /* GPIO [10]                       */
#define GPIO_GPIO11     GPIO_GPIO (11)  /* GPIO [11]                       */
#define GPIO_GPIO12     GPIO_GPIO (12)  /* GPIO [12]                       */
#define GPIO_GPIO13     GPIO_GPIO (13)  /* GPIO [13]                       */
#define GPIO_GPIO14     GPIO_GPIO (14)  /* GPIO [14]                       */
#define GPIO_GPIO15     GPIO_GPIO (15)  /* GPIO [15]                       */
#define GPIO_GPIO16     GPIO_GPIO (16)  /* GPIO [16]                       */
#define GPIO_GPIO17     GPIO_GPIO (17)  /* GPIO [17]                       */
#define GPIO_GPIO18     GPIO_GPIO (18)  /* GPIO [18]                       */
#define GPIO_GPIO19     GPIO_GPIO (19)  /* GPIO [19]                       */
#define GPIO_GPIO20     GPIO_GPIO (20)  /* GPIO [20]                       */
#define GPIO_GPIO21     GPIO_GPIO (21)  /* GPIO [21]                       */
#define GPIO_GPIO22     GPIO_GPIO (22)  /* GPIO [22]                       */
#define GPIO_GPIO23     GPIO_GPIO (23)  /* GPIO [23]                       */
#define GPIO_GPIO24     GPIO_GPIO (24)  /* GPIO [24]                       */
#define GPIO_GPIO25     GPIO_GPIO (25)  /* GPIO [25]                       */
#define GPIO_GPIO26     GPIO_GPIO (26)  /* GPIO [26]                       */
#define GPIO_GPIO27     GPIO_GPIO (27)  /* GPIO [27]                       */
#define GPIO_GPIO28     GPIO_GPIO (28)  /* GPIO [28]                       */
#define GPIO_GPIO29     GPIO_GPIO (29)  /* GPIO [29]                       */
#define GPIO_GPIO30     GPIO_GPIO (30)  /* GPIO [30]                       */
#define GPIO_GPIO31     GPIO_GPIO (31)  /* GPIO [31]                       */
#define GPIO_GPIO32     GPIO_GPIO (32)  /* GPIO [32]                       */
#define GPIO_GPIO33     GPIO_GPIO (33)  /* GPIO [33]                       */
#define GPIO_GPIO34     GPIO_GPIO (34)  /* GPIO [34]                       */
#define GPIO_GPIO35     GPIO_GPIO (35)  /* GPIO [35]                       */
#define GPIO_GPIO36     GPIO_GPIO (36)  /* GPIO [36]                       */
#define GPIO_GPIO37     GPIO_GPIO (37)  /* GPIO [37]                       */
#define GPIO_GPIO38     GPIO_GPIO (38)  /* GPIO [38]                       */
#define GPIO_GPIO39     GPIO_GPIO (39)  /* GPIO [39]                       */
#define GPIO_GPIO40     GPIO_GPIO (40)  /* GPIO [40]                       */
#define GPIO_GPIO41     GPIO_GPIO (41)  /* GPIO [41]                       */
#define GPIO_GPIO42     GPIO_GPIO (42)  /* GPIO [42]                       */
#define GPIO_GPIO43     GPIO_GPIO (43)  /* GPIO [43]                       */
#define GPIO_GPIO44     GPIO_GPIO (44)  /* GPIO [44]                       */
#define GPIO_GPIO45     GPIO_GPIO (45)  /* GPIO [45]                       */
#define GPIO_GPIO46     GPIO_GPIO (46)  /* GPIO [46]                       */
#define GPIO_GPIO47     GPIO_GPIO (47)  /* GPIO [47]                       */
#define GPIO_GPIO48     GPIO_GPIO (48)  /* GPIO [48]                       */
#define GPIO_GPIO49     GPIO_GPIO (49)  /* GPIO [49]                       */
#define GPIO_GPIO50     GPIO_GPIO (50)  /* GPIO [50]                       */
#define GPIO_GPIO51     GPIO_GPIO (51)  /* GPIO [51]                       */
#define GPIO_GPIO52     GPIO_GPIO (52)  /* GPIO [52]                       */
#define GPIO_GPIO53     GPIO_GPIO (53)  /* GPIO [53]                       */
#define GPIO_GPIO54     GPIO_GPIO (54)  /* GPIO [54]                       */
#define GPIO_GPIO55     GPIO_GPIO (55)  /* GPIO [55]                       */
#define GPIO_GPIO56     GPIO_GPIO (56)  /* GPIO [56]                       */
#define GPIO_GPIO57     GPIO_GPIO (57)  /* GPIO [57]                       */
#define GPIO_GPIO58     GPIO_GPIO (58)  /* GPIO [58]                       */
#define GPIO_GPIO59     GPIO_GPIO (59)  /* GPIO [59]                       */
#define GPIO_GPIO60     GPIO_GPIO (60)  /* GPIO [60]                       */
#define GPIO_GPIO61     GPIO_GPIO (61)  /* GPIO [61]                       */
#define GPIO_GPIO62     GPIO_GPIO (62)  /* GPIO [62]                       */
#define GPIO_GPIO63     GPIO_GPIO (63)  /* GPIO [63]                       */
#define GPIO_GPIO64     GPIO_GPIO (64)  /* GPIO [64]                       */
#define GPIO_GPIO65     GPIO_GPIO (65)  /* GPIO [65]                       */
#define GPIO_GPIO66     GPIO_GPIO (66)  /* GPIO [66]                       */
#define GPIO_GPIO67     GPIO_GPIO (67)  /* GPIO [67]                       */
#define GPIO_GPIO68     GPIO_GPIO (68)  /* GPIO [68]                       */
#define GPIO_GPIO69     GPIO_GPIO (69)  /* GPIO [69]                       */
#define GPIO_GPIO70     GPIO_GPIO (70)  /* GPIO [70]                       */
#define GPIO_GPIO71     GPIO_GPIO (71)  /* GPIO [71]                       */
#define GPIO_GPIO72     GPIO_GPIO (72)  /* GPIO [72]                       */
#define GPIO_GPIO73     GPIO_GPIO (73)  /* GPIO [73]                       */
#define GPIO_GPIO74     GPIO_GPIO (74)  /* GPIO [74]                       */
#define GPIO_GPIO75     GPIO_GPIO (75)  /* GPIO [75]                       */
#define GPIO_GPIO76     GPIO_GPIO (76)  /* GPIO [76]                       */
#define GPIO_GPIO77     GPIO_GPIO (77)  /* GPIO [77]                       */
#define GPIO_GPIO78     GPIO_GPIO (78)  /* GPIO [78]                       */
#define GPIO_GPIO79     GPIO_GPIO (79)  /* GPIO [79]                       */
#define GPIO_GPIO80     GPIO_GPIO (80)  /* GPIO [80]                       */

#define GPIO_SSP_SCLK   GPIO_GPIO (23)  /*  SSP Serial CLocK (O)           */
#define GPIO_SSP_CS     GPIO_GPIO (24)  /*  SSP Chip Select (O)            */
#define GPIO_SSP_TXD    GPIO_GPIO (25)  /*  SSP Transmit Data (O)          */
#define GPIO_SSP_RXD    GPIO_GPIO (26)  /*  SSP Receive Data (I)           */
                                        /* ser. port 1:                    */
#define GPIO_AC97_BITCLK GPIO_GPIO (28) /*  AC97 Bit Clock (I)             */
#define GPIO_AC97_SIN   GPIO_GPIO (29)  /*  AC97 Serial Input Data (I)     */
#define GPIO_AC97_SOUT  GPIO_GPIO (30)  /*  AC97 Serial Output Data (O)    */
#define GPIO_AC97_SYNC  GPIO_GPIO (31)  /*  AC97 Synchronizer (O)          */
#define GPIO_UART_RXD   GPIO_GPIO (34)  /*  UART Receive Data (I)          */
#define GPIO_UART_TXD   GPIO_GPIO (39)  /*  UART Transmit Data (O)         */

#define GPIO_SDLC_SCLK  GPIO_GPIO (16)  /*  SDLC Sample CLocK (I/O)        */
#define GPIO_SDLC_AAF   GPIO_GPIO (17)  /*  SDLC Abort After Frame (O)     */

#define GPIO_UART_SCLK1 GPIO_GPIO (18)  /*  UART Sample CLocK 1 (I)        */

/*
 * Interrupt Controller (IC) control registers
 *
 * Registers
 *    ICIP              Interrupt Controller (IC) Interrupt ReQuest (IRQ)
 *                      Pending register (read).
 *    ICMR              Interrupt Controller (IC) Mask Register (read/write).
 *    ICLR              Interrupt Controller (IC) Level Register (read/write).
 *    ICCR              Interrupt Controller (IC) Control Register
 *                      (read/write).
 *                      [The ICCR register is only implemented in versions 2.0
 *                      (rev. = 8) and higher of the StrongARM SA-1100.]
 *    ICFP              Interrupt Controller (IC) Fast Interrupt reQuest
 *                      (FIQ) Pending register (read).
 *    ICPR              Interrupt Controller (IC) Pending Register (read).
 *                      [The ICPR register is active low (inverted) in
 *                      versions 1.0 (rev. = 1) and 1.1 (rev. = 2) of the
 *                      StrongARM SA-1100, it is active high (non-inverted) in
 *                      versions 2.0 (rev. = 8) and higher.]
 */

#define _ICIP         0x40d00000    /* IC IRQ Pendding register */
#define _ICMR         0x40d00004    /* IC Mask register */
#define _ICLR         0x40d00008    /* IC Level register */
#define _ICPR         0x40d00010    /* IC Pendding register */
#define _ICCR         0x40d00014    /* IC Control register */
#define _ICFP         0x40d0000c    /* IC FIQ Pendding register */
                      
#define ICIP          __REG(_ICIP)
#define ICMR          __REG(_ICMR)
#define ICLR          __REG(_ICLR)
#define ICPR          __REG(_ICPR)
#define ICCR          __REG(_ICCR)
#define ICFP          __REG(_ICFP)



/*
    DMA-C
*/

#define DCSR_RUN		(1 << 31)
#define DCSR_NODESCFETCH	(1 << 30)
#define DCSR_STOPIRQEN		(1 << 29)
#define DCSR_REQPEND		(1 << 8)
#define DCSR_STOPSTATE		(1 << 3)
#define DCSR_ENDINTR		(1 << 2)
#define DCSR_STARTINTR		(1 << 2)

#define DCMD_INCSRCADDR		(1 << 31)
#define DCMD_INCTRGADDR		(1 << 30)
#define DCMD_FLOWSRC		(1 << 29)
#define DCMD_FLOWTRG		(1 << 28)
#define DCMD_STARTIRQEN		(1 << 22)
#define DCMD_ENDIRQEN		(1 << 21)
#define DCMD_ENDIAN		(1 << 18)
#define DCMD_SIZE32		(3 << 16)
#define DCMD_SIZE16		(2 << 16)
#define DCMD_SIZE8		(1 << 16)
#define DCMD_WIDTH_4B		(3 << 14)
#define DCMD_WIDTH_2B		(2 << 14)
#define DCMD_WIDTH_1B		(1 << 14)

#define DDADR_STOP		1

#define _DCSR(Nb)	(0x40000000 + (Nb)*4)
#define _DRCMR(Nb)	(0x40000100 + (Nb)*4)
#define _DDADR(Nb)	(0x40000200 + (Nb)*0x10)
#define _DSADR(Nb)	(0x40000204 + (Nb)*0x10)
#define _DTADR(Nb)	(0x40000208 + (Nb)*0x10)
#define _DCMD(Nb)	(0x4000020c + (Nb)*0x10)
#define DCSR(n)		__REG(_DCSR(n))
#define DRCMR(n)	__REG(_DRCMR(n))
#define DDADR(n)	__REG(_DDADR(n))
#define DSADR(n)	__REG(_DSADR(n))
#define DTADR(n)	__REG(_DTADR(n))
#define DCMD(n)		__REG(_DCMD(n))


/*
    Memory Controller
*/

/*
 * Liquid Crystal Display (LCD) control registers
 *
 * Registers
 *    LCCR0             Liquid Crystal Display (LCD) Control Register 0
 *                      (read/write).
 *    LCCR1             Liquid Crystal Display (LCD) Control Register 1
 *                      (read/write).
 *    LCCR2             Liquid Crystal Display (LCD) Control Register 2
 *                      (read/write).
 *    LCCR3             Liquid Crystal Display (LCD) Control Register 3
 *                      (read/write).
 *    LCSR              Liquid Crystal Display (LCD) Status Register
 *                      (read/write).
 *    FDADR0		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 0 Frame Descriptor Address Regester
 *			(read/write).
 *    FDSDR0		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 0 Frame Source Address Regester
 *			(read/write).
 *    FIDR0		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 0 Frame ID Regester (read/write).
 *    LDCMD0		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 0 Command Regester (read/write).
 *    FDADR1		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 1 Frame Descriptor Address Regester
 *			(read/write).
 *    FDSDR1		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 1 Frame Source Address Regester
 *			(read/write).
 *    FIDR1		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 1 Frame ID Regester (read/write).
 *    LDCMD1		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 1 Command Regester (read/write).
 *    FBR0		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 0 Frame Branch Regester (read/write).	 *    FBR1		Liquid Crystal Display (LCD) Direct Memory Access
 *                      (DMA) Channel 1 Frame Branch Regester (read/write).	 *    LCSR		Liquid Crystal Display (LCD) Controller Status 
 *			Register
 *    LIIDR		Liquid Crystal Display (LCD) Controller Input ID
 *			Regester
 *    TRGBR		TMED RGB Seed Register.
 *    TCR		TMED Controll Register.
 */

#define _LCCR(Nb)           (0x44000000 + (Nb)*4)
#define _LCCR0          _LCCR (0)       /* LCD Controller register #0 */
#define _LCCR1          _LCCR (1)       /* LCD Controller register #1 */
#define _LCCR2          _LCCR (2)       /* LCD Controller register #2 */
#define _LCCR3          _LCCR (3)       /* LCD Controller register #3 */

#define _FDADR0         0x44000200    /* DMA Frame Description #0 */
#define _FSADR0         0x44000204    /* LCD Frame Source register #0 */
#define _FIDR0          0x44000208    /* LCD Frame ID register #0 */
#define _LDCMD0         0x4400020c    /* DMA Command register #0 */

#define _FDADR1         0x44000210    /*                       #1 */
#define _FSADR1         0x44000214    /*                       #1 */
#define _FIDR1          0x44000218    /*                       #1 */
#define _LDCMD1         0x4400021c    /*                      #1 */

#define _FBR0           0x44000020    /* DMA Frame Branch register #0 */
#define _FBR1           0x44000024    /*                           #1 */

#define _LCSR           0x44000038    /* LCD Status register */
#define _LIIDR          0x4400003c
#define _TRGBR          0x44000040
#define _TCR            0x44000044

#define LCCR(n)         __REG(_LCCR (n))

#define FDADR0          __REG(_FDADR0)
#define FSADR0          __REG(_FSADR0)
#define FIDR0           __REG(_FIDR0)
#define LDCMD0          __REG(_LDCMD0)

#define FDADR1          __REG(_FDADR1)
#define FSADR1          __REG(_FSADR1)
#define FIDR1           __REG(_FIDR1)
#define LDCMD1          __REG(_LDCMD1)

#define FBR0            __REG(_FBR0)
#define FBR1            __REG(_FBR1)

#define LCSR            __REG(_LCSR)
#define LIIDR           __REG(_LIIDR)
#define TRGBR           __REG(_TRGBR)
#define TCR             __REG(_TCR)


/*
 * Synchronous Serial Port (SSP) control registers
 *
 * Registers
 *    SSCR0             Synchronous Serial Port (SSP) Control
 *                      Register 0 (read/write).
 *    SSCR1             Synchronous Serial Port (SSP) Control
 *                      Register 1 (read/write).
 *    SSDR              Synchronous Serial Port (SSP) Data
 *                      Register (read/write).
 *    SSSR              Synchronous Serial Port (SSP) Status
 *                      Register (read/write).
 *
*/

#define SSCR0_DSS	0
#define SSCR0_FRF	4
#define SSCR0_ECS	6
#define SSCR0_SSE	7
#define SSCR0_SCR	8

#define SSCR1_RIE	0
#define SSCR1_TIE	1
#define SSCR1_LBM	2
#define SSCR1_SPO	3
#define SSCR1_SPH	4
#define SSCR1_MWDS	5
#define SSCR1_TFT	6
#define SSCR1_RFT	10

#define SSSR_TNF_MSK	(1u << 2)
#define SSSR_RNE_MSK	(1u << 3)
#define SSSR_BSY_MSK	(1u << 4)
#define SSSR_TFS_MSK	(1u << 5)
#define SSSR_RFS_MSK	(1u << 6)
#define SSSR_ROR_MSK	(1u << 7)
#define SSSR_TFL_MSK	(0xfu << 8)
#define SSSR_RFL_MSK	(0xfu << 12)

#define _SSCR0           0x41000000    /* SSP Control register #0 */
#define _SSCR1           0x41000004    /*                      #1 */
#define _SSDR            0x41000010    /* SSP Data register */
#define _SSSR            0x41000008    /* SSP Status register */

#define SSCR0            __REG(_SSCR0)
#define SSCR1            __REG(_SSCR1)
#define SSDR             __REG(_SSDR)
#define SSSR             __REG(_SSSR)
#define SSP_CKEN	0x08

/*
    UART
*/
#define COTULLA_UART1_BASE   (volatile unsigned char *)io_p2v(0x40100000) /* UART #1 */
#define COTULLA_UART2_BASE   (volatile unsigned char *)io_p2v(0x40200000) /* UART #2 */
#define COTULLA_UART3_BASE   (volatile unsigned char *)io_p2v(0x40700000) /* UART #3 */


/*
 *   AC97 Controller Unit registers
 *
 * Registers
 *    GCR		Global Control Register (read/write)
 *    GSR		Global Status Register (read/write)
 *    POCR		PCM-Out Control Register (read/write)
 *    PICR		PCM-In Control Register (read/write)
 *    POSR		PCM-Out Status Register (read/write)
 *    PISR		PCM-In Status Register (read/write)
 *    CAR		Codec Access Register (read/write)
 *    PCDR		PCM Data Register (read/write)
 *    MCCR		Mic-In Control Register (read/write)
 *    MCSR		Mic-In Status Register (read/write)
 *    MCDR		MIc-In Data Register (read)
 *    MOCR		Modem-Out Control Register (read/write)
 *    MICR		Modem-in Control register (read/write)
 *    MOSR		Modem-Out Status Register (read/write)
 *    MISR		Modem-In Status Register (read/write)
 *    MODR		Modem Data Register (read/write)
 */
#define _GCR            0x4050000c
#define _GSR            0x4050001c
#define _POCR           0x40500000
#define _PICR           0x40500004
#define _POSR           0x40500010
#define _PISR           0x40500014
#define _CAR            0x40500020 
#define _PCDR           0x40500040
#define _MCCR           0x40500008
#define _MCSR           0x40500040
#define _MCDR           0x40500060
#define _MOCR           0x40500100
#define _MICR           0x40500108
#define _MOSR           0x40500110
#define _MISR           0x40500118
#define _MODR           0x40500140
/* Accessing Primary Audio Codec Register Address */
#define PRI_AUDIO_CODEC_ADD(x)	(0x40500200 + ((x) << 1))
/* Accessing Secondary Audio Codec Register Address */
#define SEC_AUDIO_CODEC_ADD(x)	(0x40500300 + ((x) << 1))
/* Accessing Primary Modem Codec Register Address */
#define PRI_MODEM_CODEC_ADD(x)	(0x40500400 + ((x) << 1))
/* Accessing Secondary Modem Codec Register Address */
#define SEC_MODEM_CODEC_ADD(x)	(0x40500500 + ((x) << 1))

#define GCR             __REG(_GCR)
#define GSR             __REG(_GSR)
#define POCR            __REG(_POCR)
#define PICR            __REG(_PICR)
#define POSR            __REG(_POSR)
#define PISR            __REG(_PISR)
#define CAR             __REG(_CAR)
#define PCDR            __REG(_PCDR)
#define MCCR            __REG(_MCCR)
#define MCSR            __REG(_MCSR)
#define MCDR            __REG(_MCDR)
#define MOCR            __REG(_MOCR)
#define MICR            __REG(_MICR)
#define MOSR            __REG(_MOSR)
#define MISR            __REG(_MISR)
#define MODR            __REG(_MODR)
/* Accessing Primary Audio Codec Register */
#define PRI_AUDIO_CODEC_REG(x)	__REG(PRI_AUDIO_CODEC_ADD(x))
/* Accessing Secondary Audio Codec Register */
#define SEC_AUDIO_CODEC_REG(x)	__REG(SEC_AUDIO_CODEC_ADD(x))
/* Accessing Primary Modem Codec Register */
#define PRI_MODEM_CODEC_REG(x)	__REG(PRI_MODEM_CODEC_ADD(x))
/* Accessing Secondary Modem Codec Register */
#define SEC_MODEM_CODEC_REG(x)	__REG(SEC_MODEM_CODEC_ADD(x))

#define AC97_M(Nb)	(0x00000001 << (Nb))
#define GCR_GIE		AC97_M(0)    /* Codec GPI Interrupt Enable */
#define GCR_COLD_RST	AC97_M(1)    /* AC97 Cold Reset */
#define GCR_WARM_RST	AC97_M(2)    /* AC97 Warm Reset */
#define GCR_ACLINK_OFF	AC97_M(3)    /* AC-Link Shut off */
#define GCR_PRIRDY	AC97_M(8)    /* Primary Codec Ready Interrupt Enable */
#define GSR_PCR		AC97_M(8)    /* Primary Codec Ready */
#define GSR_RDCS	AC97_M(15)   /* Read Completion Status */
#define GSR_SDONE	AC97_M(18)   /* Status Done */
#define GSR_CDONE	AC97_M(19)   /* Command Done */
#define CAR_CAIP	AC97_M(0)    /* Codec Access In Progress */


/*
 * Real-Time Clock (RTC) control registers
 *
 * Registers
 *    RTAR              Real-Time Clock (RTC) Alarm Register (read/write).
 *    RCNR              Real-Time Clock (RTC) CouNt Register (read/write).
 *    RTTR              Real-Time Clock (RTC) Trim Register (read/write).
 *    RTSR              Real-Time Clock (RTC) Status Register (read/write).
 */

#define _RCNR           0x40900000      /* RTC CouNt Reg.                  */
#define _RTAR           0x40900004     /* RTC Alarm Reg.                  */
#define _RTSR           0x40900008      /* RTC Status Reg.                 */
#define _RTTR           0x4090000C      /* RTC Trim Reg.                   */

#define RCNR            __REG(_RCNR)
#define RTAR            __REG(_RTAR)
#define RTSR            __REG(_RTSR)
#define RTTR            __REG(_RTTR)


#define RTTR_C          Fld (16, 0)     /* clock divider Count - 1         */
#define RTTR_D          Fld (10, 16)    /* trim Delete count               */
                                        /* frtc = (1023*(C + 1) - D)*frtx/ */
                                        /*        (1023*(C + 1)^2)         */
                                        /* Trtc = (1023*(C + 1)^2)*Trtx/   */
                                        /*        (1023*(C + 1) - D)       */

#define RTSR_AL         0x00000001      /* ALarm detected                  */
#define RTSR_HZ         0x00000002      /* 1 Hz clock detected             */
#define RTSR_ALE        0x00000004      /* ALarm interrupt Enable          */
#define RTSR_HZE        0x00000008      /* 1 Hz clock interrupt Enable     */


// user output port setting
#define USER_LVCCON		0x0001U         // LCD VCC on
#define USER_LVDDON		0x0002U         // LCD VDD on
#define USER_BLTENA		0x0004U         // LCD FrontLight on
#define USER_LCDOEN		0x0008U         // LCD Output Enable
#define USER_GPSON		0x0010U         // GPS VCC on
#define USER_AMPON		0x0020U         // Internal AMP on
#define USER_BTLED		0x0040U         // Bluetooth LED ON
#define USER_IRDASD		0x0080U         // Irda Shutdown Control
#define USER_BTVCCON		0x0100U         // Bluetooth VCC on
#define USER_MMCON		0x0200U         // MMC/SD VCC on
#define USER_BATSD		0x0400U         // Battery Shutdown Control
#define USER_LCDVGON		0x0800U         // LCD +-15V on
#define USER_CF1PWREN		0x1000U         // CF Card Slot0 power on
#define USER_CF2PWREN		0x2000U         // CF Card Slot1 power on
#define USER_P1RESET		0x4000U         // CF Card Slot0 Reset
#define USER_P2RESET		0x8000U         // CF Card Slot1 Reset

#define PWM0_CE			(0x1u << 0)         // PWM #0 clock enable
#define PWM1_CE			(0x1u << 1)         // PWM #1 clock enable
#define AC97_CE			(0x1u << 2)         // AC97
#define SSP_CE			(0x1u << 3)         // SSP
#define STUART_CE		(0x1u << 5)         // Standard UART
#define FFUART_CE		(0x1u << 6)         // Full Function UART
#define BTUART_CE		(0x1u << 7)         // Bluetooth UART
#define I2S_CE			(0x1u << 9)         // I2S
#define ADC_CE			(0x1u << 10)        // ADC
#define USB_CE			(0x1u << 11)        // USB
#define MMC_CE			(0x1u << 12)        // MMC
#define ICP_CE			(0x1u << 13)        // ICP
#define I2C_CE			(0x1u << 14)        // I2C
#define LCD_CE			(0x1u << 16)        // LCD

/*
 * PWM registers
 */
 
#define _PWM_CTRL0	0x40B00000
#define _PWM_DUTY0	0x40B00004
#define _PWM_PERVAL0	0x40B00008
#define _PWM_CTRL1	0x40C00000
#define _PWM_DUTY1	0x40C00004
#define _PWM_PERVAL1	0x40C00008

#define PWM_CTRL0	__REG(_PWM_CTRL0)
#define PWM_DUTY0	__REG(_PWM_DUTY0)
#define PWM_PERVAL0	__REG(_PWM_PERVAL0)
#define PWM_CTRL1	__REG(_PWM_CTRL1)
#define PWM_DUTY1	__REG(_PWM_DUTY1)
#define PWM_PERVAL1	__REG(_PWM_PERVAL1)

/*
 * I2S controller for SOUND Driver
 */
#define _I2C_IBMR	0x40301680
#define _I2C_IDBR	0x40301688
#define _I2C_ICR	0x40301690
#define _I2C_ISR	0x40301698
#define _I2S_ISAR	0x403016A0

#define I2C_IBMR    __REG(_I2C_IBMR)
#define I2C_IDBR    __REG(_I2C_IDBR)
#define I2C_ICR		__REG(_I2C_ICR)
#define I2C_ISR		__REG(_I2C_ISR)
#define I2C_ISAR    __REG(_I2S_ISAR)
                    
#define _I2S_SACR0	0x40400000
#define _I2S_SACR1	0x40400004
#define _I2S_SASR0	0x4040000C
#define _I2S_SADIV	0x40400060
#define _I2S_SAICR	0x40400018
#define _I2S_SAIMR	0x40400014
#define _I2S_SADR	0x40400080

#define I2S_SACR0	__REG(_I2S_SACR0)	
#define I2S_SACR1	__REG(_I2S_SACR1)	
#define I2S_SASR0	__REG(_I2S_SASR0)	
#define I2S_SADIV	__REG(_I2S_SADIV)	
#define I2S_SAICR	__REG(_I2S_SAICR)	
#define I2S_SAIMR	__REG(_I2S_SAIMR)	
#define I2S_SADR	__REG(_I2S_SADR)

/*
 * MMC Controller registers //Richard
 */
 
#define _MMC_STRPCL	0x41100000
#define _MMC_STAT	0x41100004
#define _MMC_CLKRT	0x41100008
#define _MMC_SPI	0x4110000C
#define _MMC_CMDAT	0x41100010
#define _MMC_RESTO	0x41100014
#define _MMC_RDTO	0x41100018
#define _MMC_BLKLEN	0x4110001C
#define _MMC_NOB	0x41100020
#define _MMC_PRTBUF	0x41100024
#define _MMC_I_MASK	0x41100028
#define _MMC_I_REG	0x4110002C
#define _MMC_CMD	0x41100030
#define _MMC_ARGH	0x41100034
#define _MMC_ARGL	0x41100038
#define _MMC_RES	0x4110003C
#define _MMC_RXFIFO	0X41100040
#define _MMC_TXFIFO	0X41100044


#define MMC_STRPCL	__REG(_MMC_STRPCL)
#define MMC_STAT	__REG(_MMC_STAT)
#define MMC_CLKRT	__REG(_MMC_CLKRT)
#define MMC_SPI		__REG(_MMC_SPI)
#define MMC_CMDAT	__REG(_MMC_CMDAT)
#define MMC_RESTO	__REG(_MMC_RESTO)
#define MMC_RDTO	__REG(_MMC_RDTO)
#define MMC_BLKLEN	__REG(_MMC_BLKLEN)
#define MMC_NOB		__REG(_MMC_NOB)
#define MMC_PRTBUF	__REG(_MMC_PRTBUF)
#define MMC_I_MASK	__REG(_MMC_I_MASK)
#define MMC_I_REG	__REG(_MMC_I_REG)
#define MMC_CMD		__REG(_MMC_CMD)
#define MMC_ARGH	__REG(_MMC_ARGH)
#define MMC_ARGL	__REG(_MMC_ARGL)
#define MMC_RES		__REG(_MMC_RES)
#define MMC_RXFIFO	__REG(_MMC_RXFIFO)
#define MMC_TXFIFO	__REG(_MMC_TXFIFO)

/*
 * USB Device Controller
 */

#define UDCCR		__REG(0x40600000)  /* UDC Control Register */
#define UDCCS0		__REG(0x40600010)  /* UDC Endpoint 0 Control/Status Register */
#define UDCCS1		__REG(0x40600014)  /* UDC Endpoint 1 (IN) Control/Status Register */
#define UDCCS2		__REG(0x40600018)  /* UDC Endpoint 2 (OUT) Control/Status Register */
#define UDCCS3		__REG(0x4060001C)  /* UDC Endpoint 3 (IN) Control/Status Register */
#define UDCCS4		__REG(0x40600020)  /* UDC Endpoint 4 (OUT) Control/Status Register */
#define UDCCS5		__REG(0x40600024)  /* UDC Endpoint 5 (Interrupt) Control/Status Register */
#define UDCCS6		__REG(0x40600028)  /* UDC Endpoint 6 (IN) Control/Status Register */
#define UDCCS7		__REG(0x4060002C)  /* UDC Endpoint 7 (OUT) Control/Status Register */
#define UDCCS8		__REG(0x40600030)  /* UDC Endpoint 8 (IN) Control/Status Register */
#define UDCCS9		__REG(0x40600034)  /* UDC Endpoint 9 (OUT) Control/Status Register */
#define UDCCS10		__REG(0x40600038)  /* UDC Endpoint 10 (Interrupt) Control/Status Register */
#define UDCCS11		__REG(0x4060003C)  /* UDC Endpoint 11 (IN) Control/Status Register */
#define UDCCS12		__REG(0x40600040)  /* UDC Endpoint 12 (OUT) Control/Status Register */
#define UDCCS13		__REG(0x40600044)  /* UDC Endpoint 13 (IN) Control/Status Register */
#define UDCCS14		__REG(0x40600048)  /* UDC Endpoint 14 (OUT) Control/Status Register */
#define UDCCS15		__REG(0x4060004C)  /* UDC Endpoint 15 (Interrupt) Control/Status Register */
#define UFNRH		__REG(0x40600060)  /* UDC Frame Number Register High */
#define UFNRL		__REG(0x40600064)  /* UDC Frame Number Register Low */
#define UBCR2		__REG(0x40600068)  /* UDC Byte Count Reg 2 */
#define UBCR4		__REG(0x4060006c)  /* UDC Byte Count Reg 4 */
#define UBCR7		__REG(0x40600070)  /* UDC Byte Count Reg 7 */
#define UBCR9		__REG(0x40600074)  /* UDC Byte Count Reg 9 */
#define UBCR12		__REG(0x40600078)  /* UDC Byte Count Reg 12 */
#define UBCR14		__REG(0x4060007c)  /* UDC Byte Count Reg 14 */
#define UDDR0		__REG(0x40600080)  /* UDC Endpoint 0 Data Register */
#define UDDR1		__REG(0x40600100)  /* UDC Endpoint 1 Data Register */
#define UDDR2		__REG(0x40600180)  /* UDC Endpoint 2 Data Register */
#define UDDR3		__REG(0x40600200)  /* UDC Endpoint 3 Data Register */
#define UDDR4		__REG(0x40600400)  /* UDC Endpoint 4 Data Register */
#define UDDR5		__REG(0x406000A0)  /* UDC Endpoint 5 Data Register */
#define UDDR6		__REG(0x40600600)  /* UDC Endpoint 6 Data Register */
#define UDDR7		__REG(0x40600680)  /* UDC Endpoint 7 Data Register */
#define UDDR8		__REG(0x40600700)  /* UDC Endpoint 8 Data Register */
#define UDDR9		__REG(0x40600900)  /* UDC Endpoint 9 Data Register */
#define UDDR10		__REG(0x406000C0)  /* UDC Endpoint 10 Data Register */
#define UDDR11		__REG(0x40600B00)  /* UDC Endpoint 11 Data Register */
#define UDDR12		__REG(0x40600B80)  /* UDC Endpoint 12 Data Register */
#define UDDR13		__REG(0x40600C00)  /* UDC Endpoint 13 Data Register */
#define UDDR14		__REG(0x40600E00)  /* UDC Endpoint 14 Data Register */
#define UDDR15		__REG(0x406000E0)  /* UDC Endpoint 15 Data Register */
#define UICR0		__REG(0x40600050)  /* UDC Interrupt Control Register 0 */
#define UICR1		__REG(0x40600054)  /* UDC Interrupt Control Register 1 */
#define USIR0		__REG(0x40600058)  /* UDC Status Interrupt Register 0 */
#define USIR1		__REG(0x4060005C)  /* UDC Status Interrupt Register 1 */

#ifndef __ASSEMBLY__
extern unsigned int cotulla_rev; /* Board revision */
#endif // !__ASSEMBLY__

#endif	// _COTULLA_H_
