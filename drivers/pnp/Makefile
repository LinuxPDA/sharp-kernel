#
# Makefile for the kernel Plug-and-Play device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..

O_TARGET := pnp.o 

export-objs := isapnp.o pnp_bios.o
multi-objs := isa-pnp.o pnpbios.o

proc-$(CONFIG_PROC_FS) = isapnp_proc.o
isa-pnp-objs := isapnp.o quirks.o $(proc-y)

procpnp-$(CONFIG_PROC_FS) = pnp_proc.o
pnpbios-objs := pnp_bios.o $(procpnp-y)

obj-$(CONFIG_ISAPNP) += isa-pnp.o
obj-$(CONFIG_PNPBIOS) += pnpbios.o

include $(TOPDIR)/Rules.make

isa-pnp.o: $(isa-pnp-objs) 
	$(LD) $(LD_RFLAG) -r -o $@ $(isa-pnp-objs) 

pnpbios.o: $(pnpbios-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(pnpbios-objs)
