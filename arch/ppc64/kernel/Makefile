#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

USE_STANDARD_AS_RULE    := true

EXTRA_CFLAGS = -mno-minimal-toc

KHEAD := head.o

all: $(KHEAD) kernel.o

O_TARGET := kernel.o

export-objs         := ppc_ksyms.o setup.o

obj-y               :=	ppc_ksyms.o setup.o entry.o traps.o irq.o idle.o \
			time.o process.o signal.o syscalls.o misc.o ptrace.o \
			align.o semaphore.o bitops.o stab.o htab.o pacaData.o \
			LparData.o udbg.o binfmt_elf32.o sys_ppc32.o sys32.o \
			ioctl32.o ptrace32.o signal32.o open_pic.o xics.o \
			pmc.o mf_proc.o proc_pmc.o proc_pcifr.o iSeries_setup.o \
			ItLpQueue.o hvCall.o mf.o viopath.o HvLpEvent.o \
			iSeries_proc.o HvCall.o flight_recorder.o HvLpConfig.o 

obj-$(CONFIG_PCI) +=  pci.o

ifeq ($(CONFIG_PPC_ISERIES),y)
obj-$(CONFIG_PCI) += iSeries_dma.o iSeries_rtc.o 
else
obj-$(CONFIG_PCI) += pci_dma.o  
endif

obj-$(CONFIG_KGDB) += ppc-stub.o

obj-$(CONFIG_SMP) += smp.o

# tibit: for matrox_init2()
ifeq ($(CONFIG_NVRAM),y)
   obj-$(CONFIG_NVRAM) += pmac_nvram.o
endif

ifeq ($(CONFIG_ALL_PPC),y)
  obj-y += prom.o lmb.o rtas.o rtas-eventscan.o rtas-proc.o chrp_setup.o \
		chrp_time.o pSeries_pci.o i8259.o
endif

include $(TOPDIR)/Rules.make

#
# This is just to get the dependencies...
#

head.o: head.S ppc_defs.h

ppc_defs.h: mk_defs.c ppc_defs.head \
		$(TOPDIR)/include/asm/mmu.h \
		$(TOPDIR)/include/asm/processor.h \
		$(TOPDIR)/include/asm/pgtable.h \
		$(TOPDIR)/include/asm/ptrace.h
	$(CC) $(CFLAGS) -S mk_defs.c
	cp ppc_defs.head ppc_defs.h
# for bk, this way we can write to the file even if it's not checked out
	chmod u+w ppc_defs.h
	grep '^#define' mk_defs.s >> ppc_defs.h
	rm mk_defs.s

checks: checks.c
	$(HOSTCC) -I$(HPATH) $(HOSTCFLAGS) -D__KERNEL__ -fno-builtin -o checks checks.c
	./checks
