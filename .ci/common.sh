#!/bin/sh

set -o nounset

# Makes sure SOURCE_DIR path is absolute and relative to current file
if [ ! -d ${SOURCE_DIR:-} ]; then
  SOURCE_DIR=$(readlink -f -- "$(dirname $0/..)")
fi

# show a nice message with a coloured unicode char to say it worked
function OK() {
  echo -e "  [\033[32m✓\033[0m]" $*
}

# show a nice message with a coloured unicode char to say it failed
function KO() {
  echo -e "  [\033[31m✗\033[0m]" $*
}

# helper function to exit upon error with a nice message,
# or show a nice message upon success
function check_ret() {
  if [ $? -ne 0 ]; then
    KO $2
    exit 1
  else
    if [[ $# -eq 2 ]]; then
      OK $1
    fi
  fi
}

# silent pushd
pushd () {
    command pushd "$@" > /dev/null
}

# silent popd
popd () {
    command popd "$@" > /dev/null
}

# Helper function to backup file
# Args:
#  $1: file name to be backed up
function backup_file() {
  if [ -f "$1.orig" ]; then
    restore_file "$1"
  fi
  if [ ! -f "$1" ]; then
    KO "No file found: $1"
    exit 1
  fi
  cp -f "$1" "$1.orig"
  OK "Backed up:	$1"
}

# Helper function to restore a backuped file used with the backup_file fn
# Args:
#  $1: file name that has been backed up and that's to be restored
function restore_file() {
  if [ ! -f $1.orig ]; then
    KO "No backup file found: $1.orig"
    exit 1
  fi
  mv -f $1.orig $1
  OK "Restored:	$1"
}

# checks and create the build dir if it does not exists
# Environ:
#  BUILD_DIR: path to where the build shall be done
check_build_dir() {
  if [ ! -d ${BUILD_DIR} ]; then
    mkdir -p ${BUILD_DIR}
  fi
}

# Checks and fixes the environment
#
# Environ:
#  CONFIG_DIR: path to the cloned config directory
#  BUILD_DIR: path to where the build shall be done
#  SOURCE_DIR: path to where the sources are
#  LUAC_PATH: path to the luac prefix
#  LUAJ_PATH: path to the luajit prefix
check_env() {

  if [ -z "${LUAC_PATH}" ]; then
    if [ -f "$(which luac)" ]; then
      export LUAC_PATH="/usr"
    else
      echo "Error: missing LUAC_PATH environment variable or Lua in path"
      exit 1
    fi
  else
    export LUAC_PATH=$(readlink -f ${WORK_DIR}/${LUAC_PATH})
  fi

  if [ -f "$(which luajit)" ]; then
    export LUAJ_PATH="/usr"
  else
    if [ -z "${LUAJ_PATH}" ]; then
      echo "Error: missing LUAJIT_PATH environment variable"
      exit 1
    else
      export LUAJ_PATH=$(readlink -f ${WORK_DIR}/${LUAJ_PATH}})
    fi
  fi

  if [ -z "${CONFIG_DIR}" ]; then
    echo "Error: missing CONFIG_DIR environment variable"
    exit 1
  fi

  if [ -z "${BUILD_DIR}" ]; then
    echo "Error: missing BUILD_DIR environment variable"
    exit 1
  elif [ "${BUILD_DIR:0:1}" != "/" ]; then
    BUILD_DIR=$(pwd)/${BUILD_DIR}
  fi

  if [ -z "${SOURCE_DIR}" ]; then
    echo "Error: missing SOURCE_DIR environment variable"
    exit 1
  elif [ "${SOURCE_DIR:0:1}" != "/" ]; then
    SOURCE_DIR=$(pwd)/${SOURCE_DIR}
  fi
}

# prepare and fix lua builds
# Compiles the luac shared library if missing, and exports the whole environment
# to make it possible to compile lua applications.
#
# Environ:
#   LUAC_PATH: path to the luac install prefix
#
function prepare_lua_env () {
  source ${LUAC_PATH}/bin/activate
  if [ ! -f ${LUAC_PATH}/lib/liblua53.so ]; then
    if [ -f ${LUAC_PATH}/lib/liblua53.a ]; then
      gcc -o ${LUAC_PATH}/lib/liblua53.so -shared -Wl,--no-as-needed -lm -ldl    \
	-Wl,--soname,liblua53.so -Wl,--whole-archive ${LUAC_PATH}/lib/liblua53.a \
	-Wl,--no-whole-archive
    else
      KO "Missing Lua library ${LUAC_PATH}/lib/liblua53.so (or ${LUAC_PATH}/lib/liblua53.a)"
      exit 1
    fi
  fi
  LUA=${LUAC_PATH}/bin/lua
  LUA_INCLUDE="-I${LUAC_PATH}/include"
  LUA_LIB="-L${LUAC_PATH}/lib/ -llua53"
  LUA_PREFIX=${LUAC_PATH}
  LD_LIBRARY_PATH=${LUAC_PATH}/lib
  export LUA LUA_INCLUDE LUA_LIB LUA_PREFIX LD_LIBRARY_PATH
}

# prints the configuration file path matching given branch
# Args:
#   $1 Branch to match
# Returns:
#   path to the configuration parameters file to read
function get_config_location()
{
  local TEST="/$1"

  while [ -n "$TEST" ]; do
    if [ -f "${CONFIG_DIR}${TEST}.txt" ]; then
      echo "${CONFIG_DIR}${TEST}.txt"
      return
    fi
    TEST="${TEST%/*}"
  done

  echo "${CONFIG_DIR}/default.txt"
}

# Returns current branch based on current git spec or travis env
function get_branch()
{
  if [ -n "${TRAVIS_BRANCH:-}" ]; then
    echo "$TRAVIS_BRANCH"
  else
    echo $(git rev-parse --abbrev-ref HEAD)
  fi
}

# Returns configuration file for PR or for given branch
function get_file()
{
  if [[ "${TRAVIS_PULL_REQUEST:-}" =~ ^[0-9]+$ ]]; then
    echo "$CONFIG_DIR/pull-request.txt"
  else
    echo $(get_config_location $BRANCH)
  fi
}

# Default function to be passed to build_target as patch function argument
#
# if no patching is needed, just pass this function as argument to build_target:
#
# build_target ./src ./build "" patch_none
function patch_none() {
  return
}

# build_target function
#
# Helper to compile a target
#
# Args:
#    source_dir: Path to where the sources are
#    build_dir: Path where to run the builds
#    target: what to build, the make target
#               (use an empty string to simply call make)
#    patch_fn: name of a declared function to run
#               (it will be given the build dir as parameter $1)
#    config_args: all the left over arguments will be given as configure args
#
# Environ:
#    CFLAGS: will be given to ./configure
#    LUA_INCLUDE: will be given to ./configure
#    LUA_LIB: will be given to ./configure
#
function build_target () {
  if [[ $# -lt 4 ]]; then
    KO "Not enough arguments to build target."
    return
  fi

  local source_dir=$1
  local build_dir=$2
  local target=$3
  local patch_fn=$4
  shift 4 # get rid of the four above arguments for $*
  local config_args=$*

  pushd ${build_dir}
  if [ ! -f ${BUILD_DIR}/Makefile ]; then
    autoreconf --install ${source_dir}
    check_ret "Success running autoconf" "Failed to run autoreconf"
    ${source_dir}/configure ${config_args} CFLAGS="${CFLAGS:-}" LUA_INCLUDE="${LUA_INCLUDE:-}" LUA_LIB="${LUA_LIB:-}"
    check_ret "Success running configure" "Failed to run configure"
  else
    make clean
  fi

  eval ${patch_fn} ${build_dir}

  make -s ${target}
  if [ -z ${target} ]; then
    target="mutt"
  fi
  check_ret "Built ${target} target" "Failure building ${target} target"

  popd
}
