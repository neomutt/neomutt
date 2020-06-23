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

#ifndef MUTT_MUTT_SOCKET_H
#define MUTT_MUTT_SOCKET_H

struct ConnAccount;

/* logging levels */
#define MUTT_SOCK_LOG_CMD  2
#define MUTT_SOCK_LOG_HDR  3
#define MUTT_SOCK_LOG_FULL 5

struct Connection *mutt_conn_find(const struct ConnAccount *account);
struct Connection *mutt_conn_new(const struct ConnAccount *account);

#define mutt_socket_readln(buf, buflen, conn) mutt_socket_readln_d(buf, buflen, conn, MUTT_SOCK_LOG_CMD)
#define mutt_socket_send(conn, buf)           mutt_socket_send_d(conn, buf, MUTT_SOCK_LOG_CMD)
#define mutt_socket_send_d(conn, buf, dbg)    mutt_socket_write_d(conn, buf, mutt_str_len(buf), dbg)
#define mutt_socket_write_n(conn, buf, len)   mutt_socket_write_d(conn, buf, len, MUTT_SOCK_LOG_CMD)

#endif /* MUTT_MUTT_SOCKET_H */
