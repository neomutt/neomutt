#!/bin/sh --

#
# grep for some things which may look like security problems.
#

TMPFILE="`mktemp check_sec.tmp.XXXXXX`" || exit 1

do_check ()
{
	egrep -n "$1" *.c */*.c | fgrep -v $2 > $TMPFILE
	test -s $TMPFILE && {
		echo "$3" ;
		cat $TMPFILE;
		exit 1;
	}
}



do_check '\<fopen.*'\"'.*w' __FOPEN_CHECKED__ "Alert: Unchecked fopen calls."
do_check '\<(mutt_)?strcpy' __STRCPY_CHECKED__ "Alert: Unchecked strcpy calls."
# do_check '\<strcat' __STRCAT_CHECKED__ "Alert: Unchecked strcat calls."
do_check 'sprintf.*%s' __SPRINTF_CHECKED__ "Alert: Unchecked sprintf calls."

rm -f $TMPFILE
exit 0
