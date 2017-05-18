#!/bin/bash

set -o errexit	# set -e
set -o nounset	# set -u

git config --global user.email "rich@flatcap.org"
git config --global user.name "Richard Russon (DEPLOY)"

mkdir -p ~/.ssh
echo "Host github.com" >> ~/.ssh/config
echo "        StrictHostKeyChecking no" >> ~/.ssh/config
chmod 600 ~/.ssh/config

cd .travis
openssl aes-256-cbc -K $encrypted_ff1b3f8609ac_key -iv $encrypted_ff1b3f8609ac_iv -in travis-deploy-github.enc -out travis-deploy-github.pem -d
chmod 0400 travis-deploy-github.pem

