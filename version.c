/**
 * @file
 * Display version and copyright about NeoMutt
 *
 * @authors
 * Copyright (C) 2016-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2016-2025 Richard Russon <rich@flatcap.org>
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
 * @page main_version Display version and copyright about NeoMutt
 *
 * Display version and copyright about NeoMutt
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "version.h"
#include "compress/lib.h"
#include "store/lib.h"
#include "globals.h"
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
/// Set PCRE2 to use 8-bit character strings
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

/// CLI: Width to wrap version info
static const int SCREEN_WIDTH = 80;

extern unsigned char cc_cflags[];
extern unsigned char configure_options[];

/// CLI Version: Authors' copyrights
static const char *Copyright =
    "Copyright (C) 2015-2026 Richard Russon <rich@flatcap.org>\n"
    "Copyright (C) 2016-2025 Pietro Cerutti <gahr@gahr.ch>\n"
    "Copyright (C) 2017-2019 Mehdi Abaakouk <sileht@sileht.net>\n"
    "Copyright (C) 2018-2020 Federico Kircheis <federico.kircheis@gmail.com>\n"
    "Copyright (C) 2017-2022 Austin Ray <austin@austinray.io>\n"
    "Copyright (C) 2023-2025 Dennis Schön <mail@dennis-schoen.de>\n"
    "Copyright (C) 2016-2017 Damien Riegel <damien.riegel@gmail.com>\n"
    "Copyright (C) 2023-2025 Rayford Shireman\n"
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
static const char *Notice1 = N_("Copyright (C) 2015-2026 Richard Russon and friends");
static const char *Notice2 = N_("NeoMutt is free software, provided without warranty: `neomutt -vv` for details");
// clang-format on

/* These are sorted by the display string */
/// Compile options strings for `neomutt -v` output
static const struct CompileOption CompOpts[] = {
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

/// Devel options strings for `neomutt -v` output
static const struct CompileOption DevelOpts[] = {
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
 * mutt_make_version - Generate the NeoMutt version string
 * @retval ptr Version string
 *
 * @note This returns a pointer to a static buffer
 */
const char *mutt_make_version(void)
{
  static char vstring[256];
  snprintf(vstring, sizeof(vstring), "%s%s", PACKAGE_VERSION, GitVer);
  return vstring;
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
 * system_get - Get info about system libraries
 * @param[out] kva Array for results
 */
static void system_get(struct KeyValueArray *kva)
{
  struct Buffer *buf = buf_pool_get();
  struct KeyValue kv = { 0 };

  struct utsname uts = { 0 };
  uname(&uts);

#ifdef SCO
  buf_printf(buf, "SCO %s", uts.release);
#else
  buf_printf(buf, "%s %s", uts.sysname, uts.release);
#endif
  buf_add_printf(buf, " (%s)", uts.machine);

  kv.key = mutt_str_dup("System");
  kv.value = buf_strdup(buf);
  ARRAY_ADD(kva, kv);

  buf_strcpy(buf, curses_version());
#ifdef NCURSES_VERSION
  buf_add_printf(buf, " (compiled with %s.%d)", NCURSES_VERSION, NCURSES_VERSION_PATCH);
#endif
  kv.key = mutt_str_dup("ncurses");
  kv.value = buf_strdup(buf);
  ARRAY_ADD(kva, kv);

#ifdef _LIBICONV_VERSION
  buf_printf(buf, "%d.%d", _LIBICONV_VERSION >> 8, _LIBICONV_VERSION & 0xff);
  kv.key = mutt_str_dup("libiconv");
  kv.value = buf_strdup(buf);
  ARRAY_ADD(kva, kv);
#endif

#ifdef HAVE_LIBIDN
  kv.key = mutt_str_dup("libidn2");
  kv.value = mutt_str_dup(mutt_idna_print_version());
  ARRAY_ADD(kva, kv);
#endif

#ifdef CRYPT_BACKEND_GPGME
  kv.key = mutt_str_dup("GPGME");
  kv.value = mutt_str_dup(mutt_gpgme_print_version());
  ARRAY_ADD(kva, kv);
#endif

#ifdef USE_SSL_OPENSSL
#ifdef LIBRESSL_VERSION_TEXT
  kv.key = mutt_str_dup("LibreSSL");
  kv.value = mutt_str_dup(LIBRESSL_VERSION_TEXT);
#endif
#ifdef OPENSSL_VERSION_TEXT
  kv.key = mutt_str_dup("OpenSSL");
  kv.value = mutt_str_dup(OPENSSL_VERSION_TEXT);
#endif
  ARRAY_ADD(kva, kv);
#endif

#ifdef USE_SSL_GNUTLS
  kv.key = mutt_str_dup("GnuTLS");
  kv.value = mutt_str_dup(GNUTLS_VERSION);
  ARRAY_ADD(kva, kv);
#endif

#ifdef HAVE_NOTMUCH
  buf_printf(buf, "%d.%d.%d", LIBNOTMUCH_MAJOR_VERSION,
             LIBNOTMUCH_MINOR_VERSION, LIBNOTMUCH_MICRO_VERSION);
  kv.key = mutt_str_dup("libnotmuch");
  kv.value = buf_strdup(buf);
  ARRAY_ADD(kva, kv);
#endif

#ifdef HAVE_PCRE2
  char version[24] = { 0 };
  pcre2_config(PCRE2_CONFIG_VERSION, version);
  kv.key = mutt_str_dup("PCRE2");
  kv.value = mutt_str_dup(version);
  ARRAY_ADD(kva, kv);
#endif

  buf_pool_release(&buf);
}

/**
 * paths_get - Get compiled-in paths
 * @param[out] kva Array for results
 */
static void paths_get(struct KeyValueArray *kva)
{
  struct KeyValue kv = { 0 };

#ifdef DOMAIN
  kv.key = mutt_str_dup("DOMAIN");
  kv.value = mutt_str_dup(DOMAIN);
  ARRAY_ADD(kva, kv);
#endif

#ifdef ISPELL
  kv.key = mutt_str_dup("ISPELL");
  kv.value = mutt_str_dup(ISPELL);
  ARRAY_ADD(kva, kv);
#endif
  kv.key = mutt_str_dup("MAILPATH");
  kv.value = mutt_str_dup(MAILPATH);
  ARRAY_ADD(kva, kv);

  kv.key = mutt_str_dup("PKGDATADIR");
  kv.value = mutt_str_dup(PKGDATADIR);
  ARRAY_ADD(kva, kv);

  kv.key = mutt_str_dup("SENDMAIL");
  kv.value = mutt_str_dup(SENDMAIL);
  ARRAY_ADD(kva, kv);

  kv.key = mutt_str_dup("SYSCONFDIR");
  kv.value = mutt_str_dup(SYSCONFDIR);
  ARRAY_ADD(kva, kv);
}

/**
 * kva_clear - Free the strings of a KeyValueArray
 * @param kva KeyValueArray to clear
 *
 * @note KeyValueArray itself is not freed
 */
static void kva_clear(struct KeyValueArray *kva)
{
  struct KeyValue *kv = NULL;

  ARRAY_FOREACH(kv, kva)
  {
    FREE(&kv->key);
    FREE(&kv->value);
  }

  ARRAY_FREE(kva);
}

/**
 * version_get - Get NeoMutt version info
 * @retval ptr NeoMuttVersion
 */
struct NeoMuttVersion *version_get(void)
{
  struct NeoMuttVersion *ver = MUTT_MEM_CALLOC(1, struct NeoMuttVersion);

  ver->version = mutt_str_dup(mutt_make_version());

  ARRAY_INIT(&ver->system);
  system_get(&ver->system);

#ifdef USE_HCACHE
  ver->storage = store_backend_list();
#ifdef USE_HCACHE_COMPRESSION
  ver->compression = compress_list();
#endif
#endif

  rstrip_in_place((char *) configure_options);
  ver->configure = slist_parse((char *) configure_options, D_SLIST_SEP_SPACE);

  rstrip_in_place((char *) cc_cflags);
  ver->compilation = slist_parse((char *) cc_cflags, D_SLIST_SEP_SPACE);

  ver->feature = CompOpts;

  if (DevelOpts[0].name)
    ver->devel = DevelOpts;

  ARRAY_INIT(&ver->paths);
  paths_get(&ver->paths);

  return ver;
}

/**
 * version_free - Free a NeoMuttVersion
 * @param ptr NeoMuttVersion to free
 */
void version_free(struct NeoMuttVersion **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NeoMuttVersion *ver = *ptr;

  FREE(&ver->version);

  kva_clear(&ver->system);
  kva_clear(&ver->paths);

  slist_free(&ver->storage);
  slist_free(&ver->compression);
  slist_free(&ver->configure);
  slist_free(&ver->compilation);

  FREE(ptr);
}

/**
 * print_compile_options - Print a list of enabled/disabled features
 * @param co       Array of compile options
 * @param fp       file to write to
 * @param use_ansi Use ANSI colour escape sequences
 *
 * Two lists are generated and passed to this function:
 * - List of features, e.g. +notmuch
 * - List of devel features, e.g. window
 *
 * The output is of the form: "+enabled_feature -disabled_feature" and is
 * wrapped to SCREEN_WIDTH characters.
 */
static void print_compile_options(const struct CompileOption *co, FILE *fp, bool use_ansi)
{
  if (!co || !fp)
    return;

  size_t used = 2;
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
        if (use_ansi)
          fmt = "\033[1;31m-%s\033[0m "; // Escape, red
        else
          fmt = "-%s ";
        break;
      case 1: // Enabled
        if (use_ansi)
          fmt = "\033[1;32m+%s\033[0m "; // Escape, green
        else
          fmt = "+%s ";
        break;
      case 2: // Devel only
        if (use_ansi)
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
 * print_version - Print system and compile info to a file
 * @param fp       File to print to
 * @param use_ansi Use ANSI colour escape sequences
 * @retval true Text displayed
 *
 * Print information about the current system NeoMutt is running on.
 * Also print a list of all the compile-time information.
 */
bool print_version(FILE *fp, bool use_ansi)
{
  if (!fp)
    return false;

  struct KeyValue *kv = NULL;
  struct ListNode *np = NULL;
  struct NeoMuttVersion *ver = version_get();

  const char *col_cyan = "";
  const char *col_bold = "";
  const char *col_end = "";

  if (use_ansi)
  {
    col_cyan = "\033[1;36m"; // Escape, cyan
    col_bold = "\033[1m";    // Escape, bold
    col_end = "\033[0m";     // Escape, end
  }

  fprintf(fp, "%sNeoMutt %s%s\n", col_cyan, ver->version, col_end);
  fprintf(fp, "%s\n", _(Notice1));
  fprintf(fp, "%s\n\n", _(Notice2));

  ARRAY_FOREACH(kv, &ver->system)
  {
    fprintf(fp, "%s%s:%s %s\n", col_bold, kv->key, col_end, kv->value);
  }

  if (ver->storage)
  {
    fprintf(fp, "%sstorage:%s ", col_bold, col_end);
    STAILQ_FOREACH(np, &ver->storage->head, entries)
    {
      fputs(np->data, fp);
      if (STAILQ_NEXT(np, entries))
        fputs(", ", fp);
    }
    fputs("\n", fp);
  }

  if (ver->compression)
  {
    fprintf(fp, "%scompression:%s ", col_bold, col_end);
    STAILQ_FOREACH(np, &ver->compression->head, entries)
    {
      fputs(np->data, fp);
      if (STAILQ_NEXT(np, entries))
        fputs(", ", fp);
    }
    fputs("\n", fp);
  }
  fputs("\n", fp);

  fprintf(fp, "%sConfigure options:%s ", col_bold, col_end);
  if (ver->configure)
  {
    STAILQ_FOREACH(np, &ver->configure->head, entries)
    {
      if (!np || !np->data)
        continue;
      fputs(np->data, fp);
      if (STAILQ_NEXT(np, entries))
        fputs(" ", fp);
    }
  }
  fputs("\n\n", fp);

  if (ver->compilation)
  {
    fprintf(fp, "%sCompilation CFLAGS:%s ", col_bold, col_end);
    STAILQ_FOREACH(np, &ver->compilation->head, entries)
    {
      if (!np || !np->data)
        continue;
      fputs(np->data, fp);
      if (STAILQ_NEXT(np, entries))
        fputs(" ", fp);
    }
    fputs("\n\n", fp);
  }

  fprintf(fp, "%s%s%s\n", col_bold, _("Compile options:"), col_end);
  print_compile_options(ver->feature, fp, use_ansi);
  fputs("\n", fp);

  if (ver->devel)
  {
    fprintf(fp, "%s%s%s\n", col_bold, _("Devel options:"), col_end);
    print_compile_options(ver->devel, fp, use_ansi);
    fputs("\n", fp);
  }

  ARRAY_FOREACH(kv, &ver->paths)
  {
    fprintf(fp, "%s%s%s=\"%s\"\n", col_bold, kv->key, col_end, kv->value);
  }
  fputs("\n", fp);

  fputs(_(ReachingUs), fp);

  version_free(&ver);
  return true;
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
  printf("NeoMutt %s\n", mutt_make_version());
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
 * The symbols are mirrored in "CompileOption CompOpts[]" in this
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
