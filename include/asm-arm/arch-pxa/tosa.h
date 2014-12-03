/*
 * include/asm-arm/arch-pxa/tosa.h
 * 	Copyright (C) 2003 Lineo uSolutions, Inc.
 *
 * ChangeLog:
 *   23-Oct-2003 SHARP Corporation
 */

#ifndef _ASM_ARCH_TOSA_H_
#define _ASM_ARCH_TOSA_H_	1

/*  TOSA Chip selects  */
#define TOSA_LCDC_PHYS		PXA_CS4_PHYS
/* Internel Scoop */
#define TOSA_CF_PHYS		(PXA_CS2_PHYS + 0x00800000)
/* Jacket Scoop */
#define TOSA_SCOOP_PHYS 	(PXA_CS5_PHYS + 0x00800000)

/*
 * TC6393 internal I/O mappings
 * 
 * We have the following mapping:
 *	phys		virt
 *	10000000	f1000000
 */
#define TC6393_SYS_BASE		0xf1000000
#define TC6393_NAND_BASE	(TC6393_SYS_BASE + 0x000100)
#define TC6393_SD_BASE		(TC6393_SYS_BASE + 0x000200)
#define TC6393_USB_BASE		(TC6393_SYS_BASE + 0x000300)
#define TC6393_SERIAL_BASE	(TC6393_SYS_BASE + 0x000400)
#define TC6393_GC_BASE		(TC6393_SYS_BASE + 0x000500)
#define TC6393_RAM0_BASE	(TC6393_SYS_BASE + 0x010000)
#define TC6393_RAM0_SIZE	(32*1024)
#define TC6393_RAM1_BASE	(TC6393_SYS_BASE + 0x100000)
#define TC6393_RAM1_SIZE	(64 * 1024 * 16)

/* 
 * Internal Local Memory use purpose
 *   RAM0 is used for USB
 *   RAM1 is used for GC
 */
/* Internal register mapping */
#define TC6393_GC_INTERNAL_REG_BASE	0x000600	/* Length 0x200 */
#define TC6393_USB_OHCI_OP_REG_BASE	0x000A00	/* Length 0x100 */
#define TC6393_NAND_FLASH_CTL_REG_BASE	0x001000	/* Length 0x8 */

	
/* System Configuration register */
#define TC6393_SYS_REG(ofst) (*(volatile unsigned short*)(TC6393_SYS_BASE+(ofst)))
#define TC6393_SYS_RIDR		0x008
#define TC6393_SYS_ISR		0x050
#define TC6393_SYS_IMR		0x052
#define TC6393_SYS_IRR		0x054
#define TC6393_SYS_GPER		0x060
#define TC6393_SYS_GPISR1	0x064
#define TC6393_SYS_GPISR2	0x066
#define TC6393_SYS_GPIIMR1	0x068
#define TC6393_SYS_GPIIMR2	0x06A
#define TC6393_SYS_GPIEDER1	0x06C
#define TC6393_SYS_GPIEDER2	0x06E
#define TC6393_SYS_GPILIR1	0x070
#define TC6393_SYS_GPILIR2	0x072
#define TC6393_SYS_GPODSR1	0x078
#define TC6393_SYS_GPODSR2      0x07A
#define TC6393_SYS_GPOOECR1     0x07C
#define TC6393_SYS_GPOOECR2     0x07E
#define TC6393_SYS_GPIARCR1     0x080
#define TC6393_SYS_GPIARCR2     0x082
#define TC6393_SYS_GPIARLCR1    0x084
#define TC6393_SYS_GPIARLCR2    0x086
#define TC6393_SYS_GPIBCR1      0x088
#define TC6393_SYS_GPIBCR2      0x08A
#define TC6393_SYS_GPaIARCR     0x08C
#define TC6393_SYS_GPaIARLCR    0x090
#define TC6393_SYS_GPaIBCR      0x094
#define TC6393_SYS_CCR          0x098   /* Clock Control Register */
#define TC6393_SYS_PLL2CR       0x09A
#define TC6393_SYS_PLL1CR1      0x09C
#define TC6393_SYS_PLL1CR2      0x09E
#define TC6393_SYS_DCR          0x0A0
#define TC6393_SYS_FER          0x0E0   /* Function Enable Register */
#define TC6393_SYS_MCR          0x0E4
#define TC6393_SYS_ConfigCR     0x0FC

/* NAND FLASH controller configuration register */
#define TC6393_NAND_REG(ofst) (*(volatile unsigned short*)(TC6393_NAND_BASE+(ofst)))

/* SD Card Configuration register */
#define TC6393_SD_REG(ofst) (*(volatile unsigned short*)(TC6393_SD_BASE+(ofst)))

/* USB HOST Configuration register */
#define TC6393_USB_REG(ofst) (*(volatile unsigned short*)(TC6393_USB_BASE+(ofst)))
#define TC6393_USB_SPRID	0x08
#define TC6393_USB_SPBA1	0x10
#define TC6393_USB_SPBA2	0x12
#define TC6393_USB_ILME		0x40
#define TC6393_USB_SVPMCS	0x4C
#define TC6393_USB_PM_PMES	(1 << 15)
#define TC6393_USB_PM_PMEE	(1 << 8)
#define TC6393_USB_PM_USPW2	(1 << 3)
#define TC6393_USB_PM_USPW1	(1 << 2)
#define TC6393_USB_PM_CKRNEN	(1 << 1)
#define TC6393_USB_PM_GCKEN	(1 << 0)
#define TC6393_USB_INTC		0x50
#define TC6393_USB_SP1INTC1	0x54
#define TC6393_USB_SP1INTC2	0x56
#define TC6393_USB_SP1MBA1	0x58
#define TC6393_USB_SP1MBA2	0x5A
#define TC6393_USB_SP2INTC1	0x5C
#define TC6393_USB_SP2INTC2	0x5E
#define TC6393_USB_SP2MBA1	0x60
#define TC6393_USB_SP2MBA2	0x62
#define TC6393_USB_SPPCNF	0xFC

#define IS_TC6393_RAM0(p)	(TC6393_RAM0_BASE <= (unsigned int)p \
		&& (unsigned int)p <= TC6393_RAM0_BASE + TC6393_RAM0_SIZE)
#define TC6393_RAM0_VAR_TO_OFFSET(x)	((unsigned int)x - TC6393_RAM0_BASE)
#define TC6393_RAM0_OFFSET_TO_VAR(x)	((unsigned int)x + TC6393_RAM0_BASE)

/* Serial I/O controller Configuration register */
#define TC6393_SERIAL_REG(ofst) (*(volatile unsigned short*)(TC6393_SERIAL_BASE+(ofst)))

/* Graphic controller Configuration register */
#define TC6393_GC_REG(ofst) (*(volatile unsigned short*)(TC6393_GC_BASE+(ofst)))
	
/* GPIO bit */
#define TC6393_GPIO19  ( 1 << 19 )
#define TC6393_GPIO18  ( 1 << 18 )
#define TC6393_GPIO17  ( 1 << 17 )
#define TC6393_GPIO16  ( 1 << 16 )
#define TC6393_GPIO15  ( 1 << 15 )
#define TC6393_GPIO14  ( 1 << 14 )
#define TC6393_GPIO13  ( 1 << 13 )
#define TC6393_GPIO12  ( 1 << 12 )
#define TC6393_GPIO11  ( 1 << 11 )
#define TC6393_GPIO10  ( 1 << 10 )
#define TC6393_GPIO9   ( 1 << 9 )
#define TC6393_GPIO8   ( 1 << 8 )
#define TC6393_GPIO7   ( 1 << 7 )
#define TC6393_GPIO6   ( 1 << 6 )
#define TC6393_GPIO5   ( 1 << 5 )
#define TC6393_GPIO4   ( 1 << 4 )
#define TC6393_GPIO3   ( 1 << 3 )
#define TC6393_GPIO2   ( 1 << 2 )
#define TC6393_GPIO1   ( 1 << 1 )
#define TC6393_GPIO0   ( 1 << 0 )

/*
 * TC6393 GPIOs
 */
#define TC6393_TG_ON		TC6393_GPIO0
#define TC6393_L_MUTE		TC6393_GPIO1
#define TC6393_BL_C20MA		TC6393_GPIO3
#define TC6393_CARD_VCC_ON	TC6393_GPIO4
#define TC6393_CHARGE_OFF	TC6393_GPIO6
#define TC6393_CHARGE_OFF_JC	TC6393_GPIO7
#define TC6393_BAT0_V_ON	TC6393_GPIO9
#define TC6393_BAT1_V_ON	TC6393_GPIO10
#define TC6393_BU_CHRG_ON	TC6393_GPIO11
#define TC6393_BAT_SW_ON	TC6393_GPIO12
#define TC6393_BAT0_TH_ON	TC6393_GPIO14
#define TC6393_BAT1_TH_ON	TC6393_GPIO15

#define TC6393_GPO_OE	( TC6393_TG_ON | TC6393_L_MUTE | TC6393_BL_C20MA | \
			  TC6393_CARD_VCC_ON | TC6393_CHARGE_OFF | \
			  TC6393_CHARGE_OFF_JC | TC6393_BAT0_V_ON | \
			  TC6393_BAT1_V_ON | TC6393_BU_CHRG_ON | \
			  TC6393_BAT_SW_ON | TC6393_BAT0_TH_ON | \
			  TC6393_BAT1_TH_ON )

/*
 * SCOOP2 internal I/O mappings
 *
 * We have the following mapping:
 *      phys            virt
 *      08800000        f2000000
 */
#define CF_BUF_CTRL_BASE	0xF2000000
#define SCP_REG(adr)	(*(volatile unsigned short*)(CF_BUF_CTRL_BASE+(adr)))

#define SCP_MCR 	0x00
#define SCP_CDR 	0x04
#define SCP_CSR 	0x08
#define SCP_CPR 	0x0C
#define SCP_CCR 	0x10
#define SCP_IRR 	0x14
#define SCP_IRM 	0x14
#define SCP_IMR 	0x18
#define SCP_ISR 	0x1C
#define SCP_GPCR	0x20
#define SCP_GPWR	0x24
#define SCP_GPRR	0x28
#define SCP_REG_MCR 	SCP_REG(SCP_MCR)
#define SCP_REG_CDR 	SCP_REG(SCP_CDR)
#define SCP_REG_CSR 	SCP_REG(SCP_CSR)
#define SCP_REG_CPR 	SCP_REG(SCP_CPR)
#define SCP_REG_CCR 	SCP_REG(SCP_CCR)
#define SCP_REG_IRR 	SCP_REG(SCP_IRR)
#define SCP_REG_IRM 	SCP_REG(SCP_IRM)
#define SCP_REG_IMR 	SCP_REG(SCP_IMR)
#define SCP_REG_ISR 	SCP_REG(SCP_ISR)
#define SCP_REG_GPCR	SCP_REG(SCP_GPCR)
#define SCP_REG_GPWR	SCP_REG(SCP_GPWR)
#define SCP_REG_GPRR	SCP_REG(SCP_GPRR)

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
 * SCOOP2 internal GPIOs
 */
#define SCP_PXA_VCORE1         SCP_GPCR_PA11
#define SCP_TC6393_REST_IN     SCP_GPCR_PA12
#define SCP_IR_POWERDWN        SCP_GPCR_PA13
#define SCP_SD_WP              SCP_GPCR_PA14
#define SCP_PWR_ON             SCP_GPCR_PA15
#define SCP_AUD_PWR_ON         SCP_GPCR_PA16
#define SCP_BT_RESET           SCP_GPCR_PA17
#define SCP_BT_PWR_EN          SCP_GPCR_PA18
#define SCP_AC_IN_OL           SCP_GPCR_PA19

/* GPIO Direction   1 : outpu mode / 0:input mode */
#define SCP_IO_DIR     ( SCP_PXA_VCORE1 | SCP_TC6393_REST_IN | \
		         SCP_IR_POWERDWN | SCP_PWR_ON | SCP_AUD_PWR_ON |\
		         SCP_BT_RESET | SCP_BT_PWR_EN )
/* GPIO out put level when init   1: Hi */
#define SCP_IO_OUT     ( SCP_TC6393_REST_IN )
//#define GPIO_CO		16

/*
 * SCOOP2 for jacket I/O mappings
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
 * SCOOP2 jacket GPIOs
 */
#define SCP2_BT_LED          SCP_GPCR_PA11
#define SCP2_NOTE_LED        SCP_GPCR_PA12
#define SCP2_CHRG_ERR_LED    SCP_GPCR_PA13
#define SCP2_USB_PULLUP      SCP_GPCR_PA14
#define SCP2_TC6393_SUSPEND  SCP_GPCR_PA15
#define SCP2_TC3693_L3V_ON   SCP_GPCR_PA16
#define SCP2_WLAN_DETECT     SCP_GPCR_PA17
#define SCP2_WLAN_LED                SCP_GPCR_PA18
#define SCP2_CARD_LIMIT_SEL  SCP_GPCR_PA19

/* GPIO Direction   1 : outpu mode / 0:input mode */
#define SCP2_IO_DIR	( SCP2_BT_LED | SCP2_NOTE_LED | \
			  SCP2_CHRG_ERR_LED | SCP2_USB_PULLUP | \
			  SCP2_TC6393_SUSPEND | SCP2_TC3693_L3V_ON | \
			  SCP2_WLAN_LED | SCP2_CARD_LIMIT_SEL )
/* GPIO out put level when init   1: Hi */
//#define SCP2_IO_OUT  ( SCP2_TC6393_SUSPEND | SCP2_TC3693_L3V_ON )
#define SCP2_IO_OUT	( 0 )

/*
 * NSSP
 */
#define NSSCR0		__REG(0x41400000)
#define NSSCR1		__REG(0x41400008)
#define NSSSR		__REG(0x4140000C)
#define NSSITR		__REG(0x41400010)
#define NSSDRTO		__REG(0x41400028)

/*
 * Timing Generator
 */
#define TG_PNLCTL	0x00
#define TG_TPOSCTL	0x01
#define TG_DUTYCTL	0x02
#define TG_GPOSR	0x03
#define TG_GPODR1	0x04
#define TG_GPODR2	0x05
#define TG_PINICTL	0x06
#define TG_HPOSCTL	0x07
	
#if 0
/*
 * Flash Memory mappings
 * 
 * We have the following mapping:
 *			phys		virt
 *	boot ROM	00000000	ef000000
 *	NAND Flash	0C000000	f2100000
 */
#define NAND_FLASH_REG_BASE	0xf2100000
#define CPLD_REG(ofst)	(*(volatile unsigned char*)(NAND_FLASH_REG_BASE+(ofst)))

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
#endif


/*
 * LED
 */
#define SCP_LED_BLUE 		SCP_GPCR_PA11
#define SCP_LED_GREEN		SCP_GPCR_PA12
#define SCP_LED_ORANGE		SCP_GPCR_PA13
#define SCP_LED_WLAN		SCP_GPCR_PA18


/*
 * PXA GPIOs
 */
#define GPIO_POWERON           (0)
#define GPIO_RESET             (1)
#define GPIO_AC_IN             (2)
#define GPIO_RECORD_BTN        (3)
#define GPIO_SYNC              (4)     /* Cradle SYNC Button */
#define GPIO_USB_IN            (5)
//#define GPIO_nSD_CLK         (6)
#define GPIO_JACKET_DETECT     (7)
#define GPIO_nSD_DETECT        (9)
#define GPIO_nSD_INT           (10)
#define GPIO_TC6393_CLK        (11)
#define GPIO_BAT1_CRG          (12)
#define GPIO_CF_CD             (13)
#define GPIO_BAT0_CRG          (14)
#define GPIO_TC6393_INT        (15)
#define GPIO_BAT0_LOW          (17)
#define GPIO_TC6393_RDY        (18)
#define GPIO_ON_RESET          (19)
#define GPIO_EAR_IN            (20)
#define GPIO_CF_IRQ            (21)    /* CF slot0 Ready */
#define GPIO_ON_KEY            (22)
#define GPIO_VGA_LINE          (27)
#define GPIO_TP_INT            (32)    /* Touch Panel pen down interrupt */
#define GPIO_CF2_IRQ           (36)    /* CF slot1 Ready */
#define GPIO_BAT_LOCKED        (38)    /* Battery locked */
#define GPIO_TG_SPI_SCLK       (81)
#define GPIO_TG_SPI_CS         (82)
#define GPIO_TG_SPI_MOSI       (83)
#define GPIO_BAT1_LOW          (84)

#define GPIO_HP_IN             GPIO_EAR_IN

#define GPIO_MAIN_BAT_LOW	GPIO_BAT0_LOW

#define KEY_STROBE_NUM			(11)
#define KEY_SENSE_NUM			(7)
#define GPIO_HIGH_STROBE_BIT		(0xfc000000)
#define GPIO_LOW_STROBE_BIT		(0x0000001f)
#define GPIO_ALL_SENSE_BIT		(0x00000fe0)
#define GPIO_ALL_SENSE_RSHIFT		(5)
#define GPIO_STROBE_BIT(a)		GPIO_bit(58+(a))
#define GPIO_SENSE_BIT(a)		GPIO_bit(69+(a))
#define GAFR_HIGH_STROBE_BIT		(0xfff00000)
#define GAFR_LOW_STROBE_BIT		(0x000003ff)
#define GAFR_ALL_SENSE_BIT		(0x00fffc00)
#define GPIO_KEY_SENSE(a) 		(69+(a))


/*
 * Interrupts
 */
#define IRQ_GPIO_WAKEUP	       IRQ_GPIO(GPIO_WAKEUP)
#define IRQ_GPIO_AC_IN         IRQ_GPIO(GPIO_AC_IN)
#define IRQ_GPIO_RECORD_BTN    IRQ_GPIO(GPIO_RECORD_BTN)
#define IRQ_GPIO_SYNC          IRQ_GPIO(GPIO_SYNC)
#define IRQ_GPIO_USB_IN        IRQ_GPIO(GPIO_USB_IN)
#define IRQ_GPIO_JACKET_DETECT IRQ_GPIO(GPIO_JACKET_DETECT)
#define IRQ_GPIO_nSD_INT       IRQ_GPIO(GPIO_nSD_INT)
#define IRQ_GPIO_nSD_DETECT    IRQ_GPIO(GPIO_nSD_DETECT)
#define IRQ_GPIO_BAT1_CRG      IRQ_GPIO(GPIO_BAT1_CRG)
#define IRQ_GPIO_CF_CD         IRQ_GPIO(GPIO_CF_CD)
#define IRQ_GPIO_BAT0_CRG      IRQ_GPIO(GPIO_BAT0_CRG)
#define IRQ_GPIO_TC6393_INT    IRQ_GPIO(GPIO_TC6393_INT)
#define IRQ_GPIO_BAT0_LOW      IRQ_GPIO(GPIO_BAT0_LOW)
#define IRQ_GPIO_EAR_IN                IRQ_GPIO(GPIO_EAR_IN)
#define IRQ_GPIO_CF_IRQ                IRQ_GPIO(GPIO_CF_IRQ)
#define IRQ_GPIO_ON_KEY                IRQ_GPIO(GPIO_ON_KEY)
#define IRQ_GPIO_VGA_LINE      IRQ_GPIO(GPIO_VGA_LINE)
#define IRQ_GPIO_TP_INT                IRQ_GPIO(GPIO_TP_INT)
#define IRQ_GPIO_CF2_IRQ       IRQ_GPIO(GPIO_CF2_IRQ)
#define IRQ_GPIO_BAT_LOCKED    IRQ_GPIO(GPIO_BAT_LOCKED)
#define IRQ_GPIO_BAT1_LOW      IRQ_GPIO(GPIO_BAT1_LOW)
#define IRQ_GPIO_KEY_SENSE(a)  IRQ_GPIO(69+(a))

#define IRQ_GPIO_MAIN_BAT_LOW	IRQ_GPIO(GPIO_MAIN_BAT_LOW)

// CS
#define CS_MAX1111	1
#define CS_ADS7846	2
#define CS_LZ9JG18	3

#define LOGICAL_WAKEUP_SRC


#define	AC_IN_STATUS	(~GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN))

#endif	/* _ASM_ARCH_TOSA_H_ */
