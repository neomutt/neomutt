#!/bin/sh --

#
# grep for some things which may look like security problems.
#

TMPFILE="`mktemp fopen.XXXXXX`" || exit 1
grep -n '\<fopen.*".*w' *.c */*.c | fgrep -v __FOPEN_CHECKED__  > $TMPFILE
test -s $TMPFILE && {
	echo "WARNING: UNCHECKED FOPEN CALLS FOUND" ;
	cat $TMPFILE ;
	exit 1;
}

rm -f $TMPFILE
exit 0
