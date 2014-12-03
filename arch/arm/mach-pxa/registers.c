/****************************************************************************

  registers.c: Register monitor of XPA2X0

	This code is based on SA1110's register moniter

****************************************************************************/
/*
 * ChangeLog:
 *      1-Nov-2003 Sharp Corporation   for Tosa
 *	12-Aug-2004 Lineo Solutions, Inc.  for Spitz
 */
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
#define dis_io_p2v(PhAdd)   ((PhAdd >= 0x40000000) ? (io_p2v(PhAdd)) : (0xe9000000 + PhAdd))

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>               /* because we are a module */
#include <linux/init.h>                 /* for the __init macros */
#include <linux/proc_fs.h>              /* all the /proc functions */
#include <linux/ioport.h>
#include <asm/uaccess.h>                /* to copy to/from userspace */
#include <asm/arch/hardware.h>
#include <asm/io.h>

//typedef unsigned long Word;

#if defined(CONFIG_ARCH_PXA_POODLE)
#define	USE_SCOOP
#define	USE_LOCOMO
#elif defined(CONFIG_ARCH_PXA_CORGI)
#define	USE_SCOOP
#if defined(CONFIG_ARCH_PXA_SPITZ)
#define USE_SCOOP2
#endif
#elif defined(CONFIG_ARCH_PXA_TOSA)
#define USE_SCOOP
#define USE_SCOOP2
#define USE_KUROHYO
#endif

#define MODULE_NAME "regmon"
#define CPU_DIRNAME "cpu"
#define REG_DIRNAME "registers"

#ifdef USE_SCOOP
#define SCOOP_DIRNAME "scoop"
#endif
#ifdef USE_SCOOP2
#define SCOOP2_DIRNAME "scoop2"
#endif
#ifdef USE_KUROHYO
#define KUROHYO_DIRNAME "kurohyo"
#endif
#ifdef USE_LOCOMO
#define LOCOMO_DIRNAME "locomo"
#endif

static ssize_t proc_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos);
static ssize_t proc_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos);

#ifdef USE_SCOOP
static ssize_t proc_scoop_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos);
static ssize_t proc_scoop_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos);


static struct file_operations proc_scoop_reg_operations = {
	read:	proc_scoop_read_reg,
	write:	proc_scoop_write_reg
};


typedef struct scoop_reg_entry {
	u32 phyaddr;
	char* name;
	char* description;
	unsigned short low_ino;
} scoop_reg_entry_t;


static scoop_reg_entry_t scoop_regs[] =
{
/*	{ phyaddr,    name,     description } */
	{ 0x00, "MCR", " " },
	{ 0x04, "CDR", " " },
	{ 0x08, "CSR", " " },
	{ 0x0C, "CPR", " " },
	{ 0x10, "CCR", " " },
	{ 0x14, "IRR", " " },
	{ 0x18, "IMR", " " },
	{ 0x1C, "ISR", " " },
	{ 0x20, "GPCR", " " },
	{ 0x24, "GPWR", " " },
	{ 0x28, "GPRR", " " }
};


#define NUM_OF_SCOOP_REG_ENTRY	(sizeof(scoop_regs)/sizeof(scoop_reg_entry_t))


static int proc_scoop_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	scoop_reg_entry_t* current_reg=NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0;i<NUM_OF_SCOOP_REG_ENTRY;i++) {
		if (scoop_regs[i].low_ino==i_ino) {
			current_reg = &scoop_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	count = sprintf(outputbuf, "0x%04X\n",SCP_REG(current_reg->phyaddr));
	*ppos+=count;
	if (count>nbytes)  /* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t proc_scoop_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	scoop_reg_entry_t* current_reg=NULL;
	int i;
	unsigned long newRegValue;
	char *endp;

	for (i=0;i<NUM_OF_SCOOP_REG_ENTRY;i++) {
		if (scoop_regs[i].low_ino==i_ino) {
			current_reg = &scoop_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	newRegValue = simple_strtoul(buffer,&endp,0);
	SCP_REG(current_reg->phyaddr)=newRegValue;
	return (count+endp-buffer);
}
#endif
#ifdef USE_SCOOP2
static ssize_t proc_scoop2_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos);
static ssize_t proc_scoop2_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos);


static struct file_operations proc_scoop2_reg_operations = {
	read:	proc_scoop2_read_reg,
	write:	proc_scoop2_write_reg
};

typedef struct scoop2_reg_entry {
	u32 phyaddr;
	char* name;
	char* description;
	unsigned short low_ino;
} scoop2_reg_entry_t;


static scoop2_reg_entry_t scoop2_regs[] =
{
/*	{ phyaddr,    name,     description } */
	{ 0x00, "MCR", " " },
	{ 0x04, "CDR", " " },
	{ 0x08, "CSR", " " },
	{ 0x0C, "CPR", " " },
	{ 0x10, "CCR", " " },
	{ 0x14, "IRR", " " },
	{ 0x18, "IMR", " " },
	{ 0x1C, "ISR", " " },
	{ 0x20, "GPCR", " " },
	{ 0x24, "GPWR", " " },
	{ 0x28, "GPRR", " " }
};


#define NUM_OF_SCOOP2_REG_ENTRY	(sizeof(scoop2_regs)/sizeof(scoop2_reg_entry_t))


static int proc_scoop2_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	scoop2_reg_entry_t* current_reg=NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0;i<NUM_OF_SCOOP2_REG_ENTRY;i++) {
		if (scoop2_regs[i].low_ino==i_ino) {
			current_reg = &scoop2_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	count = sprintf(outputbuf, "0x%04X\n",SCP2_REG(current_reg->phyaddr));
	*ppos+=count;
	if (count>nbytes)  /* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t proc_scoop2_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	scoop2_reg_entry_t* current_reg=NULL;
	int i;
	unsigned long newRegValue;
	char *endp;

	for (i=0;i<NUM_OF_SCOOP2_REG_ENTRY;i++) {
		if (scoop2_regs[i].low_ino==i_ino) {
			current_reg = &scoop2_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	newRegValue = simple_strtoul(buffer,&endp,0);
	SCP2_REG(current_reg->phyaddr)=newRegValue;
	return (count+endp-buffer);
}

#endif
#ifdef USE_KUROHYO
static ssize_t proc_kurohyo_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos);
static ssize_t proc_kurohyo_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos);


static struct file_operations proc_kurohyo_reg_operations = {
	read:	proc_kurohyo_read_reg,
	write:	proc_kurohyo_write_reg
};

typedef struct kurohyo_reg_entry {
	u32 phyaddr;
	char* name;
	char* description;
	unsigned short low_ino;
} kurohyo_reg_entry_t;


static kurohyo_reg_entry_t kurohyo_regs[] =
{
/*	{ phyaddr,    name,     description } */
  { 0x000, "PINTST", " " },
  { 0x008, "VHLIN", " " },
  { 0x00A, "CMDADR_L", " " },
  { 0x00C, "CMDADR_H", " " },
  { 0x00E, "CMDFIF", " " },
  { 0x010, "CMDFINT", " " },
  { 0x012, "BINTMSK", " " },
  { 0x014, "BINTST", " " },
  { 0x016, "FIPT", " " },
  { 0x018, "DMAST", " " },
  { 0x01C, "COMD_L", " "},
  { 0x01E, "COMD_H", " "}, 
  { 0x022, "FIFOR", " "},
  { 0x024, "COMSMD", " "},

  { 0x100, "PLCNT", " " },
  { 0x102, "PLCST", " " },
  { 0x108, "PLIST", " " },
  { 0x10A, "PLIEN", " " },
  { 0x10C, "PLSEN", " " },
  { 0x122, "PLDSA_L", " " },
  { 0x124, "PLDSA_H", " " },
  { 0x12A, "PLPIT_L", " " },
  { 0x12C, "PLPIT_H", " " },
  { 0x12E, "PLGMD", " " },
  { 0x140, "PLHT", " " },
  { 0x142, "PLHDS", " " },
  { 0x144, "PLHSS", " " },
  { 0x146, "PLHSE", " " },
  { 0x14C, "PLHPX", " " },
  { 0x150, "PLVT", " " },
  { 0x152, "PLVDS", " " },
  { 0x154, "PLVSS", " " },
  { 0x156, "PLVSE", " " },
  { 0x160, "PLCLN", " " },
  { 0x162, "PLILN", " " },
  { 0x164, "PLMOD", " " },
  { 0x166, "MISC", " " },
  { 0x16A, "PWHSP", " " },
  { 0x16C, "PWVDS", " " },
  { 0x16E, "PWVDE", " " },
  { 0x170, "PWVSP", " " },
  { 0x180, "VWADR_L", " " },
  { 0x182, "VWADR_H", " " },
};


#define NUM_OF_KUROHYO_REG_ENTRY	(sizeof(kurohyo_regs)/sizeof(kurohyo_reg_entry_t))

#define KUROHYO_LCD_INNER_ADDRESS  (TC6393_SYS_BASE+TC6393_GC_INTERNAL_REG_BASE)

static int proc_kurohyo_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	kurohyo_reg_entry_t* current_reg=NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0;i<NUM_OF_KUROHYO_REG_ENTRY;i++) {
		if (kurohyo_regs[i].low_ino==i_ino) {
			current_reg = &kurohyo_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	count = sprintf(outputbuf, "0x%04X\n",readw(KUROHYO_LCD_INNER_ADDRESS + current_reg->phyaddr));
	*ppos+=count;
	if (count>nbytes)  /* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t proc_kurohyo_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	kurohyo_reg_entry_t* current_reg=NULL;
	int i;
	unsigned long newRegValue;
	char *endp;

	for (i=0;i<NUM_OF_KUROHYO_REG_ENTRY;i++) {
		if (kurohyo_regs[i].low_ino==i_ino) {
			current_reg = &kurohyo_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	newRegValue = simple_strtoul(buffer,&endp,0);
	writew(newRegValue,KUROHYO_LCD_INNER_ADDRESS + current_reg->phyaddr);
	return (count+endp-buffer);
}

#endif

#ifdef USE_LOCOMO

static ssize_t proc_locomo_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos);
static ssize_t proc_locomo_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos);


static struct file_operations proc_locomo_reg_operations = {
	read:	proc_locomo_read_reg,
	write:	proc_locomo_write_reg
};


typedef struct locomo_reg_entry {
	u32 phyaddr;
	char* name;
	char* description;
	unsigned short low_ino;
} locomo_reg_entry_t;


static locomo_reg_entry_t locomo_regs[] =
{
/*	{ phyaddr,    name,     description } */
	{ 0x10000000, "VER", " " },
	{ 0x10000004, "ST", " " },
	{ 0x10000008, "C32K", " " },
	{ 0x1000000C, "ICR", " " },
	{ 0x10000010, "MCSX0", " " },
	{ 0x10000014, "MCSX1", " " },
	{ 0x10000018, "MCSX2", " " },
	{ 0x1000001C, "MCSX3", " " },
	{ 0x10000020, "ASD", " " },
	{ 0x10000028, "HSD", " " },
	{ 0x1000002C, "HSC", " " },
	{ 0x10000030, "TADC", " " },
	{ 0x10000038, "TC", " " },
	{ 0x1000003C, "CPSD", " " },
	{ 0x10000040, "KIB", " " },
	{ 0x10000044, "KSC", " " },
	{ 0x10000048, "KCMD", " " },
	{ 0x1000004C, "KIC", " " },
	{ 0x10000054, "ACC", " " },
	{ 0x10000060, "SPIMD", " " },
	{ 0x10000064, "SPICT", " " },
	{ 0x10000068, "SPIST", " " },
	{ 0x10000070, "SPIIS", " " },
	{ 0x10000074, "SPIWE", " " },
	{ 0x10000078, "SPIIE", " " },
	{ 0x1000007C, "SPIIR", " " },
	{ 0x10000080, "SPITD", " " },
	{ 0x10000084, "SPIRD", " " },
	{ 0x10000088, "SPITS", " " },
	{ 0x1000008C, "SPIRS", " " },
	{ 0x10000090, "GPD", " " },
	{ 0x10000094, "GPE", " " },
	{ 0x10000098, "GPL", " " },
	{ 0x1000009C, "GPO", " " },
	{ 0x100000a0, "GRIE", " " },
	{ 0x100000a4, "GFIE", " " },
	{ 0x100000a8, "GIS", " " },
	{ 0x100000ac, "GWE", " " },
	{ 0x100000b0, "GIE", " " },
	{ 0x100000b4, "GIR", " " },
	{ 0x100000c8, "ALC", " " },
	{ 0x100000cc, "ALR", " " },
	{ 0x100000d0, "PAIF", " " },
	{ 0x100000d8, "LTC", " " },
	{ 0x100000dc, "LTINT", " " },
	{ 0x100000e0, "DAC", " " },
	{ 0x100000e8, "LPT0", " " },
	{ 0x100000ec, "LPT1", " " },
	{ 0x100000d0, "LPT2", " " },
	{ 0x100000fc, "TCR", " " },
};


#define NUM_OF_LOCOMO_REG_ENTRY	(sizeof(locomo_regs)/sizeof(locomo_reg_entry_t))


static int proc_locomo_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	locomo_reg_entry_t* current_reg=NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0;i<NUM_OF_LOCOMO_REG_ENTRY;i++) {
		if (locomo_regs[i].low_ino==i_ino) {
			current_reg = &locomo_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	count = sprintf(outputbuf, "0x%04X\n",
			*((volatile unsigned short *) LCM_p2v(current_reg->phyaddr)));

	*ppos+=count;
	if (count>nbytes)  /* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t proc_locomo_write_reg(struct file * file, const char * buffer,
		size_t count, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	locomo_reg_entry_t* current_reg=NULL;
	int i;
	unsigned long newRegValue;
	char *endp;

	for (i=0;i<NUM_OF_LOCOMO_REG_ENTRY;i++) {
		if (locomo_regs[i].low_ino==i_ino) {
			current_reg = &locomo_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	newRegValue = simple_strtoul(buffer,&endp,0);
	*((volatile Word *) LCM_p2v(current_reg->phyaddr))=newRegValue;
	return (count+endp-buffer);
}


#endif


static struct file_operations proc_reg_operations = {
	read:	proc_read_reg,
	write:	proc_write_reg
};

typedef struct xpa2X0_reg_entry {
	u32 phyaddr;
	char* name;
	char* description;
	unsigned short low_ino;
} xpa2X0_reg_entry_t;

static xpa2X0_reg_entry_t xpa2X0_regs[] =
{
/*	{ phyaddr,    name,     description } */
	{0x40000000, "DCSR0", "DMA Control / Status Register for Channel 0"},
	{0x40000004, "DCSR1", "DMA Control / Status Register for Channel 1"},
	{0x40000008, "DCSR2", "DMA Control / Status Register for Channel 2"},
	{0x4000000C, "DCSR3", "DMA Control / Status Register for Channel 3"},
	{0x40000010, "DCSR4", "DMA Control / Status Register for Channel 4"},
	{0x40000014, "DCSR5", "DMA Control / Status Register for Channel 5"},
	{0x40000018, "DCSR6", "DMA Control / Status Register for Channel 6"},
	{0x4000001C, "DCSR7", "DMA Control / Status Register for Channel 7"},
	{0x40000020, "DCSR8", "DMA Control / Status Register for Channel 8"},
	{0x40000024, "DCSR9", "DMA Control / Status Register for Channel 9"},
	{0x40000028, "DCSR10", "DMA Control / Status Register for Channel 10"},
	{0x4000002C, "DCSR11", "DMA Control / Status Register for Channel 11"},
	{0x40000030, "DCSR12", "DMA Control / Status Register for Channel 12"},
	{0x40000034, "DCSR13", "DMA Control / Status Register for Channel 13"},
	{0x40000038, "DCSR14", "DMA Control / Status Register for Channel 14"},
	{0x4000003C, "DCSR15", "DMA Control / Status Register for Channel 15"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40000040, "DCSR16", "DMA Control / Status Register for Channel 16"},
	{0x40000044, "DCSR17", "DMA Control / Status Register for Channel 17"},
	{0x40000048, "DCSR18", "DMA Control / Status Register for Channel 18"},
	{0x4000004C, "DCSR19", "DMA Control / Status Register for Channel 19"},
	{0x40000050, "DCSR20", "DMA Control / Status Register for Channel 20"},
	{0x40000054, "DCSR21", "DMA Control / Status Register for Channel 21"},
	{0x40000058, "DCSR22", "DMA Control / Status Register for Channel 22"},
	{0x4000005C, "DCSR23", "DMA Control / Status Register for Channel 23"},
	{0x40000060, "DCSR24", "DMA Control / Status Register for Channel 24"},
	{0x40000064, "DCSR25", "DMA Control / Status Register for Channel 25"},
	{0x40000068, "DCSR26", "DMA Control / Status Register for Channel 26"},
	{0x4000006C, "DCSR27", "DMA Control / Status Register for Channel 27"},
	{0x40000070, "DCSR28", "DMA Control / Status Register for Channel 28"},
	{0x40000074, "DCSR29", "DMA Control / Status Register for Channel 29"},
	{0x40000078, "DCSR30", "DMA Control / Status Register for Channel 30"},
	{0x4000007C, "DCSR31", "DMA Control / Status Register for Channel 31"},
	{0x400000A0, "DALGN", "DMA Alignment Register"},
	{0x400000A4, "DPCSR", "DMA Programmed I/O Control Status Register"},
	{0x400000E0, "DRQSR0", "DMA DREQ<0> Status Register"},
	{0x400000E4, "DRQSR1", "DMA DREQ<1> Status Register"},
	{0x400000E8, "DRQSR2", "DMA DREQ<2> Status Register"},
#endif
	{0x400000f0, "DINT", "DMA Interrupt Register"},
	{0x40000100, "DRCMR0", "Request to Channel Map Register for DREQ 0"},
	{0x40000104, "DRCMR1", "Request to Channel Map Register for DREQ 1"},
	{0x40000108, "DRCMR2", "Request to Channel Map Register for I2S receive Request"},
	{0x4000010C, "DRCMR3", "Request to Channel Map Register for I2S transmit Request"},
	{0x40000110, "DRCMR4", "Request to Channel Map Register for BTUART receive Request"},
	{0x40000114, "DRCMR5", "Request to Channel Map Register for BTUART transmit Request."},
	{0x40000118, "DRCMR6", "Request to Channel Map Register for FFUART receive Request"},
	{0x4000011C, "DRCMR7", "Request to Channel Map Register for FFUART transmit Request"},
	{0x40000120, "DRCMR8", "Request to Channel Map Register for AC97 microphone Request"},
	{0x40000124, "DRCMR9", "Request to Channel Map Register for AC97 modem receive Request"},
	{0x40000128, "DRCMR10", "Request to Channel Map Register for AC97 modem transmit Request"},
	{0x4000012C, "DRCMR11", "Request to Channel Map Register for AC97 audio receive Request"},
	{0x40000130, "DRCMR12", "Request to Channel Map Register for AC97 audio transmit Request"},
	{0x40000134, "DRCMR13", "Request to Channel Map Register for SSP receive Request"},
	{0x40000138, "DRCMR14", "Request to Channel Map Register for SSP transmit Request"},
	{0x40000144, "DRCMR17", "Request to Channel Map Register for ICP receive Request"},
	{0x40000148, "DRCMR18", "Request to Channel Map Register for ICP transmit Request"},
	{0x4000014C, "DRCMR19", "Request to Channel Map Register for STUART receive Request"},
	{0x40000150, "DRCMR20", "Request to Channel Map Register for STUART transmit Request"},
	{0x40000154, "DRCMR21", "Request to Channel Map Register for MMC receive Request"},
	{0x40000158, "DRCMR22", "Request to Channel Map Register for MMC transmit Request"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40000160, "DRCMR24", "Request to Channel Map Register for USB endpoint 0 Request"},
	{0x40000164, "DRCMR25", "Request to Channel Map Register for USB endpoint A Request"},
	{0x40000168, "DRCMR26", "Request to Channel Map Register for USB endpoint B Request"},
	{0x4000016C, "DRCMR27", "Request to Channel Map Register for USB endpoint C Request"},
	{0x40000170, "DRCMR28", "Request to Channel Map Register for USB endpoint D Request"},
	{0x40000174, "DRCMR29", "Request to Channel Map Register for USB endpoint E Request"},
	{0x40000178, "DRCMR30", "Request to Channel Map Register for USB endpoint F Request"},
	{0x4000017C, "DRCMR31", "Request to Channel Map Register for USB endpoint G Request"},
	{0x40000180, "DRCMR32", "Request to Channel Map Register for USB endpoint H Request"},
	{0x40000184, "DRCMR33", "Request to Channel Map Register for USB endpoint I Request"},
	{0x40000188, "DRCMR34", "Request to Channel Map Register for USB endpoint J Request"},
	{0x4000018C, "DRCMR35", "Request to Channel Map Register for USB endpoint K Request"},
	{0x40000190, "DRCMR36", "Request to Channel Map Register for USB endpoint L Request"},
	{0x40000194, "DRCMR37", "Request to Channel Map Register for USB endpoint M Request"},
	{0x40000198, "DRCMR38", "Request to Channel Map Register for USB endpoint N Request"},
	{0x4000019C, "DRCMR39", "Request to Channel Map Register for USB endpoint P Request"},
	{0x400001A0, "DRCMR40", "Request to Channel Map Register for USB endpoint Q Request"},
	{0x400001A4, "DRCMR41", "Request to Channel Map Register for USB endpoint R Request"},
	{0x400001A8, "DRCMR42", "Request to Channel Map Register for USB endpoint S Request"},
	{0x400001AC, "DRCMR43", "Request to Channel Map Register for USB endpoint T Request"},
	{0x400001B0, "DRCMR44", "Request to Channel Map Register for USB endpoint U Request"},
	{0x400001B4, "DRCMR45", "Request to Channel Map Register for USB endpoint V Request"},
	{0x400001B8, "DRCMR46", "Request to Channel Map Register for USB endpoint W Request"},
	{0x400001BC, "DRCMR47", "Request to Channel Map Register for USB endpoint X Request"},
	{0x400001C0, "DRCMR48", "Request to Channel Map Register for MSL receive request 1"},
	{0x400001C4, "DRCMR49", "Request to Channel Map Register for MSL transmit request 1"},
	{0x400001C8, "DRCMR50", "Request to Channel Map Register for MSL receive request 2"},
	{0x400001CC, "DRCMR51", "Request to Channel Map Register for MSL transmit request 2"},
	{0x400001D0, "DRCMR52", "Request to Channel Map Register for MSL receive request 3"},
	{0x400001D4, "DRCMR53", "Request to Channel Map Register for MSL transmit request 3"},
	{0x400001D8, "DRCMR54", "Request to Channel Map Register for MSL receive request 4"},
	{0x400001DC, "DRCMR55", "Request to Channel Map Register for MSL transmit request 4"},
	{0x400001E0, "DRCMR56", "Request to Channel Map Register for MSL receive request 5"},
	{0x400001E4, "DRCMR57", "Request to Channel Map Register for MSL transmit request 5"},
	{0x400001E8, "DRCMR58", "Request to Channel Map Register for MSL receive request 6"},
	{0x400001EC, "DRCMR59", "Request to Channel Map Register for MSL transmit request 6"},
	{0x400001F0, "DRCMR60", "Request to Channel Map Register for MSL receive request 7"},
	{0x400001F4, "DRCMR61", "Request to Channel Map Register for MSL transmit request 7"},
	{0x400001F8, "DRCMR62", "Request to Channel Map Register for USIM receive request"},
	{0x400001FC, "DRCMR63", "Request to Channel Map Register for USIM transmit request"},
#else
	{0x40000164, "DRCMR25", "Request to Channel Map Register for USB endpoint 1 Request"},
	{0x40000168, "DRCMR26", "Request to Channel Map Register for USB endpoint 2 Request"},
	{0x4000016C, "DRCMR27", "Request to Channel Map Register for USB endpoint 3 Request"},
	{0x40000170, "DRCMR28", "Request to Channel Map Register for USB endpoint 4 Request"},
	{0x40000178, "DRCMR30", "Request to Channel Map Register for USB endpoint 6 Request"},
	{0x4000017C, "DRCMR31", "Request to Channel Map Register for USB endpoint 7 Request"},
	{0x40000180, "DRCMR32", "Request to Channel Map Register for USB endpoint 8 Request"},
	{0x40000184, "DRCMR33", "Request to Channel Map Register for USB endpoint 9 Request"},
	{0x4000018C, "DRCMR35", "Request to Channel Map Register for USB endpoint 11 Request"},
	{0x40000190, "DRCMR36", "Request to Channel Map Register for USB endpoint 12 Request"},
	{0x40000194, "DRCMR37", "Request to Channel Map Register for USB endpoint 13 Request"},
	{0x40000198, "DRCMR38", "Request to Channel Map Register for USB endpoint 14 Request"},
#endif
	{0x40000200, "DDADR0", "DMA Descriptor Address Register Channel 0"},
	{0x40000204, "DSADR0", "DMA Source Address Register Channel 0"},
	{0x40000208, "DTADR0", "DMA Target Address Register Channel 0"},
	{0x4000020C, "DCMD0", "DMA Command Address Register Channel 0"},
	{0x40000210, "DDADR1", "DMA Descriptor Address Register Channel 1"},
	{0x40000214, "DSADR1", "DMA Source Address Register Channel 1"},
	{0x40000218, "DTADR1", "DMA Target Address Register Channel 1"},
	{0x4000021C, "DCMD1", "DMA Command Address Register Channel 1"},
	{0x40000220, "DDADR2", "DMA Descriptor Address Register Channel 2"},
	{0x40000224, "DSADR2", "DMA Source Address Register Channel 2"},
	{0x40000228, "DTADR2", "DMA Target Address Register Channel 2"},
	{0x4000022C, "DCMD2", "DMA Command Address Register Channel 2"},
	{0x40000230, "DDADR3", "DMA Descriptor Address Register Channel 3"},
	{0x40000234, "DSADR3", "DMA Source Address Register Channel 3"},
	{0x40000238, "DTADR3", "DMA Target Address Register Channel 3"},
	{0x4000023C, "DCMD3", "DMA Command Address Register Channel 3"},
	{0x40000240, "DDADR4", "DMA Descriptor Address Register Channel 4"},
	{0x40000244, "DSADR4", "DMA Source Address Register Channel 4"},
	{0x40000248, "DTADR4", "DMA Target Address Register Channel 4"},
	{0x4000024C, "DCMD4", "DMA Command Address Register Channel 4"},
	{0x40000250, "DDADR5", "DMA Descriptor Address Register Channel 5"},
	{0x40000254, "DSADR5", "DMA Source Address Register Channel 5"},
	{0x40000258, "DTADR5", "DMA Target Address Register Channel 5"},
	{0x4000025C, "DCMD5", "DMA Command Address Register Channel 5"},
	{0x40000260, "DDADR6", "DMA Descriptor Address Register Channel 6"},
	{0x40000264, "DSADR6", "DMA Source Address Register Channel 6"},
	{0x40000268, "DTADR6", "DMA Target Address Register Channel 6"},
	{0x4000026C, "DCMD6", "DMA Command Address Register Channel 6"},
	{0x40000270, "DDADR7", "DMA Descriptor Address Register Channel 7"},
	{0x40000274, "DSADR7", "DMA Source Address Register Channel 7"},
	{0x40000278, "DTADR7", "DMA Target Address Register Channel 7"},
	{0x4000027C, "DCMD7", "DMA Command Address Register Channel 7"},
	{0x40000280, "DDADR8", "DMA Descriptor Address Register Channel 8"},
	{0x40000284, "DSADR8", "DMA Source Address Register Channel 8"},
	{0x40000288, "DTADR8", "DMA Target Address Register Channel 8"},
	{0x4000028C, "DCMD8", "DMA Command Address Register Channel 8"},
	{0x40000290, "DDADR9", "DMA Descriptor Address Register Channel 9"},
	{0x40000294, "DSADR9", "DMA Source Address Register Channel 9"},
	{0x40000298, "DTADR9", "DMA Target Address Register Channel 9"},
	{0x4000029C, "DCMD9", "DMA Command Address Register Channel 9"},
	{0x400002A0, "DDADR10", "DMA Descriptor Address Register Channel 10"},
	{0x400002A4, "DSADR10", "DMA Source Address Register Channel 10"},
	{0x400002A8, "DTADR10", "DMA Target Address Register Channel 10"},
	{0x400002AC, "DCMD10", "DMA Command Address Register Channel 10"},
	{0x400002B0, "DDADR11", "DMA Descriptor Address Register Channel 11"},
	{0x400002B4, "DSADR11", "DMA Source Address Register Channel 11"},
	{0x400002B8, "DTADR11", "DMA Target Address Register Channel 11"},
	{0x400002BC, "DCMD11", "DMA Command Address Register Channel 11"},
	{0x400002C0, "DDADR12", "DMA Descriptor Address Register Channel 12"},
	{0x400002C4, "DSADR12", "DMA Source Address Register Channel 12"},
	{0x400002C8, "DTADR12", "DMA Target Address Register Channel 12"},
	{0x400002CC, "DCMD12", "DMA Command Address Register Channel 12"},
	{0x400002D0, "DDADR13", "DMA Descriptor Address Register Channel 13"},
	{0x400002D4, "DSADR13", "DMA Source Address Register Channel 13"},
	{0x400002D8, "DTADR13", "DMA Target Address Register Channel 13"},
	{0x400002DC, "DCMD13", "DMA Command Address Register Channel 13"},
	{0x400002E0, "DDADR14", "DMA Descriptor Address Register Channel 14"},
	{0x400002E4, "DSADR14", "DMA Source Address Register Channel 14"},
	{0x400002E8, "DTADR14", "DMA Target Address Register Channel 14"},
	{0x400002EC, "DCMD14", "DMA Command Address Register Channel 14"},
	{0x400002F0, "DDADR15", "DMA Descriptor Address Register Channel 15"},
	{0x400002F4, "DSADR15", "DMA Source Address Register Channel 15"},
	{0x400002F8, "DTADR15", "DMA Target Address Register Channel 15"},
	{0x400002FC, "DCMD15", "DMA Command Address Register Channel 15"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40000300, "DDADR16", "DMA Descriptor Address Register Channel 16"},
	{0x40000304, "DSADR16", "DMA Source Address Register Channel 16"},
	{0x40000308, "DTADR16", "DMA Target Address Register Channel 16"},
	{0x4000030C, "DCMD16", "DMA Command Address Register Channel 16"},
	{0x40000310, "DDADR17", "DMA Descriptor Address Register Channel 17"},
	{0x40000314, "DSADR17", "DMA Source Address Register Channel 17"},
	{0x40000318, "DTADR17", "DMA Target Address Register Channel 17"},
	{0x4000031C, "DCMD17", "DMA Command Address Register Channel 17"},
	{0x40000320, "DDADR18", "DMA Descriptor Address Register Channel 18"},
	{0x40000324, "DSADR18", "DMA Source Address Register Channel 18"},
	{0x40000328, "DTADR18", "DMA Target Address Register Channel 18"},
	{0x4000032C, "DCMD18", "DMA Command Address Register Channel 18"},
	{0x40000330, "DDADR19", "DMA Descriptor Address Register Channel 19"},
	{0x40000334, "DSADR19", "DMA Source Address Register Channel 19"},
	{0x40000338, "DTADR19", "DMA Target Address Register Channel 19"},
	{0x4000033C, "DCMD19", "DMA Command Address Register Channel 19"},
	{0x40000340, "DDADR20", "DMA Descriptor Address Register Channel 20"},
	{0x40000344, "DSADR20", "DMA Source Address Register Channel 20"},
	{0x40000348, "DTADR20", "DMA Target Address Register Channel 20"},
	{0x4000034C, "DCMD20", "DMA Command Address Register Channel 20"},
	{0x40000350, "DDADR21", "DMA Descriptor Address Register Channel 21"},
	{0x40000354, "DSADR21", "DMA Source Address Register Channel 21"},
	{0x40000358, "DTADR21", "DMA Target Address Register Channel 21"},
	{0x4000035C, "DCMD21", "DMA Command Address Register Channel 21"},
	{0x40000360, "DDADR22", "DMA Descriptor Address Register Channel 22"},
	{0x40000364, "DSADR22", "DMA Source Address Register Channel 22"},
	{0x40000368, "DTADR22", "DMA Target Address Register Channel 22"},
	{0x4000036C, "DCMD22", "DMA Command Address Register Channel 22"},
	{0x40000370, "DDADR23", "DMA Descriptor Address Register Channel 23"},
	{0x40000374, "DSADR23", "DMA Source Address Register Channel 23"},
	{0x40000378, "DTADR23", "DMA Target Address Register Channel 23"},
	{0x4000037C, "DCMD23", "DMA Command Address Register Channel 23"},
	{0x40000380, "DDADR24", "DMA Descriptor Address Register Channel 24"},
	{0x40000384, "DSADR24", "DMA Source Address Register Channel 24"},
	{0x40000388, "DTADR24", "DMA Target Address Register Channel 24"},
	{0x4000038C, "DCMD24", "DMA Command Address Register Channel 24"},
	{0x40000390, "DDADR25", "DMA Descriptor Address Register Channel 25"},
	{0x40000394, "DSADR25", "DMA Source Address Register Channel 25"},
	{0x40000398, "DTADR25", "DMA Target Address Register Channel 25"},
	{0x4000039C, "DCMD25", "DMA Command Address Register Channel 25"},
	{0x400003A0, "DDADR26", "DMA Descriptor Address Register Channel 26"},
	{0x400003A4, "DSADR26", "DMA Source Address Register Channel 26"},
	{0x400003A8, "DTADR26", "DMA Target Address Register Channel 26"},
	{0x400003AC, "DCMD26", "DMA Command Address Register Channel 26"},
	{0x400003B0, "DDADR27", "DMA Descriptor Address Register Channel 27"},
	{0x400003B4, "DSADR27", "DMA Source Address Register Channel 27"},
	{0x400003B8, "DTADR27", "DMA Target Address Register Channel 27"},
	{0x400003BC, "DCMD27", "DMA Command Address Register Channel 27"},
	{0x400003C0, "DDADR28", "DMA Descriptor Address Register Channel 28"},
	{0x400003C4, "DSADR28", "DMA Source Address Register Channel 28"},
	{0x400003C8, "DTADR28", "DMA Target Address Register Channel 28"},
	{0x400003CC, "DCMD28", "DMA Command Address Register Channel 28"},
	{0x400003D0, "DDADR29", "DMA Descriptor Address Register Channel 29"},
	{0x400003D4, "DSADR29", "DMA Source Address Register Channel 29"},
	{0x400003D8, "DTADR29", "DMA Target Address Register Channel 29"},
	{0x400003DC, "DCMD29", "DMA Command Address Register Channel 29"},
	{0x400003E0, "DDADR30", "DMA Descriptor Address Register Channel 30"},
	{0x400003E4, "DSADR30", "DMA Source Address Register Channel 30"},
	{0x400003E8, "DTADR30", "DMA Target Address Register Channel 30"},
	{0x400003EC, "DCMD30", "DMA Command Address Register Channel 30"},
	{0x400003F0, "DDADR31", "DMA Descriptor Address Register Channel 31"},
	{0x400003F4, "DSADR31", "DMA Source Address Register Channel 31"},
	{0x400003F8, "DTADR31", "DMA Target Address Register Channel 31"},
	{0x400003FC, "DCMD31", "DMA Command Address Register Channel 31"},
	{0x40001100, "DRCMR64", "Request to Channel Map Register for Memory Stick receive request"},
	{0x40001104, "DRCMR65", "Request to Channel Map Register for Memory Stick transmit request"},
	{0x40001108, "DRCMR66", "Request to Channel Map Register for SSP3 receive request"},
	{0x4000110C, "DRCMR67", "Request to Channel Map Register for SSP3 transmit request"},
	{0x40001110, "DRCMR68", "Request to Channel Map Register for Quick Capture Interface receive request 0"},
	{0x40001114, "DRCMR69", "Request to Channel Map Register for Quick Capture Interface receive request 1"},
	{0x40001118, "DRCMR70", "Request to Channel Map Register for Quick Capture Interface receive request 2"},
	{0x40001128, "DRCMR74", "Request to Channel Map Register for DREQ<2> (companion chip request 2)"},
#endif
	{0x40100000, "FFRBR", "Receive Buffer Register (read only)"},
	{0x40100000, "FFTHR", "Transmit Holding Register (write only)"},
	{0x40100004, "FFIER", "Interrupt Enable Register (read/write)"},
	{0x40100008, "FFIIR", "Interrupt ID Register (read only)"},
	{0x40100008, "FFFCR", "FIFO Control Register (write only)"},
	{0x4010000C, "FFLCR", "Line Control Register (read/write)"},
	{0x40100010, "FFMCR", "Modem Control Register (read/write)"},
	{0x40100014, "FFLSR", "Line Status Register (read only)"},
	{0x40100018, "FFMSR", "Modem Status Register (read only)"},
	{0x4010001C, "FFSPR", "Scratch Pad Register (read/write)"},
	{0x40100020, "FFISR", "Infrared Selection Register (read/write)"},
	{0x40100000, "FFDLL", "Divisor Latch Low Register (DLAB = 1) (read/write)"},
	{0x40100004, "FFDLH", "Divisor Latch High Register (DLAB = 1) (read/write)"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40100024, "FFFOR", "Receive FIFO Occupancy Register"},
	{0x40100028, "FFABR", "Auto-baud Control Register"},
	{0x4010002C, "FFACR", "Auto-baud Count Register"},
#endif
	{0x40200000, "BTRBR", "Receive Buffer Register (read only)"},
	{0x40200000, "BTTHR", "Transmit Holding Register (write only)"},
	{0x40200004, "BTIER", "Interrupt Enable Register (read/write)"},
	{0x40200008, "BTIIR", "Interrupt ID Register (read only)"},
	{0x40200008, "BTFCR", "FIFO Control Register (write only)"},
	{0x4020000C, "BTLCR", "Line Control Register (read/write)"},
	{0x40200010, "BTMCR", "Modem Control Register (read/write)"},
	{0x40200014, "BTLSR", "Line Status Register (read only)"},
	{0x40200018, "BTMSR", "Modem Status Register (read only)"},
	{0x4020001C, "BTSPR", "Scratch Pad Register (read/write)"},
	{0x40200020, "BTISR", "Infrared Selection Register (read/write)"},
	{0x40200000, "BTDLL", "Divisor Latch Low Register (DLAB = 1) (read/write)"},
	{0x40200004, "BTDLH", "Divisor Latch High Register (DLAB = 1) (read/write)"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40200024, "BTFOR", "Receive FIFO Occupancy Register"},
	{0x40200028, "BTABR", "Auto-baud Control Register"},
	{0x4020002C, "BTACR", "Auto-baud Count Register"},
#endif
	{0x40301680, "IBMR", "I2C Bus Monitor Register - IBMR"},
	{0x40301688, "IDBR", "I2C Data Buffer Register - IDBR"},
	{0x40301690, "ICR", "I2C Control Register - ICR"},
	{0x40301698, "ISR", "I2C Status Register - ISR"},
	{0x403016A0, "ISAR", "I2C Slave Address Register - ISAR"},
	{0x40400000, "SACR0", "Global Control Register"},
	{0x40400004, "SACR1", "Serial Audio I 2 S/MSB-Justified Control Register"},
	{0x4040000C, "SASR0", "Serial Audio I 2 S/MSB-Justified Interface and FIFO Status Register"},
	{0x40400014, "SAIMR", "Serial Audio Interrupt Mask Register"},
	{0x40400018, "SAICR", "Serial Audio Interrupt Clear Register"},
	{0x40400060, "SADIV", "Audio Clock Divider Register."},
	{0x40400080, "SADR", "Serial Audio Data Register (TX and RX FIFO access Register)."},
	{0x40500000, "POCR", "PCM Out Control Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40500004, "PCMICR", "PCM In Control Register"},
#else
	{0x40500004, "PICR", "PCM In Control Register"},
#endif
	{0x40500008, "MCCR", "Mic In Control Register"},
	{0x4050000C, "GCR", "Global Control Register"},
	{0x40500010, "POSR", "PCM Out Status Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40500014, "PCMISR", "PCM In Status Register"},
#else
	{0x40500014, "PISR", "PCM In Status Register"},
#endif
	{0x40500018, "MCSR", "Mic In Status Register"},
	{0x4050001C, "GSR", "Global Status Register"},
	{0x40500020, "CAR", "CODEC Access Register"},
	{0x40500040, "PCDR", "PCM FIFO Data Register"},
	{0x40500060, "MCDR", "Mic-in FIFO Data Register"},
	{0x40500100, "MOCR", "Modem Out Control Register"},
	{0x40500108, "MICR", "Modem In Control Register"},
	{0x40500110, "MOSR", "Modem Out Status Register"},
	{0x40500118, "MISR", "Modem In Status Register"},
	{0x40500140, "MODR", "Modem FIFO Data Register"},
	{0x40600000, "UDCCR", "UDC Control Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40600004, "UDCICR0", "UDC Interrupt Control Register 0"},
	{0x40600008, "UDCICR1", "UDC Interrupt Control Register 1"},
	{0x4060000C, "UDCISR0", "UDC Interrupt Status Register 0"},
	{0x40600010, "UDCISR1", "UDC Interrupt Status Register 1"},
	{0x40600014, "UDCFNR", "UDC Interrupt Frame Number Register"},
	{0x40600018, "UDCOTGICR", "UDC OTG Interrupt Control Register"},
	{0x4060001C, "UDCOTGISR", "UDC OTG Interrupt Status Register"},
	{0x40600020, "UP2OCR", "USB Port 2 Output Control Register"},
	{0x40600024, "UP3OCR", "USB Port 3 Output Control Register"},
	{0x40600100, "UDCCSR0", "UDC Control/Status Register-Endpoint 0"},
	{0x40600104, "UDCCSRA", "UDC Control/Status Register-Endpoint A"},
	{0x40600108, "UDCCSRB", "UDC Control/Status Register-Endpoint B"},
	{0x4060010C, "UDCCSRC", "UDC Control/Status Register-Endpoint C"},
	{0x40600110, "UDCCSRD", "UDC Control/Status Register-Endpoint D"},
	{0x40600114, "UDCCSRE", "UDC Control/Status Register-Endpoint E"},
	{0x40600118, "UDCCSRF", "UDC Control/Status Register-Endpoint F"},
	{0x4060011C, "UDCCSRG", "UDC Control/Status Register-Endpoint G"},
	{0x40600120, "UDCCSRH", "UDC Control/Status Register-Endpoint H"},
	{0x40600124, "UDCCSRI", "UDC Control/Status Register-Endpoint I"},
	{0x40600128, "UDCCSRJ", "UDC Control/Status Register-Endpoint J"},
	{0x4060012C, "UDCCSRK", "UDC Control/Status Register-Endpoint K"},
	{0x40600130, "UDCCSRL", "UDC Control/Status Register-Endpoint L"},
	{0x40600134, "UDCCSRM", "UDC Control/Status Register-Endpoint M"},
	{0x40600138, "UDCCSRN", "UDC Control/Status Register-Endpoint N"},
	{0x4060013C, "UDCCSRP", "UDC Control/Status Register-Endpoint P"},
	{0x40600140, "UDCCSRQ", "UDC Control/Status Register-Endpoint Q"},
	{0x40600144, "UDCCSRR", "UDC Control/Status Register-Endpoint R"},
	{0x40600148, "UDCCSRS", "UDC Control/Status Register-Endpoint S"},
	{0x4060014C, "UDCCSRT", "UDC Control/Status Register-Endpoint T"},
	{0x40600150, "UDCCSRU", "UDC Control/Status Register-Endpoint U"},
	{0x40600154, "UDCCSRV", "UDC Control/Status Register-Endpoint V"},
	{0x40600158, "UDCCSRW", "UDC Control/Status Register-Endpoint W"},
	{0x4060015C, "UDCCSRX", "UDC Control/Status Register-Endpoint X"},
	{0x40600200, "UDCBCR0", "UDC Count Register-Endpoint 0"},
	{0x40600204, "UDCBCRA", "UDC Count Register-Endpoint A"},
	{0x40600208, "UDCBCRB", "UDC Count Register-Endpoint B"},
	{0x4060020C, "UDCBCRC", "UDC Count Register-Endpoint C"},
	{0x40600210, "UDCBCRD", "UDC Count Register-Endpoint D"},
	{0x40600214, "UDCBCRE", "UDC Count Register-Endpoint E"},
	{0x40600218, "UDCBCRF", "UDC Count Register-Endpoint F"},
	{0x4060021C, "UDCBCRG", "UDC Count Register-Endpoint G"},
	{0x40600220, "UDCBCRH", "UDC Count Register-Endpoint H"},
	{0x40600224, "UDCBCRI", "UDC Count Register-Endpoint I"},
	{0x40600228, "UDCBCRJ", "UDC Count Register-Endpoint J"},
	{0x4060022C, "UDCBCRK", "UDC Count Register-Endpoint K"},
	{0x40600230, "UDCBCRL", "UDC Count Register-Endpoint L"},
	{0x40600234, "UDCBCRM", "UDC Count Register-Endpoint M"},
	{0x40600238, "UDCBCRN", "UDC Count Register-Endpoint N"},
	{0x4060023C, "UDCBCRP", "UDC Count Register-Endpoint P"},
	{0x40600240, "UDCBCRQ", "UDC Count Register-Endpoint Q"},
	{0x40600244, "UDCBCRR", "UDC Count Register-Endpoint R"},
	{0x40600248, "UDCBCRS", "UDC Count Register-Endpoint S"},
	{0x4060024C, "UDCBCRT", "UDC Count Register-Endpoint T"},
	{0x40600250, "UDCBCRU", "UDC Count Register-Endpoint U"},
	{0x40600254, "UDCBCRV", "UDC Count Register-Endpoint V"},
	{0x40600258, "UDCBCRW", "UDC Count Register-Endpoint W"},
	{0x4060025C, "UDCBCRX", "UDC Count Register-Endpoint X"},
	{0x40600300, "UDCDR0", "UDC Data Register-Endpoint 0"},
	{0x40600304, "UDCDRA", "UDC Data Register-Endpoint A"},
	{0x40600308, "UDCDRB", "UDC Data Register-Endpoint B"},
	{0x4060030C, "UDCDRC", "UDC Data Register-Endpoint C"},
	{0x40600310, "UDCDRD", "UDC Data Register-Endpoint D"},
	{0x40600314, "UDCDRE", "UDC Data Register-Endpoint E"},
	{0x40600318, "UDCDRF", "UDC Data Register-Endpoint F"},
	{0x4060031C, "UDCDRG", "UDC Data Register-Endpoint G"},
	{0x40600320, "UDCDRH", "UDC Data Register-Endpoint H"},
	{0x40600324, "UDCDRI", "UDC Data Register-Endpoint I"},
	{0x40600328, "UDCDRJ", "UDC Data Register-Endpoint J"},
	{0x4060032C, "UDCDRK", "UDC Data Register-Endpoint K"},
	{0x40600330, "UDCDRL", "UDC Data Register-Endpoint L"},
	{0x40600334, "UDCDRM", "UDC Data Register-Endpoint M"},
	{0x40600338, "UDCDRN", "UDC Data Register-Endpoint N"},
	{0x4060033C, "UDCDRP", "UDC Data Register-Endpoint P"},
	{0x40600340, "UDCDRQ", "UDC Data Register-Endpoint Q"},
	{0x40600344, "UDCDRR", "UDC Data Register-Endpoint R"},
	{0x40600348, "UDCDRS", "UDC Data Register-Endpoint S"},
	{0x4060034C, "UDCDRT", "UDC Data Register-Endpoint T"},
	{0x40600350, "UDCDRU", "UDC Data Register-Endpoint U"},
	{0x40600354, "UDCDRV", "UDC Data Register-Endpoint V"},
	{0x40600358, "UDCDRW", "UDC Data Register-Endpoint W"},
	{0x4060035C, "UDCDRX", "UDC Data Register-Endpoint X"},
	{0x40600404, "UDCCRA", "UDC Configuration Register-Endpoint A"},
	{0x40600408, "UDCCRB", "UDC Configuration Register-Endpoint B"},
	{0x4060040C, "UDCCRC", "UDC Configuration Register-Endpoint C"},
	{0x40600410, "UDCCRD", "UDC Configuration Register-Endpoint D"},
	{0x40600414, "UDCCRE", "UDC Configuration Register-Endpoint E"},
	{0x40600418, "UDCCRF", "UDC Configuration Register-Endpoint F"},
	{0x4060041C, "UDCCRG", "UDC Configuration Register-Endpoint G"},
	{0x40600420, "UDCCRH", "UDC Configuration Register-Endpoint H"},
	{0x40600424, "UDCCRI", "UDC Configuration Register-Endpoint I"},
	{0x40600428, "UDCCRJ", "UDC Configuration Register-Endpoint J"},
	{0x4060042C, "UDCCRK", "UDC Configuration Register-Endpoint K"},
	{0x40600430, "UDCCRL", "UDC Configuration Register-Endpoint L"},
	{0x40600434, "UDCCRM", "UDC Configuration Register-Endpoint M"},
	{0x40600438, "UDCCRN", "UDC Configuration Register-Endpoint N"},
	{0x4060043C, "UDCCRP", "UDC Configuration Register-Endpoint P"},
	{0x40600440, "UDCCRQ", "UDC Configuration Register-Endpoint Q"},
	{0x40600444, "UDCCRR", "UDC Configuration Register-Endpoint R"},
	{0x40600448, "UDCCRS", "UDC Configuration Register-Endpoint S"},
	{0x4060044C, "UDCCRT", "UDC Configuration Register-Endpoint T"},
	{0x40600450, "UDCCRU", "UDC Configuration Register-Endpoint U"},
	{0x40600454, "UDCCRV", "UDC Configuration Register-Endpoint V"},
	{0x40600458, "UDCCRW", "UDC Configuration Register-Endpoint W"},
	{0x4060045C, "UDCCRX", "UDC Configuration Register-Endpoint X"},
#else
	{0x40600010, "UDCCS0", "UDC Endpoint 0 Control/Status Register"},
	{0x40600014, "UDCCS1", "UDC Endpoint 1 (IN) Control/Status Register"},
	{0x40600018, "UDCCS2", "UDC Endpoint 2 (OUT) Control/Status Register"},
	{0x4060001C, "UDCCS3", "UDC Endpoint 3 (IN) Control/Status Register"},
	{0x40600020, "UDCCS4", "UDC Endpoint 4 (OUT) Control/Status Register"},
	{0x40600024, "UDCCS5", "UDC Endpoint 5 (Interrupt) Control/Status Register"},
	{0x40600028, "UDCCS6", "UDC Endpoint 6 (IN) Control/Status Register"},
	{0x4060002C, "UDCCS7", "UDC Endpoint 7 (OUT) Control/Status Register"},
	{0x40600030, "UDCCS8", "UDC Endpoint 8 (IN) Control/Status Register"},
	{0x40600034, "UDCCS9", "UDC Endpoint 9 (OUT) Control/Status Register"},
	{0x40600038, "UDCCS10", "UDC Endpoint 10 (Interrupt) Control/Status Register"},
	{0x4060003C, "UDCCS11", "UDC Endpoint 11 (IN) Control/Status Register"},
	{0x40600040, "UDCCS12", "UDC Endpoint 12 (OUT) Control/Status Register"},
	{0x40600044, "UDCCS13", "UDC Endpoint 13 (IN) Control/Status Register"},
	{0x40600048, "UDCCS14", "UDC Endpoint 14 (OUT) Control/Status Register"},
	{0x4060004C, "UDCCS15", "UDC Endpoint 15 (Interrupt) Control/Status Register"},
	{0x40600060, "UFNRH", "UDC Frame Number Register High"},
	{0x40600064, "UFNRL", "UDC Frame Number Register Low"},
	{0x40600068, "UBCR2", "UDC Byte Count Register 2"},
	{0x4060006C, "UBCR4", "UDC Byte Count Register 4"},
	{0x40600070, "UBCR7", "UDC Byte Count Register 7"},
	{0x40600074, "UBCR9", "UDC Byte Count Register 9"},
	{0x40600078, "UBCR12", "UDC Byte Count Register 12"},
	{0x4060007C, "UBCR14", "UDC Byte Count Register 14"},
	{0x40600080, "UDDR0", "UDC Endpoint 0 Data Register"},
	{0x40600100, "UDDR1", "UDC Endpoint 1 Data Register"},
	{0x40600180, "UDDR2", "UDC Endpoint 2 Data Register"},
	{0x40600200, "UDDR3", "UDC Endpoint 3 Data Register"},
	{0x40600400, "UDDR4", "UDC Endpoint 4 Data Register"},
	{0x406000A0, "UDDR5", "UDC Endpoint 5 Data Register"},
	{0x40600600, "UDDR6", "UDC Endpoint 6 Data Register"},
	{0x40600680, "UDDR7", "UDC Endpoint 7 Data Register"},
	{0x40600700, "UDDR8", "UDC Endpoint 8 Data Register"},
	{0x40600900, "UDDR9", "UDC Endpoint 9 Data Register"},
	{0x406000C0, "UDDR10", "UDC Endpoint 10 Data Register"},
	{0x40600B00, "UDDR11", "UDC Endpoint 11 Data Register"},
	{0x40600B80, "UDDR12", "UDC Endpoint 12 Data Register"},
	{0x40600C00, "UDDR13", "UDC Endpoint 13 Data Register"},
	{0x40600E00, "UDDR14", "UDC Endpoint 14 Data Register"},
	{0x406000E0, "UDDR15", "UDC Endpoint 15 Data Register"},
	{0x40600050, "UICR0", "UDC Interrupt Control Register 0"},
	{0x40600054, "UICR1", "UDC Interrupt Control Register 1"},
	{0x40600058, "USIR0", "UDC Status Interrupt Register 0"},
	{0x4060005C, "USIR1", "UDC Status Interrupt Register 1"},
#endif
	{0x40700000, "STRBR", "Receive Buffer Register (read only)"},
	{0x40700000, "STTHR", "Transmit Holding Register (write only)"},
	{0x40700004, "STIER", "Interrupt Enable Register (read/write)"},
	{0x40700008, "STIIR", "Interrupt ID Register (read only)"},
	{0x40700008, "STFCR", "FIFO Control Register (write only)"},
	{0x4070000C, "STLCR", "Line Control Register (read/write)"},
	{0x40700010, "STMCR", "Modem Control Register (read/write)"},
	{0x40700014, "STLSR", "Line Status Register (read only)"},
	{0x4070001C, "STSPR", "Scratch Pad Register (read/write)"},
	{0x40700020, "STISR", "Infrared Selection Register (read/write)"},
	{0x40700000, "STDLL", "Divisor Latch Low Register (DLAB = 1) (read/write)"},
	{0x40700004, "STDLH", "Divisor Latch High Register (DLAB = 1) (read/write)"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40700024, "STFOR", "Receive FIFO Occupancy Register"},
	{0x40700028, "STABR", "Auto-baud Control Register"},
	{0x4070002C, "STACR", "Auto-baud Count Register"},
#endif
	{0x40800000, "ICCR0", "ICP Control Register 0"},
	{0x40800004, "ICCR1", "ICP Control Register 1"},
	{0x40800008, "ICCR2", "ICP Control Register 2"},
	{0x4080000C, "ICDR", "ICP Data Register"},
	{0x40800014, "ICSR0", "ICP Status Register 0"},
	{0x40800018, "ICSR1", "ICP Status Register 1"},
#if defined(CONFIG_CPU_PXA27X)
	{0x4080001C, "ICFOR", "FICP FIFO Occupancy Status Register"},
#endif
	{0x40900000, "RCNR", "RTC Count Register"},
	{0x40900004, "RTAR", "RTC Alarm Register"},
	{0x40900008, "RTSR", "RTC Status Register"},
	{0x4090000C, "RTTR", "RTC Timer Trim Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40900010, "RDCR", "RTC Day Count Register"},
	{0x40900014, "RYCR", "RTC Year Count Register"},
	{0x40900018, "RDAR1", "RTC Wristwatch Day Alarm Register 1"},
	{0x4090001C, "RYAR1", "RTC Wristwatch Year Alarm Register 1"},
	{0x40900020, "RDAR2", "RTC Wristwatch Day Alarm Register 2"},
	{0x40900024, "RYAR2", "RTC Wristwatch Year Alarm Register 2"},
	{0x40900028, "SWCR", "RTC Stopwatch Counter Register"},
	{0x4090002C, "SWAR1", "RTC Stopwatch Alarm Register 1"},
	{0x40900030, "SWAR2", "RTC Stopwatch Alarm Register 2"},
	{0x40900034, "RTCPICR", "RTC Periodic Interrupt Counter Register"},
	{0x40900038, "PIAR", "RTC Periodic Interrupt Alarm Register"},
#endif
	{0x40A00000, "OSMR0", "OS Timer Match 0 Registers"},
	{0x40A00004, "OSMR1", "OS Timer Match 1 Registers"},
	{0x40A00008, "OSMR2", "OS Timer Match 2 Registers"},
	{0x40A0000C, "OSMR3", "OS Timer Match 3 Registers"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40A00010, "OSCR0", "OS Timer Counter 0 Register"},
#else
	{0x40A00010, "OSCR", "OS Timer Counter Register"},
#endif
	{0x40A00014, "OSSR", "OS Timer Status Register"},
	{0x40A00018, "OWER", "OS Timer Watchdog Enable Register"},
	{0x40A0001C, "OIER", "OS Timer Interrupt Enable Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40A00020, "OSNR", "OS Timer Snapshot Register"},
	{0x40A00040, "OSCR4", "OS Timer Counter 4 Register"},
	{0x40A00044, "OSCR5", "OS Timer Counter 5 Register"},
	{0x40A00048, "OSCR6", "OS Timer Counter 6 Register"},
	{0x40A0004C, "OSCR7", "OS Timer Counter 7 Register"},
	{0x40A00050, "OSCR8", "OS Timer Counter 8 Register"},
	{0x40A00054, "OSCR9", "OS Timer Counter 9 Register"},
	{0x40A00058, "OSCR10", "OS Timer Counter 10 Register"},
	{0x40A0005C, "OSCR11", "OS Timer Counter 11 Register"},
	{0x40A00080, "OSMR4", "OS Timer Match 4 Registers"},
	{0x40A00084, "OSMR5", "OS Timer Match 5 Registers"},
	{0x40A00088, "OSMR6", "OS Timer Match 6 Registers"},
	{0x40A0008C, "OSMR7", "OS Timer Match 7 Registers"},
	{0x40A00090, "OSMR8", "OS Timer Match 8 Registers"},
	{0x40A00094, "OSMR9", "OS Timer Match 9 Registers"},
	{0x40A00098, "OSMR10", "OS Timer Match 10 Registers"},
	{0x40A0009C, "OSMR11", "OS Timer Match 11 Registers"},
	{0x40A000C0, "OMCR4", "OS Match Control 4 Registers"},
	{0x40A000C4, "OMCR5", "OS Match Control 5 Registers"},
	{0x40A000C8, "OMCR6", "OS Match Control 6 Registers"},
	{0x40A000CC, "OMCR7", "OS Match Control 7 Registers"},
	{0x40A000D0, "OMCR8", "OS Match Control 8 Registers"},
	{0x40A000D4, "OMCR9", "OS Match Control 9 Registers"},
	{0x40A000D8, "OMCR10", "OS Match Control 10 Registers"},
	{0x40A000DC, "OMCR11", "OS Match Control 11 Registers"},
#endif
#if defined(CONFIG_CPU_PXA27X)
	{0x40B00000, "PWMCR0", "PWM 0 Control Register"},
	{0x40B00004, "PWMDCR0", "PWM 0 Duty Cycle Register"},
	{0x40B00008, "PWMPCR0", "PWM 0 Period Register"},
	{0x40B00010, "PWMCR2", "PWM 2 Control Register"},
	{0x40B00014, "PWMDCR2", "PWM 2 Duty Cycle Register"},
	{0x40B00018, "PWMPCR2", "PWM 2 Period Register"},
	{0x40C00000, "PWMCR1", "PWM 1 Control Register"},
	{0x40C00004, "PWMDCR1", "PWM 1 Duty Cycle Register"},
	{0x40C00008, "PWMPCR1", "PWM 1 Period Register"},
	{0x40C00010, "PWMCR3", "PWM 3 Control Register"},
	{0x40C00014, "PWMDCR3", "PWM 3 Duty Cycle Register"},
	{0x40C00018, "PWMPCR3", "PWM 3 Period Register"},
#else
	{0x40B00000, "PWM_CTRL0", "PWM 0 Control Register"},
	{0x40B00004, "PWM_PWDUTY0", "PWM 0 Duty Cycle Register"},
	{0x40B00008, "PWM_PERVAL0", "PWM 0 Period Control Register"},
	{0x40C00000, "PWM_CTRL1", "PWM 1Control Register"},
	{0x40C00004, "PWM_PWDUTY1", "PWM 1 Duty Cycle Register"},
	{0x40C00008, "PWM_PERVAL1", "PWM 1 Period Control Register"},
#endif
	{0x40D00000, "ICIP", "Interrupt Controller IRQ Pending Register"},
	{0x40D00004, "ICMR", "Interrupt Controller Mask Register"},
	{0x40D00008, "ICLR", "Interrupt Controller Level Register"},
	{0x40D0000C, "ICFP", "Interrupt Controller FIQ Pending Register"},
	{0x40D00010, "ICPR", "Interrupt Controller Pending Register"},
	{0x40D00014, "ICCR", "Interrupt Controller Control Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40D00018, "ICHP", "Interrupt Controller Highest Priority Register"},
	{0x40D0001C, "IPR0", "Interrupt Priority Register for Priorities 0"},
	{0x40D00020, "IPR1", "Interrupt Priority Register for Priorities 1"},
	{0x40D00024, "IPR2", "Interrupt Priority Register for Priorities 2"},
	{0x40D00028, "IPR3", "Interrupt Priority Register for Priorities 3"},
	{0x40D0002C, "IPR4", "Interrupt Priority Register for Priorities 4"},
	{0x40D00030, "IPR5", "Interrupt Priority Register for Priorities 5"},
	{0x40D00034, "IPR6", "Interrupt Priority Register for Priorities 6"},
	{0x40D00038, "IPR7", "Interrupt Priority Register for Priorities 7"},
	{0x40D0003C, "IPR8", "Interrupt Priority Register for Priorities 8"},
	{0x40D00040, "IPR9", "Interrupt Priority Register for Priorities 9"},
	{0x40D00044, "IPR10", "Interrupt Priority Register for Priorities 10"},
	{0x40D00048, "IPR11", "Interrupt Priority Register for Priorities 11"},
	{0x40D0004C, "IPR12", "Interrupt Priority Register for Priorities 12"},
	{0x40D00050, "IPR13", "Interrupt Priority Register for Priorities 13"},
	{0x40D00054, "IPR14", "Interrupt Priority Register for Priorities 14"},
	{0x40D00058, "IPR15", "Interrupt Priority Register for Priorities 15"},
	{0x40D0005C, "IPR16", "Interrupt Priority Register for Priorities 16"},
	{0x40D00060, "IPR17", "Interrupt Priority Register for Priorities 17"},
	{0x40D00064, "IPR18", "Interrupt Priority Register for Priorities 18"},
	{0x40D00068, "IPR19", "Interrupt Priority Register for Priorities 19"},
	{0x40D0006C, "IPR20", "Interrupt Priority Register for Priorities 20"},
	{0x40D00070, "IPR21", "Interrupt Priority Register for Priorities 21"},
	{0x40D00074, "IPR22", "Interrupt Priority Register for Priorities 22"},
	{0x40D00078, "IPR23", "Interrupt Priority Register for Priorities 23"},
	{0x40D0007C, "IPR24", "Interrupt Priority Register for Priorities 24"},
	{0x40D00080, "IPR25", "Interrupt Priority Register for Priorities 25"},
	{0x40D00084, "IPR26", "Interrupt Priority Register for Priorities 26"},
	{0x40D00088, "IPR27", "Interrupt Priority Register for Priorities 27"},
	{0x40D0008C, "IPR28", "Interrupt Priority Register for Priorities 28"},
	{0x40D00090, "IPR29", "Interrupt Priority Register for Priorities 29"},
	{0x40D00094, "IPR30", "Interrupt Priority Register for Priorities 30"},
	{0x40D00098, "IPR31", "Interrupt Priority Register for Priorities 31"},
	{0x40D0009C, "ICIP2", "Interrupt Controller IRQ Pending Register 2"},
	{0x40D000A0, "ICMR2", "Interrupt Controller Mask Register 2"},
	{0x40D000A4, "ICLR2", "Interrupt Controller Level Register 2"},
	{0x40D000A8, "ICFP2", "Interrupt Controller FIQ Pending Register 2"},
	{0x40D000AC, "ICPR2", "Interrupt Controller Pending Register 2"},
	{0x40D000B0, "IPR32", "Interrupt Priority Register for Priorities 32"},
	{0x40D000B4, "IPR33", "Interrupt Priority Register for Priorities 33"},
	{0x40D000B8, "IPR34", "Interrupt Priority Register for Priorities 34"},
	{0x40D000BC, "IPR35", "Interrupt Priority Register for Priorities 35"},
	{0x40D000C0, "IPR36", "Interrupt Priority Register for Priorities 36"},
	{0x40D000C4, "IPR37", "Interrupt Priority Register for Priorities 37"},
	{0x40D000C8, "IPR38", "Interrupt Priority Register for Priorities 38"},
	{0x40D000CC, "IPR39", "Interrupt Priority Register for Priorities 39"},
#endif
	{0x40E00000, "GPLR0", "GPIO Pin-Level Register GPIO<31:0>"},
	{0x40E00004, "GPLR1", "GPIO Pin-Level Register GPIO<63:32>"},
	{0x40E00008, "GPLR2", "GPIO Pin-Level Register GPIO<80:64>"},
	{0x40E0000C, "GPDR0", "GPIO Pin Direction Register GPIO<31:0>"},
	{0x40E00010, "GPDR1", "GPIO Pin Direction Register GPIO<63:32>"},
	{0x40E00014, "GPDR2", "GPIO Pin Direction Register GPIO<80:64>"},
	{0x40E00018, "GPSR0", "GPIO Pin Direction Register GPIO<31:0>"},
	{0x40E0001C, "GPSR1", "GPIO Pin Output Set Register GPIO<63:32>"},
	{0x40E00020, "GPSR2", "GPIO Pin Output Set Register GPIO<80:64>"},
	{0x40E00024, "GPCR0", "GPIO Pin Output Clear Register GPIO<31:0>"},
	{0x40E00028, "GPCR1", "GPIO Pin Output Clear Register GPIO <63:32>"},
	{0x40E0002C, "GPCR2", "GPIO Pin Output Clear Register GPIO <80:64>"},
	{0x40E00030, "GRER0", "GPIO Rising-Edge Detect Register GPIO<31:0>"},
	{0x40E00034, "GRER1", "GPIO Rising-Edge Detect Register GPIO<63:32>"},
	{0x40E00038, "GRER2", "GPIO Rising-Edge Detect Register GPIO<80:64>"},
	{0x40E0003C, "GFER0", "GPIO Falling-Edge Detect Register GPIO<31:0>"},
	{0x40E00040, "GFER1", "GPIO Falling-Edge Detect Register GPIO<63:32>"},
	{0x40E00044, "GFER2", "GPIO Falling-Edge Detect Register GPIO<80:64>"},
	{0x40E00048, "GEDR0", "GPIO Edge Detect Status Register GPIO<31:0>"},
	{0x40E0004C, "GEDR1", "GPIO Edge Detect Status Register GPIO<63:32>"},
	{0x40E00050, "GEDR2", "GPIO Edge Detect Status Register GPIO<80:64>"},
	{0x40E00054, "GAFR0_L", "GPIO Alternate Function Select Register GPIO<15:0>"},
	{0x40E00058, "GAFR0_U", "GPIO Alternate Function Select Register GPIO<31:16>"},
	{0x40E0005C, "GAFR1_L", "GPIO Alternate Function Select Register GPIO<47:32>"},
	{0x40E00060, "GAFR1_U", "GPIO Alternate Function Select Register GPIO<63:48>"},
	{0x40E00064, "GAFR2_L", "GPIO Alternate Function Select Register GPIO<79:64>"},
#if !defined(CONFIG_CPU_PXA27X)
	{0x40E00068, "GAFR2_U", "GPIO Alternate Function Select Register GPIO 80"},
#else
	{0x40E00068, "GAFR2_U", "GPIO Alternate Function Select Register GPIO<95:80>"},
	{0x40E0006C, "GAFR3_L", "GPIO Alternate Function Select Register GPIO<111:96>"},
	{0x40E00070, "GAFR3_U", "GPIO Alternate Function Select Register GPIO<120:112>"},
	{0x40E00100, "GPLR3", "GPIO Pin-Level Register GPIO<120:96>"},
	{0x40E0010C, "GPDR3", "GPIO Pin Direction Register GPIO<120:96>"},
	{0x40E00118, "GPSR3", "GPIO Pin Direction Register GPIO<120:96>"},
	{0x40E00124, "GPCR3", "GPIO Pin Output Clear Register GPIO<120:96>"},
	{0x40E00130, "GRER3", "GPIO Rising-Edge Detect Register GPIO<120:96>"},
	{0x40E0013C, "GFER3", "GPIO Falling-Edge Detect Register GPIO<120:96>"},
	{0x40E00148, "GEDR3", "GPIO Edge Detect Status Register GPIO<120:96>"},
#endif
	{0x40F00000, "PMCR", "Power Manager Control Register"},
	{0x40F00004, "PSSR", "Power Manager Sleep Status Register"},
	{0x40F00008, "PSPR", "Power Manager Scratch Pad Register"},
	{0x40F0000C, "PWER", "Power Manager Wake-up Enable Register"},
	{0x40F00010, "PRER", "Power Manager GPIO Rising-Edge Detect Enable Register"},
	{0x40F00014, "PFER", "Power Manager GPIO Falling-Edge Detect Enable Register"},
	{0x40F00018, "PEDR", "Power Manager GPIO Edge Detect Status Register"},
	{0x40F0001C, "PCFR", "Power Manager General Configuration Register"},
	{0x40F00020, "PGSR0", "Power Manager GPIO Sleep State Register for GP[31-0]"},
	{0x40F00024, "PGSR1", "Power Manager GPIO Sleep State Register for GP[63-32]"},
	{0x40F00028, "PGSR2", "Power Manager GPIO Sleep State Register for GP[84-64]"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40F0002C, "PGSR3", "Power Manager GPIO Sleep State Register for GP[120-96]"},
#endif
	{0x40F00030, "RCSR", "Reset Controller Status Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x40F00034, "PSLR", "Power Manager Sleep Configuration Register"},
	{0x40F00038, "PSTR", "Power Manager Standby Configuration Register"},
	{0x40F00040, "PVCR", "Power Manager Voltage Change Control Register"},
	{0x40F0004C, "PUCR", "Power Manager USIM Card Control/Status Register"},
	{0x40F00050, "PKWR", "Power Manager Keyboard Wake-Up Enable Register"},
	{0x40F00054, "PKSR", "Power Manager Keyboard Level-Detect Status Register"},
	{0x40F00080, "PCMD0", "Power Manager I2C Command Register File 0"},
	{0x40F00084, "PCMD1", "Power Manager I2C Command Register File 1"},
	{0x40F00088, "PCMD2", "Power Manager I2C Command Register File 2"},
	{0x40F0008C, "PCMD3", "Power Manager I2C Command Register File 3"},
	{0x40F00090, "PCMD4", "Power Manager I2C Command Register File 4"},
	{0x40F00094, "PCMD5", "Power Manager I2C Command Register File 5"},
	{0x40F00098, "PCMD6", "Power Manager I2C Command Register File 6"},
	{0x40F0009C, "PCMD7", "Power Manager I2C Command Register File 7"},
	{0x40F000A0, "PCMD8", "Power Manager I2C Command Register File 8"},
	{0x40F000A4, "PCMD9", "Power Manager I2C Command Register File 9"},
	{0x40F000A8, "PCMD10", "Power Manager I2C Command Register File 10"},
	{0x40F000AC, "PCMD11", "Power Manager I2C Command Register File 11"},
	{0x40F000B0, "PCMD12", "Power Manager I2C Command Register File 12"},
	{0x40F000B4, "PCMD13", "Power Manager I2C Command Register File 13"},
	{0x40F000B8, "PCMD14", "Power Manager I2C Command Register File 14"},
	{0x40F000BC, "PCMD15", "Power Manager I2C Command Register File 15"},
	{0x40F000C0, "PCMD16", "Power Manager I2C Command Register File 16"},
	{0x40F000C4, "PCMD17", "Power Manager I2C Command Register File 17"},
	{0x40F000C8, "PCMD18", "Power Manager I2C Command Register File 18"},
	{0x40F000CC, "PCMD19", "Power Manager I2C Command Register File 19"},
	{0x40F000D0, "PCMD20", "Power Manager I2C Command Register File 20"},
	{0x40F000D4, "PCMD21", "Power Manager I2C Command Register File 21"},
	{0x40F000D8, "PCMD22", "Power Manager I2C Command Register File 22"},
	{0x40F000DC, "PCMD23", "Power Manager I2C Command Register File 23"},
	{0x40F000E0, "PCMD24", "Power Manager I2C Command Register File 24"},
	{0x40F000E4, "PCMD25", "Power Manager I2C Command Register File 25"},
	{0x40F000E8, "PCMD26", "Power Manager I2C Command Register File 26"},
	{0x40F000EC, "PCMD27", "Power Manager I2C Command Register File 27"},
	{0x40F000F0, "PCMD28", "Power Manager I2C Command Register File 28"},
	{0x40F000F4, "PCMD29", "Power Manager I2C Command Register File 29"},
	{0x40F000F8, "PCMD30", "Power Manager I2C Command Register File 30"},
	{0x40F000FC, "PCMD31", "Power Manager I2C Command Register File 31"},
	{0x40F00180, "PIBMR", "Power Manager I2C Bus Monitor Register"},
	{0x40F00188, "PIDBR", "Power Manager I2C Data Buffer Register"},
	{0x40F00190, "PICR", "Power Manager I2C Control Register"},
	{0x40F00198, "PISR", "Power Manager I2C Status Register"},
	{0x40F001A0, "PISAR", "Power Manager I2C Slave Address Register"},
#endif
#if defined(CONFIG_CPU_PXA27X)
	{0x41000000, "SSCR0_1", "SSP 1 Control Register 0"},
	{0x41000004, "SSCR1_1", "SSP 1 Control Register 1"},
	{0x41000008, "SSSR_1", "SSP 1 Status Register"},
	{0x4100000C, "SSITR_1", "SSP 1 Interrupt Test Register"},
	{0x41000010, "SSDR_1", "SSP 1 Data Write Register/SSP Data Read Register"},
	{0x41000028, "SSTO_1", "SSP 1 Time-Out Register"},
	{0x4100002C, "SSPSP_1", "SSP 1 Programmable Serial Protocol"},
	{0x41000030, "SSTSA_1", "SSP 1 TX Timeslot Active Register"},
	{0x41000034, "SSRSA_1", "SSP 1 RX Timeslot Active Register"},
	{0x41000038, "SSTSS_1", "SSP 1 Timeslot Status Register"},
	{0x4100003C, "SSACD_1", "SSP 1 Audio Clock Divider Register"},
#else
	{0x41000000, "SSCR0", "SSP Control Register 0"},
	{0x41000004, "SSCR1", "SSP Control Register 1"},
	{0x41000008, "SSSR", "SSP Status Register"},
	{0x4100000C, "SSITR", "SSP Interrupt Test Register"},
	{0x41000010, "SSDR", "(Write / Read) SSP Data Write Register/SSP Data Read Register"},
#endif
	{0x41100000, "MMC_STRPCL", "Control to start and stop MMC clock"},
	{0x41100004, "MMC_STAT", "MMC Status Register (read only)"},
	{0x41100008, "MMC_CLKRT", "MMC clock rate"},
	{0x4110000C, "MMC_SPI", "SPI mode control bits"},
	{0x41100010, "MMC_CMDAT", "Command/response/data sequence control"},
	{0x41100014, "MMC_RESTO", "Expected response time out"},
	{0x41100018, "MMC_RDTO", "Expected data read time out"},
	{0x4110001C, "MMC_BLKLEN", "Block length of data transaction"},
#if defined(CONFIG_CPU_PXA27X)
	{0x41100020, "MMC_BLKLEN", "Number of blocks, for block mode"},
#else
	{0x41100020, "MMC_NOB", "Number of blocks, for block mode"},
#endif
	{0x41100024, "MMC_PRTBUF", "Partial MMC_TXFIFO FIFO written"},
	{0x41100028, "MMC_I_MASK", "Interrupt Mask"},
	{0x4110002C, "MMC_I_REG", "Interrupt Register (read only)"},
	{0x41100030, "MMC_CMD", "Index of current command"},
	{0x41100034, "MMC_ARGH", "MSW part of the current command argument"},
	{0x41100038, "MMC_ARGL", "LSW part of the current command argument"},
	{0x4110003C, "MMC_RES", "Response FIFO (read only)"},
	{0x41100040, "MMC_RXFIFO", "Receive FIFO (read only)"},
	{0x41100044, "MMC_TXFIFO", "Transmit FIFO (write only)"},
#if defined(CONFIG_CPU_PXA27X)
	{0x41100048, "MMC_RDWAIT", "MMC RD_WAIT Register"},
	{0x4110004C, "MMC_BLKS_REM", "MMC Blocks Remaining Register"},
#endif
	{0x41300000, "CCCR", "Core Clock Configuration Register"},
	{0x41300004, "CKEN", "Clock Enable Register"},
	{0x41300008, "OSCC", "Oscillator Configuration Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x4130000C, "CCSR", "Core Clock Status Register"},
#endif
#if defined(CONFIG_CPU_PXA27X)
	{0x41700000, "SSCR0_2", "SSP 2 Control Register 0"},
	{0x41700004, "SSCR1_2", "SSP 2 Control Register 1"},
	{0x41700008, "SSSR_2", "SSP 2 Status Register"},
	{0x4170000C, "SSITR_2", "SSP 2 Interrupt Test Register"},
	{0x41700010, "SSDR_2", "SSP 2 Data Write Register/SSP Data Read Register"},
	{0x41700028, "SSTO_2", "SSP 2 Time-Out Register"},
	{0x4170002C, "SSPSP_2", "SSP 2 Programmable Serial Protocol"},
	{0x41700030, "SSTSA_2", "SSP 2 TX Timeslot Active Register"},
	{0x41700034, "SSRSA_2", "SSP 2 RX Timeslot Active Register"},
	{0x41700038, "SSTSS_2", "SSP 2 Timeslot Status Register"},
	{0x4170003C, "SSACD_2", "SSP 2 Audio Clock Divider Register"},
	{0x41900000, "SSCR0_3", "SSP 3 Control Register 0"},
	{0x41900004, "SSCR1_3", "SSP 3 Control Register 1"},
	{0x41900008, "SSSR_3", "SSP 3 Status Register"},
	{0x4190000C, "SSITR_3", "SSP 3 Interrupt Test Register"},
	{0x41900010, "SSDR_3", "SSP 3 Data Write Register/SSP Data Read Register"},
	{0x41900028, "SSTO_3", "SSP 3 Time-Out Register"},
	{0x4190002C, "SSPSP_3", "SSP 3 Programmable Serial Protocol"},
	{0x41900030, "SSTSA_3", "SSP 3 TX Timeslot Active Register"},
	{0x41900034, "SSRSA_3", "SSP 3 RX Timeslot Active Register"},
	{0x41900038, "SSTSS_3", "SSP 3 Timeslot Status Register"},
	{0x4190003C, "SSACD_3", "SSP 3 Audio Clock Divider Register"},
#endif
	{0x44000000, "LCCR0", "LCD Controller Control Register 0"},
	{0x44000004, "LCCR1", "LCD Controller Control Register 1"},
	{0x44000008, "LCCR2", "LCD Controller Control Register 2"},
	{0x4400000C, "LCCR3", "LCD Controller Control Register 3"},
#if defined(CONFIG_CPU_PXA27X)
	{0x44000010, "LCCR4", "LCD Controller Control Register 4"},
	{0x44000014, "LCCR5", "LCD Controller Control Register 5"},
#endif
	{0x44000200, "FDADR0", "DMA Channel 0 Frame Descriptor Address Register"},
	{0x44000204, "FSADR0", "DMA Channel 0 Frame Source Address Register"},
	{0x44000208, "FIDR0", "DMA Channel 0 Frame ID Register"},
	{0x4400020C, "LDCMD0", "DMA Channel 0 Command Register"},
	{0x44000210, "FDADR1", "DMA Channel 1 Frame Descriptor Address Register"},
	{0x44000214, "FSADR1", "DMA Channel 1 Frame Source Address Register"},
	{0x44000218, "FIDR1", "DMA Channel 1 Frame ID Register"},
	{0x4400021C, "LDCMD1", "DMA Channel 1 Command Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x44000220, "FDADR2", "DMA Channel 2 Frame Descriptor Address Register"},
	{0x44000224, "FSADR2", "DMA Channel 2 Frame Source Address Register"},
	{0x44000228, "FIDR2", "DMA Channel 2 Frame ID Register"},
	{0x4400022C, "LDCMD2", "DMA Channel 2 Command Register"},
	{0x44000230, "FDADR3", "DMA Channel 3 Frame Descriptor Address Register"},
	{0x44000234, "FSADR3", "DMA Channel 3 Frame Source Address Register"},
	{0x44000238, "FIDR3", "DMA Channel 3 Frame ID Register"},
	{0x4400023C, "LDCMD3", "DMA Channel 3 Command Register"},
	{0x44000240, "FDADR4", "DMA Channel 4 Frame Descriptor Address Register"},
	{0x44000244, "FSADR4", "DMA Channel 4 Frame Source Address Register"},
	{0x44000248, "FIDR4", "DMA Channel 4 Frame ID Register"},
	{0x4400024C, "LDCMD4", "DMA Channel 4 Command Register"},
	{0x44000250, "FDADR5", "DMA Channel 5 Frame Descriptor Address Register"},
	{0x44000254, "FSADR5", "DMA Channel 5 Frame Source Address Register"},
	{0x44000258, "FIDR5", "DMA Channel 5 Frame ID Register"},
	{0x4400025C, "LDCMD5", "DMA Channel 5 Command Register"},
	{0x44000260, "FDADR6", "DMA Channel 6 Frame Descriptor Address Register"},
	{0x44000264, "FSADR6", "DMA Channel 6 Frame Source Address Register"},
	{0x44000268, "FIDR6", "DMA Channel 6 Frame ID Register"},
	{0x4400026C, "LDCMD6", "DMA Channel 6 Command Register"},
#endif
	{0x44000020, "FBR0", "DMA Channel 0 Frame Branch Register"},
	{0x44000024, "FBR1", "DMA Channel 1 Frame Branch Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x44000028, "FBR2", "DMA Channel 2 Frame Branch Register"},
	{0x4400002C, "FBR3", "DMA Channel 3 Frame Branch Register"},
	{0x44000030, "FBR4", "DMA Channel 4 Frame Branch Register"},
	{0x44000034, "LCSR1", "LCD Controller Status Register 1"},
	{0x44000038, "LCSR0", "LCD Controller Status Register 0"},
#else
	{0x44000038, "LCSR", "LCD Controller Status Register"},
#endif
	{0x4400003C, "LIIDR", "LCD Controller Interrupt ID Register"},
	{0x44000040, "TRGBR", "TMED RGB Seed Register"},
	{0x44000044, "TCR", "TMED Control Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x44000050, "OVL1C1", "Overlay 1 Control Register 1"},
	{0x44000060, "OVL1C2", "Overlay 1 Control Register 2"},
	{0x44000070, "OVL2C1", "Overlay 2 Control Register 1"},
	{0x44000080, "OVL2C2", "Overlay 2 Control Register 2"},
	{0x44000090, "CCR", "Cursor Control Register"},
	{0x44000100, "CMDCR", "Command Control Register"},
	{0x44000104, "PRSR", "Panel Read Status Register"},
	{0x44000110, "FBR5", "DMA Channel 5 Frame Branch Register"},
	{0x44000114, "FBR6", "DMA Channel 6 Frame Branch Register"},
#endif
	{0x48000000, "MDCNFG", "SDRAM Configuration Register 0"},
	{0x48000004, "MDREFR", "SDRAM Refresh Control Register"},
	{0x48000008, "MSC0", "Static Memory Control Register 0"},
	{0x4800000C, "MSC1", "Static Memory Control Register 1"},
	{0x48000010, "MSC2", "Static Memory Control Register 2"},
	{0x48000014, "MECR", "Expansion Memory (PCMCIA/Compact Flash) Bus Configuration Register"},
	{0x4800001C, "SXCNFG", "Synchronous Static Memory Control Register"},
#if defined(CONFIG_CPU_PXA27X)
	{0x48000020, "FLYCNFG", "Fly-by DMA DVAL<1:0> polarities"},
#endif
#if !defined(CONFIG_CPU_PXA27X)
	{0x48000024, "SXMRS", "MRS value to be written to SMROM"},
#endif
	{0x48000028, "MCMEM0", "Card interface Common Memory Space Socket 0 Timing Configuration"},
	{0x4800002C, "MCMEM1", "Card interface Common Memory Space Socket 1 Timing Configuration"},
	{0x48000030, "MCATT0", "Card interface Attribute Space Socket 0 Timing Configuration"},
	{0x48000034, "MCATT1", "Card interface Attribute Space Socket 1 Timing Configuration"},
	{0x48000038, "MCIO0", "Card interface I/O Space Socket 0 Timing Configuration"},
	{0x4800003C, "MCIO1", "Card interface I/O Space Socket 1 Timing Configuration"},
	{0x48000040, "MDMRS", "MRS value to be written to SDRAM"},
	{0x48000044, "BOOT_DEF", "Read-Only Boot-Time Register. Contains BOOT_SEL and PKG_SEL values."},
#if defined(CONFIG_CPU_PXA27X)
	{0x48000048, "ARB_CNTL", "Arbiter Control register"},
	{0x4800004C, "BSCNTR0", "System Memory Buffer Strength Control register 0"},
	{0x48000050, "BSCNTR1", "System Memory Buffer Strength Control register 1"},
	{0x48000054, "LCDBSCNTR", "LCD Buffer Strength Control register"},
	{0x48000058, "MDMRSLP", "Special Low Power SDRAM Mode Register Set Configuration register"},
	{0x4800005C, "BSCNTR2", "System Memory Buffer Strength Control register 2"},
	{0x48000060, "BSCNTR3", "System Memory Buffer Strength Control register 3"},
	{0x48000064, "SA1110", "SA-1110 Compatibility Mode for Static Memory register"},
#endif
#if defined(CONFIG_CPU_PXA27X)
	{0x4C000000, "UHCREV", "UHC HCI Spec Revision register"},
	{0x4C000004, "UHCHCON", "UHC Host Control register"},
	{0x4C000008, "UHCCOMS", "UHC Command Status register"},
	{0x4C00000C, "UHCINTS", "UHC Interrupt Status register"},
	{0x4C000010, "UHCINTE", "UHC Interrupt Enable register"},
	{0x4C000014, "UHCINTD", "UHC Interrupt Disable register"},
	{0x4C000018, "UHCHCCA", "UHC Host Controller Communication Area register"},
	{0x4C00001C, "UHCPCED", "UHC Period Current Endpoint Descriptor register"},
	{0x4C000020, "UHCCHED", "UHC Control Head Endpoint Descriptor register"},
	{0x4C000024, "UHCCCED", "UHC Control Current Endpoint Descriptor register"},
	{0x4C000028, "UHCBHED", "UHC Bulk Head Endpoint Descriptor register"},
	{0x4C00002C, "UHCBCED", "UHC Bulk Current Endpoint Descriptor register"},
	{0x4C000030, "UHCDHEAD", "UHC Done Head register"},
	{0x4C000034, "UHCFMI", "UHC Frame Interval register"},
	{0x4C000038, "UHCFMR", "UHC Frame Remaining register"},
	{0x4C00003C, "UHCFMN", "UHC Frame Number register"},
	{0x4C000040, "UHCPERS", "UHC Periodic Start register"},
	{0x4C000044, "UHCLST", "UHC Low-Speed Threshold register"},
	{0x4C000048, "UHCRHDA", "UHC Root Hub Descriptor A register"},
	{0x4C00004C, "UHCRHDB", "UHC Root Hub Descriptor B register"},
	{0x4C000050, "UHCRHS", "UHC Root Hub Status register"},
	{0x4C000054, "UHCRHPS1", "UHC Root Hub Port 1 Status register"},
	{0x4C000058, "UHCRHPS2", "UHC Root Hub Port 2 Status register"},
	{0x4C00005C, "UHCRHPS3", "UHC Root Hub Port 3 Status register"},
	{0x4C000060, "UHCSTAT", "UHC Status register"},
	{0x4C000064, "UHCHR", "UHC Reset register"},
	{0x4C000068, "UHCHIE", "UHC Interrupt Enable register"},
	{0x4C00006C, "UHCHIT", "UHC Interrupt Test register"},
#endif
#ifdef CONFIG_SABINAL_DISCOVERY
	{0x08000000, "AINTMASK", ""},
	{0x08000002, "AIODIR", ""},
	{0x08000004, "AIOPIOD", ""},
	{0x08000006, "AINTYP", ""},
	{0x08000008, "AINETSEL", ""},
	{0x0800000A, "AIOTLSEL", ""},
	{0x0800000C, "AINTSLP", ""},
	{0x0800000E, "AOSLPOUT", ""},
	{0x08000010, "AOBFOUT", ""},
	{0x08000012, "AINTSTAT", ""},
	{0x08000014, "AIOALT", ""},
	{0x08000016, "AIOCONF", ""},
	{0x08000018, "AIOPSTS", ""},
	{0x08000080, "BINTMASK", ""},
	{0x08000082, "BIODIR", ""},
	{0x08000084, "BIOPIOD", ""},
	{0x08000086, "BINTYP", ""},
	{0x08000088, "BINETSEL", ""},
	{0x0800008A, "BIOTLSEL", ""},
	{0x0800008C, "BINTSLP", ""},
	{0x0800008E, "BOSLPOUT", ""},
	{0x08000090, "BOBFOUT", ""},
	{0x08000092, "BINTSTAT", ""},
	{0x08000094, "BIOALT", ""},
	{0x08000096, "BIOCONF", ""},
	{0x08000098, "BIOPSTS", ""},
	{0x08000100, "CINTMASK", ""},
	{0x08000102, "CIODIR", ""},
	{0x08000104, "CIOPIOD", ""},
	{0x08000106, "CINTYP", ""},
	{0x08000108, "CINETSEL", ""},
	{0x0800010A, "CIOTLSEL", ""},
	{0x0800010C, "CINTSLP", ""},
	{0x0800010E, "COSLPOUT", ""},
	{0x08000110, "COBFOUT", ""},
	{0x08000112, "CINTSTAT", ""},
	{0x08000114, "CIOALT", ""},
	{0x08000116, "CIOCONF", ""},
	{0x08000118, "CIOPSTS", ""},
	{0x08000180, "DINTMASK", ""},
	{0x08000182, "DIODIR", ""},
	{0x08000184, "DIOPIOD", ""},
	{0x08000186, "DINTYP", ""},
	{0x08000188, "DINETSEL", ""},
	{0x0800018A, "DIOTLSEL", ""},
	{0x0800018C, "DINTSLP", ""},
	{0x0800018E, "DOSLPOUT", ""},
	{0x08000190, "DOBFOUT", ""},
	{0x08000192, "DINTSTAT", ""},
	{0x08000194, "DIOALT", ""},
	{0x08000196, "DIOCONF", ""},
	{0x08000198, "DIOPSTS", ""},
	{0x08000580, "INTMASK", ""},
	{0x08000582, "PINTSTAT", ""},
	{0x08000584, "INTCPS", ""},
	{0x08000586, "INTTBS", ""}
#endif
};

#define NUM_OF_XPA2X0_REG_ENTRY	(sizeof(xpa2X0_regs)/sizeof(xpa2X0_reg_entry_t))

static int proc_read_reg(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	xpa2X0_reg_entry_t* current_reg=NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0;i<NUM_OF_XPA2X0_REG_ENTRY;i++) {
		if (xpa2X0_regs[i].low_ino==i_ino) {
			current_reg = &xpa2X0_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	count = sprintf(outputbuf, "0x%08X\n",
			*((volatile u32 *) dis_io_p2v(current_reg->phyaddr)));
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
	xpa2X0_reg_entry_t* current_reg=NULL;
	int i;
	unsigned long newRegValue;
	char *endp;

	for (i=0;i<NUM_OF_XPA2X0_REG_ENTRY;i++) {
		if (xpa2X0_regs[i].low_ino==i_ino) {
			current_reg = &xpa2X0_regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;

	newRegValue = simple_strtoul(buffer,&endp,0);
	*((volatile Word *) dis_io_p2v(current_reg->phyaddr))=newRegValue;
	return (count+endp-buffer);
}

static struct proc_dir_entry *regdir;
static struct proc_dir_entry *cpudir;

#ifdef USE_SCOOP
static struct proc_dir_entry *scoop_regdir;
static struct proc_dir_entry *scoopdir;
#endif
#ifdef USE_SCOOP2
static struct proc_dir_entry *scoop2_regdir;
static struct proc_dir_entry *scoop2dir;
#endif
#ifdef USE_KUROHYO
static struct proc_dir_entry *kurohyo_regdir;
static struct proc_dir_entry *kurohyodir;
#endif

#ifdef USE_LOCOMO
static struct proc_dir_entry *locomo_regdir;
static struct proc_dir_entry *locomodir;
#endif

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

	for(i=0;i<NUM_OF_XPA2X0_REG_ENTRY;i++) {
		entry = create_proc_entry(xpa2X0_regs[i].name,
				S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
				regdir);
		if(entry) {
			xpa2X0_regs[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_reg_operations;
		} else {
			printk( KERN_ERR MODULE_NAME
				": can't create /proc/" REG_DIRNAME
				"/%s\n", xpa2X0_regs[i].name);
			return(-ENOMEM);
		}
	}
#ifdef USE_SCOOP
	scoopdir = proc_mkdir(SCOOP_DIRNAME, &proc_root);
	if (scoopdir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" SCOOP_DIRNAME "\n");
		return(-ENOMEM);
	}

	scoop_regdir = proc_mkdir(REG_DIRNAME, scoopdir);
	if (scoop_regdir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" SCOOP_DIRNAME "/" REG_DIRNAME "\n");
		return(-ENOMEM);
	}

	for(i=0;i<NUM_OF_SCOOP_REG_ENTRY;i++) {
		entry = create_proc_entry(scoop_regs[i].name,
				S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
				scoop_regdir);
		if(entry) {
			scoop_regs[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_scoop_reg_operations;
		} else {
			printk( KERN_ERR MODULE_NAME
				": can't create /proc/" REG_DIRNAME
				"/%s\n", scoop_regs[i].name);
			return(-ENOMEM);
		}
	}
#endif
#ifdef USE_SCOOP2
	scoop2dir = proc_mkdir(SCOOP2_DIRNAME, &proc_root);
	if (scoop2dir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" SCOOP2_DIRNAME "\n");
		return(-ENOMEM);
	}

	scoop2_regdir = proc_mkdir(REG_DIRNAME, scoop2dir);
	if (scoop2_regdir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" SCOOP2_DIRNAME "/" REG_DIRNAME "\n");
		return(-ENOMEM);
	}

	for(i=0;i<NUM_OF_SCOOP2_REG_ENTRY;i++) {
		entry = create_proc_entry(scoop2_regs[i].name,
				S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
				scoop2_regdir);
		if(entry) {
			scoop2_regs[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_scoop2_reg_operations;
		} else {
			printk( KERN_ERR MODULE_NAME
				": can't create /proc/" REG_DIRNAME
				"/%s\n", scoop2_regs[i].name);
			return(-ENOMEM);
		}
	}
#endif
#ifdef USE_KUROHYO
	kurohyodir = proc_mkdir(KUROHYO_DIRNAME, &proc_root);
	if (kurohyodir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" KUROHYO_DIRNAME "\n");
		return(-ENOMEM);
	}

	kurohyo_regdir = proc_mkdir(REG_DIRNAME, kurohyodir);
	if (kurohyo_regdir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" KUROHYO_DIRNAME "/" REG_DIRNAME "\n");
		return(-ENOMEM);
	}

	for(i=0;i<NUM_OF_KUROHYO_REG_ENTRY;i++) {
		entry = create_proc_entry(kurohyo_regs[i].name,
				S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
				kurohyo_regdir);
		if(entry) {
			kurohyo_regs[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_kurohyo_reg_operations;
		} else {
			printk( KERN_ERR MODULE_NAME
				": can't create /proc/" REG_DIRNAME
				"/%s\n", kurohyo_regs[i].name);
			return(-ENOMEM);
		}
	}
#endif

#ifdef USE_LOCOMO
	locomodir = proc_mkdir(LOCOMO_DIRNAME, &proc_root);
	if (locomodir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" LOCOMO_DIRNAME "\n");
		return(-ENOMEM);
	}

	locomo_regdir = proc_mkdir(REG_DIRNAME, locomodir);
	if (locomo_regdir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" LOCOMO_DIRNAME "/" REG_DIRNAME "\n");
		return(-ENOMEM);
	}

	for(i=0;i<NUM_OF_LOCOMO_REG_ENTRY;i++) {
		entry = create_proc_entry(locomo_regs[i].name,
				S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
				locomo_regdir);
		if(entry) {
			locomo_regs[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_locomo_reg_operations;
		} else {
			printk( KERN_ERR MODULE_NAME
				": can't create /proc/" REG_DIRNAME
				"/%s\n", locomo_regs[i].name);
			return(-ENOMEM);
		}
	}
#endif

	return (0);
}

static void __exit cleanup_reg_monitor(void)
{
	int i;
	for(i=0;i<NUM_OF_XPA2X0_REG_ENTRY;i++)
		remove_proc_entry(xpa2X0_regs[i].name,regdir);
	remove_proc_entry(REG_DIRNAME, cpudir);
	remove_proc_entry(CPU_DIRNAME, &proc_root);
#ifdef USE_SCOOP
	for(i=0;i<NUM_OF_SCOOP_REG_ENTRY;i++)
		remove_proc_entry(scoop_regs[i].name,scoop_regdir);
	remove_proc_entry(REG_DIRNAME, scoopdir);
	remove_proc_entry(SCOOP_DIRNAME, &proc_root);
#endif
#ifdef USE_SCOOP2
	for(i=0;i<NUM_OF_SCOOP2_REG_ENTRY;i++)
		remove_proc_entry(scoop2_regs[i].name,scoop2_regdir);
	remove_proc_entry(REG_DIRNAME, scoop2dir);
	remove_proc_entry(SCOOP2_DIRNAME, &proc_root);
#endif
#ifdef USE_KUROHYO
	for(i=0;i<NUM_OF_KUROHYO_REG_ENTRY;i++)
		remove_proc_entry(kurohyo_regs[i].name,kurohyo_regdir);
	remove_proc_entry(REG_DIRNAME, kurohyodir);
	remove_proc_entry(KUROHYO_DIRNAME, &proc_root);
#endif
#ifdef USE_LOCOMO
	for(i=0;i<NUM_OF_LOCOMO_REG_ENTRY;i++)
		remove_proc_entry(locomo_regs[i].name,locomo_regdir);
	remove_proc_entry(REG_DIRNAME, locomodir);
	remove_proc_entry(LOCOMO_DIRNAME, &proc_root);
#endif
}

module_init(init_reg_monitor);
module_exit(cleanup_reg_monitor);

MODULE_AUTHOR("Sukjae Cho (sjcho@redwood.snu.ac.kr)");
MODULE_DESCRIPTION("XPA2X0 Register monitor");

EXPORT_NO_SYMBOLS;
