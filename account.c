/*
 * Copyright (C) 2000 Brendan Cully <brendan@kublai.com>
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

/* remote host account manipulation (POP/IMAP) */

#include "account.h"
#include "mutt.h"

/* mutt_account_match: compare account info (host/port/user) */
int mutt_account_match (const ACCOUNT* a1, const ACCOUNT* a2)
{
  const char* user = NONULL (Username);

  if (a1->type != a2->type)
    return 0;
  if (mutt_strcasecmp (a1->host, a2->host))
    return 0;
  if (a1->port != a2->port)
    return 0;

#ifdef USE_IMAP
  if (a1->type == M_ACCT_TYPE_IMAP && ImapUser)
    user = ImapUser;
#endif

#ifdef USE_POP
  if (a1->type == M_ACCT_TYPE_POP && PopUser)
    user = PopUser;
#endif
  
  if (a1->flags & a2->flags & M_ACCT_USER)
    return (!strcmp (a1->user, a2->user));
  if (a1->flags & M_ACCT_USER)
    return (!strcmp (a1->user, user));
  if (a2->flags & M_ACCT_USER)
    return (!strcmp (a2->user, user));

  return 1;
}
