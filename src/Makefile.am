### Makefile.am
include $(top_builddir)/version.mk

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=201001L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
AM_CPPFLAGS += -D_HAVE_VERSION_H
AM_LDFLAGS = $(XCCLDFLAGS)

bin_PROGRAMS =
noinst_PROGRAMS =
lib_LIBRARIES =
noinst_LIBRARIES =
noinst_HEADERS =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES = 
SUFFIXES =

CLEANFILES += version.c
EXTRA_DIST += version.c.in

noinst_LIBRARIES += libdfp.a
libdfp_a_SOURCES =
libdfp_a_SOURCES += dfp754_d32.c dfp754_d32.h
libdfp_a_SOURCES += dfp754_d64.c dfp754_d64.h
libdfp_a_CPPFLAGS = $(AM_CPPFLAGS)
libdfp_a_CPPFLAGS += $(dfp754_CFLAGS)

lib_LIBRARIES += libbooks.a
libbooks_a_SOURCES =
libbooks_a_SOURCES += btree.c btree.h
libbooks_a_SOURCES += books.c books.h
libbooks_a_SOURCES += nifty.h
libbooks_a_SOURCES += btree_val.h
libbooks_a_CPPFLAGS = $(AM_CPPFLAGS)
libbooks_a_CPPFLAGS += $(dfp754_CFLAGS)
libbooks_a_LIBADD = $(dfp754_LIBS)
libbooks_a_LIBADD += libdfp.a

pkginclude_HEADERS =
pkginclude_HEADERS += books.h
pkginclude_HEADERS += btree.h
pkginclude_HEADERS += btree_val.h

bin_PROGRAMS += book2book
book2book_SOURCES = book2book.c book2book.yuck
book2book_SOURCES += xquo.c xquo.h
book2book_SOURCES += hash.c hash.h
book2book_SOURCES += version.c version.h
EXTRA_book2book_SOURCES = memrchr.c
book2book_CPPFLAGS = $(AM_CPPFLAGS)
book2book_CPPFLAGS += $(dfp754_CFLAGS)
book2book_LDFLAGS = $(AM_LDFLAGS)
book2book_LDFLAGS += $(dfp754_LIBS)
book2book_LDADD =
book2book_LDADD += libdfp.a
book2book_LDADD += libbooks.a
BUILT_SOURCES += book2book.yucc

bin_PROGRAMS += booksnap
booksnap_SOURCES = booksnap.c booksnap.yuck
booksnap_SOURCES += xquo.c xquo.h
booksnap_SOURCES += hash.c hash.h
booksnap_SOURCES += version.c version.h
EXTRA_booksnap_SOURCES = memrchr.c
booksnap_CPPFLAGS = $(AM_CPPFLAGS)
booksnap_CPPFLAGS += $(dfp754_CFLAGS)
booksnap_LDFLAGS = $(AM_LDFLAGS)
booksnap_LDFLAGS += $(dfp754_LIBS)
booksnap_LDADD =
booksnap_LDADD += libdfp.a
booksnap_LDADD += libbooks.a
BUILT_SOURCES += booksnap.yucc


## version rules
version.c: version.c.in $(top_builddir)/.version
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver --ignore-noscm --force -o $@ \
			--use-reference --reference $(top_builddir)/.version $<

## yuck rule
SUFFIXES += .yuck
SUFFIXES += .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $<

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
