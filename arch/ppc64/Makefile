# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
# Changes for PPC by Gary Thomas
# Rewritten by Cort Dougan and Paul Mackerras
# Adjusted for PPC64 by Tom Gall
#

KERNELLOAD	=0xc000000000000000

ifeq ($(shell uname -m),ppc64)
CHECKS		= checks
endif

ASFLAGS		=
LINKFLAGS	= -T arch/ppc64/vmlinux.lds -Ttext $(KERNELLOAD) -Bstatic
CPPFLAGS	:= $(CPPFLAGS) -D__powerpc__ -include $(TOPDIR)/arch/ppc64/mymacros.h
CFLAGS		:= $(CFLAGS) -D__linux__ -D__powerpc__ -fsigned-char -Wa,-Saix \
		-msoft-float -pipe -Wno-uninitialized $(PRINTK) \
		-include $(TOPDIR)/arch/ppc64/mymacros.h -mminimal-toc \
		-fno-builtin
CPP		= $(CC) -E $(CFLAGS)


HEAD := arch/ppc64/kernel/head.o

ARCH_SUBDIRS = arch/ppc64/kernel arch/ppc64/mm arch/ppc64/lib
SUBDIRS := $(SUBDIRS) $(ARCH_SUBDIRS)
ARCHIVES := arch/ppc64/kernel/kernel.o arch/ppc64/mm/mm.o arch/ppc64/lib/lib.o $(ARCHIVES)
CORE_FILES := arch/ppc64/kernel/kernel.o arch/ppc64/mm/mm.o arch/ppc64/lib/lib.o $(CORE_FILES)

ifdef CONFIG_XMON
SUBDIRS += arch/ppc64/xmon
CORE_FILES += arch/ppc64/xmon/x.o
endif
ifdef CONFIG_KDB
SUBDIRS += arch/ppc64/kdb
CORE_FILES += arch/ppc64/kdb/kdba.o
endif

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

checks:
	@$(MAKE) -C arch/$(ARCH)/kernel checks

BOOT_TARGETS = zImage znetboot.initrd zImage.initrd

ifdef CONFIG_PPC_PSERIES
$(BOOT_TARGETS): $(CHECKS) vmlinux
	@$(MAKEBOOT) $@

znetboot: $(CHECKS) vmlinux
ifdef CONFIG_ALL_PPC
ifdef CONFIG_SMP
	cp -f vmlinux /tftpboot/vmlinux.smp
else
	cp -f vmlinux /tftpboot/vmlinux.smp.64
endif
endif
	@$(MAKEBOOT) $@
endif

.PHONY: clean_config
clean_config:
	rm -f .config arch/ppc64/defconfig

chrp_config: clean_config
	cp -f arch/ppc64/configs/chrp_defconfig arch/ppc64/defconfig

common_config: clean_config
	cp -f arch/ppc64/configs/common_defconfig arch/ppc64/defconfig

archclean:
	rm -f arch/ppc64/kernel/{ppc_defs.h,checks,mk_defs.s,mk_defs_out.c,mk_defs_tpl}
	@$(MAKEBOOT) clean

archmrproper:

archdep:
	$(MAKEBOOT) fastdep
