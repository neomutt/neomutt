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

#ifndef _CONN_SOCKET_H
#define _CONN_SOCKET_H

#include <stddef.h>
#include <time.h>

#include "mutt/queue.h"

enum ConnectionType
{
  MUTT_CONNECTION_SIMPLE,
  MUTT_CONNECTION_TUNNEL,
  MUTT_CONNECTION_SSL,
};

struct Connection;

/**
 * struct ConnectionList - A list of connections
 */
TAILQ_HEAD(ConnectionList, Connection);

/* stupid hack for imap_logout_all */
struct ConnectionList *mutt_socket_head(void);

struct Connection *mutt_socket_new(enum ConnectionType type);
void mutt_socket_free(struct Connection *conn);

int mutt_socket_open(struct Connection *conn);
int mutt_socket_close(struct Connection *conn);
int mutt_socket_read(struct Connection *conn, char *buf, size_t len);
int mutt_socket_write(struct Connection *conn, const char *buf, size_t len);
int mutt_socket_poll(struct Connection *conn, time_t wait_secs);
int mutt_socket_readchar(struct Connection *conn, char *c);
int mutt_socket_readln_d(char *buf, size_t buflen, struct Connection *conn, int dbg);
int mutt_socket_write_d(struct Connection *conn, const char *buf, int len, int dbg);

int raw_socket_read(struct Connection *conn, char *buf, size_t len);
int raw_socket_write(struct Connection *conn, const char *buf, size_t count);
int raw_socket_open(struct Connection *conn);
int raw_socket_close(struct Connection *conn);
int raw_socket_poll(struct Connection *conn, time_t wait_secs);

#endif /* _CONN_SOCKET_H */
