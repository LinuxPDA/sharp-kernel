/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/include/asm-arm/arch-dbmx1/tpm102.h
 *
 */

#ifndef __DBMX1_TPM102_H__
#define __DBMX1_TPM102_H__

#define APLAT_FLASH_OFFSET	0x00000000
#define APLAT_PCMCIA_REG_OFFSET	0x05000000
#define APLAT_USB_OFFSET	0x05001000
#define APLAT_FPGA_OFFSET	0x05002000
#define APLAT_PCMCIA_MEM_OFFSET	0x06000000

#define APLAT_FLASH_BASE	(APLAT_FLASH_OFFSET + DBMX1_IO2_BASE)
#define APLAT_LCDC_REG_BASE	(APLAT_LCDC_REG_OFFSET + DBMX1_IO2_BASE)
#define APLAT_LCDC_MEM_BASE	(APLAT_LCDC_MEM_OFFSET + DBMX1_IO2_BASE)
#define APLAT_PCMCIA_REG_BASE	(APLAT_PCMCIA_REG_OFFSET + DBMX1_IO2_BASE)
#define APLAT_USB_BASE		(APLAT_USB_OFFSET + DBMX1_IO2_BASE)
#define APLAT_FPGA_BASE		(APLAT_FPGA_OFFSET + DBMX1_IO2_BASE)
#define APLAT_PCMCIA_MEM_BASE	(APLAT_PCMCIA_MEM_OFFSET + DBMX1_IO2_BASE)

#define	VA_FPGA			IO2_ADDRESS(APLAT_FPGA_BASE)
#define	APLAT_FPGA_KEY_IN	(*(volatile unsigned char *)(VA_FPGA + 0x000))
#define	APLAT_FPGA_KEY_OUT	(*(volatile unsigned char *)(VA_FPGA + 0x001))
#define	APLAT_FPGA_PCC_CTR	(*(volatile unsigned char *)(VA_FPGA + 0x002))
#define	APLAT_FPGA_SIM_CTR	(*(volatile unsigned char *)(VA_FPGA + 0x003))
#define	APLAT_FPGA_SIM_STR	(*(volatile unsigned char *)(VA_FPGA + 0x004))
#define	APLAT_FPGA_SIM_BRR	(*(volatile unsigned char *)(VA_FPGA + 0x005))
#define	APLAT_FPGA_SIM_IN	(*(volatile unsigned char *)(VA_FPGA + 0x006))
#define	APLAT_FPGA_SIM_OUT	(*(volatile unsigned char *)(VA_FPGA + 0x007))
#define	APLAT_FPGA_SPI1_CTR	(*(volatile unsigned char *)(VA_FPGA + 0x008))
#define	APLAT_FPGA_SPI1_STR	(*(volatile unsigned char *)(VA_FPGA + 0x009))
#define	APLAT_FPGA_SPI1_IN	(*(volatile unsigned char *)(VA_FPGA + 0x00a))
#define	APLAT_FPGA_SPI1_OUT	(*(volatile unsigned char *)(VA_FPGA + 0x00b))
#define	APLAT_FPGA_CDC_CTR	(*(volatile unsigned char *)(VA_FPGA + 0x00c))
#define	APLAT_FPGA_CDC_STR	(*(volatile unsigned char *)(VA_FPGA + 0x00d))
#define	APLAT_FPGA_CDC_IN_U	(*(volatile unsigned char *)(VA_FPGA + 0x00e))
#define	APLAT_FPGA_CDC_IN_L	(*(volatile unsigned char *)(VA_FPGA + 0x00f))
#define	APLAT_FPGA_CDC_OUT_UU	(*(volatile unsigned char *)(VA_FPGA + 0x010))
#define	APLAT_FPGA_CDC_OUT_UL	(*(volatile unsigned char *)(VA_FPGA + 0x011))
#define	APLAT_FPGA_CDC_OUT_LU	(*(volatile unsigned char *)(VA_FPGA + 0x012))
#define	APLAT_FPGA_CDC_OUT_LL	(*(volatile unsigned char *)(VA_FPGA + 0x013))
#define	APLAT_FPGA_IRQ_CTR1	(*(volatile unsigned char *)(VA_FPGA + 0x014))
#define	APLAT_FPGA_IRQ_CTR2	(*(volatile unsigned char *)(VA_FPGA + 0x015))
#define	APLAT_FPGA_IRQ_STR1	(*(volatile unsigned char *)(VA_FPGA + 0x016))
#define	APLAT_FPGA_IRQ_STR2	(*(volatile unsigned char *)(VA_FPGA + 0x017))
#define	APLAT_FPGA_SW_STR	(*(volatile unsigned char *)(VA_FPGA + 0x018))
#define	APLAT_FPGA_CLK_STR	(*(volatile unsigned char *)(VA_FPGA + 0x019))
#define	APLAT_FPGA_FGPO_DAT	(*(volatile unsigned char *)(VA_FPGA + 0x01a))
#define	APLAT_FPGA_SPI2_CTR	(*(volatile unsigned char *)(VA_FPGA + 0x01b))
#define	APLAT_FPGA_SPI2_STR	(*(volatile unsigned char *)(VA_FPGA + 0x01c))
#define	APLAT_FPGA_SPI2_IN	(*(volatile unsigned char *)(VA_FPGA + 0x01d))
#define	APLAT_FPGA_SPI2_OUT	(*(volatile unsigned char *)(VA_FPGA + 0x01e))
#define	APLAT_FPGA_MODEN	(*(volatile unsigned char *)(VA_FPGA + 0x01f))

#ifdef CONFIG_NET_APLAT_BASE
#define	APLAT_LAN_BASE		IO2_ADDRESS(CONFIG_NET_APLAT_BASE)
#else
#define	APLAT_LAN_BASE		0
#endif


#define	IRQ_GPIO_INT7		IRQ_GPIO_A(7)
#define	IRQ_GPIO_USB		IRQ_GPIO_A(8)
#define	IRQ_GPIO_PCMCIA_DETECT	IRQ_GPIO_A(10)
#define	IRQ_GPIO_PCMCIA_CARD	IRQ_GPIO_A(11)

#define	IRQ_INT7_START		(DBMX1_MAXIRQNUM + 1)
#define	IRQ_PSW_DN		(IRQ_INT7_START + 0)
#define	IRQ_PSW_UP		(IRQ_INT7_START + 1)
#define	IRQ_ISW_DN		(IRQ_INT7_START + 2)
#define	IRQ_ISW_UP		(IRQ_INT7_START + 3)
#define	IRQ_PEN_DN		(IRQ_INT7_START + 4)
#define	IRQ_PEN_UP		(IRQ_INT7_START + 5)
#define	IRQ_WAKE		(IRQ_INT7_START + 15)
#define	IRQ_LAN			(IRQ_INT7_START + 16)
#define	IRQ_INT7_END		(IRQ_INT7_START + 16)

#define MAXIRQNUM		IRQ_INT7_END

#ifndef __ASSEMBLY__

static inline void tpm102_cs5_16bit(void)
{
    DBMX1_EIM_CS5L = (DBMX1_EIM_CS5L & 0xfffff8ff) | 0x00000500;
}

static inline void tpm102_cs5_8bit(void)
{
    DBMX1_EIM_CS5L = (DBMX1_EIM_CS5L & 0xfffff8ff) | 0x00000300;
}

#endif

#endif
