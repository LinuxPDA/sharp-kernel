# $Id: Makefile,v 1.3 2001/04/11 02:29:56 randy Exp $
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1999  Kaz Kojima
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#

#
# Select the object file format to substitute into the linker script.
#
tool_prefix	= sh3-linux-

ifdef CONFIG_CPU_LITTLE_ENDIAN
CFLAGS		+= -ml
AFLAGS		+= -ml
# LINKFLAGS	+= -EL
LDFLAGS		:= -EL
else
CFLAGS		+= -mb
AFLAGS		+= -mb
# LINKFLAGS	+= -EB
LDFLAGS		:= -EB
endif

# ifdef CONFIG_CROSSCOMPILE
CROSS_COMPILE  = $(tool_prefix)
# endif

LD	=$(CROSS_COMPILE)ld $(LDFLAGS)
OBJCOPY=$(CROSS_COMPILE)objcopy -O binary -R .note -R .comment -R .stab -R .stabstr -S

MODFLAGS	+=

#
#

ifdef CONFIG_CPU_SH2
CFLAGS		+= -m2
AFLAGS		+= -m2
TEXTADDR         = 0x60000000
endif

#
# Choosing incompatible machines durings configuration will result in
# error messages during linking.  Select a default linkscript if
# none has been choosen above.
#
LINKSCRIPT    = arch/shnommu/vmlinux.lds
LINKFLAGS     += -T $(word 1,$(LINKSCRIPT)) -e _stext

ifdef LOADADDR
LINKFLAGS     += -Ttext $(word 1,$(LOADADDR))
endif

#
CFLAGS		+= -pipe

HEAD := arch/shnommu/kernel/head.o arch/shnommu/kernel/init_task.o

SUBDIRS := $(SUBDIRS) $(addprefix arch/shnommu/, kernel mm lib)
CORE_FILES := arch/shnommu/kernel/kernel.o arch/shnommu/mm/mm.o $(CORE_FILES)
LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LIBS := $(TOPDIR)/arch/shnommu/lib/lib.a $(LIBS) $(TOPDIR)/arch/shnommu/lib/lib.a \
	$(LIBGCC)

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

vmlinux: arch/shnommu/vmlinux.lds

#arch/shnommu/vmlinux.lds: arch/shnommu/vmlinux.lds.S FORCE
#	$(CPP) -traditional -C -P -I$(HPATH) -Ush arch/shnommu/vmlinux.lds.S > vmlinux.lds

arch/shnommu/vmlinux.lds: arch/shnommu/vmlinux-sh2.lds.in FORCE
	@sed 's/TEXTADDR/$(TEXTADDR)/' <$< >tmp.ld
	@sed 's/DATAADDR/$(DATAADDR)/' <tmp.ld >$@
	$(RM) tmp.ld

FORCE: ;

zImage: linux
	@$(MAKEBOOT) zImage

compressed: zImage

zdisk: linux
	@$(MAKEBOOT) zdisk

archclean:
	@$(MAKEBOOT) clean
	$(MAKE) -C arch/$(ARCH)/kernel clean
#	$(MAKE) -C arch/$(ARCH)/tools clean

archmrproper:
	rm -f arch/shnommu/vmlinux.lds

archdep:
	@$(MAKEBOOT) dep
