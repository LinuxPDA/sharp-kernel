#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994, 1995, 1996 by Ralf Baechle
# DECStation modifications by Paul M. Antoine, 1996
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#

#
# Select the object file format to substitute into the linker script.
#

tool-prefix	= mipsisa32-elf-

ifdef CONFIG_CPU_LITTLE_ENDIAN
# tool-prefix	= mipsel-linux-
output-format	= elf32-littlemips
CFLAGS += -EL
AFLAGS += -EL
LD += -EL
else # Big endian
output-format	= elf32-bigmips
CFLAGS += -EB
AFLAGS += -EB
LD += -EB
endif

export LD CFLAGS

#### LDFLAGS += -oformat $(output-format)

ifdef CONFIG_CROSSCOMPILE
CROSS_COMPILE  = $(tool-prefix)
endif

#
# GCC uses -G0 -mabicalls -fpic as default.  We don't want PIC in the kernel
# code since it only slows down the whole thing.  At some point we might make
# use of global pointer optimizations but their use of $28 conflicts with
# the current pointer optimization.
#
# The DECStation requires an ECOFF kernel for remote booting, other MIPS
# machines may also.  Since BFD is incredibly buggy with respect to
# crossformat linking we rely on the elf2ecoff tool for format conversion.
#
GCCFLAGS	:= -G 0 -mno-abicalls -fno-pic
LINKFLAGS	+= -static -G 0
MODFLAGS	+= -mlong-calls

ifdef CONFIG_REMOTE_DEBUG
CFLAGS		:= $(CFLAGS) -g
endif

#
# CPU-dependent compiler/assembler options for optimization. 
#
ifdef CONFIG_CPU_R3000
GCCFLAGS	+= -mcpu=r3000 -mips1
endif
ifdef CONFIG_CPU_R3912
GCCFLAGS	+= -mcpu=r3000 -mips1
endif
ifdef CONFIG_CPU_R6000
GCCFLAGS	+= -mcpu=r6000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R4300
GCCFLAGS	+= -mcpu=r4300 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R4X00
GCCFLAGS	+= -mcpu=r4600 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_MIPS32
GCCFLAGS	+= -mips32 -Wa,--trap -DNO_MM -mmad
endif
#ifdef CONFIG_CPU_4KM
#GCCFLAGS	+= -mips32 -Wa,--trap -DNO_MM -mmad
#endif
ifdef CONFIG_CPU_R5000
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R5432
GCCFLAGS        += -mcpu=r8000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_NEVADA
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap -mmad
endif
ifdef CONFIG_CPU_RM7000
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R8000
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R10000
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap
endif

ifdef CONFIG_MIPS_FPU_EMULATOR
CORE_FILES	+=arch/mipsnommu/math-emu/fpu_emulator.o
SUBDIRS		+=arch/mipsnommu/math-emu
endif

#
# The pipe options is bad for my low-mem machine
# Uncomment this if you want this.
#
GCCFLAGS	+= -pipe

CFLAGS		:= -I $(TOPDIR)/include/asm/gcc $(CFLAGS) $(GCCFLAGS) -D__linux__
AFLAGS		+= $(GCCFLAGS)

#
# Board-dependent options and extra files
#
ifdef CONFIG_ALGOR_P4032
CORE_FILES    += arch/mipsnommu/algor/algor.o
SUBDIRS       += arch/mipsnommu/algor
LOADADDR      += 0x80000000
endif

#
# DECstation family
#
ifdef CONFIG_DECSTATION
CORE_FILES    += arch/mipsnommu/dec/dec.o
SUBDIRS       += arch/mipsnommu/dec arch/mipsnommu/dec/prom
LIBS          += arch/mipsnommu/dec/prom/rexlib.a
LOADADDR      += 0x80040000
endif

ifdef CONFIG_MIPS_ATLAS
LIBS          += arch/mipsnommu/mips-boards/atlas/atlas.o arch/mipsnommu/mips-boards/generic/mipsboards.o
SUBDIRS       += arch/mipsnommu/mips-boards/generic arch/mipsnommu/mips-boards/atlas
LOADADDR      += 0x80100000
endif

ifdef CONFIG_BRECIS
LIBS          += arch/mipsnommu/brecis/msp5000/msp5000.o arch/mipsnommu/brecis/generic/brecis.o
SUBDIRS       += arch/mipsnommu/brecis/generic arch/mipsnommu/brecis/msp5000
LOADADDR      += 0x80100000
PLATFORM       = brecis/msp5000
endif

ifdef CONFIG_MIPS_MALTA
LIBS          += arch/mipsnommu/mips-boards/malta/malta.o arch/mipsnommu/mips-boards/generic/mipsboards.o
SUBDIRS       += arch/mipsnommu/mips-boards/malta arch/mipsnommu/mips-boards/generic
LOADADDR      += 0x80100000
endif

#
# Acer PICA 61, Mips Magnum 4000 and Olivetti M700.
#
ifdef CONFIG_MIPS_JAZZ
CORE_FILES    += arch/mipsnommu/jazz/jazz.o
SUBDIRS       += arch/mipsnommu/jazz arch/mipsnommu/arc
LIBS          += arch/mipsnommu/arc/arclib.a
LOADADDR      += 0x80080000
endif

ifdef CONFIG_COBALT_MICRO_SERVER
ARCHIVES      += arch/mipsnommu/cobalt/cobalt.o
SUBDIRS       += arch/mipsnommu/cobalt
LOADADDR      += 0x80000000
endif

ifdef CONFIG_SNI_RM200_PCI
CORE_FILES    += arch/mipsnommu/sni/sni.o
SUBDIRS       += arch/mipsnommu/sni arch/mipsnommu/arc
LIBS          += arch/mipsnommu/arc/arclib.a
LOADADDR      += 0x80080000
endif

ifdef CONFIG_SGI_IP22
CORE_FILES    += arch/mipsnommu/sgi/kernel/ip22-kern.o
LIBS          += arch/mipsnommu/arc/arclib.a
SUBDIRS       += arch/mipsnommu/sgi/kernel arch/mipsnommu/arc
#
# Set LOADADDR to >= 0x88069000 if you want to leave space for symmon,
# 0x88002000 for production kernels.  Note that the value must be
# 8kb aligned or the handling of the current variable will break.
#
LOADADDR      += 0x88002000
endif

#
# Baget/MIPS
#
ifdef CONFIG_BAGET_MIPS
SUBDIRS       += arch/mipsnommu/baget arch/mipsnommu/baget/prom
LIBS          += arch/mipsnommu/baget/baget.a arch/mipsnommu/baget/prom/bagetlib.a
LOADADDR      += 0x80001000
endif

#
# NEC DDB Vrc-5074
#
ifdef CONFIG_DDB5074
SUBDIRS       += arch/mipsnommu/ddb5074
LIBS          += arch/mipsnommu/ddb5074/ddb5074.a
LOADADDR      += 0x80080000
endif

#
#
# NEC DDB Vrc-5476
#
ifdef CONFIG_DDB5476
SUBDIRS       += arch/mipsnommu/ddb5476
LIBS          += arch/mipsnommu/ddb5476/ddb5476.a
LOADADDR      += 0x80080000
endif

#
# Galileo EV64120 Board
#
ifdef CONFIG_MIPS_EV64120
LIBS          += arch/mipsnommu/galileo-boards/ev64120/ev64120.o
SUBDIRS       += arch/mipsnommu/galileo-boards/ev64120
LOADADDR      += 0x80100000
endif

#
# Galileo EV96100 Board
#
ifdef CONFIG_MIPS_EV96100
LIBS          += arch/mipsnommu/galileo-boards/ev96100/ev96100.o arch/mipsnommu/galileo-boards/generic/galboards.o
SUBDIRS       += arch/mipsnommu/galileo-boards/generic arch/mipsnommu/galileo-boards/ev96100
LOADADDR      += 0x80100000
endif

#
# Momentum Ocelot board
#
ifdef CONFIG_MOMENCO_OCELOT
LIBS          += arch/mipsnommu/gt64120/common/gt64120.o arch/mipsnommu/gt64120/momenco_ocelot/momenco_ocelot.o
SUBDIRS       += arch/mipsnommu/gt64120/common arch/mipsnommu/gt64120/momenco_ocelot
LOADADDR      += 0x80100000
endif

#
# Philips Nino
#
ifdef CONFIG_NINO
CORE_FILES    += arch/mipsnommu/philips/nino/nino.o \
		 arch/mipsnommu/philips/drivers/drivers.o
SUBDIRS       += arch/mipsnommu/philips/nino arch/mipsnommu/philips/drivers
LOADADDR      += 0x80000000
endif

#
# ITE 8172 eval board with QED 5231 CPU
#
ifdef CONFIG_MIPS_ITE8172
LIBS		+= arch/mipsnommu/ite-boards/qed-4n-s01b/ite.o		     \
		   arch/mipsnommu/ite-boards/generic/it8172.o
SUBDIRS		+= arch/mipsnommu/ite-boards/generic				     \
		   arch/mipsnommu/ite-boards/qed-4n-s01b
LOADADDR	+= 0x80100000
endif

#
# Choosing incompatible machines durings configuration will result in
# error messages during linking.  Select a default linkscript if
# none has been choosen above.
#
vmlinux: arch/$(ARCH)/ld.script

linux: arch/$(ARCH)/ld.script

arch/$(ARCH)/ld.script: arch/$(ARCH)/ld.script.in arch/$(ARCH)/Makefile
	sed -e 's/@@OUTPUT_FORMAT@@/$(output-format)/' \
	    -e 's/@@LOADADDR@@/$(LOADADDR)/'           \
	    -e 's|@@PLATFORM@@|$(PLATFORM)|' <$< >$@
LINKFLAGS	+= -T arch/$(ARCH)/ld.script



# flat binary image of the style used by Lineo uCbootloader:
#
# linux.bin: linux
# 	$(CROSS_COMPILE)objcopy -O binary linux linux.bin
#
# image.bin: romdisk linux.bin
#	cat linux.bin romdisk.img > image.bin



# FIXME: This 'cut' statement makes me queasy: too dependent on the exact same format
#        of objdump output (mleslie@lineo.com)

image.elf: romdisk linux
	@ADDR=0x`$(CROSS_COMPILE)objdump -h linux | grep \.fs | cut -d' ' -f17` ; \
	$(CROSS_COMPILE)objcopy --add-section=.romfs=romdisk.img \
	  --adjust-section-vma=.romfs=$$ADDR --no-adjust-warnings \
	  --set-section-flags=.romfs=alloc,load,readonly,data  linux image.elf 2> /dev/null

romdisk:	romdisk.img
	@cp arch/$(ARCH)/$(PLATFORM)/romfs.ld \
	  arch/$(ARCH)/$(PLATFORM)/romfs.default.ld
	@echo "romfs_size = `cat romdisk.img | wc -c`;" \
	  > arch/$(ARCH)/$(PLATFORM)/romfs.ld
	@echo Creating a linux rom image with root filesystem!





HEAD := arch/mipsnommu/kernel/head.o arch/mipsnommu/kernel/init_task.o

SUBDIRS := $(addprefix arch/mipsnommu/, tools) $(SUBDIRS) $(addprefix arch/mipsnommu/, kernel mm lib)
CORE_FILES := arch/mipsnommu/kernel/kernel.o arch/mipsnommu/mm/mm.o $(CORE_FILES)
LIBS := arch/mipsnommu/lib/lib.a $(LIBS)

ifdef CONFIG_BAGET_MIPS

BAGETBOOT = $(MAKE) -C arch/$(ARCH)/baget

balo:  vmlinux
	$(BAGETBOOT) balo

endif

ifdef CONFIG_MIPS_EV64120
GALILEOBOOT = $(MAKE) -C arch/$(ARCH)/galileo-boards/ev64120

gboot: vmlinux
	$(MAKE) -C arch/$(ARCH)/galileo-boards/ev64120/compressed 
endif

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

vmlinux.ecoff: vmlinux
	@$(MAKEBOOT) $@

zImage: vmlinux
	@$(MAKEBOOT) zImage

compressed: zImage

zdisk: vmlinux
	@$(MAKEBOOT) zdisk

archclean:
	@$(MAKEBOOT) clean
	rm -f arch/$(ARCH)/ld.script
	$(MAKE) -C arch/$(ARCH)/tools clean
#	$(MAKE) -C arch/mipsnommu/baget clean

archmrproper:
	@$(MAKEBOOT) mrproper
	$(MAKE) -C arch/$(ARCH)/tools mrproper

archdep:
	if [ ! -f $(TOPDIR)/include/asm-$(ARCH)/offset.h ]; then \
	 touch $(TOPDIR)/include/asm-$(ARCH)/offset.h; \
	fi;
	@$(MAKEBOOT) dep
