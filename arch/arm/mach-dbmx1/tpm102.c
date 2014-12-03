/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/arch/arm/mach-db1mx1/tpm102.c
 *
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/pgtable.h>
#include <asm/page.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

extern void dbmx1_init_irq(void);

#define SET_BANK(__nr, __start, __size) \
	mi->bank[__nr].start = (__start), \
	mi->bank[__nr].size  = (__size), \
	mi->bank[__nr].node  = (((unsigned)(__start) - PHYS_OFFSET) >> 27)

static void __init tpm102_hw_init(void)
{
#if 0
    printk("CSCR   : %08x\n", DBMX1_PLL_CSCR);
    printk("PCDR   : %08x\n", DBMX1_PLL_PCDR);
    printk("MPCTL0 : %08x\n", DBMX1_PLL_MPCTL0);
    printk("MPCTL1 : %08x\n", DBMX1_PLL_MPCTL1);
    printk("SPCTL0 : %08x\n", DBMX1_PLL_SPCTL0);
    printk("SPCTL1 : %08x\n", DBMX1_PLL_SPCTL1);
#endif
    // PLL initialize
    DBMX1_PLL_CSCR   = 0x2f008003;
    DBMX1_PLL_PCDR   = 0x000b003b;
    DBMX1_PLL_MPCTL0 = 0x003f1437;
    DBMX1_PLL_MPCTL1 = 0x00000000;
    DBMX1_PLL_SPCTL0 = 0x043f1437;
    DBMX1_PLL_SPCTL1 = 0x00008000;

#if 0
    printk("DDIR_A    : %08x\n", *(volatile u32 *)0xf021c000);
    printk("DDIR_B    : %08x\n", *(volatile u32 *)0xf021c100);
    printk("DDIR_C    : %08x\n", *(volatile u32 *)0xf021c200);
    printk("DDIR_D    : %08x\n", *(volatile u32 *)0xf021c300);
    printk("OCR1_A    : %08x\n", *(volatile u32 *)0xf021c004);
    printk("OCR1_B    : %08x\n", *(volatile u32 *)0xf021c104);
    printk("OCR1_C    : %08x\n", *(volatile u32 *)0xf021c204);
    printk("OCR1_D    : %08x\n", *(volatile u32 *)0xf021c304);
    printk("OCR2_A    : %08x\n", *(volatile u32 *)0xf021c008);
    printk("OCR2_B    : %08x\n", *(volatile u32 *)0xf021c108);
    printk("OCR2_C    : %08x\n", *(volatile u32 *)0xf021c208);
    printk("OCR2_D    : %08x\n", *(volatile u32 *)0xf021c308);
    printk("ICONFA1_A : %08x\n", *(volatile u32 *)0xf021c00c);
    printk("ICONFA1_B : %08x\n", *(volatile u32 *)0xf021c10c);
    printk("ICONFA1_C : %08x\n", *(volatile u32 *)0xf021c20c);
    printk("ICONFA1_D : %08x\n", *(volatile u32 *)0xf021c30c);
    printk("ICONFA2_A : %08x\n", *(volatile u32 *)0xf021c010);
    printk("ICONFA2_B : %08x\n", *(volatile u32 *)0xf021c110);
    printk("ICONFA2_C : %08x\n", *(volatile u32 *)0xf021c210);
    printk("ICONFA2_D : %08x\n", *(volatile u32 *)0xf021c310);
    printk("ICONFB1_A : %08x\n", *(volatile u32 *)0xf021c014);
    printk("ICONFB1_B : %08x\n", *(volatile u32 *)0xf021c114);
    printk("ICONFB1_C : %08x\n", *(volatile u32 *)0xf021c214);
    printk("ICONFB1_D : %08x\n", *(volatile u32 *)0xf021c314);
    printk("ICONFB2_A : %08x\n", *(volatile u32 *)0xf021c018);
    printk("ICONFB2_B : %08x\n", *(volatile u32 *)0xf021c118);
    printk("ICONFB2_C : %08x\n", *(volatile u32 *)0xf021c218);
    printk("ICONFB2_D : %08x\n", *(volatile u32 *)0xf021c318);
    printk("DR_A      : %08x\n", *(volatile u32 *)0xf021c01c);
    printk("DR_B      : %08x\n", *(volatile u32 *)0xf021c11c);
    printk("DR_C      : %08x\n", *(volatile u32 *)0xf021c21c);
    printk("DR_D      : %08x\n", *(volatile u32 *)0xf021c31c);
    printk("GIUS_A    : %08x\n", *(volatile u32 *)0xf021c020);
    printk("GIUS_B    : %08x\n", *(volatile u32 *)0xf021c120);
    printk("GIUS_C    : %08x\n", *(volatile u32 *)0xf021c220);
    printk("GIUS_D    : %08x\n", *(volatile u32 *)0xf021c320);
    printk("SSR_A     : %08x\n", *(volatile u32 *)0xf021c024);
    printk("SSR_B     : %08x\n", *(volatile u32 *)0xf021c124);
    printk("SSR_C     : %08x\n", *(volatile u32 *)0xf021c224);
    printk("SSR_D     : %08x\n", *(volatile u32 *)0xf021c324);
    printk("ICR1_A    : %08x\n", *(volatile u32 *)0xf021c028);
    printk("ICR1_B    : %08x\n", *(volatile u32 *)0xf021c128);
    printk("ICR1_C    : %08x\n", *(volatile u32 *)0xf021c228);
    printk("ICR1_D    : %08x\n", *(volatile u32 *)0xf021c328);
    printk("ICR2_A    : %08x\n", *(volatile u32 *)0xf021c02c);
    printk("ICR2_B    : %08x\n", *(volatile u32 *)0xf021c12c);
    printk("ICR2_C    : %08x\n", *(volatile u32 *)0xf021c22c);
    printk("ICR2_D    : %08x\n", *(volatile u32 *)0xf021c32c);
    printk("IMR_A     : %08x\n", *(volatile u32 *)0xf021c030);
    printk("IMR_B     : %08x\n", *(volatile u32 *)0xf021c130);
    printk("IMR_C     : %08x\n", *(volatile u32 *)0xf021c230);
    printk("IMR_D     : %08x\n", *(volatile u32 *)0xf021c330);
    printk("GPR_A     : %08x\n", *(volatile u32 *)0xf021c038);
    printk("GPR_B     : %08x\n", *(volatile u32 *)0xf021c138);
    printk("GPR_C     : %08x\n", *(volatile u32 *)0xf021c238);
    printk("GPR_D     : %08x\n", *(volatile u32 *)0xf021c338);
#endif
    // GPIO initialize
    DBMX1_GPIO_GIUS_A    = 0x0001fffa;
    DBMX1_GPIO_GIUS_B    = 0x0ff03fff;
    DBMX1_GPIO_GIUS_C    = 0x0004201f;
    DBMX1_GPIO_GIUS_D    = 0x00000fbf;
    DBMX1_GPIO_OCR1_A    = 0x00003fc0;
    DBMX1_GPIO_OCR1_B    = 0x00000000;
    DBMX1_GPIO_OCR1_C    = 0x00000000;
    DBMX1_GPIO_OCR1_D    = 0x00000000;
    DBMX1_GPIO_OCR2_A    = 0x00000000;
    DBMX1_GPIO_OCR2_B    = 0x0003ff00;
    DBMX1_GPIO_OCR2_C    = 0x00000000;
    DBMX1_GPIO_OCR2_D    = 0x00000000;
    DBMX1_GPIO_DR_A      = 0x00000040;
    DBMX1_GPIO_DR_B      = 0x01e00000;
    DBMX1_GPIO_DR_C      = 0x00000000;
    DBMX1_GPIO_DR_D      = 0x00000000;
    DBMX1_GPIO_DDIR_A    = 0x00000078;
    DBMX1_GPIO_DDIR_B    = 0x01f00000;
    DBMX1_GPIO_DDIR_C    = 0x00000000;
    DBMX1_GPIO_DDIR_D    = 0x00001f80;
    DBMX1_GPIO_ICONFA1_A = 0xfffffff3;
    DBMX1_GPIO_ICONFA1_B = 0xffffffff;
    DBMX1_GPIO_ICONFA1_C = 0xffffffff;
    DBMX1_GPIO_ICONFA1_D = 0xffffffff;
    DBMX1_GPIO_ICONFA2_A = 0xffffffff;
    DBMX1_GPIO_ICONFA2_B = 0xffffffff;
    DBMX1_GPIO_ICONFA2_C = 0xffffffff;
    DBMX1_GPIO_ICONFA2_D = 0xffffffff;
    DBMX1_GPIO_ICONFB1_A = 0xffffffff;
    DBMX1_GPIO_ICONFB1_B = 0xffffffff;
    DBMX1_GPIO_ICONFB1_C = 0xffffffff;
    DBMX1_GPIO_ICONFB1_D = 0xffffffff;
    DBMX1_GPIO_ICONFB2_A = 0xffffffff;
    DBMX1_GPIO_ICONFB2_B = 0xffffffff;
    DBMX1_GPIO_ICONFB2_C = 0xffffffff;
    DBMX1_GPIO_ICONFB2_D = 0xffffffff;
#if 0
    DBMX1_GPIO_SSR_A     = 0x00dbfe42;
    DBMX1_GPIO_SSR_B     = 0xffe0b700;
    DBMX1_GPIO_SSR_C     = 0xf970be18;
    DBMX1_GPIO_SSR_D     = 0x80000180;
#endif
    DBMX1_GPIO_ICR1_A    = 0x00000000;
    DBMX1_GPIO_ICR1_B    = 0x00000000;
    DBMX1_GPIO_ICR1_C    = 0x00000000;
    DBMX1_GPIO_ICR1_D    = 0x00000000;
    DBMX1_GPIO_ICR2_A    = 0x00000000;
    DBMX1_GPIO_ICR2_B    = 0x00000000;
    DBMX1_GPIO_ICR2_C    = 0x00000000;
    DBMX1_GPIO_ICR2_D    = 0x00000000;
    DBMX1_GPIO_IMR_A     = 0x00000000;
    DBMX1_GPIO_IMR_B     = 0x00000000;
    DBMX1_GPIO_IMR_C     = 0x00000000;
    DBMX1_GPIO_IMR_D     = 0x00000000;
    DBMX1_GPIO_GPR_A     = 0x00000000;
    DBMX1_GPIO_GPR_B     = 0x00000000;
    DBMX1_GPIO_GPR_C     = 0x00000000;
    DBMX1_GPIO_GPR_D     = 0x00000000;

#if 0
    printk("FMCR : %08x\n", DBMX1_SYSCTRL_FMCR);
    printk("GCCR : %08x\n", DBMX1_SYSCTRL_GCCR);
#endif
    // SYSCTRL initialize
    DBMX1_SYSCTRL_FMCR = 0x00000001;
    DBMX1_SYSCTRL_GCCR = 0x00000000;

#if 0
    printk("CS0U : %08x\n", DBMX1_EIM_CS0U);
    printk("CS0L : %08x\n", DBMX1_EIM_CS0L);
    printk("CS1U : %08x\n", DBMX1_EIM_CS1U);
    printk("CS1L : %08x\n", DBMX1_EIM_CS1L);
    printk("CS2U : %08x\n", DBMX1_EIM_CS2U);
    printk("CS2L : %08x\n", DBMX1_EIM_CS2L);
    printk("CS3U : %08x\n", DBMX1_EIM_CS3U);
    printk("CS3L : %08x\n", DBMX1_EIM_CS3L);
    printk("CS4U : %08x\n", DBMX1_EIM_CS4U);
    printk("CS4L : %08x\n", DBMX1_EIM_CS4L);
    printk("CS5U : %08x\n", DBMX1_EIM_CS5U);
    printk("CS5L : %08x\n", DBMX1_EIM_CS5L);
    printk("EIM  : %08x\n", DBMX1_EIM_EIM);
#endif
    // EIM initialize
    DBMX1_EIM_CS0U = 0x00000a00;
    DBMX1_EIM_CS0L = 0x00000e01;
    DBMX1_EIM_CS1U = 0x00000000;
    DBMX1_EIM_CS1L = 0x00000802;
    DBMX1_EIM_CS2U = 0x00000000;
    DBMX1_EIM_CS2L = 0x00000802;
    DBMX1_EIM_CS3U = 0x00001420;
    DBMX1_EIM_CS3L = 0x46460503;
    DBMX1_EIM_CS4U = 0x00001420;
    DBMX1_EIM_CS4L = 0x46460b03;
    DBMX1_EIM_CS5U = 0x80003f00;
    DBMX1_EIM_CS5L = 0x46460503;
    DBMX1_EIM_EIM  = 0x00000000;

    APLAT_FPGA_CLK_STR |= 0x0f;
}

static void __init tpm102_fixup(struct machine_desc *desc,
				struct param_struct *params,
				char **cmdline, struct meminfo *mi)
{
    SET_BANK(0, PHYS_OFFSET, 64*1024*1024);
    mi->nr_banks = 1;

#ifdef CONFIG_BLK_DEV_INITRD
    ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
    setup_ramdisk(1,  0, 0, 8192);
    setup_initrd(0xc0500000, 3*1024*1024);
#endif
#if 0
    ROOT_DEV = MKDEV(31, 2);	/* /dev/mtdblock2 */
    if (params->u1.s.page_size != PAGE_SIZE) {
	params->u1.s.page_size = PAGE_SIZE;
	params->u1.s.nr_pages = 64 * 1024 * 1024 / PAGE_SIZE;
	params->u1.s.ramdisk_size = 0;
	params->u1.s.flags = FLAG_READONLY | FLAG_RDLOAD | FLAG_RDPROMPT;
	params->u1.s.rootdev = ROOT_DEV;
	params->u1.s.initrd_start = 0;
	params->u1.s.initrd_size = 0;
	params->u1.s.rd_start = 0;
	params->u1.s.system_rev = 0;
	params->u1.s.system_serial_low = 0;
	params->u1.s.system_serial_high = 0;
	strcpy(params->commandline, CONFIG_CMDLINE);
    }
#endif
}

static struct map_desc tpm102_io_desc[] __initdata = {

  { IO_ADDRESS(DBMX1_SRAM_BASE),     DBMX1_SRAM_BASE,      SZ_128K   , DOMAIN_IO, 0, 1},
  { IO_ADDRESS(DBMX1_IO_BASE),       DBMX1_IO_BASE,        SZ_256K   , DOMAIN_IO, 0, 1},
#ifdef CONFIG_XIP_KERNEL
  { IO2_ADDRESS(DBMX1_IO2_BASE+SZ_4M),DBMX1_IO2_BASE+SZ_4M,SZ_128M-SZ_4M, DOMAIN_IO, 0, 1},
#else
  { IO2_ADDRESS(DBMX1_IO2_BASE),     DBMX1_IO2_BASE,       SZ_128M    , DOMAIN_IO, 0, 1},
#endif
  LAST_DESC
};

void __init tpm102_map_io(void)
{
    iotable_init(tpm102_io_desc);

    tpm102_hw_init();
}

MACHINE_START(TPM102, "YDC TPM102")
	BOOT_MEM(0x08000000, 0x00200000, 0xf0200000)
	BOOT_PARAMS(0x08000100)
	FIXUP(tpm102_fixup)
	MAPIO(tpm102_map_io)
	INITIRQ(dbmx1_init_irq)
MACHINE_END
