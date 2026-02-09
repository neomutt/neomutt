# Installing NeoMutt

NeoMutt is a command-line email client (Mail User Agent) that's fast, powerful, and flexible.

The simplest way to obtain NeoMutt is to use your operating system's package manager.
See the [distro page](https://neomutt.org/distro.html) to find a package for your system.

## Obtaining the Source

The source can be downloaded from our project site on GitHub.
It's available as a git repository or a 'tar.gz' archive.

### Git

Clone the NeoMutt repository:

```
git clone https://github.com/neomutt/neomutt
```

We recommend using the latest tagged version.
The 'main' branch is used for development and may be less stable.

### Source Archive

The latest release of NeoMutt can be found here:

- https://github.com/neomutt/neomutt/releases/latest

All source releases are signed for security.
See [Signing Releases](https://neomutt.org/dev/signing#source-example) for details.

## Building NeoMutt

To build NeoMutt, you will need at minimum:

- A C11-compatible compiler such as GCC or Clang
- A SysV-compatible curses library: ncurses
- Common libraries such as iconv and regex
- DocBook XSL stylesheets and DTDs (for building the documentation)

NeoMutt's build system uses [Autosetup](https://msteveb.github.io/autosetup/).
It depends on [Tcl](https://tcl.tk) and [Jim](http://jim.tcl.tk) to run its test scripts.
If they are not available, Autosetup will use a version bundled with NeoMutt.

### Configure

The Autosetup `configure` script performs two tasks: it allows the user to
enable or disable certain features of NeoMutt, and it checks that all the build
dependencies are present.

For a list of the currently supported options and a brief help text, run:
`./configure --help`

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
| `--homespool`           | Path | Spooled mail is in user's home directory     |
|                         |      |                                              |
| `--prefix`              | Path | Target directory for build (default: `/usr`) |
| `--disable-doc`         |      | Don't build the docs                         |
| `--full-doc`            |      | Document disabled features                   |
| `--quiet`               |      | Only show the summary                        |

The options marked "Path" either take a path or have an additional option for
specifying the library path.
For example: `./configure --notmuch --with-notmuch=/usr/local/lib/notmuch`

The parameter `--prefix` specifies both the default search path for
headers and libraries and the final installation directory structure.
These are often the same: if you have dependencies installed in
`/usr/include` and `/usr/lib`, you probably want the NeoMutt executable
to end up in `/usr/bin` and its documentation in `/usr/share/doc`. This behavior
can be tweaked by specifying where third-party dependencies are located using
the `--with-<dep>=path` family of options. For example, a GPGME installation in
`/opt` can be found using the arguments `--gpgme --with-gpgme=/opt`.

The build can be adjusted by setting any of six environment variables:

- `CC`            - set the compiler
- `CFLAGS`        - replace **all** compiler flags
- `EXTRA_CFLAGS`  - append flags to default compiler flags
- `LD`            - set the linker
- `LDFLAGS`       - replace **all** linker flags
- `EXTRA_LDFLAGS` - append flags to default linker flags

For example: `make EXTRA_CFLAGS=-g`

Here are the sample commands to configure and build NeoMutt:

```
./configure --gnutls --gpgme --gss --sasl --tokyocabinet
make
```

### Install / Uninstall

NeoMutt will be installed into the directory configured with `--prefix`.
This can be modified using the `DESTDIR` make variable, for example, when performing
staged builds. `DESTDIR` is prepended to the install path.

To install NeoMutt to the configured location, run:

```
make install
```

To override the root directory, use the `DESTDIR` make variable. For example:

```
make DESTDIR=$HOME/install install
```

To uninstall NeoMutt from the configured location, run:

```
make uninstall
```

To override the root directory, use the `DESTDIR` make variable. For example:

```
make DESTDIR=$HOME/install uninstall
```

