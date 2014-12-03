/*
 *  linux/arch/arm/mach-at91rm9200dk/arch.c
 *
 *  Copyright (C) 2002 ATMEL Rousset
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/mm.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/irq.h>

#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>


static void AT91F_mask_irq(unsigned int irq)
{
	__raw_writel(1 << irq, &(((AT91PS_SYS)AT91C_VA_BASE_SYS)->AIC_IDCR) );
}

static void AT91F_unmask_irq(unsigned int irq)
{
	__raw_writel(1 << irq,  &(((AT91PS_SYS)AT91C_VA_BASE_SYS)->AIC_IECR) );
}

void __init at91rm9200dk_init_irq(void)
{
	unsigned int i;

#if (NR_IRQS < 32)
#error AT91RM9200 IRQ number < 32
#endif

 	// Init the irq_desc structure
	for (i = 0; i < NR_IRQS; i++) {
		// Create an IRQ descriptor for each AIC entry
		// Init each SMR AIC entry
		((AT91PS_SYS)AT91C_VA_BASE_SYS)->AIC_SVR[i] = i;
		((AT91PS_SYS)AT91C_VA_BASE_SYS)->AIC_SMR[i] = AT91C_AIC_PRIOR_LOWEST | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE;
		irq_desc[i].valid	= 1;
		irq_desc[i].probe_ok	= 1;
		irq_desc[i].mask_ack	= AT91F_mask_irq;
		irq_desc[i].mask	= AT91F_mask_irq;
		irq_desc[i].unmask	= AT91F_unmask_irq;

		// Perform 8 End Of Interrupt Command to make sure AIC will not Lock out nIRQ
		if (i < 8)
			((AT91PS_SYS)AT91C_VA_BASE_SYS)->AIC_EOICR = ((AT91PS_SYS)AT91C_VA_BASE_SYS)->AIC_EOICR;
	}

	// Disable all interrupts
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->AIC_IDCR = -1;
	((AT91PS_SYS)AT91C_VA_BASE_SYS)->AIC_ICCR = -1;
}

static struct map_desc at91rm9200dk_io_desc[] __initdata = {
	/* virtual                             physical                        length       domain     r  w  c  b */
	{  (unsigned long)AT91C_VA_BASE_SYS,   (unsigned long)AT91C_BASE_SYS,   SZ_4K      , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_SPI,   (unsigned long)AT91C_BASE_SPI,   SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_SSC2,  (unsigned long)AT91C_BASE_SSC2,  SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_SSC1,  (unsigned long)AT91C_BASE_SSC1,  SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_SSC0,  (unsigned long)AT91C_BASE_SSC0,  SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_US3,   (unsigned long)AT91C_BASE_US3,   SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_US2,   (unsigned long)AT91C_BASE_US2,   SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_US1,   (unsigned long)AT91C_BASE_US1,   SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_US0,   (unsigned long)AT91C_BASE_US0,   SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_EMAC,  (unsigned long)AT91C_BASE_EMAC,  SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_TWI,   (unsigned long)AT91C_BASE_TWI,   SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_MCI,   (unsigned long)AT91C_BASE_MCI,   SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_UDP,   (unsigned long)AT91C_BASE_UDP,   SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_TCB0,  (unsigned long)AT91C_BASE_TCB0,  SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_TCB1,  (unsigned long)AT91C_BASE_TCB1,  SZ_16K     , DOMAIN_IO, 0,1, 0, 0},
	{  (unsigned long)AT91C_VA_BASE_DMA_EMAC,  (unsigned long)AT91C_BASE_DMA_EMAC,  SZ_1M , DOMAIN_IO, 0,1, 0, 0},
	LAST_DESC
};

void __init at91rm9200dk_map_io(void)
{
	iotable_init(at91rm9200dk_io_desc);
	at91rm9200dk_register_us1();
	at91rm9200dk_register_dbgu();
}

#define AT91C_ZIMAGE_SIZE 3*1024*1024 // Size of the compressed ramdisk

static void __init
at91rm9200dk_fixup(struct machine_desc *desc, struct param_struct *unused,
		 char **cmdline, struct meminfo *mi)
{
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	setup_ramdisk( 1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE );
	setup_initrd( 0xc1000000, AT91C_ZIMAGE_SIZE );
}

MACHINE_START(AT91RM9200DK, "AT91RM9200DK")
	MAINTAINER("ATMEL")
	BOOT_MEM(AT91C_BASE_SDRAM, (unsigned long)AT91C_BASE_SYS, (unsigned long)AT91C_VA_BASE_SYS)
	BOOT_PARAMS(0x00000000)
	FIXUP(at91rm9200dk_fixup)
	MAPIO(at91rm9200dk_map_io)
	INITIRQ(at91rm9200dk_init_irq)
MACHINE_END
