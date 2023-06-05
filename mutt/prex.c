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
 * @page mutt_prex Manage precompiled / predefined regular expressions
 *
 * Manage precompiled / predefined regular expressions.
 */

#include "config.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "prex.h"
#include "logging2.h"
#include "memory.h"

#ifdef HAVE_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <string.h>

/**
 * pcre2_has_unicode - Does pcre2 support Unicode?
 * @retval true pcre2 supports Unicode
 */
static bool pcre2_has_unicode(void)
{
  static uint32_t checked = -1;
  if (checked == -1)
  {
    pcre2_config(PCRE2_CONFIG_UNICODE, &checked);
  }
  return checked;
}
#endif

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
  enum Prex which; ///< Regex type, e.g. #PREX_URL
  size_t nmatches; ///< Number of regex matches
  const char *str; ///< Regex string
#ifdef HAVE_PCRE2
  pcre2_code *re;
  pcre2_match_data *mdata;
#else
  regex_t *re; ///< Compiled regex
#endif
  regmatch_t *matches; ///< Resulting matches
};

#define PREX_MONTH "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)"
#define PREX_DOW "(Mon|Tue|Wed|Thu|Fri|Sat|Sun)"
#define PREX_DOW_NOCASE                                                        \
  "([Mm][Oo][Nn]|[Tt][Uu][Ee]|[Ww][Ee][Dd]|[Tt][Hh][Uu]|[Ff][Rr][Ii]|[Ss][Aa][Tt]|[Ss][Uu][Nn])"
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
    // clang-format off
    {
      PREX_URL,
      PREX_URL_MATCH_MAX,
      /* Spec: https://tools.ietf.org/html/rfc3986#section-3 */
#ifdef HAVE_PCRE2
#define UNR_PCTENC_SUBDEL "][\\p{L}\\p{N}._~%!$&'()*+,;="
#else
#define UNR_PCTENC_SUBDEL "][[:alnum:]._~%!$&'()*+,;="
#endif
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
              "(\\[[[:xdigit:]:.]+\\])"            // . . . . . IPv4 or IPv6
            ")"
            "(:([[:digit:]]+))?"                   // . . . . port
            "(/([" UNR_PCTENC_SUBDEL PATH "-]*))?" // . . . . path
          ")"
          "|"
          "("                                      // . . . path only
            "[" UNR_PCTENC_SUBDEL PATH "-]*"       // . . . . path
          ")"
        ")"
        // Should be: "(\\?([" UNR_PCTENC_SUBDEL PATH "?-]*))?"
        "(\\?([^#]*))?"                            // . . query
      ")$"
#undef PATH
#undef UNR_PCTENC_SUBDEL
    },
    {
      PREX_URL_QUERY_KEY_VAL,
      PREX_URL_QUERY_KEY_VAL_MATCH_MAX,
#define QUERY_PART "^&=" // Should be: "-[:alnum:]._~%!$'()*+,;:@/"
      "([" QUERY_PART "]+)=([" QUERY_PART "]+)"   // query + ' '
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
      PREX_RFC5322_DATE_LAX,
      PREX_RFC5322_DATE_LAX_MATCH_MAX,
      /* Spec: https://tools.ietf.org/html/rfc5322#section-3.3 */
#define FWS " *"
#define C "(\\(.*\\))?"
#define CFWS FWS C FWS
      "^"
        CFWS
        "(([[:alpha:]]+)" CFWS ", *)?"   // Day of week (or whatever)
        CFWS "([[:digit:]]{1,2}) "       // Day
        CFWS PREX_MONTH                  // Month
        CFWS "([[:digit:]]{2,4}) "       // Year
        CFWS "([[:digit:]]{1,2})"        // Hour
        ":" CFWS "([[:digit:]]{1,2})"    // Minute
        CFWS
        "(:" CFWS "([[:digit:]]{1,2}))?" // Second
        CFWS
        "("
        "([+-][[:digit:]]{4})|"          // TZ
        "([[:alpha:]]+)"                 // Obsolete TZ
        ")?"
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
    },
    {
      PREX_MBOX_FROM,
      PREX_MBOX_FROM_MATCH_MAX,
      /* Spec: http://qmail.omnis.ch/man/man5/mbox.html */
      "^From "                 // From
      "([^[:space:]]+) +"      // Sender
      PREX_DOW                 // Day of week
      " +"
      PREX_MONTH               // Month
      " ( ([[:digit:]])|([[:digit:]]{2}))" // Day
      " +"
      PREX_TIME                // Time
      " +"
      PREX_YEAR                // Year
    },
    {
      PREX_MBOX_FROM_LAX,
      PREX_MBOX_FROM_LAX_MATCH_MAX,
      /* Spec: http://qmail.omnis.ch/man/man5/mbox.html */
      "^From "                  // From
      "("
        "[^[:space:]]+"         // Sender
        "( at [^[:space:]]+)?" // Possibly obfuscated, pipermail-style
      ")?"
      " *"
      PREX_DOW_NOCASE           // Day of week
      " +"
      PREX_MONTH                // Month
      " +"
      "( "                      // Day
        "([[:digit:]])|"
        "([[:digit:]]{2})"
      ")"
      " +"
      "("
        PREX_TIME               // Time (HH:MM:SS)
        "|"
        "([[:digit:]]{2}"       // Time (HH:MM)
        ":[[:digit:]]{2})"
      ")"
      " +"
      "("
        "([[:alpha:] ]+)|"      // Timezone name (which we skip)
        "([+][[:digit:]]{4} )"  // Timezone offset (which we skip)
      ")?"
      "("
        PREX_YEAR               // Year (YYYY)
        "|"
        "([[:digit:]]{2})"      // Year (YY)
      ")"
    },
    {
      PREX_ACCOUNT_CMD,
      PREX_ACCOUNT_CMD_MATCH_MAX,
      "^([[:alpha:]]+): (.*)$"
    },
    // clang-format on
  };

  assert((which < PREX_MAX) && "Invalid 'which' argument");
  struct PrexStorage *h = &storage[which];
  assert((which == h->which) && "Fix 'storage' array");
  if (!h->re)
  {
#ifdef HAVE_PCRE2
    uint32_t opt = pcre2_has_unicode() ? PCRE2_UTF : 0;
    int eno = 0;
    PCRE2_SIZE eoff = 0;
    h->re = pcre2_compile((PCRE2_SPTR8) h->str, PCRE2_ZERO_TERMINATED, opt,
                          &eno, &eoff, NULL);
    assert(h->re && "Fix your RE");
    h->mdata = pcre2_match_data_create_from_pattern(h->re, NULL);
    uint32_t ccount = 0;
    pcre2_pattern_info(h->re, PCRE2_INFO_CAPTURECOUNT, &ccount);
    assert(((ccount + 1) == h->nmatches) && "Number of matches do not match (...)");
    h->matches = mutt_mem_calloc(h->nmatches, sizeof(*h->matches));
#else
    h->re = mutt_mem_calloc(1, sizeof(*h->re));
    const int rc = regcomp(h->re, h->str, REG_EXTENDED);
    assert(rc == 0 && "Fix your RE");
    h->matches = mutt_mem_calloc(h->nmatches, sizeof(*h->matches));
#endif
  }
  return h;
}

/**
 * mutt_prex_capture - Match a precompiled regex against a string
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
#ifdef HAVE_PCRE2
  size_t len = strlen(str);
  int rc = pcre2_match(h->re, (PCRE2_SPTR8) str, len, 0, 0, h->mdata, NULL);
  if (rc < 0)
  {
    PCRE2_UCHAR errmsg[1024];
    pcre2_get_error_message(rc, errmsg, sizeof(errmsg));
    mutt_debug(LL_DEBUG2, "pcre2_match - <%s> -> <%s> =  %s\n", h->str, str, errmsg);
    return NULL;
  }
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(h->mdata);
  int i = 0;
  for (; i < rc; i++)
  {
    h->matches[i].rm_so = ovector[i * 2];
    h->matches[i].rm_eo = ovector[i * 2 + 1];
  }
  for (; i < h->nmatches; i++)
  {
    h->matches[i].rm_so = -1;
    h->matches[i].rm_eo = -1;
  }
#else
  if (regexec(h->re, str, h->nmatches, h->matches, 0))
    return NULL;

  assert((h->re->re_nsub == (h->nmatches - 1)) &&
         "Regular expression and matches enum are out of sync");
#endif
  return h->matches;
}

/**
 * mutt_prex_cleanup - Cleanup heap memory allocated by compiled regexes
 */
void mutt_prex_cleanup(void)
{
  for (enum Prex which = 0; which < PREX_MAX; which++)
  {
    struct PrexStorage *h = prex(which);
#ifdef HAVE_PCRE2
    pcre2_match_data_free(h->mdata);
    pcre2_code_free(h->re);
#else
    regfree(h->re);
    FREE(&h->re);
#endif
    FREE(&h->matches);
  }
}
