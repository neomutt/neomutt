#!/bin/sh --

#
# grep for some things which may look like security problems.
#

TMPFILE="`mktemp check_sec.tmp.XXXXXX`" || exit 1

RV=0;

do_check_files ()
{
	pattern="$1" ; shift
	magic="$1" ; shift
	msg="$1" ; shift
	egrep -n "$pattern" "$@"        	| \
		grep -v '^[^	 ]*:[^ 	]*#' 	| \
		fgrep -v "$magic" > $TMPFILE

	test -s $TMPFILE && {
		echo "$msg" ;
		cat $TMPFILE;
		rm -f $TMPFILE;
		RV=1;
	}
}

do_check ()
{
	do_check_files "$1" "$2" "$3" `find . -path ./intl -prune -o -name '*.c' -print`
}

do_check '\<fopen.*'\"'.*w' __FOPEN_CHECKED__ "Alert: Unchecked fopen calls."
do_check '\<fclose.*'\"'.*w' __FCLOSE_CHECKED__ "Alert: Unchecked fclose calls."
do_check '\<(mutt_)?strcpy' __STRCPY_CHECKED__ "Alert: Unchecked strcpy calls."
do_check '\<strcat' __STRCAT_CHECKED__ "Alert: Unchecked strcat calls."
do_check '\<sprintf.*%s' __SPRINTF_CHECKED__ "Alert: Unchecked sprintf calls."
do_check '\<strncat' __STRNCAT_CHECKED__ "You probably meant safe_strcat here."
do_check '\<safe_free' __SAFE_FREE_CHECKED__ "You probably meant FREE here."
do_check '\<FREE[ ]?\([^&]' __FREE_CHECKED__ "You probably meant FREE(&...) here."

# don't do this check on others' code.
do_check_files '\<(malloc|realloc|free|strdup)[ 	]*\(' __MEM_CHECKED__ "Alert: Use of traditional memory management calls." \
	*.c imap/*.c

rm -f $TMPFILE
exit $RV
