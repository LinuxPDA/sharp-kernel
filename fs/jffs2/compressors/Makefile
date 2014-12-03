O_TARGET	:= compressors.o

subdir-y	:= zlib_deflate
subdir-y	+= zlib_inflate

obj-y	:= $(join $(subdir-y),$(subdir-y:%=/%.o))

include $(TOPDIR)/Rules.make
