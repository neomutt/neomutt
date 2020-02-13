/**
 * @file
 * NeoMutt connections
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
 * @page mutt_socket NeoMutt connections
 *
 * NeoMutt connections
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "email/lib.h"
#include "conn/lib.h"
#include "mutt_socket.h"
#include "hook.h"
#include "mutt_account.h"
#ifndef USE_SSL
#include "mutt/lib.h"
#endif

/**
 * mutt_conn_new - Create a new Connection
 * @param cac Credentials to use
 * @retval ptr New Connection
 */
struct Connection *mutt_conn_new(const struct ConnAccount *cac)
{
  enum ConnectionType conn_type;

  if (C_Tunnel)
    conn_type = MUTT_CONNECTION_TUNNEL;
  else if (cac->flags & MUTT_ACCT_SSL)
    conn_type = MUTT_CONNECTION_SSL;
  else
    conn_type = MUTT_CONNECTION_SIMPLE;

  struct Connection *conn = mutt_socket_new(conn_type);
  if (conn)
  {
    memcpy(&conn->account, cac, sizeof(struct ConnAccount));
  }
  else
  {
    if (conn_type == MUTT_CONNECTION_SSL)
    {
#ifndef USE_SSL
      /* that's probably why it failed */
      mutt_error(_("SSL is unavailable, can't connect to %s"), cac->host);
#endif
    }
  }
  return conn;
}

/**
 * mutt_conn_find - Find a connection from a list
 * @param cac   ConnAccount to match
 * @retval ptr Matching Connection
 *
 * find a connection off the list of connections whose account matches cac.
 * If start is not null, only search for connections after the given connection
 * (allows higher level socket code to make more fine-grained searches than
 * account info - eg in IMAP we may wish to find a connection which is not in
 * IMAP_SELECTED state)
 */
struct Connection *mutt_conn_find(const struct ConnAccount *cac)
{
  struct Url url = { 0 };
  char hook[1024];

  /* cac isn't actually modified, since url isn't either */
  mutt_account_tourl((struct ConnAccount *) cac, &url);
  url.path = NULL;
  url_tostring(&url, hook, sizeof(hook), 0);
  mutt_account_hook(hook);

  return mutt_conn_new(cac);
}
