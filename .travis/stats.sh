#!/bin/bash

[ $# = 0 ] && ARGS="*.po" || ARGS="$*"

ERROR=0

TMP_FILE=$(mktemp)

for i in $ARGS; do
	echo -ne "${i%.po}:\t"
	msgfmt --statistics -c -o /dev/null "$i" 2>&1
	[ $? = 1 ] && ERROR=1
done > "$TMP_FILE"

sed 's/ \(message\|translation\)s*\.*//g' "$TMP_FILE" | sort -nr -k2 -k4 -k6
rm -f "$TMP_FILE"

exit $ERROR

