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
#include "mutt/mutt.h"
#include "auth.h"

/* These Config Variables are only used in imap/auth.c */
char *ImapAuthenticators; ///< Config: (imap) List of allowed IMAP authentication methods

/**
 * imap_authenticators - Accepted authentication methods
 */
static const struct ImapAuth imap_authenticators[] = {
  { imap_auth_plain, "plain" },
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
  { imap_auth_login, "login" },       { imap_auth_oauth, "oauthbearer" },
};

/**
 * imap_authenticate - Authenticate to an IMAP server
 * @param idata Server data
 * @retval num Result, e.g. #IMAP_AUTH_SUCCESS
 *
 * Attempt to authenticate using either user-specified authentication method if
 * specified, or any.
 */
int imap_authenticate(struct ImapData *idata)
{
  int r = IMAP_AUTH_FAILURE;

  if (ImapAuthenticators && *ImapAuthenticators)
  {
    mutt_debug(2, "Trying user-defined imap_authenticators.\n");

    /* Try user-specified list of authentication methods */
    char *methods = mutt_str_strdup(ImapAuthenticators);
    char *delim = NULL;

    for (const char *method = methods; method; method = delim)
    {
      delim = strchr(method, ':');
      if (delim)
        *delim++ = '\0';
      if (!method[0])
        continue;

      mutt_debug(2, "Trying method %s\n", method);

      for (size_t i = 0; i < mutt_array_size(imap_authenticators); ++i)
      {
        const struct ImapAuth *auth = &imap_authenticators[i];
        if (!auth->method || (mutt_str_strcasecmp(auth->method, method) == 0))
        {
          r = auth->authenticate(idata, method);
          if (r == IMAP_AUTH_SUCCESS)
          {
            FREE(&methods);
            return r;
          }
        }
      }
    }

    FREE(&methods);
  }
  else
  {
    /* Fall back to default: any authenticator */
    mutt_debug(2, "Trying pre-defined imap_authenticators.\n");

    for (size_t i = 0; i < mutt_array_size(imap_authenticators); ++i)
    {
      r = imap_authenticators[i].authenticate(idata, NULL);
      if (r == IMAP_AUTH_SUCCESS)
        return r;
    }
  }

  mutt_error(_("No authenticators available or wrong credentials"));
  return r;
}
