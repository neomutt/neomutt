/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2000 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/* IMAP login/authentication code */

#include "mutt.h"
#include "imap_private.h"
#include "auth.h"

static imap_auth_t imap_authenticators[] = {
  imap_auth_anon,
#ifdef USE_SASL
  imap_auth_sasl,
#endif
#ifdef USE_GSS
  imap_auth_gss,
#endif
  /* SASL includes CRAM-MD5 (and GSSAPI, but that's not enabled by default) */
#ifndef USE_SASL
  imap_auth_cram_md5,
#endif
  imap_auth_login,

  NULL
};

/* imap_authenticate: oh how simple! loops through authenticators. */
int imap_authenticate (IMAP_DATA* idata)
{
  imap_auth_t* authenticator = imap_authenticators;
  int r = -1;

  while (authenticator)
  {
    if ((r = (*authenticator)(idata)) != IMAP_AUTH_UNAVAIL)
      return r;
    authenticator++;
  }

  return r;
}
