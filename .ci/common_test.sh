#!/bin/sh

source $(dirname $0)/common.sh

# Patches mutt's Makefile with a new target to compile mutt as a shared lib
# This function is meant to be called with build directory as first argument
# This function is meant to be passed as patch parameter of the build_target command
# Adds the following target:
#    libmutt.so: $(mutt_OBJECTS) $(mutt_DEPENDENCIES) $(EXTRA_mutt_DEPENDENCIES)
#      @rm -f libmutt.so
#      $(AM_V_CCLD)\$(CCLD) $(AM_CFLAGS) \$(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@
function patch_with_so_target() {
  backup_file $1/Makefile

  sed -i '/^mutt\$(EXEEXT)/i\
libmutt.so: \$(BUILT_SOURCES) config.h all-recursive \$(mutt_OBJECTS) \$(mutt_DEPENDENCIES) \$(EXTRA_mutt_DEPENDENCIES)\
	\@rm -f libmutt.so\
	\$(AM_V_CCLD)\$(CCLD) \$(AM_CFLAGS) \$(CFLAGS) \$(AM_LDFLAGS) \$(LDFLAGS) \$(mutt_OBJECTS) \$(mutt_LDADD) \$(LIBS) -shared -Wl,-soname,\$\@ -o$\@\

' $1/Makefile
  check_ret "Patched:	$1 with libmutt.so target" \
            "Failure to patch $1 with libmutt.so target"
}

# Builds target for unit tests
function build_unit_test_target() {
  prepare_lua_env
  CFLAGS="-fPIC -fprofile-arcs -ftest-coverage"
  build_target ${SOURCE_DIR} ${BUILD_DIR}                   \
    libmutt.so patch_with_so_target                         \
    --quiet --disable-doc --disable-po                      \
    --disable-silent-rules --enable-everything --enable-lua
}

# Builds target for functional tests
function build_func_test_target() {
  prepare_lua_env
  CFLAGS="-fprofile-arcs -ftest-coverage"
  build_target ${SOURCE_DIR} ${BUILD_DIR} "" patch_none   \
    --quiet --disable-doc --disable-po                      \
    --disable-silent-rules --enable-everything --enable-lua
}

# Checks that current sources has a tests directory
function check_have_tests() {
  if [ ! -d  "${SOURCE_DIR}/tests" ]; then
    KO "Skipping tests. Tests specs not found at ${SOURCE_DIR}/tests."
    exit 0
  fi
}

