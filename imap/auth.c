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
#include "globals.h"
#include "protos.h"

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
  { imap_auth_login, "login" },

  { NULL, NULL },
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
  const struct ImapAuth *authenticator = NULL;
  char *methods = NULL;
  char *method = NULL;
  char *delim = NULL;
  int r = IMAP_AUTH_UNAVAIL;

  if (ImapAuthenticators && *ImapAuthenticators)
  {
    /* Try user-specified list of authentication methods */
    methods = mutt_str_strdup(ImapAuthenticators);

    for (method = methods; method; method = delim)
    {
      delim = strchr(method, ':');
      if (delim)
        *delim++ = '\0';
      if (!method[0])
        continue;

      mutt_debug(2, "Trying method %s\n", method);
      authenticator = imap_authenticators;

      while (authenticator->authenticate)
      {
        if (!authenticator->method ||
            (mutt_str_strcasecmp(authenticator->method, method) == 0))
        {
          r = authenticator->authenticate(idata, method);
          if (r != IMAP_AUTH_UNAVAIL)
          {
            FREE(&methods);
            return r;
          }
        }

        authenticator++;
      }
    }

    FREE(&methods);
  }
  else
  {
    /* Fall back to default: any authenticator */
    mutt_debug(2, "Using any available method.\n");
    authenticator = imap_authenticators;

    while (authenticator->authenticate)
    {
      r = authenticator->authenticate(idata, NULL);
      if (r != IMAP_AUTH_UNAVAIL)
        return r;
      authenticator++;
    }
  }

  if (r == IMAP_AUTH_UNAVAIL)
  {
    mutt_error(_("No authenticators available"));
  }

  return r;
}
