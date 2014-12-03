#
# Makefile for the SA1100 bus interface driver.
#
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.

# The target object and module list name.

# O_TARGET must be defined in order to compile code into
# the kernel (it collects everything in the list obj-y)

O_TARGET	:= 
M_OBJS		:= 
O_OBJS		:=

# Multipart objects.

list-multi	:= l7205_bi.o

#l7205_bi-objs	:= init.o ctl.o ep0.o recv.o send.o ../usbd-bus.o tick.o
l7205_bi-objs	:= udc.o ctl.o recv.o send.o ../usbd-bus.o ../usbd-bi.o


# Optional parts of multipart objects.

# Object file lists.

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

# Each configuration option enables a list of files.

obj-$(CONFIG_USBD_L7205_BUS)		+= l7205_bi.o

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

l7205_bi.o: $(l7205_bi-objs)
	$(LD) -r -o $@ $(subst dummy,,$^)

# dependencies:


