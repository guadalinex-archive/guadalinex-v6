CC=gcc
CFLAGS=-g -Wall -fPIC
LDFLAGS=
INCLUDES=

COMPILE = $(CC) $(INCLUDES) $(CFLAGS)
LINK = $(CC) $(CFLAGS) $(LDFLAGS)

INSTALL = /usr/bin/install -c
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_PROGRAM = $(INSTALL) -m 755

usplash-theme-guadalinex.so: throbber_back.png.c.o throbber_back_16.png.c.o throbber_fore.png.c.o throbber_fore_16.png.c.o \
						 throbber_back_32.png.c.o throbber_fore_32.png.c.o \
						 usplash_300_200.png.c.o usplash_400_300.png.c.o usplash_600_500.png.c.o \
						 usplash-theme-guadalinex.c.o
	$(COMPILE) -shared -o $@ $^

%.png.c: %.png
	pngtousplash $< > $@

%.bdf.c: %.bdf
	bdftousplash $< > $@

%.c.o: %.c
	$(COMPILE) -o $@ -c $<

install:
	$(INSTALL) -d $(DESTDIR)/usr/lib/usplash
	$(INSTALL_PROGRAM) usplash-theme-guadalinex.so $(DESTDIR)/usr/lib/usplash/usplash-theme-guadalinex.so
clean:
	rm -f *.png.c *.bdf.c *.c.o *.so
