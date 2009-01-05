## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake --foreign

AUTOMAKE_OPTIONS = 1.6 foreign
EXTRA_PROGRAMS = mutt_dotlock pgpring pgpewrap mutt_md5

if BUILD_IMAP
IMAP_SUBDIR = imap
IMAP_INCLUDES = -I$(top_srcdir)/imap
endif

SUBDIRS = m4 po intl doc contrib $(IMAP_SUBDIR)

bin_SCRIPTS = muttbug flea @SMIMEAUX_TARGET@

BUILT_SOURCES = keymap_defs.h patchlist.c reldate.h hcversion.h

bin_PROGRAMS = mutt @DOTLOCK_TARGET@ @PGPAUX_TARGET@
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
	url.c ascii.c crypt-mod.c crypt-mod.h

nodist_mutt_SOURCES = $(BUILT_SOURCES)

mutt_LDADD = @MUTT_LIB_OBJECTS@ @LIBOBJS@ $(LIBIMAP) $(MUTTLIBS) \
	$(INTLLIBS) $(LIBICONV)  $(GPGME_LIBS)

mutt_DEPENDENCIES = @MUTT_LIB_OBJECTS@ @LIBOBJS@ $(LIBIMAPDEPS) \
	$(INTLDEPS)

DEFS=-DPKGDATADIR=\"$(pkgdatadir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DBINDIR=\"$(bindir)\" -DMUTTLOCALEDIR=\"$(datadir)/locale\" \
	-DHAVE_CONFIG_H=1

AM_CPPFLAGS=-I. -I$(top_srcdir) $(IMAP_INCLUDES) $(GPGME_CFLAGS) -Iintl

CPPFLAGS=@CPPFLAGS@

EXTRA_mutt_SOURCES = account.c bcache.c crypt-gpgme.c crypt-mod-pgp-classic.c \
	crypt-mod-pgp-gpgme.c crypt-mod-smime-classic.c \
	crypt-mod-smime-gpgme.c dotlock.c gnupgparse.c hcache.c md5.c \
	mutt_idna.c mutt_sasl.c mutt_socket.c mutt_ssl.c mutt_ssl_gnutls.c \
	mutt_tunnel.c pgp.c pgpinvoke.c pgpkey.c pgplib.c pgpmicalg.c \
	pgppacket.c pop.c pop_auth.c pop_lib.c remailer.c resize.c sha1.c \
	smime.c smtp.c utf8.c wcwidth.c \
	bcache.h browser.h hcache.h mbyte.h mutt_idna.h remailer.h url.h

EXTRA_DIST = COPYRIGHT GPL OPS OPS.PGP OPS.CRYPT OPS.SMIME TODO UPDATING \
	configure account.h \
	attach.h buffy.h charset.h copy.h crypthash.h dotlock.h functions.h gen_defs \
	globals.h hash.h history.h init.h keymap.h mutt_crypt.h \
	mailbox.h mapping.h md5.h mime.h mutt.h mutt_curses.h mutt_menu.h \
	mutt_regex.h mutt_sasl.h mutt_socket.h mutt_ssl.h mutt_tunnel.h \
	mx.h pager.h pgp.h pop.h protos.h rfc1524.h rfc2047.h \
	rfc2231.h rfc822.h rfc3676.h sha1.h sort.h mime.types VERSION prepare \
	_regex.h OPS.MIX README.SECURITY remailer.c remailer.h browser.h \
	mbyte.h lib.h extlib.c pgpewrap.c smime_keys.pl pgplib.h \
	README.SSL smime.h \
	muttbug pgppacket.h depcomp ascii.h BEWARE PATCHES patchlist.sh \
	ChangeLog mkchangelog.sh mutt_idna.h \
	snprintf.c regex.c crypt-gpgme.h hcachever.sh.in

EXTRA_SCRIPTS = smime_keys

mutt_dotlock_SOURCES = mutt_dotlock.c
mutt_dotlock_LDADD = @LIBOBJS@
mutt_dotlock_DEPENDENCIES = @LIBOBJS@

pgpring_SOURCES = pgppubring.c pgplib.c lib.c extlib.c sha1.c md5.c pgppacket.c ascii.c
pgpring_LDADD = @LIBOBJS@ $(INTLLIBS) 
pgpring_DEPENDENCIES = @LIBOBJS@ $(INTLDEPS)

mutt_md5_SOURCES = md5.c
mutt_md5_CFLAGS = -DMD5UTIL
mutt_md5_LDADD =

noinst_PROGRAMS = $(MUTT_MD5)

mutt_dotlock.c: dotlock.c
	cp $(srcdir)/dotlock.c mutt_dotlock.c

CLEANFILES = mutt_dotlock.c keymap_alldefs.h $(BUILT_SOURCES)

DISTCLEANFILES= flea smime_keys

ACLOCAL_AMFLAGS = -I m4

LDADD = @LIBOBJS@ @INTLLIBS@

flea:	muttbug.sh
	cp muttbug.sh flea
	chmod +x flea

smime_keys: $(srcdir)/smime_keys.pl
	cp $(srcdir)/smime_keys.pl smime_keys
	chmod +x smime_keys

keymap_defs.h: $(OPS) $(srcdir)/gen_defs
	$(srcdir)/gen_defs $(OPS) > keymap_defs.h

keymap_alldefs.h: $(srcdir)/OPS $(srcdir)/OPS.PGP $(srcdir)/OPS.MIX $(srcdir)/OPS.CRYPT $(srcdir)/OPS.SMIME $(srcdir)/gen_defs
	rm -f $@
	$(srcdir)/gen_defs $(srcdir)/OPS $(srcdir)/OPS.PGP \
		$(srcdir)/OPS.MIX $(srcdir)/OPS.CRYPT $(srcdir)/OPS.SMIME \
			> keymap_alldefs.h

reldate.h: $(srcdir)/ChangeLog
	echo 'const char *ReleaseDate = "'`head -n 1 $(srcdir)/ChangeLog | LC_ALL=C cut -d ' ' -f 1`'";' > reldate.h.tmp; \
	cmp -s reldate.h.tmp reldate.h || cp reldate.h.tmp reldate.h; \
	rm reldate.h.tmp

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
	if test -f $(DESTDIR)$(bindir)/mutt_dotlock && test x@DOTLOCK_GROUP@ != x ; then \
		chgrp @DOTLOCK_GROUP@ $(DESTDIR)$(bindir)/mutt_dotlock && \
		chmod @DOTLOCK_PERMISSION@ $(DESTDIR)$(bindir)/mutt_dotlock || \
		{ echo "Can't fix mutt_dotlock's permissions!" >&2 ; exit 1 ; } \
	fi

install-data-local:
	$(srcdir)/mkinstalldirs $(DESTDIR)$(sysconfdir)
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

commit:
	@echo "make commit is obsolete; use hg-commit"; false

update-changelog:
	(cd $(top_srcdir); \
	sh ./mkchangelog.sh | cat  - ChangeLog > ChangeLog.$$$$ && mv ChangeLog.$$$$ ChangeLog; \
	$${VISUAL:-vi} ChangeLog)

mutt-dist:
	(cd $(srcdir) && ./build-release )

update-doc:
	(cd doc && $(MAKE) update-doc)

.PHONY: commit pclean check-security
