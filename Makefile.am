## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake --foreign

AUTOMAKE_OPTIONS = 1.6 foreign
EXTRA_PROGRAMS = mutt_dotlock pgpring makedoc

if BUILD_IMAP
IMAP_SUBDIR = imap
IMAP_INCLUDES = -I$(top_srcdir)/imap
endif

SUBDIRS = m4 po intl doc contrib $(IMAP_SUBDIR)

bin_SCRIPTS = muttbug flea @SMIMEAUX_TARGET@

BUILT_SOURCES = keymap_defs.h patchlist.c

bin_PROGRAMS = mutt @DOTLOCK_TARGET@ @PGPAUX_TARGET@
mutt_SOURCES = $(BUILT_SOURCES) \
	addrbook.c alias.c attach.c base64.c browser.c buffy.c color.c \
        crypt.c cryptglue.c \
	commands.c complete.c compose.c copy.c curs_lib.c curs_main.c date.c \
	edit.c enter.c flags.c init.c filter.c from.c getdomain.c \
	handler.c hash.c hdrline.c headers.c help.c hook.c keymap.c \
	main.c mbox.c menu.c mh.c mx.c pager.c parse.c pattern.c \
	postpone.c query.c recvattach.c recvcmd.c \
	rfc822.c rfc1524.c rfc2047.c rfc2231.c \
	score.c send.c sendlib.c signal.c sort.c \
	status.c system.c thread.c charset.c history.c lib.c \
	muttlib.c editmsg.c utf8.c mbyte.c wcwidth.c \
	url.c ascii.c mutt_idna.c crypt-mod.c crypt-mod.h

mutt_LDADD = @MUTT_LIB_OBJECTS@ @LIBOBJS@ $(LIBIMAP) $(MUTTLIBS) \
	$(INTLLIBS) $(LIBICONV)  $(LIBGPGME_LIBS)

mutt_DEPENDENCIES = @MUTT_LIB_OBJECTS@ @LIBOBJS@ $(LIBIMAPDEPS) \
	$(INTLDEPS)

makedoc_SOURCES = makedoc.c
makedoc_LDADD =
makedoc_DEPENDENCIES = 

# $(makedoc_OBJECTS): $(makedoc_SOURCES)
# 	$(HOST_CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) -c $<

# makedoc: $(makedoc_OBJECTS) $(makedoc_DEPENDENCIES)
# 	@rm -rf makedoc
# 	$(HOST_CC) $(AM_CFLAGS) $(LDFLAGS) $(makedoc_LDFLAGS) $(makedoc_OBJECTS) $(makedoc_LDADD) -o makedoc

CPP=@CPP@

DEFS=-DPKGDATADIR=\"$(pkgdatadir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DBINDIR=\"$(bindir)\" -DMUTTLOCALEDIR=\"$(datadir)/locale\" \
	-DHAVE_CONFIG_H=1

INCLUDES=-I. -I$(top_srcdir) $(IMAP_INCLUDES) $(LIBGPGME_CFLAGS) -Iintl

CPPFLAGS=@CPPFLAGS@ -I$(includedir)


EXTRA_mutt_SOURCES = account.c md5c.c mutt_sasl.c mutt_socket.c mutt_ssl.c \
	mutt_tunnel.c pop.c pop_auth.c pop_lib.c smime.c pgp.c pgpinvoke.c pgpkey.c \
	pgplib.c sha1.c pgpmicalg.c gnupgparse.c resize.c dotlock.c remailer.c \
	browser.h mbyte.h remailer.h url.h mutt_ssl_nss.c \
	crypt-mod-pgp-classic.c crypt-mod-smime-classic.c \
	pgppacket.c mutt_idna.h hcache.c mutt_ssl_gnutls.c \
	crypt-gpgme.c crypt-mod-pgp-gpgme.c crypt-mod-smime-gpgme.c

EXTRA_DIST = COPYRIGHT GPL OPS OPS.PGP OPS.CRYPT OPS.SMIME TODO \
	configure account.h \
	attach.h buffy.h charset.h copy.h crypthash.h dotlock.h functions.h gen_defs \
	globals.h hash.h history.h init.h keymap.h mutt_crypt.h \
	mailbox.h mapping.h md5.h mime.h mutt.h mutt_curses.h mutt_menu.h \
	mutt_regex.h mutt_sasl.h mutt_socket.h mutt_ssl.h mutt_tunnel.h \
	mx.h pager.h pgp.h pop.h protos.h reldate.h rfc1524.h rfc2047.h \
	rfc2231.h rfc822.h sha1.h sort.h mime.types VERSION prepare \
	_regex.h OPS.MIX README.SECURITY remailer.c remailer.h browser.h \
	mbyte.h	lib.h extlib.c pgpewrap.c smime_keys.pl pgplib.h Muttrc.head Muttrc \
	makedoc.c stamp-doc-rc README.SSL smime.h\
	muttbug pgppacket.h depcomp ascii.h BEWARE PATCHES patchlist.sh	\
	ChangeLog.old mkchangelog.sh cvslog2changelog.pl mutt_idna.h \
	snprintf.c regex.c crypt-gpgme.h

EXTRA_SCRIPTS = smime_keys

mutt_dotlock_SOURCES = mutt_dotlock.c
mutt_dotlock_LDADD = @LIBOBJS@
mutt_dotlock_DEPENDENCIES = @LIBOBJS@

pgpring_SOURCES = pgppubring.c pgplib.c lib.c extlib.c sha1.c md5c.c pgppacket.c ascii.c
pgpring_LDADD = @LIBOBJS@ $(INTLLIBS) 
pgpring_DEPENDENCIES = @LIBOBJS@ $(INTLDEPS)

mutt_dotlock.c: dotlock.c
	cp $(srcdir)/dotlock.c mutt_dotlock.c

CLEANFILES = mutt_dotlock.c stamp-doc-rc makedoc \
	keymap_alldefs.h keymap_defs.h patchlist.c

ACLOCAL_AMFLAGS = -I m4

LDADD = @LIBOBJS@ @INTLLIBS@

OPS=@OPS@

flea:	muttbug.sh
	cp muttbug.sh flea
	chmod +x flea

smime_keys: $(srcdir)/smime_keys.pl
	cp $(srcdir)/smime_keys.pl smime_keys
	chmod +x smime_keys

Makefile: $(BUILT_SOURCES)

keymap_defs.h: $(OPS) $(srcdir)/gen_defs
	$(srcdir)/gen_defs $(OPS) > keymap_defs.h

keymap_alldefs.h: $(srcdir)/OPS $(srcdir)/OPS.PGP $(srcdir)/OPS.MIX $(srcdir)/OPS.CRYPT $(srcdir)/OPS.SMIME $(srcdir)/gen_defs
	$(srcdir)/gen_defs $(srcdir)/OPS $(srcdir)/OPS.PGP \
		$(srcdir)/OPS.MIX $(srcdir)/OPS.CRYPT $(srcdir)/OPS.SMIME \
			> keymap_alldefs.h

reldate:
	echo 'const char *ReleaseDate = "'`date +%Y-%m-%d`'";' > $(srcdir)/reldate.h

patchlist.c: $(srcdir)/PATCHES $(srcdir)/patchlist.sh
	$(srcdir)/patchlist.sh < $(srcdir)/PATCHES > patchlist.c

install-exec-local:
	if test -f $(DESTDIR)$(bindir)/mutt.dotlock && test -f $(DESTDIR)$(bindir)/mutt_dotlock ; then	\
		rm -f $(DESTDIR)$(bindir)/mutt.dotlock ;		\
		ln -sf $(DESTDIR)$(bindir)/mutt_dotlock $(DESTDIR)$(bindir)/mutt.dotlock ; \
	fi
	if test -f $(DESTDIR)$(bindir)/mutt_dotlock && test x@DOTLOCK_GROUP@ != x ; then \
		chgrp @DOTLOCK_GROUP@ $(DESTDIR)$(bindir)/mutt_dotlock && \
		chmod @DOTLOCK_PERMISSION@ $(DESTDIR)$(bindir)/mutt_dotlock || \
		{ echo "Can't fix mutt_dotlock's permissions!" >&2 ; exit 1 ; } \
	fi

install-data-local: Muttrc
	$(srcdir)/mkinstalldirs $(DESTDIR)$(sysconfdir)
	-if [ -f $(DESTDIR)$(pkgdatadir)/Muttrc ] ; then \
		mv $(DESTDIR)$(pkgdatadir)/Muttrc* $(DESTDIR)$(sysconfdir) ; \
	elif [ -f $(DESTDIR)$(pkgdatadir)/../Muttrc ] ; then \
	 	mv $(DESTDIR)$(pkgdatadir)/../Muttrc* $(DESTDIR)$(sysconfdir) ; \
	elif [ ! -f $(DESTDIR)$(sysconfdir)/Muttrc ] ; then \
		$(INSTALL) -m 644 $(srcdir)/Muttrc $(DESTDIR)$(sysconfdir) ; \
	fi
	-if [ ! -f $(DESTDIR)$(sysconfdir)/mime.types ]; then \
		$(INSTALL) -m 644 $(srcdir)/mime.types $(DESTDIR)$(sysconfdir); \
	fi


# Don't make this one ChangeLog - it's intended to be
# used manually.

pclean:
	cat /dev/null > $(top_srcdir)/PATCHES

check-security:
	(cd $(top_srcdir) && ./check_sec.sh)

commit-real:
	(cd $(top_srcdir) && cvs commit)

commit-changelog:
	(cd $(top_srcdir) && cvs commit -m "# changelog commit" ChangeLog)

commit: pclean check-security commit-real changelog commit-changelog

changelog:
	(cd $(top_srcdir); \
	cvs update ChangeLog; \
	sh ./mkchangelog.sh | fmt -c | cat  - ChangeLog > ChangeLog.$$$$ && mv ChangeLog.$$$$ ChangeLog; \
	$${VISUAL:-vi} ChangeLog)

ChangeLog: changelog

# kluge around automake problems.

dist-hook:
	-for file in $(BUILT_SOURCES) ; do rm -f $(distdir)/$$file ; done

mutt-dist:
	(cd $(srcdir) && ./build-release )

update-doc: stamp-doc-rc
	(cd doc && $(MAKE) update-doc)

Muttrc: stamp-doc-rc

stamp-doc-rc: $(srcdir)/init.h makedoc Muttrc.head
	-rm -f Muttrc stamp-doc-rc
	$(CPP) $(INCLUDES) $(DEFS) $(CPPFLAGS) -D_MAKEDOC -C \
		$(srcdir)/init.h | ./makedoc -c | cat Muttrc.head - > Muttrc
	touch stamp-doc-rc


.PHONY: commit pclean check-security commit-real commit-changelog
.PHONY: changelog ChangeLog
