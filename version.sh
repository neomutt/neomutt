#!/bin/sh

# Switch to directory where this script lives so that further commands are run
# from the root directory of the source.  The script path and srcdir are double
# quoted to allow the space character to appear in the path.
srcdir=`dirname "$0"` && cd "$srcdir" || exit 1

cat VERSION* | tr -d \\n

