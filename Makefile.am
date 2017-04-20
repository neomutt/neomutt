## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake --foreign
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign
EXTRA_PROGRAMS = mutt_dotlock pgpring pgpewrap mutt_md5

# Test the .tar file by building everything
AM_DISTCHECK_CONFIGURE_FLAGS = --enable-compressed --enable-debug \
	--enable-gpgme --enable-imap --enable-nntp --enable-notmuch \
	--enable-pop --enable-sidebar --enable-smtp --with-bdb --with-gdbm \
	--with-gnutls --with-gss --with-kyotocabinet --with-lmdb --with-qdbm \
	--with-sasl --with-tokyocabinet

SUBDIRS = m4 contrib

if BUILD_DOC
SUBDIRS += doc
endif

if BUILD_PO
SUBDIRS += po
endif

if BUILD_IMAP
SUBDIRS += imap
endif

bin_SCRIPTS = $(SMIMEAUX_TARGET)

if BUILD_HCACHE
HCVERSION = hcversion.h
endif

distdir = neo$(PACKAGE)-$(VERSION)

BUILT_SOURCES = keymap_defs.h keymap_alldefs.h patchlist.c git_ver.h conststrings.c hcachever.sh $(HCVERSION)

bin_PROGRAMS = mutt $(DOTLOCK_TARGET) $(PGPAUX_TARGET)
mutt_SOURCES = addrbook.c alias.c ascii.c attach.c base64.c browser.c buffer.c \
	buffy.c charset.c color.c commands.c complete.c compose.c copy.c \
	crypt_mod.c crypt_mod.h crypt.c cryptglue.c curs_lib.c curs_main.c \
	date.c edit.c editmsg.c enter.c filter.c flags.c from.c getdomain.c \
	group.c handler.c hash.c hdrline.c headers.c help.c history.c hook.c \
	init.c keymap.c lib.c main.c mbox.c mbyte.c menu.c mh.c muttlib.c \
	mutt_idna.c mutt_sasl_plain.c mx.c pager.c parse.c pattern.c \
	postpone.c query.c recvattach.c recvcmd.c rfc1524.c rfc2047.c \
	rfc2231.c rfc3676.c rfc822.c safe_asprintf.c score.c send.c sendlib.c \
	signal.c sort.c status.c system.c thread.c url.c version.c

nodist_mutt_SOURCES = $(BUILT_SOURCES)

mutt_LDADD = $(MUTT_LIB_OBJECTS) $(LIBOBJS) $(LIBIMAP) $(MUTTLIBS) \
	$(INTLLIBS) $(LIBICONV) $(GPGME_LIBS)

mutt_DEPENDENCIES = $(MUTT_LIB_OBJECTS) $(LIBOBJS) $(LIBIMAPDEPS) \
	$(INTLDEPS)

DEFS=-DPKGDATADIR=\"$(pkgdatadir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DBINDIR=\"$(bindir)\" -DMUTTLOCALEDIR=\"$(datadir)/locale\"

AM_CPPFLAGS=-I. -I$(top_srcdir) $(GPGME_CFLAGS)

EXTRA_mutt_SOURCES = account.c bcache.c bcache.h browser.h compress.c \
	crypt_gpgme.c crypt_mod_pgp_classic.c crypt_mod_pgp_gpgme.c \
	crypt_mod_smime_classic.c crypt_mod_smime_gpgme.c dotlock.c \
	gnupgparse.c hcache.c hcache.h mbyte.h md5.c mutt_idna.c mutt_idna.h \
	mutt_lua.c mutt_sasl.c mutt_socket.c mutt_ssl.c mutt_ssl_gnutls.c \
	mutt_tunnel.c newsrc.c nntp.c pgp.c pgpinvoke.c pgpkey.c pgplib.c \
	pgpmicalg.c pgppacket.c pop.c pop_auth.c pop_lib.c remailer.c \
	remailer.h resize.c sha1.c sidebar.c smime.c smtp.c url.h utf8.c \
	wcwidth.c

EXTRA_DIST = account.h ascii.h attach.h browser.h buffer.h buffy.h ChangeLog \
	ChangeLog.neomutt ChangeLog.nntp charset.h compress.h config.rpath \
	copy.h COPYRIGHT crypt_gpgme.h crypthash.h depcomp dotlock.h extlib.c \
	filter.h functions.h gen_defs globals.h group.h hash.h \
	hcache_backend.h hcache_bdb.c hcache_gdbm.c hcache_kc.c hcache_lmdb.c \
	hcache_qdbm.c hcache_tc.c hcachever.sh.in history.h init.h keymap.h \
	lib.h LICENSE.md mailbox.h mapping.h mbyte.h md5.h mime.h mime.types \
	mutt.h mutt_commands.h mutt_crypt.h mutt_curses.h mutt_idna.h \
	mutt_lua.h mutt_menu.h mutt_options.h mutt_regex.h mutt_sasl.h \
	mutt_sasl_plain.h mutt_socket.h mutt_ssl.h mutt_tunnel.h mx.h myvar.h \
	nntp.h OPS OPS.CRYPT OPS.MIX OPS.NOTMUCH OPS.PGP OPS.SIDEBAR OPS.SMIME \
	pager.h PATCHES patchlist.sh pgp.h pgpewrap.c pgplib.h pgppacket.h \
	pop.h protos.h README.md README.neomutt README.notmuch README.SECURITY \
	README.SSL remailer.c remailer.h rfc1524.h rfc2047.h rfc2231.h \
	rfc3676.h rfc822.h sha1.h sidebar.h smime.h smime_keys.pl sort.h TODO \
	txt2c.c txt2c.sh UPDATING UPDATING.kz version.h keymap_alldefs.h

EXTRA_SCRIPTS = smime_keys

if BUILD_NOTMUCH
mutt_SOURCES += mutt_notmuch.c mutt_notmuch.h
mutt_LDADD += $(NOTMUCH_LIBS)
endif

mutt_dotlock_SOURCES = mutt_dotlock.c
mutt_dotlock_LDADD = $(LIBOBJS)
mutt_dotlock_DEPENDENCIES = $(LIBOBJS)

pgpring_SOURCES = ascii.c extlib.c lib.c md5.c pgplib.c pgppacket.c \
	pgppubring.c sha1.c
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

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

smime_keys: $(srcdir)/smime_keys.pl
	cp $(srcdir)/smime_keys.pl smime_keys
	chmod +x smime_keys

keymap_defs.h: $(OPS) $(srcdir)/gen_defs
	$(srcdir)/gen_defs $(OPS) > keymap_defs.h

keymap_alldefs.h: $(srcdir)/OPS $(srcdir)/OPS.SIDEBAR $(srcdir)/OPS.NOTMUCH $(srcdir)/OPS.MIX $(srcdir)/OPS.PGP $(srcdir)/OPS.SMIME $(srcdir)/OPS.CRYPT $(srcdir)/gen_defs config.h
	rm -f $@
	$(srcdir)/gen_defs $(srcdir)/OPS $(srcdir)/OPS.SIDEBAR $(srcdir)/OPS.NOTMUCH \
		$(srcdir)/OPS.MIX $(srcdir)/OPS.PGP $(srcdir)/OPS.SMIME $(srcdir)/OPS.CRYPT \
		> keymap_alldefs.h

git_ver.h:
	@version=`git describe --dirty --abbrev=6 --match "neomutt-*" 2> /dev/null | sed -e 's/^neomutt-[0-9]\{8\}//' -e 's/g//'`; \
	echo 'const char *GitVer = "'$$version'";' > git_ver.h.tmp; \
	cmp -s git_ver.h.tmp git_ver.h || mv git_ver.h.tmp git_ver.h; \
	rm -f git_ver.h.tmp

hcversion.h: $(srcdir)/mutt.h $(srcdir)/rfc822.h hcachever.sh $(MUTT_MD5)
	( echo '#include "config.h"'; echo '#include "mutt.h"'; ) \
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

.PHONY: git_ver.h
