## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

subdir = doc

DEFS = -DSYSCONFDIR=\"$(sysconfdir)\" -DBINDIR=\"$(bindir)\"
AM_CPPFLAGS = -I. -I.. -I$(includedir) -I$(top_srcdir)

MAKEDOC_CPP = $(CPP) $(AM_CPPFLAGS) $(DEFS) $(CPPFLAGS) -D_MAKEDOC -C

noinst_PROGRAMS = makedoc

EXTRA_DIST = smime_keys.man             \
	mutt.man			\
	pgpewrap.man			\
	pgpring.man			\
	PGP-Notes.txt			\
	gen-map-doc			\
	muttrc.man.head			\
	muttrc.man.tail			\
	mbox.man			\
	mmdf.man			\
	manual.xml.head			\
	manual.xml.tail			\
	instdoc.sh.in			\
	smime-notes.txt			\
	Muttrc Muttrc.head \
	makedoc.c makedoc_defs.h 	\
	mutt.css mutt.xsl html.xsl chunk.xsl \
	$(BUILT_DISTFILES) neomutt-syntax.vim \
	mutt-1.7.0-syntax.vim

CHUNKED_DOCFILES = intro.html gettingstarted.html \
	configuration.html mimesupport.html advancedusage.html \
	optionalfeatures.html security.html tuning.html reference.html miscellany.html

HTML_DOCFILES = manual.html index.html $(CHUNKED_DOCFILES)

BUILT_DISTFILES = manual.txt $(HTML_DOCFILES)

srcdir_DOCFILES = PGP-Notes.txt smime-notes.txt

topsrcdir_DOCFILES = CODE_OF_CONDUCT.md COPYRIGHT LICENSE.md INSTALL \
	ChangeLog.md README.SSL README.md

all: makedoc-all

makedoc-all: mutt.1 smime_keys.1 muttrc.man manual.html Muttrc index.html $(CHUNKED_DOCFILES) manual.txt pgpewrap.1 pgpring.1 neomutt-syntax.vim

install-data-local: makedoc-all instdoc
	$(MKDIR_P) $(DESTDIR)$(mandir)/man1
	$(MKDIR_P) $(DESTDIR)$(mandir)/man5
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	./instdoc mutt.1 $(DESTDIR)$(mandir)/man1/mutt.1
	./instdoc smime_keys.1 $(DESTDIR)$(mandir)/man1/smime_keys.1
	./instdoc pgpewrap.1 $(DESTDIR)$(mandir)/man1/pgpewrap.1
	./instdoc pgpring.1 $(DESTDIR)$(mandir)/man1/pgpring.1
	./instdoc muttrc.man $(DESTDIR)$(mandir)/man5/muttrc.5
	./instdoc $(srcdir)/mbox.man $(DESTDIR)$(mandir)/man5/mbox.5
	./instdoc $(srcdir)/mmdf.man $(DESTDIR)$(mandir)/man5/mmdf.5
	$(MKDIR_P) $(DESTDIR)$(docdir)
	for f in $(topsrcdir_DOCFILES) ; do \
		$(INSTALL) -m 644 $(top_srcdir)/$$f $(DESTDIR)$(docdir) ; \
	done
	for f in $(srcdir_DOCFILES) ; do \
		$(INSTALL) -m 644 $(srcdir)/$$f $(DESTDIR)$(docdir) ; \
	done
	-$(INSTALL) -m 644 $(srcdir)/manual.txt $(DESTDIR)$(docdir)
	-for f in $(HTML_DOCFILES) ; do \
		$(INSTALL) -m 644 $(srcdir)/$$f $(DESTDIR)$(docdir) ; \
	done
	$(INSTALL) -m 644 Muttrc $(DESTDIR)$(sysconfdir)/Muttrc.dist
	-if [ -f $(DESTDIR)$(pkgdatadir)/Muttrc ] ; then \
		mv $(DESTDIR)$(pkgdatadir)/Muttrc* $(DESTDIR)$(sysconfdir) ; \
	elif [ -f $(DESTDIR)$(pkgdatadir)/../Muttrc ] ; then \
	 	mv $(DESTDIR)$(pkgdatadir)/../Muttrc* $(DESTDIR)$(sysconfdir) ; \
	elif [ ! -f $(DESTDIR)$(sysconfdir)/Muttrc ] ; then \
		$(INSTALL) -m 644 Muttrc $(DESTDIR)$(sysconfdir) ; \
	fi
	-$(INSTALL) -m 644 $(srcdir)/neomutt-syntax.vim $(DESTDIR)$(docdir)

uninstall-local:
	for f in mutt.1 smime_keys.1 pgpewrap.1 pgpring.1; do \
		rm -f $(DESTDIR)$(mandir)/man1/$$f ; \
	done
	for f in muttrc.5 mbox.5 mmdf.5 ; do \
		rm -f $(DESTDIR)$(mandir)/man5/$$f ; \
	done
	for f in $(srcdir_DOCFILES) $(topsrcdir_DOCFILES) $(HTML_DOCFILES); \
	do \
		rm -f $(DESTDIR)$(docdir)/$$f ; \
	done
	-rm -f $(DESTDIR)$(docdir)/manual.txt
	for i in Muttrc ; do \
	  if cmp -s $(DESTDIR)$(sysconfdir)/$$i.dist $(DESTDIR)$(sysconfdir)/$$i ; then \
		rm $(DESTDIR)$(sysconfdir)/$$i ; \
	  fi ; \
	  rm $(DESTDIR)$(sysconfdir)/$${i}.dist ; \
	done
	rm -f $(DESTDIR)$(docdir)/neomutt-syntax.vim

check:
manual.txt: manual.html
	-LC_ALL=C w3m -dump -O UTF8 manual.html > $@ || \
	LC_ALL=C lynx -dump -nolist -with_backspaces -display_charset=us-ascii manual.html > $@ || \
	LC_ALL=C elinks -dump -no-numbering -no-references manual.html | sed -e 's,\\001, ,g' > $@

Muttrc: $(top_srcdir)/init.h makedoc$(EXEEXT) $(srcdir)/Muttrc.head
	sed -e 's,[@]docdir[@],$(docdir),' $(srcdir)/Muttrc.head > Muttrc
	$(CPP) $(AM_CPPFLAGS) $(DEFS) $(CPPFLAGS) -D_MAKEDOC -C $(top_srcdir)/init.h | ./makedoc$(EXEEXT) -c >> Muttrc

manual.html: manual.xml $(srcdir)/html.xsl $(srcdir)/mutt.xsl $(srcdir)/mutt.css
	-xsltproc --nonet -o $@ $(srcdir)/html.xsl manual.xml

$(CHUNKED_DOCFILES): index.html

index.html: $(srcdir)/chunk.xsl $(srcdir)/mutt.xsl manual.xml $(srcdir)/mutt.css
	-xsltproc --nonet $(srcdir)/chunk.xsl manual.xml > /dev/null 2>&1

validate: manual.xml
	xmllint --noout --noblanks --postvalid $<

spellcheck:
	-aspell -d american --mode=sgml  --encoding=utf-8 -p $(srcdir)/mutt.pwl check manual.xml.head
	-aspell -d american --mode=nroff --encoding=utf-8 -p $(srcdir)/mutt.pwl check muttrc.man.head
	-aspell -d american --mode=ccpp  --encoding=utf-8 -p $(srcdir)/mutt.pwl check $(top_srcdir)/init.h

sortcheck: manual.xml
	sed -n -e '1,/^<sect1 id="variables">/d' -e '1,/^<sect1 id="functions">/s/<sect2 id="\([^"]*\)">/\1/p' < manual.xml > vars.tmp.1
	sort < vars.tmp.1 > vars.tmp.2
	cmp -s vars.tmp.1 vars.tmp.2 || diff -u vars.tmp.1 vars.tmp.2 | less
	rm -rf vars.tmp.1 vars.tmp.2

clean-local:
	rm -f *~ *.html *.orig *.rej  *.ps mutt.1 smime_keys.1 muttrc.man pgpewrap.1 pgpring.1
	rm -f *.aux *.log *.tex *.out

DISTCLEANFILES=manual.txt manual.html manual.xml instdoc Muttrc

instdoc: instdoc.sh
	cp instdoc.sh instdoc
	chmod a+x instdoc

muttrc.man: makedoc$(EXEEXT) $(top_srcdir)/init.h muttrc.man.head muttrc.man.tail
	$(MAKEDOC_CPP) $(top_srcdir)/init.h | \
		./makedoc$(EXEEXT) -m | \
		cat $(srcdir)/muttrc.man.head - $(srcdir)/muttrc.man.tail > $@

EDIT = sed -e 's,@sysconfdir\@,$(sysconfdir),g' \
	-e 's,@bindir\@,$(bindir),g' \
	-e 's,@docdir\@,$(docdir),g'

mutt.1: $(srcdir)/mutt.man
	$(EDIT) $(srcdir)/mutt.man > $@

pgpewrap.1: $(srcdir)/pgpewrap.man
	$(EDIT) $(srcdir)/pgpewrap.man > $@

pgpring.1: $(srcdir)/pgpring.man
	$(EDIT) $(srcdir)/pgpring.man > $@

smime_keys.1: $(srcdir)/smime_keys.man
	$(EDIT) $(srcdir)/smime_keys.man > $@

manual.xml: makedoc$(EXEEXT) ../config.h $(top_srcdir)/init.h \
               manual.xml.head $(top_srcdir)/functions.h $(top_srcdir)/OPS* manual.xml.tail \
               $(srcdir)/gen-map-doc
	( mutt_ver=`sed -n -e '/MUTT_VERSION/s/.*"\(.*\)".*/\1/p' ../config.h` && \
		sed -e "s/@VERSION\@/$(PACKAGE_VERSION) ($$mutt_ver)/" $(srcdir)/manual.xml.head && \
		$(MAKEDOC_CPP) $(top_srcdir)/init.h | ./makedoc$(EXEEXT) -s && \
		$(MAKEDOC_CPP) $(top_srcdir)/functions.h | \
		perl $(srcdir)/gen-map-doc $(srcdir)/manual.xml.tail $(top_srcdir)/OPS* \
	) > $@

