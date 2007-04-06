#!/bin/sh

TZ=GMT; export TZ
lrev=$(hg log --limit 1 --template '{rev}' ChangeLog)

hg log --style=./hg-changelog-map -r tip:$lrev
