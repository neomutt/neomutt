/**
 * @file
 * Manage precompiled / predefined regular expressions
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
 * @page prex Manage precompiled / predefined regular expressions
 *
 * Manage precompiled / predefined regular expressions.
 */

#include <assert.h>
#include "prex.h"
#include "memory.h"

/**
 * struct PrexStorage - A predefined / precompiled regex
 *
 * This struct holds a predefined / precompiled regex including data
 * representing the corresponding identification enum, the regular expression in
 * string format, information on how many matches it defines, and storage for
 * the actual matches.  The enum entry is not strictly necessary, but is kept
 * together with the regex for validation purposes, as it allows checking that
 * the enum and the array defined in prex() do not go out of sync.
 */
struct PrexStorage
{
  enum Prex which;     ///< Regex type, e.g. #PREX_URL
  size_t nmatches;     ///< Number of regex matches
  const char *str;     ///< Regex string
  regex_t *re;         ///< Compiled regex
  regmatch_t *matches; ///< Resulting matches
};

/**
 * prex - Compile on demand and get data for a predefined regex
 * @param which Which regex to get
 * @retval ptr Pointer to a PrexStorage struct
 *
 * @note Returned pointer is guaranteed not to be NULL.
 *       The function asserts on error.
 */
static struct PrexStorage *prex(enum Prex which)
{
  static struct PrexStorage storage[] = {
    /* clang-format off */
    {
      PREX_URL,
      PREX_URL_MATCH_MAX,
      /* Spec: https://tools.ietf.org/html/rfc3986#section-3 */
#define UNR_PCTENC_SUBDEL "-a-zA-Z0-9._~%!$&'()*+,;="
#define PATH ":@/"
      "^([a-zA-Z][-+.a-zA-Z0-9]+):"                // . scheme
      "("                                          // . rest
        "("                                        // . . authority + path
                                                   // . . or path only
          "(//"                                    // . . . authority + path
            "("                                    // . . . . user info
              "([" UNR_PCTENC_SUBDEL "@]*)"        // . . . . . user name + '@'
              "(:([" UNR_PCTENC_SUBDEL "]*))?"     // . . . . . password
            "@)?"
            "("                                    // . . . . host
              "([" UNR_PCTENC_SUBDEL "]*)"         // . . . . . host name
              "|"
              "(\\[[a-zA-Z0-9:.]+\\])"             // . . . . . IPv4 or IPv6
            ")"
            "(:([0-9]+))?"                         // . . . . port
            "(/([" UNR_PCTENC_SUBDEL PATH "]*))?"  // . . . . path
          ")"
          "|"
          "("                                      // . . . path only
            "[" UNR_PCTENC_SUBDEL PATH "]*"        // . . . . path
          ")"
        ")"
        "(\\?([" UNR_PCTENC_SUBDEL PATH " ?]*))?"  // . . query + ' ' and '?'
      ")$"
#undef PATH
#undef UNR_PCTENC_SUBDEL
    },
    {
      PREX_URL_QUERY_KEY_VAL,
      PREX_URL_QUERY_KEY_VAL_MATCH_MAX,
#define QUERY_PART "-a-zA-Z0-9._~%!$'()*+,;:@/"
      "([" QUERY_PART "]+)=([" QUERY_PART " ]+)"   // query + ' '
#undef QUERY_PART
    },
    {
      PREX_RFC2047_ENCODED_WORD,
      PREX_RFC2047_ENCODED_WORD_MATCH_MAX,
      "=\\?"
      "([^][()<>@,;:\\\"/?. =]+)" // charset
      "\\?"
      "([qQbB])" // encoding
      "\\?"
      "([^?]+)" // encoded text - we accept whitespace, see #1189
      "\\?="
    },
    {
      PREX_GNUTLS_CERT_HOST_HASH,
      PREX_GNUTLS_CERT_HOST_HASH_MATCH_MAX,
      "^\\#H ([a-zA-Z0-9_\\.-]+) ([a-zA-Z0-9]{4}( [a-zA-Z0-9]{4}){7})[ \t]*$"
    },
    {
      PREX_RFC6854_DATE,
      PREX_RFC6854_DATE_MATCH_MAX,
      /* Spec: https://tools.ietf.org/html/rfc5322#section-3.3 */
#define C "" //"\\([^()\\]*\\)?" // Comment are obsolete but still allowed
      "^"
        "("
        C " *"
          "(Mon|Tue|Wed|Thu|Fri|Sat|Sun)"  // Day of week
        C ", "
        ")?"
        C " *([0-9]{1,2}) " C        // Day
        "(Jan|Feb|Mar|Apr|May|Jun|"  // Month
        "Jul|Aug|Sep|Oct|Nov|Dec)"
        C " ([0-9]{2,4}) " C         // Year
        C "([0-9]{2})" C             // Hour
        ":" C "([0-9]{2})" C         // Minute
        "(:" C "([0-9]{2})" C ")?"   // Second
        " *"
        "\\(?"
        "("
        "([+-][0-9]{4})|"            // TZ
        "([A-Z ]+)"                  // Obsolete TZ
        ")"
        "\\)?"
        C
      "$"
#undef C
    }
    /* clang-format on */
  };

  assert((which >= 0) && (which < PREX_MAX) && "Invalid 'which' argument");
  struct PrexStorage *h = &storage[which];
  assert((which == h->which) && "Fix 'storage' array");
  if (!h->re)
  {
    h->re = mutt_mem_calloc(1, sizeof(*h->re));
    if (regcomp(h->re, h->str, REG_EXTENDED))
    {
      assert("Fix your RE");
    }
    h->matches = mutt_mem_calloc(h->nmatches, sizeof(*h->matches));
  }
  return h;
}

/**
 * mutt_prex_capture - match a precompiled regex against a string
 * @param which Which regex to return
 * @param str   String to apply regex on
 * @retval ptr  Pointer to an array of matched captures
 * @retval NULL Regex didn't match
 */
regmatch_t *mutt_prex_capture(enum Prex which, const char *str)
{
  if (!str)
    return NULL;

  struct PrexStorage *h = prex(which);
  if (regexec(h->re, str, h->nmatches, h->matches, 0))
    return NULL;

  assert((h->re->re_nsub == (h->nmatches - 1)) &&
         "Regular expression and matches enum are out of sync");
  return h->matches;
}

/**
 * mutt_prex_free - Cleanup heap memory allocated by compiled regexes
 */
void mutt_prex_free(void)
{
  for (enum Prex which = 0; which < PREX_MAX; which++)
  {
    struct PrexStorage *h = prex(which);
    regfree(h->re);
    FREE(&h->re);
    FREE(&h->matches);
  }
}
