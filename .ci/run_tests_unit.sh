#!/bin/sh

export BUILD_DIR="build_test"
export WORK_DIR="$(pwd)"

source $(dirname $0)/common_test.sh

echo "┌──────────────────────────────────────────────────────────────────────────────"
echo "└ Running unit tests"

function launch_unit_tests_runner () {
  ${WORK_DIR}/luajit/bin/busted ${SOURCE_DIR}/tests/unit/
}

check_env
check_have_tests
check_build_dir

build_unit_test_target

launch_unit_tests_runner

