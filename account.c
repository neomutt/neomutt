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

#include "mutt.h"
#include "account.h"
#include "url.h"

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

/* mutt_account_fromurl: fill account with information from url. */
int mutt_account_fromurl (ACCOUNT* account, ciss_url_t* url)
{
  /* must be present */
  if (url->host)
    strfcpy (account->host, url->host, sizeof (account->host));
  else
    return -1;

  if (url->user)
  {
    strfcpy (account->user, url->user, sizeof (account->user));
    account->flags |= M_ACCT_USER;
  }
  if (url->pass)
  {
    strfcpy (account->pass, url->pass, sizeof (account->pass));
    account->flags |= M_ACCT_PASS;
  }
  if (url->port)
  {
    account->port = url->port;
    account->flags |= M_ACCT_PORT;
  }

  return 0;
}

/* mutt_account_getuser: retrieve username into ACCOUNT, if neccessary */
int mutt_account_getuser (ACCOUNT* account)
{
  char prompt[SHORT_STRING];

  /* already set */
  if (account->flags & M_ACCT_USER)
    return 0;
#ifdef USE_IMAP
  else if ((account->type == M_ACCT_TYPE_IMAP) && ImapUser)
    strfcpy (account->user, ImapUser, sizeof (account->user));
#endif
#ifdef USE_POP
  else if ((account->type == M_ACCT_TYPE_POP) && PopUser)
    strfcpy (account->user, PopUser, sizeof (account->user));
#endif
  /* prompt (defaults to unix username), copy into account->user */
  else
  {
    snprintf (prompt, sizeof (prompt), _("Username at %s: "), account->host);
    strfcpy (account->user, NONULL (Username), sizeof (account->user));
    if (mutt_get_field (prompt, account->user, sizeof (account->user), 0))
      return -1;
  }

  account->flags |= M_ACCT_USER;

  return 0;
}

/* mutt_account_getpass: fetch password into ACCOUNT, if neccessary */
int mutt_account_getpass (ACCOUNT* account)
{
  char prompt[SHORT_STRING];

  if (account->flags & M_ACCT_PASS)
    return 0;
#ifdef USE_IMAP
  else if ((account->type == M_ACCT_TYPE_IMAP) && ImapPass)
    strfcpy (account->pass, ImapPass, sizeof (account->pass));
#endif
#ifdef USE_POP
  else if ((account->type == M_ACCT_TYPE_POP) && PopPass)
    strfcpy (account->pass, PopPass, sizeof (account->pass));
#endif
  else
  {
    snprintf (prompt, sizeof (prompt), _("Password for %s@%s: "),
      account->user, account->host);
    account->pass[0] = '\0';
    if (mutt_get_field (prompt, account->pass, sizeof (account->pass), M_PASS))
      return -1;
  }

  account->flags |= M_ACCT_PASS;

  return 0;
}
