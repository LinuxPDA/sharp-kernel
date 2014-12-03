#
# Makefile for MMC drivers
#

export-objs	:= mmc_core.o

obj-$(CONFIG_MMC)	+= mmc_core.o # mmc_test.o
obj-$(CONFIG_MMC_BLOCK)	+= mmc_block.o
obj-$(CONFIG_MMC_PXA)	+= mmc_pxa.o
# EXTRA_CFLAGS += -DCONFIG_MMC_DEBUG -DCONFIG_MMC_DEBUG_VERBOSE=2

O_TARGET := mmcdrivers.o

include $(TOPDIR)/Rules.make
