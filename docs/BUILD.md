# Building NeoMutt

## Install dependencies

**Prerequisites**: Requires a C compiler, ncurses, and optional libraries like GPGME for encryption.

## Obtain the source

The NeoMutt project is hosted on GitHub, so there are two main options to get the sources – either as Git repository or GitHub archive file.

### Git

Cloning the [main branch](https://github.com/neomutt/neomutt/tree/main) but also see our [list of branches](https://neomutt.org/dev/github/branches).

```
git clone https://github.com/neomutt/neomutt
```

Same as above but pointing to an other branch, e.g. `sample/summary`, for checkout.

```
git clone -b sample/summary https://github.com/neomutt/neomutt
```

### GitHub

Latest release

<https://github.com/neomutt/neomutt/releases/latest>

Select a source package (Tar or ZIP archive) and the checksum file for download. **Important**, verify the tarball/ZIP archive file to ensure its integrity, see [Signing Code / Releases](https://neomutt.org/dev/signing#source-example) for an example.

Specific branch

<https://github.com/neomutt/neomutt/archive/main.zip>

Note, archive file verification isn't possible here because this is not a NeoMutt release archive and thus no checksum file is available. Consider to use Git instead (as mentioned [above](#git)) to clone the repository and checkout the specific branch afterwards.

## Configure

List supported options to adapt or fine tune NeoMutt's build.

```
./configure --help
```

### Configure options

This is not a comprehensive list of configure options. Check `./configure --help` for full help. The options marked "Path" have either take a path, or have an extra option for specifying the library path.

e.g. `./configure --notmuch --with-notmuch=/usr/local/lib/notmuch`

| Configure option | Path | Notes |
| ---------------- | ---- | ----- |
| `--everything` |  | Enable all options |
|  |  |  |
| `--with-ncurses=path` |  | Location of ncurses |
|  |  |  |
| `--gpgme` | Path | GPG Made Easy |
| `--gnutls` | Path | Gnu TLS (SSL) |
| `--gss` | Path | Generic Security Services |
| `--sasl` | Path | Simple Authentication and Security Layer |
| `--ssl` | Path | OpenSSL |
|  |  |  |
| `--fmemopen` |  | Optional Feature (Dangerous) |
| `--lua` | Path | Optional Feature |
| `--notmuch` | Path | Optional Feature |
|  |  |  |
| `--bdb` | Path | Header cache backend |
| `--gdbm` | Path | Header cache backend |
| `--kyotocabinet` | Path | Header cache backend |
| `--lmdb` | Path | Header cache backend |
| `--qdbm` | Path | Header cache backend |
| `--tokyocabinet` | Path | Header cache backend |
|  |  |  |
| `--disable-fcntl` |  | fcntl(2) file locking |
| `--flock` |  | flock(2) file locking |
| `--locales-fix` |  | Workaround for broken locales |
| `--disable-nls` | Path | National Language Support (translations) |
| `--disable-pgp` | Path | Pretty Good Privacy |
| `--disable-smime` | Path | Secure/Multipurpose Internet Mail Extensions |
| `--disable-idn` | Path | Internationalised domain names |
|  |  |  |
| `--with-domain=DOMAIN` |  | Default email domain |
| `--with-mailpath` | Path | Location of spooled mail |
| `--homespool` | Path | Spooled mail is in user's home dir |
|  |  |  |
| `--prefix` | Path | Target directory for build (default: /usr) |
| `--disable-doc` |  | Don't build the docs |
| `--full-doc` |  | Document disabled features |
| `--quiet` |  | Only show the summary |

## Build

Targets: **neomutt**, **test**, ...

The build can be adjusted by setting any of six environment variables:

* `CC` - set the compiler
* `CFLAGS` - replace **all** the compiler flags
* `EXTRA_CFLAGS` - append flags to the default compiler flags
* `LD` - set the linker
* `LDFLAGS` - replace **all** the linker flags
* `EXTRA_LDFLAGS` - append flags to the default linker flags

e.g. `make EXTRA_CFLAGS=-g`

```
make
```

## Install

```
make install
```

## Uninstall

```
make uninstall
```
