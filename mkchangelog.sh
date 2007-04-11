#!/bin/sh

lrev=$(hg log --limit 1 --template '{rev}' ChangeLog)

hg log --style=./hg-changelog-map -r tip:$lrev
