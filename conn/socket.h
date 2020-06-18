/**
 * @file
 * Low-level socket handling
 *
 * @authors
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2005 Brendan Cully <brendan@kublai.com>
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

#ifndef MUTT_CONN_SOCKET_H
#define MUTT_CONN_SOCKET_H

#include <time.h>

struct Connection;

/**
 * enum ConnectionType - Type of connection
 */
enum ConnectionType
{
  MUTT_CONNECTION_SIMPLE, ///< Simple TCP socket connection
  MUTT_CONNECTION_TUNNEL, ///< Tunnelled connection
  MUTT_CONNECTION_SSL,    ///< SSL/TLS-encrypted connection
};

int                mutt_socket_close   (struct Connection *conn);
void               mutt_socket_empty   (struct Connection *conn);
struct Connection *mutt_socket_new     (enum ConnectionType type);
int                mutt_socket_open    (struct Connection *conn);
int                mutt_socket_poll    (struct Connection *conn, time_t wait_secs);
int                mutt_socket_read    (struct Connection *conn, char *buf, size_t len);
int                mutt_socket_readchar(struct Connection *conn, char *c);
int                mutt_socket_readln_d(char *buf, size_t buflen, struct Connection *conn, int dbg);
int                mutt_socket_write   (struct Connection *conn, const char *buf, size_t len);
int                mutt_socket_write_d (struct Connection *conn, const char *buf, int len, int dbg);

#endif /* MUTT_CONN_SOCKET_H */
