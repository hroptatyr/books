# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_CPPFLAGS += -DTEST
AM_CPPFLAGS += $(books_CFLAGS)

EXTRA_DIST = $(BUILT_SOURCES) $(TESTS)
TESTS =
TEST_EXTENSIONS =
BUILT_SOURCES =
dt_tests =
bin_tests =
batch_tests =
check_PROGRAMS =
CLEANFILES = $(check_PROGRAMS)

books_CFLAGS = -I$(top_srcdir)/src
books_LIBS = $(top_builddir)/src/libbooks.a

## summon our helper
include clitoris.am
AM_CLIT_LOG_FLAGS = -v --builddir $(top_builddir)/src
TESTS_ENVIRONMENT = root=$(top_srcdir)


check_PROGRAMS += btree_01
TESTS += btree_01.clit
btree_01_LDADD = $(books_LIBS)

check_PROGRAMS += btree_02
TESTS += btree_02.clit
btree_02_LDADD = $(books_LIBS)

TESTS += book2book_01.clit
TESTS += book2book_02.clit
TESTS += book2book_03.clit
TESTS += book2book_04.clit
TESTS += book2book_05.clit
TESTS += book2book_06.clit
TESTS += book2book_07.clit
TESTS += book2book_08.clit
TESTS += book2book_09.clit
TESTS += book2book_10.clit
TESTS += book2book_11.clit
TESTS += book2book_12.clit
TESTS += book2book_13.clit
TESTS += book2book_14.clit
TESTS += book2book_15.clit
TESTS += book2book_16.clit
TESTS += book2book_17.clit
TESTS += book2book_18.clit
TESTS += book2book_19.clit
TESTS += book2book_20.clit

TESTS += booksnap_01.clit
TESTS += booksnap_02.clit
TESTS += booksnap_03.clit
TESTS += booksnap_04.clit
TESTS += booksnap_05.clit
TESTS += booksnap_06.clit

EXTRA_DIST += xmpl_01.b
EXTRA_DIST += xmpl_02.b
EXTRA_DIST += xmpl_03.b
EXTRA_DIST += xmpl_04.b
EXTRA_DIST += xmpl_05.b
EXTRA_DIST += xmpl_06.b
EXTRA_DIST += xmpl_07.b

## Makefile.am ends here
