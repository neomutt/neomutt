# $Id$

## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake --foreign

AUTOMAKE_OPTIONS = foreign
EXTRA_PROGRAMS = mutt_dotlock pgpring

if NEEDS_PGPEWRAP
bin_SCRIPTS = pgpewrap
else
bin_SCRIPTS =
endif

bin_PROGRAMS = mutt @DOTLOCK_TARGET@ @PGPAUX_TARGET@
mutt_SOURCES =	addrbook.c alias.c attach.c browser.c buffy.c color.c \
	commands.c complete.c compose.c copy.c curs_lib.c curs_main.c date.c \
	edit.c enter.c flags.c init.c filter.c from.c getdomain.c \
	handler.c hash.c hdrline.c headers.c help.c hook.c keymap.c \
	main.c mbox.c menu.c mh.c mx.c pager.c parse.c pattern.c \
	postpone.c query.c recvattach.c rfc822.c \
	rfc1524.c rfc2047.c score.c send.c sendlib.c signal.c sort.c \
	status.c system.c thread.c charset.c history.c lib.c muttlib.c

mutt_LDADD = @MUTT_LIB_OBJECTS@ @LIBOBJS@ $(INTLLIBS) 
mutt_DEPENDENCIES = @MUTT_LIB_OBJECTS@ @LIBOBJS@ $(INTLDEPS)

DEFS=-DSHAREDIR=\"$(sharedir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DBINDIR=\"$(bindir)\" -DHAVE_CONFIG_H=1
INCLUDES=-I. -I$(includedir)

non_us_sources = pgp.c pgpinvoke.c pgpkey.c pgplib.c sha1dgst.c \
	gnupgparse.c sha.h sha_locl.h \
	doc/language.txt doc/language50.txt OPS.PGP doc/PGP-Notes \
	OPS.MIX remailer.c remailer.h pgpewrap	\
	contrib/pgp2.rc contrib/pgp5.rc contrib/gpg.rc

EXTRA_mutt_SOURCES = pgp.c pgpinvoke.c pgpkey.c pgplib.c sha1dgst.c \
	gnupgparse.c resize.c dotlock.c pop.c imap.c socket.c
	remailer.c remailer.h browser.h

EXTRA_DIST = COPYRIGHT GPL OPS OPS.PGP TODO configure acconfig.h attach.h \
	buffy.h charset.h config.h copy.h dotlock.h functions.h gen_defs \
	globals.h hash.h history.h imap.h init.h keymap.h \
	mailbox.h mapping.h mime.h mutt.h mutt_curses.h mutt_menu.h  \
	mutt_regex.h mutt_socket.h mx.h pager.h parse.h pgp.h protos.h  \
	reldate.h rfc1524.h rfc2047.h rfc822.h sha.h sha_locl.h	\
	sort.h mime.types VERSION prepare _regex.h OPS.MIX	\
	README.SECURITY remailer.c remailer.h browser.h Muttrc.in \
	lib.h extlib.c pgpewrap pgplib.h

BUILT_SOURCES = mutt_dotlock.c keymap_defs.h

mutt_dotlock_SOURCES = mutt_dotlock.c
mutt_dotlock_LDADD = @LIBOBJS@
mutt_dotlock_DEPENDENCIES = @LIBOBJS@

pgpring_SOURCES = pgppubring.c pgplib.c lib.c extlib.c sha1dgst.c
pgpring_LDADD = @LIBOBJS@
pgpring_DEPENDENCIES = @LIBOBJS@

mutt_dotlock.c: dotlock.c
	cp $(srcdir)/dotlock.c mutt_dotlock.c

CLEANFILES = mutt_dotlock.c

ACLOCAL_AMFLAGS = -I m4

LDADD = @LIBOBJS@ @INTLLIBS@

SUBDIRS = m4 po intl doc contrib $(ADD_SUBDIRS)

OPS=@OPS@

keymap_defs.h: $(OPS) $(srcdir)/gen_defs
	$(srcdir)/gen_defs $(OPS) > keymap_defs.h

reldate:
	echo 'const char *ReleaseDate = "'`date +%Y-%m-%d`'";' > $(srcdir)/reldate.h

install-exec-local:
	if test -f $(DESTDIR)$(bindir)/mutt.dotlock && test -f $(DESTDIR)$(bindir)/mutt_dotlock ; then	\
		rm -f $(DESTDIR)$(bindir)/mutt.dotlock ;		\
		ln -sf $(DESTDIR)$(bindir)/mutt_dotlock $(DESTDIR)$(bindir)/mutt.dotlock ; \
	fi
	if test -f $(bindir)/mutt_dotlock && test x@DOTLOCK_GROUP@ != x ; then \
		chgrp @DOTLOCK_GROUP@ $(DESTDIR)$(bindir)/mutt_dotlock ; \
		chmod @DOTLOCK_PERMISSION@  $(DESTDIR)$(bindir)/mutt_dotlock ; \
	fi

install-data-local:
	$(srcdir)/mkinstalldirs $(DESTDIR)$(sharedir)
	$(srcdir)/mkinstalldirs $(DESTDIR)$(sysconfdir)
	-if [ -f $(DESTDIR)$(sharedir)/Muttrc ] ; then \
		mv $(DESTDIR)$(sharedir)/Muttrc $(DESTDIR)$(sysconfdir) ; \
	elif [ -f $(DESTDIR)$(sharedir)/../Muttrc ] ; then \
	 	mv $(DESTDIR)$(sharedir)/../Muttrc $(DESTDIR)$(sysconfdir) ; \
	elif [ ! -f $(DESTDIR)$(sysconfdir)/Muttrc ] ; then \
		$(INSTALL) -m 644 $(srcdir)/Muttrc $(DESTDIR)$(sysconfdir) ; \
	fi
	-if [ ! -f $(DESTDIR)$(sysconfdir)/mime.types ]; then \
		$(INSTALL) -m 644 $(srcdir)/mime.types $(DESTDIR)$(sysconfdir); \
	fi


# Don't make this one ChangeLog - it's intended to be
# used manually.

changelog:
	rcs2log | cat - ChangeLog > ChangeLog.$$$$ && mv ChangeLog.$$$$ ChangeLog
	$(VISUAL) ChangeLog

# kluge aroud automake problems. *grrr*

dist-hook:
	-for file in $(BUILT_SOURCES) ; do rm -f $(distdir)/$$file ; done
	-rm -f $(distdir)/config.h
	touch $(distdir)/'$$output.in'

utput.in:
	touch utput.in

us-distdir: distdir dist-hook
	mv $(distdir) $(distdir)-us
	find $(distdir)-us -type f -print | while read f ; do 		\
		cp $$f `dirname $$f`/_`basename $$f`.$$$$ &&   		\
		rm $$f && mv `dirname $$f`/_`basename $$f`.$$$$ $$f ;	\
	done


us-dist: us-distdir
	for file in $(distdir)-us/*.c $(distdir)-us/*.h ; do \
		$(srcdir)/reap.pl _PGPPATH < $$file | $(srcdir)/reap.pl MIXMASTER > $$file.n && rm $$file && mv $$file.n $$file ; \
	done
	for file in $(non_us_sources) ; do \
		echo "/* This file is only available in the international mutt version */" \
			> $(distdir)-us/$$file ; \
	done
	echo "This version of mutt may be exported from the US.">  $(distdir)-us/EXPORTABLE
	-chmod -R a+r $(distdir)-us
	GZIP=$(GZIP) $(TAR) chozf $(distdir)-us.tar.gz $(distdir)-us
	-rm -rf $(distdir)-us
