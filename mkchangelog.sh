#!/bin/sh

TZ=GMT; export TZ
date="`head -1 ChangeLog | awk '{print $1, $2}'`"
test -n "$date" || exit 1
cvs -z9 log -d ">$date" | perl ./cvslog2changelog.pl
