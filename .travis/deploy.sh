#!/bin/bash

set -o errexit	# set -e
set -o nounset	# set -u

echo "DEPLOY_REPO         = $DEPLOY_REPO"
echo "DEPLOY_DIR          = $DEPLOY_DIR"
echo "DEPLOY_FILE         = $DEPLOY_FILE"
echo "TRAVIS_BRANCH       = $TRAVIS_BRANCH"
echo "TRAVIS_PULL_REQUEST = $TRAVIS_PULL_REQUEST"

if [ "$TRAVIS_PULL_REQUEST" != "false" ]; then
	echo "This is a Pull Request.  Done."
	exit 0
fi

if [ "$TRAVIS_BRANCH" != "translate" ]; then
	echo "This isn't branch 'translate'.  Done."
	exit 0
fi

.travis/prep.sh

set -v
eval "$(ssh-agent -s)"
ssh-add .travis/travis-deploy-github.pem

git clone "$DEPLOY_REPO" "$DEPLOY_DIR"
.travis/generate-webpage.sh po/*.po > "$DEPLOY_DIR/$DEPLOY_FILE"

cd "$DEPLOY_DIR"
git add "$DEPLOY_FILE"
git commit -m "[AUTO] update translations leaderboard" -m "[ci skip]"
git push origin

