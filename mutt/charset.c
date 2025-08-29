/**
 * @file
 * Conversion between different character encodings
 *
 * @authors
 * Copyright (C) 2017 Tobias Angele <toogley@mailbox.org>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Steinar H Gunderson <steinar+neomutt@gunderson.no>
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
 * @page mutt_charset Conversion between different character encodings
 *
 * Conversion between different character encodings
 */

#include "config.h"
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "charset.h"
#include "buffer.h"
#include "list.h"
#include "logging2.h"
#include "memory.h"
#include "pool.h"
#include "queue.h"
#include "regex3.h"
#include "slist.h"
#include "string2.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

#ifndef EILSEQ
#define EILSEQ EINVAL
#endif

/**
 * ReplacementChar - When a Unicode character can't be displayed, use this instead
 */
wchar_t ReplacementChar = '?';

/**
 * CharsetIsUtf8 - Is the user's current character set utf-8?
 */
bool CharsetIsUtf8 = false;

/**
 * struct Lookup - Regex to String lookup table
 *
 * This is used by 'charset-hook' and 'iconv-hook'.
 */
struct Lookup
{
  enum LookupType type;        ///< Lookup type
  struct Regex regex;          ///< Regular expression
  char *replacement;           ///< Alternative charset to use
  TAILQ_ENTRY(Lookup) entries; ///< Linked list
};
TAILQ_HEAD(LookupList, Lookup);

/// Lookup table of preferred character set names
static struct LookupList Lookups = TAILQ_HEAD_INITIALIZER(Lookups);

/**
 * struct IconvCacheEntry - Cached iconv conversion descriptor
 */
struct IconvCacheEntry
{
  char *fromcode1; ///< Source character set
  char *tocode1;   ///< Destination character set
  iconv_t cd;      ///< iconv conversion descriptor
};

/// Max size of the iconv cache
#define ICONV_CACHE_SIZE 16
/// Cache of iconv conversion descriptors
static struct IconvCacheEntry IconvCache[ICONV_CACHE_SIZE];
/// Number of iconv descriptors in the cache
static int IconvCacheUsed = 0;

/**
 * struct MimeNames - MIME name lookup entry
 */
struct MimeNames
{
  const char *key;
  const char *pref;
};

/**
 * PreferredMimeNames - Lookup table of preferred charsets
 *
 * The following list has been created manually from the data under:
 * http://www.isi.edu/in-notes/iana/assignments/character-sets
 * Last update: 2000-09-07
 *
 * @note It includes only the subset of character sets for which a preferred
 * MIME name is given.
 */
static const struct MimeNames PreferredMimeNames[] = {
  // clang-format off
  { "ansi_x3.4-1968",        "us-ascii"      },
  { "iso-ir-6",              "us-ascii"      },
  { "iso_646.irv:1991",      "us-ascii"      },
  { "ascii",                 "us-ascii"      },
  { "iso646-us",             "us-ascii"      },
  { "us",                    "us-ascii"      },
  { "ibm367",                "us-ascii"      },
  { "cp367",                 "us-ascii"      },
  { "csASCII",               "us-ascii"      },

  { "csISO2022KR",           "iso-2022-kr"   },
  { "csEUCKR",               "euc-kr"        },
  { "csISO2022JP",           "iso-2022-jp"   },
  { "csISO2022JP2",          "iso-2022-jp-2" },

  { "ISO_8859-1:1987",       "iso-8859-1"    },
  { "iso-ir-100",            "iso-8859-1"    },
  { "iso_8859-1",            "iso-8859-1"    },
  { "latin1",                "iso-8859-1"    },
  { "l1",                    "iso-8859-1"    },
  { "IBM819",                "iso-8859-1"    },
  { "CP819",                 "iso-8859-1"    },
  { "csISOLatin1",           "iso-8859-1"    },

  { "ISO_8859-2:1987",       "iso-8859-2"    },
  { "iso-ir-101",            "iso-8859-2"    },
  { "iso_8859-2",            "iso-8859-2"    },
  { "latin2",                "iso-8859-2"    },
  { "l2",                    "iso-8859-2"    },
  { "csISOLatin2",           "iso-8859-2"    },

  { "ISO_8859-3:1988",       "iso-8859-3"    },
  { "iso-ir-109",            "iso-8859-3"    },
  { "ISO_8859-3",            "iso-8859-3"    },
  { "latin3",                "iso-8859-3"    },
  { "l3",                    "iso-8859-3"    },
  { "csISOLatin3",           "iso-8859-3"    },

  { "ISO_8859-4:1988",       "iso-8859-4"    },
  { "iso-ir-110",            "iso-8859-4"    },
  { "ISO_8859-4",            "iso-8859-4"    },
  { "latin4",                "iso-8859-4"    },
  { "l4",                    "iso-8859-4"    },
  { "csISOLatin4",           "iso-8859-4"    },

  { "ISO_8859-6:1987",       "iso-8859-6"    },
  { "iso-ir-127",            "iso-8859-6"    },
  { "iso_8859-6",            "iso-8859-6"    },
  { "ECMA-114",              "iso-8859-6"    },
  { "ASMO-708",              "iso-8859-6"    },
  { "arabic",                "iso-8859-6"    },
  { "csISOLatinArabic",      "iso-8859-6"    },

  { "ISO_8859-7:1987",       "iso-8859-7"    },
  { "iso-ir-126",            "iso-8859-7"    },
  { "ISO_8859-7",            "iso-8859-7"    },
  { "ELOT_928",              "iso-8859-7"    },
  { "ECMA-118",              "iso-8859-7"    },
  { "greek",                 "iso-8859-7"    },
  { "greek8",                "iso-8859-7"    },
  { "csISOLatinGreek",       "iso-8859-7"    },

  { "ISO_8859-8:1988",       "iso-8859-8"    },
  { "iso-ir-138",            "iso-8859-8"    },
  { "ISO_8859-8",            "iso-8859-8"    },
  { "hebrew",                "iso-8859-8"    },
  { "csISOLatinHebrew",      "iso-8859-8"    },

  { "ISO_8859-5:1988",       "iso-8859-5"    },
  { "iso-ir-144",            "iso-8859-5"    },
  { "ISO_8859-5",            "iso-8859-5"    },
  { "cyrillic",              "iso-8859-5"    },
  { "csISOLatinCyrillic",    "iso-8859-5"    },

  { "ISO_8859-9:1989",       "iso-8859-9"    },
  { "iso-ir-148",            "iso-8859-9"    },
  { "ISO_8859-9",            "iso-8859-9"    },
  { "latin5",                "iso-8859-9"    },  /* this is not a bug */
  { "l5",                    "iso-8859-9"    },
  { "csISOLatin5",           "iso-8859-9"    },

  { "ISO_8859-10:1992",      "iso-8859-10"   },
  { "iso-ir-157",            "iso-8859-10"   },
  { "latin6",                "iso-8859-10"   },  /* this is not a bug */
  { "l6",                    "iso-8859-10"   },
  { "csISOLatin6",           "iso-8859-10"   },

  { "csKOI8r",               "koi8-r"        },

  { "MS_Kanji",              "Shift_JIS"     },  /* Note the underscore! */
  { "csShiftJis",            "Shift_JIS"     },

  { "Extended_UNIX_Code_Packed_Format_for_Japanese",
                             "euc-jp"        },
  { "csEUCPkdFmtJapanese",   "euc-jp"        },

  { "csGB2312",              "gb2312"        },
  { "csbig5",                "big5"          },

  /* End of official brain damage.
   * What follows has been taken from glibc's localedata files.  */

  { "iso_8859-13",           "iso-8859-13"   },
  { "iso-ir-179",            "iso-8859-13"   },
  { "latin7",                "iso-8859-13"   },  /* this is not a bug */
  { "l7",                    "iso-8859-13"   },

  { "iso_8859-14",           "iso-8859-14"   },
  { "latin8",                "iso-8859-14"   },  /* this is not a bug */
  { "l8",                    "iso-8859-14"   },

  { "iso_8859-15",           "iso-8859-15"   },
  { "latin9",                "iso-8859-15"   },  /* this is not a bug */

  /* Suggested by Ionel Mugurel Ciobica <tgakic@sg10.chem.tue.nl> */
  { "latin0",                "iso-8859-15"   },  /* this is not a bug */

  { "iso_8859-16",           "iso-8859-16"   },
  { "latin10",               "iso-8859-16"   },  /* this is not a bug */

  { "646",                   "us-ascii"      },

  /* http://www.sun.com/software/white-papers/wp-unicode/ */

  { "eucJP",                 "euc-jp"        },
  { "PCK",                   "Shift_JIS"     },
  { "ko_KR-euc",             "euc-kr"        },
  { "zh_TW-big5",            "big5"          },

  /* seems to be common on some systems */

  { "sjis",                  "Shift_JIS"     },
  { "euc-jp-ms",             "eucJP-ms"      },

  /* If you happen to encounter system-specific brain-damage with respect to
   * character set naming, please add it above this comment, and submit a patch
   * to <neomutt-devel@neomutt.org> */

  { NULL, NULL },
  // clang-format on
};

/**
 * lookup_new - Create a new Lookup
 * @retval ptr New Lookup
 */
static struct Lookup *lookup_new(void)
{
  return MUTT_MEM_CALLOC(1, struct Lookup);
}

/**
 * lookup_free - Free a Lookup
 * @param ptr Lookup to free
 */
static void lookup_free(struct Lookup **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Lookup *l = *ptr;
  FREE(&l->replacement);
  FREE(&l->regex.pattern);
  if (l->regex.regex)
    regfree(l->regex.regex);
  FREE(&l->regex.regex);
  FREE(&l->regex);

  FREE(ptr);
}

/**
 * lookup_charset - Look for a preferred character set name
 * @param type Type, e.g. #MUTT_LOOKUP_CHARSET
 * @param cs   Character set
 * @retval ptr Charset string
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
    if (mutt_regex_match(&l->regex, cs))
      return l->replacement;
  }
  return NULL;
}

/**
 * mutt_ch_convert_nonmime_string - Try to convert a string using a list of character sets
 * @param[in]     assumed_charset From $assumed_charset
 * @param[in]     charset         From $charset
 * @param[in,out] ps              String to be converted
 * @retval 0  Success
 * @retval -1 Error
 *
 * Work through `$assumed_charset` looking for a character set conversion that
 * works.  Failing that, try mutt_ch_get_default_charset().
 */
int mutt_ch_convert_nonmime_string(const struct Slist *const assumed_charset,
                                   const char *charset, char **ps)
{
  if (!ps)
    return -1;

  char *u = *ps;
  const size_t ulen = mutt_str_len(u);
  if (ulen == 0)
    return 0;

  const struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &assumed_charset->head, entries)
  {
    char const *c = np->data;
    size_t n = mutt_str_len(c);
    char *fromcode = MUTT_MEM_MALLOC(n + 1, char);
    mutt_str_copy(fromcode, c, n + 1);
    char *s = mutt_strn_dup(u, ulen);
    int m = mutt_ch_convert_string(&s, fromcode, charset, MUTT_ICONV_NO_FLAGS);
    FREE(&fromcode);
    if (m == 0)
    {
      FREE(ps);
      *ps = s;
      return 0;
    }
    FREE(&s);
  }
  mutt_ch_convert_string(ps, mutt_ch_get_default_charset(assumed_charset),
                         charset, MUTT_ICONV_HOOK_FROM);
  return -1;
}

/**
 * mutt_ch_canonical_charset - Canonicalise the charset of a string
 * @param buf Buffer for canonical character set name
 * @param buflen Length of buffer
 * @param name Name to be canonicalised
 *
 * This first ties off any charset extension such as "//TRANSLIT",
 * canonicalizes the charset and re-adds the extension
 */
void mutt_ch_canonical_charset(char *buf, size_t buflen, const char *name)
{
  if (!buf || !name)
    return;

  char in[1024] = { 0 };
  char scratch[1024 + 10] = { 0 };
  struct Buffer *canon = buf_pool_get();

  mutt_str_copy(in, name, sizeof(in));
  char *ext = strchr(in, '/');
  if (ext)
    *ext++ = '\0';

  if (mutt_istr_equal(in, "utf-8") || mutt_istr_equal(in, "utf8"))
  {
    buf_strcpy(canon, "utf-8");
    goto out;
  }

  /* catch some common iso-8859-something misspellings */
  size_t plen;
  if ((plen = mutt_istr_startswith(in, "8859")) && (in[plen] != '-'))
    snprintf(scratch, sizeof(scratch), "iso-8859-%s", in + plen);
  else if ((plen = mutt_istr_startswith(in, "8859-")))
    snprintf(scratch, sizeof(scratch), "iso-8859-%s", in + plen);
  else if ((plen = mutt_istr_startswith(in, "iso8859")) && (in[plen] != '-'))
    snprintf(scratch, sizeof(scratch), "iso_8859-%s", in + plen);
  else if ((plen = mutt_istr_startswith(in, "iso8859-")))
    snprintf(scratch, sizeof(scratch), "iso_8859-%s", in + plen);
  else
    mutt_str_copy(scratch, in, sizeof(scratch));

  for (size_t i = 0; PreferredMimeNames[i].key; i++)
  {
    if (mutt_istr_equal(scratch, PreferredMimeNames[i].key))
    {
      buf_strcpy(canon, PreferredMimeNames[i].pref);
      goto out;
    }
  }

  buf_strcpy(canon, scratch);
  buf_lower(canon); // for cosmetics' sake

out:
  if (ext && (*ext != '\0'))
  {
    buf_addch(canon, '/');
    buf_addstr(canon, ext);
  }

  mutt_str_copy(buf, buf_string(canon), buflen);
  buf_pool_release(&canon);
}

/**
 * mutt_ch_chscmp - Are the names of two character sets equivalent?
 * @param cs1 First character set
 * @param cs2 Second character set
 * @retval true  Names are equivalent
 * @retval false Names differ
 *
 * Charsets may have extensions that mutt_ch_canonical_charset() leaves intact;
 * we expect 'cs2' to originate from neomutt code, not user input (i.e. 'cs2'
 * does _not_ have any extension) we simply check if the shorter string is a
 * prefix for the longer.
 */
bool mutt_ch_chscmp(const char *cs1, const char *cs2)
{
  if (!cs1 || !cs2)
    return false;

  char buf[256] = { 0 };

  mutt_ch_canonical_charset(buf, sizeof(buf), cs1);

  int len1 = mutt_str_len(buf);
  int len2 = mutt_str_len(cs2);

  return mutt_istrn_equal(((len1 > len2) ? buf : cs2),
                          ((len1 > len2) ? cs2 : buf), MIN(len1, len2));
}

/**
 * mutt_ch_get_default_charset - Get the default character set
 * @param assumed_charset From $assumed_charset
 * @retval ptr Name of the default character set
 *
 * @warning This returns a pointer to a static buffer.  Do not free it.
 */
const char *mutt_ch_get_default_charset(const struct Slist *const assumed_charset)
{
  static char fcharset[128];
  const char *c = NULL;

  if (assumed_charset && (assumed_charset->count > 0))
    c = STAILQ_FIRST(&assumed_charset->head)->data;
  else
    c = "us-ascii";

  mutt_str_copy(fcharset, c, sizeof(fcharset));
  return fcharset;
}

/**
 * mutt_ch_get_langinfo_charset - Get the user's choice of character set
 * @retval ptr Charset string
 *
 * Get the canonical character set used by the user's locale.
 * The caller must free the returned string.
 */
char *mutt_ch_get_langinfo_charset(void)
{
  char buf[1024] = { 0 };

  mutt_ch_canonical_charset(buf, sizeof(buf), nl_langinfo(CODESET));

  if (buf[0] != '\0')
    return mutt_str_dup(buf);

  return mutt_str_dup("iso-8859-1");
}

/**
 * mutt_ch_lookup_add - Add a new character set lookup
 * @param type    Type of character set, e.g. #MUTT_LOOKUP_CHARSET
 * @param pat     Pattern to match
 * @param replace Replacement string
 * @param err     Buffer for error message
 * @retval true  Lookup added to list
 * @retval false Regex string was invalid
 *
 * Add a regex for a character set and a replacement name.
 */
bool mutt_ch_lookup_add(enum LookupType type, const char *pat,
                        const char *replace, struct Buffer *err)
{
  if (!pat || !replace)
    return false;

  regex_t *rx = MUTT_MEM_CALLOC(1, regex_t);
  int rc = REG_COMP(rx, pat, REG_ICASE);
  if (rc != 0)
  {
    regerror(rc, rx, err->data, err->dsize);
    FREE(&rx);
    return false;
  }

  struct Lookup *l = lookup_new();
  l->type = type;
  l->replacement = mutt_str_dup(replace);
  l->regex.pattern = mutt_str_dup(pat);
  l->regex.regex = rx;
  l->regex.pat_not = false;

  TAILQ_INSERT_TAIL(&Lookups, l, entries);

  return true;
}

/**
 * mutt_ch_lookup_remove - Remove all the character set lookups
 *
 * Empty the list of replacement character set names.
 */
void mutt_ch_lookup_remove(void)
{
  struct Lookup *l = NULL;
  struct Lookup *tmp = NULL;

  TAILQ_FOREACH_SAFE(l, &Lookups, entries, tmp)
  {
    TAILQ_REMOVE(&Lookups, l, entries);
    lookup_free(&l);
  }
}

/**
 * mutt_ch_charset_lookup - Look for a replacement character set
 * @param chs Character set to lookup
 * @retval ptr  Replacement character set (if a 'charset-hook' matches)
 * @retval NULL No matching hook
 *
 * Look through all the 'charset-hook's.
 * If one matches return the replacement character set.
 */
const char *mutt_ch_charset_lookup(const char *chs)
{
  return lookup_charset(MUTT_LOOKUP_CHARSET, chs);
}

/**
 * mutt_ch_iconv_open - Set up iconv for conversions
 * @param tocode   Current character set
 * @param fromcode Target character set
 * @param flags    Flags, e.g. #MUTT_ICONV_HOOK_FROM
 * @retval ptr iconv handle for the conversion
 *
 * Like iconv_open, but canonicalises the charsets, applies charset-hooks,
 * recanonicalises, and finally applies iconv-hooks. Parameter flags=0 skips
 * charset-hooks, while MUTT_ICONV_HOOK_FROM applies them to fromcode. Callers
 * should use flags=0 when fromcode can safely be considered true, either some
 * constant, or some value provided by the user; MUTT_ICONV_HOOK_FROM should be
 * used only when fromcode is unsure, taken from a possibly wrong incoming MIME
 * label, or such. Misusing MUTT_ICONV_HOOK_FROM leads to unwanted interactions
 * in some setups.
 *
 * Since calling iconv_open() repeatedly can be expensive, we keep a cache of
 * the most recently used iconv_t objects, kept in LRU order. This means that
 * you should not call iconv_close() on the object yourself. All remaining
 * objects in the cache will exit when main() calls mutt_ch_cache_cleanup().
 *
 * @note By design charset-hooks should never be, and are never, applied
 * to tocode.
 *
 * @note The top-well-named MUTT_ICONV_HOOK_FROM acts on charset-hooks,
 * not at all on iconv-hooks.
 */
iconv_t mutt_ch_iconv_open(const char *tocode, const char *fromcode, uint8_t flags)
{
  char tocode1[128] = { 0 };
  char fromcode1[128] = { 0 };
  const char *tocode2 = NULL, *fromcode2 = NULL;
  const char *tmp = NULL;

  /* transform to MIME preferred charset names */
  mutt_ch_canonical_charset(tocode1, sizeof(tocode1), tocode);
  mutt_ch_canonical_charset(fromcode1, sizeof(fromcode1), fromcode);

  /* maybe apply charset-hooks and recanonicalise fromcode,
   * but only when caller asked us to sanitize a potentially wrong
   * charset name incoming from the wild exterior. */
  if (flags & MUTT_ICONV_HOOK_FROM)
  {
    tmp = mutt_ch_charset_lookup(fromcode1);
    if (tmp)
      mutt_ch_canonical_charset(fromcode1, sizeof(fromcode1), tmp);
  }

  /* check if we have this pair cached already */
  for (int i = 0; i < IconvCacheUsed; ++i)
  {
    if (strcmp(tocode1, IconvCache[i].tocode1) == 0 &&
        strcmp(fromcode1, IconvCache[i].fromcode1) == 0)
    {
      iconv_t cd = IconvCache[i].cd;

      /* make room for this one at the top */
      struct IconvCacheEntry top = IconvCache[i];
      for (int j = i; j-- > 0;)
      {
        IconvCache[j + 1] = IconvCache[j];
      }
      IconvCache[0] = top;

      if (iconv_t_valid(cd))
      {
        /* reset state */
        iconv(cd, NULL, NULL, NULL, NULL);
      }
      return cd;
    }
  }

  /* not found in cache */
  /* always apply iconv-hooks to suit system's iconv tastes */
  tocode2 = mutt_ch_iconv_lookup(tocode1);
  tocode2 = tocode2 ? tocode2 : tocode1;
  fromcode2 = mutt_ch_iconv_lookup(fromcode1);
  fromcode2 = fromcode2 ? fromcode2 : fromcode1;

  /* call system iconv with names it appreciates */
  iconv_t cd = iconv_open(tocode2, fromcode2);

  if (IconvCacheUsed == ICONV_CACHE_SIZE)
  {
    mutt_debug(LL_DEBUG2, "iconv: dropping %s -> %s from the cache\n",
               IconvCache[IconvCacheUsed - 1].fromcode1,
               IconvCache[IconvCacheUsed - 1].tocode1);
    /* get rid of the oldest entry */
    FREE(&IconvCache[IconvCacheUsed - 1].fromcode1);
    FREE(&IconvCache[IconvCacheUsed - 1].tocode1);
    if (iconv_t_valid(IconvCache[IconvCacheUsed - 1].cd))
    {
      iconv_close(IconvCache[IconvCacheUsed - 1].cd);
    }
    IconvCacheUsed--;
  }

  /* make room for this one at the top */
  for (int j = IconvCacheUsed; j-- > 0;)
  {
    IconvCache[j + 1] = IconvCache[j];
  }

  IconvCacheUsed++;

  mutt_debug(LL_DEBUG2, "iconv: adding %s -> %s to the cache\n", fromcode1, tocode1);
  IconvCache[0].fromcode1 = strdup(fromcode1);
  IconvCache[0].tocode1 = strdup(tocode1);
  IconvCache[0].cd = cd;

  return cd;
}

/**
 * mutt_ch_iconv - Change the encoding of a string
 * @param[in]     cd           Iconv conversion descriptor
 * @param[in,out] inbuf        Buffer to convert
 * @param[in,out] inbytesleft  Length of buffer to convert
 * @param[in,out] outbuf       Buffer for the result
 * @param[in,out] outbytesleft Length of result buffer
 * @param[in]     inrepls      Input replacement characters
 * @param[in]     outrepl      Output replacement characters
 * @param[out]    iconverrno   Errno if iconv() fails, 0 if it succeeds
 * @retval num Characters converted
 *
 * Like iconv, but keeps going even when the input is invalid
 * If you're supplying inrepls, the source charset should be stateless;
 * if you're supplying an outrepl, the target charset should be.
 */
size_t mutt_ch_iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft,
                     char **outbuf, size_t *outbytesleft, const char **inrepls,
                     const char *outrepl, int *iconverrno)
{
  size_t rc = 0;
  const char *ib = *inbuf;
  size_t ibl = *inbytesleft;
  char *ob = *outbuf;
  size_t obl = *outbytesleft;

  while (true)
  {
    errno = 0;
    const size_t ret1 = iconv(cd, (ICONV_CONST char **) &ib, &ibl, &ob, &obl);
    if (ret1 != ICONV_ILLEGAL_SEQ)
      rc += ret1;
    if (iconverrno)
      *iconverrno = errno;

    if (ibl && obl && (errno == EILSEQ))
    {
      if (inrepls)
      {
        /* Try replacing the input */
        const char **t = NULL;
        for (t = inrepls; *t; t++)
        {
          const char *ib1 = *t;
          size_t ibl1 = strlen(*t);
          char *ob1 = ob;
          size_t obl1 = obl;
          iconv(cd, (ICONV_CONST char **) &ib1, &ibl1, &ob1, &obl1);
          if (ibl1 == 0)
          {
            ib++;
            ibl--;
            ob = ob1;
            obl = obl1;
            rc++;
            break;
          }
        }
        if (*t)
          continue;
      }
      /* Replace the output */
      if (!outrepl)
        outrepl = "?";
      iconv(cd, NULL, NULL, &ob, &obl);
      if (obl)
      {
        int n = strlen(outrepl);
        if (n > obl)
        {
          outrepl = "?";
          n = 1;
        }
        memcpy(ob, outrepl, n);
        ib++;
        ibl--;
        ob += n;
        obl -= n;
        rc++;
        iconv(cd, NULL, NULL, NULL, NULL); /* for good measure */
        continue;
      }
    }
    *inbuf = ib;
    *inbytesleft = ibl;
    *outbuf = ob;
    *outbytesleft = obl;
    return rc;
  }
}

/**
 * mutt_ch_iconv_lookup - Look for a replacement character set
 * @param chs Character set to lookup
 * @retval ptr  Replacement character set (if a 'iconv-hook' matches)
 * @retval NULL No matching hook
 *
 * Look through all the 'iconv-hook's.
 * If one matches return the replacement character set.
 */
const char *mutt_ch_iconv_lookup(const char *chs)
{
  return lookup_charset(MUTT_LOOKUP_ICONV, chs);
}

/**
 * mutt_ch_check - Check whether a string can be converted between encodings
 * @param[in] s     String to check
 * @param[in] slen  Length of the string to check
 * @param[in] from  Current character set
 * @param[in] to    Target character set
 * @retval 0  Success
 * @retval -1 Error in iconv_open()
 * @retval >0 Errno as set by iconv()
 */
int mutt_ch_check(const char *s, size_t slen, const char *from, const char *to)
{
  if (!s || !from || !to)
    return -1;

  int rc = 0;
  iconv_t cd = mutt_ch_iconv_open(to, from, MUTT_ICONV_NO_FLAGS);
  if (!iconv_t_valid(cd))
    return -1;

  size_t outlen = MB_LEN_MAX * slen;
  char *out = MUTT_MEM_MALLOC(outlen + 1, char);
  char *saved_out = out;

  const size_t convlen = iconv(cd, (ICONV_CONST char **) &s, &slen, &out, &outlen);
  if (convlen == ICONV_ILLEGAL_SEQ)
    rc = errno;

  FREE(&saved_out);
  return rc;
}

/**
 * mutt_ch_convert_string - Convert a string between encodings
 * @param[in,out] ps    String to convert
 * @param[in]     from  Current character set
 * @param[in]     to    Target character set
 * @param[in]     flags Flags, e.g. #MUTT_ICONV_HOOK_FROM
 * @retval 0      Success
 * @retval -1     Invalid arguments or failure to open an iconv channel
 * @retval errno  Failure in iconv conversion
 *
 * Parameter flags is given as-is to mutt_ch_iconv_open().
 * See there for its meaning and usage policy.
 */
int mutt_ch_convert_string(char **ps, const char *from, const char *to, uint8_t flags)
{
  if (!ps)
    return -1;

  char *s = *ps;

  if (!s || (*s == '\0'))
    return 0;

  if (!to || !from)
    return -1;

  const char *repls[] = { "\357\277\275", "?", 0 };
  int rc = 0;

  iconv_t cd = mutt_ch_iconv_open(to, from, flags);
  if (!iconv_t_valid(cd))
    return -1;

  const char **inrepls = NULL;
  const char *outrepl = NULL;

  if (mutt_ch_is_utf8(to))
    outrepl = "\357\277\275";
  else if (mutt_ch_is_utf8(from))
    inrepls = repls;
  else
    outrepl = "?";

  const char *ib = s;
  size_t ibl = strlen(s);
  if (ibl >= (SIZE_MAX / MB_LEN_MAX))
  {
    return -1;
  }
  size_t obl = MB_LEN_MAX * ibl;
  char *buf = MUTT_MEM_MALLOC(obl + 1, char);
  char *ob = buf;

  mutt_ch_iconv(cd, &ib, &ibl, &ob, &obl, inrepls, outrepl, &rc);
  iconv(cd, 0, 0, &ob, &obl);

  *ob = '\0';

  FREE(ps);
  *ps = buf;

  mutt_str_adjust(ps);
  return rc;
}

/**
 * mutt_ch_check_charset - Does iconv understand a character set?
 * @param cs     Character set to check
 * @param strict Check strictly by using iconv
 * @retval true Character set is valid
 *
 * If `strict` is false, then finding a matching character set in
 * #PreferredMimeNames will be enough.
 * If `strict` is true, or the charset is not in #PreferredMimeNames, then
 * iconv() with be run.
 */
bool mutt_ch_check_charset(const char *cs, bool strict)
{
  if (!cs)
    return false;

  if (mutt_ch_is_utf8(cs))
    return true;

  if (!strict)
  {
    for (int i = 0; PreferredMimeNames[i].key; i++)
    {
      if (mutt_istr_equal(PreferredMimeNames[i].key, cs) ||
          mutt_istr_equal(PreferredMimeNames[i].pref, cs))
      {
        return true;
      }
    }
  }

  iconv_t cd = mutt_ch_iconv_open(cs, cs, MUTT_ICONV_NO_FLAGS);
  if (iconv_t_valid(cd))
  {
    return true;
  }

  return false;
}

/**
 * mutt_ch_fgetconv_open - Prepare a file for charset conversion
 * @param fp    FILE ptr to prepare
 * @param from  Current character set
 * @param to    Destination character set
 * @param flags Flags, e.g. #MUTT_ICONV_HOOK_FROM
 * @retval ptr fgetconv handle
 *
 * Parameter flags is given as-is to mutt_ch_iconv_open().
 */
struct FgetConv *mutt_ch_fgetconv_open(FILE *fp, const char *from, const char *to, uint8_t flags)
{
  iconv_t cd = ICONV_T_INVALID;

  if (from && to)
    cd = mutt_ch_iconv_open(to, from, flags);

  struct FgetConv *fc = MUTT_MEM_CALLOC(1, struct FgetConv);
  fc->fp = fp;
  fc->cd = cd;

  if (iconv_t_valid(cd))
  {
    static const char *repls[] = { "\357\277\275", "?", 0 };

    fc->p = fc->bufo;
    fc->ob = fc->bufo;
    fc->ib = fc->bufi;
    fc->ibl = 0;
    fc->inrepls = mutt_ch_is_utf8(to) ? repls : repls + 1;
  }

  return fc;
}

/**
 * mutt_ch_fgetconv_close - Close an fgetconv handle
 * @param[out] ptr fgetconv handle
 */
void mutt_ch_fgetconv_close(struct FgetConv **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * mutt_ch_fgetconv - Convert a file's character set
 * @param fc FgetConv handle
 * @retval num Next character in the converted file
 * @retval EOF Error
 *
 * A file is read into a buffer and its character set is converted.
 * Each call to this function will return one converted character.
 * The buffer is refilled automatically when empty.
 */
int mutt_ch_fgetconv(struct FgetConv *fc)
{
  if (!fc)
    return EOF;
  if (!iconv_t_valid(fc->cd))
    return fgetc(fc->fp);
  if (!fc->p)
    return EOF;
  if (fc->p < fc->ob)
    return (unsigned char) *(fc->p)++;

  /* Try to convert some more */
  fc->p = fc->bufo;
  fc->ob = fc->bufo;
  if (fc->ibl)
  {
    size_t obl = sizeof(fc->bufo);
    iconv(fc->cd, (ICONV_CONST char **) &fc->ib, &fc->ibl, &fc->ob, &obl);
    if (fc->p < fc->ob)
      return (unsigned char) *(fc->p)++;
  }

  /* If we trusted iconv a bit more, we would at this point
   * ask why it had stopped converting ... */

  /* Try to read some more */
  if ((fc->ibl == sizeof(fc->bufi)) ||
      (fc->ibl && (fc->ib + fc->ibl < fc->bufi + sizeof(fc->bufi))))
  {
    fc->p = 0;
    return EOF;
  }
  if (fc->ibl)
    memcpy(fc->bufi, fc->ib, fc->ibl);
  fc->ib = fc->bufi;
  fc->ibl += fread(fc->ib + fc->ibl, 1, sizeof(fc->bufi) - fc->ibl, fc->fp);

  /* Try harder this time to convert some */
  if (fc->ibl)
  {
    size_t obl = sizeof(fc->bufo);
    mutt_ch_iconv(fc->cd, (const char **) &fc->ib, &fc->ibl, &fc->ob, &obl,
                  fc->inrepls, 0, NULL);
    if (fc->p < fc->ob)
      return (unsigned char) *(fc->p)++;
  }

  /* Either the file has finished or one of the buffers is too small */
  fc->p = 0;
  return EOF;
}

/**
 * mutt_ch_fgetconvs - Convert a file's charset into a string buffer
 * @param buf    Buffer for result
 * @param buflen Length of buffer
 * @param fc     FgetConv handle
 * @retval ptr  Success, result buffer
 * @retval NULL Error
 *
 * Read a file into a buffer, converting the character set as it goes.
 */
char *mutt_ch_fgetconvs(char *buf, size_t buflen, struct FgetConv *fc)
{
  if (!buf)
    return NULL;

  size_t r;
  for (r = 0; (r + 1) < buflen;)
  {
    const int c = mutt_ch_fgetconv(fc);
    if (c == EOF)
      break;
    buf[r++] = (char) c;
    if (c == '\n')
      break;
  }
  buf[r] = '\0';

  if (r > 0)
    return buf;

  return NULL;
}

/**
 * mutt_ch_set_charset - Update the records for a new character set
 * @param charset New character set
 *
 * Check if this character set is utf-8 and pick a suitable replacement
 * character for unprintable characters.
 *
 * @note This calls `bind_textdomain_codeset()` which will affect future
 * message translations.
 */
void mutt_ch_set_charset(const char *charset)
{
  char buf[256] = { 0 };

  mutt_ch_canonical_charset(buf, sizeof(buf), charset);

  if (mutt_ch_is_utf8(buf))
  {
    CharsetIsUtf8 = true;
    ReplacementChar = 0xfffd; /* replacement character */
  }
  else
  {
    CharsetIsUtf8 = false;
    ReplacementChar = '?';
  }

#if defined(HAVE_BIND_TEXTDOMAIN_CODESET) && defined(ENABLE_NLS)
  bind_textdomain_codeset(PACKAGE, buf);
#endif
}

/**
 * mutt_ch_choose - Figure the best charset to encode a string
 * @param[in] fromcode Original charset of the string
 * @param[in] charsets List of potential charsets to use
 * @param[in] u        String to encode
 * @param[in] ulen     Length of the string to encode
 * @param[out] d       If not NULL, point it to the converted string
 * @param[out] dlen    If not NULL, point it to the length of the d string
 * @retval ptr  Best performing charset
 * @retval NULL None could be found
 */
char *mutt_ch_choose(const char *fromcode, const struct Slist *charsets,
                     const char *u, size_t ulen, char **d, size_t *dlen)
{
  if (!fromcode || !charsets)
    return NULL;

  char *e = NULL, *tocode = NULL;
  size_t elen = 0, bestn = 0;

  const struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &charsets->head, entries)
  {
    char *t = mutt_str_dup(np->data);
    if (!t)
      continue;

    size_t n = mutt_str_len(t);
    char *s = mutt_strn_dup(u, ulen);
    const int rc = d ? mutt_ch_convert_string(&s, fromcode, t, MUTT_ICONV_NO_FLAGS) :
                       mutt_ch_check(s, ulen, fromcode, t);
    if (rc)
    {
      FREE(&t);
      FREE(&s);
      continue;
    }
    size_t slen = mutt_str_len(s);

    if (!tocode || (n < bestn))
    {
      bestn = n;
      FREE(&tocode);
      tocode = t;
      if (d)
      {
        FREE(&e);
        e = s;
      }
      else
      {
        FREE(&s);
      }
      elen = slen;
    }
    else
    {
      FREE(&t);
      FREE(&s);
    }
  }
  if (tocode)
  {
    if (d)
      *d = e;
    if (dlen)
      *dlen = elen;

    char canonical_buf[1024] = { 0 };
    mutt_ch_canonical_charset(canonical_buf, sizeof(canonical_buf), tocode);
    mutt_str_replace(&tocode, canonical_buf);
  }
  return tocode;
}

/**
 * mutt_ch_cache_cleanup - Clean up the cached iconv handles and charset strings
 */
void mutt_ch_cache_cleanup(void)
{
  for (int i = 0; i < IconvCacheUsed; ++i)
  {
    FREE(&IconvCache[i].fromcode1);
    FREE(&IconvCache[i].tocode1);
    if (iconv_t_valid(IconvCache[i].cd))
    {
      iconv_close(IconvCache[i].cd);
    }
  }
  IconvCacheUsed = 0;
}
