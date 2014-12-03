/*
 *  linux/include/asm-sh/mrshpc.h
 *
 *  Copyright (c) 2003 Lineo uSolutions, Inc.
 *  Copyright (c) 2002 Lineo Japan, Inc.
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 *  Hitachi MR-SHPC-01 support.
 */
#ifndef __ASM_SH_MRSHPC_H
#define __ASM_SH_MRSHPC_H

#if defined(CONFIG_SH_MS7727RP)
#include <asm/ms7727rp.h>
#elif defined(CONFIG_SH_MS7290CP)
#include <asm/ms7290cp.h>
#elif defined(CONFIG_SH_MS7710SE)
#include <asm/ms7710se.h>
#elif defined(CONFIG_SH_MS7720RP)
#include <asm/ms7720rp.h>
#elif defined(CONFIG_SH_SOLUTION_ENGINE)
#include <asm/hitachi_se.h>
#define MRSHPC_CTRL_BASE	PA_MRSHPC
#define MRSHPC_MEM_BASE		PA_MRSHPC_MW1
#define MRSHPC_ATTR_BASE	PA_MRSHPC_MW2
#define MRSHPC_IO_BASE	        PA_MRSHPC_IO
#define MRSHPC_IRQ		7
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)
#include <asm/ms7760cp.h>
#define MRSHPC_CTRL_BASE        PA_MRSHPC
#define MRSHPC_MEM_BASE         PA_MRSHPC_MW1
#define MRSHPC_ATTR_BASE        PA_MRSHPC_MW2
#define MRSHPC_IO_BASE          PA_MRSHPC_IO
#define MRSHPC_IRQ              10
#endif

#undef MRSHPC_MODE
#undef MRSHPC_OPTION
#undef MRSHPC_CSR
#undef MRSHPC_ISR
#undef MRSHPC_ICR
#undef MRSHPC_CPWCR
#undef MRSHPC_MW0CR1
#undef MRSHPC_MW1CR1
#undef MRSHPC_IOWCR1
#undef MRSHPC_MW0CR2
#undef MRSHPC_MW1CR2
#undef MRSHPC_IOWCR2
#undef MRSHPC_CDCR
#undef MRSHPC_INFO

#define MRSHPC_MODE		(MRSHPC_CTRL_BASE + 0x0004)
#define MRSHPC_OPTION		(MRSHPC_CTRL_BASE + 0x0006)
#define MRSHPC_CSR		(MRSHPC_CTRL_BASE + 0x0008)
#define MRSHPC_ISR		(MRSHPC_CTRL_BASE + 0x000a)
#define MRSHPC_ICR		(MRSHPC_CTRL_BASE + 0x000c)
#define MRSHPC_CPWCR		(MRSHPC_CTRL_BASE + 0x000e)
#define MRSHPC_MW0CR1		(MRSHPC_CTRL_BASE + 0x0010)
#define MRSHPC_MW1CR1		(MRSHPC_CTRL_BASE + 0x0012)
#define MRSHPC_IOWCR1		(MRSHPC_CTRL_BASE + 0x0014)
#define MRSHPC_MW0CR2		(MRSHPC_CTRL_BASE + 0x0016)
#define MRSHPC_MW1CR2		(MRSHPC_CTRL_BASE + 0x0018)
#define MRSHPC_IOWCR2		(MRSHPC_CTRL_BASE + 0x001a)
#define MRSHPC_CDCR		(MRSHPC_CTRL_BASE + 0x001c)
#define MRSHPC_INFO		(MRSHPC_CTRL_BASE + 0x001e)

/* MRSHPC: Mode Register */
#define MRSHPC_MODE_MODE66		(1 << 4)

/* MRSHPC: Option Control Register */
#define MRSHPC_OPTION_TEST_MASK		0x0f00
#define MRSHPC_OPTION_SOFT_SPKR		(1 << 3)
#define MRSHPC_OPTION_SOFT_LED		(1 << 2)
#define MRSHPC_OPTION_SPKR_SEL		(1 << 1)
#define MRSHPC_OPTION_LED_SEL		(1 << 0)

/* MRSHPC: Card Status Register */
#define MRSHPC_CSR_ENDIAN		(1 << 14)
#define MRSHPC_CSR_RA2N			(0x000f << 10)
#define MRSHPC_CSR_PCIC_RDY_BSY		(1 << 9)
#define MRSHPC_CSR_VS2			(1 << 8)
#define MRSHPC_CSR_VS1			(1 << 7)
#define MRSHPC_CSR_PWON			(1 << 6)
#define MRSHPC_CSR_RDY_BSY		(1 << 5)
#define MRSHPC_CSR_WPS			(1 << 4)
#define MRSHPC_CSR_CD2			(1 << 3)
#define MRSHPC_CSR_CD1			(1 << 2)
#define MRSHPC_CSR_BVD2			(1 << 1)
#define MRSHPC_CSR_BVD1			(1 << 0)

/* MRSHPC: Interrupt Status Register */
#define MRSHPC_ISR_STSCHG_RI		(1 << 6)
#define MRSHPC_ISR_IREQ_CHG		(1 << 5)
#define MRSHPC_ISR_CARD_PW_GOOD		(1 << 4)
#define MRSHPC_ISR_CARD_DET		(1 << 3)
#define MRSHPC_ISR_RDY_CHG		(1 << 2)
#define MRSHPC_ISR_BAT_WARN		(1 << 1)
#define MRSHPC_ISR_BAT_DEAD		(1 << 0)

/* MRSHPC: Interrupt Control Register */
#define MRSHPC_ICR_PLUSE_SYS_IRQ	(1 << 14)
#define MRSHPC_ICR_CARD_IRQ		(7 << 11)
#define MRSHPC_ICR_RING_IRQ		(7 << 8)
#define MRSHPC_ICR_MANAGEMENT_IRQ	(7 << 5)
#define MRSHPC_ICR_CPGOOD_ENABLE	(1 << 4)
#define MRSHPC_ICR_DETECT_ENABLE	(1 << 3)
#define MRSHPC_ICR_RDY_ENABLE		(1 << 2)
#define MRSHPC_ICR_BAT_WA_ENABLE	(1 << 1)
#define MRSHPC_ICR_BAT_DE_ENABLE	(1 << 0)

/* MRSHPC: Card Power Control Register */
#define MRSHPC_CPWCR_CARD_POW_MASK	(1 << 10)
#define MRSHPC_CPWCR_CARD_RESET		(1 << 9)
#define MRSHPC_CPWCR_POWER_DOWN		(1 << 8)
#define MRSHPC_CPWCR_SYSPEND		(1 << 7)
#define MRSHPC_CPWCR_CARD_ENABLE	(1 << 6)
#define MRSHPC_CPWCR_AUTO_POWER		(1 << 5)
#define MRSHPC_CPWCR_VCC_POWER		(1 << 4)
#define MRSHPC_CPWCR_VCC5V		(1 << 3)
#define MRSHPC_CPWCR_VCC3V		(1 << 2)
#define MRSHPC_CPWCR_VPP1		(1 << 1)
#define MRSHPC_CPWCR_VPP0		(1 << 0)

/* MRSHPC: I/O widow control register 1 */
#define MRSHPC_WCR1_WIN_ENABLE		(1 << 15)
#define MRSHPC_WCR1_WIDTH(w)		(((w) & 0x1f) << 10)
#define MRSHPC_WCR1_HOLD(h)		(((h) & 3) << 8)
#define MRSHPC_WCR1_SETUP(s)		(((s) & 3) << 6)
#define MRSHPC_WCR1_SA(a)		((a) & 0x3f)

/* MRSHPC: I/O Widow Control Register 2 */
#define MRSHPC_WCR2_SWAP		(1 << 11)
#define MRSHPC_WCR2_PROTECT		(1 << 10)
#define MRSHPC_WCR2_SIZE		(1 << 9)
#define MRSHPC_WCR2_AUTOSIZE		(1 << 8)
#define MRSHPC_WCR2_REG			(1 << 8)
#define MRSHPC_WCR2_CA(a)		((a) & 0xff)

/* MRSHPC: Card Control Register */
#define MRSHPC_CDCR_CARD_IS_IO		(1 << 3)
#define MRSHPC_CDCR_LED_ENABLE		(1 << 2)
#define MRSHPC_CDCR_SPKR_ENABLE		(1 << 1)
#define MRSHPC_CDCR_INPACK_ENABLE	(1 << 0)

#endif /* __ASM_SH_MRSHPC_H */
