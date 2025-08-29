/**
 * @file
 * Handling of international domain names
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2020 Pietro Cerutti <gahr@gahr.ch>
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
 * @page addr_idna International domain names
 *
 * Handling of international domain names
 */

#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "idna2.h"
#ifdef HAVE_LIBIDN
#include <stdbool.h>
#include <string.h>
#endif
#ifdef HAVE_STRINGPREP_H
#include <stringprep.h>
#elif defined(HAVE_IDN_STRINGPREP_H)
#include <idn/stringprep.h>
#endif
#define IDN2_SKIP_LIBIDN_COMPAT
#ifdef HAVE_LIBIDN
#include <idn2.h>
#elif defined(HAVE_IDN_IDN2_H)
#include <idn/idn2.h>
#elif defined(HAVE_IDN_IDNA_H)
#include <idn/idna.h>
#endif

#ifdef HAVE_LIBIDN
/**
 * check_idn - Is domain in Punycode?
 * @param domain Domain to test
 * @retval true At least one part of domain is in Punycode
 */
static bool check_idn(char *domain)
{
  if (!domain)
    return false;

  if (mutt_istr_startswith(domain, "xn--"))
    return true;

  while ((domain = strchr(domain, '.')))
  {
    if (mutt_istr_startswith(++domain, "xn--"))
      return true;
  }

  return false;
}

/**
 * mutt_idna_to_ascii_lz - Convert a domain to Punycode
 * @param[in]  input  Domain
 * @param[out] output Result
 * @param[in]  flags  Flags, e.g. IDNA_ALLOW_UNASSIGNED
 * @retval 0 Success
 * @retval >0 Failure, error code
 *
 * Convert a domain from the current locale to Punycode.
 *
 * @note The caller must free output
 */
int mutt_idna_to_ascii_lz(const char *input, char **output, uint8_t flags)
{
  if (!input || !output)
    return 1;

  return idn2_to_ascii_8z(input, output, flags | IDN2_NFC_INPUT | IDN2_NONTRANSITIONAL);
}
#endif

/**
 * mutt_idna_intl_to_local - Convert an email's domain from Punycode
 * @param user   Username
 * @param domain Domain
 * @param flags  Flags, e.g. #MI_MAY_BE_IRREVERSIBLE
 * @retval ptr  Newly allocated local email address
 * @retval NULL Error in conversion
 *
 * If `$idn_decode` is set, then the domain will be converted from Punycode.
 * For example, "xn--ls8h.la" becomes the emoji domain: ":poop:.la"
 * Then the user and domain are changed from 'utf-8' to the encoding in
 * `$charset`.
 *
 * If the flag #MI_MAY_BE_IRREVERSIBLE is NOT given, then the results will be
 * checked to make sure that the transformation is "undo-able".
 *
 * @note The caller must free the returned string.
 */
char *mutt_idna_intl_to_local(const char *user, const char *domain, uint8_t flags)
{
  char *mailbox = NULL;
  char *reversed_user = NULL, *reversed_domain = NULL;
  char *tmp = NULL;

  char *local_user = mutt_str_dup(user);
  char *local_domain = mutt_str_dup(domain);

#ifdef HAVE_LIBIDN
  bool is_idn_encoded = check_idn(local_domain);
  const bool c_idn_decode = cs_subset_bool(NeoMutt->sub, "idn_decode");
  if (is_idn_encoded && c_idn_decode)
  {
    if (idn2_to_unicode_8z8z(local_domain, &tmp, IDN2_ALLOW_UNASSIGNED) != IDN2_OK)
    {
      goto cleanup;
    }
    mutt_str_replace(&local_domain, tmp);
    FREE(&tmp);
  }
#endif

  /* we don't want charset-hook effects, so we set flags to 0 */
  if (mutt_ch_convert_string(&local_user, "utf-8", cc_charset(), MUTT_ICONV_NO_FLAGS) != 0)
    goto cleanup;

  if (mutt_ch_convert_string(&local_domain, "utf-8", cc_charset(), MUTT_ICONV_NO_FLAGS) != 0)
    goto cleanup;

  /* make sure that we can convert back and come out with the same
   * user and domain name.  */
  if ((flags & MI_MAY_BE_IRREVERSIBLE) == 0)
  {
    reversed_user = mutt_str_dup(local_user);

    if (mutt_ch_convert_string(&reversed_user, cc_charset(), "utf-8", MUTT_ICONV_NO_FLAGS) != 0)
    {
      mutt_debug(LL_DEBUG1, "Not reversible. Charset conv to utf-8 failed for user = '%s'\n",
                 reversed_user);
      goto cleanup;
    }

    if (!mutt_istr_equal(user, reversed_user))
    {
      mutt_debug(LL_DEBUG1, "#1 Not reversible. orig = '%s', reversed = '%s'\n",
                 user, reversed_user);
      goto cleanup;
    }

    reversed_domain = mutt_str_dup(local_domain);

    if (mutt_ch_convert_string(&reversed_domain, cc_charset(), "utf-8", MUTT_ICONV_NO_FLAGS) != 0)
    {
      mutt_debug(LL_DEBUG1, "Not reversible. Charset conv to utf-8 failed for domain = '%s'\n",
                 reversed_domain);
      goto cleanup;
    }

#ifdef HAVE_LIBIDN
    /* If the original domain was UTF-8, idna encoding here could
     * produce a non-matching domain!  Thus we only want to do the
     * idn2_to_ascii_8z() if the original domain was IDNA encoded.  */
    if (is_idn_encoded && c_idn_decode)
    {
      if (idn2_to_ascii_8z(reversed_domain, &tmp,
                           IDN2_ALLOW_UNASSIGNED | IDN2_NFC_INPUT | IDN2_NONTRANSITIONAL) != IDN2_OK)
      {
        mutt_debug(LL_DEBUG1, "Not reversible. idn2_to_ascii_8z failed for domain = '%s'\n",
                   reversed_domain);
        goto cleanup;
      }
      mutt_str_replace(&reversed_domain, tmp);
    }
#endif

    if (!mutt_istr_equal(domain, reversed_domain))
    {
      mutt_debug(LL_DEBUG1, "#2 Not reversible. orig = '%s', reversed = '%s'\n",
                 domain, reversed_domain);
      goto cleanup;
    }
  }

  mutt_str_asprintf(&mailbox, "%s@%s", NONULL(local_user), NONULL(local_domain));

cleanup:
  FREE(&local_user);
  FREE(&local_domain);
  FREE(&tmp);
  FREE(&reversed_domain);
  FREE(&reversed_user);

  return mailbox;
}

/**
 * mutt_idna_local_to_intl - Convert an email's domain to Punycode
 * @param user   Username
 * @param domain Domain
 * @retval ptr  Newly allocated Punycode email address
 * @retval NULL Error in conversion
 *
 * The user and domain are assumed to be encoded according to `$charset`.
 * They are converted to 'utf-8'.  If `$idn_encode` is set, then the domain
 * will be converted to Punycode.  For example, the emoji domain:
 * ":poop:.la" becomes "xn--ls8h.la"
 *
 * @note The caller must free the returned string.
 */
char *mutt_idna_local_to_intl(const char *user, const char *domain)
{
  char *mailbox = NULL;
  char *tmp = NULL;

  char *intl_user = mutt_str_dup(user);
  char *intl_domain = mutt_str_dup(domain);

  /* we don't want charset-hook effects, so we set flags to 0 */
  if (mutt_ch_convert_string(&intl_user, cc_charset(), "utf-8", MUTT_ICONV_NO_FLAGS) != 0)
    goto cleanup;

  if (mutt_ch_convert_string(&intl_domain, cc_charset(), "utf-8", MUTT_ICONV_NO_FLAGS) != 0)
    goto cleanup;

#ifdef HAVE_LIBIDN
  const bool c_idn_encode = cs_subset_bool(NeoMutt->sub, "idn_encode");
  if (c_idn_encode)
  {
    if (idn2_to_ascii_8z(intl_domain, &tmp,
                         IDN2_ALLOW_UNASSIGNED | IDN2_NFC_INPUT | IDN2_NONTRANSITIONAL) != IDN2_OK)
    {
      goto cleanup;
    }
    mutt_str_replace(&intl_domain, tmp);
  }
#endif

  mutt_str_asprintf(&mailbox, "%s@%s", NONULL(intl_user), NONULL(intl_domain));

cleanup:
  FREE(&intl_user);
  FREE(&intl_domain);
  FREE(&tmp);

  return mailbox;
}

#ifdef HAVE_LIBIDN
/**
 * mutt_idna_print_version - Create an IDN version string
 * @retval ptr Version string
 *
 * @note This is a static string and must not be freed.
 */
const char *mutt_idna_print_version(void)
{
  static char vstring[256] = { 0 };

  snprintf(vstring, sizeof(vstring), "%s (compiled with %s)",
           idn2_check_version(NULL), IDN2_VERSION);

  return vstring;
}
#endif
