## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake --foreign

AUTOMAKE_OPTIONS = foreign
EXTRA_PROGRAMS = mutt_dotlock pgpring makedoc

if BUILD_CHARMAPS
CHARMAP_SUBDIR = charmaps
endif

if BUILD_IMAP
IMAP_SUBDIR = imap
endif

SUBDIRS = m4 po intl doc contrib $(CHARMAP_SUBDIR) $(IMAP_SUBDIR)

if NEEDS_PGPEWRAP
bin_SCRIPTS = pgpewrap
else
bin_SCRIPTS =
endif

BUILT_SOURCES = keymap_defs.h

bin_PROGRAMS = mutt @DOTLOCK_TARGET@ @PGPAUX_TARGET@
mutt_SOURCES = $(BUILT_SOURCES) \
	addrbook.c alias.c attach.c base64.c browser.c buffy.c color.c \
	commands.c complete.c compose.c copy.c curs_lib.c curs_main.c date.c \
	edit.c enter.c flags.c init.c filter.c from.c getdomain.c \
	handler.c hash.c hdrline.c headers.c help.c hook.c keymap.c \
	main.c mbox.c menu.c mh.c mx.c pager.c parse.c pattern.c \
	postpone.c query.c recvattach.c \
	rfc822.c rfc1524.c rfc2047.c rfc2231.c \
	score.c send.c sendlib.c signal.c sort.c \
	status.c system.c thread.c charset.c history.c lib.c \
	muttlib.c editmsg.c

mutt_LDADD = @MUTT_LIB_OBJECTS@ @LIBOBJS@ $(LIBIMAP) $(GSSLIBS) $(INTLLIBS) 
mutt_DEPENDENCIES = @MUTT_LIB_OBJECTS@ @LIBOBJS@ $(LIBIMAPDEPS) $(INTLDEPS)

makedoc_SOURCES = makedoc.c

CPP=@CPP@

DEFS=-DSHAREDIR=\"$(sharedir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-DBINDIR=\"$(bindir)\" -DHAVE_CONFIG_H=1
INCLUDES=-I. -I$(includedir)

non_us_sources = pgp.c pgpinvoke.c pgpkey.c pgplib.c sha1dgst.c \
	gnupgparse.c sha.h sha_locl.h \
	doc/language.txt doc/language50.txt OPS.PGP doc/PGP-Notes.txt \
	OPS.MIX remailer.c remailer.h pgpewrap	\
	contrib/pgp2.rc contrib/pgp5.rc contrib/gpg.rc

EXTRA_mutt_SOURCES = pgp.c pgpinvoke.c pgpkey.c pgplib.c sha1dgst.c \
	gnupgparse.c resize.c dotlock.c pop.c remailer.c remailer.h browser.h

EXTRA_DIST = COPYRIGHT GPL OPS OPS.PGP TODO configure acconfig.h attach.h \
	buffy.h charset.h copy.h dotlock.h functions.h gen_defs \
	globals.h hash.h history.h init.h keymap.h \
	mailbox.h mapping.h mime.h mutt.h mutt_curses.h mutt_menu.h  \
	mutt_regex.h mx.h pager.h parse.h pgp.h protos.h  \
	reldate.h rfc1524.h rfc2047.h rfc2231.h rfc822.h sha.h sha_locl.h \
	sort.h mime.types VERSION prepare _regex.h OPS.MIX	\
	README.SECURITY remailer.c remailer.h browser.h \
	lib.h extlib.c pgpewrap pgplib.h Muttrc.head Muttrc \
	makedoc.c stamp-doc-rc


mutt_dotlock_SOURCES = mutt_dotlock.c
mutt_dotlock_LDADD = @LIBOBJS@
mutt_dotlock_DEPENDENCIES = @LIBOBJS@

pgpring_SOURCES = pgppubring.c pgplib.c lib.c extlib.c sha1dgst.c
pgpring_LDADD = @LIBOBJS@ $(INTLLIBS) 
pgpring_DEPENDENCIES = @LIBOBJS@ $(INTLDEPS)


mutt_dotlock.c: dotlock.c
	cp $(srcdir)/dotlock.c mutt_dotlock.c

CLEANFILES = mutt_dotlock.c stamp-doc-rc

ACLOCAL_AMFLAGS = -I m4

LDADD = @LIBOBJS@ @INTLLIBS@

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
	if test -f $(DESTDIR)$(bindir)/mutt_dotlock && test x@DOTLOCK_GROUP@ != x ; then \
		chgrp @DOTLOCK_GROUP@ $(DESTDIR)$(bindir)/mutt_dotlock && \
		chmod @DOTLOCK_PERMISSION@ $(DESTDIR)$(bindir)/mutt_dotlock || \
		{ echo "Can't fix mutt_dotlock's permissions!" >&2 ; exit 1 ; } \
	fi

install-data-local: Muttrc
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

# kluge around automake problems.

dist-hook:
	-for file in $(BUILT_SOURCES) ; do rm -f $(distdir)/$$file ; done

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

test-tlr:
	test $$HOSTNAME = sobolev && test $$LOGNAME = roessler

actual-dist: update-doc
	# Note: We can't put these into the dependencies line, since
	# make will over-optimize some of the distdir targets.
	$(MAKE) us-dist
	$(MAKE) dist
	mv $(distdir).tar.gz $(distdir)i.tar.gz
	mv $(distdir)-us.tar.gz $(distdir).tar.gz
	$(MAKE) do-sign
	$(MAKE) do-upload

do-sign: test-tlr
	pgp -sb $(distdir)i.tar.gz && chmod 644 $(distdir)i.tar.gz.asc
	pgp -sb $(distdir).tar.gz && chmod 644 $(distdir).tar.gz.asc

do-upload: test-tlr
	if ! ssh-add -l | grep -q roessler@ ; then  \
		ssh-add ; \
	fi
	@echo "Please connect to the Net and press any key."
	@read dummy
	scp $(distdir){,i}.tar.gz{,.asc}			\
		sigtrap.guug.de:/home/ftp/pub/mutt/devel/ 	\
		|| echo "UPLOAD FAILED.  Please upload manually!"
	cvs commit -m "automatic post-release commit for $(distdir)"
	cvs tag `echo $(distdir)-rel | tr . -`
	cvs update
	$$HOME/bin/upload-mutt
	mutt -s "[Announce] $(distdir) is out" mutt-dev@mutt.org

bump-version: test-tlr
	-rm -f $(srcdir)/VERSION
	( cd $(srcdir) && cvs update VERSION )
	awk -F . '{printf("%d.%d.%d\n", $$1, $$2, $$3 + 1);}' $(srcdir)/VERSION > _VERSION.$$$$  \
		&& mv _VERSION.$$$$ $(srcdir)/VERSION
	$(VISUAL) $(srcdir)/VERSION

mutt-dist: bump-version reldate changelog
	touch $(srcdir)/configure.in
	$(MAKE) config.status
	./config.status
	$(MAKE) actual-dist

update-doc: stamp-doc-rc
	(cd doc && $(MAKE) update-doc)

Muttrc: stamp-doc-rc

stamp-doc-rc: $(srcdir)/init.h makedoc Muttrc.head
	$(CPP) $(INCLUDES) $(DEFS) $(CPPFLAGS) -D_MAKEDOC -C \
		$(srcdir)/init.h | ./makedoc -c | cat Muttrc.head - \
		> Muttrc
	touch stamp-doc-rc

