subdir = contrib

SAMPLES = colors.default colors.linux gpg.rc Mush.rc pgp2.rc pgp5.rc pgp6.rc \
	Pine.rc sample.mailcap sample.muttrc sample.muttrc-tlr smime.rc \
	smime_keys_test.pl Tin.rc

EXTRA_DIST = $(SAMPLES) colorschemes hcache-bench iconv/make.sh iconv/README \
	keybase language.txt language50.txt logo lua \
	patch.slang-1.2.2.keypad.1 smime_keys vim-keys

CONTRIB_DIRS = colorschemes hcache-bench keybase logo lua vim-keys

install-data-local:
	$(INSTALL) -d -m 755 $(DESTDIR)$(docdir)/samples $(DESTDIR)$(docdir)/samples/iconv
	for f in $(SAMPLES); do \
		$(INSTALL) -m 644 $(srcdir)/$$f $(DESTDIR)$(docdir)/samples; \
	done
	for f in $(srcdir)/iconv/*.rc; do \
		$(INSTALL) -m 644 $$f $(DESTDIR)$(docdir)/samples/iconv; \
	done
	for d in $(CONTRIB_DIRS); do \
		echo "Installing $$d"; \
		$(INSTALL) -d -m 755 $(DESTDIR)$(docdir)/$$d; \
		for f in $(srcdir)/$$d/*; do \
			echo "Installing $$f"; \
			$(INSTALL) -m 644 $$f $(DESTDIR)$(docdir)/$$d; \
		done \
	done
	chmod +x $(DESTDIR)$(docdir)/keybase/*.sh

uninstall-local:
	for f in $(SAMPLES); do \
		rm -f $(DESTDIR)$(docdir)/samples/$$f; \
	done
	for d in $(CONTRIB_DIRS); do \
		rm -fr $(DESTDIR)$(docdir)/$$d; \
	done
	-rm -rf $(DESTDIR)$(docdir)/samples/iconv
	-rmdir $(DESTDIR)$(docdir)/samples
	-rmdir $(DESTDIR)$(docdir)

distdir = ../$(PACKAGE)-$(VERSION)/$(subdir)
dist-hook: Makefile $(DISTFILES)
	$(MKDIR_P) $(distdir)/iconv
	for file in $(srcdir)/iconv/*.rc; do \
		ln $$file $(distdir)/iconv 2> /dev/null \
			|| cp -p $$file $(distdir)/iconv; \
	done
