## Process this file with automake to produce Makefile.in
## Use aclocal; automake --foreign

EXTRA_PROGRAMS = mutt_dotlock

bin_PROGRAMS = mutt @DOTLOCK_TARGET@
mutt_SOURCES =	addrbook.c alias.c attach.c browser.c buffy.c color.c \
	commands.c complete.c compose.c copy.c curs_lib.c curs_main.c date.c \
	edit.c enter.c flags.c init.c filter.c from.c getdomain.c \
	handler.c hash.c hdrline.c headers.c help.c hook.c keymap.c \
	main.c mbox.c menu.c mh.c mx.c pager.c parse.c pattern.c \
	postpone.c query.c recvattach.c rfc822.c \
	rfc1524.c rfc2047.c score.c send.c sendlib.c signal.c sort.c \
	status.c system.c thread.c charset.c history.c lib.c

mutt_LDADD = @MUTT_LIB_OBJECTS@ $(INTLLIBS)
mutt_DEPENDENCIES = @MUTT_LIB_OBJECTS@ $(INTLDEPS)

EXTRA_mutt_SOURCES = pgp.c pgpinvoke.c pgpkey.c pgppubring.c sha1dgst.c \
	gnupgparse.c resize.c snprintf.c dotlock.c pop.c imap.c socket.c

EXTRA_DIST = COPYRIGHT GPL OPS OPS.PGP TODO acconfig.h attach.h \
	buffy.h charset.h config.h copy.h dotlock.h functions.h gen_defs \
	globals.h hash.h history.h imap.h init.h keymap.h \
	mailbox.h mapping.h mime.h mutt.h mutt_curses.h mutt_menu.h  \
	mutt_regex.h mutt_socket.h mx.h pager.h parse.h pgp.h protos.h  \
	reldate.h rfc1524.h rfc2047.h rfc822.h sha.h sha_locl.h sort.h mime.types

BUILT_SOURCES = mutt_dotlock.c keymap_defs.h

mutt_dotlock.c: dotlock.c
	cp dotlock.c mutt_dotlock.c

CLEANFILES = mutt_dotlock.c

LDADD = @LIBOBJS@ @INTLLIBS@

SUBDIRS = doc intl po rx contrib charsets

OPS=$(srcdir)/OPS $(srcdir)/OPS.PGP

keymap_defs.h: $(OPS)
	$(srcdir)/gen_defs $(OPS) > keymap_defs.h

reldate:
	echo 'const char *ReleaseDate = "'`date +%Y-%m-%d`'";' > $(srcdir)/reldate.h

install-exec-local:
	if test -f $(bindir)/mutt.dotlock && test -f $(bindir)/mutt_dotlock ; then	\
		rm -f $(bindir)/mutt.dotlock ;		\
		ln -sf $(bindir)/mutt_dotlock $(bindir)/mutt.dotlock ; \
	fi
	if test -f $(bindir)/mutt_dotlock && test x@DOTLOCK_PERMISSION@ != x ; then \
		chgrp @DOTLOCK_GROUP@ $(bindir)/mutt_dotlock ; \
		chmod @DOTLOCK_PERMISSION@  $(bindir)/mutt_dotlock ; \
	fi

install-data-local:
	-if [ ! -f $(sharedir)/Muttrc ]; then \
		$(srcdir)/mkinstalldirs $(sharedir); \
		$(INSTALL) -m 644 $(srcdir)/Muttrc $(sharedir); \
	fi
	-if [ ! -f $(sharedir)/mime.types ]; then \
		$(INSTALL) -m 644 $(srcdir)/mime.types $(sharedir); \
	fi


# Don't make this one ChangeLog - it's intended to be
# used manually.

changelog:
	rcs2log | cat - ChangeLog > ChangeLog.$$$$ && mv ChangeLog.$$$$ ChangeLog
	$(VISUAL) ChangeLog

dist-hook:
	-for file in $(BUILT_SOURCES) ; do rm -f $(distdir)/$$file ; done
	-rm -f $(distdir)/config.h
