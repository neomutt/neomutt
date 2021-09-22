# Installing NeoMutt

NeoMutt is a command line email client (Mail User Agent).
It's fast, powerful and flexible.

The simplest way to obtain NeoMutt is to use your OS's package.
See the [distro page](https://neomutt.org/distro.html) to see if there's one for you.

## Obtaining the Source

The source can be downloaded from our project site on GitHub.
It's available as a git repository or a 'tar.gz' archive.

### Git

Clone the NeoMutt repository:

```
git clone https://github.com/neomutt/neomutt
```

It's recommended to use the latest tagged version.
The 'master' branch is used for development and may not be as stable.

### Source Archive

The latest release of NeoMutt can be found, here:

- https://github.com/neomutt/neomutt/releases/latest

All source releases are signed for security.
See [Signing Releases](https://neomutt.org/dev/signing#source-example) for details.

## Building NeoMutt

To build NeoMutt, you will need, at the very minimum:

- A C99 compiler such as gcc or clang
- SysV-compatible curses library: ncurses
- Some common libraries, such as iconv and regex
- DocBook XSL stylesheets and DTDs (for building the docs)

NeoMutt's build system uses [Autosetup](https://msteveb.github.io/autosetup/).
It depends on [Tcl](https://tcl.tk) and [Jim](http://jim.tcl.tk) to run its test scripts.
If they aren't available, Autosetup will use a version bundled with NeoMutt.

### Configure

Autosetup's  `configure.autosetup` performs two tasks.  It allows the user to
enable/disable certain features of NeoMutt and it checks that all the build
dependencies are present.

For a list of the currently supported options and a brief help text, run:
`./configure.autosetup --help`

| Configure option        | Path | Notes                                        |
| :---------------------- | :--- | :------------------------------------------- |
| `--with-ncurses=path`   |      | Location of ncurses                          |
|                         |      |                                              |
| `--gpgme`               | Path | GPG Made Easy                                |
| `--gnutls`              | Path | Gnu TLS (SSL)                                |
| `--gss`                 | Path | Generic Security Services                    |
| `--sasl`                | Path | Simple Authentication and Security Layer     |
| `--ssl`                 | Path | OpenSSL                                      |
|                         |      |                                              |
| `--fmemopen`            |      | Optional Feature (Dangerous)                 |
| `--lua`                 | Path | Optional Feature                             |
| `--notmuch`             | Path | Optional Feature                             |
| `--mixmaster`           |      | Optional Feature                             |
|                         |      |                                              |
| `--bdb`                 | Path | Header cache backend                         |
| `--gdbm`                | Path | Header cache backend                         |
| `--kyotocabinet`        | Path | Header cache backend                         |
| `--lmdb`                | Path | Header cache backend                         |
| `--qdbm`                | Path | Header cache backend                         |
| `--tokyocabinet`        | Path | Header cache backend                         |
|                         |      |                                              |
| `--with-lock=CHOICE`    |      | Select 'fcntl' or 'flock'                    |
| `--locales-fix`         |      | Workaround for broken locales                |
| `--disable-nls`         | Path | National Language Support (translations)     |
| `--disable-pgp`         | Path | Pretty Good Privacy                          |
| `--disable-smime`       | Path | Secure/Multipurpose Internet Mail Extensions |
| `--disable-idn`         | Path | Internationalised domain names               |
|                         |      |                                              |
| `--with-domain=DOMAIN`  |      | Default email domain                         |
| `--with-mailpath`       | Path | Location of spooled mail                     |
| `--homespool`           | Path | Spooled mail is in user's home dir           |
|                         |      |                                              |
| `--prefix`              | Path | Target directory for build (default: `/usr`) |
| `--disable-doc`         |      | Don't build the docs                         |
| `--full-doc`            |      | Document disabled features                   |
| `--quiet`               |      | Only show the summary                        |

The options marked "Path" have either take a path, or have an extra option for
specifying the library path.
e.g.  `./configure --notmuch --with-notmuch=/usr/local/lib/notmuch`

The parameter `--prefix` is used to specify both the default search path for
headers and libraries and the final directory structure of the installed files.
These are often the same: if you have your dependencies installed in
`/usr/include` and `/usr/lib`, you also probably want the NeoMutt executable to
end up in `/usr/bin` and its documentation in `/usr/share/doc`. This behavior
can be tweaked by specifying where 3rd party dependencies are to be found. This
is done on a per-dependency basis using the `--with-<dep>=path` family of
options. As an example, a GPGME installation in `/opt` can be looked up using
the arguments `--gpgme --with-gpgme=/opt`.

The build can be adjusted by setting any of six environment variables:

- `CC`            - set the compiler
- `CFLAGS`        - replace **all** the compiler flags
- `EXTRA_CFLAGS`  - append flags to the default compiler flags
- `LD`            - set the linker
- `LDFLAGS`       - replace **all** the linker flags
- `EXTRA_LDFLAGS` - append flags to the default linker flags

e.g.  `make EXTRA_CFLAGS=-g`

Here are the sample commands to configure and build NeoMutt:

```
./configure --gnutls --gpgme --gss --sasl --tokyocabinet
make
```

### Install / Uninstall

NeoMutt will be installed into the directory configured with `--prefix`.
This can be modified using the `DESTDIR` make variable, for example when doing
staged builds.  `DESTDIR` is prepended to the install path.

To install NeoMutt to the configured location, run:

```
make install
```

To override the root directory, use the `DESTDIR` make variable, e.g.

```
make DESTDIR=$HOME/install install'
```

To uninstall NeoMutt from the configured location, run:

```
make uninstall
```

To override the root directory, use the `DESTDIR` make variable, e.g.

```
make DESTDIR=$HOME/install uninstall'
```

