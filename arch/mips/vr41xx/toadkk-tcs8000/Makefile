#
# Makefile for the NEC Eagle/Hawk specific parts of the kernel
#
# Author: Yoichi Yuasa
#         yyuasa@mvista.com or source@mvista.com
#
# Copyright 2001,2002 MontaVista Software Inc.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET := toadkk-tcs8000.o

all: toadkk-tcs8000.o

obj-y	:= init.o irq.o setup.o

obj-$(CONFIG_IDE)	+= ide-vr4181a.o
obj-$(CONFIG_PCI)	+= pci_fixup.o

#@@@@@
#obj-y	+= debug-serial.o
#@@@@@

ifdef CONFIG_REMOTE_DEBUG
obj-y += dbg_io.o             
endif

include $(TOPDIR)/Rules.make
