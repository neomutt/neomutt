## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake --foreign
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign
EXTRA_PROGRAMS = pgpewrap pgpring

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

SUBDIRS = m4 contrib imap ncrypt lib

if BUILD_HCACHE
SUBDIRS += hcache
endif

if BUILD_DOC
SUBDIRS += doc
endif

if BUILD_PO
SUBDIRS += po
endif

distdir = $(PACKAGE)-$(VERSION)

BUILT_SOURCES = conststrings.c git_ver.h

bin_PROGRAMS = neomutt

neomutt_SOURCES = account.c addrbook.c address.h alias.c alias.h attach.c \
	bcache.c body.c body.h browser.c buffy.c charset.c color.c \
	commands.c complete.c compose.c compress.c content.h context.h copy.c \
	curs_lib.c curs_main.c edit.c editmsg.c enter.c enter_state.h \
	envelope.c envelope.h filter.c flags.c format_flags.h from.c getdomain.c group.c \
	handler.c hdrline.c header.h headers.c help.c history.c hook.c \
	init.c keymap.c main.c mbox.c mbyte.c mbtable.h \
	menu.c mh.c muttlib.c mutt_idna.c mutt_sasl_plain.c mutt_socket.c \
	mutt_tunnel.c mx.c newsrc.c nntp.c options.h pager.c parameter.c parameter.h \
	parse.c pattern.c pattern.h pop.c pop_auth.c pop_lib.c postpone.c \
	query.c recvattach.c recvcmd.c rfc1524.c rfc2047.c rfc2231.c rfc3676.c \
	rfc822.c safe_asprintf.c score.c send.c sendlib.c sidebar.c signal.c \
	smtp.c sort.c state.c state.h status.c system.c thread.c thread.h url.c \
	version.c where.h mutt_tags.c

nodist_neomutt_SOURCES = $(BUILT_SOURCES)

LIBIMAP = -Limap -limap
LIBIMAPDEPS = $(top_srcdir)/imap/imap.h imap/libimap.a

LIBMUTT = -Llib -lmutt
LIBMUTTDEPS = $(top_srcdir)/lib/lib.h lib/libmutt.a

neomutt_LDADD = $(MUTT_LIB_OBJECTS) $(LIBOBJS) $(HCACHE_LIBS) $(NCRYPT_LIBS) \
	$(LIBIMAP) $(LIBMUTT) $(LIBICONV) $(GPGME_LIBS) $(INTLLIBS)

neomutt_DEPENDENCIES = $(MUTT_LIB_OBJECTS) $(LIBOBJS) $(LIBIMAPDEPS) \
	$(LIBMUTTDEPS) $(HCACHE_DEPS) $(NCRYPT_DEPS) $(INTLDEPS)

DEFS=-DPKGDATADIR=\"$(pkgdatadir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DBINDIR=\"$(bindir)\" -DMUTTLOCALEDIR=\"$(datadir)/locale\"

AM_CPPFLAGS=-I. -I$(top_srcdir) $(GPGME_CFLAGS)

EXTRA_neomutt_SOURCES = browser.h mbyte.h mutt_idna.c mutt_idna.h \
	mutt_lua.c mutt_sasl.c mutt_notmuch.c mutt_ssl.c mutt_ssl_gnutls.c \
	remailer.c remailer.h resize.c url.h

EXTRA_DIST = account.h attach.h bcache.h browser.h buffy.h \
	ChangeLog.md charset.h CODE_OF_CONDUCT.md compress.h copy.h \
	COPYRIGHT filter.h functions.h globals.h \
	group.h history.h init.h keymap.h LICENSE.md mailbox.h \
	mbyte.h mime.h mutt.h mutt_commands.h \
	mutt_curses.h mutt_idna.h mutt_lua.h mutt_menu.h mutt_notmuch.h \
	mutt_options.h mutt_regex.h mutt_sasl.h mutt_sasl_plain.h \
	mutt_socket.h mutt_ssl.h mutt_tunnel.h mx.h myvar.h nntp.h opcodes.h pager.h \
	pgpewrap.c pop.h protos.h README.md README.SSL remailer.c remailer.h \
	rfc1524.h rfc2047.h rfc2231.h rfc3676.h rfc822.h sidebar.h \
	sort.h txt2c.c txt2c.sh version.h mutt_tags.h

EXTRA_SCRIPTS =

pgpring_SOURCES = pgppubring.c
pgpring_LDADD = $(LIBOBJS) $(NCRYPT_LIBS) $(INTLLIBS) $(LIBMUTT)
pgpring_DEPENDENCIES = $(LIBOBJS) $(NCRYPT_DEPS) $(INTLDEPS) $(LIBMUTTDEPS)

txt2c_SOURCES = txt2c.c
txt2c_LDADD =

noinst_PROGRAMS = txt2c $(PGPAUX_TARGET)

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

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES= txt2c

ACLOCAL_AMFLAGS = -I m4

LDADD = $(LIBOBJS) $(INTLLIBS)

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

git_ver.h: $(neomutt_SOURCES) $(SUBDIRS)
	version=`git describe --dirty --abbrev=6 --match "neomutt-*" 2> /dev/null | sed -e 's/^neomutt-[0-9]\{8\}//' -e 's/g//'`; \
	echo 'const char *GitVer = "'$$version'";' > git_ver.h.tmp; \
	cmp -s git_ver.h.tmp git_ver.h || mv git_ver.h.tmp git_ver.h; \
	rm -f git_ver.h.tmp

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	-if test -n "$(SMIMEAUX_TARGET)"; then \
		$(MKDIR_P) $(DESTDIR)$(exec_prefix)/lib/$(PACKAGE); \
		for i in $(SMIMEAUX_TARGET); do \
			$(INSTALL) -m 755 $$i $(DESTDIR)$(exec_prefix)/lib/$(PACKAGE); \
		done \
	fi
	-if test -n "$(PGPAUX_TARGET)"; then \
		$(MKDIR_P) $(DESTDIR)$(exec_prefix)/lib/$(PACKAGE); \
		for i in $(PGPAUX_TARGET); do \
			$(INSTALL) -m 755 $$i $(DESTDIR)$(exec_prefix)/lib/$(PACKAGE); \
		done \
	fi

uninstall-local:
	for i in smime_keys $(PGPAUX_TARGET); do \
		rm -fr $(DESTDIR)$(exec_prefix)/lib/$(PACKAGE)/$$i; \
	done

distclean-local:
	rm -fr html

html:
	doxygen doxygen/doxygen.conf

.PHONY: html
