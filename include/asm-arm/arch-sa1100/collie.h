/*
 * linux/include/asm-arm/arch-sa1100/collie.h
 * 
 * This file contains the hardware specific definitions for Collie
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 * 
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 * 
 * ChangeLog:
 *   04-06-2001 Lineo Japan, Inc.
 *   04-16-2001 SHARP Corporation
 */
#ifndef __ASM_ARCH_COLLIE_H
#define __ASM_ARCH_COLLIE_H  1

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

/*
 * LoCoMo internal I/O mappings
 *
 * We have the following mapping:
 *      phys            virt
 *      40000000        f0000000
 */

/* LoCoMo I/O Base */
#define LCM_BASE	0x40000000
#define _LCM( x )	((x) + LCM_BASE)

#define LCM_p2v( x )    ((x) - LCM_BASE + 0xf0000000)
#define LCM_v2p( x )    ((x) - 0xf0000000 + LCM_BASE)

/* LCM version */
#define _LCM_VER	_LCM( 0x00 )

/* Pin status */
#define _LCM_ST		_LCM( 0x04 )

/* Pin status */
#define _LCM_C32K	_LCM( 0x08 )

/* Interrupt controller */
#define _LCM_ICR	_LCM( 0x0C )

/* MCS decoder for boot selecting */
#define _LCM_MCSX0	_LCM( 0x10 )
#define _LCM_MCSX1	_LCM( 0x14 )
#define _LCM_MCSX2	_LCM( 0x18 )
#define _LCM_MCSX3	_LCM( 0x1c )

/* Touch panel controller */
#define _LCM_ASD	_LCM( 0x20 )	/* AD start delay */
#define _LCM_HSD	_LCM( 0x28 )	/* HSYS delay */
#define _LCM_HSC	_LCM( 0x2c )	/* HSYS period */
#define _LCM_TADC	_LCM( 0x30 )	/* tablet ADC clock */

/* TFT signal */
#define _LCM_TC		_LCM( 0x38 )	/* TFT control signal */
#define _LCM_CPSD	_LCM( 0x3c )	/* CPS delay */

/* Key controller */
#define _LCM_KIB	_LCM( 0x40 )	/* KIB level */
#define _LCM_KSC	_LCM( 0x44 )	/* KSTRB control */
#define _LCM_KCMD	_LCM( 0x48 )	/* KSTRB command */
#define _LCM_KIC	_LCM( 0x4c )	/* Key interrupt */

/* Audio clock */
#define _LCM_ACC	_LCM( 0x54 )

/* SPI interface */
#define _LCM_SPIMD	_LCM( 0x60 )	/* SPI mode setting */
#define _LCM_SPICT	_LCM( 0x64 )	/* SPI mode control */
#define _LCM_SPIST	_LCM( 0x68 )	/* SPI status */
#define _LCM_SPIIS	_LCM( 0x70 )	/* SPI interrupt status */
#define _LCM_SPIWE	_LCM( 0x74 )	/* SPI interrupt status write enable */
#define _LCM_SPIIE	_LCM( 0x78 )	/* SPI interrupt enable */
#define _LCM_SPIIR	_LCM( 0x7c )	/* SPI interrupt request */
#define _LCM_SPITD	_LCM( 0x80 )	/* SPI transfer data write */
#define _LCM_SPIRD	_LCM( 0x84 )	/* SPI receive data read */
#define _LCM_SPITS	_LCM( 0x88 )	/* SPI transfer data shift */
#define _LCM_SPIRS	_LCM( 0x8C )	/* SPI receive data shift */

#define	LCM_SPI_TEND	(1 << 3)	/* Transfer end bit */
#define	LCM_SPI_OVRN	(1 << 2)	/* Over Run bit */
#define	LCM_SPI_RFW	(1 << 1)	/* write buffer bit */
#define	LCM_SPI_RFR	(1)		/* read buffer bit */

/* GPIO */
#define _LCM_GPD	_LCM( 0x90 )	/* GPIO direction */
#define _LCM_GPE	_LCM( 0x94 )	/* GPIO input enable */
#define _LCM_GPL	_LCM( 0x98 )	/* GPIO level */
#define _LCM_GPO	_LCM( 0x9c )	/* GPIO out data setteing */
#define _LCM_GRIE	_LCM( 0xa0 )	/* GPIO rise detection */
#define _LCM_GFIE	_LCM( 0xa4 )	/* GPIO fall detection */
#define _LCM_GIS	_LCM( 0xa8 )	/* GPIO edge detection status */
#define _LCM_GWE	_LCM( 0xac )	/* GPIO status write enable */
#define _LCM_GIE	_LCM( 0xb0 )	/* GPIO interrupt enable */
#define _LCM_GIR	_LCM( 0xb4 )	/* GPIO interrupt request */

#define LCM_GPIO0        (1<<0)
#define LCM_GPIO1        (1<<1)
#define LCM_GPIO2        (1<<2)
#define LCM_GPIO3        (1<<3)
#define LCM_GPIO4        (1<<4)
#define LCM_GPIO5        (1<<5)
#define LCM_GPIO6        (1<<6)
#define LCM_GPIO7        (1<<7)
#define LCM_GPIO8        (1<<8)
#define LCM_GPIO9        (1<<9)
#define LCM_GPIO10       (1<<10)
#define LCM_GPIO11       (1<<11)
#define LCM_GPIO12       (1<<12)
#define LCM_GPIO13       (1<<13)
#define LCM_GPIO14       (1<<14)
#define LCM_GPIO15       (1<<15)

/* Front light adjustment controller */
#define _LCM_ALS	_LCM( 0xc8 )	/* Adjust light cycle */
#define _LCM_ALD	_LCM( 0xcc )	/* Adjust light duty */

/* PCM audio interface */
#define _LCM_PAIF	_LCM( 0xd0 )

/* Long time timer */
#define _LCM_LTC	_LCM( 0xd8 )	/* LTC interrupt setting */
#define _LCM_LTINT	_LCM( 0xdc )	/* LTC interrupt */

/* DAC control signal for LCD (COMADJ ) */
#define _LCM_DAC	_LCM( 0xe0 )

/* LED controller */
#define _LCM_LPT0	_LCM( 0xe8 )	/* LEDPWM0 timer */
#define _LCM_LPT1	_LCM( 0xec )	/* LEDPWM1 timer */


#if LANGUAGE == C

/* LCM version */
#define LCM_VER		(*((volatile Word16 *) LCM_p2v (_LCM_VER)))

/* Pin status */
#define LCM_ST		(*((volatile Word16 *) LCM_p2v (_LCM_ST)))

/* CLK32K status */
#define LCM_C32K	(*((volatile Word *) LCM_p2v (_LCM_C32K)))

/* Interrupt controller */
#define LCM_ICR		(*((volatile Word16 *) LCM_p2v (_LCM_ICR)))

/* MCS decoder for boot selecting */
#define LCM_MCSX0	(*((volatile Word16 *) LCM_p2v (_LCM_MCSX0)))
#define LCM_MCSX1	(*((volatile Word16 *) LCM_p2v (_LCM_MCSX1)))
#define LCM_MCSX2	(*((volatile Word16 *) LCM_p2v (_LCM_MCSX2)))
#define LCM_MCSX3	(*((volatile Word16 *) LCM_p2v (_LCM_MCSX3)))

/* Touch panel controller */
#define LCM_ASD		(*((volatile Word16 *) LCM_p2v (_LCM_ASD)))
#define LCM_HSD		(*((volatile Word16 *) LCM_p2v (_LCM_HSD)))
#define LCM_HSC		(*((volatile Word16 *) LCM_p2v (_LCM_HSC)))
#define LCM_TADC	(*((volatile Word16 *) LCM_p2v (_LCM_TADC)))

/* TFT signal */
#define LCM_TC		(*((volatile Word16 *) LCM_p2v (_LCM_TC)))
#define LCM_CPSD	(*((volatile Word16 *) LCM_p2v (_LCM_CPSD)))

/* Key controller */
#define LCM_KIB		(*((volatile Word16 *) LCM_p2v (_LCM_KIB)))
#define LCM_KSC		(*((volatile Word16 *) LCM_p2v (_LCM_KSC)))
#define LCM_KCMD	(*((volatile Word16 *) LCM_p2v (_LCM_KCMD)))
#define LCM_KIC		(*((volatile Word16 *) LCM_p2v (_LCM_KIC)))

/* Audio clock */
#define LCM_ACC		(*((volatile Word16 *) LCM_p2v (_LCM_ACC)))

/* SPI interface */
#define LCM_SPIMD	(*((volatile Word16 *) LCM_p2v (_LCM_SPIMD)))
#define LCM_SPICT	(*((volatile Word16 *) LCM_p2v (_LCM_SPICT)))
#define LCM_SPIST	(*((volatile Word16 *) LCM_p2v (_LCM_SPIST)))
#define LCM_SPIIS	(*((volatile Word16 *) LCM_p2v (_LCM_SPIIS)))
#define LCM_SPIWE	(*((volatile Word16 *) LCM_p2v (_LCM_SPIWE)))
#define LCM_SPIIE	(*((volatile Word16 *) LCM_p2v (_LCM_SPIIE)))
#define LCM_SPIIR	(*((volatile Word16 *) LCM_p2v (_LCM_SPIIR)))
#define LCM_SPITD	(*((volatile Word16 *) LCM_p2v (_LCM_SPITD)))
#define LCM_SPIRD	(*((volatile Word16 *) LCM_p2v (_LCM_SPIRD)))
#define LCM_SPITS	(*((volatile Word16 *) LCM_p2v (_LCM_SPITS)))
#define LCM_SPIRS	(*((volatile Word16 *) LCM_p2v (_LCM_SPIRS)))

/* GPIO */
#define LCM_GPD		(*((volatile Word16 *) LCM_p2v (_LCM_GPD)))
#define LCM_GPE		(*((volatile Word16 *) LCM_p2v (_LCM_GPE)))
#define LCM_GPL		(*((volatile Word16 *) LCM_p2v (_LCM_GPL)))
#define LCM_GPO		(*((volatile Word16 *) LCM_p2v (_LCM_GPO)))
#define LCM_GRIE	(*((volatile Word16 *) LCM_p2v (_LCM_GRIE)))
#define LCM_GFIE	(*((volatile Word16 *) LCM_p2v (_LCM_GFIE)))
#define LCM_GIS		(*((volatile Word16 *) LCM_p2v (_LCM_GIS)))
#define LCM_GWE		(*((volatile Word16 *) LCM_p2v (_LCM_GWE)))
#define LCM_GIE		(*((volatile Word16 *) LCM_p2v (_LCM_GIE)))
#define LCM_GIR		(*((volatile Word16 *) LCM_p2v (_LCM_GIR)))

/* Front light adjustment controller */
#define LCM_ALS		(*((volatile Word16 *) LCM_p2v (_LCM_ALS)))
#define LCM_ALD		(*((volatile Word16 *) LCM_p2v (_LCM_ALD)))

/* PCM audio interface */
#define LCM_PAIF	(*((volatile Word16 *) LCM_p2v (_LCM_PAIF)))

/* Long time timer */
#define LCM_LTC		(*((volatile Word16 *) LCM_p2v (_LCM_LTC)))
#define LCM_LTINT	(*((volatile Word16 *) LCM_p2v (_LCM_LTINT)))

/* DAC control signal */
#define LCM_DAC		(*((volatile Word16 *) LCM_p2v (_LCM_DAC)))

/* DAC control */
#define	LCM_DAC_SCLOEB	0x08	/* SCL pin output data       */
#define	LCM_DAC_TEST	0x04	/* Test bit                  */
#define	LCM_DAC_SDA	0x02	/* SDA pin level (read-only) */
#define	LCM_DAC_SDAOEB	0x01	/* SDA pin output data       */

/* LED controller */
#define LCM_LPT0	(*((volatile Word16 *) LCM_p2v (_LCM_LPT0)))
#define LCM_LPT1	(*((volatile Word16 *) LCM_p2v (_LCM_LPT1)))

#define LCM_LPT_TOFH		0x80			/* */
#define LCM_LPT_TOFL		0x08			/* */
#define LCM_LPT_TOH(TOH)	((TOH & 0x7) << 4)	/* */
#define LCM_LPT_TOL(TOL)	((TOL & 0x7))		/* */

/* Audio clock */
#define	LCM_ACC_XON		0x80	/*  */
#define	LCM_ACC_XEN		0x40	/*  */
#define	LCM_ACC_XSEL0		0x00	/*  */
#define	LCM_ACC_XSEL1		0x20	/*  */
#define	LCM_ACC_MCLKEN		0x10	/*  */
#define	LCM_ACC_64FSEN		0x08	/*  */
#define	LCM_ACC_CLKSEL000	0x00	/* mclk  2 */
#define	LCM_ACC_CLKSEL001	0x01	/* mclk  3 */
#define	LCM_ACC_CLKSEL010	0x02	/* mclk  4 */
#define	LCM_ACC_CLKSEL011	0x03	/* mclk  6 */
#define	LCM_ACC_CLKSEL100	0x04	/* mclk  8 */
#define	LCM_ACC_CLKSEL101	0x05	/* mclk 12 */

/* PCM audio interface */
#define	LCM_PAIF_SCINV		0x20	/*  */
#define	LCM_PAIF_SCEN		0x10	/*  */
#define	LCM_PAIF_LRCRST		0x08	/*  */
#define	LCM_PAIF_LRCEVE		0x04	/*  */
#define	LCM_PAIF_LRCINV		0x02	/*  */
#define	LCM_PAIF_LRCEN		0x01	/*  */

/* GPIO */
#define	LCM_GPIO(Nb)		(0x01 << (Nb))	/* LoCoMo GPIO [0...15] */
				
#define LCM_GPIO_DAC_ON		LCM_GPIO (8)	/* LoCoMo GPIO  [8] */
#define LCM_GPIO_DAC_SDATA	LCM_GPIO (10)	/* LoCoMo GPIO [10] */
#define LCM_GPIO_DAC_SCK	LCM_GPIO (11)	/* LoCoMo GPIO [11] */
#define LCM_GPIO_DAC_SLOAD	LCM_GPIO (12)	/* LoCoMo GPIO [12] */

#elif LANGUAGE == Assembly

/* LCM version */
#define LCM_VER		( LCM_p2v (_LCM_VER))

/* Pin status */
#define LCM_ST		( LCM_p2v (_LCM_ST))

/* Interrupt controller */
#define LCM_ICR		( LCM_p2v (_LCM_ICR))

/* MCS decoder for boot selecting */
#define LCM_MCSX0	( LCM_p2v (_LCM_MCSX0))
#define LCM_MCSX1	( LCM_p2v (_LCM_MCSX1))
#define LCM_MCSX2	( LCM_p2v (_LCM_MCSX2))
#define LCM_MCSX3	( LCM_p2v (_LCM_MCSX3))

/* Touch panel controller */
#define LCM_ASD		( LCM_p2v (_LCM_ASD))
#define LCM_HSD		( LCM_p2v (_LCM_HSD))
#define LCM_HSC		( LCM_p2v (_LCM_HSC))
#define LCM_TADC	( LCM_p2v (_LCM_TADC))

/* TFT signal */
#define LCM_TC		( LCM_p2v (_LCM_TC))
#define LCM_CPSD	( LCM_p2v (_LCM_CPSD))

/* Key controller */
#define LCM_KIB		( LCM_p2v (_LCM_KIB))
#define LCM_KSC		( LCM_p2v (_LCM_KSC))
#define LCM_KCMD	( LCM_p2v (_LCM_KCMD))
#define LCM_KIC		( LCM_p2v (_LCM_KIC))

/* Audio clock */
#define LCM_ACC		( LCM_p2v (_LCM_ACC))

/* SPI interface */
#define LCM_SPIMD	( LCM_p2v (_LCM_SPIMD))
#define LCM_SPICT	( LCM_p2v (_LCM_SPICT))
#define LCM_SPIST	( LCM_p2v (_LCM_SPIST))
#define LCM_SPIIS	( LCM_p2v (_LCM_SPIIS))
#define LCM_SPIWE	( LCM_p2v (_LCM_SPIWE))
#define LCM_SPIIE	( LCM_p2v (_LCM_SPIIE))
#define LCM_SPIIR	( LCM_p2v (_LCM_SPIIR))
#define LCM_SPITD	( LCM_p2v (_LCM_SPITD))
#define LCM_SPIRD	( LCM_p2v (_LCM_SPIRD))
#define LCM_SPITS	( LCM_p2v (_LCM_SPITS))
#define LCM_SPIRS	( LCM_p2v (_LCM_SPIRS))

/* GPIO */
#define LCM_GPD		( LCM_p2v (_LCM_GPD))
#define LCM_GPE		( LCM_p2v (_LCM_GPE))
#define LCM_GPL		( LCM_p2v (_LCM_GPL))
#define LCM_GPO		( LCM_p2v (_LCM_GPO))
#define LCM_GRIE	( LCM_p2v (_LCM_GRIE))
#define LCM_GFIE	( LCM_p2v (_LCM_GFIE))
#define LCM_GIS		( LCM_p2v (_LCM_GIS))
#define LCM_GWE		( LCM_p2v (_LCM_GWE))
#define LCM_GIE		( LCM_p2v (_LCM_GIE))
#define LCM_GIR		( LCM_p2v (_LCM_GIR))

/* Front light adjustment controller */
#define LCM_ALS		( LCM_p2v (_LCM_ALS))
#define LCM_ALD		( LCM_p2v (_LCM_ALD))

/* PCM audio interface */
#define LCM_PAIF	( LCM_p2v (_LCM_PAIF))

/* Long time timer */
#define LCM_LTC		( LCM_p2v (_LCM_LTC))
#define LCM_LTINT	( LCM_p2v (_LCM_LTINT))

/* DAC control signal for LCD (COMADJ ) */
#define LCM_DAC		( LCM_p2v (_LCM_DAC))

/* LED controller */
#define LCM_LPT0	( LCM_p2v (_LCM_LPT0))
#define LCM_LPT1	( LCM_p2v (_LCM_LPT1))

#endif  /* LANGUAGE == C */

#define GPIO_ON_KEY		GPIO_GPIO (0)
#define GPIO_AC_IN		GPIO_GPIO (1)
#define GPIO_CF_IRQ		GPIO_GPIO (14)
#define GPIO_nREMOCON_INT	GPIO_GPIO (15)
#define GPIO_UCB1200_RESET	GPIO_GPIO (16)	/* UCB RESET */
#define GPIO_CO			GPIO_GPIO (20)
#define GPIO_MCP_CLK		GPIO_GPIO (21)
#define GPIO_CF_CD		GPIO_GPIO (22)
#define GPIO_UCB1200_IRQ	GPIO_GPIO (23)	/* UCB GPIO and touchscreen */
#define GPIO_WAKEUP             GPIO_GPIO (24)
#define GPIO_GA_INT             GPIO_GPIO (25)
#define GPIO_MAIN_BAT_LOW       GPIO_GPIO (26)

/*
 * Interrupts
 */
/* SA1110 GPIOs */
#define IRQ_GPIO_ON_KEY		IRQ_GPIO0
#define IRQ_GPIO_AC_IN		IRQ_GPIO1
#define IRQ_GPIO_CF_IRQ		IRQ_GPIO14
#define IRQ_GPIO_nREMOCON_INT	IRQ_GPIO15
#define IRQ_GPIO_CO		IRQ_GPIO20
#define IRQ_GPIO_CF_CD		IRQ_GPIO22
#define IRQ_GPIO_UCB1200_IRQ	IRQ_GPIO23
#define IRQ_GPIO_WAKEUP		IRQ_GPIO24
#define IRQ_GPIO_GA_INT		IRQ_GPIO25
#define IRQ_GPIO_MAIN_BAT_LOW	IRQ_GPIO26

/* GA GPIOs */
#define IRQ_LCM_GPIO_CTS	LCM_IRQ_GPIO1
#define IRQ_LCM_GPIO_DSR	LCM_IRQ_GPIO2
#define IRQ_LCM_GPIO_nSD_DETECT	LCM_IRQ_GPIO13
#define IRQ_LCM_GPIO_nSD_WP	LCM_IRQ_GPIO14



/*
 * SCOOP internal I/O mappings
 *
 */


#ifdef CONFIG_COLLIE_TS
#define CF_BUF_CTRL_BASE 0xF3000000
#define CF_BUF_CTRL (*((volatile unsigned long*)(CF_BUF_CTRL_BASE)))

#define	CF_BUS_RESET		(1 << 2)
#define	CF_BUS_POWER_OFF	0x03
#define	CF_BUS_POWER_50V	0x02	/* bit0 : 0 / bit1 : 1 */
#define	CF_BUS_POWER_33V	0x01	/* bit0 : 1 / bit1 : 0 */

static int  cf_buf_ctrl = 0;

#else	/* ! CONFIG_COLLIE_TS */
#define CF_BUF_CTRL_BASE 0xF0800000
#define	SCP_REG(adr) (*(volatile unsigned short*)(CF_BUF_CTRL_BASE+(adr)))

#define	SCP_MCR  0x00
#define	SCP_CDR  0x04
#define	SCP_CSR  0x08
#define	SCP_CPR  0x0C
#define	SCP_CCR  0x10
#define	SCP_IRR  0x14
#define	SCP_IRM  0x14
#define	SCP_IMR  0x18
#define	SCP_ISR  0x1C
#define	SCP_GPCR 0x20
#define	SCP_GPWR 0x24
#define	SCP_GPRR 0x28
#define	SCP_REG_MCR	SCP_REG(SCP_MCR)
#define	SCP_REG_CDR	SCP_REG(SCP_CDR)
#define	SCP_REG_CSR	SCP_REG(SCP_CSR)
#define	SCP_REG_CPR	SCP_REG(SCP_CPR)
#define	SCP_REG_CCR	SCP_REG(SCP_CCR)
#define	SCP_REG_IRR	SCP_REG(SCP_IRR)
#define	SCP_REG_IRM	SCP_REG(SCP_IRM)
#define	SCP_REG_IMR	SCP_REG(SCP_IMR)
#define	SCP_REG_ISR	SCP_REG(SCP_ISR)
#define	SCP_REG_GPCR	SCP_REG(SCP_GPCR)
#define	SCP_REG_GPWR	SCP_REG(SCP_GPWR)
#define	SCP_REG_GPRR	SCP_REG(SCP_GPRR)

#define SCP_GPCR_PA19	( 1 << 9 )
#define SCP_GPCR_PA18	( 1 << 8 )
#define SCP_GPCR_PA17	( 1 << 7 )
#define SCP_GPCR_PA16	( 1 << 6 )
#define SCP_GPCR_PA15	( 1 << 5 )
#define SCP_GPCR_PA14	( 1 << 4 )
#define SCP_GPCR_PA13	( 1 << 3 )
#define SCP_GPCR_PA12	( 1 << 2 )
#define SCP_GPCR_PA11	( 1 << 1 )

#define SCP_CHARGE_ON	SCP_GPCR_PA11
#define SCP_DIAG_BOOT1	SCP_GPCR_PA12
#define SCP_DIAG_BOOT2	SCP_GPCR_PA13
#define SCP_MUTE_L	SCP_GPCR_PA14
#define SCP_MUTE_R	SCP_GPCR_PA15
#define SCP_5VON	SCP_GPCR_PA16
#define SCP_AMP_ON	SCP_GPCR_PA17
#define SCP_VPEN	SCP_GPCR_PA18
#define SCP_LB_VOL_CHG	SCP_GPCR_PA19

#define SCP_IO_DIR	( SCP_CHARGE_ON | SCP_MUTE_L | SCP_MUTE_R | \
			  SCP_5VON | SCP_AMP_ON | SCP_VPEN | \
			  SCP_LB_VOL_CHG )
#define SCP_IO_OUT	( SCP_MUTE_L | SCP_MUTE_R )

#endif


/*
 * Flash Memory mappings
 *
 */

#define FLASH_MEM_BASE 0xe8ffc000
#define	FLASH_DATA(adr) (*(volatile unsigned int*)(FLASH_MEM_BASE+(adr)))
#define	FLASH_DATA_F(adr) (*(volatile float32 *)(FLASH_MEM_BASE+(adr)))
#define FLASH_MAGIC_CHG(a,b,c,d) ( ( d << 24 ) | ( c << 16 )  | ( b << 8 ) | a )


// COMADJ
#define FLASH_COMADJ_MAJIC	FLASH_MAGIC_CHG('C','M','A','D')
#define	FLASH_COMADJ_MAGIC_ADR	0x00
#define	FLASH_COMADJ_DATA_ADR	0x04

// TOUCH PANEL
#define FLASH_TOUCH_MAJIC	FLASH_MAGIC_CHG('T','U','C','H')
#define	FLASH_TOUCH_MAGIC_ADR	0x1C
#define	FLASH_TOUCH_XP_DATA_ADR	0x20
#define	FLASH_TOUCH_YP_DATA_ADR	0x24
#define	FLASH_TOUCH_XD_DATA_ADR	0x28
#define	FLASH_TOUCH_YD_DATA_ADR	0x2C

// AD
#define FLASH_AD_MAJIC	FLASH_MAGIC_CHG('B','V','A','D')
#define	FLASH_AD_MAGIC_ADR	0x30
#define	FLASH_AD_DATA_ADR	0x34



#endif /* __ASM_ARCH_COLLIE_H  */
