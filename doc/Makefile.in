all: manual.txt mutt.man manual.html

manual.txt: manual.sgml
	sgml2txt manual

manual.html: manual.sgml
	sgml2html manual

mutt.man: mutt.sgml
	sgml2txt -man mutt

clean clean-real distclean:
	rm -f *~ *.html *.orig *.rej
