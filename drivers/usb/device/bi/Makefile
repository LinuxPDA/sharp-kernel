#
# Makefile for the kernel USBD (device not host) drivers.
#
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.

# Subdirs.
# This is a bit complex, because some subdirs are for
# proprietary code, and are simply not present in a
# general distribution.

# The all-CAPS *_DIRS get nuked in the new versions
# of Rules.make, so use only the subdir-* methods.
subdir-y 	:=
subdir-m 	:=
subdir-n 	:=
subdir- 	:=

# The target object and module list name.

# Objects that export symbols.

export-objs	:= 

# Multipart objects.

list-multi	:= gen_bi.o sl11_bi.o
gen_bi-objs	:= gen.o usbd-bi.o
sl11_bi-objs	:= sl11.o usbd-bi.o
sa1100_bi-objs	:= sa1100.o usbd-bi.o
superh_bi-objs	:= superh.o usbd-bi.o
pxa_bi-objs	:= pxa.o usbd-bi.o

# Optional parts of multipart objects.

# Object file lists.

obj-y		:= usbd-bi.o 
obj-m		:=
obj-n		:=
obj-		:=

# Each configuration option enables a list of files.

obj-$(CONFIG_USBD_SL11_BUS)		+= sl11_bi.o
obj-$(CONFIG_USBD_GENERIC_BUS)		+= sl11_bi.o
obj-$(CONFIG_USBD_SA1100_BUS)		+= sa1100_bi.o
obj-$(CONFIG_USBD_SUPERH_BUS)		+= superh_bi.o
obj-$(CONFIG_USBD_L7205_BUS)		+= l7205_bi.o
obj-$(CONFIG_USBD_PXA_BUS)		+= pxa_bi.o

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

gen_bi.o: $(gen_bi-objs)
	$(LD) -r -o $@ $(gen_bi-objs)

sl11_bi.o: $(sl11_bi-objs)
	$(LD) -r -o $@ $(sl11_bi-objs)

sa1100_bi.o: $(sa1100_bi-objs)
	$(LD) -r -o $@ $(sa1100_bi-objs)

superh_bi.o: $(superh_bi-objs)
	$(LD) -r -o $@ $(superh_bi-objs)

l7205_bi.o: $(l7205_bi-objs)
	$(LD) -r -o $@ $(l7205_bi-objs)

pxa_bi.o: $(pxa_bi-objs)
	$(LD) -r -o $@ $(pxa_bi-objs)

# dependencies:

# local


