## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

EXTRA_DIST = bdb.c gdbm.c hcache.c kc.c lmdb.c qdbm.c tc.c hcachever.sh

AM_CPPFLAGS = -I$(top_srcdir)

noinst_LIBRARIES = libhcache.a
noinst_HEADERS = backend.h hcache.h

libhcache_a_SOURCES =

if BUILD_HCACHE
HCVERSION = hcversion.h
CLEANFILES = $(HCVERSION)
libhcache_a_SOURCES += hcache.c
endif
if BUILD_HC_BDB
libhcache_a_SOURCES += bdb.c
endif
if BUILD_HC_GDBM
libhcache_a_SOURCES += gdbm.c
endif
if BUILD_HC_KC
libhcache_a_SOURCES += kc.c
endif
if BUILD_HC_LMDB
libhcache_a_SOURCES += lmdb.c
endif
if BUILD_HC_QDBM
libhcache_a_SOURCES += qdbm.c
endif
if BUILD_HC_TC
libhcache_a_SOURCES += tc.c
endif

BUILT_SOURCES = $(HCVERSION)

../$(MUTT_MD5):
	+$(MAKE) -C .. $(MUTT_MD5)

$(top_srcdir)/keymap_defs.h:
	+$(MAKE) -C $(top_srcdir) keymap_defs.h

hcversion.h: $(top_srcdir)/mutt.h $(top_srcdir)/rfc822.h $(srcdir)/hcachever.sh ../$(MUTT_MD5)
	$(AM_V_GEN)( echo '#include "config.h"'; echo '#include "mutt.h"'; ) \
	| $(CPP) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) - | sh $(srcdir)/hcachever.sh hcversion.h

