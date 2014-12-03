/****************************************************************************

  registers.c: Register monitor of XPA2X0

	This code is based on SA1110's register moniter

****************************************************************************/

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
#define dis_io_p2v(PhAdd)   ((PhAdd >= 0x40000000) ? ((PhAdd >= 0x48000000) ? (0xb5000000 + PhAdd ) : (0xb0000000 + PhAdd)) : (0xf0000000 + PhAdd))

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>               /* because we are a module */
#include <linux/init.h>                 /* for the __init macros */
#include <linux/proc_fs.h>              /* all the /proc functions */
#include <linux/ioport.h>
#include <asm/uaccess.h>                /* to copy to/from userspace */
#include <asm/arch/hardware.h>

typedef unsigned long Word;

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
	{0x40500004, "PICR", "PCM In Control Register"},
	{0x40500008, "MCCR", "Mic In Control Register"},
	{0x4050000C, "GCR", "Global Control Register"},
	{0x40500010, "POSR", "PCM Out Status Register"},
	{0x40500014, "PISR", "PCM In Status Register"},
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
	{0x40800000, "ICCR0", "ICP Control Register 0"},
	{0x40800004, "ICCR1", "ICP Control Register 1"},
	{0x40800008, "ICCR2", "ICP Control Register 2"},
	{0x4080000C, "ICDR", "ICP Data Register"},
	{0x40800014, "ICSR0", "ICP Status Register 0"},
	{0x40800018, "ICSR1", "ICP Status Register 1"},
	{0x40900000, "RCNR", "RTC Count Register"},
	{0x40900004, "RTAR", "RTC Alarm Register"},
	{0x40900008, "RTSR", "RTC Status Register"},
	{0x4090000C, "RTTR", "RTC Timer Trim Register"},
	{0x40A00000, "OSMR<0>", "OS Timer Match Registers<0>"},
	{0x40A00004, "OSMR<1>", "OS Timer Match Registers<1>"},
	{0x40A00008, "OSMR<2>", "OS Timer Match Registers<2>"},
	{0x40A0000C, "OSMR<3>", "OS Timer Match Registers<3>"},
	{0x40A00010, "OSCR", "OS Timer Counter Register"},
	{0x40A00014, "OSSR", "OS Timer Status Register"},
	{0x40A00018, "OWER", "OS Timer Watchdog Enable Register"},
	{0x40A0001C, "OIER", "OS Timer Interrupt Enable Register"},
	{0x40B00000, "PWM_CTRL0", "PWM 0 Control Register"},
	{0x40B00004, "PWM_PWDUTY0", "PWM 0 Duty Cycle Register"},
	{0x40B00008, "PWM_PERVAL0", "PWM 0 Period Control Register"},
	{0x40C00000, "PWM_CTRL1", "PWM 1Control Register"},
	{0x40C00004, "PWM_PWDUTY1", "PWM 1 Duty Cycle Register"},
	{0x40C00008, "PWM_PERVAL1", "PWM 1 Period Control Register"},
	{0x40D00000, "ICIP", "Interrupt Controller IRQ Pending Register"},
	{0x40D00004, "ICMR", "Interrupt Controller Mask Register"},
	{0x40D00008, "ICLR", "Interrupt Controller Level Register"},
	{0x40D0000C, "ICFP", "Interrupt Controller FIQ Pending Register"},
	{0x40D00010, "ICPR", "Interrupt Controller Pending Register"},
	{0x40D00014, "ICCR", "Interrupt Controller Control Register"},
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
	{0x40E00068, "GAFR2_U", "GPIO Alternate Function Select Register GPIO 80"},
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
	{0x40F00030, "RCSR", "Reset Controller Status Register"},
	{0x41000000, "SSCR0", "SSP Control Register 0"},
	{0x41000004, "SSCR1", "SSP Control Register 1"},
	{0x41000008, "SSSR", "SSP Status Register"},
	{0x4100000C, "SSITR", "SSP Interrupt Test Register"},
	{0x41000010, "SSDR", "(Write / Read) SSP Data Write Register/SSP Data Read Register"},
	{0x41100000, "MMC_STRPCL", "Control to start and stop MMC clock"},
	{0x41100004, "MMC_STAT", "MMC Status Register (read only)"},
	{0x41100008, "MMC_CLKRT", "MMC clock rate"},
	{0x4110000C, "MMC_SPI", "SPI mode control bits"},
	{0x41100010, "MMC_CMDAT", "Command/response/data sequence control"},
	{0x41100014, "MMC_RESTO", "Expected response time out"},
	{0x41100018, "MMC_RDTO", "Expected data read time out"},
	{0x4110001C, "MMC_BLKLEN", "Block length of data transaction"},
	{0x41100020, "MMC_NOB", "Number of blocks, for block mode"},
	{0x41100024, "MMC_PRTBUF", "Partial MMC_TXFIFO FIFO written"},
	{0x41100028, "MMC_I_MASK", "Interrupt Mask"},
	{0x4110002C, "MMC_I_REG", "Interrupt Register (read only)"},
	{0x41100030, "MMC_CMD", "Index of current command"},
	{0x41100034, "MMC_ARGH", "MSW part of the current command argument"},
	{0x41100038, "MMC_ARGL", "LSW part of the current command argument"},
	{0x4110003C, "MMC_RES", "Response FIFO (read only)"},
	{0x41100040, "MMC_RXFIFO", "Receive FIFO (read only)"},
	{0x41100044, "MMC_TXFIFO", "Transmit FIFO (write only)"},
	{0x41300000, "CCCR", "Core Clock Configuration Register"},
	{0x41300004, "CKEN", "Clock Enable Register"},
	{0x41300008, "OSCC", "Oscillator Configuration Register"},
	{0x44000000, "LCCR0", "LCD Controller Control Register 0"},
	{0x44000004, "LCCR1", "LCD Controller Control Register 1"},
	{0x44000008, "LCCR2", "LCD Controller Control Register 2"},
	{0x4400000C, "LCCR3", "LCD Controller Control Register 3"},
	{0x44000200, "FDADR0", "DMA Channel 0 Frame Descriptor Address Register"},
	{0x44000204, "FSADR0", "DMA Channel 0 Frame Source Address Register"},
	{0x44000208, "FIDR0", "DMA Channel 0 Frame ID Register"},
	{0x4400020C, "LDCMD0", "DMA Channel 0 Command Register"},
	{0x44000210, "FDADR1", "DMA Channel 1 Frame Descriptor Address Register"},
	{0x44000214, "FSADR1", "DMA Channel 1 Frame Source Address Register"},
	{0x44000218, "FIDR1", "DMA Channel 1 Frame ID Register"},
	{0x4400021C, "LDCMD1", "DMA Channel 1 Command Register"},
	{0x44000020, "FBR0", "DMA Channel 0 Frame Branch Register"},
	{0x44000024, "FBR1", "DMA Channel 1 Frame Branch Register"},
	{0x44000038, "LCSR", "LCD Controller Status Register"},
	{0x4400003C, "LIIDR", "LCD Controller Interrupt ID Register"},
	{0x44000040, "TRGBR", "TMED RGB Seed Register"},
	{0x44000044, "TCR", "TMED Control Register"},
	{0x48000000, "MDCNFG", "SDRAM Configuration Register 0"},
	{0x48000004, "MDREFR", "SDRAM Refresh Control Register"},
	{0x48000008, "MSC0", "Static Memory Control Register 0"},
	{0x4800000C, "MSC1", "Static Memory Control Register 1"},
	{0x48000010, "MSC2", "Static Memory Control Register 2"},
	{0x48000014, "MECR", "Expansion Memory (PCMCIA/Compact Flash) Bus Configuration Register"},
	{0x4800001C, "SXCNFG", "Synchronous Static Memory Control Register"},
	{0x48000024, "SXMRS", "MRS value to be written to SMROM"},
	{0x48000028, "MCMEM0", "Card interface Common Memory Space Socket 0 Timing Configuration"},
	{0x4800002C, "MCMEM1", "Card interface Common Memory Space Socket 1 Timing Configuration"},
	{0x48000030, "MCATT0", "Card interface Attribute Space Socket 0 Timing Configuration"},
	{0x48000034, "MCATT1", "Card interface Attribute Space Socket 1 Timing Configuration"},
	{0x48000038, "MCIO0", "Card interface I/O Space Socket 0 Timing Configuration"},
	{0x4800003C, "MCIO1", "Card interface I/O Space Socket 1 Timing Configuration"},
	{0x48000040, "MDMRS", "MRS value to be written to SDRAM"},
	{0x48000044, "BOOT_DEF", "Read-Only Boot-Time Register. Contains BOOT_SEL and PKG_SEL values."},
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

	return (0);
}

static void __exit cleanup_reg_monitor(void)
{
	int i;
	for(i=0;i<NUM_OF_XPA2X0_REG_ENTRY;i++)
		remove_proc_entry(xpa2X0_regs[i].name,regdir);
	remove_proc_entry(REG_DIRNAME, cpudir);
	remove_proc_entry(CPU_DIRNAME, &proc_root);
}

module_init(init_reg_monitor);
module_exit(cleanup_reg_monitor);

MODULE_AUTHOR("Sukjae Cho (sjcho@redwood.snu.ac.kr)");
MODULE_DESCRIPTION("XPA2X0 Register monitor");

EXPORT_NO_SYMBOLS;
