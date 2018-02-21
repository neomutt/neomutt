#!/bin/bash

export BUILD_DIR="build"
export WORK_DIR="$(pwd)"

source $(dirname $0)/common.sh

function patch_remove_dotlock() {
  sed -i '/USE_DOTLOCK/d' config.h
}

function patch_fail_dotlock() {
    # Don't exit if we can't run chgrp on mutt_dotlock
    sed -i '/fix mutt_dotlock/s/exit 1 *;//' Makefile.am
}

function run_build()
{
  if [[ $# -ne 4 ]]; then
    KO "Illegal arguments passed."
    return;
  fi

  FILE=$(get_file)
  LEN=$(wc -l "$FILE" | cut -d' ' -f1)

  echo "  ┌────────────────────────────────────────────────────────────────────────────"
  echo "  │ BRANCH: $BRANCH ($LEN configuration$([ $LEN != 1 ] && echo 's'))"
  echo "  │ CONFIG FILE: ${FILE#$CONFIG_DIR/}"
  echo "  └────────────────────────────────────────────────────────────────────────────"
  BUILD_COUNT=0
  while read CONFIG; do
    : $((BUILD_COUNT++))
    echo "BUILD: $BUILD_COUNT/$LEN"
    if [ -n "$CONFIG" ]; then
      echo $CONFIG | fmt -w 75 | sed 's/^/    /'
    else
      echo "    <no options>"
    fi
    echo
    git clean -xdfq
    local configure_params=--quiet --disable-doc --disable-po $CONFIG
    build_target ${SOURCE_DIR} ${BUILD_DIR} "" patch_remove_dotlock ${configure_params}
    echo
    ./mutt -v
    check_ret "Test running mutt command" "Failure running mutt command"
    echo "  ─────────────────────────────────────────────────────────────────────────────"
  done < "$FILE"
}

function run_full_build() {
  (
    ccache --zero-stats

    FILE=$(get_file)

    echo "  ┌────────────────────────────────────────────────────────────────────────────"
    echo "  │ BRANCH: $BRANCH"
    echo "  │ CONFIG FILE: ${FILE#$CONFIG_DIR/}"
    echo "  └────────────────────────────────────────────────────────────────────────────"

    cd ${BUILD_DIR}
    git clean -xdfq ${SOURCE_DIR}
    autoreconf --install ${SOURCE_DIR}
    ${SOURCES}/configure
    echo "┌──────────────────────────────────────────────────────────────────────────────"
    echo "└ Building Mutt binary"
    local configure_params= --quiet --prefix=$HOME/install $(tail -n 1 $(get_config_location master))
    build_target ${SOURCE_DIR} ${BUILD_DIR} "" patch_none ${configure_params}

    echo "┌──────────────────────────────────────────────────────────────────────────────"
    echo "└ Build and validate documentation"
    make -C doc validate
    check_ret "Success building and validating documentation" \
	      "Failure building or validating documentation"
    echo "┌──────────────────────────────────────────────────────────────────────────────"
    echo "└ Install"
    make install
    check_ret "Success trying install" "Failure trying install"
    echo "┌──────────────────────────────────────────────────────────────────────────────"
    echo "└ Dist"
    make -s dist
    check_ret "Success trying to build dist" "Failure trying to build dist"
    echo "┌──────────────────────────────────────────────────────────────────────────────"
    echo "└ Dist Check"
    make -s distcheck
    check_ret "Success checking dist" "Failure checking dist"
    echo "───────────────────────────────────────────────────────────────────────────────"

    ccache --show-stats

  )

  return 0
}

check_env

BRANCH=$(get_branch)

if [[ $BRANCH = "master" && ! "${TRAVIS_PULL_REQUEST:-}" =~ ^[0-9]+$ ]]; then
  # Do a full build and test it
  run_full_build
  check_ret "🍻 Full build is a success" "Full build failed"
else
  echo "Skipping build tests."
fi

exit 0


