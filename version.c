/**
 * @file
 * Display version and copyright about NeoMutt
 *
 * @authors
 * Copyright (C) 1996-2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2007 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2016-2017 Richard Russon <rich@flatcap.org>
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
 * @page version Display version and copyright about NeoMutt
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

/* #include "muttlib.h" */
const char *mutt_make_version(void);
/* #include "store/lib.h" */
const char *store_backend_list(void);
const char *store_compress_list(void);

const int SCREEN_WIDTH = 80;

extern unsigned char cc_cflags[];
extern unsigned char configure_options[];

static const char *Copyright =
    "Copyright (C) 1996-2020 Michael R. Elkins <me@mutt.org>\n"
    "Copyright (C) 1996-2002 Brandon Long <blong@fiction.net>\n"
    "Copyright (C) 1997-2009 Thomas Roessler <roessler@does-not-exist.org>\n"
    "Copyright (C) 1998-2005 Werner Koch <wk@isil.d.shuttle.de>\n"
    "Copyright (C) 1999-2017 Brendan Cully <brendan@kublai.com>\n"
    "Copyright (C) 1999-2002 Tommi Komulainen <Tommi.Komulainen@iki.fi>\n"
    "Copyright (C) 2000-2004 Edmund Grimley Evans <edmundo@rano.org>\n"
    "Copyright (C) 2006-2009 Rocco Rutte <pdmef@gmx.net>\n"
    "Copyright (C) 2014-2020 Kevin J. McCarthy <kevin@8t8.us>\n"
    "Copyright (C) 2015-2020 Richard Russon <rich@flatcap.org>\n";

static const char *Thanks =
    N_("Many others not mentioned here contributed code, fixes,\n"
       "and suggestions.\n");

static const char *License = N_(
    "    This program is free software; you can redistribute it and/or modify\n"
    "    it under the terms of the GNU General Public License as published by\n"
    "    the Free Software Foundation; either version 2 of the License, or\n"
    "    (at your option) any later version.\n"
    "\n"
    "    This program is distributed in the hope that it will be useful,\n"
    "    but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "    GNU General Public License for more details.\n"
    "\n"
    "    You should have received a copy of the GNU General Public License\n"
    "    along with this program; if not, write to the Free Software\n"
    "    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  "
    "02110-1301, USA.\n");

static const char *ReachingUs =
    N_("To learn more about NeoMutt, visit: https://neomutt.org\n"
       "If you find a bug in NeoMutt, please raise an issue at:\n"
       "    https://github.com/neomutt/neomutt/issues\n"
       "or send an email to: <neomutt-devel@neomutt.org>\n");

// clang-format off
static const char *Notice =
    N_("Copyright (C) 1996-2020 Michael R. Elkins and others.\n"
       "NeoMutt comes with ABSOLUTELY NO WARRANTY; for details type 'neomutt -vv'.\n"
       "NeoMutt is free software, and you are welcome to redistribute it\n"
       "under certain conditions; type 'neomutt -vv' for details.\n");
// clang-format on

/**
 * struct CompileOptions - List of built-in capabilities
 */
struct CompileOptions
{
  const char *name;
  int enabled; ///< 0 Disabled, 1 Enabled, 2 Devel only
};

/* These are sorted by the display string */

static struct CompileOptions comp_opts_default[] = {
  { "attach_headers_color", 1 },
  { "compose_to_sender", 1 },
  { "compress", 1 },
  { "cond_date", 1 },
  { "debug", 1 },
  { "encrypt_to_self", 1 },
  { "forgotten_attachments", 1 },
  { "forwref", 1 },
  { "ifdef", 1 },
  { "imap", 1 },
  { "index_color", 1 },
  { "initials", 1 },
  { "limit_current_thread", 1 },
  { "multiple_fcc", 1 },
  { "nested_if", 1 },
  { "new_mail", 1 },
  { "nntp", 1 },
  { "pop", 1 },
  { "progress", 1 },
  { "quasi_delete", 1 },
  { "regcomp", 1 },
  { "reply_with_xorig", 1 },
  { "sensible_browser", 1 },
  { "sidebar", 1 },
  { "skip_quoted", 1 },
  { "smtp", 1 },
  { "status_color", 1 },
  { "timeout", 1 },
  { "tls_sni", 1 },
  { "trash", 1 },
  { NULL, 0 },
};

static struct CompileOptions comp_opts[] = {
#ifdef USE_AUTOCRYPT
  { "autocrypt", 1 },
#else
  { "autocrypt", 0 },
#endif
#ifdef HAVE_LIBUNWIND
  { "backtrace", 2 },
#endif
#ifdef HAVE_BKGDSET
  { "bkgdset", 1 },
#else
  { "bkgdset", 0 },
#endif
#ifdef HAVE_COLOR
  { "color", 1 },
#else
  { "color", 0 },
#endif
#ifdef HAVE_CURS_SET
  { "curs_set", 1 },
#else
  { "curs_set", 0 },
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
#ifdef USE_DEBUG_GRAPHVIZ
  { "graphviz", 2 },
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
#ifdef HAVE_META
  { "meta", 1 },
#else
  { "meta", 0 },
#endif
#ifdef MIXMASTER
  { "mixmaster", 1 },
#else
  { "mixmaster", 0 },
#endif
#ifdef ENABLE_NLS
  { "nls", 1 },
#else
  { "nls", 0 },
#endif
#ifdef USE_DEBUG_NOTIFY
  { "notify", 2 },
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
#ifdef USE_DEBUG_PARSE_TEST
  { "parse-test", 2 },
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
#ifdef USE_SASL
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
#ifdef HAVE_START_COLOR
  { "start_color", 1 },
#else
  { "start_color", 0 },
#endif
#ifdef SUN_ATTACHMENT
  { "sun_attachment", 1 },
#else
  { "sun_attachment", 0 },
#endif
#ifdef HAVE_TYPEAHEAD
  { "typeahead", 1 },
#else
  { "typeahead", 0 },
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
static void print_compile_options(struct CompileOptions *co, FILE *fp)
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
          fmt = "\033[1;36m!%s\033[0m "; // Escape, cyan
        else
          fmt = "!%s ";
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
 * @param fp - file to print to
 *
 * Print information about the current system NeoMutt is running on.
 * Also print a list of all the compile-time information.
 */
void print_version(FILE *fp)
{
  if (!fp)
    return;

  struct utsname uts;
  bool tty = isatty(fileno(fp));
  const char *fmt = "%s\n";

  if (tty)
    fmt = "\033[1;36m%s\033[0m\n"; // Escape, cyan

  fprintf(fp, fmt, mutt_make_version());
  fprintf(fp, "%s\n", _(Notice));

  uname(&uts);

#ifdef SCO
  fprintf(fp, "System: SCO %s", uts.release);
#else
  fprintf(fp, "System: %s %s", uts.sysname, uts.release);
#endif

  fprintf(fp, " (%s)", uts.machine);

#ifdef NCURSES_VERSION
  fprintf(fp, "\nncurses: %s (compiled with %s.%d)", curses_version(),
          NCURSES_VERSION, NCURSES_VERSION_PATCH);
#elif defined(USE_SLANG_CURSES)
  fprintf(fp, "\nslang: %s", SLANG_VERSION_STRING);
#endif

#ifdef _LIBICONV_VERSION
  fprintf(fp, "\nlibiconv: %d.%d", _LIBICONV_VERSION >> 8, _LIBICONV_VERSION & 0xff);
#endif

#ifdef HAVE_LIBIDN
  fprintf(fp, "\n%s", mutt_idna_print_version());
#endif

#ifdef CRYPT_BACKEND_GPGME
  fprintf(fp, "\nGPGME: %s", mutt_gpgme_print_version());
#endif

#ifdef USE_SSL_OPENSSL
#ifdef LIBRESSL_VERSION_TEXT
  fprintf(fp, "\nLibreSSL: %s", LIBRESSL_VERSION_TEXT);
#endif
#ifdef OPENSSL_VERSION_TEXT
  fprintf(fp, "\nOpenSSL: %s", OPENSSL_VERSION_TEXT);
#endif
#endif

#ifdef USE_SSL_GNUTLS
  fprintf(fp, "\nGnuTLS: %s", GNUTLS_VERSION);
#endif

#ifdef HAVE_NOTMUCH
  fprintf(fp, "\nlibnotmuch: %d.%d.%d", LIBNOTMUCH_MAJOR_VERSION,
          LIBNOTMUCH_MINOR_VERSION, LIBNOTMUCH_MICRO_VERSION);
#endif

#ifdef HAVE_PCRE2
  {
    char version[24];
    pcre2_config(PCRE2_CONFIG_VERSION, version);
    fprintf(fp, "\nPCRE2: %s", version);
  }
#endif

#ifdef USE_HCACHE
  const char *backends = store_backend_list();
  fprintf(fp, "\nstorage: %s", backends);
  FREE(&backends);
#ifdef USE_HCACHE_COMPRESSION
  backends = compress_list();
  fprintf(fp, "\ncompression: %s", backends);
  FREE(&backends);
#endif
#endif

  rstrip_in_place((char *) configure_options);
  fprintf(fp, "\n\nConfigure options: %s\n", (char *) configure_options);

  rstrip_in_place((char *) cc_cflags);
  fprintf(fp, "\nCompilation CFLAGS: %s\n", (char *) cc_cflags);

  fprintf(fp, "\n%s\n", _("Default options:"));
  print_compile_options(comp_opts_default, fp);

  fprintf(fp, "\n%s\n", _("Compile options:"));
  print_compile_options(comp_opts, fp);

#ifdef DOMAIN
  fprintf(fp, "DOMAIN=\"%s\"\n", DOMAIN);
#endif
#ifdef ISPELL
  fprintf(fp, "ISPELL=\"%s\"\n", ISPELL);
#endif
  fprintf(fp, "MAILPATH=\"%s\"\n", MAILPATH);
#ifdef MIXMASTER
  fprintf(fp, "MIXMASTER=\"%s\"\n", MIXMASTER);
#endif
  fprintf(fp, "PKGDATADIR=\"%s\"\n", PKGDATADIR);
  fprintf(fp, "SENDMAIL=\"%s\"\n", SENDMAIL);
  fprintf(fp, "SYSCONFDIR=\"%s\"\n", SYSCONFDIR);

  fprintf(fp, "\n");
  fputs(_(ReachingUs), fp);
}

/**
 * print_copyright - Print copyright message
 *
 * Print the authors' copyright messages, the GPL license and some contact
 * information for the NeoMutt project.
 */
void print_copyright(void)
{
  puts(mutt_make_version());
  puts(Copyright);
  puts(_(Thanks));
  puts(_(License));
  puts(_(ReachingUs));
}

/**
 * feature_enabled - Test if a compile-time feature is enabled
 * @param name  Compile-time symbol of the feature
 * @retval true  Feature enabled
 * @retval false Feature not enabled, or not compiled in
 *
 * Many of the larger features of neomutt can be disabled at compile time.
 * They define a symbol and use ifdef's around their code.
 * The symbols are mirrored in "CompileOptions comp_opts[]" in this
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
  for (int i = 0; comp_opts_default[i].name; i++)
  {
    if (mutt_str_equal(name, comp_opts_default[i].name))
    {
      return true;
    }
  }
  for (int i = 0; comp_opts[i].name; i++)
  {
    if (mutt_str_equal(name, comp_opts[i].name))
    {
      return comp_opts[i].enabled;
    }
  }
  return false;
}
