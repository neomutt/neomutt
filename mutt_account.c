/**
 * @file
 * ConnAccount object used by POP and IMAP
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

/**
 * @page mutt_account ConnAccount object used by POP and IMAP
 *
 * ConnAccount object used by POP and IMAP
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "mutt_account.h"
#include "globals.h"
#include "init.h"
#include "options.h"
#include "tracker.h"

/**
 * mutt_account_fromurl - Fill ConnAccount with information from url
 * @param cac ConnAccount to fill
 * @param url Url to parse
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_account_fromurl(struct ConnAccount *cac, const struct Url *url)
{
  /* must be present */
  if (url->host)
    mutt_str_strfcpy(cac->host, url->host, sizeof(cac->host));
  else
    return -1;

  if (url->user)
  {
    mutt_str_strfcpy(cac->user, url->user, sizeof(cac->user));
    cac->flags |= MUTT_ACCT_USER;
  }
  if (url->pass)
  {
    mutt_str_strfcpy(cac->pass, url->pass, sizeof(cac->pass));
    cac->flags |= MUTT_ACCT_PASS;
  }
  if (url->port)
  {
    cac->port = url->port;
    cac->flags |= MUTT_ACCT_PORT;
  }

  return 0;
}

/**
 * mutt_account_tourl - Fill URL with info from account
 * @param cac Source ConnAccount
 * @param url Url to fill
 *
 * The URL information is a set of pointers into cac - don't free or edit cac
 * until you've finished with url (make a copy of cac if you need it for a
 * while).
 */
void mutt_account_tourl(struct ConnAccount *cac, struct Url *url)
{
  url->scheme = U_UNKNOWN;
  url->user = NULL;
  url->pass = NULL;
  url->port = 0;
  url->path = NULL;

#ifdef USE_IMAP
  if (cac->type == MUTT_ACCT_TYPE_IMAP)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      url->scheme = U_IMAPS;
    else
      url->scheme = U_IMAP;
  }
#endif

#ifdef USE_POP
  if (cac->type == MUTT_ACCT_TYPE_POP)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      url->scheme = U_POPS;
    else
      url->scheme = U_POP;
  }
#endif

#ifdef USE_SMTP
  if (cac->type == MUTT_ACCT_TYPE_SMTP)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      url->scheme = U_SMTPS;
    else
      url->scheme = U_SMTP;
  }
#endif

#ifdef USE_NNTP
  if (cac->type == MUTT_ACCT_TYPE_NNTP)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      url->scheme = U_NNTPS;
    else
      url->scheme = U_NNTP;
  }
#endif

  url->host = cac->host;
  if (cac->flags & MUTT_ACCT_PORT)
    url->port = cac->port;
  if (cac->flags & MUTT_ACCT_USER)
    url->user = cac->user;
  if (cac->flags & MUTT_ACCT_PASS)
    url->pass = cac->pass;
}

/**
 * mutt_parse_account - Parse the 'account' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_account(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  /* Go back to the default account */
  if (!MoreArgs(s))
  {
    ct_set_account(NULL);
    return MUTT_CMD_SUCCESS;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  struct Account *a = account_find(buf->data);
  if (!a)
  {
    a = account_new(buf->data, NeoMutt->sub);
    neomutt_account_add(NeoMutt, a);
  }

  /* Set the current account, nothing more to do */
  if (!MoreArgs(s))
  {
    ct_set_account(a);
    return MUTT_CMD_SUCCESS;
  }

  /* Temporarily alter the current account */
  ct_push_top();
  ct_set_account(a);

  /* Process the rest of the line */
  enum CommandResult rc = mutt_parse_rc_line(s->dptr, err);
  if (rc == MUTT_CMD_ERROR)
    mutt_error("%s", err->data);

  ct_pop(); // Restore the previous account
  mutt_buffer_reset(s);

  return rc;
}

/**
 * mutt_parse_unaccount - Parse the 'unaccount' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_unaccount(struct Buffer *buf, struct Buffer *s,
                                        intptr_t data, struct Buffer *err)
{
  mutt_message("%s", s->data);
  return MUTT_CMD_SUCCESS;
}
