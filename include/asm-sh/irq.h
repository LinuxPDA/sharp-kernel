#ifndef __ASM_SH_IRQ_H
#define __ASM_SH_IRQ_H

/*
 *
 * linux/include/asm-sh/irq.h
 *
 * Copyright (C) 1999  Niibe Yutaka & Takeshi Yaegashi
 * Copyright (C) 2000  Kazumoto Kojima
 *
 */

#include <linux/config.h>
#include <asm/machvec.h>
#include <asm/ptrace.h>		/* for pt_regs */

#if defined(__sh3__)
#if defined(CONFIG_CPU_SUBTYPE_SH7290) || defined(CONFIG_CPU_SUBTYPE_SH7710) || defined(CONFIG_CPU_SUBTYPE_SH7720)
#define INTC_IPRA	0xa414fee2UL
#define INTC_IPRB	0xa414fee4UL
#else
#define INTC_IPRA  	0xfffffee2UL
#define INTC_IPRB  	0xfffffee4UL
#endif
#elif defined(__SH4__)
#define INTC_IPRA	0xffd00004UL
#define INTC_IPRB	0xffd00008UL
#define INTC_IPRC	0xffd0000cUL
#if defined(CONFIG_CPU_SUBTYPE_SH7760)
#define INTC_IPRD		0xffd00010UL
#define INTC_INTPRI00		0xfe080000UL
#define INTC_INTPRI04		0xfe080004UL
#define INTC_INTPRI08		0xfe080008UL
#define INTC_INTPRI0C		0xfe08000cUL
#define INTC_INTREQ00		0xfe080020UL
#define INTC_INTREQ04		0xfe080024UL
#define INTC_INTMSK00		0xfe080040UL
#define INTC_INTMSK04		0xfe080044UL
#define INTC_INTMSKCLR00	0xfe080060UL
#define INTC_INTMSKCLR04	0xfe080064UL
#endif
#endif

#define TIMER_IRQ	16
#define TIMER_IPR_ADDR	INTC_IPRA
#define TIMER_IPR_POS	 3
#define TIMER_PRIORITY	 2
#define TMU1_IRQ	17
#define TMU1_IPR_ADDR	INTC_IPRA
#define TMU1_IPR_POS	 2
#define TMU1_PRIORITY	 2
#if defined(CONFIG_CPU_SUBTYPE_SH7710)
#define TMU1_PRIORITY_RT 15  //for realtime
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7290) || defined(CONFIG_CPU_SUBTYPE_SH7710) || defined(CONFIG_CPU_SUBTYPE_SH7720)
#define TUNI1_IRQ	17
#define TUNI1_IPR_ADDR	INTC_IPRA
#define TUNI1_IPR_POS	2
#define TUNI1_PRIORITY	2
#define TUNI2_IRQ	18
# if !defined(CONFIG_CPU_SUBTYPE_SH7710) && !defined(CONFIG_CPU_SUBTYPE_SH7720)
#define TUCPI_IRQ	19
# endif
#define TUNI2_IPR_ADDR	INTC_IPRA
#define TUNI2_IPR_POS	1
#define TUNI2_PRIORITY	2
# if defined(CONFIG_CPU_SUBTYPE_SH7710) || defined(CONFIG_CPU_SUBTYPE_SH7720)
# define RTC_IRQ	22
# define RTC_IPR_ADDR	INTC_IPRA
# define RTC_IPR_POS	0
# define RTC_PRIORITY	TIMER_PRIORITY
# endif
#else
#define RTC_IRQ		22
#define RTC_IPR_ADDR	INTC_IPRA
#define RTC_IPR_POS	 0
#define RTC_PRIORITY	TIMER_PRIORITY
#endif

#define RTC_ALARM_IRQ   20
#define RTC_TICK_IRQ    21

#if defined(__sh3__)
#define DMTE0_IRQ	48
#define DMTE1_IRQ	49
#define DMTE2_IRQ	50
#define DMTE3_IRQ	51
#define DMA_IPR_ADDR	INTC_IPRE
#define DMA_IPR_POS	3
#define DMA_PRIORITY	7
#if defined(CONFIG_CPU_SUBTYPE_SH7290) || defined(CONFIG_CPU_SUBTYPE_SH7710) || defined(CONFIG_CPU_SUBTYPE_SH7720)
#define DMTE4_IRQ	76
#define DMTE5_IRQ	77
#define DMAC2_IPR_ADDR	INTC_IPRF
#define DMAC2_IPR_POS	2
#define DMAC2_PRIORITY	7
#endif
#elif defined(__SH4__)
#define DMTE0_IRQ	34
#define DMTE1_IRQ	35
#define DMTE2_IRQ	36
#define DMTE3_IRQ	37
#if defined(CONFIG_CPU_SUBTYPE_SH7760) || defined(CONFIG_SH_RTS7751R2D)
#define DMTE4_IRQ	44
#define DMTE5_IRQ	45
#define DMTE6_IRQ	46
#define DMTE7_IRQ	47
#endif
#define DMAE_IRQ	38
#define DMA_IPR_ADDR	INTC_IPRC
#define DMA_IPR_POS	2
#define DMA_PRIORITY	7
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7290) || defined(CONFIG_CPU_SUBTYPE_SH7720)
#define SCIF0_IRQ	80
#define SCIF0_IPR_ADDR	INTC_IPRG
#define SCIF0_IPR_POS	3
#define SCIF0_PRIORITY	3
#define SCIF1_IRQ	81
#define SCIF1_IPR_ADDR	INTC_IPRG
#define SCIF1_IPR_POS	2
#define SCIF1_PRIORITY	3
#endif

#if defined (CONFIG_CPU_SUBTYPE_SH7707)  || \
    defined (CONFIG_CPU_SUBTYPE_SH7708)  || \
    defined (CONFIG_CPU_SUBTYPE_SH7709)  || \
    defined (CONFIG_CPU_SUBTYPE_SH7727)  || \
    defined (CONFIG_CPU_SUBTYPE_SH7750)  || \
    defined (CONFIG_CPU_SUBTYPE_SH7751)  || \
    defined (CONFIG_CPU_SUBTYPE_SH7750R) || \
    defined (CONFIG_CPU_SUBTYPE_SH7751R)
#define SCI_ERI_IRQ	23
#define SCI_RXI_IRQ	24
#define SCI_TXI_IRQ	25
#define SCI_IPR_ADDR	INTC_IPRB
#define SCI_IPR_POS	1
#define SCI_PRIORITY	3
#elif defined (CONFIG_CPU_SUBTYPE_SH7760)
#define SCI_ERI_IRQ	40
#define SCI_RXI_IRQ	41
#define SCI_BRI_IRQ	42
#define SCI_TXI_IRQ	43
#define SCI_IPR_ADDR	INTC_INTPRI08
#define SCI_IPR_POS	4
#define SCI_PRIORITY	3
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7707) || defined(CONFIG_CPU_SUBTYPE_SH7709) || defined(CONFIG_CPU_SUBTYPE_SH7727)
#define SCIF_ERI_IRQ	56
#define SCIF_RXI_IRQ	57
#define SCIF_BRI_IRQ	58
#define SCIF_TXI_IRQ	59
#define SCIF_IPR_ADDR	INTC_IPRE
#define SCIF_IPR_POS	1
#define SCIF_PRIORITY	3

#define IRDA_ERI_IRQ	52
#define IRDA_RXI_IRQ	53
#define IRDA_BRI_IRQ	54
#define IRDA_TXI_IRQ	55
#define IRDA_IPR_ADDR	INTC_IPRE
#define IRDA_IPR_POS	2
#define IRDA_PRIORITY	3

#elif defined(CONFIG_CPU_SUBTYPE_SH7710)
#define SCIF_ERI_IRQ	52
#define SCIF_RXI_IRQ	53
#define SCIF_BRI_IRQ	54
#define SCIF_TXI_IRQ	55
#define SCIF_IPR_ADDR	INTC_IPRE
#define SCIF_IPR_POS	2
#define SCIF_PRIORITY	3

#define SCIF1_ERI_IRQ	56
#define SCIF1_RXI_IRQ	57
#define SCIF1_BRI_IRQ	58
#define SCIF1_TXI_IRQ	59
#define SCIF1_IPR_ADDR	INTC_IPRE
#define SCIF1_IPR_POS	1
#define SCIF1_PRIORITY	3

#elif defined(CONFIG_CPU_SUBTYPE_SH7750)  || \
      defined(CONFIG_CPU_SUBTYPE_SH7751)  || \
      defined(CONFIG_CPU_SUBTYPE_SH7750R) || \
      defined(CONFIG_CPU_SUBTYPE_SH7751R) || \
      defined(CONFIG_CPU_SUBTYPE_ST40STB1)
#define SCIF_ERI_IRQ	40
#define SCIF_RXI_IRQ	41
#define SCIF_BRI_IRQ	42
#define SCIF_TXI_IRQ	43
#define SCIF_IPR_ADDR	INTC_IPRC
#define SCIF_IPR_POS	1
#define SCIF_PRIORITY	3
#if defined(CONFIG_CPU_SUBTYPE_ST40STB1)
#define SCIF1_ERI_IRQ	23
#define SCIF1_RXI_IRQ	24
#define SCIF1_BRI_IRQ	25
#define SCIF1_TXI_IRQ	26
#define SCIF1_IPR_ADDR	INTC_IPRB
#define SCIF1_IPR_POS	1
#define SCIF1_PRIORITY	3
#endif
#elif defined(CONFIG_CPU_SUBTYPE_SH7760)
#define SCIF_ERI_IRQ	72
#define SCIF_RXI_IRQ	73
#define SCIF_BRI_IRQ	74
#define SCIF_TXI_IRQ	75
#define SCIF_IPR_ADDR	INTC_INTPRI08
#define SCIF_IPR_POS	3
#define SCIF_PRIORITY	3
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7760)
#define GPIO_IRQ        33
#define GPIO_IPR_ADDR   INTC_IPRC
#define GPIO_IPR_POS    3
#define GPIO_PRIORITY   0
#define IRQ4_IRQ        48
#define IRQ4_IPR_ADDR   INTC_INTPRI00
#define IRQ4_IPR_POS    7
#define IRQ4_PRIORITY   0
#define IRQ5_IRQ        49
#define IRQ5_IPR_ADDR   INTC_INTPRI00
#define IRQ5_IPR_POS    6
#define IRQ5_PRIORITY   0
#define IRQ6_IRQ        50
#define IRQ6_IPR_ADDR   INTC_INTPRI00
#define IRQ6_IPR_POS    5
#define IRQ6_PRIORITY   0
#define IRQ7_IRQ        51
#define IRQ7_IPR_ADDR   INTC_INTPRI00
#define IRQ7_IPR_POS    4
#define IRQ7_PRIORITY   0
#define CAN0_IRQ        56
#define CAN0_IPR_ADDR   INTC_INTPRI04
#define CAN0_IPR_POS    7
#define CAN0_PRIORITY   0
#define CAN1_IRQ        57
#define CAN1_IPR_ADDR   INTC_INTPRI04
#define CAN1_IPR_POS    6
#define CAN1_PRIORITY   0
#define SSI0_IRQ        58
#define SSI0_IPR_ADDR   INTC_INTPRI04
#define SSI0_IPR_POS    5
#define SSI0_PRIORITY   0
#define SSI1_IRQ        59
#define SSI1_IPR_ADDR   INTC_INTPRI04
#define SSI1_IPR_POS    4
#define SSI1_PRIORITY   0
#define AC970_IRQ       60
#define AC970_IPR_ADDR  INTC_INTPRI04
#define AC970_IPR_POS   3
#define AC970_PRIORITY  0
#define AC971_IRQ       61
#define AC971_IPR_ADDR  INTC_INTPRI04
#define AC971_IPR_POS   2
#define AC971_PRIORITY  0
#define IIC0_IRQ        62
#define IIC0_IPR_ADDR   INTC_INTPRI04
#define IIC0_IPR_POS    1
#define IIC0_PRIORITY   0
#define IIC1_IRQ        63
#define IIC1_IPR_ADDR   INTC_INTPRI04
#define IIC1_IPR_POS    0
#define IIC1_PRIORITY   0
#define USBH_IRQ        64
#define USBH_IPR_ADDR   INTC_INTPRI08
#define USBH_IPR_POS    7
#define USBH_PRIORITY   3
#define LCDC_IRQ        65
#define LCDC_IPR_ADDR   INTC_INTPRI08
#define LCDC_IPR_POS    6
#define LCDC_PRIORITY   0
#define DMABRG0_IRQ     68
#define DMABRG1_IRQ     69
#define DMABRG2_IRQ     70
#define DMABRG_IPR_ADDR INTC_INTPRI08
#define DMABRG_IPR_POS  5
#define DMABRG_PRIORITY 0
#define SIM_ERI_IRQ     80
#define SIM_RXI_IRQ     81
#define SIM_TXI_IRQ     82
#define SIM_TEI_IRQ     83
#define SIM_IPR_ADDR    INTC_INTPRI08
#define SIM_IPR_POS     4
#define SIM_PRIORITY    0
#define SPI_IRQ         84
#define SPI_IPR_ADDR    INTC_INTPRI08
#define SPI_IPR_POS     0
#define SPI_PRIORITY    0
#define MMC0_IRQ        88
#define MMC1_IRQ        89
#define MMC2_IRQ        90
#define MMC3_IRQ        91
#define MMC_IPR_ADDR    INTC_INTPRI0C
#define MMC_IPR_POS     5
#define MMC_PRIORITY    0
#define MFI_IRQ         100
#define MFI_IPR_ADDR    INTC_INTPRI0C
#define MFI_IPR_POS     3
#define MFI_PRIORITY    0
#define FLSTE_IRQ       104
#define FLTEND_IRQ      105
#define FLTRQ0_IRQ      106
#define FLTRQ1_IRQ      107
#define FLCTL_IPR_ADDR  INTC_INTPRI0C
#define FLCTL_IPR_POS   2
#define FLCTL_PRIORITY  0
#define ADC_IRQ         108
#define ADC_IPR_ADDR    INTC_INTPRI0C
#define ADC_IPR_POS     1
#define ADC_PRIORITY    0
#define CMT_IRQ         109
#define CMT_IPR_ADDR    INTC_INTPRI0C
#define CMT_IPR_POS     0
#define CMT_PRIORITY    0
#endif

/* NR_IRQS is made from three components:
 *   1. ONCHIP_NR_IRQS - number of IRLS + on-chip peripherial modules
 *   2. PINT_NR_IRQS   - number of PINT interrupts
 *   3. OFFCHIP_NR_IRQS - numbe of IRQs from off-chip peripherial modules
 */

#if defined(CONFIG_CPU_SUBTYPE_SH7727)
#define DMAC_DEI0_IRQ	48
#define DMAC_DEI1_IRQ	49
#define DMAC_DEI2_IRQ	50
#define DMAC_DEI3_IRQ	51
#define DMAC_IPR_ADDR	INTC_IPRE
#define DMAC_IPR_POS	3
#define DMAC_PRIORITY	3
#define ADC_IRQ		60
#define ADC_IPR_ADDR	INTC_IPRE
#define ADC_IPR_POS	0
#define ADC_PRIORITY	3
#define LCDC_IRQ	61
#define LCDC_IPR_ADDR	INTC_IPRF
#define LCDC_IPR_POS	2
#define LCDC_PRIORITY	3
#define PCC_IRQ		62
#define PCC_IPR_ADDR	INTC_IPRF
#define PCC_IPR_POS	1
#define PCC_PRIORITY	3
#define SIOF_ERI_IRQ	72
#define SIOF_TXI_IRQ	73
#define SIOF_RXI_IRQ	74
#define SIOF_CCI_IRQ	75
#define SIOF_IPR_ADDR	INTC_IPRF
#define SIOF_IPR_POS	0
#define SIOF_PRIORITY	3
#define USBH_IRQ	64
#define USBH_IPR_ADDR	INTC_IPRG
#define USBH_IPR_POS	3
#define USBH_PRIORITY	3
#define USBF0_IRQ	65
#define USBF0_IPR_ADDR	INTC_IPRG
#define USBF0_IPR_POS	2
#define USBF0_PRIORITY	3
#define USBF1_IRQ	66
#define USBF1_IPR_ADDR	INTC_IPRG
#define USBF1_IPR_POS	1
#define USBF1_PRIORITY	3
#define AFEIF_IRQ	67
#define AFEIF_IPR_ADDR	INTC_IPRG
#define AFEIF_IPR_POS	0
#define AFEIF_PRIORITY	3
#endif

/* 1. ONCHIP_NR_IRQS */
#ifdef CONFIG_SH_GENERIC
# define ONCHIP_NR_IRQS 144
#else
# if defined(CONFIG_CPU_SUBTYPE_SH7707)
#  define ONCHIP_NR_IRQS 64
#  define PINT_NR_IRQS   16
# elif defined(CONFIG_CPU_SUBTYPE_SH7708)
#  define ONCHIP_NR_IRQS 32
# elif defined(CONFIG_CPU_SUBTYPE_SH7709)
#  define ONCHIP_NR_IRQS 64	// Actually 61
#  define PINT_NR_IRQS   16
# elif defined(CONFIG_CPU_SUBTYPE_SH7710)
#  define ONCHIP_NR_IRQS 104	// Actually 103
# elif defined(CONFIG_CPU_SUBTYPE_SH7720)
#  define ONCHIP_NR_IRQS 112	// Actually 111
#  define PINT_NR_IRQS   16
# elif defined(CONFIG_CPU_SUBTYPE_SH7727)
#  define ONCHIP_NR_IRQS 96	// Actually 75
#  define PINT_NR_IRQS   16
# elif defined(CONFIG_CPU_SUBTYPE_SH7290)
#  define ONCHIP_NR_IRQS 104
# elif defined(CONFIG_CPU_SUBTYPE_SH7750)
#  define ONCHIP_NR_IRQS 48	// Actually 44
# elif defined(CONFIG_CPU_SUBTYPE_SH7751)
#  define ONCHIP_NR_IRQS 72
# elif defined(CONFIG_CPU_SUBTYPE_SH7750R)
#  define ONCHIP_NR_IRQS 48	// Actually 44
# elif defined(CONFIG_CPU_SUBTYPE_SH7751R)
#  define ONCHIP_NR_IRQS 72
# elif defined(CONFIG_CPU_SUBTYPE_SH7760)
#  define ONCHIP_NR_IRQS 110
# elif defined(CONFIG_CPU_SUBTYPE_ST40STB1)
#  define ONCHIP_NR_IRQS 144
# endif
#endif

/* 2. PINT_NR_IRQS */
#ifdef CONFIG_SH_GENERIC
# define PINT_NR_IRQS 16
#else
# ifndef PINT_NR_IRQS
#  define PINT_NR_IRQS 0
# endif
#endif

#if PINT_NR_IRQS > 0
# define PINT_IRQ_BASE  ONCHIP_NR_IRQS
#endif

/* 3. OFFCHIP_NR_IRQS */
#ifdef CONFIG_SH_GENERIC
# define OFFCHIP_NR_IRQS 16
#else
# if defined(CONFIG_HD64461)
#  define OFFCHIP_NR_IRQS 16
# elif defined (CONFIG_SH_BIGSUR) /* must be before CONFIG_HD64465 */
#  define OFFCHIP_NR_IRQS 48
# elif defined(CONFIG_HD64465)
#  define OFFCHIP_NR_IRQS 16
# elif defined (CONFIG_SH_EC3104)
#  define OFFCHIP_NR_IRQS 16
# elif defined (CONFIG_SH_DREAMCAST)
#  define OFFCHIP_NR_IRQS 96
# else
#  define OFFCHIP_NR_IRQS 0
# endif
#endif

#if OFFCHIP_NR_IRQS > 0
# define OFFCHIP_IRQ_BASE (ONCHIP_NR_IRQS + PINT_NR_IRQS)
#endif

/* NR_IRQS. 1+2+3 */
#define NR_IRQS (ONCHIP_NR_IRQS + PINT_NR_IRQS + OFFCHIP_NR_IRQS)

/* In a generic kernel, NR_IRQS is an upper bound, and we should use
 * ACTUAL_NR_IRQS (which uses the machine vector) to get the correct value.
 */
#ifdef CONFIG_SH_GENERIC
# define ACTUAL_NR_IRQS (sh_mv.mv_nr_irqs)
#else
# define ACTUAL_NR_IRQS NR_IRQS
#endif


extern void disable_irq(unsigned int);
extern void disable_irq_nosync(unsigned int);
extern void enable_irq(unsigned int);

/*
 * Simple Mask Register Support
 */
extern void make_maskreg_irq(unsigned int irq);
extern unsigned short *irq_mask_register;

/*
 * Function for "on chip support modules".
 */
extern void make_ipr_irq(unsigned int irq, unsigned int addr,
			 int pos,  int priority);
extern void make_imask_irq(unsigned int irq);

#if defined(CONFIG_CPU_SUBTYPE_SH7290)

#define INTC_IRR0	0xa4140004UL

#define INTC_ICR0	0xa414fee0UL
#define INTC_ICR1	0xa4140010UL
#define INTC_ICR2	0xa4140020UL

#define INTC_IPRC	0xa4140016UL
#define INTC_IPRD	0xa4140018UL
#define INTC_IPRE	0xa414001aUL
#define INTC_IPRF	0xa4080000UL
#define INTC_IPRG	0xa4080002UL
#define INTC_IPRH	0xa4080004UL
#define INTC_IPRI	0xa4080006UL

#define PACR		0xa4050100UL
#define PBCR		0xa4050102UL
#define PCCR		0xa4050104UL
#define PDCR		0xa4050106UL
#define PECR		0xa4050108UL
#define PFCR		0xa405010aUL
#define PGCR		0xa405010cUL
#define PHCR		0xa405010eUL
#define PJCR		0xa4050110UL
#define PKCR		0xa4050112UL
#define PLCR		0xa4050114UL
#define PMCR		0xa4050118UL
#define PNCR		0xa405011aUL
#define PQCR		0xa405011cUL

#define ERI_IRQ		23
#define RXI_IRQ		24
#define TXI_IRQ		25
#define TEND_IRQ	26
#define SMI_IPR_ADDR	INTC_IPRB
#define SMI_IPR_POS	1
#define SMI_PRIORITY	3
#define ITI_IRQ		27
#define WDT_IPR_ADDR	INTC_IPRB
#define WDT_IPR_POS	3
#define WDT_PRIORITY	3

#define IRQ0_IRQ	32
#define IRQ1_IRQ	33
#define IRQ2_IRQ	34
#define IRQ3_IRQ	35
#define IRQ4_IRQ	36
#define IRQ5_IRQ	37
#define IRQ6_IRQ	38
#define IRQ7_IRQ	39

#define IRQ0_IPR_ADDR	INTC_IPRC
#define IRQ1_IPR_ADDR	INTC_IPRC
#define IRQ2_IPR_ADDR	INTC_IPRC
#define IRQ3_IPR_ADDR	INTC_IPRC
#define IRQ4_IPR_ADDR	INTC_IPRD
#define IRQ5_IPR_ADDR	INTC_IPRD
#define IRQ6_IPR_ADDR	INTC_IPRD
#define IRQ7_IPR_ADDR	INTC_IPRD

#define IRQ0_IPR_POS	0
#define IRQ1_IPR_POS	1
#define IRQ2_IPR_POS	2
#define IRQ3_IPR_POS	3
#define IRQ4_IPR_POS	0
#define IRQ5_IPR_POS	1
#define IRQ6_IPR_POS	2
#define IRQ7_IPR_POS	3

#define IRQ0_PRIORITY	1
#define IRQ1_PRIORITY	1
#define IRQ2_PRIORITY	1
#define IRQ3_PRIORITY	1
#define IRQ4_PRIORITY	1
#define IRQ5_PRIORITY	1
#define IRQ6_PRIORITY	1
#define IRQ7_PRIORITY	1

#define CMT_IRQ		104
#define CMT_IPR_ADDR	INTC_IPRF
#define CMT_IPR_POS	0
#define CMT_PRIORITY	3
#define USI0_IRQ	65
#define USI1_IRQ	66
#define USB_IPR_ADDR	INTC_IPRF
#define USB_IPR_POS	1
#define USB_PRIORITY	3
#define KEY_IRQ		79
#define KEY_IPR_ADDR	INTC_IPRF
#define KEY_IPR_POS	3
#define KEY_PRIORITY	3

#define ALI_IRQ		96
#define TACKI_IRQ	97
#define WAITI_IRQ	98
#define DTEI_IRQ	99
#define I2C_IPR_ADDR	INTC_IPRH
#define I2C_IPR_POS	0
#define I2C_PRIORITY	3
#define FLSTE_IRQ	92
#define FLTEND_IRQ	93
#define FLTRQ0_IRQ	94
#define FLTRQ1_IRQ	95
#define FLCTL_IPR_ADDR	INTC_IPRH
#define FLCTL_IPR_POS	1
#define SIOF1_IRQ	84
#define SIOF1_IPR_ADDR	INTC_IPRH
#define SIOF1_IPR_POS	2
#define SIOF1_PRIORITY	3
#define SIOF0_IRQ	85
#define SIOF0_IPR_ADDR	INTC_IPRH
#define SIOF0_IPR_POS	3
#define SIOF0_PRIORITY	3

#define MMCI0_IRQ	100
#define MMCI1_IRQ	101
#define MMCI2_IRQ	102
#define MMCI3_IRQ	103
#define MMC_IPR_ADDR	INTC_IPRI
#define MMC_IPR_POS	1
#define MMC_PRIORITY	3
#define SIO1_IRQ	88
#define SIO1_IPR_ADDR	INTC_IPRI
#define SIO1_IPR_POS	2
#define SIO1_PRIORITY	3
#define SIO0_IRQ	89
#define SIO0_IPR_ADDR	INTC_IPRI
#define SIO0_IPR_POS	3
#define SIO0_PRIORITY	3

extern int ipr_irq_demux(int irq);
#define __irq_demux(irq) ipr_irq_demux(irq)

#elif defined(CONFIG_CPU_SUBTYPE_SH7710)
#define INTC_IRR0	0xa4140004UL

#define INTC_ICR0	0xa414fee0UL
#define INTC_ICR1	0xa4140010UL

#define INTC_IPRC	0xa4140016UL
#define INTC_IPRD	0xa4140018UL
#define INTC_IPRE	0xa414001aUL
#define INTC_IPRF	0xa4080000UL
#define INTC_IPRG	0xa4080002UL
#define INTC_IPRH	0xa4080004UL
#define INTC_IPRI	0xa4080006UL

#define PACR		0xa4050100UL
#define PBCR		0xa4050102UL
#define PCCR		0xa4050104UL
#define PETCR		0xa4050106UL
#if 0
#define PDCR           0xa4050106UL
#define PECR           0xa4050108UL
#define PFCR           0xa405010aUL
#define PGCR           0xa405010cUL
#define PHCR           0xa405010eUL
#define PJCR           0xa4050110UL
#define PKCR           0xa4050112UL
#define PLCR           0xa4050114UL
#define PMCR           0xa4050118UL
#define PNCR           0xa405011aUL
#define PQCR           0xa405011cUL
#define ERI_IRQ                23
#define RXI_IRQ                24
#define TXI_IRQ                25
#define TEND_IRQ       26
#define SMI_IPR_ADDR   INTC_IPRB
#define SMI_IPR_POS    1
#define SMI_PRIORITY   3
#endif
#define ITI_IRQ		27
#define WDT_IPR_ADDR	INTC_IPRB
#define WDT_IPR_POS	3
#define WDT_PRIORITY	3

#define IRQ0_IRQ	32
#define IRQ4_IRQ	36
#define IRQ5_IRQ	37

#define IRQ0_IPR_ADDR	INTC_IPRC
#define IRQ1_IPR_ADDR	INTC_IPRC
#define IRQ2_IPR_ADDR	INTC_IPRC
#define IRQ3_IPR_ADDR	INTC_IPRC
#define IRQ4_IPR_ADDR	INTC_IPRD
#define IRQ5_IPR_ADDR	INTC_IPRD

#define IRQ0_IPR_POS	0
#define IRQ1_IPR_POS	1
#define IRQ2_IPR_POS	2
#define IRQ3_IPR_POS	3
#define IRQ4_IPR_POS	0
#define IRQ5_IPR_POS	1

#define IRQ0_PRIORITY	1
#define IRQ1_PRIORITY	1
#define IRQ2_PRIORITY	1
#define IRQ3_PRIORITY	1
#define IRQ4_PRIORITY	1
#define IRQ5_PRIORITY	1
#define IRQ6_PRIORITY	1
#define IRQ7_PRIORITY	1

#define SIOF1_IRQ	100
#define SIOF1_IPR_ADDR	INTC_IPRI
#define SIOF1_IPR_POS	1
#define SIOF1_PRIORITY	3
#define SIOF0_IRQ	96
#define SIOF0_IPR_ADDR	INTC_IPRH
#define SIOF0_IPR_POS	0
#define SIOF0_PRIORITY	3

#define IPSEC_IRQ	79
#define IPSEC_IPR_ADDR	INTC_IPRF
#define IPSEC_IPR_POS	3
#define IPSEC_PRIORITY	3
#define EDMAC_EINT0_IRQ	80
#define EDMAC_EINT1_IRQ	81
#define EDMAC_EINT2_IRQ	82
#define EDMAC_IPR_ADDR	INTC_IPRG
#define EDMAC_EINT0_IPR_POS	3
#define EDMAC_EINT1_IPR_POS	2
#define EDMAC_EINT2_IPR_POS	1
#define EDMAC_PRIORITY	3

extern int ipr_irq_demux(int irq);
#define __irq_demux(irq) ipr_irq_demux(irq)
 
#elif defined(CONFIG_CPU_SUBTYPE_SH7720)
#define INTC_IRR0	0xa4140004UL

#define INTC_ICR0  	0xa414fee0UL
#define INTC_ICR1  	0xa4140010UL
#define INTC_ICR2  	0xa4140012UL

#define INTC_PINTER 	0xa4140014UL

#define INTC_IPRC  	0xa4140016UL
#define INTC_IPRD  	0xa4140018UL
#define INTC_IPRE  	0xa414001aUL
#define INTC_IPRF	0xa4080000UL
#define INTC_IPRG	0xa4080002UL
#define INTC_IPRH	0xa4080004UL
#define INTC_IPRI	0xa4080006UL
#define INTC_IPRJ	0xa4080008UL

#define PORT_PACR	0xa4050100UL
#define PORT_PBCR	0xa4050102UL
#define PORT_PCCR	0xa4050104UL
#define PORT_PDCR	0xa4050106UL
#define PORT_PECR	0xa4050108UL
#define PORT_PFCR	0xa405010aUL
#define PORT_PGCR	0xa405010cUL
#define PORT_PHCR	0xa405010eUL
#define PORT_PJCR	0xa4050110UL
#define PORT_PKCR	0xa4050112UL
#define PORT_PLCR	0xa4050114UL
#define PORT_PMCR	0xa4050116UL
#define PORT_PPCR	0xa4050118UL
#define PORT_PRCR	0xa405011aUL
#define PORT_PSCR	0xa405011cUL
#define PORT_PTCR	0xa405011eUL
#define PORT_PUCR	0xa4050120UL
#define PORT_PVCR	0xa4050122UL

#define PORT_PADR  	0xa4050140UL
#define PORT_PBDR  	0xa4050142UL
#define PORT_PCDR  	0xa4050144UL
#define PORT_PDDR  	0xa4050146UL
#define PORT_PEDR  	0xa4050148UL
#define PORT_PFDR  	0xa405014aUL
#define PORT_PGDR  	0xa405014cUL
#define PORT_PHDR  	0xa405014eUL
#define PORT_PJDR  	0xa4050150UL
#define PORT_PKDR  	0xa4050152UL
#define PORT_PLDR  	0xa4050154UL
#define PORT_PMDR  	0xa4050156UL
#define PORT_PPDR  	0xa4050158UL
#define PORT_PRDR  	0xa405015aUL
#define PORT_PSDR  	0xa405015cUL
#define PORT_PTDR  	0xa405015eUL
#define PORT_PUDR  	0xa4050160UL
#define PORT_PVDR  	0xa4050162UL

#define ERI_IRQ		23
#define RXI_IRQ		24
#define TXI_IRQ		25
#define TEND_IRQ	26
#define SMI_IPR_ADDR	INTC_IPRB
#define SMI_IPR_POS	1
#define SMI_PRIORITY	3
#define ITI_IRQ		27
#define WDT_IPR_ADDR	INTC_IPRB
#define WDT_IPR_POS	3
#define WDT_PRIORITY	3

#define IRQ0_IRQ	32
#if 0
#define IRQ1_IRQ	33
#define IRQ2_IRQ	34
#define IRQ3_IRQ	35
#endif
#define IRQ4_IRQ	36
#define IRQ5_IRQ	37

#define IRQ0_IPR_ADDR	INTC_IPRC
#define IRQ1_IPR_ADDR	INTC_IPRC
#define IRQ2_IPR_ADDR	INTC_IPRC
#define IRQ3_IPR_ADDR	INTC_IPRC
#define IRQ4_IPR_ADDR	INTC_IPRD
#define IRQ5_IPR_ADDR	INTC_IPRD

#define IRQ0_IPR_POS	0
#define IRQ1_IPR_POS	1
#define IRQ2_IPR_POS	2
#define IRQ3_IPR_POS	3
#define IRQ4_IPR_POS	0
#define IRQ5_IPR_POS	1

#define IRQ0_PRIORITY	1
#define IRQ1_PRIORITY	1
#define IRQ2_PRIORITY	1
#define IRQ3_PRIORITY	1
#define IRQ4_PRIORITY	1
#define IRQ5_PRIORITY	1

#define TMU_SUNI_IRQ	38
#define TMU_SUNI_ADDR	INTC_IPRD
#define TMU_SUNI_POS	2
#define TMU_SUNI_PRIORITY	3

#define USBF_SPD	39
#define USBF_SPD_IPR_ADDR	INTC_IPRD
#define USBF_SPD_IPR_POS	3
#define USBF_SPD_PRIORITY	3

#define LCDC_IRQ	56
#define LCDC_IPR_ADDR	INTC_IPRE
#define LCDC_IPR_POS	1
#define LCDC_PRIORITY	3

#define SSLI_IRQ	60
#define SSLI_IPR_ADDR	INTC_IPRE
#define SSLI_IPR_POS	0
#define SSLI_PRIORITY	3

#define USBF0_IRQ	65
#define USBF1_IRQ	66
#define USBH_IRQ	67
#define USBF_IPR_ADDR	INTC_IPRF
#define USBH_IPR_ADDR	INTC_IPRJ
#define USBF_IPR_POS	1
#define USBH_IPR_POS	2
#define USBF_PRIORITY	3
#define USBH_PRIORITY	3

#define ADC_IRQ		79
#define ADC_IPR_ADDR	INTC_IPRF
#define ADC_IPR_POS	3
#define ADC_PRIORITY	3

#define PINTA_IRQ	84
#define PINTB_IRQ	85
#define PINTA_IPR_ADDR	INTC_IPRH
#define PINTB_IPR_ADDR	INTC_IPRH
#define PINTA_IPR_POS	3
#define PINTB_IPR_POS	2
#define PINTA_PRIORITY	2
#define PINTB_PRIORITY	2

#define SIOF0_IRQ	88
#define SIOF1_IRQ	89
#define SIOF0_IPR_ADDR	INTC_IPRI
#define SIOF1_IPR_ADDR	INTC_IPRI
#define SIOF0_IPR_POS	3
#define SIOF1_IPR_POS	2
#define SIOF0_PRIORITY	3
#define SIOF1_PRIORITY	3

#define TPU0_IRQ	92
#define TPU1_IRQ	93
#define TPU2_IRQ	94
#define TPU3_IRQ	95
#define TPU_IPR_ADDR	INTC_IPRH
#define TPU_IPR_POS	1
#define TPU_PRIORITY	3

#define IIC_IRQ		96
#define IIC_IPR_ADDR	INTC_IPRH
#define IIC_IPR_POS	0
#define IIC_PRIORITY	3

#define MMC0_IRQ	100
#define MMC1_IRQ	101
#define MMC2_IRQ	102
#define MMC3_IRQ	103
#define MMC_IPR_ADDR	INTC_IPRI
#define MMC_IPR_POS	1
#define MMC_PRIORITY	3

#define CMT_IRQ		104
#define CMT_IPR_ADDR	INTC_IPRF
#define CMT_IPR_POS	0
#define CMT_PRIORITY	3

#define PCC_IRQ		107
#define PCC_IPR_ADDR	INTC_IPRI
#define PCC_IPR_POS	0
#define PCC_PRIORITY	3

#define AFEC_IRQ	111
#define AFEC_IPR_ADDR	INTC_IPRJ
#define AFEC_IPR_POS	0
#define AFEC_PRIORITY	3

extern int ipr_irq_demux(int irq);
#define __irq_demux(irq) ipr_irq_demux(irq)

#elif defined(CONFIG_CPU_SUBTYPE_SH7707) || \
      defined(CONFIG_CPU_SUBTYPE_SH7709) || \
      defined(CONFIG_CPU_SUBTYPE_SH7727)
#define INTC_IRR0	0xa4000004UL
#define INTC_IRR1	0xa4000006UL
#define INTC_IRR2	0xa4000008UL
#if defined(CONFIG_CPU_SUBTYPE_SH7727)
#define INTC_IRR3	0xa4000224UL
#define INTC_IRR4	0xa4000226UL
#endif

#define INTC_ICR0  	0xfffffee0UL
#define INTC_ICR1  	0xa4000010UL
#define INTC_ICR2  	0xa4000012UL
#if defined(CONFIG_CPU_SUBTYPE_SH7727)
#define INTC_ICR3  	0xa4000228UL
#endif
#define INTC_INTER 	0xa4000014UL

#define INTC_IPRC  	0xa4000016UL
#define INTC_IPRD  	0xa4000018UL
#define INTC_IPRE  	0xa400001aUL
#if defined(CONFIG_CPU_SUBTYPE_SH7707)
#define INTC_IPRF	0xa400001cUL
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7727)
#define INTC_IPRF	0xa4000220UL
#define INTC_IPRG	0xa4000222UL
#endif

#define PORT_PACR	0xa4000100UL
#define PORT_PBCR	0xa4000102UL
#define PORT_PCCR	0xa4000104UL
#define PORT_PDCR	0xa4000106UL
#define PORT_PECR	0xa4000108UL
#define PORT_PFCR	0xa400010aUL
#define PORT_PGCR	0xa400010cUL
#define PORT_PHCR	0xa400010eUL
#define PORT_PJCR	0xa4000110UL
#define PORT_PKCR	0xa4000112UL
#define PORT_PLCR	0xa4000114UL
#define PORT_PMCR	0xa4000118UL

#define PORT_PADR  	0xa4000120UL
#define PORT_PBDR  	0xa4000122UL
#define PORT_PCDR  	0xa4000124UL
#define PORT_PDDR  	0xa4000126UL
#define PORT_PEDR  	0xa4000128UL
#define PORT_PFDR  	0xa400012aUL
#define PORT_PGDR  	0xa400012cUL
#define PORT_PHDR  	0xa400012eUL
#define PORT_PJDR  	0xa4000130UL
#define PORT_PKDR  	0xa4000132UL
#define PORT_PLDR  	0xa4000124UL
#define PORT_PMDR  	0xa4000138UL
 
#define IRQ0_IRQ	32
#define IRQ1_IRQ	33
#define IRQ2_IRQ	34
#define IRQ3_IRQ	35
#define IRQ4_IRQ	36
#define IRQ5_IRQ	37

#define IRQ0_IPR_ADDR	INTC_IPRC
#define IRQ1_IPR_ADDR	INTC_IPRC
#define IRQ2_IPR_ADDR	INTC_IPRC
#define IRQ3_IPR_ADDR	INTC_IPRC
#define IRQ4_IPR_ADDR	INTC_IPRD
#define IRQ5_IPR_ADDR	INTC_IPRD

#define IRQ0_IPR_POS	0
#define IRQ1_IPR_POS	1
#define IRQ2_IPR_POS	2
#define IRQ3_IPR_POS	3
#define IRQ4_IPR_POS	0
#define IRQ5_IPR_POS	1

#define IRQ0_PRIORITY	1
#define IRQ1_PRIORITY	1
#define IRQ2_PRIORITY	1
#define IRQ3_PRIORITY	1
#define IRQ4_PRIORITY	1
#define IRQ5_PRIORITY	1

#define PINT0_IRQ	40
#define PINT8_IRQ	41

#define PINT0_IPR_ADDR	INTC_IPRD
#define PINT8_IPR_ADDR	INTC_IPRD

#define PINT0_IPR_POS	3
#define PINT8_IPR_POS	2
#define PINT0_PRIORITY	2
#define PINT8_PRIORITY	2

extern int ipr_irq_demux(int irq);
#define __irq_demux(irq) ipr_irq_demux(irq)

#elif defined(CONFIG_CPU_SUBTYPE_SH7750) || \
    defined(CONFIG_CPU_SUBTYPE_SH7751) || \
    defined(CONFIG_CPU_SUBTYPE_SH7750R) || \
    defined(CONFIG_CPU_SUBTYPE_SH7751R) || \
    defined(CONFIG_CPU_SUBTYPE_SH7760) || \
    defined(CONFIG_CPU_SUBTYPE_ST40) || \
    defined(CONFIG_CPU_SUBTYPE_SH4_202)

#define INTC_ICR        0xffd00000
#define INTC_ICR_NMIL   (1<<15)
#define INTC_ICR_MAI    (1<<14)
#define INTC_ICR_NMIB   (1<<9)
#define INTC_ICR_NMIE   (1<<8)
#define INTC_ICR_IRLM   (1<<7)

#define PORT_PACR     0xfe400000
#define PORT_PBCR     0xfe400004
#define PORT_PCCR     0xfe400008
#define PORT_PDCR     0xfe40000c
#define PORT_PECR     0xfe400010
#define PORT_PFCR     0xfe400014
#define PORT_PGCR     0xfe400018
#define PORT_PHCR     0xfe40001c
#define PORT_PJCR     0xfe400020
#define PORT_PKCR     0xfe400024

#define PORT_INPUPA   0xfe400028
#define PORT_DMAPCR   0xfe40002c
#define PORT_SCIHZR   0xfe400030
#define PORT_IPSELR   0xfe400034
#define PORT_GPIOIC   0xff800048
#define PORT_PAPUPR   0xfe400080
#define PORT_PBPUPR   0xfe400084
#define PORT_PCPUPR   0xfe400088
#define PORT_PDPUPR   0xfe40008c
#define PORT_PEPUPR   0xfe400090
#define PORT_PFPUPR   0xfe400094
#define PORT_PGPUPR   0xfe400098
#define PORT_PHPUPR   0xfe40009c
#define PORT_PJPUPR   0xfe4000a0
#define PORT_PKPUPR   0xfe4000a4
#define PORT_MDPUPR   0xfe4000a8
#define PORT_MODSELR  0xfe4000ac
#define PORT_PADR     0xfe400040
#define PORT_PBDR     0xfe400044
#define PORT_PCDR     0xfe400048
#define PORT_PDDR     0xfe40004c
#define PORT_PEDR     0xfe400050
#define PORT_PFDR     0xfe400054
#define PORT_PGDR     0xfe400058
#define PORT_PHDR     0xfe40005c
#define PORT_PJDR     0xfe400060
#define PORT_PKDR     0xfe400064

#define __irq_demux(irq) irq

#else
#define __irq_demux(irq) irq
#endif /* CONFIG_CPU_SUBTYPE_SH7707 || CONFIG_CPU_SUBTYPE_SH7709 || CONFIG_CPU_SUBTYPE_SH7727 */

#ifdef CONFIG_CPU_SUBTYPE_ST40STB1
#define INTC2_FIRST_IRQ 64
#define NR_INTC2_IRQS 25
#elif defined(CONFIG_CPU_SUBTYPE_SH7760)
#define INTC2_FIRST_IRQ 48
#define NR_INTC2_IRQS 64
#endif
 
#define INTC2_BASE0 0xfe080000
#define INTC2_INTC2MODE  (INTC2_BASE0+0x80)
 
#define INTC2_INTPRI_OFFSET	0x00
#define INTC2_INTREQ_OFFSET	0x20
#define INTC2_INTMSK_OFFSET	0x40
#define INTC2_INTMSKCLR_OFFSET	0x60
 
extern void make_intc2_irq(unsigned int irq,unsigned int addr,
                           unsigned int group,int pos,int priority);
 
void init_IRQ_intc2(void);
void intc2_add_clear_irq(int irq, int (*fn)(int));

#ifdef CONFIG_SH_GENERIC

static __inline__ int irq_demux(int irq)
{
	if (sh_mv.mv_irq_demux) {
		irq = sh_mv.mv_irq_demux(irq);
	}
	return __irq_demux(irq);
}

#elif defined(CONFIG_SH_BIGSUR)

extern int bigsur_irq_demux(int irq);
#define irq_demux(irq) bigsur_irq_demux(irq)

#elif defined(CONFIG_HD64461)

extern int hd64461_irq_demux(int irq);
#define irq_demux(irq) hd64461_irq_demux(irq)

#elif defined(CONFIG_HD64465)

extern int hd64465_irq_demux(int irq);
#define irq_demux(irq) hd64465_irq_demux(irq)

#elif defined(CONFIG_SH_EC3104)

extern int ec3104_irq_demux(int irq);
#define irq_demux ec3104_irq_demux

#elif defined(CONFIG_SH_CAT68701)

extern int cat68701_irq_demux(int irq);
#define irq_demux cat68701_irq_demux

#elif defined(CONFIG_SH_DREAMCAST)

extern int systemasic_irq_demux(int irq);
#define irq_demux systemasic_irq_demux

#elif defined(CONFIG_SH_RTS7751R2D)

extern int rts7751r2d_irq_demux(int irq);
#define irq_demux rts7751r2d_irq_demux

#elif defined(CONFIG_SH_MS7290CP)

extern int ms7290cp_irq_demux(int);
#define irq_demux ms7290cp_irq_demux

#elif defined(CONFIG_SH_MS7710SE)

extern int ms7710se_irq_demux(int);
#define irq_demux ms7710se_irq_demux

#elif defined(CONFIG_SH_MS7720RP)

extern int ms7720rp_irq_demux(int);
#define irq_demux ms7720rp_irq_demux

#else

#define irq_demux(irq) __irq_demux(irq)

#endif



#endif /* __ASM_SH_IRQ_H */
