#!/bin/sh

#SA1100=`egrep "CONFIG_SA11.*=y" ../../.config | sed -e '/FREQ/d;/RTC/d;/FIR/d;/VOLT/d;/DMA/d'`

#BUS=`egrep "^CONFIG_USBD_.*_BUS" ../../.config | sed /GENERIC/d`

ARCH=`./ARCH`

case "$ARCH" in
CONFIG_USBD_CDSP_BUS)
	export DEST=/tftpboot/tmp/cdsp/tmp
	export SRC=cdsp_bi
	export NAME=cdsp
	;;
CONFIG_USBD_L7205_BUS) 	
	export DEST=/tftpboot/tmp/l7205/tmp
	export SRC=l7205_bi
	export NAME=l7205
	;;
CONFIG_USBD_SL11_BUS) 		
	export DEST=/tftpboot/tmp/sl11/tmp
	export SRC=sl11_bi
	export NAME=sl1
	;;
CONFIG_SA1100_CALYPSO|CONFIG_SA1110_CALYPSO) 
	export DEST=/tftpboot/192.168.40.101/modules 
	export SRC=sa1100_bi
	export NAME=sa1100
	;;
CONFIG_SA1100_ASSABET) 
	export DEST=/tftpboot/tmp/assabet/tmp 
	export SRC=sa1100_bi
	export NAME=sa1100
	;;
CONFIG_SA1100_BITSY) 
	export DEST=/tftpboot/tmp/bitsy/tmp 
	export SRC=sa1100_bi
	export NAME=sa1100
	;;
CONFIG_SA1100_COLLIE) 
	export DEST=/tftpboot/tmp/collie/tmp 
	export SRC=sa1100_bi
	export NAME=sa1100
	;;
esac

egrep USBD ../../.config > doc/$ARCH

if [ ! -f "usbd-build.h" ] ; then
	echo "#define USBD_BUILD          \"000\"" > usbd-build.h
else
	sed < usbd-build.h '/USBD_BUILD/s/^#define.*USBD_BUILD.*"\(.*\)"/XX \1/' |
	awk '{ if ($1 == "XX") printf "#define USBD_BUILD   \"%03d\"\n", $2+1 ; else print $0 }' > usbd-build.h-new
	mv usbd-build.h-new usbd-build.h
fi

rm *.o */*.o

(cd ../../; make modules) || exit

echo ARCH: $ARCH
echo SRC: $SRC
echo DEST: $DEST


set -x
for i in usbdcore usbdmonitor usbdserial serial_fd/serial_fd net_fd/net_fd $SRC/$SRC gen_bi/gen_bi  ; do
	[ -s "$i.o" ] && cp "$i.o" "$DEST"
done

