#!/bin/sh

txt2c_fallback () {
	# consumes stdin

	# declaration
	echo "unsigned char $1[] = "

	# initializer - filter out unwanted characters, then convert problematic
	# or odd-looking sequences.  The result is a sequence of quote-bounded
	# C strings, which the compiler concatenates into a single C string.
	tr -c '\011\012\015\040[!-~]' '?' |
	sed \
	    -e 's/\\/\\\\/g' \
	    -e 's/"/\\"/g' \
	    -e 's/??/\\?\\?/g' \
	    -e 's/	/\\t/'g \
	    -e 's//\\r/g' \
	    -e 's/^/	"/g' \
	    -e 's/$/\\n"/g'
	echo ";"
}

./txt2c test </dev/null >/dev/null 2>&1 &&
./txt2c "$1" ||
txt2c_fallback "$1"
