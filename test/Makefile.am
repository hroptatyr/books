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
LDADD = $(books_LIBS)

EXTRA_DIST = $(BUILT_SOURCES) $(clitests)
TESTS = $(bintests) $(clitests)
TEST_EXTENSIONS =
BUILT_SOURCES =
clitests =
bintests =
check_PROGRAMS =
CLEANFILES = $(check_PROGRAMS)

books_CFLAGS = -I$(top_srcdir)/src
books_LIBS = $(top_builddir)/src/libbooks.a

## summon our helper
include clitoris.am
AM_CLIT_LOG_FLAGS = -v --builddir $(top_builddir)/src
TESTS_ENVIRONMENT = root=$(top_srcdir)


clitests += book2book_01.clit
clitests += book2book_02.clit
clitests += book2book_03.clit
clitests += book2book_04.clit
clitests += book2book_05.clit
clitests += book2book_06.clit
clitests += book2book_07.clit
clitests += book2book_08.clit
clitests += book2book_09.clit
clitests += book2book_10.clit
clitests += book2book_11.clit
clitests += book2book_12.clit
clitests += book2book_13.clit
clitests += book2book_14.clit
clitests += book2book_15.clit
clitests += book2book_16.clit
clitests += book2book_17.clit
clitests += book2book_18.clit
clitests += book2book_19.clit
clitests += book2book_20.clit
clitests += book2book_21.clit
clitests += book2book_22.clit
clitests += book2book_23.clit
clitests += book2book_24.clit

clitests += booksnap_01.clit
clitests += booksnap_02.clit
clitests += booksnap_03.clit
clitests += booksnap_04.clit
clitests += booksnap_05.clit
clitests += booksnap_06.clit
clitests += booksnap_07.clit
clitests += booksnap_08.clit
clitests += booksnap_09.clit
clitests += booksnap_10.clit
clitests += booksnap_11.clit

EXTRA_DIST += xmpl_01.b
EXTRA_DIST += xmpl_02.b
EXTRA_DIST += xmpl_03.b
EXTRA_DIST += xmpl_04.b
EXTRA_DIST += xmpl_05.b
EXTRA_DIST += xmpl_06.b
EXTRA_DIST += xmpl_07.b
EXTRA_DIST += xmpl_08.b
EXTRA_DIST += xmpl_09.b
EXTRA_DIST += xmpl_10.b
EXTRA_DIST += xmpl_11.b
EXTRA_DIST += xmpl_01.s

check_PROGRAMS += book_top_01
bintests += book_top_01
check_PROGRAMS += book_top_02
bintests += book_top_02
check_PROGRAMS += book_top_03
bintests += book_top_03

check_PROGRAMS += book_pdo_01
bintests += book_pdo_01

## Makefile.am ends here
