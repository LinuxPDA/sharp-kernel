#
# Makefile for the kernel USB device drivers.
#

# Subdirs.

subdir-$(CONFIG_USBD)           += device

# The target object and module list name.

O_TARGET	:= usbdrv.o

# Objects that export symbols.

export-objs		:= hcd.o usb.o ov511.o pwc-uncompress.o

# Multipart objects.

list-multi		:= usbcore.o hid.o pwc.o
ifeq ($(CONFIG_USB_OHCI_VOYAGERGX),y)
list-multi		+= usb_ohci_voyagergx.o
endif
ifeq ($(CONFIG_USB_OHCI_VOYAGERGX),m)
list-multi		+= usb_ohci_voyagergx.o
endif
ifeq ($(CONFIG_USB_OHCI_SH7760),y)
list-multi		+= usb_ohci_sh7760.o
endif
ifeq ($(CONFIG_USB_OHCI_SH7760),m)
list-multi		+= usb_ohci_sh7760.o
endif
ifeq ($(CONFIG_USB_OHCI_TC6393),y)
list-multi		+= usb_ohci_tc6393.o
endif
ifeq ($(CONFIG_USB_OHCI_TC6393),m)
list-multi		+= usb_ohci_tc6393.o
endif

ifeq ($(CONFIG_USB_OHCI_PXA27X),y)
list-multi		+= usb-ohci-pxa27x.o
endif

usbcore-objs		:= usb.o usb-debug.o hub.o
hid-objs		:= hid-core.o
pwc-objs		:= pwc-if.o pwc-misc.o pwc-ctrl.o pwc-uncompress.o

ifeq ($(CONFIG_USB_OHCI_VOYAGERGX),y)
ohci-voyagergx-objs	:= usb-ohci.o usb_pcipool.o usb-ohci-voyagergx.o mem-onchip.o
endif
ifeq ($(CONFIG_USB_OHCI_VOYAGERGX),m)
ohci-voyagergx-objs	:= usb-ohci.o usb_pcipool.o usb-ohci-voyagergx.o mem-onchip.o
endif

ifeq ($(CONFIG_USB_OHCI_SH7760),y)
ohci-sh7760-objs	:= usb-ohci.o usb-ohci-sh.o mem-onchip.o
endif
ifeq ($(CONFIG_USB_OHCI_SH7760),m)
ohci-sh7760-objs	:= usb-ohci.o usb-ohci-sh.o mem-onchip.o
endif

ifeq ($(CONFIG_USB_OHCI_TC6393),y)
ohci-tc6393-objs	:= usb-ohci.o usb-ohci-tc6393.o pcipool.o mem-onchip.o
endif
ifeq ($(CONFIG_USB_OHCI_TC6393),m)
ohci-tc6393-objs	:= usb-ohci.o usb-ohci-tc6393.o pcipool.o mem-onchip.o
endif

ifeq ($(CONFIG_USB_OHCI_PXA27X),y)
ohci-pxa27x-objs	:= usb-ohci.o usb-ohci-pxa27x.o pcipool.o
endif
ifeq ($(CONFIG_USB_OHCI_PXA27X),m)
ohci-pxa27x-objs	:= usb-ohci.o usb-ohci-pxa27x.o pcipool.o
endif

# Optional parts of multipart objects.

ifeq ($(CONFIG_USB_DEVICEFS),y)
	usbcore-objs	+= devio.o inode.o drivers.o devices.o
endif

ifeq ($(CONFIG_USB_HIDDEV),y)
	hid-objs	+= hiddev.o
endif

ifeq ($(CONFIG_USB_HIDINPUT),y)
	hid-objs	+= hid-input.o
endif

# Object file lists.

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

# Each configuration option enables a list of files.

obj-$(CONFIG_USB)		+= usbcore.o

# EHCI should initialize/link before the other HCDs
ifeq ($(CONFIG_USB_EHCI_HCD),y)
	obj-y += hcd/ehci-hcd.o
endif

obj-$(CONFIG_USB_UHCI)		+= usb-uhci.o
obj-$(CONFIG_USB_UHCI_ALT)	+= uhci.o
obj-$(CONFIG_USB_OHCI)		+= usb-ohci.o
obj-$(CONFIG_USB_OHCI_SH7760)	+= usb_ohci_sh7760.o
obj-$(CONFIG_USB_OHCI_SH7727)	+= usb-ohci.o usb-ohci-sh.o
obj-$(CONFIG_USB_OHCI_VOYAGERGX)+= usb_ohci_voyagergx.o
obj-$(CONFIG_USB_OHCI_PXA27X)	+= usb_ohci_pxa27x.o
obj-$(CONFIG_USB_OHCI_PXA27X)	+= usb-monitor.o 

ifneq ($(CONFIG_USB_EHCI_HCD),n)
	usbcore-objs		+= hcd.o
endif
subdir-$(CONFIG_USB_EHCI_HCD)	+= hcd

obj-$(CONFIG_USB_MOUSE)		+= usbmouse.o
obj-$(CONFIG_USB_HID)		+= hid.o
obj-$(CONFIG_USB_KBD)		+= usbkbd.o
obj-$(CONFIG_USB_AIPTEK)	+= aiptek.o
obj-$(CONFIG_USB_WACOM)		+= wacom.o

obj-$(CONFIG_USB_SCANNER)	+= scanner.o
obj-$(CONFIG_USB_ACM)		+= acm.o
obj-$(CONFIG_USB_PRINTER)	+= printer.o
obj-$(CONFIG_USB_AUDIO)		+= audio.o
obj-$(CONFIG_USB_EMI26)		+= emi26.o
obj-$(CONFIG_USB_MIDI)		+= usb-midi.o
obj-$(CONFIG_USB_IBMCAM)	+= ibmcam.o usbvideo.o ultracam.o
obj-$(CONFIG_USB_PWC)		+= pwc.o
obj-$(CONFIG_USB_DC2XX)		+= dc2xx.o
obj-$(CONFIG_USB_MDC800)	+= mdc800.o
obj-$(CONFIG_USB_USS720)	+= uss720.o
obj-$(CONFIG_USB_DABUSB)	+= dabusb.o
obj-$(CONFIG_USB_VICAM)		+= vicam.o
obj-$(CONFIG_USB_OV511)		+= ov511.o
obj-$(CONFIG_USB_SE401)		+= se401.o
obj-$(CONFIG_USB_STV680)	+= stv680.o
obj-$(CONFIG_USB_PEGASUS)	+= pegasus.o
obj-$(CONFIG_USB_RTL8150)	+= rtl8150.o
obj-$(CONFIG_USB_CATC)		+= catc.o
obj-$(CONFIG_USB_KAWETH)        += kaweth.o
obj-$(CONFIG_USB_CDCETHER)	+= CDCEther.o
obj-$(CONFIG_USB_RIO500)	+= rio500.o
obj-$(CONFIG_USB_TIGL)          += tiglusb.o
obj-$(CONFIG_USB_DSBR)		+= dsbr100.o
obj-$(CONFIG_USB_MICROTEK)	+= microtek.o
obj-$(CONFIG_USB_HPUSBSCSI)	+= hpusbscsi.o
obj-$(CONFIG_USB_BLUETOOTH)	+= bluetooth.o
obj-$(CONFIG_USB_USBNET)	+= usbnet.o
obj-$(CONFIG_USB_AUERSWALD)	+= auerswald.o
obj-$(CONFIG_USB_BRLVGER)	+= brlvger.o
obj-$(CONFIG_USB_LCD)		+= usblcd.o

# Object files in subdirectories
mod-subdirs	:= serial hcd

subdir-$(CONFIG_USB_SERIAL)	+= serial
subdir-$(CONFIG_USB_STORAGE)	+= storage

ifeq ($(CONFIG_USB_SERIAL),y)
	obj-y += serial/usb-serial.o
endif

ifeq ($(CONFIG_USB_STORAGE),y)
	obj-y += storage/storage.o
endif

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

usbcore.o: $(usbcore-objs)
	$(LD) -r -o $@ $(usbcore-objs)

hid.o: $(hid-objs)
	$(LD) -r -o $@ $(hid-objs)

pwc.o: $(pwc-objs)
	$(LD) -r -o $@ $(pwc-objs)

usb_ohci_voyagergx.o: $(ohci-voyagergx-objs)
	$(LD) -r -o $@ $(ohci-voyagergx-objs)

usb_ohci_sh7760.o: $(ohci-sh7760-objs)
	$(LD) -r -o $@ $(ohci-sh7760-objs)

usb_ohci_tc6393.o: $(ohci-tc6393-objs)
	$(LD) -r -o $@ $(ohci-tc6393-objs)

usb_ohci_pxa27x.o: $(ohci-pxa27x-objs)
	$(LD) -r -o $@ $(ohci-pxa27x-objs)
