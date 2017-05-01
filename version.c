/**
 * Copyright (C) 1996-2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2007 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2016-2017 Richard Russon <rich@flatcap.org>
 *
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
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include "lib.h"
#ifdef HAVE_STRINGPREP_H
#include <stringprep.h>
#elif defined(HAVE_IDN_STRINGPREP_H)
#include <idn/stringprep.h>
#endif
#ifdef USE_SLANG_CURSES
#include <slang.h>
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
    N_("Copyright (C) 1996-2016 Michael R. Elkins <me@mutt.org>\n"
       "Copyright (C) 1996-2002 Brandon Long <blong@fiction.net>\n"
       "Copyright (C) 1997-2009 Thomas Roessler <roessler@does-not-exist.org>\n"
       "Copyright (C) 1998-2005 Werner Koch <wk@isil.d.shuttle.de>\n"
       "Copyright (C) 1999-2014 Brendan Cully <brendan@kublai.com>\n"
       "Copyright (C) 1999-2002 Tommi Komulainen <Tommi.Komulainen@iki.fi>\n"
       "Copyright (C) 2000-2004 Edmund Grimley Evans <edmundo@rano.org>\n"
       "Copyright (C) 2006-2009 Rocco Rutte <pdmef@gmx.net>\n"
       "Copyright (C) 2014-2016 Kevin J. McCarthy <kevin@8t8.us>\n"
       "Copyright (C) 2015-2017 Richard Russon <rich@flatcap.org>\n"
       "\n"
       "Many others not mentioned here contributed code, fixes,\n"
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
    N_("To learn more about NeoMutt, visit: http://www.neomutt.org/\n"
       "If you find a bug in NeoMutt, please raise an issue at:\n"
       "    https://github.com/neomutt/neomutt/issues\n"
       "or send an email to: <neomutt-devel@neomutt.org>\n");

static const char *Notice =
    N_("Copyright (C) 1996-2016 Michael R. Elkins and others.\n"
       "Mutt comes with ABSOLUTELY NO WARRANTY; for details type `mutt -vv'.\n"
       "Mutt is free software, and you are welcome to redistribute it\n"
       "under certain conditions; type `mutt -vv' for details.\n");

struct compile_options
{
  const char *name;
  bool enabled;
};

/* These are sorted by the display string */
static struct compile_options comp_opts[] = {
  { "attach_headers_color", 1 },
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
  { "compose_to_sender", 1 },
  { "compress", 1 },
  { "cond_date", 1 },
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  { "crypt_pgp", 1 },
#else
  { "crypt_pgp", 0 },
#endif
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  { "crypt_smime", 1 },
#else
  { "crypt_smime", 0 },
#endif
#ifdef CRYPT_BACKEND_GPGME
  { "crypt_gpgme", 1 },
#else
  { "crypt_gpgme", 0 },
#endif
#ifdef HAVE_CURS_SET
  { "curs_set", 1 },
#else
  { "curs_set", 0 },
#endif
#ifdef DEBUG
  { "debug", 1 },
#else
  { "debug", 0 },
#endif
#ifdef USE_DOTLOCK
  { "dotlock", 1 },
#else
  { "dotlock", 0 },
#endif
#ifdef ENABLE_NLS
  { "enable_nls", 1 },
#else
  { "enable_nls", 0 },
#endif
  { "encrypt_to_self", 1 },
#ifdef EXACT_ADDRESS
  { "exact_address", 1 },
#else
  { "exact_address", 0 },
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
  { "forgotten_attachments", 1 },
  { "forwref", 1 },
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
  { "getsid", 1 },
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
#ifdef ICONV_NONTRANS
  { "iconv_nontrans", 1 },
#else
  { "iconv_nontrans", 0 },
#endif
  { "ifdef", 1 },
  { "imap", 1 },
  { "index_color", 1 },
  { "initials", 1 },
  { "keywords", 1 },
#ifdef HAVE_LIBIDN
  { "libidn", 1 },
#else
  { "libidn", 0 },
#endif
  { "limit_current_thread", 1 },
  { "lmdb", 1 },
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
  { "multiple_fcc", 1 },
  { "nested_if", 1 },
  { "new_mail", 1 },
  { "nntp", 1 },
#ifdef USE_NOTMUCH
  { "notmuch", 1 },
#else
  { "notmuch", 0 },
#endif
  { "pop", 1 },
  { "progress", 1 },
  { "quasi_delete", 1 },
  { "regcomp", 1 },
  { "reply_with_xorig", 1 },
#ifdef HAVE_RESIZETERM
  { "resizeterm", 1 },
#else
  { "resizeterm", 0 },
#endif
#ifdef USE_SASL
  { "sasl", 1 },
#else
  { "sasl", 0 },
#endif
  { "sensible_browser", 1 },
#ifdef USE_SETGID
  { "setgid", 1 },
#else
  { "setgid", 0 },
#endif
  { "sidebar", 1 },
  { "skip_quoted", 1 },
  { "smtp", 1 },
#ifdef USE_SSL_GNUTLS
  { "ssl_gnutls", 1 },
#else
  { "ssl_gnutls", 0 },
#endif
#ifdef USE_SSL_OPENSSL
  { "ssl_openssl", 1 },
#else
  { "ssl_openssl", 0 },
#endif
#ifdef HAVE_START_COLOR
  { "start_color", 1 },
#else
  { "start_color", 0 },
#endif
  { "status_color", 1 },
#ifdef SUN_ATTACHMENT
  { "sun_attachment", 1 },
#else
  { "sun_attachment", 0 },
#endif
  { "timeout", 1 },
  { "tls_sni", 1 },
  { "trash", 1 },
#ifdef HAVE_TYPEAHEAD
  { "typeahead", 1 },
#else
  { "typeahead", 0 },
#endif
#ifdef HAVE_WC_FUNCS
  { "wc_funcs", 1 },
#else
  { "wc_funcs", 0 },
#endif
  { NULL, 0 },
};

/**
 * print_compile_options - Print a list of enabled/disabled features
 *
 * The configure script lets uses enable/disable features.
 * This shows the Mutt user which features are/aren't available.
 *
 * The output is of the form: "+enabled_feature -disabled_feature" and is
 * wrapped to SCREEN_WIDTH characters.
 */
static void print_compile_options(void)
{
  int i;
  int len;
  int used = 2;
  bool tty = stdout ? isatty(fileno(stdout)) : false;

  printf("  ");
  for (i = 0; comp_opts[i].name; i++)
  {
    len = strlen(comp_opts[i].name) + 2; /* +/- and a space */
    if ((used + len) > SCREEN_WIDTH)
    {
      used = 2;
      printf("\n  ");
    }
    used += len;
    if (comp_opts[i].enabled)
    {
      if (tty)
        printf("\033[1;32m+%s\033[0m ", comp_opts[i].name);
      else
        printf("+%s ", comp_opts[i].name);
    }
    else
    {
      if (tty)
        printf("\033[1;31m-%s\033[0m ", comp_opts[i].name);
      else
        printf("-%s ", comp_opts[i].name);
    }
  }
  puts("");
}

/**
 * rstrip_in_place - Strip a trailing carriage return
 * @s:  String to be modified
 *
 * The string has its last carriage return set to NUL.
 * Returns:
 *      The modified string
 */
static char *rstrip_in_place(char *s)
{
  if (!s)
    return NULL;

  char *p = NULL;

  p = &s[strlen(s)];
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
 * Print information about the current system Mutt is running on.
 * Also print a list of all the compile-time information.
 */
void print_version(void)
{
  struct utsname uts;

  puts(mutt_make_version());
  puts(_(Notice));

  uname(&uts);

#ifdef _AIX
  printf("System: %s %s.%s", uts.sysname, uts.version, uts.release);
#elif defined(SCO)
  printf("System: SCO %s", uts.release);
#else
  printf("System: %s %s", uts.sysname, uts.release);
#endif

  printf(" (%s)", uts.machine);

#ifdef NCURSES_VERSION
  printf("\nncurses: %s (compiled with %s)", curses_version(), NCURSES_VERSION);
#elif defined(USE_SLANG_CURSES)
  printf("\nslang: %d", SLANG_VERSION);
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

  puts(_("\nCompile options:"));
  print_compile_options();

#ifdef DOMAIN
  printf("DOMAIN=\"%s\"\n", DOMAIN);
#endif
  printf("EXECSHELL=\"%s\"\n", EXECSHELL);
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
 * information for the Mutt project.
 */
void print_copyright(void)
{
  puts(mutt_make_version());
  puts(_(Copyright));
  puts(_(License));
  puts(_(Obtaining));
  puts(_(ReachingUs));
}

/**
 * feature_enabled - Test is a compile-time feature is enabled
 * @name:  Compile-time symbol of the feature
 *
 * Many of the larger features of mutt can be disabled at compile time.
 * They define a symbol and use #ifdef's around their code.
 * The symbols are mirrored in "struct compile_options comp_opts[]" in this
 * file.
 *
 * This function checks if one of these symbols is present in the code.
 *
 * These symbols are also seen in the output of "mutt -v".
 *
 * Returns:
 *      true:  Feature enabled
 *      false: Feature not enabled, or not compiled in
 */
bool feature_enabled(const char *name)
{
  if (!name)
    return false;

  int i;
  for (i = 0; comp_opts[i].name; i++)
  {
    if (mutt_strcmp(name, comp_opts[i].name) == 0)
    {
      return true;
    }
  }
  return false;
}
