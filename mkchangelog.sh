#!/bin/sh

TZ=GMT; export TZ
date="`head -1 ChangeLog | awk '{print $1, $2}'`"
cvs log -d ">$date" | perl ./cvslog2changelog.pl
