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
	
mutt_LDADD = @MUTT_LIB_OBJECTS@
mutt_DEPENDENCIES = @MUTT_LIB_OBJECTS@

EXTRA_mutt_SOURCES = pgp.c pgpinvoke.c pgpkey.c pgppubring.c sha1dgst.c \
	gnupgparse.c resize.c snprintf.c dotlock.c pop.c imap.c socket.c

BUILT_SOURCES = mutt_dotlock.c

mutt_dotlock.c: dotlock.c
	cp dotlock.c mutt_dotlock.c

LDADD = @LIBOBJS@ @INTLLIBS@

SUBDIRS = doc intl po

OPS=$(srcdir)/OPS $(srcdir)/OPS.PGP

keymap_defs.h: $(OPS)
	$(srcdir)/gen_defs $(OPS) > keymap_defs.h

reldate:
	echo 'const char *ReleaseDate = "'`date +%Y-%m-%d`'";' > $(srcdir)/reldate.h
