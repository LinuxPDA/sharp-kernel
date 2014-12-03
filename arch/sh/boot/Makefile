#
# arch/sh/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1999 Stuart Menefy
#

SYSTEM  =$(TOPDIR)/vmlinux

ZIMAGE_OFFSET = $(shell printf "0x%8x" $$[0x80000000+0x$(CONFIG_MEMORY_START)+0x200000+0x10000])

Image:	$(CONFIGURE) $(SYSTEM)
	$(OBJCOPY) $(SYSTEM) Image

zImage: $(CONFIGURE) compressed/vmlinux
	$(OBJCOPY) compressed/vmlinux zImage
	$(OBJCOPY) -I binary -O srec --adjust-vma=$(ZIMAGE_OFFSET) zImage zImage.srec

compressed/vmlinux: $(TOPDIR)/vmlinux
	$(MAKE) -C compressed vmlinux

install: $(CONFIGURE) Image
	sh -x ./install.sh $(KERNELRELEASE) Image $(TOPDIR)/System.map "$(INSTALL_PATH)"

zinstall: $(CONFIGURE) zImage
	sh -x ./install.sh $(KERNELRELEASE) zImage $(TOPDIR)/System.map "$(INSTALL_PATH)"

dep:

clean:
	rm -f tools/build
	rm -f setup bootsect zImage zImage.srec compressed/vmlinux.out
	rm -f bsetup bbootsect bzImage compressed/bvmlinux.out
	@$(MAKE) -C compressed clean
