## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

subdir = doc

DEFS = -DSYSCONFDIR=\"$(sysconfdir)\" -DBINDIR=\"$(bindir)\"
AM_CPPFLAGS = -I. -I.. -I$(includedir) -I$(top_srcdir)

MAKEDOC_CPP = $(CPP) $(AM_CPPFLAGS) $(DEFS) $(CPPFLAGS) -D_MAKEDOC -C

noinst_PROGRAMS = makedoc

EXTRA_DIST = $(BUILT_DISTFILES) chunk.xsl gen-map-doc html.xsl makedoc.c \
	makedoc_defs.h manual.xml.head manual.xml.tail mbox.5 mmdf.5 \
	neomutt.css neomutt.man neomutt.xsl neomuttrc neomuttrc.head \
	neomuttrc.man.head neomuttrc.man.tail neomutt-syntax.vim PGP-Notes.txt \
	smime-notes.txt pgpewrap.1 pgpring.1 smime_keys.1 mime.types

CHUNKED_DOCFILES = advancedusage.html configuration.html gettingstarted.html \
	intro.html mimesupport.html miscellany.html optionalfeatures.html \
	reference.html security.html tuning.html

HTML_DOCFILES = manual.html index.html $(CHUNKED_DOCFILES)

BUILT_DISTFILES = manual.txt $(HTML_DOCFILES)

srcdir_DOCFILES = PGP-Notes.txt smime-notes.txt

topsrcdir_DOCFILES = ChangeLog.md CODE_OF_CONDUCT.md COPYRIGHT INSTALL \
	LICENSE.md README.md README.SSL

all: makedoc-all

makedoc-all: $(CHUNKED_DOCFILES) index.html manual.html manual.txt neomutt.1 \
	neomuttrc neomuttrc.man neomutt-syntax.vim

install-data-local: makedoc-all
	$(MKDIR_P) $(DESTDIR)$(mandir)/man1
	$(MKDIR_P) $(DESTDIR)$(mandir)/man5
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	$(INSTALL) -m 644 neomutt.1 $(DESTDIR)$(mandir)/man1/neomutt.1
	$(INSTALL) -m 644 neomuttrc.man $(DESTDIR)$(mandir)/man5/neomuttrc.5
	$(INSTALL) -m 644 $(srcdir)/smime_keys.1 $(DESTDIR)$(mandir)/man1/smime_keys_$(PACKAGE).1
	$(INSTALL) -m 644 $(srcdir)/pgpewrap.1 $(DESTDIR)$(mandir)/man1/pgpewrap_$(PACKAGE).1
	$(INSTALL) -m 644 $(srcdir)/pgpring.1 $(DESTDIR)$(mandir)/man1/pgpring_$(PACKAGE).1
	$(INSTALL) -m 644 $(srcdir)/mbox.5 $(DESTDIR)$(mandir)/man5/mbox_$(PACKAGE).5
	$(INSTALL) -m 644 $(srcdir)/mmdf.5 $(DESTDIR)$(mandir)/man5/mmdf_$(PACKAGE).5
	$(MKDIR_P) $(DESTDIR)$(docdir)
	for f in $(topsrcdir_DOCFILES); do \
		$(INSTALL) -m 644 $(top_srcdir)/$$f $(DESTDIR)$(docdir); \
	done
	for f in $(srcdir_DOCFILES); do \
		$(INSTALL) -m 644 $(srcdir)/$$f $(DESTDIR)$(docdir); \
	done
	-$(INSTALL) -m 644 manual.txt $(DESTDIR)$(docdir)
	-for f in $(HTML_DOCFILES); do \
		$(INSTALL) -m 644 $$f $(DESTDIR)$(docdir); \
	done
	$(INSTALL) -m 644 neomuttrc $(DESTDIR)$(sysconfdir)/neomuttrc
	-$(INSTALL) -m 644 $(srcdir)/neomutt-syntax.vim $(DESTDIR)$(docdir)
	-$(INSTALL) -m 644 $(srcdir)/mime.types $(DESTDIR)$(docdir)/mime.types

uninstall-local:
	for f in neomutt.1 smime_keys_$(PACKAGE).1 pgpewrap_$(PACKAGE).1 pgpring_$(PACKAGE).1; do \
		rm -f $(DESTDIR)$(mandir)/man1/$$f; \
	done
	for f in neomuttrc.5 mbox_$(PACKAGE).5 mmdf_$(PACKAGE).5; do \
		rm -f $(DESTDIR)$(mandir)/man5/$$f; \
	done
	for f in $(srcdir_DOCFILES) $(topsrcdir_DOCFILES) $(HTML_DOCFILES); do \
		rm -f $(DESTDIR)$(docdir)/$$f; \
	done
	rm -f $(DESTDIR)$(docdir)/manual.txt
	rm -f $(DESTDIR)$(sysconfdir)/neomuttrc
	rm -f $(DESTDIR)$(docdir)/neomutt-syntax.vim
	rm -f $(DESTDIR)$(docdir)/mime.types

check:
manual.txt: manual.html
	-LC_ALL=C w3m -dump -O UTF8 manual.html > $@ || \
	LC_ALL=C lynx -dump -nolist -with_backspaces -display_charset=us-ascii manual.html > $@ || \
	LC_ALL=C elinks -dump -no-numbering -no-references manual.html | sed -e 's,\\001, ,g' > $@

neomuttrc: $(top_srcdir)/init.h makedoc$(EXEEXT) $(srcdir)/neomuttrc.head
	sed -e 's,[@]docdir[@],$(docdir),' $(srcdir)/neomuttrc.head > neomuttrc
	$(CPP) $(AM_CPPFLAGS) $(DEFS) $(CPPFLAGS) -D_MAKEDOC -C $(top_srcdir)/init.h | ./makedoc$(EXEEXT) -c >> neomuttrc

manual.html: manual.xml $(srcdir)/html.xsl $(srcdir)/neomutt.xsl $(srcdir)/neomutt.css
	-xsltproc --nonet -o $@ $(srcdir)/html.xsl manual.xml

$(CHUNKED_DOCFILES): index.html

index.html: $(srcdir)/chunk.xsl $(srcdir)/neomutt.xsl manual.xml $(srcdir)/neomutt.css
	-xsltproc --nonet $(srcdir)/chunk.xsl manual.xml > /dev/null 2>&1

validate: manual.xml
	xmllint --noout --noblanks --postvalid $<

spellcheck:
	-aspell -d american --mode=sgml  --encoding=utf-8 -p $(srcdir)/neomutt.pwl check manual.xml.head
	-aspell -d american --mode=nroff --encoding=utf-8 -p $(srcdir)/neomutt.pwl check neomuttrc.man.head
	-aspell -d american --mode=ccpp  --encoding=utf-8 -p $(srcdir)/neomutt.pwl check $(top_srcdir)/init.h

sortcheck: manual.xml
	sed -n -e '1,/^<sect1 id="variables">/d' -e '1,/^<sect1 id="functions">/s/<sect2 id="\([^"]*\)">/\1/p' < manual.xml > vars.tmp.1
	sort < vars.tmp.1 > vars.tmp.2
	cmp -s vars.tmp.1 vars.tmp.2 || diff -u vars.tmp.1 vars.tmp.2 | less
	rm -rf vars.tmp.1 vars.tmp.2

clean-local:
	rm -f *.html neomutt.1 neomuttrc.man

DISTCLEANFILES = manual.txt manual.html manual.xml neomuttrc

neomuttrc.man: makedoc$(EXEEXT) $(top_srcdir)/init.h neomuttrc.man.head neomuttrc.man.tail
	$(MAKEDOC_CPP) $(top_srcdir)/init.h | \
		./makedoc$(EXEEXT) -m | \
		cat $(srcdir)/neomuttrc.man.head - $(srcdir)/neomuttrc.man.tail > $@

EDIT = sed -e 's,@sysconfdir\@,$(sysconfdir),g' \
	-e 's,@bindir\@,$(bindir),g' \
	-e 's,@docdir\@,$(docdir),g'

neomutt.1: $(srcdir)/neomutt.man
	$(EDIT) $(srcdir)/neomutt.man > $@

manual.xml: makedoc$(EXEEXT) $(top_srcdir)/init.h \
	manual.xml.head $(top_srcdir)/functions.h manual.xml.tail \
	$(srcdir)/gen-map-doc $(top_srcdir)/opcodes.h
	( sed -e "s/@VERSION\@/$(PACKAGE_VERSION)/" $(srcdir)/manual.xml.head && \
		$(MAKEDOC_CPP) $(top_srcdir)/init.h | ./makedoc$(EXEEXT) -s && \
		$(MAKEDOC_CPP) $(top_srcdir)/functions.h | perl $(srcdir)/gen-map-doc $(srcdir)/manual.xml.tail $(top_srcdir)/opcodes.h \
	) > $@

