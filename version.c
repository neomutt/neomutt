/**
 * @file
 * Display version and copyright about NeoMutt
 *
 * @authors
 * Copyright (C) 2016-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2016-2023 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page neo_version Display version and copyright about NeoMutt
 *
 * Display version and copyright about NeoMutt
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "version.h"
#include "compress/lib.h"
#ifdef HAVE_LIBIDN
#include "address/lib.h"
#endif
#ifdef CRYPT_BACKEND_GPGME
#include "ncrypt/lib.h"
#endif
#ifdef HAVE_NOTMUCH
#include <notmuch.h>
#endif
#ifdef USE_SSL_OPENSSL
#include <openssl/opensslv.h>
#endif
#ifdef USE_SSL_GNUTLS
#include <gnutls/gnutls.h>
#endif
#ifdef HAVE_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

const char *mutt_make_version(void);
const char *store_backend_list(void);
const char *store_compress_list(void);

/// CLI: Width to wrap version info
static const int SCREEN_WIDTH = 80;

extern unsigned char cc_cflags[];
extern unsigned char configure_options[];

/// CLI Version: Authors' copyrights
static const char *Copyright =
    "Copyright (C) 2015-2024 Richard Russon <rich@flatcap.org>\n"
    "Copyright (C) 2016-2023 Pietro Cerutti <gahr@gahr.ch>\n"
    "Copyright (C) 2017-2019 Mehdi Abaakouk <sileht@sileht.net>\n"
    "Copyright (C) 2018-2020 Federico Kircheis <federico.kircheis@gmail.com>\n"
    "Copyright (C) 2017-2022 Austin Ray <austin@austinray.io>\n"
    "Copyright (C) 2023-2024 Dennis Schön <mail@dennis-schoen.de>\n"
    "Copyright (C) 2016-2017 Damien Riegel <damien.riegel@gmail.com>\n"
    "Copyright (C) 2023      Rayford Shireman\n"
    "Copyright (C) 2021-2023 David Purton <dcpurton@marshwiggle.net>\n"
    "Copyright (C) 2020-2023 наб <nabijaczleweli@nabijaczleweli.xyz>\n";

/// CLI Version: Thanks
static const char *Thanks = N_(
    "Many others not mentioned here contributed code, fixes and suggestions.\n");

/// CLI Version: License
static const char *License = N_(
    "This program is free software; you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation; either version 2 of the License, or\n"
    "(at your option) any later version.\n"
    "\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public License for more details.\n"
    "\n"
    "You should have received a copy of the GNU General Public License\n"
    "along with this program; if not, write to the Free Software\n"
    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n");

/// CLI Version: How to reach the NeoMutt Team
static const char *ReachingUs = N_("To learn more about NeoMutt, visit: https://neomutt.org\n"
                                   "If you find a bug in NeoMutt, please raise an issue at:\n"
                                   "    https://github.com/neomutt/neomutt/issues\n"
                                   "or send an email to: <neomutt-devel@neomutt.org>\n");

// clang-format off
/// CLI Version: Warranty notice
static const char *Notice =
    N_("Copyright (C) 2015-2024 Richard Russon and friends\n"
       "NeoMutt comes with ABSOLUTELY NO WARRANTY; for details type 'neomutt -vv'.\n"
       "NeoMutt is free software, and you are welcome to redistribute it\n"
       "under certain conditions; type 'neomutt -vv' for details.\n");
// clang-format on

/**
 * struct CompileOptions - List of built-in capabilities
 */
struct CompileOptions
{
  const char *name; ///< Option name
  int enabled;      ///< 0 Disabled, 1 Enabled, 2 Devel only
};

/* These are sorted by the display string */

/// Compile options strings for `neomutt -v` output
static const struct CompileOptions CompOpts[] = {
#ifdef USE_AUTOCRYPT
  { "autocrypt", 1 },
#else
  { "autocrypt", 0 },
#endif
#ifdef USE_FCNTL
  { "fcntl", 1 },
#else
  { "fcntl", 0 },
#endif
#ifdef USE_FLOCK
  { "flock", 1 },
#else
  { "flock", 0 },
#endif
#ifdef USE_FMEMOPEN
  { "fmemopen", 1 },
#else
  { "fmemopen", 0 },
#endif
#ifdef HAVE_FUTIMENS
  { "futimens", 1 },
#else
  { "futimens", 0 },
#endif
#ifdef HAVE_GETADDRINFO
  { "getaddrinfo", 1 },
#else
  { "getaddrinfo", 0 },
#endif
#ifdef USE_SSL_GNUTLS
  { "gnutls", 1 },
#else
  { "gnutls", 0 },
#endif
#ifdef CRYPT_BACKEND_GPGME
  { "gpgme", 1 },
#else
  { "gpgme", 0 },
#endif
#ifdef USE_SASL_GNU
  { "gsasl", 1 },
#else
  { "gsasl", 0 },
#endif
#ifdef USE_GSS
  { "gss", 1 },
#else
  { "gss", 0 },
#endif
#ifdef USE_HCACHE
  { "hcache", 1 },
#else
  { "hcache", 0 },
#endif
#ifdef HOMESPOOL
  { "homespool", 1 },
#else
  { "homespool", 0 },
#endif
#ifdef HAVE_LIBIDN
  { "idn", 1 },
#else
  { "idn", 0 },
#endif
#ifdef USE_INOTIFY
  { "inotify", 1 },
#else
  { "inotify", 0 },
#endif
#ifdef LOCALES_HACK
  { "locales_hack", 1 },
#else
  { "locales_hack", 0 },
#endif
#ifdef USE_LUA
  { "lua", 1 },
#else
  { "lua", 0 },
#endif
#ifdef ENABLE_NLS
  { "nls", 1 },
#else
  { "nls", 0 },
#endif
#ifdef USE_NOTMUCH
  { "notmuch", 1 },
#else
  { "notmuch", 0 },
#endif
#ifdef USE_SSL_OPENSSL
  { "openssl", 1 },
#else
  { "openssl", 0 },
#endif
#ifdef HAVE_PCRE2
  { "pcre2", 1 },
#endif
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "pgp", 1 },
#else
  { "pgp", 0 },
#endif
#ifndef HAVE_PCRE2
  { "regex", 1 },
#endif
#ifdef USE_SASL_CYRUS
  { "sasl", 1 },
#else
  { "sasl", 0 },
#endif
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  { "smime", 1 },
#else
  { "smime", 0 },
#endif
#ifdef USE_SQLITE
  { "sqlite", 1 },
#else
  { "sqlite", 0 },
#endif
#ifdef NEOMUTT_DIRECT_COLORS
  { "truecolor", 1 },
#else
  { "truecolor", 0 },
#endif
  { NULL, 0 },
};

/// Debug options strings for `neomutt -v` output
static const struct CompileOptions DebugOpts[] = {
#ifdef USE_ASAN
  { "asan", 2 },
#endif
#ifdef USE_DEBUG_BACKTRACE
  { "backtrace", 2 },
#endif
#ifdef USE_DEBUG_COLOR
  { "color", 2 },
#endif
#ifdef USE_DEBUG_EMAIL
  { "email", 2 },
#endif
#ifdef USE_DEBUG_GRAPHVIZ
  { "graphviz", 2 },
#endif
#ifdef USE_DEBUG_KEYMAP
  { "keymap", 2 },
#endif
#ifdef USE_DEBUG_LOGGING
  { "logging", 2 },
#endif
#ifdef USE_DEBUG_NAMES
  { "names", 2 },
#endif
#ifdef USE_DEBUG_NOTIFY
  { "notify", 2 },
#endif
#ifdef QUEUE_MACRO_DEBUG_TRACE
  { "queue", 2 },
#endif
#ifdef USE_UBSAN
  { "ubsan", 2 },
#endif
#ifdef USE_DEBUG_WINDOW
  { "window", 2 },
#endif
  { NULL, 0 },
};

/**
 * print_compile_options - Print a list of enabled/disabled features
 * @param co Array of compile options
 * @param fp file to write to
 *
 * Two lists are generated and passed to this function:
 *
 * One list which just uses the configure state of each feature.
 * One list which just uses feature which are set to on directly in NeoMutt.
 *
 * The output is of the form: "+enabled_feature -disabled_feature" and is
 * wrapped to SCREEN_WIDTH characters.
 */
static void print_compile_options(const struct CompileOptions *co, FILE *fp)
{
  if (!co || !fp)
    return;

  size_t used = 2;
  bool tty = isatty(fileno(fp));

  fprintf(fp, "  ");
  for (int i = 0; co[i].name; i++)
  {
    const size_t len = strlen(co[i].name) + 2; /* +/- and a space */
    if ((used + len) > SCREEN_WIDTH)
    {
      used = 2;
      fprintf(fp, "\n  ");
    }
    used += len;
    const char *fmt = "?%s ";
    switch (co[i].enabled)
    {
      case 0: // Disabled
        if (tty)
          fmt = "\033[1;31m-%s\033[0m "; // Escape, red
        else
          fmt = "-%s ";
        break;
      case 1: // Enabled
        if (tty)
          fmt = "\033[1;32m+%s\033[0m "; // Escape, green
        else
          fmt = "+%s ";
        break;
      case 2: // Devel only
        if (tty)
          fmt = "\033[1;36m%s\033[0m "; // Escape, cyan
        else
          fmt = "%s ";
        break;
    }
    fprintf(fp, fmt, co[i].name);
  }
  fprintf(fp, "\n");
}

/**
 * rstrip_in_place - Strip a trailing carriage return
 * @param s  String to be modified
 * @retval ptr The modified string
 *
 * The string has its last carriage return set to NUL.
 */
static char *rstrip_in_place(char *s)
{
  if (!s)
    return NULL;

  char *p = &s[strlen(s)];
  if (p == s)
    return s;
  p--;
  while ((p >= s) && ((*p == '\n') || (*p == '\r')))
    *p-- = '\0';
  return s;
}

/**
 * print_version - Print system and compile info to a file
 * @param fp File to print to
 * @retval true Text displayed
 *
 * Print information about the current system NeoMutt is running on.
 * Also print a list of all the compile-time information.
 */
bool print_version(FILE *fp)
{
  if (!fp)
    return false;

  struct utsname uts = { 0 };
  bool tty = isatty(fileno(fp));

  const char *col_cyan = "";
  const char *col_bold = "";
  const char *col_end = "";

  if (tty)
  {
    col_cyan = "\033[1;36m"; // Escape, cyan
    col_bold = "\033[1m";    // Escape, bold
    col_end = "\033[0m";     // Escape, end
  }

  fprintf(fp, "%s%s%s\n", col_cyan, mutt_make_version(), col_end);
  fprintf(fp, "%s\n", _(Notice));

  uname(&uts);

  fprintf(fp, "%sSystem:%s ", col_bold, col_end);
#ifdef SCO
  fprintf(fp, "SCO %s", uts.release);
#else
  fprintf(fp, "%s %s", uts.sysname, uts.release);
#endif

  fprintf(fp, " (%s)", uts.machine);

  fprintf(fp, "\n%sncurses:%s %s", col_bold, col_end, curses_version());
#ifdef NCURSES_VERSION
  fprintf(fp, " (compiled with %s.%d)", NCURSES_VERSION, NCURSES_VERSION_PATCH);
#endif

#ifdef _LIBICONV_VERSION
  fprintf(fp, "\n%slibiconv:%s %d.%d", col_bold, col_end,
          _LIBICONV_VERSION >> 8, _LIBICONV_VERSION & 0xff);
#endif

#ifdef HAVE_LIBIDN
  fprintf(fp, "\n%slibidn2:%s %s", col_bold, col_end, mutt_idna_print_version());
#endif

#ifdef CRYPT_BACKEND_GPGME
  fprintf(fp, "\n%sGPGME:%s %s", col_bold, col_end, mutt_gpgme_print_version());
#endif

#ifdef USE_SSL_OPENSSL
#ifdef LIBRESSL_VERSION_TEXT
  fprintf(fp, "\n%sLibreSSL:%s %s", col_bold, col_end, LIBRESSL_VERSION_TEXT);
#endif
#ifdef OPENSSL_VERSION_TEXT
  fprintf(fp, "\n%sOpenSSL:%s %s", col_bold, col_end, OPENSSL_VERSION_TEXT);
#endif
#endif

#ifdef USE_SSL_GNUTLS
  fprintf(fp, "\n%sGnuTLS:%s %s", col_bold, col_end, GNUTLS_VERSION);
#endif

#ifdef HAVE_NOTMUCH
  fprintf(fp, "\n%slibnotmuch:%s %d.%d.%d", col_bold, col_end, LIBNOTMUCH_MAJOR_VERSION,
          LIBNOTMUCH_MINOR_VERSION, LIBNOTMUCH_MICRO_VERSION);
#endif

#ifdef HAVE_PCRE2
  {
    char version[24] = { 0 };
    pcre2_config(PCRE2_CONFIG_VERSION, version);
    fprintf(fp, "\n%sPCRE2:%s %s", col_bold, col_end, version);
  }
#endif

#ifdef USE_HCACHE
  const char *backends = store_backend_list();
  fprintf(fp, "\n%sstorage:%s %s", col_bold, col_end, backends);
  FREE(&backends);
#ifdef USE_HCACHE_COMPRESSION
  backends = compress_list();
  fprintf(fp, "\n%scompression:%s %s", col_bold, col_end, backends);
  FREE(&backends);
#endif
#endif

  rstrip_in_place((char *) configure_options);
  fprintf(fp, "\n\n%sConfigure options:%s %s\n", col_bold, col_end, (char *) configure_options);

  rstrip_in_place((char *) cc_cflags);
  fprintf(fp, "\n%sCompilation CFLAGS:%s %s\n", col_bold, col_end, (char *) cc_cflags);

  fprintf(fp, "\n%s%s%s\n", col_bold, _("Compile options:"), col_end);
  print_compile_options(CompOpts, fp);

  if (DebugOpts[0].name)
  {
    fprintf(fp, "\n%s%s%s\n", col_bold, _("Devel options:"), col_end);
    print_compile_options(DebugOpts, fp);
  }

  fprintf(fp, "\n");
#ifdef DOMAIN
  fprintf(fp, "DOMAIN=\"%s\"\n", DOMAIN);
#endif
#ifdef ISPELL
  fprintf(fp, "ISPELL=\"%s\"\n", ISPELL);
#endif
  fprintf(fp, "MAILPATH=\"%s\"\n", MAILPATH);
  fprintf(fp, "PKGDATADIR=\"%s\"\n", PKGDATADIR);
  fprintf(fp, "SENDMAIL=\"%s\"\n", SENDMAIL);
  fprintf(fp, "SYSCONFDIR=\"%s\"\n", SYSCONFDIR);

  fprintf(fp, "\n");
  fputs(_(ReachingUs), fp);

  fflush(fp);
  return !ferror(fp);
}

/**
 * print_copyright - Print copyright message
 * @retval true Text displayed
 *
 * Print the authors' copyright messages, the GPL license and some contact
 * information for the NeoMutt project.
 */
bool print_copyright(void)
{
  puts(mutt_make_version());
  puts(Copyright);
  puts(_(Thanks));
  puts(_(License));
  puts(_(ReachingUs));

  fflush(stdout);
  return !ferror(stdout);
}

/**
 * feature_enabled - Test if a compile-time feature is enabled
 * @param name  Compile-time symbol of the feature
 * @retval true  Feature enabled
 * @retval false Feature not enabled, or not compiled in
 *
 * Many of the larger features of neomutt can be disabled at compile time.
 * They define a symbol and use ifdef's around their code.
 * The symbols are mirrored in "CompileOptions CompOpts[]" in this
 * file.
 *
 * This function checks if one of these symbols is present in the code.
 *
 * These symbols are also seen in the output of "neomutt -v".
 */
bool feature_enabled(const char *name)
{
  if (!name)
    return false;
  for (int i = 0; CompOpts[i].name; i++)
  {
    if (mutt_str_equal(name, CompOpts[i].name))
    {
      return CompOpts[i].enabled;
    }
  }
  return false;
}
