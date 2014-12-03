# $Id: GNUmakefile,v 1.6 2001/09/02 18:57:01 dwmw2 Exp $

ifndef CONFIG_MTD

# We're being invoked outside a normal kernel build. Fake it

CONFIG_MTD := m
CONFIG_MTD_AMDSTD := m
CONFIG_MTD_CFI := m
CONFIG_MTD_CFI_AMDSTD := m
CONFIG_MTD_CFI_INTELEXT := m
CONFIG_MTD_GEN_PROBE := m
CONFIG_MTD_JEDEC := m
CONFIG_MTD_JEDECPROBE := m
CONFIG_MTD_INTELPROBE := m
CONFIG_MTD_RAM := m
CONFIG_MTD_ROM := m
CONFIG_MTD_SHARP := m
CONFIG_MTD_ABSENT := m

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


