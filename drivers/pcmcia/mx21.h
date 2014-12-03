#ifndef __MX21_h__
#define __MX21_h__

#include<asm/arch/platform.h>

typedef unsigned int U32;

#define MX21_PCMCIA_MAX_SOCK	1
#define MX21_PCMCIA_MAX_WINDOW	7

#define MX21_PCMCF_BASE		0xD4000000
#define PARTITION_SIZE		0x400000 //4M

#define _PCMCIA(x)		MX21_PCMCF_BASE
#define _PCMCIAIO(x)		_PCMCIA(x)
#define _PCMCIAAttr(x)		( _PCMCIA(x) + PARTITION_SIZE )
#define _PCMCIAMem(x)		( _PCMCIA(x) + 2*PARTITION_SIZE )
#define PCMCF_SPACE		(3*PARTITION_SIZE)

//#define PCMCIA_IO_ADDRESS(x)	(((x)-0xDF001000)+0xE4400000)

#define MX21_PCMCIA_BASE_ADDR	PCMCIA_IO_ADDRESS(0xdf002000)
#define MX21_PCMCIA_PIPR	(MX21_PCMCIA_BASE_ADDR)
#define MX21_PCMCIA_PSCR	(MX21_PCMCIA_BASE_ADDR + 0x4)
#define MX21_PCMCIA_PER 	(MX21_PCMCIA_BASE_ADDR + 0x8)

#define MX21_PCMCIA_PBR_BASE	(MX21_PCMCIA_BASE_ADDR + 0xc)
#define MX21_PCMCIA_POR_BASE	(MX21_PCMCIA_BASE_ADDR + 0x28)
#define MX21_PCMCIA_POFR_BASE	(MX21_PCMCIA_BASE_ADDR + 0x44)

#define MX21_PCMCIA_PBR0	(MX21_PCMCIA_BASE_ADDR + 0xc)
#define MX21_PCMCIA_POR0	(MX21_PCMCIA_BASE_ADDR + 0x28)
#define MX21_PCMCIA_POFR0	(MX21_PCMCIA_BASE_ADDR + 0x44)

#define MX21_PCMCIA_PBR1	(MX21_PCMCIA_BASE_ADDR + 0x10)
#define MX21_PCMCIA_POR1	(MX21_PCMCIA_BASE_ADDR + 0x2c)
#define MX21_PCMCIA_POFR1	(MX21_PCMCIA_BASE_ADDR + 0x48)

#define MX21_PCMCIA_PBR2	(MX21_PCMCIA_BASE_ADDR + 0x14)
#define MX21_PCMCIA_POR2	(MX21_PCMCIA_BASE_ADDR + 0x30)
#define MX21_PCMCIA_POFR2	(MX21_PCMCIA_BASE_ADDR + 0x4c)

#define MX21_PCMCIA_PBR3	(MX21_PCMCIA_BASE_ADDR + 0x18)
#define MX21_PCMCIA_POR3	(MX21_PCMCIA_BASE_ADDR + 0x34)
#define MX21_PCMCIA_POFR3	(MX21_PCMCIA_BASE_ADDR + 0x50)

#define MX21_PCMCIA_PBR4	(MX21_PCMCIA_BASE_ADDR + 0x1c)
#define MX21_PCMCIA_POR4	(MX21_PCMCIA_BASE_ADDR + 0x38)
#define MX21_PCMCIA_POFR4	(MX21_PCMCIA_BASE_ADDR + 0x54)

#define MX21_PCMCIA_PBR5	(MX21_PCMCIA_BASE_ADDR + 0x20)
#define MX21_PCMCIA_POR5	(MX21_PCMCIA_BASE_ADDR + 0x3c)
#define MX21_PCMCIA_POFR5	(MX21_PCMCIA_BASE_ADDR + 0x58)

#define MX21_PCMCIA_PBR6	(MX21_PCMCIA_BASE_ADDR + 0x24)
#define MX21_PCMCIA_POR6	(MX21_PCMCIA_BASE_ADDR + 0x40)
#define MX21_PCMCIA_POFR6	(MX21_PCMCIA_BASE_ADDR + 0x5c)

#define MX21_PCMCIA_PGCR	(MX21_PCMCIA_BASE_ADDR + 0x60)
#define MX21_PCMCIA_PGSR	(MX21_PCMCIA_BASE_ADDR + 0x64)

typedef struct _reg_layout {
	unsigned long shift;
	unsigned long mask;
} reg_layout_t;

/* PIPR: PCMCIA Input Pins Register (Read-only)
 *
 * PIPR layout (common part) is:  
 *  
 *   PWRON RDY BVD2/SPER_IN BVDC1/STSCHG_IN CD<1:0> WP VS<1:0>
 * 
 */

/* Index of elements of PIPR layout*/
#define PIPR_POWERON		6
#define PIPR_RDY		5
#define PIPR_BVD2		4
#define PIPR_SPER_IN		4
#define PIPR_BVD1		3
#define PIPR_STSCHG_IN		3
#define PIPR_CD			2
#define PIPR_WP			1
#define PIPR_VS			0

#define PIPR_MAX_INDEX	7
reg_layout_t pipr_layout[PIPR_MAX_INDEX]=
{
	{ 0, 0x3 },
	{ 2, 0x4 },
	{ 3, 0x18},
	{ 5, 0x20},
	{ 6, 0x40},
	{ 7, 0x80},
	{ 8, 0x100}
};

/* PSCR: PCMCIA Status Change Register
 *
 * PSCR layout (common part) is:  
 * [11:0] 
 *   POW RDYR RDYF RDYH RDYL BVDC2 BVDC1 CDC2 CDC1 WPC VSC2 VSC1
 * 
 */

/* Index of elements of PSCR layout*/
#define PSCR_POWC		11
#define PSCR_RDYR		10
#define PSCR_RDYF		9
#define PSCR_RDYH		8
#define PSCR_RDYL		7
#define PSCR_BVDC2		6
#define PSCR_BVDC1		5
#define PSCR_CDC2		4
#define PSCR_CDC1		3
#define PSCR_WPC		2
#define PSCR_VSC2		1
#define PSCR_VSC1		0

#define PSCR_MAX_INDEX	12
reg_layout_t pscr_layout[PSCR_MAX_INDEX]=
{
	{ 0,  0x1  },
	{ 1,  0x2  },
	{ 2,  0x4  },
	{ 3,  0x8  },
	{ 4,  0x10 },
	{ 5,  0x20 },
	{ 6,  0x40 },
	{ 7,  0x80 },
	{ 8,  0x100},
	{ 9,  0x200},
	{ 10, 0x400},
	{ 11, 0x800}
};
  
/* PER:  PCMCIA Enable Register
 *
 * PER layout (common part) is:  
 * [12:0] 
 *   ERRINTEN PWRONEN RDYRE RDYFE RDYHE RDYLE BVDE2 BVDE1 CDE2 CDE1 WPE VSE2 VSE1
 * 
 */

/* Index of elements of PER layout*/
#define PER_ERRINTEN		12
#define PER_PWRONEN		11
#define PER_RDYRE		10
#define PER_RDYFE		9
#define PER_RDYHE		8
#define PER_RDYLE		7
#define PER_BVDE2		6
#define PER_BVDE1		5
#define PER_CDE2		4
#define PER_CDE1		3
#define PER_WPE			2
#define PER_VSE2		1
#define PER_VSE1		0

#define PER_MAX_INDEX	13
reg_layout_t per_layout[PER_MAX_INDEX]=
{
	{ 0,  0x1  },
	{ 1,  0x2  },
	{ 2,  0x4  },
	{ 3,  0x8  },
	{ 4,  0x10 },
	{ 5,  0x20 },
	{ 6,  0x40 },
	{ 7,  0x80 },
	{ 8,  0x100},
	{ 9,  0x200},
	{ 10, 0x400},
	{ 11, 0x800},
	{ 12, 0x1000}
};

/* PBR: PCMCIA Base Register
 *
 * PBR layout is:  
 *
 *   PBA<31:0>
 *
 */
#define PBR_PBA		0
#define PBR_MAX_INDEX	1
reg_layout_t pbr_layout[PBR_MAX_INDEX]=
{
	{0, 0xffffffff}
};

/* POR: PCMCIA Option Register
 *
 * POR layout is:  
 *
 *   PV WPEN WP PRS<1:0> PPS PSL<6:0> PSST<5:0> PSHT<5:0> BSIZE<4:0>
 *
 */
 
/* Index of elements of POR layout*/
#define POR_PV			8
#define POR_WPEN		7
#define POR_WP			6
#define POR_PRS			5
#define POR_PPS			4
#define POR_PSL			3
#define POR_PSST		2
#define POR_PSHT		1
#define POR_BSIZE		0

#define POR_MAX_INDEX	9
reg_layout_t por_layout[POR_MAX_INDEX]=
{
	{ 0,  0x1f      },
	{ 5,  0x7e0     },
	{ 11, 0x1f800   },
	{ 17, 0xfe0000  },
	{ 24, 0x1000000 },
	{ 25, 0x6000000 },
	{ 27, 0x8000000 },
	{ 28, 0x10000000},
	{ 29, 0x20000000}
};

/* POFR: PCMCIA Offset Register
 *
 * POFR layout is:  
 *
 *   POFA<25:0>
 *
 */
#define POFR_POFA	0
#define POFR_MAX_INDEX	1
reg_layout_t pofr_layout[POFR_MAX_INDEX]=
{
	{0, 0x03ffffff}
};

/* PGCR: PCMCIA General Control Register
 *
 * PGCR layout is:  
 *
 *   ATASEL ATAMODE LPMEN SPKREN POE RESET
 *
 */
 
/* Index of elements of PGCR layout*/
#define PGCR_ATASEL		5
#define PGCR_ATAMODE		4
#define PGCR_LPMEN		3
#define PGCR_SPKREN		2
#define PGCR_POE		1
#define PGCR_RESET		0


#define PGCR_MAX_INDEX	6
reg_layout_t pgcr_layout[PGCR_MAX_INDEX]=
{
	{ 0,  0x1 },
	{ 1,  0x2 },
	{ 2,  0X4 },
	{ 3,  0X8 },
	{ 4,  0X10},
	{ 5,  0x20}
};
 
 /* PGSR: PCMCIA General Status Register
 *
 * PGSR layout is:  
 *
 *   NWINE LPE SE CDE WPE
 *
 */
 
/* Index of elements of PGSR layout*/
#define PGSR_NWINE	4
#define PGSR_LPE	3
#define PGSR_SE		2
#define PGSR_CDE	1
#define PGSR_WPE	0


#define PGSR_MAX_INDEX	5
reg_layout_t pgsr_layout[PGSR_MAX_INDEX]=
{
	{ 0,  0x1 },
	{ 1,  0x2 },
	{ 2,  0X4 },
	{ 3,  0X8 },
	{ 4,  0X10}
};

#define reg_element_set(reg, shift, mask, value) \
	( (reg) = ((reg)&(~(mask))) | (((value)<<(shift))&(mask)) )

#define reg_element_get(reg, shift, mask) \
	( ((reg)&(mask))>>(shift) )

#define pipr_read(element) reg_element_get(*(U32*)MX21_PCMCIA_PIPR, pipr_layout[element].shift, \
	pipr_layout[element].mask)

#define pipr_write(element,value) reg_element_set(*(U32*)MX21_PCMCIA_PIPR, pipr_layout[element].shift, \
	pipr_layout[element].mask,value)

#define pscr_read(element) reg_element_get(*(U32*)MX21_PCMCIA_PSCR, pscr_layout[element].shift, \
	pscr_layout[element].mask)

#define pscr_write(element,value) reg_element_set(*(U32*)MX21_PCMCIA_PSCR, pscr_layout[element].shift, \
	pscr_layout[element].mask,value)

#define per_read(element) reg_element_get(*(U32*)MX21_PCMCIA_PER, per_layout[element].shift, \
	per_layout[element].mask)

#define per_write(element,value) reg_element_set(*(U32*)MX21_PCMCIA_PER, per_layout[element].shift, \
	per_layout[element].mask,value)

#define pgcr_read(element) reg_element_get(*(U32*)MX21_PCMCIA_PGCR, pgcr_layout[element].shift, \
	pgcr_layout[element].mask)

#define pgcr_write(element,value) reg_element_set(*(U32*)MX21_PCMCIA_PGCR, pgcr_layout[element].shift, \
	pgcr_layout[element].mask,value)

#define pgsr_read(element) reg_element_get(*(U32*)MX21_PCMCIA_PGSR, pgsr_layout[element].shift, \
	pgsr_layout[element].mask)

#define pgsr_write(element,value) reg_element_set(*(U32*)MX21_PCMCIA_PGSR, pgsr_layout[element].shift, \
	pgsr_layout[element].mask,value)

#define pbr_read(window,element) reg_element_get(*(U32*)(MX21_PCMCIA_PBR_BASE+window*4), pbr_layout[element].shift, \
	pbr_layout[element].mask)

#define pbr_write(window,element,value) reg_element_set(*(U32*)(MX21_PCMCIA_PBR_BASE+window*4), pbr_layout[element].shift, \
	pbr_layout[element].mask,value)

#define por_read(window,element) reg_element_get(*(U32*)(MX21_PCMCIA_POR_BASE+window*4), por_layout[element].shift, \
	por_layout[element].mask)

#define por_write(window,element,value) reg_element_set(*(U32*)(MX21_PCMCIA_POR_BASE+window*4), por_layout[element].shift, \
	por_layout[element].mask,value)

#define pofr_read(window,element) reg_element_get(*(U32*)(MX21_PCMCIA_POFR_BASE+window*4), pofr_layout[element].shift, \
	pofr_layout[element].mask)

#define pofr_write(window,element,value) reg_element_set(*(U32*)(MX21_PCMCIA_POFR_BASE+window*4), pofr_layout[element].shift, \
	pofr_layout[element].mask,value)

#endif
