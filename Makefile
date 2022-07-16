DESTDIR    =
PREFIX     =/usr/local
VARDIR     =/var/lib
AR         =ar
CC         =gcc
CFLAGS     =-Wall -g
PROGRAMS   =mpaycomet$(EXE)
LIBRARIES  =libmpaycomet.a
HEADERS    =mpaycomet.h
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS)
LIBS       =          \
    "-l:libcurl.a"    \
    "-l:libssl.a"     \
    "-l:libcrypto.a"  \
    "-l:libjansson.a" \
    "-pthread"        \
    "-l:libz.a"       \
    "-l:libzstd.a"    \
    "-ldl"

## 
all: $(PROGRAMS) $(LIBRARIES)
install: all
	install -d                  $(DESTDIR)$(PREFIX)/bin
	install -m755 $(PROGRAMS)   $(DESTDIR)$(PREFIX)/bin
	install -d                  $(DESTDIR)$(PREFIX)/include
	install -m644 $(HEADERS)    $(DESTDIR)$(PREFIX)/include
	install -d                  $(DESTDIR)$(PREFIX)/lib
	install -m644 $(LIBRARIES)  $(DESTDIR)$(PREFIX)/lib
clean:
	rm -f $(PROGRAMS) $(LIBRARIES)

##
libmpaycomet.a: $(SOURCES_L) $(HEADERS)
	mkdir -p .b
	$(CC) -c -o .b/mpaycomet.o mpaycomet.c $(CFLAGS_ALL)
	$(AR) -crs $@ .b/*.o
	rm -f .b/*.o
mpaycomet$(EXE): main.c libmpaycomet.a
	$(CC) -o $@ main.c libmpaycomet.a $(CFLAGS_ALL) $(LIBS)

## -- manpages --
ifneq ($(PREFIX),)
MAN_3=./mpaycomet.3 
install: install-man3
install-man3: $(MAN_3)
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man3
	cp $(MAN_3) $(DESTDIR)$(PREFIX)/share/man/man3
endif
## -- manpages --
## -- license --
ifneq ($(PREFIX),)
install: install-license
install-license: LICENSE
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/c-mpaycomet
	cp LICENSE $(DESTDIR)$(PREFIX)/share/doc/c-mpaycomet
endif
## -- license --
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
