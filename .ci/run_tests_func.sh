#!/bin/sh

export BUILD_DIR="build_test"
export WORK_DIR="$(pwd)"

source $(dirname $0)/common_test.sh

echo "┌──────────────────────────────────────────────────────────────────────────────"
echo "└ Running functional tests"

function launch_func_tests_runner () {
  ${BUILD_DIR}/mutt -BF ${SOURCE_DIR}/tests/functional/test_runner.muttrc
}

check_env
check_have_tests
check_build_dir

build_func_test_target

launch_func_tests_runner

