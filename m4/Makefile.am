EXTRA_DIST = gettext.m4 lib-ld.m4 lib-link.m4 lib-prefix.m4 nls.m4 po.m4 README

dist-hook:
	for i in $(srcdir)/*.m4; do \
		cp -f -p $$i $(distdir); \
	done
