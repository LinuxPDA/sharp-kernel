# $Id: GNUmakefile,v 1.11 2002/04/23 13:52:14 mag Exp $

LINUXDIR=/lib/modules/$(shell uname -r)/build

ifndef VERSION

# Someone just typed 'make'

modules:
	make -C $(LINUXDIR) SUBDIRS=`pwd` modules

dep:
	make -C $(LINUXDIR) SUBDIRS=`pwd` dep

clean:
	rm -f *.o */*.o

else



ifndef CONFIG_MTD

# We're being invoked outside a normal kernel build. Fake it

# We add to $CC rather than setting EXTRA_CFLAGS because the local 
# header files _must_ appear before the in-kernel ones. 
CC += -I$(shell pwd)/../../include

CONFIG_MTD := m
CONFIG_MTD_PARTITIONS := m
CONFIG_MTD_REDBOOT_PARTS := m
CONFIG_MTD_AFS_PARTS := m
CONFIG_MTD_CHAR := m
CONFIG_MTD_BLOCK := m
CONFIG_FTL := m
CONFIG_NFTL := m

CFLAGS_nftl.o := -DCONFIG_NFTL_RW

endif

# Normal case - build in-kernel

ifeq ($(VERSION),2)
 ifneq ($(PATCHLEVEL),4)
  ifneq ($(PATCHLEVEL),5)
   OLDTOPDIR := $(TOPDIR)
   TOPDIR := $(shell pwd)
  endif
 endif
endif

ifeq ($(VERSION),2)
 ifeq ($(PATCHLEVEL),0)
   obj-y += initcalls.o
  endif
endif

include Makefile

endif
