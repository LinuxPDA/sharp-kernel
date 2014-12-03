LIB = ptproxy.a

OBJS = proxy.o ptrace.o sysdep.o wait.o

USER_CFLAGS := $(patsubst -U$(SUBARCH),,$(USER_CFLAGS))

all: $(LIB)

$(LIB): $(OBJS) $(OX_OBJS)
	rm -f $@
	ar cr $@ $^

proxy.o: proxy.c
	$(CC) $(USER_CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

ptrace.o: ptrace.c
	$(CC) $(USER_CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

sysdep.o: sysdep.c
	$(CC) $(USER_CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

wait.o: wait.c
	$(CC) $(USER_CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

clean:
	rm -f *.o core child ptproxy

include $(TOPDIR)/Rules.make
