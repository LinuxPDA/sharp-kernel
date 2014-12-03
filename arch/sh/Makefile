# $Id: Makefile,v 1.6 2000/06/10 03:03:52 gniibe Exp $
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

ifdef CONFIG_CPU_LITTLE_ENDIAN
ifdef CONFIG_CPU_SH3
CROSS_COMPILE 	= sh3-linux-
else
CROSS_COMPILE 	= sh4-linux-
endif
CFLAGS		+= -ml
AFLAGS		+= -ml
# LINKFLAGS	+= -EL
LDFLAGS		:= -EL
else
ifdef CONFIG_CPU_SH3
CROSS_COMPILE 	= sh3eb-linux-
else
CROSS_COMPILE 	= sh4eb-linux-
endif
CFLAGS		+= -mb
AFLAGS		+= -mb
# LINKFLAGS	+= -EB
LDFLAGS		:= -EB
endif

ifdef CONFIG_SH_KGDB
CFLAGS :=$(CFLAGS:-fomit-frame-pointer=) -g
AFLAGS += -g
ifdef CONFIG_KGDB_MORE
CFLAGS += $(shell echo $(CONFIG_KGDB_OPTIONS) | sed -e 's/"//g')
endif
endif

ifdef CONFIG_DEBINFO_STAB
CFLAGS += -gstab
endif
ifdef CONFIG_DEBINFO_STABP
CFLAGS += -gstabs+
endif
ifdef CONFIG_DEBINFO_DWARF
CFLAGS += -gdwarf
endif
ifdef CONFIG_DEBINFO_DWARFP
CFLAGS += -gdwarf+
endif
ifdef CONFIG_DEBINFO_DWARF2
CFLAGS += -gdwarf-2
endif

LD	=$(CROSS_COMPILE)ld $(LDFLAGS)
OBJCOPY=$(CROSS_COMPILE)objcopy -O binary -R .note -R .comment -R .stab -R .stabstr -S

MODFLAGS	+=

#
#

ifdef CONFIG_CPU_SH3
CFLAGS		+= -m3
AFLAGS		+= -m3
endif
ifdef CONFIG_CPU_SH4
CFLAGS		+= -m4 -mno-implicit-fp
AFLAGS		+= -m4 -mno-implicit-fp
endif

#
# Choosing incompatible machines durings configuration will result in
# error messages during linking.  Select a default linkscript if
# none has been choosen above.
#
LINKSCRIPT    = arch/sh/vmlinux.lds
LINKFLAGS     += -T $(word 1,$(LINKSCRIPT)) -e _stext

ifdef LOADADDR
LINKFLAGS     += -Ttext $(word 1,$(LOADADDR))
endif

#
CFLAGS		+= -pipe

HEAD := arch/sh/kernel/head.o arch/sh/kernel/init_task.o

SUBDIRS := $(SUBDIRS) $(addprefix arch/sh/, kernel mm lib)
CORE_FILES := arch/sh/kernel/kernel.o arch/sh/mm/mm.o $(CORE_FILES)
LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LIBS := $(TOPDIR)/arch/sh/lib/lib.a $(LIBS) $(TOPDIR)/arch/sh/lib/lib.a \
	$(LIBGCC)

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

ifneq ($(CONFIG_SH_GENERIC)$(CONFIG_SH_STB1_HARP)$(CONFIG_SH_STB1_OVERDRIVE),)
SUBDIRS += arch/sh/stboards
CORE_FILES += arch/sh/stboards/stboards.o
endif

vmlinux: arch/sh/vmlinux.lds

arch/sh/vmlinux.lds: arch/sh/vmlinux.lds.S FORCE
	$(CPP) -traditional -C -P -I$(HPATH) -Ush arch/sh/vmlinux.lds.S >arch/sh/vmlinux.lds

FORCE: ;

ifeq ($(CONFIG_XIP_KERNEL),y)
xImage: vmlinux
ifeq ($(CONFIG_SH_7760_SOLUTION_ENGINE),y)
	$(CROSS_COMPILE)objcopy -S -O binary -j .xtext vmlinux vmlinux-text.bin
	$(CROSS_COMPILE)objcopy -S -O binary -R .xtext vmlinux vmlinux-data.bin
	cat vmlinux-text.bin vmlinux-data.bin > xImage.bin
else
	$(CROSS_COMPILE)objcopy -S -O binary -R .empty_zero_page -R .data -R .data.init_task -R .stack -R .data.init -R .data.page_aligned -R .data.cacheline_aligned -R .bss vmlinux vmlinux-text.bin
	$(CROSS_COMPILE)objcopy -S -O binary -R .bss -R .text -R .text.lock -R .rodata -R .kstrtab -R .text.init -R .initcall.init -R .setup.init -R .machvec.init -R __ex_table -R __ksymtab -R __kallsyms vmlinux vmlinux-data.bin
	cat vmlinux-text.bin vmlinux-data.bin > xImage.bin
endif
	$(CROSS_COMPILE)objcopy -I binary -O srec --adjust-vma=0x$(CONFIG_XIP_KERNEL_TEXTBASE) xImage.bin xImage.srec
	chmod -x xImage.srec
	rm -f vmlinux-text.bin
	rm -f vmlinux-data.bin
#	rm -f xImage.bin
endif

zImage: vmlinux
	@$(MAKEBOOT) zImage

compressed: zImage

zdisk: vmlinux
	@$(MAKEBOOT) zdisk

archclean:
	@$(MAKEBOOT) clean
	$(MAKE) -C arch/$(ARCH)/kernel clean
	$(MAKE) -C arch/$(ARCH)/stboards clean
#	$(MAKE) -C arch/$(ARCH)/tools clean
	rm -f xImage.bin
	rm -f xImage.srec

archmrproper:
	rm -f arch/sh/vmlinux.lds

archdep:
	@$(MAKEBOOT) dep
