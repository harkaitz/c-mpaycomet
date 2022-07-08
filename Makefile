## Configuration
DESTDIR    =
PREFIX     =/usr/local
VARDIR     =/var/lib
AR         =ar
CC         =gcc
CFLAGS     =-Wall -g
CPPFLAGS   =
LIBS       =          \
    "-l:libcurl.a"    \
    "-l:libssl.a"     \
    "-l:libcrypto.a"  \
    "-l:libjansson.a" \
    "-pthread"        \
    "-l:libz.a"       \
    "-l:libzstd.a"    \
    "-ldl"
## Sources and targets
PROGRAMS   =mpaycomet
LIBRARIES  =libmpaycomet.a
HEADERS    =mpaycomet.h
MARKDOWNS  =README.md mpaycomet.3.md
MANPAGES_3 =mpaycomet.3
SOURCES_L  =mpaycomet.c
SOURCES    =main.c $(SOURCES_L)

## AUXILIARY
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS)

## STANDARD TARGETS
all: $(PROGRAMS) $(LIBRARIES)
help:
	@echo "all     : Build everything."
	@echo "clean   : Clean files."
	@echo "install : Install all produced files."
install: all
	install -d                  $(DESTDIR)$(PREFIX)/bin
	install -m755 $(PROGRAMS)   $(DESTDIR)$(PREFIX)/bin
	install -d                  $(DESTDIR)$(PREFIX)/include
	install -m644 $(HEADERS)    $(DESTDIR)$(PREFIX)/include
	install -d                  $(DESTDIR)$(PREFIX)/lib
	install -m644 $(LIBRARIES)  $(DESTDIR)$(PREFIX)/lib
	install -d                  $(DESTDIR)$(PREFIX)/share/man/man3	
	install -m644 $(MANPAGES_3) $(DESTDIR)$(PREFIX)/share/man/man3
clean:
	rm -f $(PROGRAMS) $(LIBRARIES)
ssnip:
	ssnip LICENSE $(MARKDOWNS) $(HEADERS) $(SOURCES) $(MANPAGES_3)

## LIBRARY
libmpaycomet.a: $(SOURCES_L) $(HEADERS)
	mkdir -p .b
	cd .b && $(CC) -c $(SOURCES_L:%=../%) $(CFLAGS_ALL)
	$(AR) -crs $@ .b/*.o
	rm -f .b/*.o
mpaycomet: main.c libmpaycomet.a
	$(CC) -o $@ main.c libmpaycomet.a $(CFLAGS_ALL) $(LIBS)
## -- gettext --
ifneq ($(PREFIX),)
install: install-po
install-po:
	mkdir -p $(DESTDIR)$(PREFIX)/share/locale/es/LC_MESSAGES
	cp locales/es/LC_MESSAGES/c-mpaycomet.mo $(DESTDIR)$(PREFIX)/share/locale/es/LC_MESSAGES
	mkdir -p $(DESTDIR)$(PREFIX)/share/locale/eu/LC_MESSAGES
	cp locales/eu/LC_MESSAGES/c-mpaycomet.mo $(DESTDIR)$(PREFIX)/share/locale/eu/LC_MESSAGES
endif
## -- gettext --
