#
# linux/drivers/devices/Makefile
#
# $Id: Makefile,v 1.1.1.1 2001/04/27 00:30:06 yata Exp $

O_TARGET	:= devlink.o

#                       *** BIG UGLY NOTE ***
#
# The removal of get_module_symbol() and replacement with
# inter_module_register() et al has introduced a link order dependency
# here where previously there was none.  We now have to ensure that
# doc200[01].o are linked before docprobe.o

obj-$(CONFIG_MTD_DOC1000)	+= doc1000.o
obj-$(CONFIG_MTD_DOC2000)	+= doc2000.o
obj-$(CONFIG_MTD_DOC2001)	+= doc2001.o
obj-$(CONFIG_MTD_DOCPROBE)	+= docprobe.o docecc.o
obj-$(CONFIG_MTD_SLRAM)		+= slram.o
obj-$(CONFIG_MTD_PMC551)	+= pmc551.o
obj-$(CONFIG_MTD_MTDRAM)	+= mtdram.o
obj-$(CONFIG_MTD_MTDROM_SA1100) += sa1100_mtdrom.o
obj-$(CONFIG_MTD_MTDRAM_SA1100) += sa1100_mtdram.o
obj-$(CONFIG_MTD_MTDRAM_L7200)  += l7200_mtdram.o
obj-$(CONFIG_MTD_MTDROM_L7200)  += l7200_mtdrom.o

include $(TOPDIR)/Rules.make
