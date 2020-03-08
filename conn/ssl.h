/**
 * @file
 * Handling of SSL encryption
 *
 * @authors
 * Copyright (C) 1999-2000 Tommi Komulainen <Tommi.Komulainen@iki.fi>
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

#ifndef MUTT_CONN_SSL_H
#define MUTT_CONN_SSL_H

#include "config.h"
#include <stdbool.h>

struct Connection;
struct ListHead;

#ifdef USE_SSL
int mutt_ssl_socket_setup(struct Connection *conn);
int dlg_verify_cert(const char *title, struct ListHead *list, bool allow_always, bool allow_skip);
#else
/**
 * [Dummy] Set up the socket multiplexor
 * @retval -1 Failure
 */
static inline int mutt_ssl_socket_setup(struct Connection *conn)
{
  return -1;
}
#endif

#endif /* MUTT_CONN_SSL_H */
