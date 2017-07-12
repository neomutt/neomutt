/**
 * @file
 * Handling of OpenSSL encryption
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

#ifndef _MUTT_SSL_H
#define _MUTT_SSL_H

#ifdef USE_SSL
struct Connection;

int mutt_ssl_starttls(struct Connection *conn);
int mutt_ssl_socket_setup(struct Connection *conn);
#endif

#endif /* _MUTT_SSL_H */
