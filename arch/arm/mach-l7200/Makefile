#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE    := true

O_TARGET		:= l7200.o

obj-y			:= arch.o dma.o
obj-m			:=
obj-n			:=
obj-			:=

obj-$(CONFIG_L7205SDB)  += 
obj-$(CONFIG_IRIS)      += dbgport.o iris_led.o iris_buzzer.o l72xx_buzzer.o sysid.o iris_gsm.o iris_audioctl.o iris_charger.o

obj-$(CONFIG_APM)	+= apm.o
obj-$(CONFIG_PM)	+= power.o suspend.o

obj-$(CONFIG_L7200_USB)	 	+= # l7200_usb.o
obj-$(CONFIG_L7200_USB_NETLINK) += # usb_eth.o
obj-$(CONFIG_L7200_USB_CHAR)    += # usb_char.o

l7200_usb-objs := 

include $(TOPDIR)/Rules.make

l7200_usb.o: $(l7200_usb-objs)
	$(LD) -r -o $@ $(l7200_usb-objs)
