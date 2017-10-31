/**
 * @file
 * Account object used by POP and IMAP
 *
 * @authors
 * Copyright (C) 2000-2007 Brendan Cully <brendan@kublai.com>
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

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "lib/lib.h"
#include "mutt.h"
#include "mutt_account.h"
#include "globals.h"
#include "options.h"
#include "protos.h"
#include "url.h"

/**
 * mutt_account_match - compare account info (host/port/user)
 */
int mutt_account_match(const struct Account *a1, const struct Account *a2)
{
  const char *user = NONULL(Username);

  if (a1->type != a2->type)
    return 0;
  if (mutt_strcasecmp(a1->host, a2->host) != 0)
    return 0;
  if (a1->port != a2->port)
    return 0;

#ifdef USE_IMAP
  if (a1->type == MUTT_ACCT_TYPE_IMAP)
  {
    if (ImapUser)
      user = ImapUser;
  }
#endif

#ifdef USE_POP
  if (a1->type == MUTT_ACCT_TYPE_POP && PopUser)
    user = PopUser;
#endif

#ifdef USE_NNTP
  if (a1->type == MUTT_ACCT_TYPE_NNTP && NntpUser)
    user = NntpUser;
#endif

  if (a1->flags & a2->flags & MUTT_ACCT_USER)
    return (strcmp(a1->user, a2->user) == 0);
#ifdef USE_NNTP
  if (a1->type == MUTT_ACCT_TYPE_NNTP)
    return a1->flags & MUTT_ACCT_USER && a1->user[0] ? 0 : 1;
#endif
  if (a1->flags & MUTT_ACCT_USER)
    return (strcmp(a1->user, user) == 0);
  if (a2->flags & MUTT_ACCT_USER)
    return (strcmp(a2->user, user) == 0);

  return 1;
}

/**
 * mutt_account_fromurl - fill account with information from url
 */
int mutt_account_fromurl(struct Account *account, struct Url *url)
{
  /* must be present */
  if (url->host)
    strfcpy(account->host, url->host, sizeof(account->host));
  else
    return -1;

  if (url->user)
  {
    strfcpy(account->user, url->user, sizeof(account->user));
    account->flags |= MUTT_ACCT_USER;
  }
  if (url->pass)
  {
    strfcpy(account->pass, url->pass, sizeof(account->pass));
    account->flags |= MUTT_ACCT_PASS;
  }
  if (url->port)
  {
    account->port = url->port;
    account->flags |= MUTT_ACCT_PORT;
  }

  return 0;
}

/**
 * mutt_account_tourl - fill URL with info from account
 *
 * The URL information is a set of pointers into account - don't free or edit
 * account until you've finished with url (make a copy of account if you need
 * it for a while).
 */
void mutt_account_tourl(struct Account *account, struct Url *url)
{
  url->scheme = U_UNKNOWN;
  url->user = NULL;
  url->pass = NULL;
  url->port = 0;
  url->path = NULL;

#ifdef USE_IMAP
  if (account->type == MUTT_ACCT_TYPE_IMAP)
  {
    if (account->flags & MUTT_ACCT_SSL)
      url->scheme = U_IMAPS;
    else
      url->scheme = U_IMAP;
  }
#endif

#ifdef USE_POP
  if (account->type == MUTT_ACCT_TYPE_POP)
  {
    if (account->flags & MUTT_ACCT_SSL)
      url->scheme = U_POPS;
    else
      url->scheme = U_POP;
  }
#endif

#ifdef USE_SMTP
  if (account->type == MUTT_ACCT_TYPE_SMTP)
  {
    if (account->flags & MUTT_ACCT_SSL)
      url->scheme = U_SMTPS;
    else
      url->scheme = U_SMTP;
  }
#endif

#ifdef USE_NNTP
  if (account->type == MUTT_ACCT_TYPE_NNTP)
  {
    if (account->flags & MUTT_ACCT_SSL)
      url->scheme = U_NNTPS;
    else
      url->scheme = U_NNTP;
  }
#endif

  url->host = account->host;
  if (account->flags & MUTT_ACCT_PORT)
    url->port = account->port;
  if (account->flags & MUTT_ACCT_USER)
    url->user = account->user;
  if (account->flags & MUTT_ACCT_PASS)
    url->pass = account->pass;
}

/**
 * mutt_account_getuser - retrieve username into Account, if necessary
 */
int mutt_account_getuser(struct Account *account)
{
  char prompt[STRING];

  /* already set */
  if (account->flags & MUTT_ACCT_USER)
    return 0;
#ifdef USE_IMAP
  else if ((account->type == MUTT_ACCT_TYPE_IMAP) && ImapUser)
    strfcpy(account->user, ImapUser, sizeof(account->user));
#endif
#ifdef USE_POP
  else if ((account->type == MUTT_ACCT_TYPE_POP) && PopUser)
    strfcpy(account->user, PopUser, sizeof(account->user));
#endif
#ifdef USE_NNTP
  else if ((account->type == MUTT_ACCT_TYPE_NNTP) && NntpUser)
    strfcpy(account->user, NntpUser, sizeof(account->user));
#endif
  else if (option(OPT_NO_CURSES))
    return -1;
  /* prompt (defaults to unix username), copy into account->user */
  else
  {
    /* L10N: Example: Username at myhost.com */
    snprintf(prompt, sizeof(prompt), _("Username at %s: "), account->host);
    strfcpy(account->user, NONULL(Username), sizeof(account->user));
    if (mutt_get_field_unbuffered(prompt, account->user, sizeof(account->user), 0))
      return -1;
  }

  account->flags |= MUTT_ACCT_USER;

  return 0;
}

int mutt_account_getlogin(struct Account *account)
{
  /* already set */
  if (account->flags & MUTT_ACCT_LOGIN)
    return 0;
#ifdef USE_IMAP
  else if (account->type == MUTT_ACCT_TYPE_IMAP)
  {
    if (ImapLogin)
    {
      strfcpy(account->login, ImapLogin, sizeof(account->login));
      account->flags |= MUTT_ACCT_LOGIN;
    }
  }
#endif

  if (!(account->flags & MUTT_ACCT_LOGIN))
  {
    mutt_account_getuser(account);
    strfcpy(account->login, account->user, sizeof(account->login));
  }

  account->flags |= MUTT_ACCT_LOGIN;

  return 0;
}

/**
 * mutt_account_getpass - fetch password into Account, if necessary
 */
int mutt_account_getpass(struct Account *account)
{
  char prompt[STRING];

  if (account->flags & MUTT_ACCT_PASS)
    return 0;
#ifdef USE_IMAP
  else if ((account->type == MUTT_ACCT_TYPE_IMAP) && ImapPass)
    strfcpy(account->pass, ImapPass, sizeof(account->pass));
#endif
#ifdef USE_POP
  else if ((account->type == MUTT_ACCT_TYPE_POP) && PopPass)
    strfcpy(account->pass, PopPass, sizeof(account->pass));
#endif
#ifdef USE_SMTP
  else if ((account->type == MUTT_ACCT_TYPE_SMTP) && SmtpPass)
    strfcpy(account->pass, SmtpPass, sizeof(account->pass));
#endif
#ifdef USE_NNTP
  else if ((account->type == MUTT_ACCT_TYPE_NNTP) && NntpPass)
    strfcpy(account->pass, NntpPass, sizeof(account->pass));
#endif
  else if (option(OPT_NO_CURSES))
    return -1;
  else
  {
    snprintf(prompt, sizeof(prompt), _("Password for %s@%s: "),
             account->flags & MUTT_ACCT_LOGIN ? account->login : account->user,
             account->host);
    account->pass[0] = '\0';
    if (mutt_get_password(prompt, account->pass, sizeof(account->pass)))
      return -1;
  }

  account->flags |= MUTT_ACCT_PASS;

  return 0;
}

void mutt_account_unsetpass(struct Account *account)
{
  account->flags &= ~MUTT_ACCT_PASS;
}
