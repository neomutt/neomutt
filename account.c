/*
 * Copyright (C) 2000-7 Brendan Cully <brendan@kublai.com>
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

/* remote host account manipulation (POP/IMAP) */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "account.h"
#include "url.h"

/* mutt_account_match: compare account info (host/port/user) */
int mutt_account_match (const ACCOUNT* a1, const ACCOUNT* a2)
{
  const char* user = NONULL (Username);

  if (a1->type != a2->type)
    return 0;
  if (ascii_strcasecmp (a1->host, a2->host))
    return 0;
  if (a1->port != a2->port)
    return 0;

#ifdef USE_IMAP
  if (a1->type == M_ACCT_TYPE_IMAP)
  {
    if (ImapUser)
      user = ImapUser;
  }
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

/* mutt_account_tourl: fill URL with info from account. The URL information
 *   is a set of pointers into account - don't free or edit account until
 *   you've finished with url (make a copy of account if you need it for
 *   a while). */
void mutt_account_tourl (ACCOUNT* account, ciss_url_t* url)
{
  url->scheme = U_UNKNOWN;
  url->user = NULL;
  url->pass = NULL;
  url->port = 0;

#ifdef USE_IMAP
  if (account->type == M_ACCT_TYPE_IMAP)
  {
    if (account->flags & M_ACCT_SSL)
      url->scheme = U_IMAPS;
    else
      url->scheme = U_IMAP;
  }
#endif

#ifdef USE_POP
  if (account->type == M_ACCT_TYPE_POP)
  {
    if (account->flags & M_ACCT_SSL)
      url->scheme = U_POPS;
    else
      url->scheme = U_POP;
  }
#endif

#ifdef USE_SMTP
  if (account->type == M_ACCT_TYPE_SMTP)
  {
    if (account->flags & M_ACCT_SSL)
      url->scheme = U_SMTPS;
    else
      url->scheme = U_SMTP;
  }
#endif

  url->host = account->host;
  if (account->flags & M_ACCT_PORT)
    url->port = account->port;
  if (account->flags & M_ACCT_USER)
    url->user = account->user;
  if (account->flags & M_ACCT_PASS)
    url->pass = account->pass;
}

/* mutt_account_getuser: retrieve username into ACCOUNT, if necessary */
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
  else if (option (OPTNOCURSES))
    return -1;
  /* prompt (defaults to unix username), copy into account->user */
  else
  {
    snprintf (prompt, sizeof (prompt), _("Username at %s: "), account->host);
    strfcpy (account->user, NONULL (Username), sizeof (account->user));
    if (mutt_get_field_unbuffered (prompt, account->user, sizeof (account->user), 0))
      return -1;
  }

  account->flags |= M_ACCT_USER;

  return 0;
}

int mutt_account_getlogin (ACCOUNT* account)
{
  /* already set */
  if (account->flags & M_ACCT_LOGIN)
    return 0;
#ifdef USE_IMAP
  else if (account->type == M_ACCT_TYPE_IMAP)
  {
    if (ImapLogin)
    {
      strfcpy (account->login, ImapLogin, sizeof (account->login));
      account->flags |= M_ACCT_LOGIN;
    }
  }
#endif

  if (!(account->flags & M_ACCT_LOGIN))
  {
    mutt_account_getuser (account);
    strfcpy (account->login, account->user, sizeof (account->login));
  }

  account->flags |= M_ACCT_LOGIN;

  return 0;
}

/* mutt_account_getpass: fetch password into ACCOUNT, if necessary */
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
#ifdef USE_SMTP
  else if ((account->type == M_ACCT_TYPE_SMTP) && SmtpPass)
    strfcpy (account->pass, SmtpPass, sizeof (account->pass));
#endif
  else if (option (OPTNOCURSES))
    return -1;
  else
  {
    snprintf (prompt, sizeof (prompt), _("Password for %s@%s: "),
              account->flags & M_ACCT_LOGIN ? account->login : account->user,
              account->host);
    account->pass[0] = '\0';
    if (mutt_get_password (prompt, account->pass, sizeof (account->pass)))
      return -1;
  }

  account->flags |= M_ACCT_PASS;

  return 0;
}

void mutt_account_unsetpass (ACCOUNT* account)
{
  account->flags &= ~M_ACCT_PASS;
}
