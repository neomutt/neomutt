/**
 * @file
 * IMAP authenticator multiplexor
 *
 * @authors
 * Copyright (C) 1996-1998 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2001 Brendan Cully <brendan@kublai.com>
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
 * @page imap_auth IMAP authenticator multiplexor
 *
 * IMAP authenticator multiplexor
 */

#include "config.h"
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "auth.h"

/* These Config Variables are only used in imap/auth.c */
struct Slist *C_ImapAuthenticators; ///< Config: (imap) List of allowed IMAP authentication methods

/**
 * struct ImapAuth - IMAP authentication multiplexor
 */
struct ImapAuth
{
  /**
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
 * imap_authenticators - Accepted authentication methods
 */
static const struct ImapAuth imap_authenticators[] = {
  { imap_auth_oauth, "oauthbearer" }, { imap_auth_plain, "plain" },
#ifdef USE_SASL
  { imap_auth_sasl, NULL },
#else
  { imap_auth_anon, "anonymous" },
#endif
#ifdef USE_GSS
  { imap_auth_gss, "gssapi" },
#endif
/* SASL includes CRAM-MD5 (and GSSAPI, but that's not enabled by default) */
#ifndef USE_SASL
  { imap_auth_cram_md5, "cram-md5" },
#endif
  { imap_auth_login, "login" },
};

/**
 * imap_authenticate - Authenticate to an IMAP server
 * @param adata Imap Account data
 * @retval num Result, e.g. #IMAP_AUTH_SUCCESS
 *
 * Attempt to authenticate using either user-specified authentication method if
 * specified, or any.
 */
int imap_authenticate(struct ImapAccountData *adata)
{
  int rc = IMAP_AUTH_FAILURE;

  if (C_ImapAuthenticators && (C_ImapAuthenticators->count > 0))
  {
    mutt_debug(LL_DEBUG2, "Trying user-defined imap_authenticators\n");

    /* Try user-specified list of authentication methods */
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &C_ImapAuthenticators->head, entries)
    {
      mutt_debug(LL_DEBUG2, "Trying method %s\n", np->data);

      for (size_t i = 0; i < mutt_array_size(imap_authenticators); i++)
      {
        const struct ImapAuth *auth = &imap_authenticators[i];
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

    for (size_t i = 0; i < mutt_array_size(imap_authenticators); i++)
    {
      rc = imap_authenticators[i].authenticate(adata, NULL);
      if (rc == IMAP_AUTH_SUCCESS)
        return rc;
    }
  }

  mutt_error(_("No authenticators available or wrong credentials"));
  return rc;
}
