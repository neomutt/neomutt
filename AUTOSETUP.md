# Autosetup build system for NeoMutt

This document explains the changes introduced to NeoMutt's build system by
switching to an Autosetup-based configuration and the rationale behind some of
the choices that have been made.

## Introduction

"[Autosetup](https://msteveb.github.io/autosetup/) is a tool, similar to the
Autotools family, to configure a build system for the appropriate environment,
according to the system capabilities and the user-selected options."

The website explains in great details the major goals of the project and the
similarities and differences with Autotools. For the sake of this short
introduction, I'll summarize the major points, as relevant to Neomutt.

### Zero-dependency

In general, Autotools-based systems read a script named `configure.ac`, which
is written in a mix of m4 and shell called M4sh. This script is translated into
a portable shell script named `configure` by means of `autoconf` and other
support tools.
Autosetup, on the other hand, reads and directly runs a configuration script,
usually named `auto.def`. The script and the support modules in the
`autosetup/` directory are written in [Tcl](https://tcl.tk). The major result
of this choice is that there is no need for an initial translation to a
portable environment.  Autosetup ships with a minimal implementation of Tcl
called [Jim](http://jim.tcl.tk), which is compiled and used on-demand, if no
full Tcl shell is found in the path.  Projects ship a `configure` script that
can be directly run.

So, this

```sh
autoreconf --install && ./configure && make
```

becomes

```sh
./configure && make
```

**Bottom line**: no build-time dependencies, faster configure stage, higher
level of debuggability of the build scripts, no more "autoconf before ship".

### Simple and consistent options system

Autosetup allows users to personalize the build at configure time. Unlike
Autotools, the object model for the options system is simple and consistent.
There are two types of options: booleans and strings. Both can be specified to
have default values. The options are defined in a self-explanatory `options`
section (it's actually a proc under the hood):

```
options {
   smime=1                    => "Disable S/Mime"
   flock=0                    => "Enable flock(1)"
   gpgme=0                    => "Enable GPGME"
   with-gpgme:path            => "Location of GPGME"
   with-mailpath:=/var/mail   => "Location of the spool mailboxes"
}
```

A user can configure the build to suit his needs by modifying the default
values, e.g.,
`./configure --disable-smime --enable-flock --gpgme --with-gpgme=/usr/local`.

Within `auto.def`, option can be easily queried with `[opt-bool smime]` and
`[opt-val with-gpgme $prefix]`, with the latter using `$prefix` if not value
was given. In the above example, `[opt-val with-mailpath]` will return the
default value `/var/mail` if not overridden by the user.

**Bottom line**: no more confusion due to the differences and similarities
between `--with-opt`, `--enable-opt`, `with_val`, `without_val`.  Simple and
self-documenting API for managing configure options.

### Focus on features

Autotools comes with high level primitives, which allow to focus on the
features to be tested. In the ~850 lines of our `auto.def` file - compare to
the current 970 lines in configure.ac - there is almost no boilerplate code.
The code is self-explanatory and easily readable - yes, it is Tcl and it might
take a little getting used to, but it's nothing compared to M4sh.

**Bottom line**: readable and debuggable configure script, no M4sh quoting
intricacies, easily extensible.

## Autosetup for Neomutt

In this section, I'll explain a few design decisions I took when porting
NeoMutt's build system to Autosetup.

### Non-recursive Makefiles

The build system is driven by the top-level Makefile, which includes additional
Makefiles from the subdirectories `doc`, `contrib`, and `po`. I'll stress that
these Makefiles are included by the main Makefile and *not* invoked recursively
(google for "recursive make considered harmful"). The build system relies on
the fact that each of the sub-makefiles defines its own targets, conventionally
named all-*subdir*, clean-*subdir*, install-*subdir*, and uninstall-*subdir*.
For example, `po/Makefile` defines the targets `all-po`, `clean-po`,
`install-po`, and `uninstall-po`. To add a new subdir named `mydir` to the
build system, follow these steps:

1. create `mydir/Makefile.autosetup`
2. define the target `all-mydir`, `clean-mydir`, `install-mydir`, and
   `uninstall-mydir`
3. update the `subdirs` variable definition in `auto.def`

The top-level Makefile will invoke your targets as dependencies for the main
`all`, `clean`, `install`, and `uninstall` targets.

### Configuration options

For a list of the currently supported options and a brief help text, please run
`./configure.autosetup --help`.

### Installation / uninstallation

Two parameters play an important role when deciding where to install NeoMutt.
The first is the `--prefix` parameter to configure. The second is the `DESTDIR`
variable used by Makefiles.

The parameter `--prefix` is used to specify both the default search path for
headers and libraries and the final directory structure of the installed files.
These are often the same: if you have your dependencies installed in
`/usr/include` and `/usr/lib`, you also probably want the NeoMutt executable to
end up in `/usr/bin` and its documentation in `/usr/share/doc`. This behavior
can be tweaked by specifying where 3rd party dependencies are to be found. This
is done on a per-dependency basis using the `--with-<dep>=path` family of
options. As an example, a GPGMe installation in `/opt` can be looked up using
the arguments `--gpgme --with-gpgme=/opt`.

The second parameter, the `DESTDIR` make variable, is used for staged builds
and is prepended to the final path. This allows to stage the whole installation
into `./tmp` by simply using a make invokation like `make DESTDIR=./tmp
install`.
Staged builds are used by downstream packagers and allow to track the list of
files installed by a package: it is easier to `find ./tmp -type f` than to
snapshot the whole `/` filesystem and figure out the modifications introduced
by installing a package. This information is usually used to list the contents
of an installed package or to uninstall it.
