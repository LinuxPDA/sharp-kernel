#
# Makefile for the SA1100 bus interface driver.
#
# Copyright (C) 2001 Lineo, Inc.

# The target object and module list name.

O_TARGET	:= cdsp_bi_drv.o
M_OBJS		:= 
O_OBJS		:=

# Multipart objects.

list-multi	:= cdsp_bi.o

#cdsp_bi-objs	:= init.o ctl.o ep0.o recv.o send.o dmabuf.o
cdsp_bi-objs	:= init.o proc.o


# Optional parts of multipart objects.

# Object file lists.

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

# Each configuration option enables a list of files.

obj-$(CONFIG_USBD_CDSP_BUS)	+= cdsp_bi.o

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

cdsp_bi.o: $(cdsp_bi-objs)
	$(LD) -r -o $@ $(subst dummy,,$^)

# dependencies:

proc.o: ../usbd.h ../usbd-func.h ../usbd-bus.h cdsp.h cdsp_proc.h

init.o: ../usbd.h ../usbd-debug.h ../usbd-func.h ../usbd-bus.h cdsp.h cdsp_u2v.h cdsp_proc.h
