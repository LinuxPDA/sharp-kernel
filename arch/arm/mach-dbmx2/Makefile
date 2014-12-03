#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= dbmx2.o

# Object file lists.

obj-y			:= irq.o time.o dma.o
obj-m			:=
obj-n			:=
obj-			:=

obj-$(CONFIG_ARCH_MX2ADS) += mx2ads.o

obj-$(CONFIG_RTHAL)	+= rthal.o

export-objs :=dma.o

include $(TOPDIR)/Rules.make
