/**
 * @file
 * Conversion between different character encodings
 *
 * @authors
 * Copyright (C) 1999-2002,2007 Thomas Roessler <roessler@does-not-exist.org>
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
#include <ctype.h>
#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "mutt_charset.h"
#include "globals.h"
#include "protos.h"

/**
 * struct Lookup - Regex to String lookup table
 *
 * This is used by 'charset-hook' and 'iconv-hook'.
 */
struct Lookup
{
  enum LookupType type; /**< Lookup type */
  struct Regex regex;   /**< Regular expression */
  char *replacement;    /**< Alternative charset to use */
  TAILQ_ENTRY(Lookup) entries;
};
static TAILQ_HEAD(LookupHead, Lookup) Lookups = TAILQ_HEAD_INITIALIZER(Lookups);

/**
 * mutt_iconv_open - Set up iconv for conversions
 *
 * Like iconv_open, but canonicalises the charsets, applies charset-hooks,
 * recanonicalises, and finally applies iconv-hooks. Parameter flags=0 skips
 * charset-hooks, while MUTT_ICONV_HOOK_FROM applies them to fromcode. Callers
 * should use flags=0 when fromcode can safely be considered true, either some
 * constant, or some value provided by the user; MUTT_ICONV_HOOK_FROM should be
 * used only when fromcode is unsure, taken from a possibly wrong incoming MIME
 * label, or such. Misusing MUTT_ICONV_HOOK_FROM leads to unwanted interactions
 * in some setups. Note: By design charset-hooks should never be, and are never,
 * applied to tocode. Highlight note: The top-well-named MUTT_ICONV_HOOK_FROM
 * acts on charset-hooks, not at all on iconv-hooks.
 */
iconv_t mutt_iconv_open(const char *tocode, const char *fromcode, int flags)
{
  char tocode1[SHORT_STRING];
  char fromcode1[SHORT_STRING];
  const char *tocode2 = NULL, *fromcode2 = NULL;
  const char *tmp = NULL;

  iconv_t cd;

  /* transform to MIME preferred charset names */
  mutt_cs_canonical_charset(tocode1, sizeof(tocode1), tocode);
  mutt_cs_canonical_charset(fromcode1, sizeof(fromcode1), fromcode);

  /* maybe apply charset-hooks and recanonicalise fromcode,
   * but only when caller asked us to sanitize a potentially wrong
   * charset name incoming from the wild exterior. */
  if (flags & MUTT_ICONV_HOOK_FROM)
  {
    tmp = mutt_cs_charset_lookup(fromcode1);
    if (tmp)
      mutt_cs_canonical_charset(fromcode1, sizeof(fromcode1), tmp);
  }

  /* always apply iconv-hooks to suit system's iconv tastes */
  tocode2 = mutt_cs_iconv_lookup(tocode1);
  tocode2 = (tocode2) ? tocode2 : tocode1;
  fromcode2 = mutt_cs_iconv_lookup(fromcode1);
  fromcode2 = (fromcode2) ? fromcode2 : fromcode1;

  /* call system iconv with names it appreciates */
  cd = iconv_open(tocode2, fromcode2);
  if (cd != (iconv_t) -1)
    return cd;

  return (iconv_t) -1;
}

/**
 * mutt_convert_string - Convert a string between encodings
 *
 * Parameter flags is given as-is to mutt_iconv_open().
 * See there for its meaning and usage policy.
 */
int mutt_convert_string(char **ps, const char *from, const char *to, int flags)
{
  iconv_t cd;
  const char *repls[] = { "\357\277\275", "?", 0 };
  char *s = *ps;

  if (!s || !*s)
    return 0;

  if (to && from && (cd = mutt_iconv_open(to, from, flags)) != (iconv_t) -1)
  {
    size_t len;
    const char *ib = NULL;
    char *buf = NULL, *ob = NULL;
    size_t ibl, obl;
    const char **inrepls = NULL;
    char *outrepl = NULL;

    if (mutt_cs_is_utf8(to))
      outrepl = "\357\277\275";
    else if (mutt_cs_is_utf8(from))
      inrepls = repls;
    else
      outrepl = "?";

    len = strlen(s);
    ib = s;
    ibl = len + 1;
    obl = MB_LEN_MAX * ibl;
    ob = buf = mutt_mem_malloc(obl + 1);

    mutt_cs_iconv(cd, &ib, &ibl, &ob, &obl, inrepls, outrepl);
    iconv_close(cd);

    *ob = '\0';

    FREE(ps);
    *ps = buf;

    mutt_str_adjust(ps);
    return 0;
  }
  else
    return -1;
}

/**
 * fgetconv_open - Prepare a file for charset conversion
 * @param file  FILE ptr to prepare
 * @param from  Current character set
 * @param to    Destination character set
 * @param flags Flags, e.g. MUTT_ICONV_HOOK_FROM
 * @retval ptr fgetconv handle
 *
 * Parameter flags is given as-is to mutt_iconv_open().
 */
struct FgetConv *fgetconv_open(FILE *file, const char *from, const char *to, int flags)
{
  struct FgetConv *fc = NULL;
  iconv_t cd = (iconv_t) -1;
  static const char *repls[] = { "\357\277\275", "?", 0 };

  if (from && to)
    cd = mutt_iconv_open(to, from, flags);

  if (cd != (iconv_t) -1)
  {
    fc = mutt_mem_malloc(sizeof(struct FgetConv));
    fc->p = fc->ob = fc->bufo;
    fc->ib = fc->bufi;
    fc->ibl = 0;
    fc->inrepls = mutt_cs_is_utf8(to) ? repls : repls + 1;
  }
  else
    fc = mutt_mem_malloc(sizeof(struct FgetConvNot));
  fc->file = file;
  fc->cd = cd;
  return fc;
}

bool mutt_check_charset(const char *s, bool strict)
{
  iconv_t cd;

  if (mutt_cs_is_utf8(s))
    return true;

  if (!strict)
    for (int i = 0; PreferredMIMENames[i].key; i++)
    {
      if ((mutt_str_strcasecmp(PreferredMIMENames[i].key, s) == 0) ||
          (mutt_str_strcasecmp(PreferredMIMENames[i].pref, s) == 0))
      {
        return true;
      }
    }

  cd = mutt_iconv_open(s, s, 0);
  if (cd != (iconv_t)(-1))
  {
    iconv_close(cd);
    return true;
  }

  return false;
}

/**
 * lookup_charset - Look for a preferred character set name
 * @param type Type, e.g. #MUTT_LOOKUP_CHARSET
 * @param cs   Character set
 *
 * If the character set matches one of the regexes,
 * then return the replacement name.
 */
static const char *lookup_charset(enum LookupType type, const char *cs)
{
  if (!cs)
    return NULL;

  struct Lookup *l = NULL;

  TAILQ_FOREACH(l, &Lookups, entries)
  {
    if (l->type != type)
      continue;
    if (regexec(l->regex.regex, cs, 0, NULL, 0) == 0)
      return l->replacement;
  }
  return NULL;
}

/**
 * mutt_cs_lookup_add - Add a new character set lookup
 * @param type    Type of character set, e.g. MUTT_LOOKUP_CHARSET 
 * @param pat     Pattern to match
 * @param replace Replacement string
 * @param err     Buffer for error message
 * @retval true, lookup added to list
 * @retval false, Regex string was invalid
 *
 * Add a regex for a character set and a replacement name.
 */
bool mutt_cs_lookup_add(enum LookupType type, const char *pat,
                        const char *replace, struct Buffer *err)
{
  if (!pat || !replace)
    return false;

  regex_t *rx = mutt_mem_malloc(sizeof(regex_t));
  int rc = REGCOMP(rx, pat, REG_ICASE);
  if (rc != 0)
  {
    regerror(rc, rx, err->data, err->dsize);
    FREE(&rx);
    return false;
  }

  struct Lookup *l = mutt_mem_calloc(1, sizeof(struct Lookup));
  l->type = type;
  l->replacement = mutt_str_strdup(replace);
  l->regex.pattern = mutt_str_strdup(pat);
  l->regex.regex = rx;
  l->regex.not = false;

  TAILQ_INSERT_TAIL(&Lookups, l, entries);

  return true;
}

/**
 * mutt_cs_lookup_remove - Remove all the character set lookups
 *
 * Empty the list of replacement character set names.
 */
void mutt_cs_lookup_remove(void)
{
  struct Lookup *l = NULL;
  struct Lookup *tmp = NULL;

  TAILQ_FOREACH_SAFE(l, &Lookups, entries, tmp)
  {
    TAILQ_REMOVE(&Lookups, l, entries);
    FREE(&l->replacement);
    FREE(&l->regex.pattern);
    if (l->regex.regex)
      regfree(l->regex.regex);
    FREE(&l->regex);
    FREE(&l);
  }
}

/**
 * mutt_cs_charset_lookup - Look for a replacement character set
 * @param chs Character set to lookup
 * @retval ptr  Replacement character set (if a 'charset-hook' matches)
 * @retval NULL No matching hook
 *
 * Look through all the 'charset-hook's.
 * If one matches return the replacement character set.
 */
const char *mutt_cs_charset_lookup(const char *chs)
{
  return lookup_charset(MUTT_LOOKUP_CHARSET, chs);
}

/**
 * mutt_cs_iconv_lookup - Look for a replacement character set
 * @param chs Character set to lookup
 * @retval ptr  Replacement character set (if a 'iconv-hook' matches)
 * @retval NULL No matching hook
 *
 * Look through all the 'iconv-hook's.
 * If one matches return the replacement character set.
 */
const char *mutt_cs_iconv_lookup(const char *chs)
{
  return lookup_charset(MUTT_LOOKUP_ICONV, chs);
}
