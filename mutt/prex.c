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

#define PREX_MONTH "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)"
#define PREX_DOW "(Mon|Tue|Wed|Thu|Fri|Sat|Sun)"
#define PREX_TIME "([[:digit:]]{2}:[[:digit:]]{2}:[[:digit:]]{2})"
#define PREX_YEAR "([[:digit:]]{4})"

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
#define UNR_PCTENC_SUBDEL "][[:alnum:]._~%!$&'()*+,;="
#define PATH ":@/ "
      "^([[:alpha:]][-+.[:alnum:]]+):"             // . scheme
      "("                                          // . rest
        "("                                        // . . authority + path
                                                   // . . or path only
          "(//"                                    // . . . authority + path
            "("                                    // . . . . user info
              "([" UNR_PCTENC_SUBDEL "@-]*)"       // . . . . . user name + '@'
              "(:([" UNR_PCTENC_SUBDEL "-]*))?"    // . . . . . password
            "@)?"
            "("                                    // . . . . host
              "([" UNR_PCTENC_SUBDEL "-]*)"        // . . . . . host name
              "|"
              "(\\[[[:alnum:]:.]+\\])"             // . . . . . IPv4 or IPv6
            ")"
            "(:([[:digit:]]+))?"                   // . . . . port
            "(/([" UNR_PCTENC_SUBDEL PATH "-]*))?" // . . . . path
          ")"
          "|"
          "("                                      // . . . path only
            "[" UNR_PCTENC_SUBDEL PATH "-]*"       // . . . . path
          ")"
        ")"
        "(\\?([" UNR_PCTENC_SUBDEL PATH "?-]*))?"  // . . query + ' ' and '?'
      ")$"
#undef PATH
#undef UNR_PCTENC_SUBDEL
    },
    {
      PREX_URL_QUERY_KEY_VAL,
      PREX_URL_QUERY_KEY_VAL_MATCH_MAX,
#define QUERY_PART "-[:alnum:]._~%!$'()*+,;:@/"
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
      "^\\#H ([[:alnum:]_\\.-]+) ([[:alnum:]]{4}( [[:alnum:]]{4}){7})[ \t]*$"
    },
    {
      PREX_RFC5322_DATE,
      PREX_RFC5322_DATE_MATCH_MAX,
      /* Spec: https://tools.ietf.org/html/rfc5322#section-3.3 */
      "^"
        "(" PREX_DOW ", )?"       // Day of week
        " *"
        "([[:digit:]]{1,2}) "     // Day
        PREX_MONTH                // Month
        " ([[:digit:]]{2,4}) "    // Year
        "([[:digit:]]{2})"        // Hour
        ":([[:digit:]]{2})"       // Minute
        "(:([[:digit:]]{2}))?"    // Second
        " *"
        "("
        "([+-][[:digit:]]{4})|"   // TZ
        "([[:alpha:]]+)"          // Obsolete TZ
        ")"
    },
    {
      PREX_RFC5322_DATE_CFWS,
      PREX_RFC5322_DATE_CFWS_MATCH_MAX,
      /* Spec: https://tools.ietf.org/html/rfc5322#section-3.3 */
#define FWS " *"
#define C "(\\(.*\\))?"
#define CFWS FWS C FWS
      "^"
        CFWS
        "(" PREX_DOW CFWS ", )?"         // Day of week
        CFWS "([[:digit:]]{1,2}) "       // Day
        CFWS PREX_MONTH                  // Month
        CFWS "([[:digit:]]{2,4}) "       // Year
        CFWS "([[:digit:]]{2})"          // Hour
        ":" CFWS "([[:digit:]]{2})" CFWS // Minute
        "(:" CFWS "([[:digit:]]{2}))?"   // Second
        CFWS
        "("
        "([+-][[:digit:]]{4})|"          // TZ
        "([[:alpha:]]+)"                 // Obsolete TZ
        ")"
#undef CFWS
#undef C
#undef FWS
    },
    {
      PREX_IMAP_DATE,
      PREX_IMAP_DATE_MATCH_MAX,
      "( ([[:digit:]])|([[:digit:]]{2}))" // Day
      "-" PREX_MONTH                      // Month
      "-" PREX_YEAR                       // Year
      " " PREX_TIME                       // Time
      " ([+-][[:digit:]]{4})"             // TZ
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
