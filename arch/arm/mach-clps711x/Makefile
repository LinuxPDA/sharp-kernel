#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= clps711x.o

# Object file lists.

obj-y			:= irq.o mm.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= leds-p720t.o

leds-$(CONFIG_ARCH_P720T) += p720t-leds.o
obj-$(CONFIG_ARCH_P720T) += p720t.o
obj-$(CONFIG_LEDS)	+= $(leds-y)

include $(TOPDIR)/Rules.make
