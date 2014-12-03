/*
 * linux/arch/sh/kernel/pci-7751se.c
 *
 * Author:  Ian DaSilva (idasilva@mvista.com)
 *
 * Modified to support PCI devices on MS7751(R)SE01. Takashi Kusuda (Sep 2003).
 *
 * Highly leveraged from pci-bigsur.c, written by Dustin McIntire.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * PCI initialization for the Hitachi SH7751 Solution Engine board (MS7751SE01)
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include <asm/io.h>
#include <asm/pci-sh7751.h>
#include <asm/hitachi_7751se.h>
#include <asm/m1543c.h>

#define PCIMCR_MRSET_OFF	0xBFFFFFFF
#define PCIMCR_RFSH_OFF		0xFFFFFFFB
#define PCIPAR PCI_REG(SH7751_PCIPAR)
#define PCIPDR PCI_REG(SH7751_PCIPDR)

static void __init sh7751se_pci_write_config(unsigned long busNo,
					     unsigned long devNo,
					     unsigned long fncNo,
   					     unsigned long cnfAdd,
					     unsigned long cnfData)
{
	ctrl_outl((0x80000000
		+ ((busNo & 0xff)<<16)
		+ ((devNo & 0x1f)<<11)
		+ ((fncNo & 0x7)<<8)
		+ (cnfAdd & 0xfc)), PCIPAR);

	ctrl_outl(cnfData, PCIPDR);
}

static long __init sh7751se_pci_read_config(unsigned long busNo,
					    unsigned long devNo,
 					    unsigned long fncNo,
					    unsigned long cnfAdd)
{
	ctrl_outl((0x80000000
		+ ((busNo & 0xff)<<16)
		+ ((devNo & 0x1f)<<11)
		+ ((fncNo & 0x7)<<8)
		+ (cnfAdd & 0xfc)), PCIPAR);

	return (ctrl_inl(PCIPDR));
}

/*
 * Only long word accesses of the PCIC's internal local registers and the
 * configuration registers from the CPU is supported.
 */
#define PCIC_WRITE(x,v) writel((v), PCI_REG(x))
#define PCIC_READ(x) readl(PCI_REG(x))

/*
 * Configure the AMD LANCE chip
 */
#if defined(CONFIG_PCNET32)

#define RDP 0x10
#define RAP 0x14
#define BDP 0x1c

static void __init csr_w(unsigned long addr, int index, u16 val)
{
	*(volatile unsigned long *)(0xfe240000 + addr + RAP) = index;
	*(volatile unsigned long *)(0xfe240000 + addr + RDP) = val;
}

static u16 __init csr_r(unsigned long addr, int index)
{
	*(volatile unsigned long *)(0xfe240000 + addr + RAP) = index;
	return *(volatile unsigned long *)(0xfe240000 + addr + RDP);
}

static void __init bcr_w(unsigned long addr, int index, u16 val)
{
	*(volatile unsigned long *)(0xfe240000 + addr + RAP) = index;
	*(volatile unsigned long *)(0xfe240000 + addr + BDP) = val;
}

static u16 __init bcr_r(unsigned long addr, int index)
{
	*(volatile unsigned long *)(0xfe240000 + addr + RAP) = index;
	return *(volatile unsigned long *)(0xfe240000 + addr + BDP);
}

static void __init init_amd79c973(void)
{
	long tmp;
	unsigned long iobase;

	iobase = sh7751se_pci_read_config(0, 0, 0, 0x10) -1;

	tmp = sh7751se_pci_read_config(0, 0, 0, 0x04);
	tmp |= 0x00000147;
	sh7751se_pci_write_config(0, 0, 0, 0x04, tmp);
	*(volatile unsigned long *)(0xFE240000 + iobase +RDP) = 0;

	bcr_w(iobase, 20, 2);
	bcr_w(iobase, 2, (bcr_r(iobase, 2)|2));
	bcr_w(iobase, 9, (bcr_r(iobase, 9)& ~0x0003));
	csr_w(iobase, 4, 0x0915);
	csr_w(iobase, 3, (csr_r(iobase, 3)|0x0040));
	bcr_w(iobase, 18, (bcr_r(iobase, 18)|0x0860));
	csr_w(iobase, 80, (csr_r(iobase, 80)&0x0c00)|0x0c00);
	bcr_w(iobase, 25, 0x17);
	bcr_w(iobase, 26, 0x04);
}
#endif

/*
 * Configure the Bus Master IDE
 */
static void __init init_m5229(void)
{
#if defined(CONFIG_BLK_DEV_ALI15X3)
	sh7751se_pci_write_config(0, 11, 0, 0x04, 0x0000000f);
#endif
	sh7751se_pci_write_config(0, 11, 0, 0x58, 0x00004800);
	sh7751se_pci_write_config(0, 11, 0, 0x5c, 0x00004800);
}

/*
 * Configure the PCI Slot
 */
#define SLOT1 1 /* PCI Slot1 = DEV No.1 */
#define SLOT2 3 /* PCI Slot2 = DEV No.3 */

static void __init init_pci_slot(void)
{
#if 0
        sh7751se_pci_write_config(0, SLOT1, 0, 0x10, 0x00000301);
        sh7751se_pci_write_config(0, SLOT1, 0, 0x14, 0xfd000100);
	/* Add pci card Configuration here, if nesesarry. */
#endif
#if 0
	sh7751se_pci_write_config(0, SLOT2, 0, 0x10, 0x00000401);
	sh7751se_pci_write_config(0, SLOT2, 0, 0x14, 0xfd000100);
#endif
}

/*
 * Configure the Super I/O chip
 */
static void __init ali_config(int index, int data)
{
	outb_p(index, INDEX_PORT);
	outb_p(data, DATA_PORT);
}

static void __init init_m1543c(void)
{
	sh7751se_pci_write_config(0, 2, 0, 0x40, 0x0040c77f);
	sh7751se_pci_write_config(0, 2, 0, 0x4c, 0x0000000f);
	sh7751se_pci_write_config(0, 2, 0, 0x58, 0x0000004c);
	sh7751se_pci_write_config(0, 2, 0, 0x74, 0x00101f19);

	outb_p(CONFIG_ENTER1, CONFIG_PORT);
	outb_p(CONFIG_ENTER2, CONFIG_PORT);

#if defined(CONFIG_PC_KEYB) || defined(CONFIG_PSMOUSE)
	/* PS/2 Keyboard or Mouse (IRQ: 1(keyb), 12(mouse)) */
	ali_config(CURRENT_LDN_INDEX, LDN_KBC);
	ali_config(ACTIVATE_INDEX, 0x01);
	ali_config(IRQ_SELECT_INDEX, 1);
	ali_config(PS2_IRQ_INDEX, 12);
#endif

#if defined(CONFIG_BLK_DEV_FD)
	/* Floppy Drive (IRQ: 6, I/O: 0x3f0) */
	ali_config(CURRENT_LDN_INDEX, LDN_FDC);
	ali_config(ACTIVATE_INDEX, 0x01);
	ali_config(IO_BASE_HI_INDEX, 0x03);
	ali_config(IO_BASE_LO_INDEX, 0xf0);
	ali_config(IRQ_SELECT_INDEX, 6); /* IRQ6 */
#endif

#if defined(CONFIG_SERIAL)
	/* UART1(COM1) (IRQ: 4, I/O: 0x3f8) */
	ali_config(CURRENT_LDN_INDEX, LDN_COM1);
	ali_config(ACTIVATE_INDEX, 0x01);
	ali_config(IO_BASE_HI_INDEX, 0x03);
	ali_config(IO_BASE_LO_INDEX, 0xf8);
	ali_config(IRQ_SELECT_INDEX, 4); /* IRQ4 */

	/* UART3(COM2) (IRQ: 3, I/O: 0x2f8) */
	ali_config(CURRENT_LDN_INDEX, LDN_COM2);
	ali_config(ACTIVATE_INDEX, 0x01);
	ali_config(IO_BASE_HI_INDEX, 0x02);
	ali_config(IO_BASE_LO_INDEX, 0xf8);
	ali_config(IRQ_SELECT_INDEX, 3); /* IRQ3 */
#endif

#if defined(CONFIG_PARPORT)
	/* Parallel Port (IRQ: 5, I/O: 0x378) */
	ali_config(CURRENT_LDN_INDEX, LDN_PARPORT);
	ali_config(ACTIVATE_INDEX, 0x01);
	ali_config(IO_BASE_HI_INDEX, 0x03);
	ali_config(IO_BASE_LO_INDEX, 0x78);
	ali_config(IRQ_SELECT_INDEX, 5); /* IRQ5 */
#endif

	outb_p(CONFIG_EXIT, CONFIG_PORT);
}


/*
 * Description:  This function sets up and initializes the pcic, sets
 * up the BARS, maps the DRAM into the address space etc, etc.
 */
int __init pcibios_init_platform(void)
{
   unsigned long bcr1, wcr1, wcr2, wcr3, mcr;
   unsigned short bcr2;

   /*
    * Initialize the slave bus controller on the pcic.  The values used
    * here should not be hardcoded, but they should be taken from the bsc
    * on the processor, to make this function as generic as possible.
    * (i.e. Another sbc may usr different SDRAM timing settings -- in order
    * for the pcic to work, its settings need to be exactly the same.)
    */
   bcr1 = (*(volatile unsigned long*)(SH7751_BCR1));
   bcr2 = (*(volatile unsigned short*)(SH7751_BCR2));
   wcr1 = (*(volatile unsigned long*)(SH7751_WCR1));
   wcr2 = (*(volatile unsigned long*)(SH7751_WCR2));
   wcr3 = (*(volatile unsigned long*)(SH7751_WCR3));
   mcr = (*(volatile unsigned long*)(SH7751_MCR));

   bcr1 = bcr1 | 0x00080000;  /* Enable Bit 19, BREQEN */
   (*(volatile unsigned long*)(SH7751_BCR1)) = bcr1;   

   bcr1 = bcr1 | 0x40080000;  /* Enable Bit 19 BREQEN, set PCIC to slave */
   PCIC_WRITE(SH7751_PCIBCR1, bcr1);	 /* PCIC BCR1 */
   PCIC_WRITE(SH7751_PCIBCR2, bcr2);     /* PCIC BCR2 */
   PCIC_WRITE(SH7751_PCIWCR1, wcr1);     /* PCIC WCR1 */
   PCIC_WRITE(SH7751_PCIWCR2, wcr2);     /* PCIC WCR2 */
   PCIC_WRITE(SH7751_PCIWCR3, wcr3);     /* PCIC WCR3 */
   mcr = (mcr & PCIMCR_MRSET_OFF) & PCIMCR_RFSH_OFF;
   PCIC_WRITE(SH7751_PCIMCR, mcr);      /* PCIC MCR */


   /* Enable all interrupts, so we know what to fix */
   PCIC_WRITE(SH7751_PCIINTM, 0x0000c3ff);
   PCIC_WRITE(SH7751_PCIAINTM, 0x0000380f);

   /* Set up standard PCI config registers */
   PCIC_WRITE(SH7751_PCICONF1, 	0xFB900047); /* Bus Master, Mem & I/O access */
   PCIC_WRITE(SH7751_PCICONF2, 	0x00000000); /* PCI Class code & Revision ID */
   PCIC_WRITE(SH7751_PCICONF4, 	0xab000001); /* PCI I/O address (local regs) */
   PCIC_WRITE(SH7751_PCICONF5, 	0x0c000000); /* PCI MEM address (local RAM)  */
   PCIC_WRITE(SH7751_PCICONF6, 	0xd0000000); /* PCI MEM address (unused)     */
   PCIC_WRITE(SH7751_PCICONF11, 0x35051054); /* PCI Subsystem ID & Vendor ID */
   PCIC_WRITE(SH7751_PCILSR0, 0x03f00000);   /* MEM (full 64M exposed)       */
   PCIC_WRITE(SH7751_PCILSR1, 0x00000000);   /* MEM (unused)                 */
   PCIC_WRITE(SH7751_PCILAR0, 0x0c000000);   /* MEM (direct map from PCI)    */
   PCIC_WRITE(SH7751_PCILAR1, 0x00000000);   /* MEM (unused)                 */

   /* Now turn it on... */
   PCIC_WRITE(SH7751_PCICR, 0xa5000001);

   /*
    * Set PCIMBR and PCIIOBR here, assuming a single window
    * (16M MEM, 256K IO) is enough.  If a larger space is
    * needed, the readx/writex and inx/outx functions will
    * have to do more (e.g. setting registers for each call).
    */

   /*
    * Set the MBR so PCI address is one-to-one with window,
    * meaning all calls go straight through... use ifdef to
    * catch erroneous assumption.
    */
#if PCIBIOS_MIN_MEM != SH7751_PCI_MEMORY_BASE
#error One-to-one assumption for PCI memory mapping is wrong!?!?!?
#endif   
   PCIC_WRITE(SH7751_PCIMBR, PCIBIOS_MIN_MEM);

   /* Set IOBR for window containing area specified in pci.h */
   PCIC_WRITE(SH7751_PCIIOBR, (PCIBIOS_MIN_IO & SH7751_PCIIOBR_MASK));

   /* All done, may as well say so... */
   printk("SH7751 PCI: Finished initialization of the PCI controller\n");

   /* Initialize peripheral devices on Solution Engine SH7751 */
   sh7751_pci_auto_config();
#if defined(CONFIG_PCNET32)
   init_amd79c973();
#endif
#if 0
   init_pci_slot();
#endif

   return 1;
}

int __init pcibios_map_platform_irq(u8 slot, u8 pin)
{
	/*
	 * IDSEL = AD16         AMD79C973 LAN
	 * IDSEL = AD17         PCI Slot1
	 * IDSEL = AD18         ALi M1543C PCI to ISA Bridge
	 * IDSEL = AD19         PCI Slot2
	 * IDSEL = AD27         M5229 IDE
	 * IDSEL = AD28         M7101 PMU
	 * IDSEL = AD31         M5237 USB Controller
	 */

	int irq[4][16] =
/* IDSEL       [16][17][18][19][20][21][22][23][24][25][26][27][28][29][30][31] */
/* INTA */     { 7,  9, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 11,
/* INTB */       9, 10, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  7,
/* INTC */      10, 11, -1,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  9,
/* INTD */      11,  7, -1,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10 };

	return irq[pin-1][slot];
}
