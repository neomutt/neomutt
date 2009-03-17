/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2001 Brendan Cully <brendan@kublai.com>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

/* IMAP login/authentication code */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "imap_private.h"
#include "auth.h"

static imap_auth_t imap_authenticators[] = {
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

  { NULL, NULL }
};

/* imap_authenticate: Attempt to authenticate using either user-specified
 *   authentication method if specified, or any. */
int imap_authenticate (IMAP_DATA* idata)
{
  imap_auth_t* authenticator;
  char* methods;
  char* method;
  char* delim;
  int r = -1;

  if (ImapAuthenticators && *ImapAuthenticators)
  {
    /* Try user-specified list of authentication methods */
    methods = safe_strdup (ImapAuthenticators);

    for (method = methods; method; method = delim)
    {
      delim = strchr (method, ':');
      if (delim)
	*delim++ = '\0';
      if (! method[0])
	continue;
      
      dprint (2, (debugfile, "imap_authenticate: Trying method %s\n", method));
      authenticator = imap_authenticators;

      while (authenticator->authenticate)
      {
	if (!authenticator->method ||
	    !ascii_strcasecmp (authenticator->method, method))
	  if ((r = authenticator->authenticate (idata, method)) !=
	      IMAP_AUTH_UNAVAIL)
	  {
	    FREE (&methods);
	    return r;
	  }
	
	authenticator++;
      }
    }

    FREE (&methods);
  }
  else
  {
    /* Fall back to default: any authenticator */
    dprint (2, (debugfile, "imap_authenticate: Using any available method.\n"));
    authenticator = imap_authenticators;

    while (authenticator->authenticate)
    {
      if ((r = authenticator->authenticate (idata, NULL)) != IMAP_AUTH_UNAVAIL)
	return r;
      authenticator++;
    }
  }

  if (r == IMAP_AUTH_UNAVAIL)
  {
    mutt_error (_("No authenticators available"));
    mutt_sleep (1);
  }
  
  return r;
}
