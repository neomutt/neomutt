#!/bin/sh --

#
# Create sample configuration files from the tables contained with libiconv.
# Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
#
# This ugly shell script is free software; you can distribute and/or modify
# it under the terms of the GNU General Public License version 2 or later.
#

LIBICONV="$1"
test -d $LIBICONV/libcharset/tools || {
	echo "Sorry, I can't find libiconv's source!" >&2 ; 
	exit 1 ;
}

for f in $LIBICONV/libcharset/tools/* ; do
	rm -f tmp.rc.
	( head -3 $f | grep -q 'locale name.*locale charmap.*locale_charset' ) && (
		sed '1,/^$/d' $f | awk '($4 != $3) { printf ("iconv-hook %s %s\n", $4, $3); }' | \
			sed -e 's/^iconv-hook SJIS /iconv-hook Shift_JIS /gi' |
			sort -u > tmp.rc )
	test -s tmp.rc && mv tmp.rc iconv.`basename $f`.rc
	rm -f tmp.rc
done
