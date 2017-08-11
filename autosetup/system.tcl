# Copyright (c) 2010 WorkWare Systems http://www.workware.net.au/
# All rights reserved

# @synopsis:
#
# This module supports common system interrogation and options
# such as '--host', '--build', '--prefix', and setting 'srcdir', 'builddir', and 'EXEEXT'.
#
# It also support the "feature" naming convention, where searching
# for a feature such as 'sys/type.h' defines 'HAVE_SYS_TYPES_H'.
#
# It defines the following variables, based on '--prefix' unless overridden by the user:
#
## datadir
## sysconfdir
## sharedstatedir
## localstatedir
## infodir
## mandir
## includedir
#
# If '--prefix' is not supplied, it defaults to '/usr/local' unless 'defaultprefix' is defined *before*
# including the 'system' module.

set defaultprefix [get-define defaultprefix /usr/local]

module-options [subst -noc -nob {
	host:host-alias =>		{a complete or partial cpu-vendor-opsys for the system where
							the application will run (defaults to the same value as --build)}
	build:build-alias =>	{a complete or partial cpu-vendor-opsys for the system
							where the application will be built (defaults to the
							result of running config.guess)}
	prefix:dir =>			{the target directory for the build (defaults to '$defaultprefix')}

	# These (hidden) options are supported for autoconf/automake compatibility
	exec-prefix:
	bindir:
	sbindir:
	includedir:
	mandir:
	infodir:
	libexecdir:
	datadir:
	libdir:
	sysconfdir:
	sharedstatedir:
	localstatedir:
	maintainer-mode=0
	dependency-tracking=0
}]

# @check-feature name { script }
#
# defines feature '$name' to the return value of '$script',
# which should be 1 if found or 0 if not found.
#
# e.g. the following will define 'HAVE_CONST' to 0 or 1.
#
## check-feature const {
##     cctest -code {const int _x = 0;}
## }
proc check-feature {name code} {
	msg-checking "Checking for $name..."
	set r [uplevel 1 $code]
	define-feature $name $r
	if {$r} {
		msg-result "ok"
	} else {
		msg-result "not found"
	}
	return $r
}

# @have-feature name ?default=0?
#
# Returns the value of feature '$name' if defined, or '$default' if not.
#
# See 'feature-define-name' for how the "feature" name
# is translated into the "define" name.
#
proc have-feature {name {default 0}} {
	get-define [feature-define-name $name] $default
}

# @define-feature name ?value=1?
#
# Sets the feature 'define' to '$value'.
#
# See 'feature-define-name' for how the "feature" name
# is translated into the "define" name.
#
proc define-feature {name {value 1}} {
	define [feature-define-name $name] $value
}

# @feature-checked name
#
# Returns 1 if feature '$name' has been checked, whether true or not.
#
proc feature-checked {name} {
	is-defined [feature-define-name $name]
}

# @feature-define-name name ?prefix=HAVE_?
#
# Converts a "feature" name to the corresponding "define",
# e.g. 'sys/stat.h' becomes 'HAVE_SYS_STAT_H'.
#
# Converts '*' to 'P' and all non-alphanumeric to underscore.
#
proc feature-define-name {name {prefix HAVE_}} {
	string toupper $prefix[regsub -all {[^a-zA-Z0-9]} [regsub -all {[*]} $name p] _]
}

# @write-if-changed filename contents ?script?
#
# If '$filename' doesn't exist, or it's contents are different to '$contents',
# the file is written and '$script' is evaluated.
#
# Otherwise a "file is unchanged" message is displayed.
proc write-if-changed {file buf {script {}}} {
	set old [readfile $file ""]
	if {$old eq $buf && [file exists $file]} {
		msg-result "$file is unchanged"
	} else {
		writefile $file $buf\n
		uplevel 1 $script
	}
}

# @make-template template ?outfile?
#
# Reads the input file '<srcdir>/$template' and writes the output file '$outfile'.
# If '$outfile' is blank/omitted, '$template' should end with '.in' which
# is removed to create the output file name.
#
# Each pattern of the form '@define@' is replaced with the corresponding
# "define", if it exists, or left unchanged if not.
# 
# The special value '@srcdir@' is substituted with the relative
# path to the source directory from the directory where the output
# file is created, while the special value '@top_srcdir@' is substituted
# with the relative path to the top level source directory.
#
# Conditional sections may be specified as follows:
## @if name == value
## lines
## @else
## lines
## @endif
#
# Where 'name' is a defined variable name and '@else' is optional.
# If the expression does not match, all lines through '@endif' are ignored.
#
# The alternative forms may also be used:
## @if name
## @if name != value
#
# Where the first form is true if the variable is defined, but not empty nor 0.
#
# Currently these expressions can't be nested.
#
proc make-template {template {out {}}} {
	set infile [file join $::autosetup(srcdir) $template]

	if {![file exists $infile]} {
		user-error "Template $template is missing"
	}

	# Define this as late as possible
	define AUTODEPS $::autosetup(deps)

	if {$out eq ""} {
		if {[file ext $template] ne ".in"} {
			autosetup-error "make_template $template has no target file and can't guess"
		}
		set out [file rootname $template]
	}

	set outdir [file dirname $out]

	# Make sure the directory exists
	file mkdir $outdir

	# Set up srcdir and top_srcdir to be relative to the target dir
	define srcdir [relative-path [file join $::autosetup(srcdir) $outdir] $outdir]
	define top_srcdir [relative-path $::autosetup(srcdir) $outdir]

	set mapping {}
	foreach {n v} [array get ::define] {
		lappend mapping @$n@ $v
	}
	set result {}
	foreach line [split [readfile $infile] \n] {
		if {[info exists cond]} {
			set l [string trimright $line]
			if {$l eq "@endif"} {
				unset cond
				continue
			}
			if {$l eq "@else"} {
				set cond [expr {!$cond}]
				continue
			}
			if {$cond} {
				lappend result $line
			}
			continue
		}
		if {[regexp {^@if\s+(\w+)(.*)} $line -> name expression]} {
			lassign $expression equal value
			set varval [get-define $name ""]
			if {$equal eq ""} {
				set cond [expr {$varval ni {"" 0}}]
			} else {
				set cond [expr {$varval eq $value}]
				if {$equal ne "=="} {
					set cond [expr {!$cond}]
				}
			}
			continue
		}
		lappend result $line
	}
	writefile $out [string map $mapping [join $result \n]]\n

	msg-result "Created [relative-path $out] from [relative-path $template]"
}

# build/host tuples and cross-compilation prefix
set build [lindex [opt-val build] end]
define build_alias $build
if {$build eq ""} {
	define build [config_guess]
} else {
	define build [config_sub $build]
}

set host [lindex [opt-val host] end]
define host_alias $host
if {$host eq ""} {
	define host [get-define build]
	set cross ""
} else {
	define host [config_sub $host]
	set cross $host-
}
define cross [get-env CROSS $cross]

# build/host _cpu, _vendor and _os
foreach type {build host} {
	set v [get-define $type]
	if {![regexp {^([^-]+)-([^-]+)-(.*)$} $v -> cpu vendor os]} {
		user-error "Invalid canonical $type: $v"
	}
	define ${type}_cpu $cpu
	define ${type}_vendor $vendor
	define ${type}_os $os
}

set prefix [lindex [opt-val prefix $defaultprefix] end]

# These are for compatibility with autoconf
define target [get-define host]
define prefix $prefix
define builddir $autosetup(builddir)
define srcdir $autosetup(srcdir)
# Allow this to come from the environment
define top_srcdir [get-env top_srcdir [get-define srcdir]]

# autoconf supports all of these
set exec_prefix [lindex [opt-val exec-prefix $prefix] end]
define exec_prefix $exec_prefix
foreach {name defpath} {
	bindir /bin
	sbindir /sbin
	libexecdir /libexec
	libdir /lib
} {
	define $name [lindex [opt-val $name $exec_prefix$defpath] end]
}
foreach {name defpath} {
	datadir /share
	sysconfdir /etc
	sharedstatedir /com
	localstatedir /var
	infodir /share/info
	mandir /share/man
	includedir /include
} {
	define $name [lindex [opt-val $name $prefix$defpath] end]
}

define SHELL [get-env SHELL [find-an-executable sh bash ksh]]

# Windows vs. non-Windows
switch -glob -- [get-define host] {
	*-*-ming* - *-*-cygwin - *-*-msys {
		define-feature windows
		define EXEEXT .exe
	}
	default {
		define EXEEXT ""
	}
}

# Display
msg-result "Host System...[get-define host]"
msg-result "Build System...[get-define build]"
