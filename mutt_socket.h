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

#ifndef _MUTT_SOCKET_H
#define _MUTT_SOCKET_H

#include "mutt/mutt.h"
#include "conn/conn.h"

struct Account;
struct Connection;

/* logging levels */
#define MUTT_SOCK_LOG_CMD  2
#define MUTT_SOCK_LOG_HDR  3
#define MUTT_SOCK_LOG_FULL 4

/**
 * struct ConnectionList - A list of connections
 */
TAILQ_HEAD(ConnectionList, Connection);

/* stupid hack for imap_logout_all */
struct ConnectionList *mutt_socket_head(void);
void mutt_socket_free(struct Connection *conn);
struct Connection *mutt_conn_find(const struct Connection *start, const struct Account *account);

#define mutt_socket_readln(A, B, C)  mutt_socket_readln_d(A, B, C, MUTT_SOCK_LOG_CMD)
#define mutt_socket_write(A, B)      mutt_socket_write_d(A, B, -1, MUTT_SOCK_LOG_CMD)
#define mutt_socket_write_n(A, B, C) mutt_socket_write_d(A, B, C, MUTT_SOCK_LOG_CMD)

#endif /* _MUTT_SOCKET_H */
