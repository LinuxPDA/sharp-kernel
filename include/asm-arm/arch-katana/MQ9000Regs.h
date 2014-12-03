/* 
 *  MQ9000Regs.H : Main header file for hardware registers
 *                 for MQ9000 Controller.
 *
 *  Copyright (c) 2002 by MediaQ, Incorporated.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _MQ9000REGS_H_
#define _MQ9000REGS_H_

// Register Block Size
#define REGBLOCK_SZ		 0x200   // 512

// For those compilers do not support anonymous union. we have to use TAG.
// (Microsoft compiler supports anonymous union.)
// (Arm compiler does not support anonymous union.)
#ifdef SUPPORT_ANONYMOUS_UNION		// Microsoft and ARM C++ Compiler

#define	REG_ENTRY(name0,name1,name16,name8) \
	union{		                            \
	UInt32	name0;                          \
	UInt32	name1;                          \
	UInt16	name16[2];                      \
	UInt8	name8[4];                       \
	};

#define	REG_ENTRY1(name0,name1,name2,name16,name8) \
	union{                                  \
	UInt32	name0;                          \
	UInt32	name1;                          \
	UInt32	name2;                          \
	UInt16	name16[2];                      \
	UInt8	name8[4];                       \
	};
#define	REG_ENTRY2(name0,name1,name2,name3,name16,name8) \
	union{                                  \
	UInt32	name0;                          \
	UInt32	name1;                          \
	UInt32	name2;                          \
	UInt32	name3;							\
	UInt16	name16[2];                      \
	UInt8	name8[4];                       \
	};

#else  //For ARM C compiler
#define	REG_ENTRY(name0,name1,name16,name8) \
	union name0{                            \
	UInt32	name0;                          \
	UInt32	name1;                          \
	UInt16	name16[2];                      \
	UInt8	name8[4];                       \
	}name0;

#define	REG_ENTRY1(name0,name1,name2,name16,name8) \
	union name0{                            \
	UInt32	name0;                          \
	UInt32	name1;                          \
	UInt32	name2;                          \
	UInt16	name16[2];                      \
	UInt8	name8[4];                       \
	}name0;

#define	REG_ENTRY2(name0,name1,name2,name3,name16,name8) \
	union name0{                            \
	UInt32	name0;                          \
	UInt32	name1;                          \
	UInt32	name2;                          \
	UInt32	name3;							\
	UInt16	name16[2];                      \
	UInt8	name8[4];                       \
	}name0;
	
#endif

#ifndef __ASSEMBLY__	//linux gcc does not like typedef
	// CPU Interface Registers
typedef struct __PM {
	REG_ENTRY(pm00, ActiveCtrl,       pm00R16, pm00R8)
	REG_ENTRY(pm01, IdleCtrl,         pm01R16, pm01R8)
	REG_ENTRY(pm02, PLL1,             pm02R16, pm02R8)
	REG_ENTRY(pm03, PLL2,             pm03R16, pm03R8)
	REG_ENTRY(pm04, StandbyState,     pm04R16, pm04R8)
	REG_ENTRY(pm05, IdleState,        pm05R16, pm05R8)
	REG_ENTRY(pm06, Counter,          pm06R16, pm06R8)
	REG_ENTRY(pm07, Clock,            pm07R16, pm07R8)
	REG_ENTRY(pm08, GEConfig,         pm08R16, pm08R8)
	REG_ENTRY(pm09, Reserved09,       pm09R16, pm09R8)
	REG_ENTRY(pm0A, Status,           pm0AR16, pm0AR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x0B];
	} _PM;
	
typedef struct __SB {
	// System Bus Interface Registers
	REG_ENTRY(sb00, ExtMemEn,         sb00R16, sb00R8)
	REG_ENTRY(sb01, CPUAccCtrl,       sb01R16, sb01R8)
	REG_ENTRY(sb02, CPUAcc1Start,     sb02R16, sb02R8)
	REG_ENTRY(sb03, CPUAcc2Start,     sb03R16, sb03R8)
	REG_ENTRY(sb04, CPUReqCtrl,       sb04R16, sb04R8)
	REG_ENTRY(sb05, FlashCtrl,        sb05R16, sb05R8)
	REG_ENTRY(sb06, Reserved06,       sb06R16, sb06R8)
	REG_ENTRY(sb07, SRAMCtrl,         sb07R16, sb07R8)
	REG_ENTRY(sb08, ExtPerf0Ctrl,     sb08R16, sb08R8)
	REG_ENTRY(sb09, ExtPerf1Ctrl,     sb09R16, sb09R8)
	REG_ENTRY(sb0A, SDRAMCtrl1,       sb0AR16, sb0AR8)
	REG_ENTRY(sb0B, SDRAMCtrl2,       sb0BR16, sb0BR8)
	REG_ENTRY(sb0C, SDRAMCtrl3,       sb0CR16, sb0CR8)
	REG_ENTRY(sb0D, SDRAMCtrl4,       sb0DR16, sb0DR8)
	REG_ENTRY(sb0E, EmSRAMCtrl,       sb0ER16, sb0ER8)
	REG_ENTRY(sb0F, PerfDMACtrl1,     sb0FR16, sb0FR8)
	REG_ENTRY(sb10, PerfDMACtrl2,     sb10R16, sb10R8)
	REG_ENTRY(sb11, PerfDMACtrl3,     sb11R16, sb11R8)
	REG_ENTRY(sb12, EBIUTest1,        sb12R16, sb12R8)
	REG_ENTRY(sb13, EBIUTest2,        sb13R16, sb13R8)
	REG_ENTRY(sb14, NANDFlashDMA1, 	  sb14R16, sb14R8)
	REG_ENTRY(sb15, NANDFlashDMA2,    sb15R16, sb15R8)
	REG_ENTRY(sb16, EBIUNANDFlashCtrl, sb16R16, sb16R8)
	REG_ENTRY(sb17, EBIUNANDFlashAddr, sb17R16, sb17R8)
	REG_ENTRY(sb18, EBIUNANDFlashCmd,  sb18R16, sb18R8)
	REG_ENTRY(sb19, EBIUNANDFlashStat, sb19R16, sb19R8)
	REG_ENTRY(sb1A, EBIUNANDFlashECCl, sb1AR16, sb1AR8)
	REG_ENTRY(sb1B, EBIUNANDFlashECCh, sb1BR16, sb1BR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x1C];
	} _SB;
	
typedef struct __RT {
	// Real Time Clock Registers
	REG_ENTRY(rt00, TimeOfDay,         rt00R16, rt00R8)
	REG_ENTRY(rt01, Date,              rt01R16, rt01R8)
	REG_ENTRY(rt02, AlarmTime,         rt02R16, rt02R8)
	REG_ENTRY(rt03, AlarmDate,         rt03R16, rt03R8)
	REG_ENTRY(rt04, Control,           rt04R16, rt04R8)
	REG_ENTRY(rt05, PeriodicTM,        rt05R16, rt05R8)
	REG_ENTRY(rt06, WatchdogTM,        rt06R16, rt06R8)
	REG_ENTRY(rt07, TMStatus,          rt07R16, rt07R8)
	REG_ENTRY(rt08, IntEnable,         rt08R16, rt08R8)
	REG_ENTRY(rt09, IntStatus,         rt09R16, rt09R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x0A];
	} _RT;
	
typedef struct __GP {
	// GPIO (Timers, Processor) Registers
	REG_ENTRY(gp00, TimerCtrl,          gp00R16, gp00R8)
	REG_ENTRY(gp01, GpioDir,            gp01R16, gp01R8)
	REG_ENTRY(gp02, GpioSelect,         gp02R16, gp02R8)
	REG_ENTRY(gp03, GpioWakeEn,         gp03R16, gp03R8)
	REG_ENTRY(gp04, GpioEnable,         gp04R16, gp04R8)
	REG_ENTRY(gp05, Timer0Cmp,          gp05R16, gp05R8)
	REG_ENTRY(gp06, Timer1Cmp,          gp06R16, gp06R8)
	REG_ENTRY(gp07, Timer2Cmp,          gp07R16, gp07R8)
	REG_ENTRY(gp08, Timer0Stamp,        gp08R16, gp08R8)
	REG_ENTRY(gp09, Timer1Stamp,        gp09R16, gp09R8)
	REG_ENTRY(gp0A, Timer2Stamp,        gp0AR16, gp0AR8)
	REG_ENTRY(gp0B, GpioData,           gp0BR16, gp0BR8)
	REG_ENTRY(gp0C, TimerClock,         gp0CR16, gp0CR8)
	REG_ENTRY(gp0D, TimerLEDInt1,       gp0DR16, gp0DR8)
	REG_ENTRY(gp0E, TimerLEDInt2,       gp0ER16, gp0ER8)
	REG_ENTRY(gp0F, TimerLEDIntStat,    gp0FR16, gp0FR8)
	REG_ENTRY(gp10, LEDCyleCnt,         gp10R16, gp10R8)
	REG_ENTRY(gp11, LEDActiveTime,      gp11R16, gp11R8)
	REG_ENTRY(gp12, LEDInActiveTime,    gp12R16, gp12R8)
	REG_ENTRY(gp13, LEDActivate,        gp13R16, gp13R8)
	REG_ENTRY(gp14, GpioPinDir,         gp14R16, gp14R8)
	REG_ENTRY(gp15, GpioPinEn,          gp15R16, gp15R8)
	REG_ENTRY(gp16, GpioPinInt,         gp16R16, gp16R8)
	REG_ENTRY(gp17, Gpio116_119,        gp17R16, gp17R8)
#if 1 //TZYYWEI
	REG_ENTRY(gp18, Gpio116_119Int,     gp18R16, gp18R8)
	REG_ENTRY(gp19, Gpio116_119IntStat, gp19R16, gp19R8)
#else
	REG_ENTRY(gp18, Gpio117_119Int,     gp18R16, gp18R8)
	REG_ENTRY(gp19, Gpio117_119IntStat, gp19R16, gp19R8)
#endif
	REG_ENTRY(gp1A, GpioModeEnable,     gp1AR16, gp1AR8)
	REG_ENTRY(gp1B, Reserved1B,         gp1BR16, gp1BR8)
	REG_ENTRY(gp1C, Reserved1C,         gp1CR16, gp1CR8)
	REG_ENTRY(gp1D, Reserved1D,         gp1DR16, gp1DR8)
	REG_ENTRY(gp1E, Reserved1E,         gp1ER16, gp1ER8)
	REG_ENTRY(gp1F, Reserved1F,         gp1FR16, gp1FR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x20];
	} _GP;
	
typedef struct __IN {
	// Interrupt Controller Registers
	REG_ENTRY(in00, Control,      		in00R16, in00R8)
	REG_ENTRY(in01, IRQMask,      		in01R16, in01R8)
	REG_ENTRY(in02, FIQMask,      		in02R16, in02R8)
	REG_ENTRY(in03, Reserved03,   		in03R16, in03R8)
	REG_ENTRY(in04, RawStatus,     		in04R16, in04R8)
	REG_ENTRY(in05, IRQStatus,    		in05R16, in05R8)
	REG_ENTRY(in06, FIQStatus, 			in06R16, in06R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x07];
	} _IN;

typedef struct __UT1 {
	// UART Registers
	REG_ENTRY(ut00, Cntr0,   ut00R16,   ut00R8)
	REG_ENTRY(ut01, Cntr1,   ut01R16,   ut01R8)
	REG_ENTRY(ut02, Cntr2,   ut02R16,   ut02R8)
	REG_ENTRY(ut03, Data,    ut03R16,   ut03R8)
	REG_ENTRY(ut04, Status,  ut04R16,   ut04R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x05];
	} _UT1;

typedef struct __UT23 {
	// UART Registers
	REG_ENTRY1(ua00, Clock1,     IRCtrl0,   ua00R16, ua00R8)
	REG_ENTRY1(ua01, Clock2,     IRCtrl1,   ua01R16, ua01R8)
	REG_ENTRY1(ua02, TxCtrl,     IRCtrl2,   ua02R16, ua02R8)
	REG_ENTRY1(ua03, RxCtrl,     IRData,    ua03R16, ua03R8)
	REG_ENTRY1(ua04, IntCtrl,    IRStatus,  ua04R16, ua04R8)
	REG_ENTRY1(ua05, IntStatus,  IRTest,    ua05R16, ua05R8)
	REG_ENTRY(ua06, TxData,      ua06R16,   ua06R8)
	REG_ENTRY(ua07, TxBufStart,  ua07R16,   ua07R8)
	REG_ENTRY(ua08, TxBufSize,   ua08R16,   ua08R8)
	REG_ENTRY(ua09, RxData,      ua09R16,   ua09R8)
	REG_ENTRY(ua0A, RxBufStart,  ua0AR16,   ua0AR8)
	REG_ENTRY(ua0B, RxBufSize,   ua0BR16,   ua0BR8)
	REG_ENTRY(ua0C, Gpio,        ua0CR16,   ua0CR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x0D];
	} _UT23;

typedef struct __KB {
	// Keyboard Matrix Registers
	REG_ENTRY(kb00, Control,     kb00R16, kb00R8)
	REG_ENTRY(kb01, RCWidth,     kb01R16, kb01R8)
	REG_ENTRY(kb02, Clock,       kb02R16, kb02R8)
	REG_ENTRY(kb03, RCData,      kb03R16, kb03R8)
	REG_ENTRY(kb04, Status,      kb04R16, kb04R8)
	REG_ENTRY(kb05, GpioCtrl,    kb05R16, kb05R8)
	REG_ENTRY(kb06, GpioStatus,  kb06R16, kb06R8)
	REG_ENTRY(kb07, GpioDir,     kb07R16, kb07R8)
	REG_ENTRY(kb08, GpioInt,     kb08R16, kb08R8)
	REG_ENTRY(kb09, GpioParity,  kb09R16, kb09R8)
	REG_ENTRY(kb0A, GpioOutput,  kb0AR16, kb0AR8)
	REG_ENTRY(kb0B, GpioEnable,  kb0BR16, kb0BR8)
	REG_ENTRY(kb0C, GpioDisable, kb0CR16, kb0CR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x0D];
	} _KB;

typedef struct __CC {
	// CPU Interface Registers
	REG_ENTRY(cc00, Control,     cc00R16, cc00R8)
	REG_ENTRY(cc01, Status,      cc01R16, cc01R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x02];
	} _CC;

typedef struct __MM {
	// Memory Interface Registers
	REG_ENTRY(mm00, Control1,     mm00R16, mm00R8)
	REG_ENTRY(mm01, Control2,     mm01R16, mm01R8)
	REG_ENTRY(mm02, TestCtrl1,    mm02R16, mm02R8)
	REG_ENTRY(mm03, TestCtrl2,    mm03R16, mm03R8)
	REG_ENTRY(mm04, TestData,     mm04R16, mm04R8)
	REG_ENTRY(mm05, ClockCtrl,    mm05R16, mm05R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x06];
	} _MM;

typedef struct __GC {
	// Graphics Controller Registers
	REG_ENTRY(gc00, Control,       gc00R16, gc00R8)
	REG_ENTRY(gc01, PwrSeqControl, gc01R16, gc01R8)
	REG_ENTRY(gc02, HTotalEnd,     gc02R16, gc02R8)
	REG_ENTRY(gc03, VTotalEnd,     gc03R16, gc03R8)
	REG_ENTRY(gc04, HSync,         gc04R16, gc04R8)
	REG_ENTRY(gc05, VSync,         gc05R16, gc05R8)
	REG_ENTRY(gc06, HCounter,      gc06R16, gc06R8)
	REG_ENTRY(gc07, VCounter,      gc07R16, gc07R8)
	REG_ENTRY(gc08, HWindow,       gc08R16, gc08R8)
	REG_ENTRY(gc09, VWindow,       gc09R16, gc09R8)
	REG_ENTRY(gc0A, LineClock,     gc0AR16, gc0AR8)
	REG_ENTRY(gc0B, LineClockA,    gc0BR16, gc0BR8)
	REG_ENTRY(gc0C, StartAddr,     gc0CR16, gc0CR8)
	REG_ENTRY(gc0D, StartAddrA,    gc0DR16, gc0DR8)
	REG_ENTRY(gc0E, Stride,        gc0ER16, gc0ER8)
	REG_ENTRY(gc0F, PS2Control,    gc0FR16, gc0FR8)
	REG_ENTRY(gc10, CursorPos,     gc10R16, gc10R8)
	REG_ENTRY(gc11, CursorStart,   gc11R16, gc11R8)
	REG_ENTRY(gc12, CursorFgColor, gc12R16, gc12R8)
	REG_ENTRY(gc13, CursorBgColor, gc13R16, gc13R8)
	REG_ENTRY(gc14, VWindowA,      gc14R16, gc14R8)
	REG_ENTRY(gc15, Reserved15,    gc15R16, gc15R8)
	REG_ENTRY(gc16, Reserved16,    gc16R16, gc16R8)
	REG_ENTRY(gc17, Reserved17,    gc17R16, gc17R8)
	REG_ENTRY(gc18, Reserved18,    gc18R16, gc18R8)
	REG_ENTRY(gc19, FrameClock,    gc19R16, gc19R8)
	REG_ENTRY(gc1A, FrameClockA,   gc1AR16, gc1AR8)
	REG_ENTRY(gc1B, Signals,       gc1BR16, gc1BR8)
	REG_ENTRY(gc1C, HParam,        gc1CR16, gc1CR8)
	REG_ENTRY(gc1D, VParam,        gc1DR16, gc1DR8)
	REG_ENTRY(gc1E, WindowLine,    gc1ER16, gc1ER8)
	REG_ENTRY(gc1F, CursorLine,    gc1FR16, gc1FR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x20];
	} _GC;
	
typedef struct __FP {
	// Flat Panel Registers
	REG_ENTRY(fp00, Control,         fp00R16, fp00R8)
	REG_ENTRY(fp01, PinControl,      fp01R16, fp01R8)
	REG_ENTRY(fp02, PinOutCtrl,      fp02R16, fp02R8)
	REG_ENTRY(fp03, PinInCtrl,       fp03R16, fp03R8)
	REG_ENTRY(fp04, STNPanelCtrl,    fp04R16, fp04R8)
	REG_ENTRY(fp05, PinPolarityCtrl, fp05R16, fp05R8)
	REG_ENTRY(fp06, PinOutSelect0,   fp06R16, fp06R8)
	REG_ENTRY(fp07, PinOutSelect1,   fp07R16, fp07R8)
	REG_ENTRY(fp08, PinOutData,      fp08R16, fp08R8)
	REG_ENTRY(fp09, PinInData,       fp09R16, fp09R8)
	REG_ENTRY(fp0A, PinWeakPullDown, fp0AR16, fp0AR8)
	REG_ENTRY(fp0B, AddinalPinOutSel,fp0BR16, fp0BR8)
	REG_ENTRY(fp0C, ModClockCtrl,    fp0CR16, fp0CR8)
	REG_ENTRY(fp0D, ALWControl,      fp0DR16, fp0DR8)
	REG_ENTRY(fp0E, TestControl,     fp0ER16, fp0ER8)
	REG_ENTRY(fp0F, PWMControl,      fp0FR16, fp0FR8)
	REG_ENTRY(fp10, PeriodcStart,    fp10R16, fp10R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x11];
	} _FP;

typedef struct __GE {
	// Primary Graphics Engine Registers
	REG_ENTRY(ge00,  DrawCmd,    	  ge00R16, ge00R8)
	REG_ENTRY1(ge01, WidthHeight,     LineDraw,   ge01R16, ge01R8)
	REG_ENTRY1(ge02, DstXY,       	  LineMajorX, ge02R16, ge02R8)
	REG_ENTRY1(ge03, SrcXY,           LineMinorY, ge03R16, ge03R8)
	REG_ENTRY(ge04,  ColorCompare,    ge04R16, ge04R8)
	REG_ENTRY(ge05,  ClipLeftTop,     ge05R16, ge05R8)
	REG_ENTRY(ge06,  ClipRightBottom, ge06R16, ge06R8)
	REG_ENTRY(ge07,  FgColor,         ge07R16, ge07R8)
	REG_ENTRY(ge08,  BgColor,         ge08R16, ge08R8)
	REG_ENTRY(ge09,  SrcStride,       ge09R16, ge09R8)
	REG_ENTRY(ge0A,  DstStride,       ge0AR16, ge0AR8)
	REG_ENTRY(ge0B,  BaseAddr,        ge0BR16, ge0BR8)
	REG_ENTRY(ge0C,  CmdStart,        ge0CR16, ge0CR8)
	REG_ENTRY(ge0D,  Reserved0D,      ge0DR16, ge0DR8)
	REG_ENTRY(ge0E,  Reserved0E,      ge0ER16, ge0ER8)
	REG_ENTRY(ge0F,  TestMode,        ge0FR16, ge0FR8)
	REG_ENTRY(ge10,  MonoPat0,        ge10R16, ge10R8)
	REG_ENTRY(ge11,  MonoPat1,        ge11R16, ge11R8)
	REG_ENTRY(ge12,  PatFgColor,      ge12R16, ge12R8)
	REG_ENTRY(ge13,  PatBgColor,      ge13R16, ge13R8)
	REG_ENTRY(ge14,  Reserved14,      ge14R16, ge14R8)
	REG_ENTRY(ge15,  Reserved15,      ge15R16, ge15R8)	
	REG_ENTRY(ge16,  Reserved16,      ge16R16, ge16R8)
	REG_ENTRY(ge17,  Reserved17,      ge17R16, ge17R8)
	REG_ENTRY(ge18,  Reserved18,      ge18R16, ge18R8)
	REG_ENTRY(ge19,  Reserved19,      ge19R16, ge19R8)			
	UInt32 Reserved[REGBLOCK_SZ - 0x1A];  
	} _GE;
	
typedef struct __G2 {
	// Secondary Graphics Engine Registers
	REG_ENTRY1(ge20,  DrawCmd,    	   VidCmd,       ge20R16, ge20R8)
	REG_ENTRY2(ge21,  WidthHeight,     VidSrcWidth,  LineDraw,   ge21R16, ge21R8)
	REG_ENTRY2(ge22,  DstXY,       	   VidSrcHeight, LineMajorX, ge22R16, ge22R8)
	REG_ENTRY2(ge23,  SrcXY,           VidSrcLine,   LineMinorY, ge23R16, ge23R8)
	REG_ENTRY1(ge24,  ColorCompare,    VidDstStart,  ge24R16, ge24R8)
	REG_ENTRY1(ge25,  ClipLeftTop,     Reserved25,   ge25R16, ge25R8)
	REG_ENTRY1(ge26,  ClipRightBottom, VidDstStride, ge26R16, ge26R8)
	REG_ENTRY1(ge27,  FgColor,         DDAVScale,    ge27R16, ge27R8)
	REG_ENTRY1(ge28,  BgColor,         DDAVInit,     ge28R16, ge28R8)
	REG_ENTRY1(ge29,  SrcStride,       DDAHScale,    ge29R16, ge29R8)
	REG_ENTRY1(ge2A,  DstStride,       DDAHInit,     ge2AR16, ge2AR8)
	REG_ENTRY1(ge2B,  BaseAddr,        VidDstWH,     ge2BR16, ge2BR8)
	REG_ENTRY1(ge2C,  CmdStart,        VidCmdStart,  ge2CR16, ge2CR8)
	REG_ENTRY1(ge2D,  Reserved0D,      Reserved2D,   ge2DR16, ge2DR8)
	REG_ENTRY1(ge2E,  Reserved0E,      Reserved2E,   ge2ER16, ge2ER8)
	REG_ENTRY1(ge2F,  TestMode,        Reserved2F,   ge2FR16, ge2FR8)
	REG_ENTRY1(ge30,  MonoPat0,        VidCSC1,      ge30R16, ge30R8)
	REG_ENTRY1(ge31,  MonoPat1,        VidCSC2,      ge31R16, ge31R8)
	REG_ENTRY1(ge32,  PatFgColor,      VidCSC3,      ge32R16, ge32R8)
	REG_ENTRY1(ge33,  PatBgColor,      VidCSC4,      ge33R16, ge33R8)
	REG_ENTRY1(ge34,  Reserved14,      ChromaKey,    ge34R16, ge34R8)
	REG_ENTRY1(ge35,  Reserved15,      VidSrcData0,  ge35R16, ge35R8)	
	REG_ENTRY1(ge36,  Reserved16,      VidSrcData1,  ge36R16, ge36R8)
	REG_ENTRY1(ge37,  Reserved17,      VidSrcData2,  ge37R16, ge37R8)
	REG_ENTRY1(ge38,  Reserved18,      VidSrcData3,  ge38R16, ge38R8)
	REG_ENTRY1(ge39,  Reserved19,      VidSrcData4,  ge39R16, ge39R8)			
	UInt32 Reserved[REGBLOCK_SZ - 0x1A];  
	} _G2;

typedef UInt32 _SF;   // Source Fifo

typedef struct __RGB {
	// Color Palette Registers
	UInt8	Red;
	UInt8	Green;
	UInt8	Blue;
	UInt8	Reserved;
	} _RGB;
	
#ifdef SUPPORT_ANONYMOUS_UNION		// Microsoft and ARM C++ Compiler		
typedef union __CP{   
		UInt32	rgb;
		_RGB	rgb8;
	} _CP;
	
#else								// ARM C Compiler
typedef union __CP {		  
		UInt32	rgb;
		_RGB 	rgb8;
	} _CP;	
#endif

typedef struct __VI {
	// Video Input Registers
	REG_ENTRY(vi00,  Control,        vi00R16, vi00R8)
	REG_ENTRY(vi01,  Command,        vi01R16, vi01R8)
	REG_ENTRY(vi02,  HOffset,        vi02R16, vi02R8)
	REG_ENTRY(vi03,  HVCounter,      vi03R16, vi03R8)
	REG_ENTRY(vi04,  VOffset,        vi04R16, vi04R8)
	REG_ENTRY(vi05,  Buf0Start,      vi05R16, vi05R8)
	REG_ENTRY(vi06,  Buf1Start,      vi06R16, vi06R8)
	REG_ENTRY(vi07,  Stride,         vi07R16, vi07R8)
	REG_ENTRY(vi08,  HDACtrl,        vi08R16, vi08R8)
	REG_ENTRY(vi09,  HLPFilter,      vi09R16, vi09R8)
	REG_ENTRY(vi0A,  VDACtrl,        vi0AR16, vi0AR8)
	REG_ENTRY(vi0B,  CSC0Ctrl,       vi0BR16, vi0BR8)
	REG_ENTRY(vi0C,  CSC1Ctrl,       vi0CR16, vi0CR8)
	REG_ENTRY(vi0D,  CSC2Ctrl,       vi0DR16, vi0DR8)
	REG_ENTRY(vi0E,  HVIRes,         vi0ER16, vi0ER8)
	REG_ENTRY(vi0F,  FifoStatus,     vi0FR16, vi0FR8)
	REG_ENTRY(vi10,  IntEnable,      vi10R16, vi10R8)
	REG_ENTRY(vi11,  IntStatus,      vi11R16, vi11R8)
	REG_ENTRY(vi12,  VHVVSync,       vi12R16, vi12R8)
	REG_ENTRY(vi13,  Reserved13,     vi13R16, vi13R8)
	REG_ENTRY(vi14,  Reserved14,     vi14R16, vi14R8)
	REG_ENTRY(vi15,  Reserved15,     vi15R16, vi15R8)
	REG_ENTRY(vi16,  Reserved16,     vi16R16, vi16R8)
	REG_ENTRY(vi17,  Reserved17,     vi17R16, vi17R8)
	REG_ENTRY(vi18,  Reserved18,     vi18R16, vi18R8)
	REG_ENTRY(vi19,  Reserved19,     vi19R16, vi19R8)
	REG_ENTRY(vi1A,  Reserved1A,     vi1AR16, vi1AR8)
	REG_ENTRY(vi1B,  IOPinCtrl0,     vi1BR16, vi1BR8)
	REG_ENTRY(vi1C,  IOPinCtrl1,     vi1CR16, vi1CR8)
	REG_ENTRY(vi1D,  IOData,         vi1DR16, vi1DR8)
	REG_ENTRY(vi1E,  Clock,          vi1ER16, vi1ER8)
	REG_ENTRY(vi1F,  Enable,         vi1FR16, vi1FR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x20];
	} _VI;

typedef UInt32 _VY;  // Y-Fifo
typedef UInt32 _VU;  // U-Fifo
typedef UInt32 _VV;  // V-Fifo

typedef struct __UD {
	// USB Device Registers
	REG_ENTRY(ud00, FunCtrl,      	ud00R16, ud00R8)
	REG_ENTRY(ud01, FunAddr,    	ud01R16, ud01R8)
	REG_ENTRY(ud02, FrameNum,  		ud02R16, ud02R8)
	REG_ENTRY(ud03, Ep0TxCtrl,    	ud03R16, ud03R8)
	REG_ENTRY(ud04, Ep0TxStatus,    ud04R16, ud04R8)
	REG_ENTRY(ud05, Ep0RxCtrl,      ud05R16, ud05R8)
	REG_ENTRY(ud06, Ep0RxStatus,   	ud06R16, ud06R8)
	REG_ENTRY(ud07, Ep0RxFifo,      ud07R16, ud07R8)
	REG_ENTRY(ud08, Ep0TxFifo,	    ud08R16, ud08R8)
	REG_ENTRY(ud09, Ep1Ctrl,	    ud09R16, ud09R8)
	REG_ENTRY(ud0A, Ep1Status,    	ud0AR16, ud0AR8)
	REG_ENTRY(ud0B, Ep1TxFifo,     	ud0BR16, ud0BR8)
	REG_ENTRY(ud0C, Ep2Ctrl, 		ud0CR16, ud0CR8)
	REG_ENTRY(ud0D, Ep2Status,     	ud0DR16, ud0DR8)
	REG_ENTRY(ud0E, Ep3Ctrl,		ud0ER16, ud0ER8)
	REG_ENTRY(ud0F, Ep3Status,     	ud0FR16, ud0FR8)
	REG_ENTRY(ud10, DevIntStatus,   ud10R16, ud10R8)
	REG_ENTRY(ud11, TestCtrl,    	ud11R16, ud11R8)
	REG_ENTRY(ud12, TestResult,     ud12R16, ud12R8)
	REG_ENTRY(ud13, Reserved13,     ud13R16, ud13R8)
	REG_ENTRY(ud14, Reserved14,     ud14R16, ud14R8)
	REG_ENTRY(ud15, Reserved15,     ud15R16, ud15R8)
	REG_ENTRY(ud16, Reserved16,     ud16R16, ud16R8)
	REG_ENTRY(ud17, Reserved17,     ud17R16, ud17R8)
	REG_ENTRY(ud18, DmaIBTxCtrl,    ud18R16, ud18R8)
	REG_ENTRY(ud19, DmaIB1Start,    ud19R16, ud19R8)
	REG_ENTRY(ud1A, DmaIB2Start,    ud1AR16, ud1AR8)
	REG_ENTRY(ud1B, DmaSMTxCtrl,    ud1BR16, ud1BR8)
	REG_ENTRY(ud1C, DmaSM1Start,    ud1CR16, ud1CR8)
	REG_ENTRY(ud1D, DmaSM2Start,    ud1DR16, ud1DR8)
	REG_ENTRY(ud1E, DmaTxSize1,     ud1ER16, ud1ER8)
	REG_ENTRY(ud1F, DmaTxSize2,     ud1FR16, ud1FR8)
	REG_ENTRY(ud20, Ep4RxCtrl,      ud20R16, ud20R8)
	REG_ENTRY(ud21, Ep4RxStatus,    ud21R16, ud21R8)
	REG_ENTRY(ud22, Ep4RxFifo,      ud22R16, ud22R8)
	REG_ENTRY(ud23, Ep5TxCtrl,      ud23R16, ud23R8)
	REG_ENTRY(ud24, Ep5TxStatus,    ud24R16, ud24R8)
	REG_ENTRY(ud25, Ep5TxFifo,      ud25R16, ud25R8)
	REG_ENTRY(ud26, Ep6RxCtrl,      ud26R16, ud26R8)
	REG_ENTRY(ud27, Ep6RxStatus,    ud27R16, ud27R8)
	REG_ENTRY(ud28, Ep6RxFifo,      ud28R16, ud28R8)
	REG_ENTRY(ud29, SM2IBBufSize,   ud29R16, ud29R8)
	REG_ENTRY(ud2A, IB2SMBufSize,   ud2AR16, ud2AR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x2B];
	} _UD;

typedef struct __DM {
	// DMA Registers
	REG_ENTRY1(dm00, Enable, SWReset,dm00R16, dm00R8)
	REG_ENTRY(dm01, IntStatus,       dm01R16, dm01R8)
	REG_ENTRY(dm02, IntEnable,       dm02R16, dm02R8)
	// SM2GE
	REG_ENTRY(dm03, CmdBufStart,     dm03R16, dm03R8)
	REG_ENTRY(dm04, CmdBufCount,     dm04R16, dm04R8)
	REG_ENTRY(dm05, CmdBufSize,      dm05R16, dm05R8)
	REG_ENTRY(dm06, CmdTxCtrl,       dm06R16, dm06R8)
	// FB2SM
	REG_ENTRY(dm07, SMAddr,          dm07R16, dm07R8)
	REG_ENTRY(dm08, FBAddr,          dm08R16, dm08R8)
	REG_ENTRY(dm09, LineSize,        dm09R16, dm09R8)
	REG_ENTRY(dm0A, LineTotal,       dm0AR16, dm0AR8)
	REG_ENTRY(dm0B, FBStride,        dm0BR16, dm0BR8)
	REG_ENTRY(dm0C, SMStride,        dm0CR16, dm0CR8)
	REG_ENTRY(dm0D, TxCtrl,          dm0DR16, dm0DR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x0E];
	} _DM;

typedef struct __SS {
	// Audio Codec Registers
	REG_ENTRY(ss00, ClockSync,      ss00R16, ss00R8)
	REG_ENTRY(ss01, ClockEnable,    ss01R16, ss01R8)
	REG_ENTRY(ss02, AC97Ctrl,  		ss02R16, ss02R8)
	REG_ENTRY(ss03, AC97DataAddr,	ss03R16, ss03R8)
	REG_ENTRY(ss04, IntEnable,      ss04R16, ss04R8)
	REG_ENTRY(ss05, Status,         ss05R16, ss05R8)
	REG_ENTRY(ss06, RecSrcSelect,   ss06R16, ss06R8)
	REG_ENTRY(ss07, Reserved07,     ss07R16, ss07R8)
	REG_ENTRY(ss08, GpioOutCtrl,    ss08R16, ss08R8)
	REG_ENTRY(ss09, GpioInCtrl,     ss09R16, ss09R8)
	REG_ENTRY(ss0A, GpioOutSel0,    ss0AR16, ss0AR8)
	REG_ENTRY(ss0B, GpioOutSel1,    ss0BR16, ss0BR8)
	REG_ENTRY(ss0C, GpioOutData,    ss0CR16, ss0CR8)
	REG_ENTRY(ss0D, GpioInData,     ss0DR16, ss0DR8)
	REG_ENTRY(ss0E, Reserved0E,     ss0ER16, ss0ER8)
	REG_ENTRY(ss0F, Reserved0F,     ss0FR16, ss0FR8)
	REG_ENTRY(ss10, CodecTxCtrl,    ss10R16, ss10R8)
	REG_ENTRY(ss11, CodecRxCtrl,    ss11R16, ss11R8)
	REG_ENTRY(ss12, Reserved12,     ss12R16, ss12R8)
	REG_ENTRY(ss13, DataPad,        ss13R16, ss13R8)
	REG_ENTRY(ss14, CodecTxBuf1,    ss14R16, ss14R8)
	REG_ENTRY(ss15, CodecTxBuf2,    ss15R16, ss15R8)
	REG_ENTRY(ss16, CodecTxWordCnt, ss16R16, ss16R8)
	REG_ENTRY(ss17, CodecRxBuf1,    ss17R16, ss17R8)
	REG_ENTRY(ss18, Reserved18,    	ss18R16, ss18R8)
	REG_ENTRY(ss19, CodecRxBuf2,    ss19R16, ss19R8)
	REG_ENTRY(ss1A, CodecRxBufSize, ss1AR16, ss1AR8)
	REG_ENTRY(ss1B, CodecRxByteCnt, ss1BR16, ss1BR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x1C];
	} _SS;
	
typedef struct __SI {
	// Serial Peripheral Registers
	REG_ENTRY(sp00, Control,       sp00R16, sp00R8)
	REG_ENTRY(sp01, Enable,        sp01R16, sp01R8)
	REG_ENTRY(sp02, Data,          sp02R16, sp02R8)
	REG_ENTRY(sp03, TimeGapCmpr,   sp03R16, sp03R8)
	REG_ENTRY(sp04, GpioDataDir,   sp04R16, sp04R8)
	REG_ENTRY(sp05, DataCount,     sp05R16, sp05R8)
	REG_ENTRY(sp06, Status,        sp06R16, sp06R8)
	REG_ENTRY(sp07, LEDCtrl,       sp07R16, sp07R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x08];
	} _SI;
	
typedef struct __SD {
	// Secure Digital Registers
	REG_ENTRY(sd00, Enable,      sd00R16, sd00R8)
	REG_ENTRY(sd01, Control,     sd01R16, sd01R8)
	REG_ENTRY(sd02, Block,       sd02R16, sd02R8)
	REG_ENTRY(sd03, TimeOut,     sd03R16, sd03R8)
	REG_ENTRY(sd04, CmdArgu,     sd04R16, sd04R8)
	REG_ENTRY(sd05, CmdStart,    sd05R16, sd05R8)
	REG_ENTRY(sd06, IntMask,     sd06R16, sd06R8)
	REG_ENTRY(sd07, Status,      sd07R16, sd07R8)
	REG_ENTRY(sd08, RespFifo,    sd08R16, sd08R8)
	REG_ENTRY(sd09, Reserved09,  sd09R16, sd09R8)
	REG_ENTRY(sd0A, TxDMA,       sd0AR16, sd0AR8)
	REG_ENTRY(sd0B, TxBuf1,      sd0BR16, sd0BR8)
	REG_ENTRY(sd0C, TxBuf2,      sd0CR16, sd0CR8)
	REG_ENTRY(sd0D, RxDMA,       sd0DR16, sd0DR8)
	REG_ENTRY(sd0E, RxBuf1,      sd0ER16, sd0ER8)
	REG_ENTRY(sd0F, RxBuf2,      sd0FR16, sd0FR8)
	REG_ENTRY(sd10, RxBufEnd,    sd10R16, sd10R8)
	REG_ENTRY(sd11, GpioDir,     sd11R16, sd11R8)
	REG_ENTRY(sd12, GpioData,    sd12R16, sd12R8)
	REG_ENTRY(sd13, TestControl, sd13R16, sd13R8)
	REG_ENTRY(sd14, TxBuf1End,   sd14R16, sd14R8)
	REG_ENTRY(sd15, TxBuf2End,   sd15R16, sd15R8)
	REG_ENTRY(sd16, RxBufSize,   sd16R16, sd16R8)
	REG_ENTRY(sd17, GpioEnable,  sd17R16, sd17R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x18];
	} _SD;

typedef struct __IC {
	// SSP (I2C) Bus Registers
	REG_ENTRY(ic00, Control,       ic00R16, ic00R8)
	REG_ENTRY(ic01, Clock,         ic01R16, ic01R8)
	REG_ENTRY(ic02, Timer,         ic02R16, ic02R8)
	REG_ENTRY(ic03, Transfer,      ic03R16, ic03R8)
	REG_ENTRY(ic04, RxData,        ic04R16, ic04R8)
	REG_ENTRY(ic05, Status,        ic05R16, ic05R8)
	REG_ENTRY(ic06, Reserved06,    ic06R16, ic06R8)
	REG_ENTRY(ic07, Reserved07,    ic07R16, ic07R8)
	REG_ENTRY(ic08, CmdStatus,     ic08R16, ic08R8)
	REG_ENTRY(ic09, IntEnable,     ic09R16, ic09R8)
	REG_ENTRY(ic0A, IntStatus,     ic0AR16, ic0AR8)
	REG_ENTRY(ic0B, GpioConfig,    ic0BR16, ic0BR8)
	REG_ENTRY(ic0C, TestData,      ic0CR16, ic0CR8)
	UInt32 Reserved[REGBLOCK_SZ - 0x0D];
	} _IC;
	
typedef UInt32 _IT;  // I2C Transmit Fifo

typedef struct __PW {
	// Pulse-Width Modulation Registers
	REG_ENTRY(pw00, Control,       pw00R16, pw00R8)
	REG_ENTRY(pw01, Clock,         pw01R16, pw01R8)
	REG_ENTRY(pw02, DutyCycle,     pw02R16, pw02R8)
	REG_ENTRY(pw03, TestData,      pw03R16, pw03R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x04];
	} _PW;
	
typedef struct __JA {
	// Java Acceleration Registers	
	REG_ENTRY(ja00, ProgStatus,    ja00R16, ja00R8)
	REG_ENTRY(ja01, ProgCounter,   ja01R16, ja01R8)
	REG_ENTRY(ja02, StackBase,     ja02R16, ja02R8)
	REG_ENTRY(ja03, TopOfStack,    ja03R16, ja03R8)
	REG_ENTRY(ja04, StackDepth,    ja04R16, ja04R8)
	REG_ENTRY(ja05, TrapBase,      ja05R16, ja05R8)
	REG_ENTRY(ja06, STBByteCode,   ja06R16, ja06R8)
	REG_ENTRY(ja07, ConstBase, 	   ja07R16, ja07R8)
	REG_ENTRY(ja08, LVarBase,      ja08R16, ja08R8)
	REG_ENTRY(ja09, IDPComb,       ja09R16, ja09R8)
	REG_ENTRY(ja0A, IDPMisc,       ja0AR16, ja0AR8)
	REG_ENTRY(ja0B, JStarPage,     ja0BR16, ja0BR8)
	REG_ENTRY(ja0C, Reserved0C,    ja0CR16, ja0CR8)
	REG_ENTRY(ja0D, Reserved0D,    ja0DR16, ja0DR8)
	REG_ENTRY(ja0E, Reserved0E,    ja0ER16, ja0ER8)
	REG_ENTRY(ja0F, IDPByteCode,   ja0FR16, ja0FR8)
	REG_ENTRY(ja10, TOSReissue,    ja10R16, ja10R8)
	REG_ENTRY(ja11, CombReissue,   ja11R16, ja11R8)
	REG_ENTRY(ja12, Reserved12,    ja12R16, ja12R8)
	REG_ENTRY(ja13, Reserved13,    ja13R16, ja13R8)
	REG_ENTRY(ja14, Reserved14,    ja14R16, ja14R8)
	REG_ENTRY(ja15, LTblData,      ja15R16, ja15R8)
	REG_ENTRY(ja16, BStatReissue,  ja16R16, ja16R8)
	REG_ENTRY(ja17, BInstReissue,  ja17R16, ja17R8)
	REG_ENTRY(ja18, SingleStep,    ja18R16, ja18R8)
	REG_ENTRY(ja19, OpStack,       ja19R16, ja19R8)
	REG_ENTRY(ja1A, BCRamData,     ja1AR16, ja1AR8)
	REG_ENTRY(ja1B, BCRamAddr,     ja1BR16, ja1BR8)
	REG_ENTRY(ja1C, LUTMisc,       ja1CR16, ja1CR8)
	REG_ENTRY(ja1D, BCCount,       ja1DR16, ja1DR8)
	REG_ENTRY(ja1E, LVarCMask,     ja1ER16, ja1ER8)
	REG_ENTRY(ja1F, LVarTag,       ja1FR16, ja1FR8)
	REG_ENTRY(ja20, BCCAddr,       ja20R16, ja20R8)
	REG_ENTRY(ja21, BCCData,       ja21R16, ja21R8)
	REG_ENTRY(ja22, Reserved22,    ja22R16, ja22R8)
	REG_ENTRY(ja23, Reserved23,    ja23R16, ja23R8)
	REG_ENTRY(ja24, Reserved24,    ja24R16, ja24R8)
	REG_ENTRY(ja25, Revision,      ja25R16, ja25R8)
	REG_ENTRY(ja26, BCCInval,      ja26R16, ja26R8)
	UInt32 Reserved[REGBLOCK_SZ - 0x27];
	} _JA;

// MQ9000 Internal Registers start at address 0x80000000.
// WARNING: DO NOT allocate this register structure at runtime (TOO BIG!)
// Instead, use it as a pointer from the device base address
typedef struct _MQ9000REGS {
	UInt32 Reserved0[0x200]; 
	_UD ud;			// 0x00800 USB Device
	_VI vi;			// 0x01000 Video Input Block
	_SS ss;			// 0x01800 Synchronous Serial I/F
	_SI sp;			// 0x02000 Serial Peripheral I/F
	_SD sd;			// 0x02800 Secure Digital block
	_KB kb;			// 0x03000 Matrix Keyboard
	_GP gp;			// 0x03800 Timer / GPIOs
	_UT1 u1;		// 0x04000 UART1 / IRDA
	_UT23 u2;		// 0x04800 UART2 / Baseband
	_UT23 u3;		// 0x05000 UART3 / BlueTooth
	_PM pm;			// 0x05800 Power Management Unit
	_IT it[0x200];		// 0x06000 I2C Transmit FIFO block
	_IC ic;			// 0x06800 I2C / SSP Bus
	_IN in;			// 0x07000 Interrupt Control Unit
	_RT rt;			// 0x07800 Real Time Clock
	_FP fp;			// 0x08000 Flat Panel I/F
	_GC gc;			// 0x08800 Graphics Controller
	_GE ge;			// 0x09000 Graphics Engine 1
	_G2 g2;			// 0x09800 Graphics Engine 2 (StretchBlt)
	_MM mm;			// 0x0A000 Embedded Memory I/F Unit
	_SB sb;			// 0x0A800 External System Bus I/F
	_DM dm;			// 0x0B000 DMA Controller
	_CC cc;			// 0x0B800 CPU Bus Adapter (CC) I/F
	UInt32 Reserved1[0x200]; 
	_PW pw;			// 0x0C800 Pulse Width Modulation
	_SF sf[0x200];		// 0x0D000 GE Source FIFO block
	UInt32 Reserved2[0x200];
	_CP cp[0x200];		// 0x0E000 GC Color Palette block
	_VY vy[0x200];		// 0x0E800 Video Source FIFO for Y  
	_VU vu[0x200];		// 0x0F000 Video Source FIFO for U (Cb)
	_VV vv[0x200];		// 0x0F800 Video Source FIFO for V (Cr)
} MQ9000REGS;

// Java Registers start at address 0x90000000. Too big of a gap when
// included in the MQ9000REGS data structure (compiler error).
typedef struct _MQ9000JAVA {
	_JA ja;			// 0x00000 Java Acceleration
} MQ9000JAVA;

#endif	//__ASSEMBLY__

#endif	//_MQ9000REGS_H_
