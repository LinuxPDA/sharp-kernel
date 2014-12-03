
TARGET = $(TOPDIR)/include/asm-x86_64/offset.h

all: $(TARGET)

.PHONY: all

$(TARGET): offset.h
	cmp -s $^ $@ || (cp $^ $(TARGET).new && mv $(TARGET).new $(TARGET))

.PHONY : offset.h

offset.h: 
	$(CC) $(CFLAGS) -S -o offset.tmp offset.c
	sed -n -f offset.sed < offset.tmp > offset.h   

clean:
	rm -f offset.[hs] $(TARGET).new 

mrproper:	
	rm -f offset.[hs] $(TARGET)
	rm -f $(TARGET)

include $(TOPDIR)/Rules.make

