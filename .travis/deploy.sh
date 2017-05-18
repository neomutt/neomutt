#!/bin/bash

set -o errexit	# set -e
set -o nounset	# set -u

function calc_percentage()
{
	local FILE="$1"
	local TNUM=0
	local FNUM=0
	local UNUM=0
	local LINE

	LINE="$(msgfmt --statistics -c -o /dev/null "$FILE" 2>&1 | sed 's/ \(message\|translation\)s*\.*//g')"

	# filename: 104 translated, 22 fuzzy, 11 untranslated
	if [[ "$LINE" =~ ([0-9]+)[[:space:]]translated,[[:space:]]([0-9]+)[[:space:]]fuzzy,[[:space:]]([0-9]+)[[:space:]]untranslated ]]; then
		TNUM=${BASH_REMATCH[1]} # translated
		FNUM=${BASH_REMATCH[2]} # fuzzy
		UNUM=${BASH_REMATCH[3]} # untranslated
	# filename: 320 translated, 20 untranslated
	elif [[ "$LINE" =~ ([0-9]+)[[:space:]]translated,[[:space:]]([0-9]+)[[:space:]]untranslated ]]; then
		TNUM=${BASH_REMATCH[1]} # translated
		UNUM=${BASH_REMATCH[2]} # untranslated
	# filename: 5 translated, 13 fuzzy
	elif [[ "$LINE" =~ ([0-9]+)[[:space:]]translated,[[:space:]]([0-9]+)[[:space:]]fuzzy ]]; then
		TNUM=${BASH_REMATCH[1]} # translated
		FNUM=${BASH_REMATCH[2]} # fuzzy
	# filename: 63 translated
	elif [[ "$LINE" =~ ([0-9]+)[[:space:]]translated ]]; then
		TNUM=${BASH_REMATCH[1]} # translated
	fi

	# number of translated strings
	local TOTAL=$((TNUM+FNUM+UNUM))
	# percentage complete
	echo $((100*TNUM/TOTAL))
}


echo "DEPLOY_DIR          = $DEPLOY_DIR"
echo "DEPLOY_FILE         = $DEPLOY_FILE"
echo "DEPLOY_REPO         = $DEPLOY_REPO"
echo "TRAVIS_BRANCH       = $TRAVIS_BRANCH"
echo "TRAVIS_COMMIT       = $TRAVIS_COMMIT"
echo "TRAVIS_PULL_REQUEST = $TRAVIS_PULL_REQUEST"

if [ "$TRAVIS_PULL_REQUEST" != "false" ]; then
	echo "This is a Pull Request.  Done."
	exit 0
fi

if [ "$TRAVIS_BRANCH" != "translate" ]; then
	echo "This isn't branch 'translate'.  Done."
	exit 0
fi

FILES="$(git diff --name-only "$TRAVIS_COMMIT^..$TRAVIS_COMMIT" -- 'po/*.po')"
FILE_COUNT="$(echo "$FILES" | wc -w)"

if [ "$FILE_COUNT" = 1 ]; then
	AUTHOR="$(git log -n1 --format="%aN" "$TRAVIS_COMMIT")"
	PO="${FILES##*/}"
	PO="${PO%.po}"
	PCT=$(calc_percentage "$FILES")
	MESSAGE="$AUTHOR, $PO, $PCT%"
else
	MESSAGE="update leaderboard"
fi

.travis/prep.sh

set -v
eval "$(ssh-agent -s)"
ssh-add .travis/travis-deploy-github.pem

git clone "$DEPLOY_REPO" "$DEPLOY_DIR"
.travis/generate-webpage.sh po/*.po > "$DEPLOY_DIR/$DEPLOY_FILE"

cd "$DEPLOY_DIR"
git add "$DEPLOY_FILE"
git commit -m "[AUTO] translation: $MESSAGE" -m "[ci skip]"
git push origin

