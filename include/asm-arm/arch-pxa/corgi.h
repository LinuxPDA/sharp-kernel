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
#define GPIO_CO			16


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
#define GPIO_LED_ORANGE		(13)
//#define SCP_LED_GREEN		SCP_GPCR_PA11


/*
 * GPIOs
 */
/* PXA GPIOs */
#define GPIO_KEY_INT		(0)	/* key interrupt */
#define GPIO_AC_IN		(1)
#define GPIO_TP_INT		(5)	/* Touch Panel interrupt */
#define GPIO_WAKEUP             (3)
#define GPIO_IR_ON		(22)
#define GPIO_AK_INT		(4)	// Remote Controller
#define GPIO_HP_IN		GPIO_AK_INT
#define GPIO_CF_IRQ		(17)
//#define GPIO_CF_PRDY		(17)
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
#define GPIO_USB_PULLUP		(45)
#define GPIO_HSYNC			(44)

/* KeyBoard */
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


/*
 * Interrupts
 */
/* PXA GPIOs */
#define IRQ_GPIO_KEY_INT	IRQ_GPIO(0)
#define IRQ_GPIO_AC_IN		IRQ_GPIO(1)
#define IRQ_GPIO_AK_INT		IRQ_GPIO(4)
#define IRQ_GPIO_HP_IN		IRQ_GPIO_AK_INT
#define IRQ_GPIO_TP_INT		IRQ_GPIO(5)
#define IRQ_GPIO_WAKEUP		IRQ_GPIO(3)
#define IRQ_GPIO_CO		IRQ_GPIO(16)
#define IRQ_GPIO_CF_IRQ		IRQ_GPIO(17)
#define IRQ_GPIO_CF_CD		IRQ_GPIO(14)
#define IRQ_GPIO_nSD_INT	IRQ_GPIO(10)
#define IRQ_GPIO_nSD_DETECT	IRQ_GPIO(9)
#define IRQ_GPIO_MAIN_BAT_LOW	IRQ_GPIO(11)
#define IRQ_GPIO_KEY_SENSE(a)	IRQ_GPIO(58+(a))


// CS
#define CS_MAX1111	1
#define CS_ADS7846	2
#define CS_LZ9JG18	3

#endif /* __ASM_ARCH_CORGI_H  */

