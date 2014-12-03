#ifndef __ASM_ARCH_FPGA_IRIS_H
#define __ASM_ARCH_FPGA_IRIS_H

/* #define BOARD_IS_SDB */
#define BOARD_IS_TARGET

#define SHARP_FPGA_BASE ((void*)0x1c000000)

#ifdef BOARD_IS_TARGET
typedef struct SharpFPGARegisters {
  unsigned short CTRL; /* 00 */
  unsigned short XS; /* 02 */
  unsigned short YS; /* 04 */
  unsigned short XW; /* 06 */
  unsigned short YW; /* 08 */
  unsigned short EEN; /* 0a */
  unsigned short ESEL; /* 0c */
  unsigned short DE; /* 0e */
  unsigned short LSEL; /* 10 */
  unsigned short ECL; /* 12 */
  unsigned short Enable; /* 14 */
  unsigned short EnSet; /* 16 */
  unsigned short EnReset; /* 18 */
  unsigned short RawStat; /* 1a */
  unsigned short Status; /* 1c */
  unsigned short OutLevel; /* 1e */
  unsigned short Scrad; /* 20 */
  unsigned short Scrx; /* 22 */
  unsigned short Scry; /* 24 */
  unsigned short dummy26; /* 26 */
  unsigned short dummy28; /* 28 */
  unsigned short dummy2a; /* 2a */
  unsigned short dummy2c; /* 2c */
  unsigned short PANEL; /* 2e */
  unsigned short GPO; /* 30 */
  unsigned short dummy32; /* 32 */
  unsigned short dummy34; /* 34 */
  unsigned short dummy36; /* 36 */
  unsigned short dummy38; /* 38 */
  unsigned short dummy3a; /* 3a */
  unsigned short dummy3c; /* 3c */
  unsigned short TEST; /* 3e */
} SharpFPGARegisters;

#define FPGA_OFFSET_CTRL      0x00
#define FPGA_OFFSET_XS        0x02
#define FPGA_OFFSET_YS        0x04
#define FPGA_OFFSET_XW        0x06
#define FPGA_OFFSET_YW        0x08
#define FPGA_OFFSET_EEN       0x0a
#define FPGA_OFFSET_ESEL      0x0c
#define FPGA_OFFSET_DE        0x0e
#define FPGA_OFFSET_LSEL      0x10
#define FPGA_OFFSET_ECL       0x12
#define FPGA_OFFSET_Enable    0x14
#define FPGA_OFFSET_EnSet     0x16
#define FPGA_OFFSET_EnReset   0x18
#define FPGA_OFFSET_RawStat   0x1a
#define FPGA_OFFSET_Status    0x1c
#define FPGA_OFFSET_OutLevel  0x1e
#define FPGA_OFFSET_Scrad     0x20
#define FPGA_OFFSET_Scrx      0x22
#define FPGA_OFFSET_Scry      0x24
#define FPGA_OFFSET_PANEL     0x2e
#define FPGA_OFFSET_GPO       0x30
#define FPGA_OFFSET_TEST      0x3e

#endif /* BOARD_IS_TARGET */

#ifdef BOARD_IS_SDB
typedef struct SharpFPGARegisters {
  unsigned int CTRL; /* 00 */
  unsigned int XS; /* 04 */
  unsigned int YS; /* 08 */
  unsigned int XW; /* 0c */
  unsigned int YW; /* 10 */
  unsigned int EEN; /* 14 */
  unsigned int ESEL; /* 18 */
  unsigned int DE; /* 1c */
  unsigned int LSEL; /* 20 */
  unsigned int ECL; /* 24 */
  unsigned int Enable; /* 28 */
  unsigned int EnSet; /* 2c */
  unsigned int EnReset; /* 30 */
  unsigned int RawStat; /* 34 */
  unsigned int Status; /* 38 */
  unsigned int OutLevel; /* 3c */
  unsigned int Scrad; /* 40 */
  unsigned int Scrx; /* 44 */
  unsigned int Scry; /* 48 */
  unsigned int dummy26; /* 4c */
  unsigned int dummy28; /* 50 */
  unsigned int dummy2a; /* 54 */
  unsigned int dummy2c; /* 58 */
  unsigned int PANEL; /* 5c */
  unsigned int GPO; /* 60 */
  unsigned int dummy32; /* 64 */
  unsigned int dummy34; /* 68 */
  unsigned int dummy36; /* 6c */
  unsigned int dummy38; /* 70 */
  unsigned int dummy3a; /* 74 */
  unsigned int dummy3c; /* 78 */
  unsigned int TEST; /* 7c */
} SharpFPGARegisters;

#define FPGA_OFFSET_CTRL      0x00
#define FPGA_OFFSET_XS        0x04
#define FPGA_OFFSET_YS        0x08
#define FPGA_OFFSET_XW        0x0c
#define FPGA_OFFSET_YW        0x10
#define FPGA_OFFSET_EEN       0x14
#define FPGA_OFFSET_ESEL      0x18
#define FPGA_OFFSET_DE        0x1c
#define FPGA_OFFSET_LSEL      0x20
#define FPGA_OFFSET_ECL       0x24
#define FPGA_OFFSET_Enable    0x28
#define FPGA_OFFSET_EnSet     0x2c
#define FPGA_OFFSET_EnReset   0x30
#define FPGA_OFFSET_RawStat   0x34
#define FPGA_OFFSET_Status    0x38
#define FPGA_OFFSET_OutLevel  0x3c
#define FPGA_OFFSET_Scrad     0x40
#define FPGA_OFFSET_Scrx      0x44
#define FPGA_OFFSET_Scry      0x48
#define FPGA_OFFSET_PANEL     0x5c
#define FPGA_OFFSET_GPO       0x60
#define FPGA_OFFSET_TEST      0x7c

#endif /* BOARD_IS_SDB */

#define CCDFPGA_CTRL_DMA_OFF  0x00
#define CCDFPGA_CTRL_DMA_ON   0x01
#define CCDFPGA_CTRL_DMA_FRM  0x00
#define CCDFPGA_CTRL_DMA_CON  0x02

#define CCDFPGA_FPN_NUMMAX      32
#define CCDFPGA_FPN_NOPDATA     0x1ff


#define TEST_NORMAL_MODE       0x0
#define TEST_ENABLE_FPNTEST    0x1

/* for CLCD
   GA Address:
   phys_addr - IO_IOCS_START + IO_IOCS_BASE
   ** 32bit access only?
 */
#define FPGA_GPO	(*(volatile unsigned long *)0xdc000030)
#define FPGA_PANEL	(*(volatile unsigned long *)0xdc00002c)

#include <asm/arch/hardware.h>

/*
 *  FPGA Interfaces (followed to FPGA-SPEC draft No.07 , 2001/01/22)
 */

#if 0
/*
 * FPGA registers (16bit mode only)
 */
#define FPGA_OFFSET_CTRL      0x00
#define FPGA_OFFSET_XS        0x02
#define FPGA_OFFSET_YS        0x04
#define FPGA_OFFSET_XW        0x06
#define FPGA_OFFSET_YW        0x08
#define FPGA_OFFSET_EEN       0x0a
#define FPGA_OFFSET_ESEL      0x0c
#define FPGA_OFFSET_DE        0x0e
#define FPGA_OFFSET_LSEL      0x10
#define FPGA_OFFSET_ECL       0x12
#define FPGA_OFFSET_Enable    0x14
#define FPGA_OFFSET_EnSet     0x16
#define FPGA_OFFSET_EnReset   0x18
#define FPGA_OFFSET_RawStat   0x1a
#define FPGA_OFFSET_Status    0x1c
#define FPGA_OFFSET_OutLevel  0x1e
#define FPGA_OFFSET_CLEX      0x20
#define FPGA_OFFSET_ADICLR    0x28
#define FPGA_OFFSET_ADIEN     0x2a
#define FPGA_OFFSET_ADINT     0x2c
#define FPGA_OFFSET_PANEL     0x2e
#define FPGA_OFFSET_GPO       0x30
#endif

/*
 *  Definitions for CMOS Imager Interface
 */

/* for CTRL */
#define CCDFPGA_CTRL_DMA_OFF  0x00
#define CCDFPGA_CTRL_DMA_ON   0x01
#define CCDFPGA_CTRL_DMA_FRM  0x00
#define CCDFPGA_CTRL_DMA_CON  0x02


/*
 *  Definitions for Wakeup Control
 */

#define bwWSRC0    1
#define bwWSRC1    1
#define bwWSRC2    1
#define bwWSRC3    1
#define bwWSRC4    1
#define bwWSRC5    1
#define bwWSRC6    1
#define bwWSRC7    1
#define bwWSRC8    1
#define bwWSRC9    1

#define bsWSRC0    0
#define bsWSRC1    1
#define bsWSRC2    2
#define bsWSRC3    3
#define bsWSRC4    4
#define bsWSRC5    5
#define bsWSRC6    6
#define bsWSRC7    7
#define bsWSRC8    8
#define bsWSRC9    9

#define bitWSRC0    0x0001
#define bitWSRC1    0x0002
#define bitWSRC2    0x0004
#define bitWSRC3    0x0008
#define bitWSRC4    0x0010
#define bitWSRC5    0x0020
#define bitWSRC6    0x0040
#define bitWSRC7    0x0080
#define bitWSRC8    0x0100
#define bitWSRC9    0x0200

/* for EEN */
#define WAKEUP_LEVELTRIG  0
#define WAKEUP_EDGETRIG   1

/* for ESEL */
#define WAKEUP_UPTRIG     0
#define WAKEUP_DOWNTRIG   1

/* for DE */
#define WAKEUP_ETHERTRIG  0
#define WAKEUP_BOTHTRIG   1

/* for LSEL */
#define WAKEUP_LOWTRIG    0
#define WAKEUP_HIGHTRIG   1

/* for OutLevel */
#define OutLevel_ActiveLow  0
#define OutLevel_ActiveHigh 0x1
#define OutLevel_INTWU      0x2
#define OutLevel_NoINTWU    0


static __inline__ void enable_fpga_irq(unsigned int pin)
{
  IO_FPGA_ECL = pin; /* clear pin set */
  IO_FPGA_CLEX = pin; /* clear exec. */
  IO_FPGA_EnSet = pin;
  /* clear WSRC0 ( reset ) interrupt before enabling INTWU */
  if( IO_FPGA_Enable & bitWSRC0 && ! ( IO_FPGA_OutLevel & OutLevel_INTWU ) ){
    IO_FPGA_ECL |= bitWSRC0;  /* clear WSRC0 edge int. */
    IO_FPGA_CLEX = bitWSRC0;  /* clear WSRC0 edge int. */
  }
  IO_FPGA_OutLevel |= OutLevel_INTWU;
  if( ! ( IO_IRQENABLE & ( 1 << IRQ_INTF ) ) )
    enable_irq(IRQ_INTF);
  return;
}

static __inline__ void disable_fpga_irq(unsigned int pin)
{
  IO_FPGA_EnReset = pin;
  if( ! IO_FPGA_Enable ){ /* no bits are enabled */
    IO_FPGA_OutLevel &= ~OutLevel_INTWU;
    disable_irq(IRQ_INTF);
  }
  return;
}

static __inline__ void clear_fpga_int(unsigned int pin)
{
  IO_FPGA_ECL = pin; /* clear pin set */
  IO_FPGA_CLEX = pin; /* clear exec. */
}

static __inline__ void set_fpga_int_trigger_highlevel(unsigned int pin)
{
  IO_FPGA_EEN &= ~pin; /* Set as Level-Trigger */
  IO_FPGA_LSEL |= pin; /* Set as High-Level Trigger */
}

static __inline__ void set_fpga_int_trigger_lowlevel(unsigned int pin)
{
  IO_FPGA_EEN &= ~pin; /* Set as Level-Trigger */
  IO_FPGA_LSEL &= ~pin; /* Set as Low-Level Trigger */
}

static __inline__ void set_fpga_int_trigger_highedge(unsigned int pin)
{
  IO_FPGA_EEN |= pin; /* Set as for Edge-Trigger */
  IO_FPGA_DE &= ~pin; /* Set as either Edge */
  IO_FPGA_ESEL |= pin; /* Set as for Raise-Edge */
}

static __inline__ void set_fpga_int_trigger_lowedge(unsigned int pin)
{
  IO_FPGA_EEN |= pin; /* Set as for Edge-Trigger */
  IO_FPGA_DE &= ~pin; /* Set as either Edge */
  IO_FPGA_ESEL &= ~pin; /* Set as for Raise-Edge */
}

static __inline__ void set_fpga_int_trigger_bothedge(unsigned int pin)
{
  IO_FPGA_EEN |= pin; /* Set as for Edge-Trigger */
  IO_FPGA_DE |= ~pin; /* Set as both Edge */
}

/*
 *  Definitions for TFT Interface
 */

/* for PANEL */
#define TFT_HRVE         0x0001
#define TFT_VRVE         0x0002
#define TFT_LCDEN        0x0004

/* for ADIEN */
#define TFT_ADIEN_DISABLE 0x0000
#define TFT_ADIEN_ENABLE  0x0001

#endif
