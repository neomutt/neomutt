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

hcversion.h: $(top_srcdir)/mutt.h $(top_srcdir)/address.h $(top_srcdir)/list.h \
		$(top_srcdir)/buffer.h $(top_srcdir)/parameter.h \
		$(top_srcdir)/body.h $(top_srcdir)/envelope.h \
		$(top_srcdir)/header.h $(srcdir)/hcachever.sh ../$(MUTT_MD5)
	( echo '#include "config.h"'; echo '#include "mutt.h"'; \
	echo '#include "address.h"'; echo '#include "list.h"'; \
	echo '#include "buffer.h"'; echo '#include "parameter.h"'; \
	echo '#include "body.h"'; echo '#include "envelope.h"'; \
	echo '#include "header.h"'; \
	) | $(CPP) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) - | sh $(srcdir)/hcachever.sh hcversion.h

