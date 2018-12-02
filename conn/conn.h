/**
 * @file
 * Connection Library
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
 * @page conn CONN: Network connections and their encryption
 *
 * Manage external connections.
 *
 * | File                | Description              |
 * | :------------------ | :----------------------- |
 * | conn/conn_globals.c | @subpage conn_globals    |
 * | conn/getdomain.c    | @subpage conn_getdomain  |
 * | conn/conn_raw.c     | @subpage conn_raw        |
 * | conn/sasl.c         | @subpage conn_sasl       |
 * | conn/sasl_plain.c   | @subpage conn_sasl_plain |
 * | conn/socket.c       | @subpage conn_socket     |
 * | conn/ssl.c          | @subpage conn_ssl        |
 * | conn/ssl_gnutls.c   | @subpage conn_ssl_gnutls |
 * | conn/tunnel.c       | @subpage conn_tunnel     |
 */

#ifndef MUTT_CONN_CONN_H
#define MUTT_CONN_CONN_H

#include <stdio.h>
#include "conn_globals.h"
#include "connaccount.h"
#include "connection.h"
#include "sasl_plain.h"
#include "socket.h"
#include "ssl.h"
#include "tunnel.h"
#ifdef USE_SASL
#include "sasl.h"
#endif

int getdnsdomainname(char *buf, size_t buflen);

#endif /* MUTT_CONN_CONN_H */
