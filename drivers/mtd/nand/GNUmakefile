# $Id: GNUmakefile,v 1.1 2001/04/19 23:57:02 dwmw2 Exp $

ifndef CONFIG_MTD

# We're being invoked outside a normal kernel build. Fake it

CONFIG_MTD_NAND := m
CONFIG_MTD_NAND_ECC := m
# CONFIG_MTD_NAND_SPIA := m

endif

# Normal case - build in-kernel

ifeq ($(VERSION),2)
 ifneq ($(PATCHLEVEL),4)
  ifneq ($(PATCHLEVEL),5)
   OLDTOPDIR := $(TOPDIR)
   TOPDIR := $(shell pwd)/..
  endif
 endif
endif

include Makefile


