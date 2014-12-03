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

ifdef CONFIG_SH_RTS7751R2D
obj-y  += rts7751r2d_led.o
endif

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs     :=	busmouse.o console.o keyboard.o sysrq.o \
			misc.o pty.o random.o selection.o serial.o \
			ledman.o \
			sonypi.o tty_io.o tty_ioctl.o generic_serial.o \
			au1000_gpio.o hp_psaux.o nvram.o serial_dbmx1.o \
			sharp_kbdctl.o serial_pxa200.o

mod-subdirs	:=	joystick ftape drm drm-4.0 pcmcia

list-multi	:=	

KEYMAP   =defkeymap.o
KEYBD    =pc_keyb.o
CONSOLE  =console.o
SERIAL   =serial.o
GDBSERIAL=gdbserial.o

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
      ifndef CONFIG_MAC
	 KEYBD =
      endif
   endif
   SERIAL   =
endif

ifeq ($(ARCH),m68knommu)
   ifdef CONFIG_68328_SERIAL
       KEYBD  =
       SERIAL = 68328serial.o
   else
       KEYBD =
   endif
endif

ifdef CONFIG_Q40
  KEYBD += q40_keyb.o
  SERIAL = serial.o
endif

ifdef CONFIG_APOLLO
  KEYBD += dn_keyb.o
endif

ifeq ($(ARCH),parisc)
   ifdef CONFIG_GSC_PS2
      KEYBD   = hp_psaux.o hp_keyb.o
   else
      KEYBD   =
   endif
   ifdef CONFIG_PDC_CONSOLE
      CONSOLE += pdc_console.o
   endif
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
  ifeq ($(CONFIG_ARCH_KATANA),y)
    KEYMAP   := defkeymap.o
    GDBSERIAL:=
  endif
  ifeq ($(CONFIG_DBMX1_TPM102),y)
    KEYMAP   := defkeymap.o
    GDBSERIAL:= gdbserial_dbmx1.o
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
    ifeq ($(CONFIG_ARCH_PXA_SPITZ),y)
     KEYBD   := spitz_keyb.o spitz_logkey.o
     KEYMAP  := spitz_keymap.o
    else
     KEYBD   := corgi_keyb.o corgi_logkey.o
     KEYMAP  := corgi_keymap.o
    endif
#     SERIAL   =
  endif
  ifeq ($(CONFIG_ARCH_PXA_TOSA),y)
     KEYBD   := tosa_keyb.o tosa_logkey.o
     KEYMAP  := tosa_keymap.o
#     SERIAL   =
  endif
endif

ifeq ($(ARCH),armnommu)
  ifneq ($(CONFIG_PC_KEYB),y)
    KEYBD    :=
  endif
endif

ifeq ($(ARCH),sh)
  KEYMAP   = defkeymap.o
  KEYBD    = 
  CONSOLE  = console.o
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
  ifeq ($(CONFIG_SH_MS7727RP),y)
  KEYMAP   = defkeymap.o
  KEYBD    =
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_MS7290CP),y)
  KEYMAP   = defkeymap.o
  KEYBD    =
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_RTS7751R2D),y)
  KEYMAP   = defkeymap.o
  KEYBD    =
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_7760_SOLUTION_ENGINE),y)
  KEYMAP   = defkeymap.o
  KEYBD    =
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_SOLUTION_ENGINE),y)
  KEYMAP   = defkeymap.o
  ifeq ($(CONFIG_PC_KEYB),y)
    KEYBD  = pc_keyb.o
  else
    KEYBD  =
  endif
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_KEYWEST),y)
    ifdef CONFIG_DUMMY_CONSOLE
      ifdef CONFIG_FB
        KEYMAP  = defkeymap.o
        CONSOLE  = console.o
      else
        KEYMAP  = defkeymap.o
#       CONSOLE  = console_sci.o
        CONSOLE  = console.o
      endif
    else
      KEYMAP  = defkeymap.o
      CONSOLE  = console.o
    endif
    ifdef CONFIG_HD64465
      KEYBD   = pc_keyb_hd64465.o
    else
      KEYBD   = pc_keyb.o
    endif
  endif
endif

ifeq ($(CONFIG_DECSTATION),y)
  KEYMAP   =
  KEYBD    =
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

ifeq ($(CONFIG_DUMMY_KEYB),y)
  KEYBD = dummy_keyb.o
endif

obj-$(CONFIG_REMOTE_DEBUG) += $(GDBSERIAL)
obj-$(CONFIG_KGDB) += $(GDBSERIAL)
obj-$(CONFIG_VT) += vt.o vc_screen.o consolemap.o consolemap_deftbl.o $(CONSOLE) selection.o
obj-$(CONFIG_SERIAL) += $(SERIAL)
obj-$(CONFIG_SERIAL_HCDP) += hcdp_serial.o
obj-$(CONFIG_SERIAL_21285) += serial_21285.o
obj-$(CONFIG_SERIAL_SA1100) += serial_sa1100.o
obj-$(CONFIG_TS_AU1X00_ADS7846) += au1000_ts.o
obj-$(CONFIG_SERIAL_DEC) += decserial.o
obj-$(CONFIG_TOUCHSCREEN_MQSPI) += mq9000_ts.o
obj-$(CONFIG_TOUCHSCREEN_TPM102) += tpm102_ts.o
obj-$(CONFIG_TS_MS7727RP) += ms7727rp_ts.o
obj-$(CONFIG_TOUCHSCREEN_MX2ADS) += mx2-digi.o
obj-$(CONFIG_LED_SW_MX2ADS) += mx2-led-sw.o

obj-$(CONFIG_TS_MS7760CP) += ms77xxxx_ts.o
obj-$(CONFIG_RTC_MS7760CP) += ms77xxxx_rtc.o
obj-$(CONFIG_KEYSW_MS7760CP) += ms77xxxx_keysw.o
obj-$(CONFIG_DIPSW_MS7760CP) += ms77xxxx_dipsw.o
obj-$(CONFIG_LED_MS7760CP) += ms77xxxx_led.o
obj-$(CONFIG_FLIGHT_MS7760CP) += ms77xxxx_flight.o
obj-$(CONFIG_SERIAL_EEPROM_MS7760CP) += ms77xxxx_serial_eeprom.o
obj-$(CONFIG_H8_3048F_ONE_MS7760CP) += ms7760cp_h8.o

ifndef CONFIG_SUN_KEYBOARD
  obj-$(CONFIG_VT) += keyboard.o $(KEYMAP) $(KEYBD)
else
  obj-$(CONFIG_PCI) += keyboard.o $(KEYMAP)
endif

#
# uClinux drivers
#
obj-$(CONFIG_68328_SERIAL) += 68328serial.o
obj-$(CONFIG_SERIAL_DSC21) += serial_dsc21.o
obj-$(CONFIG_SERIAL_ATMEL) += serial_atmel.o
obj-$(CONFIG_SERIAL_NETARM) += serial_netarm.o
obj-$(CONFIG_SERIAL_SAMSUNG) += serial_samsung.o
obj-$(CONFIG_SERIAL_S3C4530) += serial_s3c4530.o
obj-$(CONFIG_V850E_NB85E_UART) += nb85e_uart.o generic_serial.o
obj-$(CONFIG_COLDFIRE_SERIAL) += mcfserial.o
obj-$(CONFIG_MCF_QSPI) += mcf_qspi.o
obj-$(CONFIG_LEDMAN) += ledman.o
obj-$(CONFIG_LCDTXT) += lcdtxt.o
obj-$(CONFIG_68328_DIGI) += mc68328digi.o
obj-$(CONFIG_M5249AUDIO) += m5249audio.o
obj-$(CONFIG_DRAGONIX_SPI) += dragonixspi.o
obj-$(CONFIG_DS1302) += ds1302.o


obj-$(CONFIG_HIL) += hp_keyb.o
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
obj-$(CONFIG_SIBYTE_SB1250_DUART) += sb1250_duart.o
obj-$(CONFIG_COMPUTONE) += ip2.o ip2main.o
obj-$(CONFIG_RISCOM8) += riscom8.o
obj-$(CONFIG_ISI) += isicom.o
obj-$(CONFIG_ESPSERIAL) += esp.o
obj-$(CONFIG_SYNCLINK) += synclink.o
obj-$(CONFIG_SYNCLINKMP) += synclinkmp.o
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
obj-$(CONFIG_HVC_CONSOLE) += hvc_console.o
obj-$(CONFIG_SERIAL_TX3912) += generic_serial.o serial_tx3912.o
obj-$(CONFIG_TXX927_SERIAL) += serial_txx927.o
obj-$(CONFIG_SERIAL_DBMX1) += serial_dbmx1.o
obj-$(CONFIG_MX2_INT_UART) += mx2uart.o
obj-$(CONFIG_SERIAL_SL_SERIES) += serial_pxa200.o
obj-$(CONFIG_ARCH_PXA_SPITZ) += generic_serial.o

subdir-$(CONFIG_RIO) += rio
subdir-$(CONFIG_INPUT) += joystick

obj-$(CONFIG_ATIXL_BUSMOUSE) += atixlmouse.o
obj-$(CONFIG_LOGIBUSMOUSE) += logibusmouse.o
obj-$(CONFIG_PRINTER) += lp.o

ifeq ($(CONFIG_INPUT),y)
obj-y += joystick/js.o
endif

obj-$(CONFIG_DISCOVERY_LED) += sharp_led.o discovery_led.o
obj-$(CONFIG_ARCH_PXA_POODLE) += sharp_led.o sharp_kbdctl.o sharpsl_led.o sharp_buzzer.o
obj-$(CONFIG_ARCH_PXA_CORGI) += sharp_led.o sharpsl_led.o sharp_kbdctl.o corgi_rc.o sharp_buzzer.o
obj-$(CONFIG_ARCH_PXA_TOSA) += sharp_led.o sharpsl_led.o sharp_kbdctl.o
obj-$(CONFIG_BUZZER_TOSA) += sharp_buzzer.o

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
obj-$(CONFIG_MK712_MOUSE) += mk712.o
ifeq ($(ARCH),sh)
ifdef CONFIG_RTC_9701JE
obj-$(CONFIG_RTC_9701JE) += rtc9701_rtc.o
else
ifneq ($(CONFIG_SH_7760_SOLUTION_ENGINE),y)
obj-$(CONFIG_RTC) += sh-rtc.o
endif
endif
else
obj-$(CONFIG_RTC) += rtc.o
endif
obj-$(CONFIG_MQ9000_RTC) += mq9000_rtc.o
obj-$(CONFIG_EFI_RTC) += efirtc.o
obj-$(CONFIG_PXA_RTC) += sa1100-rtc.o
obj-$(CONFIG_DBMX2_RTC) += mx2-rtc.o
obj-$(CONFIG_COTULLA_RTC) += cotulla-rtc.o
obj-$(CONFIG_ADS7846_TS) += ads7846_ts.o
obj-$(CONFIG_TOSA_TS) += tosa_ts.o
ifeq ($(CONFIG_PPC),)
  obj-$(CONFIG_NVRAM) += nvram.o
endif
obj-$(CONFIG_TOSHIBA) += toshiba.o
obj-$(CONFIG_I8K) += i8k.o
obj-$(CONFIG_DS1620) += ds1620.o
obj-$(CONFIG_INTEL_RNG) += i810_rng.o
obj-$(CONFIG_AMD_RNG) += amd768_rng.o
obj-$(CONFIG_AMD_PM768) += amd76x_pm.o

obj-$(CONFIG_ITE_GPIO) += ite_gpio.o
obj-$(CONFIG_AU1X00_GPIO) += au1000_gpio.o
obj-$(CONFIG_AU1000_USB_TTY) += au1000_usbtty.o
obj-$(CONFIG_AU1000_USB_RAW) += au1000_usbraw.o
obj-$(CONFIG_COBALT_LCD) += lcd.o

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

obj-$(CONFIG_XYMEM) += xymem.o

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
obj-$(CONFIG_W83877F_WDT) += w83877f_wdt.o
obj-$(CONFIG_SC520_WDT) += sc520_wdt.o
obj-$(CONFIG_WDT) += wdt.o
obj-$(CONFIG_WDTPCI) += wdt_pci.o
obj-$(CONFIG_21285_WATCHDOG) += wdt285.o
obj-$(CONFIG_977_WATCHDOG) += wdt977.o
obj-$(CONFIG_I810_TCO) += i810-tco.o
obj-$(CONFIG_MACHZ_WDT) += machzwd.o
obj-$(CONFIG_SH_WDT) += shwdt.o
obj-$(CONFIG_EUROTECH_WDT) += eurotechwdt.o
obj-$(CONFIG_ALIM7101_WDT) += alim7101_wdt.o
#obj-$(CONFIG_ALIM1535_WDT) += alim1535d_wdt.o
obj-$(CONFIG_INDYDOG) += indydog.o
obj-$(CONFIG_SC1200_WDT) += sc1200wdt.o
obj-$(CONFIG_WAFER_WDT) += wafer5823wdt.o
obj-$(CONFIG_SOFT_WATCHDOG) += softdog.o
obj-$(CONFIG_AMD7XX_TCO) += amd7xx_tco.o
obj-$(CONFIG_INDYDOG) += indydog.o

subdir-$(CONFIG_MWAVE) += mwave
ifeq ($(CONFIG_MWAVE),y)
  obj-y += mwave/mwave.o
endif

include $(TOPDIR)/Rules.make

fastdep:

conmakehash: conmakehash.c
	$(HOSTCC) $(HOSTCFLAGS) -o conmakehash conmakehash.c

consolemap_deftbl.c: $(FONTMAPFILE) conmakehash
	./conmakehash $(FONTMAPFILE) > consolemap_deftbl.c

consolemap_deftbl.o: consolemap_deftbl.c $(TOPDIR)/include/linux/types.h

.DELETE_ON_ERROR:

defkeymap.c: defkeymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

qtronixmap.c: qtronixmap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

poodle_keymap.c: poodle_keymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

corgi_keymap.c: corgi_keymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

tosa_keymap.c: tosa_keymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

spitz_keymap.c: spitz_keymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@
