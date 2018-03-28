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

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt_curses.h"
#ifdef HAVE_STRINGPREP_H
#include <stringprep.h>
#elif defined(HAVE_IDN_STRINGPREP_H)
#include <idn/stringprep.h>
#endif

/* #include "protos.h" */
const char *mutt_make_version(void);
/* #include "hcache/hcache.h" */
const char *mutt_hcache_backend_list(void);

const int SCREEN_WIDTH = 80;

extern unsigned char cc_version[];
extern unsigned char cc_cflags[];
extern unsigned char configure_options[];

static const char *Copyright =
    "Copyright (C) 1996-2016 Michael R. Elkins <me@mutt.org>\n"
    "Copyright (C) 1996-2002 Brandon Long <blong@fiction.net>\n"
    "Copyright (C) 1997-2009 Thomas Roessler <roessler@does-not-exist.org>\n"
    "Copyright (C) 1998-2005 Werner Koch <wk@isil.d.shuttle.de>\n"
    "Copyright (C) 1999-2017 Brendan Cully <brendan@kublai.com>\n"
    "Copyright (C) 1999-2002 Tommi Komulainen <Tommi.Komulainen@iki.fi>\n"
    "Copyright (C) 2000-2004 Edmund Grimley Evans <edmundo@rano.org>\n"
    "Copyright (C) 2006-2009 Rocco Rutte <pdmef@gmx.net>\n"
    "Copyright (C) 2014-2017 Kevin J. McCarthy <kevin@8t8.us>\n"
    "Copyright (C) 2015-2017 Richard Russon <rich@flatcap.org>\n";

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
    "    GNU General Public License for more details.\n");

static const char *Obtaining =
    N_("    You should have received a copy of the GNU General Public License\n"
       "    along with this program; if not, write to the Free Software\n"
       "    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  "
       "02110-1301, USA.\n");

static const char *ReachingUs =
    N_("To learn more about NeoMutt, visit: https://www.neomutt.org\n"
       "If you find a bug in NeoMutt, please raise an issue at:\n"
       "    https://github.com/neomutt/neomutt/issues\n"
       "or send an email to: <neomutt-devel@neomutt.org>\n");

// clang-format off
static const char *Notice =
    N_("Copyright (C) 1996-2016 Michael R. Elkins and others.\n"
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
  bool enabled;
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
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "pgp", 1 },
#else
  { "pgp", 0 },
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
  { NULL, 0 },
};

/**
 * print_compile_options - Print a list of enabled/disabled features
 *
 * Two lists are generated and passed to this function:
 *
 * One list which just uses the configure state of each feature.
 * One list which just uses feature which are set to on directly in NeoMutt.
 *
 * The output is of the form: "+enabled_feature -disabled_feature" and is
 * wrapped to SCREEN_WIDTH characters.
 */
static void print_compile_options(struct CompileOptions *co)
{
  size_t used = 2;
  bool tty = stdout ? isatty(fileno(stdout)) : false;

  printf("  ");
  for (int i = 0; co[i].name; i++)
  {
    const size_t len = strlen(co[i].name) + 2; /* +/- and a space */
    if ((used + len) > SCREEN_WIDTH)
    {
      used = 2;
      printf("\n  ");
    }
    used += len;
    if (co[i].enabled)
    {
      if (tty)
        printf("\033[1;32m+%s\033[0m ", co[i].name);
      else
        printf("+%s ", co[i].name);
    }
    else
    {
      if (tty)
        printf("\033[1;31m-%s\033[0m ", co[i].name);
      else
        printf("-%s ", co[i].name);
    }
  }
  puts("");
}

/**
 * rstrip_in_place - Strip a trailing carriage return
 * @param s  String to be modified
 * @retval string The modified string
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
 * print_version - Print system and compile info
 *
 * Print information about the current system NeoMutt is running on.
 * Also print a list of all the compile-time information.
 */
void print_version(void)
{
  struct utsname uts;

  puts(mutt_make_version());
  puts(_(Notice));

  uname(&uts);

#ifdef SCO
  printf("System: SCO %s", uts.release);
#else
  printf("System: %s %s", uts.sysname, uts.release);
#endif

  printf(" (%s)", uts.machine);

#ifdef NCURSES_VERSION
  printf("\nncurses: %s (compiled with %s.%d)", curses_version(),
         NCURSES_VERSION, NCURSES_VERSION_PATCH);
#elif defined(USE_SLANG_CURSES)
  printf("\nslang: %s", SLANG_VERSION_STRING);
#endif

#ifdef _LIBICONV_VERSION
  printf("\nlibiconv: %d.%d", _LIBICONV_VERSION >> 8, _LIBICONV_VERSION & 0xff);
#endif

#ifdef HAVE_LIBIDN
  printf("\nlibidn: %s (compiled with %s)", stringprep_check_version(NULL), STRINGPREP_VERSION);
#endif

#ifdef USE_HCACHE
  const char *backends = mutt_hcache_backend_list();
  printf("\nhcache backends: %s", backends);
  FREE(&backends);
#endif

  puts("\n\nCompiler:");
  rstrip_in_place((char *) cc_version);
  puts((char *) cc_version);

  rstrip_in_place((char *) configure_options);
  printf("\nConfigure options: %s\n", (char *) configure_options);

  rstrip_in_place((char *) cc_cflags);
  printf("\nCompilation CFLAGS: %s\n", (char *) cc_cflags);

  printf("\n%s\n", _("Default options:"));
  print_compile_options(comp_opts_default);

  printf("\n%s\n", _("Compile options:"));
  print_compile_options(comp_opts);

#ifdef DOMAIN
  printf("DOMAIN=\"%s\"\n", DOMAIN);
#endif
#ifdef ISPELL
  printf("ISPELL=\"%s\"\n", ISPELL);
#endif
  printf("MAILPATH=\"%s\"\n", MAILPATH);
#ifdef MIXMASTER
  printf("MIXMASTER=\"%s\"\n", MIXMASTER);
#endif
  printf("PKGDATADIR=\"%s\"\n", PKGDATADIR);
  printf("SENDMAIL=\"%s\"\n", SENDMAIL);
  printf("SYSCONFDIR=\"%s\"\n", SYSCONFDIR);

  puts("");
  puts(_(ReachingUs));
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
  puts(_(Obtaining));
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
    if (mutt_str_strcmp(name, comp_opts_default[i].name) == 0)
    {
      return true;
    }
  }
  for (int i = 0; comp_opts[i].name; i++)
  {
    if (mutt_str_strcmp(name, comp_opts[i].name) == 0)
    {
      return comp_opts[i].enabled;
    }
  }
  return false;
}
