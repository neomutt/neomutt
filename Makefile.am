## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake --foreign
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign
EXTRA_PROGRAMS = mutt_md5 pgpewrap pgpring

# Test the .tar file by building everything
AM_DISTCHECK_CONFIGURE_FLAGS = \
	--enable-debug \
	--enable-flock \
	--enable-gpgme \
	--enable-notmuch \
	--with-bdb \
	--with-gdbm \
	--with-gnutls \
	--with-gss \
	--with-kyotocabinet \
	--with-lmdb \
	--with-mixmaster \
	--with-qdbm \
	--with-sasl \
	--with-tokyocabinet

SUBDIRS = m4 contrib imap ncrypt

if BUILD_HCACHE
SUBDIRS += hcache
endif

if BUILD_DOC
SUBDIRS += doc
endif

if BUILD_PO
SUBDIRS += po
endif

bin_SCRIPTS = $(SMIMEAUX_TARGET)

distdir = neo$(PACKAGE)-$(VERSION)

BUILT_SOURCES = conststrings.c git_ver.h keymap_alldefs.h keymap_defs.h

bin_PROGRAMS = mutt $(PGPAUX_TARGET)

mutt_SOURCES = account.c addrbook.c address.h alias.c alias.h ascii.c attach.c \
	base64.c bcache.c body.h browser.c buffer.c buffy.c charset.c color.c \
	commands.c complete.c compose.c compress.c content.h context.h copy.c \
	curs_lib.c curs_main.c date.c edit.c editmsg.c enter.c enter_state.h \
	envelope.h filter.c flags.c format_flags.h from.c getdomain.c group.c \
	handler.c hash.c hdrline.c header.h headers.c help.c history.c hook.c \
	init.c keymap.c lib.c list.h main.c mbox.c mbyte.c mbyte_table.h md5.c \
	menu.c mh.c muttlib.c mutt_idna.c mutt_sasl_plain.c mutt_socket.c \
	mutt_tunnel.c mx.c newsrc.c nntp.c options.h pager.c parameter.h \
	parse.c pattern.c pattern.h pop.c pop_auth.c pop_lib.c postpone.c \
	query.c recvattach.c recvcmd.c rfc1524.c rfc2047.c rfc2231.c rfc3676.c \
	rfc822.c safe_asprintf.c score.c send.c sendlib.c sidebar.c signal.c \
	smtp.c sort.c state.h status.c system.c thread.c thread.h url.c \
	version.c where.h

nodist_mutt_SOURCES = $(BUILT_SOURCES)

LIBIMAP = -Limap -limap
LIBIMAPDEPS = $(top_srcdir)/imap/imap.h imap/libimap.a

mutt_LDADD = $(MUTT_LIB_OBJECTS) $(LIBOBJS) $(HCACHE_LIBS) $(NCRYPT_LIBS) \
	$(LIBIMAP) $(LIBICONV) $(GPGME_LIBS) $(INTLLIBS)

mutt_DEPENDENCIES = $(MUTT_LIB_OBJECTS) $(LIBOBJS) $(LIBIMAPDEPS) \
	$(HCACHE_DEPS) $(NCRYPT_DEPS) $(INTLDEPS)

DEFS=-DPKGDATADIR=\"$(pkgdatadir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DBINDIR=\"$(bindir)\" -DMUTTLOCALEDIR=\"$(datadir)/locale\"

AM_CPPFLAGS=-I. -I$(top_srcdir) $(GPGME_CFLAGS)

EXTRA_mutt_SOURCES = browser.h mbyte.h mutt_idna.c mutt_idna.h \
	mutt_lua.c mutt_sasl.c mutt_notmuch.c mutt_ssl.c mutt_ssl_gnutls.c \
	remailer.c remailer.h resize.c sha1.c url.h utf8.c wcwidth.c

EXTRA_DIST = account.h ascii.h attach.h bcache.h browser.h buffer.h buffy.h \
	ChangeLog.md charset.h CODE_OF_CONDUCT.md compress.h copy.h \
	COPYRIGHT extlib.c filter.h functions.h gen_defs globals.h \
	group.h hash.h history.h init.h keymap.h lib.h LICENSE.md mailbox.h \
	mapping.h mbyte.h md5.h mime.h mime.types mutt.h mutt_commands.h \
	mutt_curses.h mutt_idna.h mutt_lua.h mutt_menu.h mutt_notmuch.h \
	mutt_options.h mutt_regex.h mutt_sasl.h mutt_sasl_plain.h \
	mutt_socket.h mutt_ssl.h mutt_tunnel.h mx.h myvar.h nntp.h OPS \
	OPS.CRYPT OPS.MIX OPS.NOTMUCH OPS.PGP OPS.SIDEBAR OPS.SMIME pager.h \
	pgpewrap.c pop.h protos.h README.md README.SSL remailer.c remailer.h \
	rfc1524.h rfc2047.h rfc2231.h rfc3676.h rfc822.h sha1.h sidebar.h \
	sort.h txt2c.c txt2c.sh version.h \
	keymap_alldefs.h

EXTRA_SCRIPTS =

pgpring_SOURCES = ascii.c extlib.c lib.c md5.c pgppubring.c sha1.c
pgpring_LDADD = $(LIBOBJS) $(NCRYPT_LIBS) $(INTLLIBS)
pgpring_DEPENDENCIES = $(LIBOBJS) $(NCRYPT_DEPS) $(INTLDEPS)

mutt_md5_SOURCES = md5.c
mutt_md5_CFLAGS = -DMD5UTIL
mutt_md5_LDADD =

txt2c_SOURCES = txt2c.c
txt2c_LDADD =

noinst_PROGRAMS = $(MUTT_MD5) txt2c

conststrings.c: txt2c config.status
	( \
		($(CC) -v >/dev/null 2>&1 && $(CC) -v) || \
		($(CC) --version >/dev/null 2>&1 && $(CC) --version) || \
		($(CC) -V >/dev/null 2>&1 && $(CC) -V) || \
		echo "unknown compiler"; \
	) 2>&1 | ${srcdir}/txt2c.sh cc_version >conststrings_c;
	echo "$(CFLAGS)" | ${srcdir}/txt2c.sh cc_cflags >>conststrings_c
	grep ac_cs_config= config.status | \
		cut -d= -f2- | \
		sed -e 's/^"//' -e 's/"$$//' | \
		${srcdir}/txt2c.sh configure_options >> conststrings_c
	mv -f conststrings_c conststrings.c

CLEANFILES = keymap_alldefs.h $(BUILT_SOURCES)

DISTCLEANFILES= txt2c

ACLOCAL_AMFLAGS = -I m4

LDADD = $(LIBOBJS) $(INTLLIBS)

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

keymap_defs.h: $(OPS) $(srcdir)/gen_defs
	$(srcdir)/gen_defs $(OPS) > $@

keymap_alldefs.h: $(srcdir)/OPS $(srcdir)/OPS.SIDEBAR $(srcdir)/OPS.NOTMUCH $(srcdir)/OPS.MIX $(srcdir)/OPS.PGP $(srcdir)/OPS.SMIME $(srcdir)/OPS.CRYPT $(srcdir)/gen_defs config.h
	$(srcdir)/gen_defs $(srcdir)/OPS $(srcdir)/OPS.SIDEBAR $(srcdir)/OPS.NOTMUCH \
		$(srcdir)/OPS.MIX $(srcdir)/OPS.PGP $(srcdir)/OPS.SMIME $(srcdir)/OPS.CRYPT \
		> $@

git_ver.h: $(mutt_SOURCES) $(SUBDIRS)
	version=`git describe --dirty --abbrev=6 --match "neomutt-*" 2> /dev/null | sed -e 's/^neomutt-[0-9]\{8\}//' -e 's/g//'`; \
	echo 'const char *GitVer = "'$$version'";' > git_ver.h.tmp; \
	cmp -s git_ver.h.tmp git_ver.h || mv git_ver.h.tmp git_ver.h; \
	rm -f git_ver.h.tmp

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	$(INSTALL) -m 644 $(srcdir)/mime.types $(DESTDIR)$(sysconfdir)/mime.types.dist
	-if [ ! -f $(DESTDIR)$(sysconfdir)/mime.types ]; then \
		$(INSTALL) -m 644 $(srcdir)/mime.types $(DESTDIR)$(sysconfdir); \
	fi

uninstall-local:
	for i in mime.types; do \
		if cmp -s $(DESTDIR)$(sysconfdir)/$$i.dist $(DESTDIR)$(sysconfdir)/$$i; then \
			rm $(DESTDIR)$(sysconfdir)/$$i; \
		fi; \
		rm $(DESTDIR)$(sysconfdir)/$${i}.dist; \
	done

distclean-local:
	rm -fr html

html:
	doxygen doxygen/doxygen.conf

.PHONY: html
