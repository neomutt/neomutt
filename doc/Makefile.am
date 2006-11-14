# $Id$

subdir = doc

DSLROOT = @DSLROOT@

DEFS = -DSYSCONFDIR=\"$(sysconfdir)\" -DBINDIR=\"$(bindir)\" -DHAVE_CONFIG_H=1
AM_CPPFLAGS = -I. -I.. -I$(includedir) -I$(top_srcdir)

MAKEDOC_CPP = $(CPP) $(AM_CPPFLAGS) $(DEFS) $(CPPFLAGS) -D_MAKEDOC -C

EXTRA_DIST = dotlock.man		\
	muttbug.man			\
	mutt.man			\
	PGP-Notes.txt			\
	applying-patches.txt		\
	devel-notes.txt			\
	muttrc.man.head			\
	muttrc.man.tail			\
	mbox.man			\
	mmdf.man			\
	manual.xml.head			\
	manual.xml.tail			\
	instdoc.sh.in			\
	patch-notes.txt			\
	smime-notes.txt			\
	html.xsl chunk.xsl $(BUILT_DISTFILES)

HTML_DOCFILES = manual.html index.html intro.html gettingstarted.html \
	configuration.html mimesupport.html advancedusage.html \
	tuning.html reference.html miscellany.html

BUILT_DISTFILES = stamp-doc-xml stamp-doc-chunked manual.txt $(HTML_DOCFILES)

srcdir_DOCFILES = PGP-Notes.txt applying-patches.txt	\
	devel-notes.txt patch-notes.txt smime-notes.txt

topsrcdir_DOCFILES = COPYRIGHT GPL INSTALL ChangeLog	\
	ChangeLog.old					\
	README NEWS TODO README.SECURITY README.SSL 

all: ../makedoc$(EXEEXT)
	$(MAKE) makedoc-all

makedoc-all: mutt.1 muttrc.man manual.html stamp-doc-chunked manual.txt

install-data-local: makedoc-all instdoc
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(mandir)/man1
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(mandir)/man5
	./instdoc mutt.1 $(DESTDIR)$(mandir)/man1/mutt.1
	./instdoc $(srcdir)/muttbug.man $(DESTDIR)$(mandir)/man1/flea.1
	./instdoc $(srcdir)/muttbug.man $(DESTDIR)$(mandir)/man1/muttbug.1
	./instdoc $(srcdir)/dotlock.man \
		$(DESTDIR)$(mandir)/man1/mutt_dotlock.1
	./instdoc muttrc.man $(DESTDIR)$(mandir)/man5/muttrc.5
	./instdoc $(srcdir)/mbox.man $(DESTDIR)$(mandir)/man5/mbox.5
	./instdoc $(srcdir)/mmdf.man $(DESTDIR)$(mandir)/man5/mmdf.5
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(docdir)
	for f in $(topsrcdir_DOCFILES) ; do \
		$(INSTALL) -m 644 $(top_srcdir)/$$f $(DESTDIR)$(docdir) ; \
	done
	for f in $(srcdir_DOCFILES) ; do \
		$(INSTALL) -m 644 $(srcdir)/$$f $(DESTDIR)$(docdir) ; \
	done
	-$(INSTALL) -m 644 manual.txt $(DESTDIR)$(docdir)
	-for f in $(HTML_DOCFILES) ; do \
		$(INSTALL) -m 644 $$f $(DESTDIR)$(docdir) ; \
	done

uninstall-local:
	for f in mutt.1 mutt_dotlock.1 flea.1 muttbug.1 ; do \
		rm -f $(DESTDIR)$(mandir)/man1/$$f ; \
	done
	for f in muttrc.5 mbox.5 mmdf.5 ; do \
		rm -f $(DESTDIR)$(mandir)/man5/$$f ; \
	done
	for f in $(srcdir_DOCFILES) $(topsrcdir_DOCFILES) $(HTML_DOCFILES) ; \
	do \
		rm -f $(DESTDIR)$(docdir)/$$f ; \
	done
	-rm -f $(DESTDIR)$(docdir)/manual.txt

check:
manual.txt: manual.html
	-lynx -dump -nolist -with_backspaces $^ > $@ || w3m -dump $^ > $@

manual.html: $(srcdir)/html.xsl stamp-doc-xml
	-xsltproc --nonet -o $@ $(srcdir)/html.xsl manual.xml

stamp-doc-chunked: $(srcdir)/chunk.xsl stamp-doc-xml
	-xsltproc --nonet $(srcdir)/chunk.xsl manual.xml
	touch stamp-doc-chunked

manual.pdf: manual.tex
	-if test -f manual.tex; then pdfjadetex manual.tex; fi

manual.tex: stamp-doc-xml
	-if test -n "$(DSLROOT)"; then \
	  openjade -t tex -D $(DSLROOT) -d print/docbook.dsl dtds/decls/xml.dcl manual.xml; \
	fi

clean-local:
	rm -f *~ *.html *.orig *.rej  stamp-doc-* *.ps mutt.1 muttrc.man
	rm -f *.aux *.log *.tex *.out

DISTCLEANFILES=manual.txt manual.html manual.xml manual.pdf html instdoc

instdoc: instdoc.sh
	cat instdoc.sh > instdoc
	chmod a+x instdoc

../makedoc$(EXEEXT): $(top_srcdir)/makedoc.c
	(cd .. && $(MAKE) makedoc$(EXEEXT))

../reldate.h:
	(cd .. && $(MAKE) reldate.h)

update-doc: stamp-doc-xml stamp-doc-chunked manual.txt manual.html

muttrc.man: ../makedoc$(EXEEXT) $(top_srcdir)/init.h muttrc.man.head muttrc.man.tail
	$(MAKEDOC_CPP) $(top_srcdir)/init.h | ../makedoc -m | 	    \
		cat $(srcdir)/muttrc.man.head - $(srcdir)/muttrc.man.tail\
		> muttrc.man

EDIT = sed -e 's,@sysconfdir\@,$(sysconfdir),g' \
	-e 's,@bindir\@,$(bindir),g' \
	-e 's,@docdir\@,$(docdir),g'

mutt.1: $(srcdir)/mutt.man
	$(EDIT) $(srcdir)/mutt.man > $@

stamp-doc-xml: $(top_srcdir)/makedoc.c $(top_srcdir)/init.h manual.xml.head manual.xml.tail \
               $(top_srcdir)/VERSION ../reldate.h
	$(MAKE) ../makedoc$(EXEEXT)
	( sed -e "s/@VERSION\@/`cat $(top_srcdir)/VERSION` (`cut -d\\\" -f2 ../reldate.h`)/" $(srcdir)/manual.xml.head ;\
	  $(MAKEDOC_CPP) $(top_srcdir)/init.h | ../makedoc -s ) |      \
		cat - $(srcdir)/manual.xml.tail > manual.xml
	touch stamp-doc-xml
