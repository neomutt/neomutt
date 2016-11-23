## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake --foreign
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign
EXTRA_PROGRAMS = mutt_dotlock pgpring pgpewrap mutt_md5

if BUILD_DOC
DOC_SUBDIR = doc
endif

if BUILD_PO
PO_SUBDIR = po
endif

if BUILD_IMAP
IMAP_SUBDIR = imap
IMAP_INCLUDES = -I$(top_srcdir)/imap
endif

SUBDIRS = m4 intl $(DOC_SUBDIR) $(PO_SUBDIR) contrib $(IMAP_SUBDIR)

bin_SCRIPTS = $(SMIMEAUX_TARGET)

if BUILD_HCACHE
HCVERSION = hcversion.h
endif

distdir = neo$(PACKAGE)-$(VERSION)

BUILT_SOURCES = keymap_defs.h patchlist.c oldmutt_ver.h conststrings.c hcachever.sh $(HCVERSION) $(top_srcdir)/.version

bin_PROGRAMS = mutt $(DOTLOCK_TARGET) $(PGPAUX_TARGET)
mutt_SOURCES = \
	addrbook.c alias.c attach.c base64.c browser.c buffy.c color.c \
	crypt.c cryptglue.c \
	commands.c complete.c compose.c copy.c curs_lib.c curs_main.c date.c \
	edit.c enter.c flags.c init.c filter.c from.c \
	getdomain.c group.c \
	handler.c hash.c hdrline.c headers.c help.c hook.c keymap.c \
	main.c mbox.c menu.c mh.c mx.c pager.c parse.c pattern.c \
	postpone.c query.c recvattach.c recvcmd.c \
	rfc822.c rfc1524.c rfc2047.c rfc2231.c rfc3676.c \
	score.c send.c sendlib.c signal.c sort.c \
	status.c system.c thread.c charset.c history.c lib.c \
	muttlib.c editmsg.c mbyte.c \
	url.c ascii.c crypt-mod.c crypt-mod.h safe_asprintf.c version.c

nodist_mutt_SOURCES = $(BUILT_SOURCES)

mutt_LDADD = $(MUTT_LIB_OBJECTS) $(LIBOBJS) $(LIBIMAP) $(MUTTLIBS) \
	$(INTLLIBS) $(LIBICONV)  $(GPGME_LIBS)

mutt_DEPENDENCIES = $(MUTT_LIB_OBJECTS) $(LIBOBJS) $(LIBIMAPDEPS) \
	$(INTLDEPS)

DEFS=-DPKGDATADIR=\"$(pkgdatadir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DBINDIR=\"$(bindir)\" -DMUTTLOCALEDIR=\"$(datadir)/locale\" \
	-DHAVE_CONFIG_H=1

AM_CPPFLAGS=-I. -I$(top_srcdir) $(IMAP_INCLUDES) $(GPGME_CFLAGS) -Iintl

EXTRA_mutt_SOURCES = account.c bcache.c compress.c crypt-gpgme.c crypt-mod-pgp-classic.c \
	crypt-mod-pgp-gpgme.c crypt-mod-smime-classic.c \
	crypt-mod-smime-gpgme.c dotlock.c gnupgparse.c hcache.c md5.c \
	mutt_idna.c mutt_sasl.c mutt_socket.c mutt_ssl.c mutt_ssl_gnutls.c \
	mutt_tunnel.c pgp.c pgpinvoke.c pgpkey.c pgplib.c pgpmicalg.c \
	pgppacket.c pop.c pop_auth.c pop_lib.c remailer.c resize.c sha1.c \
	nntp.c newsrc.c \
	sidebar.c smime.c smtp.c utf8.c wcwidth.c \
	bcache.h browser.h hcache.h mbyte.h mutt_idna.h remailer.h url.h

EXTRA_DIST = COPYRIGHT LICENSE.md OPS OPS.PGP OPS.CRYPT OPS.SMIME TODO UPDATING \
	account.h \
	attach.h buffy.h charset.h compress.h copy.h crypthash.h dotlock.h functions.h gen_defs \
	globals.h hash.h history.h init.h keymap.h mutt_crypt.h \
	mailbox.h mapping.h md5.h mime.h mutt.h mutt_curses.h mutt_menu.h \
	mutt_regex.h mutt_sasl.h mutt_socket.h mutt_ssl.h mutt_tunnel.h \
	mx.h pager.h pgp.h pop.h protos.h rfc1524.h rfc2047.h \
	rfc2231.h rfc822.h rfc3676.h sha1.h sort.h mime.types \
	nntp.h ChangeLog.nntp \
	_regex.h OPS.MIX README.SECURITY remailer.c remailer.h browser.h \
	mbyte.h lib.h extlib.c pgpewrap.c smime_keys.pl pgplib.h \
	README.SSL README.md README.neomutt README.notmuch smime.h group.h \
	pgppacket.h depcomp ascii.h PATCHES patchlist.sh \
	ChangeLog ChangeLog.neomutt mutt_idna.h sidebar.h OPS.SIDEBAR \
	snprintf.c regex.c crypt-gpgme.h hcachever.sh.in \
	txt2c.c txt2c.sh version.h \
	$(top_srcdir)/.version git-version-gen

EXTRA_SCRIPTS = smime_keys

if BUILD_NOTMUCH
mutt_SOURCES += mutt_notmuch.c mutt_notmuch.h
mutt_LDADD += $(NOTMUCH_LIBS)
endif

# kz
EXTRA_DIST += UPDATING.kz OPS.NOTMUCH



mutt_dotlock_SOURCES = mutt_dotlock.c
mutt_dotlock_LDADD = $(LIBOBJS)
mutt_dotlock_DEPENDENCIES = $(LIBOBJS)

pgpring_SOURCES = pgppubring.c pgplib.c lib.c extlib.c sha1.c md5.c pgppacket.c ascii.c
pgpring_LDADD = $(LIBOBJS) $(INTLLIBS)
pgpring_DEPENDENCIES = $(LIBOBJS) $(INTLDEPS)

mutt_md5_SOURCES = md5.c
mutt_md5_CFLAGS = -DMD5UTIL
mutt_md5_LDADD =

txt2c_SOURCES = txt2c.c
txt2c_LDADD =

noinst_PROGRAMS = $(MUTT_MD5) txt2c

mutt_dotlock.c: dotlock.c
	cp $(srcdir)/dotlock.c mutt_dotlock.c

conststrings.c: txt2c config.status
	( \
		($(CC) -v >/dev/null 2>&1 && $(CC) -v) || \
		($(CC) --version >/dev/null 2>&1 && $(CC) --version) || \
		($(CC) -V >/dev/null 2>&1 && $(CC) -V) || \
		echo "unknown compiler"; \
	) 2>&1 | ${srcdir}/txt2c.sh cc_version >conststrings_c
	echo "$(CFLAGS)" | ${srcdir}/txt2c.sh cc_cflags >>conststrings_c
	grep ac_cs_config= config.status | \
	cut -d= -f2- | \
	sed -e 's/^"//' -e 's/"$$//' | ${srcdir}/txt2c.sh configure_options >>conststrings_c
	mv -f conststrings_c conststrings.c

CLEANFILES = mutt_dotlock.c keymap_alldefs.h $(BUILT_SOURCES)

DISTCLEANFILES= smime_keys txt2c po/$(PACKAGE).pot

ACLOCAL_AMFLAGS = -I m4

LDADD = $(LIBOBJS) $(INTLLIBS)

$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

smime_keys: $(srcdir)/smime_keys.pl
	cp $(srcdir)/smime_keys.pl smime_keys
	chmod +x smime_keys

keymap_defs.h: $(OPS) $(srcdir)/gen_defs
	$(srcdir)/gen_defs $(OPS) > keymap_defs.h

keymap_alldefs.h: $(srcdir)/OPS $(srcdir)/OPS.SIDEBAR $(srcdir)/OPS.NOTMUCH $(srcdir)/OPS.PGP $(srcdir)/OPS.MIX $(srcdir)/OPS.CRYPT $(srcdir)/OPS.SMIME $(srcdir)/gen_defs
	rm -f $@
	$(srcdir)/gen_defs $(srcdir)/OPS $(srcdir)/OPS.SIDEBAR $(srcdir)/OPS.NOTMUCH $(srcdir)/OPS.PGP \
		$(srcdir)/OPS.MIX $(srcdir)/OPS.CRYPT $(srcdir)/OPS.SMIME \
			> keymap_alldefs.h

oldmutt_ver.h:
	version="1.7.1" && \
	echo 'const char *OldMuttVer = "'$$version'";' > oldmutt_ver.h.tmp; \
	cmp -s oldmutt_ver.h.tmp oldmutt_ver.h || mv oldmutt_ver.h.tmp oldmutt_ver.h

# reldate.h:
# 	date=`head -n 1 $(top_srcdir)/ChangeLog.neomutt | LC_ALL=C cut -b 1-10` && \
# 	echo 'const char *ReleaseDate = "'$$date'";' > reldate.h.tmp; \
# 	cmp -s reldate.h.tmp reldate.h || cp reldate.h.tmp reldate.h; \
# 	rm reldate.h.tmp

# The '#undef ENABLE_NLS' is to work around an automake ordering issue:
# BUILT_SOURCES are processed before SUBDIRS.
# If configured with --with-included-gettext this means that intl will
# not have generated libintl.h yet, and mutt.h -> lib.h will generate
# an error.
hcversion.h: $(srcdir)/mutt.h $(srcdir)/rfc822.h hcachever.sh $(MUTT_MD5)
	( echo '#include "config.h"'; echo '#undef ENABLE_NLS'; echo '#include "mutt.h"'; ) \
	| $(CPP) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) - | sh ./hcachever.sh hcversion.h

patchlist.c: $(srcdir)/PATCHES $(srcdir)/patchlist.sh
	$(srcdir)/patchlist.sh < $(srcdir)/PATCHES > patchlist.c

install-exec-hook:
	if test -f $(DESTDIR)$(bindir)/mutt.dotlock && test -f $(DESTDIR)$(bindir)/mutt_dotlock ; then	\
		rm -f $(DESTDIR)$(bindir)/mutt.dotlock ;		\
		ln -sf $(DESTDIR)$(bindir)/mutt_dotlock $(DESTDIR)$(bindir)/mutt.dotlock ; \
	fi
	if test -f $(DESTDIR)$(bindir)/mutt_dotlock && test x$(DOTLOCK_GROUP) != x ; then \
		chgrp $(DOTLOCK_GROUP) $(DESTDIR)$(bindir)/mutt_dotlock && \
		chmod $(DOTLOCK_PERMISSION) $(DESTDIR)$(bindir)/mutt_dotlock || \
		{ echo "Can't fix mutt_dotlock's permissions!  This is required to lock mailboxes in the mail spool directory." >&2 ; exit 1 ; } \
	fi

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	$(INSTALL) -m 644 $(srcdir)/mime.types $(DESTDIR)$(sysconfdir)/mime.types.dist
	-if [ ! -f $(DESTDIR)$(sysconfdir)/mime.types ]; then \
		$(INSTALL) -m 644 $(srcdir)/mime.types $(DESTDIR)$(sysconfdir); \
	fi

uninstall-local:
	for i in mime.types ; do \
		if cmp -s $(DESTDIR)$(sysconfdir)/$$i.dist $(DESTDIR)$(sysconfdir)/$$i ; then \
			rm $(DESTDIR)$(sysconfdir)/$$i ; \
		fi ; \
		rm $(DESTDIR)$(sysconfdir)/$${i}.dist ; \
	done

pclean:
	cat /dev/null > $(top_srcdir)/PATCHES

check-security:
	(cd $(top_srcdir) && ./check_sec.sh)

update-doc:
	(cd doc && $(MAKE) update-doc)

.PHONY: commit pclean check-security
