CC      := gcc
PREFIX  ?= /usr
CFLAGS  ?= -O3 -pipe
#CFLAGS  += -finline-functions
LDFLAGS ?= -Wl,-s
PSRC     = src

.PHONY: all bsd install clean

all:	sdelta

sdelta: $(PSRC)/*.c $(PSRC)/*.h
	$(CC) $(CFLAGS) $(LDFLAGS) $(PSRC)/*.c -lcrypto -o sdelta

clean:
	rm -f sdelta

bsd:  $(PSRC)/*.c $(PSRC)/*.h
	$(CC) $(CFLAGS) -DUSE_LIBMD $(PSRC)/*.c $(LDFLAGS) -ldlmalloc /usr/lib/libmd.a -o sdelta

install: sdelta
	mkdir   -m 0755  -p                     $(PREFIX)/bin
	install -m 0755  sdelta   sdeltac       $(PREFIX)/bin
	mkdir   -m 0755  -p                     $(PREFIX)/share/doc/sdelta
	install -m 0644  LICENSE  USAGE         $(PREFIX)/share/doc/sdelta
	install -m 0644  README   README.1st    $(PREFIX)/share/doc/sdelta
	install -m 0644  sdelta.magic           $(PREFIX)/share/doc/sdelta/magic
