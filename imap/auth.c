/**
 * @file
 * IMAP authenticator multiplexor
 *
 * @authors
 * Copyright (C) 1996-1998 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2001 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2017-2022 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Yousef Akbar <yousef@yhakbar.com>
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
 * @page imap_auth Authenticator multiplexor
 *
 * IMAP authenticator multiplexor
 */

#include "config.h"
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "auth.h"

/**
 * struct ImapAuth - IMAP authentication multiplexor
 */
struct ImapAuth
{
  /**
   * @defgroup imap_authenticate IMAP Authentication API
   *
   * authenticate - Authenticate an IMAP connection
   * @param adata Imap Account data
   * @param method Use this named method, or any available method if NULL
   * @retval #ImapAuthRes Result, e.g. #IMAP_AUTH_SUCCESS
   */
  enum ImapAuthRes (*authenticate)(struct ImapAccountData *adata, const char *method);

  const char *method; ///< Name of authentication method supported, NULL means variable.
      ///< If this is not null, authenticate may ignore the second parameter.
};

/**
 * ImapAuthenticators - Accepted authentication methods
 */
static const struct ImapAuth ImapAuthenticators[] = {
  // clang-format off
  { imap_auth_oauth, "oauthbearer" },
  { imap_auth_xoauth2, "xoauth2" },
  { imap_auth_plain, "plain" },
#if defined(USE_SASL_CYRUS)
  { imap_auth_sasl, NULL },
#elif defined(USE_SASL_GNU)
  { imap_auth_gsasl, NULL },
#else
  { imap_auth_anon, "anonymous" },
#endif
#ifdef USE_GSS
  { imap_auth_gss, "gssapi" },
#endif
/* SASL includes CRAM-MD5 (and GSSAPI, but that's not enabled by default) */
#ifndef HAVE_SASL
  { imap_auth_cram_md5, "cram-md5" },
#endif
  { imap_auth_login, "login" },
  // clang-format on
};

/**
 * imap_auth_is_valid - Check if string is a valid imap authentication method
 * @param authenticator Authenticator string to check
 * @retval true Argument is a valid auth method
 *
 * Validate whether an input string is an accepted imap authentication method as
 * defined by #ImapAuthenticators.
 */
bool imap_auth_is_valid(const char *authenticator)
{
  for (size_t i = 0; i < countof(ImapAuthenticators); i++)
  {
    const struct ImapAuth *auth = &ImapAuthenticators[i];
    if (auth->method && mutt_istr_equal(auth->method, authenticator))
      return true;
  }

  return false;
}

/**
 * imap_authenticate - Authenticate to an IMAP server
 * @param adata Imap Account data
 * @retval enum ImapAuthRes, e.g. #IMAP_AUTH_SUCCESS
 *
 * Attempt to authenticate using either user-specified authentication method if
 * specified, or any.
 */
int imap_authenticate(struct ImapAccountData *adata)
{
  int rc = IMAP_AUTH_FAILURE;

  const struct Slist *c_imap_authenticators = cs_subset_slist(NeoMutt->sub, "imap_authenticators");
  if (c_imap_authenticators && (c_imap_authenticators->count > 0))
  {
    mutt_debug(LL_DEBUG2, "Trying user-defined imap_authenticators\n");

    /* Try user-specified list of authentication methods */
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &c_imap_authenticators->head, entries)
    {
      mutt_debug(LL_DEBUG2, "Trying method %s\n", np->data);

      for (size_t i = 0; i < countof(ImapAuthenticators); i++)
      {
        const struct ImapAuth *auth = &ImapAuthenticators[i];
        if (!auth->method || mutt_istr_equal(auth->method, np->data))
        {
          rc = auth->authenticate(adata, np->data);
          if (rc == IMAP_AUTH_SUCCESS)
          {
            return rc;
          }
        }
      }
    }
  }
  else
  {
    /* Fall back to default: any authenticator */
    mutt_debug(LL_DEBUG2, "Trying pre-defined imap_authenticators\n");

    for (size_t i = 0; i < countof(ImapAuthenticators); i++)
    {
      rc = ImapAuthenticators[i].authenticate(adata, NULL);
      if (rc == IMAP_AUTH_SUCCESS)
        return rc;
    }
  }

  mutt_error(_("No authenticators available or wrong credentials"));
  return rc;
}
