/**
 * Copyright (C) 1996-2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2007 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2016-2017 Richard Russon
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
void mutt_print_patchlist(void);
/* #include "hcache.h" */
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
  int enabled;
};

static struct compile_options comp_opts[] = {
#ifdef CRYPT_BACKEND_CLASSIC_PGP
    {"CRYPT_BACKEND_CLASSIC_PGP", 1},
#else
    {"CRYPT_BACKEND_CLASSIC_PGP", 0},
#endif
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
    {"CRYPT_BACKEND_CLASSIC_SMIME", 1},
#else
    {"CRYPT_BACKEND_CLASSIC_SMIME", 0},
#endif
#ifdef CRYPT_BACKEND_GPGME
    {"CRYPT_BACKEND_GPGME", 1},
#else
    {"CRYPT_BACKEND_GPGME", 0},
#endif
#ifdef DEBUG
    {"DEBUG", 1},
#else
    {"DEBUG", 0},
#endif
#ifdef DL_STANDALONE
    {"DL_STANDALONE", 1},
#else
    {"DL_STANDALONE", 0},
#endif
#ifdef ENABLE_NLS
    {"ENABLE_NLS", 1},
#else
    {"ENABLE_NLS", 0},
#endif
#ifdef EXACT_ADDRESS
    {"EXACT_ADDRESS", 1},
#else
    {"EXACT_ADDRESS", 0},
#endif
#ifdef HOMESPOOL
    {"HOMESPOOL", 1},
#else
    {"HOMESPOOL", 0},
#endif
#ifdef LOCALES_HACK
    {"LOCALES_HACK", 1},
#else
    {"LOCALES_HACK", 0},
#endif
#ifdef SUN_ATTACHMENT
    {"SUN_ATTACHMENT", 1},
#else
    {"SUN_ATTACHMENT", 0},
#endif
#ifdef HAVE_BKGDSET
    {"HAVE_BKGDSET", 1},
#else
    {"HAVE_BKGDSET", 0},
#endif
#ifdef HAVE_COLOR
    {"HAVE_COLOR", 1},
#else
    {"HAVE_COLOR", 0},
#endif
#ifdef HAVE_CURS_SET
    {"HAVE_CURS_SET", 1},
#else
    {"HAVE_CURS_SET", 0},
#endif
#ifdef HAVE_FUTIMENS
    {"HAVE_FUTIMENS", 1},
#else
    {"HAVE_FUTIMENS", 0},
#endif
#ifdef HAVE_GETADDRINFO
    {"HAVE_GETADDRINFO", 1},
#else
    {"HAVE_GETADDRINFO", 0},
#endif
#ifdef HAVE_GETSID
    {"HAVE_GETSID", 1},
#else
    {"HAVE_GETSID", 0},
#endif
#ifdef HAVE_LANGINFO_CODESET
    {"HAVE_LANGINFO_CODESET", 1},
#else
    {"HAVE_LANGINFO_CODESET", 0},
#endif
#ifdef HAVE_LANGINFO_YESEXPR
    {"HAVE_LANGINFO_YESEXPR", 1},
#else
    {"HAVE_LANGINFO_YESEXPR", 0},
#endif
#ifdef HAVE_LIBIDN
    {"HAVE_LIBIDN", 1},
#else
    {"HAVE_LIBIDN", 0},
#endif
#ifdef HAVE_META
    {"HAVE_META", 1},
#else
    {"HAVE_META", 0},
#endif
#ifdef HAVE_REGCOMP
    {"HAVE_REGCOMP", 1},
#else
    {"HAVE_REGCOMP", 0},
#endif
#ifdef HAVE_RESIZETERM
    {"HAVE_RESIZETERM", 1},
#else
    {"HAVE_RESIZETERM", 0},
#endif
#ifdef HAVE_START_COLOR
    {"HAVE_START_COLOR", 1},
#else
    {"HAVE_START_COLOR", 0},
#endif
#ifdef HAVE_TYPEAHEAD
    {"HAVE_TYPEAHEAD", 1},
#else
    {"HAVE_TYPEAHEAD", 0},
#endif
#ifdef HAVE_WC_FUNCS
    {"HAVE_WC_FUNCS", 1},
#else
    {"HAVE_WC_FUNCS", 0},
#endif
#ifdef ICONV_NONTRANS
    {"ICONV_NONTRANS", 1},
#else
    {"ICONV_NONTRANS", 0},
#endif
#ifdef USE_COMPRESSED
    {"USE_COMPRESSED", 1},
#else
    {"USE_COMPRESSED", 0},
#endif
#ifdef USE_DOTLOCK
    {"USE_DOTLOCK", 1},
#else
    {"USE_DOTLOCK", 0},
#endif
#ifdef USE_FCNTL
    {"USE_FCNTL", 1},
#else
    {"USE_FCNTL", 0},
#endif
#ifdef USE_FLOCK
    {"USE_FLOCK", 1},
#else
    {"USE_FLOCK", 0},
#endif
#ifdef USE_FMEMOPEN
    {"USE_FMEMOPEN", 1},
#else
    {"USE_FMEMOPEN", 0},
#endif
#ifdef USE_GSS
    {"USE_GSS", 1},
#else
    {"USE_GSS", 0},
#endif
#ifdef USE_HCACHE
    {"USE_HCACHE", 1},
#else
    {"USE_HCACHE", 0},
#endif
#ifdef USE_IMAP
    {"USE_IMAP", 1},
#else
    {"USE_IMAP", 0},
#endif
#ifdef USE_LUA
    {"USE_LUA", 1},
#else
    {"USE_LUA", 0},
#endif
#ifdef USE_NOTMUCH
    {"USE_NOTMUCH", 1},
#else
    {"USE_NOTMUCH", 0},
#endif
#ifdef USE_NNTP
    {"USE_NNTP", 1},
#else
    {"USE_NNTP", 0},
#endif
#ifdef USE_POP
    {"USE_POP", 1},
#else
    {"USE_POP", 0},
#endif
#ifdef USE_SASL
    {"USE_SASL", 1},
#else
    {"USE_SASL", 0},
#endif
#ifdef USE_SETGID
    {"USE_SETGID", 1},
#else
    {"USE_SETGID", 0},
#endif
#ifdef USE_SIDEBAR
    {"USE_SIDEBAR", 1},
#endif
#ifdef USE_SMTP
    {"USE_SMTP", 1},
#else
    {"USE_SMTP", 0},
#endif
#ifdef USE_SSL_GNUTLS
    {"USE_SSL_GNUTLS", 1},
#else
    {"USE_SSL_GNUTLS", 0},
#endif
#ifdef USE_SSL_OPENSSL
    {"USE_SSL_OPENSSL", 1},
#else
    {"USE_SSL_OPENSSL", 0},
#endif
    {NULL, 0},
};

/**
 * print_compile_options - Print a list of enabled/disabled features
 *
 * The configure script lets uses enable/disable features.
 * This shows the Mutt user which features are/aren't available.
 *
 * The output is of the form: "+ENABLED_FEATURE -DISABLED_FEATURE" and is
 * wrapped to SCREEN_WIDTH characters.
 */
static void print_compile_options(void)
{
  int i;
  char c;
  int len;
  int used = 0;

  for (i = 0; comp_opts[i].name; i++)
  {
    len = strlen(comp_opts[i].name) + 2; /* +/- and a space */
    if ((used + len) > SCREEN_WIDTH)
    {
      used = 0;
      puts("");
    }
    used += len;
    c = comp_opts[i].enabled ? '+' : '-';
    printf("%c%s ", c, comp_opts[i].name);
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
#else
  puts("-DOMAIN");
#endif

#ifdef MIXMASTER
  printf("MIXMASTER=\"%s\"\n", MIXMASTER);
#else
  puts("-MIXMASTER");
#endif

#ifdef ISPELL
  printf("ISPELL=\"%s\"\n", ISPELL);
#else
  puts("-ISPELL");
#endif

  printf("SENDMAIL=\"%s\"\n", SENDMAIL);
  printf("MAILPATH=\"%s\"\n", MAILPATH);
  printf("PKGDATADIR=\"%s\"\n", PKGDATADIR);
  printf("SYSCONFDIR=\"%s\"\n", SYSCONFDIR);
  printf("EXECSHELL=\"%s\"\n", EXECSHELL);

  puts("");
  mutt_print_patchlist();

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
