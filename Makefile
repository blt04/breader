

CC=gcc
SBINDIR?=/usr/sbin

default: breaderd

breaderd: breaderd.c
	$(CC) $(CFLAGS) breaderd.c -o breaderd

install: breaderd
	mkdir -p $(DESTDIR)$(SBINDIR)
	cp breaderd $(DESTDIR)$(SBINDIR)/breaderd

uninstall:
	rm $(DESTDIR)$(SBINDIR)/breaderd


clean:
	rm -rf breaderd
