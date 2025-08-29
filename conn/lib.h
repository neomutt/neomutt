/**
 * @file
 * Connection Library
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page lib_conn Network connections
 *
 * Network connections and their encryption
 *
 * | File                  | Description                   |
 * | :-------------------- | :---------------------------- |
 * | conn/accountcmd.c     | @subpage conn_account_ext_cmd |
 * | conn/config.c         | @subpage conn_config          |
 * | conn/connaccount.c    | @subpage conn_account         |
 * | conn/dlg_verifycert.c | @subpage conn_dlg_verifycert  |
 * | conn/getdomain.c      | @subpage conn_getdomain       |
 * | conn/gnutls.c         | @subpage conn_gnutls          |
 * | conn/gsasl.c          | @subpage conn_gsasl           |
 * | conn/openssl.c        | @subpage conn_openssl         |
 * | conn/raw.c            | @subpage conn_raw             |
 * | conn/sasl.c           | @subpage conn_sasl            |
 * | conn/sasl_plain.c     | @subpage conn_sasl_plain      |
 * | conn/socket.c         | @subpage conn_socket          |
 * | conn/tunnel.c         | @subpage conn_tunnel          |
 * | conn/zstrm.c          | @subpage conn_zstrm           |
 */

#ifndef MUTT_CONN_LIB_H
#define MUTT_CONN_LIB_H

#include "config.h"
// IWYU pragma: begin_keep
#include "connaccount.h"
#include "connection.h"
#include "sasl_plain.h"
#include "socket.h"
#ifdef USE_SASL_GNU
#include "gsasl2.h"
#endif
#ifdef USE_SASL_CYRUS
#include "sasl.h"
#endif
#ifdef USE_ZLIB
#include "zstrm.h"
#endif
// IWYU pragma: end_keep

struct Buffer;

#ifdef USE_SSL
int mutt_ssl_starttls(struct Connection *conn);
#endif

int getdnsdomainname(struct Buffer *result);

#endif /* MUTT_CONN_LIB_H */
