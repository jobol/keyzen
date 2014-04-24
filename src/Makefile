.PHONY: all clean

all: keyzen-fs keyzen libkeyzen.a libkeyzen-dbus.a

clean:
	$(RM) *.o *.a keyzen-fs

SRCFS = procs.c keyset.c

INCFS = procs.h keyset.h itoa.c

OPTFS = $(shell pkg-config --cflags fuse) $(shell pkg-config --libs fuse)

OPTFS += -O3 -Wall 

OPTFS += -DNOSECURITY=1

keyzen-fs: keyzen-fs.c $(SRCFS) $(INCFS)
	gcc $(OPTFS) -o $@ $< $(SRCFS)
	sudo /sbin/setcap all=+pe-i $@

CFLAGS += -O3 -Wall

libkeyzen.a: keyzen.o
	$(AR) rcs $@ $^

keyzen-dbus.o: keyzen-dbus.c
	$(CC) $(CFLAGS) $(shell pkg-config --cflags dbus-1) -c -o $@ $<

libkeyzen-dbus.a: keyzen-dbus.o
	$(AR) rcs $@ $^

keyzen: keyzen-tool.c
	gcc $(OPTFS) -o $@ $< $(SRCFS)


