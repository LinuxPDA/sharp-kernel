OBJ = um.o

OBJS = process.o current.o exec_kern.o exec_user.o init_task.o irq.o \
	irq_user.o mem.o mem_user.o ptrace.o reboot.o resource.o \
	setup.o signal_user.o smp.o syscall_kern.o syscall_user.o \
	sysrq.o sys_call_table.o time.o time_kern.o tlb.o trap_kern.o \
	trap_user.o uaccess_user.o um_arch.o umid.o user_util.o

OX_OBJS = ksyms.o process_kern.o signal_kern.o user_syms.o

UNMAP_CFLAGS := $(patsubst -pg -DPROFILING,,$(USER_CFLAGS))
UNMAP_CFLAGS := $(patsubst -fprofile-arcs -ftest-coverage,,$(UNMAP_CFLAGS))

ifeq ($(CONFIG_MODULES), y)
  DMODULES = -D__CONFIG_MODULES__
endif

ifeq ($(CONFIG_MODVERSIONS), y)
  DMODVERSIONS = -D__CONFIG_MODVERSIONS__
endif

CFLAGS_user_syms.o = -D__AUTOCONF_INCLUDED__ $(DMODULES) $(DMODVERSIONS) -I- \
	-I../include

all: $(OBJ) unmap_fin.o

exec_user.o: exec_user.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

irq_user.o: irq_user.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

mem_user.o: mem_user.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

process.o: process.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

signal_user.o: signal_user.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

syscall_user.o: syscall_user.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

time.o: time.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

trap_user.o: trap_user.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

uaccess_user.o: uaccess_user.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

umid.o: umid.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

user_util.o: user_util.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<
 
unmap.o: unmap.c
	echo $(UNMAP_CFLAGS)
	$(CC) $(UNMAP_CFLAGS) -c -o $@ $<

unmap_fin.o : unmap.o
	ld -r -o $@ $< -lc -L/usr/lib

$(OBJ): $(OBJS) $(OX_OBJS)
	rm -f $@
	$(LD) $(LINKFLAGS) --start-group $^ --end-group -o $@

clean:
	rm -f $(OBJS) $(OX_OBJS)

modules:

fastdep:

include $(TOPDIR)/Rules.make
