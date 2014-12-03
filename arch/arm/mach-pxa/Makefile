#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= pxa.o

obj-y :=
obj-m :=
obj-n :=
obj-  :=

export-objs := generic.o irq.o dma.o sa1111.o \
               usb_ctl.o usb_recv.o usb_send.o \
	       discovery.o cotulla_dma.o pxa_ssp.o tosa.o \
	       tosa_ac97.o

# Common support (must be linked before board specific support)
obj-y += generic.o irq.o
ifneq ($(CONFIG_COTULLA_DMA),y)
obj-y += dma.o
endif
obj-$(CONFIG_SA1111) += sa1111.o

# Specific board support
obj-$(CONFIG_ARCH_LUBBOCK) += lubbock.o
obj-$(CONFIG_ARCH_PXA_IDP) += idp.o
obj-$(CONFIG_ARCH_PXA_CERF) += cerf.o
obj-$(CONFIG_COTULLA_DMA) += cotulla_dma.o
obj-$(CONFIG_SABINAL_DISCOVERY) += discovery.o discovery_arch.o pxa_ssp.o
obj-$(CONFIG_ARCH_PXA_POODLE) += poodle.o m62332.o pxa_ssp.o poodle_buzzer.o
obj-$(CONFIG_ARCH_PXA_CORGI) += corgi.o pxa_ssp.o poodle_buzzer.o
obj-$(CONFIG_ARCH_PXA_TOSA) += tosa.o pxa_nssp.o tosa_ac97.o
obj-$(CONFIG_BUZZER_TOSA) += tosa_buzzer.o

# Support for blinky lights
leds-y := leds.o
leds-$(CONFIG_ARCH_LUBBOCK) += leds-lubbock.o
leds-$(CONFIG_ARCH_PXA_IDP) += leds-idp.o
leds-$(CONFIG_ARCH_PXA_CERF) += leds-cerf.o

obj-$(CONFIG_LEDS) += $(leds-y)

# PXA USB client support
list-multi += pxausb_core.o
pxausb_core-objs := usb_ctl.o usb_ep0.o usb_recv.o usb_send.o
obj-$(CONFIG_PXA_USB) += pxausb_core.o
obj-$(CONFIG_PXA_USB_NETLINK) += usb-eth.o
obj-$(CONFIG_PXA_USB_CHAR) += usb-char.o

# Misc features
ifeq ($(CONFIG_ARCH_SHARP_SL),y)
  obj-$(CONFIG_APM) += sharpsl_apm.o
  obj-$(CONFIG_PM) += sharpsl_power.o sharpsl_suspend.o
  export-objs += sharpsl_apm.o sharpsl_power.o
  ifeq ($(CONFIG_SABINAL_DISCOVERY),y)
    obj-$(CONFIG_BATT) += discovery_battery.o
    export-objs += discovery_battery.o
  endif
  ifeq ($(CONFIG_ARCH_PXA_POODLE),y)
    obj-$(CONFIG_BATT) += sharpsl_battery.o sharpsl_param.o
    export-objs += sharpsl_battery.o
  endif
  ifeq ($(CONFIG_ARCH_PXA_CORGI),y)
    obj-$(CONFIG_BATT) += sharpsl_battery.o sharpsl_param.o
    export-objs += sharpsl_battery.o
  endif
  ifeq ($(CONFIG_ARCH_PXA_TOSA),y)
    obj-$(CONFIG_BATT) += tosa_battery.o sharpsl_param.o
    export-objs += tosa_battery.o
  endif
else
  obj-$(CONFIG_PM) += pm.o sleep.o
endif

ifeq ($(CONFIG_DEVICEINFO),y)
  obj-$(CONFIG_DEVICEINFO) += devinfo.o
  ifeq ($(CONFIG_SABINAL_DISCOVERY),y)
    devinfo-objs-m += deviceinfo.o
    devinfo-objs-$(CONFIG_DISCOVERY_DEVICEINFO) += discovery_deviceinfo.o
  endif
  ifeq ($(CONFIG_ARCH_PXA_POODLE),y)
    devinfo-objs-m += sharpsl_deviceinfo.o
  endif
  ifeq ($(CONFIG_ARCH_PXA_CORGI),y)
    devinfo-objs-m += sharpsl_deviceinfo.o
  endif
  ifeq ($(CONFIG_ARCH_PXA_TOSA),y)
    devinfo-objs-m += sharpsl_deviceinfo.o
  endif
endif

obj-m += registers.o

include $(TOPDIR)/Rules.make

pxausb_core.o: $(pxausb_core-objs)
	$(LD) -r -o $@ $(pxausb_core-objs)

devinfo.o: $(devinfo-objs-m)
	$(LD) -r -o $@ $(devinfo-objs-m)
