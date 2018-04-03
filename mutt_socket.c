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

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt_socket.h"
#include "mutt_account.h"
#include "protos.h"
#include "url.h"

/* support for multiple socket connections */
static struct ConnectionList Connections = TAILQ_HEAD_INITIALIZER(Connections);

struct ConnectionList *mutt_socket_head(void)
{
  return &Connections;
}

/**
 * mutt_socket_free - remove connection from connection list and free it
 */
void mutt_socket_free(struct Connection *conn)
{
  struct Connection *np = NULL;
  TAILQ_FOREACH(np, &Connections, entries)
  {
    if (np == conn)
    {
      TAILQ_REMOVE(&Connections, np, entries);
      FREE(&np);
      return;
    }
  }
}

/**
 * mutt_conn_find - Find a connection from a list
 *
 * find a connection off the list of connections whose account matches account.
 * If start is not null, only search for connections after the given connection
 * (allows higher level socket code to make more fine-grained searches than
 * account info - eg in IMAP we may wish to find a connection which is not in
 * IMAP_SELECTED state)
 */
struct Connection *mutt_conn_find(const struct Connection *start, const struct Account *account)
{
  struct Url url;
  char hook[LONG_STRING];

  /* account isn't actually modified, since url isn't either */
  mutt_account_tourl((struct Account *) account, &url);
  url.path = NULL;
  url_tostring(&url, hook, sizeof(hook), 0);
  mutt_account_hook(hook);

  struct Connection *conn = start ? TAILQ_NEXT(start, entries) : TAILQ_FIRST(&Connections);
  while (conn)
  {
    if (mutt_account_match(account, &(conn->account)))
      return conn;
    conn = TAILQ_NEXT(conn, entries);
  }

  conn = socket_new_conn();
  memcpy(&conn->account, account, sizeof(struct Account));

  TAILQ_INSERT_HEAD(&Connections, conn, entries);

  if (Tunnel && *Tunnel)
    mutt_tunnel_socket_setup(conn);
  else if (account->flags & MUTT_ACCT_SSL)
  {
#ifdef USE_SSL
    if (mutt_ssl_socket_setup(conn) < 0)
    {
      mutt_socket_free(conn);
      return NULL;
    }
#else
    mutt_error(_("SSL is unavailable, cannot connect to %s"), account->host);
    mutt_socket_free(conn);

    return NULL;
#endif
  }
  else
  {
    conn->conn_read = raw_socket_read;
    conn->conn_write = raw_socket_write;
    conn->conn_open = raw_socket_open;
    conn->conn_close = raw_socket_close;
    conn->conn_poll = raw_socket_poll;
  }

  return conn;
}
