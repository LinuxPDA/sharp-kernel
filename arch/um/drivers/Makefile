# 
# Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
# Licensed under the GPL
#

O_TARGET := um_drivers.o

obj-y = 
obj-m =

CFLAGS_stdio_console.o := $(CFLAGS)
CFLAGS_stdio_console_user.o := $(USER_CFLAGS)
CFLAGS_ssl.o := $(CFLAGS)
CFLAGS_chan_kern.o := $(CFLAGS)
CFLAGS_chan_user.o := $(USER_CFLAGS)
CFLAGS_ubd.o := $(CFLAGS)
CFLAGS_ubd_user.o := $(USER_CFLAGS)
CFLAGS_slip_kern.o := $(CFLAGS)
CFLAGS_slip_user.o := $(USER_CFLAGS)
CFLAGS_ethertap_kern.o := $(CFLAGS)
CFLAGS_ethertap_user.o := $(USER_CFLAGS)
CFLAGS_tuntap_kern.o := $(CFLAGS)
CFLAGS_tuntap_user.o := $(USER_CFLAGS)
CFLAGS_daemon_kern.o := $(CFLAGS)
CFLAGS_daemon_user.o := $(USER_CFLAGS)
CFLAGS_net_kern.o := $(CFLAGS)
CFLAGS_net_user.o := $(USER_CFLAGS)
CFLAGS_mcast_user.o := $(USER_CFLAGS)
CFLAGS_mcast_kern.o := $(CFLAGS)
CFLAGS_mconsole_kern.o := $(CFLAGS)
CFLAGS_mconsole_user.o := $(USER_CFLAGS)
CFLAGS_mmapper_kern.o := $(CFLAGS)

obj-$(CONFIG_STDIO_CONSOLE) += stdio_console.o stdio_console_user.o
obj-$(CONFIG_SSL) += ssl.o
obj-$(CONFIG_SSL) += chan_user.o chan_kern.o
obj-$(CONFIG_STDIO_CONSOLE) += chan_user.o chan_kern.o
obj-$(CONFIG_BLK_DEV_UBD) += ubd.o ubd_user.o
obj-$(CONFIG_UML_NET) += net_kern.o net_user.o 
obj-$(CONFIG_UML_NET_SLIP) += slip_user.o slip_kern.o
obj-$(CONFIG_UML_NET_ETHERTAP) += ethertap_user.o ethertap_kern.o 
obj-$(CONFIG_UML_NET_TUNTAP) += tuntap_user.o tuntap_kern.o 
obj-$(CONFIG_UML_NET_DAEMON) += daemon_kern.o daemon_user.o
obj-$(CONFIG_UML_NET_MCAST) += mcast_user.o mcast_kern.o
obj-$(CONFIG_MCONSOLE) += mconsole_kern.o mconsole_user.o
obj-$(CONFIG_MMAPPER) += mmapper_kern.o

override CFLAGS =

include $(TOPDIR)/Rules.make
