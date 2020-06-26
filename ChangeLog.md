2020-06-26  Richard Russon  <rich@flatcap.org>
* Bug Fixes
  - Avoid opening the same hcache file twice
  - Re-open Mailbox after folder-hook
  - Fix the matching of the spoolfile Mailbox
  - Fix link-thread to link all tagged emails
* Changed Config
  - Add $tunnel_is_secure config, defaulting to true
* Upstream
  - Don't check IMAP PREAUTH encryption if $tunnel is in use
  - Add recommendation to use $ssl_force_tls

2020-06-19  Richard Russon  <rich@flatcap.org>
* Security
  - Abort GnuTLS certificate check if a cert in the chain is rejected
  - TLS: clear data after a starttls acknowledgement
  - Prevent possible IMAP MITM via PREAUTH response
* Features
  - add config operations +=/-= for number,long
  - Address book has a comment field
  - Query menu has a comment field
* Contrib
  - sample.neomuttrc-starter: Do not echo promted password
* Bug Fixes
  - make "news://" and "nntp://" schemes interchangeable
  - Fix CRLF to LF conversion in base64 decoding
  - Double comma in query
  - compose: fix redraw after history
  - Crash inside empty query menu
  - mmdf: fix creating new mailbox
  - mh: fix creating new mailbox
  - mbox: error out when an mbox/mmdf is a pipe
  - Fix list-reply by correct parsing of List-Post headers
  - Decode references according to RFC2047
  - fix tagged message count
  - hcache: fix keylen not being considered when building the full key
  - sidebar: fix path comparison
  - Don't mess with the original pattern when running IMAP searches
  - Handle IMAP "NO" resps by issuing a msg instead of failing badly
  - imap: use the connection delimiter if provided
  - Memory leaks
* Changed Config
  - `$alias_format` default changed to include `%c` comment
  - `$query_format` default changed to include `%e` extra info
* Translations
  - 100% Lithuanian
  - 84% French
  - Log the translation in use
* Docs
  - Add missing commands unbind, unmacro to man pages
* Build
  - Check size of long using `LONG_MAX` instead of `__WORDSIZE`
  - Allow ./configure to not record cflags
  - fix out-of-tree build
  - Avoid locating gdbm symbols in qdbm library
* Code
  - Refactor unsafe TAILQ returns
  - add window notifications
  - flip negative ifs
  - Update to latest acutest.h
  - test: add store tests
  - test: add compression tests
  - graphviz: email
  - make more opcode info available
  - refactor: `main_change_folder()`
  - refactor: `mutt_mailbox_next()`
  - refactor: `generate_body()`
  - compress: add `{min,max}_level` to ComprOps
  - emphasise empty loops: "// do nothing"
  - prex: convert `is_from()` to use regex
  - Refactor IMAP's search routines

2020-05-01  Richard Russon  <rich@flatcap.org>
* Bug Fixes
  - Make sure buffers are initialized on error
  - fix(sidebar): use abbreviated path if possible
* Translations
  - 100% Lithuanian
* Docs
  - make header cache config more explicit

2020-04-24  Richard Russon  <rich@flatcap.org>
* Bug Fixes
  -Fix history corruption
  -Handle pretty much anything in a URL query part
  -Correctly parse escaped characters in header phrases
  -Fix crash reading received header
  -Fix sidebar indentation
  -Avoid crashing on failure to parse an IMAP mailbox
  -Maildir: handle deleted emails correctly
  -Ensure OP_NULL is always first
* Translations
  -100% Czech
* Build
  -cirrus: enable pcre2, make pkgconf a special case
  -Fix finding pcre2 w/o pkgconf
  -build: tdb.h needs size_t, bring it in with stddef.h

2020-04-17  Richard Russon  <rich@flatcap.org>
* Features
  - Fluid layout for Compose Screen, see: https://vimeo.com/407231157
  - Trivial Database (TDB) header cache backend
  - RocksDB header cache backend
  - Add \<sidebar-first\> and \<sidebar-last\> functions
* Bug Fixes
  - add error for CLI empty emails
  - Allow spaces and square brackets in paths
  - browser: fix hidden mailboxes
  - fix initial email display
  - notmuch: fix time window search.
  - fix resize bugs
  - notmuch: fix entire-thread: update current email pointer
  - sidebar: support indenting and shortening of names
  - Handle variables inside backticks in sidebar_whitelist
  - browser: fix mask regex error reporting
* Translations
  - 100.00% Lithuanian
  - 98.88% Chinese (simplified)
* Build
  - Use regexes for common parsing tasks: urls, dates
  - Add configure option `--pcre2` -- Enable PCRE2 regular expressions
  - Add configure option `--tdb` -- Use TDB for the header cache
  - Add configure option `--rocksdb` -- Use RocksDB for the header cache
  - Create libstore (key/value backends)
  - Update to latest autosetup
  - Update to latest acutest.h
  - Rename `doc/` directory to `docs/`
  - make: fix location of .Po dependency files
  - Change libcompress to be more universal
  - Fix test fails on х32
  - fix uidvalidity to unsigned 32-bit int
* Code
  - Increase test coverage
  - Fix memory leaks
  - Fix null checks
* Upstream
  - Buffer refactoring
  - Fix use-after-free in mutt_str_replace()
  - Clarify PGP Pseudo-header S\<id\> duration
  - Try to respect MUTT_QUIET for IMAP contexts too
  - Limit recurse depth when parsing mime messages

2020-03-20  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - Fix COLUMNS env var
  - Fix sync after delete
  - Fix crash in notmuch
  - Fix sidebar indent
  - Fix emptying trash
  - Fix command line sending
  - Fix reading large address lists
  - Resolve symlinks only when necessary
* Translations
  - 100% Lithuanian
  - 96% Spanish
* Docs
  - Include OpenSSL/LibreSSL/GnuTLS version in neomutt -v output
  - Fix case of GPGME and SQLite
* Build
  - Create libcompress (lz4, zlib, zstd)
  - Create libhistory
  - Create libbcache
  - Move zstrm to libconn
* Code
  - Add more test coverage
  - Rename magic to type
  - Use mutt_file_fopen() on config variables
  - Change commands to use intptr_t for data

2020-03-13  Richard Russon  \<rich@flatcap.org\>
* Features
  - UI: add number of old messages to sidebar_format
  - UI: support ISO 8601 calendar date
  - UI: fix commands that don’t need to have a non-empty mailbox to be valid
  - PGP: inform about successful decryption of inline PGP messages
  - PGP: try to infer the signing key from the From address
  - PGP: enable GPGME by default
  - Notmuch: use query as name for vfolder-from-query
  - IMAP: add network traffic compression (COMPRESS=DEFLATE, RFC4978)
  - Header cache: add support for generic header cache compression
* Bug Fixes
  - Fix uncollapse_jump
  - Only try to perform entire-thread on maildir/mh mailboxes
  - Fix crash in pager
  - Avoid logging single new lines at the end of header fields
  - Fix listing mailboxes
  - Do not recurse a non-threaded message
  - Fix initial window order
  - Fix leaks on IMAP error paths
  - Notmuch: compose(attach-message): support notmuch backend
  - Fix IMAP flag comparison code
  - Fix $move for IMAP mailboxes
  - Maildir: maildir_mbox_check_stats should only update mailbox stats if requested
  - Fix unmailboxes for virtual mailboxes
  - Maildir: sanitize filename before hashing
  - OAuth: if 'login' name isn't available use 'user'
  - Add error message on failed encryption
  - Fix a bunch of crashes
  - Force C locale for email date
  - Abort if run without a terminal
* Changed Config
  - `$crypt_use_gpgme`                         - Now defaults to 'yes' (enabled)
  - `$abort_backspace`                         - Hitting backspace against an empty prompt aborts the prompt
  - `$abort_key`                               - String representation of key to abort prompts
  - `$arrow_string`                            - Use an custom string for arrow_cursor
  - `$crypt_opportunistic_encrypt_strong_keys` - Enable encryption only when strong a key is available
  - `$header_cache_compress_dictionary`        - Filepath to dictionary for zstd compression
  - `$header_cache_compress_level`             - Level of compression for method
  - `$header_cache_compress_method`            - Enable generic hcache database compression
  - `$imap_deflate`                            - Compress network traffic
  - `$smtp_user`                               - Username for the SMTP server
* Translations
  - 100% Lithuanian
  - 81% Spanish
  - 78% Russian
* Build
  - Add libdebug
  - Rename public headers to lib.h
  - Create libcompress for compressed folders code
  - Enable Cirrus CI for FreeBSD
* Code
  - Refactor Windows and Dialogs
  - Lots of code tidying
  - Refactor: mutt_addrlist\_{search,write}
  - Lots of improvements to the Config code
  - Use Buffers more pervasively
  - Unify API function naming
  - Rename library shared headers
  - Refactor libconn gui dependencies
  - Refactor: init.[ch]
  - Refactor config to use subsets
  - Config: add path type
  - Remove backend deps from the connection code
* Upstream
  - Allow ~b ~B ~h patterns in send2-hook
  - Rename smime oppenc mode parameter to get_keys_by_addr()
  - Add $crypt_opportunistic_encrypt_strong_keys config var
  - Fix crash when polling a closed ssl connection
  - Turn off auto-clear outside of autocrypt initialization
  - Add protected-headers="v1" to Content-Type when protecting headers
  - Fix segv in IMAP postponed menu caused by reopen_allow
  - Adding ISO 8601 calendar date
  - Fix $fcc_attach to not prompt in batch mode
  - Convert remaining mutt_encode_path() call to use struct Buffer
  - Fix rendering of replacement_char when Charset_is_utf8
  - Update to latest acutest.h

2019-12-07  Richard Russon  \<rich@flatcap.org\>
* Features
  - compose: draw status bar with highlights
* Bug Fixes
  - crash opening notmuch mailbox
  - crash in mutt_autocrypt_ui_recommendation
  - Avoid negative allocation
  - Mbox new mail
  - Setting of DT_MAILBOX type variables from Lua
  - imap: empty cmdbuf before connecting
  - imap: select the mailbox on reconnect
  - compose: fix attach message
* Build
  - make files conditional
  - add gpgme check for RHEL6
* Code
  - enum-ify log levels
  - fix function prototypes
  - refactor virtual email lookups
  - factor out global Context

2019-11-29  Richard Russon  \<rich@flatcap.org\>
* Features
  - Add raw mailsize expando (%cr)
* Bug Fixes
  - Avoid double question marks in bounce confirmation msg
  - Fix bounce confirmation
  - fix new-mail flags and behaviour
  - fix: browser \<descend-directory\>
  - fix ssl crash
  - fix move to trash
  - fix flickering
  - Do not check hidden mailboxes for new mail
  - Fix new_mail_command notifications
  - fix crash in examine_mailboxes()
  - fix crash in mutt_sort_threads()
  - fix: crash after sending
  - Fix crash in tunnel's conn_close
  - fix fcc for deep dirs
  - imap: fix crash when new mail arrives
  - fix colour 'quoted9'
  - quieten messages on exit
  - fix: crash after failed mbox_check
  - browser: default to a file/dir view when attaching a file
* Changed Config
  - Change $write_bcc to default off
* Translations
  - 100% Portuguese (Brazil)
  -  92% Polish
* Docs
  - Add a bit more documentation about sending
  - Clarify $write_bcc documentation.
  - Update documentation for raw size expando
  - docbook: set generate.consistent.ids to make generated html reproducible
* Build
  - fix build/tests for 32-bit arches
  - tests: fix test that would fail soon
  - tests: fix context for failing idna tests

2019-11-11  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - browser: fix directory view
  - fix crash in mutt_extract_token()
  - force a screen refresh
  - fix crash sending message from command line
  - notmuch: use nm_default_url if no mailbox data
  - fix forward attachments
  - fix: vfprintf undefined behaviour in body_handler
  - Fix relative symlink resolution
  - fix: trash to non-existent file/dir
  - fix re-opening of mbox Mailboxes
  - close logging as late as possible
  - log unknown mailboxes
  - fix crash in command line postpone
  - fix memory leaks
  - fix icommand parsing
  - fix new mail interaction with mail_check_recent

2019-11-02  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - Mailboxes command with empty backticks
  - Mbox save-to-trash
  - Mkdir for new maildir folders
  - Maildir: new mail detection
  - Truncation of "set" command on a path variable
  - Update crash (when changing folder)
  - Resolve symbolic links when saving a message
  - Folder-hook calling "unmailboxes \*"
  - Failed ssl negotiation
  - Crash when using "alias -group"
  - LibIDN error when charset wasn't set
  - Notmuch abort entire-thread if database lacks message

2019-10-25  Richard Russon  \<rich@flatcap.org\>
* Features
  - Add $fcc_before_send, defaulting unset
  - Deprecate TLS 1.0 and 1.1 by default
  - Turn on $ssl_force_tls by default
  - Command line -z and -Z options to work with IMAP
  - Add size display configuration variables
  - Summary pages: version, set, set all, bind, macro
  - CONDSTORE and QRESYNC support
  - OAUTHBEARER support
  - inotify support
  - add index-format-hook
  - Add $auto_subscribe variable
  - Allow relative date hour/min/sec offsets
  - Add attributes support on color declarations
  - Style Menu Options
  - Add new pattern type ~I for external searches
  - Add \<named-mailboxes\> command
* Changed Config
  - folder_format
  - pgp_use_gpg_agent
  - shell
  - ssl_force_tls
  - ssl_use_tlsv1
  - ssl_use_tlsv1_1
  - status_format
  - to_chars
  - user_agent
* New Config
  - attach_save_dir
  - attach_save_without_prompting
  - autocrypt
  - autocrypt_acct_format
  - autocrypt_dir
  - autocrypt_reply
  - auto_subscribe
  - crypt_chars
  - crypt_protected_headers_read
  - crypt_protected_headers_save
  - crypt_protected_headers_subject
  - crypt_protected_headers_write
  - external_search_command
  - fcc_before_send
  - forward_attachments
  - imap_condstore
  - imap_fetch_chunk_size
  - imap_oauth_refresh_command
  - imap_qresync
  - imap_rfc5161
  - include_encrypted
  - nm_flagged_tag
  - nm_replied_tag
  - pop_oauth_refresh_command
  - sidebar_non_empty_mailbox_only
  - size_show_bytes
  - size_show_fractions
  - size_show_mb
  - size_units_on_left
  - smtp_oauth_refresh_command
  - ssl_use_tlsv1_3
* New Commands
  - index-format-hook
  - named-mailboxes
  - unbind
  - unmacro
* New Functions
  - autocrypt-acct-menu
  - autocrypt-menu
  - compose-to-sender
  - create-account
  - delete-account
  - descend-directory
  - group-chat-reply
  - mailbox-cycle
  - mailbox-list
  - toggle-active
  - toggle-prefer-encrypt
  - toggle-write
  - vfolder-from-query-readonly
* Bug Fixes
  - Fix crashes
  - Fix memory leaks
  - Fix undefined behaviour
  - Fix coverity defects
* Translations
  - 100% Lithuanian
  - 100% Chinese (Simplified)
  - 100% Portuguese (Brazil)
  - 95% German
  - 95% Finnish
  - 95% Czech
  - 91% Polish
  - 78% Japanese
  - 73% Dutch
  - 72% Spanish
  - 62% Swedish
  - 55% Slovak
* Docs
  - OpenPGP and S/MIME configuration
  - Quick-starter config section
  - Autocrypt feature
  - "Message Composition Flow" section to manual
  - OAUTH support

2018-07-16  Richard Russon  \<rich@flatcap.org\>
* Features
  - \<check-stats\> function
* Bug Fixes
  - Lots

2018-06-22  Richard Russon  \<rich@flatcap.org\>
* Features
  - Expand variables inside backticks
  - Honour SASL-IR IMAP capability in SASL PLAIN
* Bug Fixes
  - Fix toggle-read
  - Do not truncate shell commands on ; or #
  - pager: index must be rebuilt on MUTT_REOPENED
  - Handle a BAD response in AUTH PLAIN w/o initial response
  - fcc_attach: Don't ask every time
  - Enlarge path buffers PATH_MAX (4096)
  - Move LSUB call from connection establishment to mailbox SELECTion
* Translations
  - Update Chinese (Simplified): 100%
  - Update Czech: 100%
  - Update German: 100%
  - Update Lithuanian: 100%
  - Update Portuguese (Brazil): 100%
  - Update Slovak: 59%
  - Reduce duplication of messages
* Code
  - Tidy up the mailbox API
  - Tidy up the header cache API
  - Tidy up the encryption API
  - Add doxygen docs for more functions
  - Refactor more structs to use STAILQ

2018-05-12  Richard Russon  \<rich@flatcap.org\>
* Features
  - echo command
  - Add $browser_abbreviate_mailboxes
  - Add ~M pattern to match mime Content-Types
  - Add support for multipart/multilingual emails
  - Jump to a collapsed email
  - Add support for idn2 (IDNA2008)
* Bug Fixes
  - Let mutt_ch_choose report conversion failure
  - minor IMAP string handling fixes
* Translations
  - Chinese (Simplified) (100%)
  - Czech (100%)
  - German (100%)
  - Lithuanian (62%)
  - Portuguese (Brazil) (100%)
* Coverity defects
  - match prototypes to their functions
  - make logic clearer
  - reduce scope of variables
  - fix coverity defects
* Docs
  - development: analysis
  - development: easy tasks
  - development: roadmap
* Code
  - start refactoring libconn
  - split out progress functions
  - split out window functions
  - split out terminal setting
  - convert MyVars to use TAILQ
  - split mutt_file_{lock,unlock}
  - Move IDN version string to mutt/idna.c
  - refactor: init_locale()
  - Eliminate static variable in mutt_file_dirname
* Tidy
  - test int functions against 0
  - rename lots of constants
  - rename lots of functions
  - sort lots of fields/definitions
* Upstream
  - Increase account.user/login size to 128
  - Fix comparison of flags with multiple bits set
  - Change mutt_error call in mutt_gpgme_set_sender() to dprint
  - Improve the error message when a signature is missing
  - pager specific "show incoming mailboxes list" macro
  - Improve gss debug printing of status_string
  - Remove trailing null count from gss_buffer_desc.length field
  - Add a comment in auth_gss about RFCs and null-termination
  - Change prompt string for $crypt_verify_sig

2018-03-23  Richard Russon  \<rich@flatcap.org\>
* Features
  - unify logging/messaging
  - add alert (blink) colors
* Contrib
  - Vim syntax for NeoMutt log files
* Bug Fixes
  - Fix progress bar range
  - notmuch: stop if db open fails
  - Improve index color cache flushing behavior
  - lua: fix crash when setting a string
* Translations
  - Update Czech translation (100%)
  - Update German translation (100%)
  - Update Polish translation (94%)
  - Update Portuguese (BR) translation (100%)
  - Update Spanish translation (64%)
  - Update Turkish translation (75%)
  - Merge similar messages
* Docs
  - Clarify precedence of settings in config files
  - Fix subjectrx example in the manual
* Website
  - Update Gentoo distro page
  - Devel: Static analysis
* Build
  - Support —with-sysroot configure arg
  - Expose EXTRA_CFLAGS_FOR_BUILD and EXTRA_LDFLAGS_FOR_BUIlD
  - Update to latest autosetup
  - Make sure git_ver.h doesn't eat random 'g's out of tag names
* Code
  - Refactor to reduce complexity
  - Refactor to reduce variables' scope
  - Sort functions/config to make docs more legible

2018-02-23  Richard Russon  \<rich@flatcap.org\>
* Features
  - browser: `\<goto-parent\>` function bound to "p"
  - editor: `\<history-search\>` function bound to "Ctrl-r"
  - Cygwin support: https://neomutt.org/distro/cygwin
  - openSUSE support: https://neomutt.org/distro/suse
  - Upstream Homebrew support: Very soon - https://neomutt.org/distro/homebrew
* Bug Fixes
  - gmail server-size search
  - nested-if: correctly handle "\<" and "\>" with %?
  - display of special chars
  - lua: enable myvars
  - for pgpewrap in default gpg.rc
  - reply_regexp which wasn't formatted correctly.
  - parsing of urls containing '?'
  - out-of-bounds read in mutt_str_lws_len
* Translations
  - Review fuzzy lt translations
  - Updated French translation
* Website
  - Installation guide for Cygwin
  - Installation guide for openSUSE
  - Installation guide for CRUX
* Build
  - check that DTDs are installed
  - autosetup improvements
  - option for which version of bdb to use
  - drop test for resizeterm -- it's always present
* Code
  - split if's containing assignments
  - doxygen: add/improve comments
  - rename functions / parameters for consistency
  - add missing {}s for clarity
  - move functions to library
  - reduce scope of variables
  - boolify more variables
  - iwyu: remove unnecessary headers
  - name unicode chars
  - tailq: migrate parameter api
  - md5: refactor and tidy
  - rfc2047: refactor and tidy
  - buffer: improvements
  - create unit test framework
  - fix several coverity defects
* Upstream
  - Fix s/mime certificate deletion bug
  - Disable message security if the backend is not available
  - Fix improper signed int conversion of IMAP uid and msn values
  - Change imap literal counts to parse and store unsigned ints
  - Fix imap status count range check
  - cmd_handle_fatal: make error message a bit more descriptive
  - Create pgp and s/mime default and sign_as key vars
  - Add missing setup calls when resuming encrypted drafts
  - mutt_pretty_size: show real number for small files
  - examine_directory: set directory/symlink size to zero
  - Add history-search function, bound to ctrl-r
  - Avoid a potential integer overflow if a Content-Length value is huge

2017-12-15  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - Fix some regressions in the previous release

2017-12-08  Richard Russon  \<rich@flatcap.org\>
* Features
  - Enhance ifdef feature to support my_ vars
  - Add \<edit-or-view-raw-message\>
  - Remove vim syntax file from the main repo
  - Support reading FQDN from mailname files
* Bug Fixes
  - Do not turn CRLF into LF when dealing with transfer-encoding=base64
  - Cleanup "SSL is unavailable" error in mutt_conn_find
  - Don't clear the macro buffer during startup
  - Fixup smart modify-labels-then-hide for !tag case
  - Add sleep after SMTP error
  - Restore folder settings after folder-hook
  - Fix segfault when pipe'ing a deleted message
* Docs
  - Display_filter escape sequence
  - Correct spelling mistakes
  - Add a sentence to quasi-delete docs
  - Modify gpg.rc to accommodate GPG 2.1 changes
* Build
  - Fix build for RHEL6
  - Define NCURSES_WIDECHAR to require wide-char support from ncurses
  - Autosetup: fix check for missing sendmail
  - Respect --with-ssl path
  - Check that OpenSSL md5 supports -r before using it
  - Autosetup: expand --everything in `neomutt -v`
  - Make sure objects are not compiled before git_ver.h is generated
  - Build: fix update-po target
  - Fix out-of-tree builds
  - Fix stdout + stderr redirection in hcachever.sh
  - Build: moved the check for idn before the check for notmuch
  - Define prefix in Makefile.autosetup
  - Install stuff to $(PACKAGE) in $(libexecdir), not $(libdir)
  - Update autosetup to latest master
* Code
  - Rename files
  - Rename functions
  - Rename variables
  - Rename constants
  - Remove unused parameters
  - Document functions
  - Rearrange functions
  - Move functions to libraries
  - Add new library functions
  - Rearrange switch statements
  - Boolification
  - Drop #ifdef DEBUG
  - Fix Coverity defects
  - Insert braces
  - Split ifs
  - Fallthrough
  - Fix shadow variable
  - Replace mutt_debug with a macro
  - Return early where possible
* Upstream
  - Note which ssl config vars are GnuTLS or OpenSSL only
  - Add message count to $move quadoption prompt
  - Add %R (number of read messages) for $status_format
  - Add $change_folder_next option to control mailbox suggestion order
  - Fix $smart_wrap to not be disabled by whitespace-prefixed lines
  - Remove useless else branch in the $smart_wrap code
  - Fix ansi escape sequences with both reset and color parameters

2017-10-27  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - variable type when using fread
  - prevent timezone overflow
  - tags: Show fake header for all backends
  - notmuch: virtual-mailboxes should accept a limit
  - Issue 888: Fix imap mailbox flag logging
  - fix actions on tagged messages
  - call the folder-hook before saving to $record
  - Fix smart wrap in pager without breaking header
  - Add polling for the IDLE command
* Docs
  - imap/notmuch tags: Add some documentation
  - English and other cleanups
  - compressed and nntp features are now always built
* Website
  - Update Arch instructions
* Build
  - Fix update-po
  - Fix neomutt.pot location, remove from git
  - Allow to specify --docdir at configure time
  - Generate neomuttrc even if configured with --disable-doc
  - Let autosetup define PWD, do not unnecessarily try to create hcache dir
  - Use bundled wcscasecmp if an implementation is not found in libc
  - Use host compiler to build the documentation
  - Update autosetup to latest master branch
  - autosetup: delete makedoc on 'make clean'
  - Fixes for endianness detection
  - Update autosetup to latest master branch
  - Do not use CPPFLAGS / CFLAGS together with CC_FOR_BUILD
  - --enable-everything includes lua
  - autosetup: check for sys_siglist[]
* Code
  - move functions to library
  - lib: move MIN/MAX macros
  - simplify null checks
  - kill preproc expansion laziness
  - reduce scope of variables
  - merge: minor code cleanups
  - split up 'if' statements that assign and test
  - Refactor: Remove unused return type
  - Bool: change functions in mx.h
  - bool: convert function parameters in nntp.h
  - add extra checks to mutt_pattern_exec()
  - Use safe_calloc to initialize memory, simplify size_t overflow check
  - Move mutt_rename_file to lib/file.[hc]
  - doxygen: fix a few warnings
  - minor code fixes
  - use mutt_array_size()
  - refactor out O_NOFOLLOW
  - initialise variables
  - lib: move List and Queue into library
  - url: make notmuch query string parser generic
  - Wrap dirname(3) inside a mutt_dirname() function

2017-10-13  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - crash using uncolor
  - Sort the folders list when browsing an IMAP server
  - Prefer a helpful error message over a BEEP
* Build
  - Do not fail if deflate is not in libz
  - Support EXTRA_CFLAGS and EXTRA_LDFLAGS, kill unused variable

2017-10-06  Richard Russon  \<rich@flatcap.org\>
* Features
  - Add IMAP keywords support
* Bug Fixes
  - set mbox_type
  - %{fmt} date format
  - Fix off-by-one buffer overflow in add_index_color
  - crash in mbox_to_udomain
  - crash in mutt_substrdup
  - crash looking up mime body type
  - digest_collapse was broken
  - crash using notmuch expando with imap
  - imap: Fix mx.mbox leak in imap_get_parent_path
  - overflow in mutt_mktime()
  - add more range-checking on dates/times
  - Remove spurious error message
  - Unsubscribe after deleting an imap folder
  - Do not pop from MuttrcStack what wasn't pushed
* Docs
  - replace mutt refs with neomutt
  - drop old vim syntax file
* Code
  - convert functions to use 'bool'
  - convert structs to use STAILQ
* Build
  - Autosetup-based configuration
  - drop upstream mutt references
  - rename everything 'mutt' to 'neomutt'
  - move helper programs to lib dir
  - rename regexp to regex
  - expand buffers to avoid gcc7 warnings
* Upstream
  - Remove \Seen flag setting for imap trash
  - Change imap copy/save and trash to sync flags, excluding deleted
  - Improve imap fetch handler to accept an initial UID
  - Display an error message when delete mailbox fails
  - Updated French translation
  - Fix imap sync segfault due to inactive headers during an expunge
  - Close the imap socket for the selected mailbox on error
  - Add missing IMAP_CMD_POLL flag in imap mailbox check
  - Change maildir and mh check_mailbox to use dynamic sized hash
  - Fix uses of context-\>changed as a counter
  - Make cmd_parse_fetch() more precise about setting reopen/check flags
  - Enable $reply_self for group-reply, even with $metoo unset

2017-09-12  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - broken check on resend message
  - crash in vfolder-from-query
* Build
  - Be more formal about quoting in m4 macros
  - fix warnings raised by gcc7
  - notmuch: add support for the v5 API

2017-09-07  Richard Russon  \<rich@flatcap.org\>
* Contrib
  - Add guix build support
* Bug Fixes
  - Only match real mailboxes when looking for new mail
  - Fix the printing of ncurses version in -v output
  - Bind editor \<delete\> to delete-char
  - Fix overflowing colours
  - Fix empty In-Reply-To generation
  - Trim trailing slash from completed dirs
  - Add guix-neomutt.scm
  - Fix setting custom query_type in notmuch query
* Website
  - New technical documentation LINK
  - Improve Gentoo distro page
* Build
  - Better curses identification
  - Use the system's wchar_t support
  - Use the system's md5 tool (or equivalent)
  - Clean up configure.ac
  - Teach gen-map-doc about the new opcode header
* Source
  - Rename functions (snake_case)
  - Rename constants/defines (UPPER_CASE)
  - Create library of shared functions
  - Much tidying
  - Rename globals to match user config
  - Drop unnecessary functions/macros
  - Use a standard list implementation
  - Coverity fixes
  - Use explicit NUL for string terminators
  - Drop OPS\* in favour of opcodes.h
* Upstream
  - Fix menu color calls to occur before positioning the cursor
  - When guessing an attachment type, don't allow text/plain if there is a null character
  - Add $imap_poll_timeout to allow mailbox polling to time out
  - Handle error if REGCOMP in pager fails when resizing
  - Change recvattach to allow nested encryption
  - Fix attachment check_traditional and extract_keys operations
  - Add edit-content-type helper and warning for decrypted attachments
  - Add option to run command to query attachment mime type
  - Add warning about using inline pgp with format=flowed

2017-07-14  Richard Russon  \<rich@flatcap.org\>
* Translations
  - Update German translation
* Docs
  - compile-time output: use two lists
  - doxygen: add config file
  - doxygen: tidy existing comments
* Build
  - fix hcachever.sh script
* Upstream
  - Fix crash when $postponed is on another server.

2017-07-07  Richard Russon  \<rich@flatcap.org\>
* Features
  - Support Gmail's X-GM-RAW server-side search
  - Include pattern for broken threads
  - Allow sourcing of multiple files
* Contrib
  - vombatidae colorscheme
  - zenburn colorscheme
  - black 256 solarized colorscheme
  - neonwolf colorscheme
  - Mutt logos
* Bug Fixes
  - flags: update the hdr message last
  - gpgme S/MIME non-detached signature handling
  - menu: the thread tree color
  - Uses CurrentFolder to populate LastDir with IMAP
  - stabilise sidebar sort order
  - colour emails with a '+' in them
  - the padding expando '%\>'
  - Do not set old flag if mark_old is false
  - maildir creation
  - Decode CRLF line endings to LF when copying headers
  - score address pattern do not match personal name
  - open attachments in read-only mode
  - Add Cc, In-Reply-To, and References to default mailto_allow
  - Improve search for mime.types
* Translations
  - Update Chinese (Simplified) translation
* Coverity defects
  - dodgy buffers
  - leaks in lua get/set options
  - some resource leaks
* Docs
  - update credits
  - limitations of new-mail %f expando
  - escape \<\>'s in nested conditions
  - add code of conduct
  - fix ifdef examples
  - update mailmap
  - Update modify-labels-then-hide
  - fix mailmap
  - drop UPDATING files
* Website
  - Changes pages (diff)
  - Update Arch distro page
  - Update NixOS distro page
  - Add new Exherbo distro page
  - Update translation hi-score table
  - Update code of conduct
  - Update Newbies page
  - Add page about Rebuilding the Documentation
  - Add page of hard problems
* Build
  - remove unnecessary steps
  - drop instdoc script
  - move smime_keys into contrib
  - fixes for Solaris
  - don't delete non-existent files
  - remove another reference to devel-notes.txt
  - Handle native Solaris GSSAPI.
  - drop configure options --enable-exact-address
  - drop configure option --with-exec-shell
  - drop configure option --enable-nfs-fix
  - drop configure option --disable-warnings
  - Completely remove dotlock
  - More sophisticated check for BDB version + support for DB6 (non default)
* Tidy
  - drop VirtIncoming
  - split parse_mailboxes into parse_unmailboxes
  - tidy some mailbox code
  - tidy the version strings
* Upstream
  - Add ~\<() and ~\>() immediate parent/children patterns
  - Add L10N comments to the GNUTLS certificate prompt
  - Add more description for the %S and %Z $index_format characters
  - Add config vars for forwarded message attribution intro/trailer
  - Block SIGWINCH during connect()
  - Improve the L10N comment about Sign as
  - Auto-pad translation for the GPGME key selection "verify key" headers
  - Enable all header fields in the compose menu to be translated
  - Force hard redraw after $sendmail instead of calling mutt_endwin
  - Make GPGME key selection behavior the same as classic-PGP
  - Rename 'sign as' to 'Sign as'; makes compose menu more consistent
  - Change the compose menu fields to be dynamically padded

2017-06-09  Richard Russon  \<rich@flatcap.org\>
* Contrib
  - unbind mappings before overwriting in vim-keys
* Bug Fixes
  - latest coverity issues (#624)
  - don't pass colour-codes to filters
  - Don't set a colour unless it's been defined.
  - crash if no from is set or founds
  - ifdef command
* Translations
  - fix translations
  - fix some remaining translation problems
* Docs
  - explain binding warnings
  - don't document unsupported arches
* Build
  - fix make git_ver.h
  - allow xsltproc and w3m calls to fail
  - fix make dist
* Upstream
  - Add a mutt_endwin() before invoking $sendmail
  - Restore setenv function
  - Fix tag-prefix to not abort on $timeout
  - Change km_dokey() to return -2 on a timeout/sigwinch
  - Enable TEXTDOMAINDIR override to make translation testing easier
  - Fix "format string is not a string literal" warnings

2017-06-02  Richard Russon  \<rich@flatcap.org\>
* Features
  - Warn on bindkey aliasing
  - Drop PATCHES, tidy 'mutt -v' output
  - Add %z format strings to index_format
  - Add debug_level/debug_file options
* Bug Fixes
  - Fix nntp group selection
  - Fix status color
  - Tidy up S/MIME contrib
  - Do not try to create Maildir if it is an NNTP URL
  - Fix missing NONULL for mutt.set() in Lua
* Translations
  - Fix German PGP shortkeys
* Docs
  - Remove feature muttrc files
  - Merge README.notmuch into manual
  - Remove unneeded scripts
  - Remove README.SECURITY
  - Remove BEWARE and devel-notes.txt
  - Update Makefiles
  - Delete TODO files
  - Remove legacy files
  - Don't generate vim-neomutt syntax file
  - Remove LaTeX/pdf manual generation
  - Add missing docs for expandos
  - Fix sidebar howto examples
  - Remove some upstream references
  - Drop refs to patches
  - Improve PR template and CONTRIBUTING.md
* Website
  - Fix list items in newbie-tutorial's Mailing List Guidelines
  - Remove configure options that no longer exist
  - fix newbie tutorial
  - document signing tags / releases
  - config: drop unused paginate command
  - script: split tests up into several
  - convert credits page to markdown
  - simpify 404 page
  - improve newbie tutorial
  - remove help.html and integrate its content elsewhere
  - make: "graphviz" program is needed for generating diagram
  - improve getting started guide // include legacy files
  - dev: add list of architectures/operating systems
  - numerous small fixes
* Build
  - Remove typedefs and rename ~130 structs
  - Add separate hcache dir
  - Move crypto files to ncrypt dir
  - Split up mutt.h, protos.h
  - Always build: sidebar, imap, pop, smtp, compressed, nntp
  - Remove --enable-mailtool configure option
  - Make dotlock optional
  - Change gpgme requirement back to 1.1.0
  - Remove check_sec.sh
  - Fix safe_calloc args
  - Remove unused macros
  - Remove unused option: SmimeSignOpaqueCommand
  - Move configure-generated files
  - Update distcheck build flags
  - Drop obsolete iconv check
  - Unused prototypes - unsupported systems
  - Drop many configure tests for things defined in POSIX:2001
  - Kill useless crypthash.h file
  - Run clang-format on the code
  - Fail early if ncursesw cannot be found
  - Add names prototype arguments
  - Abbreviate pointer tests against NULL
  - Initialise pointers to NULL
  - Reduce the scope of for loop variables
  - Coverity: fix defects
* Upstream
  - Convert all exec calls to use mutt_envlist(), remove setenv function
  - Note that mbox-hooks are dependent on $move
  - Refresh header color when updating label
  - Remove glibc-specific execvpe() call in sendlib.c
  - Add color commands for the compose menu headers and security status
  - Fix sidebar count updates when closing mailbox
  - Don't modify LastFolder/CurrentFolder upon aborting a change folder operation
  - Change message modifying operations to additively set redraw flags
  - Improve maildir and mh to report flag changes in mx_check_mailbox()
  - Add $header_color_partial to allow partial coloring of headers
  - Rename REDRAW_SIGWINCH to REDRAW_FLOW
  - Create R_PAGER_FLOW config variable flag
  - Turn IMAP_EXPUNGE_EXPECTED back off when syncing
  - Add $history_remove_dups option to remove dups from history ring
  - Also remove duplicates from the history file
  - Don't filter new entries when compacting history save file
  - Move the IMAP msn field to IMAP_HEADER_DATA
  - Fix imap expunge to match msn and fix index
  - Fix cmd_parse_fetch() to match against MSN
  - Start fixing imap_read_headers() to account for MSN gaps
  - Add msn_index and max_msn to find and check boundaries by MSN
  - Properly adjust fetch ranges when handling new mail
  - Small imap fetch fixes
  - Don't abort header cache evaluation when there is a hole
  - Fix mfc overflow check and uninitialized variable
  - Fix potential segv if mx_open_mailbox is passed an empty string
  - Don't clean up idata when closing an open-append mailbox
  - Don't clean up msn idata when closing an open-append mailbox
  - Fix memory leak when closing mailbox and using the sidebar
  - Change imap body cache cleanup to use the uid_hash
  - Convert classic s/mime to space delimit findKeys output
  - Add self-encrypt options for PGP and S/MIME
  - Change $postpone_encrypt to use self-encrypt variables first
  - Automatic post-release commit for mutt-1.8.3
  - Add note about message scoring and thread patterns

2017-04-28  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - Fix and simplify handling of GPGME in configure.ac (@gahr)
* Docs
  - Fix typo in README.neomutt (@l2dy)
* Upstream
  - Fix km_error_key() infinite loop and unget buffer pollution
  - Fix error message when opening a mailbox with no read permission

2017-04-21  Richard Russon  \<rich@flatcap.org\>
* Features
  - add lua scripting
  - add command-line batch mode
  - index_format: add support of %K
* Bug Fixes
  - attachment/pager: Use mailcap for test/* except plain
  - Fix uncollapse_new in pager
  - fix garbage in chdir prompt due to unescaped string
  - Fix inbox-first functionality when using mutt_pretty_mailbox
  - add full neomutt version to log startup
  - fix bug in uncolor for notmuch tag
  - fix broken from_chars behaviour
* Coverity defects
  - strfcpy
  - add variable - function arg could be NULL/invalid
  - add variable - failed function leads to invalid variable
  - add variable - Context could become NULL
  - add variable - alloc/strdup could return NULL
  - add variable - route through code leads to invalid variable
  - remove variable test
  - test functions
  - tidy switches
  - unused variables
  - refactor only
  - check for buffer underruns
  - fix leaks
  - minor fixes
  - bug: add missing break
  - bug: don't pass large object by value
  - fix: use correct buffer size
  - shadow variables
  - 0 -\> NULL
* Docs
  - many minor updates
  - sync translations
  - delete trailing whitespace
  - indent the docbook manual
  - use w3m as default for generating UTF8 manual.txt
* Website
  - many minor updates
  - fix broken links
  - add to list of useful programs
  - test automatic html checker
  - remove trailing whitespace
  - add irc description
  - update issue labels (dev)
  - new page: closed discussions
  - new page: making neomutt (dev)
* Build
  - drop obsolete m4 scripts
  - don't look for lua libs unless asked for
  - workaround slang warnings
  - lower the gettext requirement 0.18 -\> 0.17
  - add keymap_alldefs.h to BUILT_SOURCES
  - fix make dist distcheck
  - Remove -Iimap from CFLAGS and include imap/imap.h explicitly
  - mx: fix conditional builds
  - Make iconv mandatory (no more --disable-iconv)
  - refactor: Split out BUFFER-handling functions
* Tidy
  - drop control characters from the source
  - drop vim modelines
  - delete trailing whitespace
  - mark all local functions as static
  - delete unused functions
  - replace FOREVER with while (true)
  - drop #if HAVE_CONFIG_H
  - use #ifdef for potentially missing symbols
  - remove #if 0 code blocks
  - drop commented out source
  - IMAP auth functions are stored by pointer cannot be static
  - force OPS to be rebuilt after a reconfigure
  - be specific about void functions
  - expand a few more alloc macros
  - add argument names to function prototypes
  - drop local copy of regex code
  - rearrange code to avoid forward declarations
  - limit the scope of some functions
  - give the compress functions a unique name
  - use snake_case for function names
  - add missing newlines to mutt_debug
  - remove generated files from repo
  - look for translations in all files
  - fix arguments to printf-style functions
  - license text
  - unify include-guards
  - tidy makefiles
  - initialise pointers
  - make strcmp-like functions clearer
  - unify sizeof usage
  - remove forward declarations
  - remove ()s from return
  - rename files hyphen to underscore
  - remove unused macros
  - use SEEK_SET, SEEK_CUR, SEEK_END
  - remove constant code
  - fix typos and grammar in the comments
  - Switch to using an external gettext runtime
  - apply clang-format to the source code
  - boolify returns of 84 functions
  - boolify lots of struct members
  - boolify some function parameters
* Upstream
  - Add $ssl_verify_partial_chains option for OpenSSL
  - Move the OpenSSL partial chain support check inside configure.ac
  - Don't allow storing duplicate certs for OpenSSL interactive prompt
  - Prevent skipped certs from showing a second time
  - OpenSSL: Don't offer (a)ccept always choice for hostname mismatches
  - Add SNI support for OpenSSL
  - Add SNI support for GnuTLS
  - Add shortcuts for IMAP and POP mailboxes in the file browser
  - Change OpenSSL to use SHA-256 for cert comparison
  - Fix conststrings type mismatches
  - Pass envlist to filter children too
  - Fix mutt_envlist_set() for the case that envlist is null
  - Fix setenv overwriting to not truncate the envlist
  - Fix (un)sidebar_whitelist to expand paths
  - Fix mutt_refresh() pausing during macro events
  - Add a menu stack to track current and past menus
  - Change CurrentMenu to be controlled by the menu stack
  - Set refresh when popping the menu stack
  - Remove redraw parameter from crypt send_menus
  - Don't full redraw the index when handling a command from the pager
  - Filter other directional markers that corrupt the screen
  - Remove the OPTFORCEREDRAW options
  - Remove SidebarNeedsRedraw
  - Change reflow_windows() to set full redraw
  - Create R_MENU redraw option
  - Remove refresh parameter from mutt_enter_fname()
  - Remove redraw flag setting after mutt_endwin()
  - Change km_dokey() to pass SigWinch on for the MENU_EDITOR
  - Separate out the compose menu redrawing
  - Separate out the index menu redrawing
  - Prepare for pager redraw separation
  - Separate out the pager menu redrawing
  - Don't create query menu until after initial prompt
  - Silence imap progress messages for pipe-message
  - Ensure mutt stays in endwin during calls to pipe_msg()
  - Fix memleak when attaching files
  - Add $ssl_verify_partial_chains option for OpenSSL
  - Move the OpenSSL partial chain support check inside configureac
  - Don't allow storing duplicate certs for OpenSSL interactive prompt
  - Prevent skipped certs from showing a second time
  - OpenSSL: Don't offer (a)ccept always choice for hostname mismatches
  - Add SNI support for OpenSSL
  - Add SNI support for GnuTLS
  - Add shortcuts for IMAP and POP mailboxes in the file browser
  - Updated French translation
  - Change OpenSSL to use SHA-256 for cert comparison
  - Fix conststrings type mismatches
  - Pass envlist to filter children too
  - Fix mutt_envlist_set() for the case that envlist is null
  - Fix setenv overwriting to not truncate the envlist
  - Fix mutt_refresh() pausing during macro events
  - Add a menu stack to track current and past menus
  - Change CurrentMenu to be controlled by the menu stack
  - Set refresh when popping the menu stack
  - Remove redraw parameter from crypt send_menus
  - Don't full redraw the index when handling a command from the pager
  - Fix (un)sidebar_whitelist to expand paths
  - Filter other directional markers that corrupt the screen
  - Remove the OPTFORCEREDRAW options
  - Remove SidebarNeedsRedraw
  - Change reflow_windows() to set full redraw
  - Create R_MENU redraw option
  - Remove refresh parameter from mutt_enter_fname()
  - Remove redraw flag setting after mutt_endwin()
  - Change km_dokey() to pass SigWinch on for the MENU_EDITOR
  - Separate out the compose menu redrawing
  - Separate out the index menu redrawing
  - Prepare for pager redraw separation
  - Separate out the pager menu redrawing
  - Don't create query menu until after initial prompt
  - Silence imap progress messages for pipe-message
  - Ensure mutt stays in endwin during calls to pipe_msg()
  - Fix memleak when attaching files
  - automatic post-release commit for mutt-181
  - Added tag mutt-1-8-1-rel for changeset f44974c10990
  - mutt-181 signed
  - Add ifdefs around new mutt_resize_screen calls
  - Add multiline and sigwinch handling to mutt_multi_choice
  - Set pager's REDRAW_SIGWINCH when reflowing windows
  - Add multiline and sigwinch handling to mutt_yesorno
  - Change the sort prompt to use (s)ort style prompts
  - Handle the pager sort prompt inside the pager
  - Fix GPG_TTY to be added to envlist
  - automatic post-release commit for mutt-182

2017-03-06  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - Get the correct buffer size under fmemopen/torify (#441)
  - Use static inlines to make gcc 4.2.1 happy
  - getdnsdomainname: cancel getaddrinfo_a if needed
  - imap: remove useless code (#434) (origin/master)
  - Fixes missing semi-colon compilation issue (#433)
* Docs
  - github: added template for Pull Requests, issues and a CONTRIBUTION.md (#339)
  - editorconfig: support for new files, fix whitespace (#439)
  - add blocking fmemopen bug on debian to manual (#422)
* Upstream
  - Increase ACCOUNT.pass field size. (closes #3921)
  - SSL: Fix memory leak in subject alternative name code. (closes #3920)
  - Prevent segv if open-appending to an mbox fails. (closes #3918)
  - Clear out extraneous errors before SSL_connect() (see #3916)

2017-02-25  Richard Russon  \<rich@flatcap.org\>
* Features
  - Add option $show_multipart_alternative
  - notmuch: Allow to use untransformed tag for color
  - Use getaddrinfo_a if possible (#420)
* Bug Fixes
  - handle sigint within socket operations (#411)
  - Avoid browsing the remote spoolfile by setting MUTT_SELECT_MULTI attach
  - notmuch: fix crash when completing tags (#395)
  - Fixes missing failure return of notmuch msg open (#401)
  - Fix latest Coverity issues (#387)
  - Advance by the correct number of position even for unknown characters (#368)
  - Release KyotoCabinet data with kcfree() (#384)
  - 22 resource leaks
* Translations
  - Update translations
  - Update the German translation (#397)
* Docs
  - fix typo in notmuch example
  - remove duplicate "default" in the sidebar intro
  - fix confusing description of notmuch operators (#371)
  - correct spelling mistakes (#412)
* Website
  - link to clang-format config in main repo (#28)
  - updated list of useful programs
  - update/improve list of useful programs
  - sidebar_format has a single default value
  - fix name of GNU Guix
  - added guix distro
  - added link to new afew maintainers
  - add code of conduct
  - add mutt-addressbook to useful
  - remove unnecessary unicode non-breaking spaces
  - github merging
* Build
  - Enable and run unit-tests on the feature/unit-test branch
  - add notmuch to default, feature
  - new dbs for mutt
  - master is now the main branch
  - streamline builds
  - fix doc generator
  - add a few includes (prelude to clang-format)
  - slcurses.h defines its own bool type
  - travis: use container build
  - add clang-format file
  - Remove ugly macros and casts from crypt_gpgme.c
  - fix minor reflow issues in some comments
  - editorconfig: use spaces to indent in *.[ch] files
  - added comment-blocks for clang-format to ignore
  - fix 80 column limit, align statements
  - Remove snprintf.c from EXTRA_DIST (#406)
  - Kill homebrew (v)snprintf implementations, as they are C99 (#402)
  - Display charset + small refactoring
  - Do not cast or check returns from safe_calloc (#396)
  - refactor: create a generic base64 encode/decode
  - debug: remove dprint in favor of mutt_debug (#375)
  - Fix dubious use macro for _() / gettext() (#376)
  - Use mutt_buffer_init instead of memset
  - Make the heap method and datatype a plain list
  - Reverts making AliasFile into a list_t (#379)
  - Turn mutt_new_* macros into inline functions
  - Do not cast return values from malloc (or similar)
* Upstream
  - Simplify mutt_label_complete().
  - Permit tab completion of pattern expressions with ~y (labels).
  - Fix the mutt_label_complete() pos parameter.
  - Fix the x-label update code check location.
  - Improve the label completion hash table usage.
  - Adds label completion.
  - Add hash_find_elem to get the hash element.
  - Minor fixes to the x-label patch from David.
  - Adds capability to edit x-labels inside mutt, and to sort by label.
  - Allow "unsubjectrc *" to remove all patterns.
  - Add subjectrx command to replace matching subjects with something else.
  - Abstract the SPAM_LIST as a generic REPLACE_LIST
  - Improve Reply-to vs From comparison when replying. (closes #3909)
  - Fix sidebar references to the "new count" to be "unread". (closes #3908)
  - Fix several alias hashtable issues.
  - Add casecmp and strdup_key flags to hash_create()
  - Improve error handling in mbox magic detection.
  - Allow initial blank lines in local mailboxes.
  - Fix minor documentation issues.
  - Convert cmd_parse_search to use the uid hash. (closes #3905)
  - Create a uid hash for imap. (see #3905)
  - Convert HASH to be indexable by unsigned int. (see #3905)
  - Fix imap server-side search to call uid2msgno() only once. (see #3905)
  - Add a pattern_cache_t to speed up a few repeated matches.
  - Canonicalize line endings for GPGME S/MIME encryption. (closes #3904)
  - Fix build for bdb.
  - Create function to free header cache data.
  - Add Kyoto Cabinet support to the header cache.
  - Prevent null pointer exception for h-\>ai_canonname
  - Show SHA1 fp in interactive cert check menu.
  - Fix potential cert memory leak in check_certificate_by_digest().
  - Plug memory leak in weed-expired-certs code.
  - Filter expired local certs for OpenSSL verification.
  - Change "allow_dups" into a flag at hash creation.

2017-02-06  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - Unicode 0x202F is a non-break space too (#358) (@gahr)
  - improve readability of find_subject() (@toogley)
  - Import hcache-lmdb fixes from upstream (#363) (@gahr)
  - Rework the "inbox-first" implementation to make code self-explanatory (#356) (@gahr)
  - If possible, only redraw after gpgme has invoked pinentry (#352) (@gahr)
  - Remove two use-after free in global hooks (#353) (@guiniol)
  - Handle BAD as IMAP_AUTH_UNAVAIL (#351) (@gahr)
  - Do not crash when closing a non-opened mailbox (origin/requests/github/343) (@gahr)
  - Import hcache benchmark (@gahr)
  - fix: bug introduced by mkdir changes (#350)
  - change pager to allow timehook-hook to fire
* Docs
  - Update documentation about modify-labels-then-hide (@bbenne10)

2017-01-28  Richard Russon  \<rich@flatcap.org\>
* Features
  - Add option for missing subject replacement
  - notmuch: Allow \<modify-labels\> to toggle labels
  - Support for aborting mailbox loading
  - Do a mailbox check after shell escape
  - Support of relative paths sourcing and cyclic source detection
  - Support of multiple config files as CLI arguments
  - Extend the ~m pattern to allow relative ranges
  - Implement SASL's PLAIN mechanism as a standalone authenticator
  - Add support for sensitive config options
  - Searching with a window over notmuch vfolders
* Contrib
  - fix vim syntax file for index-color commands
  - add .editorconfig
* Bug Fixes
  - fix global hooks to not take a pattern
  - Avoid breaking relative paths when avoiding cyclic checks on
  - Fix sorting when using '/' as a namespace separator
* Docs
  - Added waffle badges to readme
  - Describe the new message ranges
  - add documentation for -DS command line switch
  - fix typos in section on config locations
  - remove reference to missing keybinding
  - fix docbook validation
* Build
  - Start migrating to stdbool logic
  - add recursive mkdir()
  - reformat the source to mutt standards
  - appease check_sec.sh

2017-01-13  Richard Russon  \<rich@flatcap.org\>
* Features
  - Allow custom status flags in index_format
  - $from_chars highlights differences in authorship
  - notmuch: make 'Folder' and 'Tags' respect (un)ignore
  - notmuch: add "virtual-unmailboxes" command
* Bug Fixes
  - pick smarter default for $sidebar_divider_char
  - status color breaks "mutt -D"
  - Enable reconstruct-thread in the pager
  - manually touch 'atime' when reading a mbox file
  - allow $to_chars to contain Unicode characters
  - increase the max lmdb database size
  - restore limit current thread
  - don't reset the alarm unless we set it
  - some more places that may get NULL pointers
  - rework initials to allow unicode characters
* Translations
  - Spanish translation
  - German translation
* Docs
  - Improve whitespace and grammar on the NNTP feature page
  - make $to_chars docs more legible
  - de-tab the DocBook
  - fix 301 redirects
* Build
  - New configure option --enable-everything
  - add a constant for an aborted question
  - enhance mutt_to_base64() (and callers)
  - Fix configure.ac to require md5 if hcache is enabled
  - Bail if a selected hcache backend cannot be found
  - refactor mutt_matches_ignore
  - fix hcache + make dist
  - add unicode string helper function
  - Re-indent configure.ac
  - generate devel version suffix
  - fix check_sec.sh warnings
  - remove unnecessary #ifdef's
  - add missing #ifdef for nntp
  - ignore some configure temp files
  - fix "make dist" target
  - fix function prototypes
  - fix coverity warnings
  - notmuch: drop strndup, replace with mutt_substrdup
* Upstream
  - Fix failure with GPGME 1.8: do not steal the gpgme_ prefix.
  - search muttrc file according to XDG Base Specification (closes #3207)
  - Improve openssl interactive_check_cert. (closes #3899)
  - Add mutt_array_size macro, change interactive_check_cert() to use it. (see #3899)
  - Return to pager upon aborting a jump operation. (closes #3901)
  - Change sidebar_spoolfile coloring to be lower precedence.
  - Move '@' pattern modifier documentation to the right section.
  - Add setenv/unsetenv commands.
  - Rework OpenSSL certificate verification to support alternative chains. (closes #3903)
  - Add option to control whether threads uncollapse when new mail arrives.
  - In the manual, replaced 2 para by example (similar to the first example).
  - Create MbTable type for multibyte character arrays. (see #3024)
  - Make to_chars and status_chars accept mulitibyte characters. (closes #3024)

2016-11-26  Richard Russon  \<rich@flatcap.org\>
* Features
  - Upstream adoption of compress
  - Multiple hcache backends and run-time selection
  - $forward_references includes References: header on forwards
  - Hooks: define hooks for startup and shutdown
  - Add $collapse_all to close threads automatically
* Bug Fixes
  - Index in pager crash
  - Tag with multiple labels
  - Make sure gdbm's symbols are not resolved in QDBM's compatibility layer
  - Fix crash when doing collapse_all on an empty folder
  - Fix: crash when browsing empty dir
  - Initialize imap_authenticate's return value to something meaningful
* Translations
  - Update German translation
  - Update Slovak translation
  - Update French translation
  - Add English (British) translation
  - Convert files to utf-8
  - Mass tidy up of the translation messages
* Docs
  - new-mail bug is fixed
  - add since date for features
  - expand example command options for compress
  - fix entries for beep and new-mail-command
  - add a version number to the generated vimrc
  - fix links in README
  - don't use smart quotes in manual examples
  - \<escape\> and \e means refers to both alt and escape key
* Build
  - Travis: test messages
  - Add option to disable translation messages
  - Split hcache code into per-backend files
  - Doc/Makefile clean neomutt-syntax.vim
  - Improve discovery for the Berkeley Database
  - Fix nntp/notmuch conditionals
  - Implement mutt_strchrnul()
  - Rename vim-keybindings to vim-keys
* Upstream
  - attach_format: add new %F placeholder
  - Compose: add operation to rename an attachment
  - Chain %d-\>%F-\>%f in the attachment menu
  - Move mbox close-append logic inside mbox_close_mailbox()
  - When $flag_safe is set, flagged messages cannot be deleted
  - Adds the '@' pattern modifier to limit matches to known aliases
  - Adds \<mark-message\> binding to create "hotkeys" for messages
  - Updated requirement on the C compiler
  - Fix mark-message translation and keybind menu
  - More openssl1.1 fixes: remove uses of X509-\>name in debugging. (closes #3870)
  - Don't close stderr when opening a tunnel. (closes #3726)
  - Minor resource and error logic cleanup in tunnel_socket_open()
  - Make sure that the output of X509_NAME_oneline is null-terminated

2016-11-04  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - don't crash when the imap connection dies
* Upstream
  - Add root-message function to jump to root message in thread.
  - Updated French translation.
  - Prevent an integer overflow in mutt_mktime() (closes #3880)
  - Fix pager segfault when lineInfo.chunks overflows. (closes #3888)
  - Perform charset conversion on text attachments when piping. (closes #3773) (see #3886)
  - Add a --disable-doc configuration option.
  - Make ncurses and ncursesw header checking the same.
  - Attempt to silence a clang range warning. (closes #3891)
  - Fixed issue from changeset 4da647a80c55. (closes #3892)
  - Define PATH_MAX, it's missing on the GNU Hurd. (closes #3815)

2016-10-28  Richard Russon  \<rich@flatcap.org\>
* Features
  - nntp: use safe_{fopen,fclose}
  - nntp: fix resource leak
  - forgotten-attachment: Ignore lines matching quote_regex.
  - forgotten-attachment: Fix checking logic.
  - forgotten-attachment: Update docs regarding $quote_regex.
  - notmuch: Add a fake "Folder" header to viewed emails
  - sidebar: consider description when using whitelist
  - skip-quoted: skip to body
* Bug Fixes
  - sensible-browser/notmuch changing mailbox
  - "inbox" sorting function
  - overhaul the index/pager updates
  - crash in hdrline
  - remove stray line introduced by pager fix
  - Possible fix for random pager crashes.
* Docs
  - use a more expressive coverity scan badge
  - light tidying
* Build
  - replace the ugly strfcpy() macro with a function
  - build: Look for tgetent in ncurses, fallback to tinfo only if not found
  - build: fix a couple of build warnings
  - travis: install doc dependencies
  - build: fix install/dist/distcheck targets
* Upstream
  - Fix POP3 SASL authentication mechanism DIGEST-MD5. (closes #3862)
  - Add a few explanatory comments to pop_auth_sasl().  (see #3862)
  - Fix GPGME signature zero timestamp and locale awareness issues. (closes #3882)
  - Handle presence of '--' delimiter in $sendmail. (closes #3168)
  - Allow IPv6 literal addresses in URLs. (closes #3681)
  - Fix gpgme segfault in create_recipient_set().
  - Use mutt_strlen and mutt_strncmp in sidebar.c.
  - Change sidebar to only match $folder prefix on a $sidebar_divider_char. (closes #3887)
  - Actually fix gpgme segfault in create_recipient_set().

2016-10-14  Richard Russon  \<rich@flatcap.org\>
* Features
  - sidebar: Make sure INBOX appears first in the list.
  - notmuch: Synchronise tags to flags
* Bug Fixes
  - updates when pager is open
  - crash when neither $spoolfile, $folder are set
  - forgotten-attachment: fix empty regex expression
  - status-color when pager_index_lines \> 0
  - buffer underrun when no menu item is selected
  - crash handling keywords/labels
* Docs
  - update notmuch references
* Build
  - update references to 1.7.1
  - strfcpy() improvement
* Upstream
  - automatic post-release commit for mutt-1.7.1
  - Mark IMAP fast-trash'ed messages as read before copying. (see #3860)
  - Updated Czech translation.
  - Preserve forwarded attachment names in d_filename.

2016-10-03  Richard Russon  \<rich@flatcap.org\>
* Build
  - Fix install and dist targets

2016-10-02  Richard Russon  \<rich@flatcap.org\>
* Features
  - Kyoto Cabinet header cache
  - Compose to Sender
  - Forgotten Attachment uses a regex
  - Optimize LMDB's hcache backend
  - Sensible-browser behaviour fixes
* Bug Fixes
  - Fixes repaint problem with $pager_index_lines #159
  - Quasi-Delete: check there's a selection
  - Bulletproof the pager
  - Typo in the version string
* Docs
  - Add badges to README.neomutt
  - Document the Kyoto cabinet hcache backend
  - Fix the layout of the syntax file
  - Make the license clear to github
  - Fix the alignment in a 'nested-if' example
  - Fix notmuch vim syntax file
  - Added Mailinglist mailto links to "Where is NeoMutt" section
  - Fix build of neomutt-syntax.vim
  - Fixed typo of devel mailinglist name
* Build
  - Travis: install the kyoto-cabinet dev files
  - Build source before docs
  - Build fix for strndup / malloc
  - Change gcc build options to prevent crashes
* Upstream
  - Ensure signatures exist when verifying multipart/signed emails. (closes #3881).
  - RFC2047-decode mailto url headers after RFC2822 parsing. (closes #3879)
  - RFC2047-decode mailto header values. (closes #3879)
  - Reset invalid parsed received dates to 0.  (closes #3878)
  - Clear pager position when toggling headers.
  - Don't abort the menu editor on sigwinch. (closes #3875)
  - Mark some gpgme pgp menu keybinding translations as fuzzy. (closes #3874)
  - Check for NULL mx_ops in mx.c
  - Use body color for gpgme output. (closes #3872)
  - Fix gpgme segfault when querying candidates with a '+' in the address. (closes #3873)

2016-09-16  Richard Russon  \<rich@flatcap.org\>
* Bug Fixes
  - Avoid segfault when listing mailboxes on startup
    John Swinbank
  - Fix buffer overrun in search for attach keyword
    James McCoy
  - Fix off-by-one in error message
    Antonio Radici
  - fix AC_INIT tarname parameter
  - fix crash when exiting the pager
  - fix another crash in the pager
  - nntp: close message handles
  - fix: make the pager more robust
  - fix sidebar sort order
  - fix notmuch tag completion
* Docs
  - doc: Removes bug entry in new-mail docs
    Santiago Torres
  - fix some translations in crypt_gpgme.c
    Antonio Radici
  - docs: mass tidy up
* Upstream
  - Fix sidebar documentation a bit
  - Add unsidebar_whitelist command
  - Remove the $locale configuration variable
  - Add $attribution_locale configuration variable
  - Add missing include \<locale.h\> to send.c and edit.c
  - Filter out zero width no-break space (U+FEFF)
  - Update a confusing and obsolete comment
  - Moves mutt_copy_list to muttlib.c, where it belongs
  - Redraw screen after an SSL cert prompt
  - Preserve message-id and mft headers for recalled messages
  - Fix openssl 1.1 compilation issues

2016-09-10  Richard Russon  \<rich@flatcap.org\>
* New Features
  - Colouring Attachments with Regex
    Guillaume Brogi
  - PGP Encrypt to Self
    Guillaume Brogi
  - Sensible Browser
    Pierre-Elliott Bécue
  - Reply using X-Original-To: header
    Pierre-Elliott Bécue
  - Purge Thread
    Darshit Shah
  - Forgotten attachment
    Darshit Shah
  - Add sidebar_ordinary color
* Bug Fixes
  - align the nntp code with mutt
    Fabian Groffen
  - check for new mail while in pager when idle
    Stefan Assmann
  - Allow the user to interrupt slow IO operations
    Antonio Radici
  - keywords: check there are emails to tag
  - fix duplicate saved messages
  - flatten contrib/keybase dir to fix install
  - restore the pager keymapping 'i' to exit
  - proposed fix for clearing labels
  - notmuch: sync vfolder_format to folder_format
* Docs
  - Update List of Features and Authors
* Build
  - fix configure check for fmemopen
  - use fixed version strings
* Upstream
  - Increase date buffer size for $folder_format.
  - Disable ~X when message scoring.
  - Fix pgpring reporting of DSA and Elgamal key lengths.
  - Stub out getdnsdomainname() unless HAVE_GETADDRINFO.
  - Autoconf: always check for getaddrinfo().
  - Add missing sidebar contrib sample files to dist tarball.

2016-08-27  Richard Russon  \<rich@flatcap.org\>
* NeoMutt for Mutt 1.7.0
* Build
  - Disable fmemopen until bug is fixed
* Contrib
  - Keybase portability improvements
    Joshua Jordi (JakkinStewart)

2016-08-21  Richard Russon  \<rich@flatcap.org\>
* Contrib
  - Updates to Keybase Support
    Joshua Jordi (JakkinStewart)
* Bug Fixes
  - Fix data-loss when appending a compressed file
  - Don't paint invisible progress bars
  - Revert to Mutt keybindings
  - Don't de-tag emails after labelling them
  - Don't whine if getrandom() fails
    Adam Borowski (kilobyte)
  - Fix display when 'from' field is invalid
* Config
  - Support for $XDG_CONFIG_HOME and $XDG_CONFIG_DIRS
    Marco Hinz (mhinz)
* Docs
  - Fix DocBook validation
  - Document Notmuch queries
* Build
  - More Autoconf improvements
    Darshit Shah (darnir)
  - Create Distribution Tarballs with autogen sources
    Darshit Shah (darnir)

2016-08-08  Richard Russon  \<rich@flatcap.org\>
* New Features
  - Timeout Hook - Run a command periodically
  - Multiple fcc - Save multiple copies of outgoing mail
* Contrib
  - Keybase Integration
    Joshua Jordi (JakkinStewart)
* Devel
  - Attached - Prevent missing attachments
    Darshit Shah (darnir)
  - Virtual Unmailboxes - Remove unwanted virtual mailboxes
    Richard Russon (flatcap)
* Bug Fixes
  - Sidebar's inbox occasionally shows zero/wrong value
  - Fix crash opening a second compressed mailbox
* Config
  - Look for /etc/neomuttrc and ~/.neomuttrc
* Docs
  - Fix broken links, typos
  - Update project link
  - Fix version string in the manual
* Build
  - Add option to disable fmemopen
  - Install all the READMEs and contribs
  - Big overhaul of the build
    Darshit Shah (darnir)

2016-07-23  Richard Russon  \<rich@flatcap.org\>
* New Motto: "Teaching an Old Dog New Tricks"
  - Thanks to Alok Singh
* New Features
  - New Mail Command - Execute a command on receipt of new mail
  - vim-keys - Mutt config for vim users
  - LMDB: In-memory header caching database
  - SMIME Encrypt to Self - Secure storage of sensitive email
* Bug Fixes
  - rework mutt_draw_statusline()
  - fix cursor position after sidebar redraw
  - Add sidebar_format flag '%n' to display 'N' on new mail.
  - fix index_format truncation problem
  - Fix compiler warnings due to always true condition
  - Change sidebar next/prev-new to look at mailbox-\>new too.
  - Change the default for sidebar_format to use %n.
  - sidebar "unsorted" order to match Mailbox list order.
  - Include ncurses tinfo library if found.
  - Sidebar width problem
  - sidebar crash for non-existent mailbox
  - Temporary compatibility workaround
  - Reset mailbox-\>new for the current mailbox in IMAP.
  - version.sh regression
  - crash when notmuch tries to read a message
  - status line wrapping
* Docs
  - Mass tidy up of the docs
  - Fix xml validation
  - Add missing docs for new features
* Travis
  - New build system:
    https://github.com/neomutt/travis-build
    Now we have central control over what gets built

2016-07-09  Richard Russon  \<rich@flatcap.org\>
* Bug-fixes
  - This release was a temporary measure

2016-06-11  Richard Russon  \<rich@flatcap.org\>
* Change in behaviour
  - Temporarily disable $sidebar_refresh_time
    Unfortunately, this was causing too many problems.
    It will be fixed and re-enabled as soon as possible.
* Bug Fixes
  - Fix several crashes, on startup, in Keywords
  - Reflow text now works as it should
  - Lots of typos fixed
  - Compress config bug prevented it working
  - Some minor bug-fixes from mutt/default
  - Single quote at line beginning misinterpreted by groff
  - Setting $sidebar_width to more than 128 would cause bad things to happen.
  - Fix alignment in the compose menu.
  - Fix sidebar mailbox stats updating on mailbox close.
* Build Changes
  - Sync whitespace to mutt/default
  - Alter ChangeLog date format to simplify Makefiles
  - Use the new notmuch functions that return a status
  - Rename sidebar functions sb_* -\> mutt_sb_*

2016-05-23  Richard Russon  \<rich@flatcap.org\>
* New Features:
  - Keywords: Email Label/Keywords/Tagging
  - Compress: Compressed mailboxes support
  - NNTP: Talk to a usenet news server
  - Separate mappings for \<enter\> and \<return\>
  - New configure option: --enable-quick-build
  - Various build fixes

2016-05-02  Richard Russon  \<rich@flatcap.org\>
* Update for Mutt-1.6.0
* Bug Fixes:
  - Build for Notmuch works if Sidebar is disabled
  - Sidebar functions work even if the Sidebar is hidden
  - sidebar-next-new, etc, only find *new* mail, as documented
  - Notmuch supports *very* long queries

2016-04-16  Richard Russon  \<rich@flatcap.org\>
* Big Bugfix Release
* Bug Fixes:
  - Fix crash caused by sidebar_folder_indent
  - Allow the user to change mailboxes again
  - Correct sidebar's messages counts
  - Only sort the sidebar if we're asked to
  - Fix refresh of pager when toggling the sidebar
  - Compose mode: make messages respect the TITLE_FMT
  - Conditional include if sys/syscall.h
  - Build fix for old compilers
  - Try harder to keep track of the open mailbox
* Changes to Features
  - Allow sidebar_divider_char to be longer
    (it was limited to one character)
  - Ignore case when sorting the sidebar alphabetically
* Other Changes
  - Numerous small tweaks to the docs
  - Lots of minor code tidy-ups
  - Enabling Notmuch now forcibly enables Sidebar
    (it is dependent on it, for now)
  - A couple of bug fixes from mutt/stable

2016-04-04  Richard Russon  \<rich@flatcap.org\>
* Update for Mutt-1.6.0
* No other changes in this release

2016-03-28  Richard Russon  \<rich@flatcap.org\>
* New Features
  - skip-quoted          - skip quoted text
  - limit-current-thread - limit index view to current thread
* Sidebar Intro - A Gentle Introduction to the Sidebar (with pictures).

2016-03-20  Richard Russon  \<rich@flatcap.org\>
* Numerous small bugfixes
* TravisCI integration

2016-03-17  Richard Russon  \<rich@flatcap.org\>
* New Features
  - notmuch - email search support
  - ifdef   - improvements

2016-03-07  Richard Russon  \<rich@flatcap.org\>
* First NeoMutt release
* List of Features:
  - bug-fixes    - various bug fixes
  - cond-date    - use rules to choose date format
  - fmemopen     - use memory buffers instead of files
  - ifdef        - conditional config options
  - index-color  - theme the email index
  - initials     - expando for author's initials
  - nested-if    - allow deeply nested conditions
  - progress     - show a visual progress bar
  - quasi-delete - mark emails to be hidden
  - sidebar      - overview of mailboxes
  - status-color - theming the status bar
  - tls-sni      - negotiate for a certificate
  - trash        - move 'deleted' emails to a trash bin

