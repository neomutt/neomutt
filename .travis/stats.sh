#!/bin/bash

if [ $# = 0 ]; then
	[ -d po ] && ARGS="po/*.po" || ARGS="*.po"
else
	ARGS="$*"
fi

ERROR=0

TMP_FILE=$(mktemp)

for i in $ARGS; do
	L=${i##*/}
	echo -ne "${L%.po}:\\t"
	msgfmt --statistics -c -o /dev/null "$i" 2>&1
	[ $? = 1 ] && ERROR=1
done > "$TMP_FILE"

sed 's/ \(message\|translation\)s*\.*//g' "$TMP_FILE" | sort -nr -k2 -k4 -k6
rm -f "$TMP_FILE"

exit $ERROR

