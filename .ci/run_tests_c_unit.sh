#!/bin/sh

export BUILD_DIR="build_test"
export WORK_DIR="$(pwd)"

source $(dirname $0)/common_test.sh

echo "┌──────────────────────────────────────────────────────────────────────────────"
echo "└ Running C unit tests"

function launch_c_unit_tests_runner () {
  export LD_LIBRARY_PATH="${BUILD_DIR}/;${LUAC_PATH}/lib"
  for spec_source in ${SOURCE_DIR}/tests/c_unit/*_spec.c; do
    local spec_runtime=${spec_source/.c/}
    gcc --std=c99 -o ${spec_runtime} ${spec_source} -fprofile-arcs -ftest-coverage \
        -I${SOURCE_DIR}/tests/c_unit/ext -I${SOURCE_DIR} -I${BUILD_DIR}    \
	${LUA_LIB} -L./build_test -lmutt
    echo
    ${spec_runtime}
    if [[ $? -ne 0 ]]; then
      return $?
    fi
  done
  return 0
}

check_env
check_have_tests
check_build_dir

build_unit_test_target

launch_c_unit_tests_runner
echo
check_ret "All tests went fine" "Failure running c_unit tests"

