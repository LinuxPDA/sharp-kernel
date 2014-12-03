#
# linux/drivers/nand/Makefile
#
# $Id: Makefile,v 1.7 2002/04/30 09:34:16 mag Exp $

O_TARGET	:= nandlink.o

export-objs	:= nand.o nand_ecc.o

nandobjs-y			:= nand.o
nandobjs-$(CONFIG_MTD_NAND_ECC) += nand_ecc.o

#subdir-$(CONFIG_MTD_NAND_SHARP_SL)	:= sharp_sl

obj-$(CONFIG_MTD_NAND)		+= $(nandobjs-y)
obj-$(CONFIG_MTD_NAND_SPIA)	+= spia.o
obj-$(CONFIG_MTD_NAND_AUTCPU12)	+= autcpu12.o
obj-$(CONFIG_MTD_NAND_EDB7312)  += edb7312.o
obj-$(CONFIG_MTD_NAND_SHARP_SL)	+= sharp_sl.o
obj-$(CONFIG_MTD_NAND_SHARP_SL_TC6393)	+= sharp_sl_tc6393.o
obj-$(CONFIG_MTD_NAND_LOGICAL_ADDRESS_ACCESS)	+= sharp_sl_logical.o

include $(TOPDIR)/Rules.make
