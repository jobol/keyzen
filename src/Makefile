.PHONY: all clean

all: keyzen-fs libkeyzen.a libkeyzen-dbus.a

clean:
	$(RM) *.o *.a keyzen-fs

SRCFS = procs.c keyset.c genname.c

INCFS = procs.h keyset.h genname.h

OPTFS = $(shell pkg-config --cflags fuse) $(shell pkg-config --libs fuse)

OPTFS += -O3 -Wall 

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




