# Copyright (c) 2016 WorkWare Systems http://www.workware.net.au/
# All rights reserved

# @synopsis:
#
# The 'pkg-config' module allows package information to be found via 'pkg-config'.
#
# If not cross-compiling, the package path should be determined automatically
# by 'pkg-config'.
# If cross-compiling, the default package path is the compiler sysroot.
# If the C compiler doesn't support '-print-sysroot', the path can be supplied
# by the '--sysroot' option or by defining 'SYSROOT'.
#
# 'PKG_CONFIG' may be set to use an alternative to 'pkg-config'.

use cc

module-options {
	sysroot:dir => "Override compiler sysroot for pkg-config search path"
}

# @pkg-config-init ?required?
#
# Initialises the 'pkg-config' system. Unless '$required' is set to 0,
# it is a fatal error if a usable 'pkg-config' is not found .
#
# This command will normally be called automatically as required,
# but it may be invoked explicitly if lack of 'pkg-config' is acceptable.
#
# Returns 1 if ok, or 0 if 'pkg-config' not found/usable (only if '$required' is 0).
#
proc pkg-config-init {{required 1}} {
	if {[is-defined HAVE_PKG_CONFIG]} {
		return [get-define HAVE_PKG_CONFIG]
	}
	set found 0

	define PKG_CONFIG [get-env PKG_CONFIG pkg-config]
	msg-checking "Checking for pkg-config..."

	if {[catch {exec [get-define PKG_CONFIG] --version} version]} {
		msg-result "[get-define PKG_CONFIG] (not found)"
		if {$required} {
			user-error "No usable pkg-config"
		}
	} else {
		msg-result $version
		define PKG_CONFIG_VERSION $version

		set found 1

		if {[opt-str sysroot o]} {
			define SYSROOT [file-normalize $o]
			msg-result "Using specified sysroot [get-define SYSROOT]"
		} elseif {[get-define build] ne [get-define host]} {
			if {[catch {exec-with-stderr [get-define CC] -print-sysroot} result errinfo] == 0} {
				# Use the compiler sysroot, if there is one
				define SYSROOT $result
				msg-result "Found compiler sysroot $result"
			} else {
				set msg "pkg-config: Cross compiling, but no compiler sysroot and no --sysroot supplied"
				if {$required} {
					user-error $msg
				} else {
					msg-result $msg
				}
				set found 0
			}
		}
		if {[is-defined SYSROOT]} {
			set sysroot [get-define SYSROOT]

			# XXX: It's possible that these should be set only when invoking pkg-config
			global env
			set env(PKG_CONFIG_DIR) ""
			# Do we need to try /usr/local as well or instead?
			set env(PKG_CONFIG_LIBDIR) $sysroot/usr/lib/pkgconfig:$sysroot/usr/share/pkgconfig
			set env(PKG_CONFIG_SYSROOT_DIR) $sysroot
		}
	}
	define HAVE_PKG_CONFIG $found
	return $found
}

# @pkg-config module ?requirements?
#
# Use 'pkg-config' to find the given module meeting the given requirements.
# e.g.
#
## pkg-config pango >= 1.37.0
#
# If found, returns 1 and sets 'HAVE_PKG_PANGO' to 1 along with:
#
## PKG_PANGO_VERSION to the found version
## PKG_PANGO_LIBS    to the required libs (--libs-only-l)
## PKG_PANGO_LDFLAGS to the required linker flags (--libs-only-L)
## PKG_PANGO_CFLAGS  to the required compiler flags (--cflags)
#
# If not found, returns 0.
#
proc pkg-config {module args} {
	set ok [pkg-config-init]

	msg-checking "Checking for $module $args..."

	if {!$ok} {
		msg-result "no pkg-config"
		return 0
	}

	if {[catch {exec [get-define PKG_CONFIG] --modversion "$module $args"} version]} {
		msg-result "not found"
		configlog "pkg-config --modversion $module $args: $version"
		return 0
	}
	msg-result $version
	set prefix [feature-define-name $module PKG_]
	define HAVE_${prefix}
	define ${prefix}_VERSION $version
	define ${prefix}_LIBS [exec pkg-config --libs-only-l $module]
	define ${prefix}_LDFLAGS [exec pkg-config --libs-only-L $module]
	define ${prefix}_CFLAGS [exec pkg-config --cflags $module]
	return 1
}

# @pkg-config-get module setting
#
# Convenience access to the results of 'pkg-config'.
#
# For example, '[pkg-config-get pango CFLAGS]' returns
# the value of 'PKG_PANGO_CFLAGS', or '""' if not defined.
proc pkg-config-get {module name} {
	set prefix [feature-define-name $module PKG_]
	get-define ${prefix}_${name} ""
}
