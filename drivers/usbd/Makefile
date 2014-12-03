#
# Makefile for the kernel USBD (device not host) drivers.
#
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.

# Subdirs.
# This is a bit complex, because some subdirs are for
# proprietary code, and are simply not present in a
# general distribution.

TOPDIR ?= ../..

ifeq ($(CONFIG_MIPS_SEAD),y)
   # Use the older method of adding modules in subdirectories
   SUB_DIRS     :=
   MOD_SUB_DIRS :=
   ALL_SUB_DIRS := cdsp_bi serial_fd net_fd
   ifeq ($(CONFIG_USBD_CDSP_BUS),y)
      SUB_DIRS += cdsp_bi
   else
      ifeq ($(CONFIG_USBD_CDSP_BUS),m)
         MOD_SUB_DIRS += cdsp_bi
      endif
   endif
   ifeq ($(CONFIG_USBD_NET),y)
      SUB_DIRS += net_fd
   else
      ifeq ($(CONFIG_USBD_NET),m)
         MOD_SUB_DIRS += net_fd
      endif
   endif
   ifeq ($(CONFIG_USBD_SERIAL),y)
      SUB_DIRS += serial_fd
   else
      ifeq ($(CONFIG_USBD_SERIAL),m)
         MOD_SUB_DIRS += serial_fd
      endif
   endif
else
   # The all-CAPS *_DIRS get nuked in the new versions
   # of Rules.make, so use only the subdir-* methods.
   subdir-y 	:=
   subdir-m 	:=
   subdir-n 	:=
   subdir- 	:=

   subdir-$(CONFIG_USBD_NET) += net_fd
   subdir-$(CONFIG_USBD_SERIAL) += serial_fd

   subdir-$(CONFIG_USBD_GENERIC_BUS) += gen_bi
   subdir-$(CONFIG_USBD_L7205_BUS) += l7205_bi
   subdir-$(CONFIG_USBD_SA1100_BUS) += sa1100_bi
   subdir-$(CONFIG_USBD_SL11_BUS) += sl11_bi
endif

# The target object and module list name.

O_TARGET	:= usbddrv.o

# Objects that export symbols.

export-objs	:= usbd.o

# Multipart objects.

list-multi		:= usbdcore.o usbdmonitor.o usbdserial.o
usbdcore-objs		:= usbd.o ep0.o usbd-debug.o hotplug.o
usbdmonitor-objs	:= usbd-monitor.o hotplug.o
usbdserial-objs	:= usbd-serialnumber.o hotplug.o

# Optional parts of multipart objects.

# Object file lists.

obj-y		:= usbd-bi.o usbd-bus.o usbd-func.o crc8.o crc10.o crc16.o crc32.o
obj-m		:=
obj-n		:=
obj-		:=

# Each configuration option enables a list of files.

obj-$(CONFIG_USBD)		+= usbdcore.o 
obj-$(CONFIG_USBD_MONITOR)	+= usbdmonitor.o 
obj-$(CONFIG_USBD_MONITOR)	+= usbdserial.o 

# Object files in subdirectories


# Extract lists of the multi-part drivers.
# The 'int-*' lists are the intermediate files used to build the multi's.

multi-y		:= $(filter $(list-multi), $(obj-y))
multi-m		:= $(filter $(list-multi), $(obj-m))
int-y		:= $(sort $(foreach m, $(multi-y), $($(basename $(m))-objs)))
int-m		:= $(sort $(foreach m, $(multi-m), $($(basename $(m))-objs)))

# Files that are both resident and modular: remove from modular.

obj-m		:= $(filter-out $(obj-y), $(obj-m))
int-m		:= $(filter-out $(int-y), $(int-m))

# Translate to Rules.make lists.

O_OBJS		:= $(filter-out $(export-objs), $(obj-y))
OX_OBJS		:= $(filter     $(export-objs), $(obj-y))
M_OBJS		:= $(sort $(filter-out $(export-objs), $(obj-m)))
MX_OBJS		:= $(sort $(filter     $(export-objs), $(obj-m)))
MI_OBJS		:= $(sort $(filter-out $(export-objs), $(int-m)))
MIX_OBJS	:= $(sort $(filter     $(export-objs), $(int-m)))

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

usbdmonitor.o: $(usbdmonitor-objs)
	$(LD) -r -o $@ $(subst dummy,,$^)

usbdserial.o: $(usbdserial-objs)
	$(LD) -r -o $@ $(subst dummy,,$^)

usbdcore.o: $(usbdcore-objs)
	$(LD) -r -o $@ $(subst dummy,,$^)

# dependencies:

usbd-monitor.o: usbd-export.h usbd-build.h
usbd-serial.o: usbd-export.h usbd-build.h
usbd.o: usbd-export.h usbd-build.h

usbd-export.h:
	echo "#define USBD_EXPORT_DATE    \"`date '+%Y-%m-%d %H:%M'`\""  > $@

usbd-build.h:
	echo "#define USBD_BUILD          \"000\"" > $@
	

# local

%.h:%.p
	release inc build < $< > $@
	cp $@ $<

release.h: release.p

inc-build:
	release inc build < release.p > release.h
	cp release.h release.p


menuconfig:
	cd ../..; make menuconfig

xconfig:
	cd ../..; make xconfig



