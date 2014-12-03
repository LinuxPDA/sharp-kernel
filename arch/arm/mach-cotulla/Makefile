#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= cotulla.o

# Object file lists.

obj-y                   := arch.o mm.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= irq.o dma.o apm.o discovery.o

obj-$(CONFIG_ARCH_COTULLA) += irq.o
obj-$(CONFIG_COTULLA_DMA) += dma.o
obj-$(CONFIG_SABINAL_DISCOVERY) += discovery.o


ifeq ($(CONFIG_ARCH_COTULLA),y)
obj-$(CONFIG_APM) += discovery_apm.o
obj-$(CONFIG_PM) += discovery_power.o discovery_suspend.o
obj-$(CONFIG_BATT) += discovery_battery.o
export-objs += discovery_apm.o discovery_power.o discovery_battery.o
endif

ifeq ($(CONFIG_DEVICEINFO),m)
  obj-$(CONFIG_DEVICEINFO) += devinfo.o
  devinfo-objs-m += deviceinfo.o
  devinfo-objs-$(CONFIG_DISCOVERY_DEVICEINFO) += discovery_deviceinfo.o
endif

obj-m += registers.o

include $(TOPDIR)/Rules.make

devinfo.o: $(devinfo-objs-m)
	$(LD) -r -o $@ $(devinfo-objs-m)

