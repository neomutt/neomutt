# $Id$

subdir = contrib

SAMPLES = Mush.rc Pine.rc gpg.rc pgp2.rc pgp5.rc pgp6.rc Tin.rc \
	sample.mailcap sample.muttrc sample.muttrc-sidebar sample.muttrc-tlr \
	sample.vimrc-sidebar colors.default colors.linux smime.rc \
	ca-bundle.crt smime_keys_test.pl mutt_xtitle

EXTRA_DIST = language.txt language50.txt  \
	patch.slang-1.2.2.keypad.1	\
	$(SAMPLES) \
	iconv/README \
	iconv/make.sh \
	vim-keys \
	keybase

CONTRIB_DIRS = vim-keys keybase

install-data-local:
	$(INSTALL) -d -m 755 $(DESTDIR)$(docdir)/samples $(DESTDIR)$(docdir)/samples/iconv
	for f in $(SAMPLES) ; do \
		$(INSTALL) -m 644 $(srcdir)/$$f $(DESTDIR)$(docdir)/samples ;	\
	done
	for f in $(srcdir)/iconv/*.rc ; do					\
		$(INSTALL) -m 644 $$f $(DESTDIR)$(docdir)/samples/iconv	  ;	\
	done
	for d in $(CONTRIB_DIRS); do						\
		echo "Installing $$d" ;	\
		$(INSTALL) -d -m 755 $(DESTDIR)$(docdir)/$$d ;\
		for f in $(srcdir)/$$d/*; do	\
			echo "Installing $$f" ;	\
			$(INSTALL) -m 644 $$f $(DESTDIR)$(docdir)/$$d ; \
		done \
	done
	chmod +x $(DESTDIR)$(docdir)/keybase/*.sh

uninstall-local:
	for f in $(SAMPLES) ; do \
		rm -f $(DESTDIR)$(docdir)/samples/$$f ; \
	done
	for d in $(CONTRIB_DIRS); do						\
		rm -fr $(DESTDIR)$(docdir)/$$d;	                		\
	done
	-rm -rf $(DESTDIR)$(docdir)/samples/iconv
	-rmdir $(DESTDIR)$(docdir)/samples
	-rmdir $(DESTDIR)$(docdir)

distdir = ../$(PACKAGE)-$(VERSION)/$(subdir)
dist-hook: Makefile $(DISTFILES)
	mkdir -p $(distdir)/iconv
	for file in $(srcdir)/iconv/*.rc ; do					\
		ln $$file $(distdir)/iconv 2> /dev/null				\
			|| cp -p $$file $(distdir)/iconv ;			\
	done
