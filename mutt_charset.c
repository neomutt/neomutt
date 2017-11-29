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
  char *tocode2 = NULL, *fromcode2 = NULL;
  char *tmp = NULL;

  iconv_t cd;

  /* transform to MIME preferred charset names */
  mutt_cs_canonical_charset(tocode1, sizeof(tocode1), tocode);
  mutt_cs_canonical_charset(fromcode1, sizeof(fromcode1), fromcode);

  /* maybe apply charset-hooks and recanonicalise fromcode,
   * but only when caller asked us to sanitize a potentially wrong
   * charset name incoming from the wild exterior. */
  if ((flags & MUTT_ICONV_HOOK_FROM) && (tmp = mutt_charset_hook(fromcode1)))
    mutt_cs_canonical_charset(fromcode1, sizeof(fromcode1), tmp);

  /* always apply iconv-hooks to suit system's iconv tastes */
  tocode2 = mutt_iconv_hook(tocode1);
  tocode2 = (tocode2) ? tocode2 : tocode1;
  fromcode2 = mutt_iconv_hook(fromcode1);
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
FGETCONV *fgetconv_open(FILE *file, const char *from, const char *to, int flags)
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
  return (FGETCONV *) fc;
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
