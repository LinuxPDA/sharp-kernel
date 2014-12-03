/*
 * linux/include/asm-arm/arch-pxa/corgi.h
 * 
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *
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
 *   24-Sep-2004 Lineo Solutions, Inc.  for Spitz
 */
#ifndef __ASM_ARCH_CORGI_H
#define __ASM_ARCH_CORGI_H  1

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

/*
 * LCDC internal I/O mappings
 *
 * We have the following mapping:
 *      phys            virt
 *      08000000        f1000000
 */



/*
 * SCOOP internal I/O mappings
 *
 * We have the following mapping:
 *      phys            virt
 *      10800000        f2000000
 */


#define CF_BUF_CTRL_BASE 0xF2000000
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

#define SCP_GPCR_PA22	( 1 << 12 )
#define SCP_GPCR_PA21	( 1 << 11 )
#define SCP_GPCR_PA20	( 1 << 10 )
#define SCP_GPCR_PA19	( 1 << 9 )
#define SCP_GPCR_PA18	( 1 << 8 )
#define SCP_GPCR_PA17	( 1 << 7 )
#define SCP_GPCR_PA16	( 1 << 6 )
#define SCP_GPCR_PA15	( 1 << 5 )
#define SCP_GPCR_PA14	( 1 << 4 )
#define SCP_GPCR_PA13	( 1 << 3 )
#define SCP_GPCR_PA12	( 1 << 2 )
#define SCP_GPCR_PA11	( 1 << 1 )


/*
 * GPIOs
 */

#if defined(CONFIG_ARCH_PXA_SPITZ)

#define SCP_LED_GREEN		SCP_GPCR_PA11
#define SCP_JK_B		SCP_GPCR_PA12
#define SCP_CHRG_ON		SCP_GPCR_PA13
#define SCP_MUTE_L		SCP_GPCR_PA14
#define SCP_MUTE_R		SCP_GPCR_PA15
#define	SCP_CF_POWER		SCP_GPCR_PA16
#define SCP_LED_ORANGE		SCP_GPCR_PA17
#define SCP_JK_A		SCP_GPCR_PA18
#define SCP_ADC_TEMP_ON		SCP_GPCR_PA19

#define SCP_IO_DIR	( SCP_LED_GREEN | SCP_JK_B | SCP_CHRG_ON | \
			  SCP_MUTE_L | SCP_MUTE_R | SCP_LED_ORANGE | \
			  SCP_CF_POWER | SCP_JK_A | SCP_ADC_TEMP_ON )
#define SCP_IO_OUT	( SCP_CHRG_ON | SCP_MUTE_L | SCP_MUTE_R )

#else

#define SCP_LED_GREEN		SCP_GPCR_PA11
#define SCP_SWA			SCP_GPCR_PA12
#define SCP_SWB			SCP_GPCR_PA13
#define SCP_MUTE_L		SCP_GPCR_PA14
#define SCP_MUTE_R		SCP_GPCR_PA15
#define SCP_AKIN_PULLUP		SCP_GPCR_PA16
#define SCP_APM_ON		SCP_GPCR_PA17
#define SCP_BACKLIGHT_CONT	SCP_GPCR_PA18
#define SCP_MIC_BIAS		SCP_GPCR_PA19

#define SCP_IO_DIR	( SCP_LED_GREEN | SCP_MUTE_L | SCP_MUTE_R | \
			  SCP_AKIN_PULLUP | SCP_APM_ON | SCP_BACKLIGHT_CONT | \
			  SCP_MIC_BIAS )
#define SCP_IO_OUT	( SCP_MUTE_L | SCP_MUTE_R )

#endif


/*
 * SCOOP2 I/O mappings
 *
 * We have the following mapping:
 *      phys            virt
 *      14800000        f2200000
 */

#define CF2_BUF_CTRL_BASE 0xF2200040
#define SCP2_REG(adr) (*(volatile unsigned short*)(CF2_BUF_CTRL_BASE+(adr)))
	
#define SCP2_REG_MCR  SCP2_REG(SCP_MCR)
#define SCP2_REG_CDR  SCP2_REG(SCP_CDR)
#define SCP2_REG_CSR  SCP2_REG(SCP_CSR)
#define SCP2_REG_CPR  SCP2_REG(SCP_CPR)
#define SCP2_REG_CCR  SCP2_REG(SCP_CCR)
#define SCP2_REG_IRR  SCP2_REG(SCP_IRR)
#define SCP2_REG_IRM  SCP2_REG(SCP_IRM)
#define SCP2_REG_IMR  SCP2_REG(SCP_IMR)
#define SCP2_REG_ISR  SCP2_REG(SCP_ISR)
#define SCP2_REG_GPCR SCP2_REG(SCP_GPCR)
#define SCP2_REG_GPWR SCP2_REG(SCP_GPWR)
#define SCP2_REG_GPRR SCP2_REG(SCP_GPRR)

/*
 * SCOOP2 GPIOs
 */
#define SCP2_IR_ON		SCP_GPCR_PA11
#define SCP2_AKIN_PULLUP	SCP_GPCR_PA12
#define SCP2_RESERVED_1		SCP_GPCR_PA13
#define SCP2_RESERVED_2		SCP_GPCR_PA14
#define SCP2_RESERVED_3		SCP_GPCR_PA15
#define SCP2_RESERVED_4		SCP_GPCR_PA16
#define SCP2_BACKLIGHT_CONT	SCP_GPCR_PA17
#define SCP2_BACKLIGHT_ON	SCP_GPCR_PA18
#define SCP2_MIC_BIAS		SCP_GPCR_PA19

/* GPIO Direction   1 : outpu mode / 0:input mode */
#define SCP2_IO_DIR	( SCP2_IR_ON | SCP2_AKIN_PULLUP | \
			  SCP2_RESERVED_1 | SCP2_RESERVED_2 | \
			  SCP2_RESERVED_3 | SCP2_RESERVED_4 | \
			  SCP2_BACKLIGHT_CONT | SCP2_BACKLIGHT_ON | \
			  SCP2_MIC_BIAS )
/* GPIO out put level when init   1: Hi */
//#define SCP2_IO_OUT  ( SCP2_TC6393_SUSPEND | SCP2_TC3693_L3V_ON )
#define SCP2_IO_OUT	( SCP2_IR_ON | SCP2_AKIN_PULLUP | \
                          SCP2_RESERVED_1 )


/*
 * Flash Memory mappings
 *
 * We have the following mapping:
 *                      phys            virt
 *      boot ROM        00000000        ef000000
 *      NAND Flash      0C000000	f2100000
 */
#define NAND_FLASH_REG_BASE 0xf2100000
#define CPLD_REG(ofst) (*(volatile unsigned char*)(NAND_FLASH_REG_BASE+(ofst)))

/* register offset */
#define ECCLPLB		0x00	/* line parity 7 - 0 bit */
#define ECCLPUB		0x04	/* line parity 15 - 8 bit */
#define ECCCP		0x08	/* column parity 5 - 0 bit */
#define ECCCNTR		0x0C	/* ECC byte counter */
#define ECCCLRR		0x10	/* cleare ECC */
#define FLASHIO		0x14	/* Flash I/O */
#define FLASHCTL	0x18	/* Flash Control */

/* Flash control bit */
#define FLRYBY		(1 << 5)
#define FLCE1		(1 << 4)
#define FLWP		(1 << 3)
#define FLALE		(1 << 2)
#define FLCLE		(1 << 1)
#define FLCE0		(1 << 0)




/*
 * LED
 */
//#define GPIO_LED_ORANGE		(13)
//#define SCP_LED_GREEN		SCP_GPCR_PA11


/*
 * GPIOs
 */
/* PXA GPIOs */

#if defined(CONFIG_ARCH_PXA_SPITZ)

#define GPIO_KEY_INT		(0)	/* key interrupt */
#define GPIO_RESET		(1)
#if defined(CONFIG_SPITZ_TR0)
#define GPIO_AC_IN		(1)
#else
#define GPIO_AC_IN		(115)
#endif
#define GPIO_TP_INT		(11)	/* Touch Panel interrupt */
#define GPIO_SYNC            	(16)
#define GPIO_WAKEUP            	GPIO_SYNC
#define GPIO_ON_KEY		(95)
#define GPIO_SWA		(97)
#define GPIO_SWB		(96)
#define GPIO_AK_INT		(13)	// Remote Controller
#define GPIO_HP_IN		(116)
#define GPIO_CF_IRQ		(105)
//#define GPIO_CF_PRDY		(105)
#define GPIO_CF2_IRQ		(106)	/* CF slot1 Ready */
#define GPIO_CF_CD		(94)
#define GPIO_CF2_CD		(93)
//#define GPIO_SD_PWR		(33)
#define GPIO_nSD_CLK            (32)
#define GPIO_nSD_WP		(81)
////#define GPIO_nSD_INT		(10)
#define GPIO_nSD_DETECT		(9)
#define GPIO_MAIN_BAT_LOW       (90)
#define GPIO_BAT_COVER		(90)
#define GPIO_CHRG_FULL		(101)
#define GPIO_CO			(101)
////#define GPIO_USB_PULLUP		(45)
#define GPIO_HSYNC		(22)
#define	GPIO_nPCE		(54)
#define GPIO_ON_RESET		(89)
#define GPIO_FATAL_BAT          (21)
#else

#define GPIO_KEY_INT		(0)	/* key interrupt */
#define GPIO_AC_IN		(1)
#define GPIO_TP_INT		(5)	/* Touch Panel interrupt */
#define GPIO_WAKEUP             (3)
#define GPIO_IR_ON		(22)
#define GPIO_AK_INT		(4)	// Remote Controller
#define GPIO_HP_IN		GPIO_AK_INT
#define GPIO_CF_IRQ		(17)
//#define GPIO_CF_PRDY		(17)
#define GPIO_CF2_IRQ		(36)	/* CF slot1 Ready */
#define GPIO_LED_ORANGE		(13)
#define GPIO_CF_CD		(14)
#define GPIO_SD_PWR		(33)
#define GPIO_nSD_CLK            (6)
#define GPIO_nSD_WP		(7)
#define GPIO_nSD_INT		(10)
#define GPIO_nSD_DETECT		(9)
#define GPIO_MAIN_BAT_LOW       (11)
#define GPIO_BAT_COVER		(11)
#define GPIO_ADC_TEMP_ON	(21)
#define GPIO_CHRG_ON		(38)
#define GPIO_CHRG_FULL		(16)
#define GPIO_CO			(16)
#define GPIO_USB_PULLUP		(45)
#define GPIO_HSYNC			(44)
#define	GPIO_nPCE		GPIO53_nPCE_2

#endif


/* KeyBoard */
#if defined(CONFIG_ARCH_PXA_SPITZ)

#define KEY_STROBE_NUM	(12)
#define KEY_SENSE_NUM	(8)
#define	GPIO_G0_STROBE_BIT		0x0f800000
#define	GPIO_G1_STROBE_BIT		0x00100000
#define	GPIO_G2_STROBE_BIT		0x01000000
#define	GPIO_G3_STROBE_BIT		0x00041880
#define	GPIO_G0_SENSE_BIT		0x00021000
#define	GPIO_G1_SENSE_BIT		0x000000d4
#define	GPIO_G2_SENSE_BIT		0x08000000
#define	GPIO_G3_SENSE_BIT		0x00000000

#define	GPIO_KEY_STROBE0		88
#define	GPIO_KEY_STROBE1		23
#define	GPIO_KEY_STROBE2		24
#define	GPIO_KEY_STROBE3		25
#define	GPIO_KEY_STROBE4		26
#define	GPIO_KEY_STROBE5		27
#define	GPIO_KEY_STROBE6		52
#define	GPIO_KEY_STROBE7		103
#define	GPIO_KEY_STROBE8		107
#define	GPIO_KEY_STROBE9		-1
#define	GPIO_KEY_STROBE10       	108
#define	GPIO_KEY_STROBE11       	114

#define	GPIO_KEY_SENSE0			12
#define	GPIO_KEY_SENSE1			17
#define	GPIO_KEY_SENSE2			91
#define	GPIO_KEY_SENSE3			34
#define	GPIO_KEY_SENSE4			36
#define	GPIO_KEY_SENSE5			38
#define	GPIO_KEY_SENSE6			39
#define	GPIO_KEY_SENSE7			-1

#else

#define KEY_STROBE_NUM	(12)
#define KEY_SENSE_NUM	(8)
#define GPIO_ALL_STROBE_BIT		(0x00003ffc)
#define GPIO_HIGH_SENSE_BIT		(0xfc000000)
#define GPIO_HIGH_SENSE_RSHIFT	(26)
#define GPIO_LOW_SENSE_BIT		(0x00000003)
#define GPIO_LOW_SENSE_LSHIFT	(6)
#define GPIO_STROBE_BIT(a)		GPIO_bit(66+(a))
#define GPIO_SENSE_BIT(a)		GPIO_bit(58+(a))
#define GAFR_ALL_STROBE_BIT		(0x0ffffff0)
#define GAFR_HIGH_SENSE_BIT		(0xfff00000)
#define GAFR_LOW_SENSE_BIT		(0x0000000f)
#define GPIO_KEY_SENSE(a)		(58+(a))

#endif


/*
 * Interrupts
 */
/* PXA GPIOs */
#define IRQ_GPIO_KEY_INT	IRQ_GPIO(GPIO_KEY_INT)
#define IRQ_GPIO_AC_IN		IRQ_GPIO(GPIO_AC_IN)
#define IRQ_GPIO_AK_INT		IRQ_GPIO(GPIO_AK_INT)
#define IRQ_GPIO_HP_IN		IRQ_GPIO(GPIO_HP_IN)
#define IRQ_GPIO_TP_INT		IRQ_GPIO(GPIO_TP_INT)
#if defined(CONFIG_ARCH_PXA_SPITZ)
#define IRQ_GPIO_SYNC		IRQ_GPIO(GPIO_SYNC)
#define IRQ_GPIO_ON_KEY		IRQ_GPIO(GPIO_ON_KEY)
#define IRQ_GPIO_SWA		IRQ_GPIO(GPIO_SWA)
#define IRQ_GPIO_SWB		IRQ_GPIO(GPIO_SWB)
#define IRQ_GPIO_BAT_COVER	IRQ_GPIO(GPIO_BAT_COVER)
#define IRQ_GPIO_FATAL_BAT	IRQ_GPIO(GPIO_FATAL_BAT)
#else
#define IRQ_GPIO_WAKEUP		IRQ_GPIO(GPIO_WAKEUP)
#endif
#define IRQ_GPIO_CO		IRQ_GPIO(GPIO_CO)
#define IRQ_GPIO_CF_IRQ		IRQ_GPIO(GPIO_CF_IRQ)
#define IRQ_GPIO_CF_CD		IRQ_GPIO(GPIO_CF_CD)
#define IRQ_GPIO_CF2_IRQ	IRQ_GPIO(GPIO_CF2_IRQ)
#define IRQ_GPIO_nSD_INT	IRQ_GPIO(GPIO_nSD_INT)
#define IRQ_GPIO_nSD_DETECT	IRQ_GPIO(GPIO_nSD_DETECT)
#define IRQ_GPIO_MAIN_BAT_LOW	IRQ_GPIO(GPIO_MAIN_BAT_LOW)
#define IRQ_GPIO_KEY_SENSE(a)	IRQ_GPIO(58+(a))


// CS
#define CS_MAX1111	1
#define CS_ADS7846	2
#define CS_LZ9JG18	3


#if defined(CONFIG_ARCH_PXA_SPITZ)
#define	AC_IN_STATUS	((~GPLR(GPIO_AC_IN)) & GPIO_bit(GPIO_AC_IN))
#else
#define	AC_IN_STATUS	(GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN))
#endif

#if defined(CONFIG_ARCH_PXA_SPITZ)
#define LOGICAL_WAKEUP_SRC
#define USE_HDD_LED
#define SPITZ_MCIO1_WAIT_SETTING	0x0001C60A
#endif

#endif /* __ASM_ARCH_CORGI_H  */

