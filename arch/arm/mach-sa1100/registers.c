/****************************************************************************

  registers.c: Register monitor of SA-1110

Date: Fri, 2 Feb 2001 10:03:10 -0500
From: Sukjae Cho <sjcho@east.isi.edu>

The code has a long table but simple. It makes proc file of each special
registers in SA-1110 under /proc/cpu/registers directory. And you can read
and write to each registers using shell command such as "cat / echo". For
example,

(none):~# insmod ./registers.o
(none):~# cd /proc/cpu/registers/
(none):/proc/cpu/registers# ls
ClrDCSR0  DBTA1  GFER     ICPR     MSC0   PSPR       Ser1UTCR1  SetDCSR0
ClrDCSR1  DBTA2  GPCLKR0  LCCR0    MSC1   PSSR       Ser1UTCR2  SetDCSR1
ClrDCSR2  DBTA3  GPCLKR1  LCCR1    MSC2   PWER       Ser1UTCR3  SetDCSR2
ClrDCSR3  DBTA4  GPCLKR2  LCCR2    OIER   RCNR       Ser1UTDR   SetDCSR3
ClrDCSR4  DBTA5  GPCLKR3  LCCR3    OSCR   RCSR       Ser1UTSR0  SetDCSR4
ClrDCSR5  DBTB0  GPCR     LCSR     OSMR0  RSRR       Ser1UTSR1  SetDCSR5
DBAR1     DBTB1  GPDR     MCCR0    OSMR1  RTAR       Ser2UTCR0  TUCR
DBAR2     DBTB2  GPLR     MCCR1    OSMR2  RTSR       Ser2UTCR1  UDCAR
DBSA0     DBTB3  GPSR     MCDR0    OSMR3  RTTR       Ser2UTCR2  UDCCR
DBSA1     DBTB4  GRER     MCDR1    OSSR   RdDCSR0    Ser2UTCR3  UDCCS0
DBSA2     DBTB5  HSCR0    MCDR2    OWER   RdDCSR1    Ser2UTCR4  UDCCS1
DBSA3     DCAR1  HSCR1    MCSR     PCFR   RdDCSR2    Ser2UTDR   UDCCS2
DBSA4     DCAR2  HSCR2    MDCAS00  PGSR   RdDCSR3    Ser2UTSR0  UDCD0
DBSA5     DDAR0  HSDR     MDCAS01  PMCR   RdDCSR4    Ser2UTSR1  UDCDR
DBSB0     DDAR1  HSSR0    MDCAS02  POSR   RdDCSR5    Ser3UTCR0  UDCIMP
DBSB1     DDAR2  HSSR1    MDCAS20  PPAR   SMCNFG     Ser3UTCR1  UDCOMP
DBSB2     DDAR3  ICCR     MDCAS21  PPCR   SSCR0      Ser3UTCR2  UDCSR
DBSB3     DDAR4  ICFP     MDCAS22  PPDR   SSCR1      Ser3UTCR3  UDCWC
DBSB4     DDAR5  ICIP     MDCNFG   PPFR   SSDR       Ser3UTDR
DBSB5     GAFR   ICLR     MDREFR   PPSR   SSSR       Ser3UTSR0
DBTA0     GEDR   ICMR     MECR     PSDR   Ser1UTCR0  Ser3UTSR1
(none):/proc/cpu/registers# cat PSPR
0x00000011
(none):/proc/cpu/registers# echo 0x12345678 >PSPR
(none):/proc/cpu/registers# cat PSPR
0x12345678
(none):/proc/cpu/registers#

This module does not implement full file i/o support, and is made for just
debugging and testing purpose. And I used this much but I didn't test it
much.

-Sukjae

****************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>               /* because we are a module */
#include <linux/init.h>                 /* for the __init macros */
#include <linux/proc_fs.h>              /* all the /proc functions */
#include <linux/ioport.h>
#include <asm/uaccess.h>                /* to copy to/from userspace */
#include <asm/arch/hardware.h>

#define MODULE_NAME "regmon"
#define CPU_DIRNAME "cpu"
#define REG_DIRNAME "registers"

static ssize_t proc_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos);
static ssize_t proc_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos);

static struct file_operations proc_reg_operations = {
	read:	proc_read_reg,
	write:	proc_write_reg
};

typedef struct sa1110_reg_entry {
	u32 phyaddr;
	char* name;
	char* description;
	unsigned short low_ino;
} sa1110_reg_entry_t;

static sa1110_reg_entry_t sa1110_regs[] =
{
/*	{ phyaddr,    name,     description } */
	{ 0x80000000, "UDCCR", "UDC control register" },
	{ 0x80000004, "UDCAR", "UDC address register" },
	{ 0x80000008, "UDCOMP", "UDC OUT max packet register" },
	{ 0x8000000C, "UDCIMP", "UDC IN max packet register" },
	{ 0x80000010, "UDCCS0", "UDC endpoint 0 control/status register" },
	{ 0x80000014, "UDCCS1", "UDC endpoint 1 (out) control/status register" },
	{ 0x80000018, "UDCCS2", "UDC endpoint 2 (in) control/status register" },
	{ 0x8000001C, "UDCD0", "UDC endpoint 0 data register" },
	{ 0x80000020, "UDCWC", "UDC endpoint 0 write count register" },
	{ 0x80000028, "UDCDR", "UDC transmit/receive data register (FIFOs)" },
	{ 0x80000030, "UDCSR", "UDC status/interrupt register" },
	{ 0x80010000, "Ser1UTCR0", "UART control register 0" },
	{ 0x80010004, "Ser1UTCR1", "UART control register 1" },
	{ 0x80010008, "Ser1UTCR2", "UART control register 2" },
	{ 0x8001000C, "Ser1UTCR3", "UART control register 3" },
	{ 0x80010014, "Ser1UTDR", "UART data register" },
	{ 0x8001001C, "Ser1UTSR0", "UART status register 0" },
	{ 0x80010020, "Ser1UTSR1", "UART status register 1" },
	{ 0x80020060, "GPCLKR0", "GPCLK Control Register 0" },
	{ 0x80020064, "GPCLKR1", "GPCLK Control Register 1" },
	{ 0x8002006C, "GPCLKR2", "GPCLK Control Register 2" },
	{ 0x80020070, "GPCLKR3", "GPCLK Control Register 3" },
	{ 0x80030000, "Ser2UTCR0", "UART control register 0" },
	{ 0x80030004, "Ser2UTCR1", "UART control register 1" },
	{ 0x80030008, "Ser2UTCR2", "UART control register 2" },
	{ 0x8003000C, "Ser2UTCR3", "UART control register 3" },
	{ 0x80030010, "Ser2UTCR4", "UART control register 4" },
	{ 0x80030014, "Ser2UTDR", "UART data register" },
	{ 0x8003001C, "Ser2UTSR0", "UART status register 0" },
	{ 0x80030020, "Ser2UTSR1", "UART status register 1" },
	{ 0x80040060, "HSCR0", "HSSP control register 0" },
	{ 0x80040064, "HSCR1", "HSSP control register 1" },
	{ 0x8004006C, "HSDR", "HSSP data register" },
	{ 0x80040074, "HSSR0", "HSSP status register 0" },
	{ 0x80040078, "HSSR1", "HSSP status register 1" },
	{ 0x80050000, "Ser3UTCR0", "UART control register 0" },
	{ 0x80050004, "Ser3UTCR1", "UART control register 1" },
	{ 0x80050008, "Ser3UTCR2", "UART control register 2" },
	{ 0x8005000C, "Ser3UTCR3", "UART control register 3" },
	{ 0x80050014, "Ser3UTDR", "UART data register" },
	{ 0x8005001C, "Ser3UTSR0", "UART status register 0" },
	{ 0x80050020, "Ser3UTSR1", "UART status register 1" },
	{ 0x80060000, "MCCR0", "MCP control register 0" },
	{ 0x80060008, "MCDR0", "MCP data register 0" },
	{ 0x8006000C, "MCDR1", "MCP data register 1" },
	{ 0x80060010, "MCDR2", "MCP data register 2" },
	{ 0x80060018, "MCSR", "MCP status register" },
	{ 0x80070060, "SSCR0", "SSP control register 0" },
	{ 0x80070064, "SSCR1", "SSP control register 1" },
	{ 0x8007006C, "SSDR", "SSP data register" },
	{ 0x80070074, "SSSR", "SSP status register" },
	{ 0x90000000, "OSMR0", "OS timer match registers 0" },
	{ 0x90000004, "OSMR1", "OS timer match registers 1" },
	{ 0x90000008, "OSMR2", "OS timer match registers 2" },
	{ 0x9000000C, "OSMR3", "OS timer match registers 3" },
	{ 0x90000010, "OSCR", "OS timer counter register" },
	{ 0x90000014, "OSSR", "OS timer status register" },
	{ 0x90000018, "OWER", "OS timer watchdog enable register" },
	{ 0x9000001C, "OIER", "OS timer interrupt enable register" },
	{ 0x90010000, "RTAR", "Real-time clock alarm register" },
	{ 0x90010004, "RCNR", "Real-time clock count register" },
	{ 0x90010008, "RTTR", "Real-time clock trim register" },
	{ 0x90010010, "RTSR", "Real-time clock status register" },
	{ 0x90020000, "PMCR", "Power manager control register" },
	{ 0x90020004, "PSSR", "Power manager sleep status register" },
	{ 0x90020008, "PSPR", "Power manager scratchpad register" },
	{ 0x9002000C, "PWER", "Power manager wakeup enable register" },
	{ 0x90020010, "PCFR", "Power manager configuration register" },
	{ 0x90020014, "PPCR", "Power manager PLL configuration register" },
	{ 0x90020018, "PGSR", "Power manager GPIO sleep state register" },
	{ 0x9002001C, "POSR", "Power manager oscillator status register" },
	{ 0x90030000, "RSRR", "Reset controller software reset register" },
	{ 0x90030004, "RCSR", "Reset controller status register" },
	{ 0x90030008, "TUCR", "Reserved for test" },
	{ 0x90040000, "GPLR", "GPIO pin level register" },
	{ 0x90040004, "GPDR", "GPIO pin direction register" },
	{ 0x90040008, "GPSR", "GPIO pin output set register" },
	{ 0x9004000C, "GPCR", "GPIO pin output clear register" },
	{ 0x90040010, "GRER", "GPIO rising-edge register" },
	{ 0x90040014, "GFER", "GPIO falling-edge register" },
	{ 0x90040018, "GEDR", "GPIO edge detect status register" },
	{ 0x9004001C, "GAFR", "GPIO alternate function register" },
	{ 0x90050000, "ICIP", "Interrupt controller irq pending register" },
	{ 0x90050004, "ICMR", "Interrupt controller mask register" },
	{ 0x90050008, "ICLR", "Interrupt controller FIQ level register" },
	{ 0x9005000C, "ICCR", "Interrupt controller control register" },
	{ 0x90050010, "ICFP", "Interrupt controller FIQ pending register" },
	{ 0x90050020, "ICPR", "Interrupt controller pending register" },
	{ 0x90060000, "PPDR", "PPC pin direction register" },
	{ 0x90060004, "PPSR", "PPC pin state register" },
	{ 0x90060008, "PPAR", "PPC pin assignment register" },
	{ 0x9006000C, "PSDR", "PPC sleep mode direction register" },
	{ 0x90060010, "PPFR", "PPC pin flag register" },
	{ 0x90060028, "HSCR2", "HSSP control register 2" },
	{ 0x90060030, "MCCR1", "MCP control register 1" },
	{ 0xA0000000, "MDCNFG", "DRAM configuration register" },
	{ 0xA0000004, "MDCAS00", "DRAM CAS waveform rotate register 0 for DRAM bank pair 0/1" },
	{ 0xA0000008, "MDCAS01", "DRAM CAS waveform rotate register 1 for DRAM bank pair 0/1" },
	{ 0xA000000C, "MDCAS02", "DRAM CAS waveform rotate register 2 for DRAM bank pair 0/1" },
	{ 0xA0000010, "MSC0", "Static memory control register 0" },
	{ 0xA0000014, "MSC1", "Static memory control register 1" },
	{ 0xA0000018, "MECR", "Expansion bus configuration register" },
	{ 0xA000001C, "MDREFR", "DRAM refresh control register" },
	{ 0xA0000020, "MDCAS20", "DRAM CAS waveform rotate register 0 for DRAM bank pair 2/3" },
	{ 0xA0000024, "MDCAS21", "DRAM CAS waveform rotate register 1 for DRAM bank pair 2/3" },
	{ 0xA0000028, "MDCAS22", "DRAM CAS waveform rotate register 2 for DRAM bank pair 2/3" },
	{ 0xA000002C, "MSC2", "Static memory control register 2" },
	{ 0xA0000030, "SMCNFG", "SMROM configuration register" },
	{ 0xB0000000, "DDAR0", "DMA device address register" },
	{ 0xB0000004, "SetDCSR0", "DMA control/status register 0 - write ones to set" },
	{ 0xB0000008, "ClrDCSR0", "DMA control/status register 0 - write ones to clear" },
	{ 0xB000000C, "RdDCSR0", "DMA control/status register 0 - read only" },
	{ 0xB0000010, "DBSA0", "DMA buffer A start address 0" },
	{ 0xB0000014, "DBTA0", "DMA buffer A transfer count 0" },
	{ 0xB0000018, "DBSB0", "DMA buffer B start address 0" },
	{ 0xB000001C, "DBTB0", "DMA buffer B transfer count 0" },
	{ 0xB0000020, "DDAR1", "DMA device address register 1" },
	{ 0xB0000024, "SetDCSR1", "DMA control/status register 1 - write ones to set" },
	{ 0xB0000028, "ClrDCSR1", "DMA control/status register 1 - write ones to clear" },
	{ 0xB000002C, "RdDCSR1", "DMA control/status register 1 - read only" },
	{ 0xB0000030, "DBSA1", "DMA buffer A start address 1" },
	{ 0xB0000034, "DBTA1", "DMA buffer A transfer count 1" },
	{ 0xB0000038, "DBSB1", "DMA buffer B start address 1" },
	{ 0xB000003C, "DBTB1", "DMA buffer B transfer count 1" },
	{ 0xB0000040, "DDAR2", "DMA device address register 2" },
	{ 0xB0000044, "SetDCSR2", "DMA control/status register 2 - write ones to set" },
	{ 0xB0000048, "ClrDCSR2", "DMA control/status register 2 - write ones to clear" },
	{ 0xB000004C, "RdDCSR2", "DMA control/status register 2 - read only" },
	{ 0xB0000050, "DBSA2", "DMA buffer A start address 2" },
	{ 0xB0000054, "DBTA2", "DMA buffer A transfer count 2" },
	{ 0xB0000058, "DBSB2", "DMA buffer B start address 2" },
	{ 0xB000005C, "DBTB2", "DMA buffer B transfer count 2" },
	{ 0xB0000060, "DDAR3", "DMA device address register 3" },
	{ 0xB0000064, "SetDCSR3", "DMA control/status register 3 - write ones to set" },
	{ 0xB0000068, "ClrDCSR3", "DMA control/status register 3 - Write ones to clear" },
	{ 0xB000006C, "RdDCSR3", "DMA control/status register 3 - Read only" },
	{ 0xB0000070, "DBSA3", "DMA buffer A start address 3" },
	{ 0xB0000074, "DBTA3", "DMA buffer A transfer count 3" },
	{ 0xB0000078, "DBSB3", "DMA buffer B start address 3" },
	{ 0xB000007C, "DBTB3", "DMA buffer B transfer count 3" },
	{ 0xB0000080, "DDAR4", "DMA device address register 4" },
	{ 0xB0000084, "SetDCSR4", "DMA control/status register 4 - write ones to set" },
	{ 0xB0000088, "ClrDCSR4", "DMA control/status register 4 - write ones to clear" },
	{ 0xB000008C, "RdDCSR4", "DMA control/status register 4 - read only" },
	{ 0xB0000090, "DBSA4", "DMA buffer A start address 4" },
	{ 0xB0000094, "DBTA4", "DMA buffer A transfer count 4" },
	{ 0xB0000098, "DBSB4", "DMA buffer B start address 4" },
	{ 0xB000009C, "DBTB4", "DMA buffer B transfer count 4" },
	{ 0xB00000A0, "DDAR5", "DMA device address register 5" },
	{ 0xB00000A4, "SetDCSR5", "DMA control/status register 5 - write ones to set" },
	{ 0xB00000A8, "ClrDCSR5", "DMA control/status register 5 - write ones to clear" },
	{ 0xB00000AC, "RdDCSR5", "DMA control/status register 5 - read only" },
	{ 0xB00000B0, "DBSA5", "DMA buffer A start address 5" },
	{ 0xB00000B4, "DBTA5", "DMA buffer A transfer count 5" },
	{ 0xB00000B8, "DBSB5", "DMA buffer B start address 5" },
	{ 0xB00000BC, "DBTB5", "DMA buffer B transfer count 5" },
	{ 0xB0100000, "LCCR0", "LCD controller control register 0" },
	{ 0xB0100004, "LCSR", "LCD controller status register" },
	{ 0xB0100010, "DBAR1", "DMA channel 1 base address register" },
	{ 0xB0100014, "DCAR1", "DMA channel 1 current address register" },
	{ 0xB0100018, "DBAR2", "DMA channel 2 base address register" },
	{ 0xB010001C, "DCAR2", "DMA channel 2 current address register" },
	{ 0xB0100020, "LCCR1", "LCD controller control register 1" },
	{ 0xB0100024, "LCCR2", "LCD controller control register 2" },
	{ 0xB0100028, "LCCR3", "LCD controller control register 3" }
};

#define NUM_OF_SA1110_REG_ENTRY	(sizeof(sa1110_regs)/sizeof(sa1110_reg_entry_t))

static int proc_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	sa1110_reg_entry_t* current_reg=NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0;i<NUM_OF_SA1110_REG_ENTRY;i++) {
		if (sa1110_regs[i].low_ino==i_ino) {
			current_reg = &sa1110_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	count = sprintf(outputbuf, "0x%08X\n",
			*((volatile Word *) io_p2v(current_reg->phyaddr)));
	*ppos+=count;
	if (count>nbytes)  /* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t proc_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	sa1110_reg_entry_t* current_reg=NULL;
	int i;
	unsigned long newRegValue;
	char *endp;

	for (i=0;i<NUM_OF_SA1110_REG_ENTRY;i++) {
		if (sa1110_regs[i].low_ino==i_ino) {
			current_reg = &sa1110_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	newRegValue = simple_strtoul(buffer,&endp,0);
	*((volatile Word *) io_p2v(current_reg->phyaddr))=newRegValue;
	return (count+endp-buffer);
}

static struct proc_dir_entry *regdir;
static struct proc_dir_entry *cpudir;

static int __init init_reg_monitor(void)
{
	struct proc_dir_entry *entry;
	int i;

	cpudir = proc_mkdir(CPU_DIRNAME, &proc_root);
	if (cpudir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" CPU_DIRNAME "\n");
		return(-ENOMEM);
	}

	regdir = proc_mkdir(REG_DIRNAME, cpudir);
	if (regdir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" CPU_DIRNAME "/" REG_DIRNAME "\n");
		return(-ENOMEM);
	}

	for(i=0;i<NUM_OF_SA1110_REG_ENTRY;i++) {
		entry = create_proc_entry(sa1110_regs[i].name,
				S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
				regdir);
		if(entry) {
			sa1110_regs[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_reg_operations;
		} else {
			printk( KERN_ERR MODULE_NAME
				": can't create /proc/" REG_DIRNAME
				"/%s\n", sa1110_regs[i].name);
			return(-ENOMEM);
		}
	}
	return (0);
}

static void __exit cleanup_reg_monitor(void)
{
	int i;
	for(i=0;i<NUM_OF_SA1110_REG_ENTRY;i++)
		remove_proc_entry(sa1110_regs[i].name,regdir);
	remove_proc_entry(REG_DIRNAME, cpudir);
	remove_proc_entry(CPU_DIRNAME, &proc_root);
}

module_init(init_reg_monitor);
module_exit(cleanup_reg_monitor);

MODULE_AUTHOR("Sukjae Cho (sjcho@redwood.snu.ac.kr)");
MODULE_DESCRIPTION("SA1110 Register monitor");

EXPORT_NO_SYMBOLS;
