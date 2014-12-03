#
# Makefile for the Linux/SuperH kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

all: kernel.o head.o init_task.o

clean:

O_TARGET := kernel.o

export-objs	:= io.o io_generic.o io_hd64461.o setup_hd64461.o io_bigsur.o \
		   rthal.o sh_ksyms.o 

ifeq ($(CONFIG_SH_MS7710SE),y)
export-objs	+=	setup.o
endif

ifeq ($(CONFIG_SH_RTS7751R2D),y)
export-objs    += io_rts7751r2d.o
endif

obj-y	:= process.o signal.o entry.o traps.o irq.o irq_ipr.o \
	ptrace.o setup.o time.o sys_sh.o semaphore.o \
	irq_imask.o io.o io_generic.o sh_ksyms.o

obj-$(CONFIG_CF_ENABLER)	+= cf-enabler.o
obj-$(CONFIG_CPU_SH4)		+= fpu.o
obj-$(CONFIG_SH_RTC)            += rtc.o
obj-$(CONFIG_SH_DMA)		+= dma.o
obj-$(CONFIG_SH_STANDARD_BIOS)	+= sh_bios.o
obj-$(CONFIG_SH_KGDB)		+= kgdb_stub.o kgdb_jmp.o

ifeq ($(CONFIG_PCI),y)
ifeq ($(CONFIG_SH_DREAMCAST),y)
obj-y 				+= pci-dc.o pcibios.o
else
obj-y                           += pci-dma.o pcibios.o
obj-$(CONFIG_CPU_SUBTYPE_ST40STB1)+= pci_st40.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751)+= pci-sh7751.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751R)+= pci-sh7751.o
obj-$(CONFIG_SH_BIGSUR)+= pci-bigsur.o
obj-$(CONFIG_SH_7751_SOLUTION_ENGINE)+= pci-7751se.o pci-sh7751_autoconfig.o
obj-$(CONFIG_SH_RTS7751R2D)+= pci-sh7751.o pci-rts7751r2d.o
endif
endif
ifeq ($(CONFIG_PCIPOOL),y)
obj-y 				+= pci-dma.o pcipool.o
endif

obj-$(CONFIG_PM)		+= 
obj-$(CONFIG_APM)		+= apm.o

obj-$(CONFIG_SH_HP600)		+= mach_hp600.o
machine-specific-objs		+= mach_hp600.o

obj-$(CONFIG_SH_SOLUTION_ENGINE)+= mach_se.o setup_se.o io_se.o led_se.o
machine-specific-objs		+= mach_se.o setup_se.o io_se.o led_se.o

obj-$(CONFIG_SH_7751_SOLUTION_ENGINE)+= mach_7751se.o setup_7751se.o \
					io_7751se.o led_7751se.o
machine-specific-objs           += mach_7751se.o 7751setup_se.o \
				   io_7751se.o led_7751se.o pci-7751se.o

obj-$(CONFIG_SH_7760_SOLUTION_ENGINE)+= mach_ms7760cp.o setup_ms7760cp.o \
                                        io_ms7760cp.o rtc-ms7760cp.o
machine-specific-objs                += mach_ms7760cp.o setup_ms7760cp.o \
                                        io_ms7760cp.o rtc-ms7760cp.o

obj-$(CONFIG_SH_BIGSUR)		+= mach_bigsur.o setup_bigsur.o io_bigsur.o led_bigsur.o
machine-specific-objs		+= mach_bigsur.o setup_bigsur.o io_bigsur.o led_bigsur.o

obj-$(CONFIG_SH_SH2000)		+= setup_sh2000.o io_sh2000.o
machine-specific-objs		+= setup_sh2000.o io_sh2000.o

obj-$(CONFIG_SH_CAT68701) 	+= mach_cat68701.o io_cat68701.o
machine-specific-objs		+= mach_cat68701.o io_cat68701.o

obj-$(CONFIG_SH_CQREEK)		+= setup_cqreek.o
machine-specific-objs		+= setup_cqreek.o

obj-$(CONFIG_SH_UNKNOWN)	+= mach_unknown.o io_unknown.o
machine-specific-objs		+= mach_unknown.o io_unknown.o

obj-$(CONFIG_HD64461)		+= setup_hd64461.o io_hd64461.o
machine-specific-objs		+= setup_hd64461.o io_hd64461.o

obj-$(CONFIG_CPU_SUBTYPE_ST40STB1) +=irq_intc2.o

obj-$(CONFIG_CPU_SUBTYPE_SH7760) +=irq_intc2-sh7760.o

obj-$(CONFIG_SH_ADX)		+= mach_adx.o setup_adx.o io_adx.o irq_maskreg.o
machine-specific-objs		+= mach_adx.o setup_adx.o io_adx.o irq_maskreg.o

obj-$(CONFIG_SH_MS7727RP)	+= mach_ms7727rp.o setup_ms7727rp.o io_ms7727rp.o
machine-specific-objs		+= mach_ms7727rp.o setup_ms7727rp.o io_ms7727rp.o

obj-$(CONFIG_CPU_SUBTYPE_SH7727) += sh7727_setup.o
obj-$(CONFIG_SH7727_ADC)         += sh7727_adc.o
obj-$(CONFIG_SH7727_DMA)         += sh7727_dma.o

obj-$(CONFIG_SH7720_DMA)         += sh7720_dma.o

obj-$(CONFIG_SH_MS7290CP)	+= mach_ms7290cp.o setup_ms7290cp.o io_ms7290cp.o
machine-specific-objs		+= mach_ms7290cp.o setup_ms7290cp.o io_ms7290cp.o

obj-$(CONFIG_SH_MS7710SE)	+= mach_ms7710se.o setup_ms7710se.o io_ms7710se.o led_se.o
machine-specific-objs		+= mach_ms7710se.o setup_ms7710se.o io_ms7710se.o led_se.o

obj-$(CONFIG_SH_MS7720RP)	+= mach_ms7720rp.o setup_ms7720rp.o io_ms7720rp.o
machine-specific-objs		+= mach_ms7720rp.o setup_ms7720rp.o io_ms7720rp.o

obj-$(CONFIG_SH_RTS7751R2D)	+= mach_rts7751r2d.o setup_rts7751r2d.o io_rts7751r2d.o
machine-specific-objs		+= mach_rts7751r2d.o setup_rts7751r2d.o io_rts7751r2d.o

obj-$(CONFIG_CPU_SUBTYPE_SH7290) += sh7290_setup.o
obj-$(CONFIG_CPU_SUBTYPE_SH7720) += sh7720_setup.o

# Doesn't compile well, so don't include in machine-specific-objs
obj-$(CONFIG_HD64465)		+= setup_hd64465.o io_hd64465.o hd64465_gpio.o
obj-$(CONFIG_SH_DMIDA)		+= mach_dmida.o
obj-$(CONFIG_SH_EC3104)         += setup_ec3104.o io_ec3104.o mach_ec3104.o
obj-$(CONFIG_SH_DREAMCAST)	+= mach_dc.o setup_dc.o io_dc.o rtc-aica.o
obj-$(CONFIG_VOYAGERGX)		+= setup_voyagergx.o

obj-$(CONFIG_APM)		+= apm.o
ifeq ($(CONFIG_SH_GENERIC),y)
obj-y		+= $(machine-specific-objs)
endif

obj-$(CONFIG_RTHAL)		+= rthal.o

USE_STANDARD_AS_RULE := true

ifeq ($(CONFIG_SH_DSP),y)
entry.o: entry.S
	$(CPP) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) -E -o entry.otmp_ $< ; $(AS) -dsp -o entry.o entry.otmp_;rm entry.otmp_
endif

include $(TOPDIR)/Rules.make
