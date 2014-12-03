#
# Makefile for the kernel character device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

#
# This file contains the font map for the default (hardware) font
#
FONTMAPFILE = cp437.uni

O_TARGET := char.o

obj-y	 += mem.o tty_io.o n_tty.o tty_ioctl.o raw.o pty.o misc.o random.o

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs     :=	busmouse.o console.o keyboard.o sysrq.o \
			misc.o pty.o random.o selection.o serial.o \
			sonypi.o tty_io.o tty_ioctl.o generic_serial.o \
			sharp_kbdctl.o serial_pxa200.o

mod-subdirs	:=	joystick ftape drm drm-4.0 pcmcia

list-multi	:=	

KEYMAP   =defkeymap.o
KEYBD    =pc_keyb.o
CONSOLE  =console.o
SERIAL   =serial.o

ifeq ($(ARCH),s390)
  KEYMAP   =
  KEYBD    =
  CONSOLE  =
  SERIAL   =
endif

ifeq ($(ARCH),mips)
  ifneq ($(CONFIG_PC_KEYB),y)
    KEYBD    =
  endif
endif

ifeq ($(ARCH),s390x)
  KEYMAP   =
  KEYBD    =
  CONSOLE  =
  SERIAL   =
endif

ifeq ($(ARCH),m68k)
   ifdef CONFIG_AMIGA
      KEYBD = amikeyb.o
   else
      KEYBD =
   endif
   SERIAL   =
endif

ifeq ($(ARCH),arm)
  KEYMAP     :=
  KEYBD      :=
  ifeq ($(CONFIG_PC_KEYMAP),y)
    KEYMAP   := defkeymap.o
  endif
  ifeq ($(CONFIG_PC_KEYB),y)
    KEYBD    += pc_keyb.o
  endif
  ifeq ($(CONFIG_KMI_KEYB),y)
    KEYBD    += amba_kmi_keyb.o
  endif
  ifeq ($(CONFIG_SA1111),y)
    KEYBD    += sa1111_keyb.o
  endif
  ifeq ($(CONFIG_ARCH_EDB7211),y)
    KEYBD    += edb7211_keyb.o
  endif
  ifeq ($(CONFIG_ARCH_AUTCPU12),y)
    KEYMAP   := defkeymap.o
    KEYBD    += clps711x_keyb.o
  endif
  ifeq ($(CONFIG_SA1100_GRAPHICSCLIENT),y)
    KEYMAP    = gckeymap.o
    KEYBD    += gc_keyb.o
  endif
  ifeq ($(CONFIG_SA1100_CERF_CPLD),y)
    KEYBD    += cerf_keyb.o
  endif
  ifeq ($(CONFIG_ARCH_FORTUNET),y)
    KEYMAP   := defkeymap.o
  endif
  ifeq ($(CONFIG_SA1100_COLLIE),y)
     KEYBD   := collie_keyb.o collie_logkey.o
     KEYMAP  := collie_keymap.o
  endif
  ifeq ($(CONFIG_SABINAL_DISCOVERY),y)
    KEYBD    := discovery_keyb.o sharp_logkey.o
    SERIAL   =
  endif
  ifeq ($(CONFIG_ARCH_PXA_POODLE),y)
     KEYBD   := poodle_keyb.o poodle_logkey.o
     KEYMAP  := poodle_keymap.o
#     SERIAL   =
  endif
  ifeq ($(CONFIG_ARCH_PXA_CORGI),y)
     KEYBD   := corgi_keyb.o corgi_logkey.o
     KEYMAP  := corgi_keymap.o
#     SERIAL   =
  endif
endif

ifeq ($(ARCH),sh)
  KEYMAP   =
  KEYBD    =
  CONSOLE  =
  ifeq ($(CONFIG_SH_HP600),y)
  KEYMAP   = defkeymap.o
  KEYBD    = scan_keyb.o hp600_keyb.o
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_DMIDA),y)
  # DMIDA does not connect the HD64465 PS/2 keyboard port
  # but we allow for USB keyboards to be plugged in.
  KEYMAP   = defkeymap.o
  KEYBD    = # hd64465_keyb.o pc_keyb.o
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_EC3104),y)
  KEYMAP   = defkeymap.o
  KEYBD    = ec3104_keyb.o
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_DREAMCAST),y)
  KEYMAP   = defkeymap.o
  KEYBD    =
  CONSOLE  = console.o
  endif
endif

ifeq ($(CONFIG_DECSTATION),y)
  KEYMAP   =
  KEYBD    =
  SERIAL   = decserial.o
endif

ifeq ($(CONFIG_BAGET_MIPS),y)
  KEYBD    =
  SERIAL   =
endif

ifeq ($(CONFIG_NINO),y)
  SERIAL   =
endif

ifneq ($(CONFIG_SUN_SERIAL),)
  SERIAL   =
endif

ifeq ($(CONFIG_QTRONIX_KEYBOARD),y)
  KEYBD    = qtronix.o
  KEYMAP   = qtronixmap.o
endif

obj-$(CONFIG_VT) += vt.o vc_screen.o consolemap.o consolemap_deftbl.o $(CONSOLE) selection.o
obj-$(CONFIG_SERIAL) += $(SERIAL)
obj-$(CONFIG_SERIAL_ACPI) += acpi_serial.o
obj-$(CONFIG_SERIAL_COLLIE) += serial_collie.o
obj-$(CONFIG_TOUCHSCREEN_ANAKIN) += anakin_ts.o

list-multi += ucb1200_core.o
ucb1200_core-objs := collie_ucb1200_generic.o collie_ucb1200_sa1100.o collie_ucb1200_adc_exc.o
export-objs += collie_ucb1200_generic.o collie_ucb1200_sa1100.o collie_ucb1200_adc_exc.o
obj-$(CONFIG_UCB1200) += ucb1200_core.o
obj-$(CONFIG_TOUCHSCREEN_UCB1200) += collie_ucb1200_ts.o
obj-$(CONFIG_ADC_UCB1200) += collie_ucb1200_adc.o
obj-$(CONFIG_AUDIO_UCB1200) += collie_ucb1200_audio.o

ifndef CONFIG_SUN_KEYBOARD
  obj-$(CONFIG_VT) += keyboard.o $(KEYMAP) $(KEYBD)
else
  obj-$(CONFIG_PCI) += keyboard.o $(KEYMAP)
endif

obj-$(CONFIG_MAGIC_SYSRQ) += sysrq.o
obj-$(CONFIG_ATARI_DSP56K) += dsp56k.o
obj-$(CONFIG_ROCKETPORT) += rocket.o
obj-$(CONFIG_MOXA_SMARTIO) += mxser.o
obj-$(CONFIG_MOXA_INTELLIO) += moxa.o
obj-$(CONFIG_DIGI) += pcxx.o
obj-$(CONFIG_DIGIEPCA) += epca.o
obj-$(CONFIG_CYCLADES) += cyclades.o
obj-$(CONFIG_STALLION) += stallion.o
obj-$(CONFIG_ISTALLION) += istallion.o
obj-$(CONFIG_COMPUTONE) += ip2.o ip2main.o
obj-$(CONFIG_RISCOM8) += riscom8.o
obj-$(CONFIG_ISI) += isicom.o
obj-$(CONFIG_ESPSERIAL) += esp.o
obj-$(CONFIG_SYNCLINK) += synclink.o
obj-$(CONFIG_N_HDLC) += n_hdlc.o
obj-$(CONFIG_SPECIALIX) += specialix.o
obj-$(CONFIG_AMIGA_BUILTIN_SERIAL) += amiserial.o
obj-$(CONFIG_A2232) += ser_a2232.o generic_serial.o
obj-$(CONFIG_SX) += sx.o generic_serial.o
obj-$(CONFIG_RIO) += rio/rio.o generic_serial.o
obj-$(CONFIG_SH_SCI) += sh-sci.o generic_serial.o
obj-$(CONFIG_SERIAL167) += serial167.o
obj-$(CONFIG_MVME147_SCC) += generic_serial.o vme_scc.o
obj-$(CONFIG_MVME162_SCC) += generic_serial.o vme_scc.o
obj-$(CONFIG_BVME6000_SCC) += generic_serial.o vme_scc.o
obj-$(CONFIG_SERIAL_TX3912) += generic_serial.o serial_tx3912.o
obj-$(CONFIG_SERIAL_SL_SERIES) += serial_pxa200.o

subdir-$(CONFIG_RIO) += rio
subdir-$(CONFIG_INPUT) += joystick

obj-$(CONFIG_ATIXL_BUSMOUSE) += atixlmouse.o
obj-$(CONFIG_LOGIBUSMOUSE) += logibusmouse.o
obj-$(CONFIG_PRINTER) += lp.o

ifeq ($(CONFIG_INPUT),y)
obj-y += joystick/js.o
endif

obj-$(CONFIG_SA1100_COLLIE) += sharp_led.o sharp_kbdctl.o sharp_buzzer.o
obj-$(CONFIG_DISCOVERY_LED) += sharp_led.o discovery_led.o
obj-$(CONFIG_ARCH_PXA_POODLE) += sharp_led.o sharp_kbdctl.o sharpsl_led.o sharp_buzzer.o
obj-$(CONFIG_ARCH_PXA_CORGI) += sharp_led.o sharpsl_led.o sharp_kbdctl.o corgi_rc.o sharp_buzzer.o
obj-$(CONFIG_BUSMOUSE) += busmouse.o
obj-$(CONFIG_DTLK) += dtlk.o
obj-$(CONFIG_R3964) += n_r3964.o
obj-$(CONFIG_APPLICOM) += applicom.o
obj-$(CONFIG_SONYPI) += sonypi.o
obj-$(CONFIG_MS_BUSMOUSE) += msbusmouse.o
obj-$(CONFIG_82C710_MOUSE) += qpmouse.o
obj-$(CONFIG_AMIGAMOUSE) += amigamouse.o
obj-$(CONFIG_ATARIMOUSE) += atarimouse.o
obj-$(CONFIG_ADBMOUSE) += adbmouse.o
obj-$(CONFIG_PC110_PAD) += pc110pad.o
obj-$(CONFIG_RTC) += rtc.o
obj-$(CONFIG_EFI_RTC) += efirtc.o
obj-$(CONFIG_SA1100_RTC) += sa1100-rtc.o
obj-$(CONFIG_COTULLA_RTC) += cotulla-rtc.o
obj-$(CONFIG_ADS7846_TS) += ads7846_ts.o
ifeq ($(CONFIG_PPC),)
  obj-$(CONFIG_NVRAM) += nvram.o
endif
obj-$(CONFIG_TOSHIBA) += toshiba.o
obj-$(CONFIG_I8K) += i8k.o
obj-$(CONFIG_DS1620) += ds1620.o
obj-$(CONFIG_INTEL_RNG) += i810_rng.o

obj-$(CONFIG_QIC02_TAPE) += tpqic02.o

subdir-$(CONFIG_FTAPE) += ftape
subdir-$(CONFIG_DRM_OLD) += drm-4.0
subdir-$(CONFIG_DRM_NEW) += drm
subdir-$(CONFIG_PCMCIA) += pcmcia
subdir-$(CONFIG_AGP) += agp

ifeq ($(CONFIG_FTAPE),y)
obj-y       += ftape/ftape.o
endif

obj-$(CONFIG_H8) += h8.o
obj-$(CONFIG_PPDEV) += ppdev.o
obj-$(CONFIG_DZ) += dz.o
obj-$(CONFIG_NWBUTTON) += nwbutton.o
obj-$(CONFIG_NWFLASH) += nwflash.o
obj-$(CONFIG_SA1100_CONSUS) += consusbutton.o

# Only one watchdog can succeed. We probe the hardware watchdog
# drivers first, then the softdog driver.  This means if your hardware
# watchdog dies or is 'borrowed' for some reason the software watchdog
# still gives you some cover.

obj-$(CONFIG_PCWATCHDOG) += pcwd.o
obj-$(CONFIG_ACQUIRE_WDT) += acquirewdt.o
obj-$(CONFIG_ADVANTECH_WDT) += advantechwdt.o
obj-$(CONFIG_IB700_WDT) += ib700wdt.o
obj-$(CONFIG_MIXCOMWD) += mixcomwd.o
obj-$(CONFIG_60XX_WDT) += sbc60xxwdt.o
obj-$(CONFIG_WDT) += wdt.o
obj-$(CONFIG_WDTPCI) += wdt_pci.o
obj-$(CONFIG_21285_WATCHDOG) += wdt285.o
obj-$(CONFIG_977_WATCHDOG) += wdt977.o
obj-$(CONFIG_I810_TCO) += i810-tco.o
obj-$(CONFIG_MACHZ_WDT) += machzwd.o
obj-$(CONFIG_SH_WDT) += shwdt.o
obj-$(CONFIG_EUROTECH_WDT) += eurotechwdt.o
obj-$(CONFIG_SA1100_WATCHDOG) += sa1100_wdt.o
obj-$(CONFIG_PXA_WATCHDOG) += sa1100_wdt.o
obj-$(CONFIG_SOFT_WATCHDOG) += softdog.o

subdir-$(CONFIG_MWAVE) += mwave
ifeq ($(CONFIG_MWAVE),y)
  obj-y += mwave/mwave.o
endif

ifeq ($(CONFIG_ARCH_ACORN),y)
mod-subdirs	+= ../acorn/char
subdir-y	+= ../acorn/char
obj-y		+= ../acorn/char/acorn-char.o
endif

include $(TOPDIR)/Rules.make

fastdep:

conmakehash: conmakehash.c
	$(HOSTCC) $(HOSTCFLAGS) -o conmakehash conmakehash.c

consolemap_deftbl.c: $(FONTMAPFILE) conmakehash
	./conmakehash $(FONTMAPFILE) > consolemap_deftbl.c

consolemap_deftbl.o: consolemap_deftbl.c $(TOPDIR)/include/linux/types.h

ucb1200_core.o: $(ucb1200_core-objs)
	$(LD) -r -o $@ $(ucb1200_core-objs)

.DELETE_ON_ERROR:

defkeymap.c: defkeymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

qtronixmap.c: qtronixmap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

collie_keymap.c: collie_keymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

poodle_keymap.c: poodle_keymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

corgi_keymap.c: corgi_keymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@
