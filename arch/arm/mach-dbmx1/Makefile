#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= dbmx1.o

# Object file lists.

obj-y			:= irq.o time.o
obj-m			:=
obj-n			:=
obj-			:=

obj-$(CONFIG_DBMX1_TPM102) += tpm102.o

obj-$(CONFIG_APM)	+= apm.o
obj-$(CONFIG_PM)	+= pm.o sleep.o
obj-$(CONFIG_RTHAL)	+= rthal.o

include $(TOPDIR)/Rules.make
