include arch/$(ARCH)/Makefile-$(SUBARCH)

include/linux/version.h: arch/$(ARCH)/Makefile

ARCH_DIR = $(TOPDIR)/arch/um

MAKEBOOT = $(MAKE) -C $(ARCH_DIR)/boot

ifeq ($(CONFIG_DEBUGSYM),y)
DEBUG = -g
CFLAGS := $(subst -fomit-frame-pointer,,$(CFLAGS))
endif

ifeq ($(CONFIG_GCOV),y)
CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifeq ($(CONFIG_GPROF), y)
PROFILE += -pg -DPROFILING
LINK_PROFILE = $(PROFILE) -Wl,--wrap,__monstartup
endif

SUBDIRS += $(ARCH_DIR)/fs $(ARCH_DIR)/drivers $(ARCH_DIR)/kernel \
	$(ARCH_DIR)/sys-$(SUBARCH)

LIBS += $(shell [ -e $(ARCH_DIR)/fs/fs.o ] && echo $(ARCH_DIR)/fs/fs.o) \
	$(ARCH_DIR)/kernel/um.o $(ARCH_DIR)/drivers/um_drivers.o \
	$(ARCH_DIR)/sys-$(SUBARCH)/sys.o 

ifeq ($(CONFIG_PT_PROXY), y)
SUBDIRS += $(ARCH_DIR)/ptproxy
LIBS += $(ARCH_DIR)/ptproxy/ptproxy.a
endif

NESTING = 0

ARCH_INCLUDE = $(ARCH_DIR)/include

CFLAGS += $(DEBUG) $(PROFILE) $(ARCH_CFLAGS) -D__arch_um__ \
	-DSUBARCH=\"$(SUBARCH)\" -DNESTING=$(NESTING) -D_LARGEFILE64_SOURCE \
	-I$(ARCH_INCLUDE) -Derrno=kernel_errno

LINKFLAGS += -r

LINK_WRAPS = -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,calloc

$(ARCH_DIR)/link.ld:
	m4 -DSTART=$(START_ADDR) -DSUBARCH=$(SUBARCH) \
		-DELF_SUBARCH=$(ELF_SUBARCH) \
		$(ARCH_DIR)/link.ld.in > $(ARCH_DIR)/link.ld

SYMLINK_HEADERS = include/asm-um/archparam.h include/asm-um/system.h \
	include/asm-um/sigcontext.h include/asm-um/processor.h

ARCH_SYMLINKS = include/asm-um/arch arch/um/include/sysdep $(SYMLINK_HEADERS)

linux: $(ARCH_SYMLINKS) $(ARCH_DIR)/main.o vmlinux $(ARCH_DIR)/link.ld
	mv vmlinux vmlinux.o
	$(CC) -Wl,-T,$(ARCH_DIR)/link.ld $(LINK_PROFILE) $(LINK_WRAPS) \
		-o linux -static $(ARCH_DIR)/main.o vmlinux.o -L/usr/lib
	rm -f $(ARCH_DIR)/link.ld

USER_CFLAGS := $(patsubst -I%,,$(CFLAGS))
USER_CFLAGS := $(patsubst -Derrno=kernel_errno,,$(USER_CFLAGS))
USER_CFLAGS := $(patsubst -D__KERNEL__,,$(USER_CFLAGS)) -I$(ARCH_INCLUDE)

$(ARCH_DIR)/main.o: $(ARCH_DIR)/main.c
	$(CC) -D__KERNEL__ $(USER_CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

archmrproper:
	$(MAKE) -C $(ARCH_DIR)/sys-$(SUBARCH) archmrproper
	rm -f $(SYMLINK_HEADERS) $(ARCH_SYMLINKS) include/asm \
		$(addprefix $(ARCH_DIR)/kernel/,$(KERN_SYMLINKS))

archclean:
	find . \( -name '*.bb' -o -name '*.bbg' -o -name '*.da' \
		-o -name '*.gcov' \) -type f -print | xargs rm -f
	rm -f linux x.i gmon.out link.ld
	@$(MAKEBOOT) clean

archdep: $(ARCH_SYMLINKS)
	@$(MAKEBOOT) dep

$(SYMLINK_HEADERS):
	echo $@
	cd $(TOPDIR)/$(dir $@) ; \
	ln -sf $(basename $(notdir $@))-$(SUBARCH)$(suffix $@) $(notdir $@)

include/asm-um/arch:
	cd $(TOPDIR)/include/asm-um && ln -sf ../asm-$(SUBARCH) arch

arch/um/include/sysdep:
	cd $(TOPDIR)/arch/um/include && ln -sf sysdep-$(SUBARCH) sysdep

export SUBARCH USER_CFLAGS
